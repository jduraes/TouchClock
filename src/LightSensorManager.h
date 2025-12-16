#pragma once
#include <Arduino.h>

// Forward declaration
class DisplayManager;

class LightSensorManager {
private:
    // Light sensor on ADC pin (GPIO 34 on ESP32-2432S028 CYD)
    static const uint8_t LIGHT_SENSOR_PIN = 34;
    static const uint16_t ADC_MAX_VALUE = 4095;      // 12-bit ADC
    static const uint16_t BRIGHTNESS_THRESHOLD = 80; // Percentage increase to trigger screen off
    static const uint32_t SAMPLE_INTERVAL = 500;     // Sample every 500ms
    static const uint8_t SAMPLE_COUNT_10SEC = 20;    // 20 samples × 500ms = 10 seconds
    static const uint8_t SAMPLE_COUNT_5SEC = 10;     // 10 samples × 500ms = 5 seconds
    static const uint16_t DARKNESS_THRESHOLD = 30;   // Turn screen off when sensor reads BELOW this (very bright)

    TaskHandle_t _lightTaskHandle;
    DisplayManager* _display;
    void (*_brightnessCallback)(uint16_t);  // Callback for brightness updates
    
    // Light level tracking
    uint16_t _lightSamples10Sec[SAMPLE_COUNT_10SEC];  // 10-second rolling buffer
    uint8_t _sampleIndex;
    uint16_t _baselineLight;       // Baseline ambient light level
    uint16_t _currentAverage5Sec;  // 5-second rolling average (for screen brightness control)
    uint16_t _currentAverage10Sec; // 10-second rolling average (absolute brightness display)
    uint16_t _latestRawReading;    // Most recent raw sensor reading
    bool _screenOn;
    unsigned long _lastSampleTime;
    
    // Screen-off debouncing
    unsigned long _brightLightStartTime;  // When sensor first dropped below threshold
    unsigned long _lastScreenCheckTime;   // Last time we checked screen-off condition
    static const uint32_t BRIGHT_LIGHT_DEBOUNCE_MS = 2000;  // 2 seconds debounce
    static const uint32_t SCREEN_CHECK_INTERVAL_MS = 500;   // Check screen condition every 500ms

    static void lightTaskWrapper(void* pvParameters) {
        LightSensorManager* pThis = static_cast<LightSensorManager*>(pvParameters);
        pThis->lightTaskLoop();
        vTaskDelete(nullptr);
    }

    void lightTaskLoop() {
        // Initialize baseline on first run
        _baselineLight = readLightLevel();
        _currentAverage5Sec = _baselineLight;
        _currentAverage10Sec = _baselineLight;
        _latestRawReading = _baselineLight;
        for (int i = 0; i < SAMPLE_COUNT_10SEC; i++) {
            _lightSamples10Sec[i] = _baselineLight;
        }
        _sampleIndex = 0;

        Serial.printf("LightSensor baseline: %d\n", _baselineLight);

        unsigned long lastDebugPrint = 0;

        while (1) {
            unsigned long now = millis();

            if (now - _lastSampleTime >= SAMPLE_INTERVAL) {
                _lastSampleTime = now;

                // Read new sample and add to rolling buffer
                uint16_t newSample = readLightLevel();
                _latestRawReading = newSample;
                _lightSamples10Sec[_sampleIndex] = newSample;
                _sampleIndex = (_sampleIndex + 1) % SAMPLE_COUNT_10SEC;

                // Calculate 5-second rolling average (first 10 samples)
                uint32_t sum5Sec = 0;
                for (int i = 0; i < SAMPLE_COUNT_5SEC; i++) {
                    sum5Sec += _lightSamples10Sec[i];
                }
                _currentAverage5Sec = sum5Sec / SAMPLE_COUNT_5SEC;

                // Calculate 10-second rolling average (all 20 samples)
                uint32_t sum10Sec = 0;
                for (int i = 0; i < SAMPLE_COUNT_10SEC; i++) {
                    sum10Sec += _lightSamples10Sec[i];
                }
                _currentAverage10Sec = sum10Sec / SAMPLE_COUNT_10SEC;

                // Call brightness callback with 5-second average for RGB LED
                if (_brightnessCallback) {
                    _brightnessCallback(_currentAverage5Sec);
                }

                // Screen-off logic: Turn screen off when bright light detected for 2+ seconds
                if (_screenOn) {  // Only check if screen is currently on
                    if (now - _lastScreenCheckTime >= SCREEN_CHECK_INTERVAL_MS) {
                        _lastScreenCheckTime = now;
                        
                        if (_latestRawReading < DARKNESS_THRESHOLD) {  // Sensor reads BELOW 30 = very bright
                            if (_brightLightStartTime == 0) {
                                _brightLightStartTime = now;  // Record when bright light first detected
                            } else if (now - _brightLightStartTime >= BRIGHT_LIGHT_DEBOUNCE_MS) {
                                turnScreenOff();  // 2 seconds of sustained brightness
                            }
                        } else {
                            _brightLightStartTime = 0;  // Reset debounce timer if brightness goes away
                        }
                    }
                }

                // Debug: print values every 2 seconds
                if (now - lastDebugPrint >= 2000) {
                    lastDebugPrint = now;
                    Serial.printf("DEBUG Light: raw=%d, avg5s=%d, avg10s=%d, threshold=%d, screen=%s\n", 
                        _latestRawReading, _currentAverage5Sec, _currentAverage10Sec, DARKNESS_THRESHOLD,
                        _screenOn ? "ON" : "OFF");
                }
            }

            // Poll interval: 100ms (actual sampling at SAMPLE_INTERVAL)
            delay(100);
        }
    }

    uint16_t readLightLevel() {
        // Read ADC and average a few readings for stability
        uint32_t sum = 0;
        const int READINGS = 5;
        for (int i = 0; i < READINGS; i++) {
            sum += analogRead(LIGHT_SENSOR_PIN);
            delayMicroseconds(100);
        }
        return sum / READINGS;
    }

    void turnScreenOff() {
        if (_screenOn) {  // Only turn off if currently on
            Serial.println("SCREEN OFF - Bright light detected");
            _screenOn = false;
            digitalWrite(TFT_BL, LOW);  // Turn off backlight
        }
    }

public:
    LightSensorManager()
        : _lightTaskHandle(nullptr),
          _display(nullptr),
          _brightnessCallback(nullptr),
          _sampleIndex(0),
          _baselineLight(0),
          _currentAverage5Sec(0),
          _currentAverage10Sec(0),
          _latestRawReading(0),
          _screenOn(true),
          _lastSampleTime(0),
          _brightLightStartTime(0),
          _lastScreenCheckTime(0) {}

    ~LightSensorManager() {
        if (_lightTaskHandle) {
            vTaskDelete(_lightTaskHandle);
        }
    }

    void begin(DisplayManager* display, void (*brightnessCallback)(uint16_t) = nullptr) {
        _display = display;
        _brightnessCallback = brightnessCallback;

        // Configure ADC for light sensor
        pinMode(LIGHT_SENSOR_PIN, INPUT);
        analogSetAttenuation(ADC_6db);   // Reduced sensitivity (0-2V range instead of 0-3.3V)
        analogSetWidth(12);              // 12-bit resolution

        // Create and pin light polling task to Core 1
        xTaskCreatePinnedToCore(
            lightTaskWrapper,
            "LightTask",
            2048,                  // Stack size (bytes)
            this,                  // Task parameter
            1,                     // Priority (low, below touch)
            &_lightTaskHandle,
            1                      // Core 1
        );

        Serial.println("LightSensorManager initialized on Core 1");
    }

    // Called by TouchManager when screen is off and user touches
    void wakeScreenFromTouch() {
        if (!_screenOn) {
            Serial.println("SCREEN ON - Woken by touch");
            _screenOn = true;
            digitalWrite(TFT_BL, HIGH);  // Turn on backlight
            
            // Reset baseline to current light level when waking
            _baselineLight = _currentAverage5Sec;
            Serial.printf("LightSensor baseline reset to: %d\n", _baselineLight);
            
            // Reset bright-light debounce timer
            _brightLightStartTime = 0;
        }
    }

    bool isScreenOn() const {
        return _screenOn;
    }

    uint16_t getLightLevel() const {
        return _currentAverage10Sec;  // Return 10-second average for display
    }

    uint16_t getLightLevelRaw() const {
        return _latestRawReading;  // Return raw latest reading
    }

    uint16_t getBaseline() const {
        return _baselineLight;
    }
};
