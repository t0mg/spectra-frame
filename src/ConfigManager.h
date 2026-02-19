#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Debug.h"

struct Config
{
  String ssid;
  String password;
  unsigned long inactivityTimeoutMs;
  int touchThreshold;
  String orientation;
};

class ConfigManager {
public:
    ConfigManager();
    bool load();
    bool save();
    Config& getConfig() { return _config; }

    // Default values
    static const unsigned long DEFAULT_INACTIVITY_TIMEOUT_MS;
    static const int DEFAULT_TOUCH_THRESHOLD;
    static constexpr const char* DEFAULT_ORIENTATION = "portrait";
    static constexpr const char* CONFIG_FILE = "/config.json";

private:
    Config _config;
};

#endif // CONFIG_MANAGER_H
