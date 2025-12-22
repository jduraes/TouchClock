#include "ChimeManager.h"

// Global variables for DAC audio interrupt handler
volatile bool chimeTimerActive = false;
volatile uint32_t chimePhaseAccumulator = 0;
volatile uint32_t chimePhaseIncrement = 0;
volatile uint8_t chimeAmplitude = 0;
// Failsafe tracking to end notes by sample count
volatile uint32_t chimeSampleCount = 0;
volatile uint32_t chimeNoteSampleTarget = 0;
volatile bool chimeNoteCompleted = false;

// Complete 64-sample sine lookup table in flash (PROGMEM) - full 360° cycle
// Peak amplitude at ±91 (70% of 8-bit range, safe for mixing)
// Proper zero-crossings at indices 0, 32, 64 for clean looping
static const int8_t PROGMEM sineTable64[64] = {
    // 0 - 90 degrees
     0,   9,  18,  27,  35,  43,  51,  58,
    64,  70,  76,  80,  84,  88,  90,  91,
    
    // 90 - 180 degrees
    91,  90,  88,  84,  80,  76,  70,  64,
    58,  51,  43,  35,  27,  18,   9,   0,
    
    // 180 - 270 degrees
     0,  -9, -18, -27, -35, -43, -51, -58,
   -64, -70, -76, -80, -84, -88, -90, -91,
   
    // 270 - 360 degrees
   -91, -90, -88, -84, -80, -76, -70, -64,
   -58, -51, -43, -35, -27, -18,  -9,   0
};

// Global interrupt handler for DAC audio
void IRAM_ATTR chimeTimerHandler() {
    if (!chimeTimerActive) return;
    
    // Increment sample counter and check for note completion
    chimeSampleCount++;
    if (chimeNoteSampleTarget != 0 && chimeSampleCount >= chimeNoteSampleTarget) {
        chimeTimerActive = false;      // stop generating samples
        chimeNoteCompleted = true;     // signal completion to main loop
        // Do not call heavy functions here; update() will finalize
        return;
    }

    chimePhaseAccumulator += chimePhaseIncrement;
    
    // Extract phase: upper 6 bits for 64-entry table (0-63)
    uint8_t phaseIndex = (chimePhaseAccumulator >> 26) & 0x3F;
    
    // Read sine value from flash
    int8_t sineValue = pgm_read_byte(&sineTable64[phaseIndex]);
    
    // Scale by amplitude and add DC offset
    int16_t sample = (sineValue * chimeAmplitude) / 64 + 128;
    
    // Clamp to DAC range
    if (sample < 0) sample = 0;
    if (sample > 255) sample = 255;
    
    dacWrite(26, (uint8_t)sample);
}

// Westminster Quarters note frequencies - E Major key (Big Ben authentic)
// B3 = 247Hz, E4 = 330Hz, F#4 = 370Hz, G#4 = 415Hz, E3 = 165Hz (hour strike)
// Tempo: ~100 BPM, quarter note = 600ms, half note = 1200ms

// Full hour: phrases 2, 3, 4, 5 (each measure in 5/4: q, q, q, h)
// Measures per score: 
// 1) e4, gis4, fis4, b3 (half) 
// 2) e4, fis4, gis4, e4 (half)
// 3) gis4, e4, fis4, b3 (half)
// 4) b3, fis4, gis4, e4 (half)
const ChimeManager::Note ChimeManager::WESTMINSTER_SEQUENCE[16] = {
    // Measure 1
    {330, 600}, {415, 600}, {370, 600}, {247, 1200},
    // Measure 2
    {330, 600}, {370, 600}, {415, 600}, {330, 1200},
    // Measure 3
    {415, 600}, {330, 600}, {370, 600}, {247, 1200},
    // Measure 4 (fis' is octave up: F#5)
    {247, 600}, {370, 600}, {415, 600}, {330, 1200}
};

// Hour strike note (Big Ben low E)
const uint16_t ChimeManager::HOUR_STRIKE_FREQ = 165; // E3
const uint16_t ChimeManager::HOUR_STRIKE_DURATION = 1000; // 1 second per strike
