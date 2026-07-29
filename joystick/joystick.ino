// ─────────────────────────────────────────────────────────────────────
//  Futuretown – Patruljedysten, Gamers Edition · Joystick
//  Udviklet & sponsoreret af WeDoMobility ApS · wedomobility.com
//  © FutureTown · Licens: CC BY-NC 4.0 (Kreditering–IkkeKommerciel)
//  Fri brug og tilpasning for spejdere og spejdergrupper.
//  MÅ IKKE bruges kommercielt – bokse, kode eller koncept må ikke
//  sælges eller ændres for at tjene penge. Se LICENSE.
//  https://creativecommons.org/licenses/by-nc/4.0/
// ─────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <Audio.h>
#include <driver/gpio.h>
#include "LittleFS.h"
#include "config_manager.h"
#include "sound_manager.h"
#include <Adafruit_NeoPixel.h>
#include "esp_now_manager.h"
#include "wifi_manager.h"

// Status-LED (pin 21, 1 stk.)
#define PIN       21
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Spil-LEDs (pin 12, 15 stk.)
#define LED_PIN   12
#define LED_COUNT 15
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

// --- GLOBALE VARIABLER ---
Audio audio;
AsyncWebServer server(80);
bool configMode       = false;
bool skaltændewifi    = false;
unsigned long lastButtonScan = 0;

// --- PENDING FLAGS ---
// Skrives fra Core 0 (ESP-NOW callback), læses/håndteres på Core 1 (loop)
volatile bool pendingLed      = false;
volatile bool pendingLedBatch = false;
volatile bool pendingSound    = false;
volatile struct_led_cmd pendingLedCmd;
volatile byte           pendingLedBatchRaw[61];  // rå bytes – undgår volatile struct-problemer
volatile struct_cmd     pendingSoundCmd;
volatile uint8_t        wifiRequesterMAC[6];

// --- HJÆLPEFUNKTION ---
void setStatusLeds() {
    // LED 0: Grøn = ESP-NOW spiltilstand, Blå = WiFi/config
    if (configMode)
        leds.setPixelColor(1, leds.Color(0, 0, 200));
    else
        leds.setPixelColor(1, leds.Color(0, 200, 0));

    // LED 3: Grøn = Gamebox MAC konfigureret, Rød = ikke konfigureret
    bool macOk = (Gamebox_MAC != "FF:FF:FF:FF:FF:FF");
    leds.setPixelColor(3, macOk ? leds.Color(0, 200, 0) : leds.Color(200, 0, 0));

    leds.show();
}

// --- PROTOTYPER ---
void scanButtons();

// --- ESP-NOW MODTAGER (Core 0) ---
// MÅ IKKE kalde leds.show(), playSound(), Serial.printf() eller Flash-I/O.
// Sæt kun flags — al I/O håndteres i loop() på Core 1.
void OnDataRecv(const esp_now_recv_info* info, const uint8_t* data, int len) {
    // Enkelt LED (4 bytes)
    if (len == sizeof(struct_led_cmd)) {
        memcpy((void*)&pendingLedCmd, data, sizeof(pendingLedCmd));
        pendingLed = true;
        return;
    }
    // LED batch: 1 + count×4 bytes (5, 9, 13 … 61)
    if (len >= 5 && len <= 61 && (len - 1) % 4 == 0) {
        byte count = data[0];
        if (count >= 1 && count <= 15 && len == 1 + count * 4) {
            memcpy((void*)pendingLedBatchRaw, data, len);
            pendingLedBatch = true;
            return;
        }
    }
    if (len == sizeof(struct_cmd)) {
        memcpy((void*)&pendingSoundCmd, data, sizeof(pendingSoundCmd));
        if (data[0] == 250) {
            memcpy((void*)wifiRequesterMAC, info->src_addr, 6);
        }
        pendingSound = true;
    }
}

void setup() {
    // Frigiv GPIO-holds fra deep sleep og log årsag
    gpio_deep_sleep_hold_dis();
    Serial.begin(115200);
    esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
    if (wakeReason == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t wakeStatus = esp_sleep_get_ext1_wakeup_status();
        int wakePin = (wakeStatus > 0) ? __builtin_ctzll(wakeStatus) : -1;
        Serial.printf("[WAKE] Vågnet fra deep sleep – GPIO %d (mask: 0x%llX)\n", wakePin, wakeStatus);
    }

    pixels.begin();
    leds.begin();
    leds.clear();
    leds.show();
    delay(2000);
    pixels.setPixelColor(0, pixels.Color(128, 0, 0));
    pixels.show();

    if (!LittleFS.begin()) Serial.println("LittleFS Error!");

    getCurrentConfig();
    Serial.println("WiFi-channel: " + String(WiFi_Channel) + "  SyncWindow: " + String(SyncWindow) + "ms");

    for (int i = 0; i < BtnCount; i++) {
        if (ButtonPins[i] > 0) pinMode(ButtonPins[i], INPUT_PULLUP);
    }

    // Hold knap 1+2 nede ved boot → config mode
    if (BtnCount >= 2 && digitalRead(ButtonPins[0]) == LOW && digitalRead(ButtonPins[1]) == LOW) {
        configMode = true;
        initConfigAP();
        initWebServer(server);
        Serial.println("Boot: CONFIG MODE (SSID: JoystickSetup, IP: 192.168.4.1)");
        if (HasAudio) {
            initAudio(2, 3, 4);
            playSound(98, 15);
        }
        pixels.setPixelColor(0, pixels.Color(0, 0, 128));
        pixels.show();

    } else {
        // Spil-tilstand: ESP-NOW på konfigureret kanal
        if (HasAudio) initAudio(2, 3, 4);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        esp_wifi_set_channel(WiFi_Channel, WIFI_SECOND_CHAN_NONE);
        initESPNow(WiFi_Channel);
        pixels.setPixelColor(0, pixels.Color(0, 128, 0));
        pixels.show();
        Serial.println("Boot: SPIL-TILSTAND (ESP-NOW kanal " + String(WiFi_Channel) + ")");
        if (HasAudio) playSound(0, 15);
    }

    // LED-strip: vis status (LED 1 + LED 3)
    setStatusLeds();

    // Ping Gamebox ved boot (kun i spiltilstand med konfigureret MAC)
    if (!configMode && Gamebox_MAC != "FF:FF:FF:FF:FF:FF") {
        lastSendSuccess = false;
        sendPing();
        delay(200);                // vent på ACK fra Gameboxen
        if (lastSendSuccess) {
            leds.setPixelColor(3, leds.Color(0, 200, 0));   // Grøn = online
            Serial.println("[PING] Gamebox er online!");
        } else {
            leds.setPixelColor(3, leds.Color(255, 80, 0));  // Orange = offline
            Serial.println("[PING] Gamebox svarer ikke.");
        }
        leds.show();
    }
}

void loop() {
    // --- LED (Core 1 – sikkert at kalde RMT/NeoPixel her) ---
    if (pendingLed) {
        pendingLed = false;
        struct_led_cmd cmd;
        memcpy(&cmd, (const void*)&pendingLedCmd, sizeof(cmd));
        Serial.printf("[LED] Index: %d  RGB: (%d,%d,%d)\n", cmd.ledIndex, cmd.r, cmd.g, cmd.b);
        if (cmd.ledIndex == 255) {
            for (int i = 0; i < LED_COUNT; i++)
                leds.setPixelColor(i, leds.Color(cmd.r, cmd.g, cmd.b));
        } else if (cmd.ledIndex < LED_COUNT) {
            leds.setPixelColor(cmd.ledIndex, leds.Color(cmd.r, cmd.g, cmd.b));
        }
        leds.show();
    }

    // --- LED batch (Core 1) ---
    if (pendingLedBatch) {
        pendingLedBatch = false;
        EspNowLedBatch batch;
        memcpy(&batch, (const void*)pendingLedBatchRaw, sizeof(batch));
        for (int i = 0; i < batch.count && batch.count <= 15; i++) {
            byte idx = batch.leds[i].id;
            if (idx == 255) {
                for (int j = 0; j < LED_COUNT; j++)
                    leds.setPixelColor(j, leds.Color(batch.leds[i].r, batch.leds[i].g, batch.leds[i].b));
            } else if (idx < LED_COUNT) {
                leds.setPixelColor(idx, leds.Color(batch.leds[i].r, batch.leds[i].g, batch.leds[i].b));
            }
        }
        leds.show();
    }

    // --- Lyd + WiFi-trigger (Core 1 – sikkert at tilgå LittleFS her) ---
    if (pendingSound) {
        pendingSound = false;
        struct_cmd cmd;
        memcpy(&cmd, (const void*)&pendingSoundCmd, sizeof(cmd));
        Serial.printf("[ESP-NOW] Lyd ID: %d  Vol: %d\n", cmd.soundId, cmd.volume);

        if (cmd.soundId == 250) {
            if (!configMode) skaltændewifi = true;
            if (HasAudio) playSound(98, 15);
            leds.setPixelColor(1, leds.Color(0, 0, 200));
            leds.show();

        } else if (cmd.soundId == 251) {
            // --- DEEP SLEEP ---
            Serial.println("[SLEEP] Dvale-kommando modtaget – lukker ned...");
            if (HasAudio) { playSound(99, cmd.volume); delay(800); }

            // Sluk alle LEDs
            leds.clear(); leds.show();
            pixels.clear(); pixels.show();

            // Konfigurer GPIO-wake:
            // - Med lyd (HasAudio):  kun knap 1-4 (pin 7,8,9,10) — I2S bruger pin 2,3,4
            // - Uden lyd:            alle konfigurerede knapper 1-11
            int wakeCount = HasAudio ? min((int)BtnCount, 4) : min((int)BtnCount, 11);
            uint64_t wakeMask = 0;
            for (int i = 0; i < wakeCount; i++) {
                if (ButtonPins[i] > 0) wakeMask |= (1ULL << ButtonPins[i]);
            }
            if (wakeMask > 0) {
                esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);
                Serial.printf("[SLEEP] Vågner ved GPIO-mask: 0x%llX\n", wakeMask);
            }

            // Lås pull-up tilstand på hvert wake-pin inden sleep
            // (forhindrer ukoblede pins i at flyde til LOW og give falsk opvågning)
            for (int i = 0; i < wakeCount; i++) {
                byte pin = ButtonPins[i];
                if (pin > 0) {
                    gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
                    gpio_hold_en((gpio_num_t)pin);
                }
            }
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

            Serial.println("[SLEEP] God nat.");
            Serial.flush();
            esp_deep_sleep_start();

        } else if (HasAudio) {
            playSound(cmd.soundId, cmd.volume);
        }
    }

    // --- Dynamisk WiFi (STA) trigget af pakke 250 ---
    if (skaltændewifi) {
        skaltændewifi = false;
        if (initWiFiSTA()) {
            configMode = true;
            esp_now_deinit();
            initESPNow(WiFi_Channel);
            initWebServer(server);

            // Send WiFi-status tilbage til afsenderen
            uint8_t requester[6];
            memcpy(requester, (const void*)wifiRequesterMAC, 6);
            String ip   = WiFi.localIP().toString();
            String ssid = WiFi.SSID();
            sendWiFiStatus(requester, ip.c_str(), ssid.c_str());

            // Board-LED + strip LED 1 → Blå (WiFi aktivt)
            pixels.setPixelColor(0, pixels.Color(0, 0, 128));
            pixels.show();
            leds.setPixelColor(1, leds.Color(0, 0, 200));
            leds.show();

            Serial.println("[WiFi STA] Webserver kører. ESP-NOW på kanal " + String(WiFi_Channel));
        }
    }

    if (configMode && WiFi.status() == WL_CONNECTED) ElegantOTA.loop();

    if (millis() - lastButtonScan >= 5) {
        scanButtons();
        lastButtonScan = millis();
    }

    if (HasAudio) audio.loop();
}

void scanButtons() {
    static bool lastStates[12]       = {1,1,1,1,1,1,1,1,1,1,1,1};
    static unsigned long pressTime[4]   = {0,0,0,0};
    static unsigned long releaseTime[4] = {0,0,0,0};
    static bool syncActive = false;

    for (int i = 0; i < BtnCount; i++) {
        if (ButtonPins[i] == 0) continue;
        bool current = digitalRead(ButtonPins[i]);
        if (current != lastStates[i]) {
            if (i < 4) {
                if (current == LOW) pressTime[i]   = millis();
                else                releaseTime[i]  = millis();
            }
            sendButtonPress(i + 1, current == LOW);
            lastStates[i] = current;
            Serial.println("Button: " + String(i + 1) + (current == LOW ? " TRYK" : " SLIP"));
            delay(10);
        }
    }

    // Sync-tjek: kun hvis mindst 4 knapper er konfigureret
    if (BtnCount < 4 || ButtonPins[0] == 0 || ButtonPins[1] == 0 ||
        ButtonPins[2] == 0 || ButtonPins[3] == 0) return;

    bool allPressed  = (lastStates[0]==LOW  && lastStates[1]==LOW  && lastStates[2]==LOW  && lastStates[3]==LOW);
    bool allReleased = (lastStates[0]==HIGH && lastStates[1]==HIGH && lastStates[2]==HIGH && lastStates[3]==HIGH);

    if (allPressed && !syncActive) {
        unsigned long tMax = max(max(pressTime[0], pressTime[1]), max(pressTime[2], pressTime[3]));
        unsigned long tMin = min(min(pressTime[0], pressTime[1]), min(pressTime[2], pressTime[3]));
        if (tMin > 0 && (tMax - tMin) <= SyncWindow) {
            syncActive = true;
            sendButtonPress(255, true);
            Serial.printf("[SYNC] Tryk! Spread: %ums  (vindue: %dms)\n", (unsigned)(tMax - tMin), (int)SyncWindow);
        }
    }

    if (allReleased && syncActive) {
        unsigned long tMax = max(max(releaseTime[0], releaseTime[1]), max(releaseTime[2], releaseTime[3]));
        unsigned long tMin = min(min(releaseTime[0], releaseTime[1]), min(releaseTime[2], releaseTime[3]));
        if (tMin > 0 && (tMax - tMin) <= SyncWindow) {
            syncActive = false;
            sendButtonPress(255, false);
            Serial.printf("[SYNC] Slip! Spread: %ums\n", (unsigned)(tMax - tMin));
        }
    }
}
