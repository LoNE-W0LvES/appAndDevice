import 'package:flutter/foundation.dart';
import '../models/timestamp_sync.dart';
import '../services/timestamp_service.dart';
import '../config/app_config.dart';

/// Provider for timestamp synchronization management
class TimestampProvider with ChangeNotifier {
  final TimestampService _timestampService = TimestampService();

  TimestampSyncResponse? _lastDeviceSync;
  ServerTimestampResponse? _lastServerSync;
  DateTime? _lastSyncTime;
  bool _isSyncing = false;
  String? _lastSyncSource; // 'server' or 'device'

  // Getters
  TimestampSyncResponse? get lastDeviceSync => _lastDeviceSync;
  ServerTimestampResponse? get lastServerSync => _lastServerSync;
  DateTime? get lastSyncTime => _lastSyncTime;
  bool get isSyncing => _isSyncing;
  String? get lastSyncSource => _lastSyncSource;

  /// Check if device is synced with server
  bool get isDeviceSynced => _lastDeviceSync?.isServerSynced ?? false;

  /// Get last known timestamp source
  String get timestampSource {
    if (_lastDeviceSync != null) {
      return _lastDeviceSync!.source;
    }
    return 'unknown';
  }

  /// Get drift in milliseconds
  int get drift => _lastDeviceSync?.drift ?? 0;

  /// Sync device timestamp
  /// This should be called when:
  /// 1. App starts
  /// 2. Device comes online
  /// 3. Server comes back online after being offline
  Future<bool> syncDevice(
    String deviceId, {
    String? localIp,
  }) async {
    if (_isSyncing) {
      AppConfig.deviceLog('Timestamp sync already in progress');
      return false;
    }

    _isSyncing = true;
    notifyListeners();

    try {
      // Get server timestamp
      final serverTimestamp = await _timestampService.getServerTimestamp(deviceId);
      if (serverTimestamp != null) {
        _lastServerSync = serverTimestamp;
        _lastSyncSource = 'server';
        _lastSyncTime = DateTime.now();
        AppConfig.deviceLog('Server timestamp synced: ${serverTimestamp.serverTime}');
      }

      // Sync with device if local IP available
      if (localIp != null && localIp.isNotEmpty) {
        final deviceSync = await _timestampService.syncDeviceTimestamp(
          deviceId,
          localIp: localIp,
        );

        if (deviceSync != null) {
          _lastDeviceSync = deviceSync;
          _lastSyncSource = 'device';
          _lastSyncTime = DateTime.now();

          AppConfig.deviceLog(
            'Device timestamp synced: ${deviceSync.timestamp}, '
            'source: ${deviceSync.source}, drift: ${deviceSync.drift}ms',
          );

          _isSyncing = false;
          notifyListeners();
          return true;
        }
      }

      _isSyncing = false;
      notifyListeners();
      return serverTimestamp != null;
    } catch (e) {
      AppConfig.deviceLog('Error syncing timestamp: $e');
      _isSyncing = false;
      notifyListeners();
      return false;
    }
  }

  /// Get current timestamp with fallback strategy
  Future<int> getCurrentTimestamp(
    String deviceId, {
    String? localIp,
  }) async {
    return await _timestampService.getCurrentTimestamp(
      deviceId,
      localIp: localIp,
    );
  }

  /// Calculate drift between server and device
  int calculateDrift({
    required int serverTimestamp,
    required int deviceTimestamp,
  }) {
    return _timestampService.calculateDrift(
      serverTimestamp: serverTimestamp,
      deviceTimestamp: deviceTimestamp,
    );
  }

  /// Clear sync state
  void clearSyncState() {
    _lastDeviceSync = null;
    _lastServerSync = null;
    _lastSyncTime = null;
    _lastSyncSource = null;
    notifyListeners();
  }

  /// Check if sync is needed (every hour)
  bool needsSync() {
    if (_lastSyncTime == null) return true;

    final timeSinceSync = DateTime.now().difference(_lastSyncTime!);
    return timeSinceSync.inHours >= 1;
  }
}
