#include "WiFiManager.h"

WiFiManager::WiFiManager(ConfigManager& configMgr, DisplayManager& displayMgr, LedManager& ledMgr)
    : _configMgr(configMgr), _displayMgr(displayMgr), _ledMgr(ledMgr) {
}

bool WiFiManager::connect() {
    Config& config = _configMgr.getConfig();
    if (config.ssid.length() == 0) {
        Debugln("SSID not set in configuration.");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid.c_str(), config.password.c_str());
    Debug("Connecting to WiFi: ");
    Debugln(config.ssid.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) { // ~15 seconds timeout
        delay(500);
        Debug(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Debug("Connected to WiFi. IP: ");
        Debugln(WiFi.localIP().toString().c_str());
        return true;
    } else {
        Debugln("Failed to connect to WiFi.");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }
}

void WiFiManager::startAP() {
    Debug("Starting AP Mode. SSID: ");
    Debugln(AP_SSID);
    _ledMgr.setMode(LedMode::LED_MODE_DISPLAY_ACTIVE);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    IPAddress apIP = WiFi.softAPIP();
    Debug("AP IP address: ");
    Debugln(apIP.toString().c_str());

    _dnsServer.start(DNS_PORT, "*", apIP);
    Debugln("DNS Server started for captive portal.");

    _displayMgr.showWiFiAPQRCode(AP_SSID, AP_PASSWORD, "WPA", false, "Scan to connect and configure");
    _ledMgr.setMode(LedMode::LED_MODE_AP);
}

void WiFiManager::update() {
    if (WiFi.getMode() == WIFI_AP) {
        _dnsServer.processNextRequest();
    }
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIP() {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}
