#pragma once
#include <time.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

// Forward declaration
class DisplayManager;

class TimeManager {
    // Number of redundant NTP servers to cycle through
    static constexpr size_t NTP_COUNT = 6;

    long  _gmtOffset_sec = 0;      // standard offset in seconds
    int   _daylightOffset_sec = 0; // daylight offset in seconds (applied only if DST active now)
    bool _synced = false;
    String _usedNtpServer;
    unsigned long _lastAttemptMs = 0;
    size_t _serverIndex = 0; // rotates through NTP servers

    // Timezone metadata
    String _tzName = "Europe/London";
    bool _hasDst = true;
    bool _dstActive = false;
    long _stdOffsetSec = 0;
    long _dstOffsetSec = 3600;
    bool _tzLoaded = false;

    // Storage to read current location for timezone bootstrap
    Preferences _locPrefs;

public:
    TimeManager(long offset = 0, int daylight = 3600) 
        : _gmtOffset_sec(offset), _daylightOffset_sec(daylight), _stdOffsetSec(offset), _dstOffsetSec(daylight) {}

    void begin(DisplayManager* display = nullptr) {
        // Try to load timezone based on stored location (if any)
        bootstrapTimezoneFromPrefs(display);
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
        Serial.printf("Configuring NTP: %s, %s, %s (offset=%ld, dst=%d)\n", s1, s2, s3, _gmtOffset_sec, _daylightOffset_sec);
        configTime(_gmtOffset_sec, _daylightOffset_sec, s1, s2, s3);
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
            if (display) display->showStatus(String("Time synced from ") + _usedNtpServer + " (" + _tzName + ")");
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

    // Fetch timezone/offsets for given coordinates and apply them
    bool refreshTimezone(float lat, float lon, DisplayManager* display = nullptr) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[TimeManager] Cannot refresh timezone: WiFi not connected");
            return false;
        }

        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        String url = String("https://timeapi.io/api/TimeZone/coordinate?latitude=") + String(lat, 6) + "&longitude=" + String(lon, 6);
        Serial.print("[TimeManager] Fetching timezone: ");
        Serial.println(url);
        if (!http.begin(client, url)) {
            Serial.println("[TimeManager] Failed to begin HTTP");
            return false;
        }
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.print("[TimeManager] HTTP error: ");
            Serial.println(code);
            http.end();
            return false;
        }
        String payload = http.getString();
        http.end();
        Serial.print("[TimeManager] Timezone response length: ");
        Serial.println(payload.length());

        auto extractIntField = [&](const String& key)->long {
            int idx = payload.indexOf(key);
            if (idx < 0) return 0;
            idx += key.length();
            // Skip non-digit/non-sign chars
            while (idx < (int)payload.length() && !(payload[idx] == '-' || (payload[idx] >= '0' && payload[idx] <= '9'))) idx++;
            bool neg = false;
            if (idx < (int)payload.length() && payload[idx] == '-') { neg = true; idx++; }
            long val = 0;
            while (idx < (int)payload.length() && isDigit(payload[idx])) {
                val = val * 10 + (payload[idx] - '0');
                idx++;
            }
            return neg ? -val : val;
        };

        auto extractBoolField = [&](const String& key)->bool {
            int idx = payload.indexOf(key);
            if (idx < 0) return false;
            idx += key.length();
            if (payload.startsWith("true", idx)) return true;
            if (payload.startsWith("false", idx)) return false;
            return false;
        };

        auto extractStringField = [&](const String& key)->String {
            int idx = payload.indexOf(key);
            if (idx < 0) return String("");
            idx += key.length();
            int quote = payload.indexOf('"', idx);
            if (quote < 0) return String("");
            int end = payload.indexOf('"', quote + 1);
            if (end < 0) return String("");
            return payload.substring(quote + 1, end);
        };

        String tzName = extractStringField("\"timeZone\":");
        long stdOffset = extractIntField("\"standardUtcOffset\":{\"seconds\":");
        long dstOffset = extractIntField("\"dstOffsetToUtc\":{\"seconds\":");
        bool hasDst = extractBoolField("\"hasDayLightSaving\":");
        bool dstActive = extractBoolField("\"isDayLightSavingActive\":");

        if (tzName.length() == 0) tzName = _tzName; // fallback
        _tzName = tzName;
        _stdOffsetSec = stdOffset;
        _dstOffsetSec = dstOffset;
        _hasDst = hasDst;
        _dstActive = dstActive;
        _gmtOffset_sec = _stdOffsetSec;
        _daylightOffset_sec = (_hasDst && _dstActive) ? (int)_dstOffsetSec : 0;
        _tzLoaded = true;

        Serial.printf("[TimeManager] TZ=%s std=%ld dst=%ld active=%s\n", _tzName.c_str(), _stdOffsetSec, _dstOffsetSec, _dstActive ? "yes" : "no");
        if (display) display->showStatus(String("TZ: ") + _tzName + " (dst " + (_dstActive ? "on" : "off") + ")");

        // Reconfigure SNTP with new offsets
        _synced = false; // force resync with new offsets
        trySyncOnce(display);
        return true;
    }

    // Try to load stored location to bootstrap timezone
    void bootstrapTimezoneFromPrefs(DisplayManager* display = nullptr) {
        _locPrefs.begin("location", true);
        bool hasLat = _locPrefs.isKey("lat");
        bool hasLon = _locPrefs.isKey("lon");
        float lat = hasLat ? _locPrefs.getFloat("lat", 51.5074f) : 51.5074f;
        float lon = hasLon ? _locPrefs.getFloat("lon", -0.1278f) : -0.1278f;
        _locPrefs.end();
        // Attempt timezone fetch; ignore failure silently
        refreshTimezone(lat, lon, display);
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

    String getTimezoneName() const { return _tzName; }
    long getStdOffsetSeconds() const { return _stdOffsetSec; }
    long getDstOffsetSeconds() const { return _dstOffsetSec; }
    bool isDstActive() const { return _dstActive; }
};
