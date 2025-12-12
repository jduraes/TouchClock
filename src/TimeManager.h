#pragma once
#include <time.h>

// Forward declaration
class DisplayManager;

class TimeManager {
    const char* ntpServer1 = "uk.pool.ntp.org";
    const char* ntpServer2 = "time.nist.gov";
    const char* ntpServer3 = "pool.ntp.org";
    const long  gmtOffset_sec;
    const int   daylightOffset_sec;
    bool _synced = false;
    String _usedNtpServer;

public:
    TimeManager(long offset = 0, int daylight = 3600) 
        : gmtOffset_sec(offset), daylightOffset_sec(daylight) {}

    void begin(DisplayManager* display = nullptr) {
        if (display) display->showStatus("Syncing with " + String(ntpServer1) + "...");
        Serial.println("Configuring time with NTP servers...");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
        _usedNtpServer = String(ntpServer1);
        
        // Wait for time to be set (up to 15 seconds)
        time_t now = time(nullptr);
        int attempts = 0;
        while (now < 24 * 3600 && attempts < 150) {
            delay(100);
            now = time(nullptr);
            attempts++;
            if (display && attempts % 10 == 0) {
                display->showStatus("NTP: " + String(ntpServer1) + " (" + String(attempts / 10) + "/15s)");
            }
        }
        
        if (now > 24 * 3600) {
            _synced = true;
            Serial.println("Time synchronized from NTP");
            if (display) display->showStatus("Time synced from " + String(ntpServer1));
            delay(1500);
        } else {
            Serial.println("Warning: NTP sync timeout, using default time");
            if (display) display->showStatus("NTP FAILED (" + String(ntpServer1) + " timeout)");
            delay(1500);
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
        
        // Get day of week and week number
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
        if (!_synced) {
            return "NTP sync FAILED";
        }
        return _usedNtpServer; 
    }
};
