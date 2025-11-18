#ifndef SYNC_TYPES_H
#define SYNC_TYPES_H

#include <Arduino.h>

// ============================================================================
// SYNC DATA TYPES
// ============================================================================
// Each synced value has 3 versions: API, Local, and Self
// Each version tracks its own lastModified timestamp
// Merge logic uses Last-Write-Wins (LWW) with priority flag support

// Boolean sync value with 3-way tracking
struct SyncBool {
    // API source (from cloud server)
    bool api_value;
    uint64_t api_lastModified;

    // Local source (from app via webserver)
    bool local_value;
    uint64_t local_lastModified;

    // Self (device's current/stored value)
    bool value;
    uint64_t lastModified;

    // Constructor
    SyncBool() : api_value(false), api_lastModified(0),
                 local_value(false), local_lastModified(0),
                 value(false), lastModified(0) {}
};

// Float sync value with 3-way tracking
struct SyncFloat {
    // API source (from cloud server)
    float api_value;
    uint64_t api_lastModified;

    // Local source (from app via webserver)
    float local_value;
    uint64_t local_lastModified;

    // Self (device's current/stored value)
    float value;
    uint64_t lastModified;

    // Constructor
    SyncFloat() : api_value(0.0f), api_lastModified(0),
                  local_value(0.0f), local_lastModified(0),
                  value(0.0f), lastModified(0) {}
};

// String sync value with 3-way tracking
struct SyncString {
    // API source (from cloud server)
    String api_value;
    uint64_t api_lastModified;

    // Local source (from app via webserver)
    String local_value;
    uint64_t local_lastModified;

    // Self (device's current/stored value)
    String value;
    uint64_t lastModified;

    // Constructor
    SyncString() : api_value(""), api_lastModified(0),
                   local_value(""), local_lastModified(0),
                   value(""), lastModified(0) {}
};

#endif // SYNC_TYPES_H
