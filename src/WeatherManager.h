#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "DisplayManager.h"

// Fetches tomorrow's weather (first 12 hours) for RG45 7LF via open-meteo (no API key).
// Displays 6 slots (every 2 hours) using DisplayManager glyphs.
class WeatherManager {
    static constexpr float LAT = 51.370f;   // RG45 7LF approximate latitude (Crowthorne, UK)
    static constexpr float LON = -0.794f;   // RG45 7LF approximate longitude

    uint8_t _codes[6] = {0, 0, 0, 0, 0, 0};
    bool _hasData = false;
    int _lastFetchDay = -1;  // tm_mday when last fetched

    String buildTomorrowUrl() {
        time_t now = time(nullptr) + 24 * 3600;  // move to tomorrow
        struct tm tmTomorrow;
        localtime_r(&now, &tmTomorrow);

        char dateBuf[11];
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmTomorrow);
        String dateStr(dateBuf);

        String url = "https://api.open-meteo.com/v1/forecast?latitude=";
        url += String(LAT, 3);
        url += "&longitude=";
        url += String(LON, 3);
        url += "&hourly=weathercode&start_date=";
        url += dateStr;
        url += "&end_date=";
        url += dateStr;
        url += "&timezone=auto";
        return url;
    }

    bool parseWeatherCodes(const String& payload, uint8_t outCodes[6]) {
        const char* key = "\"weathercode\":[";
        int idx = payload.indexOf(key);
        if (idx < 0) return false;
        idx += strlen(key);
        int len = payload.length();

        for (int i = 0; i < 6; i++) {
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
            outCodes[i] = static_cast<uint8_t>(val);

            // Move to next comma for next value
            int comma = payload.indexOf(',', idx);
            if (i < 5) {
                if (comma < 0) return false;
                idx = comma + 1;
            }
        }
        return true;
    }

public:
    bool refresh(DisplayManager* display) {
        if (WiFi.status() != WL_CONNECTED) return false;

        String url = buildTomorrowUrl();
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

        if (!parseWeatherCodes(payload, _codes)) {
            return false;
        }

        // Record fetch day (today), so we only re-fetch after midnight
        time_t now = time(nullptr);
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        _lastFetchDay = tmNow.tm_mday;
        _hasData = true;

        if (display) {
            // Tomorrow from 00:00 at 2-hour steps
            display->showWeatherIconsWithLabels(_codes, 0);
        }
        return true;
    }

    void show(DisplayManager* display) {
        if (_hasData && display) {
            display->showWeatherIconsWithLabels(_codes, 0);
        }
    }

    void maybeRefreshDaily(const tm& timeinfo, DisplayManager* display) {
        // Fetch once per calendar day, shortly after midnight
        if (timeinfo.tm_mday != _lastFetchDay && timeinfo.tm_hour >= 0 && timeinfo.tm_hour <= 1) {
            refresh(display);
        }
    }
};
