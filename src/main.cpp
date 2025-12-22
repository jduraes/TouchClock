#include <Arduino.h>
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "TimeManager.h"
#include "TouchManager.h"
#include "LightSensorManager.h"
#include "RGBLedManager.h"
#include "ChimeManager.h"
#include "WeatherManager.h"

// Helper functions to avoid circular dependency between NetworkManager and WeatherManager
void weatherManagerReload(void* mgr) {
    if (mgr) {
        static_cast<WeatherManager*>(mgr)->reloadLocation();
    }
}

bool weatherManagerGeocode(void* mgr, const String& query, float& outLat, float& outLon, String& outTown) {
    if (mgr) {
        return static_cast<WeatherManager*>(mgr)->verifyAndGeocode(query, outLat, outLon, outTown);
    }
    return false;
}

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
    netMgr.setWeatherManager(&weatherMgr);
    
    // Check if we have stored credentials first
    if (netMgr.hasStoredCredentials()) {
        dispMgr.showStatus("Connecting to WiFi...");
    } else {
        // Show provisioning instructions before blocking
        dispMgr.showInstruction(String("Connect to ") + netMgr.apName() + "\nOpen a browser to configure WiFi");
        dispMgr.showStatus("Waiting for WiFi setup...");
    }
    
    // Try to connect (will use stored credentials if available, or start provisioning AP)
    // If AP mode needed, this returns false but the HTTP server is now running
    bool connected = netMgr.begin();
    
    static bool timeInitialized = false;
    
    // If we have stored credentials, wait for connection
    if (!connected && netMgr.hasStoredCredentials()) {
        // Retry connection attempt
        unsigned long connStart = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - connStart < 30000) {
            netMgr.update();  // Handle HTTP requests
            delay(100);
        }
        connected = (WiFi.status() == WL_CONNECTED);
    }
    
    // If connected or provisioned, proceed with initialization
    if (connected || netMgr.isProvisioned()) {
        dispMgr.clearInstructions();
        if (WiFi.status() == WL_CONNECTED) {
            dispMgr.showStatus(String("WiFi: ") + WiFi.SSID());
            WiFi.setSleep(WIFI_PS_NONE);
            if (!timeInitialized) {
                timeMgr.begin(&dispMgr);
                timeInitialized = true;
            }
        }
    }
    
    // Initialize display and weather even if WiFi not ready yet
    if (!timeInitialized && WiFi.status() == WL_CONNECTED) {
        timeMgr.begin(&dispMgr);
        timeInitialized = true;
        weatherMgr.refresh(&dispMgr);
    }
    
    if (timeInitialized) {
        String timeStr = timeMgr.getFormattedTime();
        String dateStr = timeMgr.getFormattedDate();
        dispMgr.updateClock(timeStr);
        dispMgr.updateDate(dateStr);
        Serial.println(timeStr);
        Serial.println(dateStr);
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

    // Update network server (handle HTTP requests from provisioning or config pages)
    netMgr.update();

    // Keep attempting NTP sync until successful
    timeMgr.maybeEnsureSynced(&dispMgr);

    // Check if location was updated via config page - force immediate weather refresh
    if (netMgr.checkAndClearLocationUpdated()) {
        Serial.println("[Main Loop] Location updated flag detected, forcing weather refresh");
        weatherMgr.refresh(&dispMgr);
    }

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
                    newStatus = String("Connected to: ") + WiFi.SSID() + " - IP: " + WiFi.localIP().toString();
                    break;
                case 1:
                    newStatus = timeMgr.isSynced() ? String("Time from: ") + timeMgr.getNtpServer() : "WARNING: Time sync FAILED!";
                    break;
            }
            
            dispMgr.showStatus(newStatus);  // Only redraws if different
            statusIndex = (statusIndex + 1) % 2;  // Cycle through 2 messages
        }
    }

    // Update town name display in header
    static String lastDisplayedTown = "";
    String currentTown = weatherMgr.getTownName();
    if (currentTown != lastDisplayedTown) {
        lastDisplayedTown = currentTown;
        dispMgr.updateHeaderText("TouchClock", currentTown);
    }
    
    delay(5);
}
