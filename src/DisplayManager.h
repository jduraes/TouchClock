#pragma once
#include <TFT_eSPI.h>
#include <SPI.h>

class DisplayManager {
    TFT_eSPI tft = TFT_eSPI();

public:
    void begin() {
        tft.init();
        tft.setRotation(1);
        tft.fillScreen(TFT_BLACK);
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }

    void drawStaticInterface() {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawCentreString("SYSTEM CLOCK", 160, 10, 4);
        tft.drawFastHLine(0, 40, 320, TFT_BLUE);
    }

    void updateClock(String timeStr) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawCentreString(timeStr, 160, 100, 7); 
    }
    
    void showStatus(String status) {
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(status, 10, 220, 2);
    }
};
