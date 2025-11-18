import 'package:equatable/equatable.dart';

/// Represents a single control data field
class ControlData extends Equatable {
  final String key;
  final String label;
  final String type; // 'boolean', 'number', 'string'
  final dynamic value;
  final dynamic defaultValue;
  final int? lastModified; // Unix timestamp in milliseconds
  final bool system; // True for system flags like config_update

  const ControlData({
    required this.key,
    required this.type,
    required this.value,
    String? label,
    dynamic defaultValue,
    this.lastModified,
    this.system = false,
  })  : label = label ?? key,
        defaultValue = defaultValue ?? value;

  /// Create from JSON
  factory ControlData.fromJson(String key, Map<String, dynamic> json) {
    // Parse lastModified - handle int, string (Unix timestamp), and ISO date string
    int? lastModified;
    final lastModifiedValue = json['lastModified'];
    if (lastModifiedValue != null) {
      if (lastModifiedValue is int) {
        lastModified = _validateTimestamp(lastModifiedValue);
      } else if (lastModifiedValue is String) {
        // Try parsing as Unix timestamp first
        final parsed = int.tryParse(lastModifiedValue);
        if (parsed != null) {
          lastModified = _validateTimestamp(parsed);
        } else {
          // Try parsing as ISO date string
          try {
            final dateTime = DateTime.parse(lastModifiedValue);
            lastModified = dateTime.millisecondsSinceEpoch;
          } catch (e) {
            // If parsing fails, leave as null
            lastModified = null;
          }
        }
      }
    }

    return ControlData(
      key: key,
      label: json['label'] as String? ?? key,
      type: json['type'] as String,
      value: json['value'],
      defaultValue: json['defaultValue'] ?? json['value'],
      lastModified: lastModified,
      system: json['system'] as bool? ?? false,
    );
  }

  /// Validate and fix timestamp (handle microseconds or invalid values)
  static int? _validateTimestamp(int timestamp) {
    // Valid range for DateTime in Dart: -8640000000000000 to 8640000000000000 ms
    const maxValidTimestamp = 8640000000000000;
    const minValidTimestamp = -8640000000000000;

    // If timestamp is way too large, it might be in microseconds - convert to milliseconds
    if (timestamp > maxValidTimestamp) {
      final converted = timestamp ~/ 1000; // Divide by 1000 (microseconds to milliseconds)
      if (converted >= minValidTimestamp && converted <= maxValidTimestamp) {
        print('[ControlData] Converted microsecond timestamp $timestamp to milliseconds: $converted');
        return converted;
      }
      // Still invalid after conversion - reject it
      print('[ControlData] Invalid timestamp $timestamp (out of range), using null');
      return null;
    }

    // If timestamp is negative and out of range
    if (timestamp < minValidTimestamp) {
      print('[ControlData] Invalid timestamp $timestamp (too small), using null');
      return null;
    }

    // Valid timestamp
    return timestamp;
  }

  /// Convert to JSON for API requests
  Map<String, dynamic> toJson() {
    return {
      'key': key,
      'label': label,
      'type': type,
      'value': value,
      'defaultValue': defaultValue,
      if (lastModified != null) 'lastModified': lastModified,
      if (system) 'system': system,
    };
  }

  /// Create a copy with updated value
  ControlData copyWith({
    String? key,
    String? label,
    String? type,
    dynamic value,
    dynamic defaultValue,
    int? lastModified,
    bool? system,
  }) {
    return ControlData(
      key: key ?? this.key,
      label: label ?? this.label,
      type: type ?? this.type,
      value: value ?? this.value,
      defaultValue: defaultValue ?? this.defaultValue,
      lastModified: lastModified ?? this.lastModified,
      system: system ?? this.system,
    );
  }

  /// Create a copy with new value and timestamp
  ControlData withNewValue(dynamic newValue) {
    return copyWith(
      value: newValue,
      defaultValue: newValue,
      lastModified: DateTime.now().millisecondsSinceEpoch,
    );
  }

  /// Get value as boolean (with fallback)
  bool get boolValue => value is bool ? value as bool : false;

  /// Get value as number (with fallback)
  double get numberValue {
    if (value is num) return (value as num).toDouble();
    return 0.0;
  }

  /// Get value as string
  String get stringValue => value?.toString() ?? '';

  @override
  List<Object?> get props => [key, label, type, value, defaultValue, lastModified, system];

  @override
  String toString() =>
      'ControlData(key: $key, type: $type, value: $value, lastModified: $lastModified)';
}
