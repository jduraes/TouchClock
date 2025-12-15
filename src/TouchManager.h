#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Forward declaration
class DisplayManager;

// Touch event structure for inter-core communication
struct TouchEvent {
    uint16_t x;
    uint16_t y;
    uint32_t timestamp;
};

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
    QueueHandle_t _eventQueue;
    TaskHandle_t _touchTaskHandle;
    DisplayManager* _display;
    bool _debugMode;
    uint8_t _versionPressCount;
    unsigned long _lastVersionPressTime;
    uint8_t _titlePressCount;
    unsigned long _lastTitlePressTime;
    bool _titleIsCopyright;

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

    // Static wrapper for FreeRTOS task
    static void touchTaskWrapper(void* pvParameters) {
        TouchManager* pThis = static_cast<TouchManager*>(pvParameters);
        pThis->touchTaskLoop();
        vTaskDelete(nullptr);
    }

    void touchTaskLoop() {
        while (1) {
            // Poll touch input at ~100Hz
            if (_ts->tirqTouched() && _ts->touched()) {
                TS_Point p = _ts->getPoint();

                // Calibrate raw coordinates to logical display coordinates
                uint16_t touchX = map(p.x, TS_MINX, TS_MAXX, 0, 320);
                uint16_t touchY = map(p.y, TS_MINY, TS_MAXY, 0, 240);

                // Create and queue the touch event
                TouchEvent event = {
                    .x = touchX,
                    .y = touchY,
                    .timestamp = millis()
                };

                // Use queue to pass event to Core 0 safely
                xQueueSend(_eventQueue, &event, 0);

                // Debounce delay
                delay(100);
            }

            // Poll interval: 10ms = ~100Hz touch polling rate
            delay(10);
        }
    }

    bool isTouchInArea(const TouchEvent& event, const TouchArea& area) {
        return event.x >= area.x1 && event.x <= area.x2 &&
               event.y >= area.y1 && event.y <= area.y2;
    }

    void drawDebugOverlay() {
        // Draw rectangles for all touch areas
        for (int i = 0; i < AREA_COUNT; i++) {
            TouchArea& area = _touchAreas[i];
            uint16_t color = TFT_GREEN;
            
            // Draw outline rectangle
            _display->drawRectOutline(area.x1, area.y1, area.x2 - area.x1, area.y2 - area.y1, color);
            
            // Draw label inside (tiny font)
            _display->drawTextInArea(area.x1 + 2, area.y1 + 2, area.label, TFT_GREEN);
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
          _eventQueue(nullptr),
          _touchTaskHandle(nullptr),
          _display(nullptr),
          _debugMode(false),
          _versionPressCount(0),
          _lastVersionPressTime(0),
          _titlePressCount(0),
          _lastTitlePressTime(0),
          _titleIsCopyright(false) {}

    ~TouchManager() {
        if (_touchTaskHandle) {
            vTaskDelete(_touchTaskHandle);
        }
        if (_eventQueue) {
            vQueueDelete(_eventQueue);
        }
        if (_ts) delete _ts;
        if (_spi) delete _spi;
    }

    void begin(DisplayManager* display) {
        _display = display;

        // Initialize SPI for touch controller on HSPI to avoid contention with TFT VSPI bus
        _spi = new SPIClass(HSPI);
        _spi->begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

        // Initialize XPT2046 touchscreen
        _ts = new XPT2046_Touchscreen(XPT2046_CS, XPT2046_IRQ);
        _ts->begin(*_spi);
        _ts->setRotation(1);  // Match display rotation

        // Create FreeRTOS queue for touch events (10 events max)
        _eventQueue = xQueueCreate(10, sizeof(TouchEvent));
        if (!_eventQueue) {
            Serial.println("ERROR: Failed to create touch event queue");
            return;
        }

        // Create and pin touch polling task to Core 1
        xTaskCreatePinnedToCore(
            touchTaskWrapper,      // Task function
            "TouchTask",           // Task name
            4096,                  // Stack size (bytes)
            this,                  // Task parameter (pointer to this)
            2,                     // Priority (medium)
            &_touchTaskHandle,     // Task handle output
            1                      // Core 1
        );

        Serial.println("TouchManager initialized on Core 1");
    }

    void update() {
        // This should be called from Core 0 (main loop)
        TouchEvent event;

        while (xQueueReceive(_eventQueue, &event, 0) == pdTRUE) {
            handleTouchEvent(event);
        }
    }

    void handleTouchEvent(const TouchEvent& event) {
        // Check which area was touched
        for (int i = 0; i < AREA_COUNT; i++) {
            if (isTouchInArea(event, _touchAreas[i])) {
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

    bool isDebugMode() const {
        return _debugMode;
    }

    bool hasPendingEvents() const {
        return uxQueueMessagesWaiting(_eventQueue) > 0;
    }

    // Add a new touch area dynamically
    void addTouchArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                      const char* label, TouchAreaId id) {
        // Note: Would need to expand _touchAreas or use a vector
        // For now, fixed at 1 area for simplicity
    }
};
