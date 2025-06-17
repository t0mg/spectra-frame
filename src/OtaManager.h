#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ESPAsyncWebServer.h> // Required for AsyncWebServerRequest
#include "LedManager.h" // Include for LedMode enum

// Forward declarations to avoid circular dependencies
class DisplayManager;
class HibernationManager;

class OtaManager {
public:
    OtaManager(HibernationManager& hibernationMgr, LedManager &ledMgr);
    void handleFirmwareUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);

private:
    HibernationManager& _hibernationMgr;
    LedManager& _ledManager;
};

#endif // OTA_MANAGER_H
