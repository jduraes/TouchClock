#pragma once
#include <WiFi.h>

class NetworkManager {
public:
    void begin(const char* ssid, const char* password) {
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
        }
    }
};
