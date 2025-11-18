#ifndef CONTROL_DATA_H
#define CONTROL_DATA_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================================
// CONTROL DATA STRUCTURE
// ============================================================================

// Control data structure
struct ControlData {
    bool pumpSwitch;
    bool config_update;     // Matches server's "config_update" key

    // Timestamps for each field (Unix epoch milliseconds)
    // Use uint64_t because timestamps can exceed 32-bit unsigned long limit
    uint64_t pumpSwitchLastModified;
    uint64_t configUpdateLastModified;

    // Constructor to initialize all fields to default values
    ControlData()
        : pumpSwitch(false),
          config_update(false),
          pumpSwitchLastModified(0),
          configUpdateLastModified(0) {}
};

// ============================================================================
// CONTROL DATA MANAGER CLASS
// ============================================================================

class ControlDataManager {
public:
    ControlDataManager();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    // Set authentication token for API calls
    void setToken(const String& token);

    // Set hardware ID
    void setHardwareId(const String& id);

    // ========================================================================
    // CONTROL OPERATIONS
    // ========================================================================

    // Fetch control data from server
    bool fetchControl(ControlData& control);

    // Upload control data to server (with priority flag)
    bool uploadControl(const ControlData& control);

    // Build JSON payload for control upload (used for async operations)
    String buildControlPayload(const ControlData& control);

    // Upload control data using pre-built JSON payload
    bool uploadControlWithPayload(const String& payload);

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

    // Parse server JSON response to ControlData
    bool parseControl(const String& json, ControlData& control);
};

#endif // CONTROL_DATA_H
