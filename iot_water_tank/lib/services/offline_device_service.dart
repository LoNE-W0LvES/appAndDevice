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

  // In-memory cache to avoid repeatedly reading from SharedPreferences
  final Map<String, Map<String, DeviceConfigParameter>> _configCache = {};

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

    // Try local connection first if IP is available
    if (localIp != null && localIp.isNotEmpty) {
      try {
        AppConfig.offlineLog('⚡ Fetching LIVE telemetry from: http://$localIp/$deviceId');
        final localDevice = await _getDeviceFromLocal(deviceId, localIp);

        // Use the fresh config from the device and update cache
        await _cacheDeviceData(deviceId, localDevice);
        await _cacheDeviceConfig(deviceId, localDevice.deviceConfig);

        AppConfig.offlineLog('✅ FRESH data received from device:');
        AppConfig.offlineLog('   Water Level: ${localDevice.telemetryData['waterLevel']}');
        AppConfig.offlineLog('   Pump: ${localDevice.controlData['pumpSwitch']?['value']}');
        AppConfig.offlineLog('   Inflow: ${localDevice.telemetryData['inflow']}');
        return localDevice;
      } catch (e) {
        AppConfig.errorLog('❌ FAILED to fetch from device', error: e);

        // In offline mode, don't fall back to server
        if (isOffline) {
          AppConfig.offlineLog('⚠️  Offline mode: Skipping server fallback');
          final cachedDevice = await _getCachedDeviceData(deviceId);
          if (cachedDevice != null) {
            AppConfig.offlineLog('📦 Using CACHED data (device unreachable)');
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
        AppConfig.offlineLog('🌐 Falling back to SERVER for live data');
        final response = await _apiClient.get('/api/devices/$deviceId');
        final device = Device.fromJson(response.data);

        // Use the fresh data from server and update cache
        await _cacheDeviceData(deviceId, device);
        await _cacheDeviceConfig(deviceId, device.deviceConfig);

        AppConfig.offlineLog('✅ FRESH data received from server');
        return device;
      } catch (e) {
        // Try to return cached data if available
        final cachedDevice = await _getCachedDeviceData(deviceId);
        if (cachedDevice != null) {
          AppConfig.offlineLog('📦 Using CACHED data (server failed, device failed)');
          return cachedDevice;
        }

        throw ApiException(message: 'Failed to fetch device data');
      }
    } else {
      // Offline mode with no local IP - use cached data only
      final cachedDevice = await _getCachedDeviceData(deviceId);
      if (cachedDevice != null) {
        AppConfig.offlineLog('📦 Offline mode: Using CACHED data (no local IP)');
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

  /// Check device status via local endpoint
  Future<Map<String, dynamic>> checkDeviceStatus(String localIp, String deviceId) async {
    if (_localDio == null) throw Exception('Service not initialized');

    final url = 'http://$localIp/$deviceId/status';
    AppConfig.offlineLog('Checking device status at: $url');

    try {
      final response = await _localDio!.get(url);

      if (response.statusCode == 200 && response.data is Map) {
        return Map<String, dynamic>.from(response.data as Map);
      }

      throw ApiException(message: 'Invalid status response from device');
    } catch (e) {
      AppConfig.offlineLog('Device status check failed: $e');
      rethrow;
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

        // Cache the full config (deviceConfig already contains all fields)
        await _cacheDeviceConfig(deviceId, deviceConfig);

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
    // Store in memory cache
    _configCache[deviceId] = config;

    // Store in SharedPreferences
    final prefs = await SharedPreferences.getInstance();
    final cacheKey = 'device_config_$deviceId';
    final configJson = config.map((key, value) => MapEntry(key, value.toJson()));
    final jsonString = jsonEncode(configJson);
    await prefs.setString(cacheKey, jsonString);
    AppConfig.offlineLog('Cached device config for $deviceId');
  }

  /// Get cached device config
  Future<Map<String, DeviceConfigParameter>?> _getCachedDeviceConfig(String deviceId) async {
    // Check in-memory cache first
    if (_configCache.containsKey(deviceId)) {
      return _configCache[deviceId];
    }

    // Not in memory, read from SharedPreferences
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
        // Store in memory cache for next time
        _configCache[deviceId] = config;
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
    // Clear in-memory cache
    _configCache.clear();

    // Clear SharedPreferences cache
    final prefs = await SharedPreferences.getInstance();
    final keys = prefs.getKeys();

    for (final key in keys) {
      if (key.startsWith('device_cache_') ||
          key.startsWith('device_config_') ||
          key == 'sync_queue') {
        await prefs.remove(key);
      }
    }
    AppConfig.offlineLog('Cleared all cached device data');
  }

  /// Clear cache for a specific device
  Future<void> clearDeviceCache(String deviceId) async {
    // Clear in-memory cache
    _configCache.remove(deviceId);

    // Clear SharedPreferences cache
    final prefs = await SharedPreferences.getInstance();
    final deviceCacheKey = 'device_cache_$deviceId';
    final configCacheKey = 'device_config_$deviceId';

    await prefs.remove(deviceCacheKey);
    await prefs.remove(configCacheKey);
    await prefs.remove('${deviceCacheKey}_timestamp');

    AppConfig.offlineLog('Cleared cache for device: $deviceId');
  }

  // =========================================================================
  // TIMESTAMP SYNC METHODS
  // =========================================================================

  /// Check device timestamp and sync if needed
  /// Returns true if sync was performed, false if timestamp was already accurate
  Future<bool> checkAndSyncDeviceTime(String deviceId, String localIp) async {
    if (_localDio == null) throw Exception('Service not initialized');

    try {
      // Get device timestamp info
      final url = 'http://$localIp/$deviceId/timestamp';
      final response = await _localDio!.get(url);

      if (response.statusCode != 200) {
        AppConfig.offlineLog('Failed to get device timestamp: ${response.statusCode}');
        return false;
      }

      final data = response.data as Map<String, dynamic>;
      final deviceTimestamp = data['timestamp'] as int? ?? 0;
      final isSynced = data['synced'] as bool? ?? false;

      // Get phone's current timestamp (milliseconds)
      final phoneTimestamp = DateTime.now().millisecondsSinceEpoch;

      // Check if sync is needed
      bool needsSync = false;
      String reason = '';

      if (deviceTimestamp == 0) {
        needsSync = true;
        reason = 'device timestamp is zero';
      } else if (!isSynced) {
        needsSync = true;
        reason = 'device time not synced with server';
      } else {
        // Check drift (difference between phone and device time)
        final driftMs = (phoneTimestamp - deviceTimestamp).abs();
        final driftSeconds = driftMs / 1000;

        if (driftSeconds > 2.0) {
          needsSync = true;
          reason = 'time drift ${driftSeconds.toStringAsFixed(1)}s (> 2s threshold)';
        }
      }

      if (needsSync) {
        AppConfig.offlineLog('Device time sync needed: $reason');
        AppConfig.offlineLog('  Device: ${DateTime.fromMillisecondsSinceEpoch(deviceTimestamp)}');
        AppConfig.offlineLog('  Phone:  ${DateTime.fromMillisecondsSinceEpoch(phoneTimestamp)}');

        // Sync device time from phone
        await _syncDeviceTimestamp(deviceId, localIp, phoneTimestamp);
        return true;
      } else {
        AppConfig.offlineLog('Device time is accurate (drift < 2s)');
        return false;
      }
    } catch (e) {
      AppConfig.offlineLog('Error checking device time: $e');
      return false;
    }
  }

  /// Sync device timestamp from phone
  Future<void> _syncDeviceTimestamp(String deviceId, String localIp, int timestampMs) async {
    if (_localDio == null) throw Exception('Service not initialized');

    try {
      final url = 'http://$localIp/$deviceId/timestamp';
      final response = await _localDio!.post(
        url,
        data: jsonEncode({'timestamp': timestampMs}),
        options: Options(
          headers: {'Content-Type': 'application/json'},
        ),
      );

      if (response.statusCode == 200) {
        final data = response.data as Map<String, dynamic>;
        final success = data['success'] as bool? ?? false;

        if (success) {
          AppConfig.offlineLog('Device time synced successfully to: ${DateTime.fromMillisecondsSinceEpoch(timestampMs)}');
        } else {
          AppConfig.offlineLog('Device time sync failed: ${data['error']}');
          throw ApiException(message: 'Failed to sync device time');
        }
      } else {
        AppConfig.offlineLog('Device time sync failed: ${response.statusCode}');
        throw ApiException(message: 'Failed to sync device time: ${response.statusCode}');
      }
    } catch (e) {
      AppConfig.offlineLog('Error syncing device time: $e');
      rethrow;
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
