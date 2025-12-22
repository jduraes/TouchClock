# TouchClock Build & Development Guide

## Prerequisites
- **PlatformIO CLI** (installed via pip or VS Code extension)
- **Python 3.8+**
- **USB Serial Connection** to ESP32 (COM port, typically COM6)

## Project Structure
```
TouchClock/
├── platformio.ini          # Build configuration & dependencies
├── User_Setup.h            # TFT_eSPI display driver configuration
├── src/
│   ├── main.cpp           # Application entry point
│   ├── DisplayManager.h    # TFT display controller
│   ├── NetworkManager.h    # WiFi provisioning & persistence
│   └── TimeManager.h       # Time synchronization
├── references/            # External reference projects
│   └── ESP32-Cheap-Yellow-Display/  # Official ESP32-CYD examples
├── HARDWARE.md            # Hardware pin configuration & specs
└── BUILD.md               # This file
```

## Building

### Full Build & Upload
```bash
cd c:\Users\miguel\Documents\GitHub\TouchClock
platformio run --target upload --upload-port COM6
```

### Just Compile (no upload)
```bash
platformio run
```

### Clean Build
```bash
platformio run --target clean
platformio run --target upload --upload-port COM6
```

## Firmware Configuration

### platformio.ini
The `platformio.ini` defines:
- **Dependencies:** TFT_eSPI (display), XPT2046 (touch)
- **Build Flags:** Display driver, pin definitions, SPI speeds
- **Serial Monitor:** 115200 baud, exception decoder enabled

Key flags:
- `-DILI9341_2_DRIVER` – Display controller type (CRITICAL)
- `-DTFT_INVERSION_ON` – Color correction for this panel (CRITICAL)
- `-DSPI_FREQUENCY=55000000` – Display SPI speed (55 MHz)

### Configuration
No LVGL configuration. TFT_eSPI and XPT2046 are configured via build flags and code in `DisplayManager.h` and `TouchManager.h`.

### User_Setup.h
This file is **included first** (before TFT_eSPI's default) to ensure correct pin configuration. It explicitly defines:
- Display GPIO pins (MISO, MOSI, SCLK, CS, DC, BL)
- Touch controller CS pin
- SPI frequencies

## Flashing

### Serial Monitor (to view logs)
```bash
platformio device monitor --port COM6 --baud 115200
```

### Troubleshooting Upload Issues
0. **Serial monitor busy** → If `platformio device monitor` (or any other terminal) is attached to COM4, close/kill it before uploading, otherwise upload will fail with "port doesn't exist" or timeouts.
1. **"Port doesn't exist"** → Reconnect USB cable, wait 2 sec, retry
2. **"Device is not in bootloader mode"** → Press EN (reset) button on board
3. **"Hash mismatch"** → Try a clean build and re-upload

## WiFi Provisioning Flow

### First Boot (No Stored Credentials)
1. Device creates open AP: `TouchClock-Setup`
2. Connect via WiFi to `TouchClock-Setup` (no password required)
3. Open browser → captive portal redirects to config page
4. Select & connect to your network SSID
5. Device stores SSID in NVS (Preferences)
6. On reboot, tries stored SSID first

### Subsequent Boots (Stored Credentials)
1. Device reads SSID from Preferences
2. Attempts connection with stored credentials
3. If successful: skips captive portal, boots normally
4. If fails: falls back to captive portal setup

### Resetting WiFi
To force re-provisioning:
```cpp
// In code:
netMgr.eraseStored();  // Clear saved credentials
// Then reboot
```

## Dependencies & Versioning

| Library      | Version | Purpose                          |
|--------------|---------|----------------------------------|
| TFT_eSPI     | ^2.5.31 | Display driver                    |
| XPT2046      | latest  | Touchscreen driver               |
| Preferences  | (built-in) | NVS key-value storage       |
| WiFi         | (built-in) | ESP32 WiFi stack            |
| WebServer    | (built-in) | HTTP server for config UI   |

## Serial Output Examples

### Successful Connection
```
Trying stored SSID: MyWiFi
Connected to MyWiFi
IP: 192.168.1.100
```

### First Boot (Captive Portal)
```
Starting captive portal AP: TouchClock-Setup
```

### Time Sync
```
12:34:56
12:34:57
12:34:58
```

## Performance Notes
- **Flash Usage:** ~84% (≈1.1 MB)
- **RAM Usage:** ~15% (≈48 KB)
- **Display Refresh:** 1 Hz (clock updates once per second)
- **SPI Bandwidth:** ~55 MHz provides smooth rendering

## References
- [Official ESP32-CYD Repo](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [TFT_eSPI Documentation](https://github.com/Bodmer/TFT_eSPI/wiki)
- [AutoConnect Documentation](https://hieromon.github.io/AutoConnect/quickstart/)
