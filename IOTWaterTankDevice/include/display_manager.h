#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

enum DisplayScreen {
    SCREEN_STATUS,    // Screen 1: Water level bar, pump status, WiFi signal
    SCREEN_NETWORK,   // Screen 2: Network info (IP, RSSI, uptime)
    SCREEN_SETTINGS,  // Screen 3: Tank settings
    SCREEN_COUNT      // Total number of screens
};

class DisplayManager {
public:
    DisplayManager();

    // Initialize OLED display
    bool begin();

    // Update display with current data
    void update(float waterLevel, float waterLevelPercent, bool pumpOn,
               const String& pumpMode, int rssi, bool wifiConnected);

    // Set network info
    void setNetworkInfo(const String& ip, const String& ssid);

    // Set tank settings
    void setTankSettings(float height, float width, const String& shape,
                        float upperThreshold, float lowerThreshold);

    // Cycle to next screen (BTN1)
    void nextScreen();

    // Get current screen
    DisplayScreen getCurrentScreen();

    // Show message (for errors, status updates, etc.)
    void showMessage(const String& title, const String& message, int duration = 2000);

    // Show startup splash screen
    void showSplash();

    // Clear display
    void clear();

private:
    Adafruit_SSD1306 display;
    DisplayScreen currentScreen;

    // Network info
    String ipAddress;
    String ssidName;

    // Tank settings
    float tankHeight;
    float tankWidth;
    String tankShape;
    float upperThreshold;
    float lowerThreshold;

    unsigned long uptime;

    // Draw different screens
    void drawStatusScreen(float waterLevel, float waterLevelPercent,
                         bool pumpOn, const String& pumpMode, int rssi, bool wifiConnected);
    void drawNetworkScreen(int rssi, bool wifiConnected);
    void drawSettingsScreen();

    // Draw helper functions
    void drawWiFiIcon(int x, int y, int rssi, bool connected);
    void drawProgressBar(int x, int y, int width, int height, float percent);
    void drawPumpIcon(int x, int y, bool on);

    // Get uptime string
    String getUptimeString();
};

#endif // DISPLAY_MANAGER_H
