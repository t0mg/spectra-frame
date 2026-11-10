# SpectraFrame

A battery powered, WiFi-enabled Spectra6 color E-Paper display frame with web configuration, image upload UI, and more.

## Features

- **Color E-Paper Display:** Utilizes a 1600 x 1200, 13.3inch Spectra6 E-Paper display for high-resolution, low-power image display.
- **Web Configuration:** Provides a simple web interface for initial WiFi setup and device configuration.
- **Image Upload:** Allows users to tweak, preview and upload images via the web interface.
- **Over-The-Air (OTA) Updates:** Supports wireless firmware and filesystem updates directly from the web interface.
- **Automatic Hibernation:** SpectraFrame basically deep sleeps all the time, until you wake it up manually when you're ready to change the picture.
- **Touch Wakeup:** Wakes from deep sleep via a touch sensor.
- **LED Status Indicator:** An RGB LED provides visual feedback on the device's state (connecting, busy, idle, error, etc.).

## Hardware

- **Waveshare Spectra6 13.3inch E-Paper with Driver Board:** The bundle version comes with a driver board, which handles the SPI communication and power management for the display. This board connects to the microcontroller via SPI and GPIO pins.
- **Microcontroller:** The code is developed for ESP32-S3.
  - I'm now using an [Unexpected Maker ProS3](https://esp32s3.com/pros3.html), an ESP32-S3 based board featuring 16MB Flash, 8MB PSRAM, built-in LiPo charging with PicoBlade connector, 2 LDOs, and ultra-low deep sleep current. The ProS3's onboard RGB LED uses GPIO 18. 
  - The project can work with any generic ESP32-S3-DevKitC-1 board with PSRAM module and enough Flash storage to store and handle the high resolution image to be displayed. I started this project with an n16r8 clone (16Mb flash, 8Mb RAM) which has its RGB led wired differently from the reference (GPIO 48 instead of GPIO 38).
- **Dedicated Display Buck Converter:** [Adafruit TPS62827 3.3V Buck Converter Breakout (PID 4920)](https://www.adafruit.com/product/4920). The 13.3" Spectra6 display draws high peak current (up to 1.5-2A during full refresh waveforms, particularly on dense dithered images), which overloads typical ESP32 onboard LDO regulators and triggers brownout resets. This high-efficiency synchronous buck converter powers the display directly from battery voltage (`BAT` / 3.7V LiPo). Its Enable (`EN`) pin is driven by `GPIO 16`, allowing display power to be completely cut off during deep sleep to eliminate quiescent current.
- **Power Supply:** 
  - A LiPo battery can be connected directly to the ProS3's PicoBlade battery connector for integrated charging and power management.
  - Alternatively, if the microcontroller board doesn't have built in battery managment, a 3.2V LiFePO4 battery cell can be connected directly to the 3v3 and GND pins thanks to its compatible voltage range (add a Schottky diode to protect from wrong polarity). 
- **Touch Sensor:** It is used to wake up the device from hibernation prior to uploading a new image. I used a piece of wide copper tape and adjusted sensitivity in the code so it could be triggered through the glass and passe-partout.

### Assembly

- I picked up a generic 40×30 cm wooden frame and 27×20 cm ish passe-partout from my local DIY store. The display is 270.4×202.8 mm and I might get a better fitting passe-partout
- The display and ESP are secured on a 40×30 cm, 6mm thick MDF board that was laser cut with [this svg template](doc/lasercut.svg) (blue lines for mark and red for cut)
- The display is locked onto the MDF board with custom 3d printed clamps ([STL](doc/clamp.stl), [STEP](doc/clamp.step)), clamps can be tightened with 2×5mm flathead wood screws into laser cut preholes.
- The ESP was hotglued yay
- I initially used a 18500 LiFePO4 battery cell as power source, and 3D printed battery holder remixed from [this 18650 model](https://www.thingiverse.com/thing:456900). I have since replaced it with a 2000mAh LiPo battery connected to the ProS3's PicoBlade battery connector.

### Wiring

#### Adafruit 4920 (TPS62827 3.3V Buck Converter)

The Adafruit 4920 provides dedicated 3.3V power (up to 2A) to the Waveshare HAT, bypassing the ESP32's onboard LDO to prevent brownout crashes during refresh spikes.

| Module Pin | Connected To | Notes |
|---|---|---|
| VIN | ProS3 `BAT` / Battery (+) | Input voltage (3.0V-5.5V). Connect to LiPo battery (+) or ProS3 `BAT` pin. |
| GND | GND | Common ground with ESP32 and E-Paper HAT. |
| 3V3 | E-Paper `VCC` (Red wire) | Regulated 3.3V output to power the Waveshare HAT. |
| EN | ESP32 GPIO 16 (`EPD_POW_EN_PIN`) | Enable pin (active HIGH, default pulled up to VIN). Driven LOW by ESP32 to cut power during deep sleep. |
| PG | *Not connected* | Power Good output (leave floating). |

#### E-Paper display

| e-Paper Pin | Cable color | Connected To | Notes |
|---|---|---|---|
| VCC | Red | Adafruit 4920 `3V3` | Dedicated 3.3V supply from buck converter (do **not** power from ESP32 3.3V pin!). |
| GND | Black | GND | Common ground. |
| DIN | Blue | ESP32 GPIO 35 (MOSI) | SPI Data line from ESP32 to the display. |
| CLK | Yellow | ESP32 GPIO 36 (SCLK) | Clock line of the SPI bus. |
| CS_M | Orange | ESP32 GPIO 34 | Chip Select for the display driver board. |
| CS_S | Green | ESP32 GPIO 9 | Chip Select for the built-in flash storage. |
| DC | White | ESP32 GPIO 8 | Data/Command select. |
| RST | Purple | ESP32 GPIO 7 | Display reset. |
| BUSY | Brown | ESP32 GPIO 6 | Busy status from display to ESP32 (input). |
| PWR | Grey | ESP32 GPIO 5 (`EPD_PWR_PIN`) | Display onboard power switch control. |

#### Other

- LiFePO4 battery powers the ESP32 directly on its 3v3 pin (if not using ProS3 with LiPo).
- A copper tape is wired to GPIO 4 to act as a touch sensor.
- A high impedance voltage divider is set up and read on GPIO 14 to monitor the battery level (150kΩ R1 and 1MΩ R2 resistors, when not using ProS3's internal VBAT sense).

#### Power Management & Sequencing Notes

- **Dual Power Gating:** Two GPIOs coordinate display power:
  - `GPIO 16` (`EPD_POW_EN_PIN`) controls the Adafruit TPS62827 buck converter `EN` pin to turn the main 3.3V rail on/off.
  - `GPIO 5` (`EPD_PWR_PIN`) controls the Waveshare HAT's onboard power switch.
- **Deep Sleep Power Lock:** On boot and in deep sleep, `GPIO 16` is held `LOW` using `gpio_deep_sleep_hold_en()` to overcome the TPS62827's onboard pull-up resistor on `EN`, ensuring the buck converter stays completely powered down.
- **Back-Powering Prevention:** While the TPS62827 is OFF (`GPIO 16` LOW), all SPI and control pins (`CS_M`, `CS_S`, `DC`, `RST`, `SCK`, `MOSI`) are kept `LOW` and `BUSY` is set to `INPUT` without pull-up to prevent current leaking into the unpowered HAT through internal ESD protection diodes.
- **Bulk Capacitance:** If powering via battery or USB, adding a bulk capacitor (220µF-470µF) across the Waveshare HAT's `VCC` and `GND` lines is recommended to absorb sharp current transients during the `DRF` (Display Refresh) phase on heavily dithered images.
- The ProS3's onboard RGB LED is on GPIO 18 (see pinout reference below).
- The ProS3 has ultra-low deep sleep current, making it ideal for battery-powered operation.
- GPIO 4 should be connected to some kind of capacitive sensor (a strip of copper tape in my case).

### Reference pinouts

<details>
<summary>ProS3 pinout (recommended)</summary>
<br>
The ProS3 is the recommended board for this project. It features an onboard RGB LED on GPIO 18, LiPo charging via PicoBlade connector, VBAT sense pin, and ultra-low deep sleep current.
<br><br>

![ProS3 pinout](doc/pros3_pinout.jpg)

[source](https://esp32s3.com/pros3.html) - [schematic & resources](https://github.com/unexpectedmaker/esp32s3)
</details>
<br>
<details>
<summary>ESP32-S3-DevKitC-1 pinout (alternative)</summary>
<br>

![ESP32-S3-DevKitC-1 pinout](doc/ESP32-S3-DevKitC-1_pinout.png)

[source](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html#hardware-reference)
</details>
<br>
<details>
<summary>YD-ESP32-S3 clone pinout (alternative)</summary>
<br>
This clone was used in earlier iterations. The onboard RGB LED is wired to GPIO 48.
<br><br>

![YD-ESP32-S3](doc/yd-esp32-s3-devkitc-1-clone-pinout.png)
[source](https://mischianti.org/vcc-gnd-studio-yd-esp32-s3-devkitc-1-clone-high-resolution-pinout-and-specs/) - [additional info](https://github.com/rtek1000/YD-ESP32-23)
</details>

## Architecture

The firmware is organized into modular manager classes orchestrated by `main.cpp`. `DisplayManager` handles E-Paper communication and persistent display state across sleep cycles; `HibernationManager` monitors inactivity and coordinates ESP32 deep sleep and display power-down; `LedManager` provides NeoPixel status feedback; and `OtaManager` handles Over-The-Air updates. Web assets and uploaded images are hosted from LittleFS using an asynchronous web server (`ESPAsyncWebServer`) with captive portal support (`DNSServer`).

## User Manual

1. **Initial Setup (AP Mode):**
   - If no WiFi credentials are saved, the frame starts as an Access Point (`SpectraFrameSetup`, password: `spectra1234`).
   - Connect to it and navigate to `http://192.168.4.1` (or use the captive portal) to enter your WiFi network details.

2. **Accessing the Web Interface:**
   - Once connected to WiFi, the frame displays a QR code with its assigned IP address. Open this URL in any browser on the same network.
   - Touching the capacitive sensor will also display the IP QR code on demand.

3. **Uploading an Image:**
   - In the web UI, choose an image, adjust crop or dithering, and upload.
   - The preview uses a manually calibrated palette matching the true Spectra6 colors to accurately simulate the physical display, helping you tune and optimize the image before uploading.

4. **OTA Updates:**
   - Use the OTA section of the web UI to upload new firmware or LittleFS filesystem binaries (`.bin`). The device automatically reboots to apply the update.

5. **Hibernation & Wakeup:**
   - After 5 minutes of inactivity, the device enters deep sleep to conserve battery.
   - Touch the sensor (e.g. copper tape on the frame) to wake it up.

6. **LED Status Indicators:**
   - **Off:** Deep sleep.
   - **Pulsing Green:** AP mode (waiting for configuration).
   - **Pulsing White:** Connected to WiFi and idle.
   - **Pulsing Orange:** Busy (file upload or OTA update).
   - **Static Teal:** Display actively updating.
   - **Static Green:** Update successful (rebooting).
   - **Static Red:** Error.

## Building

This is a PlatformIO project: simply load it, adapt `platformio.ini` to your specific board, and `pio run` or `pio run -t upload`

## Deploying the filesystem image

To build the filesystem image in VS Code, go to PlatformIO panel and run Project Tasks > Build Filesystem Image

To flash it:
- from VS Code: kill all terminals or anything possibly keeping the COM port busy, then in PlatformIO panel, run Project Tasks > Upload Filesystem Image.
- or upload using OTA, by selecting Filesystem and picking littlefs.bin.
- from CLI: `pio run -t uploadfs`

## TODO

- Add battery monitoring
- Add a simple automation/polling system, maybe similar to [this project](https://github.com/Hilko113/epaper_imageclient_spectra6)

## Credits & dependencies

This project is made possible by:

- Waveshare 13.3" Spectra6 E-Paper display [documentation](https://www.waveshare.com/wiki/13.3inch_e-Paper_HAT+_(E)_Manual) 
- Web UI dithering tool was inspired from https://www.e-paper-display.com/dithering.html by Zhang Hui
- Sources redistributed in this repository:
    - Waveshare driver code from https://files.waveshare.com/wiki/13.3inch%20e-Paper%20HAT%2B/13.3inch_e-Paper_E.zip (`DEV_Config.*`, `EPD_13in3e.*`, `GUI_Paint.*`, `fonts.*`) - with small tweaks
    *  [QR Code generator](https://github.com/nayuki/QR-Code-generator) (`qrcodegen.hpp`) by Nayuki
- Dependencies managed from ` platformio.ini`:
    - [FastLED](https://github.com/FastLED/FastLED) 
    - [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
    - [ArduinoJson](https://github.com/bblanchon/ArduinoJson) by Benjamin Blanchon
