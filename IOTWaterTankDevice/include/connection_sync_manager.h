#ifndef CONNECTION_SYNC_MANAGER_H
#define CONNECTION_SYNC_MANAGER_H

#include <Arduino.h>

// ============================================================================
// SYNC STATUS STRUCTURE
// ============================================================================

// Sync status management
struct ConnectionSyncStatus {
    bool serverSync;                  // true = connected to server, false = offline
    bool device_config_sync_status;   // true = sync FROM server, false = sync TO server (device priority)
    uint64_t lastServerTimestamp;     // Last server time in milliseconds (64-bit for timestamps > 4.2 billion)
    uint64_t millisAtSync;            // millis() value when last synced (64-bit for overflow tracking)
    uint32_t overflowCount;           // Handle millis() overflow (49 days)
};

// ============================================================================
// CALLBACK FUNCTION TYPES
// ============================================================================

// Callback for fetching config FROM server (returns true on success)
typedef bool (*FetchConfigCallback)(void* config);

// Callback for sending config TO server with priority (returns true on success)
typedef bool (*SendConfigPriorityCallback)(void* config);

// Callback for syncing time with server (returns timestamp in ms, 0 on failure)
typedef uint64_t (*SyncTimeCallback)();

// ============================================================================
// CONNECTION SYNC MANAGER CLASS
// ============================================================================

/**
 * ConnectionSyncManager - Reusable synchronization manager for IoT devices
 *
 * Handles:
 * - Online/offline state transitions
 * - Time synchronization with overflow protection
 * - Priority-based configuration sync
 * - Local modification tracking
 *
 * Usage:
 * 1. Create instance and set callbacks
 * 2. Call begin() to load stored sync status
 * 3. Call onDeviceOnline() when connection established
 * 4. Call onDeviceOffline() when connection lost
 * 5. Call markConfigModified() when local changes occur
 * 6. Use getCurrentTimestamp() for timestamping
 */
class ConnectionSyncManager {
public:
    ConnectionSyncManager();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    // Initialize sync manager (loads stored sync status)
    void begin();

    // Set callback functions for server communication
    void setFetchConfigCallback(FetchConfigCallback callback);
    void setSendConfigPriorityCallback(SendConfigPriorityCallback callback);
    void setSyncTimeCallback(SyncTimeCallback callback);

    // ========================================================================
    // ONLINE/OFFLINE TRANSITIONS
    // ========================================================================

    // Handle device coming online
    // - Syncs time with server
    // - If device_config_sync_status = true: fetches FROM server
    // - If device_config_sync_status = false: sends TO server with priority
    bool onDeviceOnline(void* config);

    // Handle device going offline
    void onDeviceOffline();

    // ========================================================================
    // CONFIGURATION SYNC
    // ========================================================================

    // Mark config as locally modified (sets device_config_sync_status = false)
    // Call this when device makes local changes that should override server
    void markConfigModified();

    // Reset config sync status (sets device_config_sync_status = true)
    // Call this after successfully syncing TO server
    void resetConfigSync();

    // ========================================================================
    // TIME SYNCHRONIZATION
    // ========================================================================

    // Sync time with server using callback
    bool syncTimeWithServer();

    // Manually set timestamp (for app-initiated time correction)
    void setTimestamp(uint64_t timestamp);

    // Get current Unix timestamp (milliseconds)
    // Returns calculated time based on lastServerTimestamp + (millis() - millisAtSync)
    uint64_t getCurrentTimestamp();

    // Check if time is synced
    bool isTimeSynced();

    // ========================================================================
    // STATUS QUERIES
    // ========================================================================

    // Get current sync status
    ConnectionSyncStatus getSyncStatus();

    // Check if server is online
    bool isServerOnline();

    // Check if device config needs sync TO server
    bool needsConfigUpload();

    // ========================================================================
    // STORAGE
    // ========================================================================

    // Save sync status to persistent storage
    void saveSyncStatus();

    // Load sync status from persistent storage
    void loadSyncStatus();

private:
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    ConnectionSyncStatus syncStatus;

    // Callbacks
    FetchConfigCallback fetchConfigCallback;
    SendConfigPriorityCallback sendConfigPriorityCallback;
    SyncTimeCallback syncTimeCallback;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    // Handle millis() overflow (every 49.7 days)
    void checkMillisOverflow();
};

#endif // CONNECTION_SYNC_MANAGER_H
