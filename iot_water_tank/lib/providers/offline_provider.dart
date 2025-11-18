import 'dart:async';
import 'package:flutter/material.dart';
import 'package:connectivity_plus/connectivity_plus.dart';
import '../services/offline_device_service.dart';

/// Provider for managing offline functionality and sync
class OfflineProvider extends ChangeNotifier {
  final OfflineDeviceService _offlineService = OfflineDeviceService();
  final Connectivity _connectivity = Connectivity();

  bool _isOnline = true;
  bool _isSyncing = false;
  int _pendingChanges = 0;
  SyncResult? _lastSyncResult;
  StreamSubscription<ConnectivityResult>? _connectivitySubscription;

  bool get isOnline => _isOnline;
  bool get isSyncing => _isSyncing;
  int get pendingChanges => _pendingChanges;
  SyncResult? get lastSyncResult => _lastSyncResult;

  Future<void> initialize() async {
    await _offlineService.initialize();
    await _checkConnectivity();
    await _updatePendingChanges();

    // Listen for connectivity changes
    _connectivitySubscription = _connectivity.onConnectivityChanged.listen((result) {
      _handleConnectivityChange(result);
    });
  }

  @override
  void dispose() {
    _connectivitySubscription?.cancel();
    super.dispose();
  }

  Future<void> _checkConnectivity() async {
    final result = await _connectivity.checkConnectivity();
    _isOnline = result == ConnectivityResult.wifi ||
                result == ConnectivityResult.mobile;
    notifyListeners();
  }

  void _handleConnectivityChange(ConnectivityResult result) async {
    final wasOnline = _isOnline;
    _isOnline = result == ConnectivityResult.wifi ||
                result == ConnectivityResult.mobile;

    notifyListeners();

    // Auto-sync when coming back online
    if (!wasOnline && _isOnline && _pendingChanges > 0) {
      await syncPendingChanges();
    }
  }

  Future<void> _updatePendingChanges() async {
    _pendingChanges = await _offlineService.getPendingChangesCount();
    notifyListeners();
  }

  /// Sync pending changes to server
  Future<void> syncPendingChanges() async {
    if (_isSyncing) return;

    _isSyncing = true;
    notifyListeners();

    try {
      _lastSyncResult = await _offlineService.syncPendingChanges();
      await _updatePendingChanges();
    } finally {
      _isSyncing = false;
      notifyListeners();
    }
  }

  /// Increment pending changes counter
  void incrementPendingChanges() {
    _pendingChanges++;
    notifyListeners();
  }

  /// Clear all offline data
  Future<void> clearOfflineData() async {
    await _offlineService.clearAllCache();
    _pendingChanges = 0;
    _lastSyncResult = null;
    notifyListeners();
  }

  OfflineDeviceService get service => _offlineService;
}
