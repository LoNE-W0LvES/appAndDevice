#include "handle_telemetry_data.h"
#include "config.h"

void TelemetryDataHandler::begin() {
    waterLevel = 0.0f;
    distance = 0.0f;
    currInflow = 0.0f;
    pumpStatus = 0.0f;
    isOnline = 1.0f;  // Device is online by default
    timestamp = 0;

    DEBUG_PRINTLN("[TelemetryHandler] Initialized");
}

void TelemetryDataHandler::update(float waterLevel, float distance, float currInflow,
                                   float pumpStatus, float isOnline) {
    this->waterLevel = waterLevel;
    this->distance = distance;
    this->currInflow = currInflow;
    this->pumpStatus = pumpStatus;
    this->isOnline = isOnline;
    this->timestamp = millis();

    DEBUG_PRINTLN("[TelemetryHandler] Updated");
    DEBUG_PRINTF("  waterLevel: %.2f%%\n", waterLevel);
    DEBUG_PRINTF("  distance: %.2f cm\n", distance);
    DEBUG_PRINTF("  currInflow: %.2f L/min\n", currInflow);
    DEBUG_PRINTF("  pumpStatus: %.0f\n", pumpStatus);
}

void TelemetryDataHandler::printState() {
    Serial.println("[TelemetryHandler] Current State:");
    Serial.printf("  Water Level: %.2f%%\n", waterLevel);
    Serial.printf("  Distance: %.2f cm\n", distance);
    Serial.printf("  Inflow: %.2f L/min\n", currInflow);
    Serial.printf("  Pump: %s\n", pumpStatus > 0 ? "ON" : "OFF");
    Serial.printf("  Online: %s\n", isOnline > 0 ? "YES" : "NO");
    Serial.printf("  Timestamp: %llu ms\n", timestamp);
}
