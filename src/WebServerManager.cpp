#include "WebServerManager.h"
#ifdef ARDUINO_PROS3
#include <UMS3.h>
extern UMS3 ums3;
#endif

WebServerManager::WebServerManager(ConfigManager& configMgr, DisplayManager& displayMgr,
                                 HibernationManager& hibernationMgr, LedManager& ledMgr,
                                 OtaManager& otaMgr)
    : _server(80), _configMgr(configMgr), _displayMgr(displayMgr),
      _hibernationMgr(hibernationMgr), _ledMgr(ledMgr), _otaMgr(otaMgr) {
}

void WebServerManager::begin() {
    setupHandlers();
    _server.begin();
    Debugln("HTTP server started");
}

void WebServerManager::setupHandlers() {
    // Serve main page and reset activity timer
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        request->send(LittleFS, "/index.html", "text/html", false);
    });
    _server.on("/index.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        request->send(LittleFS, "/index.html", "text/html", false);
    });

    // Serve other static files
    _server.serveStatic("/style.css", LittleFS, "/style.css");
    _server.serveStatic("/js/config.js", LittleFS, "/js/config.js");
    _server.serveStatic("/js/image-processor.js", LittleFS, "/js/image-processor.js");
    _server.serveStatic("/js/webgl-processor.js", LittleFS, "/js/webgl-processor.js");
    _server.serveStatic("/js/main.js", LittleFS, "/js/main.js");
    _server.serveStatic("/js/ui.js", LittleFS, "/js/ui.js");
    _server.serveStatic("/js/utils.js", LittleFS, "/js/utils.js");

    // API endpoint for configuration
    _server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSaveConfig(request);
    });

    // API endpoint for image upload
    _server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Response handled in handleImageUpload final block
    }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        handleImageUpload(request, filename, index, data, len, final);
    });

    // API endpoint for firmware update
    _server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest *request) {},
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            _otaMgr.handleFirmwareUpload(request, filename, index, data, len, final);
        }
    );

    // API endpoint for current touch read value
    _server.on("/api/touch_read", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        int touchValue = touchRead(TOUCH_PIN);
        JsonDocument doc;
        doc["value"] = touchValue;
        doc["threshold"] = _configMgr.getConfig().touchThreshold;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API endpoint to set touch threshold
    _server.on("/api/set_touch_threshold", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        if (request->hasParam("threshold", true)) {
            int newThreshold = request->getParam("threshold", true)->value().toInt();
            if (newThreshold > 0 && newThreshold < 100000) {
                _configMgr.getConfig().touchThreshold = newThreshold;
                if (_configMgr.save()) {
                    touchSleepWakeUpEnable(TOUCH_PIN, newThreshold);
                    request->send(200, "text/plain", "Touch threshold updated and saved.");
                } else {
                    request->send(500, "text/plain", "Failed to save configuration.");
                }
            } else {
                request->send(400, "text/plain", "Invalid threshold value. Must be between 1 and 99999.");
            }
        } else {
            request->send(400, "text/plain", "Missing 'threshold' parameter.");
        }
    });

    // API endpoint to set sleep delay
    _server.on("/api/set_sleep_delay", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        if (request->hasParam("delay_minutes", true)) {
            unsigned long delayMinutes = request->getParam("delay_minutes", true)->value().toInt();
            if (delayMinutes >= 1) {
                _configMgr.getConfig().inactivityTimeoutMs = delayMinutes * 60 * 1000;
                if (_configMgr.save()) {
                    _hibernationMgr.setInactivityTimeout(_configMgr.getConfig().inactivityTimeoutMs);
                    request->send(200, "text/plain", "Sleep delay updated and saved.");
                } else {
                    request->send(500, "text/plain", "Failed to save configuration.");
                }
            } else {
                request->send(400, "text/plain", "Invalid sleep delay (must be at least 1 minute).");
            }
        } else {
            request->send(400, "text/plain", "Missing 'delay_minutes' parameter.");
        }
    });

    // API endpoint to set orientation
    _server.on("/api/set_orientation", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        if (request->hasParam("orientation", true)) {
            String newOrientation = request->getParam("orientation", true)->value();
            if (newOrientation == "portrait" || newOrientation == "landscape") {
                _configMgr.getConfig().orientation = newOrientation;
                if (_configMgr.save()) {
                    request->send(200, "text/plain", "Orientation updated. Please restart the device for changes to take effect.");
                } else {
                    request->send(500, "text/plain", "Failed to save configuration.");
                }
            } else {
                request->send(400, "text/plain", "Invalid orientation value. Must be 'portrait' or 'landscape'.");
            }
        } else {
            request->send(400, "text/plain", "Missing 'orientation' parameter.");
        }
    });

    // API endpoint for immediate sleep
    _server.on("/api/sleep_now", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        request->send(200, "text/plain", "Initiating deep sleep...");
        delay(100);
        _ledMgr.setMode(LedMode::LED_MODE_OFF);
        _hibernationMgr.goToDeepSleep();
    });

    // API endpoint to get current status/config
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _hibernationMgr.resetActivityTimer();
        Config& config = _configMgr.getConfig();
        JsonDocument doc;
        doc["ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "N/A";
        doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "N/A";
        doc["mode"] = WiFi.getMode() == WIFI_AP ? "AP" : "STA";
        doc["touchThreshold"] = config.touchThreshold;
        doc["inactivityTimeoutMinutes"] = config.inactivityTimeoutMs / (60 * 1000);
        doc["orientation"] = config.orientation;
        doc["hibernationPaused"] = _hibernationMgr.isPaused();

        #ifdef ARDUINO_PROS3
        float batt_voltage = ums3.getBatteryVoltage();
        #else
        float batt_voltage = (analogReadMilliVolts(BATT_ADC_PIN) / 1000.0) * BATT_VOLTAGE_DIVIDER_RATIO;
        #endif
        doc["batteryVoltage"] = batt_voltage;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Captive portal handler
    _server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);

    _server.onNotFound([this](AsyncWebServerRequest *request) {
        if (WiFi.getMode() == WIFI_AP && request->host() != WiFi.softAPIP().toString() && request->host() != (String(WiFiManager::AP_SSID) + ".local")) {
            AsyncResponseStream *response = request->beginResponseStream("text/html");
            response->print("<!DOCTYPE HTML><html><head><title>Redirecting</title>");
            response->printf("<meta http-equiv='refresh' content='0; url=http://%s/#config'>", WiFi.softAPIP().toString().c_str());
            response->print("</head><body>Redirecting to configuration page...</body></html>");
            request->send(response);
            _hibernationMgr.resetActivityTimer();
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
}

void WebServerManager::handleSaveConfig(AsyncWebServerRequest *request) {
    _hibernationMgr.resetActivityTimer();
    if (request->method() == HTTP_POST) {
        String ssid, password;
        bool success = true;
        if (request->hasParam("ssid", true)) {
            ssid = request->getParam("ssid", true)->value();
        } else {
            success = false;
        }
        if (request->hasParam("password", true)) {
            password = request->getParam("password", true)->value();
        }

        if (success && ssid.length() > 0) {
            _configMgr.getConfig().ssid = ssid;
            _configMgr.getConfig().password = password;
            if (_configMgr.save()) {
                request->send(200, "text/plain", "Configuration saved. Restarting to connect...");
                delay(1000);
                ESP.restart();
            } else {
                request->send(500, "text/plain", "Error: Failed to save configuration.");
            }
        } else {
            request->send(400, "text/plain", "Error: SSID is required.");
        }
    } else {
        request->send(405, "text/plain", "Method Not Allowed");
    }
}

void WebServerManager::handleImageUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;
    static size_t totalReceived = 0;

    if (!index) {
        totalReceived = 0;
        _hibernationMgr.resetActivityTimer();
        Debugln("Image Upload Started: " + filename);

        if (uploadFile) {
            uploadFile.close();
        }

        if (LittleFS.exists(UPLOADED_IMAGE_PATH)) {
            if (!LittleFS.remove(UPLOADED_IMAGE_PATH)) {
                Debugln("WARNING: Failed to remove existing image.bin!");
            }
        }

        uploadFile = LittleFS.open(UPLOADED_IMAGE_PATH, "w");
        if (!uploadFile || uploadFile.getWriteError() != 0) {
            Debugln("CRITICAL: Failed to open " + String(UPLOADED_IMAGE_PATH) + " for writing.");
            if (uploadFile) uploadFile.close();
            request->send(500, "text/plain", "Failed to open file for upload");
            return;
        }
    }

    if (len) {
        if (!uploadFile) return;
        size_t bytes_written = uploadFile.write(data, len);
        if (bytes_written != len) {
            Debugf("ERROR: File write failed. Expected %u, wrote %u.\n", len, bytes_written);
            uploadFile.close();
            return;
        }
        totalReceived += len;
    }

    if (final) {
        if (!uploadFile) {
            request->send(500, "text/plain", "Internal server error during upload finalization.");
            return;
        }

        uploadFile.flush();
        uploadFile.close();

        File verifyFile = LittleFS.open(UPLOADED_IMAGE_PATH, "r");
        size_t fileSize = verifyFile ? verifyFile.size() : 0;
        if (verifyFile) verifyFile.close();

        Debugf("Image Upload Finished. Total Size: %lu bytes\n", (unsigned long)fileSize);

        if (fileSize == FULL_IMAGE_SIZE) {
            request->send(200, "text/plain", "Image uploaded successfully. Displaying...");
            _newImageReady = true;
            _hibernationMgr.resetActivityTimer();
        } else {
            Debugf("Image size incorrect. Expected %lu, got %lu.\n", (unsigned long)FULL_IMAGE_SIZE, (unsigned long)fileSize);
            request->send(400, "text/plain", "Image size incorrect.");
            LittleFS.remove(UPLOADED_IMAGE_PATH);
        }
    }
}

bool WebServerManager::CaptiveRequestHandler::canHandle(AsyncWebServerRequest *request) const {
    if (WiFi.getMode() == WIFI_AP) {
        if (request->url().startsWith("/index.html") ||
            request->url() == "/style.css" ||
            request->url().startsWith("/js/")) {
            return false;
        }
        if (request->host() != WiFi.softAPIP().toString() && request->host() != (String(WiFiManager::AP_SSID) + ".local")) {
            return true;
        }
    }
    return false;
}

void WebServerManager::CaptiveRequestHandler::handleRequest(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE HTML><html><head><title>Captive Portal</title>");
    response->printf("<meta http-equiv='refresh' content='0; url=http://%s/#config'>", WiFi.softAPIP().toString().c_str());
    response->print("</head><body>Redirecting to configuration page...</body></html>");
    request->send(response);
}
