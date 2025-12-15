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

    void showWeatherIcons(const uint8_t codes[6]) {
        // Draw 6 glyphs centered across the width, below the date line
        const int y = 160;
        const int slotW = Lw / 6;
        tft.fillRect(0, 148, Lw, 32, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        for (int i = 0; i < 6; i++) {
            int cx = (slotW * i) + (slotW / 2);
            tft.drawCentreString(codeToGlyph(codes[i]), cx, y, 2);
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