#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

enum PumpMode {
    MODE_AUTO,      // Automatic based on thresholds
    MODE_MANUAL,    // Manual control via buttons or cloud
    MODE_OVERRIDE   // Hardware override switch (BTN6)
};

class RelayController {
public:
    RelayController();

    // Initialize relay pin
    void begin();

    // Update pump state based on mode and conditions
    void update(float waterLevel, float upperThreshold, float lowerThreshold);

    // Manual pump control
    void turnOn();
    void turnOff();

    // Set pump mode
    void setMode(PumpMode mode);
    PumpMode getMode();

    // Get current pump state
    bool isPumpOn();

    // Get pump status as number (0=OFF, 1=ON) for telemetry
    int getPumpStatus();

    // Set pump from cloud command
    void setCloudCommand(bool state);

    // Hardware override (BTN6)
    void setHardwareOverride(bool state);
    bool isHardwareOverride();

    // Toggle between auto and manual modes
    void toggleMode();

    // Get mode as string for display
    String getModeString();

private:
    bool pumpState;           // Current relay state (true=ON, false=OFF)
    PumpMode currentMode;     // Current operating mode
    bool cloudCommand;        // Last cloud command state
    bool hardwareOverride;    // Hardware override switch state
    bool autoModeEnabled;     // Auto mode hysteresis flag

    Preferences preferences;

    // Apply pump state to relay pin
    void applyPumpState(bool state);

    // Auto mode logic with hysteresis
    void handleAutoMode(float waterLevel, float upperThreshold, float lowerThreshold);

    // Load saved mode from NVS
    void loadMode();

    // Save current mode to NVS
    void saveMode();
};

#endif // RELAY_CONTROLLER_H
