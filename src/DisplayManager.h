#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "GUI_Paint.h"   // Waveshare paint library
#include "fonts.h"       // Waveshare fonts
#include "EPD_13in3e.h"  // Waveshare EPD color definitions and EPD_13IN3E_WIDTH/HEIGHT
#include "HibernationManager.h" // For pausing/resuming hibernation
#include "LedManager.h" // For LED control
#include "qrcodegen.hpp" // QR code library from https://github.com/nayuki/QR-Code-generator

// Forward declaration for QrCode class from the qrcodegen namespace
namespace qrcodegen
{
  class QrCode;
}

class DisplayManager
{
public:
  enum class DisplayContentType
  {
    UNKNOWN = 0, // Default for RTC memory if uninitialized
    IMAGE_FILE,
    AP_QR_CODE,
    URL_QR_CODE,
    MESSAGE,
    CLEARED
  };

  DisplayManager(PAINT &paint_instance, UWORD displayWidth, UWORD displayHeight, UWORD orientation, HibernationManager &hibernationMgr, LedManager &ledMgr);
  ~DisplayManager();

  void showCenteredMessage(const char *title,
                           const char *messageLine1,
                           const char *messageLine2 = nullptr,
                           const char *messageLine3 = nullptr,
                           sFONT *titleFont = &Font24,
                           sFONT *messageFont = &Font20,
                           UWORD boxBorderColor = EPD_13IN3E_BLACK,
                           UWORD boxFillColor = EPD_13IN3E_WHITE,
                           UWORD titleTextColor = EPD_13IN3E_BLACK,
                           UWORD messageTextColor = EPD_13IN3E_BLACK);

  void showWiFiAPQRCode(const char *ssid, const char *password, const char *securityType = "WPA", bool hidden = false, const char *caption = "Scan to Connect WiFi");
  void showURLQRCode(const char *url, const char *caption = "", const char *additionalInfo = nullptr);

  bool displayImageFile(const char *filePath);
  void clearDisplayAndRefresh();                                  // Clears buffer to WHITE, displays, then sleeps EPD
  UBYTE *getDisplayBuffer();                                      // Allows external access to the buffer if absolutely needed before a display() call
  DisplayContentType getCurrentContentType() const { return _currentContentType; } // Get the current type of content being displayed

private:
  PAINT &_paint_ref; // Reference to the global Paint object
  UWORD _displayWidth;
  UWORD _displayHeight;
  UWORD _orientation;
  UBYTE *_imageBuffer;
  const UDOUBLE _bufferSize;
  bool _epdHardwareInitialized;
  DisplayContentType _currentContentType;
  HibernationManager &_hibernationManager; // Reference to HibernationManager
  LedManager &_ledManager; // Reference to LedManager

  static RTC_DATA_ATTR DisplayContentType rtcDisplayContentType; // Persists across deep sleep

  // Internal EPD and display methods
  void initializeEPDHardwareIfNeeded();
  void sleepEPDHardware();
  bool display(); // Sends current _imageBuffer to EPD, then sleeps EPD

  // Helper methods
  void drawQRCode(int xOffset, int yOffset, const qrcodegen::QrCode &qr, int scale, UWORD foregroundColor, UWORD backgroundColor);
  void drawTextWithMaxWidth(UWORD x, UWORD y, UWORD maxWidth, const char *text, sFONT *font, UWORD textColor, UWORD bgColor);
  void saveStateToRTC();
  void restoreStateFromRTC();
};
#endif // DISPLAY_MANAGER_H
