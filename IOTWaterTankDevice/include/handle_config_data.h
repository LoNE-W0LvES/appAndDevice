#ifndef HANDLE_CONFIG_DATA_H
#define HANDLE_CONFIG_DATA_H

#include <Arduino.h>
#include "sync_types.h"

// ============================================================================
// CONFIG DATA HANDLER
// ============================================================================
// Manages device configuration with 3-way sync
// Each field has: API source, Local source, and Self (current value)

class ConfigDataHandler {
public:
    // Config data fields with 3-way sync
    SyncFloat upperThreshold;
    SyncFloat lowerThreshold;
    SyncFloat tankHeight;
    SyncFloat tankWidth;
    SyncString tankShape;
    SyncFloat usedTotal;
    SyncFloat maxInflow;
    SyncBool forceUpdate;
    SyncString ipAddress;

    // Initialize with default values
    void begin();

    // Update from API source (from cloud server)
    void updateFromAPI(float api_upperThreshold, uint64_t api_upperThreshold_ts,
                       float api_lowerThreshold, uint64_t api_lowerThreshold_ts,
                       float api_tankHeight, uint64_t api_tankHeight_ts,
                       float api_tankWidth, uint64_t api_tankWidth_ts,
                       const String& api_tankShape, uint64_t api_tankShape_ts,
                       float api_usedTotal, uint64_t api_usedTotal_ts,
                       float api_maxInflow, uint64_t api_maxInflow_ts,
                       bool api_forceUpdate, uint64_t api_forceUpdate_ts,
                       const String& api_ipAddress, uint64_t api_ipAddress_ts);

    // Update from Local source (from app via webserver)
    void updateFromLocal(float local_upperThreshold, uint64_t local_upperThreshold_ts,
                         float local_lowerThreshold, uint64_t local_lowerThreshold_ts,
                         float local_tankHeight, uint64_t local_tankHeight_ts,
                         float local_tankWidth, uint64_t local_tankWidth_ts,
                         const String& local_tankShape, uint64_t local_tankShape_ts,
                         float local_usedTotal, uint64_t local_usedTotal_ts,
                         float local_maxInflow, uint64_t local_maxInflow_ts,
                         bool local_forceUpdate, uint64_t local_forceUpdate_ts,
                         const String& local_ipAddress, uint64_t local_ipAddress_ts);

    // Update self (device's own stored values)
    void updateSelf(float self_upperThreshold, float self_lowerThreshold,
                    float self_tankHeight, float self_tankWidth,
                    const String& self_tankShape, float self_usedTotal,
                    float self_maxInflow, bool self_forceUpdate,
                    const String& self_ipAddress);

    // Perform 3-way merge - returns true if any value changed
    bool merge();

    // Get current values (after merge)
    float getUpperThreshold() const { return upperThreshold.value; }
    float getLowerThreshold() const { return lowerThreshold.value; }
    float getTankHeight() const { return tankHeight.value; }
    float getTankWidth() const { return tankWidth.value; }
    String getTankShape() const { return tankShape.value; }
    float getUsedTotal() const { return usedTotal.value; }
    float getMaxInflow() const { return maxInflow.value; }
    bool getForceUpdate() const { return forceUpdate.value; }
    String getIpAddress() const { return ipAddress.value; }

    // Get timestamps (after merge)
    uint64_t getUpperThresholdTimestamp() const { return upperThreshold.lastModified; }
    uint64_t getLowerThresholdTimestamp() const { return lowerThreshold.lastModified; }
    uint64_t getTankHeightTimestamp() const { return tankHeight.lastModified; }
    uint64_t getTankWidthTimestamp() const { return tankWidth.lastModified; }
    uint64_t getTankShapeTimestamp() const { return tankShape.lastModified; }
    uint64_t getUsedTotalTimestamp() const { return usedTotal.lastModified; }
    uint64_t getMaxInflowTimestamp() const { return maxInflow.lastModified; }
    uint64_t getForceUpdateTimestamp() const { return forceUpdate.lastModified; }
    uint64_t getIpAddressTimestamp() const { return ipAddress.lastModified; }

    // Set all values with priority flag for uploading to server
    void setAllPriority();

    // Debug: Print current state
    void printState();
};

#endif // HANDLE_CONFIG_DATA_H
