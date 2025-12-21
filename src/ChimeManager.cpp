#include "ChimeManager.h"

// Global variables for DAC audio interrupt handler
volatile bool chimeTimerActive = false;
volatile uint32_t chimePhaseAccumulator = 0;
volatile uint32_t chimePhaseIncrement = 0;
volatile uint8_t chimeAmplitude = 0;

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

// Westminster chime note frequencies
// G4 = 392Hz, C5 = 523Hz, D5 = 587Hz, E5 = 659Hz

// Westminster rhythm: dotted quarter (900ms), quarter (600ms)
// Pattern: long-short-long-short per phrase
const ChimeManager::Note ChimeManager::PHRASE1[4] = {
    {392, 900}, // G4 - dotted quarter
    {523, 600}, // C5 - quarter
    {587, 900}, // D5 - dotted quarter
    {392, 600}, // G4 - quarter
};

const ChimeManager::Note ChimeManager::PHRASE2[4] = {
    {392, 900}, // G4 - dotted quarter
    {587, 600}, // D5 - quarter
    {659, 900}, // E5 - dotted quarter
    {523, 600}, // C5 - quarter
};

const ChimeManager::Note ChimeManager::PHRASE3[4] = {
    {392, 900}, // G4 - dotted quarter
    {659, 600}, // E5 - quarter
    {587, 900}, // D5 - dotted quarter
    {523, 600}, // C5 - quarter
};

const ChimeManager::Note ChimeManager::PHRASE4[4] = {
    {392, 900}, // G4 - dotted quarter
    {523, 600}, // C5 - quarter
    {587, 900}, // D5 - dotted quarter
    {392, 600}, // G4 - quarter
};
