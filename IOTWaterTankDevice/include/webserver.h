#ifndef WEBSERVER_H
#define WEBSERVER_H

// HTTP Methods for ESPAsyncWebServer compatibility
// IMPORTANT: These must be defined BEFORE ESPAsyncWebServer.h is included
#ifndef HTTP_GET
#define HTTP_GET 0x01
#endif
#ifndef HTTP_POST
#define HTTP_POST 0x02
#endif
#ifndef HTTP_ANY
#define HTTP_ANY 0xFF
#endif

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "api_client.h"

// Callback function types
typedef void (*PumpControlCallback)(bool state);
typedef void (*WiFiSaveCallback)(const String& ssid, const String& password,
                                 const String& dashUser, const String& dashPass);

class WebServer {
public:
    WebServer();

    // Initialize web server with device ID and API client
    void begin(const String& deviceId, APIClient* apiCli);

    // Set current sensor data for API endpoints
    void updateSensorData(float waterLevel, float currInflow, int pumpStatus);

    // Set device configuration for API endpoints
    void updateDeviceConfig(const DeviceConfig& config);

    // Set control data for API endpoints
    void updateControlData(const ControlData& control);

    // Set pump control callback
    void setPumpControlCallback(PumpControlCallback callback);

    // Set WiFi save callback
    void setWiFiSaveCallback(WiFiSaveCallback callback);

    // Handle server (called in loop if needed)
    void handle();

private:
    AsyncWebServer server;

    // Current sensor readings
    float currentWaterLevel;
    float currentInflow;
    int currentPumpStatus;

    // Device configuration and control data
    DeviceConfig deviceConfig;
    ControlData controlData;

    // Device ID and API client
    String deviceId;
    APIClient* apiClient;

    // Callbacks
    PumpControlCallback pumpCallback;
    WiFiSaveCallback wifiSaveCallback;

    // Setup routes
    void setupRoutes();

    // Route handlers - device endpoints
    void handleGetTelemetry(AsyncWebServerRequest* request);
    void handleGetControl(AsyncWebServerRequest* request);
    void handlePostControl(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                           size_t index, size_t total);
    void handleGetDeviceConfig(AsyncWebServerRequest* request);
    void handlePostDeviceConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                size_t index, size_t total);
    void handleGetTimestamp(AsyncWebServerRequest* request);
    void handlePostTimestamp(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                             size_t index, size_t total);

    // Route handlers - WiFi provisioning endpoints
    void handleProvisioningStatus(AsyncWebServerRequest* request);
    void handleScanWiFi(AsyncWebServerRequest* request);
    void handleSaveCredentials(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                               size_t index, size_t total);

    // Route handlers - other
    void handleNotFound(AsyncWebServerRequest* request);
};

#endif // WEBSERVER_H
