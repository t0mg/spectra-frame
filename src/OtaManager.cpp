#include "OtaManager.h"
#include "HibernationManager.h"
#include "Debug.h"
#include <Update.h> // For U_FLASH, U_SPIFFS, Update object
// LedManager.h is included via OtaManager.h, providing LedManager and LedMode

OtaManager::OtaManager(HibernationManager &hibernationMgr, LedManager &ledMgr)
    : _hibernationMgr(hibernationMgr), _ledManager(ledMgr)
{
}

void OtaManager::handleFirmwareUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{

  static bool otaInProgress = false;
  static size_t otaTotalSize = 0;
  static size_t otaCurrentSize = 0;
  static int otaUpdateCmd = U_FLASH; // Default to firmware update
  static String otaTypeString = "Firmware";

  if (!index)
  { // First chunk of the upload
    otaTotalSize = request->contentLength();
    otaInProgress = true;
    _hibernationMgr.pause(); // Pause hibernation during OTA

    // Determine update type (firmware or filesystem)
    if (request->hasParam("updateType", true))
    {
      String updateTypeParam = request->getParam("updateType", true)->value();
      if (updateTypeParam.equalsIgnoreCase("filesystem"))
      {
        otaUpdateCmd = U_SPIFFS; // Use U_SPIFFS for the partition named "spiffs" (used by LittleFS here)
        otaTypeString = "Filesystem";
      }
      else
      {
        otaUpdateCmd = U_FLASH;
        otaTypeString = "Firmware";
      }
    }
    else
    {
      Debugln("OTA Error: 'updateType' parameter missing. Defaulting to Firmware.");
      otaUpdateCmd = U_FLASH; // Default if parameter is missing
      otaTypeString = "Firmware (defaulted)";
    }

    if (request->hasParam("updateFileSize", true))
    {
      String fileSizeStr = request->getParam("updateFileSize", true)->value();
      otaTotalSize = fileSizeStr.toInt();
      if (otaTotalSize == 0 && filename.length() > 0)
      { // Basic check: if filename exists but size is 0
        Debugf("OTA Error: 'updateFileSize' parameter is 0 for file %s. Full request size: %u\n", filename.c_str(), request->contentLength());
        _ledManager.setMode(LedMode::LED_MODE_ERROR);
        request->send(400, "text/plain", "OTA Error: Reported file size is 0.");
        otaInProgress = false;
        _hibernationMgr.resumeAndReset();
        return;
      }
    }
    else
    {
      Debugf("OTA Error: 'updateFileSize' parameter missing. Full request size: %u\n", request->contentLength());
      _ledManager.setMode(LedMode::LED_MODE_ERROR);
      request->send(400, "text/plain", "OTA Error: updateFileSize parameter missing.");
      otaInProgress = false;
      _hibernationMgr.resumeAndReset();
      return;
    }
    otaCurrentSize = 0; // Reset current size here after otaTotalSize is confirmed

    Debugf("OTA %s Update Started. File: %s, Actual File Size: %u (Full Request: %u)\n", otaTypeString.c_str(), filename.c_str(), otaTotalSize, request->contentLength());
    _ledManager.setMode(LedMode::LED_MODE_BUSY);
    if (!Update.begin(otaTotalSize, otaUpdateCmd))
    {
      Debugf("OTA %s Error: Not enough space or invalid type to begin OTA. Error: ", otaTypeString.c_str());
      Debug(Update.getError());
      Debug(" - ");
      Debugln(Update.errorString());
      _ledManager.setMode(LedMode::LED_MODE_ERROR);
      request->send(500, "text/plain", String("OTA Error: Not enough space - ") + Update.errorString());
      otaInProgress = false;
      _hibernationMgr.resumeAndReset();
      return;
    }
    Debugf("Update.begin(%s) successful.\n", otaTypeString.c_str());
  }

  if (len > 0 && otaInProgress)
  {
    otaCurrentSize += len;
    if (Update.write(data, len) != len)
    {
      Debugf("OTA %s Error: Failed to write chunk. Error: ", otaTypeString.c_str());
      Debug(Update.getError());
      Debug(" - ");
      Debugln(Update.errorString());
      _ledManager.setMode(LedMode::LED_MODE_ERROR);
      request->send(500, "text/plain", String("OTA " + otaTypeString + " Error: Write failed - ") + Update.errorString());
      Update.abort();
      otaInProgress = false;
      _hibernationMgr.resumeAndReset();
      return;
    }

#ifdef USE_DEBUG
    static int lastProgressPercent = -1;
    int currentProgressPercent = (otaTotalSize > 0) ? (otaCurrentSize * 100) / otaTotalSize : 0;

    if (currentProgressPercent / 10 != lastProgressPercent / 10 || currentProgressPercent == 100)
    {
      Debugf("OTA %s Progress: %d%%\n", otaTypeString.c_str(), currentProgressPercent);
      lastProgressPercent = currentProgressPercent;
    }
#endif
  }

  if (final)
  {
    if (otaInProgress)
    { // Proceed only if no errors occurred during chunks
      Debugf("OTA %s Finalizing. Total bytes: %u / %u\n", otaTypeString.c_str(), otaCurrentSize, otaTotalSize);
      // Capture otaTypeString for the lambda, as it might go out of scope if the function returns before the lambda executes.
      String finalOtaTypeString = otaTypeString;
      if (Update.end(true))
      {
        Debugf("OTA %s Update Successful! Scheduling reboot after response.\n", finalOtaTypeString.c_str());
        _ledManager.setMode(LedMode::LED_MODE_OTA_SUCCESS);
        request->onDisconnect([finalOtaTypeString]() {
          Debugf("OTA %s response sent. Rebooting NOW.\n", finalOtaTypeString.c_str());
          ESP.restart();
        });
        request->send(200, "text/plain", finalOtaTypeString + " Update Successful! Device is rebooting.");
      }
      else
      {
        Debugf("OTA %s Error: Failed to end update. Error: ", otaTypeString.c_str());
        Debug(Update.getError());
        Debug(" - ");
        Debugln(Update.errorString());
        _ledManager.setMode(LedMode::LED_MODE_ERROR);
        request->send(500, "text/plain", String("OTA " + otaTypeString + " Error: Finalization failed - ") + Update.errorString());
      }
    }
    otaInProgress = false; // Reset state for next potential OTA
    _hibernationMgr.resumeAndReset();
  }
}
