#include "telemetry.h"
#include "endpoints.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

TelemetryManager::TelemetryManager() {
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void TelemetryManager::setToken(const String& token) {
    deviceToken = token;
}

void TelemetryManager::setHardwareId(const String& id) {
    hardwareId = id;
}

// ============================================================================
// TELEMETRY OPERATIONS
// ============================================================================

bool TelemetryManager::uploadTelemetry(float waterLevel, float currInflow, int pumpStatus) {
    String payload = buildTelemetryPayload(waterLevel, currInflow, pumpStatus);
    String response;

    // Single attempt for telemetry to avoid blocking
    return httpRequest("POST", API_DEVICE_TELEMETRY, payload, response, 1);
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool TelemetryManager::httpRequest(const String& method, const String& endpoint,
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

        DEBUG_PRINTF("[Telemetry] %s %s (attempt %d/%d)\n",
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
                DEBUG_PRINTF("[Telemetry] Upload successful (HTTP %d)\n", httpCode);
                return true;
            } else {
                DEBUG_PRINTF("[Telemetry] Upload failed (HTTP %d): %s\n",
                           httpCode, response.c_str());
            }
        } else {
            DEBUG_PRINTF("[Telemetry] HTTP error: %s\n",
                        http.errorToString(httpCode).c_str());
            http.end();
        }

        if (attempt < retries) {
            int delayMs = API_RETRY_DELAY_MS * attempt;
            DEBUG_PRINTF("[Telemetry] Retrying in %dms...\n", delayMs);
            delay(delayMs);
        }
    }

    http.end();
    return false;
}

void TelemetryManager::addAuthHeader(HTTPClient& http) {
    String authHeader = "Bearer " + deviceToken;
    http.addHeader("Authorization", authHeader);
    DEBUG_PRINTLN("[Telemetry] Added JWT Authorization header");
}

String TelemetryManager::buildTelemetryPayload(float waterLevel, float currInflow, int pumpStatus) {
    StaticJsonDocument<2048> doc;

    doc["deviceId"] = DEVICE_ID;

    // Backend expects telemetry data with named keys (not array)
    JsonObject telemetryData = doc.createNestedObject("sensorData");

    // Water level
    JsonObject level = telemetryData.createNestedObject("waterLevel");
    level["key"] = "waterLevel";
    level["label"] = "Water Level";
    level["type"] = "number";
    level["value"] = waterLevel;

    // Current inflow
    JsonObject inflow = telemetryData.createNestedObject("currInflow");
    inflow["key"] = "currInflow";
    inflow["label"] = "Current Inflow";
    inflow["type"] = "number";
    inflow["value"] = currInflow;

    // Pump status
    JsonObject pump = telemetryData.createNestedObject("pumpStatus");
    pump["key"] = "pumpStatus";
    pump["label"] = "Pump Status";
    pump["type"] = "number";
    pump["value"] = pumpStatus;

    // Status (always 1 for online tracking)
    // Server marks device offline if no telemetry for >60 seconds
    JsonObject status = telemetryData.createNestedObject("Status");
    status["key"] = "Status";
    status["label"] = "Device Status";
    status["type"] = "number";
    status["value"] = 1;

    String payload;
    serializeJson(doc, payload);
    return payload;
}
