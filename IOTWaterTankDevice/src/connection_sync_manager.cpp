#include "connection_sync_manager.h"
#include "storage_manager.h"
#include "config.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ConnectionSyncManager::ConnectionSyncManager() {
    // Initialize sync status
    syncStatus.serverSync = false;
    syncStatus.device_config_sync_status = true;  // Default: sync FROM server
    syncStatus.lastServerTimestamp = 0;
    syncStatus.millisAtSync = 0;
    syncStatus.overflowCount = 0;

    // Initialize callbacks
    fetchConfigCallback = nullptr;
    sendConfigPriorityCallback = nullptr;
    syncTimeCallback = nullptr;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ConnectionSyncManager::begin() {
    DEBUG_PRINTLN("[ConnSync] Initializing Connection Sync Manager");
    loadSyncStatus();
}

void ConnectionSyncManager::setFetchConfigCallback(FetchConfigCallback callback) {
    fetchConfigCallback = callback;
}

void ConnectionSyncManager::setSendConfigPriorityCallback(SendConfigPriorityCallback callback) {
    sendConfigPriorityCallback = callback;
}

void ConnectionSyncManager::setSyncTimeCallback(SyncTimeCallback callback) {
    syncTimeCallback = callback;
}

// ============================================================================
// ONLINE/OFFLINE TRANSITIONS
// ============================================================================

bool ConnectionSyncManager::onDeviceOnline(void* config) {
    DEBUG_PRINTLN("[ConnSync] Device transitioning to ONLINE");

    // Sync time first
    if (!syncTimeWithServer()) {
        DEBUG_PRINTLN("[ConnSync] WARNING: Time sync failed, using stored time");
    }

    // Check sync direction
    if (syncStatus.device_config_sync_status) {
        // Sync FROM server (normal case)
        DEBUG_PRINTLN("[ConnSync] Syncing FROM server (device_config_sync_status = true)");

        if (fetchConfigCallback != nullptr) {
            if (fetchConfigCallback(config)) {
                DEBUG_PRINTLN("[ConnSync] Successfully fetched config from server");
                syncStatus.serverSync = true;
                saveSyncStatus();
                return true;
            } else {
                DEBUG_PRINTLN("[ConnSync] Failed to fetch config from server");
                return false;
            }
        } else {
            DEBUG_PRINTLN("[ConnSync] ERROR: Fetch config callback not set");
            return false;
        }
    } else {
        // Sync TO server (device has priority)
        DEBUG_PRINTLN("[ConnSync] Syncing TO server with priority (device_config_sync_status = false)");

        if (sendConfigPriorityCallback != nullptr) {
            if (sendConfigPriorityCallback(config)) {
                DEBUG_PRINTLN("[ConnSync] Successfully sent config to server with priority");

                // After success, reset sync status and fetch back to get timestamp
                syncStatus.device_config_sync_status = true;
                saveSyncStatus();

                // Fetch back from server to get updated timestamp
                if (fetchConfigCallback != nullptr) {
                    if (fetchConfigCallback(config)) {
                        DEBUG_PRINTLN("[ConnSync] Successfully fetched updated config from server");
                    } else {
                        DEBUG_PRINTLN("[ConnSync] WARNING: Failed to fetch updated config");
                    }
                }

                syncStatus.serverSync = true;
                saveSyncStatus();
                return true;
            } else {
                DEBUG_PRINTLN("[ConnSync] Failed to send config to server");
                return false;
            }
        } else {
            DEBUG_PRINTLN("[ConnSync] ERROR: Send config priority callback not set");
            return false;
        }
    }
}

void ConnectionSyncManager::onDeviceOffline() {
    DEBUG_PRINTLN("[ConnSync] Device transitioning to OFFLINE");
    syncStatus.serverSync = false;
    saveSyncStatus();
}

// ============================================================================
// CONFIGURATION SYNC
// ============================================================================

void ConnectionSyncManager::markConfigModified() {
    DEBUG_PRINTLN("[ConnSync] Config marked as modified (device priority)");
    syncStatus.device_config_sync_status = false;
    saveSyncStatus();
}

void ConnectionSyncManager::resetConfigSync() {
    DEBUG_PRINTLN("[ConnSync] Config sync reset (server sync)");
    syncStatus.device_config_sync_status = true;
    saveSyncStatus();
}

// ============================================================================
// TIME SYNCHRONIZATION
// ============================================================================

bool ConnectionSyncManager::syncTimeWithServer() {
    DEBUG_PRINTLN("[ConnSync] Syncing time with server...");

    if (syncTimeCallback == nullptr) {
        DEBUG_PRINTLN("[ConnSync] ERROR: Sync time callback not set");
        return false;
    }

    uint64_t serverTime = syncTimeCallback();

    if (serverTime > 0) {
        syncStatus.lastServerTimestamp = serverTime;
        syncStatus.millisAtSync = millis();
        syncStatus.overflowCount = 0;

        DEBUG_PRINTF("[ConnSync] Time synced: %llu ms\n", serverTime);

        saveSyncStatus();
        return true;
    } else {
        DEBUG_PRINTLN("[ConnSync] Failed to sync time with server");
        return false;
    }
}

void ConnectionSyncManager::setTimestamp(uint64_t timestamp) {
    DEBUG_PRINTF("[ConnSync] Manually setting timestamp: %llu\n", timestamp);

    syncStatus.lastServerTimestamp = timestamp;
    syncStatus.millisAtSync = millis();
    syncStatus.overflowCount = 0;  // Reset overflow counter

    saveSyncStatus();
    DEBUG_PRINTLN("[ConnSync] Time sync updated via manual correction");
}

uint64_t ConnectionSyncManager::getCurrentTimestamp() {
    checkMillisOverflow();

    uint64_t currentMillis = millis();
    uint64_t elapsedMillis = currentMillis - syncStatus.millisAtSync;

    // Add overflow compensation
    uint64_t overflowCompensation = (uint64_t)syncStatus.overflowCount * 4294967296ULL;

    return syncStatus.lastServerTimestamp + elapsedMillis + overflowCompensation;
}

bool ConnectionSyncManager::isTimeSynced() {
    return syncStatus.lastServerTimestamp > 0;
}

// ============================================================================
// STATUS QUERIES
// ============================================================================

ConnectionSyncStatus ConnectionSyncManager::getSyncStatus() {
    return syncStatus;
}

bool ConnectionSyncManager::isServerOnline() {
    return syncStatus.serverSync;
}

bool ConnectionSyncManager::needsConfigUpload() {
    return !syncStatus.device_config_sync_status;
}

// ============================================================================
// STORAGE
// ============================================================================

void ConnectionSyncManager::saveSyncStatus() {
    storageManager.saveServerSync(syncStatus.serverSync);
    storageManager.saveConfigSync(syncStatus.device_config_sync_status);
    storageManager.saveServerTime(syncStatus.lastServerTimestamp);
    storageManager.saveMillisSync(syncStatus.millisAtSync);
    storageManager.saveOverflowCount(syncStatus.overflowCount);

    DEBUG_PRINTLN("[ConnSync] Sync status saved to storage");
}

void ConnectionSyncManager::loadSyncStatus() {
    syncStatus.serverSync = storageManager.getServerSync();
    syncStatus.device_config_sync_status = storageManager.getConfigSync();
    syncStatus.lastServerTimestamp = storageManager.getServerTime();
    syncStatus.millisAtSync = storageManager.getMillisSync();
    syncStatus.overflowCount = storageManager.getOverflowCount();

    DEBUG_PRINTLN("[ConnSync] Sync status loaded from storage:");
    DEBUG_PRINTF("[ConnSync]   serverSync: %s\n", syncStatus.serverSync ? "true" : "false");
    DEBUG_PRINTF("[ConnSync]   device_config_sync_status: %s\n", syncStatus.device_config_sync_status ? "true" : "false");
    DEBUG_PRINTF("[ConnSync]   lastServerTimestamp: %llu\n", syncStatus.lastServerTimestamp);
    DEBUG_PRINTF("[ConnSync]   millisAtSync: %llu\n", syncStatus.millisAtSync);
    DEBUG_PRINTF("[ConnSync]   overflowCount: %u\n", syncStatus.overflowCount);
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void ConnectionSyncManager::checkMillisOverflow() {
    static unsigned long lastMillis = 0;
    unsigned long currentMillis = millis();

    // Detect overflow (millis() wrapped around)
    if (currentMillis < lastMillis) {
        syncStatus.overflowCount++;
        DEBUG_PRINTF("[ConnSync] millis() overflow detected! Count: %u\n", syncStatus.overflowCount);
        saveSyncStatus();
    }

    lastMillis = currentMillis;
}
