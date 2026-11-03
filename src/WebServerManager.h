#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "HibernationManager.h"
#include "LedManager.h"
#include "OtaManager.h"
#include "WiFiManager.h"
#include "Constants.h"

class WebServerManager {
public:
    WebServerManager(ConfigManager& configMgr, DisplayManager& displayMgr,
                     HibernationManager& hibernationMgr, LedManager& ledMgr,
                     OtaManager& otaMgr);
    void begin();
    bool isNewImageReady() const { return _newImageReady; }
    void clearNewImageReady() { _newImageReady = false; }

    static void setDisplayStatus(DisplayUpdateStatus status, const char* message = nullptr);
    static void setDisplayInProgress(bool inProgress);
    static bool isDisplayInProgress();
    static void checkResetReason(esp_reset_reason_t reason);
    static const char* getDisplayStatusString();
    static const char* getDisplayStatusMessage();
    static const char* getResetReasonString(esp_reset_reason_t reason);

private:
    AsyncWebServer _server;
    ConfigManager& _configMgr;
    DisplayManager& _displayMgr;
    HibernationManager& _hibernationMgr;
    LedManager& _ledMgr;
    OtaManager& _otaMgr;
    bool _newImageReady = false;

    void setupHandlers();
    void handleSaveConfig(AsyncWebServerRequest *request);
    void handleImageUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

    // Captive portal handler
    class CaptiveRequestHandler : public AsyncWebHandler {
    public:
        CaptiveRequestHandler() {}
        virtual ~CaptiveRequestHandler() {}
        bool canHandle(AsyncWebServerRequest *request) const override;
        void handleRequest(AsyncWebServerRequest *request) override;
    };
};

#endif // WEB_SERVER_MANAGER_H
