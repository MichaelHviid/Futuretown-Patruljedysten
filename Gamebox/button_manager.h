#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

// =============================================================================
//  button_manager.h  –  Centraliseret knapstyring til Gamebox
//  Futuretown
//
//  Eksponerer:
//    joyBtnPressed[joy][btn]  –  aktuel knaptilstand fra ESP-NOW
//    joyReady[joy]            –  joystik har meldt klar (første tryk i lobby)
//    physRedPressed()         –  fysisk rød knap på Gamebox
//    physGreenPressed()       –  fysisk grøn knap på Gamebox
//    allFourPressed(joyId)    –  alle 4 knapper holdt nede på ét joystik
//    checkBootButtons()       –  kald tidligt i setup() – sætter _bm_forceAPMode
//    _bm_forceAPMode          –  true = start altid som "GameboxSetup" AP
//
//  Knap-mapping (joystik-firmware sender):
//    button 1 = Rød, 2 = Gul, 3 = Grøn, 4 = Blå, 5–11 = ekstra
//  Array-indeks internt:
//    [0]=Rød, [1]=Gul, [2]=Grøn, [3]=Blå, [4..10]=ekstra (UltimateWAM)
//
//  VIGTIGT: Inkluder FØR wifi_manager.h i Gamebox.ino
// =============================================================================

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Joystik-knaptilstand  [joy 0/1][btn 0-10: Rød(0), Gul(1), Grøn(2), Blå(3), ekstra(4-10)]
//  Opdateres fra onJoyButtonEvent() (kaldes fra ESP-NOW Core 1 callback)
//  Knapper 0-3 afspejles også i _joyPinPressed via espnow_buttons.h
//  Knapper 4-10 er kun tilgængelige via joyBtnPressed (UltimateWAM m.fl.)
// ─────────────────────────────────────────────────────────────────────────────
volatile bool joyBtnPressed[2][11] = {};

// ─────────────────────────────────────────────────────────────────────────────
//  Klar-status – sættes til true ved første knapnedtryk i lobby
//  Nulstilles af clearJoyReady() (GameManager ved ny spilstart)
// ─────────────────────────────────────────────────────────────────────────────
volatile bool joyReady[2] = { false, false };
volatile bool joyAllBtnsSignal[2] = { false, false };

// Remote control flags (WebSocket + ESP-NOW remote → læses i Gamebox.ino loop)
volatile byte _remoteActionPending = 0; // 1=rød(skift), 2=grøn(start), 3=send_state
volatile byte _remoteCmdPending    = 0;
volatile byte _remoteParam1        = 0;
volatile byte _remoteParam2        = 0;

inline void clearJoyReady() {
  joyReady[0] = false;
  joyReady[1] = false;
}

inline bool bothJoysReady() {
  return joyReady[0] && joyReady[1];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Alle 4 knapper holdt nede på ét joystik?
//  Bruges som vinder-betingelse i spil
// ─────────────────────────────────────────────────────────────────────────────
inline bool allFourPressed(byte joyId) {
  if (joyId >= 2) return false;
  for (int i = 0; i < 4; i++) if (!joyBtnPressed[joyId][i]) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fysiske Gamebox-knapper (pins læses fra preferences i setup)
// ─────────────────────────────────────────────────────────────────────────────
static int _bm_redPin   = 4;
static int _bm_greenPin = 15;

inline void initButtonManager(int redPin, int greenPin) {
  _bm_redPin   = redPin;
  _bm_greenPin = greenPin;
  pinMode(redPin,   INPUT_PULLUP);
  pinMode(greenPin, INPUT_PULLUP);
}

inline bool physRedPressed()   { return digitalRead(_bm_redPin)   == LOW; }
inline bool physGreenPressed() { return digitalRead(_bm_greenPin) == LOW; }

// ─────────────────────────────────────────────────────────────────────────────
//  Boot-flag: tvungen AP-mode
//  Sættes hvis rød + grøn begge holdes nede ved opstart
//  Bruges i wifi_manager.h's initWiFi() til at starte "GameboxSetup" AP
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
//  Boot-flags – sættes af checkBootButtons() tidligt i setup()
//
//  _bm_forceAPMode  – Rød+Grøn holdt: "GameboxSetup" AP, ingen ESP-NOW
//  _bm_wifiMode     – Kun Grøn holdt: Forsøg WiFi → fallback Gamebox AP + ESP-NOW
//  Ingen knap       – Spil-mode: kun ESP-NOW på konfigureret kanal, ingen web-server
// ─────────────────────────────────────────────────────────────────────────────
bool _bm_forceAPMode = false;
bool _bm_wifiMode    = false;
bool _bm_redAtBoot   = false;   // Rød knap holdt nede ved opstart → hello-broadcast

inline bool checkBootButtons() {
  bool red   = physRedPressed();
  bool green = physGreenPressed();
  _bm_forceAPMode = red && green;
  _bm_wifiMode    = green && !red;
  _bm_redAtBoot   = red;
  if (_bm_forceAPMode) Serial.println("[BOOT] Rød+Grøn – config-mode: GameboxSetup AP (ingen ESP-NOW).");
  if (_bm_wifiMode)    Serial.println("[BOOT] Grøn – WiFi-mode: forsøger WiFi, fallback til Gamebox AP.");
  if (!red && !green)  Serial.println("[BOOT] Ingen knap – spil-mode: ESP-NOW kun, ingen WiFi.");
  return _bm_forceAPMode;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ESP-NOW callback – kædes ind fra wifi_manager.h's _initEspNow()
//
//  btnNum: 1=Rød, 2=Gul, 3=Grøn, 4=Blå
//  joyId:  0 = JOY_A, 1 = JOY_B, 255 = ukendt (ignoreres her)
// ─────────────────────────────────────────────────────────────────────────────
inline void onJoyButtonEvent(byte joyId, const uint8_t* /*mac*/, byte btnNum, bool pressed) {
  if (joyId >= 2) return;   // Ukendt joystik → ignorér

  if (btnNum == 255) {
    // Alle 4 knapper holdt nede simultant (firmware-signal)
    for (int i = 0; i < 4; i++) joyBtnPressed[joyId][i] = pressed;
    if (pressed) {
      joyReady[joyId] = true;
      joyAllBtnsSignal[joyId] = true;  // Puls-signal til spil-kode
    }
    return;
  }

  if (btnNum < 1 || btnNum > 11) return;   // Kun knap 1–11
  int idx = btnNum - 1;                     // 0=Rød, 1=Gul, 2=Grøn, 3=Blå, 4-10=ekstra
  joyBtnPressed[joyId][idx] = pressed;
  if (pressed) joyReady[joyId] = true;
}

#endif // BUTTON_MANAGER_H
