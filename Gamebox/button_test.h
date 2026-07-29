#ifndef BUTTON_TEST_H
#define BUTTON_TEST_H

// =============================================================================
//  button_test.h  –  Joystik-test / hardware-verificering
//  Futuretown  –  Gamebox
//
//  Knap-adfærd:
//    Grøn knap (idx 2) → afspil HitTune på det joystik der trykkes
//    Rød  knap (idx 0) → afspil MissTune på det joystik der trykkes
//    Gul/Blå           → ingen lyd
//
//  Vinder-betingelse:
//    Alle 4 knapper holdt nede på ét joystik → det joystik vinder
//    Spillet afsluttes med WinnerTune (vinder) + GameSlut (begge) + vinderanimation
//
//  Visuelt:
//    Joystik-LEDs lyser i knappens farve mens trykket holdes
//    Skud-animation på banen ved hvert knapnedtryk
// =============================================================================

#include <FastLED.h>
#include "hw_utils.h"
#include "GameManager.h"

extern GameManager gameManager;
extern int FIRST_SCREEN_LED;
extern int SCREEN_LEDS;

// Lyd-preferences (fra config_manager.h)
extern byte HitTune_id,    HitTune_vol;
extern byte MissTune_id,   MissTune_vol;
extern byte WinnerTune_id, WinnerTune_vol;
extern byte GameSlut_id,   GameSlut_vol;
extern byte JoystickTypeA;  // Grønt joystik: 0=lille/4-knapper/lyd  1=stor/11-knapper/ingen lyd
extern byte JoystickTypeB;  // Rødt joystik

// button_manager (inkluderes FØR denne fil)
extern volatile bool joyBtnPressed[2][11];
extern volatile bool joyAllBtnsSignal[2];  // true = btn-255 modtaget (alle 4 trykket)
extern bool allFourPressed(byte joyId);

// ESP-NOW lyd
void sendSoundOrBroadcast(byte joystickId, byte soundId, byte volume);
void sendSoundBoth(byte soundId, byte volume);

// ─── LILLE JOYSTIK (JoystickType=0): 4 knapper med lyd ───────────────────────
//  Rød(idx 0)→LED 8  Gul(1)→LED 6  Grøn(2)→LED 4  Blå(3)→LED 0
static const byte _bt_smLedOffset[4] = { 8, 6, 4, 0 };
static const CRGB _bt_smBtnColor[4]  = {
    CRGB::Red, CRGB(255,100,0), CRGB::Green, CRGB::Blue
};

// ─── STORT JOYSTIK (JoystickType=1): 11 knapper, ingen lyd ───────────────────
//  [(btn_idx, LED_ID)]: (0,0)(1,1)(2,2)(3,3)(4,4)(5,6)(6,7)(7,8)(8,10)(9,11)(10,12)
//  Blå 1-2  ·  Hvid 3  ·  Grøn 4-5  ·  Hvid 6  ·  Orange 7-8  ·  Hvid 9  ·  Rød 10-11
static const byte _bt_lgLedOffset[11] = { 0, 1, 2, 3, 4, 6, 7, 8, 10, 11, 12 };
static const CRGB _bt_lgBtnColor[11]  = {
    CRGB::Blue,        CRGB::Blue,           // Knap 1-2  – Blå
    CRGB::White,                             // Knap 3    – Hvid
    CRGB::Green,       CRGB::Green,          // Knap 4-5  – Grøn
    CRGB::White,                             // Knap 6    – Hvid
    CRGB(255,100,0),   CRGB(255,100,0),      // Knap 7-8  – Orange
    CRGB::White,                             // Knap 9    – Hvid
    CRGB::Red,         CRGB::Red,            // Knap 10-11 – Rød
};

// Skud-animation (bevæger sig på banen)
static int  _bt_skudPosA  = -1;
static int  _bt_skudPosB  = -1;
static CRGB _bt_skudFarveA = CRGB::Black;
static CRGB _bt_skudFarveB = CRGB::Black;

// ─────────────────────────────────────────────────────────────────────────────
//  runButtonTest()  –  Kald hvert frame fra GameManager (STATE_PLAYING)
// ─────────────────────────────────────────────────────────────────────────────
void runButtonTest(CRGB* leds) {
  // Per-joystik type (A=grøn, B=rød)
  const bool  isLargeA   = (JoystickTypeA == 1);
  const bool  isLargeB   = (JoystickTypeB == 1);
  const int   numBtnsA   = isLargeA ? 11 : 4;
  const int   numBtnsB   = isLargeB ? 11 : 4;
  const byte* ledOffsetsA= isLargeA ? _bt_lgLedOffset : _bt_smLedOffset;
  const byte* ledOffsetsB= isLargeB ? _bt_lgLedOffset : _bt_smLedOffset;
  const CRGB* btnColorsA = isLargeA ? _bt_lgBtnColor  : _bt_smBtnColor;
  const CRGB* btnColorsB = isLargeB ? _bt_lgBtnColor  : _bt_smBtnColor;

  static bool prevState[2][11] = {};   // Forrige frames knaptilstand (maks 11)

  // ── 1. ALLE 4 KNAPPER – HVID BOLD (lokal detektion, ingen joystik-afhængighed)
  //  Rising edge på allFourPressed(): suprimér individuelle tryk i 100ms
  static bool     prevAllFour[2]  = {};
  static unsigned long ignoreUntil[2] = {};
  unsigned long now = millis();

  for (byte joy = 0; joy < 2; joy++) {
    bool allFour = allFourPressed(joy);
    if (allFour && !prevAllFour[joy]) {
      // Ny "alle-4"-combo: hvid bold og ignorer individuelle tryk i 100ms
      ignoreUntil[joy] = now + 100;
      if (joy == 0) { _bt_skudPosA = 0;             _bt_skudFarveA = CRGB::White; }
      else          { _bt_skudPosB = SCREEN_LEDS-1;  _bt_skudFarveB = CRGB::White; }
    }
    prevAllFour[joy] = allFour;
    // Ryd også btn-255 signalet fra firmware (bruges ikke selvstændigt her)
    joyAllBtnsSignal[joy] = false;
  }

  // ── 2. EDGE-DETEKTERING + LYD (per joystik) ───────────────────────────────
  auto processJoy = [&](byte joy, int numBtns, const CRGB* btnColors, bool isLarge) {
    for (int btn = 0; btn < numBtns; btn++) {
      bool cur  = joyBtnPressed[joy][btn];
      bool prev = prevState[joy][btn];
      if (cur && !prev) {
        if (now < ignoreUntil[joy]) { prevState[joy][btn] = cur; continue; }
        if (!isLarge) {
          if (btn == 2) { sendSoundOrBroadcast(joy, HitTune_id,  HitTune_vol);  delayMicroseconds(8000); }
          else if (btn == 0) { sendSoundOrBroadcast(joy, MissTune_id, MissTune_vol); delayMicroseconds(8000); }
        }
        CRGB sk = btnColors[btn]; sk.nscale8(180);
        if (joy == 0) { _bt_skudPosA = 0;             _bt_skudFarveA = sk; }
        else          { _bt_skudPosB = SCREEN_LEDS-1;  _bt_skudFarveB = sk; }
      }
      prevState[joy][btn] = cur;
    }
  };
  processJoy(0, numBtnsA, btnColorsA, isLargeA);
  processJoy(1, numBtnsB, btnColorsB, isLargeB);

  // ── 3. JOYSTIK-LEDs via ESP-NOW (batch-send, per joystik) ─────────────────
  {
    EspNowLedBatch bA; bA.count = numBtnsA;
    for (int i = 0; i < numBtnsA; i++) {
      CRGB c = joyBtnPressed[0][i] ? btnColorsA[i] : CRGB::Black;
      bA.leds[i] = { ledOffsetsA[i], c.r, c.g, c.b };
    }
    sendLedBatch(JOY_A, bA);
  }
  {
    EspNowLedBatch bB; bB.count = numBtnsB;
    for (int i = 0; i < numBtnsB; i++) {
      CRGB c = joyBtnPressed[1][i] ? btnColorsB[i] : CRGB::Black;
      bB.leds[i] = { ledOffsetsB[i], c.r, c.g, c.b };
    }
    sendLedBatch(JOY_B, bB);
  }

  // ── 4. BANE-ANIMATION ──────────────────────────────────────────────────────
  fadeToBlackBy(leds + FIRST_SCREEN_LED, SCREEN_LEDS, 80);

  if (_bt_skudPosA >= 0 && _bt_skudPosA < SCREEN_LEDS) {
    leds[FIRST_SCREEN_LED + _bt_skudPosA] = _bt_skudFarveA;
    _bt_skudPosA += 3;
  } else {
    _bt_skudPosA = -1;
  }

  if (_bt_skudPosB >= 0 && _bt_skudPosB < SCREEN_LEDS) {
    leds[FIRST_SCREEN_LED + _bt_skudPosB] = _bt_skudFarveB;
    _bt_skudPosB -= 3;
  } else {
    _bt_skudPosB = -1;
  }

  showLeds();
}

#endif // BUTTON_TEST_H
