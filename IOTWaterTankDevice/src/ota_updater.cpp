#include "ota_updater.h"

OTAUpdater* OTAUpdater::instance = nullptr;

OTAUpdater::OTAUpdater() : updating(false), progress(0), lastError("") {
    instance = this;
}

void OTAUpdater::begin() {
    Serial.println("[OTA] OTA Updater initialized");
}

void OTAUpdater::onProgress(size_t current, size_t total) {
    if (instance) {
        instance->progress = (current * 100) / total;

        if (instance->progress % 10 == 0) {
            Serial.println("[OTA] Progress: " + String(instance->progress) + "%");
        }
    }
}

bool OTAUpdater::downloadAndInstall(const String& url, const String& token) {
    HTTPClient http;

    Serial.println("[OTA] Downloading firmware from: " + url);

    http.begin(url);
    http.addHeader("Authorization", "Bearer " + token);
    http.setTimeout(30000); // 30 second timeout

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        lastError = "HTTP error: " + String(httpCode);
        Serial.println("[OTA] " + lastError);
        http.end();
        return false;
    }

    int contentLength = http.getSize();

    if (contentLength <= 0) {
        lastError = "Invalid content length";
        Serial.println("[OTA] " + lastError);
        http.end();
        return false;
    }

    Serial.println("[OTA] Firmware size: " + String(contentLength) + " bytes");

    // Check if we have enough space
    if (!Update.begin(contentLength)) {
        lastError = "Not enough space for update";
        Serial.println("[OTA] " + lastError);
        http.end();
        return false;
    }

    // Set progress callback
    Update.onProgress(onProgress);

    // Get stream
    WiFiClient* stream = http.getStreamPtr();

    // Write firmware
    size_t written = Update.writeStream(*stream);

    if (written != contentLength) {
        lastError = "Write failed: " + String(written) + "/" + String(contentLength);
        Serial.println("[OTA] " + lastError);
        Update.abort();
        http.end();
        return false;
    }

    Serial.println("[OTA] Firmware written: " + String(written) + " bytes");

    // End update
    if (!Update.end()) {
        lastError = "Update end failed: " + String(Update.getError());
        Serial.println("[OTA] " + lastError);
        http.end();
        return false;
    }

    http.end();

    // Verify update
    if (Update.isFinished()) {
        Serial.println("[OTA] Update successfully completed!");
        return true;
    } else {
        lastError = "Update not finished";
        Serial.println("[OTA] " + lastError);
        return false;
    }
}

bool OTAUpdater::checkAndUpdate(const String& deviceToken) {
    if (updating) {
        Serial.println("[OTA] Update already in progress");
        return false;
    }

    updating = true;
    progress = 0;
    lastError = "";

    Serial.println("[OTA] Checking for firmware update...");

    // Build firmware URL
    String firmwareUrl = String(SERVER_URL) + API_FIRMWARE_LATEST;

    // Download and install
    bool success = downloadAndInstall(firmwareUrl, deviceToken);

    updating = false;

    if (success) {
        Serial.println("[OTA] Firmware update successful, restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
        return true;
    } else {
        Serial.println("[OTA] Firmware update failed: " + lastError);
        return false;
    }
}

bool OTAUpdater::isUpdating() {
    return updating;
}

int OTAUpdater::getProgress() {
    return progress;
}

String OTAUpdater::getLastError() {
    return lastError;
}
