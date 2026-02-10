#ifndef SYNC_MANAGER_H
#define SYNC_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include "HibernationManager.h"
#include "LedManager.h"

class SyncManager {
public:
    SyncManager(HibernationManager &hibernationMgr, LedManager &ledMgr);

    // Pulls image from remote URL. Returns true if a new image was downloaded.
    // etag is updated if a new one is provided by the server.
    bool pullImage(const String &url, const String &apiKey, String &etag);

    // Pushes local image to remote URL.
    bool pushImage(const String &url, const String &apiKey, const String &filePath);

private:
    HibernationManager &_hibernationManager;
    LedManager &_ledManager;

    void setupClient(HTTPClient &http, const String &url, const String &apiKey);
};

#endif // SYNC_MANAGER_H
