#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "DisplayManager.h"

// Fetches rolling weather via open-meteo (no API key). Adjust coordingates below as needed.
// Displays 6 slots (every 2 hours) starting ~2h from now, using DisplayManager icons.
class WeatherManager {
    static constexpr float LAT = 51.370f;   // RG45 7LF approximate latitude (Crowthorne, UK)
    static constexpr float LON = -0.794f;   // RG45 7LF approximate longitude

    uint8_t _codes[6] = {0, 0, 0, 0, 0, 0};
    float _temps[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // Temperature in Celsius for each slot
    bool _hasData = false;
    int _lastFetchDay = -1;  // tm_mday when last fetched
    int _lastRenderedStartHour = -1; // start hour used in last render
    time_t _lastFetchEpoch = 0; // epoch seconds of last successful fetch

    String buildTodayTomorrowUrl() {
        // Fetch from today to tomorrow (48 hourly entries)
        time_t now = time(nullptr);
        struct tm tmToday;
        localtime_r(&now, &tmToday);

        time_t tom = now + 24 * 3600;
        struct tm tmTomorrow;
        localtime_r(&tom, &tmTomorrow);

        char dateToday[11];
        char dateTomorrow[11];
        strftime(dateToday, sizeof(dateToday), "%Y-%m-%d", &tmToday);
        strftime(dateTomorrow, sizeof(dateTomorrow), "%Y-%m-%d", &tmTomorrow);

        String url = "https://api.open-meteo.com/v1/forecast?latitude=";
        url += String(LAT, 3);
        url += "&longitude=";
        url += String(LON, 3);
        url += "&hourly=weathercode,temperature_2m&start_date=";
        url += String(dateToday);
        url += "&end_date=";
        url += String(dateTomorrow);
        url += "&timezone=auto";
        return url;
    }

    bool parseWeatherCodesAll(const String& payload, uint8_t outAll[], int maxCount, int& outCount) {
        const char* key = "\"weathercode\":[";
        int idx = payload.indexOf(key);
        if (idx < 0) return false;
        idx += strlen(key);
        int len = payload.length();

        int count = 0;
        while (idx < len && count < maxCount) {
            // Skip whitespace
            while (idx < len && (payload[idx] == ' ' || payload[idx] == '\n' || payload[idx] == '\r')) idx++;
            if (idx >= len) return false;

            int val = 0;
            bool foundDigit = false;
            while (idx < len && isDigit(payload[idx])) {
                val = val * 10 + (payload[idx] - '0');
                idx++;
                foundDigit = true;
            }
            if (!foundDigit) return false;
            outAll[count++] = static_cast<uint8_t>(val);

            // Move to next comma for next value
            int comma = payload.indexOf(',', idx);
            if (comma < 0) break; // End of array
            idx = comma + 1;
        }
        outCount = count;
        return true;
    }

    bool parseTemperaturesAll(const String& payload, float outAll[], int maxCount, int& outCount) {
        const char* key = "\"temperature_2m\":[";
        int idx = payload.indexOf(key);
        if (idx < 0) return false;
        idx += strlen(key);
        int len = payload.length();

        int count = 0;
        while (idx < len && count < maxCount) {
            // Skip whitespace
            while (idx < len && (payload[idx] == ' ' || payload[idx] == '\n' || payload[idx] == '\r')) idx++;
            if (idx >= len) return false;

            // Parse float value (can be negative)
            bool negative = false;
            if (payload[idx] == '-') {
                negative = true;
                idx++;
            }
            
            float val = 0.0f;
            bool foundDigit = false;
            // Parse integer part
            while (idx < len && isDigit(payload[idx])) {
                val = val * 10.0f + (payload[idx] - '0');
                idx++;
                foundDigit = true;
            }
            
            // Parse decimal part if exists
            if (idx < len && payload[idx] == '.') {
                idx++;
                float decimal = 0.1f;
                while (idx < len && isDigit(payload[idx])) {
                    val += (payload[idx] - '0') * decimal;
                    decimal *= 0.1f;
                    idx++;
                    foundDigit = true;
                }
            }
            
            if (!foundDigit) return false;
            outAll[count++] = negative ? -val : val;

            // Move to next comma for next value
            int comma = payload.indexOf(',', idx);
            if (comma < 0) break; // End of array
            idx = comma + 1;
        }
        outCount = count;
        return true;
    }

public:
    bool refresh(DisplayManager* display) {
        if (WiFi.status() != WL_CONNECTED) return false;

        String url = buildTodayTomorrowUrl();
        WiFiClientSecure client;
        client.setInsecure();  // Use TLS without certificate pinning for simplicity
        HTTPClient http;
        if (!http.begin(client, url)) {
            return false;
        }

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return false;
        }

        String payload = http.getString();
        http.end();

        // Parse up to 48 hourly codes (today+tomorrow)
        uint8_t allCodes[48];
        int total = 0;
        if (!parseWeatherCodesAll(payload, allCodes, 48, total)) {
            return false;
        }

        // Parse up to 48 hourly temperatures (today+tomorrow)
        float allTemps[48];
        int totalTemps = 0;
        if (!parseTemperaturesAll(payload, allTemps, 48, totalTemps)) {
            return false;
        }
        
        // Ensure we have matching data
        if (total != totalTemps) {
            return false;
        }

        // Determine start hour: 2h from now, round to next even hour boundary
        time_t now = time(nullptr);
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        int startHourLocal = tmNow.tm_hour + 2;
        if (startHourLocal % 2 == 1) startHourLocal++; // move to next even hour
        int startIndex = startHourLocal; // since array starts at today's 00:00 local
        if (startIndex + 5 >= total) startIndex = max(0, total - 6); // Clamp if near end

        int startHourDisplay = startHourLocal % 24;

        for (int i = 0; i < 6; i++) {
            int idx = startIndex + i * 2;
            if (idx >= total) idx = total - 1;
            _codes[i] = allCodes[idx];
            _temps[i] = allTemps[idx];
        }

        // Record fetch day (today), so we only re-fetch after midnight
        _lastFetchDay = tmNow.tm_mday;
        _hasData = true;
        _lastRenderedStartHour = startHourDisplay;
        _lastFetchEpoch = now;

        if (display) {
            display->showWeatherIconsWithLabelsAndTemps(_codes, _temps, startHourDisplay);
        }
        return true;
    }

    void show(DisplayManager* display) {
        if (_hasData && display) {
            display->showWeatherIconsWithLabelsAndTemps(_codes, _temps, _lastRenderedStartHour >= 0 ? _lastRenderedStartHour : 0);
        }
    }

    void maybeRefreshDaily(const tm& timeinfo, DisplayManager* display) {
        // Fetch once per calendar day, shortly after midnight
        if (timeinfo.tm_mday != _lastFetchDay && timeinfo.tm_hour >= 0 && timeinfo.tm_hour <= 1) {
            refresh(display);
        }
    }

    void maybeRefreshRolling(const tm& timeinfo, DisplayManager* display) {
        // Re-fetch at least hourly (or if missing), and re-render at each 2h boundary
        time_t nowEpoch = time(nullptr);
        bool needsFetch = (!_hasData) || difftime(nowEpoch, _lastFetchEpoch) >= 3600;

        int nextStart = timeinfo.tm_hour + 2;
        if (nextStart % 2 == 1) nextStart++; // next even hour
        int nextStartDisplay = nextStart % 24;

        if (needsFetch) {
            refresh(display);
            return;
        }

        if (nextStartDisplay != _lastRenderedStartHour) {
            _lastRenderedStartHour = nextStartDisplay;
            if (display) display->showWeatherIconsWithLabelsAndTemps(_codes, _temps, nextStartDisplay);
        }
    }
};
