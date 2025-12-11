#pragma once
#include <time.h>

class TimeManager {
    const char* ntpServer = "pool.ntp.org";
    const long  gmtOffset_sec;
    const int   daylightOffset_sec;

public:
    TimeManager(long offset = 0, int daylight = 3600) 
        : gmtOffset_sec(offset), daylightOffset_sec(daylight) {}

    void begin() {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }

    String getFormattedTime() {
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)) return "Syncing...";
        
        char timeStringBuff[10];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
        return String(timeStringBuff);
    }
};
