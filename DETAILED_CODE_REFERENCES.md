# Detailed Code References - Device Online/Offline Status

## 1. TELEMETRY UPLOAD INTERVAL & FREQUENCY

### File: `/home/user/appAndDevice/IOTWaterTankDevice/include/config.h`
**Lines: 108-116**
```c
// ============================================================================
// TIMING CONFIGURATION
// ============================================================================

#define SENSOR_READ_INTERVAL 1000       // 1 second - sensor reading
#define TELEMETRY_UPLOAD_INTERVAL 30000 // 30 seconds - upload data
#define CONTROL_FETCH_INTERVAL 300000   // 5 minutes - fetch control data
#define CONFIG_CHECK_INTERVAL 300000    // 5 minutes - check config update
#define OTA_CHECK_INTERVAL 300000       // 5 minutes - check firmware update
#define DISPLAY_UPDATE_INTERVAL 500     // 0.5 seconds - update display
```

---

### File: `/home/user/appAndDevice/IOTWaterTankDevice/src/main.cpp`
**Lines: 66, 1502, 1585-1587**

#### Timer tracking:
```cpp
unsigned long lastTelemetryUpload = 0;
```

#### Initialization:
```cpp
lastTelemetryUpload = millis();
```

#### Main loop telemetry check:
```cpp
// Upload telemetry (every 30 seconds)
if (currentTime - lastTelemetryUpload >= TELEMETRY_UPLOAD_INTERVAL) {
    lastTelemetryUpload = currentTime;
    uploadTelemetry();
}
```

---

## 2. TELEMETRY PAYLOAD WITH STATUS FIELD

### File: `/home/user/appAndDevice/IOTWaterTankDevice/src/telemetry.cpp`
**Lines: 103-143**

```cpp
String TelemetryManager::buildTelemetryPayload(float waterLevel, float currInflow, int pumpStatus) {
    StaticJsonDocument<2048> doc;
    
    doc["deviceId"] = DEVICE_ID;
    
    // Backend expects telemetry data with named keys (not array)
    JsonObject telemetryData = doc.createNestedObject("sensorData");
    
    // Water level
    JsonObject level = telemetryData.createNestedObject("waterLevel");
    level["key"] = "waterLevel";
    level["label"] = "Water Level";
    level["type"] = "number";
    level["value"] = waterLevel;
    
    // Current inflow
    JsonObject inflow = telemetryData.createNestedObject("currInflow");
    inflow["key"] = "currInflow";
    inflow["label"] = "Current Inflow";
    inflow["type"] = "number";
    inflow["value"] = currInflow;
    
    // Pump status
    JsonObject pump = telemetryData.createNestedObject("pumpStatus");
    pump["key"] = "pumpStatus";
    pump["label"] = "Pump Status";
    pump["type"] = "number";
    pump["value"] = pumpStatus;
    
    // Status (always 1 for online tracking)
    // Server marks device offline if no telemetry for >60 seconds
    JsonObject status = telemetryData.createNestedObject("Status");
    status["key"] = "Status";
    status["label"] = "Device Status";
    status["type"] = "number";
    status["value"] = 1;
    
    String payload;
    serializeJson(doc, payload);
    return payload;
}
```

**KEY ISSUE**: Status is ALWAYS 1 - it never indicates offline state!

---

## 3. TELEMETRY UPLOAD (ENDPOINT)

### File: `/home/user/appAndDevice/IOTWaterTankDevice/include/endpoints.h`
**Line: 31**

```c
#define API_DEVICE_TELEMETRY         "/api/device-telemetry"       // POST telemetry data
```

### File: `/home/user/appAndDevice/IOTWaterTankDevice/src/telemetry.cpp`
**Lines: 27-33**

```cpp
bool TelemetryManager::uploadTelemetry(float waterLevel, float currInflow, int pumpStatus) {
    String payload = buildTelemetryPayload(waterLevel, currInflow, pumpStatus);
    String response;
    
    // Single attempt for telemetry to avoid blocking
    return httpRequest("POST", API_DEVICE_TELEMETRY, payload, response, 1);
}
```

---

## 4. DEVICE MODEL - LAST SEEN FIELD

### File: `/home/user/appAndDevice/iot_water_tank/lib/models/device.dart`
**Lines: 18, 34, 109-111, 141**

#### Field definition:
```dart
final DateTime? lastSeen;
```

#### Constructor parameter:
```dart
this.lastSeen,
```

#### JSON parsing:
```dart
lastSeen: json['lastSeen'] != null
    ? DateTime.parse(json['lastSeen'] as String)
    : null,
```

#### JSON serialization:
```dart
if (lastSeen != null) 'lastSeen': lastSeen!.toIso8601String(),
```

---

## 5. APP STATUS COLOR DETERMINATION

### File: `/home/user/appAndDevice/iot_water_tank/lib/models/device.dart`
**Lines: 169-180**

```dart
/// Get status color based on device state
String getStatusColor() {
  if (!isActive) return 'grey';
  if (lastSeen == null) return 'grey';
  
  final now = DateTime.now();
  final difference = now.difference(lastSeen!).inSeconds;
  
  if (difference < 60) return 'green';       // Active in last minute
  if (difference < 300) return 'yellow';     // Active in last 5 minutes
  return 'red';                              // Inactive
}
```

**PROBLEM**: Depends 100% on `lastSeen` being updated by the server when telemetry is received.

---

## 6. APP STATUS TEXT DETERMINATION

### File: `/home/user/appAndDevice/iot_water_tank/lib/models/device.dart`
**Lines: 182-199**

```dart
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
```

---

## 7. ALTERNATE ONLINE STATUS (FROM TELEMETRY)

### File: `/home/user/appAndDevice/iot_water_tank/lib/models/device.dart`
**Lines: 201-211**

```dart
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
```

**PROBLEM**: Since device ALWAYS sends Status=1, this always returns true!

---

## 8. DEVICE SIDEBAR - SORTING BY LAST SEEN

### File: `/home/user/appAndDevice/iot_water_tank/lib/widgets/device_sidebar.dart`
**Lines: 271-275**

```dart
case 'lastSeen':
  devices.sort((a, b) {
    final aTime = a.lastSeen ?? DateTime(1970);
    final bTime = b.lastSeen ?? DateTime(1970);
    return bTime.compareTo(aTime);  // Most recent first
  });
```

---

## 9. DEVICE SIDEBAR - DISPLAY LAST SEEN

### File: `/home/user/appAndDevice/iot_water_tank/lib/widgets/device_sidebar.dart`
**Line: 388**

```dart
'${device.deviceId} • ${device.lastSeenText}',
```

---

## 10. DOCUMENTATION REFERENCES

### File: `/home/user/appAndDevice/IOTWaterTankDevice/README.md`
**Line: 18**

```
Online/Offline Tracking: Automatic status tracking - device marked offline if no telemetry for >60s
```

### File: `/home/user/appAndDevice/IOTWaterTankDevice/include/api_client.h`
**Line: 98**

```cpp
// Upload telemetry data to server
// Automatically includes Status field (always 1 for online tracking)
// Server marks device offline if no telemetry for >60 seconds
bool uploadTelemetry(float waterLevel, float currInflow, int pumpStatus);
```

### File: `/home/user/appAndDevice/IOTWaterTankDevice/include/telemetry.h`
**Lines: 31-34**

```cpp
// Upload telemetry data to server
// Automatically includes Status field (always 1 for online tracking)
// Server marks device offline if no telemetry for >60 seconds
bool uploadTelemetry(float waterLevel, float currInflow, int pumpStatus);
```

---

## 11. WATER TANK CONTROL SCREEN

### File: `/home/user/appAndDevice/iot_water_tank/lib/screens/water_tank_control_screen.dart`
**Lines: 313, 319, 378**

```dart
final isOnline = device.isOnline; // Device online status from telemetry

// Used in header
_buildModernHeader(context, device, isOnline, isDarkMode),
```

---

## SUMMARY OF TIMEOUT CONSTANTS

| Constant | Location | Value | Purpose |
|----------|----------|-------|---------|
| TELEMETRY_UPLOAD_INTERVAL | config.h:112 | 30000 ms | How often device sends heartbeat |
| Server offline check | README.md:18 | >60 seconds | Server should mark offline after 60s no telemetry |
| App green threshold | device.dart:177 | 60 seconds | Show green if seen < 60s ago |
| App yellow threshold | device.dart:178 | 300 seconds | Show yellow if seen < 5m ago |
| App red threshold | device.dart:179 | >300 seconds | Show red if seen > 5m ago |

---

## CRITICAL PATH FOR STATUS UPDATE

**Expected flow (what SHOULD happen)**:
```
Device (every 30s)
  ↓ sends telemetry with Status=1
Server
  ↓ receives telemetry
  ↓ SHOULD update device.lastSeen = NOW() [NOT IMPLEMENTED]
  ↓ saves to database
App (on device list fetch)
  ↓ gets device JSON with lastSeen
  ↓ parses lastSeen timestamp
  ↓ calls device.getStatusColor()
  ↓ compares DateTime.now() - lastSeen
  ↓ displays green/yellow/red
User UI
  ↓ sees device status
```

**Actual flow (what IS happening)**:
```
Device (every 30s)
  ↓ sends telemetry with Status=1
Server
  ↓ receives telemetry
  ✗ NO CODE TO UPDATE device.lastSeen (MISSING)
App (on device list fetch)
  ↓ gets device JSON with stale lastSeen
  ↓ compares DateTime.now() - OLD_lastSeen
  ↓ displays wrong status color
User UI
  ↓ sees device marked "online" (GREEN) when it's actually offline
```

