# WiFi Provisioning System

Complete guide for ESP32-S3 WiFi setup and provisioning system for Flutter mobile app integration.

## Overview

The device supports WiFi provisioning through a dedicated setup mode that creates an Access Point (AP) for configuration. Users can connect via a Flutter mobile app to:
- Scan available WiFi networks
- Enter WiFi credentials
- Configure dashboard access credentials
- Automatically connect to the configured network

## Setup Mode Activation

### Automatic Entry
- **First Boot**: Device automatically enters setup mode if no WiFi credentials are saved
- **Connection Failure**: After 3 failed connection attempts, device enters setup mode

### Manual Entry
- **Button BTN5**: Press and hold for 5 seconds to reset WiFi credentials and enter setup mode
- Device will restart in setup mode

## Access Point Configuration

When in setup mode, the device creates an open WiFi network:

```
SSID: AquaFlow-{DEVICE_ID}
Password: (none - open network)
Device IP: 192.168.4.1
```

**Example**: `AquaFlow-wt001-DEV-02`

## Provisioning API Endpoints

All endpoints are accessed at `http://192.168.4.1/{device_id}/`

### 1. Check Device Status

**Endpoint**: `GET http://192.168.4.1/{device_id}/status`

**Example**: `GET http://192.168.4.1/wt001-DEV-02/status`

**Response**:
```json
{
  "status": "ready",
  "deviceId": "wt001-DEV-02"
}
```

**Purpose**: Verify device is ready for configuration

---

### 2. Scan WiFi Networks

**Endpoint**: `GET http://192.168.4.1/{device_id}/scanWifi`

**Example**: `GET http://192.168.4.1/wt001-DEV-02/scanWifi`

**Response**:
```json
{
  "networks": [
    {
      "ssid": "HomeWiFi",
      "signal": -45,
      "auth": "WPA2"
    },
    {
      "ssid": "Office_Net",
      "signal": -67,
      "auth": "WPA2"
    },
    {
      "ssid": "OpenNetwork",
      "signal": -78,
      "auth": "OPEN"
    }
  ]
}
```

**Features**:
- Networks sorted by signal strength (strongest first)
- Signal strength in dBm (negative values, closer to 0 is stronger)
- Authentication type: OPEN, WEP, WPA, WPA2, WPA/WPA2, WPA2-EAP, WPA3

**Note**: Scan may take 5-10 seconds to complete

---

### 3. Save Credentials

**Endpoint**: `POST http://192.168.4.1/{device_id}/save`

**Example**: `POST http://192.168.4.1/wt001-DEV-02/save`

**Request Body**:
```json
{
  "ssid": "HomeWiFi",
  "password": "mypassword123",
  "dashboardUsername": "admin",
  "dashboardPassword": "dashpass456"
}
```

**Required Fields**:
- `ssid`: WiFi network name (cannot be empty)
- `password`: WiFi password (can be empty for open networks)

**Optional Fields**:
- `dashboardUsername`: Username for backend dashboard authentication
- `dashboardPassword`: Password for backend dashboard authentication

**Success Response**:
```json
{
  "success": true,
  "message": "Connecting to WiFi..."
}
```

**Error Responses**:
```json
{
  "success": false,
  "message": "Invalid JSON"
}
```
```json
{
  "success": false,
  "message": "Missing ssid or password"
}
```
```json
{
  "success": false,
  "message": "SSID cannot be empty"
}
```
```json
{
  "success": false,
  "message": "Failed to connect to WiFi. Please check credentials."
}
```

**Behavior After Save**:
1. Credentials saved to NVS (non-volatile storage)
2. Device attempts to connect to WiFi network
3. If successful:
   - Registers with backend server
   - Fetches device configuration
   - Restarts in normal operation mode
4. If failed:
   - Remains in setup mode
   - Returns error response
   - User can retry with correct credentials

---

## CORS Support

All endpoints include CORS headers for cross-origin requests:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

This allows Flutter web apps to make direct requests to the device.

## Setup Mode Timeout

- **Duration**: 10 minutes
- **Behavior**: After 10 minutes of no configuration, device attempts to connect with any previously saved credentials
- **Reset**: Timeout resets if device enters setup mode again

## Flutter Mobile App Integration

### Connection Flow

```
1. User opens Flutter app
2. App detects no device configured
3. App prompts user to connect to AquaFlow-{DEVICE_ID} WiFi
4. User connects phone to device AP
5. App calls /{device_id}/status to verify connection
6. App calls /{device_id}/scanWifi to get networks
7. User selects network and enters password
8. User enters dashboard credentials (optional)
9. App calls /{device_id}/save with credentials
10. Device connects to WiFi and registers with backend
11. App instructs user to reconnect phone to home WiFi
12. Device now accessible via backend API
```

### Example Code (Flutter/Dart)

```dart
import 'dart:convert';
import 'package:http/http.dart' as http;

class WifiProvisioning {
  final String deviceId;
  final String deviceIP = '192.168.4.1';

  WifiProvisioning(this.deviceId);

  // Check device status
  Future<bool> checkStatus() async {
    try {
      final response = await http.get(
        Uri.parse('http://$deviceIP/$deviceId/status'),
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        return data['status'] == 'ready';
      }
      return false;
    } catch (e) {
      print('Error checking status: $e');
      return false;
    }
  }

  // Scan WiFi networks
  Future<List<WifiNetwork>> scanNetworks() async {
    try {
      final response = await http.get(
        Uri.parse('http://$deviceIP/$deviceId/scanWifi'),
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        List<WifiNetwork> networks = [];

        for (var network in data['networks']) {
          networks.add(WifiNetwork(
            ssid: network['ssid'],
            signal: network['signal'],
            auth: network['auth'],
          ));
        }

        return networks;
      }
      return [];
    } catch (e) {
      print('Error scanning networks: $e');
      return [];
    }
  }

  // Save WiFi credentials
  Future<bool> saveCredentials({
    required String ssid,
    required String password,
    String? dashboardUsername,
    String? dashboardPassword,
  }) async {
    try {
      final Map<String, dynamic> body = {
        'ssid': ssid,
        'password': password,
      };

      if (dashboardUsername != null && dashboardPassword != null) {
        body['dashboardUsername'] = dashboardUsername;
        body['dashboardPassword'] = dashboardPassword;
      }

      final response = await http.post(
        Uri.parse('http://$deviceIP/$deviceId/save'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode(body),
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        return data['success'] == true;
      }
      return false;
    } catch (e) {
      print('Error saving credentials: $e');
      return false;
    }
  }
}

class WifiNetwork {
  final String ssid;
  final int signal;
  final String auth;

  WifiNetwork({
    required this.ssid,
    required this.signal,
    required this.auth,
  });

  String get signalStrength {
    if (signal > -50) return 'Excellent';
    if (signal > -60) return 'Good';
    if (signal > -70) return 'Fair';
    return 'Weak';
  }

  bool get isSecure => auth != 'OPEN';
}
```

## Credential Storage

All credentials are stored in ESP32's NVS (Non-Volatile Storage):

| Key | Description | Type |
|-----|-------------|------|
| `wifi_ssid` | WiFi network name | String |
| `wifi_pass` | WiFi password | String |
| `wifi_configured` | Configuration status flag | Boolean |
| `dash_user` | Dashboard username | String |
| `dash_pass` | Dashboard password | String |

**Security Notes**:
- NVS data persists across reboots
- NVS is encrypted on ESP32
- Credentials are never transmitted over serial
- Only logged in obfuscated form for debugging

## Dashboard Credentials Usage

Dashboard credentials are used for HTTP Basic Authentication when communicating with the backend server:

```cpp
// HTTP Basic Auth header format
String username = "admin";
String password = "dashpass456";
String auth = base64Encode(username + ":" + password);
String authHeader = "Basic " + auth;

http.addHeader("Authorization", authHeader);
```

**Use Cases**:
- Backend API authentication
- Dashboard web interface access
- Administrative operations

## Troubleshooting

### Device Not Creating AP

**Symptoms**: Cannot find AquaFlow-{DEVICE_ID} network

**Solutions**:
1. Check if device is powered on
2. Press and hold BTN5 for 5 seconds to force setup mode
3. Look for OLED display message "AP Mode"
4. Check device serial output for AP IP address

### Cannot Connect to AP

**Symptoms**: Phone cannot connect to device AP

**Solutions**:
1. Ensure using correct SSID (case-sensitive)
2. AP has no password - leave password field blank
3. Disable cellular data on phone
4. Forget network and reconnect
5. Try different phone/device

### Network Scan Returns Empty

**Symptoms**: `/scanWifi` returns empty networks array

**Solutions**:
1. Wait 10 seconds and try again
2. Check WiFi antenna connection on ESP32
3. Move device closer to WiFi router
4. Check if 2.4GHz WiFi is enabled (ESP32 doesn't support 5GHz)

### Save Returns Success But Not Connecting

**Symptoms**: Credentials save but device doesn't connect

**Solutions**:
1. Verify WiFi password is correct
2. Check if WiFi network is 2.4GHz (not 5GHz)
3. Ensure WiFi router is not filtering MAC addresses
4. Check device serial output for connection errors
5. Try entering setup mode again and reconfigure

### Dashboard Credentials Not Working

**Symptoms**: Backend authentication fails

**Solutions**:
1. Verify credentials match backend user account
2. Check if dashboard user has device permissions
3. Ensure credentials were saved during provisioning
4. Re-enter setup mode and update credentials

## Serial Monitor Output

Example output during provisioning:

```
========================================
  ESP32-S3 Water Tank Monitor
  Firmware: 1.0.0
========================================

[WiFi] WiFi Manager initialized
[WiFi] Device ID: wt001-DEV-02
[WiFi] AP SSID: AquaFlow-wt001-DEV-02
[WiFi] MAC Address: AA:BB:CC:DD:EE:FF
[WiFi] WiFi not configured, starting AP mode
[WiFi] AP Mode Started
[WiFi] SSID: AquaFlow-wt001-DEV-02
[WiFi] Password: (open network)
[WiFi] IP Address: 192.168.4.1

[WebServer] Local web server started on port 80
[WebServer] Device ID: wt001-DEV-02
[WebServer] Endpoints:
  GET  /api/status          - Get current sensor readings
  POST /api/control         - Control pump
  GET  /api/config          - Get device configuration
  GET  /wt001-DEV-02/status    - Provisioning status
  GET  /wt001-DEV-02/scanWifi  - Scan WiFi networks
  POST /wt001-DEV-02/save      - Save WiFi credentials

[Main] System initialized but not connected (AP mode)

[WebServer] GET /wt001-DEV-02/status
[WebServer] GET /wt001-DEV-02/scanWifi - Scanning networks...
[WiFi] Scanning for networks...
[WiFi] Found 5 networks
  HomeWiFi (-45 dBm) WPA2
  Office_Net (-67 dBm) WPA2
  Guest_WiFi (-72 dBm) OPEN
  Neighbor (-80 dBm) WPA2
  CafeNet (-85 dBm) WPA2
[WebServer] WiFi scan complete

[WebServer] POST /wt001-DEV-02/save
[WebServer] Received credentials:
  SSID: HomeWiFi
  Dashboard User: admin
[WiFi] Credentials saved for SSID: HomeWiFi
[WiFi] Dashboard credentials saved for user: admin
[Main] WiFi credentials received from web interface
[Main] Attempting to connect to: HomeWiFi
[WiFi] Connection attempt 1/3
[WiFi] Attempting to connect to: HomeWiFi
.....
[WiFi] Connected successfully!
[WiFi] IP Address: 192.168.1.100
[WiFi] RSSI: -45 dBm
[API] Device registered successfully
[Main] WiFi configured. Restarting in 3 seconds...
```

## Security Considerations

### Current Implementation
- AP is open (no password) for easier initial setup
- Credentials transmitted over HTTP (no encryption)
- NVS storage encrypted by ESP32
- No rate limiting on API endpoints

### Production Recommendations
1. **Add WPA2 password to AP** for setup security
2. **Implement HTTPS** for credential transmission
3. **Add rate limiting** to prevent brute force
4. **Implement timeout** for failed login attempts
5. **Add certificate pinning** for backend communication
6. **Implement device pairing** with QR code or PIN

## Advanced Configuration

### Changing AP SSID Pattern

Edit `wifi_manager.cpp`:
```cpp
apSSID = "MyDevice-" + deviceId;
```

### Changing Default IP

ESP32 AP mode defaults to `192.168.4.1`. To change:
```cpp
IPAddress local_IP(192,168,5,1);
IPAddress gateway(192,168,5,1);
IPAddress subnet(255,255,255,0);

WiFi.softAPConfig(local_IP, gateway, subnet);
WiFi.softAP(apSSID.c_str());
```

### Adding AP Password

Edit `wifi_manager.cpp`:
```cpp
const char* ap_password = "your_secure_password";
WiFi.softAP(apSSID.c_str(), ap_password);
```

### Extending Setup Timeout

Edit `config.h`:
```cpp
#define SETUP_MODE_TIMEOUT 1200000  // 20 minutes
```

## Testing

### Manual Testing with curl

```bash
# Check status
curl http://192.168.4.1/wt001-DEV-02/status

# Scan networks
curl http://192.168.4.1/wt001-DEV-02/scanWifi

# Save credentials
curl -X POST http://192.168.4.1/wt001-DEV-02/save \
  -H "Content-Type: application/json" \
  -d '{
    "ssid": "TestNetwork",
    "password": "testpass123",
    "dashboardUsername": "admin",
    "dashboardPassword": "admin123"
  }'
```

### Testing with Postman

1. Connect computer to `AquaFlow-{DEVICE_ID}` network
2. Import endpoints:
   - GET `http://192.168.4.1/wt001-DEV-02/status`
   - GET `http://192.168.4.1/wt001-DEV-02/scanWifi`
   - POST `http://192.168.4.1/wt001-DEV-02/save`
3. Test each endpoint
4. Verify responses match documentation

## FAQ

**Q: How do I know the device ID?**
A: Check the OLED display or device label. Default format: `wt001-DEV-02`

**Q: Can I change WiFi credentials after setup?**
A: Yes, press and hold BTN5 for 5 seconds to reset and re-enter setup mode

**Q: Does the device support 5GHz WiFi?**
A: No, ESP32 only supports 2.4GHz WiFi networks

**Q: What happens if I enter wrong password?**
A: Device will fail to connect and remain in setup mode. Try again with correct password.

**Q: Can I skip dashboard credentials?**
A: Yes, they're optional. Leave fields empty if not using dashboard authentication.

**Q: How long does WiFi scan take?**
A: Typically 5-10 seconds, depending on number of networks

**Q: Will device remember credentials after power loss?**
A: Yes, credentials stored in NVS persist across reboots and power cycles

**Q: Can multiple devices have same SSID?**
A: No, each device creates unique AP with device ID in SSID

**Q: What if device can't connect to internet after WiFi setup?**
A: Check router internet connection and firewall settings

**Q: How do I factory reset the device?**
A: Long press BTN5 to reset WiFi, or re-flash firmware to reset all settings

---

## Summary

The WiFi provisioning system provides a seamless setup experience through:
- Automatic AP mode on first boot
- Network scanning for easy SSID selection
- Secure credential storage
- Dashboard integration
- Comprehensive error handling
- Flutter app compatibility

For technical support or issues, check serial monitor output and refer to troubleshooting section.
