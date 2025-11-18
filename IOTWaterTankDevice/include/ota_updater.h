#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include <Arduino.h>
#include <Update.h>
#include <HTTPClient.h>
#include "config.h"

class OTAUpdater {
public:
    OTAUpdater();

    // Initialize OTA updater
    void begin();

    // Check for firmware update and perform if available
    // Returns true if update was successful, false otherwise
    bool checkAndUpdate(const String& deviceToken);

    // Get update status
    bool isUpdating();

    // Get update progress (0-100)
    int getProgress();

    // Get last error message
    String getLastError();

private:
    bool updating;
    int progress;
    String lastError;

    // Download and install firmware
    bool downloadAndInstall(const String& url, const String& token);

    // Callback for update progress
    static void onProgress(size_t current, size_t total);

    static OTAUpdater* instance; // For static callback
};

#endif // OTA_UPDATER_H
