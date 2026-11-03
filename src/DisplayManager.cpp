// c:\Users\tomgr\dev\PlatformIO\SpectraFrame\src\DisplayManager.cpp
#include "DisplayManager.h"
#include "Debug.h"
#include <vector>               // For qrcodegen
#include <string>               // For std::string for QR codes
#include "HibernationManager.h" // For pausing/resuming hibernation
#include "LittleFS.h"

// External HibernationManager instance (declared in main.cpp)

// Initialize the static RTC data member
RTC_DATA_ATTR DisplayManager::DisplayContentType DisplayManager::rtcDisplayContentType = DisplayManager::DisplayContentType::UNKNOWN;

DisplayManager::DisplayManager(PAINT &paint_instance, UWORD displayWidth, UWORD displayHeight, UWORD orientation,
                               HibernationManager &hibernationMgr, LedManager &ledMgr)
    : _paint_ref(paint_instance),
      _displayWidth(displayWidth),
      _displayHeight(displayHeight),
      _imageBuffer(nullptr),
      _bufferSize((EPD_13IN3E_WIDTH / 2) * EPD_13IN3E_HEIGHT), // 4bpp: 1 byte for 2 pixels
      _epdHardwareInitialized(false),
      _currentContentType(DisplayContentType::UNKNOWN),
      _orientation(orientation)
      , _hibernationManager(hibernationMgr)
      , _ledManager(ledMgr)
{

  _imageBuffer = (UBYTE *)malloc(_bufferSize);
  if (_imageBuffer == NULL)
  {
    Debugln("CRITICAL: Failed to allocate memory for DisplayManager image buffer!");
    // Consider a more robust error handling, like a system halt or specific error state.
    // For now, execution will likely fail later.
    return;
  }
  Debugf("DisplayManager image buffer allocated: %lu bytes\r\n", (unsigned long)_bufferSize);
  // Initialize the Paint library to use this buffer
  Paint_NewImage(_imageBuffer, displayWidth, displayHeight, _orientation, EPD_13IN3E_WHITE);
  // Update logical width/height based on rotation from the global Paint object
  _displayWidth = Paint.Width;
  _displayHeight = Paint.Height;
  Paint_SetScale(7);             // Crucial for 4bpp operation with GUI_Paint
  Paint_Clear(EPD_13IN3E_WHITE); // Clear buffer to EPD white
  this->_currentContentType = DisplayContentType::CLEARED;
  restoreStateFromRTC();
  Debugf("DisplayManager: Content type after RTC restore: %d\r\n", static_cast<int>(_currentContentType));
}

DisplayManager::~DisplayManager()
{
  if (_imageBuffer)
  {
    free(_imageBuffer);
    _imageBuffer = nullptr;
  }
}

void DisplayManager::saveStateToRTC()
{
  rtcDisplayContentType = _currentContentType;
  Debugf("DisplayManager: Saved content type to RTC: %d\r\n", static_cast<int>(rtcDisplayContentType));
}

void DisplayManager::restoreStateFromRTC()
{
  // On very first boot, rtcDisplayContentType will be UNKNOWN.
  // On subsequent boots after deep sleep, it will hold the last saved state.
  _currentContentType = rtcDisplayContentType;
  Debugf("DisplayManager: Restored content type from RTC: %d\r\n", static_cast<int>(_currentContentType));
}

UBYTE *DisplayManager::getDisplayBuffer()
{
  // Primarily for cases where external code might fill the buffer before calling display()
  return _imageBuffer;
}

void DisplayManager::drawTextWithMaxWidth(UWORD x, UWORD y, UWORD maxWidth, const char *text, sFONT *font, UWORD textColor, UWORD bgColor)
{
  if (!text)
    return;

  UWORD currentX = x;
  const char *pText = text;
  char buffer[2] = {0}; // Buffer for a single character

  while (*pText != '\0')
  {
    if (currentX + font->Width > x + maxWidth)
    {
      // Word wrapping would be more complex here, for now, we just truncate or move to next line if handled by caller.
      // This basic version just draws char by char until maxWidth.
      // A more advanced version would break by words.
      break;
    }
    buffer[0] = *pText;
    Paint_DrawString_EN(currentX, y, buffer, font, textColor, bgColor);
    currentX += font->Width;
    pText++;
  }
}

void DisplayManager::showCenteredMessage(const char *title,
                                         const char *messageLine1,
                                         const char *messageLine2,
                                         const char *messageLine3,
                                         sFONT *titleFont,
                                         sFONT *messageFont,
                                         UWORD boxBorderColor,
                                         UWORD boxFillColor,
                                         UWORD titleTextColor,
                                         UWORD messageTextColor)
{
  Paint_Clear(EPD_13IN3E_WHITE); // Clear internal buffer

  // Box dimensions
  UWORD boxWidth = _displayWidth * 0.8;
  UWORD boxPadding = 40;
  UWORD textMaxWidth = boxWidth - 2 * boxPadding;

  // Calculate required height
  UWORD currentY = boxPadding;
  if (title)
  {
    currentY += titleFont->Height + boxPadding / 2;
  }
  if (messageLine1)
  {
    currentY += messageFont->Height + boxPadding / 2;
  }
  if (messageLine2)
  {
    currentY += messageFont->Height + boxPadding / 2;
  }
  if (messageLine3)
  {
    currentY += messageFont->Height + boxPadding / 2;
  }
  currentY += boxPadding; // Bottom padding
  UWORD boxHeight = currentY;

  UWORD boxXstart = (_displayWidth - boxWidth) / 2;
  UWORD boxYstart = (_displayHeight - boxHeight) / 2;
  if (boxYstart < 20)
    boxYstart = 20; // Ensure some top margin if box is too tall

  // Draw the box
  Paint_DrawRectangle(boxXstart, boxYstart, boxXstart + boxWidth - 1, boxYstart + boxHeight - 1, boxFillColor, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  Paint_DrawRectangle(boxXstart, boxYstart, boxXstart + boxWidth - 1, boxYstart + boxHeight - 1, boxBorderColor, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);

  // Draw Title
  currentY = boxYstart + boxPadding;
  if (title)
  {
    UWORD titleWidth = strlen(title) * titleFont->Width;
    UWORD titleX = boxXstart + (boxWidth - titleWidth) / 2;
    Paint_DrawString_EN(titleX, currentY, title, titleFont, boxFillColor, titleTextColor);
    currentY += titleFont->Height + boxPadding;
  }

  // Draw Message Lines (using the helper for potential wrapping, though basic here)
  if (messageLine1)
  {
    drawTextWithMaxWidth(boxXstart + boxPadding, currentY, textMaxWidth, messageLine1, messageFont, boxFillColor, messageTextColor);
    currentY += messageFont->Height + boxPadding / 2;
  }
  if (messageLine2)
  {
    drawTextWithMaxWidth(boxXstart + boxPadding, currentY, textMaxWidth, messageLine2, messageFont, boxFillColor, messageTextColor);
    currentY += messageFont->Height + boxPadding / 2;
  }
  if (messageLine3)
  {
    drawTextWithMaxWidth(boxXstart + boxPadding, currentY, textMaxWidth, messageLine3, messageFont, boxFillColor, messageTextColor);
  }
  // Note: This is a simplified message display. For true multi-line centered text,
  // you'd need to calculate total text block height and adjust boxYstart accordingly,
  // or pre-calculate line breaks.
  _currentContentType = DisplayContentType::MESSAGE;
  this->display();
}

void DisplayManager::drawQRCode(int xOffset, int yOffset, const qrcodegen::QrCode &qr, int scale, UWORD foregroundColor, UWORD backgroundColor)
{
  int qrSize = qr.getSize();
  for (int y = 0; y < qrSize; y++)
  {
    for (int x = 0; x < qrSize; x++)
    {
      UWORD color = qr.getModule(x, y) ? foregroundColor : backgroundColor;
      // Draw a 'scale' x 'scale' square for each module
      Paint_DrawRectangle(xOffset + x * scale, yOffset + y * scale,
                          xOffset + (x + 1) * scale - 1, yOffset + (y + 1) * scale,
                          color, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
  }
}

void DisplayManager::showWiFiAPQRCode(const char *ssid, const char *password, const char *securityType, bool hidden, const char *caption)
{
  Paint_Clear(EPD_13IN3E_WHITE); // Clear internal buffer

  // Format WiFi data string: WIFI:S:<SSID>;T:<WPA|WEP|nopass>;P:<PASSWORD>;H:<true|false>;;
  char wifiData[256]; // Ensure buffer is large enough
  snprintf(wifiData, sizeof(wifiData), "WIFI:S:%s;T:%s;P:%s;H:%s;;",
           ssid, securityType, password, hidden ? "true" : "false");

  Debugf("WiFi QR Data: %s\n", wifiData);

  const qrcodegen::QrCode::Ecc errCorLvl = qrcodegen::QrCode::Ecc::MEDIUM;
  const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(wifiData, errCorLvl);

  int qrModules = qr.getSize();
  if (qrModules == 0 || qrModules > 177)
  { // 177 is max modules, qrcodegen might return 0 on error or too much data
    Debugln("Failed to generate QR code for WiFi.");
    // showCenteredMessage itself calls display(), so this is a complete operation
    showCenteredMessage("Error", "Could not generate", "WiFi QR Code.", nullptr, &Font24, &Font20, EPD_13IN3E_BLACK, EPD_13IN3E_WHITE, EPD_13IN3E_BLACK, EPD_13IN3E_BLACK);
    return;
  }

  int minPixelSize = 800;
  int scale = minPixelSize / qrModules;
  if (scale < 1)
    scale = 1;

  // Ensure QR code + caption fits
  int qrActualPixelWidth = qrModules * scale;
  int qrActualPixelHeight = qrModules * scale;
  int captionHeight = 0;
  sFONT *captionFont = &Font24;

  if (caption)
  {
    captionHeight = captionFont->Height + 40; // Font height + padding
  }

  int totalHeight = qrActualPixelHeight + captionHeight;

  int qrXOffset = (_displayWidth - qrActualPixelWidth) / 2;
  int qrYOffset = (_displayHeight - totalHeight) / 2 - 50;
  if (qrYOffset < 10)
    qrYOffset = 10; // Min top margin

  drawQRCode(qrXOffset, qrYOffset, qr, scale, BLACK, WHITE);

  if (caption)
  {
    UWORD captionTextWidth = strlen(caption) * captionFont->Width;
    UWORD captionX = (_displayWidth - captionTextWidth) / 2;
    UWORD captionY = qrYOffset + qrActualPixelHeight + 50; // 30px padding
    // For Paint_DrawString_EN: BG then FG
    Paint_DrawString_EN(captionX, captionY, caption, captionFont, WHITE, BLACK);
  }

  _currentContentType = DisplayContentType::AP_QR_CODE;
  this->display();
}

void DisplayManager::showURLQRCode(const char *url, const char *caption, const char *additionalInfo)
{
  Debugf("DisplayManager: Generating IP QR Code for %s\n", url);
  Paint_Clear(EPD_13IN3E_WHITE); // Clear internal buffer

  const qrcodegen::QrCode::Ecc errCorLvl = qrcodegen::QrCode::Ecc::MEDIUM;
  const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(url, errCorLvl);

  int qrModules = qr.getSize();
  if (qrModules == 0 || qrModules > 177)
  {
    Debugln("Failed to generate QR code for IP.");
    showCenteredMessage("Error", "Could not generate", "IP QR Code.", nullptr, &Font24, &Font20, BLACK, WHITE, BLACK, BLACK);
    return;
  }

  int minPixelSize = 800;
  int scale = minPixelSize / qrModules;
  if (scale < 1)
    scale = 1;

  int qrActualPixelWidth = qrModules * scale;
  int qrActualPixelHeight = qrModules * scale;

  sFONT *captionFont = &Font24;
  sFONT *infoFont = &Font20;
  int captionHeight = 0;
  int infoHeight = 0;

  char fullCaption[100] = {0};
  if (caption)
  {
    snprintf(fullCaption, sizeof(fullCaption), "%s", caption);
    captionHeight = captionFont->Height + 10; // Font height + padding
  }
  if (additionalInfo)
  {
    infoHeight = infoFont->Height + 10;
  }

  int totalHeight = qrActualPixelHeight + captionHeight + infoHeight;

  int qrXOffset = (_displayWidth - qrActualPixelWidth) / 2;
  int qrYOffset = (_displayHeight - totalHeight) / 2 - 80;
  if (qrYOffset < 10)
    qrYOffset = 10; // Min top margin

  drawQRCode(qrXOffset, qrYOffset, qr, scale, BLACK, WHITE);

  UWORD currentTextY = qrYOffset + qrActualPixelHeight + 50;

  if (caption)
  {
    UWORD captionTextWidth = strlen(fullCaption) * captionFont->Width;
    UWORD captionX = (_displayWidth - captionTextWidth) / 2;
    // For Paint_DrawString_EN: BG then FG
    Paint_DrawString_EN(captionX, currentTextY, fullCaption, captionFont, WHITE, BLACK);
    currentTextY += captionFont->Height + 20;
  }

  if (additionalInfo)
  {
    UWORD infoTextWidth = strlen(additionalInfo) * infoFont->Width;
    UWORD infoX = (_displayWidth - infoTextWidth) / 2;
    // For Paint_DrawString_EN: BG then FG
    Paint_DrawString_EN(infoX, currentTextY, additionalInfo, infoFont, WHITE, BLACK);
  }

  _currentContentType = DisplayContentType::URL_QR_CODE;
  this->display();
}

void DisplayManager::initializeEPDHardwareIfNeeded()
{
  if (!_epdHardwareInitialized)
  {
    Debugln("DisplayManager: Initializing EPD Hardware (EPD_13IN3E_Init)...");
    EPD_13IN3E_Init();
    _epdHardwareInitialized = true;
  }
}

void DisplayManager::sleepEPDHardware()
{
  Debugln("DisplayManager: Putting EPD Hardware to Sleep (EPD_13IN3E_Sleep)...");
  EPD_13IN3E_Sleep();
  _epdHardwareInitialized = false;
}

void DisplayManager::display()
{
  _hibernationManager.pause();
  Debugln("DisplayManager: Sending buffer to EPD (EPD_13IN3E_Display)...");

  // LED indicator for display update
  _ledManager.setMode(LedMode::LED_MODE_DISPLAY_ACTIVE);
  unsigned long startTime = millis();
  while (millis() - startTime < 2000) // Blink green fast for 2 seconds
  {
    _ledManager.update();
    delay(10);
  }

  initializeEPDHardwareIfNeeded();
  _ledManager.setMode(LedMode::LED_MODE_EPD_POWERED);
  DEV_Delay_ms(10);
  EPD_13IN3E_Display(_imageBuffer); // Display the content of the internal buffer
  Debugln("DisplayManager: EPD display complete.");
  DEV_Delay_ms(100);
  sleepEPDHardware();
  _ledManager.setMode(LedMode::LED_MODE_OFF);
  saveStateToRTC();
  _hibernationManager.resumeAndReset();
}

void DisplayManager::displayImageFile(const char *filePath)
{
  File imageFile = LittleFS.open(filePath, "r");
  if (!imageFile)
  {
    Debugf("DisplayManager: Failed to open image file: %s\n", filePath);
    showCenteredMessage("Error", "Could not open", "image file.", filePath); // This will call display()
    return;
  }
  size_t bytesRead = imageFile.read(_imageBuffer, _bufferSize);
  imageFile.close();

  if (bytesRead != _bufferSize)
  {
    Debugf("DisplayManager: Failed to read full image. Read %zu, expected %lu.\n", bytesRead, (unsigned long)_bufferSize);
    showCenteredMessage("Error", "Image file read error.", filePath); // This will call display()
    return;
  }
  Debugf("DisplayManager: Image %s loaded to buffer. Displaying...\n", filePath);
  _currentContentType = DisplayContentType::IMAGE_FILE;
  this->display();
}

void DisplayManager::clearDisplayAndRefresh()
{
  _currentContentType = DisplayContentType::CLEARED;
  Paint_Clear(EPD_13IN3E_WHITE); // Clear the internal buffer
  this->display();               // Display the cleared buffer and sleep EPD
}
