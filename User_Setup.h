//                            USER DEFINED SETTINGS
//   Configuration for ESP32-2432S028 (CYD) with ILI9341 display
//   Pin mappings and driver selection match platformio.ini build flags

#define USER_SETUP_INFO "ESP32-2432S028"
#define DISABLE_ALL_LIBRARY_WARNINGS

// ==================== Driver ====================
#define ILI9342_DRIVER
#define TFT_INVERSION_OFF

// ==================== Pins for ESP32 ====================
// SPI pins
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14

// Control pins
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1       // RST tied to ESP32 reset

// Backlight
#define TFT_BL   21
#define TFT_BACKLIGHT_ON 1

// Touch (optional)
#define TOUCH_CS 33

// ==================== SPI Frequency ====================
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY  2500000

// ==================== Fonts ====================
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_GFXFF
#define SMOOTH_FONT

// ==================== Optional Settings ====================
// #define TFT_RGB_ORDER TFT_RGB   // Uncomment if colours are swapped
// #define TFT_SDA_READ            // For reading data back from display (slower)

