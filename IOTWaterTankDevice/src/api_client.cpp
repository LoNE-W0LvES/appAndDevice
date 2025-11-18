#include "api_client.h"
#include "storage_manager.h"

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

bool APIClient::registerDevice() {
    Serial.println("[API] Registering device...");

    // Build registration payload
    StaticJsonDocument<1024> doc;
    doc["deviceId"] = DEVICE_ID;        // Device identifier from config
    doc["hardwareId"] = hardwareId;      // MAC address
    doc["deviceName"] = DEVICE_NAME;
    doc["projectId"] = PROJECT_ID;

    JsonObject metadata = doc.createNestedObject("metadata");
    metadata["chipModel"] = "ESP32-S3";
    metadata["firmwareVersion"] = FIRMWARE_VERSION;

    String payload;
    serializeJson(doc, payload);

    // Send registration request
    String response;
    if (!httpRequest("POST", API_REGISTER, payload, response)) {
        Serial.println("[API] Registration failed");
        return false;
    }

    // Parse response
    StaticJsonDocument<2048> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
        Serial.println("[API] Failed to parse registration response");
        Serial.println("[API] Raw response: " + response);
        return false;
    }

    // Debug: Print entire response
    Serial.println("[API] Registration response received:");
    serializeJsonPretty(responseDoc, Serial);
    Serial.println();

    // Check if registration was successful
    bool success = responseDoc["success"] | false;

    if (!success) {
        const char* errorMsg = responseDoc["error"];
        Serial.println("[API] Registration failed: " + String(errorMsg ? errorMsg : "Unknown error"));
        return false;
    }

    // Registration successful - device is now created but unclaimed
    // No token is returned at this stage - must login to get token
    const char* message = responseDoc["message"];
    bool requiresLogin = responseDoc["requiresLogin"] | false;

    Serial.println("[API] Device registered successfully");
    if (message) {
        Serial.println("[API] " + String(message));
    }
    if (requiresLogin) {
        Serial.println("[API] Device requires login to get authentication token");
    }

    // Mark as registered (device exists on backend, but not yet authenticated)
    storageManager.setDeviceRegistered(true);

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

bool APIClient::fetchAndApplyServerConfig(DeviceConfig& config) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot fetch config");
        return false;
    }

    // Delegate to device config manager
    bool result = deviceConfigManager.fetchAndApplyServerConfig(config);

    // If config was fetched from direct format endpoint (no per-field timestamps),
    // use current server time for all fields (only if time is synced)
    // Note: This happens regardless of whether values changed (result true/false)
    if (isTimeSynced()) {
        uint64_t currentTime = getCurrentTimestamp();
        bool anyZero = false;

        if (config.upperThresholdLastModified == 0) { config.upperThresholdLastModified = currentTime; anyZero = true; }
        if (config.lowerThresholdLastModified == 0) { config.lowerThresholdLastModified = currentTime; anyZero = true; }
        if (config.tankHeightLastModified == 0) { config.tankHeightLastModified = currentTime; anyZero = true; }
        if (config.tankWidthLastModified == 0) { config.tankWidthLastModified = currentTime; anyZero = true; }
        if (config.tankShapeLastModified == 0) { config.tankShapeLastModified = currentTime; anyZero = true; }
        if (config.usedTotalLastModified == 0) { config.usedTotalLastModified = currentTime; anyZero = true; }
        if (config.maxInflowLastModified == 0) { config.maxInflowLastModified = currentTime; anyZero = true; }
        if (config.forceUpdateLastModified == 0) { config.forceUpdateLastModified = currentTime; anyZero = true; }
        if (config.sensorFilterLastModified == 0) { config.sensorFilterLastModified = currentTime; anyZero = true; }
        if (config.ipAddressLastModified == 0) { config.ipAddressLastModified = currentTime; anyZero = true; }

        if (anyZero) {
            DEBUG_PRINTF("[API] Set missing field timestamps to current server time: %llu\n", currentTime);
        }
    }

    return result;
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

    // Delegate to control data manager
    bool result = controlDataManager.fetchControl(control);

    // If control was fetched from direct format endpoint (no timestamps),
    // use current server time as lastModified (only if time is synced)
    if (result && isTimeSynced()) {
        uint64_t currentTime = getCurrentTimestamp();
        if (control.pumpSwitchLastModified == 0) {
            control.pumpSwitchLastModified = currentTime;
        }
        if (control.configUpdateLastModified == 0) {
            control.configUpdateLastModified = currentTime;
        }
    }

    return result;
}

bool APIClient::uploadControl(const ControlData& control) {
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot upload control");
        return false;
    }

    // Delegate to control data manager
    return controlDataManager.uploadControl(control);
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
    if (!authenticated) {
        Serial.println("[API] Not authenticated, cannot sync time");
        return false;
    }

    Serial.println("[API] Syncing time with server...");

    String url = String(API_TIME_SYNC) + "?deviceId=" + String(DEVICE_ID);
    String response;

    if (!httpRequest("GET", url, "", response)) {
        Serial.println("[API] Failed to sync time");
        return false;
    }

    // Debug: Print raw response (only if DEBUG_RESPONSE_API enabled)
    DEBUG_RESPONSE_API_PRINTLN("[API] Time sync response:");
    DEBUG_RESPONSE_API_PRINTLN(response);

    // Parse server time from JSON response
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.println("[API] Failed to parse time sync response");
        DEBUG_PRINTF("[API] Parse error: %s\n", error.c_str());
        return false;
    }

    // Debug: Print parsed JSON (only if DEBUG_RESPONSE_API enabled)
    DEBUG_RESPONSE_API_PRINTLN("[API] Parsed time sync JSON:");
    #ifdef DEBUG_RESPONSE_API
    serializeJsonPretty(doc, Serial);
    Serial.println();
    #endif

    // Extract server time (Unix timestamp in milliseconds)
    // Backend returns numeric timestamp, not ISO 8601 string
    // Use uint64_t because timestamp can exceed 32-bit unsigned long limit
    uint64_t serverUnixTime = doc["serverTime"] | (uint64_t)0;

    if (serverUnixTime == 0) {
        Serial.println("[API] No serverTime in response or invalid value");
        DEBUG_PRINTLN("[API] Available fields:");
        #ifdef DEBUG_ENABLED
        serializeJson(doc, Serial);
        Serial.println();
        #endif
        return false;
    }

    // Save sync information via ConnectionSyncManager
    connSyncManager.setTimestamp(serverUnixTime);

    Serial.println("[API] Time synced successfully");
    Serial.printf("  Server Timestamp: %llu ms\n", serverUnixTime);

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
