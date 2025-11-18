#include "handle_config_data.h"
#include "sync_merge.h"
#include "config.h"

void ConfigDataHandler::begin() {
    // Initialize with default values
    upperThreshold.value = DEFAULT_UPPER_THRESHOLD;
    lowerThreshold.value = DEFAULT_LOWER_THRESHOLD;
    tankHeight.value = DEFAULT_TANK_HEIGHT;
    tankWidth.value = DEFAULT_TANK_WIDTH;
    tankShape.value = "Cylindrical";
    usedTotal.value = 0.0f;
    maxInflow.value = 0.0f;
    forceUpdate.value = false;
    ipAddress.value = "";

    DEBUG_PRINTLN("[ConfigHandler] Initialized with defaults");
}

void ConfigDataHandler::updateFromAPI(float api_upperThreshold, uint64_t api_upperThreshold_ts,
                                       float api_lowerThreshold, uint64_t api_lowerThreshold_ts,
                                       float api_tankHeight, uint64_t api_tankHeight_ts,
                                       float api_tankWidth, uint64_t api_tankWidth_ts,
                                       const String& api_tankShape, uint64_t api_tankShape_ts,
                                       float api_usedTotal, uint64_t api_usedTotal_ts,
                                       float api_maxInflow, uint64_t api_maxInflow_ts,
                                       bool api_forceUpdate, uint64_t api_forceUpdate_ts,
                                       const String& api_ipAddress, uint64_t api_ipAddress_ts) {
    upperThreshold.api_value = api_upperThreshold;
    upperThreshold.api_lastModified = api_upperThreshold_ts;

    lowerThreshold.api_value = api_lowerThreshold;
    lowerThreshold.api_lastModified = api_lowerThreshold_ts;

    tankHeight.api_value = api_tankHeight;
    tankHeight.api_lastModified = api_tankHeight_ts;

    tankWidth.api_value = api_tankWidth;
    tankWidth.api_lastModified = api_tankWidth_ts;

    tankShape.api_value = api_tankShape;
    tankShape.api_lastModified = api_tankShape_ts;

    usedTotal.api_value = api_usedTotal;
    usedTotal.api_lastModified = api_usedTotal_ts;

    maxInflow.api_value = api_maxInflow;
    maxInflow.api_lastModified = api_maxInflow_ts;

    forceUpdate.api_value = api_forceUpdate;
    forceUpdate.api_lastModified = api_forceUpdate_ts;


    ipAddress.api_value = api_ipAddress;
    ipAddress.api_lastModified = api_ipAddress_ts;

    DEBUG_PRINTLN("[ConfigHandler] Updated from API");
}

void ConfigDataHandler::updateFromLocal(float local_upperThreshold, uint64_t local_upperThreshold_ts,
                                         float local_lowerThreshold, uint64_t local_lowerThreshold_ts,
                                         float local_tankHeight, uint64_t local_tankHeight_ts,
                                         float local_tankWidth, uint64_t local_tankWidth_ts,
                                         const String& local_tankShape, uint64_t local_tankShape_ts,
                                         float local_usedTotal, uint64_t local_usedTotal_ts,
                                         float local_maxInflow, uint64_t local_maxInflow_ts,
                                         bool local_forceUpdate, uint64_t local_forceUpdate_ts,
                                         const String& local_ipAddress, uint64_t local_ipAddress_ts) {
    upperThreshold.local_value = local_upperThreshold;
    upperThreshold.local_lastModified = local_upperThreshold_ts;

    lowerThreshold.local_value = local_lowerThreshold;
    lowerThreshold.local_lastModified = local_lowerThreshold_ts;

    tankHeight.local_value = local_tankHeight;
    tankHeight.local_lastModified = local_tankHeight_ts;

    tankWidth.local_value = local_tankWidth;
    tankWidth.local_lastModified = local_tankWidth_ts;

    tankShape.local_value = local_tankShape;
    tankShape.local_lastModified = local_tankShape_ts;

    usedTotal.local_value = local_usedTotal;
    usedTotal.local_lastModified = local_usedTotal_ts;

    maxInflow.local_value = local_maxInflow;
    maxInflow.local_lastModified = local_maxInflow_ts;

    forceUpdate.local_value = local_forceUpdate;
    forceUpdate.local_lastModified = local_forceUpdate_ts;


    ipAddress.local_value = local_ipAddress;
    ipAddress.local_lastModified = local_ipAddress_ts;

    DEBUG_PRINTLN("[ConfigHandler] Updated from Local");
}

void ConfigDataHandler::updateSelf(float self_upperThreshold, float self_lowerThreshold,
                                    float self_tankHeight, float self_tankWidth,
                                    const String& self_tankShape, float self_usedTotal,
                                    float self_maxInflow, bool self_forceUpdate,
    uint64_t now = millis();

    upperThreshold.value = self_upperThreshold;
    upperThreshold.lastModified = now;

    lowerThreshold.value = self_lowerThreshold;
    lowerThreshold.lastModified = now;

    tankHeight.value = self_tankHeight;
    tankHeight.lastModified = now;

    tankWidth.value = self_tankWidth;
    tankWidth.lastModified = now;

    tankShape.value = self_tankShape;
    tankShape.lastModified = now;

    usedTotal.value = self_usedTotal;
    usedTotal.lastModified = now;

    maxInflow.value = self_maxInflow;
    maxInflow.lastModified = now;

    forceUpdate.value = self_forceUpdate;
    forceUpdate.lastModified = now;


    ipAddress.value = self_ipAddress;
    ipAddress.lastModified = now;

    DEBUG_PRINTLN("[ConfigHandler] Updated self");
}

bool ConfigDataHandler::merge() {
    DEBUG_PRINTLN("[ConfigHandler] Starting 3-way merge...");

    bool changed = false;
    changed |= SyncMerge::mergeFloat(upperThreshold);
    changed |= SyncMerge::mergeFloat(lowerThreshold);
    changed |= SyncMerge::mergeFloat(tankHeight);
    changed |= SyncMerge::mergeFloat(tankWidth);
    changed |= SyncMerge::mergeString(tankShape);
    changed |= SyncMerge::mergeFloat(usedTotal);
    changed |= SyncMerge::mergeFloat(maxInflow);
    changed |= SyncMerge::mergeBool(forceUpdate);
    changed |= SyncMerge::mergeString(ipAddress);

    if (changed) {
        DEBUG_PRINTLN("[ConfigHandler] Config values changed after merge");
    }

    return changed;
}

void ConfigDataHandler::setAllPriority() {
    // Set priority flag (timestamp=0) for all fields
    upperThreshold.lastModified = 0;
    lowerThreshold.lastModified = 0;
    tankHeight.lastModified = 0;
    tankWidth.lastModified = 0;
    tankShape.lastModified = 0;
    usedTotal.lastModified = 0;
    maxInflow.lastModified = 0;
    forceUpdate.lastModified = 0;
    ipAddress.lastModified = 0;

    DEBUG_PRINTLN("[ConfigHandler] Set all config fields with priority flag");
}

void ConfigDataHandler::printState() {
    Serial.println("[ConfigHandler] Current State:");
    Serial.printf("  upperThreshold: %.2f (API: %.2f, Local: %.2f)\n",
                  upperThreshold.value, upperThreshold.api_value, upperThreshold.local_value);
    Serial.printf("  lowerThreshold: %.2f (API: %.2f, Local: %.2f)\n",
                  lowerThreshold.value, lowerThreshold.api_value, lowerThreshold.local_value);
    Serial.printf("  tankHeight: %.2f (API: %.2f, Local: %.2f)\n",
                  tankHeight.value, tankHeight.api_value, tankHeight.local_value);
    Serial.printf("  tankWidth: %.2f (API: %.2f, Local: %.2f)\n",
                  tankWidth.value, tankWidth.api_value, tankWidth.local_value);
}
