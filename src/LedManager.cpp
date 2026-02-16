#include "LedManager.h"

LedManager *ledManager = nullptr;

LedManager::LedManager() : _currentLedMode(LedMode::LED_MODE_OFF), _currentLedColor(LedColor::Black()), _targetLedColor(LedColor::Black()), _ledBpm(60), _isBreathingMode(false)
{
  ledManager = this;
}

void LedManager::begin()
{
  // On ProS3, GPIO 17 enables LDO2 which powers the onboard LED
  pinMode(LDO2_PIN, OUTPUT);
  digitalWrite(LDO2_PIN, HIGH);
  Debugln("LDO2 enabled for LED power.");

  setMode(LedMode::LED_MODE_OFF);            // Start with LEDs off
  Debugln("LedManager initialized.");
}

void LedManager::writeLed(LedColor color)
{
  _currentLedColor = color;
  neopixelWrite(LED_PIN, color.r, color.g, color.b);
}

void LedManager::updateBreathingLed()
{
  // Simple beatsin8 replacement using sine wave
  // beatsin8(bpm, low, high)
  float phase = (millis() * _ledBpm * 2.0 * PI) / 60000.0;
  uint8_t brightness = (uint8_t)((sin(phase) + 1.0) * 127.5);
  
  LedColor dimmedColor;
  dimmedColor.r = (_targetLedColor.r * brightness) / 255;
  dimmedColor.g = (_targetLedColor.g * brightness) / 255;
  dimmedColor.b = (_targetLedColor.b * brightness) / 255;
  
  writeLed(dimmedColor);
}

void LedManager::setMode(LedMode mode)
{
  _currentLedMode = mode;
  _isBreathingMode = false; // Reset breathing mode flag

  Debugf("LedManager: Setting mode to %d\r\n", static_cast<int>(mode));

  switch (mode)
  {
  case LedMode::LED_MODE_OFF:
    writeLed(LedColor::Black());
    break;
  case LedMode::LED_MODE_OTA_SUCCESS: // Static Green
    _targetLedColor = LedColor::Green();
    writeLed(_targetLedColor);
    break;

  // Breathing modes: set color and BPM
  case LedMode::LED_MODE_DISPLAY_ACTIVE: // Fast green blink
    _targetLedColor = LedColor::Green();
    _ledBpm = 180; // Fast pulse
    _isBreathingMode = true;
    break;
  case LedMode::LED_MODE_ERROR:
    _targetLedColor = LedColor::Red();
    writeLed(_targetLedColor);
    break;
  case LedMode::LED_MODE_BUSY:
    _targetLedColor = LedColor::Red();
    _ledBpm = 180; // Fast pulse
    _isBreathingMode = true;
    break;
  case LedMode::LED_MODE_IDLE:
    _targetLedColor = LedColor::Red();
    _ledBpm = 30; // Slow pulse
    _isBreathingMode = true;
    break;
  case LedMode::LED_MODE_AP:
    _targetLedColor = LedColor::Green(); // Distinct Green for AP mode
    _ledBpm = 30;
    _isBreathingMode = true;
    break;
  case LedMode::LED_MODE_EPD_POWERED:
    _targetLedColor = LedColor::Red();
    _ledBpm = 30; // Slow pulse
    _isBreathingMode = true;
    break;
  default:
    writeLed(LedColor::Black());
    break;
  }

  if (_isBreathingMode)
  {
    // For breathing modes, updateBreathingLed() in update() will handle animation.
    // Show the first frame immediately.
    updateBreathingLed();
  }
}

void LedManager::update()
{
  if (_isBreathingMode)
  {
    updateBreathingLed();
  }
}
