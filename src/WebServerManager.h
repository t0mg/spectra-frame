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
