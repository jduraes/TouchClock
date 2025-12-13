# LVGL9 Migration Guide

This document explains the migration from TFT_eSPI direct drawing to LVGL9 framework.

## Overview

TouchClock has been upgraded from using TFT_eSPI for direct pixel manipulation to using LVGL v9.2, a modern embedded GUI framework. This provides better structure, easier maintenance, and more features.

## What Changed

### Architecture

**Before (TFT_eSPI Direct):**
```
main.cpp → DisplayManager → TFT_eSPI → ILI9341 Display
```

**After (LVGL9):**
```
main.cpp → DisplayManager → LVGL → TFT_eSPI → ILI9341 Display
                          ↘ TouchManager → LVGL Input System
```

### File Changes

| File | Status | Notes |
|------|--------|-------|
| `platformio.ini` | Modified | Added LVGL dependency and config path |
| `lv_conf.h` | **NEW** | LVGL configuration file |
| `src/DisplayManagerLVGL.h` | **NEW** | LVGL-based display manager |
| `src/TouchManagerLVGL.h` | **NEW** | LVGL-based touch manager |
| `src/DisplayManager.h` | Preserved | Original version kept for reference |
| `src/TouchManager.h` | Preserved | Original version kept for reference |
| `src/main.cpp` | Modified | Updated includes and added LVGL timer |

### API Compatibility

The DisplayManager and TouchManager maintain the same public API, so existing code in `main.cpp` requires minimal changes.

**Changes in main.cpp:**
1. Include LVGL header: `#include <lvgl.h>`
2. Include new managers: `DisplayManagerLVGL.h`, `TouchManagerLVGL.h`
3. Add LVGL timer in loop: `dispMgr.update();`

## Key Features of LVGL9

### 1. Widget-Based UI

Instead of drawing primitives directly:
```cpp
// Old way (TFT_eSPI)
tft.drawCentreString(timeStr, Lw / 2, 85, 7);

// New way (LVGL)
lv_label_set_text(labelTime, timeStr.c_str());
lv_obj_align(labelTime, LV_ALIGN_CENTER, 0, -10);
```

### 2. Automatic Rendering

LVGL handles:
- Dirty region detection
- Double buffering
- Partial updates
- Memory management

### 3. Touch Input Integration

Touch events are now handled by LVGL's input system:
```cpp
// LVGL input device callback
static void touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    // Read touch coordinates
    // LVGL automatically routes to appropriate widgets
}
```

### 4. Styling System

LVGL provides a powerful styling system:
```cpp
lv_obj_set_style_text_color(label, lv_color_hex(0xFFFF00), 0); // Yellow
lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
```

## Configuration

### Memory Usage

LVGL is configured to use 48KB of RAM for graphics operations:
```c
#define LV_MEM_SIZE (48 * 1024U)  // in lv_conf.h
```

### Display Buffer

Two 40-line buffers for smooth partial rendering:
```cpp
static const uint32_t BUF_SIZE = 320 * 40; // 40 lines
static lv_color_t buf1[BUF_SIZE];
static lv_color_t buf2[BUF_SIZE];
```

### Fonts

Montserrat fonts enabled in sizes 12-48pt:
```c
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
// ... up to 48
```

## Benefits

### 1. Better Structure
- Separation of UI logic from hardware
- Widget hierarchy instead of absolute positioning
- Event-driven architecture

### 2. Easier Maintenance
- Change colors/fonts via styles
- Reposition elements with alignment
- Less manual coordinate calculation

### 3. Future Expandability
- Easy to add buttons, sliders, lists
- Built-in animations and transitions
- Theme support for different looks

### 4. Industry Standard
- LVGL is used in thousands of embedded products
- Active development and community support
- Extensive documentation and examples

## Performance

### Memory Impact
- **Flash:** +~200KB (LVGL library + fonts)
- **RAM:** +~55KB (48KB LVGL heap + buffers)
- **ESP32:** Has plenty of both (4MB flash, 520KB RAM)

### CPU Impact
- **Rendering:** Slightly more CPU due to LVGL layer
- **Optimization:** LVGL only redraws changed regions
- **Result:** Similar or better performance overall

### Display Refresh
- **Before:** Manual per-second updates
- **After:** LVGL handles refresh automatically
- **Rate:** Configurable (default 30ms refresh period)

## Troubleshooting

### Build Errors

**"lv_conf.h not found"**
- Ensure `-DLV_CONF_PATH="${PROJECT_DIR}/lv_conf.h"` in platformio.ini
- Check that lv_conf.h exists in project root

**"undefined reference to lv_*"**
- LVGL library not installed
- Run `platformio lib install "lvgl/lvgl@^9.2.0"`

### Runtime Issues

**Display not working**
- Check TFT_eSPI configuration (User_Setup.h)
- Verify `lv_timer_handler()` called in loop
- Check serial output for LVGL errors

**Touch not responding**
- Verify XPT2046 initialization
- Check touch calibration values
- Enable debug mode (triple-tap version label)

**Memory errors**
- Increase `LV_MEM_SIZE` in lv_conf.h
- Reduce font sizes or disable unused widgets
- Monitor heap with `ESP.getFreeHeap()`

## Migration Checklist

- [x] Update platformio.ini with LVGL dependency
- [x] Create lv_conf.h configuration
- [x] Implement DisplayManagerLVGL.h
- [x] Implement TouchManagerLVGL.h
- [x] Update main.cpp includes and loop
- [x] Test build
- [x] Test display output
- [x] Test touch input
- [x] Test WiFi setup flow
- [x] Test NTP time sync
- [x] Update documentation

## Resources

- [LVGL v9.2 Documentation](https://docs.lvgl.io/9.2/)
- [LVGL Examples](https://docs.lvgl.io/9.2/examples.html)
- [LVGL Forum](https://forum.lvgl.io/)
- [LVGL GitHub](https://github.com/lvgl/lvgl)

## Support

For issues specific to this TouchClock implementation:
1. Check the serial monitor for error messages
2. Enable LVGL debug logging in lv_conf.h
3. Verify hardware connections (display, touch)
4. Review the migration checklist above

For general LVGL questions:
- [LVGL Documentation](https://docs.lvgl.io/)
- [LVGL Forum](https://forum.lvgl.io/)
- [LVGL Discord](https://discord.gg/lvgl)
