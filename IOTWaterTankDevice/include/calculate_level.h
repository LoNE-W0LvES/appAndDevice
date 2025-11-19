#ifndef CALCULATE_LEVEL_H
#define CALCULATE_LEVEL_H

#include <Arduino.h>

// ============================================================================
// WATER LEVEL CALCULATION
// ============================================================================

class LevelCalculator {
public:
    LevelCalculator();

    // Initialize with tank configuration
    void begin(float tankHeight, float tankWidth, const String& tankShape);

    // Update tank configuration
    void setTankConfig(float height, float width, const String& shape);

    // Calculate and update water level from sensor distance
    void updateLevel(float distance);

    // Get current water level (cm from bottom)
    float getWaterLevel() const;

    // Get water level as percentage (0-100%)
    float getWaterLevelPercent() const;

    // Get tank volume in liters
    float getTankVolume() const;

    // Get current water volume in liters
    float getCurrentVolume() const;

private:
    float tankHeight;       // Tank height in cm
    float tankWidth;        // Tank width/diameter in cm
    String tankShape;       // "Cylindrical" or "Rectangular"

    float waterLevel;       // Current water level in cm from bottom

    // Calculate volume from water level
    float calculateVolume(float level) const;
};

// Global instance
extern LevelCalculator levelCalculator;

#endif // CALCULATE_LEVEL_H
