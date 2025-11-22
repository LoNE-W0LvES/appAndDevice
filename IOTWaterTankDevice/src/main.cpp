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
#include <ESP32Ping.h>
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
#include "handle_control_data.h"
#include "handle_config_data.h"
#include "handle_telemetry_data.h"
#include "calculate_level.h"

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

// 3-way sync handlers
ControlDataHandler controlHandler;
ConfigDataHandler configHandler;
TelemetryDataHandler telemetryHandler;

// ============================================================================
// GLOBAL STATE
// ============================================================================

// Old config/control data - DEPRECATED, use handlers instead
// TODO: Remove after full integration
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
bool deviceIsOnline = false;       // True when NTP sync successful and device can communicate with server
bool initial_config_update = false; // True after NTP sync until first config fetch completes
bool needReboot = false;           // True after midnight until device restarts
int failedCount = 0;               // Count consecutive server request failures (reset deviceIsOnline after 10)
unsigned long lastNTPRetry = 0;    // Track NTP retry attempts when offline
unsigned long lastMidnightCheck = 0; // Track when we last checked for midnight

// FreeRTOS task management
SemaphoreHandle_t configMutex = NULL;  // Protect deviceConfig and lastSyncedConfig
TaskHandle_t telemetryTaskHandle = NULL;
TaskHandle_t controlTaskHandle = NULL;
TaskHandle_t controlUploadTaskHandle = NULL;
TaskHandle_t configFetchTaskHandle = NULL;
TaskHandle_t configSyncTaskHandle = NULL;
TaskHandle_t ntpSyncTaskHandle = NULL;

// Task limiting to prevent too many concurrent tasks
#define MAX_CONCURRENT_SERVER_TASKS 2  // Maximum 2 server tasks at once
volatile int activeServerTasks = 0;     // Counter for active server tasks

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void syncConfigToServer();
void fetchConfigFromServer();
void uploadControlData();
void ntpSyncTask(void* parameter);
bool syncWithInternet();
void finalizeNTP(uint64_t timestamp);

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

        // Fetch config from server and perform 3-way merge
        bool configChanged = false;
        bool deviceWon = false;
        if (apiClient.fetchAndApplyServerConfig(deviceConfig, &configChanged, &deviceWon)) {
            Serial.println("[Main] Config fetched and merged successfully");

            // If values changed after merge, apply to system components
            if (configChanged) {
                Serial.println("[Main] Applying merged config to system components...");

                sensorManager.setTankConfig(
                    deviceConfig.tankHeight,
                    deviceConfig.tankWidth,
                    deviceConfig.tankShape
                );
                levelCalculator.setTankConfig(
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

                // Save to NVS storage for persistence
                storageManager.saveDeviceConfig(
                    deviceConfig.upperThreshold,
                    deviceConfig.lowerThreshold,
                    deviceConfig.tankHeight,
                    deviceConfig.tankWidth,
                    deviceConfig.tankShape
                );
                Serial.println("[Main] Config applied and saved to NVS");
            } else {
                Serial.println("[Main] Config values unchanged after merge");
            }

            // If device values won, sync to server
            if (deviceWon) {
                Serial.println("[Main] Device config differs from server - syncing to server...");
                if (apiClient.sendConfigWithPriority(deviceConfig)) {
                    Serial.println("[Main] Device config synced to server successfully");
                } else {
                    Serial.println("[Main] Failed to sync device config to server");
                }
            }
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
// INTERNET CONNECTION MANAGEMENT
// ============================================================================

/**
 * Synchronize with internet via NTP
 * Attempts to sync time with NTP servers
 * Returns true if successful, false otherwise
 */
bool syncWithInternet() {
    Serial.println("[Main] Attempting to sync with internet via NTP...");

    // Don't attempt NTP sync in AP mode (no internet)
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE) {
        Serial.println("[Main] Cannot sync - not in client mode (no internet)");
        return false;
    }

    // Attempt NTP time sync
    if (apiClient.syncTimeWithServer()) {
        // Get the synced timestamp
        uint64_t ntpTimestamp = apiClient.getCurrentTimestamp();

        // Validate timestamp (sanity check: must be > 2025-11-19)
        const uint64_t MIN_VALID_TIMESTAMP = 1763520052526ULL;  // 2025-11-19 approx

        if (ntpTimestamp > MIN_VALID_TIMESTAMP) {
            Serial.println("[Main] NTP sync successful!");
            Serial.printf("[Main] Timestamp: %llu ms\n", ntpTimestamp);

            // Finalize NTP and mark device as online
            finalizeNTP(ntpTimestamp);
            return true;
        } else {
            Serial.printf("[Main] NTP sync returned invalid timestamp: %llu (expected > %llu)\n",
                         ntpTimestamp, MIN_VALID_TIMESTAMP);
            return false;
        }
    } else {
        Serial.println("[Main] NTP sync failed");
        return false;
    }
}

/**
 * Finalize NTP synchronization
 * Sets the timestamp and marks device as online
 */
void finalizeNTP(uint64_t timestamp) {
    Serial.println("[Main] Finalizing NTP sync...");

    // Set timestamp in API client
    apiClient.setTimestamp(timestamp);

    // Mark device as online
    deviceIsOnline = true;
    failedCount = 0;  // Reset failure counter

    // Set initial_config_update flag to trigger first config fetch
    initial_config_update = true;

    Serial.println("[Main] Device is now ONLINE");
    Serial.println("[Main] initial_config_update flag set - will fetch config from server");
    displayManager.showMessage("Online", "Internet OK", 2000);
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

    // Initialize 3-way sync handlers
    controlHandler.begin();
    configHandler.begin();
    telemetryHandler.begin();
    Serial.println("[Main] Sync handlers initialized");

    // Load device config from NVS if available
    // This allows device to work offline without server on first boot
    float upperThr, lowerThr, tankH, tankW;
    String tankSh;
    if (storageManager.loadDeviceConfig(upperThr, lowerThr, tankH, tankW, tankSh)) {
        Serial.println("[Main] Device config loaded from NVS storage");
        Serial.printf("  Tank: %.0f x %.0f cm (%s)\n", tankH, tankW, tankSh.c_str());

        // Update config handler with loaded values
        configHandler.updateSelf(upperThr, lowerThr, tankH, tankW, tankSh,
                                 0.0f, 0.0f, false, "", true,
                                 apiClient.getCurrentTimestamp());

        // Also update old deviceConfig for backward compatibility
        deviceConfig.upperThreshold = upperThr;
        deviceConfig.lowerThreshold = lowerThr;
        deviceConfig.tankHeight = tankH;
        deviceConfig.tankWidth = tankW;
        deviceConfig.tankShape = tankSh;
    } else {
        Serial.println("[Main] No stored config - will fetch from server on connect");

        // Initialize config handler with defaults to ensure timestamps are set
        configHandler.updateSelf(DEFAULT_UPPER_THRESHOLD, DEFAULT_LOWER_THRESHOLD,
                                 DEFAULT_TANK_HEIGHT, DEFAULT_TANK_WIDTH, "Cylindrical",
                                 0.0f, 0.0f, false, "", true,
                                 apiClient.getCurrentTimestamp());
    }

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

    // ============================================================================
    // WiFi connected - Start webserver immediately for local control
    // ============================================================================
    Serial.println("[Main] WiFi connected - Starting local webserver");
    Serial.println("[Main] Local IP: " + getIPAddress());

    // Update network info
    displayManager.setNetworkInfo(
        getIPAddress(),
        "Connected"
    );

    // Initialize API client (needed for webserver even if not authenticated)
    String hardwareId = getMACAddress();
    apiClient.begin(hardwareId);

    // Start web server immediately for local control (works even if backend is unreachable)
    webServer.begin(DEVICE_ID, &apiClient);
    webServer.setPumpControlCallback(onPumpControl);
    webServer.setWiFiSaveCallback(onWiFiSave);
    webServer.setConfigSyncCallback(syncConfigToServer);    // Immediate async sync when config changes
    webServer.setControlSyncCallback(uploadControlData);    // Immediate async sync when control changes
    webServer.setTimestampSyncCallback(finalizeNTP);        // Called when app syncs time
    webServer.setComponentPointers(&sensorManager, &displayManager, &storageManager, &levelCalculator);

    Serial.println("[Main] Local webserver started - device accessible at http://" + getIPAddress());

    // ============================================================================
    // STEP 1: Sync with internet via NTP (MUST succeed before any server requests)
    // ============================================================================
    Serial.println("[Main] ========================================");
    Serial.println("[Main] STEP 1: Syncing with internet via NTP");
    Serial.println("[Main] ========================================");

    displayManager.showMessage("NTP Sync", "Connecting...", 0);

    if (!syncWithInternet()) {
        Serial.println("[Main] NTP sync failed - device will work locally only");
        Serial.println("[Main] App can sync time via webserver, or device will retry NTP periodically");
        displayManager.showMessage("Offline", "No internet", 3000);

        // Device works locally, but deviceIsOnline = false
        // Periodic sync will retry NTP every 10-20 seconds
        return true;  // Return true because device is functional locally
    }

    // ============================================================================
    // STEP 2: Authenticate with backend (now that we have internet)
    // ============================================================================
    Serial.println("[Main] ========================================");
    Serial.println("[Main] STEP 2: Authenticating with backend");
    Serial.println("[Main] ========================================");

    // Login to get JWT token if not authenticated
    if (!apiClient.isAuthenticated()) {
        // Load dashboard credentials from storage
        String dashUsername, dashPassword;
        bool hasCredentials = getDashboardCredentials(dashUsername, dashPassword);

        if (!hasCredentials || dashUsername.length() == 0 || dashPassword.length() == 0) {
            Serial.println("[Main] WARNING: No dashboard credentials found!");
            Serial.println("[Main] Device will work locally but cannot sync with backend");
            displayManager.showMessage("Local Mode", "No credentials", 3000);
            deviceIsOnline = false;  // Mark as offline since can't authenticate
            return true;  // Local mode
        }

        displayManager.showMessage("Backend", "Logging in...", 0);

        if (!apiClient.loginDevice(dashUsername, dashPassword)) {
            Serial.println("[Main] WARNING: Device login failed!");
            Serial.println("[Main] Device will work locally but cannot sync with backend");
            displayManager.showMessage("Local Mode", "Login failed", 3000);
            deviceIsOnline = false;  // Mark as offline since can't authenticate
            failedCount++;
            return true;  // Local mode
        }

        Serial.println("[Main] Device logged in successfully");
        displayManager.showMessage("Backend", "Authenticated", 2000);
    }

    // ============================================================================
    // STEP 3: Fetch config from server (only if online)
    // ============================================================================
    Serial.println("[Main] ========================================");
    Serial.println("[Main] STEP 3: Fetching config from server");
    Serial.println("[Main] ========================================");

    bool configChanged = false;
    bool deviceWon = false;

    if (apiClient.fetchAndApplyServerConfig(deviceConfig, &configChanged, &deviceWon)) {
        Serial.println("[Main] Config fetched and merged successfully");

        // Clear initial_config_update flag after successful initial fetch
        if (initial_config_update) {
            Serial.println("[Main] Initial config fetch completed - clearing initial_config_update flag");
            initial_config_update = false;
        }

        // If values changed after merge, apply to system components
        if (configChanged) {
            Serial.println("[Main] Applying merged config to system components...");

            sensorManager.setTankConfig(
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape
            );
            levelCalculator.setTankConfig(
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

            // Save to NVS storage for persistence
            storageManager.saveDeviceConfig(
                deviceConfig.upperThreshold,
                deviceConfig.lowerThreshold,
                deviceConfig.tankHeight,
                deviceConfig.tankWidth,
                deviceConfig.tankShape
            );
            Serial.println("[Main] Config applied and saved to NVS");
        } else {
            Serial.println("[Main] Config values unchanged after merge");
        }

        // If device values won, sync to server
        if (deviceWon) {
            Serial.println("[Main] Device config differs from server - syncing to server...");
            if (apiClient.sendConfigWithPriority(deviceConfig)) {
                Serial.println("[Main] Device config synced to server successfully");
            } else {
                Serial.println("[Main] Failed to sync device config to server");
                failedCount++;
            }
        }

        configFetched = true;
        failedCount = 0;  // Reset on success
    } else {
        Serial.println("[Main] Failed to fetch config from server - using local config");
        failedCount++;

        // Load config from NVS storage (fallback)
        float upperThr, lowerThr, tankH, tankW;
        String tankSh;
        if (storageManager.loadDeviceConfig(upperThr, lowerThr, tankH, tankW, tankSh)) {
            deviceConfig.upperThreshold = upperThr;
            deviceConfig.lowerThreshold = lowerThr;
            deviceConfig.tankHeight = tankH;
            deviceConfig.tankWidth = tankW;
            deviceConfig.tankShape = tankSh;

            sensorManager.setTankConfig(tankH, tankW, tankSh);
            levelCalculator.setTankConfig(tankH, tankW, tankSh);
            displayManager.setTankSettings(tankH, tankW, tankSh, upperThr, lowerThr);

            Serial.println("[Main] Loaded config from NVS storage");
        }
    }

    // Initialize last synced config (oldData = newData)
    lastSyncedConfig = deviceConfig;

    Serial.println("[Main] ========================================");
    Serial.println("[Main] Boot sequence complete!");
    Serial.println("[Main] Device is ONLINE and ready");
    Serial.println("[Main] ========================================");

    displayManager.showMessage("Ready", "System online", 2000);

    return true;
}

// ============================================================================
// ASYNC TASK FUNCTIONS (FreeRTOS)
// ============================================================================

/**
 * Async task: Upload telemetry to backend
 * Runs in background to prevent blocking main loop during network delays
 */
void uploadTelemetryTask(void* parameter) {
    Serial.println("[AsyncTask] Telemetry upload started");
    activeServerTasks++;  // Increment active task counter

    // Don't attempt server calls if device is offline
    if (!deviceIsOnline) {
        Serial.println("[AsyncTask] Cannot upload telemetry - device is offline");
        activeServerTasks--;  // Decrement before exit
        telemetryTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Don't attempt server calls in AP mode (no internet) or when not authenticated
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE || !apiClient.isAuthenticated()) {
        Serial.println("[AsyncTask] Cannot upload telemetry - not in client mode or not authenticated");
        failedCount++;
        if (failedCount >= 10) {
            Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
            deviceIsOnline = false;
        }
        activeServerTasks--;  // Decrement before exit
        telemetryTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Telemetry doesn't need time sync - it's just sensor data for monitoring
    // Time sync is only needed for control/config uploads (conflict resolution)

    float waterLevelPercent = levelCalculator.getWaterLevelPercent();
    float currInflow = sensorManager.getCurrentInflow();
    int pumpStatus = relayController.getPumpStatus();

    if (apiClient.uploadTelemetry(waterLevelPercent, currInflow, pumpStatus)) {
        Serial.println("[AsyncTask] Telemetry uploaded successfully");
        failedCount = 0;  // Reset failure counter on success
    } else {
        Serial.println("[AsyncTask] Failed to upload telemetry");
        failedCount++;
        if (failedCount >= 10) {
            Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
            deviceIsOnline = false;
        }
    }

    activeServerTasks--;  // Decrement after completion
    telemetryTaskHandle = NULL;
    vTaskDelete(NULL);
}

/**
 * Async task: Sync time via NTP at boot
 * Runs once at boot to synchronize device time with NTP servers
 * If NTP fails (no internet), app can later sync time using phone's timestamp
 */
void ntpSyncTask(void* parameter) {
    Serial.println("[AsyncTask] NTP time sync started");

    // Don't attempt NTP sync in AP mode (no internet)
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE) {
        Serial.println("[AsyncTask] Cannot sync time - not in client mode (no internet)");
        Serial.println("[AsyncTask] App can sync time using phone's timestamp when connected");
        ntpSyncTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Attempt NTP time sync
    if (apiClient.syncTimeWithServer()) {
        Serial.println("[AsyncTask] NTP time sync completed successfully");
        displayManager.showMessage("Time Synced", "NTP", 2000);
    } else {
        Serial.println("[AsyncTask] NTP time sync failed - will retry later or use app sync");
        displayManager.showMessage("Time Not Synced", "Use app", 2000);
    }

    ntpSyncTaskHandle = NULL;
    vTaskDelete(NULL);
}

/**
 * Async task: Upload control data to backend
 * Runs in background to prevent blocking main loop during network delays
 * Called immediately when local webserver receives control update from app
 *
 * @param parameter Pointer to heap-allocated String containing pre-built JSON payload
 */
void uploadControlTask(void* parameter) {
    Serial.println("[AsyncTask] Control upload started");
    activeServerTasks++;  // Increment active task counter

    // Extract the pre-built JSON payload from parameter
    String* jsonPayload = (String*)parameter;

    // Don't attempt server calls if device is offline
    if (!deviceIsOnline) {
        Serial.println("[AsyncTask] Cannot upload control - device is offline");
        delete jsonPayload;  // Free allocated memory before exit
        activeServerTasks--;  // Decrement before exit
        controlUploadTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Don't attempt server calls in AP mode (no internet) or when not authenticated
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE || !apiClient.isAuthenticated()) {
        Serial.println("[AsyncTask] Cannot upload control - not in client mode or not authenticated");
        delete jsonPayload;  // Free allocated memory before exit
        failedCount++;
        if (failedCount >= 10) {
            Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
            deviceIsOnline = false;
        }
        activeServerTasks--;  // Decrement before exit
        controlUploadTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Don't send data if time is not synced (timestamps would be invalid)
    if (!apiClient.isTimeSynced()) {
        Serial.println("[AsyncTask] Cannot upload control - time not synced yet");
        delete jsonPayload;  // Free allocated memory before exit
        activeServerTasks--;  // Decrement before exit
        controlUploadTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Upload using the pre-built JSON payload
    // No race condition because JSON was built synchronously when callback was triggered
    if (apiClient.uploadControlWithPayload(*jsonPayload)) {
        Serial.println("[AsyncTask] Control data uploaded to server successfully");
        failedCount = 0;  // Reset failure counter on success
    } else {
        Serial.println("[AsyncTask] Failed to upload control data to server");
        failedCount++;
        if (failedCount >= 10) {
            Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
            deviceIsOnline = false;
        }
    }

    // Free the allocated memory
    delete jsonPayload;

    activeServerTasks--;  // Decrement after completion
    controlUploadTaskHandle = NULL;
    vTaskDelete(NULL);
}

/**
 * Async task: Fetch control data from backend
 * Runs in background to prevent blocking main loop during network delays
 */
void fetchControlDataTask(void* parameter) {
    Serial.println("[AsyncTask] Control fetch started");
    activeServerTasks++;  // Increment active task counter

    // Don't attempt server calls if device is offline
    if (!deviceIsOnline) {
        Serial.println("[AsyncTask] Cannot fetch control - device is offline");
        activeServerTasks--;  // Decrement before exit
        controlTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Don't attempt server calls in AP mode (no internet) or when not authenticated
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE || !apiClient.isAuthenticated()) {
        Serial.println("[AsyncTask] Cannot fetch control - not in client mode or not authenticated");
        failedCount++;
        if (failedCount >= 10) {
            Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
            deviceIsOnline = false;
        }
        activeServerTasks--;  // Decrement before exit
        controlTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    ControlData tempControlData;
    if (apiClient.fetchControl(tempControlData)) {
        Serial.println("[AsyncTask] Control data fetched");
        failedCount = 0;  // Reset failure counter on success

        // Take mutex before updating shared state
        if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
            controlData = tempControlData;

            // Update webserver with latest control data
            webServer.updateControlData(controlData);

            // Apply pump switch command
            if (relayController.getMode() == MODE_MANUAL) {
                relayController.setCloudCommand(controlData.pumpSwitch);
            }

            // Check config update flag
            if (controlData.config_update) {
                Serial.println("[AsyncTask] Config update requested, re-fetching configuration...");

                // Store previous config to detect changes
                DeviceConfig previousConfig = deviceConfig;

                if (apiClient.fetchAndApplyServerConfig(deviceConfig)) {
                    // Clear initial_config_update flag after successful fetch
                    if (initial_config_update) {
                        Serial.println("[AsyncTask] Initial config fetch completed - clearing initial_config_update flag");
                        initial_config_update = false;
                    }

                    // Check if values actually changed during merge
                    bool valuesChanged = deviceConfig.valuesChanged(previousConfig);

                    // Check if merged values differ from server values
                    if (configHandler.valuesDifferFromAPI()) {
                        Serial.println("[AsyncTask] Merged values differ from server - syncing back to server...");
                        apiClient.markConfigModified();
                        // Note: Don't call syncConfigToServer here, let main loop handle it
                    }

                    // Only apply and save if values actually changed
                    if (valuesChanged) {
                        Serial.println("[AsyncTask] Config values changed - applying and saving...");

                        // Apply new configuration
                        sensorManager.setTankConfig(
                            deviceConfig.tankHeight,
                            deviceConfig.tankWidth,
                            deviceConfig.tankShape
                        );
                        levelCalculator.setTankConfig(
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

                        // Save config to NVS for persistence across reboots
                        storageManager.saveDeviceConfig(
                            deviceConfig.upperThreshold,
                            deviceConfig.lowerThreshold,
                            deviceConfig.tankHeight,
                            deviceConfig.tankWidth,
                            deviceConfig.tankShape
                        );

                        Serial.println("[AsyncTask] Configuration updated successfully");
                    } else {
                        Serial.println("[AsyncTask] Config fetched but values unchanged - skipping save");
                    }

                    // Reset config_update flag to false after processing
                    Serial.println("[AsyncTask] Resetting config_update flag to false...");
                    controlHandler.setConfigUpdatePriority(false);

                    // Upload control data to inform server that config_update has been processed
                    ControlData resetControl;
                    resetControl.pumpSwitch = controlHandler.getPumpSwitch();
                    resetControl.pumpSwitchLastModified = controlHandler.getPumpSwitchTimestamp();
                    resetControl.config_update = false;  // Reset to false
                    resetControl.configUpdateLastModified = 0;  // Priority flag

                    if (apiClient.uploadControl(resetControl)) {
                        Serial.println("[AsyncTask] config_update flag reset successfully");
                    } else {
                        Serial.println("[AsyncTask] Failed to reset config_update flag");
                    }
                }
            }

            xSemaphoreGive(configMutex);
        }
    } else {
        Serial.println("[AsyncTask] Failed to fetch control data");
        failedCount++;
        if (failedCount >= 10) {
            Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
            deviceIsOnline = false;
        }
    }

    activeServerTasks--;  // Decrement after completion
    controlTaskHandle = NULL;
    vTaskDelete(NULL);
}

/**
 * Async task: Fetch config from server
 * Runs in background to prevent blocking main loop during network delays
 */
void fetchConfigFromServerTask(void* parameter) {
    Serial.println("[AsyncTask] Config fetch started");
    activeServerTasks++;  // Increment active task counter

    // Don't attempt server calls if device is offline
    if (!deviceIsOnline) {
        Serial.println("[AsyncTask] Cannot fetch config - device is offline");
        activeServerTasks--;  // Decrement before exit
        configFetchTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Don't attempt server calls in AP mode (no internet) or when not authenticated
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE || !apiClient.isAuthenticated()) {
        Serial.println("[AsyncTask] Cannot fetch config - not in client mode or not authenticated");
        failedCount++;
        if (failedCount >= 10) {
            Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
            deviceIsOnline = false;
        }
        activeServerTasks--;  // Decrement before exit
        configFetchTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // IMPORTANT: Only fetch FROM server if we don't have pending changes TO server
    if (apiClient.hasPendingConfigSync()) {
        Serial.println("[AsyncTask] Skipping server fetch - pending local changes to sync first");
        activeServerTasks--;  // Decrement before exit
        configFetchTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    Serial.println("[AsyncTask] Fetching config from server...");

    // Take mutex before accessing deviceConfig
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        // Store previous config to detect changes
        DeviceConfig previousConfig = deviceConfig;

        // Fetch latest config from server
        if (apiClient.fetchAndApplyServerConfig(deviceConfig)) {
            failedCount = 0;  // Reset failure counter on success

            // Clear initial_config_update flag after successful first fetch
            if (initial_config_update) {
                Serial.println("[AsyncTask] Initial config fetch completed - clearing initial_config_update flag");
                initial_config_update = false;
            }

            // Check if values actually changed during merge
            bool valuesChanged = deviceConfig.valuesChanged(previousConfig);

            Serial.printf("[AsyncTask] Values changed check: %s\n", valuesChanged ? "YES" : "NO");
            if (valuesChanged) {
                Serial.println("[AsyncTask] Old vs New values:");
                Serial.printf("  upperThreshold: %.2f -> %.2f\n", previousConfig.upperThreshold, deviceConfig.upperThreshold);
                Serial.printf("  lowerThreshold: %.2f -> %.2f\n", previousConfig.lowerThreshold, deviceConfig.lowerThreshold);
            }

            // Check if merged values differ from server values (API values)
            if (configHandler.valuesDifferFromAPI()) {
                Serial.println("[AsyncTask] Merged values differ from server - syncing back to server...");
                apiClient.markConfigModified();
                // Note: Don't call syncConfigToServer here, let main loop handle it
            } else {
                Serial.println("[AsyncTask] Merged values match server values - no sync needed");
            }

            // Only apply and save if values actually changed
            if (valuesChanged) {
                Serial.println("[AsyncTask] Config values changed - applying and saving...");

                // Update last synced config (oldData = newData)
                lastSyncedConfig = deviceConfig;

                // Apply new configuration
                sensorManager.setTankConfig(
                    deviceConfig.tankHeight,
                    deviceConfig.tankWidth,
                    deviceConfig.tankShape
                );
                levelCalculator.setTankConfig(
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

                // Save config to NVS for persistence across reboots
                storageManager.saveDeviceConfig(
                    deviceConfig.upperThreshold,
                    deviceConfig.lowerThreshold,
                    deviceConfig.tankHeight,
                    deviceConfig.tankWidth,
                    deviceConfig.tankShape
                );

                Serial.println("[AsyncTask] Config fetched and applied from server");
            } else {
                Serial.println("[AsyncTask] Config fetched but values unchanged - skipping save");
            }
        } else {
            Serial.println("[AsyncTask] Failed to fetch config from server");
            failedCount++;
            if (failedCount >= 10) {
                Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
                deviceIsOnline = false;
            }
        }

        xSemaphoreGive(configMutex);
    }

    activeServerTasks--;  // Decrement after completion
    configFetchTaskHandle = NULL;
    vTaskDelete(NULL);
}

/**
 * Async task: Sync config to server
 * Runs in background to prevent blocking main loop during network delays
 */
void syncConfigToServerTask(void* parameter) {
    Serial.println("[AsyncTask] Config sync started");
    activeServerTasks++;  // Increment active task counter

    // Don't attempt server calls in AP mode (no internet) or when not authenticated
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE || !apiClient.isAuthenticated()) {
        Serial.println("[AsyncTask] Cannot sync config - not in client mode or not authenticated");
        activeServerTasks--;  // Decrement before exit
        configSyncTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Don't send data if time is not synced (timestamps would be invalid)
    if (!apiClient.isTimeSynced()) {
        Serial.println("[AsyncTask] Cannot sync config - time not synced yet");
        activeServerTasks--;  // Decrement before exit
        configSyncTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // Take mutex before accessing deviceConfig
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        // Check if device has pending config changes to sync
        if (apiClient.hasPendingConfigSync()) {
            // Check if config values have actually changed (oldData != newData)
            if (deviceConfig.valuesChanged(lastSyncedConfig)) {
                Serial.println("[AsyncTask] Config values changed - syncing to server...");
                Serial.println("  Upper Threshold: " + String(lastSyncedConfig.upperThreshold) + " → " + String(deviceConfig.upperThreshold));
                Serial.println("  Lower Threshold: " + String(lastSyncedConfig.lowerThreshold) + " → " + String(deviceConfig.lowerThreshold));

                // Send config to server with priority (non-blocking, just upload)
                // Don't call onDeviceOnline() - it's BLOCKING with retries and time sync!
                if (apiClient.sendConfigWithPriority(deviceConfig)) {
                    Serial.println("[AsyncTask] Config uploaded to server with priority");

                    // Update last synced config (oldData = newData)
                    lastSyncedConfig = deviceConfig;

                    // Save config to NVS for persistence across reboots
                    storageManager.saveDeviceConfig(
                        deviceConfig.upperThreshold,
                        deviceConfig.lowerThreshold,
                        deviceConfig.tankHeight,
                        deviceConfig.tankWidth,
                        deviceConfig.tankShape
                    );

                    // Reapply config after sync
                    sensorManager.setTankConfig(
                        deviceConfig.tankHeight,
                        deviceConfig.tankWidth,
                        deviceConfig.tankShape
                    );
                    levelCalculator.setTankConfig(
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
                    Serial.println("[AsyncTask] Failed to upload config to server");
                    failedCount++;
                    if (failedCount >= 10) {
                        Serial.println("[AsyncTask] 10 consecutive failures - marking device as OFFLINE");
                        deviceIsOnline = false;
                    }
                }
            } else {
                Serial.println("[AsyncTask] Config values unchanged - skipping sync");
            }
        }

        xSemaphoreGive(configMutex);
    }

    activeServerTasks--;  // Decrement after completion
    configSyncTaskHandle = NULL;
    vTaskDelete(NULL);
}

// ============================================================================
// MAIN TASK FUNCTIONS
// ============================================================================

/**
 * Update sensor readings (every 1 second)
 */
void updateSensors() {
    sensorManager.update();

    float waterLevelPercent = levelCalculator.getWaterLevelPercent();
    float currInflow = sensorManager.getCurrentInflow();

    // Update relay controller with current water level
    relayController.update(
        waterLevelPercent,
        deviceConfig.upperThreshold,
        deviceConfig.lowerThreshold
    );

    // Update web server data (send percentage for telemetry)
    int pumpStatus = relayController.getPumpStatus();
    webServer.updateSensorData(waterLevelPercent, currInflow, pumpStatus);
}

/**
 * Upload telemetry to backend (every 30 seconds)
 * Launches async task to prevent blocking main loop
 */
void uploadTelemetry() {
    // Skip if task is already running
    if (telemetryTaskHandle != NULL) {
        Serial.println("[Main] Telemetry task already running, skipping...");
        return;
    }

    // Check if too many tasks are running (prevent device crash)
    if (activeServerTasks >= MAX_CONCURRENT_SERVER_TASKS) {
        Serial.printf("[Main] Too many active tasks (%d/%d), skipping telemetry upload\n",
                     activeServerTasks, MAX_CONCURRENT_SERVER_TASKS);
        return;
    }

    // Create async task for telemetry upload
    BaseType_t result = xTaskCreate(
        uploadTelemetryTask,     // Task function
        "TelemetryUpload",       // Task name
        8192,                    // Stack size (bytes) - increased for HTTP + time sync
        NULL,                    // Task parameters
        1,                       // Priority (1 = low, higher than idle)
        &telemetryTaskHandle     // Task handle
    );

    if (result != pdPASS) {
        Serial.println("[Main] Failed to create telemetry task");
        telemetryTaskHandle = NULL;
    }
}

/**
 * Upload control data to backend (called immediately when app updates control)
 * Launches async task to prevent blocking main loop
 * Ensures remote users see updates quickly (not waiting for 5-minute cycle)
 *
 * IMPORTANT: JSON payload is built SYNCHRONOUSLY (with mutex) before creating task
 * This prevents race conditions - the JSON captures exact values at callback time
 */
void uploadControlData() {
    // Skip if task is already running
    if (controlUploadTaskHandle != NULL) {
        Serial.println("[Main] Control upload task already running, skipping...");
        return;
    }

    // Check if too many tasks are running (prevent device crash)
    if (activeServerTasks >= MAX_CONCURRENT_SERVER_TASKS) {
        Serial.printf("[Main] Too many active tasks (%d/%d), skipping control upload\n",
                     activeServerTasks, MAX_CONCURRENT_SERVER_TASKS);
        return;
    }

    // Build JSON payload SYNCHRONOUSLY from controlHandler (source of truth)
    // Don't use old controlData - it gets overwritten by periodic fetch tasks!
    String* jsonPayload = new String();

    // Build ControlData struct from controlHandler values
    ControlData tempControl;
    tempControl.pumpSwitch = controlHandler.getPumpSwitch();
    tempControl.pumpSwitchLastModified = controlHandler.getPumpSwitchTimestamp();
    tempControl.config_update = controlHandler.getConfigUpdate();
    tempControl.configUpdateLastModified = controlHandler.getConfigUpdateTimestamp();

    Serial.printf("[Main] Building JSON from controlHandler: pumpSwitch=%d, ts=%llu\n",
                 tempControl.pumpSwitch, tempControl.pumpSwitchLastModified);

    // Build JSON from controlHandler values (no mutex needed - handler is independent)
    *jsonPayload = apiClient.buildControlPayload(tempControl);
    Serial.println("[Main] Built JSON payload for async upload:");
    Serial.println(*jsonPayload);

    // Create async task for control upload with pre-built JSON
    BaseType_t result = xTaskCreate(
        uploadControlTask,       // Task function
        "ControlUpload",         // Task name
        4096,                    // Stack size (bytes)
        jsonPayload,             // Task parameters (pre-built JSON string)
        1,                       // Priority (1 = low, higher than idle)
        &controlUploadTaskHandle // Task handle
    );

    if (result != pdPASS) {
        Serial.println("[Main] Failed to create control upload task");
        delete jsonPayload;  // Free memory if task creation failed
        controlUploadTaskHandle = NULL;
    }
}

/**
 * Fetch control data from backend (every 5 minutes)
 * Launches async task to prevent blocking main loop
 */
void fetchControlData() {
    // Skip if task is already running
    if (controlTaskHandle != NULL) {
        Serial.println("[Main] Control fetch task already running, skipping...");
        return;
    }

    // Check if too many tasks are running (prevent device crash)
    if (activeServerTasks >= MAX_CONCURRENT_SERVER_TASKS) {
        Serial.printf("[Main] Too many active tasks (%d/%d), skipping control fetch\n",
                     activeServerTasks, MAX_CONCURRENT_SERVER_TASKS);
        return;
    }

    // Create async task for control data fetch
    BaseType_t result = xTaskCreate(
        fetchControlDataTask,    // Task function
        "ControlFetch",          // Task name
        8192,                    // Stack size (bytes) - larger for config operations
        NULL,                    // Task parameters
        1,                       // Priority (1 = low, higher than idle)
        &controlTaskHandle       // Task handle
    );

    if (result != pdPASS) {
        Serial.println("[Main] Failed to create control fetch task");
        controlTaskHandle = NULL;
    }
}

/**
 * Check for OTA firmware updates (every 5 minutes)
 */
void checkOTAUpdate() {
    // Don't attempt OTA checks in AP mode (no internet)
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE || !apiClient.isAuthenticated()) {
        Serial.println("[Main] Cannot check OTA - not in client mode or not authenticated");
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
 * Launches async task to prevent blocking main loop
 */
void syncConfigToServer() {
    // Skip if task is already running
    if (configSyncTaskHandle != NULL) {
        Serial.println("[Main] Config sync task already running, skipping...");
        return;
    }

    // Check if too many tasks are running (prevent device crash)
    if (activeServerTasks >= MAX_CONCURRENT_SERVER_TASKS) {
        Serial.printf("[Main] Too many active tasks (%d/%d), skipping config sync\n",
                     activeServerTasks, MAX_CONCURRENT_SERVER_TASKS);
        return;
    }

    // Create async task for config sync
    BaseType_t result = xTaskCreate(
        syncConfigToServerTask,  // Task function
        "ConfigSync",            // Task name
        8192,                    // Stack size (bytes)
        NULL,                    // Task parameters
        1,                       // Priority (1 = low, higher than idle)
        &configSyncTaskHandle    // Task handle
    );

    if (result != pdPASS) {
        Serial.println("[Main] Failed to create config sync task");
        configSyncTaskHandle = NULL;
    }
}

/**
 * Fetch config from server periodically (every 30 seconds)
 * Only fetches when there's NO pending config to upload (bidirectional sync)
 * If hasPendingConfigSync() == true, skip fetch to avoid conflicts
 * Launches async task to prevent blocking main loop
 */
void fetchConfigFromServer() {
    // Skip if task is already running
    if (configFetchTaskHandle != NULL) {
        Serial.println("[Main] Config fetch task already running, skipping...");
        return;
    }

    // Check if too many tasks are running (prevent device crash)
    if (activeServerTasks >= MAX_CONCURRENT_SERVER_TASKS) {
        Serial.printf("[Main] Too many active tasks (%d/%d), skipping config fetch\n",
                     activeServerTasks, MAX_CONCURRENT_SERVER_TASKS);
        return;
    }

    // Create async task for config fetch
    BaseType_t result = xTaskCreate(
        fetchConfigFromServerTask,  // Task function
        "ConfigFetch",              // Task name
        8192,                       // Stack size (bytes)
        NULL,                       // Task parameters
        1,                          // Priority (1 = low, higher than idle)
        &configFetchTaskHandle      // Task handle
    );

    if (result != pdPASS) {
        Serial.println("[Main] Failed to create config fetch task");
        configFetchTaskHandle = NULL;
    }
}

/**
 * Update OLED display (every 0.5 seconds)
 */
void updateDisplay() {
    float waterLevel = levelCalculator.getWaterLevel();
    float waterLevelPercent = levelCalculator.getWaterLevelPercent();
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

    // Create mutex for protecting shared config data
    configMutex = xSemaphoreCreateMutex();
    if (configMutex == NULL) {
        Serial.println("[Main] ERROR: Failed to create config mutex!");
    } else {
        Serial.println("[Main] Config mutex created successfully");
    }

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
    lastMidnightCheck = millis();

    Serial.println("[Main] Entering main loop...\n");
}

// ============================================================================
// 24-HOUR UPTIME REBOOT FOR NTP RESYNC
// ============================================================================

/**
 * Check internet connectivity by pinging Google DNS
 * Returns true if internet is reachable
 */
bool checkInternetConnectivity() {
    if (!isWiFiConnected() || getWiFiMode() != WIFI_CLIENT_MODE) {
        return false;
    }

    // Ping Google DNS (8.8.8.8) to check internet connectivity
    Serial.println("[Main] Checking internet connectivity (ping 8.8.8.8)...");

    bool success = Ping.ping("8.8.8.8", 3);  // 3 attempts

    if (success) {
        Serial.println("[Main] Internet connectivity: OK");
    } else {
        Serial.println("[Main] Internet connectivity: FAILED");
    }

    return success;
}

/**
 * Check if device has been running for 24 hours and schedule reboot
 * Reboot reduces NTP time drift by resyncing after 24 hours
 * Only reboots if internet is available (so NTP sync works after reboot)
 *
 * Uses millis() uptime instead of timestamp hour to avoid reboot loops:
 * - After 24 hours uptime, set needReboot = true (once)
 * - After reboot, millis() resets to 0, won't trigger again for 24 hours
 */
void check24HourReboot() {
    // Only check if we have a synced timestamp
    if (!apiClient.isTimeSynced()) {
        return;
    }

    // Check if device has been running for 24 hours (86400000 milliseconds)
    const unsigned long TWENTY_FOUR_HOURS = 86400000UL;  // 24 * 60 * 60 * 1000

    unsigned long currentUptime = millis();

    // If uptime > 24 hours and we haven't set needReboot yet
    if (currentUptime >= TWENTY_FOUR_HOURS && !needReboot) {
        Serial.println("[Main] ========================================");
        Serial.println("[Main] 24 HOUR UPTIME REACHED - Scheduling reboot");
        Serial.println("[Main] ========================================");
        Serial.printf("[Main] Uptime: %lu ms (%.1f hours)\n", currentUptime, currentUptime / 3600000.0);
        Serial.println("[Main] Reboot will resync NTP and reduce time drift");

        needReboot = true;
        displayManager.showMessage("24h Uptime", "Reboot pending", 3000);
    }

    // If reboot is needed, check internet and reboot
    if (needReboot) {
        Serial.println("[Main] Reboot pending - checking internet connectivity...");

        if (checkInternetConnectivity()) {
            Serial.println("[Main] Internet available - rebooting now...");
            Serial.println("[Main] Device will resync NTP after reboot");
            displayManager.showMessage("Rebooting", "NTP resync", 2000);
            delay(2000);
            ESP.restart();
        } else {
            Serial.println("[Main] No internet - waiting for connectivity before reboot");
            // needReboot stays true, will check again next time
        }
    }
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

            // NOTE: Don't call apiClient.onDeviceOnline() here - it's BLOCKING with retries!
            // The periodic sync tasks will handle syncing within 30 seconds (non-blocking).
            // Just reset the sync timers to trigger sync quickly.
            lastTelemetryUpload = millis() - TELEMETRY_UPLOAD_INTERVAL + 5000;  // Upload in 5s
            lastConfigCheck = millis() - TELEMETRY_UPLOAD_INTERVAL + 5000;      // Fetch config in 5s
            lastControlFetch = millis() - CONTROL_FETCH_INTERVAL + 10000;       // Fetch control in 10s

            Serial.println("[Main] Scheduled async sync tasks to run soon");
        }

        // Device just went offline
        if (!isConnected && wasConnected) {
            Serial.println("[Main] Device transitioned to OFFLINE");
            apiClient.onDeviceOffline();  // This is non-blocking (just sets a flag)
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

    // ============================================================================
    // PERIODIC NTP RETRY WHEN OFFLINE
    // ============================================================================
    // If device is offline but WiFi is connected, periodically retry NTP sync
    if (systemInitialized && !deviceIsOnline && isWiFiConnected() && getWiFiMode() == WIFI_CLIENT_MODE) {
        const unsigned long NTP_RETRY_INTERVAL = 15000;  // 15 seconds

        if (currentTime - lastNTPRetry >= NTP_RETRY_INTERVAL) {
            lastNTPRetry = currentTime;
            Serial.println("[Main] Device is offline - attempting NTP sync...");

            if (syncWithInternet()) {
                Serial.println("[Main] NTP sync successful - device is now ONLINE");
                // Device is now online, will start syncing with server on next iteration
            } else {
                Serial.println("[Main] NTP sync failed - will retry in " + String(NTP_RETRY_INTERVAL / 1000) + " seconds");
            }
        }
    }

    // Only perform backend operations if connected, initialized, and in client mode
    // IMPORTANT: Don't attempt server calls in AP mode (no internet, only for WiFi provisioning)
    if (systemInitialized && isWiFiConnected() && getWiFiMode() == WIFI_CLIENT_MODE) {

        // Upload telemetry (every 30 seconds)
        if (currentTime - lastTelemetryUpload >= TELEMETRY_UPLOAD_INTERVAL) {
            lastTelemetryUpload = currentTime;
            uploadTelemetry();
        }

        // Fetch config from server (every 30 seconds) if needed
        // Only fetch if:
        // 1. initial_config_update is true (after NTP sync on first boot), OR
        // 2. config_update flag is set by server (checked in control data fetch every 5 min)
        // Note: auto_update is for firmware OTA updates, NOT config updates
        if (currentTime - lastConfigCheck >= TELEMETRY_UPLOAD_INTERVAL) {
            lastConfigCheck = currentTime;
            // Check if initial config fetch is needed after NTP sync
            if (apiClient.isTimeSynced() && initial_config_update) {
                Serial.println("[Main] Triggering initial config fetch after NTP sync");
                fetchConfigFromServer();
            }
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

        // Check 24-hour uptime reboot (every minute to avoid spam)
        // Reduces NTP drift by resyncing after 24-hour uptime
        if (currentTime - lastMidnightCheck >= 60000) {  // Check every minute
            lastMidnightCheck = currentTime;
            check24HourReboot();
        }
    }

    // Small delay to prevent watchdog issues
    delay(10);
}
