#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include "Debug.h"      // Serial debug, go there to enable/disable
#include "EPD_13in3e.h" // Waveshare E-Paper display driver

// --- Thing Managers ---
#include "HibernationManager.h"
#include "DisplayManager.h"
#include "OtaManager.h"
#include "LedManager.h"

// --- Configuration Constants ---
const char *AP_SSID = "SpectraFrameSetup";                                  // Provided in the AP QR code
const char *AP_PASSWORD = "spectra1234";                                    // Provided in the AP QR code
const char *CONFIG_FILE = "/config.json";                                   // LittleFS path for persisting configuration
const char *UPLOADED_IMAGE_PATH = "/image.bin";                             // LittleFS path for uploaded images                                        // 0-255
const unsigned long INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;                  // 5 minutes
const UDOUBLE FULL_IMAGE_SIZE = (EPD_13IN3E_WIDTH / 2) * EPD_13IN3E_HEIGHT; // 1200x1600 4bpp

// --- Global Objects ---
AsyncWebServer server(80);
DNSServer dnsServer;
OtaManager *otaManager = nullptr;
LedManager *ledManager = nullptr;
const byte DNS_PORT = 53;

// --- Configuration Structure ---
struct Config
{
  String ssid;
  String password;
  // Add other settings here, e.g., default display mode, update interval
};
Config currentConfig;

// --- E-Paper State ---
DisplayManager *displayMgr = nullptr; // Global pointer for DisplayManager
bool newImageReadyForDisplay = false; // Flag to signal main loop

// --- Hibernation Configuration ---
HibernationManager hibernationManager(INACTIVITY_TIMEOUT_MS);

// --- Touch Input Configuration for active mode ---
const int TOUCH_PIN = GPIO_NUM_4;
const int TOUCH_THRESHOLD = 58000; // Threshold for touchRead(), lower is more sensitive
bool lastTouchState = false;       // To detect changes in touch state

void setupDisplay()
{
  DEV_Module_Init(); // Initialize GPIOs and SPI for EPD
  if (displayMgr == nullptr)
  {
    displayMgr = new DisplayManager(Paint, EPD_13IN3E_WIDTH, EPD_13IN3E_HEIGHT);
    if (displayMgr->getDisplayBuffer() == nullptr)
    {
      Debugln("CRITICAL: DisplayManager failed to initialize its buffer. Halting.");
      if(ledManager) ledManager->setMode(LedMode::LED_MODE_ERROR);
      while (1)
      {
        delay(500);
      } // Halt
    }
    Debugln("DisplayManager created. Display system ready.");
  }
}

// --- Configuration and WiFi Functions ---
bool loadConfiguration()
{
  if (LittleFS.exists(CONFIG_FILE))
  {
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (!configFile)
    {
      Debugln("Failed to open config file");
      return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error)
    {
      Debug("Failed to parse config file: ");
      Debugln(error.c_str());
      return false;
    }

    currentConfig.ssid = doc["ssid"].as<String>();
    currentConfig.password = doc["password"].as<String>();
    Debug("Config loaded: SSID=");
    Debugln(currentConfig.ssid.c_str());
    return true;
  }
  Debugln("Config file not found.");
  return false;
}

bool saveConfiguration()
{
  JsonDocument doc;
  doc["ssid"] = currentConfig.ssid;
  doc["password"] = currentConfig.password;

  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile)
  {
    Debugln("Failed to open config file for writing");
    return false;
  }

  if (serializeJson(doc, configFile) == 0)
  {
    Debugln("Failed to write to config file");
    configFile.close();
    return false;
  }
  configFile.close();
  Debugln("Config saved.");
  return true;
}

bool connectToWiFi()
{
  if (currentConfig.ssid.length() == 0)
  {
    Debugln("SSID not set in configuration.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(currentConfig.ssid.c_str(), currentConfig.password.c_str());
  Debug("Connecting to WiFi: ");
  Debugln(currentConfig.ssid.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  { // ~15 seconds timeout
    delay(500);
    Debug(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Debug("Connected to WiFi. IP: ");
    Debugln(WiFi.localIP().toString().c_str());
    return true;
  }
  else
  {
    Debugln("Failed to connect to WiFi.");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }
}

void startAPMode()
{
  Debug("Starting AP Mode. SSID: ");
  Debugln(AP_SSID);
  if(ledManager) ledManager->setMode(LedMode::LED_MODE_DISPLAY_ACTIVE);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress apIP = WiFi.softAPIP();
  Debug("AP IP address: ");
  Debugln(apIP.toString().c_str());

  dnsServer.start(DNS_PORT, "*", apIP);
  Debugln("DNS Server started for captive portal.");

  displayMgr->showWiFiAPQRCode(AP_SSID, AP_PASSWORD, "WPA", false, "Scan to connect and configure");
  if(ledManager) ledManager->setMode(LedMode::LED_MODE_AP);
}

// --- Web Server Handlers ---
class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    // If in AP mode and not a request for the AP's IP or config resources, redirect.
    if (WiFi.getMode() == WIFI_AP)
    {
      // Check if the request is for a specific file that should be served directly
      if (request->url().startsWith("/index.html") ||
          request->url() == "/style.css" ||
          request->url().startsWith("/js/") && request->url().endsWith(".js"))
      {
        return false; // Let serveStatic handle these
      }
      // Otherwise, if not accessing by IP, assume captive portal
      if (request->host() != WiFi.softAPIP().toString() && request->host() != (String(AP_SSID) + ".local"))
      {
        return true;
      }
    }
    return false;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE HTML><html><head><title>Captive Portal</title>");
    response->printf("<meta http-equiv='refresh' content='0; url=http://%s/#config'>", WiFi.softAPIP().toString().c_str());
    response->print("</head><body>Redirecting to configuration page...</body></html>");
    request->send(response);
  }
};

void handleSaveConfig(AsyncWebServerRequest *request)
{
  hibernationManager.resetActivityTimer();
  if (request->method() == HTTP_POST)
  {
    String ssid, password;
    bool success = true;
    if (request->hasParam("ssid", true))
    {
      ssid = request->getParam("ssid", true)->value();
    }
    else
    {
      success = false;
    }
    if (request->hasParam("password", true))
    {
      password = request->getParam("password", true)->value();
    } // Password can be optional for open networks

    if (success && ssid.length() > 0)
    {
      currentConfig.ssid = ssid;
      currentConfig.password = password;
      if (saveConfiguration())
      {
        request->send(200, "text/plain", "Configuration saved. Restarting to connect...");
        delay(1000);
        ESP.restart();
      }
      else
      {
        request->send(500, "text/plain", "Error: Failed to save configuration.");
      }
    }
    else
    {
      request->send(400, "text/plain", "Error: SSID is required.");
    }
  }
  else
  {
    request->send(405, "text/plain", "Method Not Allowed");
  }
}

void handleImageUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  static File uploadFile;
  static size_t totalReceived = 0;

  if (!index)
  { // First chunk
    totalReceived = 0;
    hibernationManager.resetActivityTimer(); // Activity detected
    Debugln("Image Upload Started: " + filename);

    // Crucial: If uploadFile is valid (i.e., operator bool() is true),
    // it might be open from a previous incomplete/failed upload. Close it.
    if (uploadFile)
    {
      uploadFile.close();
    }

    // Explicitly check and attempt to remove the old file
    if (LittleFS.exists(UPLOADED_IMAGE_PATH))
    {
      if (!LittleFS.remove(UPLOADED_IMAGE_PATH))
      {
        Debugln("WARNING: Failed to remove existing image.bin!");
      }
    }

    uploadFile = LittleFS.open(UPLOADED_IMAGE_PATH, "w");

    int open_err = 0;
    if (uploadFile)
    {                                        // Only call methods if object is considered valid by boolean operator
      open_err = uploadFile.getWriteError(); // Check stream error state
    }

    // Consider the open failed if the File object is invalid OR if there's an immediate stream error
    if (!uploadFile || open_err != 0)
    {
      Debug("CRITICAL: Failed to open ");
      Debug(UPLOADED_IMAGE_PATH);
      Debugf(" for writing. uploadFile valid: %s, getWriteError(): %d\r\n", uploadFile ? "true" : "false", open_err);
      if (uploadFile)
        uploadFile.close(); // Attempt to close if it was deceptively "valid"
      request->send(500, "text/plain", "Failed to open file for upload");
      return;
    }
  }

  if (len)
  {
    if (!uploadFile)
    { // Should have been caught earlier, but as a safeguard
      Debugln("ERROR: Received data chunk, but uploadFile is not valid! Aborting write.");
      return;
    }
    size_t bytes_written = uploadFile.write(data, len);
    if (bytes_written != len)
    {
      Debugf("ERROR: File write failed. Expected %u, wrote %u. Current size: %lu. Closing.\r\n", len, bytes_written, (unsigned long)uploadFile.size());
      uploadFile.close();                         // Close the file on write error
      int write_err = uploadFile.getWriteError(); // Check error after failed write
      if (write_err != 0)
      {
        Debugf("uploadFile.getWriteError() after write failure: %d\r\n", write_err);
      }
      // The 'final' block will likely report an incorrect size.
      return;
    }
    totalReceived += len;
  }

  if (final)
  {
    if (!uploadFile)
    { // Should have been caught if open failed or write failed and closed
      Debugln("ERROR: Final chunk received, but uploadFile is not valid! Cannot finalize.");
      request->send(500, "text/plain", "Internal server error during upload finalization (file not open).");
      return;
    }

    Debugln("Flushing file before close...");
    uploadFile.flush(); // Ensure all data is written
    uploadFile.close();

    File verifyFile = LittleFS.open(UPLOADED_IMAGE_PATH, "r");
    size_t fileSize = verifyFile ? verifyFile.size() : 0;
    if (verifyFile)
      verifyFile.close();

    Debugf("Image Upload Finished. Total Size: %lu bytes\r\n", (unsigned long)fileSize);

    if (fileSize == FULL_IMAGE_SIZE)
    {
      request->send(200, "text/plain", "Image uploaded successfully. Displaying...");
      delay(100);
      newImageReadyForDisplay = true;
      hibernationManager.resetActivityTimer(); // Reset timer on successful upload
    }
    else
    {
      char msg[100];
      // Use %lu for size_t (unsigned long)
      sprintf(msg, "Image size incorrect. Expected %lu, got %lu.", (unsigned long)FULL_IMAGE_SIZE, (unsigned long)fileSize);
      Debugln(msg);
      request->send(400, "text/plain", msg);
      LittleFS.remove(UPLOADED_IMAGE_PATH); // Clean up incorrect file
    }
  }
}

void setupWebServer()
{
  // Serve main page and reset activity timer
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    hibernationManager.resetActivityTimer();
    request->send(LittleFS, "/index.html", "text/html", false); });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    hibernationManager.resetActivityTimer();
    request->send(LittleFS, "/index.html", "text/html", false); });

  // Serve other static files
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/js/config.js", LittleFS, "/js/config.js");
  server.serveStatic("/js/image-processor.js", LittleFS, "/js/image-processor.js");
  server.serveStatic("/js/main.js", LittleFS, "/js/main.js");
  server.serveStatic("/js/ui.js", LittleFS, "/js/ui.js");
  server.serveStatic("/js/utils.js", LittleFS, "/js/utils.js");
  // API endpoint for configuration
  server.on("/api/config", HTTP_POST, handleSaveConfig);

  // API endpoint for image upload
  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              // This response is sent before the upload body handler is called.
              // A 202 Accepted might be more appropriate if processing takes time.
              // Or send nothing and rely on the final chunk handler to send the final response.
              // For simplicity, let's send a basic 200 OK here.
              // The actual success/failure message will come from handleImageUpload's final block.
              // request->send(200);
            },
            handleImageUpload);
  // Errors during upload (like file open) are handled in handleImageUpload.

  // API endpoint for firmware update
  server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              // This is a placeholder. The actual response is sent by the upload handler.
              // request->send(200, "text/plain", "Firmware upload process started.");
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            {
        if (otaManager) {
            otaManager->handleFirmwareUpload(request, filename, index, data, len, final);
        } else {
            request->send(500, "text/plain", "OTA Manager not initialized.");
        } });

  // Captive portal handler
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // Only active in AP mode

  // Modified onNotFound to also reset timer if it serves something or is an unknown API call
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
  // If in AP mode and not accessing via AP IP, redirect to config (captive portal behavior)
  if (WiFi.getMode() == WIFI_AP && request->host() != WiFi.softAPIP().toString() && request->host() != (String(AP_SSID) + ".local")) {
      AsyncResponseStream *response = request->beginResponseStream("text/html");
      response->print("<!DOCTYPE HTML><html><head><title>Redirecting</title>");
      response->printf("<meta http-equiv='refresh' content='0; url=http://%s/#config'>", WiFi.softAPIP().toString().c_str());
      response->print("</head><body>Redirecting to configuration page...</body></html>");
      request->send(response);
      hibernationManager.resetActivityTimer(); // Reset timer as this is AP interaction
  } else {
      request->send(404, "text/plain", "Not found");
      // Optionally reset timer here too if any 404 could be user probing
  } });

  server.begin();
  Debugln("HTTP server started");
}

void printWakeupReason()
{
  esp_sleep_wakeup_cause_t wakeupReason;

  wakeupReason = esp_sleep_get_wakeup_cause();

  switch (wakeupReason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Debugln("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Debugln("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Debugln("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Debugln("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Debugln("Wakeup caused by ULP program");
    break;
  default:
    Debugf("Wakeup was not caused by deep sleep: %d\n", wakeupReason);
    break;
  }
}

void showWebUIQRcode()
{
  if (displayMgr->getCurrentContentType() != DisplayManager::DisplayContentType::URL_QR_CODE)
  {
    if(ledManager) ledManager->setMode(LedMode::LED_MODE_DISPLAY_ACTIVE);
    Debugln("Displaying IP QR code.");

    String qrDataStr = "http://" + WiFi.localIP().toString();
    String connectedMsg = "Connected to " + currentConfig.ssid + " at " + qrDataStr;
    displayMgr->showURLQRCode(qrDataStr.c_str(), connectedMsg.c_str(), "Scan to open the Web interface and upload a new image.");
    if(ledManager) ledManager->setMode(LedMode::LED_MODE_IDLE);
  }
  else
  {
    Debugln("IP QR code requested but is already displayed: nothing to do.");
  }
  hibernationManager.resetActivityTimer();
}

// --- Arduino Setup and Loop ---
void setup()
{
#ifdef USE_DEBUG
  Serial.begin(115200);
  // while (!Serial)
  //   ;
#endif
  Debugln("SpectraFrame is starting up...");
  printWakeupReason();

  Debugln("Initializing EPD hardware.");
  setupDisplay();

  Debugln("Hibernation: Configuring touch wakeup.");
  touchSleepWakeUpEnable(TOUCH_PIN, 4000); // Lower = more sensitive, adjust as needed
  hibernationManager.pause();              // Pause hibernation manager until WiFi setup is complete

  Debugln("Creating LedManager.");
  ledManager = new LedManager();
  ledManager->begin();
  ledManager->setMode(LedMode::LED_MODE_BUSY);

  Debugln("Creating OtaManager.");
  otaManager = new OtaManager(hibernationManager, *ledManager);

  Debugln("Mounting LittleFS.");
  if (!LittleFS.begin(true)) // Format LittleFS if not yet formatted
  {
    Debugln("LittleFS Mount Failed. Halting.");
    if(ledManager) ledManager->setMode(LedMode::LED_MODE_ERROR);
    while (1)
      ;
  }

  Debugln("Loading configuration and connecting to WiFi.");
  if (loadConfiguration())
  { // True if config.json is found and loaded
    if (connectToWiFi())
    {
      Debugln("WiFi connected with saved credentials.");
      hibernationManager.resumeAndReset();
      if(ledManager) ledManager->setMode(LedMode::LED_MODE_IDLE);
      esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
      if (!LittleFS.exists(UPLOADED_IMAGE_PATH))
      {
        showWebUIQRcode(); // Show QR code for Web UI
      }
    }
    else
    {
      Debugln("Failed to connect with saved WiFi credentials. Starting AP mode. Hiberation timer paused.");
      startAPMode();
    }
  }
  else
  { // Config file not found
    Debugln("Configuration not found. Starting AP mode. Hiberation timer paused.");
    startAPMode();
  }

  setupWebServer();                        // Start web server regardless of mode
  hibernationManager.resetActivityTimer(); // Reset after web server is fully set up

  // Initialize last touch state for active mode polling
  lastTouchState = (touchRead(TOUCH_PIN) > TOUCH_THRESHOLD);

  Debugln("Setup complete.");
}

void loop()
{
  if (WiFi.getMode() == WIFI_AP)
  {
    dnsServer.processNextRequest();
  }

  if (newImageReadyForDisplay)
  {
    newImageReadyForDisplay = false;
    if(ledManager) ledManager->setMode(LedMode::LED_MODE_DISPLAY_ACTIVE);
    Debugln("New image detected in loop. Displaying...");
    displayMgr->displayImageFile(UPLOADED_IMAGE_PATH);
    Debugln("Image display from loop complete.");
    hibernationManager.resetActivityTimer();
    if(ledManager) ledManager->setMode(LedMode::LED_MODE_IDLE);
  }

  // Check for touch input for active mode QR display (only in STA mode and connected)
  if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
  {
    int currentTouchValue = touchRead(TOUCH_PIN);
    // Debugf("Touch read value: %d\r\n", currentTouchValue);
    bool isCurrentlyTouched = (currentTouchValue > TOUCH_THRESHOLD);

    if (isCurrentlyTouched && !lastTouchState) // Rising edge of touch
    {
      lastTouchState = true;                   // Mark as touched to prevent re-triggering
      hibernationManager.resetActivityTimer(); // User interaction
      Debugln("Touch detected in active mode. Displaying URL QR code.");
      if(ledManager) ledManager->setMode(LedMode::LED_MODE_BUSY); // Busy while QR generates
      showWebUIQRcode();
    } // Note: showWebUIQRcode now handles setting IDLE after its display.
    else if (!isCurrentlyTouched && lastTouchState) // Falling edge of touch
    {
      lastTouchState = false; // Mark as released
      if(ledManager) ledManager->setMode(LedMode::LED_MODE_IDLE);
    }

    // Check for deep sleep conditions
    if (hibernationManager.shouldGoToSleep())
    {
      if(ledManager) ledManager->setMode(LedMode::LED_MODE_DISPLAY_ACTIVE);
      if (!LittleFS.exists(UPLOADED_IMAGE_PATH))
      {
        displayMgr->showCenteredMessage("SpectraFrame is sleeping", "No image has been set.", "Touch bottom right corner to wake up.");
      }
      else if (displayMgr->getCurrentContentType() != DisplayManager::DisplayContentType::IMAGE_FILE)
      {
        displayMgr->displayImageFile(UPLOADED_IMAGE_PATH);
      }
      if(ledManager) ledManager->setMode(LedMode::LED_MODE_OFF);
      hibernationManager.goToDeepSleep();
    }
  }
  if(ledManager) ledManager->update();
  delay(100);
}
