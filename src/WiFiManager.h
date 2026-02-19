#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <DNSServer.h>
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "LedManager.h"

class WiFiManager {
public:
    WiFiManager(ConfigManager& configMgr, DisplayManager& displayMgr, LedManager& ledMgr);
    bool connect();
    void startAP();
    void update(); // For DNS server
    bool isConnected();
    String getIP();

    static constexpr const char *AP_SSID = "SpectraFrameSetup";
    static constexpr const char *AP_PASSWORD = "spectra1234";

private:
    ConfigManager& _configMgr;
    DisplayManager& _displayMgr;
    LedManager& _ledMgr;
    DNSServer _dnsServer;

    static const byte DNS_PORT = 53;
};

#endif // WIFI_MANAGER_H
