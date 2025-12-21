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
    uint8_t sample = ((chimePhaseAccumulator >> 31) & 1) ? (128 + chimeAmplitude) : (128 - chimeAmplitude);
    if (sample > 255) sample = 255;
    dacWrite(26, sample);
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
