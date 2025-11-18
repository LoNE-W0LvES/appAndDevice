import 'package:flutter/foundation.dart';
import '../models/wifi_network.dart';
import '../services/wifi_setup_service.dart';
import '../utils/api_exception.dart';

enum WiFiSetupStep {
  connectionCheck,
  wifiConfiguration,
  saving,
  completed,
}

class WiFiSetupProvider with ChangeNotifier {
  final WiFiSetupService _wifiSetupService = WiFiSetupService();

  WiFiSetupStep _currentStep = WiFiSetupStep.connectionCheck;
  bool _isLoading = false;
  String? _error;
  DeviceStatusResponse? _deviceStatus;
  List<WiFiNetwork> _availableNetworks = [];
  String _selectedSSID = '';
  String _password = '';
  String _dashboardUsername = '';
  String _dashboardPassword = '';
  bool _isPasswordVisible = false;
  bool _isDashboardPasswordVisible = false;
  bool _isScanning = false;

  // Getters
  WiFiSetupStep get currentStep => _currentStep;
  bool get isLoading => _isLoading;
  String? get error => _error;
  DeviceStatusResponse? get deviceStatus => _deviceStatus;
  List<WiFiNetwork> get availableNetworks => _availableNetworks;
  String get selectedSSID => _selectedSSID;
  String get password => _password;
  String get dashboardUsername => _dashboardUsername;
  String get dashboardPassword => _dashboardPassword;
  bool get isPasswordVisible => _isPasswordVisible;
  bool get isDashboardPasswordVisible => _isDashboardPasswordVisible;
  bool get isScanning => _isScanning;

  /// Reset state for new setup flow
  void reset() {
    _currentStep = WiFiSetupStep.connectionCheck;
    _isLoading = false;
    _error = null;
    _deviceStatus = null;
    _availableNetworks = [];
    _selectedSSID = '';
    _password = '';
    _dashboardUsername = '';
    _dashboardPassword = '';
    _isPasswordVisible = false;
    _isDashboardPasswordVisible = false;
    _isScanning = false;
    notifyListeners();
  }

  /// Check if device is ready for setup
  Future<bool> checkDeviceConnection(String deviceId) async {
    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      _deviceStatus = await _wifiSetupService.checkDeviceStatus(deviceId);

      if (_deviceStatus!.isReady) {
        _currentStep = WiFiSetupStep.wifiConfiguration;
        _isLoading = false;
        notifyListeners();
        return true;
      } else {
        _error = 'Device is not ready for setup';
        _isLoading = false;
        notifyListeners();
        return false;
      }
    } on ApiException catch (e) {
      _error = e.message;
      _isLoading = false;
      notifyListeners();
      return false;
    } catch (e) {
      _error = 'Failed to connect to device: ${e.toString()}';
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  /// Scan for WiFi networks
  Future<bool> scanWiFiNetworks(String deviceId) async {
    _isScanning = true;
    _error = null;
    notifyListeners();

    try {
      final response = await _wifiSetupService.scanWiFiNetworks(deviceId);
      _availableNetworks = response.networks;

      // Sort networks by signal strength (strongest first)
      _availableNetworks.sort((a, b) => b.signal.compareTo(a.signal));

      _isScanning = false;
      notifyListeners();
      return true;
    } on ApiException catch (e) {
      _error = e.message;
      _isScanning = false;
      notifyListeners();
      return false;
    } catch (e) {
      _error = 'Failed to scan WiFi networks: ${e.toString()}';
      _isScanning = false;
      notifyListeners();
      return false;
    }
  }

  /// Set selected SSID
  void setSelectedSSID(String ssid) {
    _selectedSSID = ssid;
    notifyListeners();
  }

  /// Set password
  void setPassword(String password) {
    _password = password;
    notifyListeners();
  }

  /// Set dashboard username
  void setDashboardUsername(String username) {
    _dashboardUsername = username;
    notifyListeners();
  }

  /// Set dashboard password
  void setDashboardPassword(String password) {
    _dashboardPassword = password;
    notifyListeners();
  }

  /// Toggle password visibility
  void togglePasswordVisibility() {
    _isPasswordVisible = !_isPasswordVisible;
    notifyListeners();
  }

  /// Toggle dashboard password visibility
  void toggleDashboardPasswordVisibility() {
    _isDashboardPasswordVisible = !_isDashboardPasswordVisible;
    notifyListeners();
  }

  /// Save WiFi credentials to device
  Future<bool> saveWiFiCredentials(String deviceId) async {
    if (_selectedSSID.isEmpty) {
      _error = 'Please select a WiFi network';
      notifyListeners();
      return false;
    }

    // Check if network is secured and password is required
    final selectedNetwork = _availableNetworks.firstWhere(
      (network) => network.ssid == _selectedSSID,
      orElse: () => const WiFiNetwork(ssid: '', signal: 0, auth: 'UNKNOWN'),
    );

    if (selectedNetwork.isSecured && _password.isEmpty) {
      _error = 'Password is required for secured networks';
      notifyListeners();
      return false;
    }

    _currentStep = WiFiSetupStep.saving;
    _isLoading = true;
    _error = null;
    notifyListeners();

    try {
      final response = await _wifiSetupService.saveWiFiCredentials(
        deviceId: deviceId,
        ssid: _selectedSSID,
        password: _password,
        dashboardUsername: _dashboardUsername,
        dashboardPassword: _dashboardPassword,
      );

      if (response.success) {
        _currentStep = WiFiSetupStep.completed;
        _isLoading = false;
        notifyListeners();
        return true;
      } else {
        _error = response.message;
        _currentStep = WiFiSetupStep.wifiConfiguration;
        _isLoading = false;
        notifyListeners();
        return false;
      }
    } on ApiException catch (e) {
      _error = e.message;
      _currentStep = WiFiSetupStep.wifiConfiguration;
      _isLoading = false;
      notifyListeners();
      return false;
    } catch (e) {
      _error = 'Failed to save WiFi credentials: ${e.toString()}';
      _currentStep = WiFiSetupStep.wifiConfiguration;
      _isLoading = false;
      notifyListeners();
      return false;
    }
  }

  /// Clear error message
  void clearError() {
    _error = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _wifiSetupService.dispose();
    super.dispose();
  }
}
