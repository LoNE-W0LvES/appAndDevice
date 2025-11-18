#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================================
// DEVICE CONFIGURATION STRUCTURE
// ============================================================================

// Device configuration structure with single timestamp for sync
struct DeviceConfig {
    // Configuration values
    float upperThreshold;
    float lowerThreshold;
    float tankHeight;
    float tankWidth;
    String tankShape;
    float usedTotal;
    float maxInflow;
    bool force_update;      // Matches server's "force_update" key
    bool sensorFilter;      // Enable/disable sensor filtering/smoothing
    String ipAddress;

    // Single timestamp for entire config (Unix epoch milliseconds)
    // Use uint64_t because timestamps can exceed 32-bit unsigned long limit
    uint64_t lastModified;

    // Constructor to initialize all fields to default values
    DeviceConfig()
        : upperThreshold(0.0f),
          lowerThreshold(0.0f),
          tankHeight(0.0f),
          tankWidth(0.0f),
          tankShape(""),
          usedTotal(0.0f),
          maxInflow(0.0f),
          force_update(false),
          sensorFilter(DEFAULT_SENSOR_FILTER),
          ipAddress(""),
          lastModified(0) {}

    // Check if config values have changed (excluding timestamp)
    // Returns true if any value is different
    bool valuesChanged(const DeviceConfig& other) const {
        if (upperThreshold != other.upperThreshold) return true;
        if (lowerThreshold != other.lowerThreshold) return true;
        if (tankHeight != other.tankHeight) return true;
        if (tankWidth != other.tankWidth) return true;
        if (tankShape != other.tankShape) return true;
        if (usedTotal != other.usedTotal) return true;
        if (maxInflow != other.maxInflow) return true;
        if (force_update != other.force_update) return true;
        if (sensorFilter != other.sensorFilter) return true;
        if (ipAddress != other.ipAddress) return true;
        return false;  // All values identical
    }
};

// ============================================================================
// DEVICE CONFIG MANAGER CLASS
// ============================================================================

class DeviceConfigManager {
public:
    DeviceConfigManager();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    // Set authentication token for API calls
    void setToken(const String& token);

    // Set hardware ID
    void setHardwareId(const String& id);

    // ========================================================================
    // CONFIGURATION FETCH/SEND
    // ========================================================================

    // Fetch config FROM server and apply if values changed
    // Used when device_config_sync_status = true (no local changes)
    bool fetchAndApplyServerConfig(DeviceConfig& config);

    // Send config TO server with priority (lastModified = 0)
    // Used when device_config_sync_status = false (local changes pending)
    bool sendConfigWithPriority(DeviceConfig& config);

    // ========================================================================
    // CONFIG UTILITIES
    // ========================================================================

    // Check if config values differ (ignores timestamps)
    bool configValuesChanged(const DeviceConfig& a, const DeviceConfig& b);

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

    // Parse server JSON response to DeviceConfig
    bool parseConfig(const String& json, DeviceConfig& config);

    // Build config JSON payload for server
    String buildConfigPayload(const DeviceConfig& config, bool priority = false);
};

#endif // DEVICE_CONFIG_H
