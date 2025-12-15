#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "AppVersion.h"

class DisplayManager {
private:
    int Lw = 320; 
    int Lh = 240;
    String _lastStatusShown = "";
    
    // LVGL objects
    lv_obj_t *scr;
    lv_obj_t *labelTitle;
    lv_obj_t *labelVersion;
    lv_obj_t *labelTime;
    lv_obj_t *labelDate;
    lv_obj_t *labelStatus;
    lv_obj_t *labelInstruction;
    
    // Draw buffer (allocated on heap, 1/10 screen)
    static const uint32_t DRAW_BUF_SIZE = (320 * 240 / 10 * (LV_COLOR_DEPTH / 8));

public:
    void begin() {
        Serial.println("[Display] Initializing LVGL with TFT_eSPI...");
        
        // Initialize LVGL
        lv_init();
        
        // Allocate draw buffer on heap
        uint8_t* draw_buf = new uint8_t[DRAW_BUF_SIZE];
        if (!draw_buf) {
            Serial.println("[Display] ERROR: Failed to allocate draw buffer!");
            return;
        }
        
        // Create display using TFT_eSPI integration
        lv_display_t *disp = lv_tft_espi_create(320, 240, draw_buf, DRAW_BUF_SIZE);
        if (!disp) {
            Serial.println("[Display] ERROR: Failed to create LVGL display!");
            return;
        }
        // Ensure correct orientation for CYD (landscape)
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
        
        // Get screen and set background
        scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        
        Serial.println("[Display] LVGL initialization complete");
    }

    void drawStaticInterface() {
        // Clear existing objects
        lv_obj_clean(scr);
        
        // Create title label at top
        labelTitle = lv_label_create(scr);
        lv_label_set_text(labelTitle, "TouchClock");
        lv_obj_set_style_text_color(labelTitle, lv_color_hex(0xFFFF00), 0); // Yellow
        lv_obj_set_style_text_font(labelTitle, &lv_font_montserrat_14, 0);
        lv_obj_align(labelTitle, LV_ALIGN_TOP_MID, 0, 10);
        
        // Create horizontal line below title
        lv_obj_t *line = lv_obj_create(scr);
        lv_obj_set_size(line, Lw, 2);
        lv_obj_set_pos(line, 0, 40);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x0000FF), 0); // Blue
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_set_style_radius(line, 0, 0);
        
        // Create version label (top right)
        labelVersion = lv_label_create(scr);
        lv_label_set_text(labelVersion, appVersion());
        lv_obj_align(labelVersion, LV_ALIGN_TOP_RIGHT, -2, 25);
        
        // Create time label (large, center)
        labelTime = lv_label_create(scr);
        lv_label_set_text(labelTime, "--:--:--");
        lv_obj_set_style_text_color(labelTime, lv_color_white(), 0);
        lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_14, 0);
        lv_obj_align(labelTime, LV_ALIGN_CENTER, 0, -10);
        
        // Create date label (medium, below time)
        labelDate = lv_label_create(scr);
        lv_label_set_text(labelDate, "");
        lv_obj_set_style_text_color(labelDate, lv_color_white(), 0);
        lv_obj_set_style_text_font(labelDate, &lv_font_montserrat_14, 0);
        lv_obj_align(labelDate, LV_ALIGN_CENTER, 0, 55);
        
        // Create status label at bottom
        labelStatus = lv_label_create(scr);
        lv_label_set_text(labelStatus, "");
        lv_obj_set_style_text_color(labelStatus, lv_color_hex(0x888888), 0); // Dark grey
        lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_14, 0);
        lv_obj_align(labelStatus, LV_ALIGN_BOTTOM_MID, 0, -5);
        
        // Create instruction label (hidden by default)
        labelInstruction = lv_label_create(scr);
        lv_label_set_text(labelInstruction, "");
        lv_obj_set_style_text_color(labelInstruction, lv_color_white(), 0);
        lv_obj_set_style_text_font(labelInstruction, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(labelInstruction, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(labelInstruction, Lw - 20);
        lv_obj_align(labelInstruction, LV_ALIGN_BOTTOM_MID, 0, -35);
        lv_obj_add_flag(labelInstruction, LV_OBJ_FLAG_HIDDEN);
    }

    void updateHeaderText(const String& text) {
        if (labelTitle) {
            lv_label_set_text(labelTitle, text.c_str());
            lv_obj_align(labelTitle, LV_ALIGN_TOP_MID, 0, 10);
        }
    }

    void updateClock(String timeStr) {
        if (labelTime) {
            lv_label_set_text(labelTime, timeStr.c_str());
            lv_obj_align(labelTime, LV_ALIGN_CENTER, 0, -10);
        }
    }
    
    void updateDate(String dateStr) {
        if (labelDate) {
            lv_label_set_text(labelDate, dateStr.c_str());
            lv_obj_align(labelDate, LV_ALIGN_CENTER, 0, 55);
        }
    }
    
    void showStatus(String status) {
        if (status == _lastStatusShown) {
            return;
        }
        _lastStatusShown = status;
        
        if (labelStatus) {
            lv_label_set_text(labelStatus, status.c_str());
            lv_obj_align(labelStatus, LV_ALIGN_BOTTOM_MID, 0, -5);
        }
    }

    void showInstruction(const String& text) {
        if (labelInstruction) {
            lv_label_set_text(labelInstruction, text.c_str());
            lv_obj_clear_flag(labelInstruction, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(labelInstruction, LV_ALIGN_BOTTOM_MID, 0, -35);
        }
    }

    void clearInstructions() {
        if (labelInstruction) {
            lv_obj_add_flag(labelInstruction, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void drawRectOutline(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
        lv_obj_t *rect = lv_obj_create(scr);
        lv_obj_set_size(rect, w, h);
        lv_obj_set_pos(rect, x, y);
        lv_obj_set_style_bg_opa(rect, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(rect, lv_color_hex(color), 0);
        lv_obj_set_style_border_width(rect, 2, 0);
        lv_obj_set_style_radius(rect, 0, 0);
    }

    void drawTextInArea(uint16_t x, uint16_t y, const char* text, uint16_t color) {
        lv_obj_t *label = lv_label_create(scr);
        lv_label_set_text(label, text);
        lv_obj_set_pos(label, x, y);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    }

    void showBrightness(uint16_t rawValue) {
        static lv_obj_t *labelBrightness = nullptr;
        if (!labelBrightness) {
            labelBrightness = lv_label_create(scr);
            lv_obj_set_style_text_color(labelBrightness, lv_color_hex(0x0000FF), 0);
            lv_obj_set_style_text_font(labelBrightness, &lv_font_montserrat_14, 0);
            lv_obj_set_pos(labelBrightness, 2, 43);
        }
        lv_label_set_text_fmt(labelBrightness, "%d", rawValue);
    }
    
    // LVGL timer handler - must be called regularly
    void update() {
        lv_timer_handler();
    }
    
    // Get LVGL screen for custom widgets
    lv_obj_t* getScreen() {
        return scr;
    }
};
