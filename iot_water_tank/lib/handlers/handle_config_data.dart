import '../models/sync_types.dart';
import '../utils/sync_merge.dart';
import '../config/app_config.dart';

/// ============================================================================
/// CONFIG DATA HANDLER (Phone App - 2-way sync)
/// ============================================================================
/// Manages device configuration with 2-way merge
/// Uses mode-based merging (API or Local)

class ConfigDataHandler {
  // Config data fields with 2-way sync
  final SyncDouble upperThreshold = SyncDouble();
  final SyncDouble lowerThreshold = SyncDouble();
  final SyncDouble tankHeight = SyncDouble();
  final SyncDouble tankWidth = SyncDouble();
  final SyncString tankShape = SyncString();
  final SyncDouble usedTotal = SyncDouble();
  final SyncDouble maxInflow = SyncDouble();
  final SyncBool forceUpdate = SyncBool();
  final SyncBool sensorFilter = SyncBool();
  final SyncString ipAddress = SyncString();

  /// Initialize with default values
  void begin() {
    upperThreshold.value = 95.0;
    lowerThreshold.value = 20.0;
    tankHeight.value = 100.0;
    tankWidth.value = 50.0;
    tankShape.value = 'Cylindrical';
    usedTotal.value = 0.0;
    maxInflow.value = 0.0;
    forceUpdate.value = false;
    sensorFilter.value = true;
    ipAddress.value = '';

    AppConfig.deviceLog('[ConfigHandler] Initialized with defaults');
  }

  /// Update from API source (from cloud server)
  void updateFromAPI({
    required double apiUpperThreshold,
    required int apiUpperThresholdTs,
    required double apiLowerThreshold,
    required int apiLowerThresholdTs,
    required double apiTankHeight,
    required int apiTankHeightTs,
    required double apiTankWidth,
    required int apiTankWidthTs,
    required String apiTankShape,
    required int apiTankShapeTs,
    required double apiUsedTotal,
    required int apiUsedTotalTs,
    required double apiMaxInflow,
    required int apiMaxInflowTs,
    required bool apiForceUpdate,
    required int apiForceUpdateTs,
    required bool apiSensorFilter,
    required int apiSensorFilterTs,
    required String apiIpAddress,
    required int apiIpAddressTs,
  }) {
    upperThreshold.apiValue = apiUpperThreshold;
    upperThreshold.apiLastModified = apiUpperThresholdTs;

    lowerThreshold.apiValue = apiLowerThreshold;
    lowerThreshold.apiLastModified = apiLowerThresholdTs;

    tankHeight.apiValue = apiTankHeight;
    tankHeight.apiLastModified = apiTankHeightTs;

    tankWidth.apiValue = apiTankWidth;
    tankWidth.apiLastModified = apiTankWidthTs;

    tankShape.apiValue = apiTankShape;
    tankShape.apiLastModified = apiTankShapeTs;

    usedTotal.apiValue = apiUsedTotal;
    usedTotal.apiLastModified = apiUsedTotalTs;

    maxInflow.apiValue = apiMaxInflow;
    maxInflow.apiLastModified = apiMaxInflowTs;

    forceUpdate.apiValue = apiForceUpdate;
    forceUpdate.apiLastModified = apiForceUpdateTs;

    sensorFilter.apiValue = apiSensorFilter;
    sensorFilter.apiLastModified = apiSensorFilterTs;

    ipAddress.apiValue = apiIpAddress;
    ipAddress.apiLastModified = apiIpAddressTs;

    AppConfig.deviceLog('[ConfigHandler] Updated from API');
  }

  /// Update from Local source (from device webserver)
  void updateFromLocal({
    required double localUpperThreshold,
    required int localUpperThresholdTs,
    required double localLowerThreshold,
    required int localLowerThresholdTs,
    required double localTankHeight,
    required int localTankHeightTs,
    required double localTankWidth,
    required int localTankWidthTs,
    required String localTankShape,
    required int localTankShapeTs,
    required double localUsedTotal,
    required int localUsedTotalTs,
    required double localMaxInflow,
    required int localMaxInflowTs,
    required bool localForceUpdate,
    required int localForceUpdateTs,
    required bool localSensorFilter,
    required int localSensorFilterTs,
    required String localIpAddress,
    required int localIpAddressTs,
  }) {
    upperThreshold.localValue = localUpperThreshold;
    upperThreshold.localLastModified = localUpperThresholdTs;

    lowerThreshold.localValue = localLowerThreshold;
    lowerThreshold.localLastModified = localLowerThresholdTs;

    tankHeight.localValue = localTankHeight;
    tankHeight.localLastModified = localTankHeightTs;

    tankWidth.localValue = localTankWidth;
    tankWidth.localLastModified = localTankWidthTs;

    tankShape.localValue = localTankShape;
    tankShape.localLastModified = localTankShapeTs;

    usedTotal.localValue = localUsedTotal;
    usedTotal.localLastModified = localUsedTotalTs;

    maxInflow.localValue = localMaxInflow;
    maxInflow.localLastModified = localMaxInflowTs;

    forceUpdate.localValue = localForceUpdate;
    forceUpdate.localLastModified = localForceUpdateTs;

    sensorFilter.localValue = localSensorFilter;
    sensorFilter.localLastModified = localSensorFilterTs;

    ipAddress.localValue = localIpAddress;
    ipAddress.localLastModified = localIpAddressTs;

    AppConfig.deviceLog('[ConfigHandler] Updated from Local');
  }

  /// Perform 2-way merge based on current mode
  /// Returns true if any value changed
  bool merge(SyncMode mode) {
    AppConfig.deviceLog('[ConfigHandler] Starting 2-way merge (mode: ${mode.name})...');

    bool changed = false;
    changed |= SyncMerge.mergeDouble(upperThreshold, mode);
    changed |= SyncMerge.mergeDouble(lowerThreshold, mode);
    changed |= SyncMerge.mergeDouble(tankHeight, mode);
    changed |= SyncMerge.mergeDouble(tankWidth, mode);
    changed |= SyncMerge.mergeString(tankShape, mode);
    changed |= SyncMerge.mergeDouble(usedTotal, mode);
    changed |= SyncMerge.mergeDouble(maxInflow, mode);
    changed |= SyncMerge.mergeBool(forceUpdate, mode);
    changed |= SyncMerge.mergeBool(sensorFilter, mode);
    changed |= SyncMerge.mergeString(ipAddress, mode);

    if (changed) {
      AppConfig.deviceLog('[ConfigHandler] Config values changed after merge');
    }

    return changed;
  }

  /// Get current values (after merge)
  double getUpperThreshold() => upperThreshold.value;
  double getLowerThreshold() => lowerThreshold.value;
  double getTankHeight() => tankHeight.value;
  double getTankWidth() => tankWidth.value;
  String getTankShape() => tankShape.value;
  double getUsedTotal() => usedTotal.value;
  double getMaxInflow() => maxInflow.value;
  bool getForceUpdate() => forceUpdate.value;
  bool getSensorFilter() => sensorFilter.value;
  String getIpAddress() => ipAddress.value;

  /// Set all values with priority flag for uploading
  void setAllPriority() {
    upperThreshold.lastModified = 0;
    lowerThreshold.lastModified = 0;
    tankHeight.lastModified = 0;
    tankWidth.lastModified = 0;
    tankShape.lastModified = 0;
    usedTotal.lastModified = 0;
    maxInflow.lastModified = 0;
    forceUpdate.lastModified = 0;
    sensorFilter.lastModified = 0;
    ipAddress.lastModified = 0;

    AppConfig.deviceLog('[ConfigHandler] Set all config fields with priority flag');
  }

  /// Debug: Print current state
  void printState() {
    print('[ConfigHandler] Current State:');
    print('  upperThreshold: ${upperThreshold.value} (API: ${upperThreshold.apiValue}, Local: ${upperThreshold.localValue})');
    print('  lowerThreshold: ${lowerThreshold.value} (API: ${lowerThreshold.apiValue}, Local: ${lowerThreshold.localValue})');
    print('  tankHeight: ${tankHeight.value} (API: ${tankHeight.apiValue}, Local: ${tankHeight.localValue})');
    print('  tankWidth: ${tankWidth.value} (API: ${tankWidth.apiValue}, Local: ${tankWidth.localValue})');
  }
}
