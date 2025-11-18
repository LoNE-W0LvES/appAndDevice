# ConnectionSyncManager - Reusable IoT Synchronization Component

## Overview

`ConnectionSyncManager` is a standalone, reusable component for managing connection synchronization in IoT devices. It handles time synchronization, configuration sync with priority-based conflict resolution, and online/offline state transitions.

## Features

- ✅ **Time Synchronization** with milliseconds precision
- ✅ **Priority-Based Config Sync** (device vs server)
- ✅ **Overflow Protection** for long-running devices (millis() overflow after 49.7 days)
- ✅ **Persistent Storage** integration
- ✅ **Online/Offline Transitions** with automatic sync
- ✅ **Callback-Based Architecture** for maximum flexibility
- ✅ **Zero Dependencies** on specific API implementations

## Architecture

```
┌─────────────────────────────────────┐
│      Your Application               │
│  (API Client, MQTT Client, etc.)    │
└──────────────┬──────────────────────┘
               │
               │ Uses
               ▼
┌─────────────────────────────────────┐
│   ConnectionSyncManager             │
│  - Time synchronization             │
│  - Config sync logic                │
│  - Online/offline handling          │
└──────────────┬──────────────────────┘
               │
               │ Callbacks
               ▼
┌─────────────────────────────────────┐
│   Your Server Communication Layer   │
│  - HTTP, MQTT, CoAP, WebSocket      │
│  - Custom protocol                  │
└─────────────────────────────────────┘
```

## Quick Start

### 1. Copy Files to Your Project

```
include/connection_sync_manager.h
src/connection_sync_manager.cpp
```

### 2. Implement Storage Interface

The manager expects a `storageManager` global instance with these methods:

```cpp
// Required storage methods:
void saveServerSync(bool synced);
bool getServerSync();
void saveConfigSync(bool synced);
bool getConfigSync();
void saveServerTime(unsigned long timestamp);
unsigned long getServerTime();
void saveMillisSync(unsigned long millis);
unsigned long getMillisSync();
void saveOverflowCount(uint32_t count);
uint32_t getOverflowCount();
```

### 3. Create Callback Functions

Define three callbacks for server communication:

```cpp
// Callback to fetch config FROM server
bool fetchConfigCallback(void* config) {
    MyConfig* cfg = (MyConfig*)config;
    // Implement your server fetch logic
    // Return true on success, false on failure
}

// Callback to send config TO server with priority
bool sendConfigPriorityCallback(void* config) {
    MyConfig* cfg = (MyConfig*)config;
    // Implement your server send logic
    // Return true on success, false on failure
}

// Callback to sync time with server
unsigned long syncTimeCallback() {
    // Fetch server time
    // Return timestamp in milliseconds, 0 on failure
}
```

### 4. Initialize and Configure

```cpp
#include "connection_sync_manager.h"

ConnectionSyncManager syncManager;

void setup() {
    // Initialize
    syncManager.begin();

    // Set callbacks
    syncManager.setFetchConfigCallback(fetchConfigCallback);
    syncManager.setSendConfigPriorityCallback(sendConfigPriorityCallback);
    syncManager.setSyncTimeCallback(syncTimeCallback);
}
```

### 5. Use in Your Application

```cpp
// When device connects to network/server
void onConnected() {
    MyConfig config;
    syncManager.onDeviceOnline(&config);
    // Config is now synchronized
}

// When device disconnects
void onDisconnected() {
    syncManager.onDeviceOffline();
}

// When user makes local changes
void onLocalConfigChange() {
    syncManager.markConfigModified();
    // Next time device goes online, local changes will be uploaded
}

// Get current timestamp
unsigned long now = syncManager.getCurrentTimestamp();

// Manual time correction (from external source)
syncManager.setTimestamp(newTimestamp);
```

## Synchronization Logic

### Priority-Based Configuration Sync

The manager uses a flag-based sync strategy:

**device_config_sync_status = true** (Server Priority)
- Device fetches configuration FROM server
- Server values override local values
- Used after successful upload or on first connection

**device_config_sync_status = false** (Device Priority)
- Device sends configuration TO server with priority flag
- Local values override server values
- Used when local changes are made offline

### Online Transition Flow

```
Device Connects
     │
     ├─> Sync Time with Server
     │
     ├─> Check device_config_sync_status
     │
     ├─> TRUE: Fetch FROM server
     │          └─> Apply server config
     │
     └─> FALSE: Send TO server with priority
                └─> Fetch back to get timestamp
                └─> Set sync status to TRUE
```

### Time Synchronization

- Stores server timestamp + local millis() at sync point
- Calculates current time: `serverTimestamp + (millis() - millisAtSync)`
- Handles millis() overflow automatically (49.7 day cycles)
- Supports manual time correction from external sources

## API Reference

### Initialization

```cpp
void begin()
```
Initialize sync manager and load stored sync status.

```cpp
void setFetchConfigCallback(FetchConfigCallback callback)
void setSendConfigPriorityCallback(SendConfigPriorityCallback callback)
void setSyncTimeCallback(SyncTimeCallback callback)
```
Set callback functions for server communication.

### Online/Offline Management

```cpp
bool onDeviceOnline(void* config)
```
Handle device coming online. Syncs time and config based on priority flag.
- Returns: true on success, false on failure

```cpp
void onDeviceOffline()
```
Handle device going offline. Updates internal state.

### Configuration Management

```cpp
void markConfigModified()
```
Mark configuration as locally modified. Sets `device_config_sync_status = false`.
Call this when device makes local changes that should override server.

```cpp
void resetConfigSync()
```
Reset config sync status. Sets `device_config_sync_status = true`.
Usually called automatically after successful upload.

### Time Synchronization

```cpp
bool syncTimeWithServer()
```
Sync time with server using callback.
- Returns: true on success, false on failure

```cpp
void setTimestamp(unsigned long timestamp)
```
Manually set timestamp (for app-initiated time correction).
- Parameters: `timestamp` - Unix timestamp in milliseconds

```cpp
unsigned long getCurrentTimestamp()
```
Get current Unix timestamp in milliseconds with overflow protection.
- Returns: Current timestamp or 0 if never synced

```cpp
bool isTimeSynced()
```
Check if time has been synced at least once.
- Returns: true if synced, false otherwise

### Status Queries

```cpp
ConnectionSyncStatus getSyncStatus()
```
Get current sync status structure.

```cpp
bool isServerOnline()
```
Check if device is currently online (connected to server).
- Returns: true if online, false if offline

```cpp
bool needsConfigUpload()
```
Check if device has pending config changes to upload.
- Returns: true if `device_config_sync_status = false`

### Storage

```cpp
void saveSyncStatus()
void loadSyncStatus()
```
Save/load sync status to/from persistent storage.

## Data Structures

### ConnectionSyncStatus

```cpp
struct ConnectionSyncStatus {
    bool serverSync;                  // true = online, false = offline
    bool device_config_sync_status;   // true = sync FROM server, false = sync TO server
    unsigned long lastServerTimestamp; // Last server time (ms)
    unsigned long millisAtSync;       // millis() at last sync
    uint32_t overflowCount;           // millis() overflow counter
};
```

## Integration Example: HTTP API Client

```cpp
class MyAPIClient {
private:
    ConnectionSyncManager syncManager;
    HTTPClient http;

    // Static instance for callbacks
    static MyAPIClient* instance;

    // Callback wrappers
    static bool fetchConfigCallback(void* config) {
        return instance->fetchConfigFromServer((MyConfig*)config);
    }

    static bool sendConfigCallback(void* config) {
        return instance->sendConfigToServer((MyConfig*)config);
    }

    static unsigned long syncTimeCallback() {
        return instance->fetchServerTime();
    }

    bool fetchConfigFromServer(MyConfig* config) {
        // HTTP GET to fetch config
        http.begin("https://api.example.com/config");
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            // Parse JSON to config
            return true;
        }
        return false;
    }

    bool sendConfigToServer(MyConfig* config) {
        // HTTP POST to send config with priority
        String json = buildConfigJSON(config, true); // priority=true
        http.begin("https://api.example.com/config");
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(json);
        return (httpCode == 200);
    }

    unsigned long fetchServerTime() {
        // HTTP GET to fetch server time
        http.begin("https://api.example.com/time");
        int httpCode = http.GET();
        if (httpCode == 200) {
            String payload = http.getString();
            // Parse timestamp from response
            return parseTimestamp(payload);
        }
        return 0;
    }

public:
    void begin() {
        instance = this;

        syncManager.begin();
        syncManager.setFetchConfigCallback(fetchConfigCallback);
        syncManager.setSendConfigPriorityCallback(sendConfigCallback);
        syncManager.setSyncTimeCallback(syncTimeCallback);
    }

    void onWiFiConnected(MyConfig& config) {
        syncManager.onDeviceOnline(&config);
    }

    void onWiFiDisconnected() {
        syncManager.onDeviceOffline();
    }

    void onUserChangedConfig() {
        syncManager.markConfigModified();
    }
};

MyAPIClient* MyAPIClient::instance = nullptr;
```

## Integration Example: MQTT Client

```cpp
class MyMQTTClient {
private:
    ConnectionSyncManager syncManager;
    PubSubClient mqtt;

    static MyMQTTClient* instance;

    static bool fetchConfigCallback(void* config) {
        // Publish request, wait for response
        instance->mqtt.publish("device/config/request", "");
        // Wait for response on "device/config/response" topic
        // Parse and apply config
        return true;
    }

    static bool sendConfigCallback(void* config) {
        // Publish config with priority flag
        String json = buildConfigJSON((MyConfig*)config, true);
        return instance->mqtt.publish("device/config/update", json.c_str());
    }

    static unsigned long syncTimeCallback() {
        // Request time sync via MQTT
        instance->mqtt.publish("device/time/request", "");
        // Wait for response
        // Return timestamp
        return 0;
    }

public:
    void begin() {
        instance = this;
        syncManager.begin();
        syncManager.setFetchConfigCallback(fetchConfigCallback);
        syncManager.setSendConfigPriorityCallback(sendConfigCallback);
        syncManager.setSyncTimeCallback(syncTimeCallback);
    }
};
```

## Best Practices

1. **Always Call begin()** - Initialize before using any other methods
2. **Set All Callbacks** - All three callbacks should be set before calling `onDeviceOnline()`
3. **Handle Failures Gracefully** - Callbacks should return false on failure
4. **Save After Local Changes** - Call `markConfigModified()` immediately after local config changes
5. **Time Sync First** - Time synchronization happens automatically in `onDeviceOnline()`
6. **Periodic Time Sync** - Consider syncing time periodically (e.g., every 24 hours)
7. **Test Overflow Handling** - Verify millis() overflow handling for long-running devices

## Debugging

Enable debug output by defining `DEBUG_ENABLED` in your config:

```cpp
#define DEBUG_ENABLED
```

Debug messages use these macros:
```cpp
DEBUG_PRINTLN("[ConnSync] Message")
DEBUG_PRINTF("[ConnSync] Value: %d\n", value)
```

## Requirements

- Arduino Framework or compatible platform
- Storage backend implementing the storage interface
- Network/communication library for callbacks

## License

This component is part of the IOT Water Tank Device project and can be freely reused in other projects.

## Support

For issues, questions, or contributions, please refer to the main project repository.
