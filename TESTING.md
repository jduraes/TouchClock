# TouchClock LVGL9 Testing Guide

This document provides comprehensive testing procedures for the LVGL9-enabled TouchClock.

## Build Testing

### Prerequisites
1. PlatformIO installed (via VS Code extension or CLI)
2. ESP32 connected via USB
3. Correct COM port identified

### Build Steps

```bash
# Clean build
platformio run --target clean

# Build firmware
platformio run

# Upload to device
platformio run --target upload --upload-port COM6

# Monitor serial output
platformio device monitor --port COM6 --baud 115200
```

### Expected Build Output
- Compilation should complete without errors
- Binary size: ~1.3 MB (increased from ~1.1 MB due to LVGL)
- RAM usage: ~70KB static (increased from ~15KB)

### Build Verification Checklist
- [ ] Compiles without errors
- [ ] LVGL library linked correctly
- [ ] TFT_eSPI configured properly
- [ ] All managers compile
- [ ] Firmware size < 2MB (huge_app.csv partition)

## Functional Testing

### 1. Display Initialization

**Test:** Power on device

**Expected Behavior:**
- Display turns on
- Backlight activates
- "TouchClock" title appears in yellow
- Version number "v1.1.0-LVGL9" in top-right (blue)
- Time shows as "--:--:--" initially

**Verification:**
- [ ] Display initializes properly
- [ ] No flickering or artifacts
- [ ] Colors correct (not inverted)
- [ ] Text renders clearly

### 2. WiFi Setup (First Boot)

**Test:** First boot without stored credentials

**Expected Behavior:**
1. Display shows "Waiting for WiFi setup..."
2. Instructions appear: "Connect to TouchClock-Setup"
3. Device creates open WiFi AP
4. Captive portal accessible at 192.168.4.1

**Verification:**
- [ ] AP appears in WiFi list
- [ ] Can connect to AP
- [ ] Captive portal loads
- [ ] Network scan works
- [ ] Can select and configure WiFi
- [ ] Credentials saved to NVS

### 3. WiFi Connection (Subsequent Boots)

**Test:** Reboot with stored credentials

**Expected Behavior:**
1. Display shows "Connecting to WiFi..."
2. Progress counter shown
3. Connects within 10 seconds
4. Status shows "WiFi: [SSID]"

**Verification:**
- [ ] Auto-connects to stored network
- [ ] No captive portal appears
- [ ] Connection successful
- [ ] WiFi power saving disabled

### 4. Time Synchronization

**Test:** After WiFi connects

**Expected Behavior:**
1. Display shows "Syncing with uk.pool.ntp.org..."
2. Progress shown (0/15s)
3. Time syncs within 15 seconds
4. Large time display updates every second
5. Date appears below time

**Verification:**
- [ ] NTP sync successful
- [ ] Time displays correctly
- [ ] Time updates every second
- [ ] Date formatted properly
- [ ] Week number shown

### 5. Touch Input - Version Label

**Test:** Triple-tap version label (top-right)

**Expected Behavior:**
1. First tap: Serial shows "Version pressed (1/3)"
2. Second tap: "Version pressed (2/3)"
3. Third tap: "DEBUG MODE ENABLED"
4. Debug overlay appears (green rectangles)
5. Status shows "DEBUG MODE ON"

**Verification:**
- [ ] Touch detected correctly
- [ ] Triple-tap recognized
- [ ] Debug mode toggles
- [ ] Overlay appears/disappears
- [ ] Serial messages correct

### 6. Touch Input - Title Label

**Test:** Triple-tap title "TouchClock"

**Expected Behavior:**
1. First tap: Serial shows "Title pressed (1/3)"
2. Second tap: "Title pressed (2/3)"
3. Third tap: Title changes to "(c)2025 Joao Miguel Duraes"
4. Triple-tap again: Returns to "TouchClock"

**Verification:**
- [ ] Touch area detected
- [ ] Triple-tap recognized
- [ ] Title toggles correctly
- [ ] LVGL redraws properly

### 7. Light Sensor - Screen Off

**Test:** Cover light sensor (GPIO 34)

**Expected Behavior:**
1. Sensor reads low value (< 30)
2. After 2 seconds: Backlight turns off
3. Serial shows "SCREEN OFF - Bright light detected"

**Verification:**
- [ ] Sensor reading shown in debug mode
- [ ] Debounce works (2 second delay)
- [ ] Backlight turns off
- [ ] Screen remains off

### 8. Light Sensor - Screen Wake

**Test:** Touch screen while off

**Expected Behavior:**
1. Touch detected
2. Backlight turns on immediately
3. Serial shows "SCREEN ON - Woken by touch"
4. Display fully functional

**Verification:**
- [ ] Touch wakes screen
- [ ] Backlight turns on
- [ ] Display refreshes
- [ ] Normal operation resumes

### 9. Status Cycling

**Test:** Leave device running for 10+ seconds

**Expected Behavior:**
1. Status alternates every 5 seconds:
   - "Connected to: [SSID]"
   - "Time from: uk.pool.ntp.org"
2. Messages cycle continuously

**Verification:**
- [ ] Status updates every 5 seconds
- [ ] Messages cycle correctly
- [ ] No flicker when changing
- [ ] Debug mode pauses cycling

### 10. LVGL Rendering

**Test:** Observe display updates

**Expected Behavior:**
- Time updates every second
- Date updates at midnight
- Status updates every 5 seconds
- No flickering or tearing
- Smooth transitions

**Verification:**
- [ ] Partial updates work
- [ ] No full-screen flicker
- [ ] Text remains sharp
- [ ] Colors consistent
- [ ] No artifacts

## Performance Testing

### Memory Usage

**Test:** Monitor serial output during operation

```cpp
// Add to main loop for testing
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
```

**Expected:**
- Free heap: ~250-300KB
- LVGL heap usage: < 48KB
- No memory leaks over time

**Verification:**
- [ ] Sufficient free memory
- [ ] Memory stable over time
- [ ] No heap fragmentation

### CPU Usage

**Test:** Measure loop frequency

```cpp
// Add to main loop
static unsigned long lastTime = 0;
static int loopCount = 0;
if (millis() - lastTime >= 1000) {
    Serial.printf("Loops per second: %d\n", loopCount);
    loopCount = 0;
    lastTime = millis();
} else {
    loopCount++;
}
```

**Expected:**
- ~20 loops per second (50ms delay)
- Consistent timing
- No blocking operations

**Verification:**
- [ ] Loop rate stable
- [ ] LVGL timer not blocking
- [ ] Touch responsive

### Display Performance

**Test:** Rapid time updates

**Expected:**
- Time updates every second
- No lag or delay
- Smooth rendering
- No SPI conflicts

**Verification:**
- [ ] Updates punctual
- [ ] No display corruption
- [ ] WiFi doesn't interfere
- [ ] Touch doesn't interfere

## Compatibility Testing

### TFT_eSPI Backend

**Test:** Verify low-level display works

**Verification:**
- [ ] SPI communication stable
- [ ] ILI9341_2_DRIVER correct
- [ ] Inversion enabled
- [ ] Rotation set properly
- [ ] Colors accurate

### XPT2046 Touch

**Test:** Touch calibration

**Verification:**
- [ ] Touch coordinates accurate
- [ ] Calibration values correct
- [ ] No drift or offset
- [ ] Edge detection works

### LVGL Integration

**Test:** LVGL-TFT_eSPI bridge

**Verification:**
- [ ] Flush callback works
- [ ] Byte swapping correct
- [ ] Buffer management proper
- [ ] No memory corruption

## Regression Testing

### Original Features

Test all features from pre-LVGL version:

- [ ] WiFi provisioning
- [ ] NTP time sync
- [ ] Display rendering
- [ ] Touch input
- [ ] Light sensor
- [ ] RGB LED (if enabled)
- [ ] Status messages
- [ ] Debug mode

### Known Issues

Document any issues found:

1. **Issue:** [Description]
   - **Impact:** [High/Medium/Low]
   - **Workaround:** [If any]
   - **Status:** [Open/Fixed]

## Serial Output Examples

### Successful Boot
```
TouchManager initialized with LVGL
Starting WiFi connection...
Found stored credentials for: MyNetwork
Connection attempt 1/3
Connected! IP: 192.168.1.100
WiFi: Connected to MyNetwork
Configuring time with NTP servers...
Time synchronized from NTP
12:34:56
Friday, 13 December, 2024, week 50
```

### Debug Mode
```
Version pressed (1/3)
Version pressed (2/3)
Version pressed (3/3)
DEBUG MODE ENABLED
DEBUG Light: raw=1234, avg5s=1240, avg10s=1235, threshold=30, screen=ON
```

### Touch Events
```
Title pressed (1/3)
Title pressed (2/3)
Title pressed (3/3)
Header set to copyright
```

## Troubleshooting

### Display Issues

**Problem:** Washed out colors
- **Check:** TFT_INVERSION_ON enabled
- **Check:** ILI9341_2_DRIVER selected
- **Check:** SPI frequency (55MHz)

**Problem:** No display output
- **Check:** Backlight GPIO (21) HIGH
- **Check:** TFT initialization in DisplayManager
- **Check:** LVGL flush callback working

### Touch Issues

**Problem:** Touch not responding
- **Check:** XPT2046 SPI initialization
- **Check:** Calibration values (TS_MINX, etc.)
- **Check:** LVGL input device created
- **Check:** Touch callback registered

### Memory Issues

**Problem:** Crashes or resets
- **Check:** LV_MEM_SIZE sufficient
- **Check:** Stack size adequate
- **Check:** Heap fragmentation
- **Check:** Buffer overflow

### Performance Issues

**Problem:** Slow rendering
- **Check:** LVGL timer called in loop
- **Check:** SPI frequency optimized
- **Check:** Partial updates working
- **Check:** No blocking in callbacks

## Validation Checklist

Complete testing verified:
- [ ] All functional tests pass
- [ ] Performance acceptable
- [ ] No memory leaks
- [ ] No regressions
- [ ] Documentation accurate
- [ ] Code compiles cleanly
- [ ] Serial output correct
- [ ] Touch fully functional
- [ ] Display quality good

## Sign Off

**Tester:** _______________  
**Date:** _______________  
**Build Version:** v1.1.0-LVGL9  
**Result:** PASS / FAIL  
**Notes:**

---

## Next Steps After Testing

1. **If tests pass:**
   - Tag release as v1.1.0-LVGL9
   - Update main branch
   - Archive TFT_eSPI version

2. **If tests fail:**
   - Document issues
   - Fix critical bugs
   - Re-test
   - Update documentation

3. **Future enhancements:**
   - Add LVGL widgets (buttons, sliders)
   - Implement settings menu
   - Add themes/skins
   - Improve touch areas
