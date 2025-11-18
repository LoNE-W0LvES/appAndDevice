/**
 * ESP32-S3 Water Tank Monitoring Device
 *
 * Main application that integrates:
 * - WiFi connectivity with AP fallback
 * - Backend API integration (JWT authentication)
 * - Ultrasonic sensor reading
 * - Relay control (auto/manual/cloud modes)
 * - OLED display (3 screens)
 * - Button controls
 * - Local web server for Flutter app
 * - OTA firmware updates
 *
 * Data Flow:
 * Startup: Connect WiFi → Login → Fetch config → Start webserver
 * Every 1s: Update sensor readings
 * Every 30s: Upload telemetry
 * Every 5min: Fetch control data → Check config_update → Check force_update
 */

#include <Arduino.h>
#include "config.h"
#include "storage_manager.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "sensor_manager.h"
#include "relay_controller.h"
#include "display_manager.h"
#include "button_handler.h"
#include "webserver.h"
#include "ota_updater.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

APIClient apiClient;
SensorManager sensorManager;
RelayController relayController;
DisplayManager displayManager;
ButtonHandler buttonHandler;
WebServer webServer;
OTAUpdater otaUpdater;

// ============================================================================
// GLOBAL STATE
// ============================================================================

DeviceConfig deviceConfig;
DeviceConfig lastSyncedConfig;  // Track last synced config (oldData)
ControlData controlData;

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastTelemetryUpload = 0;
unsigned long lastControlFetch = 0;
unsigned long lastConfigCheck = 0;
unsigned long lastOTACheck = 0;
unsigned long lastDisplayUpdate = 0;

// System state
bool systemInitialized = false;
bool configFetched = false;

// ============================================================================
// CALLBACK FUNCTIONS
// ============================================================================

/**
 * Pump control callback for web server
 * Called when Flutter app sends pump control command via local webserver
 *
 * IMPORTANT: App commands via local webserver should always work,
 * regardless of mode. Mode restriction only applies to cloud commands.
 */
void onPumpControl(bool state) {
    Serial.println("[Main] Web server pump control: " + String(state ? "ON" : "OFF"));

    // App has direct control via local webserver - always apply
    if (state) {
        relayController.turnOn();
        Serial.println("[Main] Pump turned ON by app");
    } else {
        relayController.turnOff();
        Serial.println("[Main] Pump turned OFF by app");
    }
}

/**
 * WiFi credentials save callback for web server
 * Called when user submits WiFi credentials via provisioning interface
 */
void onWiFiSave(const String& ssid, const String& password,
                const String& dashUser, const String& dashPass) {
    Serial.println("[Main] WiFi credentials received from web interface");
    Serial.println("[Main] Attempting to connect to: " + ssid);

    // Display connection status
    displayManager.showMessage("Connecting...", ssid, 0);

    // Start WiFi client connection
    startWiFiClient();

    // Wait for connection to complete (non-blocking check in loop)
    unsigned long startTime = millis();
    while (isWiFiConnecting() && (millis() - startTime < WIFI_TIMEOUT_MS)) {
        updateWiFiConnection();
        delay(100);
    }

    if (isWiFiConnected() && getWiFiMode() == WIFI_CLIENT_MODE) {
        Serial.println("[Main] WiFi connected successfully!");
        displayManager.showMessage("Connected!", getIPAddress(), 3000);

        // Initialize backend API
        String hardwareId = getMACAddress();
        apiClient.begin(hardwareId);

        // Authentication flow:
        // Admin pre-creates device in dashboard, then device calls login
        // Login will claim the device to user AND return JWT token in one step

        if (apiClient.isAuthenticated()) {
            // Already have valid token - skip authentication
            Serial.println("[Main] Valid token found - skipping authentication");
            displayManager.showMessage("Backend", "Authenticated", 2000);
        } else {
            // Need to authenticate - login to claim device and get JWT token
            Serial.println("[Main] No valid token - attempting login");
            Serial.println("[Main] (Login will claim device to user account if unclaimed)");

            // Load dashboard credentials from storage (entered by user via web portal)
            String dashUsername, dashPassword;
            bool hasCredentials = getDashboardCredentials(dashUsername, dashPassword);

            if (!hasCredentials || dashUsername.length() == 0 || dashPassword.length() == 0) {
                Serial.println("[Main] ERROR: No dashboard credentials found!");
                Serial.println("[Main] Please configure credentials via web portal:");
                Serial.println("[Main]   1. Connect to WiFi AP: " + String(AP_SSID));
                Serial.println("[Main]   2. Go to http://192.168.4.1");
                Serial.println("[Main]   3. Enter WiFi and dashboard credentials");
                displayManager.showMessage("Error", "No credentials", 3000);
                // Continue anyway - device can still work in local mode
            } else {
                Serial.println("[Main] Using dashboard credentials from storage");
                displayManager.showMessage("Backend", "Logging in...", 0);

                if (apiClient.loginDevice(dashUsername, dashPassword)) {
                    Serial.println("[Main] Device logged in successfully");
                    Serial.println("[Main] Device claimed and JWT token obtained");
                    displayManager.showMessage("Backend", "Logged in!", 2000);
                } else {
                    Serial.println("[Main] ERROR: Login failed");
                    Serial.println("[Main] Possible reasons:");
                    Serial.println("[Main]   1. Device not created by admin in dashboard");
                    Serial.println("[Main]   2. Invalid username/password");
                    Serial.println("[Main]   3. Network/server issues");
                    displayManager.showMessage("Backend", "Login failed", 3000);
                    // Continue anyway - device can still work in local mode
                }
            }
        }

        // Try to fetch config from server (works even if registration failed)
        if (apiClient.fetchAndApplyServerConfig(deviceConfig)) {
            sensorManager.setTankConfig(
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape
            );

            displayManager.setTankSettings(
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape,
                deviceConfig.upperThreshold,
                deviceConfig.lowerThreshold
            );
        }

        systemInitialized = true;

        // Restart to enter normal mode
        Serial.println("[Main] WiFi configured. Restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
    } else {
        Serial.println("[Main] WiFi connection failed");
        displayManager.showMessage("Failed", "Check credentials", 3000);
    }
}

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * Initialize all system components
 */
void initializeSystem() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("  ESP32-S3 Water Tank Monitor");
    Serial.println("  Firmware: " + String(FIRMWARE_VERSION));
    Serial.println("========================================\n");

    // Initialize display first (for visual feedback)
    if (!displayManager.begin()) {
        Serial.println("[Main] ERROR: Display initialization failed!");
    }

    displayManager.showMessage("Starting...", "Initializing system", 2000);

    // Initialize storage manager
    storageManager.begin();

    // Initialize WiFi manager
    initWiFiManager();

    // Initialize sensors
    sensorManager.begin();

    // Initialize relay
    relayController.begin();

    // Initialize buttons
    buttonHandler.begin();

    // Initialize OTA
    otaUpdater.begin();

    Serial.println("[Main] System components initialized");
}

/**
 * Connect to WiFi and backend
 */
bool connectToBackend() {
    // Load WiFi credentials and attempt connection
    displayManager.showMessage("WiFi", "Connecting...", 0);

    String ssid, password;
    if (loadWiFiCredentials(ssid, password)) {
        // Start WiFi client connection
        startWiFiClient();

        // Wait for connection to complete (non-blocking check in loop)
        unsigned long startTime = millis();
        while (isWiFiConnecting() && (millis() - startTime < WIFI_TIMEOUT_MS)) {
            updateWiFiConnection();
            delay(100);
        }
    }

    // If not connected, start AP mode
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE) {
        Serial.println("[Main] WiFi connection failed, running in AP mode");
        startWiFiAP();
        displayManager.showMessage("AP Mode", AP_SSID, 3000);

        // Start web server for WiFi configuration (no API client yet in AP mode)
        webServer.begin(DEVICE_ID, nullptr);
        webServer.setWiFiSaveCallback(onWiFiSave);
        return false;
    }

    // Initialize API client
    String hardwareId = getMACAddress();
    apiClient.begin(hardwareId);

    // Login to get JWT token if not authenticated
    if (!apiClient.isAuthenticated()) {
        // Load dashboard credentials from storage
        String dashUsername, dashPassword;
        bool hasCredentials = getDashboardCredentials(dashUsername, dashPassword);

        if (!hasCredentials || dashUsername.length() == 0 || dashPassword.length() == 0) {
            Serial.println("[Main] ERROR: No dashboard credentials found!");
            Serial.println("[Main] Cannot connect to backend without credentials");
            displayManager.showMessage("Error", "No credentials", 3000);
            return false;
        }

        displayManager.showMessage("Backend", "Logging in...", 0);

        if (!apiClient.loginDevice(dashUsername, dashPassword)) {
            Serial.println("[Main] ERROR: Device login failed!");
            Serial.println("[Main] Ensure device is created by admin in dashboard");
            displayManager.showMessage("Error", "Login failed", 3000);
            return false;
        }

        Serial.println("[Main] Device logged in successfully");
    }

    // Handle device coming online - sync time and config based on priority flag
    // If device_config_sync_status = true: fetches FROM server
    // If device_config_sync_status = false: sends TO server with priority
    displayManager.showMessage("Backend", "Syncing...", 0);
    apiClient.onDeviceOnline(deviceConfig);

    // Initialize last synced config (oldData = newData)
    lastSyncedConfig = deviceConfig;

    configFetched = true;

    // Apply configuration to sensor manager
    sensorManager.setTankConfig(
        deviceConfig.tankHeight,
        deviceConfig.tankWidth,
        deviceConfig.tankShape
    );

    // Update display settings
    displayManager.setTankSettings(
        deviceConfig.tankHeight,
        deviceConfig.tankWidth,
        deviceConfig.tankShape,
        deviceConfig.upperThreshold,
        deviceConfig.lowerThreshold
    );

    Serial.println("[Main] Device online sync complete, config applied");

    // Update network info
    displayManager.setNetworkInfo(
        getIPAddress(),
        "Connected"
    );

    // Start web server
    webServer.begin(DEVICE_ID, &apiClient);
    webServer.setPumpControlCallback(onPumpControl);
    webServer.setWiFiSaveCallback(onWiFiSave);
    webServer.setConfigSyncCallback(syncConfigToServer);  // Immediate sync when config changes

    displayManager.showMessage("Ready", "System online", 2000);

    return true;
}

// ============================================================================
// MAIN TASK FUNCTIONS
// ============================================================================

/**
 * Update sensor readings (every 1 second)
 */
void updateSensors() {
    sensorManager.update();

    float waterLevel = sensorManager.getWaterLevel();
    float waterLevelPercent = sensorManager.getWaterLevelPercent();
    float currInflow = sensorManager.getCurrentInflow();

    // Update relay controller with current water level
    relayController.update(
        waterLevelPercent,
        deviceConfig.upperThreshold,
        deviceConfig.lowerThreshold
    );

    // Update web server data
    int pumpStatus = relayController.getPumpStatus();
    webServer.updateSensorData(waterLevel, currInflow, pumpStatus);
}

/**
 * Upload telemetry to backend (every 30 seconds)
 */
void uploadTelemetry() {
    if (!isWiFiConnected() || !apiClient.isAuthenticated()) {
        Serial.println("[Main] Cannot upload telemetry - not connected");
        return;
    }

    float waterLevel = sensorManager.getWaterLevel();
    float currInflow = sensorManager.getCurrentInflow();
    int pumpStatus = relayController.getPumpStatus();

    if (apiClient.uploadTelemetry(waterLevel, currInflow, pumpStatus)) {
        Serial.println("[Main] Telemetry uploaded successfully");
    } else {
        Serial.println("[Main] Failed to upload telemetry");
    }
}

/**
 * Fetch control data from backend (every 5 minutes)
 */
void fetchControlData() {
    if (!isWiFiConnected() || !apiClient.isAuthenticated()) {
        Serial.println("[Main] Cannot fetch control - not connected");
        return;
    }

    if (apiClient.fetchControl(controlData)) {
        Serial.println("[Main] Control data fetched");

        // Update webserver with latest control data
        webServer.updateControlData(controlData);

        // Apply pump switch command
        if (relayController.getMode() == MODE_MANUAL) {
            relayController.setCloudCommand(controlData.pumpSwitch);
        }

        // Check config update flag
        if (controlData.config_update) {
            Serial.println("[Main] Config update requested, re-fetching configuration...");

            if (apiClient.fetchAndApplyServerConfig(deviceConfig)) {
                // Apply new configuration
                sensorManager.setTankConfig(
                    deviceConfig.tankHeight,
                    deviceConfig.tankWidth,
                    deviceConfig.tankShape
                );

                displayManager.setTankSettings(
                    deviceConfig.tankHeight,
                    deviceConfig.tankWidth,
                    deviceConfig.tankShape,
                    deviceConfig.upperThreshold,
                    deviceConfig.lowerThreshold
                );

                webServer.updateDeviceConfig(deviceConfig);

                Serial.println("[Main] Configuration updated successfully");
            }
        }
    } else {
        Serial.println("[Main] Failed to fetch control data");
    }
}

/**
 * Check for OTA firmware updates (every 5 minutes)
 */
void checkOTAUpdate() {
    if (!isWiFiConnected() || !apiClient.isAuthenticated()) {
        Serial.println("[Main] Cannot check OTA - not connected");
        return;
    }

    // Check force_update flag
    if (deviceConfig.force_update) {
        Serial.println("[Main] Force update flag detected, starting OTA update...");

        displayManager.showMessage("OTA Update", "Downloading...", 0);

        if (otaUpdater.checkAndUpdate(apiClient.getToken())) {
            // Device will restart after successful update
        } else {
            displayManager.showMessage("OTA Failed", otaUpdater.getLastError(), 3000);
        }
    }
}

/**
 * Sync config to server if locally modified
 * Only syncs if data has actually changed (oldData != newData)
 * Called immediately when config changes (via callback from webserver)
 */
void syncConfigToServer() {
    if (!isWiFiConnected() || !apiClient.isAuthenticated()) {
        return;  // Skip if not connected
    }

    // Check if device has pending config changes to sync
    if (apiClient.hasPendingConfigSync()) {
        // Check if config values have actually changed (oldData != newData)
        if (deviceConfig.valuesChanged(lastSyncedConfig)) {
            Serial.println("[Main] Config values changed - syncing to server...");
            Serial.println("  Upper Threshold: " + String(lastSyncedConfig.upperThreshold) + " → " + String(deviceConfig.upperThreshold));
            Serial.println("  Lower Threshold: " + String(lastSyncedConfig.lowerThreshold) + " → " + String(deviceConfig.lowerThreshold));

            // Trigger sync (this will send config to server with priority)
            apiClient.onDeviceOnline(deviceConfig);

            // Update last synced config (oldData = newData)
            lastSyncedConfig = deviceConfig;

            // Reapply config after sync
            sensorManager.setTankConfig(
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape
            );
            displayManager.setTankSettings(
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape,
                deviceConfig.upperThreshold,
                deviceConfig.lowerThreshold
            );
            webServer.updateDeviceConfig(deviceConfig);
        } else {
            Serial.println("[Main] Config values unchanged - skipping sync");
        }
    }
}

/**
 * Fetch config from server periodically (every 30 seconds)
 * Only fetches when there's NO pending config to upload (bidirectional sync)
 * If hasPendingConfigSync() == true, skip fetch to avoid conflicts
 */
void fetchConfigFromServer() {
    if (!isWiFiConnected() || !apiClient.isAuthenticated()) {
        return;  // Skip if not connected
    }

    // IMPORTANT: Only fetch FROM server if we don't have pending changes TO server
    // This prevents overwriting local changes that haven't been synced yet
    if (apiClient.hasPendingConfigSync()) {
        Serial.println("[Main] Skipping server fetch - pending local changes to sync first");
        return;
    }

    Serial.println("[Main] Fetching config from server...");

    // Fetch latest config from server
    if (apiClient.fetchAndApplyServerConfig(deviceConfig)) {
        // Update last synced config (oldData = newData)
        lastSyncedConfig = deviceConfig;

        // Apply new configuration
        sensorManager.setTankConfig(
            deviceConfig.tankHeight,
            deviceConfig.tankWidth,
            deviceConfig.tankShape
        );

        displayManager.setTankSettings(
            deviceConfig.tankHeight,
            deviceConfig.tankWidth,
            deviceConfig.tankShape,
            deviceConfig.upperThreshold,
            deviceConfig.lowerThreshold
        );

        webServer.updateDeviceConfig(deviceConfig);

        Serial.println("[Main] Config fetched and applied from server");
    } else {
        Serial.println("[Main] Failed to fetch config from server");
    }
}

/**
 * Update OLED display (every 0.5 seconds)
 */
void updateDisplay() {
    float waterLevel = sensorManager.getWaterLevel();
    float waterLevelPercent = sensorManager.getWaterLevelPercent();
    bool pumpOn = relayController.isPumpOn();
    String pumpMode = relayController.getModeString();
    int rssi = getRSSI();
    bool wifiConnected = isWiFiConnected();

    displayManager.update(
        waterLevel,
        waterLevelPercent,
        pumpOn,
        pumpMode,
        rssi,
        wifiConnected
    );
}

/**
 * Handle button events
 *
 * NOTE: When buttons change device config (e.g., thresholds), use:
 *   deviceConfig.upperThreshold = newValue;
 *   deviceConfig.lastModified = apiClient.getCurrentTimestamp();
 *   apiClient.markConfigModified();
 *
 * This marks the config as locally modified, triggering priority-based sync
 * (device_config_sync_status = false). When device reconnects to server,
 * the local changes will be sent with lastModified=0 (priority flag) and
 * will override server values.
 */
void handleButtons() {
    buttonHandler.update();

    ButtonEvent event = buttonHandler.getEvent();

    switch (event) {
        case BTN1_PRESSED:
            // Cycle display screens
            displayManager.nextScreen();
            break;

        case BTN2_PRESSED:
            // Manual pump ON
            if (relayController.getMode() == MODE_MANUAL) {
                relayController.turnOn();
                displayManager.showMessage("Pump", "Turned ON", 1000);
            } else {
                displayManager.showMessage("Error", "Not in MANUAL mode", 2000);
            }
            break;

        case BTN3_PRESSED:
            // Toggle Auto/Manual mode
            relayController.toggleMode();
            displayManager.showMessage("Mode", relayController.getModeString(), 1500);
            break;

        case BTN4_PRESSED:
            // Manual pump OFF
            if (relayController.getMode() == MODE_MANUAL) {
                relayController.turnOff();
                displayManager.showMessage("Pump", "Turned OFF", 1000);
            } else {
                displayManager.showMessage("Error", "Not in MANUAL mode", 2000);
            }
            break;

        case BTN5_LONG_PRESS:
            // Reset WiFi credentials
            displayManager.showMessage("WiFi Reset", "Clearing credentials...", 2000);
            clearWiFiCredentials();
            delay(1000);
            ESP.restart();
            break;

        case BTN6_PRESSED:
            {
                // Hardware override toggle
                bool currentOverride = relayController.isHardwareOverride();
                relayController.setHardwareOverride(!currentOverride);
                displayManager.showMessage("Override",
                    !currentOverride ? "ENABLED" : "DISABLED", 1500);
            }
            break;

        default:
            // No event
            break;
    }
}

// ============================================================================
// ARDUINO SETUP AND LOOP
// ============================================================================

void setup() {
    // Initialize system
    initializeSystem();

    // Connect to backend
    systemInitialized = connectToBackend();

    if (systemInitialized) {
        Serial.println("[Main] System fully initialized and connected");
    } else {
        Serial.println("[Main] System initialized but not connected (AP mode)");
    }

    // Initialize timing
    lastSensorRead = millis();
    lastTelemetryUpload = millis();
    lastControlFetch = millis();
    lastConfigCheck = millis();
    lastOTACheck = millis();
    lastDisplayUpdate = millis();

    Serial.println("[Main] Entering main loop...\n");
}

void loop() {
    unsigned long currentTime = millis();
    static bool wasConnected = false;

    // Handle WiFi connection
    handleWiFiConnection();

    // Handle buttons
    handleButtons();

    // Detect online/offline transitions
    if (systemInitialized) {
        bool isConnected = isWiFiConnected();

        // Device just came online
        if (isConnected && !wasConnected) {
            Serial.println("[Main] Device transitioned to ONLINE");
            apiClient.onDeviceOnline(deviceConfig);

            // Update last synced config (oldData = newData)
            lastSyncedConfig = deviceConfig;

            // Reapply config after sync
            sensorManager.setTankConfig(
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape
            );
            displayManager.setTankSettings(
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape,
                deviceConfig.upperThreshold,
                deviceConfig.lowerThreshold
            );
            webServer.updateDeviceConfig(deviceConfig);
        }

        // Device just went offline
        if (!isConnected && wasConnected) {
            Serial.println("[Main] Device transitioned to OFFLINE");
            apiClient.onDeviceOffline();
        }

        wasConnected = isConnected;
    }

    // Update sensors (every 1 second)
    if (currentTime - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = currentTime;
        updateSensors();
    }

    // Update display (every 0.5 seconds)
    if (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = currentTime;
        updateDisplay();
    }

    // Only perform backend operations if connected and initialized
    if (systemInitialized && isWiFiConnected()) {

        // Upload telemetry (every 30 seconds)
        if (currentTime - lastTelemetryUpload >= TELEMETRY_UPLOAD_INTERVAL) {
            lastTelemetryUpload = currentTime;
            uploadTelemetry();
        }

        // Fetch config from server periodically (every 30 seconds)
        // Note: Sync TO server is now immediate (triggered by callback when config changes)
        if (currentTime - lastConfigCheck >= TELEMETRY_UPLOAD_INTERVAL) {
            lastConfigCheck = currentTime;
            fetchConfigFromServer();
        }

        // Fetch control data (every 5 minutes)
        if (currentTime - lastControlFetch >= CONTROL_FETCH_INTERVAL) {
            lastControlFetch = currentTime;
            fetchControlData();
        }

        // Check OTA updates (every 5 minutes)
        if (currentTime - lastOTACheck >= OTA_CHECK_INTERVAL) {
            lastOTACheck = currentTime;
            checkOTAUpdate();
        }
    }

    // Small delay to prevent watchdog issues
    delay(10);
}
