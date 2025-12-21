#include "ChimeManager.h"

// Global variables for DAC audio interrupt handler
volatile bool chimeTimerActive = false;
volatile uint32_t chimePhaseAccumulator = 0;
volatile uint32_t chimePhaseIncrement = 0;
volatile uint8_t chimeAmplitude = 0;

// Compact 32-sample sine lookup table in flash (PROGMEM) - only 32 bytes
// Values are 0-127 for half-cycle (0° to 180°), symmetry handles full cycle
static const int8_t PROGMEM sineTable32[32] = {
    0, 12, 24, 35, 45, 55, 63, 71,
    78, 83, 87, 90, 91, 91, 90, 87,
    83, 78, 71, 63, 55, 45, 35, 24,
    12, 0, -12, -24, -35, -45, -55, -63
};

// Global interrupt handler for DAC audio
void IRAM_ATTR chimeTimerHandler() {
    if (!chimeTimerActive) return;
    
    chimePhaseAccumulator += chimePhaseIncrement;
    
    // Extract phase: upper 5 bits for 32-entry table (0-31)
    uint8_t phaseIndex = (chimePhaseAccumulator >> 27) & 0x1F;
    
    // Read sine value from flash
    int8_t sineValue = pgm_read_byte(&sineTable32[phaseIndex]);
    
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
