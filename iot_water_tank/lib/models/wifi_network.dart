import 'package:equatable/equatable.dart';

/// WiFi network information from device scan
class WiFiNetwork extends Equatable {
  final String ssid;
  final int signal; // RSSI value (e.g., -45 to -90)
  final String auth; // Authentication type (WPA2, WPA, OPEN, etc.)

  const WiFiNetwork({
    required this.ssid,
    required this.signal,
    required this.auth,
  });

  /// Get signal strength as percentage (0-100)
  int get signalStrength {
    // RSSI typically ranges from -30 (excellent) to -90 (poor)
    // Convert to 0-100 scale
    if (signal >= -30) return 100;
    if (signal <= -90) return 0;
    return ((signal + 90) * 100 / 60).round();
  }

  /// Get signal quality text
  String get signalQuality {
    final strength = signalStrength;
    if (strength >= 75) return 'Excellent';
    if (strength >= 50) return 'Good';
    if (strength >= 25) return 'Fair';
    return 'Poor';
  }

  /// Get number of bars (1-4) for signal strength indicator
  int get bars {
    final strength = signalStrength;
    if (strength >= 75) return 4;
    if (strength >= 50) return 3;
    if (strength >= 25) return 2;
    return 1;
  }

  /// Check if network is secured
  bool get isSecured => auth.toUpperCase() != 'OPEN';

  /// Convert ESP32 encryption type integer to string
  static String _encryptionTypeToString(int encryption) {
    switch (encryption) {
      case 0:
        return 'OPEN';
      case 1:
        return 'WEP';
      case 2:
        return 'WPA';
      case 3:
        return 'WPA2';
      case 4:
        return 'WPA/WPA2';
      case 5:
        return 'WPA2-Enterprise';
      default:
        return 'UNKNOWN';
    }
  }

  factory WiFiNetwork.fromJson(Map<String, dynamic> json) {
    // Handle both formats: device format (rssi, encryption) and app format (signal, auth)
    final int signalValue = json['rssi'] as int? ?? json['signal'] as int? ?? -90;

    String authValue;
    if (json.containsKey('encryption')) {
      // Device format: encryption is an integer
      authValue = _encryptionTypeToString(json['encryption'] as int? ?? 0);
    } else {
      // App format: auth is already a string
      authValue = json['auth'] as String? ?? 'UNKNOWN';
    }

    return WiFiNetwork(
      ssid: json['ssid'] as String? ?? '',
      signal: signalValue,
      auth: authValue,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'ssid': ssid,
      'signal': signal,
      'auth': auth,
    };
  }

  @override
  List<Object?> get props => [ssid, signal, auth];
}

/// WiFi setup response from device
class WiFiSetupResponse extends Equatable {
  final bool success;
  final String message;

  const WiFiSetupResponse({
    required this.success,
    required this.message,
  });

  factory WiFiSetupResponse.fromJson(Map<String, dynamic> json) {
    return WiFiSetupResponse(
      success: json['success'] as bool? ?? false,
      message: json['message'] as String? ?? '',
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'success': success,
      'message': message,
    };
  }

  @override
  List<Object?> get props => [success, message];
}

/// Device status response
class DeviceStatusResponse extends Equatable {
  final String status;
  final String deviceId;

  const DeviceStatusResponse({
    required this.status,
    required this.deviceId,
  });

  bool get isReady => status.toLowerCase() == 'ready';

  factory DeviceStatusResponse.fromJson(Map<String, dynamic> json) {
    return DeviceStatusResponse(
      status: json['status'] as String? ?? '',
      deviceId: json['deviceId'] as String? ?? '',
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'status': status,
      'deviceId': deviceId,
    };
  }

  @override
  List<Object?> get props => [status, deviceId];
}

/// WiFi scan response from device
class WiFiScanResponse extends Equatable {
  final List<WiFiNetwork> networks;

  const WiFiScanResponse({
    required this.networks,
  });

  factory WiFiScanResponse.fromJson(Map<String, dynamic> json) {
    final networksList = json['networks'] as List<dynamic>? ?? [];
    return WiFiScanResponse(
      networks: networksList
          .map((network) => WiFiNetwork.fromJson(network as Map<String, dynamic>))
          .toList(),
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'networks': networks.map((network) => network.toJson()).toList(),
    };
  }

  @override
  List<Object?> get props => [networks];
}
