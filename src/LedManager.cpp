#include "LedManager.h"

LedManager::LedManager() : _currentLedMode(LedMode::LED_MODE_OFF), _offMS(0), _fadeInMS(0), _onMS(0), _fadeOutMS(0), _patternStartTime(0)
{
}

void LedManager::begin()
{
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 0);

  setMode(LedMode::LED_MODE_OFF);
  Debugln("LedManager initialized for monochrome external LED.");
}

void LedManager::writeLed(uint8_t brightness)
{
  analogWrite(LED_PIN, brightness);
}

void LedManager::setBreathingPattern(uint32_t offMS, uint32_t fadeInMS, uint32_t onMS, uint32_t fadeOutMS)
{
  _offMS = offMS;
  _fadeInMS = fadeInMS;
  _onMS = onMS;
  _fadeOutMS = fadeOutMS;
  _patternStartTime = millis();
}

void LedManager::setMode(LedMode mode)
{
  _currentLedMode = mode;

  Debugf("LedManager: Setting mode to %d\r\n", static_cast<int>(mode));

  switch (mode)
  {
  case LedMode::LED_MODE_OFF:
    setBreathingPattern(0, 0, 0, 0);
    break;
  case LedMode::LED_MODE_OTA_SUCCESS:
    setBreathingPattern(0, 0, 1, 0); // Static ON
    break;
  case LedMode::LED_MODE_DISPLAY_ACTIVE:
    setBreathingPattern(100, 200, 100, 200); // Fast pulse
    break;
  case LedMode::LED_MODE_ERROR:
    setBreathingPattern(100, 0, 100, 0); // Fast blink
    break;
  case LedMode::LED_MODE_BUSY:
    setBreathingPattern(200, 400, 200, 400); // Medium pulse
    break;
  case LedMode::LED_MODE_IDLE:
    setBreathingPattern(1000, 1000, 200, 1000); // Slow breathing
    break;
  case LedMode::LED_MODE_AP:
    setBreathingPattern(500, 500, 500, 500); // Steady breathing
    break;
  case LedMode::LED_MODE_EPD_POWERED:
    setBreathingPattern(0, 0, 1, 0); // Static ON
    break;
  default:
    setBreathingPattern(0, 0, 0, 0);
    break;
  }
}

void LedManager::update()
{
  uint32_t totalPeriod = _offMS + _fadeInMS + _onMS + _fadeOutMS;
  if (totalPeriod == 0)
  {
    writeLed(0);
    return;
  }

  // Special case for static ON
  if (_offMS == 0 && _fadeInMS == 0 && _fadeOutMS == 0 && _onMS > 0)
  {
    writeLed(255);
    return;
  }

  uint32_t elapsed = (millis() - _patternStartTime) % totalPeriod;
  uint8_t brightness = 0;

  if (elapsed < _offMS)
  {
    brightness = 0;
  }
  else if (elapsed < _offMS + _fadeInMS)
  {
    brightness = (uint8_t)((elapsed - _offMS) * 255 / _fadeInMS);
  }
  else if (elapsed < _offMS + _fadeInMS + _onMS)
  {
    brightness = 255;
  }
  else if (_fadeOutMS > 0)
  {
    brightness = (uint8_t)(255 - (elapsed - (_offMS + _fadeInMS + _onMS)) * 255 / _fadeOutMS);
  }
  else
  {
    brightness = 0;
  }

  writeLed(brightness);
}
