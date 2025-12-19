# TouchClock Firmware

TouchClock is an open-source smart desk clock for ESP32 microcontrollers. It features:
- Color TFT display (TFT_eSPI)
- Touchscreen controls (XPT2046)
- RGB LED ambient light
- Wi-Fi connectivity (provisioning & captive portal)
- Ambient light sensing
- NTP time syncing
- Weather updates via internet

## Features
- Wi-Fi setup and auto-reconnection
- Local time display and weather data
- Touch navigation for settings and info
- Automatic screen brightness via ambient sensor
- RGB LED notifications

## Hardware Requirements
- ESP32 microcontroller
- 320x240 TFT display (ILI9341 controller)
- Touchscreen (XPT2046)
- RGB LED
- Ambient light sensor

## Building & Flashing
Flash using [PlatformIO](https://platformio.org/) for VSCode. All dependencies are managed via `platformio.ini`.

```bash
# Build the firmware
pio run
# Upload to device
pio run --target upload
```

## Source Files

src/
├── main.cpp              # Main application & setup
├── AppVersion.h          # Version management
├── DisplayManager.h      # Display control (TFT_eSPI)
├── LightSensorManager.h  # Ambient light sensor logic
├── NetworkManager.h      # Wi-Fi provisioning & captive portal
├── RGBLedManager.h       # RGB LED control
├── TimeManager.h         # NTP sync & time formatting
├── TouchManager.h        # Touchscreen handling (XPT2046)
├── WeatherManager.h      # Weather data fetch & display
├── weather_icons.h       # Bitmap assets for weather display
