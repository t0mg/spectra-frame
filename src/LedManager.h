// LedManager.h
#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include "Debug.h"
#include "DEV_Config.h"

// LED Configuration
#define LED_PIN 13  // External monochrome LED

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

class LedManager
{
public:
  LedManager();
  void begin();
  void setMode(LedMode mode);
  void update(); // Call this in the main loop

  /**
   * Set a breathing pattern for the monochrome LED.
   * @param offMS Duration in milliseconds the LED is completely off.
   * @param fadeInMS Duration in milliseconds the LED takes to fade from 0 to 255.
   * @param onMS Duration in milliseconds the LED is at full brightness.
   * @param fadeOutMS Optional: Duration in milliseconds the LED takes to fade from 255 to 0.
   */
  void setBreathingPattern(uint32_t offMS, uint32_t fadeInMS, uint32_t onMS, uint32_t fadeOutMS = 0);

private:
  LedMode _currentLedMode;
  uint32_t _offMS;
  uint32_t _fadeInMS;
  uint32_t _onMS;
  uint32_t _fadeOutMS;
  uint32_t _patternStartTime;

  void writeLed(uint8_t brightness);
};

#endif // LED_MANAGER_H
