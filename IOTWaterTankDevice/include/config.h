#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "endpoints.h"  // API endpoint definitions

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================

// Comment out to disable debug output
#define DEBUG_ENABLED

// Comment out to disable verbose HTTP response logging
// When disabled, still shows request info and status, but hides response bodies
// #define DEBUG_RESPONSE

#ifdef DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(format, ...)
#endif

#ifdef DEBUG_RESPONSE
  #define DEBUG_RESPONSE_PRINT(x) Serial.print(x)
  #define DEBUG_RESPONSE_PRINTLN(x) Serial.println(x)
  #define DEBUG_RESPONSE_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
  #define DEBUG_RESPONSE_PRINT(x)
  #define DEBUG_RESPONSE_PRINTLN(x)
  #define DEBUG_RESPONSE_PRINTF(format, ...)
#endif

// ============================================================================
// HARDWARE PIN CONFIGURATION
// ============================================================================

// OLED Display (SSD1306 I2C)
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_RESET -1

// Ultrasonic Sensor (HC-SR04)
#define ULTRASONIC_TRIG_PIN 6
#define ULTRASONIC_ECHO_PIN 7

// Relay Control (Pump)
#define RELAY_PIN 5

// Button Inputs
#define BTN1_PIN 10  // Cycle display screens
#define BTN2_PIN 46  // Manual pump ON
#define BTN3_PIN 3   // Toggle Auto/Manual mode
#define BTN4_PIN 18  // Manual pump OFF
#define BTN5_PIN 17  // Reset WiFi (long press)
#define BTN6_PIN 12  // Hardware override switch

// ============================================================================
// BACKEND API CONFIGURATION
// ============================================================================

#define SERVER_URL "http://103.136.236.16"
#define PROJECT_ID "wt001"
#define DEVICE_NAME "DEV-02"
#define MONGODB_DEVICE_ID "690e6a9d092433c0acfb9178"
#define DEVICE_ID "wt001-DEV-02-690e6a9d092433c0acfb9178"
#define FIRMWARE_VERSION "1.0.0"

// Device Credentials
// Note: Device uses dashboard credentials from web portal (user-entered)
// Credentials are stored in NVS: PREF_DASHBOARD_USER and PREF_DASHBOARD_PASS
// User enters credentials via WiFi setup portal at http://192.168.4.1

// API Endpoints - defined in endpoints.h

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================

#define AP_SSID "AquaFlow-Setup"
#define AP_PASSWORD "PassAquaWT001"
#define WIFI_TIMEOUT_MS 20000           // 20 seconds
#define WIFI_RETRY_INTERVAL_MS 30000    // 30 seconds between reconnection attempts
#define WIFI_RECONNECT_INTERVAL 30000   // 30 seconds (legacy)
#define WIFI_TIMEOUT 20000               // 20 seconds (legacy)

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================

#define SENSOR_READ_INTERVAL 1000       // 1 second - sensor reading
#define TELEMETRY_UPLOAD_INTERVAL 30000 // 30 seconds - upload data
#define CONTROL_FETCH_INTERVAL 300000   // 5 minutes - fetch control data
#define CONFIG_CHECK_INTERVAL 300000    // 5 minutes - check config update
#define OTA_CHECK_INTERVAL 300000       // 5 minutes - check firmware update
#define DISPLAY_UPDATE_INTERVAL 500     // 0.5 seconds - update display

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================

#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 5000  // 5 seconds for setup mode

// ============================================================================
// SETUP MODE CONFIGURATION
// ============================================================================

#define SETUP_MODE_TIMEOUT 600000    // 10 minutes
#define WIFI_SCAN_TIMEOUT 10000      // 10 seconds
#define WIFI_CONNECT_TIMEOUT 30000   // 30 seconds
#define WIFI_CONNECT_RETRIES 3

// ============================================================================
// API RETRY CONFIGURATION
// ============================================================================

#define API_RETRY_COUNT 3
#define API_RETRY_DELAY_MS 2000
#define HTTP_TIMEOUT 10000

// ============================================================================
// DEFAULT VALUES
// ============================================================================

#define DEFAULT_TANK_HEIGHT 100.0f
#define DEFAULT_UPPER_THRESHOLD 85.0f
#define DEFAULT_LOWER_THRESHOLD 20.0f
#define DEFAULT_TANK_WIDTH 50.0f

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================

// Spike detection threshold (cm)
// Maximum realistic water level change per measurement interval (500ms)
// 20cm/0.5s = 40cm/s is extremely fast for a water tank
// Adjust lower for slower systems, higher for fast-filling industrial tanks
#define SENSOR_SPIKE_THRESHOLD 20.0f  // cm

// Sensor filter enable/disable
// When enabled, applies filtering/smoothing to sensor readings
// Can be toggled via device config from server or app
#define DEFAULT_SENSOR_FILTER true

// ============================================================================
// PREFERENCES KEYS (NVS Storage)
// ============================================================================

#define PREF_NAMESPACE "watertank"
#define PREF_WIFI_SSID "wifi_ssid"
#define PREF_WIFI_PASS "wifi_pass"
#define PREF_WIFI_CONFIGURED "wifi_configured"
#define PREF_DASHBOARD_USER "dash_user"
#define PREF_DASHBOARD_PASS "dash_pass"
#define PREF_DEVICE_TOKEN "device_token"
#define PREF_HARDWARE_ID "hardware_id"
#define PREF_AUTO_MODE "auto_mode"

// Sync status keys
#define PREF_SERVER_SYNC "server_sync"
#define PREF_CONFIG_SYNC "config_sync"
#define PREF_SERVER_TIME "server_time"
#define PREF_MILLIS_SYNC "millis_sync"
#define PREF_OVERFLOW_CNT "overflow_cnt"

#endif // CONFIG_H
