#include "webserver.h"
#include "wifi_manager.h"

WebServer::WebServer()
    : server(80),
      currentWaterLevel(0),
      currentInflow(0),
      currentPumpStatus(0),
      apiClient(nullptr),
      pumpCallback(nullptr),
      wifiSaveCallback(nullptr),
      configSyncCallback(nullptr) {
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

    // Pump Status
    JsonObject pumpStatus = doc.createNestedObject("pumpStatus");
    pumpStatus["key"] = "pumpStatus";
    pumpStatus["label"] = "Pump Status";
    pumpStatus["type"] = "number";
    pumpStatus["value"] = currentPumpStatus;

    // Device Status (always 1 for online, updated by backend based on heartbeat)
    JsonObject status = doc.createNestedObject("Status");
    status["key"] = "Status";
    status["label"] = "Device Status";
    status["type"] = "number";
    status["value"] = 1;

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    DEBUG_RESPONSE_PRINTLN("[WebServer] Telemetry: " + response);
}

void WebServer::handleGetControl(AsyncWebServerRequest* request) {
    // GET /{device_id}/control - Return control data with timestamps
    // Format matches server structure: {field: {key, label, type, value, lastModified}}
    Serial.println("[WebServer] GET /" + deviceId + "/control");

    StaticJsonDocument<1024> doc;

    // Pump Switch (command from server)
    JsonObject pumpSwitch = doc.createNestedObject("pumpSwitch");
    pumpSwitch["key"] = "pumpSwitch";
    pumpSwitch["label"] = "Pump Switch";
    pumpSwitch["type"] = "boolean";
    pumpSwitch["value"] = controlData.pumpSwitch;
    pumpSwitch["lastModified"] = controlData.pumpSwitchLastModified;

    // Config Update Flag (system control)
    JsonObject config_update = doc.createNestedObject("config_update");
    config_update["key"] = "config_update";
    config_update["label"] = "Configuration Update";
    config_update["type"] = "boolean";
    config_update["value"] = controlData.config_update;
    config_update["description"] = "When enabled, device will update its configuration from server";
    config_update["system"] = true;
    config_update["lastModified"] = controlData.configUpdateLastModified;

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    DEBUG_RESPONSE_PRINTLN("[WebServer] Control status: " + response);
}

void WebServer::handlePostControl(AsyncWebServerRequest* request, uint8_t* data,
                                   size_t len, size_t index, size_t total) {
    // POST /{device_id}/control - Update control data from app
    // Implements the same 3-rule logic as config sync:
    // RULE 1: lastModified == 0 → Always update (priority flag, if value differs)
    // RULE 2: Value unchanged → Skip update (even if timestamp differs)
    // RULE 3: Value changed → Compare timestamps (Last-Write-Wins)

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

    // Extract incoming control values and timestamps
    ControlData incomingControl = controlData;  // Start with current values
    bool hasPriorityFlag = false;
    bool hasPumpSwitchInRequest = false;  // Track if pumpSwitch field was sent
    bool hasConfigUpdateInRequest = false;  // Track if config_update field was sent
    bool pumpValueChanged = false;
    bool configValueChanged = false;

    // Parse pumpSwitch field
    if (doc.containsKey("pumpSwitch")) {
        hasPumpSwitchInRequest = true;  // Field was sent in request
        bool newPumpSwitch = doc["pumpSwitch"]["value"] | controlData.pumpSwitch;
        uint64_t pumpTs = doc["pumpSwitch"]["lastModified"] | (uint64_t)0;

        if (pumpTs == 0) hasPriorityFlag = true;

        incomingControl.pumpSwitch = newPumpSwitch;
        incomingControl.pumpSwitchLastModified = pumpTs;

        if (newPumpSwitch != controlData.pumpSwitch) {
            pumpValueChanged = true;
        }
    }

    // Parse config_update field
    if (doc.containsKey("config_update")) {
        hasConfigUpdateInRequest = true;  // Field was sent in request
        bool newConfigUpdate = doc["config_update"]["value"] | controlData.config_update;
        uint64_t configTs = doc["config_update"]["lastModified"] | (uint64_t)0;

        if (configTs == 0) hasPriorityFlag = true;

        incomingControl.config_update = newConfigUpdate;
        incomingControl.configUpdateLastModified = configTs;

        if (newConfigUpdate != controlData.config_update) {
            configValueChanged = true;
        }
    }

    // ✅ SERVER RULE 2: Value unchanged → Skip update (but still trigger pump callback!)
    if (!pumpValueChanged && !configValueChanged) {
        Serial.println("[WebServer] Control values identical - updating timestamps only");

        // Even if values are identical, trigger pump callback if app sent pump command
        // This ensures pump is physically controlled even if value didn't change
        if (hasPumpSwitchInRequest && pumpCallback != nullptr) {
            Serial.println("[WebServer] Triggering pump callback (value unchanged, app command received)");
            pumpCallback(incomingControl.pumpSwitch);
        }

        StaticJsonDocument<512> responseDoc;
        responseDoc["success"] = true;
        responseDoc["message"] = "Values identical - callback triggered";

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
        jsonBuffer = "";
        return;
    }

    // ✅ SERVER RULE 1: lastModified == 0 → Always update (priority flag)
    if (hasPriorityFlag) {
        Serial.println("[WebServer] Priority flag detected - accepting app control");

        controlData = incomingControl;

        // Assign new timestamp
        if (apiClient != nullptr) {
            if (hasPumpSwitchInRequest) {
                controlData.pumpSwitchLastModified = apiClient->getCurrentTimestamp();
            }
            if (hasConfigUpdateInRequest) {
                controlData.configUpdateLastModified = apiClient->getCurrentTimestamp();
            }
        } else {
            if (hasPumpSwitchInRequest) {
                controlData.pumpSwitchLastModified = millis();
            }
            if (hasConfigUpdateInRequest) {
                controlData.configUpdateLastModified = millis();
            }
        }

        Serial.println("[WebServer] Control updated from app (priority)");
        Serial.println("  Pump Switch: " + String(controlData.pumpSwitch));
        Serial.println("  Config Update: " + String(controlData.config_update));

        // ALWAYS trigger pump callback when app sends pump command (even if value unchanged)
        // This ensures pump is physically controlled even if value didn't change
        if (hasPumpSwitchInRequest && pumpCallback != nullptr) {
            Serial.println("[WebServer] Triggering pump callback (value " +
                         String(pumpValueChanged ? "changed" : "unchanged") + ")");
            pumpCallback(controlData.pumpSwitch);
        }

        // Immediately sync control data to server (if authenticated)
        if (apiClient != nullptr && apiClient->isAuthenticated()) {
            Serial.println("[WebServer] Uploading control data to server...");
            if (apiClient->uploadControl(controlData)) {
                Serial.println("[WebServer] Control data synced to server successfully");
            } else {
                Serial.println("[WebServer] WARNING: Failed to sync control data to server");
            }
        }

        // Send success response immediately
        StaticJsonDocument<512> responseDoc;
        responseDoc["success"] = true;
        responseDoc["message"] = "Control updated and synced to server";

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
        jsonBuffer = "";
        return;
    }

    // ✅ SERVER RULE 3: Value changed → Last-Write-Wins (per field)
    bool acceptedUpdate = false;
    bool pumpUpdateAccepted = false;

    // Check pumpSwitch timestamp
    if (hasPumpSwitchInRequest) {
        if (incomingControl.pumpSwitchLastModified > controlData.pumpSwitchLastModified) {
            Serial.println("[WebServer] Pump switch timestamp newer - accepting update");

            controlData.pumpSwitch = incomingControl.pumpSwitch;
            controlData.pumpSwitchLastModified = incomingControl.pumpSwitchLastModified;
            acceptedUpdate = true;
            pumpUpdateAccepted = true;
        } else {
            Serial.println("[WebServer] Pump switch timestamp older - rejecting");
        }
    }

    // Check config_update timestamp
    if (hasConfigUpdateInRequest) {
        if (incomingControl.configUpdateLastModified > controlData.configUpdateLastModified) {
            Serial.println("[WebServer] Config update timestamp newer - accepting update");

            controlData.config_update = incomingControl.config_update;
            controlData.configUpdateLastModified = incomingControl.configUpdateLastModified;
            acceptedUpdate = true;
        } else {
            Serial.println("[WebServer] Config update timestamp older - rejecting");
        }
    }

    if (acceptedUpdate) {
        Serial.println("[WebServer] Control updated from app (LWW)");
        Serial.println("  Pump Switch: " + String(controlData.pumpSwitch));
        Serial.println("  Config Update: " + String(controlData.config_update));

        // ALWAYS trigger pump callback when pump update is accepted (even if value unchanged)
        // This ensures the pump is physically controlled even if a previous update failed
        if (pumpUpdateAccepted && pumpCallback != nullptr) {
            Serial.println("[WebServer] Triggering pump control callback (value " +
                         String(pumpValueChanged ? "changed" : "unchanged") + ")");
            pumpCallback(controlData.pumpSwitch);
        }

        // Immediately sync control data to server (if authenticated)
        if (apiClient != nullptr && apiClient->isAuthenticated()) {
            Serial.println("[WebServer] Uploading control data to server...");
            if (apiClient->uploadControl(controlData)) {
                Serial.println("[WebServer] Control data synced to server successfully");
            } else {
                Serial.println("[WebServer] WARNING: Failed to sync control data to server");
            }
        }

        // Send success response
        StaticJsonDocument<512> responseDoc;
        responseDoc["success"] = true;
        responseDoc["message"] = "Control updated and synced to server";

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
    } else {
        Serial.println("[WebServer] All timestamps older - rejecting update");

        // Send rejection response
        StaticJsonDocument<512> responseDoc;
        responseDoc["success"] = false;
        responseDoc["error"] = "STALE_TIMESTAMP";
        responseDoc["message"] = "Current control data is newer";

        String response;
        serializeJson(responseDoc, response);
        request->send(409, "application/json", response);  // 409 Conflict
    }

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
    upperThreshold["lastModified"] = deviceConfig.upperThresholdLastModified;
    upperThreshold["value"] = deviceConfig.upperThreshold;

    // Lower Threshold
    JsonObject lowerThreshold = doc.createNestedObject("lowerThreshold");
    lowerThreshold["key"] = "lowerThreshold";
    lowerThreshold["label"] = "Lower Threshold";
    lowerThreshold["type"] = "number";
    lowerThreshold["lastModified"] = deviceConfig.lowerThresholdLastModified;
    lowerThreshold["value"] = deviceConfig.lowerThreshold;

    // Tank Height
    JsonObject tankHeight = doc.createNestedObject("tankHeight");
    tankHeight["key"] = "tankHeight";
    tankHeight["label"] = "Tank Height";
    tankHeight["type"] = "number";
    tankHeight["lastModified"] = deviceConfig.tankHeightLastModified;
    tankHeight["value"] = deviceConfig.tankHeight;

    // Tank Width
    JsonObject tankWidth = doc.createNestedObject("tankWidth");
    tankWidth["key"] = "tankWidth";
    tankWidth["label"] = "Tank Width";
    tankWidth["type"] = "number";
    tankWidth["lastModified"] = deviceConfig.tankWidthLastModified;
    tankWidth["value"] = deviceConfig.tankWidth;

    // Tank Shape
    JsonObject tankShape = doc.createNestedObject("tankShape");
    tankShape["key"] = "tankShape";
    tankShape["label"] = "Tank Shape";
    tankShape["type"] = "dropdown";
    JsonArray tankShapeOptions = tankShape.createNestedArray("options");
    tankShapeOptions.add("Cylindrical");
    tankShapeOptions.add("Rectangular");
    tankShape["lastModified"] = deviceConfig.tankShapeLastModified;
    tankShape["value"] = deviceConfig.tankShape;

    // Used Total
    JsonObject usedTotal = doc.createNestedObject("UsedTotal");
    usedTotal["key"] = "UsedTotal";
    usedTotal["label"] = "Total Water Used";
    usedTotal["type"] = "number";
    usedTotal["lastModified"] = deviceConfig.usedTotalLastModified;
    usedTotal["value"] = deviceConfig.usedTotal;

    // Max Inflow
    JsonObject maxInflow = doc.createNestedObject("maxInflow");
    maxInflow["key"] = "maxInflow";
    maxInflow["label"] = "Max Inflow";
    maxInflow["type"] = "number";
    maxInflow["lastModified"] = deviceConfig.maxInflowLastModified;
    maxInflow["value"] = deviceConfig.maxInflow;

    // Force Update
    JsonObject force_update = doc.createNestedObject("force_update");
    force_update["key"] = "force_update";
    force_update["label"] = "Force Firmware Update";
    force_update["type"] = "boolean";
    force_update["value"] = deviceConfig.force_update;
    force_update["description"] = "When enabled, device will force download and install firmware update";
    force_update["system"] = true;
    force_update["hidden"] = false;
    force_update["lastModified"] = deviceConfig.forceUpdateLastModified;

    // Sensor Filter
    JsonObject sensorFilter = doc.createNestedObject("sensorFilter");
    sensorFilter["key"] = "sensorFilter";
    sensorFilter["label"] = "Sensor Filter";
    sensorFilter["type"] = "boolean";
    sensorFilter["value"] = deviceConfig.sensorFilter;
    sensorFilter["description"] = "Enable/disable sensor filtering and smoothing for more stable readings";
    sensorFilter["lastModified"] = deviceConfig.sensorFilterLastModified;

    // IP Address
    JsonObject ipAddress = doc.createNestedObject("ip_address");
    ipAddress["key"] = "ip_address";
    ipAddress["label"] = "Device Local IP Address";
    ipAddress["type"] = "string";
    ipAddress["value"] = deviceConfig.ipAddress;
    ipAddress["description"] = "Local IP address of the device for offline app communication via webserver";
    ipAddress["system"] = true;
    ipAddress["lastModified"] = deviceConfig.ipAddressLastModified;

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);

    Serial.println("[WebServer] Device config with full metadata sent");
}

void WebServer::handlePostDeviceConfig(AsyncWebServerRequest* request, uint8_t* data,
                                       size_t len, size_t index, size_t total) {
    // POST /{device_id}/config - Update device configuration from app
    // Implements the same 3-rule logic as server sync:
    // RULE 1: lastModified == 0 → Always update (priority flag, if value differs)
    // RULE 2: Value unchanged → Skip update (even if timestamp differs)
    // RULE 3: Value changed → Compare timestamps (Last-Write-Wins)

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

    // Extract incoming config values with per-field timestamps
    DeviceConfig incomingConfig = deviceConfig;  // Start with current config
    bool hasPriorityFlag = false;

    // Parse nested JSON structure with per-field timestamps
    if (doc.containsKey("upperThreshold")) {
        incomingConfig.upperThreshold = doc["upperThreshold"]["value"] | deviceConfig.upperThreshold;
        incomingConfig.upperThresholdLastModified = doc["upperThreshold"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.upperThresholdLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("lowerThreshold")) {
        incomingConfig.lowerThreshold = doc["lowerThreshold"]["value"] | deviceConfig.lowerThreshold;
        incomingConfig.lowerThresholdLastModified = doc["lowerThreshold"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.lowerThresholdLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("tankHeight")) {
        incomingConfig.tankHeight = doc["tankHeight"]["value"] | deviceConfig.tankHeight;
        incomingConfig.tankHeightLastModified = doc["tankHeight"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.tankHeightLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("tankWidth")) {
        incomingConfig.tankWidth = doc["tankWidth"]["value"] | deviceConfig.tankWidth;
        incomingConfig.tankWidthLastModified = doc["tankWidth"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.tankWidthLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("tankShape")) {
        incomingConfig.tankShape = doc["tankShape"]["value"] | deviceConfig.tankShape;
        incomingConfig.tankShapeLastModified = doc["tankShape"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.tankShapeLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("UsedTotal")) {
        incomingConfig.usedTotal = doc["UsedTotal"]["value"] | deviceConfig.usedTotal;
        incomingConfig.usedTotalLastModified = doc["UsedTotal"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.usedTotalLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("maxInflow")) {
        incomingConfig.maxInflow = doc["maxInflow"]["value"] | deviceConfig.maxInflow;
        incomingConfig.maxInflowLastModified = doc["maxInflow"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.maxInflowLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("force_update")) {
        incomingConfig.force_update = doc["force_update"]["value"] | deviceConfig.force_update;
        incomingConfig.forceUpdateLastModified = doc["force_update"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.forceUpdateLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("sensorFilter")) {
        incomingConfig.sensorFilter = doc["sensorFilter"]["value"] | deviceConfig.sensorFilter;
        incomingConfig.sensorFilterLastModified = doc["sensorFilter"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.sensorFilterLastModified == 0) hasPriorityFlag = true;
    }
    if (doc.containsKey("ip_address")) {
        incomingConfig.ipAddress = doc["ip_address"]["value"] | deviceConfig.ipAddress;
        incomingConfig.ipAddressLastModified = doc["ip_address"]["lastModified"] | (uint64_t)0;
        if (incomingConfig.ipAddressLastModified == 0) hasPriorityFlag = true;
    }

    // ✅ SERVER RULE 2: Value unchanged → Skip update
    bool valuesChanged = false;
    if (incomingConfig.upperThreshold != deviceConfig.upperThreshold) valuesChanged = true;
    if (incomingConfig.lowerThreshold != deviceConfig.lowerThreshold) valuesChanged = true;
    if (incomingConfig.tankHeight != deviceConfig.tankHeight) valuesChanged = true;
    if (incomingConfig.tankWidth != deviceConfig.tankWidth) valuesChanged = true;
    if (incomingConfig.tankShape != deviceConfig.tankShape) valuesChanged = true;
    if (incomingConfig.usedTotal != deviceConfig.usedTotal) valuesChanged = true;
    if (incomingConfig.maxInflow != deviceConfig.maxInflow) valuesChanged = true;
    if (incomingConfig.force_update != deviceConfig.force_update) valuesChanged = true;
    if (incomingConfig.sensorFilter != deviceConfig.sensorFilter) valuesChanged = true;
    if (incomingConfig.ipAddress != deviceConfig.ipAddress) valuesChanged = true;

    if (!valuesChanged) {
        // Values identical - skip update
        Serial.println("[WebServer] Config values identical - skipping update");

        StaticJsonDocument<512> responseDoc;
        responseDoc["success"] = true;
        responseDoc["message"] = "No changes - values identical";

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
        jsonBuffer = "";
        return;
    }

    // ✅ SERVER RULE 1: lastModified == 0 → Always update (priority flag)
    if (hasPriorityFlag) {
        Serial.println("[WebServer] Priority flag detected - accepting app config");
        deviceConfig = incomingConfig;

        // Assign new timestamp to ALL fields
        uint64_t currentTimestamp;
        if (apiClient != nullptr) {
            currentTimestamp = apiClient->getCurrentTimestamp();
        } else {
            currentTimestamp = millis();  // Fallback to millis
        }

        // Update all field timestamps
        deviceConfig.upperThresholdLastModified = currentTimestamp;
        deviceConfig.lowerThresholdLastModified = currentTimestamp;
        deviceConfig.tankHeightLastModified = currentTimestamp;
        deviceConfig.tankWidthLastModified = currentTimestamp;
        deviceConfig.tankShapeLastModified = currentTimestamp;
        deviceConfig.usedTotalLastModified = currentTimestamp;
        deviceConfig.maxInflowLastModified = currentTimestamp;
        deviceConfig.forceUpdateLastModified = currentTimestamp;
        deviceConfig.sensorFilterLastModified = currentTimestamp;
        deviceConfig.ipAddressLastModified = currentTimestamp;

        // Mark as locally modified (sets device_config_sync_status = false)
        if (apiClient != nullptr) {
            apiClient->markConfigModified();
        }

        // Trigger immediate sync callback
        if (configSyncCallback != nullptr) {
            Serial.println("[WebServer] Triggering immediate config sync...");
            configSyncCallback();
        }

        Serial.println("[WebServer] Config updated from app (priority)");
        Serial.println("  Upper Threshold: " + String(deviceConfig.upperThreshold));
        Serial.println("  Lower Threshold: " + String(deviceConfig.lowerThreshold));

        // Send success response immediately (don't block on server sync)
        // The heartbeat will handle syncing to server asynchronously
        StaticJsonDocument<512> responseDoc;
        responseDoc["success"] = true;
        responseDoc["message"] = "Config updated (will sync to server on next heartbeat)";

        String response;
        serializeJson(responseDoc, response);
        request->send(200, "application/json", response);
        jsonBuffer = "";
        return;
    }

    // ✅ No other paths: Config must have priority flag (lastModified=0)
    // The app always sends config with priority flag for local updates
    // Per-field LWW is too complex for config (unlike control data)
    Serial.println("[WebServer] Config update rejected - no priority flag");

    StaticJsonDocument<512> responseDoc;
    responseDoc["success"] = false;
    responseDoc["error"] = "PRIORITY_REQUIRED";
    responseDoc["message"] = "Config updates require priority flag (lastModified=0)";

    String response;
    serializeJson(responseDoc, response);
    request->send(400, "application/json", response);
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

    DEBUG_RESPONSE_PRINTLN("[WebServer] Timestamp info: " + response);
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

void WebServer::handle() {
    // ESPAsyncWebServer handles requests asynchronously
    // No need to call handle() in loop
}
