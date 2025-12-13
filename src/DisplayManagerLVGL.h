#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <SPI.h>

class DisplayManager {
private:
    TFT_eSPI tft = TFT_eSPI();
    int Lw = 320; 
    int Lh = 240;
    const char* VERSION = "v1.1.0-LVGL9";
    String _lastStatusShown = "";
    
    // LVGL display buffer
    static const uint32_t BUF_SIZE = 320 * 40; // 40 lines buffer
    static lv_color_t buf1[BUF_SIZE];
    static lv_color_t buf2[BUF_SIZE];
    
    // LVGL objects
    lv_display_t *display;
    lv_obj_t *scr;
    lv_obj_t *labelTitle;
    lv_obj_t *labelVersion;
    lv_obj_t *labelTime;
    lv_obj_t *labelDate;
    lv_obj_t *labelStatus;
    lv_obj_t *labelInstruction;
    
    // Display flush callback for LVGL
    static void display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map) {
        uint32_t w = lv_area_get_width(area);
        uint32_t h = lv_area_get_height(area);
        
        DisplayManager* pThis = (DisplayManager*)lv_display_get_user_data(disp);
        if (pThis) {
            // Swap bytes for RGB565 if needed
            lv_draw_sw_rgb565_swap(px_map, w * h);
            pThis->tft.startWrite();
            pThis->tft.setAddrWindow(area->x1, area->y1, w, h);
            pThis->tft.pushPixels((uint16_t*)px_map, w * h);
            pThis->tft.endWrite();
        }
        
        lv_display_flush_ready(disp);
    }

public:
    void begin() {
        // Initialize TFT
        tft.init();
        tft.setRotation(1); // Landscape (320x240)
        Lw = tft.width();
        Lh = tft.height();
        tft.fillScreen(TFT_BLACK);
        
        // Turn on backlight
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
        
        // Initialize LVGL
        lv_init();
        
        // Create display
        display = lv_display_create(Lw, Lh);
        lv_display_set_flush_cb(display, display_flush_cb);
        lv_display_set_buffers(display, buf1, buf2, BUF_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_user_data(display, this);
        
        // Get default screen
        scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    }

    void drawStaticInterface() {
        // Clear existing objects if any
        lv_obj_clean(scr);
        
        // Create title label at top
        labelTitle = lv_label_create(scr);
        lv_label_set_text(labelTitle, "TouchClock");
        lv_obj_set_style_text_color(labelTitle, lv_color_hex(0xFFFF00), 0); // Yellow
        lv_obj_set_style_text_font(labelTitle, &lv_font_montserrat_24, 0);
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
        lv_label_set_text(labelVersion, VERSION);
        lv_obj_set_style_text_color(labelVersion, lv_color_hex(0x0000FF), 0); // Blue
        lv_obj_set_style_text_font(labelVersion, &lv_font_montserrat_12, 0);
        lv_obj_align(labelVersion, LV_ALIGN_TOP_RIGHT, -2, 25);
        
        // Create time label (large, center)
        labelTime = lv_label_create(scr);
        lv_label_set_text(labelTime, "--:--:--");
        lv_obj_set_style_text_color(labelTime, lv_color_white(), 0);
        lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_48, 0);
        lv_obj_align(labelTime, LV_ALIGN_CENTER, 0, -10);
        
        // Create date label (medium, below time)
        labelDate = lv_label_create(scr);
        lv_label_set_text(labelDate, "");
        lv_obj_set_style_text_color(labelDate, lv_color_white(), 0);
        lv_obj_set_style_text_font(labelDate, &lv_font_montserrat_16, 0);
        lv_obj_align(labelDate, LV_ALIGN_CENTER, 0, 55);
        
        // Create status label at bottom
        labelStatus = lv_label_create(scr);
        lv_label_set_text(labelStatus, "");
        lv_obj_set_style_text_color(labelStatus, lv_color_hex(0x888888), 0); // Dark grey
        lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_12, 0);
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
        
        updateHeaderText("TouchClock");
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
        // Only redraw if status text actually changed
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

    // Debug overlay helpers for touch areas
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
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    }

    void showBrightness(uint16_t rawValue) {
        // Create a small label in the top-left corner for brightness value
        static lv_obj_t *labelBrightness = nullptr;
        if (!labelBrightness) {
            labelBrightness = lv_label_create(scr);
            lv_obj_set_style_text_color(labelBrightness, lv_color_hex(0x0000FF), 0);
            lv_obj_set_style_text_font(labelBrightness, &lv_font_montserrat_12, 0);
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

// Static buffer definitions
lv_color_t DisplayManager::buf1[DisplayManager::BUF_SIZE];
lv_color_t DisplayManager::buf2[DisplayManager::BUF_SIZE];
