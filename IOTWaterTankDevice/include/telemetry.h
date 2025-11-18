#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================================
// TELEMETRY MANAGER CLASS
// ============================================================================

class TelemetryManager {
public:
    TelemetryManager();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    // Set authentication token for API calls
    void setToken(const String& token);

    // Set hardware ID
    void setHardwareId(const String& id);

    // ========================================================================
    // TELEMETRY OPERATIONS
    // ========================================================================

    // Upload telemetry data to server
    // Automatically includes Status field (always 1 for online tracking)
    // Server marks device offline if no telemetry for >60 seconds
    bool uploadTelemetry(float waterLevel, float currInflow, int pumpStatus);

private:
    String deviceToken;
    String hardwareId;

    // ========================================================================
    // HELPER METHODS
    // ========================================================================

    // HTTP helper with retry logic
    bool httpRequest(const String& method, const String& endpoint,
                    const String& payload, String& response, int retries = API_RETRY_COUNT);

    // Add authorization header to HTTP client
    void addAuthHeader(HTTPClient& http);

    // Build telemetry JSON payload
    String buildTelemetryPayload(float waterLevel, float currInflow, int pumpStatus);
};

#endif // TELEMETRY_H
