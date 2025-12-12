# TouchClock Hardware Documentation

## Display Module
**Model:** ESP32-2432S028 (ESP32 Cheap Yellow Display - CYD)  
**Resolution:** 320×240 pixels (landscape)  
**Controller IC:** ILI9341 (revision 2 variant: `ILI9341_2_DRIVER`)  
**Display Type:** TFT LCD, Colour 16-bit

### Physical Characteristics
- **Dimensions:** ~240mm × 80mm (including bezel)
- **Viewing Area:** ~240mm × 75mm landscape
- **Brightness:** Backlit with adjustable brightness (GPIO 21)
- **Connector:** 40-pin standard ESP32 header

### Color Behavior
The display panel uses **color inversion** (`TFT_INVERSION_ON`). This is critical for proper color representation:
- Without inversion: colors appear washed-out/inverted
- With inversion: colors render correctly (black = black, white = white, etc.)

### Pinout (ESP32 GPIO)

| Signal      | GPIO | Purpose                          |
|-------------|------|----------------------------------|
| TFT_MISO    | 12   | SPI MISO (Master In, Slave Out) |
| TFT_MOSI    | 13   | SPI MOSI (Master Out, Slave In) |
| TFT_SCLK    | 14   | SPI Clock                        |
| TFT_CS      | 15   | TFT Chip Select                  |
| TFT_DC      | 2    | TFT Data/Command                 |
| TFT_RST     | -1   | TFT Reset (not used, tied high)  |
| TFT_BL      | 21   | Backlight PWM (active high)      |
| TOUCH_CS    | 33   | Touch Panel Chip Select          |

## SPI Configuration
- **Frequency:** 55 MHz (SPI write)
- **Read Frequency:** 20 MHz (SPI read)
- **Touch Frequency:** 2.5 MHz (capacitive touch)
- **Mode:** SPI Mode 0, MSB-first

## Display Driver Configuration
The TFT_eSPI library requires `User_Setup.h` configuration. Key flags:

```
-DUSER_SETUP_LOADED           # Use project-level User_Setup.h
-DUSE_HSPI_PORT               # Use HSPI (SPI2, not SPI1)
-DILI9341_2_DRIVER            # ILI9341 revision 2 (CRITICAL)
-DTFT_INVERSION_ON            # Color inversion (CRITICAL)
-DLOAD_FONT2, LOAD_FONT4, ... # Font support
-DSMOOTH_FONT                 # Smooth font rendering
```

### Important Notes
1. **ILI9341_2_DRIVER vs ILI9341_DRIVER:**  
   The `ILI9341_2_DRIVER` variant is specifically tested for this board. Using the plain `ILI9341_DRIVER` may cause display issues.

2. **Inversion Setting:**  
   If colors appear inverted or washed-out, verify `TFT_INVERSION_ON` is set in `platformio.ini` or `User_Setup.h`.

3. **Backlight Control:**  
   The backlight is controlled via GPIO 21 with high-active logic. Set `digitalWrite(TFT_BL, HIGH)` to enable.

## References
- **Official Repo:** https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
- **TFT_eSPI Library:** https://github.com/Bodmer/TFT_eSPI
- **ILI9341 Datasheet:** Available in the references/ directory
