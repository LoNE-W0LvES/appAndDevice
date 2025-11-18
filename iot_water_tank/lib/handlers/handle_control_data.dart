import '../models/sync_types.dart';
import '../utils/sync_merge.dart';
import '../config/app_config.dart';

/// ============================================================================
/// CONTROL DATA HANDLER (Phone App - 2-way sync)
/// ============================================================================
/// Manages pump switch and config update flags
/// Uses 2-way merge based on current mode (API or Local)

class ControlDataHandler {
  // Control data fields with 2-way sync
  final SyncBool pumpSwitch = SyncBool();
  final SyncBool configUpdate = SyncBool();

  /// Initialize with default values
  void begin() {
    pumpSwitch.value = false;
    configUpdate.value = true; // Default: fetch config from source

    AppConfig.deviceLog('[ControlHandler] Initialized');
  }

  /// Update from API source (from cloud server)
  void updateFromAPI({
    required bool apiPumpSwitch,
    required int apiPumpSwitchTs,
    required bool apiConfigUpdate,
    required int apiConfigUpdateTs,
  }) {
    pumpSwitch.apiValue = apiPumpSwitch;
    pumpSwitch.apiLastModified = apiPumpSwitchTs;

    configUpdate.apiValue = apiConfigUpdate;
    configUpdate.apiLastModified = apiConfigUpdateTs;

    AppConfig.deviceLog('[ControlHandler] Updated from API');
  }

  /// Update from Local source (from device webserver)
  void updateFromLocal({
    required bool localPumpSwitch,
    required int localPumpSwitchTs,
    required bool localConfigUpdate,
    required int localConfigUpdateTs,
  }) {
    pumpSwitch.localValue = localPumpSwitch;
    pumpSwitch.localLastModified = localPumpSwitchTs;

    configUpdate.localValue = localConfigUpdate;
    configUpdate.localLastModified = localConfigUpdateTs;

    AppConfig.deviceLog('[ControlHandler] Updated from Local');
  }

  /// Update self (app changes its own value)
  void updateSelf({
    required bool selfPumpSwitch,
    required bool selfConfigUpdate,
  }) {
    final now = DateTime.now().millisecondsSinceEpoch;

    pumpSwitch.value = selfPumpSwitch;
    pumpSwitch.lastModified = now;

    configUpdate.value = selfConfigUpdate;
    configUpdate.lastModified = now;

    AppConfig.deviceLog('[ControlHandler] Updated self');
  }

  /// Perform 2-way merge based on current mode
  /// Returns true if any value changed
  bool merge(SyncMode mode) {
    AppConfig.deviceLog('[ControlHandler] Starting 2-way merge (mode: ${mode.name})...');

    final pumpChanged = SyncMerge.mergeBool(pumpSwitch, mode);
    final configChanged = SyncMerge.mergeBool(configUpdate, mode);

    if (pumpChanged) {
      AppConfig.deviceLog('[ControlHandler] pumpSwitch changed to: ${pumpSwitch.value}');
    }

    if (configChanged) {
      AppConfig.deviceLog('[ControlHandler] configUpdate changed to: ${configUpdate.value}');
    }

    return pumpChanged || configChanged;
  }

  /// Get current values (after merge)
  bool getPumpSwitch() => pumpSwitch.value;
  bool getConfigUpdate() => configUpdate.value;

  /// Get timestamps
  int getPumpSwitchTimestamp() => pumpSwitch.lastModified;
  int getConfigUpdateTimestamp() => configUpdate.lastModified;

  /// Set priority flag (timestamp=0) for uploading
  void setPumpSwitchPriority(bool value) {
    pumpSwitch.value = value;
    pumpSwitch.lastModified = 0; // Priority flag
    AppConfig.deviceLog('[ControlHandler] Set pumpSwitch with priority: $value');
  }

  void setConfigUpdatePriority(bool value) {
    configUpdate.value = value;
    configUpdate.lastModified = 0; // Priority flag
    AppConfig.deviceLog('[ControlHandler] Set configUpdate with priority: $value');
  }

  /// Debug: Print current state
  void printState() {
    print('[ControlHandler] Current State:');
    print('  pumpSwitch:');
    print('    Self:  ${pumpSwitch.value} (ts: ${pumpSwitch.lastModified})');
    print('    API:   ${pumpSwitch.apiValue} (ts: ${pumpSwitch.apiLastModified})');
    print('    Local: ${pumpSwitch.localValue} (ts: ${pumpSwitch.localLastModified})');
    print('  configUpdate:');
    print('    Self:  ${configUpdate.value} (ts: ${configUpdate.lastModified})');
    print('    API:   ${configUpdate.apiValue} (ts: ${configUpdate.apiLastModified})');
    print('    Local: ${configUpdate.localValue} (ts: ${configUpdate.localLastModified})');
  }
}
