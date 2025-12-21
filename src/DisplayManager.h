#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "AppVersion.h"
#include "weather_icons.h"

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
    const int DATE_CLEAR_PAD = 2;   // pixels above/below to clear
    const int DATE_CLEAR_HEIGHT = 20; // height of the cleared strip for date

    // Weather icons row
    const int WEATHER_BASE_Y = 150; // top of icons row
    const int WEATHER_ICON_W = 36; // matches bitmap assets in weather_icons.h
    const int WEATHER_ICON_H = 26;
    const int WEATHER_LABEL_GAP = 6; // gap between icons and labels
    const int WEATHER_LABEL_HEIGHT = 14; // height per label line (time and temp)
    const int WEATHER_LABELS_TOTAL_HEIGHT = 30; // total height for both label lines

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
    
    // Special characters
    static constexpr char DEGREE_SYMBOL = 247; // Extended ASCII degree symbol (°)

public:
    void begin() {
        tft.init();
        tft.setSwapBytes(true); // Bitmaps stored in flash use big-endian 565
        
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
        // Clear a strip across the date area to avoid leftover pixels when text becomes shorter
        tft.fillRect(0, DATE_Y - DATE_CLEAR_PAD, Lw, DATE_CLEAR_HEIGHT, TFT_BLACK);
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

    struct IconBitmap {
        const uint16_t* data;
        int w;
        int h;
    };

    enum WeatherIcon { ICON_SUN, ICON_PARTLY, ICON_CLOUD, ICON_RAIN, ICON_SNOW, ICON_THUNDER, ICON_FOG, ICON_WIND };

    // Map our logical icons to PROGMEM bitmaps (Open-Meteo style), each 36x26
    const IconBitmap iconBitmaps[8] = {
        {icon_clear, WEATHER_ICON_W, WEATHER_ICON_H},          // ICON_SUN
        {icon_partly_cloudy, WEATHER_ICON_W, WEATHER_ICON_H},  // ICON_PARTLY
        {icon_overcast, WEATHER_ICON_W, WEATHER_ICON_H},       // ICON_CLOUD
        {icon_rain, WEATHER_ICON_W, WEATHER_ICON_H},           // ICON_RAIN
        {icon_snow, WEATHER_ICON_W, WEATHER_ICON_H},           // ICON_SNOW
        {icon_thunder, WEATHER_ICON_W, WEATHER_ICON_H},        // ICON_THUNDER
        {icon_fog, WEATHER_ICON_W, WEATHER_ICON_H},            // ICON_FOG
        {icon_wind, WEATHER_ICON_W, WEATHER_ICON_H},           // ICON_WIND
    };

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

    String formatHour12(int hour) {
        // Normalize to 0-23 range
        hour = hour % 24;
        if (hour < 0) hour += 24;
        
        // Convert to 12-hour format
        int h = hour % 12;
        if (h == 0) h = 12;
        bool pm = hour >= 12;
        
        // yields e.g. "12am", "2am", "12pm", "2pm"
        return String(h) + (pm ? "pm" : "am");
    }

    // Weather icons display (PROGMEM bitmaps)
    void showWeatherIcons(const uint8_t codes[6]) {
        // Draw 6 icons across the width, below the date line
        const float slotW = Lw / 6.0f;              // use float to center precisely
        const int iconW = WEATHER_ICON_W;
        const int iconH = WEATHER_ICON_H;
        const int baseY = WEATHER_BASE_Y;
        tft.fillRect(0, baseY - 2, Lw, iconH + 6, TFT_BLACK);

        for (int i = 0; i < 6; i++) {
            int cx = (int)round(slotW * (i + 0.5f)); // centre per slot without accumulating truncation
            int x = cx - iconW / 2;
            int y = baseY;
            WeatherIcon ic = mapWmoToIcon(codes[i]);
            const IconBitmap& bmp = iconBitmaps[ic];
            tft.pushImage(x, y, bmp.w, bmp.h, bmp.data);
        }
    }

    // Show weather icons with 12-hour labels below
    void showWeatherIconsWithLabels(const uint8_t codes[6], int startHour) {
        // Draw icons and 12h labels 5px below
        showWeatherIcons(codes);
        const float slotW = Lw / 6.0f;
        const int labelY = WEATHER_BASE_Y + WEATHER_ICON_H + WEATHER_LABEL_GAP; // baseY + iconH + gap
        // Clear the label strip to avoid ghost characters when shorter labels (e.g., "2pm") overwrite longer ones (e.g., "12pm")
        tft.fillRect(0, labelY - 2, Lw, 16, TFT_BLACK);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        for (int i = 0; i < 6; i++) {
            int cx = (int)round(slotW * (i + 0.5f));
            int hour = (startHour + i * 2) % 24;
            tft.drawCentreString(formatHour12(hour), cx, labelY, 2);
        }
    }

    // Show weather icons with 12-hour labels and temperature in Celsius
    void showWeatherIconsWithLabelsAndTemps(const uint8_t codes[6], const float temps[6], int startHour) {
        // Draw icons
        showWeatherIcons(codes);
        const float slotW = Lw / 6.0f;
        const int labelY = WEATHER_BASE_Y + WEATHER_ICON_H + WEATHER_LABEL_GAP;
        const int tempY = labelY + WEATHER_LABEL_HEIGHT;
        
        // Clear the label and temp strip
        tft.fillRect(0, labelY - 2, Lw, WEATHER_LABELS_TOTAL_HEIGHT, TFT_BLACK);
        
        // Draw time labels
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        for (int i = 0; i < 6; i++) {
            int cx = (int)round(slotW * (i + 0.5f));
            int hour = (startHour + i * 2) % 24;
            tft.drawCentreString(formatHour12(hour), cx, labelY, 2);
        }
        
        // Draw temperature labels
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        for (int i = 0; i < 6; i++) {
            int cx = (int)round(slotW * (i + 0.5f));
            // Format temperature as integer with degree symbol (°C)
            String tempStr = String((int)round(temps[i]));
            tempStr += DEGREE_SYMBOL;
            tempStr += "C";
            // Add small offset to visually center (°C adds asymmetry)
            tft.drawCentreString(tempStr, cx + 4, tempY, 2);
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