#include "webserver.h"
#include "wifi_manager.h"
#include "handle_control_data.h"
#include "handle_config_data.h"
#include "handle_telemetry_data.h"

// External references to global handlers (defined in main.cpp)
extern ControlDataHandler controlHandler;
extern ConfigDataHandler configHandler;
extern TelemetryDataHandler telemetryHandler;

// External references to old global data (for backward compatibility)
extern DeviceConfig deviceConfig;
extern ControlData controlData;

// External reference to config mutex (for thread-safe access)
extern SemaphoreHandle_t configMutex;

WebServer::WebServer()
    : server(80),
      currentWaterLevel(0),
      currentInflow(0),
      currentPumpStatus(0),
      apiClient(nullptr),
      pumpCallback(nullptr),
      wifiSaveCallback(nullptr),
      configSyncCallback(nullptr),
      controlSyncCallback(nullptr) {
}

void WebServer::begin(const String& devId, APIClient* apiCli) {
    deviceId = devId;
    apiClient = apiCli;

    setupRoutes();
    server.begin();

    Serial.println("[WebServer] Local web server started on port 80");
    Serial.println("[WebServer] Device ID: " + deviceId);
    Serial.println("[WebServer] Endpoints:");
    Serial.println("  GET  /" + deviceId + "/telemetry      - Get current sensor readings");
    Serial.println("  GET  /" + deviceId + "/control        - Get control data with timestamps");
    Serial.println("  POST /" + deviceId + "/control        - Update control data from app");
    Serial.println("  GET  /" + deviceId + "/config         - Get device configuration");
    Serial.println("  POST /" + deviceId + "/config         - Update device configuration from app");
    Serial.println("  GET  /" + deviceId + "/timestamp      - Get device timestamp and sync status");
    Serial.println("  POST /" + deviceId + "/timestamp      - Sync device time from app (auto-detects seconds/millis)");
    Serial.println("  GET  /" + deviceId + "/status         - Provisioning status");
    Serial.println("  GET  /" + deviceId + "/scanWifi       - Scan WiFi networks");
    Serial.println("  POST /" + deviceId + "/save           - Save WiFi credentials");
}

void WebServer::setupRoutes() {
    // CORS headers for all routes
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // ========================================================================
    // DEVICE ENDPOINTS (Server/App integration)
    // ========================================================================

    // GET /{device_id}/telemetry - Get current sensor readings
    String telemetryEndpoint = "/" + deviceId + "/telemetry";
    server.on(telemetryEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetTelemetry(request);
    });

    // GET /{device_id}/control - Control pump and get config update flag
    String controlEndpoint = "/" + deviceId + "/control";
    server.on(controlEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetControl(request);
    });

    // POST /{device_id}/control - Update control data from app
    server.on(controlEndpoint.c_str(), HTTP_POST,
        [](AsyncWebServerRequest* request) {
            // Response will be sent in body handler
        },
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostControl(request, data, len, index, total);
        }
    );

    // GET /{device_id}/config - Get device configuration
    String configEndpoint = "/" + deviceId + "/config";
    server.on(configEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetDeviceConfig(request);
    });

    // POST /{device_id}/config - Update device configuration from app
    server.on(configEndpoint.c_str(), HTTP_POST,
        [](AsyncWebServerRequest* request) {
            // Response will be sent in body handler
        },
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostDeviceConfig(request, data, len, index, total);
        }
    );

    // GET /{device_id}/timestamp - Get device timestamp and sync status
    String timestampEndpoint = "/" + deviceId + "/timestamp";
    server.on(timestampEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetTimestamp(request);
    });

    // POST /{device_id}/timestamp - Set device timestamp (time correction from app)
    server.on(timestampEndpoint.c_str(), HTTP_POST,
        [](AsyncWebServerRequest* request) {
            // Response will be sent in body handler
        },
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostTimestamp(request, data, len, index, total);
        }
    );

    // ========================================================================
    // PROVISIONING ENDPOINTS (WiFi setup mode)
    // ========================================================================

    // GET /{device_id}/status - Check if device is ready for setup
    String statusEndpoint = "/" + deviceId + "/status";
    server.on(statusEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleProvisioningStatus(request);
    });

    // GET /{device_id}/scanWifi - Scan for available WiFi networks
    String scanEndpoint = "/" + deviceId + "/scanWifi";
    server.on(scanEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleScanWiFi(request);
    });

    // POST /{device_id}/save - Save WiFi and dashboard credentials
    String saveEndpoint = "/" + deviceId + "/save";
    server.on(saveEndpoint.c_str(), HTTP_POST,
        [](AsyncWebServerRequest* request) {
            // Response will be sent in body handler
        },
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handleSaveCredentials(request, data, len, index, total);
        }
    );

    // 404 handler
    server.onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
}

// ========================================================================
// PROVISIONING HANDLERS
// ========================================================================

void WebServer::handleProvisioningStatus(AsyncWebServerRequest* request) {
    // GET /{device_id}/status - Return ready status
    Serial.println("[WebServer] GET /" + deviceId + "/status");

    StaticJsonDocument<1024> doc;
    doc["status"] = "ready";
    doc["deviceId"] = deviceId;

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);
}

void WebServer::handleScanWiFi(AsyncWebServerRequest* request) {
    // GET /{device_id}/scanWifi - Scan and return available networks
    Serial.println("[WebServer] GET /" + deviceId + "/scanWifi - Scanning networks...");

    // Perform WiFi scan using new function-based API (returns JSON array string)
    String networksArray = scanWiFiNetworks();

    // Wrap array in object with "networks" key for backwards compatibility
    String networksJson = "{\"networks\":" + networksArray + "}";

    request->send(200, "application/json", networksJson);

    Serial.println("[WebServer] WiFi scan complete");
}

void WebServer::handleSaveCredentials(AsyncWebServerRequest* request, uint8_t* data,
                                     size_t len, size_t index, size_t total) {
    // POST /{device_id}/save - Save credentials and attempt connection
    Serial.println("[WebServer] POST /" + deviceId + "/save");

    String body = String((char*)data).substring(0, len);

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        Serial.println("[WebServer] Invalid JSON");
        return;
    }

    // Extract credentials
    if (!doc.containsKey("ssid") || !doc.containsKey("password")) {
        request->send(400, "application/json",
                     "{\"success\":false,\"message\":\"Missing ssid or password\"}");
        Serial.println("[WebServer] Missing ssid or password");
        return;
    }

    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();
    String dashUser = doc["dashboardUsername"] | "";
    String dashPass = doc["dashboardPassword"] | "";

    // Validate SSID is not empty
    if (ssid.length() == 0) {
        request->send(400, "application/json",
                     "{\"success\":false,\"message\":\"SSID cannot be empty\"}");
        Serial.println("[WebServer] Empty SSID");
        return;
    }

    Serial.println("[WebServer] Received credentials:");
    Serial.println("  SSID: " + ssid);
    Serial.println("  Dashboard User: " + dashUser);

    // Save credentials to NVS using new function-based API
    saveWiFiCredentials(ssid.c_str(), password.c_str());

    if (dashUser.length() > 0 && dashPass.length() > 0) {
        saveDashboardCredentials(dashUser.c_str(), dashPass.c_str());
    }

    // Call callback if set (for main.cpp to handle connection)
    if (wifiSaveCallback != nullptr) {
        wifiSaveCallback(ssid, password, dashUser, dashPass);
    }

    // Send success response
    StaticJsonDocument<1024> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Connecting to WiFi...";

    String response;
    serializeJson(responseDoc, response);

    request->send(200, "application/json", response);

    Serial.println("[WebServer] Credentials saved, attempting to connect...");
}

// ========================================================================
// NEW DEVICE ENDPOINT HANDLERS
// ========================================================================

void WebServer::handleGetTelemetry(AsyncWebServerRequest* request) {
    // GET /{device_id}/telemetry - Return current sensor readings
    // Format matches server structure: {key, label, type, value}
    Serial.println("[WebServer] GET /" + deviceId + "/telemetry");

    StaticJsonDocument<1024> doc;

    // Water Level
    JsonObject waterLevel = doc.createNestedObject("waterLevel");
    waterLevel["key"] = "waterLevel";
    waterLevel["label"] = "Water Level";
    waterLevel["type"] = "number";
    waterLevel["value"] = currentWaterLevel;

    // Current Inflow
    JsonObject currInflow = doc.createNestedObject("currInflow");
    currInflow["key"] = "currInflow";
    currInflow["label"] = "Current Inflow";
    currInflow["type"] = "number";
    currInflow["value"] = currentInflow;

    // Pump Status (use webserver's current value, not telemetryHandler)
    JsonObject pumpStatus = doc.createNestedObject("pumpStatus");
    pumpStatus["key"] = "pumpStatus";
    pumpStatus["label"] = "Pump Status";
    pumpStatus["type"] = "number";
    pumpStatus["value"] = currentPumpStatus;

    // Device Status (always 1 for online when responding)
    JsonObject status = doc.createNestedObject("Status");
    status["key"] = "Status";
    status["label"] = "Device Status";
    status["type"] = "number";
    status["value"] = 1;  // Always online if webserver is responding

    // Timestamp
    doc["timestamp"] = (unsigned long)telemetryHandler.getTimestamp();

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    DEBUG_RESPONSE_WS_PRINTLN("[WebServer] Telemetry: " + response);
}

void WebServer::handleGetControl(AsyncWebServerRequest* request) {
    // GET /{device_id}/control - Return control data with timestamps
    // Format matches server structure: {field: {key, label, type, value, lastModified}}
    // Uses controlHandler.value (merged self value) for JSON response
    Serial.println("[WebServer] GET /" + deviceId + "/control");

    StaticJsonDocument<1024> doc;

    // Pump Switch - Use handler's merged value (not api_value or local_value)
    JsonObject pumpSwitch = doc.createNestedObject("pumpSwitch");
    pumpSwitch["key"] = "pumpSwitch";
    pumpSwitch["label"] = "Pump Switch";
    pumpSwitch["type"] = "boolean";
    pumpSwitch["value"] = controlHandler.getPumpSwitch();  // Use merged value
    pumpSwitch["lastModified"] = (unsigned long)controlHandler.getPumpSwitchTimestamp();

    // Config Update Flag - Use handler's merged value
    JsonObject config_update = doc.createNestedObject("config_update");
    config_update["key"] = "config_update";
    config_update["label"] = "Configuration Update";
    config_update["type"] = "boolean";
    config_update["value"] = controlHandler.getConfigUpdate();  // Use merged value
    config_update["description"] = "When enabled, device will update its configuration from server";
    config_update["system"] = true;
    config_update["lastModified"] = (unsigned long)controlHandler.getConfigUpdateTimestamp();

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    DEBUG_RESPONSE_WS_PRINTLN("[WebServer] Control status: " + response);
}

void WebServer::handlePostControl(AsyncWebServerRequest* request, uint8_t* data,
                                   size_t len, size_t index, size_t total) {
    // POST /{device_id}/control - Update control data from app
    // Uses 3-way sync handlers: updateFromLocal() → merge() → apply
    Serial.println("[WebServer] POST /" + deviceId + "/control - Control update from app");

    // Accumulate full request body
    static String jsonBuffer;

    if (index == 0) {
        jsonBuffer = "";  // Start of new request
    }

    // Append chunk
    for (size_t i = 0; i < len; i++) {
        jsonBuffer += (char)data[i];
    }

    // Wait for complete body
    if (index + len < total) {
        return;  // More chunks coming
    }

    // Parse incoming control data from app
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonBuffer);

    if (error) {
        Serial.println("[WebServer] JSON parse error: " + String(error.c_str()));
        request->send(400, "application/json", "{\"success\":false,\"error\":\"INVALID_JSON\"}");
        jsonBuffer = "";
        return;
    }

    // Extract values and timestamps from app
    bool hasPumpSwitch = doc.containsKey("pumpSwitch");
    bool hasConfigUpdate = doc.containsKey("config_update");

    bool pumpSwitchValue = false;
    uint64_t pumpSwitchTs = 0;
    bool configUpdateValue = false;
    uint64_t configUpdateTs = 0;

    // Get current timestamp for values without explicit timestamps
    // IMPORTANT: Use current time, NOT 0! (0 would lose to API ts=0 in merge)
    uint64_t currentTime = (apiClient != nullptr) ? apiClient->getCurrentTimestamp() : millis();

    if (hasPumpSwitch) {
        pumpSwitchValue = doc["pumpSwitch"]["value"] | false;
        // If app provides timestamp, use it; otherwise use current time
        // IMPORTANT: Check if lastModified field exists, don't use 0 as default!
        if (doc["pumpSwitch"].containsKey("lastModified")) {
            pumpSwitchTs = doc["pumpSwitch"]["lastModified"];
        } else {
            pumpSwitchTs = currentTime;  // Use current time if not provided
        }
    }

    if (hasConfigUpdate) {
        configUpdateValue = doc["config_update"]["value"] | false;
        // If app provides timestamp, use it; otherwise use current time
        // IMPORTANT: Check if lastModified field exists, don't use 0 as default!
        if (doc["config_update"].containsKey("lastModified")) {
            configUpdateTs = doc["config_update"]["lastModified"];
        } else {
            configUpdateTs = currentTime;  // Use current time if not provided
        }
    }

    // Update handler with values from Local (app webserver)
    if (hasPumpSwitch || hasConfigUpdate) {
        // Get current values to preserve fields not in request
        bool currentPump = hasPumpSwitch ? pumpSwitchValue : controlHandler.getPumpSwitch();
        bool currentConfig = hasConfigUpdate ? configUpdateValue : controlHandler.getConfigUpdate();
        uint64_t currentPumpTs = hasPumpSwitch ? pumpSwitchTs : controlHandler.getPumpSwitchTimestamp();
        uint64_t currentConfigTs = hasConfigUpdate ? configUpdateTs : controlHandler.getConfigUpdateTimestamp();

        controlHandler.updateFromLocal(currentPump, currentPumpTs, currentConfig, currentConfigTs);
    }

    // Perform 3-way merge (API vs Local vs Self)
    bool changed = controlHandler.merge();

    // Trigger pump callback with merged value
    if (hasPumpSwitch && pumpCallback != nullptr) {
        bool mergedPumpValue = controlHandler.getPumpSwitch();
        Serial.printf("[WebServer] Applying pump control: %s\n", mergedPumpValue ? "ON" : "OFF");
        pumpCallback(mergedPumpValue);
    }

    // Update old controlData for backward compatibility
    // IMPORTANT: Take mutex to prevent race with async tasks reading controlData
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        controlData.pumpSwitch = controlHandler.getPumpSwitch();
        controlData.pumpSwitchLastModified = controlHandler.getPumpSwitchTimestamp();
        controlData.config_update = controlHandler.getConfigUpdate();
        controlData.configUpdateLastModified = controlHandler.getConfigUpdateTimestamp();

        Serial.println("[WebServer] Updated controlData with mutex:");
        Serial.printf("  controlData.pumpSwitch = %d (from handler: %d)\n",
                     controlData.pumpSwitch, controlHandler.getPumpSwitch());
        Serial.printf("  controlData.pumpSwitchLastModified = %llu\n", controlData.pumpSwitchLastModified);

        xSemaphoreGive(configMutex);
    } else {
        Serial.println("[WebServer] ERROR: Failed to take mutex for controlData update!");
    }

    Serial.println("[WebServer] Control updated from app (Local):");
    Serial.println("  Pump Switch: " + String(controlHandler.getPumpSwitch()));
    Serial.println("  Config Update: " + String(controlHandler.getConfigUpdate()));
    if (changed) {
        Serial.println("  Merge result: Values changed after 3-way merge");
    }

    // Trigger async control sync callback if provided and values changed
    // This uploads control data to server immediately (non-blocking) so remote users see updates
    if (controlSyncCallback != nullptr && changed) {
        Serial.println("[WebServer] Triggering async control sync callback...");
        controlSyncCallback();
    }

    // Send success response with merged values
    StaticJsonDocument<512> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Control updated and synced";

    JsonObject pumpSwitchResp = responseDoc.createNestedObject("pumpSwitch");
    pumpSwitchResp["value"] = controlHandler.getPumpSwitch();
    pumpSwitchResp["lastModified"] = (unsigned long)controlHandler.getPumpSwitchTimestamp();

    JsonObject configUpdateResp = responseDoc.createNestedObject("config_update");
    configUpdateResp["value"] = controlHandler.getConfigUpdate();
    configUpdateResp["lastModified"] = (unsigned long)controlHandler.getConfigUpdateTimestamp();

    String response;
    serializeJson(responseDoc, response);
    request->send(200, "application/json", response);

    jsonBuffer = "";
}

void WebServer::handleGetDeviceConfig(AsyncWebServerRequest* request) {
    // GET /{device_id}/config - Return device configuration with per-field timestamps
    // Format matches server structure: {field: {key, label, type, value, lastModified}}
    Serial.println("[WebServer] GET /" + deviceId + "/config");

    StaticJsonDocument<2048> doc;

    // Upper Threshold
    JsonObject upperThreshold = doc.createNestedObject("upperThreshold");
    upperThreshold["key"] = "upperThreshold";
    upperThreshold["label"] = "Upper Threshold";
    upperThreshold["type"] = "number";
    upperThreshold["lastModified"] = (unsigned long)configHandler.getUpperThresholdTimestamp();
    upperThreshold["value"] = configHandler.getUpperThreshold();

    // Lower Threshold
    JsonObject lowerThreshold = doc.createNestedObject("lowerThreshold");
    lowerThreshold["key"] = "lowerThreshold";
    lowerThreshold["label"] = "Lower Threshold";
    lowerThreshold["type"] = "number";
    lowerThreshold["lastModified"] = (unsigned long)configHandler.getLowerThresholdTimestamp();
    lowerThreshold["value"] = configHandler.getLowerThreshold();

    // Tank Height
    JsonObject tankHeight = doc.createNestedObject("tankHeight");
    tankHeight["key"] = "tankHeight";
    tankHeight["label"] = "Tank Height";
    tankHeight["type"] = "number";
    tankHeight["lastModified"] = (unsigned long)configHandler.getTankHeightTimestamp();
    tankHeight["value"] = configHandler.getTankHeight();

    // Tank Width
    JsonObject tankWidth = doc.createNestedObject("tankWidth");
    tankWidth["key"] = "tankWidth";
    tankWidth["label"] = "Tank Width";
    tankWidth["type"] = "number";
    tankWidth["lastModified"] = (unsigned long)configHandler.getTankWidthTimestamp();
    tankWidth["value"] = configHandler.getTankWidth();

    // Tank Shape
    JsonObject tankShape = doc.createNestedObject("tankShape");
    tankShape["key"] = "tankShape";
    tankShape["label"] = "Tank Shape";
    tankShape["type"] = "dropdown";
    JsonArray tankShapeOptions = tankShape.createNestedArray("options");
    tankShapeOptions.add("Cylindrical");
    tankShapeOptions.add("Rectangular");
    tankShape["lastModified"] = (unsigned long)configHandler.getTankShapeTimestamp();
    tankShape["value"] = configHandler.getTankShape();

    // Used Total
    JsonObject usedTotal = doc.createNestedObject("UsedTotal");
    usedTotal["key"] = "UsedTotal";
    usedTotal["label"] = "Total Water Used";
    usedTotal["type"] = "number";
    usedTotal["lastModified"] = (unsigned long)configHandler.getUsedTotalTimestamp();
    usedTotal["value"] = configHandler.getUsedTotal();

    // Max Inflow
    JsonObject maxInflow = doc.createNestedObject("maxInflow");
    maxInflow["key"] = "maxInflow";
    maxInflow["label"] = "Max Inflow";
    maxInflow["type"] = "number";
    maxInflow["lastModified"] = (unsigned long)configHandler.getMaxInflowTimestamp();
    maxInflow["value"] = configHandler.getMaxInflow();

    // Force Update
    JsonObject force_update = doc.createNestedObject("force_update");
    force_update["key"] = "force_update";
    force_update["label"] = "Force Firmware Update";
    force_update["type"] = "boolean";
    force_update["value"] = configHandler.getForceUpdate();
    force_update["description"] = "When enabled, device will force download and install firmware update";
    force_update["system"] = true;
    force_update["hidden"] = false;
    force_update["lastModified"] = (unsigned long)configHandler.getForceUpdateTimestamp();

    // Sensor Filter

    // IP Address
    JsonObject ipAddress = doc.createNestedObject("ip_address");
    ipAddress["key"] = "ip_address";
    ipAddress["label"] = "Device Local IP Address";
    ipAddress["type"] = "string";
    ipAddress["value"] = configHandler.getIpAddress();
    ipAddress["description"] = "Local IP address of the device for offline app communication via webserver";
    ipAddress["system"] = true;
    ipAddress["lastModified"] = (unsigned long)configHandler.getIpAddressTimestamp();

    // Auto Update
    JsonObject autoUpdate = doc.createNestedObject("auto_update");
    autoUpdate["key"] = "auto_update";
    autoUpdate["label"] = "Auto Update Configuration";
    autoUpdate["type"] = "boolean";
    autoUpdate["value"] = configHandler.getAutoUpdate();
    autoUpdate["description"] = "When enabled, device will automatically fetch and apply configuration updates from server";
    autoUpdate["system"] = true;
    autoUpdate["hidden"] = false;
    autoUpdate["lastModified"] = (unsigned long)configHandler.getAutoUpdateTimestamp();

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    Serial.println("[WebServer] Device config with full metadata sent");
}

void WebServer::handlePostDeviceConfig(AsyncWebServerRequest* request, uint8_t* data,
                                       size_t len, size_t index, size_t total) {
    // POST /{device_id}/config - Update device configuration from app
    // Uses 3-way merge (API vs Local vs Self)

    Serial.println("[WebServer] POST /" + deviceId + "/config - Config update from app");

    // Accumulate full request body
    static String jsonBuffer;

    if (index == 0) {
        jsonBuffer = "";  // Start of new request
    }

    // Append chunk
    for (size_t i = 0; i < len; i++) {
        jsonBuffer += (char)data[i];
    }

    // Wait for complete body
    if (index + len < total) {
        return;  // More chunks coming
    }

    // Parse incoming config from app
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, jsonBuffer);

    if (error) {
        Serial.println("[WebServer] JSON parse error: " + String(error.c_str()));
        request->send(400, "application/json", "{\"success\":false,\"error\":\"INVALID_JSON\"}");
        jsonBuffer = "";
        return;
    }

    DEBUG_RESPONSE_WS_PRINTLN("[WebServer] Received config JSON from app:");
    DEBUG_RESPONSE_WS_PRINTLN(jsonBuffer);

    // Get current timestamp for values without explicit timestamps
    // IMPORTANT: Use current time, NOT 0! (0 would lose to API ts=0 in merge)
    uint64_t currentTime = (apiClient != nullptr) ? apiClient->getCurrentTimestamp() : millis();

    // Extract all 10 config fields with timestamps
    float localUpperThreshold = doc["upperThreshold"]["value"] | configHandler.getUpperThreshold();
    uint64_t localUpperThresholdTs = doc["upperThreshold"]["lastModified"] | currentTime;

    float localLowerThreshold = doc["lowerThreshold"]["value"] | configHandler.getLowerThreshold();
    uint64_t localLowerThresholdTs = doc["lowerThreshold"]["lastModified"] | currentTime;

    float localTankHeight = doc["tankHeight"]["value"] | configHandler.getTankHeight();
    uint64_t localTankHeightTs = doc["tankHeight"]["lastModified"] | currentTime;

    float localTankWidth = doc["tankWidth"]["value"] | configHandler.getTankWidth();
    uint64_t localTankWidthTs = doc["tankWidth"]["lastModified"] | currentTime;

    String localTankShape = doc["tankShape"]["value"] | configHandler.getTankShape();
    uint64_t localTankShapeTs = doc["tankShape"]["lastModified"] | currentTime;

    float localUsedTotal = doc["UsedTotal"]["value"] | configHandler.getUsedTotal();
    uint64_t localUsedTotalTs = doc["UsedTotal"]["lastModified"] | currentTime;

    float localMaxInflow = doc["maxInflow"]["value"] | configHandler.getMaxInflow();
    uint64_t localMaxInflowTs = doc["maxInflow"]["lastModified"] | currentTime;

    bool localForceUpdate = doc["force_update"]["value"] | configHandler.getForceUpdate();
    uint64_t localForceUpdateTs = doc["force_update"]["lastModified"] | currentTime;

    String localIpAddress = doc["ip_address"]["value"] | configHandler.getIpAddress();
    uint64_t localIpAddressTs = doc["ip_address"]["lastModified"] | currentTime;

    bool localAutoUpdate = doc["auto_update"]["value"] | configHandler.getAutoUpdate();
    uint64_t localAutoUpdateTs = doc["auto_update"]["lastModified"] | currentTime;

    // Update handler with values from Local (app webserver)
    configHandler.updateFromLocal(
        localUpperThreshold, localUpperThresholdTs,
        localLowerThreshold, localLowerThresholdTs,
        localTankHeight, localTankHeightTs,
        localTankWidth, localTankWidthTs,
        localTankShape, localTankShapeTs,
        localUsedTotal, localUsedTotalTs,
        localMaxInflow, localMaxInflowTs,
        localForceUpdate, localForceUpdateTs,
        localIpAddress, localIpAddressTs,
        localAutoUpdate, localAutoUpdateTs
    );

    // Perform 3-way merge (API vs Local vs Self)
    bool changed = configHandler.merge();

    // Update old deviceConfig for backward compatibility
    // IMPORTANT: Take mutex to prevent race with async tasks reading deviceConfig
    if (configMutex != NULL && xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        deviceConfig.upperThreshold = configHandler.getUpperThreshold();
        deviceConfig.upperThresholdLastModified = configHandler.getUpperThresholdTimestamp();
        deviceConfig.lowerThreshold = configHandler.getLowerThreshold();
        deviceConfig.lowerThresholdLastModified = configHandler.getLowerThresholdTimestamp();
        deviceConfig.tankHeight = configHandler.getTankHeight();
        deviceConfig.tankHeightLastModified = configHandler.getTankHeightTimestamp();
        deviceConfig.tankWidth = configHandler.getTankWidth();
        deviceConfig.tankWidthLastModified = configHandler.getTankWidthTimestamp();
        deviceConfig.tankShape = configHandler.getTankShape();
        deviceConfig.tankShapeLastModified = configHandler.getTankShapeTimestamp();
        deviceConfig.usedTotal = configHandler.getUsedTotal();
        deviceConfig.usedTotalLastModified = configHandler.getUsedTotalTimestamp();
        deviceConfig.maxInflow = configHandler.getMaxInflow();
        deviceConfig.maxInflowLastModified = configHandler.getMaxInflowTimestamp();
        deviceConfig.force_update = configHandler.getForceUpdate();
        deviceConfig.forceUpdateLastModified = configHandler.getForceUpdateTimestamp();
        deviceConfig.ipAddress = configHandler.getIpAddress();
        deviceConfig.ipAddressLastModified = configHandler.getIpAddressTimestamp();
        deviceConfig.auto_update = configHandler.getAutoUpdate();
        deviceConfig.autoUpdateLastModified = configHandler.getAutoUpdateTimestamp();
        xSemaphoreGive(configMutex);
    }

    Serial.println("[WebServer] Config updated from app (Local):");
    Serial.println("  Upper Threshold: " + String(configHandler.getUpperThreshold()));
    Serial.println("  Lower Threshold: " + String(configHandler.getLowerThreshold()));
    Serial.println("  Tank Height: " + String(configHandler.getTankHeight()));
    Serial.println("  Tank Width: " + String(configHandler.getTankWidth()));
    if (changed) {
        Serial.println("  Merge result: Values changed after 3-way merge");
    }

    // Trigger config sync callback if provided
    if (configSyncCallback != nullptr && changed) {
        Serial.println("[WebServer] Triggering config sync callback...");
        configSyncCallback();
    }

    // Send success response with merged values
    StaticJsonDocument<512> responseDoc;
    responseDoc["success"] = true;
    responseDoc["message"] = "Config updated and synced";

    String response;
    serializeJson(responseDoc, response);
    request->send(200, "application/json", response);

    jsonBuffer = "";
}

void WebServer::handleGetTimestamp(AsyncWebServerRequest* request) {
    // GET /{device_id}/timestamp - Return current timestamp and sync status
    Serial.println("[WebServer] GET /" + deviceId + "/timestamp");

    StaticJsonDocument<512> doc;

    if (apiClient != nullptr) {
        uint64_t currentTimestamp = apiClient->getCurrentTimestamp();
        uint64_t currentMillis = millis();
        bool synced = apiClient->isTimeSynced();
        SyncStatus syncStatus = apiClient->getSyncStatus();

        // Calculate drift (estimated vs actual time passage)
        uint64_t estimatedDrift = 0;
        if (synced && syncStatus.millisAtSync > 0) {
            // Drift = how much device time has drifted since last sync
            estimatedDrift = currentMillis - syncStatus.millisAtSync;
        }

        doc["timestamp"] = currentTimestamp;              // Current Unix time (millis() if not synced)
        doc["millis"] = currentMillis;                    // Device uptime
        doc["synced"] = synced;                          // Has valid time sync
        doc["lastSync"] = syncStatus.lastServerTimestamp; // When last synced
        doc["drift"] = estimatedDrift;                   // Estimated drift in ms
    } else {
        // API client not available - return minimal info
        doc["timestamp"] = 0;
        doc["millis"] = millis();
        doc["synced"] = false;
        doc["lastSync"] = 0;
        doc["drift"] = 0;
    }

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    DEBUG_RESPONSE_WS_PRINTLN("[WebServer] Timestamp info: " + response);
}

void WebServer::handlePostTimestamp(AsyncWebServerRequest* request, uint8_t* data,
                                    size_t len, size_t index, size_t total) {
    // POST /{device_id}/timestamp - Set device timestamp (time correction from app)
    // Used when app detects significant drift and sends corrected Unix timestamp
    Serial.println("[WebServer] POST /" + deviceId + "/timestamp - Time correction from app");

    // Accumulate full request body
    static String jsonBuffer;

    if (index == 0) {
        jsonBuffer = "";  // Start of new request
    }

    // Append chunk
    for (size_t i = 0; i < len; i++) {
        jsonBuffer += (char)data[i];
    }

    // Wait for complete body
    if (index + len < total) {
        return;  // More chunks coming
    }

    // Parse incoming timestamp
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, jsonBuffer);

    if (error) {
        Serial.println("[WebServer] JSON parse error: " + String(error.c_str()));
        request->send(400, "application/json", "{\"success\":false,\"error\":\"INVALID_JSON\"}");
        jsonBuffer = "";
        return;
    }

    // Extract timestamp from request
    if (!doc.containsKey("timestamp")) {
        Serial.println("[WebServer] Missing timestamp field");
        request->send(400, "application/json", "{\"success\":false,\"error\":\"MISSING_TIMESTAMP\"}");
        jsonBuffer = "";
        return;
    }

    uint64_t newTimestamp = doc["timestamp"];

    if (newTimestamp == 0) {
        Serial.println("[WebServer] Invalid timestamp (zero)");
        request->send(400, "application/json", "{\"success\":false,\"error\":\"INVALID_TIMESTAMP\"}");
        jsonBuffer = "";
        return;
    }

    // Auto-detect if timestamp is in seconds or milliseconds
    // Unix time in seconds: ~10 digits (e.g., 1763445130 for year 2025)
    // Unix time in milliseconds: ~13 digits (e.g., 1763445130002 for year 2025)
    // Threshold: 10000000000 (10 billion) = Sep 2286 in seconds
    if (newTimestamp < 10000000000ULL) {
        // Timestamp is in seconds - convert to milliseconds
        Serial.println("[WebServer] Detected timestamp in seconds: " + String((unsigned long)newTimestamp));
        newTimestamp = newTimestamp * 1000;
        Serial.println("[WebServer] Converted to milliseconds: " + String((unsigned long)newTimestamp));
    } else {
        Serial.println("[WebServer] Detected timestamp in milliseconds: " + String((unsigned long)newTimestamp));
    }

    // Set the timestamp via API client
    if (apiClient != nullptr) {
        apiClient->setTimestamp(newTimestamp);

        Serial.println("[WebServer] Time corrected to: " + String(newTimestamp));

        // Send success response
        StaticJsonDocument<256> responseDoc;
        responseDoc["success"] = true;
        responseDoc["timestamp"] = newTimestamp;
        responseDoc["message"] = "Time synchronized successfully";

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
    } else {
        Serial.println("[WebServer] API client not available");
        request->send(500, "application/json", "{\"success\":false,\"error\":\"API_CLIENT_UNAVAILABLE\"}");
    }

    jsonBuffer = "";
}

void WebServer::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
}

// ========================================================================
// PUBLIC METHODS
// ========================================================================

void WebServer::updateSensorData(float waterLevel, float currInflow, int pumpStatus) {
    currentWaterLevel = waterLevel;
    currentInflow = currInflow;
    currentPumpStatus = pumpStatus;
}

void WebServer::updateDeviceConfig(const DeviceConfig& config) {
    deviceConfig = config;
}

void WebServer::updateControlData(const ControlData& control) {
    controlData = control;
}

void WebServer::setPumpControlCallback(PumpControlCallback callback) {
    pumpCallback = callback;
}

void WebServer::setWiFiSaveCallback(WiFiSaveCallback callback) {
    wifiSaveCallback = callback;
}

void WebServer::setConfigSyncCallback(ConfigSyncCallback callback) {
    configSyncCallback = callback;
}

void WebServer::setControlSyncCallback(ControlSyncCallback callback) {
    controlSyncCallback = callback;
}

void WebServer::handle() {
    // ESPAsyncWebServer handles requests asynchronously
    // No need to call handle() in loop
}
