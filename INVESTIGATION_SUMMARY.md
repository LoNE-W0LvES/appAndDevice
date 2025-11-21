# Device Online/Offline Status Investigation

## Key Findings

### 1. Device Heartbeat & Telemetry Upload Mechanism

**Location**: `/home/user/appAndDevice/IOTWaterTankDevice/include/config.h` (Line 112)
```c
#define TELEMETRY_UPLOAD_INTERVAL 30000 // 30 seconds - upload data
```

**Implementation**: `/home/user/appAndDevice/IOTWaterTankDevice/src/main.cpp` (Lines 1585-1587)
```cpp
// Upload telemetry (every 30 seconds)
if (currentTime - lastTelemetryUpload >= TELEMETRY_UPLOAD_INTERVAL) {
    lastTelemetryUpload = currentTime;
    uploadTelemetry();
}
```

**Device sends telemetry every 30 seconds** ✓

---

### 2. Status Field in Telemetry

**Location**: `/home/user/appAndDevice/IOTWaterTankDevice/src/telemetry.cpp` (Lines 132-138)
```cpp
// Status (always 1 for online tracking)
// Server marks device offline if no telemetry for >60 seconds
JsonObject status = telemetryData.createNestedObject("Status");
status["key"] = "Status";
status["label"] = "Device Status";
status["type"] = "number";
status["value"] = 1;  // Always 1 (online)
```

**Device sends Status=1 with every telemetry upload** ✓

---

### 3. Server-Side Timeout Logic (Documentation)

**Location**: `/home/user/appAndDevice/IOTWaterTankDevice/README.md` (Line 18)
```
Online/Offline Tracking: Automatic status tracking - device marked offline if no telemetry for >60s
```

**Also mentioned in**:
- `/home/user/appAndDevice/IOTWaterTankDevice/src/telemetry.cpp` (Line 133)
- `/home/user/appAndDevice/IOTWaterTankDevice/include/api_client.h` (Line 98)
- `/home/user/appAndDevice/IOTWaterTankDevice/include/telemetry.h` (Line 33)

**Expected behavior**: Server marks device offline if no telemetry for >60 seconds ✓

---

### 4. App-Side Status Determination

**Location**: `/home/user/appAndDevice/iot_water_tank/lib/models/device.dart`

#### A. LastSeen Timestamp Field (Lines 18, 109-111, 141)
```dart
final DateTime? lastSeen;  // Last seen timestamp from server

// Parsing from JSON
lastSeen: json['lastSeen'] != null
    ? DateTime.parse(json['lastSeen'] as String)
    : null,

// Serializing to JSON
if (lastSeen != null) 'lastSeen': lastSeen!.toIso8601String(),
```

#### B. Status Determination Methods (Lines 170-199)

**getStatusColor()** (Lines 170-180):
```dart
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

**getStatusText()** (Lines 183-199):
```dart
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

#### C. Online Status from Telemetry (Lines 202-205)
```dart
/// Get device online status from telemetry Status field
bool get isOnline {
  final statusValue = telemetryData['Status']?.numberValue ?? 0.0;
  return statusValue > 0;
}
```

---

## IDENTIFIED ISSUES

### Issue #1: Missing Server-Side Implementation
**Severity**: CRITICAL

The code comments and device firmware repeatedly mention "Server marks device offline if no telemetry for >60 seconds" but:
- ✗ **NO actual server code found in this repository** to implement this logic
- ✗ **lastSeen timestamp is assumed to be updated by server** when telemetry is received
- ✗ **Unknown if server actually updates lastSeen field** when processing telemetry uploads

**Impact**: Device could remain marked as "online" indefinitely even when offline for days/weeks

---

### Issue #2: App Status Shows Green Even When Device Offline
**Severity**: HIGH

The app displays status based on `lastSeen` timestamp:
- Green: < 60 seconds ago
- Yellow: < 5 minutes ago  
- Red: > 5 minutes ago

BUT if the server **never updates lastSeen**, the app will show:
- Green forever (if lastSeen was recent)
- Or Yellow forever (depending on initial value)

Even when device is actually offline for hours/days.

---

### Issue #3: Multiple Status Fields (Confusion)
**Severity**: MEDIUM

The system has conflicting status indicators:

1. **`device.isOnline`** - Based on telemetry Status field (always 1)
   - Only shows offline when Status = 0 (which never happens)
   
2. **`device.getStatusColor()`** - Based on lastSeen timestamp
   - Shows green/yellow/red based on time elapsed
   - **Depends entirely on server updating lastSeen**

3. **Device online flag** (ESP32 side) - Based on NTP sync
   - Different concept (internet connectivity, not device activity)

---

### Issue #4: No Automatic lastSeen Updates on Server
**Severity**: HIGH

**Expected flow**:
1. Device sends telemetry every 30 seconds
2. Server receives telemetry
3. Server updates `device.lastSeen = NOW()`
4. App displays status based on lastSeen

**Actual flow**:
1. Device sends telemetry every 30 seconds ✓
2. Server receives telemetry ✓
3. Server updates... **UNKNOWN** (code not in repo)
4. App can't determine if device is truly online

---

### Issue #5: No Heartbeat Without Data
**Severity**: MEDIUM

If a device has no new telemetry data (sensor values haven't changed), it still sends Status=1 but:
- Other telemetry fields may be identical
- Server might skip updating if it deduplicates identical telemetry
- App would think device is offline if lastSeen stops being updated

---

## Recommended Fixes

### 1. CRITICAL: Implement Server-Side lastSeen Update
- When server receives telemetry POST to `/api/device-telemetry`
- Update `device.lastSeen = NOW()`
- This is the source of truth for device status

### 2. CRITICAL: Add Server-Side Offline Check Endpoint
- Implement `/api/devices/check-offline` (already defined in endpoints.h)
- Periodically mark devices as offline if `lastSeen < NOW() - 60 seconds`
- Or client-side: check `if (NOW() - lastSeen > 60 seconds) -> offline`

### 3: Remove Confusing Status Field
- The telemetry Status field (always=1) is redundant and misleading
- Use only lastSeen timestamp for online/offline status
- Or make Status=1 on device "seen at least once" and Status=0 for "never seen"

### 4: Implement Keep-Alive Mechanism
- Even with no sensor changes, send minimal telemetry every 30 seconds
- Ensures lastSeen is updated regularly
- Currently this IS happening, so ✓

---

## Status Check Thresholds

**Current logic in app**:
- Green (active): < 60 seconds
- Yellow (warning): < 5 minutes
- Red (offline): > 5 minutes

**Recommended timings** (to match backend):
- Green: < 60 seconds (matches server timeout)
- Yellow: < 120 seconds (approaching timeout)
- Red/Offline: > 60 seconds (matches server offline logic)

---

## Files to Review/Fix

1. ✗ **Backend API handler** for `/api/device-telemetry`
   - Must update `device.lastSeen = NOW()` when receiving telemetry
   - **NOT FOUND IN THIS REPO**

2. ✗ **Backend offline check logic** 
   - Must implement `/api/devices/check-offline` endpoint
   - Should mark devices as offline if `lastSeen < NOW() - 60 seconds`
   - **NOT FOUND IN THIS REPO**

3. ✓ **Device firmware** (telemetry upload)
   - Currently working correctly (30 second interval)
   - `/home/user/appAndDevice/IOTWaterTankDevice/src/main.cpp`

4. ✓ **Device firmware** (Status field)
   - Currently sending Status=1 with each telemetry
   - `/home/user/appAndDevice/IOTWaterTankDevice/src/telemetry.cpp`

5. ✓ **Flutter app** (status display)
   - Currently correctly displays status based on lastSeen
   - `/home/user/appAndDevice/iot_water_tank/lib/models/device.dart`

