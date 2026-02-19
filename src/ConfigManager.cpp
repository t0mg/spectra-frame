#include "ConfigManager.h"

const unsigned long ConfigManager::DEFAULT_INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;
const int ConfigManager::DEFAULT_TOUCH_THRESHOLD = 52000;

ConfigManager::ConfigManager() {
    _config.ssid = "";
    _config.password = "";
    _config.inactivityTimeoutMs = DEFAULT_INACTIVITY_TIMEOUT_MS;
    _config.touchThreshold = DEFAULT_TOUCH_THRESHOLD;
    _config.orientation = DEFAULT_ORIENTATION;
}

bool ConfigManager::load() {
    if (LittleFS.exists(CONFIG_FILE)) {
        File configFile = LittleFS.open(CONFIG_FILE, "r");
        if (!configFile) {
            Debugln("Failed to open config file");
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, configFile);
        configFile.close();

        if (error) {
            Debug("Failed to parse config file: ");
            Debugln(error.c_str());
            return false;
        }

        _config.ssid = doc["ssid"].as<String>();
        _config.password = doc["password"].as<String>();
        _config.inactivityTimeoutMs = doc["inactivityTimeoutMs"] | DEFAULT_INACTIVITY_TIMEOUT_MS;
        _config.touchThreshold = doc["touchThreshold"] | DEFAULT_TOUCH_THRESHOLD;
        _config.orientation = doc["orientation"] | DEFAULT_ORIENTATION;

        Debug("Config loaded: SSID=");
        Debugln(_config.ssid.c_str());
        Debugf("  Inactivity Timeout: %lu ms, Touch Threshold: %d, Orientation: %s\n",
               _config.inactivityTimeoutMs, _config.touchThreshold, _config.orientation.c_str());
        return true;
    }
    Debugln("Config file not found. Using defaults.");
    return false;
}

bool ConfigManager::save() {
    JsonDocument doc;
    doc["ssid"] = _config.ssid;
    doc["password"] = _config.password;
    doc["inactivityTimeoutMs"] = _config.inactivityTimeoutMs;
    doc["touchThreshold"] = _config.touchThreshold;
    doc["orientation"] = _config.orientation;

    File configFile = LittleFS.open(CONFIG_FILE, "w");
    if (!configFile) {
        Debugln("Failed to open config file for writing");
        return false;
    }

    if (serializeJson(doc, configFile) == 0) {
        Debugln("Failed to write to config file");
        configFile.close();
        return false;
    }
    configFile.close();
    Debugln("Config saved.");
    return true;
}
