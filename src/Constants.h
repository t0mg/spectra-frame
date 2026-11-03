#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "driver/gpio.h"
#include "EPD_13in3e.h"

// Touch Configuration
static constexpr gpio_num_t TOUCH_PIN = GPIO_NUM_4;

// Battery Configuration
#ifndef ARDUINO_PROS3
static constexpr int BATT_ADC_PIN = 14;
// Voltage divider: Assuming R1=150k (upper), R2=1M (lower). V_batt = V_adc * (R1+R2)/R2.
static constexpr float BATT_VOLTAGE_DIVIDER_RATIO = (150000.0 + 1000000.0) / 1000000.0;
#endif

// Image Configuration
static constexpr const char* UPLOADED_IMAGE_PATH = "/image.bin";
static constexpr uint32_t FULL_IMAGE_SIZE = (EPD_13IN3E_WIDTH / 2) * EPD_13IN3E_HEIGHT;

// Display Update Status
enum class DisplayUpdateStatus {
    IDLE = 0,
    UPDATING = 1,
    SUCCESS = 2,
    BROWNOUT = 3,
    ERROR = 4
};

#endif // CONSTANTS_H
