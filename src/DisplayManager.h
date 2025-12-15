#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "AppVersion.h"

class DisplayManager {
    TFT_eSPI tft = TFT_eSPI();
    int Lw = 320; 
    int Lh = 240;
    String _lastStatusShown = "";  // Cache last status to avoid redraw

    // Layout constants (tunable positions and sizes)
    // Header
    const int HEADER_HEIGHT = 50;
    const int HEADER_TITLE_Y = 10;
    const int HEADER_DIVIDER_Y = 40;
    const int HEADER_VERSION_Y = 25;
    const int HEADER_VERSION_RIGHT_PAD = 35; // distance from right edge
    
    // Clock & date
    const int CLOCK_Y = 65;         // centre Y for main time
    const int DATE_Y = 120;         // centre Y for date line

    // Weather icons row
    const int WEATHER_BASE_Y = 155; // top of icons row
    const int WEATHER_ICON_W = 36;
    const int WEATHER_ICON_H = 26;
    const int WEATHER_LABEL_GAP = 10; // gap between icons and labels

    // Status & instruction areas
    const int STATUS_BAR_HEIGHT = 30;
    const int STATUS_TEXT_Y_OFFSET = 18; // distance from bottom to centre text
    const int INSTR_BAR_HEIGHT = 50;
    const int INSTR_LINE1_Y = 45;  // distance from bottom for first line
    const int INSTR_LINE2_Y = 25;  // distance from bottom for second line
    const int BRIGHTNESS_AREA_Y = 42; // area just below header divider
    const int BRIGHTNESS_AREA_H = 20;
    const int BRIGHTNESS_TEXT_X = 2;
    const int BRIGHTNESS_TEXT_Y = 43;

public:
    void begin() {
        tft.init();
        
        // Rotation 1 = Landscape (320x240) - same as HelloWorld example
        tft.setRotation(1); 
        
        // After rotation, driver reports correct dimensions
        Lw = tft.width();
        Lh = tft.height();
        
        // Clear screen
        tft.fillScreen(TFT_BLACK);
        
        // Turn on backlight
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    void drawStaticInterface() {
        updateHeaderText("TouchClock");
    }

    // Redraws the top bar title, divider line, and version label
    void updateHeaderText(const String& text) {
        tft.fillRect(0, 0, Lw, HEADER_HEIGHT, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawCentreString(text, Lw / 2, HEADER_TITLE_Y, 4);
        tft.drawFastHLine(0, HEADER_DIVIDER_Y, Lw, TFT_BLUE);

        // Draw version in tiny blue font at top right, above the blue line
        tft.setTextColor(TFT_BLUE, TFT_BLACK);
        tft.drawString(appVersion(), Lw - HEADER_VERSION_RIGHT_PAD, HEADER_VERSION_Y, 1);
    }

    // Update clock display
    void updateClock(String timeStr) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawCentreString(timeStr, Lw / 2, CLOCK_Y, 7);
    }
    
    // Update date display
    void updateDate(String dateStr) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString(dateStr, Lw / 2, DATE_Y, 2);
    }

    const char* codeToGlyph(uint8_t code) {
        // Map WMO weather codes to short ASCII glyphs
        if (code == 0) return "SUN";                                                    // Clear
        if (code == 1 || code == 2) return "PCLD";                                      // Partly cloudy
        if (code == 3) return "CLD";                                                    // Overcast
        if (code == 45 || code == 48) return "FG";                                      // Fog
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "RAIN";    // Drizzle/rain
        if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return "SNW";     // Snow
        if (code >= 95) return "TSTM";                                                  // Thunder
        return "WND";                                                                   // Default windy/other
    }

    enum WeatherIcon { ICON_SUN, ICON_PARTLY, ICON_CLOUD, ICON_RAIN, ICON_SNOW, ICON_THUNDER, ICON_FOG, ICON_WIND };

    WeatherIcon mapWmoToIcon(uint8_t code) {
        if (code == 0) return ICON_SUN;
        if (code == 1 || code == 2) return ICON_PARTLY;
        if (code == 3) return ICON_CLOUD;
        if (code == 45 || code == 48) return ICON_FOG;
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return ICON_RAIN;
        if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return ICON_SNOW;
        if (code >= 95) return ICON_THUNDER;
        return ICON_WIND;
    }

    // Pixel-art style weather icon drawing functions
    void drawPixelSun(int x, int y, int size) {
        // Background: cyan/blue
        tft.fillRect(x, y, size, size, 0x04DF); // Light cyan
        
        // Sun body (yellow circle)
        int cx = x + size / 2;
        int cy = y + size / 2;
        int r = size / 4;
        tft.fillCircle(cx, cy, r, TFT_YELLOW);
        
        // Sun rays (8-directional, pixel-art style)
        int rayLen = size / 6;
        // Top
        tft.fillRect(cx - 1, y + 2, 3, rayLen, TFT_ORANGE);
        // Bottom
        tft.fillRect(cx - 1, y + size - rayLen - 2, 3, rayLen, TFT_ORANGE);
        // Left
        tft.fillRect(x + 2, cy - 1, rayLen, 3, TFT_ORANGE);
        // Right
        tft.fillRect(x + size - rayLen - 2, cy - 1, rayLen, 3, TFT_ORANGE);
        // Diagonals (pixel blocks)
        tft.fillRect(x + 4, y + 4, 3, 3, TFT_ORANGE);
        tft.fillRect(x + size - 7, y + 4, 3, 3, TFT_ORANGE);
        tft.fillRect(x + 4, y + size - 7, 3, 3, TFT_ORANGE);
        tft.fillRect(x + size - 7, y + size - 7, 3, 3, TFT_ORANGE);
    }

    void drawPixelPartlyCloudy(int x, int y, int size) {
        // Background: cyan
        tft.fillRect(x, y, size, size, 0x04DF);
        
        // Sun peeking from top-right
        int sunX = x + size * 3 / 4;
        int sunY = y + size / 4;
        tft.fillCircle(sunX, sunY, size / 5, TFT_YELLOW);
        
        // White cloud (bottom-left, pixel-art puffy shape)
        int cloudY = y + size / 2;
        tft.fillRect(x + 4, cloudY, size - 8, size / 3, TFT_WHITE);
        tft.fillRect(x + 2, cloudY + 2, size - 4, size / 4, TFT_WHITE);
        // Cloud outline for depth
        tft.drawRect(x + 4, cloudY, size - 8, size / 3, 0x8C51); // Darker blue outline
    }

    void drawPixelOvercast(int x, int y, int size) {
        // Background: light gray
        tft.fillRect(x, y, size, size, 0xC618); // Light gray
        
        // Two layers of gray clouds
        uint16_t darkGray = 0x8410;
        uint16_t lightGray = 0xBDF7;
        
        // Top cloud
        tft.fillRect(x + 3, y + 4, size - 6, size / 3, lightGray);
        tft.fillRect(x + 2, y + 6, size - 4, size / 4, lightGray);
        
        // Bottom cloud (darker)
        tft.fillRect(x + 2, y + size / 2, size - 4, size / 3, darkGray);
        tft.fillRect(x + 4, y + size / 2 + 2, size - 8, size / 4, darkGray);
    }

    void drawPixelFoggy(int x, int y, int size) {
        // Background: pale/hazy
        tft.fillRect(x, y, size, size, 0xDEDB); // Pale blue-gray
        
        // Faded sun with rays
        int cx = x + size / 2;
        int cy = y + size / 2;
        uint16_t paleYellow = 0xFFE0; // Pale yellow
        tft.fillCircle(cx, cy, size / 4, paleYellow);
        
        // Faded rays
        int rayLen = size / 6;
        tft.fillRect(cx - 1, y + 3, 2, rayLen, 0xEF7D);
        tft.fillRect(cx - 1, y + size - rayLen - 3, 2, rayLen, 0xEF7D);
        tft.fillRect(x + 3, cy - 1, rayLen, 2, 0xEF7D);
        tft.fillRect(x + size - rayLen - 3, cy - 1, rayLen, 2, 0xEF7D);
    }

    void drawPixelRain(int x, int y, int size) {
        // Background: blue
        tft.fillRect(x, y, size, size, 0x0233); // Darker blue
        
        // Raindrops (gray diagonal lines, pixel-art style)
        uint16_t dropColor = 0x7BEF; // Gray
        int spacing = size / 5;
        for (int i = 0; i < 4; i++) {
            int dx = x + 3 + i * spacing;
            int dy = y + 5 + (i % 2) * 3;
            // Diagonal pixel line
            tft.drawLine(dx, dy, dx + 2, dy + 4, dropColor);
            tft.drawLine(dx + 1, dy, dx + 3, dy + 4, dropColor);
        }
    }

    void drawPixelSnow(int x, int y, int size) {
        // Background: cyan
        tft.fillRect(x, y, size, size, 0x04DF);
        
        // Snowflake (white, pixel-art symmetric)
        int cx = x + size / 2;
        int cy = y + size / 2;
        int armLen = size / 4;
        
        // Horizontal and vertical
        tft.drawFastHLine(cx - armLen, cy, armLen * 2 + 1, TFT_WHITE);
        tft.drawFastVLine(cx, cy - armLen, armLen * 2 + 1, TFT_WHITE);
        
        // Diagonals
        for (int i = -armLen; i <= armLen; i++) {
            tft.drawPixel(cx + i, cy + i, TFT_WHITE);
            tft.drawPixel(cx + i, cy - i, TFT_WHITE);
        }
        
        // Cross tips
        tft.fillRect(cx - 2, cy - armLen - 2, 5, 2, TFT_WHITE);
        tft.fillRect(cx - 2, cy + armLen + 1, 5, 2, TFT_WHITE);
        tft.fillRect(cx - armLen - 2, cy - 2, 2, 5, TFT_WHITE);
        tft.fillRect(cx + armLen + 1, cy - 2, 2, 5, TFT_WHITE);
    }

    void drawPixelThunder(int x, int y, int size) {
        // Background: purple/dark
        tft.fillRect(x, y, size, size, 0x4810); // Dark purple
        
        // Dark cloud
        uint16_t cloudColor = 0x8410;
        tft.fillRect(x + 3, y + 3, size - 6, size / 3, cloudColor);
        tft.fillRect(x + 2, y + 5, size - 4, size / 4, cloudColor);
        
        // Lightning bolt (white/yellow, zig-zag)
        int cx = x + size / 2;
        int by = y + size / 3;
        tft.fillTriangle(cx - 3, by, cx + 2, by, cx - 1, by + 6, TFT_WHITE);
        tft.fillTriangle(cx - 1, by + 5, cx + 4, by + 5, cx + 1, by + 12, TFT_YELLOW);
    }

    void drawPixelWind(int x, int y, int size) {
        // Background: turquoise/green
        tft.fillRect(x, y, size, size, 0x07FD); // Turquoise
        
        // Swirly wind lines (white, curved pixel-art)
        uint16_t windColor = TFT_WHITE;
        int cy = y + size / 2;
        
        // Top swirl
        tft.drawFastHLine(x + 3, cy - 5, size - 10, windColor);
        tft.drawPixel(x + size - 7, cy - 6, windColor);
        tft.drawPixel(x + size - 6, cy - 6, windColor);
        tft.drawFastVLine(x + size - 6, cy - 6, 3, windColor);
        
        // Middle swirl
        tft.drawFastHLine(x + 5, cy, size - 8, windColor);
        
        // Bottom swirl
        tft.drawFastHLine(x + 3, cy + 5, size - 10, windColor);
        tft.drawPixel(x + size - 7, cy + 6, windColor);
        tft.drawPixel(x + size - 6, cy + 6, windColor);
        tft.drawFastVLine(x + size - 6, cy + 4, 3, windColor);
    }

    String formatHour12(int hour) {
        int h = hour % 12;
        if (h == 0) h = 12;
        bool pm = (hour % 24) >= 12;
        // yields e.g. "12am", "2am", "12pm", "2pm"
        return String(h) + (pm ? "pm" : "am");
    }

    // Weather icons display (pixel-art style)
    void showWeatherIcons(const uint8_t codes[6]) {
        // Draw 6 icons across the width, below the date line
        const int slotW = Lw / 6;
        const int iconSize = 28; // Square pixel-art icons
        const int baseY = WEATHER_BASE_Y;
        tft.fillRect(0, baseY - 2, Lw, iconSize + 6, TFT_BLACK);

        for (int i = 0; i < 6; i++) {
            int cx = (slotW * i) + (slotW / 2);
            int x = cx - iconSize / 2;
            int y = baseY;
            WeatherIcon ic = mapWmoToIcon(codes[i]);

            switch (ic) {
                case ICON_SUN:
                    drawPixelSun(x, y, iconSize);
                    break;
                case ICON_PARTLY:
                    drawPixelPartlyCloudy(x, y, iconSize);
                    break;
                case ICON_CLOUD:
                    drawPixelOvercast(x, y, iconSize);
                    break;
                case ICON_RAIN:
                    drawPixelRain(x, y, iconSize);
                    break;
                case ICON_SNOW:
                    drawPixelSnow(x, y, iconSize);
                    break;
                case ICON_THUNDER:
                    drawPixelThunder(x, y, iconSize);
                    break;
                case ICON_FOG:
                    drawPixelFoggy(x, y, iconSize);
                    break;
                case ICON_WIND:
                    drawPixelWind(x, y, iconSize);
                    break;
            }
        }
    }

    // Show weather icons with 12-hour labels below
    void showWeatherIconsWithLabels(const uint8_t codes[6], int startHour) {
        // Draw icons and 12h labels 5px below
        showWeatherIcons(codes);
        const int slotW = Lw / 6;
        const int labelY = WEATHER_BASE_Y + WEATHER_ICON_H + WEATHER_LABEL_GAP; // baseY + iconH + gap
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        for (int i = 0; i < 6; i++) {
            int cx = (slotW * i) + (slotW / 2);
            int hour = (startHour + i * 2) % 24;
            tft.drawCentreString(formatHour12(hour), cx, labelY, 2);
        }
    }
    
    void showStatus(String status) {
        // Only redraw if status text actually changed
        if (status == _lastStatusShown) {
            return;  // Skip redraw if same
        }
        _lastStatusShown = status;
        
        tft.fillRect(0, Lh - STATUS_BAR_HEIGHT, Lw, STATUS_BAR_HEIGHT, TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawCentreString(status, Lw / 2, Lh - STATUS_TEXT_Y_OFFSET, 1);
    }

    void showInstruction(const String& text) {
        tft.fillRect(0, Lh - INSTR_BAR_HEIGHT, Lw, INSTR_BAR_HEIGHT, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        
        int split = text.indexOf('\n');
        if (split < 0) {
            tft.drawCentreString(text, Lw / 2, Lh - (INSTR_BAR_HEIGHT - INSTR_LINE2_Y), 2);
        } else {
            String a = text.substring(0, split);
            String b = text.substring(split + 1);
            tft.drawCentreString(a, Lw / 2, Lh - INSTR_LINE1_Y, 2);
            tft.drawCentreString(b, Lw / 2, Lh - INSTR_LINE2_Y, 2);
        }
    }

    void clearInstructions() {
        tft.fillRect(0, Lh - (INSTR_BAR_HEIGHT + STATUS_BAR_HEIGHT), Lw, (INSTR_BAR_HEIGHT + STATUS_BAR_HEIGHT), TFT_BLACK);
    }

    // Debug overlay helpers for touch areas
    void drawRectOutline(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
        tft.drawRect(x, y, w, h, color);
    }

    void drawTextInArea(uint16_t x, uint16_t y, const char* text, uint16_t color) {
        tft.setTextColor(color, TFT_BLACK);
        tft.drawString(text, x, y, 1);
    }

    void showBrightness(uint16_t rawValue) {
        // Clear the left side area just below the blue line
        tft.fillRect(0, BRIGHTNESS_AREA_Y, 80, BRIGHTNESS_AREA_H, TFT_BLACK);
        // Draw raw sensor value in font 1 (small), blue color
        tft.setTextColor(TFT_BLUE, TFT_BLACK);
        tft.drawString(String(rawValue).c_str(), BRIGHTNESS_TEXT_X, BRIGHTNESS_TEXT_Y, 1);
    }
};