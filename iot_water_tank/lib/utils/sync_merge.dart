import '../models/sync_types.dart';

/// ============================================================================
/// 2-WAY MERGE LOGIC (Last-Write-Wins)
/// ============================================================================
/// Phone app uses either API or Local mode (not both)
/// Merge compares 2 sources: self vs (api OR local)
/// After merge: all 3 become equal for seamless mode switching

enum SyncMode {
  api,   // Using cloud server
  local, // Using device webserver
}

class SyncMerge {
  /// Merge boolean value (2-way for app)
  /// Returns true if value changed
  static bool mergeBool(SyncBool sync, SyncMode mode) {
    bool oldValue = sync.value;

    if (mode == SyncMode.api) {
      // Compare self vs API
      final winner = _findWinner(sync.apiLastModified, sync.lastModified);

      if (winner == 1) {
        // API wins - update Self to match API
        sync.value = sync.apiValue;
        sync.lastModified = sync.apiLastModified;
        // Don't update apiValue/localValue - they represent what was received
      } else {
        // Self wins - no update to apiValue/localValue
        // They get updated on next fetch from API/Local
      }
    } else {
      // Compare self vs Local
      final winner = _findWinner(sync.localLastModified, sync.lastModified);

      if (winner == 1) {
        // Local wins - update Self to match Local
        sync.value = sync.localValue;
        sync.lastModified = sync.localLastModified;
        // Don't update apiValue/localValue - they represent what was received
      } else {
        // Self wins - no update to apiValue/localValue
        // They get updated on next fetch from API/Local
      }
    }

    return sync.value != oldValue;
  }

  /// Merge double value (2-way for app)
  /// Returns true if value changed
  static bool mergeDouble(SyncDouble sync, SyncMode mode) {
    double oldValue = sync.value;

    if (mode == SyncMode.api) {
      // Compare self vs API
      final winner = _findWinner(sync.apiLastModified, sync.lastModified);

      if (winner == 1) {
        // API wins - update Self to match API
        sync.value = sync.apiValue;
        sync.lastModified = sync.apiLastModified;
        // Don't update apiValue/localValue - they represent what was received
      } else {
        // Self wins - no update to apiValue/localValue
        // They get updated on next fetch from API/Local
      }
    } else {
      // Compare self vs Local
      final winner = _findWinner(sync.localLastModified, sync.lastModified);

      if (winner == 1) {
        // Local wins - update Self to match Local
        sync.value = sync.localValue;
        sync.lastModified = sync.localLastModified;
        // Don't update apiValue/localValue - they represent what was received
      } else {
        // Self wins - no update to apiValue/localValue
        // They get updated on next fetch from API/Local
      }
    }

    return (sync.value - oldValue).abs() > 0.001;
  }

  /// Merge string value (2-way for app)
  /// Returns true if value changed
  static bool mergeString(SyncString sync, SyncMode mode) {
    String oldValue = sync.value;

    if (mode == SyncMode.api) {
      // Compare self vs API
      final winner = _findWinner(sync.apiLastModified, sync.lastModified);

      if (winner == 1) {
        // API wins - update Self to match API
        sync.value = sync.apiValue;
        sync.lastModified = sync.apiLastModified;
        // Don't update apiValue/localValue - they represent what was received
      } else {
        // Self wins - no update to apiValue/localValue
        // They get updated on next fetch from API/Local
      }
    } else {
      // Compare self vs Local
      final winner = _findWinner(sync.localLastModified, sync.lastModified);

      if (winner == 1) {
        // Local wins - update Self to match Local
        sync.value = sync.localValue;
        sync.lastModified = sync.localLastModified;
        // Don't update apiValue/localValue - they represent what was received
      } else {
        // Self wins - no update to apiValue/localValue
        // They get updated on next fetch from API/Local
      }
    }

    return sync.value != oldValue;
  }

  /// Find winner between 2 sources
  /// Returns: 1=source1 wins, 2=source2 wins
  static int _findWinner(int source1Timestamp, int source2Timestamp) {
    // Priority flag: timestamp = 0 means "force accept"
    if (source1Timestamp == 0 && source2Timestamp == 0) {
      return 1; // Both have priority, source1 wins
    }

    if (source1Timestamp == 0) {
      return 1; // Source1 has priority
    }

    if (source2Timestamp == 0) {
      return 2; // Source2 has priority
    }

    // Last-Write-Wins: newest timestamp wins
    return source1Timestamp > source2Timestamp ? 1 : 2;
  }
}
