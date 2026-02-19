#include <Arduino.h>
#include <LittleFS.h>
#include "Debug.h"      // Serial debug, go there to enable/disable
#include "EPD_13in3e.h" // Waveshare E-Paper display driver
#include "driver/gpio.h"

#ifdef ARDUINO_PROS3
#include <UMS3.h> // UMS3 Helper library for ProS3-specific features
UMS3 ums3;
#endif

// --- Thing Managers ---
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "WebServerManager.h"
#include "HibernationManager.h"
#include "DisplayManager.h"
#include "OtaManager.h"
#include "LedManager.h"
#include "Constants.h"

// --- Global Objects ---
ConfigManager configMgr;
LedManager *ledManager = nullptr;
DisplayManager *displayMgr = nullptr;
HibernationManager *hibernationMgr = nullptr;
OtaManager *otaManager = nullptr;
WiFiManager *wifiMgr = nullptr;
WebServerManager *webServerMgr = nullptr;

// Configuration constants
const int TOUCH_WAKEUP_DELTA = 2500; // Delta for wakeup (approx 1-5k)
const unsigned long TOUCH_WAKE_DEBOUNCE = 2000; // Debounce time in ms

bool lastTouchState = false; // To detect changes in touch state

void printResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    Debugf("Reset reason: %d - ", reason);
    switch (reason) {
        case ESP_RST_UNKNOWN:    Debugln("Unknown"); break;
        case ESP_RST_POWERON:    Debugln("Power on"); break;
        case ESP_RST_EXT:        Debugln("External pin"); break;
        case ESP_RST_SW:         Debugln("Software"); break;
        case ESP_RST_PANIC:      Debugln("Panic/exception"); break;
        case ESP_RST_INT_WDT:    Debugln("Interrupt watchdog"); break;
        case ESP_RST_TASK_WDT:   Debugln("Task watchdog"); break;
        case ESP_RST_WDT:        Debugln("Other watchdog"); break;
        case ESP_RST_DEEPSLEEP:  Debugln("Exiting deep sleep"); break;
        case ESP_RST_BROWNOUT:   Debugln("Brownout"); break;
        case ESP_RST_SDIO:       Debugln("SDIO"); break;
        default:                 Debugln("Other"); break;
    }
}

void printWakeupReason() {
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
    switch (wakeupReason) {
        case ESP_SLEEP_WAKEUP_EXT0:      Debugln("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1:      Debugln("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER:     Debugln("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:  Debugln("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP:       Debugln("Wakeup caused by ULP program"); break;
        default:                         Debugf("Wakeup was not caused by deep sleep: %d\n", wakeupReason); break;
    }
}

void showWebUIQRcode() {
    if (displayMgr->getCurrentContentType() != DisplayManager::DisplayContentType::URL_QR_CODE) {
        Debugln("Displaying IP QR code.");
        String ip = wifiMgr->getIP();
        String qrDataStr = "http://" + ip;
        String connectedMsg = "Connected to " + configMgr.getConfig().ssid + " at " + qrDataStr;
        displayMgr->showURLQRCode(qrDataStr.c_str(), connectedMsg.c_str(), "Scan to open the Web interface and upload a new image.");
    } else {
        Debugln("IP QR code requested but is already displayed: nothing to do.");
    }
    hibernationMgr->resetActivityTimer();
}

void setup() {
    Serial.begin(115200);

    // Set up the Adafruit 4920 enable pin to lock during hibernation
    gpio_hold_dis(EPD_POW_EN_PIN);
    gpio_reset_pin(EPD_POW_EN_PIN);
    gpio_set_direction(EPD_POW_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(EPD_POW_EN_PIN, 0);

#ifdef ARDUINO_PROS3
    ums3.begin();
#endif

    ledManager = new LedManager();
    ledManager->begin();

    printResetReason();
    if (esp_reset_reason() == ESP_RST_BROWNOUT) {
        Debugln("Brownout detected! Signaling with static LED for 5s.");
        ledManager->setMode(LedMode::LED_MODE_ERROR);
        delay(5000);
    }
    Debugln("SpectraFrame is starting up...");
    printWakeupReason();

    Debugln("Mounting LittleFS.");
    if (!LittleFS.begin(true)) {
        Debugln("LittleFS Mount Failed. Halting.");
        ledManager->setMode(LedMode::LED_MODE_ERROR);
        while (1);
    }

    Debugln("Loading configuration.");
    configMgr.load();

    Debugln("Initializing EPD hardware.");
    DEV_Module_Init();
    hibernationMgr = new HibernationManager(configMgr.getConfig().inactivityTimeoutMs);
    displayMgr = new DisplayManager(Paint, EPD_13IN3E_WIDTH, EPD_13IN3E_HEIGHT,
                                  configMgr.getConfig().orientation == "portrait" ? ROTATE_0 : ROTATE_90,
                                  *hibernationMgr, *ledManager);

    if (displayMgr->getDisplayBuffer() == nullptr) {
        Debugln("CRITICAL: DisplayManager failed to initialize its buffer. Halting.");
        ledManager->setMode(LedMode::LED_MODE_ERROR);
        while (1) delay(500);
    }

    hibernationMgr->pause();
    ledManager->setMode(LedMode::LED_MODE_BUSY);

    otaManager = new OtaManager(*hibernationMgr, *ledManager);
    wifiMgr = new WiFiManager(configMgr, *displayMgr, *ledManager);
    webServerMgr = new WebServerManager(configMgr, *displayMgr, *hibernationMgr, *ledManager, *otaManager);

    touchSleepWakeUpEnable(TOUCH_PIN, TOUCH_WAKEUP_DELTA);

    Debugln("Connecting to WiFi.");
    if (configMgr.getConfig().ssid.length() > 0) {
        if (wifiMgr->connect()) {
            Debugln("WiFi connected with saved credentials.");
            hibernationMgr->resumeAndReset();
            ledManager->setMode(LedMode::LED_MODE_IDLE);
            if (!LittleFS.exists(UPLOADED_IMAGE_PATH)) {
                showWebUIQRcode();
            }
        } else {
            Debugln("Failed to connect with saved WiFi credentials. Starting AP mode.");
            wifiMgr->startAP();
        }
    } else {
        Debugln("Configuration not found or SSID not set. Starting AP mode.");
        wifiMgr->startAP();
    }

    webServerMgr->begin();
    hibernationMgr->resetActivityTimer();

    lastTouchState = (touchRead(TOUCH_PIN) > configMgr.getConfig().touchThreshold);

    Debugln("Setup complete.");
}

void loop() {
    wifiMgr->update();

    if (webServerMgr->isNewImageReady()) {
        webServerMgr->clearNewImageReady();
        Debugln("New image detected in loop. Displaying...");
        displayMgr->displayImageFile(UPLOADED_IMAGE_PATH);
        Debugln("Image display from loop complete.");
        hibernationMgr->resetActivityTimer();
        ledManager->setMode(LedMode::LED_MODE_IDLE);
    }

    // Check for touch input for active mode QR display
    if (WiFi.getMode() == WIFI_STA && wifiMgr->isConnected()) {
        int currentTouchValue = touchRead(TOUCH_PIN);
        bool isCurrentlyTouched = (currentTouchValue > configMgr.getConfig().touchThreshold);

        if (isCurrentlyTouched && !lastTouchState) {
            lastTouchState = true;
            if (millis() > TOUCH_WAKE_DEBOUNCE) {
                hibernationMgr->resetActivityTimer();
                Debugln("Touch detected in active mode. Displaying URL QR code.");
                ledManager->setMode(LedMode::LED_MODE_BUSY);
                showWebUIQRcode();
            }
        } else if (!isCurrentlyTouched && lastTouchState) {
            lastTouchState = false;
            if (millis() > TOUCH_WAKE_DEBOUNCE) ledManager->setMode(LedMode::LED_MODE_IDLE);
        }

        if (hibernationMgr->shouldGoToSleep()) {
            if (!LittleFS.exists(UPLOADED_IMAGE_PATH)) {
                displayMgr->showCenteredMessage("SpectraFrame is sleeping", "No image has been set.", "Touch bottom right corner to wake up.");
            } else if (displayMgr->getCurrentContentType() != DisplayManager::DisplayContentType::IMAGE_FILE) {
                displayMgr->displayImageFile(UPLOADED_IMAGE_PATH);
            }
            ledManager->setMode(LedMode::LED_MODE_OFF);
            hibernationMgr->goToDeepSleep();
        }
    }

    ledManager->update();
    delay(100);
}
