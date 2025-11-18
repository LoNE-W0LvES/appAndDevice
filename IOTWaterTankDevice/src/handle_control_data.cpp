#include "handle_control_data.h"
#include "sync_merge.h"
#include "config.h"

void ControlDataHandler::begin() {
    // Initialize with default values
    pumpSwitch.value = false;
    pumpSwitch.lastModified = 0;
    pumpSwitch.api_value = false;
    pumpSwitch.api_lastModified = 0;
    pumpSwitch.local_value = false;
    pumpSwitch.local_lastModified = 0;

    configUpdate.value = true;  // Default: update from server
    configUpdate.lastModified = 0;
    configUpdate.api_value = true;
    configUpdate.api_lastModified = 0;
    configUpdate.local_value = true;
    configUpdate.local_lastModified = 0;

    DEBUG_PRINTLN("[ControlHandler] Initialized");
}

void ControlDataHandler::updateFromAPI(bool api_pumpSwitch, uint64_t api_pumpSwitch_ts,
                                        bool api_configUpdate, uint64_t api_configUpdate_ts) {
    pumpSwitch.api_value = api_pumpSwitch;
    pumpSwitch.api_lastModified = api_pumpSwitch_ts;

    configUpdate.api_value = api_configUpdate;
    configUpdate.api_lastModified = api_configUpdate_ts;

    DEBUG_PRINTLN("[ControlHandler] Updated from API");
    DEBUG_PRINTF("  pumpSwitch: %s (ts: %llu)\n", api_pumpSwitch ? "true" : "false", api_pumpSwitch_ts);
    DEBUG_PRINTF("  configUpdate: %s (ts: %llu)\n", api_configUpdate ? "true" : "false", api_configUpdate_ts);
}

void ControlDataHandler::updateFromLocal(bool local_pumpSwitch, uint64_t local_pumpSwitch_ts,
                                          bool local_configUpdate, uint64_t local_configUpdate_ts) {
    pumpSwitch.local_value = local_pumpSwitch;
    pumpSwitch.local_lastModified = local_pumpSwitch_ts;

    configUpdate.local_value = local_configUpdate;
    configUpdate.local_lastModified = local_configUpdate_ts;

    DEBUG_PRINTLN("[ControlHandler] Updated from Local");
    DEBUG_PRINTF("  pumpSwitch: %s (ts: %llu)\n", local_pumpSwitch ? "true" : "false", local_pumpSwitch_ts);
    DEBUG_PRINTF("  configUpdate: %s (ts: %llu)\n", local_configUpdate ? "true" : "false", local_configUpdate_ts);
}

void ControlDataHandler::updateSelf(bool self_pumpSwitch, bool self_configUpdate) {
    // When device updates its own values, use current millis as timestamp
    uint64_t now = millis();

    pumpSwitch.value = self_pumpSwitch;
    pumpSwitch.lastModified = now;

    configUpdate.value = self_configUpdate;
    configUpdate.lastModified = now;

    DEBUG_PRINTLN("[ControlHandler] Updated self");
    DEBUG_PRINTF("  pumpSwitch: %s (ts: %llu)\n", self_pumpSwitch ? "true" : "false", now);
    DEBUG_PRINTF("  configUpdate: %s (ts: %llu)\n", self_configUpdate ? "true" : "false", now);
}

bool ControlDataHandler::merge() {
    DEBUG_PRINTLN("[ControlHandler] Starting 3-way merge...");

    bool pumpChanged = SyncMerge::mergeBool(pumpSwitch);
    bool configChanged = SyncMerge::mergeBool(configUpdate);

    if (pumpChanged) {
        DEBUG_PRINTF("[ControlHandler] pumpSwitch changed to: %s\n", pumpSwitch.value ? "true" : "false");
    }

    if (configChanged) {
        DEBUG_PRINTF("[ControlHandler] configUpdate changed to: %s\n", configUpdate.value ? "true" : "false");
    }

    return pumpChanged || configChanged;
}

void ControlDataHandler::setPumpSwitchPriority(bool value) {
    pumpSwitch.value = value;
    pumpSwitch.lastModified = 0;  // Priority flag
    pumpSwitch.api_lastModified = 0;
    pumpSwitch.local_lastModified = 0;
    DEBUG_PRINTF("[ControlHandler] Set pumpSwitch with priority: %s\n", value ? "true" : "false");
}

void ControlDataHandler::setConfigUpdatePriority(bool value) {
    configUpdate.value = value;
    configUpdate.lastModified = 0;  // Priority flag
    configUpdate.api_lastModified = 0;
    configUpdate.local_lastModified = 0;
    DEBUG_PRINTF("[ControlHandler] Set configUpdate with priority: %s\n", value ? "true" : "false");
}

void ControlDataHandler::printState() {
    Serial.println("[ControlHandler] Current State:");
    Serial.println("  pumpSwitch:");
    Serial.printf("    Self:  %s (ts: %llu)\n", pumpSwitch.value ? "true" : "false", pumpSwitch.lastModified);
    Serial.printf("    API:   %s (ts: %llu)\n", pumpSwitch.api_value ? "true" : "false", pumpSwitch.api_lastModified);
    Serial.printf("    Local: %s (ts: %llu)\n", pumpSwitch.local_value ? "true" : "false", pumpSwitch.local_lastModified);

    Serial.println("  configUpdate:");
    Serial.printf("    Self:  %s (ts: %llu)\n", configUpdate.value ? "true" : "false", configUpdate.lastModified);
    Serial.printf("    API:   %s (ts: %llu)\n", configUpdate.api_value ? "true" : "false", configUpdate.api_lastModified);
    Serial.printf("    Local: %s (ts: %llu)\n", configUpdate.local_value ? "true" : "false", configUpdate.local_lastModified);
}
