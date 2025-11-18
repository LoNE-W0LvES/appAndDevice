#include "control_data.h"
#include "endpoints.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ControlDataManager::ControlDataManager() {
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ControlDataManager::setToken(const String& token) {
    deviceToken = token;
}

void ControlDataManager::setHardwareId(const String& id) {
    hardwareId = id;
}

// ============================================================================
// CONTROL OPERATIONS
// ============================================================================

bool ControlDataManager::fetchControl(ControlData& control) {
    String url = String(API_DEVICE_CONTROL) + "?deviceId=" + String(DEVICE_ID);
    String response;

    if (!httpRequest("GET", url, "", response)) {
        return false;
    }

    return parseControl(response, control);
}

String ControlDataManager::buildControlPayload(const ControlData& control) {
    // Build payload with priority flag (lastModified=0) for all fields
    // This ensures device changes always override server values
    // Server expects full structure with key, label, type, value, lastModified
    StaticJsonDocument<1024> doc;

    JsonObject pumpSwitch = doc.createNestedObject("pumpSwitch");
    pumpSwitch["key"] = "pumpSwitch";
    pumpSwitch["label"] = "Pump Switch";
    pumpSwitch["type"] = "boolean";
    pumpSwitch["value"] = control.pumpSwitch;
    pumpSwitch["lastModified"] = 0;  // Priority flag

    JsonObject configUpdate = doc.createNestedObject("config_update");
    configUpdate["key"] = "config_update";
    configUpdate["label"] = "Configuration Update";
    configUpdate["type"] = "boolean";
    configUpdate["value"] = control.config_update;
    configUpdate["lastModified"] = 0;  // Priority flag
    configUpdate["description"] = "When enabled, device will update its configuration from server";
    configUpdate["system"] = true;

    String payload;
    serializeJson(doc, payload);

    return payload;
}

bool ControlDataManager::uploadControlWithPayload(const String& payload) {
    DEBUG_PRINTLN("[ControlData] Uploading control data:");
    DEBUG_PRINTLN(payload);

    String url = String(API_DEVICE_CONTROL) + "?deviceId=" + String(DEVICE_ID);
    String response;

    return httpRequest("POST", url, payload, response);
}

bool ControlDataManager::uploadControl(const ControlData& control) {
    // Build JSON payload and upload
    String payload = buildControlPayload(control);
    return uploadControlWithPayload(payload);
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool ControlDataManager::httpRequest(const String& method, const String& endpoint,
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

        DEBUG_PRINTF("[ControlData] %s %s (attempt %d/%d)\n",
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
                DEBUG_PRINTF("[ControlData] Request successful (HTTP %d)\n", httpCode);
                return true;
            } else {
                Serial.println("[ControlData] Request failed (HTTP " + String(httpCode) + "): " + response);
            }
        } else {
            Serial.println("[ControlData] HTTP error: " + http.errorToString(httpCode));
            http.end();
        }

        if (attempt < retries) {
            int delayMs = API_RETRY_DELAY_MS * attempt;
            DEBUG_PRINTF("[ControlData] Retrying in %dms...\n", delayMs);
            delay(delayMs);
        }
    }

    http.end();
    return false;
}

void ControlDataManager::addAuthHeader(HTTPClient& http) {
    String authHeader = "Bearer " + deviceToken;
    http.addHeader("Authorization", authHeader);
    DEBUG_PRINTLN("[ControlData] Added JWT Authorization header");
}

bool ControlDataManager::parseControl(const String& json, ControlData& control) {
    DEBUG_RESPONSE_API_PRINTLN("[ControlData] Control response (raw):");
    DEBUG_RESPONSE_API_PRINTLN(json);

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.println("[ControlData] JSON parse error: " + String(error.c_str()));
        return false;
    }

    // Debug: Print parsed JSON structure (only if DEBUG_RESPONSE_API enabled)
    DEBUG_RESPONSE_API_PRINTLN("[ControlData] Control response (parsed):");
    #ifdef DEBUG_RESPONSE_API
    serializeJsonPretty(doc, Serial);
    Serial.println();
    #endif

    // Parse nested JSON structure: controlData.{key}.value
    JsonObject controlData = doc["controlData"];

    // Try: data.controlData (alternative path)
    if (!controlData) {
        controlData = doc["data"]["controlData"];
    }

    // Try: device.controlData (used by /api/timeSync)
    if (!controlData) {
        controlData = doc["device"]["controlData"];
    }

    if (!controlData) {
        Serial.println("[ControlData] No controlData in response");
        return false;
    }

    // Check if control values are nested (with "value" key)
    bool isNested = controlData["pumpSwitch"].containsKey("value");

    if (isNested) {
        control.pumpSwitch = controlData["pumpSwitch"]["value"] | false;
        control.config_update = controlData["config_update"]["value"] | false;

        // Extract timestamps if available
        control.pumpSwitchLastModified = controlData["pumpSwitch"]["lastModified"] | (uint64_t)0;
        control.configUpdateLastModified = controlData["config_update"]["lastModified"] | (uint64_t)0;
    } else {
        control.pumpSwitch = controlData["pumpSwitch"] | false;
        control.config_update = controlData["config_update"] | false;

        // No timestamps in direct format
        control.pumpSwitchLastModified = 0;
        control.configUpdateLastModified = 0;
    }

    return true;
}
