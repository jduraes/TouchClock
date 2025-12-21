#include "ChimeManager.h"

// Global variables for DAC audio interrupt handler
volatile bool chimeTimerActive = false;
volatile uint32_t chimePhaseAccumulator = 0;
volatile uint32_t chimePhaseIncrement = 0;
volatile uint8_t chimeAmplitude = 0;

// Global interrupt handler for DAC audio
void IRAM_ATTR chimeTimerHandler() {
    if (!chimeTimerActive) return;
    
    chimePhaseAccumulator += chimePhaseIncrement;
    
    // Triangle wave: creates smooth ramp up/down unlike harsh square wave
    // Phase from 0 to 2^32, extract upper 9 bits for linear ramp (0-511)
    uint16_t phase = (chimePhaseAccumulator >> 23) & 0x1FF;  // 9-bit phase (0-511)
    int16_t triangleValue;
    
    if (phase < 256) {
        // First half: ramp up from 0 to 255
        triangleValue = phase;
    } else {
        // Second half: ramp down from 255 to 0
        triangleValue = 511 - phase;
    }
    
    // Scale by amplitude and add DC offset
    int16_t sample = (triangleValue * chimeAmplitude) / 255 + 128 - (chimeAmplitude / 2);
    
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
