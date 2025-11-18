import 'package:flutter/foundation.dart';
import '../models/device.dart';
import '../models/control_data.dart';
import '../services/device_service.dart';
import '../services/offline_mode_service.dart';
import '../utils/api_exception.dart';
import '../config/app_config.dart';

/// Provider for device state management
class DeviceProvider with ChangeNotifier {
  final DeviceService _deviceService = DeviceService();
  final OfflineModeService _offlineModeService = OfflineModeService();

  List<Device> _devices = [];
  Device? _selectedDevice;
  bool _isLoading = false;
  String? _error;
  String? _selectedProjectId;

  List<Device> get devices => _selectedProjectId == null
      ? _devices
      : _devices.where((d) => d.projectId == _selectedProjectId).toList();

  Device? get selectedDevice => _selectedDevice;
  bool get isLoading => _isLoading;
  String? get error => _error;
  String? get selectedProjectId => _selectedProjectId;

  /// Get available projects for filtering
  Map<String, String> get projects {
    return _deviceService.getProjects(_devices);
  }

  /// Initialize the device provider
  Future<void> initialize() async {
    try {
      await _deviceService.initialize();
      await _offlineModeService.initialize();
    } catch (e) {
      _setError('Failed to initialize device service');
    }
  }

  /// Fetch all devices
  Future<void> fetchDevices({bool assignedOnly = true}) async {
    _setLoading(true);
    _setError(null);

    try {
      // Check if offline mode is enabled
      final isOffline = await _offlineModeService.isOfflineModeEnabled();

      if (isOffline) {
        // Load offline devices from local storage
        final offlineDevices = await _offlineModeService.getOfflineDevices();

        // Convert offline devices to Device models (with minimal data)
        _devices = offlineDevices.map((offlineDevice) {
          return Device(
            id: offlineDevice.deviceId,
            deviceId: offlineDevice.deviceId,
            name: offlineDevice.deviceName,
            projectId: AppConfig.projectId,
            projectName: 'Offline Mode',
            deviceConfig: {},
            controlData: {},
            telemetryData: {},
            isActive: false, // We'll update this when we fetch live data
            localIp: offlineDevice.localIp,
          );
        }).toList();

        AppConfig.offlineLog('Loaded ${_devices.length} offline devices');
        _setLoading(false);
      } else {
        // Online mode: Fetch from server
        _devices = await _deviceService.getDevices(
          assignedOnly: assignedOnly,
          projectId: AppConfig.projectId, // Use configured project ID
        );
        _setLoading(false);
      }
    } on ApiException catch (e) {
      _setError(e.message);
      _setLoading(false);
    } catch (e) {
      _setError('Failed to fetch devices');
      _setLoading(false);
    }
  }

  /// Refresh devices (for pull-to-refresh)
  Future<void> refreshDevices() async {
    await fetchDevices();
  }

  /// Select a device by fetching from API
  Future<void> selectDevice(String deviceId) async {
    _setLoading(true);
    _setError(null);

    try {
      _selectedDevice = await _deviceService.getDevice(deviceId);
      _setLoading(false);
    } on ApiException catch (e) {
      _setError(e.message);
      _setLoading(false);
    } catch (e) {
      _setError('Failed to fetch device details');
      _setLoading(false);
    }
  }

  /// Set selected device directly (without API call)
  void setSelectedDevice(Device device) {
    _selectedDevice = device;
    _error = null;
    notifyListeners();
  }

  /// Update control data for the selected device
  Future<bool> updateControl(
    String controlKey,
    dynamic value,
    String type,
  ) async {
    if (_selectedDevice == null) {
      _setError('No device selected');
      return false;
    }

    try {
      final updatedDevice = await _deviceService.updateSingleControl(
        _selectedDevice!.id,
        controlKey,
        value,
        type,
      );

      // Update the selected device
      _selectedDevice = updatedDevice;

      // Update the device in the list
      final index = _devices.indexWhere((d) => d.id == updatedDevice.id);
      if (index != -1) {
        _devices[index] = updatedDevice;
      }

      notifyListeners();
      return true;
    } on ApiException catch (e) {
      _setError(e.message);
      return false;
    } catch (e) {
      _setError('Failed to update control');
      return false;
    }
  }

  /// Update multiple controls at once
  Future<bool> updateControls(Map<String, ControlData> controlData) async {
    if (_selectedDevice == null) {
      _setError('No device selected');
      return false;
    }

    try {
      final updatedDevice = await _deviceService.updateControlData(
        _selectedDevice!.id,
        controlData,
      );

      // Update the selected device
      _selectedDevice = updatedDevice;

      // Update the device in the list
      final index = _devices.indexWhere((d) => d.id == updatedDevice.id);
      if (index != -1) {
        _devices[index] = updatedDevice;
      }

      notifyListeners();
      return true;
    } on ApiException catch (e) {
      _setError(e.message);
      return false;
    } catch (e) {
      _setError('Failed to update controls');
      return false;
    }
  }

  /// Filter devices by project
  void setProjectFilter(String? projectId) {
    _selectedProjectId = projectId;
    notifyListeners();
  }

  /// Clear project filter
  void clearProjectFilter() {
    _selectedProjectId = null;
    notifyListeners();
  }

  /// Clear error message
  void clearError() {
    _setError(null);
  }

  /// Clear selected device
  void clearSelectedDevice() {
    _selectedDevice = null;
    notifyListeners();
  }

  void _setLoading(bool loading) {
    _isLoading = loading;
    notifyListeners();
  }

  void _setError(String? error) {
    _error = error;
    notifyListeners();
  }
}
