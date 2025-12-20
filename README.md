# TouchClock Firmware

TouchClock is an open-source smart desk clock for ESP32 microcontrollers. It features:
- Color TFT display (TFT_eSPI)
- Touchscreen controls (XPT2046)
- RGB LED ambient lighting
- Wi-Fi connectivity (provisioning & captive portal)
- Ambient light sensing with auto screen-off
- NTP time synchronization
- Hourly Westminster chimes (Big Ben)
- Weather forecast with temperature display
- Rolling 12-hour weather outlook

## Hardware Requirements
- ESP32-WROOM-32 (ESP32-2432S028 / Cheap Yellow Display)
- 320x240 TFT display (ILI9341 controller)
- Touchscreen (XPT2046)
- RGB LED
- Ambient light sensor
- Speaker (GPIO26/DAC)

See [HARDWARE.md](HARDWARE.md) for detailed pinout and configuration.

## Building & Flashing
Flash using [PlatformIO](https://platformio.org/) for VSCode. All dependencies are managed via `platformio.ini`.

```bash
# Build the firmware
pio run

# Upload to device
pio run --target upload

# Serial monitor
pio device monitor --baud 115200
```

## Quick Start

### First Boot
1. Connect ESP32 via USB
2. Upload firmware
3. Device creates `TouchClock-Setup` Wi-Fi AP (no password)
4. Connect to `TouchClock-Setup` from your phone/computer
5. Captive portal opens automatically → configure your Wi-Fi SSID
6. Device connects and stores credentials

### Subsequent Boots
- Device auto-connects to saved Wi-Fi
- Fetches time from NTP
- Updates weather every 2 hours
- Chimes hourly (08:00-22:00)
- Falls back to setup mode if stored credentials fail

## Development

### Project Structure
```
src/
├── main.cpp              # Main application & setup
├── AppVersion.h          # Version management
├── ChimeManager.cpp      # Chime logic implementation
├── ChimeManager.h        # Hourly chime manager (Big Ben sounds)
├── DisplayManager.h      # Display control (TFT_eSPI)
├── LightSensorManager.h  # Ambient light sensor logic
├── NetworkManager.h      # Wi-Fi provisioning & captive portal
├── RGBLedManager.h       # RGB LED control
├── TimeManager.h         # NTP sync & time formatting
├── TouchManager.h        # Touchscreen handling (XPT2046)
├── WeatherManager.h      # Weather data fetch & display
├── weather_icons.h       # Bitmap assets for weather display
```

### Configuration
Edit `platformio.ini` to adjust:
- SPI display frequency (currently 55 MHz)
- Pin assignments (MISO, MOSI, SCLK, CS, DC, BL)
- Font loading options
- WiFi AP name (`TouchClock-Setup`)

See [BUILD.md](BUILD.md) for detailed build instructions.

## Technical Details

### Display Driver
- **Chip:** ILI9341 (revision 2)
- **Driver:** TFT_eSPI (^2.5.43) for low-level display control
- **Inversion:** Enabled (`TFT_INVERSION_ON`)
- **Rotation:** Landscape (1)
- **Resolution:** 320×240 pixels

### WiFi Provisioning
- **Method:** AutoConnect captive portal (open AP, no password)
- **Persistence:** NVS Preferences (namespace: "touchclock")
- **Fallback:** If stored credentials fail, portal re-activates

### Weather Features
- **Source:** Open-Meteo API (no API key required)
- **Data:** Hourly weather codes and temperatures
- **Display:** 6 slots showing conditions every 2 hours
- **Temperature:** Celsius with precise centering under icons
- **Refresh:** Every 2 hours, auto-updates at midnight

## Troubleshooting

### Display shows washed-out colors
→ Verify `TFT_INVERSION_ON` is set in `platformio.ini`

### WiFi captive portal appears every boot
→ Stored credentials may be invalid:
   - Reconfigure via captive portal with correct SSID/password
   - Or erase stored creds: call `netMgr.eraseStored()` then reboot

### Time doesn't sync
→ Ensure WiFi is connected before TimeManager starts (check serial output)

### Light sensor too sensitive
→ Adjust `ADC_ATTENUATION` in LightSensorManager.h (currently ADC_11db for lowest sensitivity)

## Architecture

Direct drawing via TFT_eSPI (no LVGL overhead). Touch input via XPT2046 with FreeRTOS tasks on Core 1. Light sensor polling with 10-second calibration and 2-second debounce for screen-off.

## References
- [Official ESP32-CYD Repository](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- [Open-Meteo API](https://open-meteo.com/)

## License
As per original project. See LICENSE file.