#ifndef ESPNOW_LEDS_H
#define ESPNOW_LEDS_H

// =============================================================================
//  espnow_leds.h  –  ESP-NOW trådløs LED-styring til joystiks
//  Gamebox, Futuretown
//
//  Pakke-skelnen på størrelse (joystik-FW):
//    2 bytes        = lyd-kommando   (soundId, volume)
//    3 bytes        = knap-event     (id, button, pressed)
//    4 bytes        = LED enkelt     (ledId, r, g, b)
//    21 bytes       = ping           (id, name[20])
//    49 bytes       = WiFi-status    (ip[16], ssid[33])
//    5-61 bytes*    = LED batch      (count, {id,r,g,b}×count)   ← NY
//                    *size = 1 + count×4, count = 1..15
//
//  LED IDs (standard 4-knap joystik):
//    0 = Blå knap    4 = Grøn knap    6 = Gul knap    8 = Rød knap
//    1 = Point 1     2 = Point 2      3 = Point 3     255 = Alle LEDs
// =============================================================================

#include <Arduino.h>
#include <esp_now.h>
#include <FastLED.h>

// ─── LED ID konstanter ──────────────────────────────────────────────────────
#define JOY_LED_BLUE    0   // Blå knap-LED
#define JOY_LED_PT1     1   // Point LED 1
#define JOY_LED_PT2     2   // Point LED 2
#define JOY_LED_PT3     3   // Point LED 3
#define JOY_LED_GREEN   4   // Grøn knap-LED
#define JOY_LED_YELLOW  6   // Gul knap-LED
#define JOY_LED_RED     8   // Rød knap-LED
#define JOY_LED_ALL   255   // Alle LEDs på joystikket

// Knap-indeks (0=Rød,1=Gul,2=Grøn,3=Blå) → LED ID  (matches JOYSTICK_LED_MAP indirekte)
static const byte _joyBtnToLed[4] = { JOY_LED_RED, JOY_LED_YELLOW, JOY_LED_GREEN, JOY_LED_BLUE };

// Knap-farver (Rød, Gul, Grøn, Blå – samme rækkefølge som joyBtnPressed[][])
static const CRGB _joyBtnColors[4] = { CRGB::Red, CRGB::Yellow, CRGB::Green, CRGB::Blue };

// ─── Kommando-structs ────────────────────────────────────────────────────────

// 4-byte enkelt LED
struct EspNowLedCmd {
  byte ledId;
  byte r, g, b;
};

// Batch LED – 1 + count×4 bytes (count = 1..15)
struct EspNowLedEntry { byte id, r, g, b; };
struct EspNowLedBatch {
  byte          count;      // Antal LED-entries (1–15)
  EspNowLedEntry leds[15];  // id+r+g+b per LED
};

// ─── Intern tilstand (undgår redundante sends) ───────────────────────────────
// Sporet tilstand per joystik per LED-position.
// sendLed() sammenligner med dette og sender KUN ved ændring.
static CRGB _joyLedState[2][16];

// ─── Intern send-funktion ────────────────────────────────────────────────────
static void _sendLedPacket(byte joyId, byte ledId, byte r, byte g, byte b) {
  if (!_espnowSoundReady) return;  // ESP-NOW ikke initialiseret
  if (joyId >= 2) return;
  EspNowLedCmd cmd = { ledId, r, g, b };
  const uint8_t* target = _espnow_macIsZero(_espnow_joyMAC[joyId])
                          ? _espnow_broadcastMAC
                          : _espnow_joyMAC[joyId];
  esp_now_send(target, (uint8_t*)&cmd, sizeof(cmd));
}

// ─── Public API ──────────────────────────────────────────────────────────────

/**
 * Sæt én LED på ét joystik – sender KUN hvis farven faktisk ændrer sig.
 * Brug dette i spil-loop for at undgå at oversvømme ESP-NOW-kanalen.
 */
void sendLed(byte joyId, byte ledId, CRGB color) {
  if (joyId >= 2) return;

  if (ledId == JOY_LED_ALL) {
    // Tjek om nogen LED faktisk ændrer sig
    bool any = false;
    for (int i = 0; i < 16; i++) if (_joyLedState[joyId][i] != color) { any = true; break; }
    if (!any) return;
    for (int i = 0; i < 16; i++) _joyLedState[joyId][i] = color;
    _sendLedPacket(joyId, JOY_LED_ALL, color.r, color.g, color.b);
    return;
  }

  if (ledId > 15) return;
  if (_joyLedState[joyId][ledId] == color) return;  // Ingen ændring – spring over
  _joyLedState[joyId][ledId] = color;
  _sendLedPacket(joyId, ledId, color.r, color.g, color.b);
}

/**
 * Sæt én LED på BEGGE joystiks (state-tracked).
 */
void sendLedBoth(byte ledId, CRGB color) {
  sendLed(JOY_A, ledId, color);
  sendLed(JOY_B, ledId, color);
}

/**
 * Tving-send LED uden tilstandstjek.
 * Bruges til blink (on→off→on) og eksplicitte nulstilsignaler.
 */
void forceSendLed(byte joyId, byte ledId, CRGB color) {
  if (joyId >= 2) return;
  if (ledId == JOY_LED_ALL) {
    for (int i = 0; i < 16; i++) _joyLedState[joyId][i] = color;
  } else if (ledId <= 15) {
    _joyLedState[joyId][ledId] = color;
  }
  _sendLedPacket(joyId, ledId, color.r, color.g, color.b);
}

/**
 * Sluk alle LEDs på ét joystik (tvungen).
 */
void clearJoyLeds(byte joyId) {
  forceSendLed(joyId, JOY_LED_ALL, CRGB::Black);
}

/**
 * Sluk alle LEDs på begge joystiks (tvungen).
 */
void clearAllJoyLeds() {
  clearJoyLeds(JOY_A);
  clearJoyLeds(JOY_B);
}

// ─── Batch-send ──────────────────────────────────────────────────────────────

/** Intern: send batch uden state-tjek */
static void _sendLedBatch(byte joyId, const EspNowLedBatch& b) {
  if (!_espnowSoundReady || joyId >= 2 || b.count == 0 || b.count > 15) return;
  const uint8_t* target = _espnow_macIsZero(_espnow_joyMAC[joyId])
                          ? _espnow_broadcastMAC
                          : _espnow_joyMAC[joyId];
  esp_now_send(target, (const uint8_t*)&b, 1 + (size_t)b.count * 4);
}

/**
 * Send LED batch – state-tracked (springer over hvis intet ændrer sig).
 * Alle entries i batch sendes i ÉN pakke → ingen buffer-overflow.
 */
void sendLedBatch(byte joyId, const EspNowLedBatch& b) {
  if (joyId >= 2 || b.count == 0) return;
  bool changed = false;
  for (int i = 0; i < b.count && !changed; i++) {
    byte id = b.leds[i].id;
    if (id < 16 && _joyLedState[joyId][id] != CRGB(b.leds[i].r, b.leds[i].g, b.leds[i].b))
      changed = true;
  }
  if (!changed) return;
  for (int i = 0; i < b.count; i++) {
    byte id = b.leds[i].id;
    if (id < 16) _joyLedState[joyId][id] = CRGB(b.leds[i].r, b.leds[i].g, b.leds[i].b);
  }
  _sendLedBatch(joyId, b);
}

/**
 * Force-send LED batch – sender altid, opdaterer state-cache.
 */
void forceSendLedBatch(byte joyId, const EspNowLedBatch& b) {
  if (joyId >= 2 || b.count == 0) return;
  for (int i = 0; i < b.count; i++) {
    byte id = b.leds[i].id;
    if (id < 16) _joyLedState[joyId][id] = CRGB(b.leds[i].r, b.leds[i].g, b.leds[i].b);
  }
  _sendLedBatch(joyId, b);
}

// ─── Hjælper: byg score-batch ─────────────────────────────────────────────
static inline EspNowLedBatch _makeScoreBatch(int score, CRGB color) {
  score = constrain(score, 0, 3);
  EspNowLedBatch b;
  b.count = 3;
  b.leds[0] = {JOY_LED_PT1, score>=1?color.r:(byte)0, score>=1?color.g:(byte)0, score>=1?color.b:(byte)0};
  b.leds[1] = {JOY_LED_PT2, score>=2?color.r:(byte)0, score>=2?color.g:(byte)0, score>=2?color.b:(byte)0};
  b.leds[2] = {JOY_LED_PT3, score>=3?color.r:(byte)0, score>=3?color.g:(byte)0, score>=3?color.b:(byte)0};
  return b;
}

/**
 * Sæt score-LEDs (ID 1-3) via batch – 1 pakke i stedet for 3.
 * State-tracked.
 */
void setScoreLeds(byte joyId, int score, CRGB color) {
  sendLedBatch(joyId, _makeScoreBatch(score, color));
}

/**
 * Force-send score-LEDs – 1 pakke, altid sendt.
 * Bruges efter clearAllJoyLeds() så state-cachen nulstilles korrekt.
 */
void forceSetScoreLeds(byte joyId, int score, CRGB color) {
  forceSendLedBatch(joyId, _makeScoreBatch(score, color));
}

/**
 * Sæt knap-LED via knap-indeks (0=Rød, 1=Gul, 2=Grøn, 3=Blå).
 * State-tracked.
 */
void setButtonLed(byte joyId, byte btnIdx, CRGB color) {
  if (btnIdx >= 4) return;
  sendLed(joyId, _joyBtnToLed[btnIdx], color);
}

#endif // ESPNOW_LEDS_H
