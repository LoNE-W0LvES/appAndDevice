#include "relay_controller.h"

RelayController::RelayController()
    : pumpState(false),
      currentMode(MODE_AUTO),
      cloudCommand(false),
      hardwareOverride(false),
      autoModeEnabled(false) {
}

void RelayController::begin() {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Ensure pump is OFF at startup

    // Load mode from NVS (opens/closes preferences internally)
    loadMode();

    Serial.println("[Relay] Relay controller initialized");
    Serial.println("[Relay] Mode: " + getModeString());
    Serial.println("[Relay] Pump: OFF");
}

void RelayController::loadMode() {
    // Open preferences namespace for reading
    if (!preferences.begin(PREF_NAMESPACE, true)) {
        Serial.println("[Relay] Failed to open preferences for reading");
        autoModeEnabled = true;  // Default to auto mode
        currentMode = MODE_AUTO;
        return;
    }

    // Load saved auto mode preference
    autoModeEnabled = preferences.getBool(PREF_AUTO_MODE, true);
    currentMode = autoModeEnabled ? MODE_AUTO : MODE_MANUAL;

    // Close preferences namespace
    preferences.end();
}

void RelayController::saveMode() {
    // Open preferences namespace for writing
    if (!preferences.begin(PREF_NAMESPACE, false)) {
        Serial.println("[Relay] Failed to open preferences for writing");
        return;
    }

    // Save auto mode preference
    preferences.putBool(PREF_AUTO_MODE, currentMode == MODE_AUTO);

    // Close preferences namespace
    preferences.end();
}

void RelayController::applyPumpState(bool state) {
    if (pumpState != state) {
        pumpState = state;
        digitalWrite(RELAY_PIN, state ? HIGH : LOW);

        Serial.println("[Relay] Pump " + String(state ? "ON" : "OFF") +
                      " (" + getModeString() + ")");
    }
}

void RelayController::handleAutoMode(float waterLevel, float upperThreshold, float lowerThreshold) {
    // Auto mode with hysteresis to prevent rapid switching

    if (waterLevel < lowerThreshold) {
        // Water level below lower threshold - turn pump ON
        applyPumpState(true);
    } else if (waterLevel > upperThreshold) {
        // Water level above upper threshold - turn pump OFF
        applyPumpState(false);
    }
    // Between thresholds - maintain current state (hysteresis)
}

void RelayController::update(float waterLevel, float upperThreshold, float lowerThreshold) {
    // Priority order:
    // 1. Hardware override (highest priority)
    // 2. Auto mode (if enabled)
    // 3. Manual/Cloud control

    if (hardwareOverride) {
        // Hardware override is active - maintain current state
        // User controls pump directly via BTN6
        return;
    }

    switch (currentMode) {
        case MODE_AUTO:
            handleAutoMode(waterLevel, upperThreshold, lowerThreshold);
            break;

        case MODE_MANUAL:
            // In manual mode, pump state is controlled by:
            // - Cloud commands (setCloudCommand)
            // - Button presses (turnOn/turnOff)
            // State is already set, no action needed here
            break;

        case MODE_OVERRIDE:
            // Override mode - no automatic changes
            break;
    }
}

void RelayController::turnOn() {
    if (currentMode == MODE_AUTO) {
        Serial.println("[Relay] Cannot manually control in AUTO mode");
        return;
    }

    applyPumpState(true);
}

void RelayController::turnOff() {
    if (currentMode == MODE_AUTO) {
        Serial.println("[Relay] Cannot manually control in AUTO mode");
        return;
    }

    applyPumpState(false);
}

void RelayController::setMode(PumpMode mode) {
    if (currentMode != mode) {
        currentMode = mode;
        saveMode();

        Serial.println("[Relay] Mode changed to: " + getModeString());
    }
}

PumpMode RelayController::getMode() {
    return currentMode;
}

void RelayController::toggleMode() {
    if (currentMode == MODE_AUTO) {
        setMode(MODE_MANUAL);
    } else {
        setMode(MODE_AUTO);
    }
}

bool RelayController::isPumpOn() {
    return pumpState;
}

int RelayController::getPumpStatus() {
    // Return as number: 0=OFF, 1=ON (for telemetry)
    return pumpState ? 1 : 0;
}

void RelayController::setCloudCommand(bool state) {
    cloudCommand = state;

    if (currentMode == MODE_MANUAL) {
        applyPumpState(state);
    } else {
        Serial.println("[Relay] Cloud command received but ignored (not in MANUAL mode)");
    }
}

void RelayController::setHardwareOverride(bool state) {
    if (hardwareOverride != state) {
        hardwareOverride = state;

        if (state) {
            currentMode = MODE_OVERRIDE;
            Serial.println("[Relay] Hardware override activated");
        } else {
            // Return to previous mode
            loadMode();
            Serial.println("[Relay] Hardware override deactivated, returning to " + getModeString());
        }
    }
}

bool RelayController::isHardwareOverride() {
    return hardwareOverride;
}

String RelayController::getModeString() {
    switch (currentMode) {
        case MODE_AUTO:
            return "AUTO";
        case MODE_MANUAL:
            return "MANUAL";
        case MODE_OVERRIDE:
            return "OVERRIDE";
        default:
            return "UNKNOWN";
    }
}
