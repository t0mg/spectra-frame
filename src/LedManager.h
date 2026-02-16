// LedManager.h
#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include "Debug.h"

// LED Configuration
#define LED_PIN 18  // ProS3 onboard RGB LED
#define LDO2_PIN 17 // ProS3 LDO2 enable pin
#define NUM_LEDS 1
#define LED_MAX_BRIGHTNESS 255 // Default max brightness

struct LedColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  static LedColor Black() { return {0, 0, 0}; }
  static LedColor Red() { return {255, 0, 0}; }
  static LedColor Green() { return {0, 255, 0}; }
};

enum class LedMode
{
  LED_MODE_OFF,
  LED_MODE_OTA_SUCCESS,
  LED_MODE_DISPLAY_ACTIVE,
  LED_MODE_ERROR,
  LED_MODE_BUSY,
  LED_MODE_IDLE,
  LED_MODE_AP,
  LED_MODE_EPD_POWERED
};

extern class LedManager *ledManager;

class LedManager
{
public:
  LedManager();
  void begin();
  void setMode(LedMode mode);
  void update(); // Call this in the main loop

private:
  LedColor _currentLedColor;
  LedMode _currentLedMode;
  LedColor _targetLedColor;
  uint8_t _ledBpm;
  bool _isBreathingMode;

  void updateBreathingLed();
  void writeLed(LedColor color);
};

#endif // LED_MANAGER_H
