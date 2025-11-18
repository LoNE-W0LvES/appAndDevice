# ESP32-S3 Water Tank Monitoring Device

Production-ready firmware for ESP32-S3 water tank monitoring device with cloud integration and offline Flutter app support.

## Features

### Hardware Integration
- **Ultrasonic Sensor (HC-SR04)**: Real-time water level measurement with smoothing algorithm
- **OLED Display (SSD1306)**: 3-screen interface with status, network info, and settings
- **Relay Control**: Pump control with auto/manual/override modes
- **6 Buttons**: Full device control without mobile app

### Cloud Integration
- **JWT Authentication**: Secure device registration and communication
- **Backend API**: Full integration with IoT platform at http://103.136.236.16
- **Nested JSON Parsing**: Proper handling of complex backend data structures
- **Telemetry Upload**: Water level, inflow rate, pump status, and online/offline status (every 30s)
- **Online/Offline Tracking**: Automatic status tracking - device marked offline if no telemetry for >60s
- **Remote Control**: Cloud-based pump control and configuration updates
- **OTA Updates**: Automatic firmware updates via backend flag

### Local Access & Provisioning
- **WiFi Provisioning**: Easy setup via open AP (AquaFlow-{DEVICE_ID})
- **Network Scanning**: Automated WiFi network discovery and selection
- **Dashboard Credentials**: Secure storage of backend authentication
- **Web Server**: REST API for offline Flutter app access
- **Setup Mode**: Automatic entry on first boot or manual via BTN5 (5s hold)
- **Zero-dependency Operation**: Works offline after initial setup

### Smart Features
- **Auto Mode**: Threshold-based pump control with hysteresis
- **Manual Mode**: Cloud or button-based control
- **Hardware Override**: Physical switch for emergency control
- **Current Inflow Calculation**: Real-time water flow monitoring
- **Auto-Reconnect**: Robust WiFi and backend connection handling

## Hardware Configuration

### Pin Connections

```
OLED Display (SSD1306 I2C):
  SDA: GPIO 8
  SCL: GPIO 9

Ultrasonic Sensor (HC-SR04):
  TRIG: GPIO 6
  ECHO: GPIO 7

Relay (Pump Control):
  SIGNAL: GPIO 5

Buttons:
  BTN1: GPIO 10  (Cycle display screens)
  BTN2: GPIO 46  (Manual pump ON)
  BTN3: GPIO 3   (Toggle Auto/Manual mode)
  BTN4: GPIO 18  (Manual pump OFF)
  BTN5: GPIO 17  (WiFi reset - long press 5s)
  BTN6: GPIO 12  (Hardware override switch)
```

## WiFi Provisioning & Setup

The device features an intelligent WiFi provisioning system for easy setup via Flutter mobile app.

### Setup Mode Activation

**Automatic**:
- First boot (no credentials saved)
- After 3 failed WiFi connection attempts

**Manual**:
- Press and hold BTN5 for 5 seconds

### Access Point Details

When in setup mode:
```
SSID: AquaFlow-wt001-DEV-02
Password: PassAquaWT001
Device IP: 192.168.4.1
```

### Provisioning API Endpoints

**Check Status**:
```http
GET http://192.168.4.1/wt001-DEV-02/status

Response:
{
  "status": "ready",
  "deviceId": "wt001-DEV-02"
}
```

**Scan WiFi Networks**:
```http
GET http://192.168.4.1/wt001-DEV-02/scanWifi

Response:
{
  "networks": [
    {"ssid": "HomeWiFi", "signal": -45, "auth": "WPA2"},
    {"ssid": "Office", "signal": -67, "auth": "WPA2"}
  ]
}
```

**Save Credentials**:
```http
POST http://192.168.4.1/wt001-DEV-02/save
Content-Type: application/json

{
  "ssid": "HomeWiFi",
  "password": "mypassword123",
  "dashboardUsername": "admin",
  "dashboardPassword": "dashpass456"
}

Response:
{
  "success": true,
  "message": "Connecting to WiFi..."
}
```

### Features

- ✅ Automatic WiFi network scanning
- ✅ Networks sorted by signal strength
- ✅ Dashboard credential storage
- ✅ Auto-restart after successful setup
- ✅ 10-minute setup timeout
- ✅ CORS support for Flutter web apps
- ✅ Comprehensive error handling

**For detailed provisioning documentation, see [WIFI_PROVISIONING.md](WIFI_PROVISIONING.md)**

## Backend API Integration

### Authentication Flow

1. **Device Registration** (First boot):
```http
POST /api/device-auth/register
Content-Type: application/json

{
  "hardwareId": "{ESP32_MAC_ADDRESS}",
  "deviceName": "wt001-DEV-02",
  "projectId": "690e6a9d092433c0acfb9177",
  "metadata": {
    "chipModel": "ESP32-S3",
    "firmwareVersion": "1.0.0"
  }
}

Response:
{
  "deviceToken": "JWT_TOKEN_HERE",
  "device": {...}
}
```

2. **Authenticated Requests**: All subsequent requests use `Authorization: Bearer {token}`

### Data Structures

#### Device Configuration (from backend)
```json
{
  "data": {
    "deviceConfig": {
      "tankHeight": {
        "key": "tankHeight",
        "label": "Tank Height",
        "type": "number",
        "value": 100,
        "lastModified": 1763311803637
      },
      "upperThreshold": {
        "key": "upperThreshold",
        "value": 85
      },
      "lowerThreshold": {
        "key": "lowerThreshold",
        "value": 20
      },
      "force_update": {
        "key": "force_update",
        "value": false
      }
    }
  }
}
```

**IMPORTANT**: All config values accessed via `deviceConfig.{key}.value`

#### Control Data (from backend)
```json
{
  "data": {
    "controlData": {
      "pumpSwitch": {
        "key": "pumpSwitch",
        "type": "boolean",
        "value": true
      },
      "config_update": {
        "key": "config_update",
        "value": false,
        "system": true
      }
    }
  }
}
```

**IMPORTANT**: All control values accessed via `controlData.{key}.value`

#### Telemetry Upload (to backend)
```json
{
  "sensorData": {
    "waterLevel": {
      "key": "waterLevel",
      "label": "Water Level",
      "type": "number",
      "value": 75.5
    },
    "currInflow": {
      "key": "currInflow",
      "label": "Current Inflow",
      "type": "number",
      "value": 12.3
    },
    "pumpStatus": {
      "key": "pumpStatus",
      "label": "Pump Status",
      "type": "number",
      "value": 1
    },
    "Status": {
      "key": "Status",
      "label": "Device Status",
      "type": "number",
      "value": 1
    }
  }
}
```

**IMPORTANT**:
- pumpStatus is NUMBER (0=OFF, 1=ON), NOT boolean
- **Status is NUMBER: Always set to 1 when sending telemetry**
- Server automatically sets Status to 0 if device stops sending for >60 seconds
- Full nested structure with key, label, type, value required
- Send telemetry every 30 seconds to maintain online status

## Local Web Server API

For offline Flutter app access:

### GET /api/status
Returns simple values (not nested):
```json
{
  "waterLevel": 75.5,
  "currInflow": 12.3,
  "pumpStatus": 1,
  "timestamp": 123456789
}
```

### POST /api/control
Control pump locally:
```json
{
  "pumpSwitch": true
}
```

### GET /api/config
Returns device configuration:
```json
{
  "upperThreshold": 85,
  "lowerThreshold": 20,
  "tankHeight": 100,
  "tankWidth": 50,
  "tankShape": "Cylindrical"
}
```

### WiFi Configuration (AP Mode)
- **GET /wifi/config**: Configuration web page
- **POST /wifi/config**: Save WiFi credentials

## Operation Modes

### Auto Mode
- Pump turns ON when water level < lowerThreshold
- Pump turns OFF when water level > upperThreshold
- Hysteresis prevents rapid switching
- Cloud commands ignored in this mode

### Manual Mode
- Pump controlled via:
  - Cloud commands (pumpSwitch)
  - Button presses (BTN2/BTN4)
  - Local web API
- Auto threshold logic disabled

### Override Mode
- Activated by BTN6 hardware switch
- All automatic control disabled
- Manual button control only
- Emergency use only

## Data Flow Timeline

```
Startup:
├─ Initialize hardware
├─ Connect WiFi (or start AP mode)
├─ Register device / authenticate
├─ Fetch device configuration
├─ Apply configuration to sensors
└─ Start local web server

Every 1 second:
├─ Read ultrasonic sensor
├─ Calculate water level
├─ Calculate inflow rate
└─ Update relay based on mode

Every 30 seconds:
├─ Upload telemetry to backend
│  ├─ Water level
│  ├─ Current inflow
│  ├─ Pump status (0 or 1)
│  └─ Status: 1 (device online)
└─ Server marks device as online

Every 5 minutes:
├─ Fetch control data
├─ Check config_update flag
│  └─ If true: re-fetch and apply config
└─ Check force_update flag
   └─ If true: perform OTA update
```

## Online/Offline Status Tracking

The device automatically maintains its online/offline status with the backend server.

### How It Works

**Device Side (ESP32):**
- Every telemetry upload includes `Status` field set to `1`
- Telemetry sent every 30 seconds (configurable via `TELEMETRY_UPLOAD_INTERVAL`)
- Status field format:
  ```json
  "Status": {
    "key": "Status",
    "label": "Device Status",
    "type": "number",
    "value": 1
  }
  ```

**Server Side:**
- Receives telemetry with Status = 1
- Marks device as **ONLINE** (green indicator in dashboard)
- If no telemetry received for >60 seconds:
  - Automatically sets Status to 0
  - Marks device as **OFFLINE** (red indicator in dashboard)

### Status Values
- `1` = Device is **ONLINE** (actively sending data)
- `0` = Device is **OFFLINE** (no data for >60 seconds)

### Important Notes

1. **Always Send Status = 1**
   - Never send Status = 0 from device
   - Server automatically manages offline status

2. **Telemetry Frequency**
   - Current: 30 seconds (safe margin under 60s timeout)
   - Recommended: 10-45 seconds
   - Maximum: <60 seconds to avoid offline status

3. **Network Interruptions**
   - Brief disconnections (<60s): Device stays online
   - Extended disconnections (>60s): Device marked offline
   - Auto-recovery: Next successful telemetry restores online status

4. **Power Cycles**
   - Device marked offline after 60s of being powered off
   - Comes back online automatically after boot and first telemetry

### Monitoring Device Status

Check device status in backend dashboard:
- **Green**: Device online (receiving regular telemetry)
- **Red**: Device offline (no telemetry for >60 seconds)
- **Last Seen**: Timestamp of last received telemetry

## Building and Uploading

### Prerequisites
- PlatformIO installed
- ESP32-S3 board connected
- USB cable with data lines

### Build Commands
```bash
# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor

# Build and upload
pio run --target upload && pio device monitor
```

## Configuration

Edit `src/config.h` to customize:

```cpp
// Backend server
#define SERVER_URL "http://103.136.236.16"
#define PROJECT_ID "wt001"
#define DEVICE_NAME "DEV-02"
#define MONGODB_DEVICE_ID "690e6a9d092433c0acfb9178"
#define DEVICE_ID "wt001-DEV-02-690e6a9d092433c0acfb9178"

// WiFi AP credentials
#define AP_PASSWORD "PassAquaWT001"

// Timing intervals
#define SENSOR_READ_INTERVAL 1000       // 1s
#define TELEMETRY_UPLOAD_INTERVAL 30000 // 30s
#define CONTROL_FETCH_INTERVAL 300000   // 5min
```

## Serial Monitor Output

Example startup sequence:
```
========================================
  ESP32-S3 Water Tank Monitor
  Firmware: 1.0.0
========================================

[Display] OLED initialized
[WiFi] WiFi Manager initialized
[WiFi] MAC Address: AA:BB:CC:DD:EE:FF
[WiFi] Loaded credentials for SSID: MyNetwork
[WiFi] Attempting to connect to: MyNetwork
[WiFi] Connected successfully!
[WiFi] IP Address: 192.168.1.100
[API] Device registered successfully
[API] Config parsed successfully
  tankHeight: 100.0
  upperThreshold: 85.0
  lowerThreshold: 20.0
[WebServer] Local web server started on port 80
[Main] System fully initialized and connected
```

## Troubleshooting

### WiFi Won't Connect
1. Long-press BTN5 (3 seconds) to reset WiFi credentials
2. Device will restart in AP mode
3. Connect to `WaterTank-SETUP-{MAC}` (password: `watertank123`)
4. Navigate to `http://192.168.4.1/wifi/config`
5. Enter your WiFi credentials

### Backend Registration Failed
- Check SERVER_URL in config.h
- Verify internet connection
- Check PROJECT_ID is correct
- Review serial monitor for HTTP error codes

### Sensor Reading Issues
- Verify TRIG and ECHO pin connections
- Check 5V power supply to HC-SR04
- Ensure no obstacles in sensor path
- Check serial output for distance readings

### Pump Not Responding
- Verify relay connection to GPIO 5
- Check current mode (Auto/Manual/Override)
- In Auto mode, check water level vs thresholds
- In Manual mode, use BTN2/BTN4 or web API

## File Structure

```
include/                          # Header files
├── config.h                      # Configuration and pin definitions
├── wifi_manager.h                # WiFi connectivity with AP fallback
├── api_client.h                  # Backend API integration (JWT auth)
├── sensor_manager.h              # Ultrasonic sensor + inflow calculation
├── relay_controller.h            # Pump control logic
├── display_manager.h             # OLED display with 3 screens
├── button_handler.h              # 6-button input handling
├── webserver.h                   # Local REST API for Flutter
└── ota_updater.h                 # OTA firmware updates

src/                              # Source files
├── main.cpp                      # Main application logic
├── wifi_manager.cpp              # WiFi implementation
├── api_client.cpp                # Backend API implementation
├── sensor_manager.cpp            # Sensor reading implementation
├── relay_controller.cpp          # Pump control implementation
├── display_manager.cpp           # OLED display implementation
├── button_handler.cpp            # Button handling implementation
├── webserver.cpp                 # Web server implementation
└── ota_updater.cpp               # OTA update implementation
```

## Security Considerations

### Current Implementation (HTTP)
- Server URL: `http://103.136.236.16`
- JWT token stored in NVS (encrypted flash)
- No sensitive data logging to serial

### Production Recommendations
1. **Use HTTPS**: Change SERVER_URL to `https://...`
2. **Secure WiFi**: Use WPA2/WPA3 networks
3. **API Keys**: Consider additional API key authentication
4. **Certificate Pinning**: Validate server certificates
5. **Encrypted Storage**: Consider additional encryption for tokens

## Current Inflow Calculation

Algorithm calculates flow rate from water level changes:

```cpp
// Time difference in seconds
float timeDiff = (currentTime - previousTime) / 1000.0;

// Water level change in cm
float levelChange = currentWaterLevel - previousWaterLevel;

// Volume change based on tank shape
if (tankShape == "Cylindrical") {
    volumeChange = π * r² * Δh;  // cm³
} else if (tankShape == "Rectangular") {
    volumeChange = width² * Δh;  // cm³
}

// Convert to L/min
currentInflow = (volumeChange / 1000.0) / timeDiff * 60.0;
```

## Known Limitations

1. **Ultrasonic Accuracy**: ±1cm accuracy, affected by temperature
2. **Inflow Calculation**: Assumes constant tank cross-section
3. **WiFi Range**: Limited by ESP32-S3 antenna strength
4. **OTA Size**: Firmware must fit in OTA partition (~1.5MB)
5. **HTTP Only**: No HTTPS support in current version

## Future Enhancements

- [ ] HTTPS support with certificate validation
- [ ] MQTT integration for real-time updates
- [ ] Historical data logging to SD card
- [ ] Multi-tank support (I2C multiplexing)
- [ ] Battery backup monitoring
- [ ] SMS/email alerts
- [ ] Web-based dashboard (served from ESP32)

## License

Proprietary - For authorized use only

## Support

For issues or questions:
- Check serial monitor output
- Review backend API logs
- Verify hardware connections
- Ensure firmware version matches backend requirements

---

**Firmware Version**: 1.0.0
**Target Device**: ESP32-S3-DevKitC-1
**Backend**: http://103.136.236.16
**Last Updated**: 2025-01-16
