#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

class DisplayManager {
    TFT_eSPI tft = TFT_eSPI();
    int Lw = 320; 
    int Lh = 240; 

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
        tft.fillRect(0, 0, Lw, 50, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawCentreString("TouchClock", Lw / 2, 10, 4);
        tft.drawFastHLine(0, 40, Lw, TFT_BLUE);
    }

    void updateClock(String timeStr) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawCentreString(timeStr, Lw / 2, 85, 7);
    }
    
    void updateDate(String dateStr) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawCentreString(dateStr, Lw / 2, 140, 2);
    }
    
    void showStatus(String status) {
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
    }};