#include <Arduino.h>
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "TimeManager.h"

// WiFi credentials are stored/prompted by NetworkManager now

// --- Objects ---
NetworkManager netMgr;
TimeManager timeMgr; // Defaults to UK GMT/BST
DisplayManager dispMgr;

// --- Timing ---
unsigned long previousMillis = 0;
const long interval = 1000;
String lastDisplayedTime = "";
String lastDisplayedDate = "";

void setup() {
    Serial.begin(115200);
    
    dispMgr.begin();
    dispMgr.drawStaticInterface();

    // Pass display to NetworkManager so it can show connection progress
    netMgr.setDisplay(&dispMgr);
    
    // Check if we have stored credentials first
    if (netMgr.hasStoredCredentials()) {
        dispMgr.showStatus("Connecting to WiFi...");
    } else {
        // Show provisioning instructions before blocking
        dispMgr.showInstruction(String("Connect to ") + netMgr.apName() + "\nOpen a browser to configure WiFi");
        dispMgr.showStatus("Waiting for WiFi setup...");
    }
    
    // Try to connect (will use stored credentials if available, or start provisioning)
    bool ok = netMgr.begin();
    
    // After connection attempt, clear any instructions and show network status
    if (ok && WiFi.status() == WL_CONNECTED) {
        dispMgr.clearInstructions();
        dispMgr.showStatus(String("WiFi: ") + WiFi.SSID());
        
        // Now sync time from NTP
        timeMgr.begin(&dispMgr);
        dispMgr.drawStaticInterface();
        
        // Display first time and date immediately
        String timeStr = timeMgr.getFormattedTime();
        String dateStr = timeMgr.getFormattedDate();
        dispMgr.updateClock(timeStr);
        dispMgr.updateDate(dateStr);
        Serial.println(timeStr);
        Serial.println(dateStr);
    } else {
        dispMgr.showStatus("WiFi Failed");
    }
}

void loop() {
    // Get current time with millisecond precision
    unsigned long currentMillis = millis();
    
    // Get formatted time string
    String timeStr = timeMgr.getFormattedTime();
    
    // Only update display if the time string actually changed (new second)
    if (timeStr != lastDisplayedTime) {
        lastDisplayedTime = timeStr;
        dispMgr.updateClock(timeStr);
    }
    
    // Update date only when the day actually changes (at midnight)
    struct tm timeinfo;
    time_t now = time(nullptr);
    localtime_r(&now, &timeinfo);
    
    static int lastDay = -1;
    if (timeinfo.tm_mday != lastDay) {
        lastDay = timeinfo.tm_mday;
        String dateStr = timeMgr.getFormattedDate();
        if (dateStr != lastDisplayedDate) {
            lastDisplayedDate = dateStr;
            dispMgr.updateDate(dateStr);
        }
    }
    
    // Cycle status messages every 5 seconds
    static unsigned long lastStatusUpdate = 0;
    static int statusIndex = 0;
    if (currentMillis - lastStatusUpdate >= 5000) {
        lastStatusUpdate = currentMillis;
        
        switch (statusIndex) {
            case 0:
                dispMgr.showStatus("Connected to: " + WiFi.SSID());
                break;
            case 1:
                dispMgr.showStatus(timeMgr.isSynced() ? "Time from: " + timeMgr.getNtpServer() : "WARNING: Time sync FAILED!");
                break;
        }
        
        statusIndex = (statusIndex + 1) % 2;  // Cycle through 2 messages
    }
    
    // Small delay to prevent tight loop and allow time to settle
    delay(10);
}
