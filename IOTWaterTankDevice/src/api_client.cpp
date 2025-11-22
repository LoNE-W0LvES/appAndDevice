#include "api_client.h"
#include "storage_manager.h"
#include "handle_control_data.h"
#include "handle_config_data.h"
#include "handle_telemetry_data.h"
#include <time.h>

// External handler instances (defined in main.cpp)
extern ControlDataHandler controlHandler;
extern ConfigDataHandler configHandler;
extern TelemetryDataHandler telemetryHandler;

// Initialize static instance pointer
APIClient* APIClient::instance = nullptr;

// ============================================================================
// CONFIGURATION SYNC IMPLEMENTATION
// ============================================================================
//
// This file implements the device-side logic for configuration synchronization
// with the server. The server uses a 3-rule update logic:
//
// RULE 1: lastModified == 0 → Always update (priority flag, if value differs)
// RULE 2: Value unchanged → Skip update (even if timestamp differs)
// RULE 3: Value changed → Compare timestamps (Last-Write-Wins)
//
// See SERVER_SYNC_LOGIC.md for complete server-side specification.
//
// Device behavior:
// - device_config_sync_status = true  → Fetch FROM server
// - device_config_sync_status = false → Send TO server with priority (timestamp=0)
//
// ============================================================================

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

APIClient::APIClient() : authenticated(false) {
    // Set instance pointer for callbacks
    instance = this;
}

void APIClient::begin(const String& hwId) {
    hardwareId = hwId;

    // Load authentication token
    deviceToken = storageManager.getDeviceToken();
    if (deviceToken.length() > 0) {
        authenticated = true;
        Serial.println("[API] Loaded existing device token");
    } else {
        Serial.println("[API] No existing token found, registration required");
    }

    // Initialize specialized managers with token and hardware ID
    updateManagerTokens();

    // Initialize connection sync manager
    connSyncManager.begin();

    // Setup callbacks for ConnectionSyncManager
    connSyncManager.setFetchConfigCallback(fetchConfigCallbackWrapper);
    connSyncManager.setSendConfigPriorityCallback(sendConfigPriorityCallbackWrapper);
    connSyncManager.setSyncTimeCallback(syncTimeCallbackWrapper);

    Serial.println("[API] API Client initialized with specialized managers");

    // Print sync status
    ConnectionSyncStatus status = connSyncManager.getSyncStatus();
    Serial.println("  Server Sync: " + String(status.serverSync ? "true" : "false"));
    Serial.println("  Config Sync Status: " + String(status.device_config_sync_status ? "FROM server" : "TO server"));
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

bool APIClient::loadToken() {
    deviceToken = storageManager.getDeviceToken();
    return deviceToken.length() > 0;
}

void APIClient::saveToken(const String& token) {
    storageManager.saveDeviceToken(token);
    deviceToken = token;
    Serial.println("[API] Device token saved");
}

void APIClient::addAuthHeader(HTTPClient& http) {
    String authHeader = "Bearer " + deviceToken;
    http.addHeader("Authorization", authHeader);
    DEBUG_PRINTLN("[API] Added JWT Authorization header");
    DEBUG_PRINTF("[API] Token: %s...%s\n",
                 deviceToken.substring(0, 10).c_str(),
                 deviceToken.substring(deviceToken.length() - 10).c_str());
}

bool APIClient::isAuthenticated() {
    return authenticated;
}

String APIClient::getToken() {
    return deviceToken;
}

void APIClient::setToken(const String& token) {
    deviceToken = token;
    authenticated = true;
    saveToken(token);
    updateManagerTokens();
}

bool APIClient::loginDevice(const String& username, const String& password) {
    Serial.println("[API] Attempting device login...");

    // Debug: Print credentials being used
    Serial.println("[API] Login Credentials:");
    Serial.println("  Username: " + String(username));
    Serial.println("  Password: " + String(password.length() > 0 ? "********" : "[EMPTY]") + " (length: " + String(password.length()) + ")");
    Serial.println("  DeviceId: " + String(DEVICE_ID));
    Serial.println("  HardwareId: " + hardwareId);
    Serial.println("  DeviceName: " + String(DEVICE_NAME));

    // Build login payload with deviceId and hardwareId (required by backend)
    StaticJsonDocument<512> doc;
    doc["username"] = username;
    doc["password"] = password;
    doc["deviceId"] = DEVICE_ID;        // Required by backend
    doc["hardwareId"] = hardwareId;      // Required by backend
    doc["deviceName"] = DEVICE_NAME;     // Optional: update device name

    String payload;
    serializeJson(doc, payload);

    // Debug: Print full payload (with masked password)
    Serial.println("[API] Login payload:");
    StaticJsonDocument<512> debugDoc;
    debugDoc["username"] = username;
    debugDoc["password"] = "********";
    debugDoc["deviceId"] = DEVICE_ID;
    debugDoc["hardwareId"] = hardwareId;
    debugDoc["deviceName"] = DEVICE_NAME;
    serializeJsonPretty(debugDoc, Serial);
    Serial.println();

    // Send login request
    String response;
    if (!httpRequest("POST", API_DEVICE_LOGIN, payload, response)) {
        Serial.println("[API] Login failed - endpoint not reachable or credentials invalid");
        return false;
    }

    // Parse response
    StaticJsonDocument<2048> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
        Serial.println("[API] Failed to parse login response");
        Serial.println("[API] Raw response: " + response);
        return false;
    }

    // Debug: Print entire response
    Serial.println("[API] Login response received:");
    serializeJsonPretty(responseDoc, Serial);
    Serial.println();

    // Check if login was successful
    bool success = responseDoc["success"] | false;
    if (!success) {
        const char* errorMsg = responseDoc["error"];
        const char* message = responseDoc["message"];
        Serial.println("[API] Login failed: " + String(errorMsg ? errorMsg : (message ? message : "Unknown error")));
        return false;
    }

    // Extract token from response (backend uses "deviceToken" field)
    const char* token = responseDoc["deviceToken"];

    // Fallback: try other possible token field names
    if (!token) {
        token = responseDoc["token"];
    }
    if (!token) {
        token = responseDoc["data"]["deviceToken"];
    }
    if (!token) {
        token = responseDoc["data"]["token"];
    }

    if (!token) {
        Serial.println("[API] No deviceToken found in login response");
        Serial.println("[API] Response structure:");
        serializeJson(responseDoc, Serial);
        Serial.println();
        return false;
    }

    // Save token and set registered flag
    saveToken(String(token));
    authenticated = true;
    storageManager.setDeviceRegistered(true);

    // Log token expiration info
    unsigned long expiresIn = responseDoc["expiresIn"] | 0;
    if (expiresIn > 0) {
        // Backend returns expiresIn in seconds, not milliseconds
        unsigned long expiresInDays = expiresIn / (60 * 60 * 24);
        Serial.println("[API] Token expires in " + String(expiresInDays) + " days (" + String(expiresIn) + " seconds)");
    }

    Serial.println("[API] Device logged in successfully");
    return true;
}

bool APIClient::isRegistered() {
    return storageManager.isDeviceRegistered();
}

bool APIClient::refreshToken() {
    Serial.println("[API] Refreshing JWT token...");

    if (!authenticated || deviceToken.length() == 0) {
        Serial.println("[API] No existing token to refresh");
        return false;
    }

    // Send refresh request with current token in Authorization header
    String response;
    if (!httpRequest("POST", API_DEVICE_REFRESH, "", response)) {
        Serial.println("[API] Token refresh failed - endpoint not reachable");
        return false;
    }

    // Parse response
    StaticJsonDocument<2048> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
        Serial.println("[API] Failed to parse refresh response");
        Serial.println("[API] Raw response: " + response);
        return false;
    }

    // Debug: Print entire response
    Serial.println("[API] Refresh response received:");
    serializeJsonPretty(responseDoc, Serial);
    Serial.println();

    // Extract new token (backend uses "deviceToken" field)
    const char* token = responseDoc["deviceToken"];

    // Fallback: try other possible token field names
    if (!token) {
        token = responseDoc["token"];
    }
    if (!token) {
        token = responseDoc["data"]["deviceToken"];
    }

    if (!token) {
        Serial.println("[API] No deviceToken in refresh response");
        return false;
    }

    // Save new token
    saveToken(String(token));
    authenticated = true;

    Serial.println("[API] JWT token refreshed successfully");
    return true;
}

// ============================================================================
// HTTP HELPER
// ============================================================================

bool APIClient::httpRequest(const String& method, const String& endpoint,
                           const String& payload, String& response, int retries) {
    HTTPClient http;
    int httpCode = -1;
    int attempt = 0;

    while (attempt < retries) {
        attempt++;

        String url = String(SERVER_URL) + endpoint;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(HTTP_TIMEOUT);

        // Add auth header if authenticated
        if (authenticated && deviceToken.length() > 0) {
            addAuthHeader(http);
        }

        Serial.println("[API] " + method + " " + url + " (attempt " + String(attempt) + "/" + String(retries) + ")");

        if (method == "GET") {
            httpCode = http.GET();
        } else if (method == "POST") {
            httpCode = http.POST(payload);
        } else if (method == "PUT") {
            httpCode = http.PUT(payload);
        }

        if (httpCode > 0) {
            response = http.getString();
            http.end();

            if (httpCode >= 200 && httpCode < 300) {
                Serial.println("[API] Request successful (HTTP " + String(httpCode) + ")");
                return true;
            } else if (httpCode == 401) {
                // Unauthorized - JWT token expired or invalid
                Serial.println("[API] Unauthorized (401) - JWT token may be expired");
                Serial.println("[API] Response: " + response);
                // Token needs refresh - caller should handle re-authentication
                authenticated = false;
                return false;
            } else {
                Serial.println("[API] Request failed (HTTP " + String(httpCode) + "): " + response);
            }
        } else {
            Serial.println("[API] HTTP error: " + http.errorToString(httpCode));
            http.end();
        }

        if (attempt < retries) {
            int delayMs = API_RETRY_DELAY_MS * attempt; // Exponential backoff
            Serial.println("[API] Retrying in " + String(delayMs) + "ms...");
            delay(delayMs);
        }
    }

    http.end();
    return false;
}

// ============================================================================
// CALLBACK WRAPPERS FOR CONNECTION SYNC MANAGER
// ============================================================================

bool APIClient::fetchConfigCallbackWrapper(void* config) {
    if (instance == nullptr) {
        Serial.println("[API] ERROR: Instance pointer is null");
        return false;
    }
    return instance->fetchAndApplyServerConfig(*((DeviceConfig*)config));
}

bool APIClient::sendConfigPriorityCallbackWrapper(void* config) {
    if (instance == nullptr) {
        Serial.println("[API] ERROR: Instance pointer is null");
        return false;
    }
    return instance->sendConfigWithPriority(*((DeviceConfig*)config));
}

uint64_t APIClient::syncTimeCallbackWrapper() {
    if (instance == nullptr) {
        Serial.println("[API] ERROR: Instance pointer is null");
        return 0;
    }

    // Call internal sync time method and return the timestamp
    if (instance->syncTimeWithServer()) {
        return instance->connSyncManager.getCurrentTimestamp();
    }
    return 0;
}

// ============================================================================
// CONFIGURATION SYNC (Priority-Based Conflict Resolution)
// ============================================================================

bool APIClient::configValuesChanged(const DeviceConfig& a, const DeviceConfig& b) {
    // Delegate to device config manager
    return deviceConfigManager.configValuesChanged(a, b);
}

bool APIClient::fetchAndApplyServerConfig(DeviceConfig& config, bool* changed, bool* deviceWon) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot fetch config");
        return false;
    }

    // Fetch config from server
    DeviceConfig apiConfig;
    bool result = deviceConfigManager.fetchAndApplyServerConfig(apiConfig);

    if (!result) {
        return false;
    }

    // If config was fetched without timestamps, use current server time
    if (isTimeSynced()) {
        uint64_t currentTime = getCurrentTimestamp();
        if (apiConfig.upperThresholdLastModified == 0) apiConfig.upperThresholdLastModified = currentTime;
        if (apiConfig.lowerThresholdLastModified == 0) apiConfig.lowerThresholdLastModified = currentTime;
        if (apiConfig.tankHeightLastModified == 0) apiConfig.tankHeightLastModified = currentTime;
        if (apiConfig.tankWidthLastModified == 0) apiConfig.tankWidthLastModified = currentTime;
        if (apiConfig.tankShapeLastModified == 0) apiConfig.tankShapeLastModified = currentTime;
        if (apiConfig.usedTotalLastModified == 0) apiConfig.usedTotalLastModified = currentTime;
        if (apiConfig.maxInflowLastModified == 0) apiConfig.maxInflowLastModified = currentTime;
        if (apiConfig.forceUpdateLastModified == 0) apiConfig.forceUpdateLastModified = currentTime;
        if (apiConfig.ipAddressLastModified == 0) apiConfig.ipAddressLastModified = currentTime;
        if (apiConfig.autoUpdateLastModified == 0) apiConfig.autoUpdateLastModified = currentTime;
    }

    // Update handler with API values
    configHandler.updateFromAPI(
        apiConfig.upperThreshold, apiConfig.upperThresholdLastModified,
        apiConfig.lowerThreshold, apiConfig.lowerThresholdLastModified,
        apiConfig.tankHeight, apiConfig.tankHeightLastModified,
        apiConfig.tankWidth, apiConfig.tankWidthLastModified,
        apiConfig.tankShape, apiConfig.tankShapeLastModified,
        apiConfig.usedTotal, apiConfig.usedTotalLastModified,
        apiConfig.maxInflow, apiConfig.maxInflowLastModified,
        apiConfig.force_update, apiConfig.forceUpdateLastModified,
        apiConfig.ipAddress, apiConfig.ipAddressLastModified,
        apiConfig.auto_update, apiConfig.autoUpdateLastModified
    );

    // Perform 3-way merge
    bool valuesChanged = configHandler.merge();

    // Check if merged values differ from API (device values won the merge)
    bool deviceValuesWon = configHandler.valuesDifferFromAPI();

    // Set output parameters if provided
    if (changed != nullptr) {
        *changed = valuesChanged;
    }
    if (deviceWon != nullptr) {
        *deviceWon = deviceValuesWon;
    }

    // Return merged values
    config.upperThreshold = configHandler.getUpperThreshold();
    config.upperThresholdLastModified = configHandler.getUpperThresholdTimestamp();
    config.lowerThreshold = configHandler.getLowerThreshold();
    config.lowerThresholdLastModified = configHandler.getLowerThresholdTimestamp();
    config.tankHeight = configHandler.getTankHeight();
    config.tankHeightLastModified = configHandler.getTankHeightTimestamp();
    config.tankWidth = configHandler.getTankWidth();
    config.tankWidthLastModified = configHandler.getTankWidthTimestamp();
    config.tankShape = configHandler.getTankShape();
    config.tankShapeLastModified = configHandler.getTankShapeTimestamp();
    config.usedTotal = configHandler.getUsedTotal();
    config.usedTotalLastModified = configHandler.getUsedTotalTimestamp();
    config.maxInflow = configHandler.getMaxInflow();
    config.maxInflowLastModified = configHandler.getMaxInflowTimestamp();
    config.force_update = configHandler.getForceUpdate();
    config.forceUpdateLastModified = configHandler.getForceUpdateTimestamp();
    config.ipAddress = configHandler.getIpAddress();
    config.ipAddressLastModified = configHandler.getIpAddressTimestamp();
    config.auto_update = configHandler.getAutoUpdate();
    config.autoUpdateLastModified = configHandler.getAutoUpdateTimestamp();

    if (valuesChanged) {
        Serial.println("[API] Config values changed after 3-way merge");
    }
    if (deviceValuesWon) {
        Serial.println("[API] Device values won merge - will need to sync to server");
    }

    return true;
}

bool APIClient::sendConfigWithPriority(DeviceConfig& config) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot send config");
        return false;
    }

    // Delegate to device config manager
    bool success = deviceConfigManager.sendConfigWithPriority(config);

    if (success) {
        // Mark as synced in ConnectionSyncManager
        connSyncManager.resetConfigSync();
    }

    return success;
}

void APIClient::markConfigModified() {
    Serial.println("[API] Config marked as locally modified");
    connSyncManager.markConfigModified();
}

// ============================================================================
// CONTROL & TELEMETRY
// ============================================================================

bool APIClient::fetchControl(ControlData& control) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot fetch control");
        return false;
    }

    // Fetch control data from server
    ControlData apiControl;
    bool result = controlDataManager.fetchControl(apiControl);

    if (!result) {
        return false;
    }

    // If fetched data has no timestamps, use current server time
    if (isTimeSynced()) {
        uint64_t currentTime = getCurrentTimestamp();
        if (apiControl.pumpSwitchLastModified == 0) {
            apiControl.pumpSwitchLastModified = currentTime;
        }
        if (apiControl.configUpdateLastModified == 0) {
            apiControl.configUpdateLastModified = currentTime;
        }
    }

    // Update handler with API values
    controlHandler.updateFromAPI(
        apiControl.pumpSwitch, apiControl.pumpSwitchLastModified,
        apiControl.config_update, apiControl.configUpdateLastModified
    );

    // Perform 3-way merge
    bool changed = controlHandler.merge();

    // Return merged values
    control.pumpSwitch = controlHandler.getPumpSwitch();
    control.pumpSwitchLastModified = controlHandler.getPumpSwitchTimestamp();
    control.config_update = controlHandler.getConfigUpdate();
    control.configUpdateLastModified = controlHandler.getConfigUpdateTimestamp();

    if (changed) {
        Serial.println("[API] Control values changed after 3-way merge");
    }

    return true;
}

bool APIClient::uploadControl(const ControlData& control) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot upload control");
        return false;
    }

    // Delegate to control data manager
    return controlDataManager.uploadControl(control);
}

bool APIClient::uploadControlWithPayload(const String& payload) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot upload control");
        return false;
    }

    // Delegate to control data manager with pre-built payload
    return controlDataManager.uploadControlWithPayload(payload);
}

String APIClient::buildControlPayload(const ControlData& control) {
    // Delegate to control data manager to build JSON payload
    return controlDataManager.buildControlPayload(control);
}

bool APIClient::uploadTelemetry(float waterLevel, float currInflow, int pumpStatus) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot upload telemetry");
        return false;
    }

    // Delegate to telemetry manager
    return telemetryManager.uploadTelemetry(waterLevel, currInflow, pumpStatus);
}

// ============================================================================
// TIME SYNCHRONIZATION
// ============================================================================

bool APIClient::syncTimeWithServer() {
    Serial.println("[API] Syncing time via NTP...");

    // Configure NTP - Use multiple reliable servers
    // Google NTP is usually more reliable and less likely to be blocked
    const char* ntpServer1 = "time.google.com";
    const char* ntpServer2 = "pool.ntp.org";
    const char* ntpServer3 = "time.nist.gov";
    const long gmtOffset_sec = 0;       // UTC timezone
    const int daylightOffset_sec = 0;   // No daylight saving

    // Configure time sync with NTP servers
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);

    // Wait for time to sync (max 10 seconds)
    Serial.println("[API] Waiting for NTP time sync...");
    struct tm timeinfo;
    int retries = 0;
    const int maxRetries = 20;  // 20 x 500ms = 10 seconds max wait

    while (!getLocalTime(&timeinfo) && retries < maxRetries) {
        Serial.print(".");
        delay(500);
        retries++;
    }
    Serial.println();

    if (retries >= maxRetries) {
        Serial.println("[API] Failed to sync time via NTP - timeout");
        return false;
    }

    // Get current time in Unix timestamp (seconds since epoch)
    time_t now;
    time(&now);

    // Convert to milliseconds (backend expects milliseconds)
    uint64_t ntpTimestamp = (uint64_t)now * 1000ULL;

    // Save sync information via ConnectionSyncManager
    connSyncManager.setTimestamp(ntpTimestamp);

    Serial.println("[API] Time synced successfully via NTP");
    Serial.printf("  NTP Timestamp: %llu ms\n", ntpTimestamp);
    Serial.printf("  Date/Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    return true;
}

uint64_t APIClient::getCurrentTimestamp() {
    return connSyncManager.getCurrentTimestamp();
}

void APIClient::setTimestamp(uint64_t timestamp) {
    Serial.printf("[API] Manually setting timestamp: %llu\n", timestamp);
    connSyncManager.setTimestamp(timestamp);
}

bool APIClient::isTimeSynced() {
    return connSyncManager.isTimeSynced();
}

// ============================================================================
// SYNC STATUS MANAGEMENT
// ============================================================================

void APIClient::onDeviceOnline(DeviceConfig& config) {
    Serial.println("[API] Device came online - delegating to ConnectionSyncManager...");
    connSyncManager.onDeviceOnline(&config);
}

void APIClient::onDeviceOffline() {
    Serial.println("[API] Device went offline");
    connSyncManager.onDeviceOffline();
}

SyncStatus APIClient::getSyncStatus() {
    return connSyncManager.getSyncStatus();
}

bool APIClient::isServerOnline() {
    return connSyncManager.isServerOnline();
}

bool APIClient::hasPendingConfigSync() {
    return connSyncManager.needsConfigUpload();
}

void APIClient::saveSyncStatus() {
    connSyncManager.saveSyncStatus();
}

void APIClient::loadSyncStatus() {
    connSyncManager.loadSyncStatus();
}

// ============================================================================
// MANAGER TOKEN UPDATE
// ============================================================================

void APIClient::updateManagerTokens() {
    // Update all managers with current token and hardware ID
    deviceConfigManager.setToken(deviceToken);
    deviceConfigManager.setHardwareId(hardwareId);

    telemetryManager.setToken(deviceToken);
    telemetryManager.setHardwareId(hardwareId);

    controlDataManager.setToken(deviceToken);
    controlDataManager.setHardwareId(hardwareId);

    DEBUG_PRINTLN("[API] Updated tokens for all specialized managers");
}
