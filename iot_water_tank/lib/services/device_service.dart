import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import '../config/app_config.dart';
import '../models/device.dart';
import '../models/control_data.dart';
import '../models/device_config_parameter.dart';
import 'api_client.dart';
import 'offline_mode_service.dart';
import '../utils/api_exception.dart';

/// Service for managing devices and their control/telemetry data
class DeviceService {
  final ApiClient _apiClient = ApiClient();
  final OfflineModeService _offlineModeService = OfflineModeService();

  /// Initialize the device service
  Future<void> initialize() async {
    await _apiClient.initialize();
    await _offlineModeService.initialize();
  }

  /// Fetch all devices
  Future<List<Device>> getDevices({
    bool assignedOnly = true,
    String? projectId,
  }) async {
    // Check if offline mode is enabled - prevent server calls
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    if (isOffline) {
      AppConfig.offlineLog('DeviceService: Skipping getDevices server call in offline mode');
      throw ApiException(
        message: 'Cannot fetch devices in offline mode. Use DeviceProvider which loads offline devices.',
        statusCode: 0,
      );
    }

    try {
      final queryParams = <String, dynamic>{};
      if (assignedOnly) {
        queryParams['assignedOnly'] = 'true';
      }
      if (projectId != null && projectId.isNotEmpty) {
        queryParams['projectId'] = projectId;
      }

      final response = await _apiClient.get(
        AppConfig.devicesEndpoint,
        queryParameters: queryParams,
      );

      if (response.statusCode == 200) {
        final data = response.data;

        // Handle both {data: [...]} and direct array response
        List<dynamic> deviceList;
        if (data is Map && data.containsKey('data')) {
          deviceList = data['data'] as List<dynamic>;
        } else if (data is List) {
          deviceList = data;
        } else {
          throw ApiException(
            message: 'Unexpected response format',
            statusCode: response.statusCode,
            data: data,
          );
        }

        return deviceList
            .map((json) => Device.fromJson(json as Map<String, dynamic>))
            .toList();
      }

      throw ApiException(
        message: 'Failed to fetch devices',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Get a single device by ID
  Future<Device> getDevice(String deviceId) async {
    // Check if offline mode is enabled - prevent server calls
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    if (isOffline) {
      AppConfig.offlineLog('DeviceService: Skipping getDevice server call in offline mode');
      throw ApiException(
        message: 'Cannot fetch device in offline mode. Use OfflineDeviceService instead.',
        statusCode: 0,
      );
    }

    try {
      final response = await _apiClient.get('${AppConfig.devicesEndpoint}/$deviceId');

      if (response.statusCode == 200) {
        final data = response.data;

        // Handle both {data: {...}} and direct object response
        Map<String, dynamic> deviceData;
        if (data is Map && data.containsKey('data')) {
          deviceData = data['data'] as Map<String, dynamic>;
        } else if (data is Map<String, dynamic>) {
          deviceData = data;
        } else {
          throw ApiException(
            message: 'Unexpected response format',
            statusCode: response.statusCode,
            data: data,
          );
        }

        final device = Device.fromJson(deviceData);

        // Cache device config for offline use
        await _cacheDeviceConfig(deviceId, device.deviceConfig);

        return device;
      }

      throw ApiException(
        message: 'Failed to fetch device',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Update device control data
  Future<Device> updateControlData(
    String deviceId,
    Map<String, ControlData> controlData,
  ) async {
    try {
      // Convert control data to API format
      final payload = {
        'controlData': controlData.map(
          (key, value) => MapEntry(
            key,
            value.toJson(),
          ),
        ),
      };

      final response = await _apiClient.patch(
        '${AppConfig.devicesEndpoint}/$deviceId',
        data: payload,
      );

      if (response.statusCode == 200) {
        final data = response.data;

        // Handle both {data: {...}} and direct object response
        Map<String, dynamic> deviceData;
        if (data is Map && data.containsKey('data')) {
          deviceData = data['data'] as Map<String, dynamic>;
        } else if (data is Map<String, dynamic>) {
          deviceData = data;
        } else {
          throw ApiException(
            message: 'Unexpected response format',
            statusCode: response.statusCode,
            data: data,
          );
        }

        return Device.fromJson(deviceData);
      }

      throw ApiException(
        message: 'Failed to update control data',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Update a single control value
  Future<Device> updateSingleControl(
    String deviceId,
    String controlKey,
    dynamic value,
    String type,
  ) async {
    try {
      final controlData = {
        controlKey: ControlData(
          key: controlKey,
          type: type,
          value: value,
        ),
      };

      return await updateControlData(deviceId, controlData);
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Get unique project IDs from devices list (for filtering)
  Set<String> getProjectIds(List<Device> devices) {
    return devices
        .where((device) => device.projectId != null)
        .map((device) => device.projectId!)
        .toSet();
  }

  /// Get unique project names from devices list (for filtering)
  Map<String, String> getProjects(List<Device> devices) {
    final projects = <String, String>{};
    for (final device in devices) {
      if (device.projectId != null && device.projectName != null) {
        projects[device.projectId!] = device.projectName!;
      }
    }
    return projects;
  }

  /// Claim a device by Device ID
  Future<Map<String, dynamic>> claimDevice(String deviceId) async {
    try {
      final response = await _apiClient.post(
        '${AppConfig.devicesEndpoint}/claim',
        data: {'deviceId': deviceId.trim()},
      );

      if (response.statusCode == 200) {
        return response.data as Map<String, dynamic>;
      }

      throw ApiException(
        message: 'Failed to claim device',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Unclaim a device (remove from account)
  Future<Map<String, dynamic>> unclaimDevice(String deviceId) async {
    try {
      final response = await _apiClient.post(
        '${AppConfig.devicesEndpoint}/unclaim',
        data: {'deviceId': deviceId.trim()},
      );

      if (response.statusCode == 200) {
        return response.data as Map<String, dynamic>;
      }

      throw ApiException(
        message: 'Failed to remove device',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Update device configuration and set config_update flag
  Future<Device> updateDeviceConfig(
    String deviceId,
    Map<String, DeviceConfigParameter> deviceConfig,
  ) async {
    // Check if offline mode is enabled - prevent server calls
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    if (isOffline) {
      AppConfig.offlineLog('DeviceService: Cannot update device config in offline mode. Use OfflineDeviceService instead.');
      throw ApiException(
        message: 'Cannot update device config in offline mode. Use local device endpoint.',
        statusCode: 0,
      );
    }

    try {
      // Prepare the config_update flag with current timestamp
      final configUpdateFlag = ControlData(
        key: 'config_update',
        label: 'Configuration Update',
        type: 'boolean',
        value: true,
        defaultValue: true,
        lastModified: DateTime.now().millisecondsSinceEpoch,
        system: true,
      );

      // Prepare payload with both deviceConfig and controlData
      final payload = {
        'deviceConfig': deviceConfig.map(
          (key, value) => MapEntry(key, value.toJson()),
        ),
        'controlData': {
          'config_update': configUpdateFlag.toJson(),
        },
      };

      final response = await _apiClient.patch(
        '${AppConfig.devicesEndpoint}/$deviceId',
        data: payload,
      );

      if (response.statusCode == 200) {
        final data = response.data;

        // Handle both {data: {...}} and direct object response
        Map<String, dynamic> deviceData;
        if (data is Map && data.containsKey('data')) {
          deviceData = data['data'] as Map<String, dynamic>;
        } else if (data is Map<String, dynamic>) {
          deviceData = data;
        } else {
          throw ApiException(
            message: 'Unexpected response format',
            statusCode: response.statusCode,
            data: data,
          );
        }

        return Device.fromJson(deviceData);
      }

      throw ApiException(
        message: 'Failed to update device configuration',
        statusCode: response.statusCode,
      );
    } catch (e) {
      if (e is ApiException) rethrow;
      throw ApiException.fromError(e);
    }
  }

  /// Cache device config for offline use
  Future<void> _cacheDeviceConfig(String deviceId, Map<String, DeviceConfigParameter> config) async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final cacheKey = 'device_config_$deviceId';
      final configJson = config.map((key, value) => MapEntry(key, value.toJson()));
      final jsonString = jsonEncode(configJson);
      await prefs.setString(cacheKey, jsonString);
      AppConfig.offlineLog('Cached device config for $deviceId');
    } catch (e) {
      AppConfig.errorLog('Failed to cache device config', error: e);
    }
  }
}
