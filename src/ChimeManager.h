#pragma once
#include <Arduino.h>
#include <cmath>

// Forward declarations for interrupt handler
extern volatile bool chimeTimerActive;
extern volatile uint32_t chimePhaseAccumulator;
extern volatile uint32_t chimePhaseIncrement;
extern volatile uint8_t chimeAmplitude;

// Global interrupt handler
void chimeTimerHandler();

// Non-blocking Westminster/Big Ben style chimes using DAC output
// CYD speaker is on GPIO26 (DAC_CHANNEL_2)
class ChimeManager {
    int _speakerPin = 26;  // CYD speaker on GPIO26 (DAC_CHANNEL_2)
    int _lastChimedHour = -1;

    // DAC and timer config
    hw_timer_t* _timer;
    static constexpr uint32_t SAMPLE_RATE = 32000; // 32kHz sample rate for smooth sine interpolation
    static constexpr uint8_t DC_OFFSET = 128; // DAC center point
    mutable uint8_t _volumePercent = 10; // Volume as percentage 0-100

    struct Note { uint16_t freq; uint16_t ms; };

    // Four phrases of the Westminster chime (G4, C5, D5, E5 combinations)
    static const Note PHRASE1[4];
    static const Note PHRASE2[4];
    static const Note PHRASE3[4];
    static const Note PHRASE4[4];

    // Non-blocking playback state machine
    enum PlaybackState {
        IDLE,
        PLAYING_NOTE,
        NOTE_GAP
    };

    PlaybackState _state = IDLE;
    const Note* _currentSequence = nullptr;
    int _sequenceLength = 0;
    int _sequenceIndex = 0;
    int _strikeCount = 0;
    int _strikeIndex = 0;
    bool _inStrikeMode = false;
    
    // Current note playback state
    uint16_t _currentFreq = 0;
    unsigned long _noteStartMs = 0;
    unsigned long _noteDurationMs = 0;
    
    // Chime sequence tracking
    enum ChimePhase {
        PHASE_NONE,
        PHASE_1,
        PHASE_2,
        PHASE_3,
        PHASE_4,
        PHASE_STRIKES,
        PHASE_COMPLETE
    };
    ChimePhase _chimePhase = PHASE_NONE;

    void startNote(uint16_t freq, uint16_t durationMs) {
        _currentFreq = freq;
        _noteStartMs = millis();
        _noteDurationMs = durationMs;
        _state = PLAYING_NOTE;
        
        // Calculate phase increment for frequency
        // phaseIncrement = (freq * 2^32) / sampleRate
        chimePhaseIncrement = ((uint64_t)freq << 32) / SAMPLE_RATE;
        
        // Set amplitude based on volume (0-100% maps to 0-127 for sine wave)
        chimeAmplitude = map(_volumePercent, 0, 100, 0, 127);
        
        // Start timer
        chimeTimerActive = true;
    }

    // Timer interrupt handler for DAC sample generation
    void stopNote() {
        chimeTimerActive = false;
        chimePhaseAccumulator = 0;
        dacWrite(26, DC_OFFSET); // Return to center voltage
    }

    void startNextNote() {
        if (_inStrikeMode) {
            if (_strikeIndex < _strikeCount) {
                startNote(392, 500); // G4 strike
                _strikeIndex++;
                return;
            } else {
                // Strikes complete
                _chimePhase = PHASE_COMPLETE;
                _state = IDLE;
                stopNote();
                return;
            }
        }

        // Regular phrase playback
        if (_sequenceIndex < _sequenceLength) {
            const Note& note = _currentSequence[_sequenceIndex];
            startNote(note.freq, note.ms);
            _sequenceIndex++;
        } else {
            // Phrase complete, move to next phase
            advanceChimePhase();
        }
    }

    void advanceChimePhase() {
        switch (_chimePhase) {
            case PHASE_1:
                _chimePhase = PHASE_2;
                _currentSequence = PHRASE2;
                _sequenceLength = 4;
                _sequenceIndex = 0;
                startNextNote();
                break;
            case PHASE_2:
                _chimePhase = PHASE_3;
                _currentSequence = PHRASE3;
                _sequenceLength = 4;
                _sequenceIndex = 0;
                startNextNote();
                break;
            case PHASE_3:
                _chimePhase = PHASE_4;
                _currentSequence = PHRASE4;
                _sequenceLength = 4;
                _sequenceIndex = 0;
                startNextNote();
                break;
            case PHASE_4:
                _chimePhase = PHASE_STRIKES;
                _inStrikeMode = true;
                _strikeIndex = 0;
                startNextNote();
                break;
            default:
                _chimePhase = PHASE_COMPLETE;
                _state = IDLE;
                stopNote();
                break;
        }
    }

    void startChimeSequence(int strikes) {
        if (_state != IDLE) return; // Already playing
        
        _chimePhase = PHASE_1;
        _currentSequence = PHRASE1;
        _sequenceLength = 4;
        _sequenceIndex = 0;
        _strikeCount = strikes;
        _inStrikeMode = false;
        startNextNote();
    }

public:
    void begin(int speakerPin = 26) {
        _speakerPin = speakerPin;
        
        // Initialize DAC
        dacWrite(_speakerPin, DC_OFFSET);
        
        // Setup hardware timer (Timer 0, prescaler 80 for 1MHz)
        _timer = timerBegin(0, 80, true);
        timerAttachInterrupt(_timer, &chimeTimerHandler, true);
        timerAlarmWrite(_timer, 1000000 / SAMPLE_RATE, true); // Fire at SAMPLE_RATE Hz
        timerAlarmEnable(_timer);
        
        chimeTimerActive = false;
        chimePhaseAccumulator = 0;
        chimePhaseIncrement = 0;
        chimeAmplitude = 0;
    }

    // Must be called frequently from main loop for non-blocking audio generation
    void update() {
        if (_state == IDLE) return;

        unsigned long nowMs = millis();

        if (_state == PLAYING_NOTE) {
            // Check if note duration complete
            if (nowMs - _noteStartMs >= _noteDurationMs) {
                stopNote();
                _state = NOTE_GAP;
                _noteStartMs = nowMs;
                return;
            }
        } else if (_state == NOTE_GAP) {
            // Gap between notes in phrases (80ms for rhythm), between strikes (200ms), between phrases (400ms)
            unsigned long gapMs;
            if (_inStrikeMode) {
                gapMs = 200; // longer gap between strikes
            } else if (_sequenceIndex >= _sequenceLength) {
                gapMs = 400; // longer gap between phrases
            } else {
                gapMs = 80; // normal inter-note gap
            }
            if (nowMs - _noteStartMs >= gapMs) {
                startNextNote();
            }
        }
    }

    bool isPlaying() const {
        return _state != IDLE;
    }

    // Set volume (0-100 percentage)
    void setVolume(uint8_t percent) {
        if (percent > 100) percent = 100;
        _volumePercent = percent;
    }

    // Play Westminster chime for debug (3 strikes)
    void playDebugChime(int strikes = 3) {
        startChimeSequence(strikes);
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
        if (minute == 0 && second < 2 && _lastChimedHour != hour && !isPlaying()) {
            _lastChimedHour = hour;

            int strikes = ((hour + 11) % 12) + 1; // Convert 0-23 -> 1-12
            startChimeSequence(strikes);
        }
    }
};

