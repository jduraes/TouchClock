#pragma once
#include <Arduino.h>
#include <cmath>

// Forward declarations for interrupt handler
extern volatile bool chimeTimerActive;
extern volatile uint32_t chimePhaseAccumulator;
extern volatile uint32_t chimePhaseIncrement;
extern volatile uint8_t chimeAmplitude;
extern volatile uint32_t chimeSampleCount;
extern volatile uint32_t chimeNoteSampleTarget;
extern volatile bool chimeNoteCompleted;

// Global interrupt handler
void chimeTimerHandler();

// Non-blocking Westminster/Big Ben style chimes using DAC output
// CYD speaker is on GPIO26 (DAC_CHANNEL_2)
class ChimeManager {
    int _speakerPin = 26;  // CYD speaker on GPIO26 (DAC_CHANNEL_2)
    int _lastChimedHour = -1;

    // DAC and timer config
    hw_timer_t* _timer;
    static constexpr uint32_t SAMPLE_RATE = 44000; // 44kHz sample rate for smooth sine interpolation
    static constexpr uint8_t DC_OFFSET = 128; // DAC center point
    mutable uint8_t _volumePercent = 5; // Volume as percentage 0-100

    struct Note { uint16_t freq; uint16_t ms; };

    // Westminster Quarters full-hour sequence (E Major, Big Ben authentic)
    static const Note WESTMINSTER_SEQUENCE[16];
    static const uint16_t HOUR_STRIKE_FREQ;
    static const uint16_t HOUR_STRIKE_DURATION;

    // Non-blocking playback state machine
    enum PlaybackState {
        IDLE,
        PLAYING_NOTE,
        NOTE_GAP
    };

    volatile PlaybackState _state = IDLE; // shared across cores
    const Note* _currentSequence = nullptr;
    volatile int _sequenceLength = 0;
    volatile int _sequenceIndex = 0;
    volatile int _strikeCount = 0;
    volatile int _strikeIndex = 0;
    volatile bool _inStrikeMode = false;
    
    // Current note playback state
    volatile uint16_t _currentFreq = 0;
    volatile unsigned long _noteStartMs = 0;
    volatile unsigned long _noteDurationMs = 0;
    
    // Chime sequence tracking
    enum ChimePhase {
        PHASE_NONE,
        PHASE_WESTMINSTER,           // Playing full Westminster Quarters sequence
        PHASE_PAUSE_BEFORE_STRIKES,  // 1.5 second pause before hour gongs
        PHASE_STRIKES,
        PHASE_COMPLETE
    };
    volatile ChimePhase _chimePhase = PHASE_NONE; // accessed from multiple cores

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
        
        // Initialize failsafe sample-count target
        chimeSampleCount = 0;
        chimeNoteSampleTarget = ((uint32_t)durationMs * SAMPLE_RATE) / 1000;
        chimeNoteCompleted = false;
        
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
                startNote(HOUR_STRIKE_FREQ, HOUR_STRIKE_DURATION); // E3 Big Ben strike
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

        // Westminster sequence playback
        if (_sequenceIndex < _sequenceLength) {
            const Note& note = _currentSequence[_sequenceIndex];
            startNote(note.freq, note.ms);
            _sequenceIndex++;
        } else {
            // Westminster sequence complete, move to pause before strikes
            advanceChimePhase();
        }
    }

    void advanceChimePhase() {
        switch (_chimePhase) {
            case PHASE_WESTMINSTER:
                // Westminster complete, start pause before hour strikes
                _chimePhase = PHASE_PAUSE_BEFORE_STRIKES;
                _state = NOTE_GAP;
                _noteStartMs = millis();  // Start the 1.5 second pause
                // Don't call startNextNote() yet - pause first
                break;
            case PHASE_PAUSE_BEFORE_STRIKES:
                // Pause complete, transition to strikes
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
        
        _chimePhase = PHASE_WESTMINSTER;
        _currentSequence = WESTMINSTER_SEQUENCE;
        _sequenceLength = 16;  // Full-hour Westminster sequence (phrases 2-5)
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
        static unsigned long lastIdleLog = 0;
        unsigned long nowMs = millis();

        // Handle ISR signaled completion regardless of state
        if (chimeNoteCompleted) {
            chimeNoteCompleted = false;
            stopNote();
            _state = NOTE_GAP;
            _noteStartMs = nowMs;
        }

        // Failsafe: if audio is active but state machine is idle and duration passed, stop and enter gap
        if (chimeTimerActive && (_state == IDLE)) {
            if (nowMs - _noteStartMs >= _noteDurationMs && _noteDurationMs > 0) {
                stopNote();
                _state = NOTE_GAP;
                _noteStartMs = nowMs;
            }
        }

        if (_state == IDLE) {
            return;
        }

        

        if (_state == PLAYING_NOTE) {
            // Check if note duration complete
            if (nowMs - _noteStartMs >= _noteDurationMs) {
                stopNote();
                _state = NOTE_GAP;
                _noteStartMs = nowMs;
                return;
            }
        } else if (_state == NOTE_GAP) {
            // Gap between notes in phrases (80ms for rhythm), between strikes (1000ms), before strikes (1500ms)
            unsigned long gapMs;
            if (_chimePhase == PHASE_PAUSE_BEFORE_STRIKES) {
                gapMs = 1500; // 1.5 second pause before hour gongs
            } else if (_inStrikeMode) {
                gapMs = 1000; // gap between hour strikes
            } else {
                gapMs = 80; // normal inter-note gap in Westminster sequence
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

