#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================================
// DEVICE CONFIGURATION STRUCTURE
// ============================================================================

// Device configuration structure with per-field timestamps
// Each field has its own lastModified timestamp for fine-grained sync control
struct DeviceConfig {
    // Configuration values with individual timestamps
    float upperThreshold;
    uint64_t upperThresholdLastModified;

    float lowerThreshold;
    uint64_t lowerThresholdLastModified;

    float tankHeight;
    uint64_t tankHeightLastModified;

    float tankWidth;
    uint64_t tankWidthLastModified;

    String tankShape;
    uint64_t tankShapeLastModified;

    float usedTotal;
    uint64_t usedTotalLastModified;

    float maxInflow;
    uint64_t maxInflowLastModified;

    bool force_update;
    uint64_t forceUpdateLastModified;

    bool sensorFilter;
    uint64_t sensorFilterLastModified;

    String ipAddress;
    uint64_t ipAddressLastModified;

    bool auto_update;
    uint64_t autoUpdateLastModified;

    // Constructor to initialize all fields to default values
    DeviceConfig()
        : upperThreshold(0.0f),
          upperThresholdLastModified(0),
          lowerThreshold(0.0f),
          lowerThresholdLastModified(0),
          tankHeight(0.0f),
          tankHeightLastModified(0),
          tankWidth(0.0f),
          tankWidthLastModified(0),
          tankShape(""),
          tankShapeLastModified(0),
          usedTotal(0.0f),
          usedTotalLastModified(0),
          maxInflow(0.0f),
          maxInflowLastModified(0),
          force_update(false),
          forceUpdateLastModified(0),
          sensorFilter(DEFAULT_SENSOR_FILTER),
          sensorFilterLastModified(0),
          ipAddress(""),
          ipAddressLastModified(0),
          auto_update(true),
          autoUpdateLastModified(0) {}

    // Check if config values have changed (excluding timestamps)
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
        if (auto_update != other.auto_update) return true;
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
