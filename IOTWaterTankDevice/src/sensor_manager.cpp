#include "sensor_manager.h"

SensorManager::SensorManager()
    : ultrasonicSensor(nullptr),
      tankHeight(DEFAULT_TANK_HEIGHT),
      tankWidth(DEFAULT_TANK_WIDTH),
      tankShape("Cylindrical"),
      currentDistance(0),
      currentWaterLevel(0),
      previousWaterLevel(0),
      lastReadTime(0),
      previousReadTime(0),
      currentInflow(0),
      bufferIndex(0) {

    // Initialize distance buffer
    for (int i = 0; i < BUFFER_SIZE; i++) {
        distanceBuffer[i] = 0;
    }

    // Create JSN-SR04T sensor object with LOG_LEVEL_SILENT for cleaner output
    // Use LOG_LEVEL_VERBOSE for debugging if needed
    ultrasonicSensor = new JsnSr04T(ULTRASONIC_ECHO_PIN, ULTRASONIC_TRIG_PIN, LOG_LEVEL_SILENT);
}

void SensorManager::begin() {
    // Initialize JSN-SR04T sensor with Serial for logging
    if (ultrasonicSensor != nullptr) {
        ultrasonicSensor->begin(Serial);
    }

    // Start non-blocking measurement timer (500ms interval)
    measureDelay.start(500, AsyncDelay::MILLIS);

    Serial.println("[Sensor] Ultrasonic sensor initialized");
    Serial.println("[Sensor] TRIG: " + String(ULTRASONIC_TRIG_PIN) + ", ECHO: " + String(ULTRASONIC_ECHO_PIN));
    Serial.println("[Sensor] Using JSN-SR04T waterproof sensor");
}

float SensorManager::measureDistance() {
    // JSN-SR04T library uses readDistance() for both triggering and getting result
    // It returns the distance directly in non-blocking mode
    if (ultrasonicSensor != nullptr) {
        // Check if it's time to trigger a new measurement
        if (measureDelay.isExpired()) {
            float distance = ultrasonicSensor->readDistance();
            measureDelay.repeat();

            // JSN-SR04T returns -1 for invalid readings
            if (distance < 0) {
                DEBUG_PRINTLN("[Sensor] Warning: Invalid sensor reading");
                return -1;
            }

            return distance;
        }
    }

    // Return current distance if not time for new measurement
    return currentDistance;
}

float SensorManager::readDistance() {
    float distance = measureDistance();

    if (distance > 0 && distance < 400) { // Valid range: 2cm to 400cm

        // Spike detection filter
        // Water level changes gradually - reject sudden spikes
        // Threshold configured in config.h: SENSOR_SPIKE_THRESHOLD
        if (currentDistance > 0) {
            float distanceChange = abs(distance - currentDistance);

            if (distanceChange > SENSOR_SPIKE_THRESHOLD) {
                DEBUG_PRINTF("[Sensor] Spike detected! Current: %.2f cm, New: %.2f cm, Change: %.2f cm (threshold: %.2f cm) - REJECTED\n",
                            currentDistance, distance, distanceChange, SENSOR_SPIKE_THRESHOLD);
                // Return previous stable reading
                return currentDistance;
            }
        }

        // Valid reading - add to circular buffer for smoothing
        distanceBuffer[bufferIndex] = distance;
        bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

        return getAverageDistance();
    } else {
        // Invalid reading, return previous value
        return currentDistance;
    }
}

float SensorManager::getAverageDistance() {
    float sum = 0;
    int count = 0;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (distanceBuffer[i] > 0) {
            sum += distanceBuffer[i];
            count++;
        }
    }

    return (count > 0) ? (sum / count) : currentDistance;
}

void SensorManager::update() {
    unsigned long currentTime = millis();

    // Store previous values
    previousReadTime = lastReadTime;
    previousWaterLevel = currentWaterLevel;

    // Read new distance
    currentDistance = readDistance();

    // Calculate water level (tankHeight - distance from top)
    currentWaterLevel = tankHeight - currentDistance;

    // Ensure water level is not negative
    if (currentWaterLevel < 0) {
        currentWaterLevel = 0;
    }

    // Ensure water level doesn't exceed tank height
    if (currentWaterLevel > tankHeight) {
        currentWaterLevel = tankHeight;
    }

    lastReadTime = currentTime;

    // Calculate inflow rate
    calculateInflow();

    // Debug output (can be disabled in production)
    // Serial.println("[Sensor] Distance: " + String(currentDistance) + " cm, " +
    //               "Water Level: " + String(currentWaterLevel) + " cm (" +
    //               String(getWaterLevelPercent()) + "%), " +
    //               "Inflow: " + String(currentInflow) + " L/min");
}

void SensorManager::calculateInflow() {
    // Calculate inflow based on water level change over time

    if (previousReadTime == 0 || lastReadTime == previousReadTime) {
        currentInflow = 0;
        return;
    }

    // Time difference in seconds
    float timeDiff = (lastReadTime - previousReadTime) / 1000.0;

    if (timeDiff <= 0) {
        currentInflow = 0;
        return;
    }

    // Water level change in cm
    float levelChange = currentWaterLevel - previousWaterLevel;

    // Calculate volume change based on tank shape
    float volumeChange = 0;

    if (tankShape == "Cylindrical") {
        // Volume = π * r² * h
        // For level change Δh: ΔV = π * r² * Δh
        float radius = tankWidth / 2.0;
        volumeChange = 3.14159 * radius * radius * levelChange; // cm³
    } else if (tankShape == "Rectangular") {
        // Volume = width * width * h (assuming square cross-section)
        // For level change Δh: ΔV = width² * Δh
        volumeChange = tankWidth * tankWidth * levelChange; // cm³
    }

    // Convert cm³ to liters (1L = 1000cm³)
    float volumeChangeLiters = volumeChange / 1000.0;

    // Calculate flow rate in L/min
    float flowRatePerSecond = volumeChangeLiters / timeDiff;
    currentInflow = flowRatePerSecond * 60.0; // Convert to L/min

    // If inflow is negative (water draining), set to 0
    // Only track incoming water
    if (currentInflow < 0) {
        currentInflow = 0;
    }

    // Sanity check: limit to reasonable values
    if (currentInflow > 1000) { // Max 1000 L/min
        currentInflow = 0;
    }
}

float SensorManager::getWaterLevel() {
    return currentWaterLevel;
}

float SensorManager::getWaterLevelPercent() {
    if (tankHeight <= 0) {
        return 0;
    }

    float percent = (currentWaterLevel / tankHeight) * 100.0;

    // Clamp to 0-100%
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return percent;
}

float SensorManager::getCurrentInflow() {
    return currentInflow;
}

void SensorManager::setTankConfig(float height, float width, const String& shape) {
    tankHeight = height;
    tankWidth = width;
    tankShape = shape;

    Serial.println("[Sensor] Tank config updated:");
    Serial.println("  Height: " + String(tankHeight) + " cm");
    Serial.println("  Width: " + String(tankWidth) + " cm");
    Serial.println("  Shape: " + tankShape);
    Serial.println("  Volume: " + String(getTankVolume()) + " L");
}

float SensorManager::getTankVolume() {
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

float SensorManager::getCurrentVolume() {
    return calculateVolume(currentWaterLevel);
}

float SensorManager::calculateVolume(float waterLevel) {
    if (tankShape == "Cylindrical") {
        // Volume = π * r² * h
        float radius = tankWidth / 2.0;
        float volumeCm3 = 3.14159 * radius * radius * waterLevel;
        return volumeCm3 / 1000.0; // Convert to liters
    } else if (tankShape == "Rectangular") {
        // Volume = width * width * h
        float volumeCm3 = tankWidth * tankWidth * waterLevel;
        return volumeCm3 / 1000.0; // Convert to liters
    }

    return 0;
}
