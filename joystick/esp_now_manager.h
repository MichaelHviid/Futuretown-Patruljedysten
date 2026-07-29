#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Joystick → Gamebox: knaptryk
typedef struct struct_message {
    byte id;
    byte button;
    bool pressed;
} struct_message;

// Gamebox → Joystick: lydkommando
typedef struct struct_cmd {
    byte soundId;
    byte volume;
} struct_cmd;

// Gamebox → Joystick: enkelt LED (4 bytes)
// ledIndex 255 = alle LEDs
typedef struct struct_led_cmd {
    byte ledIndex;
    byte r;
    byte g;
    byte b;
} struct_led_cmd;

// Gamebox → Joystick: LED batch (1 + count×4 bytes → 5, 9, 13 … 61)
// Størrelses-formlen sikrer ingen kollision med andre pakketyper.
// id=255 i en enkelt entry = sæt alle LEDs til den farve.
struct EspNowLedBatch {
    byte count;           // antal entries (1–15)
    struct {
        byte id, r, g, b;
    } leds[15];
};

// Joystick → Gamebox: ping + identifikation ved opstart
// Gamebox ignorerer den indtil videre (størrelsen matcher ingen kendt pakke)
typedef struct struct_ping {
    byte id;        // Joystick ID
    char name[20];  // Joystick navn (max 19 tegn + null)
} struct_ping;

// Joystick → Gamebox: WiFi-status svar på pakke 250
typedef struct struct_wifi_status {
    char ip[16];    // f.eks. "192.168.1.42" eller "192.168.4.1"
    char ssid[33];  // tilsluttet SSID eller "JoystickSetup"
} struct_wifi_status;


uint8_t broadcastAddress[6];
struct_message myData;

volatile bool lastSendSuccess = false;
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    lastSendSuccess = (status == ESP_NOW_SEND_SUCCESS);
}

void OnDataRecv(const esp_now_recv_info* info, const uint8_t* data, int len);

void parseMAC(String macStr, uint8_t *byteArray) {
    int values[6];
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; ++i) byteArray[i] = (uint8_t)values[i];
    }
}

void initESPNow(int kanal) {
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
    esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);

    parseMAC(Gamebox_MAC, broadcastAddress);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = kanal;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer (Gamebox)");
    } else {
        Serial.printf("ESP-NOW startet! Peer kanal: %d (hardware: %d)\n", kanal, WiFi.channel());
    }
}

void sendPing() {
    struct_ping pkt = {};
    pkt.id = Joystick_ID;
    strncpy(pkt.name, Joystick_Navn.c_str(), sizeof(pkt.name) - 1);
    esp_now_send(broadcastAddress, (uint8_t*)&pkt, sizeof(pkt));
    Serial.printf("[PING] Sender ID: %d  Navn: %s\n", pkt.id, pkt.name);
}

void sendButtonPress(int btnId, bool state) {
    myData.id      = Joystick_ID;
    myData.button  = btnId;
    myData.pressed = state;
    esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));
}

// Send WiFi-status tilbage til afsenderen af pakke 250
void sendWiFiStatus(const uint8_t* targetMAC, const char* ip, const char* ssid) {
    struct_wifi_status status;
    strncpy(status.ip,   ip,   sizeof(status.ip)   - 1); status.ip[sizeof(status.ip)-1]     = 0;
    strncpy(status.ssid, ssid, sizeof(status.ssid) - 1); status.ssid[sizeof(status.ssid)-1] = 0;

    if (!esp_now_is_peer_exist(targetMAC)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, targetMAC, 6);
        peerInfo.channel = WiFi_Channel;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    esp_now_send(targetMAC, (uint8_t*)&status, sizeof(status));
    Serial.printf("[ESP-NOW] WiFi-status sendt → IP: %s  SSID: %s\n", ip, ssid);
}

#endif
