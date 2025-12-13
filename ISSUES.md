# Known Issues

## v1.0.1

### Screen Backlight Flickering (3Hz)
**Status:** OPEN  
**Severity:** High  
**Description:** The entire LCD backlight flickers approximately 3 times per second. This appears to be unrelated to the light sensor logic or display update frequency.

**Investigation:**
- Not caused by screen-off logic (disabled and issue persists)
- Not caused by debug serial output
- Not caused by display update timing (time display has proper change detection)
- Appears to be hardware/driver issue, possibly SPI bus contention or PWM interference

**Possible Causes:**
1. TFT_eSPI driver PWM backlight control conflicting with Light Sensor ADC sampling on Core 1
2. SPI bus timing issues with concurrent Core 0/Core 1 operations
3. GPIO interference between light sensor input and backlight output

**Workarounds Attempted:**
- Disabling light sensor screen-off logic ❌
- Disabling debug output ❌
- Reducing screen-off check frequency ❌

**Next Steps:**
- Test with different TFT_eSPI configuration
- Check if light sensor task is causing SPI bus blocking
- Verify backlight PWM configuration on GPIO32
- Test if reducing light sensor sampling frequency helps

---

## v1.0.0

### Resolved Issues
- ✅ RGB LED always on (fixed with active-LOW analogWrite)
- ✅ Light sensor readings frozen at 0 (fixed with ADC_6db attenuation)
- ✅ Screen-off logic inverted (corrected comparison operators)
