# TouchClock

A networked digital clock for the **ESP32 Cheap Yellow Display (ESP32-2432S028)** with Wi-Fi provisioning, powered by **LVGL9**.

## Features
- **Modern UI Framework:** Built with LVGL9 (Light and Versatile Graphics Library)
- **Dynamic Wi-Fi Setup:** Captive portal for easy SSID/password configuration on first boot
- **Persistent Storage:** Saves Wi-Fi credentials to NVS (Preferences) for automatic reconnection
- **Real-time Display:** Shows current time in large, readable format (landscape 320×240)
- **Responsive UI:** Status messages and setup instructions displayed during boot
- **Touch Input:** Full touchscreen support via LVGL input system

## Hardware
- **Board:** ESP32-WROOM-32 (ESP32-2432S028 module)
- **Display:** 3.2" TFT LCD, 320×240, ILI9341 controller with inversion
- **Power:** 5V USB (via micro-USB)
- **Touch:** Capacitive touch panel (optional)

See [HARDWARE.md](HARDWARE.md) for detailed pinout and configuration.

## Quick Start

### Prerequisites
- PlatformIO (VS Code extension or CLI)
- USB cable + Serial driver

### First Boot
1. Connect ESP32 via USB
2. Build & upload: `platformio run --target upload --upload-port COM6`
3. Device creates `TouchClock-Setup` Wi-Fi AP (no password)
4. Connect to `TouchClock-Setup` from your phone/computer
5. Captive portal opens automatically → configure your Wi-Fi SSID
6. Device connects and stores credentials

### Subsequent Boots
- Device auto-connects to saved Wi-Fi
- Skips captive portal if connection succeeds
- Falls back to setup if stored credentials fail

## Development

### Project Layout
```
src/
├── main.cpp           # Setup & main loop
├── DisplayManager.h    # TFT display operations
├── NetworkManager.h    # Wi-Fi + captive portal
└── TimeManager.h       # Time sync & formatting
```

### Building
```bash
# Build & upload
platformio run --target upload --upload-port COM6

# Serial monitor
platformio device monitor --port COM6 --baud 115200

# Clean build
platformio run --target clean
platformio run
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
- **UI Framework:** LVGL v9.2 (Light and Versatile Graphics Library)
- **Display Driver:** TFT_eSPI (^2.5.31) for low-level display control
- **Inversion:** Enabled (`TFT_INVERSION_ON`)
- **Rotation:** Landscape (1)

### WiFi Provisioning
- **Method:** AutoConnect captive portal (open AP, no password)
- **Persistence:** NVS Preferences (namespace: "touchclock")
- **Fallback:** If stored credentials fail, portal re-activates

### Time Synchronization
- **Method:** NTP (SNTP) via WiFi
- **Zone:** UTC or configured offset
- **Format:** 24-hour HH:MM:SS

## Troubleshooting

### Display shows washed-out colors
→ Verify `TFT_INVERSION_ON` is set in `platformio.ini`

### WiFi captive portal appears every boot
→ Stored credentials may be invalid. Either:
   - Reconfigure via captive portal with new SSID
   - Or erase stored creds: call `netMgr.eraseStored()` then reboot

### Can't connect to COM6
→ Check Device Manager; may need USB serial driver installed

### Time doesn't sync
→ Ensure WiFi is connected before TimeManager starts (see `setup()`)

## LVGL9 Integration

This project has been updated to use LVGL v9.2 for improved graphics and UI management.

### Key Changes:
- **DisplayManager:** Now uses LVGL widgets instead of direct TFT_eSPI drawing
- **TouchManager:** Integrated with LVGL's input device system
- **Configuration:** See `lv_conf.h` for LVGL settings
- **Memory:** LVGL uses 48KB for graphics operations (configurable)
- **Rendering:** Double-buffered partial rendering for smooth updates

### Benefits of LVGL:
- **Structured UI:** Widget-based system instead of pixel manipulation
- **Themes & Styles:** Built-in styling system for consistent look
- **Touch Handling:** Integrated touch input with gesture support
- **Future Expandability:** Easy to add buttons, sliders, and other widgets

## References
- [LVGL Documentation](https://docs.lvgl.io/9.2/)
- [Official ESP32-CYD Repository](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- [AutoConnect](https://hieromon.github.io/AutoConnect/)
- Example projects: `references/ESP32-Cheap-Yellow-Display/Examples/Basics/`

## License
As per original project. See LICENSE file.
