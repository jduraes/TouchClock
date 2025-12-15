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
    bool _provisioned;
    Preferences _prefs;
    DisplayManager* _display;

public:
    NetworkManager() 
        : _apName("TouchClock-Setup"),
          _server(nullptr),
          _dnsServer(nullptr),
          _provisioned(false),
          _display(nullptr) {}

    ~NetworkManager() {
        if (_server) delete _server;
        if (_dnsServer) delete _dnsServer;
    }

    void setDisplay(DisplayManager* display) {
        _display = display;
    }

    bool hasStoredCredentials() {
        _prefs.begin("wifi", true);  // read-only
        bool hasSSID = _prefs.isKey("ssid");
        _prefs.end();
        return hasSSID;
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
            
            // Try up to 3 times with 10-second timeout each, resetting WiFi between attempts
            const int maxRetries = 3;
            const int timeoutSeconds = 10;
            
            for (int attempt = 1; attempt <= maxRetries; attempt++) {
                Serial.print("Connection attempt ");
                Serial.print(attempt);
                Serial.print("/");
                Serial.println(maxRetries);
                
                if (_display) {
                    _display->showStatus("WiFi: " + storedSSID + " (attempt " + String(attempt) + "/" + String(maxRetries) + ")");
                }
                
                WiFi.mode(WIFI_STA);
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
                    return true;
                } else {
                    Serial.print("Connection failed, attempt ");
                    Serial.print(attempt);
                    Serial.println(" unsuccessful");
                    
                    // Reset WiFi for next attempt (except after last attempt)
                    if (attempt < maxRetries) {
                        WiFi.disconnect(true);  // true = turn off WiFi radio
                        Serial.println("WiFi reset, preparing for next attempt...");
                        delay(100);
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

        // Start HTTP server with provisioning UI
        _server = new WebServer(80);
        setupWebServer();
        _server->begin();

        Serial.println("HTTP server started");

        // Wait for provisioning to complete or timeout
        unsigned long startTime = millis();
        const unsigned long timeout = 180000;  // 3 minutes
        
        Serial.println("Waiting for provisioning...");
        while (!_provisioned && (millis() - startTime < timeout)) {
            _dnsServer->processNextRequest();
            _server->handleClient();
            delay(50);
        }

        // Stop DNS and HTTP server
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
        _server->stop();
        delete _server;
        _server = nullptr;

        if (_provisioned) {
            // Try to connect with provisioned credentials
            Serial.print("Connecting to: ");
            Serial.println(_selectedSSID);
            WiFi.mode(WIFI_STA);  // Switch to STA only
            WiFi.begin(_selectedSSID.c_str(), _selectedPass.c_str());
            
            unsigned long connStart = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - connStart < 30000) {
                delay(500);
                Serial.print(".");
            }
            Serial.println();

            if (WiFi.isConnected()) {
                Serial.print("Connected! IP: ");
                Serial.println(WiFi.localIP());
                
                // Save credentials to NVS for next boot
                _prefs.putString("ssid", _selectedSSID);
                _prefs.putString("pass", _selectedPass);
                Serial.println("WiFi credentials saved to NVS");
                
                return true;
            } else {
                Serial.println("Failed to connect");
                return false;
            }
        } else {
            Serial.println("Provisioning timeout - rebooting to retry");
            if (_display) {
                _display->showStatus("AP timeout, rebooting...");
            }
            delay(5000);
            ESP.restart();
            return false;
        }
    }

    const char* apName() const { 
        return _apName.c_str(); 
    }

private:
    void setupWebServer() {
        // Root redirects to /config
        _server->on("/", HTTP_GET, [this]() {
            _server->sendHeader("Location", "http://192.168.4.1/config");
            _server->send(302, "text/plain", "");
        });

        // Config page
        _server->on("/config", HTTP_GET, [this]() {
            String html = buildConfigPage();
            _server->send(200, "text/html", html);
        });

        // API endpoint to scan networks
        _server->on("/api/scan", HTTP_GET, [this]() {
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

        // API endpoint to submit credentials
        _server->on("/api/connect", HTTP_POST, [this]() {
            if (_server->hasArg("ssid") && _server->hasArg("pass")) {
                _selectedSSID = _server->arg("ssid");
                _selectedPass = _server->arg("pass");
                _provisioned = true;
                _server->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                _server->send(400, "application/json", "{\"error\":\"Missing credentials\"}");
            }
        });

        // Catch-all for captive portal
        _server->onNotFound([this]() {
            _server->sendHeader("Location", "http://192.168.4.1/config");
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
        <p style="text-align: center; color: #666;">Configure your WiFi network</p>
        
        <div class="input-group">
            <label>WiFi Network:</label>
            <div id="networks" style="border: 1px solid #ddd; border-radius: 4px; padding: 10px;"></div>
            <input type="text" id="ssid" placeholder="Or enter SSID manually" style="margin-top: 10px;">
        </div>

        <div class="input-group">
            <label for="pass">Password:</label>
            <input type="password" id="pass" placeholder="WiFi password">
        </div>

        <button onclick="connect()">Connect</button>
        <div id="status" class="status"></div>
    </div>

    <script>
        function showStatus(msg, type) {
            const status = document.getElementById('status');
            status.textContent = msg;
            status.className = 'status ' + type;
        }

        function scanNetworks() {
            fetch('/api/scan')
                .then(r => r.json())
                .then(networks => {
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
                .catch(e => showStatus('Failed to scan networks', 'error'));
        }

        function connect() {
            const ssid = document.getElementById('ssid').value;
            const pass = document.getElementById('pass').value;
            
            if (!ssid) {
                showStatus('Please select or enter an SSID', 'error');
                return;
            }

            showStatus('Connecting...', 'loading');
            
            fetch('/api/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
            })
            .then(r => r.json())
            .then(data => {
                if (data.status === 'ok') {
                    showStatus('✓ Connected! Device will restart...', 'success');
                } else {
                    showStatus('Error: ' + (data.error || 'Unknown'), 'error');
                }
            })
            .catch(e => showStatus('Connection failed', 'error'));
        }

        // Scan on load
        scanNetworks();
        setInterval(scanNetworks, 5000);
    </script>
</body>
</html>)HTML";
    }
};


