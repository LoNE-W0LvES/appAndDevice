#include "device_config.h"
#include "endpoints.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

DeviceConfigManager::DeviceConfigManager() {
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void DeviceConfigManager::setToken(const String& token) {
    deviceToken = token;
}

void DeviceConfigManager::setHardwareId(const String& id) {
    hardwareId = id;
}

// ============================================================================
// CONFIGURATION FETCH/SEND
// ============================================================================

bool DeviceConfigManager::fetchAndApplyServerConfig(DeviceConfig& config) {
    Serial.println("[DeviceConfig] Fetching config FROM server...");

    String url = String(API_DEVICE_CONFIG) + "?deviceId=" + String(DEVICE_ID);
    String response;

    if (!httpRequest("GET", url, "", response)) {
        Serial.println("[DeviceConfig] Failed to fetch config from server");
        return false;
    }

    // Parse server config
    DeviceConfig serverConfig;
    if (!parseConfig(response, serverConfig)) {
        Serial.println("[DeviceConfig] Failed to parse server config");
        return false;
    }

    // Compare only VALUES, ignore timestamps
    if (configValuesChanged(config, serverConfig)) {
        // Values are different - apply server config
        config = serverConfig;
        Serial.println("[DeviceConfig] Config updated FROM server (values changed)");
        Serial.println("  Upper Threshold: " + String(config.upperThreshold));
        Serial.println("  Lower Threshold: " + String(config.lowerThreshold));
        Serial.println("  Last Modified: " + String((unsigned long)config.lastModified));
        return true;
    } else {
        // Values identical - just update timestamp for reference
        Serial.println("[DeviceConfig] Config values identical to server - no update needed");
        config.lastModified = serverConfig.lastModified;
        return false;  // No actual change
    }
}

bool DeviceConfigManager::sendConfigWithPriority(DeviceConfig& config) {
    Serial.println("[DeviceConfig] Sending config TO server with priority...");

    // Build payload with priority flag (timestamp = 0)
    String payload = buildConfigPayload(config, true);
    String response;

    if (!httpRequest("POST", API_DEVICE_CONFIG, payload, response)) {
        Serial.println("[DeviceConfig] Failed to send config to server");
        return false;
    }

    // Parse response
    StaticJsonDocument<2048> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
        Serial.println("[DeviceConfig] Failed to parse config response");
        return false;
    }

    if (responseDoc["success"] == true) {
        // Server accepted and assigned timestamp
        config.lastModified = responseDoc["timestamp"];
        Serial.println("[DeviceConfig] Config synced TO server with priority");
        return true;
    }

    return false;
}

// ============================================================================
// CONFIG UTILITIES
// ============================================================================

bool DeviceConfigManager::configValuesChanged(const DeviceConfig& a, const DeviceConfig& b) {
    // Compare only actual configuration values, NOT timestamps
    if (a.upperThreshold != b.upperThreshold) return true;
    if (a.lowerThreshold != b.lowerThreshold) return true;
    if (a.tankHeight != b.tankHeight) return true;
    if (a.tankWidth != b.tankWidth) return true;
    if (a.tankShape != b.tankShape) return true;
    if (a.usedTotal != b.usedTotal) return true;
    if (a.maxInflow != b.maxInflow) return true;
    if (a.force_update != b.force_update) return true;
    if (a.ipAddress != b.ipAddress) return true;

    // All values identical
    return false;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool DeviceConfigManager::httpRequest(const String& method, const String& endpoint,
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
        if (deviceToken.length() > 0) {
            addAuthHeader(http);
        }

        DEBUG_PRINTF("[DeviceConfig] %s %s (attempt %d/%d)\n",
                    method.c_str(), url.c_str(), attempt, retries);

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
                DEBUG_PRINTF("[DeviceConfig] Request successful (HTTP %d)\n", httpCode);
                return true;
            } else {
                Serial.println("[DeviceConfig] Request failed (HTTP " + String(httpCode) + "): " + response);
            }
        } else {
            Serial.println("[DeviceConfig] HTTP error: " + http.errorToString(httpCode));
            http.end();
        }

        if (attempt < retries) {
            int delayMs = API_RETRY_DELAY_MS * attempt;
            DEBUG_PRINTF("[DeviceConfig] Retrying in %dms...\n", delayMs);
            delay(delayMs);
        }
    }

    http.end();
    return false;
}

void DeviceConfigManager::addAuthHeader(HTTPClient& http) {
    String authHeader = "Bearer " + deviceToken;
    http.addHeader("Authorization", authHeader);
    DEBUG_PRINTLN("[DeviceConfig] Added JWT Authorization header");
}

bool DeviceConfigManager::parseConfig(const String& json, DeviceConfig& config) {
    // Debug: Print raw response
    DEBUG_PRINTLN("[DeviceConfig] Config response (raw):");
    DEBUG_PRINTLN(json);

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.println("[DeviceConfig] JSON parse error: " + String(error.c_str()));
        return false;
    }

    // Debug: Print parsed JSON structure
    DEBUG_PRINTLN("[DeviceConfig] Config response (parsed):");
    #ifdef DEBUG_ENABLED
    serializeJsonPretty(doc, Serial);
    Serial.println();
    #endif

    // Parse JSON structure - try multiple possible paths
    JsonObject deviceConfig = doc["deviceConfig"];

    // Try: data.deviceConfig (used by /api/device/config)
    if (!deviceConfig) {
        deviceConfig = doc["data"]["deviceConfig"];
    }

    // Try: device.deviceConfig (used by /api/timeSync)
    if (!deviceConfig) {
        deviceConfig = doc["device"]["deviceConfig"];
    }

    if (!deviceConfig) {
        Serial.println("[DeviceConfig] No deviceConfig in response");
        return false;
    }

    // Check if config values are nested (with "value" and "lastModified" keys)
    // or direct values
    bool isNested = deviceConfig["upperThreshold"].containsKey("value");

    if (isNested) {
        // Nested format: {upperThreshold: {value: 95, lastModified: 123}}
        config.upperThreshold = deviceConfig["upperThreshold"]["value"] | DEFAULT_UPPER_THRESHOLD;
        config.lowerThreshold = deviceConfig["lowerThreshold"]["value"] | DEFAULT_LOWER_THRESHOLD;
        config.tankHeight = deviceConfig["tankHeight"]["value"] | DEFAULT_TANK_HEIGHT;
        config.tankWidth = deviceConfig["tankWidth"]["value"] | DEFAULT_TANK_WIDTH;
        config.tankShape = deviceConfig["tankShape"]["value"] | "CYLINDRICAL";
        config.usedTotal = deviceConfig["UsedTotal"]["value"] | 0.0f;
        config.maxInflow = deviceConfig["maxInflow"]["value"] | 0.0f;
        config.force_update = deviceConfig["force_update"]["value"] | false;
        config.ipAddress = deviceConfig["ip_address"]["value"] | "";

        // Use uint64_t for timestamp to avoid 32-bit overflow
        config.lastModified = deviceConfig["upperThreshold"]["lastModified"] | (uint64_t)0;
    } else {
        // Direct format: {upperThreshold: 95, lowerThreshold: 20, ...}
        config.upperThreshold = deviceConfig["upperThreshold"] | DEFAULT_UPPER_THRESHOLD;
        config.lowerThreshold = deviceConfig["lowerThreshold"] | DEFAULT_LOWER_THRESHOLD;
        config.tankHeight = deviceConfig["tankHeight"] | DEFAULT_TANK_HEIGHT;
        config.tankWidth = deviceConfig["tankWidth"] | DEFAULT_TANK_WIDTH;
        config.tankShape = deviceConfig["tankShape"] | "CYLINDRICAL";
        config.usedTotal = deviceConfig["UsedTotal"] | 0.0f;
        config.maxInflow = deviceConfig["maxInflow"] | 0.0f;
        config.force_update = deviceConfig["force_update"] | false;
        config.ipAddress = deviceConfig["ip_address"] | "";
        config.lastModified = 0;  // Not available in direct format
    }

    return true;
}

String DeviceConfigManager::buildConfigPayload(const DeviceConfig& config, bool priority) {
    StaticJsonDocument<2048> doc;

    doc["deviceId"] = DEVICE_ID;

    // Create nested objects for each field
    JsonObject upperThreshold = doc.createNestedObject("upperThreshold");
    upperThreshold["key"] = "upperThreshold";
    upperThreshold["value"] = config.upperThreshold;
    upperThreshold["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject lowerThreshold = doc.createNestedObject("lowerThreshold");
    lowerThreshold["key"] = "lowerThreshold";
    lowerThreshold["value"] = config.lowerThreshold;
    lowerThreshold["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject tankHeight = doc.createNestedObject("tankHeight");
    tankHeight["key"] = "tankHeight";
    tankHeight["value"] = config.tankHeight;
    tankHeight["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject tankWidth = doc.createNestedObject("tankWidth");
    tankWidth["key"] = "tankWidth";
    tankWidth["value"] = config.tankWidth;
    tankWidth["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject tankShape = doc.createNestedObject("tankShape");
    tankShape["key"] = "tankShape";
    tankShape["value"] = config.tankShape;
    tankShape["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject usedTotal = doc.createNestedObject("UsedTotal");
    usedTotal["key"] = "UsedTotal";
    usedTotal["value"] = config.usedTotal;
    usedTotal["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject maxInflow = doc.createNestedObject("maxInflow");
    maxInflow["key"] = "maxInflow";
    maxInflow["value"] = config.maxInflow;
    maxInflow["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject forceUpdate = doc.createNestedObject("force_update");
    forceUpdate["key"] = "force_update";
    forceUpdate["value"] = config.force_update;
    forceUpdate["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    JsonObject ipAddress = doc.createNestedObject("ip_address");
    ipAddress["key"] = "ip_address";
    ipAddress["value"] = config.ipAddress;
    ipAddress["timestamp"] = priority ? 0 : (unsigned long)config.lastModified;

    String payload;
    serializeJson(doc, payload);
    return payload;
}
