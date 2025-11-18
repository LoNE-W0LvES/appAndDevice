import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import '../config/app_config.dart';

/// Model for offline device with manual IP
class OfflineDevice {
  final String deviceId;
  final String deviceName;
  final String localIp;
  final DateTime addedAt;

  OfflineDevice({
    required this.deviceId,
    required this.deviceName,
    required this.localIp,
    required this.addedAt,
  });

  Map<String, dynamic> toJson() {
    return {
      'deviceId': deviceId,
      'deviceName': deviceName,
      'localIp': localIp,
      'addedAt': addedAt.toIso8601String(),
    };
  }

  factory OfflineDevice.fromJson(Map<String, dynamic> json) {
    return OfflineDevice(
      deviceId: json['deviceId'] as String,
      deviceName: json['deviceName'] as String,
      localIp: json['localIp'] as String,
      addedAt: DateTime.parse(json['addedAt'] as String),
    );
  }
}

/// Service for managing offline mode
class OfflineModeService {
  static final OfflineModeService _instance = OfflineModeService._internal();
  factory OfflineModeService() => _instance;
  OfflineModeService._internal();

  SharedPreferences? _prefs;

  /// Initialize the service
  Future<void> initialize() async {
    _prefs ??= await SharedPreferences.getInstance();
  }

  /// Check if offline mode is enabled
  Future<bool> isOfflineModeEnabled() async {
    _prefs ??= await SharedPreferences.getInstance();
    final isOffline = _prefs?.getBool(AppConfig.offlineModeKey) ?? false;
    AppConfig.offlineLog('Offline mode is ${isOffline ? "enabled" : "disabled"}');
    return isOffline;
  }

  /// Enable offline mode and clear all credentials
  Future<void> enableOfflineMode() async {
    _prefs ??= await SharedPreferences.getInstance();

    AppConfig.offlineLog('Enabling offline mode...');

    // Set offline mode flag
    await _prefs?.setBool(AppConfig.offlineModeKey, true);

    // Clear all saved credentials
    await _prefs?.remove(AppConfig.sessionStorageKey);
    await _prefs?.remove(AppConfig.keepLoggedInKey);
    await _prefs?.remove(AppConfig.dashboardUsernameKey);
    await _prefs?.remove(AppConfig.dashboardPasswordKey);

    AppConfig.offlineLog('Offline mode enabled and credentials cleared');
  }

  /// Disable offline mode
  Future<void> disableOfflineMode() async {
    _prefs ??= await SharedPreferences.getInstance();

    AppConfig.offlineLog('Disabling offline mode...');

    await _prefs?.setBool(AppConfig.offlineModeKey, false);

    // Clear offline devices when exiting offline mode
    await clearOfflineDevices();

    AppConfig.offlineLog('Offline mode disabled');
  }

  /// Add offline device with manual IP
  Future<void> addOfflineDevice({
    required String deviceId,
    required String deviceName,
    required String localIp,
  }) async {
    _prefs ??= await SharedPreferences.getInstance();

    final device = OfflineDevice(
      deviceId: deviceId,
      deviceName: deviceName,
      localIp: localIp,
      addedAt: DateTime.now(),
    );

    final devices = await getOfflineDevices();

    // Remove existing device with same ID if exists
    devices.removeWhere((d) => d.deviceId == deviceId);

    // Add new device
    devices.add(device);

    // Save to preferences
    final devicesJson = devices.map((d) => d.toJson()).toList();
    await _prefs?.setString(AppConfig.offlineDevicesKey, jsonEncode(devicesJson));

    AppConfig.offlineLog('Added offline device: $deviceId at $localIp');
  }

  /// Get all offline devices
  Future<List<OfflineDevice>> getOfflineDevices() async {
    _prefs ??= await SharedPreferences.getInstance();

    final devicesJson = _prefs?.getString(AppConfig.offlineDevicesKey);

    if (devicesJson == null || devicesJson.isEmpty) {
      return [];
    }

    try {
      final List<dynamic> decoded = jsonDecode(devicesJson);
      return decoded.map((json) => OfflineDevice.fromJson(json)).toList();
    } catch (e) {
      AppConfig.errorLog('Failed to parse offline devices', error: e);
      return [];
    }
  }

  /// Get offline device by ID
  Future<OfflineDevice?> getOfflineDevice(String deviceId) async {
    final devices = await getOfflineDevices();
    try {
      return devices.firstWhere((d) => d.deviceId == deviceId);
    } catch (e) {
      return null;
    }
  }

  /// Update device IP
  Future<void> updateDeviceIp(String deviceId, String newIp) async {
    final devices = await getOfflineDevices();
    final index = devices.indexWhere((d) => d.deviceId == deviceId);

    if (index != -1) {
      final oldDevice = devices[index];
      devices[index] = OfflineDevice(
        deviceId: oldDevice.deviceId,
        deviceName: oldDevice.deviceName,
        localIp: newIp,
        addedAt: oldDevice.addedAt,
      );

      _prefs ??= await SharedPreferences.getInstance();
      final devicesJson = devices.map((d) => d.toJson()).toList();
      await _prefs?.setString(AppConfig.offlineDevicesKey, jsonEncode(devicesJson));

      AppConfig.offlineLog('Updated device $deviceId IP to $newIp');
    }
  }

  /// Remove offline device
  Future<void> removeOfflineDevice(String deviceId) async {
    final devices = await getOfflineDevices();
    devices.removeWhere((d) => d.deviceId == deviceId);

    _prefs ??= await SharedPreferences.getInstance();
    final devicesJson = devices.map((d) => d.toJson()).toList();
    await _prefs?.setString(AppConfig.offlineDevicesKey, jsonEncode(devicesJson));

    AppConfig.offlineLog('Removed offline device: $deviceId');
  }

  /// Clear all offline devices
  Future<void> clearOfflineDevices() async {
    _prefs ??= await SharedPreferences.getInstance();
    await _prefs?.remove(AppConfig.offlineDevicesKey);
    AppConfig.offlineLog('Cleared all offline devices');
  }
}
