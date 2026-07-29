#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <esp_wifi.h>       // esp_wifi_set_promiscuous()
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include "LittleFS.h"
// ESP-NOW inkluderes her så WiFi-manager kan initialisere det og sende lyde/LEDs
#include "espnow_buttons.h"
#include "espnow_sound.h"
#include "espnow_leds.h"

// ---------------------------------------------------------------------------
//  WebSocket  –  live knap-events til browseren
// ---------------------------------------------------------------------------
AsyncWebSocket _gbws("/ws");

// AP-mode flag – true når gamebox kører som AP (ingen router-forbindelse)
bool _gb_isAPMode = false;

// Web-server flag – sættes af initWiFi(), læses i Gamebox.ino setup()
// false i spil-mode (ingen knap) → web-server startes ikke
bool _gb_webServerNeeded = false;

// Blå onboard LED (GPIO 2) – tændes når WiFi/AP er aktivt (kun WiFi-mode og config-mode)
#define ONBOARD_LED 2
inline void _setWifiLed(bool on) {
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, on ? HIGH : LOW);
}

// Vi skal bruge disse fra config_manager
extern JsonDocument getCurrentConfig();
extern String Red_mac, Green_mac;
extern String jsonConfig;
extern std::vector<const char*> byteKeys;
extern std::vector<const char*> intKeys;
extern std::vector<const char*> stringKeys;
extern std::vector<const char*> floatKeys;


extern Preferences preferences;
// ---------------------------------------------------------------------------
//  broadcastButtonEvent()
//  Sender et JSON-objekt til alle tilsluttede WebSocket-klienter.
// ---------------------------------------------------------------------------
void broadcastButtonEvent(byte joystickId, const uint8_t* mac, byte buttonId, bool isPressed) {
  if (_gbws.count() == 0) return;  // Ingen browsere tilsluttet – spring over

  // Ny mapping: 1=Rød, 2=Gul, 3=Grøn, 4=Blå
  const char* btnNames[] = { "Rod", "Gul", "Groen", "Blaa" };
  const char* btnName = (buttonId == 255) ? "Alle" :
                        (buttonId >= 1 && buttonId <= 4) ? btnNames[buttonId - 1] : "?";

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  JsonDocument doc;
  doc["type"]      = "button_event";
  doc["joystick"]  = joystickId;   // 0 = Groent (A), 1 = Roedt (B), 255 = ukendt
  doc["mac"]       = macStr;
  doc["buttonId"]  = buttonId;
  doc["btnName"]   = btnName;
  doc["isPressed"] = isPressed;
  doc["unknown"]   = (joystickId == 255);  // true = MAC ikke konfigureret endnu

  String json;
  serializeJson(doc, json);
  _gbws.textAll(json);
}

// ---------------------------------------------------------------------------
//  _onBtnEvent()  –  Kædet callback: WebSocket-broadcast OG button_manager
//  Defineret HER (efter broadcastButtonEvent) så begge er i scope.
// ---------------------------------------------------------------------------
static void _onBtnEvent(byte joyId, const uint8_t* mac, byte btnId, bool pressed) {
  broadcastButtonEvent(joyId, mac, btnId, pressed);
  onJoyButtonEvent(joyId, mac, btnId, pressed);
}

// ---------------------------------------------------------------------------
//  _initEspNow()  –  Starter ESP-NOW ovenpå den allerede-aktive WiFi-forbindelse.
//
//  BEMÆRK: Denne funktion kaldes indefra initWiFi(), som kører FØR
//  getCurrentConfig() i Gamebox.ino. Derfor læser vi Green_mac / Red_mac
//  direkte fra Preferences her, i stedet for at bruge de globale strings.
// ---------------------------------------------------------------------------
static void _initEspNow() {
  // Læs joystick-MACs direkte fra Preferences (getCurrentConfig ikke kaldt endnu)
  preferences.begin("gamebox", true);
  String gMac = preferences.getString("Green_mac", "");
  String rMac = preferences.getString("Red_mac",   "");
  preferences.end();

  if (gMac.length() >= 17) {
    setJoystickMACFromString(JOY_A, gMac.c_str());
    Serial.println("[ESP-NOW] Green MAC fra prefs: " + gMac);
  } else {
    Serial.println("[ESP-NOW] Green MAC ikke konfigureret – bruger auto-discovery.");
  }
  if (rMac.length() >= 17) {
    setJoystickMACFromString(JOY_B, rMac.c_str());
    Serial.println("[ESP-NOW] Red MAC fra prefs: " + rMac);
  } else {
    Serial.println("[ESP-NOW] Red MAC ikke konfigureret – bruger auto-discovery.");
  }

  // ── DIAGNOSTIK: Gamebox identitet ──────────────────────────────
  // I AP-mode: hent MAC via ESP-IDF (WiFi.macAddress() giver kun STA-MAC)
  uint8_t mac[6];
  wifi_mode_t wm; esp_wifi_get_mode(&wm);
  esp_wifi_get_mac((wm==WIFI_MODE_AP||wm==WIFI_MODE_APSTA) ? WIFI_IF_AP : WIFI_IF_STA, mac);
  Serial.printf("[ESP-NOW] >>> GAMEBOX MAC: %02X:%02X:%02X:%02X:%02X:%02X <<<\n",
                mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  Serial.printf("[ESP-NOW] WiFi kanal: %d  mode: %s\n",
                WiFi.channel(), (wm==WIFI_MODE_AP)?"AP":(wm==WIFI_MODE_STA)?"STA":"APSTA");

  // ── KØ til Core 0→Core 1 kommunikation ─────────────────────────
  initEspNowQueue();

  esp_err_t r = esp_now_init();
  if (r == ESP_OK) {
    Serial.println("[ESP-NOW] Initialiseret (WiFi+ESP-NOW aktiv).");
  } else if (r == ESP_ERR_ESPNOW_BASE) {
    Serial.println("[ESP-NOW] Allerede initialiseret – OK.");
  } else {
    Serial.printf("[ESP-NOW] Init fejlede (fejl: %d)\n", r);
    return;
  }

  // Registrer modtage-callback (tjek returkode!)
  esp_err_t cbErr = esp_now_register_recv_cb(_espnow_onBtnReceive);
  Serial.printf("[ESP-NOW] recv_cb: %s\n", cbErr == ESP_OK ? "OK" : "FEJL!");

  // Kobl knap-events: WebSocket-broadcast OG button_manager (kædet callback)
  setButtonEventCallback(_onBtnEvent);

  // Ping-event: joystik melder sig online ved opstart
  setPingEventCallback([](byte joyId, const uint8_t* mac, byte pingId, const char* name) {
    if (_gbws.count() == 0) return;
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    JsonDocument doc;
    doc["type"]         = "ping_event";
    doc["joystick"]     = joyId;
    doc["joystick_id"]  = pingId;
    doc["name"]         = name;
    doc["mac"]          = macStr;
    String json; serializeJson(doc, json);
    _gbws.textAll(json);
  });

  // WiFi-status: joystik svarer med IP efter lyd-ID 250
  setWifiStatusCallback([](byte joyId, const uint8_t* mac, const char* ip, const char* ssid) {
    if (_gbws.count() == 0) return;
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    JsonDocument doc;
    doc["type"]     = "wifi_status";
    doc["joystick"] = joyId;
    doc["ip"]       = ip;
    doc["ssid"]     = ssid;
    doc["mac"]      = macStr;
    String json; serializeJson(doc, json);
    _gbws.textAll(json);
  });

  // Diagnostik: send alle rå ESP-NOW-pakker til browseren (hjælper ved fejlfinding)
  setRawEventCallback([](const uint8_t* mac, const uint8_t* data, int len, bool isBroadcast) {
    if (_gbws.count() == 0) return;

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char hexBuf[37] = {};
    int printLen = len < 12 ? len : 12;
    for (int i = 0; i < printLen; i++) {
      snprintf(hexBuf + i * 3, 4, "%02X ", data[i]);
    }

    JsonDocument doc;
    doc["type"]  = "raw_espnow";
    doc["mac"]   = macStr;
    doc["len"]   = len;
    doc["hex"]   = hexBuf;
    doc["bcast"] = isBroadcast;   // true = broadcast, false = unicast

    String json;
    serializeJson(doc, json);
    _gbws.textAll(json);
  });

  // Registrer lyd-peers
  initEspNowSound();

  // Remote MAC fra preferences → registrer som peer
  preferences.begin("gamebox", true);
  String rMacStr = preferences.getString("Remote_mac", "");
  preferences.end();
  if (rMacStr.length() >= 17) {
    setRemoteMACFromString(rMacStr.c_str());
  }

  // Remote-kommando callback → sæt flags der håndteres i Gamebox.ino loop()
  setRemoteCmdCallback([](byte cmd, byte p1, byte p2, const uint8_t*) {
    _remoteCmdPending = cmd;
    _remoteParam1     = p1;
    _remoteParam2     = p2;
  });

  Serial.println("[ESP-NOW] Klar – modtager knapper OG kan sende lyd.");
}

// ---------------------------------------------------------------------------
//  WebSocket event handler – inkl. indgående lyd-kommandoer fra browseren
// ---------------------------------------------------------------------------
static void _sendDeviceInfo(AsyncWebSocketClient* client = nullptr) {
  JsonDocument doc;
  doc["type"]    = "device_info";
  doc["mac"]     = WiFi.macAddress();
  doc["channel"] = WiFi.channel();
  String json;
  serializeJson(doc, json);
  if (client) client->text(json);
  else _gbws.textAll(json);
}

static void _handleWsData(uint8_t* data, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) return;

  String action = doc["action"].as<String>();

  if (action == "send_sound") {
    byte joyId   = doc["joystick"]  | 0;
    byte soundId = doc["soundId"]   | 0;
    byte volume  = doc["volume"]    | ESPNOW_SOUND_DEFAULT_VOL;
    sendSoundOrBroadcast(joyId, soundId, volume);
    Serial.printf("[WS→SOUND] joy=%d sound=%d vol=%d\n", joyId, soundId, volume);
  }
  // Remote control actions fra browser (WebSocket → gamebox → handling i loop())
  else if (action == "remote_red_btn")  { _remoteActionPending = 1; }
  else if (action == "remote_green_btn"){ _remoteActionPending = 2; }
  else if (action == "send_to_remote")  { _remoteActionPending = 3; }
  else if (action == "send_led") {
    byte joyId = doc["joystick"] | 0;
    byte ledId = doc["ledId"]    | 0;
    byte r     = doc["r"]        | 0;
    byte g     = doc["g"]        | 0;
    byte b     = doc["b"]        | 0;
    forceSendLed(joyId, ledId, CRGB(r, g, b));
  }
}

static void _onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Browser #%u tilsluttet\n", client->id());
    _sendDeviceInfo(client);
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Browser #%u afbrudt\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    // Kun behandl hele, afsluttede beskeder (ikke fragmenterede)
    if (info->final && info->index == 0 && info->len == len) {
      _handleWsData(data, len);
    }
  }
}

// Kald denne i loop() – VIGTIGT for korrekt ESP-NOW + WebSocket funktion
// Kører på Core 1 (main loop): trådsikkert at kalde WebSocket og Serial herfra
void loopWebServer() {
  _gbws.cleanupClients();
  processEspNowQueue();  // Tøm ESP-NOW kø fra Core 0 → kald callbacks sikkert
}

// ---------------------------------------------------------------------------
//  disconnectWiFiForGameplay()
//  Frakobler WiFi-forbindelsen og sætter ESP-NOW til preferences-kanalen.
//  Kaldt første gang et spil startes – WiFi genstartes ikke igen i sessionen.
//
//  Fremgangsmåde:
//    1. WiFi.disconnect(false)  – frakobler router, men holder radio tændt
//    2. esp_wifi_set_channel()  – sætter ESP-NOW's kanal eksplicit
//    3. esp_now_mod_peer()      – opdaterer alle peers til den nye kanal
// ---------------------------------------------------------------------------
void disconnectWiFiForGameplay() {
  // Tvungen konfig-AP: ESP-NOW er ikke initialiseret – intet at gøre
  if (_bm_forceAPMode) {
    Serial.println("[WiFi] Konfig-AP mode – ingen WiFi-frakobiling.");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Ikke forbundet til router – intet at frakoble.");
    return;
  }

  // Læs præference-kanalen
  preferences.begin("gamebox", true);
  int ch = preferences.getInt("wifi_channel", 1);
  preferences.end();

  // Slå auto-genopkobling fra FØR disconnect – ellers prøver stacken at reconnecte
  WiFi.setAutoReconnect(false);

  // Frakobil router – behold radio tændt til ESP-NOW.
  // VIGTIG: Skift IKKE kanal! Joystikkene kommunikerer allerede på routerens kanal,
  // og esp_wifi_set_channel() under aktiv ESP-NOW kan forårsage en crash.
  WiFi.disconnect(false);
  _setWifiLed(false);  // Admin ikke længere tilgængeligt
  Serial.printf("[WiFi] Frakoblet router – ESP-NOW fortsætter på kanal %d\n", ch);
}

void initWiFi() {
  // Alle preferences er garanteret skrevet (ensureDefaultsWritten kaldt i setup)
  preferences.begin("gamebox", true);
  String storedSSID = preferences.getString("wifi_ssid",    "GamersWiFi");
  String storedPass = preferences.getString("wifi_pass",    "12345678");
  String AP_SSID    = preferences.getString("Gamebox_SSID", "GameboxSetup");
  String AP_pass    = preferences.getString("Gamebox_pass", "12345678");
  String navnSSID   = preferences.getString("Navn", _defaultNavn());
  int    storedCh   = preferences.getInt("wifi_channel", 6);
  preferences.end();

  // ── MODE 1: Rød+Grøn – Config-mode ────────────────────────────────────────
  // "GameboxSetup" AP på tilfældig kanal. Ingen ESP-NOW. Kun web-admin.
  if (_bm_forceAPMode) {
    int ch = random(1, 14);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("GameboxSetup", "12345678", ch);
    _gb_isAPMode        = true;
    _gb_webServerNeeded = true;
    _setWifiLed(true);
    Serial.printf("[WiFi] CONFIG-MODE: SSID=GameboxSetup kode=12345678 kanal=%d\n", ch);
    Serial.println("[WiFi] IP: 192.168.4.1  (ESP-NOW ikke aktiv)");
    return;  // Ingen _initEspNow
  }

  // ── MODE 2: Ingen knap – Spil-mode ────────────────────────────────────────
  // Springer WiFi over. APSTA på konfigureret kanal – AP'et er kun et
  // kanal-anker og annoncerer gameboxens NAVN (ikke hjemme-SSID'et, som
  // ellers forveksles med routeren). Joystiks bruger kun kanal-nummeret.
  // Ingen web-server, hurtig boot.
  if (!_bm_wifiMode) {
    Serial.printf("[WiFi] SPIL-MODE: APSTA '%s' kanal %d (ingen web-server)\n",
                  navnSSID.c_str(), storedCh);
    WiFi.mode(WIFI_AP_STA);
    // WPA2 kræver mindst 8 tegn – ellers åbent AP
    const char* ankerPw = (storedPass.length() >= 8) ? storedPass.c_str() : NULL;
    WiFi.softAP(navnSSID.c_str(), ankerPw, storedCh);
    WiFi.disconnect(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    _gb_isAPMode        = true;
    _gb_webServerNeeded = false;
    _setWifiLed(false);  // Ingen admin i spil-mode
    Serial.printf("[WiFi] AP IP: %s  kanal: %d\n",
                  WiFi.softAPIP().toString().c_str(), storedCh);
    _initEspNow();
    return;
  }

  // ── MODE 3: Grøn – WiFi-mode ───────────────────────────────────────────────
  // Forsøger at forbinde til konfigureret WiFi (10 sek).
  // Lykkes → normal drift med admin og ESP-NOW på router-kanalen.
  // Fejler → starter AP med Gamebox_SSID/Gamebox_pass på konfigureret kanal + ESP-NOW.
  _gb_webServerNeeded = true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(storedSSID.c_str(), storedPass.c_str());
  Serial.printf("[WiFi] WiFi-MODE: forbinder til '%s'", storedSSID.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print('.');
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setSleep(false);
    _setWifiLed(true);
    Serial.printf("\n[WiFi] Forbundet! IP: %s  kanal: %d\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel());
  } else {
    // Fallback: Gamebox_SSID AP på konfigureret kanal
    Serial.printf("\n[WiFi] Fejl – starter AP '%s' kanal %d\n", AP_SSID.c_str(), storedCh);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID.c_str(), AP_pass.c_str(), storedCh);
    WiFi.disconnect(false);
    _gb_isAPMode = true;
    _setWifiLed(true);
    Serial.printf("[WiFi] AP IP: %s  (forbind til '%s' for admin)\n",
                  WiFi.softAPIP().toString().c_str(), AP_SSID.c_str());
  }

  _initEspNow();
}

// Ny funktion til at håndtere alle web-ruter
void initWebServer(AsyncWebServer &server) {
  
  // Route for root / index.html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");

  // Route til at hente nuværende config
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", jsonConfig);
  });

  // Route til at modtage ny config (POST)
  // BEMÆRK: Body-callback kaldes én gang per TCP-chunk. En stor JSON (>1460 bytes)
  // ankommer i flere chunks. Vi akkumulerer i en statisk String og parser først
  // når hele body'en er modtaget (index + len == total).
  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      static String _postBody;
      if (index == 0) _postBody = "";          // Ny request – nulstil buffer
      _postBody.concat((const char*)data, len); // Tilføj denne chunk

      if (index + len < total) return;         // Vent – der kommer mere data

      // Hele body'en er modtaget – parse JSON
      JsonDocument doc;
      if (deserializeJson(doc, _postBody)) {
          Serial.printf("[POST] JSON-parse fejl (body=%d bytes)\n", _postBody.length());
          request->send(400, "application/json", "{\"error\":\"Ugyldig JSON\"}");
          _postBody = "";
          return;
      }
      serializeJson(doc, Serial);
      preferences.begin("gamebox", false);
      for (const char* key : intKeys) { 
    if (doc.containsKey(key)) {
        int val = doc[key].as<int>(); // Tving JSON til at læse som int
        preferences.putInt(key, val);
        Serial.printf("Gemte INT: %s = %d\n", key, val);
    }
}

// 2. Håndter Bytes eksplicit
for (const char* key : byteKeys) { 
    if (doc.containsKey(key)) {
        uint8_t val = doc[key].as<uint8_t>();
        preferences.putUChar(key, val);
    }
}
      // --- Håndter Strings ---
    for (const char* key : stringKeys) {
        if (doc.containsKey(key)) {
            const char* val = doc[key].as<const char*>();
            preferences.putString(key, val);
            Serial.printf("Gemte STRING: %s = %s\n", key, val);
        }
    }
    // Hvis joystick-MACs er opdateret, anvend dem med det samme (ingen genstart nødvendig)
    if (doc.containsKey("Green_mac")) {
      setJoystickMACFromString(JOY_A, doc["Green_mac"].as<const char*>());
      refreshEspNowPeer(JOY_A);
    }
    if (doc.containsKey("Red_mac")) {
      setJoystickMACFromString(JOY_B, doc["Red_mac"].as<const char*>());
      refreshEspNowPeer(JOY_B);
    }
    if (doc.containsKey("Remote_mac")) {
      setRemoteMACFromString(doc["Remote_mac"].as<const char*>());
    }
            // --- Håndter Floats ---
    for (const char* key : floatKeys) {
        if (doc.containsKey(key)) {
            // Vi læser værdien direkte som en float
            float val = doc[key].as<float>(); 
            
            // Vi gemmer den som en float (ingen konvertering til char* nødvendig!)
            preferences.putFloat(key, val);
            
            Serial.printf("Gemte FLOAT: %s = %.2f\n", key, val);
        }
    }
    
      preferences.end();

      // Anvend brightness-ændringer med det samme på LED-strippen
      if (doc.containsKey("LEDBRIGHTNESS")) {
          LEDBRIGHTNESS = doc["LEDBRIGHTNESS"].as<uint8_t>();
          FastLED.setBrightness(LEDBRIGHTNESS);
          FastLED.show();
      }
      if (doc.containsKey("BackBrightness")) {
          BackBrightness = doc["BackBrightness"].as<uint8_t>();
      }

      // Opdater den globale jsonConfig streng
      JsonDocument freshConfig = getCurrentConfig();
      serializeJson(freshConfig, jsonConfig);

      // Genparser spil-liste hvis den er ændret
      if (doc.containsKey("game_list")) {
        extern void parseGameList();
        parseGameList();
      }

      JsonDocument responseDoc;
      responseDoc["status"] = "OK";
      responseDoc["config"] = freshConfig;
      String responseString;
      serializeJson(responseDoc, responseString);
      _postBody = "";   // Frigiv hukommelse
      // Nulstil dvale-timer – bruger er aktiv via web-interface
      extern unsigned long lastInputTime;
      lastInputTime = millis();
      request->send(200, "application/json", responseString);
  });

  // WebSocket på /ws
  _gbws.onEvent(_onWsEvent);
  server.addHandler(&_gbws);

  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("Web server started");
}

#endif
