import '../config/app_config.dart';

/// ============================================================================
/// TELEMETRY DATA HANDLER (Phone App - Read-only)
/// ============================================================================
/// Manages sensor telemetry data from device
/// No sync needed - device is the authoritative source
/// App only reads and displays these values

class TelemetryDataHandler {
  // Telemetry data fields (device produces, app consumes)
  double waterLevel = 0.0;    // Current water level percentage (0-100)
  double distance = 0.0;       // Distance from sensor to water surface (cm)
  double currInflow = 0.0;     // Current inflow rate (L/min)
  double pumpStatus = 0.0;     // Pump status (0=off, 1=on)
  double isOnline = 0.0;       // Online status (0=offline, 1=online)
  int timestamp = 0;           // Last update timestamp

  /// Initialize with default values
  void begin() {
    waterLevel = 0.0;
    distance = 0.0;
    currInflow = 0.0;
    pumpStatus = 0.0;
    isOnline = 0.0;
    timestamp = 0;

    AppConfig.deviceLog('[TelemetryHandler] Initialized');
  }

  /// Update telemetry values (from device response)
  void update({
    required double waterLevel,
    required double distance,
    required double currInflow,
    required double pumpStatus,
    required double isOnline,
  }) {
    this.waterLevel = waterLevel;
    this.distance = distance;
    this.currInflow = currInflow;
    this.pumpStatus = pumpStatus;
    this.isOnline = isOnline;
    timestamp = DateTime.now().millisecondsSinceEpoch;

    AppConfig.deviceLog('[TelemetryHandler] Updated');
    AppConfig.deviceLog('  waterLevel: ${waterLevel.toStringAsFixed(2)}%');
    AppConfig.deviceLog('  distance: ${distance.toStringAsFixed(2)} cm');
    AppConfig.deviceLog('  currInflow: ${currInflow.toStringAsFixed(2)} L/min');
    AppConfig.deviceLog('  pumpStatus: ${pumpStatus.toInt()}');
  }

  /// Get values
  double getWaterLevel() => waterLevel;
  double getDistance() => distance;
  double getCurrInflow() => currInflow;
  double getPumpStatus() => pumpStatus;
  double getIsOnline() => isOnline;
  int getTimestamp() => timestamp;

  /// Check if pump is on
  bool isPumpOn() => pumpStatus > 0;

  /// Check if device is online
  bool isDeviceOnline() => isOnline > 0;

  /// Debug: Print current state
  void printState() {
    print('[TelemetryHandler] Current State:');
    print('  Water Level: ${waterLevel.toStringAsFixed(2)}%');
    print('  Distance: ${distance.toStringAsFixed(2)} cm');
    print('  Inflow: ${currInflow.toStringAsFixed(2)} L/min');
    print('  Pump: ${isPumpOn() ? "ON" : "OFF"}');
    print('  Online: ${isDeviceOnline() ? "YES" : "NO"}');
    print('  Timestamp: $timestamp ms');
  }
}
