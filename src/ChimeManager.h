#pragma once
#include <Arduino.h>
#include <driver/i2s.h>

// Simple Westminster/Big Ben style chime using I2S DAC output for CYD speaker.
// CYD speaker is on GPIO26 connected to an amplifier; requires I2S DAC mode.
class ChimeManager {
    int _speakerPin = 26;                      // CYD speaker pin (connected to amp)
    int _lastChimedHour = -1;

    struct Note { uint16_t freq; uint16_t ms; };

    // Four phrases of the Westminster chime (approximate pitches)
    static const Note PHRASE1[4];
    static const Note PHRASE2[4];
    static const Note PHRASE3[4];
    static const Note PHRASE4[4];

    void playNote(uint16_t freq, uint16_t ms) {
        Serial.printf("Playing note: %d Hz for %d ms\n", freq, ms);
        // Generate tone via I2S DAC (simple square wave approximation)
        unsigned long startMs = millis();
        int halfPeriodUs = (500000 / freq); // Half period in microseconds
        
        while (millis() - startMs < ms) {
            dacWrite(_speakerPin, 200); // High level
            delayMicroseconds(halfPeriodUs);
            dacWrite(_speakerPin, 55);  // Low level
            delayMicroseconds(halfPeriodUs);
        }
        dacWrite(_speakerPin, 128); // Center/silent
        delay(40); // Small gap
    }

    void playPhrase(const Note* phrase) {
        for (int i = 0; i < 4; i++) {
            playNote(phrase[i].freq, phrase[i].ms);
        }
    }

    void playHourStrikes(int strikes) {
        for (int i = 0; i < strikes; i++) {
            playNote(392, 380); // G4 strike
            delay(120);
        }
    }

public:
    void begin(int speakerPin = 26) {
        _speakerPin = speakerPin;
        // No setup needed for DAC - dacWrite handles it
        Serial.printf("ChimeManager: speaker on GPIO%d (DAC mode)\n", _speakerPin);
    }

    // Explicitly play the full Westminster chime followed by a custom strike count (for debug/manual triggers)
    void playDebugChime(int strikes = 3) {
        Serial.printf("ChimeManager: playDebugChime with %d strikes\n", strikes);
        playPhrase(PHRASE1);
        playPhrase(PHRASE2);
        playPhrase(PHRASE3);
        playPhrase(PHRASE4);
        playHourStrikes(strikes);
        Serial.println("ChimeManager: playDebugChime complete");
    }

    // Call frequently with current local time; will self-debounce to once per hour.
    void maybeChime(const tm& timeinfo) {
        int hour = timeinfo.tm_hour;
        int minute = timeinfo.tm_min;
        int second = timeinfo.tm_sec;

        // Quiet hours outside 08:00-21:59 (inclusive of 21:59)
        if (hour < 8 || hour >= 22) {
            return;
        }

        // Only fire at the top of the hour and only once per hour
        if (minute == 0 && second < 2 && _lastChimedHour != hour) {
            _lastChimedHour = hour;
            Serial.printf("ChimeManager: hourly chime at %02d:00\n", hour);

            // Full Westminster sequence (phrases 1-4) then hour strikes (12-hour format)
            playPhrase(PHRASE1);
            playPhrase(PHRASE2);
            playPhrase(PHRASE3);
            playPhrase(PHRASE4);

            int strikes = ((hour + 11) % 12) + 1; // Convert 0-23 -> 1-12
            playHourStrikes(strikes);
            Serial.println("ChimeManager: hourly chime complete");
        }
    }
};

