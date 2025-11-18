import 'package:equatable/equatable.dart';

/// Represents a single telemetry data field
class TelemetryData extends Equatable {
  final String key;
  final String type; // 'number', 'string', 'boolean'
  final dynamic value;
  final DateTime? timestamp;

  const TelemetryData({
    required this.key,
    required this.type,
    required this.value,
    this.timestamp,
  });

  /// Create from JSON
  factory TelemetryData.fromJson(String key, Map<String, dynamic> json) {
    return TelemetryData(
      key: key,
      type: json['type'] as String,
      value: json['value'],
      timestamp: json['timestamp'] != null
          ? DateTime.parse(json['timestamp'] as String)
          : null,
    );
  }

  /// Convert to JSON
  Map<String, dynamic> toJson() {
    return {
      'type': type,
      'value': value,
      if (timestamp != null) 'timestamp': timestamp!.toIso8601String(),
    };
  }

  /// Get value as number (with fallback)
  double get numberValue {
    if (value is num) return (value as num).toDouble();
    return 0.0;
  }

  /// Get value as string
  String get stringValue => value?.toString() ?? '';

  /// Get value as boolean
  bool get boolValue => value is bool ? value as bool : false;

  /// Get formatted display value based on type
  String get displayValue {
    switch (type) {
      case 'number':
        return numberValue.toStringAsFixed(2);
      case 'boolean':
        return boolValue ? 'ON' : 'OFF';
      default:
        return stringValue;
    }
  }

  @override
  List<Object?> get props => [key, type, value, timestamp];

  @override
  String toString() =>
      'TelemetryData(key: $key, type: $type, value: $value, timestamp: $timestamp)';
}
