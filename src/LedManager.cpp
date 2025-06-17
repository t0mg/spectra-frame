#include "LedManager.h"

LedManager::LedManager() : _currentLedMode(LedMode::LED_MODE_OFF), _targetLedColor(CRGB::Black), _ledBpm(60), _isBreathingMode(false)
{
  // Constructor
}

void LedManager::begin()
{
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_MAX_BRIGHTNESS); // Set global max brightness for FastLED
  setMode(LedMode::LED_MODE_OFF);            // Start with LEDs off
  Debugln("LedManager initialized.");
}

void LedManager::updateBreathingLed()
{
  uint8_t brightness = beatsin8(_ledBpm, 0, 255); // Use FastLED's full range for beatsin8
  leds[0] = _targetLedColor;
  leds[0].nscale8(brightness);
  FastLED.show();
}

void LedManager::setMode(LedMode mode)
{
  _currentLedMode = mode;
  _isBreathingMode = false; // Reset breathing mode flag

  Debugf("LedManager: Setting mode to %d\r\n", static_cast<int>(mode));

  switch (mode)
  {
  case LedMode::LED_MODE_OFF:
    leds[0] = CRGB::Black;
    FastLED.show();
    break;
  case LedMode::LED_MODE_OTA_SUCCESS: // Static Green
    _targetLedColor = CRGB::Green;
    leds[0] = _targetLedColor;
    // FastLED.setBrightness will cap this if LED_MAX_BRIGHTNESS is lower than 255
    FastLED.show();
    break;
  case LedMode::LED_MODE_DISPLAY_ACTIVE: // Static Teal
    _targetLedColor = CRGB::Teal;
    leds[0] = _targetLedColor;
    FastLED.show();
    break;

  // Breathing modes: set color and BPM
  case LedMode::LED_MODE_ERROR:
    _targetLedColor = CRGB::Red;
    _ledBpm = 240; // Very fast pulse
    _isBreathingMode = true;
    break;
  case LedMode::LED_MODE_BUSY:
    _targetLedColor = CRGB::Orange;
    _ledBpm = 120; // Fast pulse
    _isBreathingMode = true;
    break;
  case LedMode::LED_MODE_IDLE:
    _targetLedColor = CRGB::White;
    _ledBpm = 60; // Slow pulse
    _isBreathingMode = true;
    break;
  case LedMode::LED_MODE_AP:
    _targetLedColor = CRGB::Green; // Distinct Green for AP mode
    _ledBpm = 60;                  // Slow pulse
    _isBreathingMode = true;
    break;
  default:
    leds[0] = CRGB::Black;
    FastLED.show();
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
  // For static modes, FastLED.show() was already called in setMode.
  // No continuous update needed unless the mode itself implies animation.
}
