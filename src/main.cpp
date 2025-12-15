#include <Arduino.h>
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "TimeManager.h"
#include "TouchManager.h"
#include "LightSensorManager.h"
#include "RGBLedManager.h"
#include "AppVersion.h"

// Single source of truth: implement appVersion()
const char* appVersion() {
    return "v1.0.3"; // TODO: bump here only when releasing
}

// WiFi credentials are stored/prompted by NetworkManager now

// --- Objects ---
NetworkManager netMgr;
TimeManager timeMgr; // Defaults to UK GMT/BST
DisplayManager dispMgr;
TouchManager touchMgr;
RGBLedManager rgbLed;
LightSensorManager lightSensor;

// --- Timing ---
unsigned long previousMillis = 0;
const long interval = 1000;
String lastDisplayedTime = "";
String lastDisplayedDate = "";
uint16_t lastDisplayedBrightness = 65535;  // Track brightness for display updates

void setup() {
    Serial.begin(115200);
    Serial.println("=== Memory Diagnostics ===");
    Serial.printf("PSRAM found: %s\n", psramFound() ? "YES" : "NO");
    Serial.printf("Heap total/free: %u / %u\n", ESP.getHeapSize(), ESP.getFreeHeap());
#if CONFIG_SPIRAM_SUPPORT
    Serial.printf("PSRAM total/free: %u / %u\n", ESP.getPsramSize(), ESP.getFreePsram());
#endif
    
    dispMgr.begin();
    dispMgr.drawStaticInterface();

    // Initialize RGB LED manager and turn it off
    rgbLed.begin();
    rgbLed.off();

    // Initialize light sensor (runs on Core 1)
    // Pass null callback - RGB LED is disabled for now
    lightSensor.begin(&dispMgr, nullptr);

    // Initialize touch manager (runs on Core 1)
    touchMgr.begin(&dispMgr);

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
        
        // FIX: Disable WiFi power saving to prevent SPI bus contention/display flickering
        // WiFi power saving causes frequent radio wakeups that interfere with SPI display timing
        WiFi.setSleep(WIFI_PS_NONE);
        
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
    // No LVGL — standard loop timing only
    
    // Check if screen is off and wake on any touch
    static unsigned long lastTouchCheckTime = 0;
    if (!lightSensor.isScreenOn() && (millis() - lastTouchCheckTime > 100)) {
        // Sample touch to detect if user touched (simple polling)
        if (touchMgr.hasPendingEvents()) {
            lightSensor.wakeScreenFromTouch();
            lastTouchCheckTime = millis();
        }
    }

    // Pump touch events from queue (non-LVGL)
    touchMgr.update();

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

    // Cycle status messages every 5 seconds (unless in debug mode)
    static unsigned long lastStatusUpdate = 0;
    static int statusIndex = 0;
    if (currentMillis - lastStatusUpdate >= 5000) {
        lastStatusUpdate = currentMillis;
        
        // Don't override status if in debug mode
        if (!touchMgr.isDebugMode()) {
            String newStatus;
            switch (statusIndex) {
                case 0:
                    newStatus = String("Connected to: ") + WiFi.SSID();
                    break;
                case 1:
                    newStatus = timeMgr.isSynced() ? String("Time from: ") + timeMgr.getNtpServer() : "WARNING: Time sync FAILED!";
                    break;
            }
            
            dispMgr.showStatus(newStatus);  // Only redraws if different
            statusIndex = (statusIndex + 1) % 2;  // Cycle through 2 messages
        }
    }
    
    delay(5);
}
