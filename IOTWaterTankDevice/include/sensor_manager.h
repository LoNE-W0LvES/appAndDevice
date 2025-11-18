#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include <jsnsr04t.h>
#include <AsyncDelay.h>

class SensorManager {
public:
    SensorManager();

    // Initialize sensor pins
    void begin();

    // Read distance from ultrasonic sensor (in cm)
    float readDistance();

    // Update sensor readings (call every SENSOR_READ_INTERVAL)
    void update();

    // Get current water level in cm (tankHeight - distance)
    float getWaterLevel();

    // Get current water level as percentage (0-100%)
    float getWaterLevelPercent();

    // Get current inflow rate (calculated from water level changes)
    // Returns L/min based on tank dimensions
    float getCurrentInflow();

    // Set tank configuration for calculations
    void setTankConfig(float height, float width, const String& shape);

    // Get tank volume in liters
    float getTankVolume();

    // Get current water volume in liters
    float getCurrentVolume();

    // Calculate volume from water level
    float calculateVolume(float waterLevel);

private:
    // JSN-SR04T ultrasonic sensor
    JsnSr04T* ultrasonicSensor;
    AsyncDelay measureDelay;

    float tankHeight;
    float tankWidth;
    String tankShape;

    float currentDistance;
    float currentWaterLevel;
    float previousWaterLevel;

    unsigned long lastReadTime;
    unsigned long previousReadTime;

    float currentInflow; // L/min

    // Circular buffer for distance smoothing
    static const int BUFFER_SIZE = 5;
    float distanceBuffer[BUFFER_SIZE];
    int bufferIndex;

    // Spike detection with consecutive stable reading tracking
    static const int STABILITY_BUFFER_SIZE = 5;
    float stabilityBuffer[STABILITY_BUFFER_SIZE];  // Last 5 raw readings
    int stabilityIndex;
    int stabilityCount;  // Number of consecutive stable readings

    // Calculate average distance from buffer
    float getAverageDistance();

    // Check if last N readings are stable (within threshold of each other)
    bool areReadingsStable(int count, float threshold);

    // Calculate inflow rate based on water level change
    void calculateInflow();

    // Ultrasonic sensor reading with timeout protection
    float measureDistance();
};

#endif // SENSOR_MANAGER_H
