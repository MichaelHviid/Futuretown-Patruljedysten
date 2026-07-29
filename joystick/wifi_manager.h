#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>
extern Adafruit_NeoPixel leds;
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include "LittleFS.h"
#include "config_manager.h"
#include "sound_manager.h"

// --- CONFIG AP ---
// Starter et fast access point til konfiguration.
// SSID: "JoystickSetup", kode: "12345678", vilkårlig kanal, ingen ESP-NOW.
void initConfigAP() {
    Serial.println("\n[WiFi] Starter config access point...");

    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    delay(200);

    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    if (WiFi.softAP("JoystickSetup", "12345678")) {
        Serial.printf("[WiFi] AP OK! SSID: JoystickSetup  IP: %s  TX: 19.5dBm\n",
                      WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[WiFi] FEJL: Kunne ikke starte AP!");
    }
}

// --- STA FORBINDELSE ---
// Forbinder til hjemme-WiFi (STA mode). Bruges når ESP-NOW pakke 250 modtages.
// Kalder IKKE preferences for kanal - ESP-NOW skal bruge WiFi.channel() efter forbindelse.
// Returnerer true hvis forbindelsen lykkedes.
bool initWiFiSTA() {
    // Brug globals indlæst af getCurrentConfig() i setup() — ét sæt defaults, ét sted
    Serial.printf("[WiFi STA] Forbinder til: %s ...\n", wifi_ssid.c_str());
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        WiFi_Channel = WiFi.channel();
        Serial.printf("\n[WiFi STA] Forbundet! IP: %s  Kanal: %d\n",
                      WiFi.localIP().toString().c_str(), WiFi_Channel);
        return true;
    }

    Serial.println("\n[WiFi STA] Kunne ikke forbinde.");
    return false;
}

// --- WEB SERVER ---
void initWebServer(AsyncWebServer &server) {
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc = getCurrentConfig();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"rebooting\"}");
        delay(500);
        ESP.restart();
    });

    server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "[";
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        bool first = true;
        while (file) {
            if (String(file.name()).endsWith(".mp3") || String(file.name()).endsWith(".wav")) {
                if (!first) json += ",";
                json += "{\"navn\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
                first = false;
            }
            file = root.openNextFile();
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    server.on("/api/play-file", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            deserializeJson(doc, data);
            playSoundDirect(doc["fil"].as<String>());
            request->send(200, "application/json", "{\"status\":\"playing\"}");
        }
    );

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            deserializeJson(doc, data, len);

            preferences.begin("joystick", false);
            if (doc["JoyID"].is<int>())        preferences.putUChar("JoyID",     doc["JoyID"]);
            if (doc["HasAud"].is<bool>())      preferences.putBool("HasAud",    doc["HasAud"]);
            if (doc["BtnCnt"].is<int>())       preferences.putUChar("BtnCnt",   doc["BtnCnt"]);
            if (doc["Vol"].is<int>())          preferences.putUChar("Vol",      doc["Vol"]);
            if (doc["WiFi_Chan"].is<int>())    preferences.putUChar("WiFi_Chan",doc["WiFi_Chan"]);
        if (doc["SyncMs"].is<int>())       preferences.putUChar("SyncMs",   doc["SyncMs"]);
            if (doc["GB_MAC"].is<String>())    preferences.putString("GB_MAC",  doc["GB_MAC"].as<String>());
            if (doc["Name"].is<String>())      preferences.putString("Name",    doc["Name"].as<String>());
            if (doc["wifi_ssid"].is<String>()) preferences.putString("wifi_ssid",doc["wifi_ssid"].as<String>());

            if (doc["wifi_pass"].is<String>()) {
                String nyKode = doc["wifi_pass"].as<String>();
                if (nyKode.length() > 0) {
                    preferences.putString("wifi_pass", nyKode);
                }
            }

            for (int i = 1; i <= 11; i++) {
                String pKey = "P" + String(i);
                if (doc[pKey].is<int>()) preferences.putUChar(pKey.c_str(), doc[pKey]);
            }
            preferences.end();

            request->send(200, "application/json", "{\"status\":\"saved\"}");
        }
    );

    // LED-test fra webinterface
    server.on("/api/led", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            deserializeJson(doc, data, len);
            byte ledIndex = doc["index"].as<int>();
            byte r = doc["r"].as<int>();
            byte g = doc["g"].as<int>();
            byte b = doc["b"].as<int>();
            if (ledIndex == 255) {
                for (int i = 0; i < leds.numPixels(); i++)
                    leds.setPixelColor(i, leds.Color(r, g, b));
            } else if (ledIndex < leds.numPixels()) {
                leds.setPixelColor(ledIndex, leds.Color(r, g, b));
            }
            leds.show();
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    ElegantOTA.begin(&server);
    server.begin();
}

#endif
