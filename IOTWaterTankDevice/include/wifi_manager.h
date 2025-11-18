#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

// WiFi operation modes
enum WiFiMode {
  WIFI_CLIENT_MODE,
  WIFI_AP_MODE
};

// Initialize WiFi manager
void initWiFiManager();

// Load saved WiFi credentials
bool loadWiFiCredentials(String &ssid, String &password);

// Save WiFi credentials to persistent storage
void saveWiFiCredentials(const char* ssid, const char* password);

// Clear saved WiFi credentials
void clearWiFiCredentials();

// Save dashboard credentials
void saveDashboardCredentials(const char* username, const char* password);

// Get dashboard credentials
bool getDashboardCredentials(String &username, String &password);

// Start WiFi client mode (non-blocking)
bool startWiFiClient();

// Update WiFi connection status (call in loop)
bool updateWiFiConnection();

// Start WiFi Access Point mode
void startWiFiAP();

// Handle WiFi connection maintenance
void handleWiFiConnection();

// Get current WiFi mode
WiFiMode getWiFiMode();

// Get WiFi status as string
String getWiFiStatus();

// Get IP address
String getIPAddress();

// Check if WiFi is connected
bool isWiFiConnected();

// Check if WiFi is disabled
bool isWiFiDisabled();

// Check if WiFi connection is in progress
bool isWiFiConnecting();

// Scan for WiFi networks (returns JSON string)
String scanWiFiNetworks();

// Get MAC address (hardware ID)
String getMACAddress();

// Get WiFi signal strength (RSSI)
int getRSSI();

#endif // WIFI_MANAGER_H
