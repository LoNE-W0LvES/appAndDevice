#include "display_manager.h"

DisplayManager::DisplayManager()
    : display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET),
      currentScreen(SCREEN_STATUS),
      tankHeight(DEFAULT_TANK_HEIGHT),
      tankWidth(DEFAULT_TANK_WIDTH),
      tankShape("Cylindrical"),
      upperThreshold(DEFAULT_UPPER_THRESHOLD),
      lowerThreshold(DEFAULT_LOWER_THRESHOLD),
      uptime(0) {
}

bool DisplayManager::begin() {
    // Initialize I2C with custom pins
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    // Initialize display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("[Display] SSD1306 allocation failed");
        return false;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    Serial.println("[Display] OLED initialized");

    showSplash();

    return true;
}

void DisplayManager::showSplash() {
    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println("WATER");
    display.println("  TANK");

    display.setTextSize(1);
    display.setCursor(0, 50);
    display.println("Initializing...");

    display.display();
    delay(2000);
}

void DisplayManager::clear() {
    display.clearDisplay();
    display.display();
}

void DisplayManager::update(float waterLevel, float waterLevelPercent, bool pumpOn,
                           const String& pumpMode, int rssi, bool wifiConnected) {
    uptime = millis();

    display.clearDisplay();

    switch (currentScreen) {
        case SCREEN_STATUS:
            drawStatusScreen(waterLevel, waterLevelPercent, pumpOn, pumpMode, rssi, wifiConnected);
            break;

        case SCREEN_NETWORK:
            drawNetworkScreen(rssi, wifiConnected);
            break;

        case SCREEN_SETTINGS:
            drawSettingsScreen();
            break;

        default:
            currentScreen = SCREEN_STATUS;
            break;
    }

    display.display();
}

void DisplayManager::drawStatusScreen(float waterLevel, float waterLevelPercent,
                                      bool pumpOn, const String& pumpMode,
                                      int rssi, bool wifiConnected) {
    // Screen 1: Water level bar, pump status, WiFi signal

    // Title
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WATER TANK STATUS");

    // Draw WiFi icon in top right
    drawWiFiIcon(110, 0, rssi, wifiConnected);

    // Water level percentage (large text)
    display.setTextSize(2);
    display.setCursor(0, 15);
    display.print(waterLevelPercent, 1);
    display.println("%");

    // Water level in cm (small text)
    display.setTextSize(1);
    display.setCursor(70, 20);
    display.print(waterLevel, 1);
    display.println(" cm");

    // Progress bar
    drawProgressBar(0, 35, 128, 10, waterLevelPercent);

    // Pump status
    display.setCursor(0, 48);
    display.print("Pump: ");
    display.print(pumpOn ? "ON " : "OFF");
    display.print(" (");
    display.print(pumpMode);
    display.println(")");

    // Draw pump icon
    drawPumpIcon(110, 48, pumpOn);

    // Screen indicator
    display.setCursor(0, 56);
    display.print("1/3");
}

void DisplayManager::drawNetworkScreen(int rssi, bool wifiConnected) {
    // Screen 2: Network info (IP, RSSI, uptime)

    // Title
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("NETWORK INFO");

    // WiFi status
    display.setCursor(0, 12);
    display.print("WiFi: ");
    display.println(wifiConnected ? "Connected" : "Disconnected");

    // SSID
    if (ssidName.length() > 0) {
        display.setCursor(0, 22);
        display.print("SSID: ");
        display.println(ssidName);
    }

    // IP Address
    display.setCursor(0, 32);
    display.print("IP: ");
    display.println(ipAddress);

    // Signal strength
    if (wifiConnected) {
        display.setCursor(0, 42);
        display.print("RSSI: ");
        display.print(rssi);
        display.println(" dBm");
    }

    // Uptime
    display.setCursor(0, 52);
    display.print("Up: ");
    display.println(getUptimeString());

    // Screen indicator
    display.setCursor(0, 56);
    display.print("2/3");

    // WiFi icon
    drawWiFiIcon(110, 0, rssi, wifiConnected);
}

void DisplayManager::drawSettingsScreen() {
    // Screen 3: Tank settings

    // Title
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("TANK SETTINGS");

    // Tank dimensions
    display.setCursor(0, 12);
    display.print("Height: ");
    display.print(tankHeight, 0);
    display.println(" cm");

    display.setCursor(0, 22);
    display.print("Width: ");
    display.print(tankWidth, 0);
    display.println(" cm");

    display.setCursor(0, 32);
    display.print("Shape: ");
    display.println(tankShape);

    // Thresholds
    display.setCursor(0, 42);
    display.print("Upper: ");
    display.print(upperThreshold, 0);
    display.println("%");

    display.setCursor(0, 52);
    display.print("Lower: ");
    display.print(lowerThreshold, 0);
    display.println("%");

    // Screen indicator
    display.setCursor(0, 56);
    display.print("3/3");
}

void DisplayManager::drawWiFiIcon(int x, int y, int rssi, bool connected) {
    if (!connected) {
        // Draw X for disconnected
        display.drawLine(x, y, x + 8, y + 8, SSD1306_WHITE);
        display.drawLine(x + 8, y, x, y + 8, SSD1306_WHITE);
        return;
    }

    // Draw WiFi signal bars based on RSSI
    int bars = 0;
    if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;

    for (int i = 0; i < 3; i++) {
        int barHeight = (i + 1) * 3;
        if (i < bars) {
            display.fillRect(x + (i * 3), y + 9 - barHeight, 2, barHeight, SSD1306_WHITE);
        } else {
            display.drawRect(x + (i * 3), y + 9 - barHeight, 2, barHeight, SSD1306_WHITE);
        }
    }
}

void DisplayManager::drawProgressBar(int x, int y, int width, int height, float percent) {
    // Draw border
    display.drawRect(x, y, width, height, SSD1306_WHITE);

    // Draw filled portion
    int fillWidth = (int)((width - 2) * (percent / 100.0));
    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    }
}

void DisplayManager::drawPumpIcon(int x, int y, bool on) {
    // Simple pump icon (circle with arrow)
    if (on) {
        display.fillCircle(x + 4, y + 4, 4, SSD1306_WHITE);
        display.drawLine(x + 4, y, x + 4, y - 3, SSD1306_WHITE); // Up arrow
    } else {
        display.drawCircle(x + 4, y + 4, 4, SSD1306_WHITE);
    }
}

void DisplayManager::nextScreen() {
    int next = (int)currentScreen + 1;
    if (next >= SCREEN_COUNT) {
        next = 0;
    }
    currentScreen = (DisplayScreen)next;

    Serial.println("[Display] Screen changed to: " + String(next + 1) + "/" + String(SCREEN_COUNT));
}

DisplayScreen DisplayManager::getCurrentScreen() {
    return currentScreen;
}

void DisplayManager::setNetworkInfo(const String& ip, const String& ssid) {
    ipAddress = ip;
    ssidName = ssid;
}

void DisplayManager::setTankSettings(float height, float width, const String& shape,
                                    float upper, float lower) {
    tankHeight = height;
    tankWidth = width;
    tankShape = shape;
    upperThreshold = upper;
    lowerThreshold = lower;
}

void DisplayManager::showMessage(const String& title, const String& message, int duration) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(title);

    display.setCursor(0, 20);
    display.println(message);

    display.display();

    if (duration > 0) {
        delay(duration);
    }
}

String DisplayManager::getUptimeString() {
    unsigned long seconds = uptime / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    if (days > 0) {
        return String(days) + "d " + String(hours % 24) + "h";
    } else if (hours > 0) {
        return String(hours) + "h " + String(minutes % 60) + "m";
    } else {
        return String(minutes) + "m " + String(seconds % 60) + "s";
    }
}
