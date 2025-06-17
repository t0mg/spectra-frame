// LedManager.h
#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <FastLED.h>
#include "Debug.h"

// LED Configuration
#define LED_PIN 48
#define NUM_LEDS 1
#define LED_MAX_BRIGHTNESS 255 // Default max brightness

enum class LedMode
{
  LED_MODE_OFF,
  LED_MODE_OTA_SUCCESS,
  LED_MODE_DISPLAY_ACTIVE,
  LED_MODE_ERROR,
  LED_MODE_BUSY,
  LED_MODE_IDLE,
  LED_MODE_AP
};

class LedManager
{
public:
  LedManager();
  void begin();
  void setMode(LedMode mode);
  void update(); // Call this in the main loop

private:
  CRGB leds[NUM_LEDS];
  LedMode _currentLedMode;
  CRGB _targetLedColor;
  uint8_t _ledBpm;
  bool _isBreathingMode;

  void updateBreathingLed();
};

#endif // LED_MANAGER_H
