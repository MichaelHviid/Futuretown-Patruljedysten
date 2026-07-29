#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>
#include <WiFi.h>

Preferences preferences;

String wifi_ssid, wifi_pass, Gamebox_MAC, Joystick_Navn;
byte Volume, BtnCount, WiFi_Channel, SyncWindow;
bool HasAudio;
byte Joystick_ID;
byte ButtonPins[12];

std::vector<const char*> byteKeys   = {"Vol", "BtnCnt", "HasAud", "WiFi_Chan", "SyncMs", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8", "P9", "P10", "P11"};
std::vector<const char*> intKeys    = {};
std::vector<const char*> stringKeys = {"wifi_ssid", "wifi_pass", "GB_MAC", "Name"};

JsonDocument getCurrentConfig() {
    preferences.begin("joystick", true);

    Joystick_Navn = preferences.getString("Name",     "Joystick_XX");
    Gamebox_MAC   = preferences.getString("GB_MAC",   "FF:FF:FF:FF:FF:FF");
    wifi_ssid     = preferences.getString("wifi_ssid","GamersWiFi");
    wifi_pass     = preferences.getString("wifi_pass","12345678");

    Joystick_ID  = preferences.getUChar("JoyID",    1);
    HasAudio     = preferences.getBool ("HasAud",   true);
    BtnCount     = preferences.getUChar("BtnCnt",   4);
    Volume       = preferences.getUChar("Vol",      21);
    WiFi_Channel = preferences.getUChar("WiFi_Chan",6);
    SyncWindow   = preferences.getUChar("SyncMs",   50);

    for (int i = 0; i < 11; i++) {
        String key = "P" + String(i + 1);
        byte def = (i == 0) ? 7 : (i == 1) ? 8 : (i == 2) ? 9 : (i == 3) ? 10 : 0;
        ButtonPins[i] = preferences.getUChar(key.c_str(), def);
    }

    preferences.end();

    JsonDocument doc;
    doc["Name"]       = Joystick_Navn;
    doc["JoyID"]      = Joystick_ID;
    doc["GB_MAC"]     = Gamebox_MAC;
    doc["mac"]        = WiFi.macAddress();
    doc["ip"]         = WiFi.localIP().toString();
    doc["HasAud"]     = HasAudio;
    doc["BtnCnt"]     = BtnCount;
    doc["Vol"]        = Volume;
    doc["WiFi_Chan"]  = WiFi_Channel;
    doc["aktiv_kanal"]= WiFi.channel();
    doc["wifi_ssid"]  = wifi_ssid;
    doc["SyncMs"]     = SyncWindow;

    for (int i = 0; i < 11; i++) {
        doc["P" + String(i + 1)] = ButtonPins[i];
    }

    return doc;
}

#endif
