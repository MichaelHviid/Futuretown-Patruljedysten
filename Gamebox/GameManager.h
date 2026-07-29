#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <FastLED.h>
#include "hw_utils.h"

extern int scoreGreen;
extern int scoreRed;
extern int selected_game;

// Sound preferences (fra config_manager.h)
extern byte GameStart_id,   GameStart_vol;
extern byte GameSlut_id,    GameSlut_vol;
extern byte WinnerTune_id,  WinnerTune_vol;

// button_manager (inkluderes før denne fil)
extern volatile bool joyReady[2];
extern volatile bool joyBtnPressed[2][11];
extern bool physGreenPressed();
extern void clearJoyReady();
extern bool bothJoysReady();

// ESP-NOW lyd (inkluderes via wifi_manager.h)
void sendSoundBoth(byte soundId, byte volume);
void sendSoundOrBroadcast(byte joystickId, byte soundId, byte volume);

// ESP-NOW LEDs (inkluderes via wifi_manager.h → espnow_leds.h)
void sendLed(byte joyId, byte ledId, CRGB color);
void forceSendLed(byte joyId, byte ledId, CRGB color);
void clearAllJoyLeds();
void clearJoyLeds(byte joyId);
void setScoreLeds(byte joyId, int score, CRGB color);
void setButtonLed(byte joyId, byte btnIdx, CRGB color);
extern const byte _joyBtnToLed[4];
extern const CRGB _joyBtnColors[4];

// WiFi gameplay-mode (defineret i wifi_manager.h)
void disconnectWiFiForGameplay();
extern bool _joyLedsSilent;  // Dvale-flag – sættes i Gamebox.ino

enum GameState { STATE_WAITING, STATE_COUNTDOWN, STATE_PLAYING, STATE_VICTORY };

class GameManager {
  private:
    GameState state     = STATE_WAITING;
    CRGB winnerColor    = CRGB::Black;
    byte winnerJoy      = 0;
    void (*runGameFunc)()  = nullptr;
    void (*initGameFunc)() = nullptr;

    unsigned long _victStart   = 0;
    bool          _soundPlayed = false;
    bool _wifiDisconnected = false;

  public:
    int  getState()           { return (int)state; }
    bool isWifiDisconnected() { return _wifiDisconnected; }

    void setState(int newState) {
      GameState prev = state;
      state = (GameState)newState;

      if (state == STATE_VICTORY) {
        _victStart   = 0;
        _soundPlayed = false;
      }
      if (prev == STATE_VICTORY && state != STATE_VICTORY) {
        scoreGreen  = 0;
        scoreRed    = 0;
        winnerColor = CRGB::Black;
      }
      // Ryd joystik-LEDs ved state-skift ind i klar-fase eller spil
      if (state == STATE_COUNTDOWN || state == STATE_PLAYING) {
        clearAllJoyLeds();
      }
    }

    void setWinnerColor(CRGB c) { winnerColor = c; }
    void setWinner(byte joyId, CRGB c) { winnerJoy = joyId; winnerColor = c; }

    void setup(void (*initFunc)(), void (*runFunc)()) {
      initGameFunc = initFunc;
      runGameFunc  = runFunc;
      state        = STATE_WAITING;
    }

    void update(CRGB* leds, int screenLen, int firstLed) {
      switch (state) {
        case STATE_WAITING:   runWaiting(leds, screenLen, firstLed);      break;
        case STATE_COUNTDOWN: runReadyWaiting(leds, screenLen, firstLed); break;
        case STATE_PLAYING:   if (runGameFunc) runGameFunc();             break;
        case STATE_VICTORY:   runVictoryAnimation(leds, screenLen, firstLed); break;
      }
    }

    // ──────────────────────────────────────────────────────────────────────────
    //  LOBBY  –  Joystik-LEDs blinker i holdfarver
    //  Strip holdes sort – overlay tegnes af Gamebox.ino
    // ──────────────────────────────────────────────────────────────────────────
    void runWaiting(CRGB* leds, int screenLen, int firstLed) {
      fill_solid(leds + firstLed, screenLen, CRGB::Black);

      if (_joyLedsSilent) {
        // Dvale-forvarsel: hold alt mørkt – stop joystik-blink
        showLeds();
        return;
      }

      bool blink = (millis() / 500) % 2 == 0;
      static bool lastBlink = false;
      if (blink != lastBlink) {
        lastBlink = blink;
        forceSendLed(JOY_A, JOY_LED_ALL, blink ? CRGB::Green : CRGB::Black);
        forceSendLed(JOY_B, JOY_LED_ALL, blink ? CRGB::Red   : CRGB::Black);
      }

      showLeds();
    }

    // ──────────────────────────────────────────────────────────────────────────
    //  KLAR-FASE  –  Joystiks melder klar individuelt
    //
    //  Strip: 12 grønne LEDs til venstre fra center | 12 røde til højre
    //    [dim × 4][fuld × 4][dim × 4]  fra center og ud
    //    → alle 12 fuld farve når holdet er klar
    //
    //  Joystik-LEDs:
    //    Ikke klar   → blinker alle i holdfarve
    //    Klar        → 3 point-LEDs tændt (konstant) + 4 knap-LEDs reaktive
    // ──────────────────────────────────────────────────────────────────────────
    void runReadyWaiting(CRGB* leds, int screenLen, int firstLed) {
      fill_solid(leds + firstLed, screenLen, CRGB::Black);

      int center = screenLen / 2;

      // Dim-farver (BackBrightness skaleret til 0-255)
      uint8_t dimVal = map(BackBrightness, 0, 100, 0, 255);
      CRGB greenDim  = CRGB(0, dimVal, 0);
      CRGB redDim    = CRGB(dimVal, 0, 0);
      CRGB greenFull = CRGB::Green;
      CRGB redFull   = CRGB::Red;

      // Mønster fra center og ud: [dim × 4][fuld × 4][dim × 4]
      for (int i = 0; i < 12; i++) {
        bool midBand = (i >= 4 && i < 8);
        CRGB gColor = joyReady[0] ? greenFull : (midBand ? greenFull : greenDim);
        CRGB rColor = joyReady[1] ? redFull   : (midBand ? redFull   : redDim);
        leds[firstLed + center - 1 - i] = gColor;
        leds[firstLed + center     + i] = rColor;
      }
      showLeds();

      // ── Joystik-LEDs ────────────────────────────────────────────────────────

      // Blink for ikke-klar joystiks
      bool blink = (millis() / 500) % 2 == 0;
      static bool lastBlinkR = false;
      if (blink != lastBlinkR) {
        lastBlinkR = blink;
        if (!joyReady[0]) forceSendLed(JOY_A, JOY_LED_ALL, blink ? CRGB::Green : CRGB::Black);
        if (!joyReady[1]) forceSendLed(JOY_B, JOY_LED_ALL, blink ? CRGB::Red   : CRGB::Black);
      }

      // Klar-joystik: point-LEDs tændt + knap-LEDs reaktive
      static bool lastReadyA = false, lastReadyB = false;

      if (joyReady[0] && !lastReadyA) {
        // Netop meldt klar: tænd alle 3 point-LEDs, sluk knap-LEDs
        setScoreLeds(JOY_A, 3, CRGB::Green);
        for (int b = 0; b < 4; b++) setButtonLed(JOY_A, b, CRGB::Black);
      }
      if (joyReady[1] && !lastReadyB) {
        setScoreLeds(JOY_B, 3, CRGB::Red);
        for (int b = 0; b < 4; b++) setButtonLed(JOY_B, b, CRGB::Black);
      }
      lastReadyA = joyReady[0];
      lastReadyB = joyReady[1];

      // Reaktive knap-LEDs mens klar
      if (joyReady[0]) {
        for (int b = 0; b < 4; b++)
          setButtonLed(JOY_A, b, joyBtnPressed[0][b] ? _joyBtnColors[b] : CRGB::Black);
      }
      if (joyReady[1]) {
        for (int b = 0; b < 4; b++)
          setButtonLed(JOY_B, b, joyBtnPressed[1][b] ? _joyBtnColors[b] : CRGB::Black);
      }

      // ── Begge klar → start spil ─────────────────────────────────────────────
      if (bothJoysReady()) {
        clearJoyReady();
        lastReadyA = lastReadyB = false;

        // 1. Nulstil joystik-LEDs – to gange med pause (ESP-NOW har ingen ACK)
        clearAllJoyLeds();
        delay(30);
        clearAllJoyLeds();

        // 2. Frakobil WiFi (kun første gang)
        if (!_wifiDisconnected) {
          disconnectWiFiForGameplay();
          _wifiDisconnected = true;
        }

        // Spilstart-lyd afspilles allerede ved grøn knap i Gamebox.ino – ikke her

        // 100ms pause: giver joysticket tid til at behandle clear-pakken
        // og ESP-NOW-radio tid til at stabilisere sig – gælder ALLE spilstart
        delay(100);

        // 3. Start spil
        fill_solid(leds + firstLed, screenLen, CRGB::Black);
        showLeds();
        if (initGameFunc) initGameFunc();
        state = STATE_PLAYING;
      }
    }

    // ──────────────────────────────────────────────────────────────────────────
    //  VINDERANIMATION  –  Blinker i vinderfarven (ingen auto-timeout)
    //
    //  Joystik-LEDs blinker permanent i vinderfarven.
    //  Strip holdes sort – drawGameIDIndicator() tegnes ovenpå af Gamebox.ino.
    //  Tilstand skiftes eksternt via grøn knap (Gamebox.ino).
    // ──────────────────────────────────────────────────────────────────────────
    void runVictoryAnimation(CRGB* leds, int screenLen, int firstLed) {
      if (_victStart == 0) {
        _victStart   = millis();
        _soundPlayed = false;
      }

      if (!_soundPlayed) {
        sendSoundOrBroadcast(winnerJoy, WinnerTune_id, WinnerTune_vol);
        sendSoundBoth(GameSlut_id, GameSlut_vol);
        _soundPlayed = true;
      }

      unsigned long elapsed = millis() - _victStart;
      bool on = (elapsed / 250) % 2 == 0;

      static bool lastVictOn = false;
      if (on != lastVictOn) {
        lastVictOn = on;
        forceSendLed(JOY_A, JOY_LED_ALL, on ? winnerColor : CRGB::Black);
        forceSendLed(JOY_B, JOY_LED_ALL, on ? winnerColor : CRGB::Black);
      }

      // Strip holdes sort; spil-indikatoren tegnes ovenpå
      fill_solid(leds + firstLed, screenLen, CRGB::Black);
      showLeds();
    }
};

#endif // GAME_MANAGER_H
