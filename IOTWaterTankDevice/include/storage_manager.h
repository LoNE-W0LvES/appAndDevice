#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// Storage manager for all NVS operations
class StorageManager {
public:
    StorageManager();

    // Initialize storage
    void begin();

    // WiFi credentials
    bool loadWiFiCredentials(String &ssid, String &password);
    void saveWiFiCredentials(const String& ssid, const String& password);
    void clearWiFiCredentials();

    // Dashboard credentials
    bool loadDashboardCredentials(String &username, String &password);
    void saveDashboardCredentials(const String& username, const String& password);

    // Device authentication
    String getDeviceToken();
    void saveDeviceToken(const String& token);
    void clearDeviceToken();

    String getHardwareId();
    void saveHardwareId(const String& hardwareId);

    // Device registration flag
    bool isDeviceRegistered();
    void setDeviceRegistered(bool registered);

    // Auto mode preference
    bool getAutoMode();
    void saveAutoMode(bool autoMode);

    // Sync status
    bool getServerSync();
    void saveServerSync(bool synced);

    bool getConfigSync();
    void saveConfigSync(bool synced);

    uint64_t getServerTime();
    void saveServerTime(uint64_t timestamp);

    uint64_t getMillisSync();
    void saveMillisSync(uint64_t millis);

    uint32_t getOverflowCount();
    void saveOverflowCount(uint32_t count);

    // WiFi configured flag
    bool isWiFiConfigured();
    void setWiFiConfigured(bool configured);

    // Device configuration persistence
    // Save critical config values so device works offline without server
    void saveDeviceConfig(float upperThreshold, float lowerThreshold,
                         float tankHeight, float tankWidth, const String& tankShape);
    bool loadDeviceConfig(float& upperThreshold, float& lowerThreshold,
                         float& tankHeight, float& tankWidth, String& tankShape);
    bool hasDeviceConfig();  // Check if config exists in storage

private:
    Preferences prefs;

    // Helper to open preferences namespace
    bool openNamespace(const char* ns, bool readOnly);
    void closeNamespace();
};

// Global storage manager instance
extern StorageManager storageManager;

#endif // STORAGE_MANAGER_H
