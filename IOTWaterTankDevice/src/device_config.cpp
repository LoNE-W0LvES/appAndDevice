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
        // Values are different - apply server config (including per-field timestamps)
        config = serverConfig;
        Serial.println("[DeviceConfig] Config updated FROM server (values changed)");
        Serial.println("  Upper Threshold: " + String(config.upperThreshold));
        Serial.println("  Lower Threshold: " + String(config.lowerThreshold));
        return true;
    } else {
        // Values identical - update timestamps from server for reference
        Serial.println("[DeviceConfig] Config values identical to server - updating timestamps only");
        config.upperThresholdLastModified = serverConfig.upperThresholdLastModified;
        config.lowerThresholdLastModified = serverConfig.lowerThresholdLastModified;
        config.tankHeightLastModified = serverConfig.tankHeightLastModified;
        config.tankWidthLastModified = serverConfig.tankWidthLastModified;
        config.tankShapeLastModified = serverConfig.tankShapeLastModified;
        config.usedTotalLastModified = serverConfig.usedTotalLastModified;
        config.maxInflowLastModified = serverConfig.maxInflowLastModified;
        config.forceUpdateLastModified = serverConfig.forceUpdateLastModified;
        config.sensorFilterLastModified = serverConfig.sensorFilterLastModified;
        config.ipAddressLastModified = serverConfig.ipAddressLastModified;
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
        // Server accepted and assigned timestamp - update all field timestamps
        uint64_t serverTimestamp = responseDoc["timestamp"];
        config.upperThresholdLastModified = serverTimestamp;
        config.lowerThresholdLastModified = serverTimestamp;
        config.tankHeightLastModified = serverTimestamp;
        config.tankWidthLastModified = serverTimestamp;
        config.tankShapeLastModified = serverTimestamp;
        config.usedTotalLastModified = serverTimestamp;
        config.maxInflowLastModified = serverTimestamp;
        config.forceUpdateLastModified = serverTimestamp;
        config.sensorFilterLastModified = serverTimestamp;
        config.ipAddressLastModified = serverTimestamp;
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
    if (a.sensorFilter != b.sensorFilter) return true;
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
    // Debug: Print raw response (only if DEBUG_RESPONSE_API enabled)
    DEBUG_RESPONSE_API_PRINTLN("[DeviceConfig] Config response (raw):");
    DEBUG_RESPONSE_API_PRINTLN(json);

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.println("[DeviceConfig] JSON parse error: " + String(error.c_str()));
        return false;
    }

    // Debug: Print parsed JSON structure (only if DEBUG_RESPONSE_API enabled)
    DEBUG_RESPONSE_API_PRINTLN("[DeviceConfig] Config response (parsed):");
    #ifdef DEBUG_RESPONSE_API
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
        config.upperThresholdLastModified = deviceConfig["upperThreshold"]["lastModified"] | (uint64_t)0;

        config.lowerThreshold = deviceConfig["lowerThreshold"]["value"] | DEFAULT_LOWER_THRESHOLD;
        config.lowerThresholdLastModified = deviceConfig["lowerThreshold"]["lastModified"] | (uint64_t)0;

        config.tankHeight = deviceConfig["tankHeight"]["value"] | DEFAULT_TANK_HEIGHT;
        config.tankHeightLastModified = deviceConfig["tankHeight"]["lastModified"] | (uint64_t)0;

        config.tankWidth = deviceConfig["tankWidth"]["value"] | DEFAULT_TANK_WIDTH;
        config.tankWidthLastModified = deviceConfig["tankWidth"]["lastModified"] | (uint64_t)0;

        config.tankShape = deviceConfig["tankShape"]["value"] | "CYLINDRICAL";
        config.tankShapeLastModified = deviceConfig["tankShape"]["lastModified"] | (uint64_t)0;

        config.usedTotal = deviceConfig["UsedTotal"]["value"] | 0.0f;
        config.usedTotalLastModified = deviceConfig["UsedTotal"]["lastModified"] | (uint64_t)0;

        config.maxInflow = deviceConfig["maxInflow"]["value"] | 0.0f;
        config.maxInflowLastModified = deviceConfig["maxInflow"]["lastModified"] | (uint64_t)0;

        config.force_update = deviceConfig["force_update"]["value"] | false;
        config.forceUpdateLastModified = deviceConfig["force_update"]["lastModified"] | (uint64_t)0;

        config.sensorFilter = deviceConfig["sensorFilter"]["value"] | DEFAULT_SENSOR_FILTER;
        config.sensorFilterLastModified = deviceConfig["sensorFilter"]["lastModified"] | (uint64_t)0;

        config.ipAddress = deviceConfig["ip_address"]["value"] | "";
        config.ipAddressLastModified = deviceConfig["ip_address"]["lastModified"] | (uint64_t)0;
    } else {
        // Direct format: {upperThreshold: 95, lowerThreshold: 20, ...}
        config.upperThreshold = deviceConfig["upperThreshold"] | DEFAULT_UPPER_THRESHOLD;
        config.upperThresholdLastModified = 0;

        config.lowerThreshold = deviceConfig["lowerThreshold"] | DEFAULT_LOWER_THRESHOLD;
        config.lowerThresholdLastModified = 0;

        config.tankHeight = deviceConfig["tankHeight"] | DEFAULT_TANK_HEIGHT;
        config.tankHeightLastModified = 0;

        config.tankWidth = deviceConfig["tankWidth"] | DEFAULT_TANK_WIDTH;
        config.tankWidthLastModified = 0;

        config.tankShape = deviceConfig["tankShape"] | "CYLINDRICAL";
        config.tankShapeLastModified = 0;

        config.usedTotal = deviceConfig["UsedTotal"] | 0.0f;
        config.usedTotalLastModified = 0;

        config.maxInflow = deviceConfig["maxInflow"] | 0.0f;
        config.maxInflowLastModified = 0;

        config.force_update = deviceConfig["force_update"] | false;
        config.forceUpdateLastModified = 0;

        config.sensorFilter = deviceConfig["sensorFilter"] | DEFAULT_SENSOR_FILTER;
        config.sensorFilterLastModified = 0;

        config.ipAddress = deviceConfig["ip_address"] | "";
        config.ipAddressLastModified = 0;
    }

    return true;
}

String DeviceConfigManager::buildConfigPayload(const DeviceConfig& config, bool priority) {
    StaticJsonDocument<2048> doc;

    // Server expects all config fields wrapped in "configUpdates" object
    JsonObject configUpdates = doc.createNestedObject("configUpdates");

    // Create nested objects for each field with per-field timestamps
    // Server expects: key, label, type, value, lastModified
    JsonObject upperThreshold = configUpdates.createNestedObject("upperThreshold");
    upperThreshold["key"] = "upperThreshold";
    upperThreshold["label"] = "Upper Threshold";
    upperThreshold["type"] = "number";
    upperThreshold["value"] = config.upperThreshold;
    upperThreshold["lastModified"] = priority ? 0 : (unsigned long)config.upperThresholdLastModified;

    JsonObject lowerThreshold = configUpdates.createNestedObject("lowerThreshold");
    lowerThreshold["key"] = "lowerThreshold";
    lowerThreshold["label"] = "Lower Threshold";
    lowerThreshold["type"] = "number";
    lowerThreshold["value"] = config.lowerThreshold;
    lowerThreshold["lastModified"] = priority ? 0 : (unsigned long)config.lowerThresholdLastModified;

    JsonObject tankHeight = configUpdates.createNestedObject("tankHeight");
    tankHeight["key"] = "tankHeight";
    tankHeight["label"] = "Tank Height";
    tankHeight["type"] = "number";
    tankHeight["value"] = config.tankHeight;
    tankHeight["lastModified"] = priority ? 0 : (unsigned long)config.tankHeightLastModified;

    JsonObject tankWidth = configUpdates.createNestedObject("tankWidth");
    tankWidth["key"] = "tankWidth";
    tankWidth["label"] = "Tank Width";
    tankWidth["type"] = "number";
    tankWidth["value"] = config.tankWidth;
    tankWidth["lastModified"] = priority ? 0 : (unsigned long)config.tankWidthLastModified;

    JsonObject tankShape = configUpdates.createNestedObject("tankShape");
    tankShape["key"] = "tankShape";
    tankShape["label"] = "Tank Shape";
    tankShape["type"] = "string";
    tankShape["value"] = config.tankShape;
    tankShape["lastModified"] = priority ? 0 : (unsigned long)config.tankShapeLastModified;

    JsonObject usedTotal = configUpdates.createNestedObject("UsedTotal");
    usedTotal["key"] = "UsedTotal";
    usedTotal["label"] = "Used Total";
    usedTotal["type"] = "number";
    usedTotal["value"] = config.usedTotal;
    usedTotal["lastModified"] = priority ? 0 : (unsigned long)config.usedTotalLastModified;

    JsonObject maxInflow = configUpdates.createNestedObject("maxInflow");
    maxInflow["key"] = "maxInflow";
    maxInflow["label"] = "Max Inflow";
    maxInflow["type"] = "number";
    maxInflow["value"] = config.maxInflow;
    maxInflow["lastModified"] = priority ? 0 : (unsigned long)config.maxInflowLastModified;

    JsonObject forceUpdate = configUpdates.createNestedObject("force_update");
    forceUpdate["key"] = "force_update";
    forceUpdate["label"] = "Force Update";
    forceUpdate["type"] = "boolean";
    forceUpdate["value"] = config.force_update;
    forceUpdate["lastModified"] = priority ? 0 : (unsigned long)config.forceUpdateLastModified;

    JsonObject sensorFilter = configUpdates.createNestedObject("sensorFilter");
    sensorFilter["key"] = "sensorFilter";
    sensorFilter["label"] = "Sensor Filter";
    sensorFilter["type"] = "boolean";
    sensorFilter["value"] = config.sensorFilter;
    sensorFilter["lastModified"] = priority ? 0 : (unsigned long)config.sensorFilterLastModified;

    JsonObject ipAddress = configUpdates.createNestedObject("ip_address");
    ipAddress["key"] = "ip_address";
    ipAddress["label"] = "IP Address";
    ipAddress["type"] = "string";
    ipAddress["value"] = config.ipAddress;
    ipAddress["lastModified"] = priority ? 0 : (unsigned long)config.ipAddressLastModified;

    String payload;
    serializeJson(doc, payload);
    return payload;
}
