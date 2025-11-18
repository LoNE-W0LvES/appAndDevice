#ifndef HANDLE_TELEMETRY_DATA_H
#define HANDLE_TELEMETRY_DATA_H

#include <Arduino.h>

// ============================================================================
// TELEMETRY DATA HANDLER
// ============================================================================
// Manages sensor telemetry data (read-only, device produces, app/server consume)
// No 3-way sync needed - device is the authoritative source

class TelemetryDataHandler {
public:
    // Telemetry data fields (device produces these)
    float waterLevel;          // Current water level percentage (0-100)
    float distance;            // Distance from sensor to water surface (cm)
    float currInflow;          // Current inflow rate (L/min)
    float pumpStatus;          // Pump status (0=off, 1=on)
    float isOnline;            // Online status (0=offline, 1=online)
    uint64_t timestamp;        // Last update timestamp

    // Initialize with default values
    void begin();

    // Update telemetry values (called by sensor manager)
    void update(float waterLevel, float distance, float currInflow,
                float pumpStatus, float isOnline);

    // Get values
    float getWaterLevel() const { return waterLevel; }
    float getDistance() const { return distance; }
    float getCurrInflow() const { return currInflow; }
    float getPumpStatus() const { return pumpStatus; }
    float getIsOnline() const { return isOnline; }
    uint64_t getTimestamp() const { return timestamp; }

    // Debug: Print current state
    void printState();
};

#endif // HANDLE_TELEMETRY_DATA_H
