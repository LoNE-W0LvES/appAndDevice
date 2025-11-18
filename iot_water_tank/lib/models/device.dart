import 'package:equatable/equatable.dart';
import 'control_data.dart';
import 'telemetry_data.dart';
import 'device_config_parameter.dart';

/// Represents an IoT device
class Device extends Equatable {
  final String id;
  final String name;
  final String deviceId;
  final String? description;
  final String? projectId;
  final String? projectName;
  final Map<String, DeviceConfigParameter> deviceConfig;
  final Map<String, ControlData> controlData;
  final Map<String, TelemetryData> telemetryData;
  final bool isActive;
  final DateTime? lastSeen;
  final DateTime? createdAt;
  final DateTime? updatedAt;
  final String? localIp; // Local IP address for offline mode

  const Device({
    required this.id,
    required this.name,
    required this.deviceId,
    this.description,
    this.projectId,
    this.projectName,
    this.deviceConfig = const {},
    this.controlData = const {},
    this.telemetryData = const {},
    this.isActive = false,
    this.lastSeen,
    this.createdAt,
    this.updatedAt,
    this.localIp,
  });

  /// Create from JSON
  factory Device.fromJson(Map<String, dynamic> json) {
    // Parse projectId - handle both string and object format
    String? projectId;
    String? projectName;
    final projectIdValue = json['projectId'];
    if (projectIdValue != null) {
      if (projectIdValue is String) {
        projectId = projectIdValue;
        projectName = json['projectName'] as String?;
      } else if (projectIdValue is Map<String, dynamic>) {
        projectId = projectIdValue['projectId'] as String?;
        projectName = projectIdValue['name'] as String?;
      }
    }

    // Parse device config
    final Map<String, DeviceConfigParameter> config = {};
    final deviceConfigJson = json['deviceConfig'] as Map<String, dynamic>?;
    if (deviceConfigJson != null) {
      deviceConfigJson.forEach((key, value) {
        if (value is Map<String, dynamic>) {
          config[key] = DeviceConfigParameter.fromJson(key, value);
        }
      });
    }

    // Parse control data
    final Map<String, ControlData> controls = {};
    final controlDataJson = json['controlData'] as Map<String, dynamic>?;
    if (controlDataJson != null) {
      controlDataJson.forEach((key, value) {
        if (value is Map<String, dynamic>) {
          controls[key] = ControlData.fromJson(key, value);
        }
      });
    }

    // Parse telemetry data
    final Map<String, TelemetryData> telemetry = {};
    final telemetryDataJson = json['telemetryData'] as Map<String, dynamic>?;
    if (telemetryDataJson != null) {
      telemetryDataJson.forEach((key, value) {
        if (value is Map<String, dynamic>) {
          telemetry[key] = TelemetryData.fromJson(key, value);
        } else if (value != null) {
          // Handle raw values (just numbers or strings)
          // Create a TelemetryData object from the raw value
          final type = value is num ? 'number' : (value is bool ? 'boolean' : 'string');
          telemetry[key] = TelemetryData(
            key: key,
            type: type,
            value: value,
          );
        }
      });
    }

    return Device(
      id: json['id'] as String,
      name: json['name'] as String,
      deviceId: json['deviceId'] as String,
      description: json['description'] as String?,
      projectId: projectId,
      projectName: projectName,
      deviceConfig: config,
      controlData: controls,
      telemetryData: telemetry,
      isActive: json['isActive'] as bool? ?? false,
      lastSeen: json['lastSeen'] != null
          ? DateTime.parse(json['lastSeen'] as String)
          : null,
      createdAt: json['createdAt'] != null
          ? DateTime.parse(json['createdAt'] as String)
          : null,
      updatedAt: json['updatedAt'] != null
          ? DateTime.parse(json['updatedAt'] as String)
          : null,
      localIp: json['localIp'] as String?,
    );
  }

  /// Convert to JSON
  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'name': name,
      'deviceId': deviceId,
      if (description != null) 'description': description,
      if (projectId != null) 'projectId': projectId,
      if (projectName != null) 'projectName': projectName,
      'deviceConfig': deviceConfig.map(
        (key, value) => MapEntry(key, value.toJson()),
      ),
      'controlData': controlData.map(
        (key, value) => MapEntry(key, value.toJson()),
      ),
      'telemetryData': telemetry.map(
        (key, value) => MapEntry(key, value.toJson()),
      ),
      'isActive': isActive,
      if (lastSeen != null) 'lastSeen': lastSeen!.toIso8601String(),
      if (createdAt != null) 'createdAt': createdAt!.toIso8601String(),
      if (updatedAt != null) 'updatedAt': updatedAt!.toIso8601String(),
      if (localIp != null) 'localIp': localIp,
    };
  }

  /// Shorthand for telemetryData
  Map<String, TelemetryData> get telemetry => telemetryData;

  /// Get user-controllable control data (exclude system flags)
  Map<String, ControlData> getUserControls() {
    return Map.fromEntries(
      controlData.entries.where(
        (entry) => !['force_update', 'config_update'].contains(entry.key),
      ),
    );
  }

  /// Get system control flags
  Map<String, ControlData> getSystemControls() {
    return Map.fromEntries(
      controlData.entries.where(
        (entry) => ['force_update', 'config_update'].contains(entry.key),
      ),
    );
  }

  /// Get status color based on device state
  String getStatusColor() {
    if (!isActive) return 'grey';
    if (lastSeen == null) return 'grey';

    final now = DateTime.now();
    final difference = now.difference(lastSeen!).inSeconds;

    if (difference < 60) return 'green'; // Active in last minute
    if (difference < 300) return 'yellow'; // Active in last 5 minutes
    return 'red'; // Inactive
  }

  /// Get human-readable status
  String getStatusText() {
    if (!isActive) return 'Inactive';
    if (lastSeen == null) return 'Unknown';

    final now = DateTime.now();
    final difference = now.difference(lastSeen!);

    if (difference.inSeconds < 60) {
      return 'Active (${difference.inSeconds}s ago)';
    } else if (difference.inMinutes < 60) {
      return 'Active (${difference.inMinutes}m ago)';
    } else if (difference.inHours < 24) {
      return 'Active (${difference.inHours}h ago)';
    } else {
      return 'Active (${difference.inDays}d ago)';
    }
  }

  /// Get device online status from telemetry Status field
  bool get isOnline {
    final statusValue = telemetryData['Status']?.numberValue ?? 0.0;
    return statusValue > 0;
  }

  /// Get status value (1 = online, 0 = offline)
  int get statusValue {
    final statusValue = telemetryData['Status']?.numberValue ?? 0.0;
    return statusValue.toInt();
  }

  /// Get last seen text
  String get lastSeenText {
    if (lastSeen == null) return 'Never';

    final now = DateTime.now();
    final difference = now.difference(lastSeen!);

    if (difference.inSeconds < 60) return '${difference.inSeconds}s ago';
    if (difference.inMinutes < 60) return '${difference.inMinutes}m ago';
    if (difference.inHours < 24) return '${difference.inHours}h ago';
    return '${difference.inDays}d ago';
  }

  /// Create a copy with updated fields
  Device copyWith({
    String? id,
    String? name,
    String? deviceId,
    String? description,
    String? projectId,
    String? projectName,
    Map<String, DeviceConfigParameter>? deviceConfig,
    Map<String, ControlData>? controlData,
    Map<String, TelemetryData>? telemetryData,
    bool? isActive,
    DateTime? lastSeen,
    DateTime? createdAt,
    DateTime? updatedAt,
    String? localIp,
  }) {
    return Device(
      id: id ?? this.id,
      name: name ?? this.name,
      deviceId: deviceId ?? this.deviceId,
      description: description ?? this.description,
      projectId: projectId ?? this.projectId,
      projectName: projectName ?? this.projectName,
      deviceConfig: deviceConfig ?? this.deviceConfig,
      controlData: controlData ?? this.controlData,
      telemetryData: telemetryData ?? this.telemetryData,
      isActive: isActive ?? this.isActive,
      lastSeen: lastSeen ?? this.lastSeen,
      createdAt: createdAt ?? this.createdAt,
      updatedAt: updatedAt ?? this.updatedAt,
      localIp: localIp ?? this.localIp,
    );
  }

  @override
  List<Object?> get props => [
        id,
        name,
        deviceId,
        description,
        projectId,
        projectName,
        deviceConfig,
        controlData,
        telemetryData,
        isActive,
        lastSeen,
        createdAt,
        updatedAt,
        localIp,
      ];

  @override
  String toString() => 'Device(id: $id, name: $name, deviceId: $deviceId)';
}
