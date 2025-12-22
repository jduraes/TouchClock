#pragma once
#include <time.h>

// Forward declaration
class DisplayManager;

class TimeManager {
    // Number of redundant NTP servers to cycle through
    static constexpr size_t NTP_COUNT = 6;

    const long  gmtOffset_sec;
    const int   daylightOffset_sec;
    bool _synced = false;
    String _usedNtpServer;
    unsigned long _lastAttemptMs = 0;
    size_t _serverIndex = 0; // rotates through NTP_SERVERS

public:
    TimeManager(long offset = 0, int daylight = 3600) 
        : gmtOffset_sec(offset), daylightOffset_sec(daylight) {}

    void begin(DisplayManager* display = nullptr) {
        // Kick off initial sync attempt but do not block indefinitely
        trySyncOnce(display);
    }

    // Attempt a single sync using a rotating trio of servers; non-blocking beyond short wait
    bool trySyncOnce(DisplayManager* display = nullptr) {
        // Choose three servers in a round-robin fashion
        auto serverByIndex = [](size_t idx) -> const char* {
            switch (idx % NTP_COUNT) {
                case 0: return "time.google.com";
                case 1: return "time.cloudflare.com";
                case 2: return "pool.ntp.org";
                case 3: return "uk.pool.ntp.org";
                case 4: return "time.nist.gov";
                default: return "europe.pool.ntp.org";
            }
        };
        const char* s1 = serverByIndex(_serverIndex);
        const char* s2 = serverByIndex(_serverIndex + 1);
        const char* s3 = serverByIndex(_serverIndex + 2);
        _serverIndex = (_serverIndex + 1) % NTP_COUNT; // advance for next round

        if (display) display->showStatus(String("Syncing NTP: ") + s1 + ", " + s2 + ", " + s3);
        Serial.printf("Configuring NTP: %s, %s, %s\n", s1, s2, s3);
        configTime(gmtOffset_sec, daylightOffset_sec, s1, s2, s3);
        _usedNtpServer = String(s1);
        _lastAttemptMs = millis();

        // Poll briefly for success (up to ~5 seconds)
        time_t now = time(nullptr);
        int attempts = 0;
        while (now < 24 * 3600 && attempts < 50) { // 50 * 100ms = 5s
            delay(100);
            now = time(nullptr);
            attempts++;
        }
        if (now > 24 * 3600) {
            _synced = true;
            Serial.println("Time synchronized from NTP");
            if (display) display->showStatus(String("Time synced from ") + _usedNtpServer);
            return true;
        }
        Serial.println("NTP attempt failed, will retry");
        if (display) display->showStatus(String("NTP attempt failed on ") + _usedNtpServer);
        return false;
    }

    // Continuously retry NTP until an update is obtained; call from loop()
    void maybeEnsureSynced(DisplayManager* display = nullptr) {
        if (_synced) return;
        unsigned long nowMs = millis();
        // Retry every 10 seconds while unsynced
        if (nowMs - _lastAttemptMs >= 10000) {
            trySyncOnce(display);
        }
    }

    String getFormattedTime() {
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)) return "--:--:--";
        char timeStringBuff[10];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
        return String(timeStringBuff);
    }
    
    String getFormattedDate() {
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)) return "";
        const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        const char* monthNames[] = {"January", "February", "March", "April", "May", "June", 
                                     "July", "August", "September", "October", "November", "December"};
        int weekNum = (timeinfo.tm_yday / 7) + 1;  // Simple week number calculation
        String dateStr = String(dayNames[timeinfo.tm_wday]) + ", " + 
                         String(timeinfo.tm_mday) + " " + 
                         monthNames[timeinfo.tm_mon] + ", " + 
                         String(1900 + timeinfo.tm_year) + ", week " + 
                         String(weekNum);
        return dateStr;
    }
    
    bool isSynced() { return _synced; }
    
    String getNtpServer() { 
        return _usedNtpServer.length() ? _usedNtpServer : String("NTP not yet synced");
    }
};
