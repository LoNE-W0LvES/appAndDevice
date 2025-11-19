#include "calculate_level.h"

// Global instance
LevelCalculator levelCalculator;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

LevelCalculator::LevelCalculator() {
    tankHeight = 0;
    tankWidth = 0;
    tankShape = "Cylindrical";
    waterLevel = 0;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void LevelCalculator::begin(float height, float width, const String& shape) {
    setTankConfig(height, width, shape);
}

void LevelCalculator::setTankConfig(float height, float width, const String& shape) {
    tankHeight = height;
    tankWidth = width;
    tankShape = shape;

    Serial.println("[LevelCalc] Tank config updated:");
    Serial.println("  Height: " + String(tankHeight) + " cm");
    Serial.println("  Width: " + String(tankWidth) + " cm");
    Serial.println("  Shape: " + tankShape);
    Serial.println("  Volume: " + String(getTankVolume()) + " L");
}

// ============================================================================
// WATER LEVEL CALCULATION
// ============================================================================

void LevelCalculator::updateLevel(float distance) {
    // Calculate water level from distance sensor reading
    // Distance is measured from sensor at TOP of tank to water surface
    // Water level is measured from BOTTOM of tank
    waterLevel = tankHeight - distance;

    // Clamp to valid range [0, tankHeight]
    if (waterLevel < 0) {
        waterLevel = 0;
    }
    if (waterLevel > tankHeight) {
        waterLevel = tankHeight;
    }

    Serial.printf("[LevelCalc] Distance: %.2f cm, Water Level: %.2f cm (%.1f%%)\n",
                 distance, waterLevel, getWaterLevelPercent());
}

// ============================================================================
// GETTERS
// ============================================================================

float LevelCalculator::getWaterLevel() const {
    return waterLevel;
}

float LevelCalculator::getWaterLevelPercent() const {
    if (tankHeight <= 0) {
        return 0;
    }

    float percent = (waterLevel / tankHeight) * 100.0;

    // Clamp to 0-100%
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return percent;
}

float LevelCalculator::getTankVolume() const {
    if (tankShape == "Cylindrical") {
        // Volume = π * r² * h
        float radius = tankWidth / 2.0;
        float volumeCm3 = 3.14159 * radius * radius * tankHeight;
        return volumeCm3 / 1000.0; // Convert to liters
    } else if (tankShape == "Rectangular") {
        // Volume = width * width * h
        float volumeCm3 = tankWidth * tankWidth * tankHeight;
        return volumeCm3 / 1000.0; // Convert to liters
    }

    return 0;
}

float LevelCalculator::getCurrentVolume() const {
    return calculateVolume(waterLevel);
}

// ============================================================================
// HELPER METHODS
// ============================================================================

float LevelCalculator::calculateVolume(float level) const {
    if (tankShape == "Cylindrical") {
        // Volume = π * r² * h
        float radius = tankWidth / 2.0;
        float volumeCm3 = 3.14159 * radius * radius * level;
        return volumeCm3 / 1000.0; // Convert to liters
    } else if (tankShape == "Rectangular") {
        // Volume = width * width * h
        float volumeCm3 = tankWidth * tankWidth * level;
        return volumeCm3 / 1000.0; // Convert to liters
    }

    return 0;
}
