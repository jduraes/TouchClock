#pragma once
#include <Arduino.h>

class RGBLedManager {
private:
    // RGB LED pins for ESP32-2432S028 CYD
    static const uint8_t LED_RED = 4;
    static const uint8_t LED_GREEN = 16;
    static const uint8_t LED_BLUE = 17;

    // LEDC PWM configuration
    static const uint8_t LEDC_CHANNEL_RED = 0;
    static const uint8_t LEDC_CHANNEL_GREEN = 1;
    static const uint8_t LEDC_CHANNEL_BLUE = 2;
    static const uint32_t LEDC_FREQUENCY = 1000;  // 1kHz PWM
    static const uint8_t LEDC_RESOLUTION = 8;     // 8-bit PWM (0-255)
    static const uint16_t PWM_MAX = (1 << LEDC_RESOLUTION) - 1;  // 255

    uint16_t _lastBrightness;

    // Rainbow color mapping: 7 colors, each responsible for 1/7th of brightness
    // Maps brightness (0-4095 ADC) to RGB components
    struct RGBColor {
        uint8_t r, g, b;
    };

    void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
        // LED is active LOW - invert the values
        analogWrite(LED_RED, 255 - r);
        analogWrite(LED_GREEN, 255 - g);
        analogWrite(LED_BLUE, 255 - b);
    }

    // Convert brightness (0-4095) to rainbow color
    RGBColor brightnessToRainbow(uint16_t brightness) {
        RGBColor color = {0, 0, 0};
        
        // Normalize brightness to 0-1 range
        float normalized = (float)brightness / 4095.0f;
        
        // 7 color bands in rainbow: Red, Orange, Yellow, Green, Cyan, Blue, Magenta
        // Each band is 1/7th of the spectrum
        float bandPosition = normalized * 7.0f;  // 0 to 7
        int band = (int)bandPosition;
        float bandProgress = bandPosition - band;  // 0 to 1 within the band
        
        switch (band) {
            case 0:  // Red -> Orange (Red=full, Green rises, Blue=0)
                color.r = 255;
                color.g = (uint8_t)(255.0f * bandProgress);
                color.b = 0;
                break;
            case 1:  // Orange -> Yellow (Red=full, Green=full, Blue=0)
                color.r = 255;
                color.g = 255;
                color.b = 0;
                break;
            case 2:  // Yellow -> Green (Red falls, Green=full, Blue=0)
                color.r = (uint8_t)(255.0f * (1.0f - bandProgress));
                color.g = 255;
                color.b = 0;
                break;
            case 3:  // Green -> Cyan (Red=0, Green=full, Blue rises)
                color.r = 0;
                color.g = 255;
                color.b = (uint8_t)(255.0f * bandProgress);
                break;
            case 4:  // Cyan -> Blue (Red=0, Green falls, Blue=full)
                color.r = 0;
                color.g = (uint8_t)(255.0f * (1.0f - bandProgress));
                color.b = 255;
                break;
            case 5:  // Blue -> Magenta (Red rises, Green=0, Blue=full)
                color.r = (uint8_t)(255.0f * bandProgress);
                color.g = 0;
                color.b = 255;
                break;
            case 6:  // Magenta -> Red (Red=full, Green=0, Blue falls)
                color.r = 255;
                color.g = 0;
                color.b = (uint8_t)(255.0f * (1.0f - bandProgress));
                break;
            default:  // Beyond range, default to red
                color.r = 255;
                color.g = 0;
                color.b = 0;
        }
        
        return color;
    }

public:
    RGBLedManager() : _lastBrightness(0xFFFF) {}

    void begin() {
        // Configure LED pins as outputs
        pinMode(LED_RED, OUTPUT);
        pinMode(LED_GREEN, OUTPUT);
        pinMode(LED_BLUE, OUTPUT);

        // Turn off LED (active LOW - write 255 to turn off)
        analogWrite(LED_RED, 255);
        analogWrite(LED_GREEN, 255);
        analogWrite(LED_BLUE, 255);

        Serial.println("RGBLedManager initialized (LED off)");
    }

    // Update LED color based on brightness level
    // Called from LightSensorManager whenever brightness changes
    void updateBrightness(uint16_t brightness) {
        // Only update if brightness changed significantly (avoid flickering)
        // Use 50-unit hysteresis to prevent continuous updates
        if (brightness < _lastBrightness - 50 || brightness > _lastBrightness + 50) {
            _lastBrightness = brightness;
            
            RGBColor color = brightnessToRainbow(brightness);
            setLedColor(color.r, color.g, color.b);

            // Debug output
            Serial.printf("RGB Update: brightness=%d -> RGB(%d, %d, %d)\n", brightness, color.r, color.g, color.b);
        } else {
            // Even if not updating due to hysteresis, show the value occasionally
            static unsigned long lastSerialTime = 0;
            if (millis() - lastSerialTime > 5000) {  // Print every 5 seconds
                lastSerialTime = millis();
                RGBColor color = brightnessToRainbow(brightness);
                Serial.printf("RGB (no change): brightness=%d -> would be RGB(%d, %d, %d)\n", brightness, color.r, color.g, color.b);
            }
        }
    }

    // Turn off LED
    void off() {
        // Active LOW - write 255 to turn off
        analogWrite(LED_RED, 255);
        analogWrite(LED_GREEN, 255);
        analogWrite(LED_BLUE, 255);
    }

    // Test all colors in sequence
    void testRainbow() {
        for (uint16_t i = 0; i <= 4095; i += 256) {
            updateBrightness(i);
            delay(100);
        }
    }
};
