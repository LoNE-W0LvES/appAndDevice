# App-Device Configuration Sync Logic

## Overview
This document specifies the app-side logic for communicating directly with ESP32-S3 AquaFlow devices when the server is offline or unreachable. The device implements the same 3-rule update logic as the server for consistency.

## Use Cases

### When to Use Direct Device Communication

1. **Server Offline:** Backend server is down or unreachable
2. **No Internet:** Device connected to WiFi but no internet access
3. **Local Network Only:** Device and phone on same WiFi network
4. **Offline Mode:** App operating in offline mode with local device control

## Three Update Rules (Same as Server)

The device webserver implements the exact same 3-rule logic:

### Rule 1: Priority Flag (timestamp == 0)
**Condition:** `incoming.timestamp == 0 AND incoming.value != current.value`

**Action:** ALWAYS accept the incoming value

**Use Case:** App has critical changes that must override device state

### Rule 2: Value Unchanged (Optimization)
**Condition:** `incoming.value == current.value`

**Action:** SKIP update entirely (no flash write)

**Use Case:** Prevent unnecessary device flash writes when values identical

### Rule 3: Last-Write-Wins (Normal Case)
**Condition:** `incoming.value != current.value AND timestamps > 0`

**Action:** Use the value with the NEWER timestamp

**Use Case:** Normal conflict resolution when both have valid timestamps

## Device Endpoints

### Base URL
```
http://<device-ip>/<device-id>
```

Example:
```
http://192.168.1.100/wt001-DEV-02-690e6a9d092433c0acfb9178
```

### GET /{device_id}/config
Fetch current device configuration

**Response:**
```json
{
  "upperThreshold": {
    "key": "upperThreshold",
    "value": 85.0,
    "timestamp": 1737123456789
  },
  "lowerThreshold": {
    "key": "lowerThreshold",
    "value": 20.0,
    "timestamp": 1737123456789
  }
  // ... other fields
}
```

### POST /{device_id}/config
Update device configuration (implements 3-rule logic)

**Request:**
```json
{
  "upperThreshold": {
    "key": "upperThreshold",
    "value": 90.0,
    "timestamp": 0  // 0 = priority flag, or Unix timestamp in ms
  },
  "lowerThreshold": {
    "key": "lowerThreshold",
    "value": 25.0,
    "timestamp": 0
  }
  // ... other fields (optional, can update subset)
}
```

**Success Response (200 OK):**
```json
{
  "success": true,
  "timestamp": 1737123999999,
  "message": "Config updated with priority"
}
```

**No Changes Response (200 OK):**
```json
{
  "success": true,
  "timestamp": 1737123456789,
  "message": "No changes - values identical"
}
```

**Rejection Response (409 Conflict):**
```json
{
  "success": false,
  "error": "STALE_TIMESTAMP",
  "message": "Current config is newer",
  "timestamp": 1737123999999
}
```

### GET /{device_id}/timestamp
Get device timestamp and sync status

**Response:**
```json
{
  "timestamp": 1737123999999,  // Current Unix time (millis() if not synced)
  "millis": 45000,              // Device uptime
  "synced": true,               // Has valid time sync
  "lastSync": 1737123456000,    // When last synced with server
  "drift": 45000                // Estimated drift in ms
}
```

### GET /{device_id}/telemetry
Get current sensor readings

**Response:**
```json
{
  "waterLevel": {
    "key": "waterLevel",
    "value": 45.5
  },
  "currInflow": {
    "key": "currInflow",
    "value": 5.2
  },
  "pumpStatus": {
    "key": "pumpStatus",
    "value": 1
  },
  "Status": {
    "key": "Status",
    "value": 1
  }
}
```

### GET /{device_id}/control
Get control status

**Response:**
```json
{
  "pumpSwitch": {
    "key": "pumpSwitch",
    "value": true
  },
  "config_update": {
    "key": "config_update",
    "value": false
  }
}
```

## App Implementation Guide

### Step 1: Detect Connectivity Mode

```typescript
enum ConnectivityMode {
  ONLINE_SERVER,    // Server reachable
  OFFLINE_DEVICE,   // Server offline, device on local network
  FULLY_OFFLINE     // No server, no device
}

async function detectConnectivity(): Promise<ConnectivityMode> {
  // Try server first
  try {
    await fetch('https://api.aquaflow.com/health', { timeout: 5000 });
    return ConnectivityMode.ONLINE_SERVER;
  } catch (serverError) {
    // Server unreachable, try device
    try {
      const deviceIp = getDeviceIpFromCache();
      await fetch(`http://${deviceIp}/${deviceId}/timestamp`, { timeout: 3000 });
      return ConnectivityMode.OFFLINE_DEVICE;
    } catch (deviceError) {
      return ConnectivityMode.FULLY_OFFLINE;
    }
  }
}
```

### Step 2: Implement Config Update with Priority

```typescript
interface ConfigUpdate {
  [field: string]: {
    key: string;
    value: any;
    timestamp: number;  // 0 for priority flag
  };
}

async function updateConfig(
  mode: ConnectivityMode,
  updates: ConfigUpdate,
  usePriority: boolean = false
): Promise<boolean> {

  const timestamp = usePriority ? 0 : Date.now();

  // Add timestamp to all fields
  Object.keys(updates).forEach(field => {
    updates[field].timestamp = timestamp;
  });

  if (mode === ConnectivityMode.ONLINE_SERVER) {
    // Update via server
    const response = await fetch('https://api.aquaflow.com/device/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ deviceId, ...updates })
    });
    return response.ok;
  }
  else if (mode === ConnectivityMode.OFFLINE_DEVICE) {
    // Update via local device
    const deviceIp = getDeviceIpFromCache();
    const response = await fetch(`http://${deviceIp}/${deviceId}/config`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(updates)
    });

    if (response.ok) {
      // Device accepted - store for later server sync
      storeOfflineChange(updates);
      return true;
    }
    return false;
  }
  else {
    // Fully offline - queue for later
    queueOfflineChange(updates);
    return true;
  }
}
```

### Step 3: Handle Sync When Coming Back Online

```typescript
async function syncOnReconnect() {
  const mode = await detectConnectivity();

  if (mode === ConnectivityMode.ONLINE_SERVER) {
    // Sync queued changes with server
    const queuedChanges = getQueuedChanges();

    for (const change of queuedChanges) {
      // Send with priority flag (timestamp=0) to ensure device changes win
      const updates = { ...change, timestamp: 0 };
      await updateConfig(mode, updates, true);
    }

    clearQueuedChanges();
  }
}
```

### Step 4: Fetch Device Timestamp

```typescript
async function getDeviceTimestamp(deviceIp: string): Promise<number> {
  const response = await fetch(`http://${deviceIp}/${deviceId}/timestamp`);
  const data = await response.json();

  if (data.synced) {
    // Device has valid server time
    return data.timestamp;
  } else {
    // Device never synced - use millis() for relative timing
    return data.millis;
  }
}
```

## Sync Scenarios

### Scenario 1: Server Offline, App Changes Config
```
1. App detects server offline
2. App detects device on local network (192.168.1.100)
3. User changes upperThreshold to 80 in app
4. App sends POST to device with timestamp=0 (priority)
5. Device accepts, marks as locally modified
6. Server comes back online
7. Device syncs TO server with priority flag
8. Server accepts device value (80)
✅ App change preserved through offline period
```

### Scenario 2: Values Identical (Optimization)
```
1. App: upperThreshold = 90
2. Device: upperThreshold = 90
3. App sends update
4. Device applies RULE 2: values identical
5. Device responds with success but no flash write
✅ No unnecessary device flash wear
```

### Scenario 3: Conflicting Changes (Last-Write-Wins)
```
1. Device: upperThreshold = 85, timestamp = 1000
2. User changes threshold locally via buttons: 80, timestamp = 2000
3. App sends update: 90, timestamp = 1500
4. Device applies RULE 3: compares timestamps
5. Device keeps 80 (newer timestamp 2000)
6. Device responds with 409 Conflict
✅ Most recent change wins
```

### Scenario 4: No Internet, Local Network Only
```
1. Device connected to WiFi, but router has no internet
2. Server unreachable
3. App and phone on same WiFi network
4. App communicates directly with device via local IP
5. All config changes go directly to device
6. Device marks changes as locally modified
7. When internet returns, device syncs TO server
✅ Full functionality without internet
```

## Configuration Fields

All fields use the nested structure `{key, value, timestamp}`:

1. **upperThreshold** (float) - Water level % to turn pump OFF
2. **lowerThreshold** (float) - Water level % to turn pump ON
3. **tankHeight** (float) - Tank height in cm
4. **tankWidth** (float) - Tank width/diameter in cm
5. **tankShape** (string) - "CYLINDRICAL" or "RECTANGULAR"
6. **UsedTotal** (float) - Total water consumed in liters (capital U)
7. **maxInflow** (float) - Maximum inflow rate in L/min
8. **force_update** (bool) - Force config update flag
9. **ip_address** (string) - Device local IP address

## Device Behavior After App Update

When app updates config via POST:

1. **Device applies 3-rule logic** (same as server)
2. **Calls `markConfigModified()`** to set `device_config_sync_status = false`
3. **Stores in NVS** (persists across reboot)
4. **On server reconnect:**
   - Checks `device_config_sync_status = false`
   - Sends config TO server with `timestamp=0` (priority)
   - Server accepts device changes
   - Sets `device_config_sync_status = true`

## Error Handling

```typescript
try {
  const response = await fetch(deviceUrl, { ... });

  if (response.status === 200) {
    const data = await response.json();
    if (data.success) {
      console.log('Config updated successfully');
    } else {
      console.log('No changes needed:', data.message);
    }
  } else if (response.status === 409) {
    // Conflict - device has newer data
    const data = await response.json();
    console.log('Update rejected:', data.message);
    // Fetch device config and update app UI
    await fetchDeviceConfig();
  } else if (response.status === 400) {
    console.error('Invalid request format');
  } else {
    console.error('Unknown error:', response.status);
  }
} catch (error) {
  // Network error - device unreachable
  console.error('Device unreachable:', error);
  // Queue change for later or notify user
}
```

## Best Practices

### 1. Always Check Connectivity First
```typescript
const mode = await detectConnectivity();
// Use appropriate endpoint based on mode
```

### 2. Use Priority Flag for User-Initiated Changes
```typescript
// User directly changed setting
await updateConfig(mode, updates, usePriority: true);
```

### 3. Cache Device IP for Offline Mode
```typescript
// When device first discovered
localStorage.setItem(`device_${deviceId}_ip`, deviceIp);

// When server offline
const cachedIp = localStorage.getItem(`device_${deviceId}_ip`);
```

### 4. Implement Retry Logic with Exponential Backoff
```typescript
async function fetchWithRetry(url: string, maxRetries = 3) {
  for (let attempt = 0; attempt < maxRetries; attempt++) {
    try {
      return await fetch(url, { timeout: 5000 });
    } catch (error) {
      if (attempt === maxRetries - 1) throw error;
      await delay(Math.pow(2, attempt) * 1000);  // 1s, 2s, 4s
    }
  }
}
```

### 5. Sync Queued Changes When Online
```typescript
window.addEventListener('online', () => {
  syncOnReconnect();
});
```

## Security Considerations

1. **Local Network Only:** Device endpoints only accessible on same WiFi network
2. **No Authentication:** Local endpoints don't require JWT (device is physically secured)
3. **CORS Enabled:** Device allows cross-origin requests from app
4. **Input Validation:** Device validates all incoming values before applying
5. **Rate Limiting:** App should implement local rate limiting

## Testing Checklist

- [ ] App can fetch config from device when server offline
- [ ] App can update config on device when server offline
- [ ] Priority flag (timestamp=0) always wins if values differ
- [ ] Identical values are skipped (no flash write)
- [ ] Last-Write-Wins works for normal timestamps
- [ ] Device syncs changes TO server when reconnecting
- [ ] App handles 409 Conflict responses gracefully
- [ ] App queues changes when fully offline
- [ ] App syncs queue when coming back online
- [ ] Device IP caching works across app restarts

## Example App Flow

```typescript
class ConfigSyncManager {
  async updateThreshold(upper: number, lower: number) {
    // 1. Detect connectivity
    const mode = await this.detectConnectivity();

    // 2. Prepare update
    const updates = {
      upperThreshold: {
        key: 'upperThreshold',
        value: upper,
        timestamp: 0  // Priority flag for user changes
      },
      lowerThreshold: {
        key: 'lowerThreshold',
        value: lower,
        timestamp: 0
      }
    };

    // 3. Send to appropriate endpoint
    if (mode === ConnectivityMode.ONLINE_SERVER) {
      await this.updateViaServer(updates);
    } else if (mode === ConnectivityMode.OFFLINE_DEVICE) {
      await this.updateViaDevice(updates);
    } else {
      await this.queueForLater(updates);
    }
  }

  private async updateViaDevice(updates: ConfigUpdate) {
    const deviceIp = this.getDeviceIp();
    const url = `http://${deviceIp}/${this.deviceId}/config`;

    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(updates)
    });

    if (response.ok) {
      const data = await response.json();
      console.log('Device updated:', data.message);

      // Store for server sync when online
      this.storeOfflineChange(updates);
    } else if (response.status === 409) {
      // Device has newer data
      console.log('Conflict - fetching device state');
      await this.fetchDeviceConfig();
    }
  }
}
```

## Summary

The device webserver implements the exact same 3-rule logic as the server, ensuring consistent behavior whether the app communicates with the server or device directly. This enables seamless offline operation and automatic synchronization when connectivity is restored.

**Key Points:**
- ✅ Same 3-rule logic as server
- ✅ Works when server offline
- ✅ Automatic sync on reconnect
- ✅ Priority flag for critical changes
- ✅ Value comparison optimization
- ✅ Conflict resolution via timestamps
