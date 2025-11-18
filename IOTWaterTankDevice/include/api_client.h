#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "connection_sync_manager.h"
#include "device_config.h"
#include "telemetry.h"
#include "control_data.h"

// ============================================================================
// TYPE ALIASES
// ============================================================================

// Sync status management (alias for ConnectionSyncStatus for compatibility)
typedef ConnectionSyncStatus SyncStatus;

// ============================================================================
// API CLIENT CLASS
// ============================================================================

class APIClient {
public:
    APIClient();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    // Initialize API client with hardware ID
    void begin(const String& hardwareId);

    // ========================================================================
    // AUTHENTICATION
    // ========================================================================

    // Login device with username/password and obtain JWT token
    bool loginDevice(const String& username, const String& password);

    // Register device and obtain JWT token
    bool registerDevice();

    // Refresh JWT token (extends session)
    bool refreshToken();

    // Check if device is authenticated (has valid token)
    bool isAuthenticated();

    // Check if device is registered (has attempted login/registration before)
    bool isRegistered();

    // Get stored JWT token
    String getToken();

    // Manual token setting (for testing)
    void setToken(const String& token);

    // ========================================================================
    // CONFIGURATION SYNC (Priority-Based Conflict Resolution)
    // ========================================================================

    // Fetch config FROM server and apply if values changed
    // Used when device_config_sync_status = true (no local changes)
    bool fetchAndApplyServerConfig(DeviceConfig& config);

    // Send config TO server with priority (lastModified = 0)
    // Used when device_config_sync_status = false (local changes pending)
    bool sendConfigWithPriority(DeviceConfig& config);

    // Mark config as locally modified (triggers sync TO server)
    void markConfigModified();

    // Check if config values differ (ignores timestamps)
    bool configValuesChanged(const DeviceConfig& a, const DeviceConfig& b);

    // ========================================================================
    // CONTROL & TELEMETRY
    // ========================================================================

    // Fetch control data from server
    bool fetchControl(ControlData& control);

    // Upload telemetry data to server
    // Automatically includes Status field (always 1 for online tracking)
    // Server marks device offline if no telemetry for >60 seconds
    bool uploadTelemetry(float waterLevel, float currInflow, int pumpStatus);

    // ========================================================================
    // TIME SYNCHRONIZATION
    // ========================================================================

    // Sync time with server and update internal clock
    bool syncTimeWithServer();

    // Manually set timestamp (for app-initiated time correction)
    // Used when app detects significant drift and sends corrected time
    void setTimestamp(uint64_t timestamp);

    // Get current Unix timestamp (milliseconds)
    // Returns calculated time based on lastServerTimestamp + (millis() - millisAtSync)
    uint64_t getCurrentTimestamp();

    // Check if time is synced
    bool isTimeSynced();

    // ========================================================================
    // SYNC STATUS MANAGEMENT
    // ========================================================================

    // Handle device coming online (checks sync flag and syncs accordingly)
    void onDeviceOnline(DeviceConfig& config);

    // Handle device going offline
    void onDeviceOffline();

    // Get current sync status
    SyncStatus getSyncStatus();

    // Check if server is online
    bool isServerOnline();

    // ========================================================================
    // STORAGE
    // ========================================================================

    // Save sync status to preferences
    void saveSyncStatus();

    // Load sync status from preferences
    void loadSyncStatus();

private:
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    String deviceToken;
    String hardwareId;
    bool authenticated;

    // Specialized managers for different API operations
    DeviceConfigManager deviceConfigManager;
    TelemetryManager telemetryManager;
    ControlDataManager controlDataManager;

    // Connection sync manager (handles all sync logic)
    ConnectionSyncManager connSyncManager;

    // Static instance pointer for callbacks
    static APIClient* instance;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    // Callback wrappers for ConnectionSyncManager
    static bool fetchConfigCallbackWrapper(void* config);
    static bool sendConfigPriorityCallbackWrapper(void* config);
    static uint64_t syncTimeCallbackWrapper();

    // HTTP helper with retry logic
    bool httpRequest(const String& method, const String& endpoint,
                    const String& payload, String& response, int retries = API_RETRY_COUNT);

    // Load token from NVS storage
    bool loadToken();

    // Save token to NVS storage
    void saveToken(const String& token);

    // Add authorization header to HTTP client
    void addAuthHeader(HTTPClient& http);

    // Update manager tokens when authentication changes
    void updateManagerTokens();
};

#endif // API_CLIENT_H
