#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>

// Forward declaration
class DisplayManager;

// Touch area definitions (in logical coordinates after calibration)
enum TouchAreaId {
    TOUCH_TITLE = 0,     // Header text area
    TOUCH_VERSION = 1,   // Top right corner (version label)
    TOUCH_AREA_MAX = 2   // Keep this in sync with area count
};

struct TouchArea {
    uint16_t x1, y1;     // Top-left
    uint16_t x2, y2;     // Bottom-right
    const char* label;   // Debug label
    TouchAreaId id;      // Area identifier
};

class TouchManager {
private:
    // Hardware setup for XPT2046
    static const uint8_t XPT2046_CLK = 25;
    static const uint8_t XPT2046_MOSI = 32;
    static const uint8_t XPT2046_MISO = 39;
    static const uint8_t XPT2046_CS = 33;
    static const uint8_t XPT2046_IRQ = 36;

    // Raw touch calibration values (from reference examples for CYD)
    static const uint16_t TS_MINX = 200;
    static const uint16_t TS_MAXX = 3700;
    static const uint16_t TS_MINY = 240;
    static const uint16_t TS_MAXY = 3800;

    SPIClass* _spi;
    XPT2046_Touchscreen* _ts;
    lv_indev_t* _indev;
    DisplayManager* _display;
    bool _debugMode;
    uint8_t _versionPressCount;
    unsigned long _lastVersionPressTime;
    uint8_t _titlePressCount;
    unsigned long _lastTitlePressTime;
    bool _titleIsCopyright;
    
    // Last touch point for LVGL
    static int16_t last_x;
    static int16_t last_y;
    static bool last_touched;

    // Touch areas configuration (can be extended)
    static const int AREA_COUNT = TOUCH_AREA_MAX;
    TouchArea _touchAreas[AREA_COUNT] = {
        { // Title area roughly covering "TouchClock" text in header
            .x1 = 80, .y1 = 4,
            .x2 = 240, .y2 = 32,
            .label = "Title",
            .id = TOUCH_TITLE
        },
        { // Version label area (top-right tiny text)
            .x1 = 285, .y1 = 20,
            .x2 = 320, .y2 = 35,
            .label = "Version",
            .id = TOUCH_VERSION
        }
    };

    // Static touch read callback for LVGL
    static void touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
        TouchManager* pThis = (TouchManager*)lv_indev_get_user_data(indev);
        if (pThis && pThis->_ts) {
            if (pThis->_ts->tirqTouched() && pThis->_ts->touched()) {
                TS_Point p = pThis->_ts->getPoint();
                
                // Calibrate raw coordinates to logical display coordinates
                last_x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
                last_y = map(p.y, TS_MINY, TS_MAXY, 0, 240);
                last_touched = true;
                
                // Handle custom touch areas (title, version)
                pThis->handleCustomTouch(last_x, last_y);
            } else {
                last_touched = false;
            }
        }
        
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = last_touched ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    }
    
    void handleCustomTouch(int16_t x, int16_t y) {
        // Check which area was touched
        for (int i = 0; i < AREA_COUNT; i++) {
            if (x >= _touchAreas[i].x1 && x <= _touchAreas[i].x2 &&
                y >= _touchAreas[i].y1 && y <= _touchAreas[i].y2) {
                handleAreaTouched(_touchAreas[i]);
                break;
            }
        }
    }

    void handleAreaTouched(const TouchArea& area) {
        unsigned long now = millis();

        if (area.id == TOUCH_VERSION) {
            // Track consecutive presses within a 500ms window
            if (now - _lastVersionPressTime < 500) {
                _versionPressCount++;
            } else {
                _versionPressCount = 1;
            }
            _lastVersionPressTime = now;

            Serial.printf("Version pressed (%d/3)\n", _versionPressCount);

            // On 3rd press within window, toggle debug mode
            if (_versionPressCount >= 3) {
                _debugMode = !_debugMode;
                _versionPressCount = 0;

                if (_debugMode) {
                    drawDebugOverlay();
                    Serial.println("DEBUG MODE ENABLED");
                } else {
                    disableDebugOverlay();
                    Serial.println("DEBUG MODE DISABLED");
                }
            }
        } else if (area.id == TOUCH_TITLE) {
            // Triple-tap to toggle header text between title and copyright notice
            if (now - _lastTitlePressTime < 500) {
                _titlePressCount++;
            } else {
                _titlePressCount = 1;
            }
            _lastTitlePressTime = now;

            Serial.printf("Title pressed (%d/3)\n", _titlePressCount);

            if (_titlePressCount >= 3) {
                _titlePressCount = 0;
                _titleIsCopyright = !_titleIsCopyright;

                if (_titleIsCopyright) {
                    _display->updateHeaderText("(c)2025 Joao Miguel Duraes");
                    Serial.println("Header set to copyright");
                } else {
                    _display->updateHeaderText("TouchClock");
                    Serial.println("Header set to TouchClock");
                }

                // If debug mode is active, refresh overlay to ensure rectangles stay visible
                if (_debugMode) {
                    drawDebugOverlay();
                }
            }
        }
    }

    void drawDebugOverlay() {
        // Draw rectangles for all touch areas
        for (int i = 0; i < AREA_COUNT; i++) {
            TouchArea& area = _touchAreas[i];
            uint16_t color = 0x00FF00; // Green
            
            // Draw outline rectangle
            _display->drawRectOutline(area.x1, area.y1, area.x2 - area.x1, area.y2 - area.y1, color);
            
            // Draw label inside (tiny font)
            _display->drawTextInArea(area.x1 + 2, area.y1 + 2, area.label, 0x00FF00);
        }

        // Draw status message
        _display->showStatus("DEBUG MODE ON - Touch areas shown");
    }

    void disableDebugOverlay() {
        // Redraw the static interface to clear debug overlay
        _display->drawStaticInterface();
    }

public:
    TouchManager()
        : _spi(nullptr),
          _ts(nullptr),
          _indev(nullptr),
          _display(nullptr),
          _debugMode(false),
          _versionPressCount(0),
          _lastVersionPressTime(0),
          _titlePressCount(0),
          _lastTitlePressTime(0),
          _titleIsCopyright(false) {}

    ~TouchManager() {
        if (_ts) delete _ts;
        if (_spi) delete _spi;
    }

    void begin(DisplayManager* display) {
        _display = display;

        // Initialize SPI for touch controller on VSPI
        _spi = new SPIClass(VSPI);
        _spi->begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

        // Initialize XPT2046 touchscreen
        _ts = new XPT2046_Touchscreen(XPT2046_CS, XPT2046_IRQ);
        _ts->begin(*_spi);
        _ts->setRotation(1);  // Match display rotation

        // Create LVGL input device
        _indev = lv_indev_create();
        lv_indev_set_type(_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(_indev, touchpad_read_cb);
        lv_indev_set_user_data(_indev, this);

        Serial.println("TouchManager initialized with LVGL");
    }

    void update() {
        // LVGL handles touch reading automatically via the callback
        // This method kept for API compatibility
    }

    bool isDebugMode() const {
        return _debugMode;
    }

    bool hasPendingEvents() const {
        // Check if touch is currently active
        return last_touched;
    }
};

// Static member initialization
int16_t TouchManager::last_x = 0;
int16_t TouchManager::last_y = 0;
bool TouchManager::last_touched = false;
