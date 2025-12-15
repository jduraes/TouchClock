#pragma once
#include <Arduino.h>

// Simple Westminster/Big Ben style chime using LEDC tone output.
// Uses a dedicated LEDC channel and toggles once per hour between 08:00-22:00.
class ChimeManager {
    static const uint8_t LEDC_CHANNEL = 4;     // Free channel (0-2 used by RGB LED)
    static const uint8_t LEDC_TIMER_BITS = 10; // Resolution for tone output
    int _speakerPin = 25;                      // Default DAC/speaker pin on CYD
    int _lastChimedHour = -1;

    struct Note { uint16_t freq; uint16_t ms; };

    // Four phrases of the Westminster chime (approximate pitches)
    static const Note PHRASE1[4];
    static const Note PHRASE2[4];
    static const Note PHRASE3[4];
    static const Note PHRASE4[4];

    void playNote(uint16_t freq, uint16_t ms) {
        ledcWriteTone(LEDC_CHANNEL, freq);
        delay(ms);
        ledcWriteTone(LEDC_CHANNEL, 0); // Stop tone
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
    void begin(int speakerPin = 25) {
        _speakerPin = speakerPin;
        ledcAttachPin(_speakerPin, LEDC_CHANNEL);
        ledcSetup(LEDC_CHANNEL, 1000, LEDC_TIMER_BITS); // Base setup; freq overridden per note
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

            // Full Westminster sequence (phrases 1-4) then hour strikes (12-hour format)
            playPhrase(PHRASE1);
            playPhrase(PHRASE2);
            playPhrase(PHRASE3);
            playPhrase(PHRASE4);

            int strikes = ((hour + 11) % 12) + 1; // Convert 0-23 -> 1-12
            playHourStrikes(strikes);
        }
    }
};

