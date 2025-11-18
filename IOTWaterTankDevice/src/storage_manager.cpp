#include "storage_manager.h"
#include "config.h"

// Global instance
StorageManager storageManager;

StorageManager::StorageManager() {
}

void StorageManager::begin() {
    DEBUG_PRINTLN("[Storage] Storage Manager initialized");
}

bool StorageManager::openNamespace(const char* ns, bool readOnly) {
    return prefs.begin(ns, readOnly);
}

void StorageManager::closeNamespace() {
    prefs.end();
}

// ============================================================================
// WiFi Credentials
// ============================================================================

bool StorageManager::loadWiFiCredentials(String &ssid, String &password) {
    if (!openNamespace("wificfg", true)) {
        DEBUG_PRINTLN("[Storage] Failed to open wificfg namespace");
        return false;
    }

    ssid = prefs.getString("ssid", "");
    password = prefs.getString("password", "");

    closeNamespace();

    if (ssid.length() > 0) {
        DEBUG_PRINTF("[Storage] Loaded WiFi credentials for SSID: %s\n", ssid.c_str());
        return true;
    }

    DEBUG_PRINTLN("[Storage] No WiFi credentials found");
    return false;
}

void StorageManager::saveWiFiCredentials(const String& ssid, const String& password) {
    if (!openNamespace("wificfg", false)) {
        DEBUG_PRINTLN("[Storage] Failed to open wificfg namespace");
        return;
    }

    prefs.putString("ssid", ssid);
    prefs.putString("password", password);

    closeNamespace();

    DEBUG_PRINTF("[Storage] Saved WiFi credentials for SSID: %s\n", ssid.c_str());
}

void StorageManager::clearWiFiCredentials() {
    if (!openNamespace("wificfg", false)) {
        DEBUG_PRINTLN("[Storage] Failed to open wificfg namespace");
        return;
    }

    prefs.remove("ssid");
    prefs.remove("password");

    closeNamespace();

    DEBUG_PRINTLN("[Storage] Cleared WiFi credentials");
}

// ============================================================================
// Dashboard Credentials
// ============================================================================

bool StorageManager::loadDashboardCredentials(String &username, String &password) {
    if (!openNamespace("wificfg", true)) {
        DEBUG_PRINTLN("[Storage] Failed to open wificfg namespace");
        return false;
    }

    username = prefs.getString("dash_user", "");
    password = prefs.getString("dash_pass", "");

    closeNamespace();

    if (username.length() > 0) {
        DEBUG_PRINTF("[Storage] Loaded dashboard credentials for user: %s\n", username.c_str());
        return true;
    }

    DEBUG_PRINTLN("[Storage] No dashboard credentials found");
    return false;
}

void StorageManager::saveDashboardCredentials(const String& username, const String& password) {
    if (!openNamespace("wificfg", false)) {
        DEBUG_PRINTLN("[Storage] Failed to open wificfg namespace");
        return;
    }

    prefs.putString("dash_user", username);
    prefs.putString("dash_pass", password);

    closeNamespace();

    DEBUG_PRINTF("[Storage] Saved dashboard credentials for user: %s\n", username.c_str());
}

// ============================================================================
// Device Authentication
// ============================================================================

String StorageManager::getDeviceToken() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return "";
    }

    String token = prefs.getString(PREF_DEVICE_TOKEN, "");
    closeNamespace();

    return token;
}

void StorageManager::saveDeviceToken(const String& token) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return;
    }

    prefs.putString(PREF_DEVICE_TOKEN, token);
    closeNamespace();

    DEBUG_PRINTLN("[Storage] Saved device token");
}

bool StorageManager::isDeviceRegistered() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return false;
    }

    bool registered = prefs.getBool("dev_registered", false);
    closeNamespace();

    return registered;
}

void StorageManager::setDeviceRegistered(bool registered) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return;
    }

    prefs.putBool("dev_registered", registered);
    closeNamespace();

    DEBUG_PRINTF("[Storage] Device registration flag set to: %s\n", registered ? "true" : "false");
}

void StorageManager::clearDeviceToken() {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return;
    }

    prefs.remove(PREF_DEVICE_TOKEN);
    closeNamespace();

    DEBUG_PRINTLN("[Storage] Cleared device token");
}

String StorageManager::getHardwareId() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return "";
    }

    String hardwareId = prefs.getString(PREF_HARDWARE_ID, "");
    closeNamespace();

    return hardwareId;
}

void StorageManager::saveHardwareId(const String& hardwareId) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return;
    }

    prefs.putString(PREF_HARDWARE_ID, hardwareId);
    closeNamespace();

    DEBUG_PRINTF("[Storage] Saved hardware ID: %s\n", hardwareId.c_str());
}

// ============================================================================
// Auto Mode
// ============================================================================

bool StorageManager::getAutoMode() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return true;  // Default to auto mode
    }

    bool autoMode = prefs.getBool(PREF_AUTO_MODE, true);
    closeNamespace();

    return autoMode;
}

void StorageManager::saveAutoMode(bool autoMode) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        DEBUG_PRINTLN("[Storage] Failed to open namespace");
        return;
    }

    prefs.putBool(PREF_AUTO_MODE, autoMode);
    closeNamespace();

    DEBUG_PRINTF("[Storage] Saved auto mode: %s\n", autoMode ? "true" : "false");
}

// ============================================================================
// Sync Status
// ============================================================================

bool StorageManager::getServerSync() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        return false;
    }

    bool synced = prefs.getBool(PREF_SERVER_SYNC, false);
    closeNamespace();

    return synced;
}

void StorageManager::saveServerSync(bool synced) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        return;
    }

    prefs.putBool(PREF_SERVER_SYNC, synced);
    closeNamespace();
}

bool StorageManager::getConfigSync() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        return true;  // Default: sync FROM server
    }

    bool synced = prefs.getBool(PREF_CONFIG_SYNC, true);
    closeNamespace();

    return synced;
}

void StorageManager::saveConfigSync(bool synced) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        return;
    }

    prefs.putBool(PREF_CONFIG_SYNC, synced);
    closeNamespace();
}

uint64_t StorageManager::getServerTime() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        return 0;
    }

    uint64_t timestamp = prefs.getULong64(PREF_SERVER_TIME, 0);
    closeNamespace();

    return timestamp;
}

void StorageManager::saveServerTime(uint64_t timestamp) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        return;
    }

    prefs.putULong64(PREF_SERVER_TIME, timestamp);
    closeNamespace();
}

uint64_t StorageManager::getMillisSync() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        return 0;
    }

    uint64_t millis = prefs.getULong64(PREF_MILLIS_SYNC, 0);
    closeNamespace();

    return millis;
}

void StorageManager::saveMillisSync(uint64_t millis) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        return;
    }

    prefs.putULong64(PREF_MILLIS_SYNC, millis);
    closeNamespace();
}

uint32_t StorageManager::getOverflowCount() {
    if (!openNamespace(PREF_NAMESPACE, true)) {
        return 0;
    }

    uint32_t count = prefs.getUInt(PREF_OVERFLOW_CNT, 0);
    closeNamespace();

    return count;
}

void StorageManager::saveOverflowCount(uint32_t count) {
    if (!openNamespace(PREF_NAMESPACE, false)) {
        return;
    }

    prefs.putUInt(PREF_OVERFLOW_CNT, count);
    closeNamespace();
}

// ============================================================================
// WiFi Configured Flag
// ============================================================================

bool StorageManager::isWiFiConfigured() {
    if (!openNamespace("wificfg", true)) {
        return false;
    }

    bool configured = prefs.getBool(PREF_WIFI_CONFIGURED, false);
    closeNamespace();

    return configured;
}

void StorageManager::setWiFiConfigured(bool configured) {
    if (!openNamespace("wificfg", false)) {
        return;
    }

    prefs.putBool(PREF_WIFI_CONFIGURED, configured);
    closeNamespace();

    DEBUG_PRINTF("[Storage] WiFi configured flag set to: %s\n", configured ? "true" : "false");
}

// ============================================================================
// DEVICE CONFIGURATION PERSISTENCE
// ============================================================================

void StorageManager::saveDeviceConfig(float upperThreshold, float lowerThreshold,
                                      float tankHeight, float tankWidth, const String& tankShape) {
    if (!openNamespace("devcfg", false)) {
        return;
    }

    prefs.putFloat("upperThr", upperThreshold);
    prefs.putFloat("lowerThr", lowerThreshold);
    prefs.putFloat("tankH", tankHeight);
    prefs.putFloat("tankW", tankWidth);
    prefs.putString("tankShape", tankShape);

    closeNamespace();

    DEBUG_PRINTLN("[Storage] Device config saved to NVS:");
    DEBUG_PRINTF("  Upper Threshold: %.2f\n", upperThreshold);
    DEBUG_PRINTF("  Lower Threshold: %.2f\n", lowerThreshold);
    DEBUG_PRINTF("  Tank Height: %.2f\n", tankHeight);
    DEBUG_PRINTF("  Tank Width: %.2f\n", tankWidth);
    DEBUG_PRINTF("  Tank Shape: %s\n", tankShape.c_str());
}

bool StorageManager::loadDeviceConfig(float& upperThreshold, float& lowerThreshold,
                                      float& tankHeight, float& tankWidth, String& tankShape) {
    if (!openNamespace("devcfg", true)) {
        return false;
    }

    // Check if config exists by checking if tankHeight was ever set
    if (!prefs.isKey("tankH")) {
        closeNamespace();
        DEBUG_PRINTLN("[Storage] No device config found in NVS");
        return false;
    }

    upperThreshold = prefs.getFloat("upperThr", 95.0f);
    lowerThreshold = prefs.getFloat("lowerThr", 20.0f);
    tankHeight = prefs.getFloat("tankH", 0.0f);
    tankWidth = prefs.getFloat("tankW", 0.0f);
    tankShape = prefs.getString("tankShape", "");

    closeNamespace();

    DEBUG_PRINTLN("[Storage] Device config loaded from NVS:");
    DEBUG_PRINTF("  Upper Threshold: %.2f\n", upperThreshold);
    DEBUG_PRINTF("  Lower Threshold: %.2f\n", lowerThreshold);
    DEBUG_PRINTF("  Tank Height: %.2f\n", tankHeight);
    DEBUG_PRINTF("  Tank Width: %.2f\n", tankWidth);
    DEBUG_PRINTF("  Tank Shape: %s\n", tankShape.c_str());

    return true;
}

bool StorageManager::hasDeviceConfig() {
    if (!openNamespace("devcfg", true)) {
        return false;
    }

    bool hasConfig = prefs.isKey("tankH");
    closeNamespace();

    return hasConfig;
}
