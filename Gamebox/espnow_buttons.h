#ifndef ESPNOW_BUTTONS_H
#define ESPNOW_BUTTONS_H

// =============================================================================
//  espnow_buttons.h  –  ESP-NOW trådløs knap-modtager
//  Gamebox, Futuretown
//
//  Modtager button-push / button-release fra joystick-ESPs via ESP-NOW og
//  opdaterer en virtuel pin-state tabel.
//
//  Brug joyRead(pin) som erstatning for digitalRead(pin) overalt i spilkoden.
//  Ingen ændring nødvendig i spillogik – kun et find/erstat:
//    digitalRead(  →  joyRead(
//
//  ARKITEKTUR (to-core):
//    Core 0 (WiFi-task): _espnow_onBtnReceive() – kun pin-state + kø-push, ingen I/O
//    Core 1 (main loop): processEspNowQueue()   – kalder callbacks, WebSocket, Serial
//
//  Kald i setup() efter initWiFi():
//    esp_wifi_set_promiscuous(true) sættes i wifi_manager.h – NØDVENDIGT i STA-tilstand
//
//  VIGTIGT: Kald IKKE WiFi.mode(WIFI_OFF) – brug kun WiFi.disconnect().
// =============================================================================

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// -----------------------------------------------------------------------------
//  MAC-adresser for de to joysticks.
// -----------------------------------------------------------------------------
static uint8_t _espnow_joyMAC[2][6] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  // Joystick A (Team A / Grønt)
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  // Joystick B (Team B / Rødt)
};

#define JOY_A 0
#define JOY_B 1

// -----------------------------------------------------------------------------
//  Event-callback hooks (kaldes fra processEspNowQueue() på Core 1 – trådsikkert)
//
//  BtnEventCb – Tolket knapskift: (joystickId, mac[6], buttonId, isPressed)
//  RawEventCb – Alle rå ESP-NOW-pakker til diagnostik: (mac[6], data, len)
// -----------------------------------------------------------------------------
typedef void (*BtnEventCb)(byte joystickId, const uint8_t* mac, byte buttonId, bool isPressed);
typedef void (*RawEventCb)(const uint8_t* mac, const uint8_t* data, int len, bool isBroadcast);
typedef void (*PingEventCb)(byte joyId, const uint8_t* mac, byte pingId, const char* name);
typedef void (*WifiStatusCb)(byte joyId, const uint8_t* mac, const char* ip, const char* ssid);
typedef void (*RemoteCmdCb)(byte cmd, byte param1, byte param2, const uint8_t* mac);

static BtnEventCb   _btnEventCb    = nullptr;
static RawEventCb   _rawEventCb    = nullptr;
static PingEventCb  _pingEventCb   = nullptr;
static WifiStatusCb _wifiStatusCb  = nullptr;
static RemoteCmdCb  _remoteCmdCb   = nullptr;

inline void setButtonEventCallback(BtnEventCb cb)  { _btnEventCb   = cb; }
inline void setRawEventCallback(RawEventCb cb)      { _rawEventCb   = cb; }
inline void setPingEventCallback(PingEventCb cb)    { _pingEventCb  = cb; }
inline void setWifiStatusCallback(WifiStatusCb cb)  { _wifiStatusCb = cb; }
inline void setRemoteCmdCallback(RemoteCmdCb cb)    { _remoteCmdCb  = cb; }

// -----------------------------------------------------------------------------
//  FreeRTOS event-kø  (Core 0 skriver, Core 1 læser)
// -----------------------------------------------------------------------------
struct _BtnEvt {
  bool    isRaw;        // true = rå diagnostik-pakke
  bool    isPing;       // true = ping-pakke fra joystik (21 bytes)
  bool    isWifiStatus; // true = WiFi-status svar på lyd-ID 250 (49 bytes)
  bool    isRemoteCmd;  // true = remote-kommando (8 bytes)
  bool    isBroadcast;
  uint8_t mac[6];
  // Knapskift:
  byte    joy;
  byte    buttonId;
  bool    isPressed;
  // Rå pakke + ping + wifi-status deler denne buffer
  // (max-størrelse: 49 bytes til EspNowWifiStatus)
  int     rawLen;
  uint8_t rawData[49];
};

static QueueHandle_t _btnEvtQueue = nullptr;

// Opret køen – kald FØR esp_now_register_recv_cb
inline void initEspNowQueue() {
  if (_btnEvtQueue == nullptr) {
    _btnEvtQueue = xQueueCreate(32, sizeof(_BtnEvt));
  }
}

// -----------------------------------------------------------------------------
//  Kommando-structs
//
//  LYD (2 bytes):    { byte soundId; byte volume }   – sendes fra Gamebox til joystik
//  KNAP (3 bytes):   { byte id; byte button; bool pressed }  – ny joystik-firmware
//
//  Knap-mapping:  button 1=Rød, 2=Gul, 3=Grøn, 4=Blå
//  (Erstatter det gamle 12-byte extended format)
// -----------------------------------------------------------------------------

// 2-byte lyd-format – genkendes KUN på størrelse, ignoreres som knap-event
struct EspNowSndCkeck {
  byte soundId;
  byte volume;
};

// 3-byte knap-format – ny joystik-firmware
struct EspNowBtnCmd3 {
  byte id;       // Joystik-ID (0 eller 1 – verificeres via MAC)
  byte button;   // Knap-nummer: 1=Rød, 2=Gul, 3=Grøn, 4=Blå; 255=alle 4 trykket
  bool pressed;  // true = trykket, false = sluppet
};

// 21-byte ping-pakke – joystik sender ved opstart for at se om gamebox er online
struct EspNowPingCmd {
  byte id;       // Joystik-ID fra config
  char name[20]; // Joystik-navn (null-termineret)
};

// 49-byte WiFi-status – joystik sender tilbage som svar på lyd-ID 250
struct EspNowWifiStatus {
  char ip[16];   // "192.168.1.42" eller "192.168.4.1"
  char ssid[33]; // WiFi-navn eller "JoystickSetup"
};

// -----------------------------------------------------------------------------
//  Virtuel pin-state tabel
// -----------------------------------------------------------------------------
#define _ESPNOW_MAX_PIN 40
static volatile bool _joyPinPressed[_ESPNOW_MAX_PIN] = {};

#define _ESPNOW_BTN_MAP_SIZE 11  // Joystick sender knap 1–11 (99 = joystick-akse, håndteres separat)

extern int pinsTeamA[];
extern int pinsTeamB[];

// -----------------------------------------------------------------------------
//  MAC-hjælpefunktioner
// -----------------------------------------------------------------------------
inline bool parseMacString(const char* str, uint8_t* mac) {
  int v[6] = {};
  if (sscanf(str, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)v[i];
  return true;
}

inline void setJoystickMACFromString(byte joystickId, const char* macStr) {
  if (joystickId >= 2) return;
  if (!parseMacString(macStr, _espnow_joyMAC[joystickId])) {
    Serial.printf("[ESP-NOW] Ugyldig MAC for joystick %d: %s\n", joystickId, macStr);
  }
}

inline void setJoystickMAC(byte joystickId, const uint8_t* mac) {
  if (joystickId >= 2) return;
  memcpy(_espnow_joyMAC[joystickId], mac, 6);
}

static inline bool _espnow_macIsZero(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) if (mac[i] != 0) return false;
  return true;
}

// -----------------------------------------------------------------------------
//  Auto-discovery
// -----------------------------------------------------------------------------
static uint8_t _espnow_autoMAC[2][6] = {{0},{0}};
static int     _espnow_autoCount = 0;

static int _espnow_getJoyIdx(const uint8_t* mac) {
  for (int j = 0; j < 2; j++) {
    if (_espnow_macIsZero(_espnow_joyMAC[j])) continue;
    bool match = true;
    for (int i = 0; i < 6; i++) {
      if (mac[i] != _espnow_joyMAC[j][i]) { match = false; break; }
    }
    if (match) return j;
  }
  if (_espnow_macIsZero(_espnow_joyMAC[0]) && _espnow_macIsZero(_espnow_joyMAC[1])) {
    for (int j = 0; j < _espnow_autoCount; j++) {
      bool match = true;
      for (int i = 0; i < 6; i++) {
        if (mac[i] != _espnow_autoMAC[j][i]) { match = false; break; }
      }
      if (match) return j;
    }
    if (_espnow_autoCount < 2) {
      memcpy(_espnow_autoMAC[_espnow_autoCount], mac, 6);
      return _espnow_autoCount++;
    }
  }
  return -1;
}

// -----------------------------------------------------------------------------
//  ESP-NOW receive-callback  (kører på Core 0 / WiFi-task)
//
//  REGLER FOR DENNE FUNKTION:
//   ✓ Opdater _joyPinPressed (volatile, trådsikkert)
//   ✓ Push til kø med xQueueSend (trådsikkert)
//   ✗ INGEN Serial.print – Serial bruger mutex, risiko for deadlock med Core 1
//   ✗ INGEN WebSocket/HTTP-kald – AsyncWebServer er ikke re-entrant fra Core 0
//   ✗ INGEN malloc/new – heap-allokering er ikke garanteret trådsikker her
// -----------------------------------------------------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void _espnow_onBtnReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* mac  = info->src_addr;
  const uint8_t* dest = info->des_addr;
  bool isBroadcast = (dest[0]==0xFF && dest[1]==0xFF && dest[2]==0xFF &&
                      dest[3]==0xFF && dest[4]==0xFF && dest[5]==0xFF);
#else
static void _espnow_onBtnReceive(const uint8_t* mac, const uint8_t* data, int len) {
  bool isBroadcast = false;  // Ikke tilgængeligt i ældre API
#endif

  if (!_btnEvtQueue) return;

  // --- Kø rå diagnostik-pakke (Core 1 printer den til Serial + WebSocket) ---
  _BtnEvt rawEvt = {};
  rawEvt.isRaw      = true;
  rawEvt.isBroadcast = isBroadcast;
  memcpy(rawEvt.mac, mac, 6);
  rawEvt.rawLen = len;
  int copyLen = len < 12 ? len : 12;
  memcpy(rawEvt.rawData, data, copyLen);
  xQueueSend(_btnEvtQueue, &rawEvt, 0);  // 0 = non-blocking

  // --- Forsøg at tolke pakken ---
  byte parsedButtonId  = 0;
  bool parsedIsPressed = false;
  bool parsed          = false;

  if (len == 2) {
    // 2 bytes = lyd-kommando – ignorér som knap-event
    return;
  } else if (len == sizeof(EspNowBtnCmd3)) {
    // 3 bytes = knap-format: {id, button, pressed}
    EspNowBtnCmd3 cmd;
    memcpy(&cmd, data, sizeof(cmd));
    parsedButtonId  = cmd.button;
    parsedIsPressed = cmd.pressed;
    parsed = true;
  } else if (len == sizeof(EspNowPingCmd)) {
    // 21 bytes = ping fra joystik ved opstart
    _BtnEvt pingEvt = {};
    pingEvt.isPing     = true;
    pingEvt.isBroadcast = isBroadcast;
    memcpy(pingEvt.mac, mac, 6);
    pingEvt.joy    = (byte)_espnow_getJoyIdx(mac);
    pingEvt.rawLen = len;
    memcpy(pingEvt.rawData, data, len);
    xQueueSend(_btnEvtQueue, &pingEvt, 0);
    return;
  } else if (len == sizeof(EspNowWifiStatus)) {
    // 49 bytes = WiFi-status svar fra joystik (svar på lyd-ID 250)
    _BtnEvt wifiEvt = {};
    wifiEvt.isWifiStatus = true;
    wifiEvt.isBroadcast  = isBroadcast;
    memcpy(wifiEvt.mac, mac, 6);
    wifiEvt.joy    = (byte)_espnow_getJoyIdx(mac);
    wifiEvt.rawLen = len;
    memcpy(wifiEvt.rawData, data, len);
    xQueueSend(_btnEvtQueue, &wifiEvt, 0);
    return;
  } else if (len == 8) {
    // 8 bytes = remote-kommando {marker=0x52, cmd, param1, param2, reserved[4]}
    if (data[0] == 0x52) {
      _BtnEvt remEvt = {};
      remEvt.isRemoteCmd = true;
      remEvt.isBroadcast = isBroadcast;
      memcpy(remEvt.mac, mac, 6);
      remEvt.rawLen = len;
      memcpy(remEvt.rawData, data, len);
      xQueueSend(_btnEvtQueue, &remEvt, 0);
    }
    return;
  }

  if (!parsed) return;
  // Tillad knap 1–11 og 255 (alle knapper) – afvis alt andet
  if (parsedButtonId < 1 || (parsedButtonId > 11 && parsedButtonId != 255)) return;

  int joy = _espnow_getJoyIdx(mac);
  // joy < 0 = ukendt afsender → send stadig event med joy=255 til browser-log
  byte joyByte = (joy >= 0) ? (byte)joy : 255;

  // Opdater virtuel pin-state for kendte joysticks – KUN for knap 1–4
  // (Knapper 5–11 tilgås via joyBtnPressed[joy][idx] i button_manager.h)
  if (joy >= 0 && parsedButtonId <= 4) {
    int* pins   = (joy == JOY_A) ? pinsTeamA : pinsTeamB;
    int  pinIdx = 4 - parsedButtonId;          // 1→3, 2→2, 3→1, 4→0
    int  pin    = pins[pinIdx];
    if (pin >= 0 && pin < _ESPNOW_MAX_PIN) {
      _joyPinPressed[pin] = parsedIsPressed;
    }
  }

  // Ko event til Core 1 (joy=255 = ukendt – vises i log med MAC til copy/paste)
  _BtnEvt evt = {};
  evt.isRaw      = false;
  evt.isBroadcast = isBroadcast;
  evt.joy        = joyByte;
  evt.buttonId   = parsedButtonId;
  evt.isPressed  = parsedIsPressed;
  memcpy(evt.mac, mac, 6);
  xQueueSend(_btnEvtQueue, &evt, 0);
}

// -----------------------------------------------------------------------------
//  processEspNowQueue()  –  Kald fra main loop (Core 1)
//
//  Tømmer køen og kalder callbacks (Serial, WebSocket, game-logic) sikkert.
//  Kaldt af loopWebServer() i wifi_manager.h.
// -----------------------------------------------------------------------------
inline void processEspNowQueue() {
  if (!_btnEvtQueue) return;
  _BtnEvt evt;
  while (xQueueReceive(_btnEvtQueue, &evt, 0) == pdTRUE) {
    if (evt.isPing) {
      // Ping fra joystik ved opstart
      EspNowPingCmd* p = (EspNowPingCmd*)evt.rawData;
      p->name[19] = '\0';  // Sikr null-terminering
      Serial.printf("[PING] Joystik #%d '%s' er online  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    p->id, p->name,
                    evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5]);
      if (_pingEventCb) _pingEventCb(evt.joy, evt.mac, p->id, p->name);

    } else if (evt.isRemoteCmd) {
      // Remote-kommando (8 bytes)
      byte cmd = evt.rawData[1], p1 = evt.rawData[2], p2 = evt.rawData[3];
      Serial.printf("[REMOTE CMD] cmd=0x%02X p1=%d p2=%d  fra %02X:%02X:%02X:%02X:%02X:%02X\n",
                    cmd, p1, p2,
                    evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5]);
      if (_remoteCmdCb) _remoteCmdCb(cmd, p1, p2, evt.mac);

    } else if (evt.isWifiStatus) {
      // WiFi-status svar fra joystik (efter lyd-ID 250)
      EspNowWifiStatus* ws = (EspNowWifiStatus*)evt.rawData;
      ws->ip[15] = '\0'; ws->ssid[32] = '\0';
      Serial.printf("[WiFi Status] Joystik %d  IP: %s  SSID: %s\n",
                    evt.joy, ws->ip, ws->ssid);
      if (_wifiStatusCb) _wifiStatusCb(evt.joy, evt.mac, ws->ip, ws->ssid);

    } else if (evt.isRaw) {
      // Rå diagnostik-pakke
      Serial.printf("[ESP-NOW RX] %s  Fra %02X:%02X:%02X:%02X:%02X:%02X  len=%d  data=",
                    evt.isBroadcast ? "BROADCAST" : "UNICAST  ",
                    evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4], evt.mac[5],
                    evt.rawLen);
      for (int i = 0; i < evt.rawLen && i < 12; i++) Serial.printf("%02X ", evt.rawData[i]);
      Serial.println();
      if (_rawEventCb) _rawEventCb(evt.mac, evt.rawData, evt.rawLen, evt.isBroadcast);

    } else {
      // Tolket knapskift – knap 1-11 vises med navn/nummer
      static const char* joyNames[] = { "Grønt", "Rødt" };
      const char* joyName = (evt.joy < 2) ? joyNames[evt.joy] : "Ukendt";
      char btnBuf[8];
      const char* btnName;
      switch(evt.buttonId) {
        case 255: btnName = "Alle";   break;
        case 1:   btnName = "1/Rød";  break;
        case 2:   btnName = "2/Gul";  break;
        case 3:   btnName = "3/Grøn"; break;
        case 4:   btnName = "4/Blå";  break;
        default:
          if(evt.buttonId >= 5 && evt.buttonId <= 11) {
            snprintf(btnBuf, sizeof(btnBuf), "%d", evt.buttonId);
            btnName = btnBuf;
          } else { btnName = "?"; }
      }
      Serial.printf("[KNAP] %-6s knap %-7s %s\n",
                    joyName, btnName,
                    evt.isPressed ? "↓ NEDE   " : "↑ SLUPPET");
      if (_btnEventCb) _btnEventCb(evt.joy, evt.mac, evt.buttonId, evt.isPressed);
    }
  }
}

// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------

inline int joyRead(int pin) {
  if (pin >= 0 && pin < _ESPNOW_MAX_PIN && _joyPinPressed[pin]) return LOW;
  return HIGH;
}

inline void clearAllButtonStates() {
  memset((void*)_joyPinPressed, 0, sizeof(_joyPinPressed));
}

#endif // ESPNOW_BUTTONS_H
