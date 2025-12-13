# Known Issues

## v1.0.2 (Current)

### Screen Backlight Flickering (3Hz) - RESOLVED ✅
**Status:** FIXED  
**Severity:** High (was)  
**Root Cause:** WiFi power saving mode causing SPI bus contention  

**Solution:**
Added `WiFi.setSleep(WIFI_PS_NONE)` after WiFi connection to disable power saving mode. The ESP32 WiFi radio was frequently waking up in power-save mode, causing interrupts that conflicted with SPI display timing, resulting in ~3Hz backlight flicker.

**Code:**
```cpp
if (ok && WiFi.status() == WL_CONNECTED) {
    // Disable WiFi power saving to prevent SPI bus contention/display flickering
    WiFi.setSleep(WIFI_PS_NONE);
    ...
}
```

**Trade-off:** Disabling WiFi power saving increases power consumption slightly, but eliminates display flicker completely. This is acceptable for a desk clock that's typically powered.

---

## v1.0.1

### Screen Backlight Flickering (3Hz) - INVESTIGATED
**Status:** OPEN (at time of v1.0.1)  
**Severity:** High  
**Description:** The entire LCD backlight flickers approximately 3 times per second.

**Investigation Process:**
1. ❌ Not caused by screen-off logic (disabled and issue persisted)
2. ❌ Not caused by debug serial output
3. ❌ Not caused by display update timing
4. ❌ Not caused by light sensor task on Core 1
5. ❌ Not caused by touch manager task on Core 1
6. ✅ **Root cause identified: WiFi power saving mode**

---

## v1.0.0

### Resolved Issues
- ✅ RGB LED always on (fixed with active-LOW analogWrite)
- ✅ Light sensor readings frozen at 0 (fixed with ADC_6db attenuation)
- ✅ Screen-off logic inverted (corrected comparison operators)

