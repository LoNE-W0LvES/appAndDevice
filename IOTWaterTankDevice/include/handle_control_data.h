#ifndef HANDLE_CONTROL_DATA_H
#define HANDLE_CONTROL_DATA_H

#include <Arduino.h>
#include "sync_types.h"

// ============================================================================
// CONTROL DATA HANDLER
// ============================================================================
// Manages pump switch and config update flags with 3-way sync
// Each field has: API source, Local source, and Self (current value)

class ControlDataHandler {
public:
    // Control data fields with 3-way sync
    SyncBool pumpSwitch;
    SyncBool configUpdate;

    // Initialize with default values
    void begin();

    // Update from API source (from cloud server)
    void updateFromAPI(bool api_pumpSwitch, uint64_t api_pumpSwitch_ts,
                       bool api_configUpdate, uint64_t api_configUpdate_ts);

    // Update from Local source (from app via webserver)
    void updateFromLocal(bool local_pumpSwitch, uint64_t local_pumpSwitch_ts,
                         bool local_configUpdate, uint64_t local_configUpdate_ts);

    // Update self (device changes its own value)
    void updateSelf(bool self_pumpSwitch, bool self_configUpdate);

    // Perform 3-way merge - returns true if any value changed
    bool merge();

    // Get current values (after merge)
    bool getPumpSwitch() const { return pumpSwitch.value; }
    bool getConfigUpdate() const { return configUpdate.value; }

    // Get timestamps
    uint64_t getPumpSwitchTimestamp() const { return pumpSwitch.lastModified; }
    uint64_t getConfigUpdateTimestamp() const { return configUpdate.lastModified; }

    // Set priority flag (timestamp=0) for uploading to server
    void setPumpSwitchPriority(bool value);
    void setConfigUpdatePriority(bool value);

    // Debug: Print current state
    void printState();
};

#endif // HANDLE_CONTROL_DATA_H
