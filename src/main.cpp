#include <Arduino.h>
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "TimeManager.h"
#include "TouchManager.h"
#include "LightSensorManager.h"
#include "RGBLedManager.h"
#include "ChimeManager.h"
#include "WeatherManager.h"

// Single source of truth: app version as a constant (and backward-compatible accessor)
static constexpr char APP_VERSION[] = "v1.0.8";
/**
 * Returns the application's version string (from the APP_VERSION macro).
 */
inline const char* appVersion() { return APP_VERSION; }

// --- Objects ---
NetworkManager netMgr;
TimeManager timeMgr; // Defaults to UK GMT/BST
DisplayManager dispMgr;
TouchManager touchMgr;
RGBLedManager rgbLed;
LightSensorManager lightSensor;
ChimeManager chimeMgr;
WeatherManager weatherMgr;

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
    dispMgr.updateHeaderText("TouchClock");
    
    // Initialize RGB LED manager and turn it off
    rgbLed.begin();
    rgbLed.off();

    // Initialize light sensor (runs on Core 1)
    // Pass null callback - RGB LED is disabled for now
    lightSensor.begin(&dispMgr, nullptr);

    // Initialize chime (default speaker pin = 26 on CYD)
    chimeMgr.begin();
    chimeMgr.setVolume(10);  // Set volume to 10%

    // Initialize touch manager (runs on Core 1)
    touchMgr.begin(&dispMgr);
    touchMgr.setChimeManager(&chimeMgr);

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
        // Static interface was already drawn at startup; avoid redundant redraws
        
        // Display first time and date immediately
        String timeStr = timeMgr.getFormattedTime();
        String dateStr = timeMgr.getFormattedDate();
        dispMgr.updateClock(timeStr);
        dispMgr.updateDate(dateStr);
        weatherMgr.refresh(&dispMgr);  // Fetch and draw rolling forecast starting ~2h from now
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

    // Update non-blocking chime audio generation
    chimeMgr.update();

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

        // Refresh forecast once per day (after date change)
        weatherMgr.refresh(&dispMgr);
    }

    // Daily guard to refetch shortly after midnight if missed
    weatherMgr.maybeRefreshDaily(timeinfo, &dispMgr);

    // Hourly Big Ben chime between 08:00-22:00
    chimeMgr.maybeChime(timeinfo);

    // Rolling weather label update at every 2-hour boundary
    weatherMgr.maybeRefreshRolling(timeinfo, &dispMgr);

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
