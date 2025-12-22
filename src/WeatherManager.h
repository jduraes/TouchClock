#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "DisplayManager.h"

// Fetch rolling weather via open-meteo (no API key). Location loaded from Preferences.
// Displays 6 slots (every 2 hours) starting ~2h from now, using DisplayManager icons.
class WeatherManager {
    // Default: London
    static constexpr float DEFAULT_LAT = 51.5074f;
    static constexpr float DEFAULT_LON = -0.1278f;

    float _lat = DEFAULT_LAT;
    float _lon = DEFAULT_LON;
    String _townName = "London";  // Town/city name from geocoding
    bool _locationLoaded = false;
    Preferences _locPrefs;

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
        url += String(_lat, 3);
        url += "&longitude=";
        url += String(_lon, 3);
        url += "&hourly=weathercode,temperature_2m&start_date=";
        url += String(dateToday);
        url += "&end_date=";
        url += String(dateTomorrow);
        url += "&timezone=auto";
        return url;
    }

    bool ensureLocationLoaded() {
        if (_locationLoaded) return true;
        _locPrefs.begin("location", true);
        bool hasLat = _locPrefs.isKey("lat");
        bool hasLon = _locPrefs.isKey("lon");
        if (hasLat && hasLon) {
            _lat = _locPrefs.getFloat("lat", DEFAULT_LAT);
            _lon = _locPrefs.getFloat("lon", DEFAULT_LON);
            _townName = _locPrefs.getString("town", "");  // Try to load saved town
            String savedPostcode = _locPrefs.getString("postcode", "");
            _locPrefs.end();
            
            // If town not saved OR equals the saved postcode (legacy), attempt reverse geocode from coordinates
            String tnTrim = _townName; tnTrim.trim();
            String pcTrim = savedPostcode; pcTrim.trim();
            bool townMissingOrEqualsPostcode = (tnTrim.length() == 0) || (pcTrim.length() > 0 && tnTrim.equalsIgnoreCase(pcTrim));
            if (townMissingOrEqualsPostcode) {
                Serial.printf("[WeatherManager::ensureLocationLoaded] No town saved for coords (%.4f, %.4f), attempting reverse geocode...\n", _lat, _lon);
                String reverseTown = "";
                if (reverseGeocode(_lat, _lon, reverseTown)) {
                    _townName = reverseTown;
                    Serial.print("[WeatherManager::ensureLocationLoaded] Reverse geocode success: ");
                    Serial.println(_townName);
                    // Save the discovered town for next time
                    _locPrefs.begin("location", false);
                    _locPrefs.putString("town", _townName);
                    _locPrefs.end();
                } else {
                    Serial.println("[WeatherManager::ensureLocationLoaded] Reverse geocode failed, using placeholder");
                    _townName = "Custom Location";
                }
            }
            _locationLoaded = true;
            return true;
        }
        String postcode = _locPrefs.getString("postcode", "");
        _locPrefs.end();
        if (postcode.length() > 0) {
            float outLat = DEFAULT_LAT, outLon = DEFAULT_LON;
            String outTown = "London";
            if (geocodeName(postcode, outLat, outLon, outTown)) {
                _lat = outLat; _lon = outLon; _townName = outTown; _locationLoaded = true;
                _locPrefs.begin("location", false);
                _locPrefs.putFloat("lat", _lat);
                _locPrefs.putFloat("lon", _lon);
                _locPrefs.putString("town", _townName);
                _locPrefs.end();
                return true;
            }
        }
        // Fallback to London
        _lat = DEFAULT_LAT;
        _lon = DEFAULT_LON;
        _townName = "London";
        _locationLoaded = true;
        return true;
    }

public:
    // Force reload location from preferences and update weather immediately
    void reloadLocation() {
        Serial.println("[WeatherManager] reloadLocation() called");
        _locationLoaded = false;
        bool loaded = ensureLocationLoaded();
        _lastFetchEpoch = 0; // Force immediate weather update on next update() call
        Serial.print("[WeatherManager] Location reloaded: ");
        Serial.print(_townName);
        Serial.print(" (");
        Serial.print(_lat, 3);
        Serial.print(", ");
        Serial.print(_lon, 3);
        Serial.println(")");
        Serial.println("[WeatherManager] Next weather fetch will be forced immediately");
    }

    String getTownName() const { return _townName; }
    float getLatitude() const { return _lat; }
    float getLongitude() const { return _lon; }
    
    // Public method for external geocoding (used by config page verification)
    bool verifyAndGeocode(const String& query, float& outLat, float& outLon, String& outTown) {
        return geocodeName(query, outLat, outLon, outTown);
    }

private:

    // Reverse geocode coordinates to town name using open-meteo reverse geocoding API
    bool reverseGeocode(float lat, float lon, String& outTown) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WeatherManager::reverseGeocode] WiFi not connected");
            return false;
        }
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        // Use the proper reverse geocoding endpoint
        String url = "https://geocoding-api.open-meteo.com/v1/reverse?";
        url += "latitude=" + String(lat, 4);
        url += "&longitude=" + String(lon, 4);
        url += "&language=en&format=json&limit=1";
        
        Serial.print("[WeatherManager::reverseGeocode] URL: ");
        Serial.println(url);
        
        if (!http.begin(client, url)) {
            Serial.println("[WeatherManager::reverseGeocode] Failed to begin HTTP");
            return false;
        }
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.print("[WeatherManager::reverseGeocode] HTTP error: ");
            Serial.println(code);
            http.end();
            return false;
        }
        String payload = http.getString();
        http.end();
        
        // Parse response: find first result's name
        int resIdx = payload.indexOf("\"results\":[");
        if (resIdx < 0) {
            Serial.println("[WeatherManager::reverseGeocode] No results field found");
            return false;
        }
        int nameIdx = payload.indexOf("\"name\":", resIdx);
        if (nameIdx < 0) {
            Serial.println("[WeatherManager::reverseGeocode] No name field found");
            return false;
        }
        nameIdx += 7;  // length of "name":
        
        auto parseName = [](const String& s, int start)->String {
            int i = start;
            while (i < (int)s.length() && s[i] == ' ') i++;
            if (i < (int)s.length() && s[i] == '"') i++;
            int end = i;
            while (end < (int)s.length() && s[end] != '"') end++;
            return s.substring(i, end);
        };
        
        outTown = parseName(payload, nameIdx);
        Serial.print("[WeatherManager::reverseGeocode] Result: ");
        Serial.println(outTown);
        
        return outTown.length() > 0;
    }

    String urlEncode(const String& input) {
        String encoded = "";
        for (int i = 0; i < (int)input.length(); i++) {
            char c = input[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else if (c == ' ') {
                encoded += "%20";
            } else {
                encoded += "%";
                if (((uint8_t)c) < 0x10) encoded += "0";  // Pad to 2 digits
                encoded += String((uint8_t)c, HEX);  // Convert to hex
            }
        }
        return encoded;
    }

    // Attempt to geocode UK postcode using postcodes.io API
    bool geocodeUKPostcode(const String& postcode, float& outLat, float& outLon, String& outTown) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WeatherManager::geocodeUKPostcode] WiFi not connected");
            return false;
        }
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        String encodedPostcode = urlEncode(postcode);
        String url = "https://api.postcodes.io/postcodes/" + encodedPostcode;
        Serial.print("[WeatherManager::geocodeUKPostcode] Looking up UK postcode: ");
        Serial.println(postcode);
        Serial.print("[WeatherManager::geocodeUKPostcode] URL: ");
        Serial.println(url);
        
        if (!http.begin(client, url)) {
            Serial.println("[WeatherManager::geocodeUKPostcode] Failed to begin HTTP");
            return false;
        }
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.print("[WeatherManager::geocodeUKPostcode] HTTP error: ");
            Serial.println(code);
            http.end();
            return false;
        }
        String payload = http.getString();
        http.end();
        Serial.print("[WeatherManager::geocodeUKPostcode] Response length: ");
        Serial.println(payload.length());
        
        // Parse response: should have "result" object with "latitude", "longitude" and location name fields
        if (payload.indexOf("\"result\":null") >= 0) {
            Serial.println("[WeatherManager::geocodeUKPostcode] Postcode not found");
            return false;
        }
        
        int resultIdx = payload.indexOf("\"result\":{");
        if (resultIdx < 0) {
            Serial.println("[WeatherManager::geocodeUKPostcode] No result object found");
            return false;
        }
        
        int latIdx = payload.indexOf("\"latitude\":", resultIdx);
        int lonIdx = payload.indexOf("\"longitude\":", resultIdx);
        if (latIdx < 0 || lonIdx < 0) {
            Serial.println("[WeatherManager::geocodeUKPostcode] Failed to find latitude/longitude");
            return false;
        }
        
        latIdx += 11; // length of "latitude":
        lonIdx += 12; // length of "longitude":
        
        auto parseNumber = [](const String& s, int start)->float {
            int i = start;
            while (i < (int)s.length() && (s[i] == ' ')) i++;
            bool neg = false; if (i < (int)s.length() && s[i] == '-') { neg = true; i++; }
            float val = 0.0f; bool seen = false;
            while (i < (int)s.length() && isDigit(s[i])) { val = val * 10.0f + (s[i]-'0'); i++; seen = true; }
            if (i < (int)s.length() && s[i] == '.') {
                i++; float dec = 0.1f; while (i < (int)s.length() && isDigit(s[i])) { val += (s[i]-'0')*dec; dec *= 0.1f; i++; }
            }
            return neg ? -val : val;
        };
        auto parseStringField = [&](const String& key)->String {
            int keyIdx = payload.indexOf(key, resultIdx);
            if (keyIdx < 0) return String("");
            keyIdx += key.length();
            // Skip optional spaces and opening quote
            int i = keyIdx;
            while (i < (int)payload.length() && (payload[i] == ' ' || payload[i] == '"' || payload[i] == ':')) i++;
            // If previous char was ':', adjust start to next quote
            int quote = payload.indexOf('"', keyIdx);
            if (quote >= 0) {
                int end = payload.indexOf('"', quote + 1);
                if (end > quote) return payload.substring(quote + 1, end);
            }
            return String("");
        };
        
        outLat = parseNumber(payload, latIdx);
        outLon = parseNumber(payload, lonIdx);
        // Prefer a human-friendly place name: built-up area (bua), then parish, then admin_ward, then admin_district
        String town = parseStringField("\"bua\":");
        if (town.length() == 0) town = parseStringField("\"parish\":");
        if (town.length() == 0) town = parseStringField("\"admin_ward\":");
        if (town.length() == 0) town = parseStringField("\"admin_district\":");
        if (town.length() == 0) town = postcode; // fallback
        outTown = town;
        
        Serial.print("[WeatherManager::geocodeUKPostcode] Result: ");
        Serial.print(outTown);
        Serial.print(" (");
        Serial.print(outLat, 6);
        Serial.print(", ");
        Serial.print(outLon, 6);
        Serial.println(")");
        
        if (outLat == 0.0f && outLon == 0.0f) {
            Serial.println("[WeatherManager::geocodeUKPostcode] Invalid coordinates (0,0)");
            return false;
        }
        return true;
    }

    bool geocodeName(const String& query, float& outLat, float& outLon, String& outTown) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WeatherManager::geocodeName] WiFi not connected");
            return false;
        }
        
        // Check if query looks like a UK postcode (contains space and letters/numbers)
        // UK postcodes have format like "SW1A 1AA" or similar
        bool looksLikeUKPostcode = query.length() >= 6 && query.indexOf(' ') > 0;
        if (looksLikeUKPostcode) {
            Serial.println("[WeatherManager::geocodeName] Query looks like UK postcode, trying UK postcode lookup first...");
            if (geocodeUKPostcode(query, outLat, outLon, outTown)) {
                return true;
            }
            Serial.println("[WeatherManager::geocodeName] UK postcode lookup failed, falling back to city name lookup");
        }
        
        // Try open-meteo geocoding for city names
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        String encodedQuery = urlEncode(query);
        String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=" + encodedQuery;
        Serial.print("[WeatherManager::geocodeName] Query: '");
        Serial.print(query);
        Serial.print("' → Encoded: '");
        Serial.print(encodedQuery);
        Serial.println("'");
        Serial.print("[WeatherManager::geocodeName] URL: ");
        Serial.println(url);
        if (!http.begin(client, url)) {
            Serial.println("[WeatherManager::geocodeName] Failed to begin HTTP");
            return false;
        }
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.print("[WeatherManager::geocodeName] HTTP error: ");
            Serial.println(code);
            http.end();
            return false;
        }
        String payload = http.getString();
        http.end();
        Serial.print("[WeatherManager::geocodeName] Response length: ");
        Serial.println(payload.length());
        Serial.print("[WeatherManager::geocodeName] Response (first 200 chars): ");
        Serial.println(payload.substring(0, 200));
        
        // Very simple parsing: find first result's latitude, longitude, and name
        int resIdx = payload.indexOf("\"results\":[");
        if (resIdx < 0) {
            Serial.println("[WeatherManager::geocodeName] No 'results' field found");
            return false;
        }
        int latIdx = payload.indexOf("\"latitude\":", resIdx);
        int lonIdx = payload.indexOf("\"longitude\":", resIdx);
        int nameIdx = payload.indexOf("\"name\":", resIdx);
        if (latIdx < 0 || lonIdx < 0 || nameIdx < 0) {
            Serial.println("[WeatherManager::geocodeName] Failed to find latitude/longitude/name fields");
            return false;
        }
        latIdx += 11; // length of "latitude":
        lonIdx += 12; // length of "longitude":
        nameIdx += 7;  // length of "name":
        // Extract number until comma or closing brace
        auto parseNumber = [](const String& s, int start)->float {
            int i = start;
            while (i < (int)s.length() && (s[i] == ' ')) i++;
            bool neg = false; if (i < (int)s.length() && s[i] == '-') { neg = true; i++; }
            float val = 0.0f; bool seen = false;
            while (i < (int)s.length() && isDigit(s[i])) { val = val * 10.0f + (s[i]-'0'); i++; seen = true; }
            if (i < (int)s.length() && s[i] == '.') {
                i++; float dec = 0.1f; while (i < (int)s.length() && isDigit(s[i])) { val += (s[i]-'0')*dec; dec *= 0.1f; i++; }
            }
            return neg ? -val : val;
        };
        auto parseName = [](const String& s, int start)->String {
            int i = start;
            while (i < (int)s.length() && s[i] == ' ') i++;
            if (i < (int)s.length() && s[i] == '"') i++;
            int end = i;
            while (end < (int)s.length() && s[end] != '"') end++;
            return s.substring(i, end);
        };
        outLat = parseNumber(payload, latIdx);
        outLon = parseNumber(payload, lonIdx);
        outTown = parseName(payload, nameIdx);
        Serial.print("[WeatherManager::geocodeName] Result: ");
        Serial.print(outTown);
        Serial.print(" (");
        Serial.print(outLat, 6);
        Serial.print(", ");
        Serial.print(outLon, 6);
        Serial.println(")");
        // Basic validity check
        if (outLat == 0.0f && outLon == 0.0f) {
            Serial.println("[WeatherManager::geocodeName] Invalid coordinates (0,0)");
            return false;
        }
        return true;
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

        ensureLocationLoaded();

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
            Serial.println("WeatherManager: Failed to parse temperatures from API response");
            return false;
        }
        
        // Ensure we have matching data
        if (total != totalTemps) {
            Serial.printf("WeatherManager: Data mismatch - codes=%d, temps=%d\n", total, totalTemps);
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
