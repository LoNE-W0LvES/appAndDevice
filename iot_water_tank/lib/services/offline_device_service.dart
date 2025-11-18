import 'dart:convert';
import 'package:dio/dio.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/device.dart';
import '../models/control_data.dart';
import '../models/device_config_parameter.dart';
import '../utils/api_exception.dart';
import '../config/app_config.dart';
import 'api_client.dart';
import 'offline_mode_service.dart';

/// Service for offline-capable device operations
/// Tries local IP first, then falls back to server (unless in offline mode)
class OfflineDeviceService {
  final ApiClient _apiClient;
  final OfflineModeService _offlineModeService;
  Dio? _localDio;

  OfflineDeviceService()
      : _apiClient = ApiClient(),
        _offlineModeService = OfflineModeService();

  Future<void> initialize() async {
    await _apiClient.initialize();
    await _offlineModeService.initialize();

    // Create a separate Dio instance for local connections
    _localDio = Dio(BaseOptions(
      connectTimeout: const Duration(seconds: 3),
      receiveTimeout: const Duration(seconds: 5),
      sendTimeout: const Duration(seconds: 5),
      validateStatus: (status) => status! < 500,
    ));
  }

  /// Get full device data including config (use for settings)
  /// Tries local IP first, then falls back to server (unless in offline mode)
  Future<Device> getDevice(String deviceId, {String? localIp}) async {
    final isOffline = await _offlineModeService.isOfflineModeEnabled();

    // Try local connection first if IP is available
    if (localIp != null && localIp.isNotEmpty) {
      try {
        AppConfig.offlineLog('Attempting to fetch full device from local IP: $localIp');
        final localDevice = await _getDeviceFromLocal(deviceId, localIp);

        // Cache the full data locally
        await _cacheDeviceData(deviceId, localDevice);
        await _cacheDeviceConfig(deviceId, localDevice.deviceConfig);

        AppConfig.offlineLog('Successfully fetched full device from local IP');
        return localDevice;
      } catch (e) {
        AppConfig.offlineLog('Local connection failed: $e');

        // In offline mode, don't fall back to server
        if (isOffline) {
          AppConfig.offlineLog('Offline mode: Skipping server fallback');
          final cachedDevice = await _getCachedDeviceData(deviceId);
          if (cachedDevice != null) {
            AppConfig.offlineLog('Using cached full device data');
            return cachedDevice;
          }
          throw ApiException(message: 'Device not reachable and no cached data available');
        }

        AppConfig.offlineLog('Falling back to server...');
      }
    }

    // Fall back to server (only if not in offline mode)
    if (!isOffline) {
      try {
        final response = await _apiClient.get('/api/devices/$deviceId');
        final device = Device.fromJson(response.data);

        // Cache the full data locally
        await _cacheDeviceData(deviceId, device);
        await _cacheDeviceConfig(deviceId, device.deviceConfig);

        return device;
      } catch (e) {
        // Try to return cached data if available
        final cachedDevice = await _getCachedDeviceData(deviceId);
        if (cachedDevice != null) {
          AppConfig.offlineLog('Using cached full device data (server failed)');
          return cachedDevice;
        }

        throw ApiException(message: 'Failed to fetch device data');
      }
    } else {
      // Offline mode with no local IP - use cached data only
      final cachedDevice = await _getCachedDeviceData(deviceId);
      if (cachedDevice != null) {
        AppConfig.offlineLog('Offline mode: Using cached full device data');
        return cachedDevice;
      }
      throw ApiException(message: 'No device data available in offline mode');
    }
  }

  /// Get device live data (telemetry + control only, use cached config)
  /// This is more efficient for regular dashboard updates
  Future<Device> getDeviceLiveData(String deviceId, {String? localIp}) async {
    final isOffline = await _offlineModeService.isOfflineModeEnabled();

    // Get cached config first
    final cachedConfig = await _getCachedDeviceConfig(deviceId);

    // Try local connection first if IP is available
    if (localIp != null && localIp.isNotEmpty) {
      try {
        AppConfig.offlineLog('Attempting to fetch live data from local IP: $localIp');
        final localDevice = await _getDeviceFromLocal(deviceId, localIp);

        // Merge with cached config if available
        final mergedDevice = cachedConfig != null
            ? localDevice.copyWith(deviceConfig: cachedConfig)
            : localDevice;

        // Cache the live data
        await _cacheDeviceData(deviceId, mergedDevice);

        AppConfig.offlineLog('Successfully fetched live data from local IP');
        return mergedDevice;
      } catch (e) {
        AppConfig.offlineLog('Local connection failed: $e');

        // In offline mode, don't fall back to server
        if (isOffline) {
          AppConfig.offlineLog('Offline mode: Skipping server fallback');
          final cachedDevice = await _getCachedDeviceData(deviceId);
          if (cachedDevice != null) {
            AppConfig.offlineLog('Using cached live data');
            return cachedDevice;
          }
          throw ApiException(message: 'Device not reachable and no cached data available');
        }

        AppConfig.offlineLog('Falling back to server...');
      }
    }

    // Fall back to server (only if not in offline mode)
    if (!isOffline) {
      try {
        final response = await _apiClient.get('/api/devices/$deviceId');
        final device = Device.fromJson(response.data);

        // Use cached config if available to avoid overwriting
        final mergedDevice = cachedConfig != null
            ? device.copyWith(deviceConfig: cachedConfig)
            : device;

        // Cache the live data
        await _cacheDeviceData(deviceId, mergedDevice);

        return mergedDevice;
      } catch (e) {
        // Try to return cached data if available
        final cachedDevice = await _getCachedDeviceData(deviceId);
        if (cachedDevice != null) {
          AppConfig.offlineLog('Using cached live data (server failed)');
          return cachedDevice;
        }

        throw ApiException(message: 'Failed to fetch device data');
      }
    } else {
      // Offline mode with no local IP - use cached data only
      final cachedDevice = await _getCachedDeviceData(deviceId);
      if (cachedDevice != null) {
        AppConfig.offlineLog('Offline mode: Using cached live data');
        return cachedDevice;
      }
      throw ApiException(message: 'No device data available in offline mode');
    }
  }

  /// Get device from local IP
  Future<Device> _getDeviceFromLocal(String deviceId, String localIp) async {
    if (_localDio == null) throw Exception('Service not initialized');

    // Fetch telemetry, control, and config data from device's local endpoints
    final telemetryUrl = 'http://$localIp/$deviceId/telemetry';
    final controlUrl = 'http://$localIp/$deviceId/control';
    final configUrl = 'http://$localIp/$deviceId/config';

    try {
      // Fetch all three endpoints
      final telemetryResponse = await _localDio!.get(telemetryUrl);
      final controlResponse = await _localDio!.get(controlUrl);
      final configResponse = await _localDio!.get(configUrl);

      if (telemetryResponse.statusCode == 200 &&
          controlResponse.statusCode == 200 &&
          configResponse.statusCode == 200) {
        // Merge telemetry, control, and config data into a device object
        // Explicitly cast response data to Map<String, dynamic>
        final telemetryData = telemetryResponse.data is Map
            ? Map<String, dynamic>.from(telemetryResponse.data as Map)
            : <String, dynamic>{};
        final controlData = controlResponse.data is Map
            ? Map<String, dynamic>.from(controlResponse.data as Map)
            : <String, dynamic>{};
        final configData = configResponse.data is Map
            ? Map<String, dynamic>.from(configResponse.data as Map)
            : <String, dynamic>{};

        final deviceData = <String, dynamic>{
          'id': deviceId,
          'name': deviceId, // Will be overridden by cached data if available
          'deviceId': deviceId,
          'projectId': AppConfig.projectId,
          'isActive': true,
          'telemetryData': telemetryData,
          'controlData': controlData,
          'deviceConfig': configData,
          'localIp': localIp, // Preserve local IP for future requests
        };

        return Device.fromJson(deviceData);
      } else {
        throw ApiException(
          message: 'Local device returned status: telemetry=${telemetryResponse.statusCode}, control=${controlResponse.statusCode}, config=${configResponse.statusCode}',
        );
      }
    } catch (e) {
      AppConfig.errorLog('Failed to fetch from local device', error: e);
      rethrow;
    }
  }

  /// Update control data with offline support
  Future<bool> updateControl(
    String deviceId,
    String key,
    dynamic value,
    String type, {
    String? localIp,
  }) async {
    final timestamp = DateTime.now().millisecondsSinceEpoch;
    final isOffline = await _offlineModeService.isOfflineModeEnabled();

    // Try local connection first if IP is available
    if (localIp != null && localIp.isNotEmpty) {
      try {
        AppConfig.offlineLog('Attempting to update control via local IP: $localIp');
        await _updateControlLocal(deviceId, key, value, type, timestamp, localIp);

        AppConfig.offlineLog('Control updated via local IP');
        return true;
      } catch (e) {
        AppConfig.offlineLog('Local control update failed: $e');

        // In offline mode, don't fall back to server - just queue it
        if (isOffline) {
          AppConfig.offlineLog('Offline mode: Queuing control update without server fallback');
          await _queueControlUpdate(deviceId, key, value, type, timestamp);
          return true;
        }

        AppConfig.offlineLog('Falling back to server...');
      }
    }

    // Try server (only if not in offline mode)
    if (!isOffline) {
      try {
        await _apiClient.patch('/api/devices/$deviceId', data: {
          'controlData': {
            key: {
              'type': type,
              'value': value,
              'lastModified': timestamp,
            }
          }
        });

        // Remove from queue if it was there
        await _removeFromQueue(deviceId, key);

        return true;
      } catch (e) {
        // Queue for later sync
        await _queueControlUpdate(deviceId, key, value, type, timestamp);
        AppConfig.offlineLog('Control update queued for sync (server failed)');
        return true; // Return true since it's queued
      }
    } else {
      // Offline mode with no local IP - queue for later
      await _queueControlUpdate(deviceId, key, value, type, timestamp);
      AppConfig.offlineLog('Offline mode: Control update queued (no local IP)');
      return true;
    }
  }

  /// Update control on local device
  Future<void> _updateControlLocal(
    String deviceId,
    String key,
    dynamic value,
    String type,
    int timestamp,
    String localIp,
  ) async {
    if (_localDio == null) throw Exception('Service not initialized');

    // Use device's local endpoint: POST /{deviceId}/control
    final url = 'http://$localIp/$deviceId/control';
    final response = await _localDio!.post(url, data: {
      key: {
        'type': type,
        'value': value,
        'lastModified': timestamp,
      }
    });

    if (response.statusCode != 200) {
      throw ApiException(message: 'Local control update failed with status ${response.statusCode}');
    }
  }

  /// Update device configuration with offline support
  Future<bool> updateDeviceConfig(
    String deviceId,
    Map<String, DeviceConfigParameter> deviceConfig, {
    String? localIp,
  }) async {
    final isOffline = await _offlineModeService.isOfflineModeEnabled();

    // Try local connection first if IP is available
    if (localIp != null && localIp.isNotEmpty) {
      try {
        AppConfig.offlineLog('Attempting to update device config via local IP: $localIp');
        await _updateConfigLocal(deviceId, deviceConfig, localIp);

        AppConfig.offlineLog('Device config updated via local IP');
        return true;
      } catch (e) {
        AppConfig.offlineLog('Local config update failed: $e');

        // In offline mode, don't fall back to server
        if (isOffline) {
          AppConfig.offlineLog('Offline mode: Cannot update config without local connection');
          throw ApiException(message: 'Device not reachable for config update');
        }

        AppConfig.offlineLog('Falling back to server...');
      }
    }

    // Try server (only if not in offline mode)
    if (!isOffline) {
      try {
        // Use DeviceService for server update
        throw ApiException(message: 'Use DeviceService.updateDeviceConfig for server updates');
      } catch (e) {
        throw ApiException(message: 'Failed to update device config');
      }
    } else {
      // Offline mode with no local IP
      throw ApiException(message: 'Cannot update config in offline mode without local IP');
    }
  }

  /// Update config on local device
  Future<void> _updateConfigLocal(
    String deviceId,
    Map<String, DeviceConfigParameter> deviceConfig,
    String localIp,
  ) async {
    if (_localDio == null) throw Exception('Service not initialized');

    // Use device's local endpoint: POST /{deviceId}/config
    final url = 'http://$localIp/$deviceId/config';

    // Convert config to JSON format expected by device
    final configData = deviceConfig.map(
      (key, value) => MapEntry(key, {
        'value': value.value,
        'lastModified': DateTime.now().millisecondsSinceEpoch,
      }),
    );

    final response = await _localDio!.post(url, data: configData);

    if (response.statusCode != 200) {
      throw ApiException(message: 'Local config update failed with status ${response.statusCode}');
    }
  }

  /// Cache device data locally
  Future<void> _cacheDeviceData(String deviceId, Device device) async {
    final prefs = await SharedPreferences.getInstance();
    final cacheKey = 'device_cache_$deviceId';
    final jsonString = jsonEncode(device.toJson());
    await prefs.setString(cacheKey, jsonString);
    await prefs.setInt('${cacheKey}_timestamp', DateTime.now().millisecondsSinceEpoch);
  }

  /// Get cached device data
  Future<Device?> _getCachedDeviceData(String deviceId) async {
    final prefs = await SharedPreferences.getInstance();
    final cacheKey = 'device_cache_$deviceId';
    final jsonString = prefs.getString(cacheKey);

    if (jsonString != null) {
      try {
        final json = jsonDecode(jsonString);
        return Device.fromJson(json);
      } catch (e) {
        AppConfig.errorLog('Error parsing cached device data', error: e);
        return null;
      }
    }

    return null;
  }

  /// Cache device config separately (for use with live data updates)
  Future<void> _cacheDeviceConfig(String deviceId, Map<String, DeviceConfigParameter> config) async {
    final prefs = await SharedPreferences.getInstance();
    final cacheKey = 'device_config_$deviceId';
    final configJson = config.map((key, value) => MapEntry(key, value.toJson()));
    final jsonString = jsonEncode(configJson);
    await prefs.setString(cacheKey, jsonString);
    AppConfig.offlineLog('Cached device config for $deviceId');
  }

  /// Get cached device config
  Future<Map<String, DeviceConfigParameter>?> _getCachedDeviceConfig(String deviceId) async {
    final prefs = await SharedPreferences.getInstance();
    final cacheKey = 'device_config_$deviceId';
    final jsonString = prefs.getString(cacheKey);

    if (jsonString != null) {
      try {
        final json = jsonDecode(jsonString) as Map<String, dynamic>;
        final Map<String, DeviceConfigParameter> config = {};
        json.forEach((key, value) {
          if (value is Map<String, dynamic>) {
            config[key] = DeviceConfigParameter.fromJson(key, value);
          }
        });
        AppConfig.offlineLog('Using cached device config for $deviceId');
        return config;
      } catch (e) {
        AppConfig.errorLog('Error parsing cached device config', error: e);
        return null;
      }
    }

    return null;
  }

  /// Queue control update for later sync
  Future<void> _queueControlUpdate(
    String deviceId,
    String key,
    dynamic value,
    String type,
    int timestamp,
  ) async {
    final prefs = await SharedPreferences.getInstance();
    final queueKey = 'sync_queue';

    // Get existing queue
    final queueJson = prefs.getString(queueKey);
    List<Map<String, dynamic>> queue = [];

    if (queueJson != null) {
      queue = List<Map<String, dynamic>>.from(jsonDecode(queueJson));
    }

    // Add new item or update existing
    final existingIndex = queue.indexWhere(
      (item) => item['deviceId'] == deviceId && item['key'] == key,
    );

    final updateData = {
      'deviceId': deviceId,
      'key': key,
      'value': value,
      'type': type,
      'timestamp': timestamp,
    };

    if (existingIndex >= 0) {
      // Update existing entry if timestamp is newer
      if (timestamp > (queue[existingIndex]['timestamp'] as int)) {
        queue[existingIndex] = updateData;
      }
    } else {
      queue.add(updateData);
    }

    // Save queue
    await prefs.setString(queueKey, jsonEncode(queue));
  }

  /// Remove item from sync queue
  Future<void> _removeFromQueue(String deviceId, String key) async {
    final prefs = await SharedPreferences.getInstance();
    final queueKey = 'sync_queue';

    final queueJson = prefs.getString(queueKey);
    if (queueJson != null) {
      List<Map<String, dynamic>> queue = List<Map<String, dynamic>>.from(jsonDecode(queueJson));
      queue.removeWhere((item) => item['deviceId'] == deviceId && item['key'] == key);
      await prefs.setString(queueKey, jsonEncode(queue));
    }
  }

  /// Sync pending changes to server
  Future<SyncResult> syncPendingChanges() async {
    final prefs = await SharedPreferences.getInstance();
    final queueKey = 'sync_queue';
    final queueJson = prefs.getString(queueKey);

    if (queueJson == null) {
      return SyncResult(synced: 0, failed: 0);
    }

    List<Map<String, dynamic>> queue = List<Map<String, dynamic>>.from(jsonDecode(queueJson));

    int synced = 0;
    int failed = 0;
    List<Map<String, dynamic>> failedItems = [];

    for (final item in queue) {
      try {
        // Fetch current server data to compare timestamps
        final serverDevice = await _apiClient.get('/api/devices/${item['deviceId']}');
        final serverControlData = serverDevice.data['controlData'] ?? {};
        final serverControl = serverControlData[item['key']];

        int? serverTimestamp = serverControl?['lastModified'];
        int localTimestamp = item['timestamp'] as int;

        // Only sync if local change is newer (last-write-wins)
        if (serverTimestamp == null || localTimestamp > serverTimestamp) {
          await _apiClient.patch('/api/devices/${item['deviceId']}', data: {
            'controlData': {
              item['key']: {
                'type': item['type'],
                'value': item['value'],
                'lastModified': localTimestamp,
              }
            }
          });

          synced++;
          AppConfig.offlineLog('Synced ${item['key']} for device ${item['deviceId']}');
        } else {
          AppConfig.offlineLog('Skipped ${item['key']} - server has newer data');
          synced++; // Count as synced since we resolved the conflict
        }
      } catch (e) {
        AppConfig.offlineLog('Failed to sync ${item['key']}: $e');
        failed++;
        failedItems.add(item);
      }
    }

    // Update queue with only failed items
    await prefs.setString(queueKey, jsonEncode(failedItems));

    return SyncResult(synced: synced, failed: failed);
  }

  /// Get sync queue status
  Future<int> getPendingChangesCount() async {
    final prefs = await SharedPreferences.getInstance();
    final queueJson = prefs.getString('sync_queue');

    if (queueJson == null) return 0;

    List<Map<String, dynamic>> queue = List<Map<String, dynamic>>.from(jsonDecode(queueJson));
    return queue.length;
  }

  /// Clear all cached data and pending changes
  Future<void> clearAllCache() async {
    final prefs = await SharedPreferences.getInstance();
    final keys = prefs.getKeys();

    for (final key in keys) {
      if (key.startsWith('device_cache_') || key == 'sync_queue') {
        await prefs.remove(key);
      }
    }
  }
}

/// Result of sync operation
class SyncResult {
  final int synced;
  final int failed;

  SyncResult({required this.synced, required this.failed});

  bool get hasFailures => failed > 0;
  bool get allSynced => failed == 0 && synced > 0;
}
