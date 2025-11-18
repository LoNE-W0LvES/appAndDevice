import 'package:dio/dio.dart';
import '../models/timestamp_sync.dart';
import '../config/app_config.dart';
import 'api_client.dart';
import 'offline_mode_service.dart';

/// Service for timestamp synchronization with server and device
///
/// Three-Tier Timestamp Hierarchy:
/// 1. Server Time (Best) - Authoritative Unix timestamp from server
/// 2. App System Time (Good) - Phone's clock synced via cellular/WiFi
/// 3. Device millis() (Fallback) - ESP32's internal counter (can drift)
///
/// Synchronization Flow:
/// 1. App tries to get server timestamp (GET /api/timeSync?deviceId={id})
/// 2. If server unavailable, app uses phone's system time (DateTime.now())
/// 3. App gets device timestamp (GET http://{ip}/{id}/timestamp)
/// 4. App checks drift - if > 5 seconds, sends correction
/// 5. App → Device: Send corrected timestamp (POST http://{ip}/{id}/timestamp)
/// 6. Device updates internal clock and syncs with server when available
/// 7. App → Device: Verify correction (GET http://{ip}/{id}/timestamp)
///
/// This ensures device time stays accurate even without internet, using
/// the phone's cellular-synced clock as a reliable fallback.
class TimestampService {
  static const Duration _timeout = Duration(seconds: 10);  // Increased for ESP32 processing time
  static const int _maxDriftMs = 5000; // 5 seconds max acceptable drift (user requirement: 4-5 seconds)
  final ApiClient _apiClient = ApiClient();
  final OfflineModeService _offlineModeService = OfflineModeService();

  /// Get current timestamp from server
  /// GET /api/timeSync?deviceId={deviceId}
  /// Returns server timestamp in milliseconds
  Future<ServerTimestampResponse?> getServerTimestamp(String deviceId) async {
    // Skip server call in offline mode
    final isOffline = await _offlineModeService.isOfflineModeEnabled();
    if (isOffline) {
      print('[TimestampService] Offline mode: Skipping server timestamp request');
      AppConfig.deviceLog('Offline mode: Skipping server timestamp request');
      return null;
    }

    try {
      final response = await _apiClient.get(
        '/api/timeSync',
        queryParameters: {'deviceId': deviceId},
        options: Options(
          sendTimeout: _timeout,
          receiveTimeout: _timeout,
        ),
      );

      if (response.statusCode == 200 && response.data != null) {
        print('[TimestampService] Server timestamp response:');
        print('[TimestampService] Status Code: ${response.statusCode}');
        print('[TimestampService] Response Data: ${response.data}');
        return ServerTimestampResponse.fromJson(response.data);
      }
      print('[TimestampService] No valid response: statusCode=${response.statusCode}, hasData=${response.data != null}');
      return null;
    } catch (e) {
      print('[TimestampService] Error getting server timestamp: $e');
      AppConfig.deviceLog('Failed to get server timestamp: $e');
      return null;
    }
  }

  /// Get timestamp from device via local IP
  /// GET http://{device_ip}/{device_id}/timestamp
  Future<TimestampSyncResponse?> getDeviceTimestamp(
    String deviceId, {
    String? localIp,
  }) async {
    if (localIp == null || localIp.isEmpty) {
      AppConfig.deviceLog('Device local IP not available for timestamp sync');
      return null;
    }

    try {
      final dio = Dio(BaseOptions(
        baseUrl: 'http://$localIp',
        connectTimeout: _timeout,
        receiveTimeout: _timeout,
        sendTimeout: _timeout,
      ));

      final response = await dio.get('/$deviceId/timestamp');

      print('[TimestampService] Device timestamp response from $localIp:');
      print('[TimestampService] Status Code: ${response.statusCode}');
      print('[TimestampService] Response Data: ${response.data}');

      if (response.statusCode == 200 && response.data != null) {
        return TimestampSyncResponse.fromJson(response.data);
      }
      return null;
    } catch (e) {
      print('[TimestampService] Error getting device timestamp from $localIp: $e');
      AppConfig.deviceLog('Failed to get device timestamp from $localIp: $e');
      return null;
    }
  }

  /// Send corrected timestamp to device via local IP
  /// POST http://{device_ip}/{device_id}/timestamp
  /// Body: { "timestamp": unixTimestampInMs }
  Future<bool> sendTimestampToDevice(
    String deviceId,
    int timestamp, {
    String? localIp,
  }) async {
    if (localIp == null || localIp.isEmpty) {
      AppConfig.deviceLog('Device local IP not available for sending timestamp');
      return false;
    }

    try {
      final dio = Dio(BaseOptions(
        baseUrl: 'http://$localIp',
        connectTimeout: _timeout,
        receiveTimeout: _timeout,
        sendTimeout: _timeout,
      ));

      print('[TimestampService] Sending timestamp to device at $localIp');
      print('[TimestampService] POST /$deviceId/timestamp');
      print('[TimestampService] Data: {"timestamp": $timestamp}');

      final response = await dio.post(
        '/$deviceId/timestamp',
        data: {'timestamp': timestamp},
      );

      print('[TimestampService] POST timestamp response:');
      print('[TimestampService] Status Code: ${response.statusCode}');
      print('[TimestampService] Response Data: ${response.data}');

      if (response.statusCode == 200) {
        AppConfig.deviceLog('Timestamp sent to device: $timestamp');
        return true;
      }
      return false;
    } catch (e) {
      print('[TimestampService] Error sending timestamp to device at $localIp: $e');
      AppConfig.deviceLog('Failed to send timestamp to device at $localIp: $e');
      return false;
    }
  }

  /// Sync timestamp with device and correct if drift is too large
  /// 1. Get server timestamp (or use app system time if server unavailable)
  /// 2. Get device timestamp
  /// 3. Check drift - if > maxDrift, send correction
  /// 4. Device will then sync with server or use corrected time
  /// Returns the timestamp sync response from device
  Future<TimestampSyncResponse?> syncDeviceTimestamp(
    String deviceId, {
    String? localIp,
  }) async {
    if (localIp == null || localIp.isEmpty) {
      AppConfig.deviceLog('Cannot sync device timestamp: No local IP');
      return null;
    }

    try {
      // Step 1: Get reference timestamp (server or app Unix time)
      int referenceTimestamp;
      String timestampSource;

      // In offline mode, always use phone's Unix time (cellular/WiFi synced)
      final isOffline = await _offlineModeService.isOfflineModeEnabled();

      if (isOffline) {
        // Offline mode: Use phone's Unix time directly
        referenceTimestamp = DateTime.now().millisecondsSinceEpoch;
        timestampSource = 'phone-unix-time';
        AppConfig.deviceLog(
          'Offline mode: Using phone Unix time: $referenceTimestamp',
        );
      } else {
        // Online mode: Try server first, then fallback to phone time
        final serverTimestamp = await getServerTimestamp(deviceId);

        if (serverTimestamp != null) {
          referenceTimestamp = serverTimestamp.serverTime;
          timestampSource = 'server';
          AppConfig.deviceLog('Server timestamp obtained: $referenceTimestamp');
        } else {
          // Fallback to app's system time (phone clock from cellular/WiFi)
          referenceTimestamp = DateTime.now().millisecondsSinceEpoch;
          timestampSource = 'phone-unix-time';
          AppConfig.deviceLog(
            'Server unavailable. Using phone Unix time: $referenceTimestamp',
          );
        }
      }

      // Step 2: Get device's current timestamp state
      final deviceTimestamp = await getDeviceTimestamp(
        deviceId,
        localIp: localIp,
      );

      if (deviceTimestamp != null) {
        AppConfig.deviceLog(
          'Device timestamp: ${deviceTimestamp.timestamp}, '
          'source: ${deviceTimestamp.source}, '
          'drift: ${deviceTimestamp.drift}ms',
        );

        // Step 3: Check if timestamp is zero or drift is too large
        bool needsCorrection = false;
        String correctionReason = '';

        // Check if device timestamp is zero
        if (deviceTimestamp.timestamp == 0) {
          needsCorrection = true;
          correctionReason = 'device timestamp is zero';
        } else {
          // Check drift and correct if needed
          final currentDrift = calculateDrift(
            serverTimestamp: referenceTimestamp,
            deviceTimestamp: deviceTimestamp.timestamp,
          );

          final driftAbs = currentDrift.abs();

          AppConfig.deviceLog(
            'Calculated drift against $timestampSource: ${currentDrift}ms (abs: ${driftAbs}ms)',
          );

          // If drift is too large, send corrected timestamp to device
          if (driftAbs > _maxDriftMs) {
            needsCorrection = true;
            correctionReason = 'drift ${(driftAbs / 1000).toStringAsFixed(1)}s exceeds ${(_maxDriftMs / 1000).toStringAsFixed(1)}s threshold';
            AppConfig.deviceLog(
              'Large drift detected! Device: ${deviceTimestamp.timestamp}ms, '
              '$timestampSource: ${referenceTimestamp}ms, '
              'Difference: ${currentDrift}ms (${(currentDrift / 1000).toStringAsFixed(1)}s)',
            );
          }
        }

        if (needsCorrection) {
          AppConfig.deviceLog(
            'Time sync needed: $correctionReason. '
            'Sending correction from $timestampSource to device...',
          );

          final corrected = await sendTimestampToDevice(
            deviceId,
            referenceTimestamp,
            localIp: localIp,
          );

          if (corrected) {
            AppConfig.deviceLog(
              'Timestamp correction sent ($timestampSource). '
              'Waiting for device to process...',
            );

            // Wait a bit for device to process the correction
            await Future.delayed(const Duration(seconds: 2));

            // Get updated timestamp from device after correction
            final updatedTimestamp = await getDeviceTimestamp(
              deviceId,
              localIp: localIp,
            );

            if (updatedTimestamp != null) {
              AppConfig.deviceLog(
                'Device timestamp after correction: ${updatedTimestamp.timestamp}, '
                'source: ${updatedTimestamp.source}, drift: ${updatedTimestamp.drift}ms',
              );
              return updatedTimestamp;
            }
          } else {
            AppConfig.deviceLog('Failed to send timestamp correction to device');
          }
        } else {
          AppConfig.deviceLog('Drift within acceptable range');
        }
      }

      return deviceTimestamp;
    } catch (e) {
      AppConfig.deviceLog('Error syncing device timestamp: $e');
      return null;
    }
  }

  /// Calculate time drift between server and device
  /// Returns drift in milliseconds (positive = device is ahead)
  int calculateDrift({
    required int serverTimestamp,
    required int deviceTimestamp,
  }) {
    return deviceTimestamp - serverTimestamp;
  }

  /// Get current timestamp with fallback strategy
  /// Priority: Server > Device Local > System time
  ///
  /// Note: System time is used as last resort to keep app running,
  /// but if both server and device are offline, the device won't be
  /// able to save data anyway. App will keep trying to reconnect.
  Future<int> getCurrentTimestamp(
    String deviceId, {
    String? localIp,
  }) async {
    // Try server first
    final serverTimestamp = await getServerTimestamp(deviceId);
    if (serverTimestamp != null) {
      return serverTimestamp.serverTime;
    }

    // Try device if local IP available
    if (localIp != null && localIp.isNotEmpty) {
      final deviceTimestamp = await getDeviceTimestamp(
        deviceId,
        localIp: localIp,
      );
      if (deviceTimestamp != null) {
        return deviceTimestamp.timestamp;
      }
    }

    // Fallback to system time (app keeps running until server/device reconnects)
    return DateTime.now().millisecondsSinceEpoch;
  }
}
