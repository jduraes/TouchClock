#include <Arduino.h>
#include "NetworkManager.h"
#include "TimeManager.h"
#include "DisplayManager.h"

// --- Configuration ---
const char* SSID = "YOUR_WIFI_SSID";
const char* PASS = "YOUR_WIFI_PASS";

// --- Objects ---
NetworkManager netMgr;
TimeManager timeMgr; // Defaults to UK GMT/BST
DisplayManager dispMgr;

// --- Timing ---
unsigned long previousMillis = 0;
const long interval = 1000;

void setup() {
    Serial.begin(115200);
    
    dispMgr.begin();
    dispMgr.showStatus("Initializing...");
    
    netMgr.begin(SSID, PASS);
    dispMgr.showStatus("WiFi Connected");
    
    timeMgr.begin();
    dispMgr.drawStaticInterface();
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        String timeStr = timeMgr.getFormattedTime();
        dispMgr.updateClock(timeStr);
        Serial.println(timeStr);
    }
}
