#include "SyncManager.h"
#include "Debug.h"
#include <WiFiClientSecure.h>

SyncManager::SyncManager(HibernationManager &hibernationMgr, LedManager &ledMgr)
    : _hibernationManager(hibernationMgr), _ledManager(ledMgr) {}

static String appendQueryParam(String url, String param, String value) {
    if (value.length() == 0) return url;
    char separator = (url.indexOf('?') == -1) ? '?' : '&';
    return url + separator + param + "=" + value;
}

void SyncManager::setupClient(HTTPClient &http, const String &url, const String &apiKey) {
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000); // 15 seconds timeout
    http.addHeader("User-Agent", "SpectraFrame-ESP32S3");
}

bool SyncManager::pullImage(const String &url, const String &apiKey, String &etag) {
    if (url.length() == 0) return false;

    // First request: Get the download URL and current ETag from the sync server
    String syncUrl = url;
    syncUrl = appendQueryParam(syncUrl, "apiKey", apiKey);
    syncUrl = appendQueryParam(syncUrl, "etag", etag); // Send current etag to allow server-side check

    HTTPClient http;
    WiFiClientSecure clientSecure;
    WiFiClient clientPlain;

    if (syncUrl.startsWith("https://")) {
        clientSecure.setInsecure();
        http.begin(clientSecure, syncUrl);
    } else {
        http.begin(clientPlain, syncUrl);
    }

    setupClient(http, syncUrl, apiKey);

    Debugf("Sync: Checking for updates at %s\n", url.c_str());
    _ledManager.setMode(LedMode::LED_MODE_BUSY);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        http.end();

        if (payload == "304" || payload.length() == 0) {
            Debugln("Sync: Image not modified.");
            _ledManager.setMode(LedMode::LED_MODE_IDLE);
            return false;
        }

        int pipeIdx = payload.indexOf('|');
        if (pipeIdx == -1) {
            Debugf("Sync: Invalid response format: %s\n", payload.c_str());
            _ledManager.setMode(LedMode::LED_MODE_ERROR);
            return false;
        }

        String remoteEtag = payload.substring(0, pipeIdx);
        String downloadUrl = payload.substring(pipeIdx + 1);

        if (remoteEtag == etag) {
            Debugln("Sync: Image already up to date.");
            _ledManager.setMode(LedMode::LED_MODE_IDLE);
            return false;
        }

        Debugf("Sync: New version found (%s). Downloading from %s\n", remoteEtag.c_str(), downloadUrl.c_str());

        // Second request: Download the actual binary data
        if (downloadUrl.startsWith("https://")) {
            clientSecure.setInsecure();
            http.begin(clientSecure, downloadUrl);
        } else {
            http.begin(clientPlain, downloadUrl);
        }
        setupClient(http, downloadUrl, ""); // No apiKey needed for direct download

        httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            Debugf("Sync: Download failed, error: %s (%d)\n", http.errorToString(httpCode).c_str(), httpCode);
            http.end();
            _ledManager.setMode(LedMode::LED_MODE_ERROR);
            return false;
        }

        File file = LittleFS.open("/image.bin", "w");
        if (!file) {
            Debugln("Sync: Failed to open /image.bin for writing");
            http.end();
            _ledManager.setMode(LedMode::LED_MODE_ERROR);
            return false;
        }

        // Stream the response to file
        int len = http.getSize();
        WiFiClient *stream = http.getStreamPtr();
        uint8_t buffer[2048];
        size_t totalRead = 0;

        Debugf("Sync: Downloading %d bytes...\n", len);

        while (http.connected() && (len > 0 || len == -1)) {
            size_t size = stream->available();
            if (size) {
                int c = stream->readBytes(buffer, std::min(size, sizeof(buffer)));
                file.write(buffer, c);
                totalRead += c;
                if (len > 0) len -= c;

                // Reset activity timer so we don't hibernate during download
                _hibernationManager.resetActivityTimer();
            }
            if (len == 0) break;
            delay(1);
        }

        file.close();
        http.end();

        if (totalRead > 0) {
            Debugf("Sync: Download complete. Total: %u bytes\n", totalRead);
            if (newEtag.length() > 0) {
                etag = newEtag;
            } else {
                // If no ETag/Last-Modified, we use the current timestamp as a fake ETag
                etag = String(millis());
            }
            _ledManager.setMode(LedMode::LED_MODE_IDLE);
            return true;
        } else {
            Debugln("Sync: Download failed (0 bytes received)");
            _ledManager.setMode(LedMode::LED_MODE_ERROR);
            return false;
        }
    } else if (httpCode == HTTP_CODE_NOT_MODIFIED) {
        Debugln("Sync: Image not modified (304)");
        http.end();
        _ledManager.setMode(LedMode::LED_MODE_IDLE);
        return false;
    } else {
        Debugf("Sync: GET failed, error: %s (%d)\n", http.errorToString(httpCode).c_str(), httpCode);
        http.end();
        _ledManager.setMode(LedMode::LED_MODE_ERROR);
        return false;
    }
}

bool SyncManager::pushImage(const String &url, const String &apiKey, const String &filePath) {
    if (url.length() == 0) return false;

    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Debugf("Sync: Failed to open %s for reading\n", filePath.c_str());
        return false;
    }

    String pushUrl = url;
    pushUrl = appendQueryParam(pushUrl, "apiKey", apiKey);

    HTTPClient http;
    WiFiClientSecure clientSecure;
    WiFiClient clientPlain;

    if (pushUrl.startsWith("https://")) {
        clientSecure.setInsecure();
        http.begin(clientSecure, pushUrl);
    } else {
        http.begin(clientPlain, pushUrl);
    }

    setupClient(http, pushUrl, apiKey);
    http.addHeader("Content-Type", "application/octet-stream");

    Debugf("Sync: Pushing image to %s (%u bytes)\n", url.c_str(), (unsigned int)file.size());
    _ledManager.setMode(LedMode::LED_MODE_BUSY);

    int httpCode = http.sendRequest("POST", &file, file.size());

    file.close();
    http.end();

    if (httpCode >= 200 && httpCode < 300) {
        Debugf("Sync: Push successful (%d)\n", httpCode);
        _ledManager.setMode(LedMode::LED_MODE_IDLE);
        return true;
    } else {
        Debugf("Sync: Push failed, error: %s (%d)\n", http.errorToString(httpCode).c_str(), httpCode);
        _ledManager.setMode(LedMode::LED_MODE_ERROR);
        return false;
    }
}
