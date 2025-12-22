#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// Forward declaration
class DisplayManager;

class NetworkManager {
private:
    String _apName;
    WebServer *_server;
    DNSServer *_dnsServer;
    String _selectedSSID;
    String _selectedPass;
    String _selectedPostcode;
    String _selectedLat;
    String _selectedLon;
    bool _provisioned;
    bool _inApMode = false;
    unsigned long _apStartTime = 0;
    Preferences _prefs;
    Preferences _locPrefs;
    DisplayManager* _display;
    void* _weatherMgr;  // Use void* to avoid circular dependency
    bool _locationUpdated = false;  // Flag to signal location changed

public:
    NetworkManager() 
        : _apName("TouchClock-Setup"),
          _server(nullptr),
          _dnsServer(nullptr),
          _provisioned(false),
          _display(nullptr),
          _weatherMgr(nullptr) {}

    ~NetworkManager() {
        if (_server) delete _server;
        if (_dnsServer) delete _dnsServer;
    }

    void setDisplay(DisplayManager* display) {
        _display = display;
    }

    void setWeatherManager(void* weatherMgr) {
        _weatherMgr = weatherMgr;
    }

    bool hasStoredCredentials() {
        _prefs.begin("wifi", true);  // read-only
        bool hasSSID = _prefs.isKey("ssid");
        _prefs.end();
        return hasSSID;
    }

    // Must be called from main loop to handle server requests and AP timeout
    void update() {
        if (_server) {
            _server->handleClient();
        }
        if (_dnsServer) {
            _dnsServer->processNextRequest();
        }
        
        // If in AP mode, check for timeout (2 minutes)
        if (_inApMode && _apStartTime > 0) {
            unsigned long elapsed = millis() - _apStartTime;
            if (elapsed > 120000 && !_provisioned) {
                Serial.println("AP timeout - rebooting to retry");
                if (_display) {
                    _display->showStatus("AP timeout, rebooting...");
                }
                delay(2000);
                ESP.restart();
            }
        }
    }

    bool begin() {
        Serial.println("Starting WiFi connection...");
        
        // Try stored credentials first
        _prefs.begin("wifi", false);
        String storedSSID = _prefs.getString("ssid", "");
        String storedPass = _prefs.getString("pass", "");
        
        if (storedSSID.length() > 0) {
            Serial.print("Found stored credentials for: ");
            Serial.println(storedSSID);
            
            // Try up to 3 times with 20-second timeout each, resetting WiFi between attempts
            const int maxRetries = 3;
            const int timeoutSeconds = 20;

            // Optional: quick visibility on whether SSID is present (2.4GHz only)
            int foundChannel = -1;
            int nScan = WiFi.scanNetworks();
            for (int i = 0; i < nScan; i++) {
                if (WiFi.SSID(i) == storedSSID) {
                    foundChannel = WiFi.channel(i);
                    break;
                }
            }
            WiFi.scanDelete();
            if (foundChannel < 0) {
                Serial.println("[WiFi] Target SSID not visible on 2.4GHz scan. Ensure 2.4GHz is enabled and SSID is broadcasting.");
            } else {
                Serial.printf("[WiFi] Found SSID '%s' on channel %d\n", storedSSID.c_str(), foundChannel);
            }
            
            for (int attempt = 1; attempt <= maxRetries; attempt++) {
                Serial.print("Connection attempt ");
                Serial.print(attempt);
                Serial.print("/");
                Serial.println(maxRetries);
                
                if (_display) {
                    _display->showStatus("WiFi: " + storedSSID + " (attempt " + String(attempt) + "/" + String(maxRetries) + ")");
                }
                
                WiFi.mode(WIFI_STA);
                // Optional hostname for easier router identification
                WiFi.setHostname("TouchClock");
                WiFi.begin(storedSSID.c_str(), storedPass.c_str());
                
                // Wait up to 10 seconds for connection
                int elapsedSeconds = 0;
                while (WiFi.status() != WL_CONNECTED && elapsedSeconds < timeoutSeconds) {
                    delay(1000);
                    elapsedSeconds++;
                    if (_display) {
                        String progress = "WiFi: " + storedSSID + " (" + String(elapsedSeconds) + "/" + String(timeoutSeconds) + "s)";
                        _display->showStatus(progress);
                    }
                    Serial.print(".");
                }
                Serial.println();
                
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.print("Connected! IP: ");
                    Serial.println(WiFi.localIP());
                    ensureServerRunning(false); // Run HTTP server on STA IP
                    return true;
                } else {
                    Serial.print("Connection failed, attempt ");
                    Serial.print(attempt);
                    Serial.println(" unsuccessful");
                    Serial.print("[WiFi] Status code: ");
                    Serial.println(WiFi.status());
                    Serial.println("[WiFi] Tips: Use 2.4GHz, WPA2 (not WPA3), avoid hidden SSIDs.");
                    
                    // Reset WiFi for next attempt (except after last attempt)
                    if (attempt < maxRetries) {
                        WiFi.disconnect(true);  // true = turn off WiFi radio
                        Serial.println("WiFi reset, preparing for next attempt...");
                        delay(2000);
                    }
                }
            }
            
            // All 3 attempts failed
            Serial.println("Failed to connect after 3 attempts");
            if (_display) {
                _display->showStatus("WiFi failed after 3 attempts - switching to AP mode");
                delay(2000);
            }
            WiFi.disconnect();
            // Note: NOT clearing stored credentials - they are preserved for next boot
        }
        
        // No stored credentials or connection failed 3 times - start provisioning AP mode
        // Stored credentials are preserved and will be tried again on next boot
        Serial.println("Starting WiFi provisioning (AP mode)...");

        // Ensure clean WiFi state
        WiFi.softAPdisconnect(true);
        delay(100);
        
        // Set WiFi mode to AP+STA
        WiFi.mode(WIFI_AP_STA);

        // Enable provisioning in WiFi core
        WiFi.enableProv(true);

        // Create the SoftAP (open network)
        bool apStarted = WiFi.softAP(_apName.c_str(), NULL, 1, 0, 4, false);
        if (!apStarted) {
            Serial.println("Failed to start SoftAP");
            return false;
        }

        Serial.print("SoftAP started: ");
        Serial.println(WiFi.softAPSSID());
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());

        // Start DNS server (redirect all requests to AP IP)
        _dnsServer = new DNSServer();
        _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
        _dnsServer->start(53, "*", WiFi.softAPIP());

        // Start HTTP server with provisioning UI (AP mode)
        ensureServerRunning(true);

        Serial.println("HTTP server started in AP mode");

        _provisioned = false;
        _inApMode = true;
        _apStartTime = millis();
        
        // Return immediately - server will be handled in update() calls from main loop
        return false;  // Provisioning not yet complete; wait for user input
    }

    bool isProvisioned() const {
        return _provisioned;
    }

    const char* apName() const { 
        return _apName.c_str(); 
    }

    bool isInApMode() const {
        return _inApMode;
    }

    bool checkAndClearLocationUpdated() {
        if (_locationUpdated) {
            _locationUpdated = false;
            return true;
        }
        return false;
    }

    void ensureServerRunning(bool apMode) {
        _inApMode = apMode;
        // Stop DNS when not in AP captive portal mode
        if (!apMode && _dnsServer) {
            _dnsServer->stop();
            delete _dnsServer;
            _dnsServer = nullptr;
        }

        // If leaving AP mode, stop SoftAP to avoid interfering with STA traffic
        if (!apMode && WiFi.getMode() == WIFI_AP_STA) {
            WiFi.softAPdisconnect(true);
        }

        // Start HTTP server if not already running
        if (!_server) {
            _server = new WebServer(80);
            setupWebServer();
            _server->begin();
        }
    }

private:
    // Resolve the proper host/IP for the config server depending on mode
    String serverHost() {
        IPAddress ip = _inApMode ? WiFi.softAPIP() : WiFi.localIP();
        if (ip == IPAddress()) {
            // Fallback to canonical AP address if not yet assigned
            return String("192.168.4.1");
        }
        return ip.toString();
    }

    void setupWebServer() {
        // Root redirects to /config
        _server->on("/", HTTP_GET, [this]() {
            String host = serverHost();
            _server->sendHeader("Location", String("http://") + host + "/config");
            _server->send(302, "text/plain", "");
        });

        // Config page
        _server->on("/config", HTTP_GET, [this]() {
            String html = buildConfigPage();
            _server->send(200, "text/html", html);
        });

        // API endpoint to scan networks
        _server->on("/api/scan", HTTP_GET, [this]() {
            // Avoid channel-hopping scans while in STA mode (can glitch WiFi/display)
            if (!_inApMode) {
                _server->send(403, "application/json", "{\"error\":\"scan not available in STA mode\"}");
                return;
            }
            int n = WiFi.scanNetworks();
            String json = "[";
            for (int i = 0; i < n; i++) {
                if (i > 0) json += ",";
                json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
            }
            json += "]";
            _server->send(200, "application/json", json);
            WiFi.scanDelete();
        });

        // API endpoint to get current location
        _server->on("/api/location", HTTP_GET, [this]() {
            _locPrefs.begin("location", true);
            String postcode = _locPrefs.getString("postcode", "");
            float lat = _locPrefs.getFloat("lat", 0.0f);
            float lon = _locPrefs.getFloat("lon", 0.0f);
            String town = _locPrefs.getString("town", "");
            _locPrefs.end();
            
            String json = "{";
            json += "\"postcode\":\"" + postcode + "\",";
            json += "\"lat\":" + String(lat, 6) + ",";
            json += "\"lon\":" + String(lon, 6) + ",";
            json += "\"town\":\"" + town + "\"";
            json += "}";
            _server->send(200, "application/json", json);
        });

        // API endpoint to verify a location without saving
        _server->on("/api/verify-location", HTTP_POST, [this]() {
            bool hasCoords = _server->hasArg("lat") && _server->hasArg("lon") && _server->arg("lat").length() > 0 && _server->arg("lon").length() > 0;
            bool hasPostcode = _server->hasArg("postcode") && _server->arg("postcode").length() > 0;

            if (!hasCoords && !hasPostcode) {
                _server->send(400, "application/json", "{\"error\":\"Missing location data\"}");
                return;
            }

            String json = "{";
            if (hasCoords) {
                json += "\"lat\":" + _server->arg("lat") + ",";
                json += "\"lon\":" + _server->arg("lon") + ",";
                json += "\"valid\":true,";
                json += "\"town\":\"Custom Location\"";
            } else if (hasPostcode) {
                // Try to geocode the postcode
                float outLat = 0.0f, outLon = 0.0f;
                String outTown = "";
                String postcode = _server->arg("postcode");
                
                extern bool weatherManagerGeocode(void*, const String&, float&, float&, String&);
                if (_weatherMgr && weatherManagerGeocode(_weatherMgr, postcode, outLat, outLon, outTown)) {
                    json += "\"lat\":" + String(outLat, 6) + ",";
                    json += "\"lon\":" + String(outLon, 6) + ",";
                    json += "\"town\":\"" + outTown + "\",";
                    json += "\"valid\":true";
                } else {
                    json += "\"valid\":false,";
                    json += "\"error\":\"Location not found. Try a city name or country, e.g. Paris, London, New York\"";
                }
            }
            json += "}";
            _server->send(200, "application/json", json);
        });

        // API endpoint to submit credentials + optional location
        _server->on("/api/connect", HTTP_POST, [this]() {
            bool hasSsid = _server->hasArg("ssid") && _server->arg("ssid").length() > 0;
            bool hasPass = _server->hasArg("pass") && _server->arg("pass").length() > 0;
            bool hasCoords = _server->hasArg("lat") && _server->hasArg("lon") && _server->arg("lat").length() > 0 && _server->arg("lon").length() > 0;
            bool hasPostcode = _server->hasArg("postcode") && _server->arg("postcode").length() > 0;

            // Collect optional fields
            _selectedSSID = hasSsid ? _server->arg("ssid") : "";
            _selectedPass = hasPass ? _server->arg("pass") : "";
            _selectedPostcode = hasPostcode ? _server->arg("postcode") : "";
            _selectedLat = hasCoords ? _server->arg("lat") : "";
            _selectedLon = hasCoords ? _server->arg("lon") : "";

            // Persist location preferences for WeatherManager to use later
            if (hasCoords || hasPostcode) {
                _locPrefs.begin("location", false);
                if (hasCoords) {
                    float lat = _selectedLat.toFloat();
                    float lon = _selectedLon.toFloat();
                    _locPrefs.putFloat("lat", lat);
                    _locPrefs.putFloat("lon", lon);
                    _locPrefs.putString("postcode", "");
                    // If client provided a town name (from verification), persist it immediately
                    if (_server->hasArg("town") && _server->arg("town").length() > 0) {
                        _locPrefs.putString("town", _server->arg("town"));
                    }
                } else if (hasPostcode) {
                    // Geocode postcode immediately to store lat/lon and friendly town
                    float outLat = 0.0f, outLon = 0.0f; String outTown = "";
                    extern bool weatherManagerGeocode(void*, const String&, float&, float&, String&);
                    bool ok = (_weatherMgr && weatherManagerGeocode(_weatherMgr, _selectedPostcode, outLat, outLon, outTown));
                    if (ok) {
                        _locPrefs.putFloat("lat", outLat);
                        _locPrefs.putFloat("lon", outLon);
                        _locPrefs.putString("town", outTown);
                        _locPrefs.putString("postcode", _selectedPostcode);
                        Serial.printf("[Location Save] Geocoded '%s' → %s (%.4f, %.4f)\n", _selectedPostcode.c_str(), outTown.c_str(), outLat, outLon);
                    } else {
                        // Fallback: store postcode; WeatherManager will resolve later
                        _locPrefs.putString("postcode", _selectedPostcode);
                        Serial.printf("[Location Save] Geocode failed for '%s', stored postcode for later resolution\n", _selectedPostcode.c_str());
                    }
                }
                _locPrefs.end();
                Serial.print("Location saved: ");
                if (hasCoords) {
                    Serial.print("coords ");
                    Serial.print(_selectedLat);
                    Serial.print(",");
                    Serial.println(_selectedLon);
                } else {
                    Serial.print("postcode ");
                    Serial.println(_selectedPostcode);
                }
            }

            if (_inApMode) {
                // Provisioning requires WiFi credentials
                if (!(hasSsid && hasPass)) {
                    _server->send(400, "application/json", "{\"error\":\"Missing credentials\"}");
                    return;
                }
                _prefs.begin("wifi", false);
                _prefs.putString("ssid", _selectedSSID);
                _prefs.putString("pass", _selectedPass);
                _prefs.end();
                Serial.println("WiFi credentials saved");
                _provisioned = true;
                _server->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                // STA mode: allow location-only updates without forcing reconnection
                if (hasSsid && hasPass) {
                    // Optional future: support live WiFi change; for now, ignore to avoid disruption
                    Serial.println("Ignoring SSID/pass update in STA mode (not supported live)");
                }
                if (hasCoords || hasPostcode) {
                    Serial.println("[Location Update] Saving verified location...");
                    // Trigger weather manager to reload location and fetch new weather
                    // Use extern function to avoid circular dependency
                    extern void weatherManagerReload(void*);
                    if (_weatherMgr) {
                        weatherManagerReload(_weatherMgr);
                        _locationUpdated = true;  // Signal to main loop to force weather refresh
                        Serial.println("[Location Update] Weather reload triggered, flag set for immediate refresh");
                    } else {
                        Serial.println("[Location Update] WARNING: WeatherManager pointer is null!");
                    }
                    _server->send(200, "application/json", "{\"status\":\"ok\"}");
                } else {
                    _server->send(400, "application/json", "{\"error\":\"No data to update\"}");
                }
            }
        });

        // Catch-all for captive portal
        _server->onNotFound([this]() {
            String host = serverHost();
            _server->sendHeader("Location", String("http://") + host + "/config");
            _server->send(302, "text/plain", "");
        });
    }

    String buildConfigPage() {
        return R"HTML(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>TouchClock Setup</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 500px; margin: 50px auto; padding: 20px; background: #f5f5f5; }
        .container { background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; }
        .input-group { margin: 15px 0; }
        label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }
        input, select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; margin-top: 10px; }
        button:hover { background: #45a049; }
        .status { margin-top: 20px; padding: 10px; border-radius: 4px; text-align: center; display: none; }
        .status.loading { background: #e3f2fd; color: #1976d2; display: block; }
        .status.success { background: #c8e6c9; color: #2e7d32; display: block; }
        .status.error { background: #ffcdd2; color: #c62828; display: block; }
        #networks { max-height: 150px; overflow-y: auto; }
        .network-item { padding: 8px; margin: 5px 0; background: #f9f9f9; border: 1px solid #eee; border-radius: 4px; cursor: pointer; }
        .network-item:hover { background: #e8f5e9; }
    </style>
</head>
<body>
    <div class="container">
        <h1>⏰ TouchClock Setup</h1>
        <p style="text-align: center; color: #666;">Configure your device</p>
        
        <!-- WiFi Provisioning Form (AP Mode) -->
        <div id="wifi-form" style="display:none;">
            <h2 style="color:#333; border-bottom:2px solid #4CAF50; padding-bottom:10px;">WiFi Setup</h2>
            <p style="color:#666;">Select your network and enter the password.</p>
            
            <div class="input-group">
                <label>WiFi Network:</label>
                <div id="networks" style="border: 1px solid #ddd; border-radius: 4px; padding: 10px; min-height:40px;"></div>
                <input type="text" id="ssid" placeholder="Or enter SSID manually" style="margin-top: 10px;">
            </div>

            <div class="input-group">
                <label for="pass">Password:</label>
                <input type="password" id="pass" placeholder="WiFi password">
            </div>

            <div class="input-group">
                <label for="postcode-ap">Postcode / City / Place (optional):</label>
                <input type="text" id="postcode-ap" placeholder="e.g., SW1A 1AA, 10001, Paris">
            </div>

            <div class="input-group">
                <label>Coordinates (optional):</label>
                <div style="display:flex; gap:10px;">
                    <input type="text" id="lat-ap" placeholder="Latitude e.g., 51.5074" style="flex:1;">
                    <input type="text" id="lon-ap" placeholder="Longitude e.g., -0.1278" style="flex:1;">
                </div>
                <small style="color:#777;">If both are provided, coordinates take precedence. If neither is provided, default is London.</small>
            </div>

            <button onclick="connectWiFi()" style="background:#4CAF50;">Connect WiFi</button>
            <div id="status-wifi" class="status"></div>
        </div>

        <!-- Location Update Form (STA Mode) -->
        <div id="location-form" style="display:none;">
            <h2 style="color:#333; border-bottom:2px solid #2196F3; padding-bottom:10px;">Location Settings</h2>
            <p style="color:#666;">Update your location for accurate weather.</p>
            
            <div class="input-group">
                <label for="postcode-sta">Postcode / City / Place:</label>
                <input type="text" id="postcode-sta" placeholder="e.g., SW1A 1AA, Rio de Janeiro, Paris">
            </div>

            <div class="input-group">
                <label>Or Coordinates:</label>
                <div style="display:flex; gap:10px;">
                    <input type="text" id="lat-sta" placeholder="Latitude e.g., 51.5074" style="flex:1;">
                    <input type="text" id="lon-sta" placeholder="Longitude e.g., -0.1278" style="flex:1;">
                </div>
                <small style="color:#777;">If both are provided, coordinates take precedence.</small>
            </div>

            <button onclick="verifyLocation()" style="background:#2196F3;">Verify Location</button>
            <div id="verify-result" style="margin-top:15px; padding:10px; border-radius:4px; display:none; text-align:center;">
                <div id="verify-message"></div>
                <button id="save-btn" onclick="saveLocation()" style="background:#4CAF50; margin-top:10px; display:none;">Save & Update</button>
            </div>
            <div id="status-location" class="status"></div>
        </div>
    </div>

    <script>
        let inApMode = true;
        let scanTimer = null;
        let verifiedLocation = null;  // Stores verified location data

        function showStatus(msg, type, formType) {
            const statusId = formType === 'wifi' ? 'status-wifi' : 'status-location';
            const status = document.getElementById(statusId);
            status.textContent = msg;
            status.className = 'status ' + type;
        }

        function setMode(apMode) {
            inApMode = apMode;
            document.getElementById('wifi-form').style.display = apMode ? 'block' : 'none';
            document.getElementById('location-form').style.display = apMode ? 'none' : 'block';
            if (apMode) {
                stopScanLoop();
                startScanLoop();
            }
        }

        function startScanLoop() {
            if (scanTimer) clearInterval(scanTimer);
            scanTimer = setInterval(scanNetworks, 5000);
        }

        function stopScanLoop() {
            if (scanTimer) {
                clearInterval(scanTimer);
                scanTimer = null;
            }
        }

        function scanNetworks() {
            fetch('/api/scan')
                .then(r => {
                    if (r.status === 403) {
                        // Already in STA mode
                        setMode(false);
                        stopScanLoop();
                        return null;
                    }
                    setMode(true);
                    return r.json();
                })
                .then(networks => {
                    if (!networks) return;
                    const div = document.getElementById('networks');
                    div.innerHTML = '';
                    networks.forEach(net => {
                        const item = document.createElement('div');
                        item.className = 'network-item';
                        item.textContent = net.ssid + ' (' + net.rssi + ' dBm)';
                        item.onclick = () => { document.getElementById('ssid').value = net.ssid; };
                        div.appendChild(item);
                    });
                })
                .catch(() => {
                    // Scan failed - could be STA mode with 403 or network error
                    // If we detect 403 via status, mode will be set; otherwise stay in AP
                });
        }

        function connectWiFi() {
            const ssid = document.getElementById('ssid').value.trim();
            const pass = document.getElementById('pass').value;
            const postcode = document.getElementById('postcode-ap').value.trim();
            const lat = document.getElementById('lat-ap').value.trim();
            const lon = document.getElementById('lon-ap').value.trim();
            
            if (!ssid) {
                showStatus('Please select or enter an SSID', 'error', 'wifi');
                return;
            }

            showStatus('Connecting...', 'loading', 'wifi');
            
            const params = new URLSearchParams();
            params.append('ssid', ssid);
            params.append('pass', pass);
            if (postcode) params.append('postcode', postcode);
            if (lat) params.append('lat', lat);
            if (lon) params.append('lon', lon);
            
            fetch('/api/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: params.toString()
            })
            .then(r => r.json())
            .then(data => {
                if (data.status === 'ok') {
                    showStatus('✓ Connected! Device will restart...', 'success', 'wifi');
                } else {
                    showStatus('Error: ' + (data.error || 'Unknown'), 'error', 'wifi');
                }
            })
            .catch(() => showStatus('Connection failed', 'error', 'wifi'));
        }

        function loadCurrentLocation() {
            fetch('/api/location')
                .then(r => r.json())
                .then(data => {
                    const town = data.town && data.town.length ? data.town : '';
                    const pc = data.postcode && data.postcode.length ? data.postcode : '';
                    if (town || pc) {
                        document.getElementById('postcode-sta').placeholder = 'Current: ' + (town || pc);
                    } else if (data.lat && data.lon) {
                        document.getElementById('lat-sta').placeholder = 'Current: ' + data.lat.toFixed(4);
                        document.getElementById('lon-sta').placeholder = 'Current: ' + data.lon.toFixed(4);
                    }
                    const locLine = document.getElementById('location-form').querySelector('p');
                    if (town || pc || (data.lat && data.lon)) {
                        const coordStr = (data.lat && data.lon) ? (data.lat.toFixed(4) + ", " + data.lon.toFixed(4)) : '';
                        locLine.textContent = 'Current location: ' + (town || pc || coordStr) + '. Update below:';
                    }
                })
                .catch(() => {});
        }

        function verifyLocation() {
            const postcode = document.getElementById('postcode-sta').value.trim();
            const lat = document.getElementById('lat-sta').value.trim();
            const lon = document.getElementById('lon-sta').value.trim();
            
            if (!postcode && (!lat || !lon)) {
                document.getElementById('verify-result').style.display = 'none';
                showStatus('Enter postcode or both coordinates', 'error', 'location');
                return;
            }

            document.getElementById('verify-result').style.display = 'none';
            showStatus('Verifying location...', 'loading', 'location');
            
            const params = new URLSearchParams();
            if (postcode) params.append('postcode', postcode);
            if (lat) params.append('lat', lat);
            if (lon) params.append('lon', lon);
            
            fetch('/api/verify-location', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: params.toString()
            })
            .then(r => r.json())
            .then(data => {
                if (data.valid) {
                    showStatus('✓ Location verified!', 'success', 'location');
                    verifiedLocation = data;  // Save verified data
                    const resultDiv = document.getElementById('verify-result');
                    document.getElementById('verify-message').textContent = '✓ Valid: ' + data.town;
                    document.getElementById('save-btn').style.display = 'block';
                    resultDiv.style.display = 'block';
                } else {
                    showStatus('✗ ' + (data.error || 'Location not found'), 'error', 'location');
                    document.getElementById('verify-result').style.display = 'none';
                }
            })
            .catch(() => {
                showStatus('Verification failed', 'error', 'location');
                document.getElementById('verify-result').style.display = 'none';
            });
        }

        function saveLocation() {
            if (!verifiedLocation) {
                showStatus('Please verify location first', 'error', 'location');
                return;
            }

            showStatus('Saving and updating weather...', 'loading', 'location');
            
            const params = new URLSearchParams();
            if (verifiedLocation.lat) params.append('lat', verifiedLocation.lat);
            if (verifiedLocation.lon) params.append('lon', verifiedLocation.lon);
            if (verifiedLocation.town) params.append('town', verifiedLocation.town);
            
            fetch('/api/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: params.toString()
            })
            .then(r => r.json())
            .then(data => {
                if (data.status === 'ok') {
                    showStatus('✓ Location saved! ' + verifiedLocation.town + ' - Weather updating...', 'success', 'location');
                    // Clear fields, hide result, and reload current location
                    setTimeout(() => {
                        document.getElementById('postcode-sta').value = '';
                        document.getElementById('lat-sta').value = '';
                        document.getElementById('lon-sta').value = '';
                        document.getElementById('verify-result').style.display = 'none';
                        verifiedLocation = null;
                        loadCurrentLocation();
                    }, 1500);
                } else {
                    showStatus('Error saving: ' + (data.error || 'Unknown'), 'error', 'location');
                }
            })
            .catch(() => showStatus('Request failed', 'error', 'location'));
        }

        function updateLocation() {
            // Legacy function for backwards compatibility - now calls verifyLocation instead
            verifyLocation();
        }

        // Check mode on load
        scanNetworks();
        startScanLoop();
        loadCurrentLocation();
    </script>
</body>
</html>)HTML";
    }
};


