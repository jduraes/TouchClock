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
        tft.fillRect(0, 0, Lw, 50, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawCentreString(text, Lw / 2, 10, 4);
        tft.drawFastHLine(0, 40, Lw, TFT_BLUE);

        // Draw version in tiny blue font at top right, above the blue line
        tft.setTextColor(TFT_BLUE, TFT_BLACK);
        tft.drawString(appVersion(), Lw - 35, 25, 1);
    }

    void updateClock(String timeStr) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawCentreString(timeStr, Lw / 2, 80, 7);
    }
    
    void updateDate(String dateStr) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString(dateStr, Lw / 2, 135, 2);
    }

    const char* codeToGlyph(uint8_t code) {
        // Map WMO weather codes to short ASCII glyphs
        if (code == 0) return "SUN";             // Clear
        if (code == 1 || code == 2) return "PCLD"; // Partly cloudy
        if (code == 3) return "CLD";              // Overcast
        if (code == 45 || code == 48) return "FG"; // Fog
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "RAIN"; // Drizzle/rain
        if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return "SNW";   // Snow
        if (code >= 95) return "TSTM";            // Thunder
        return "WND";                             // Default windy/other
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

    void drawSun(int cx, int cy, int r) {
        tft.fillCircle(cx, cy, r, TFT_YELLOW);
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4.0f;
            int x1 = cx + (int)((r + 2) * cosf(a));
            int y1 = cy + (int)((r + 2) * sinf(a));
            int x2 = cx + (int)((r + 8) * cosf(a));
            int y2 = cy + (int)((r + 8) * sinf(a));
            tft.drawLine(x1, y1, x2, y2, TFT_ORANGE);
        }
    }

    void drawCloud(int x, int y, int w, int h, uint16_t color = TFT_LIGHTGREY) {
        int r = h / 2;
        // Base rectangle
        tft.fillRoundRect(x, y + h / 4, w, h / 2, h / 4, color);
        // Puffs
        tft.fillCircle(x + r, y + h / 2, r, color);
        tft.fillCircle(x + w / 2, y + h / 3, r, color);
        tft.fillCircle(x + w - r, y + h / 2, r, color);
    }

    void drawRainDrops(int x, int y, int w, int h) {
        int step = w / 4;
        for (int i = 1; i <= 3; i++) {
            int dx = x + i * step;
            tft.drawLine(dx, y, dx - 3, y + 10, TFT_CYAN);
        }
    }

    void drawSnowFlakes(int x, int y, int w, int h) {
        int step = w / 4;
        for (int i = 1; i <= 3; i++) {
            int cx = x + i * step;
            int cy = y + 6;
            tft.drawLine(cx - 3, cy, cx + 3, cy, TFT_WHITE);
            tft.drawLine(cx, cy - 3, cx, cy + 3, TFT_WHITE);
            tft.drawLine(cx - 2, cy - 2, cx + 2, cy + 2, TFT_WHITE);
            tft.drawLine(cx - 2, cy + 2, cx + 2, cy - 2, TFT_WHITE);
        }
    }

    void drawBolt(int x, int y) {
        // Simple zig-zag bolt
        tft.fillTriangle(x, y, x + 8, y + 2, x + 2, y + 12, TFT_YELLOW);
        tft.fillTriangle(x + 2, y + 10, x + 12, y + 12, x + 4, y + 24, TFT_YELLOW);
    }

    void drawFog(int x, int y, int w) {
        tft.drawLine(x, y, x + w, y, TFT_LIGHTGREY);
        tft.drawLine(x, y + 6, x + w, y + 6, TFT_LIGHTGREY);
        tft.drawLine(x, y + 12, x + w, y + 12, TFT_LIGHTGREY);
    }

    void drawWind(int x, int y, int w) {
        tft.drawLine(x, y, x + w - 8, y, TFT_SKYBLUE);
        tft.drawLine(x + 6, y + 6, x + w, y + 6, TFT_SKYBLUE);
    }

    void showWeatherIcons(const uint8_t codes[6]) {
        // Draw 6 icons across the width, below the date line
        const int slotW = Lw / 6;
        const int iconW = 36;
        const int iconH = 26;
        const int baseY = 150; // top of icons row
        tft.fillRect(0, baseY - 2, Lw, iconH + 6, TFT_BLACK);

        for (int i = 0; i < 6; i++) {
            int cx = (slotW * i) + (slotW / 2);
            int x = cx - iconW / 2;
            int y = baseY;
            WeatherIcon ic = mapWmoToIcon(codes[i]);

            switch (ic) {
                case ICON_SUN:
                    drawSun(cx, y + 12, 8);
                    break;
                case ICON_PARTLY:
                    drawSun(cx - 8, y + 8, 6);
                    drawCloud(x + 6, y + 6, iconW - 8, 16);
                    break;
                case ICON_CLOUD:
                    drawCloud(x + 4, y + 6, iconW - 8, 16);
                    break;
                case ICON_RAIN:
                    drawCloud(x + 4, y + 4, iconW - 8, 16);
                    drawRainDrops(x + 8, y + 16, iconW - 16, 10);
                    break;
                case ICON_SNOW:
                    drawCloud(x + 4, y + 4, iconW - 8, 16);
                    drawSnowFlakes(x + 8, y + 16, iconW - 16, 10);
                    break;
                case ICON_THUNDER:
                    drawCloud(x + 4, y + 4, iconW - 8, 16);
                    drawBolt(cx - 4, y + 12);
                    break;
                case ICON_FOG:
                    drawFog(x + 4, y + 8, iconW - 8);
                    drawFog(x + 4, y + 14, iconW - 8);
                    break;
                case ICON_WIND:
                    drawWind(x + 4, y + 10, iconW - 8);
                    break;
            }
        }
    }
    
    void showStatus(String status) {
        // Only redraw if status text actually changed
        if (status == _lastStatusShown) {
            return;  // Skip redraw if same
        }
        _lastStatusShown = status;
        
        tft.fillRect(0, Lh - 30, Lw, 30, TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawCentreString(status, Lw / 2, Lh - 18, 1);
    }

    void showInstruction(const String& text) {
        tft.fillRect(0, Lh - 50, Lw, 50, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        
        int split = text.indexOf('\n');
        if (split < 0) {
            tft.drawCentreString(text, Lw / 2, Lh - 35, 2);
        } else {
            String a = text.substring(0, split);
            String b = text.substring(split + 1);
            tft.drawCentreString(a, Lw / 2, Lh - 45, 2);
            tft.drawCentreString(b, Lw / 2, Lh - 25, 2);
        }
    }

    void clearInstructions() {
        tft.fillRect(0, Lh - 60, Lw, 60, TFT_BLACK);
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
        tft.fillRect(0, 42, 80, 20, TFT_BLACK);
        // Draw raw sensor value in font 1 (small), blue color
        tft.setTextColor(TFT_BLUE, TFT_BLACK);
        tft.drawString(String(rawValue).c_str(), 2, 43, 1);
    }
};