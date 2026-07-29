#ifndef DEMO_GAME_H
#define DEMO_GAME_H

// =============================================================================
//  demo_game.h  –  Spil 8: Demo-spil med animationer
//
//  Styres udelukkende af rødt joystik (JOY_B):
//    Blå knap → næste   (11..15, 21..24, 31..34, 41..44, 51..53, 61..63, 71..73)
//    Rød knap → forrige
//    Gul/grøn → animation-specifik manipulering
//
//  Anim 11 : TOW-simulation (hvid bar, 10 sek cyklus)
//  Anim 12 : LED-fade grøn↔gul, gul knap = stun-blink
//  Anim 13 : Point-demo (gul=rødt point, grøn=grønt point)
//  Anim 14 : Vinder fra anim 13 + vindertune
//  Anim 15 : Dvale-preview (sort + 2 lilla)
//  Anim 21 : Simon – farver skifter i midten
//  Anim 22 : Simon – 16 rigtige på 10 sek, begge ender blinker
//  Anim 23 : Vinder (rød – tvungen)
//  Anim 24 : Dvale-preview
//  Anim 31 : ColorCatcher – kugler flyver mod begge ender
//  Anim 32 : ColorCatcher – rødt område vokser på rød side, loop
//  Anim 33 : Vinder (grøn – tvungen)
//  Anim 34 : Dvale-preview
//  Anim 41 : Speed Ball – statisk bane, bold flyver frem og tilbage
//  Anim 42 : Speed Ball – aktiv knap skifter hvert 2. sek
//  Anim 43 : Vinder (grøn – tvungen)
//  Anim 44 : Dvale-preview
//  Anim 51 : Whac-A-Mole – blinkende knap på joysticks, pointbjælker (20 sek loop)
//  Anim 52 : Vinder (rød – tvungen)
//  Anim 53 : Dvale-preview
//  Anim 61 : Chain Reaction – bane + bold flyver frem/tilbage, skifter farve
//  Anim 62 : Vinder (grøn – tvungen)
//  Anim 63 : Dvale-preview
//  Anim 71 : Ultimate WAM – hvide knapper blinker forskellige steder (20 sek loop)
//  Anim 72 : Vinder (rød – tvungen)
//  Anim 73 : Dvale-preview
//  Anim 81 : Lyd-demo – spiller "spil slut" ved indgang; gul=spil start, grøn=spil slut
// =============================================================================

#include <math.h>
#include <FastLED.h>
#include "hw_utils.h"
#include "espnow_leds.h"

extern volatile bool joyBtnPressed[2][11];
extern byte WinnerTune_id, WinnerTune_vol;
extern byte GameStart_id,  GameStart_vol;
extern byte GameSlut_id,   GameSlut_vol;
extern byte BackBrightness;
extern int SB_HomeSize, SB_WhiteGrow;
extern int CHA_Zone;
void sendSoundBoth(byte soundId, byte volume);

// Skalér en farve med BackBrightness (0-100 → 0-255)
static CRGB _daDim(CRGB color) {
    uint8_t dim = map(BackBrightness, 0, 100, 0, 255);
    return color.nscale8_video(dim);
}

// ─── Knap-indeks  (Rød=0, Gul=1, Grøn=2, Blå=3) ─────────────────────────────
#define _DB_RED    0
#define _DB_YELLOW 1
#define _DB_GREEN  2
#define _DB_BLUE   3

// ─── Farve-paletter ───────────────────────────────────────────────────────────
static const CRGB _DA_SIMON[4] = { CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Yellow };
static const CRGB _DA_BALL[4]  = { CRGB::Blue, CRGB(255,100,0), CRGB::Red, CRGB::Green };
// Whac-A-Mole / Chain Reaction knap-farver (Blå, Grøn, Orange, Rød)
static const CRGB _DA_WAM4[4]  = { CRGB::Blue, CRGB::Green, CRGB(255,100,0), CRGB::Red };
// Ultimate WAM: LED-ID'er på det store joystik (11 knapper)
static const int _DA_UWAM_LED[11] = { 0, 1, 2, 3, 4, 6, 7, 8, 10, 11, 12 };
#define _DA_NUWAM 11

// ─── Navigation-sekvens ──────────────────────────────────────────────────────
static const int _DA_SEQ[]  = { 11,12,13,14,15, 21,22,23,24, 31,32,33,34, 41,42,43,44,
                                51,52,53, 61,62,63, 71,72,73, 81 };
static const int _DA_SEQN   = 27;
static int _daNext(int c) { for(int i=0;i<_DA_SEQN;i++) if(_DA_SEQ[i]==c) return _DA_SEQ[(i+1)%_DA_SEQN]; return 11; }
static int _daPrev(int c) { for(int i=0;i<_DA_SEQN;i++) if(_DA_SEQ[i]==c) return _DA_SEQ[(i-1+_DA_SEQN)%_DA_SEQN]; return 11; }

// ─── Global state ────────────────────────────────────────────────────────────
static int           _da           = 11;
static unsigned long _daStart      = 0;
static int           _daScoreGreen = 0;
static int           _daScoreRed   = 0;
static bool          _daWinGreen   = true;
static bool          _daTunePlayed = false;
// Anim 21
static int           _da21Idx  = 0;
static unsigned long _da21Last = 0;
// Anim 22
static int           _da22Count = 0;
static unsigned long _da22Last  = 0;
// Anim 41/42
static float         _da41BallPos     = 0.0f;
static float         _da41BallVel     = 0.4f;
static int           _da41ActiveBtnA  = 0;
static int           _da41ActiveBtnB  = 0;
static unsigned long _da42BtnLast     = 0;
// Anim 51 / 71 (Whac-A-Mole / Ultimate WAM demo)
static unsigned long _da51Last = 0;
static unsigned long _da71Last = 0;
// Anim 61 (Chain Reaction demo)
static float         _da61Pos = 0.0f;
static float         _da61Vel = 0.5f;
static int           _da61Col = 0;

// ─── Kugle-state (anim 31/32) ────────────────────────────────────────────────
// Boldpar: 1 bold går venstre, 1 går højre – samme farve (som i rigtigt spil)
#define _DA_NBALLS 6
struct _DaPair { float pos; int col; float spd; };
static _DaPair _daPairs[_DA_NBALLS];
static bool    _daBallsInited = false;
static unsigned long _daBallLast = 0;

extern int CCatcherHomePix;

static void _daPairSpawn(int b, int screenLen) {
    _daPairs[b].pos = 0.0f;
    _daPairs[b].col = random(4);
    _daPairs[b].spd = 0.25f + random(20) / 100.0f;
}

static void _daBallsInit(int screenLen, int /*firstLed*/) {
    int maxDist = screenLen / 2 - 1;
    for (int b = 0; b < _DA_NBALLS; b++) {
        _daPairs[b].pos = (float)(b * maxDist / _DA_NBALLS);
        _daPairs[b].col = b % 4;
        _daPairs[b].spd = 0.22f + b * 0.04f;
    }
    _daBallsInited = true;
}

// ─── Skift animation ─────────────────────────────────────────────────────────
static void _daChange(CRGB* leds, int screenLen, int firstLed, int newAnim) {
    _da      = newAnim;
    _daStart = millis();
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    showLeds();
    clearAllJoyLeds(); delay(30); clearAllJoyLeds();

    // Nulstil animation-specifikke tællere
    _da21Idx  = 0; _da21Last = 0;
    _da22Count = 0; _da22Last = 0;

    if (newAnim == 12) {
        for (int b = 0; b < 4; b++) { setButtonLed(JOY_A, b, CRGB::Green); setButtonLed(JOY_B, b, CRGB::Green); }
    }
    if (newAnim == 13) {
        _daScoreGreen = 1; _daScoreRed = 1;
        setScoreLeds(JOY_A, 1, CRGB::Green); setScoreLeds(JOY_B, 1, CRGB::Red);
    }
    if (newAnim == 14) { _daWinGreen = (_daScoreGreen >= _daScoreRed); _daTunePlayed = false; }
    if (newAnim == 23) { _daWinGreen = false; _daTunePlayed = false; }
    if (newAnim == 33 || newAnim == 43) { _daWinGreen = true; _daTunePlayed = false; }
    if (newAnim == 52 || newAnim == 72) { _daWinGreen = false; _daTunePlayed = false; }
    if (newAnim == 62) { _daWinGreen = true; _daTunePlayed = false; }
    if (newAnim == 31 || newAnim == 32) _daBallsInited = false;
    if (newAnim == 41 || newAnim == 42) {
        _da41BallPos    = 0.0f;
        _da41BallVel    = 0.4f;
        _da41ActiveBtnA = random(4);
        _da41ActiveBtnB = random(4);
        _da42BtnLast    = millis();
        clearAllJoyLeds(); delay(30);
        setButtonLed(JOY_A, _da41ActiveBtnA, CRGB::White);
        setButtonLed(JOY_B, _da41ActiveBtnB, CRGB::White);
    }
    if (newAnim == 51) _da51Last = 0;   // → moldvarp vises straks
    if (newAnim == 71) _da71Last = 0;
    if (newAnim == 61) { _da61Pos = 0.0f; _da61Vel = 0.5f; _da61Col = random(4); }
    if (newAnim == 81) {
        sendSoundBoth(GameSlut_id, GameSlut_vol);   // "spil slut" afspilles én gang ved indgang
        // Knap-hints: gul = spil start, grøn = spil slut
        setButtonLed(JOY_A, _DB_YELLOW, CRGB::Yellow); setButtonLed(JOY_A, _DB_GREEN, CRGB::Green);
        setButtonLed(JOY_B, _DB_YELLOW, CRGB::Yellow); setButtonLed(JOY_B, _DB_GREEN, CRGB::Green);
    }
}

// =============================================================================
//  Animationer
// =============================================================================

// ─── 11 : TOW-simulation ─────────────────────────────────────────────────────
static void _daAnim11(CRGB* leds, int screenLen, int firstLed) {
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    for (int i = 0; i < 6; i++) {
        leds[firstLed + i]                 = _daDim(CRGB::Green);
        leds[firstLed + screenLen - 1 - i] = _daDim(CRGB::Red);
    }
    float phase  = fmod((float)(millis() - _daStart) / 10000.0f, 1.0f);
    float sinVal = -sinf(2.0f * M_PI * phase);
    int leftLim  = firstLed + 8;
    int rightLim = firstLed + screenLen - 9;
    int mid      = (leftLim + rightLim) / 2;
    int amp      = (rightLim - leftLim) / 2;
    int barPos   = mid + (int)((float)amp * sinVal);
    for (int k = -1; k <= 2; k++) {
        int p = barPos + k;
        if (p >= firstLed && p < firstLed + screenLen) leds[p] = CRGB::White;
    }
    showLeds();
}

// ─── 12 : LED-fade ───────────────────────────────────────────────────────────
static void _daAnim12(CRGB* leds, int screenLen, int firstLed, const bool just[4]) {
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    showLeds();
    static unsigned long stunEnd    = 0;
    static unsigned long lastUpd    = 0;
    static uint8_t       lastRval   = 0;
    static bool          lastStunOn = false;
    if (just[_DB_YELLOW]) stunEnd = millis() + 800;
    if (millis() < stunEnd) {
        bool on = (millis() / 80) % 2;
        if (on != lastStunOn) {
            lastStunOn = on;
            CRGB c = on ? CRGB::White : CRGB::Black;
            forceSendLed(JOY_A, JOY_LED_ALL, c); forceSendLed(JOY_B, JOY_LED_ALL, c);
        }
        return;
    }
    lastStunOn = false;
    if (millis() - lastUpd < 50) return;
    lastUpd = millis();
    float phase  = fmod((float)(millis() - _daStart) / 4000.0f, 1.0f);
    float ratio  = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
    uint8_t rVal = (uint8_t)(255 * ratio);
    if (abs((int)rVal - (int)lastRval) < 4) return;
    lastRval = rVal;
    CRGB fade = CRGB(rVal, 255, 0);
    setButtonLed(JOY_A, _DB_GREEN, CRGB::Green); setButtonLed(JOY_A, _DB_BLUE,   CRGB::Blue);
    setButtonLed(JOY_B, _DB_GREEN, CRGB::Green); setButtonLed(JOY_B, _DB_BLUE,   CRGB::Blue);
    setButtonLed(JOY_A, _DB_RED,   fade);         setButtonLed(JOY_A, _DB_YELLOW, fade);
    setButtonLed(JOY_B, _DB_RED,   fade);         setButtonLed(JOY_B, _DB_YELLOW, fade);
}

// ─── 13 : Point-demo ─────────────────────────────────────────────────────────
static void _daAnim13(CRGB* leds, int screenLen, int firstLed, const bool just[4]) {
    if (just[_DB_YELLOW]) { _daScoreRed   = (_daScoreRed   < 3) ? _daScoreRed+1   : 0; setScoreLeds(JOY_B, _daScoreRed,   CRGB::Red);   }
    if (just[_DB_GREEN])  { _daScoreGreen = (_daScoreGreen < 3) ? _daScoreGreen+1 : 0; setScoreLeds(JOY_A, _daScoreGreen, CRGB::Green); }
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    int mid = firstLed + screenLen / 2, sc = max(1, screenLen / 8);
    for (int i = 0; i < _daScoreGreen * sc; i++) leds[mid - 1 - i] = CRGB::Green;
    for (int i = 0; i < _daScoreRed   * sc; i++) leds[mid + i]     = CRGB::Red;
    showLeds();
}

// ─── 14 / 23 / 33 : Vinder-animation ────────────────────────────────────────
static void _daAnim14(CRGB* leds, int screenLen, int firstLed) {
    if (!_daTunePlayed) { sendSoundBoth(WinnerTune_id, WinnerTune_vol); _daTunePlayed = true; }
    CRGB wc = _daWinGreen ? CRGB::Green : CRGB::Red;
    bool on = (millis() / 250) % 2;
    CRGB  c = on ? wc : CRGB::Black;
    fill_solid(leds + firstLed, screenLen, c); showLeds();
    static bool lastOn = false;
    if (on != lastOn) { lastOn = on; forceSendLed(JOY_A, JOY_LED_ALL, c); forceSendLed(JOY_B, JOY_LED_ALL, c); }
}

// ─── 15 / 24 / 34 : Dvale-preview ───────────────────────────────────────────
static void _daAnim15(CRGB* leds, int screenLen, int firstLed) {
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    int mid = firstLed + screenLen / 2;
    leds[mid - 1] = _daDim(CRGB(255, 0, 255)); leds[mid] = _daDim(CRGB(255, 0, 255));
    showLeds();
}

// ─── 21 : Simon – farver i midten (30 pixels som i det rigtige spil) ─────────
static void _daAnim21(CRGB* leds, int screenLen, int firstLed) {
    if (millis() - _da21Last > 1500) {
        _da21Idx  = (_da21Idx + 1) % 4;
        _da21Last = millis();
    }
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    int mid = firstLed + screenLen / 2;
    for (int i = -15; i < 15; i++) leds[mid + i] = _DA_SIMON[_da21Idx];
    showLeds();
}

// ─── 22 : Simon – voksende rækker (1 pixel pr. hit, 2-pixel spacing som i det
// rigtige spil). Grøn spiller: grønne pixels fra venstre. Rød: røde fra højre.
// Tæller til 10 pixels og starter forfra.
static void _daAnim22(CRGB* leds, int screenLen, int firstLed) {
    const unsigned long HIT_MS = 625;
    unsigned long now = millis();
    if (now - _da22Last >= HIT_MS) {
        _da22Last = now;
        _da22Count++;
        if (_da22Count > 10) _da22Count = 0;
    }
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    for (int i = 0; i < _da22Count; i++) {
        leds[firstLed + 2 + i * 2]             = CRGB::Green;
        leds[firstLed + screenLen - 3 - i * 2] = CRGB::Red;
    }
    showLeds();
}

// ─── 31/32 : ColorCatcher – bane og boldpar ──────────────────────────────────
// Bane: begge hold har grønt hjemmeområde (CCatcherHomePix). Kun rød spillers
// straf-zone (missesB) vokser – præcis som i det rigtige spil.
static void _daDrawCCField(CRGB* leds, int screenLen, int firstLed, int missesB) {
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    int mid = firstLed + screenLen / 2;
    // Grøn spiller (venstre) – ingen straf
    for (int i = 0; i < CCatcherHomePix; i++)
        leds[firstLed + i] = _daDim(CRGB::DarkGreen);
    // Rød spiller (højre) – straf yderst, grøn hjemmebane indenfor
    for (int i = 0; i < missesB; i++)
        leds[firstLed + screenLen - 1 - i] = _daDim(CRGB::Red);
    for (int i = 0; i < CCatcherHomePix; i++) {
        int p = firstLed + screenLen - 1 - missesB - i;
        if (p >= mid) leds[p] = _daDim(CRGB::DarkGreen);
    }
    // Midtermarkering
    leds[mid - 1] = _daDim(CRGB(255, 0, 255));
    leds[mid]     = _daDim(CRGB(255, 0, 255));
}

// Boldpar flyver fra midten ud mod begge sider – 1 pixel, samme farve per par
static void _daMovePairs(int screenLen, int maxDist) {
    unsigned long now = millis();
    unsigned long dt  = now - _daBallLast;
    _daBallLast = now;
    if (dt > 50) dt = 50;
    for (int b = 0; b < _DA_NBALLS; b++) {
        _daPairs[b].pos += _daPairs[b].spd * (float)dt / 16.0f;
        if ((int)_daPairs[b].pos >= maxDist) _daPairSpawn(b, screenLen);
    }
}

// missesB: skjul bold på rød side hvis den er i straf-zonen
static void _daDrawPairs(CRGB* leds, int screenLen, int firstLed, int missesB = 0) {
    int mid = firstLed + screenLen / 2;
    for (int b = 0; b < _DA_NBALLS; b++) {
        int lPos = mid - 1 - (int)_daPairs[b].pos;
        int rPos = mid     + (int)_daPairs[b].pos;
        if (lPos >= firstLed)
            leds[lPos] = _DA_BALL[_daPairs[b].col];
        // Skjul på rød side hvis bold er i straf-zonen (yderst)
        if (rPos < firstLed + screenLen - missesB)
            leds[rPos] = _DA_BALL[_daPairs[b].col];
    }
}

static void _daAnim31(CRGB* leds, int screenLen, int firstLed) {
    if (!_daBallsInited) { _daBallsInit(screenLen, firstLed); _daBallLast = millis(); }
    // Boldene flyver IGENNEM det grønne område – stopper ved kanten (ingen rød zone)
    int maxDist = screenLen / 2 - 1;
    _daMovePairs(screenLen, maxDist);
    _daDrawCCField(leds, screenLen, firstLed, 0);
    _daDrawPairs(leds, screenLen, firstLed);
    showLeds();
}

// ─── 32 : ColorCatcher – kun rød spiller taber (5 sek loop) ─────────────────
static void _daAnim32(CRGB* leds, int screenLen, int firstLed) {
    if (!_daBallsInited) { _daBallsInit(screenLen, firstLed); _daBallLast = millis(); }

    unsigned long elapsed = millis() - _daStart;
    const unsigned long LOSE_MS = 5000;
    if (elapsed >= LOSE_MS) {
        _daStart       = millis();
        _daBallsInited = false;
        elapsed        = 0;
    }

    int maxPenalty  = screenLen / 2 - 1;
    int missesB_sim = (int)((float)elapsed / LOSE_MS * maxPenalty);

    // Boldene flyver fuld distance – skjules på rød side i straf-zonen
    _daMovePairs(screenLen, screenLen / 2 - 1);
    _daDrawCCField(leds, screenLen, firstLed, missesB_sim);
    _daDrawPairs(leds, screenLen, firstLed, missesB_sim);
    showLeds();
}

// ─── 41/42 : Speed Ball ──────────────────────────────────────────────────────
// Tegner begge halvdeles zoner: hvid(yderst)→blå→rød→orange→grøn (som rigtigt spil)
static void _daDrawSBHalf(CRGB* leds, int start, int dir, int half, float f) {
    float pG  = 0.15f + f * 0.25f;
    float pY  = 0.07f + f * 0.13f;
    float pR  = 0.03f + f * 0.07f;
    float pB  = 0.00f + f * 0.30f;
    float pct = (SB_HomeSize / 100.0f) + f * (1.0f - SB_HomeSize / 100.0f);
    int total = (int)(half * pct);
    float sum = pG + pY + pR + pB;
    int bZ = (int)(total * pB / sum);
    int rZ = (int)(total * pR / sum);
    int yZ = (int)(total * pY / sum);
    int gZ = total - bZ - rZ - yZ;
    for (int i = 0; i < half; i++) {
        CRGB col = CRGB::Black;
        if      (i < bZ)              col = CRGB::Blue;
        else if (i < bZ + rZ)         col = CRGB::Red;
        else if (i < bZ + rZ + yZ)    col = CRGB(255, 100, 0);
        else if (i < bZ + rZ + yZ + gZ) col = CRGB::Green;
        leds[start + i * dir] = _daDim(col);
    }
}

// Anim 41: bane + bold frem og tilbage, fast aktiv knap
static void _daAnim41(CRGB* leds, int screenLen, int firstLed) {
    int   half = screenLen / 2;
    int   mid  = firstLed + half;
    float f    = 0.4f;  // midtvejs i spillet – alle zoner synlige

    _da41BallPos += _da41BallVel;
    if (_da41BallPos >=  (float)(half - 1)) { _da41BallPos =  (float)(half - 2); _da41BallVel = -_da41BallVel; }
    if (_da41BallPos <= -(float)(half - 1)) { _da41BallPos = -(float)(half - 2); _da41BallVel = -_da41BallVel; }

    _daDrawSBHalf(leds, firstLed,              1,  half, f);
    _daDrawSBHalf(leds, firstLed + screenLen - 1, -1, half, f);
    leds[mid + (int)_da41BallPos] = CRGB::White;
    showLeds();
}

// Anim 42: som 41, men aktiv knap skifter hvert 2. sek
static void _daAnim42(CRGB* leds, int screenLen, int firstLed) {
    // Skift aktiv knap hvert 2. sek
    if (millis() - _da42BtnLast >= 2000) {
        _da42BtnLast    = millis();
        _da41ActiveBtnA = (_da41ActiveBtnA + 1) % 4;
        _da41ActiveBtnB = (_da41ActiveBtnB + 1) % 4;
        clearAllJoyLeds();
        setButtonLed(JOY_A, _da41ActiveBtnA, CRGB::White);
        setButtonLed(JOY_B, _da41ActiveBtnB, CRGB::White);
    }

    _daAnim41(leds, screenLen, firstLed);
}

// ─── 51/71 : Whac-A-Mole pointbjælker ────────────────────────────────────────
// Grøn fylder halvdelen af sin side, rød fylder hele sin side (rød "vinder").
static void _daDrawWamBars(CRGB* leds, int screenLen, int firstLed, float progress) {
    int half     = screenLen / 2;
    int mid      = firstLed + half;
    int greenBar = (int)(progress * (half / 2));   // grøn når halvvejs på sin side
    int redBar   = (int)(progress * half);          // rød fylder hele sin side
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    for (int i = 0; i < greenBar; i++) { int p = firstLed + i;                 if (p < mid)  leds[p] = CRGB::Green; }
    for (int i = 0; i < redBar;   i++) { int p = firstLed + screenLen - 1 - i;  if (p >= mid) leds[p] = CRGB::Red;   }
    leds[mid - 1] = CRGB::White;
    leds[mid]     = CRGB::White;
}

// Anim 51: Whac-A-Mole – én blinkende knap ad gangen på begge joysticks
static void _daAnim51(CRGB* leds, int screenLen, int firstLed) {
    const unsigned long CYCLE = 20000;
    unsigned long elapsed = millis() - _daStart;
    if (elapsed >= CYCLE) { _daStart = millis(); elapsed = 0; }

    // Ny "moldvarp" hver ~700ms: tilfældig knap i tilfældig farve
    if (millis() - _da51Last >= 700) {
        _da51Last = millis();
        clearAllJoyLeds();
        int  btn = random(4);
        CRGB col = _DA_WAM4[random(4)];
        setButtonLed(JOY_A, btn, col);
        setButtonLed(JOY_B, btn, col);
    }

    _daDrawWamBars(leds, screenLen, firstLed, (float)elapsed / CYCLE);
    showLeds();
}

// Anim 61: Chain Reaction – bane + bold der flyver frem/tilbage, skifter farve
static void _daAnim61(CRGB* leds, int screenLen, int firstLed) {
    int mid  = firstLed + screenLen / 2;
    int zone = CHA_Zone;

    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    // Hjemmezoner (mørkegrøn) i begge ender
    CRGB hz = _daDim(CRGB::DarkGreen);
    for (int i = 0; i < zone; i++) {
        leds[firstLed + i]                 = hz;
        leds[firstLed + screenLen - 1 - i] = hz;
    }
    // Midtermarkering (lilla)
    leds[mid - 1] = _daDim(CRGB(255, 0, 255));
    leds[mid]     = _daDim(CRGB(255, 0, 255));

    // Bold bevæger sig – skifter farve ved retningsskift (som i det rigtige spil)
    _da61Pos += _da61Vel;
    int ledPos = mid + (int)_da61Pos;
    if (ledPos >= firstLed + screenLen - 2) { _da61Vel = -fabsf(_da61Vel); _da61Col = (_da61Col + 1) % 4; }
    else if (ledPos <= firstLed + 1)        { _da61Vel =  fabsf(_da61Vel); _da61Col = (_da61Col + 1) % 4; }
    ledPos = mid + (int)_da61Pos;

    // Tegn 3-pixel bold
    CRGB bc = _DA_WAM4[_da61Col];
    if (ledPos     >= firstLed && ledPos     < firstLed + screenLen) leds[ledPos]     = bc;
    if (ledPos + 1 >= firstLed && ledPos + 1 < firstLed + screenLen) leds[ledPos + 1] = bc;
    if (ledPos - 1 >= firstLed && ledPos - 1 < firstLed + screenLen) leds[ledPos - 1] = bc;
    showLeds();
}

// Anim 71: Ultimate WAM – 2-3 hvide knapper blinker forskellige steder
static void _daAnim71(CRGB* leds, int screenLen, int firstLed) {
    const unsigned long CYCLE = 20000;
    unsigned long elapsed = millis() - _daStart;
    if (elapsed >= CYCLE) { _daStart = millis(); elapsed = 0; }

    // Nye aktive knapper hver ~800ms: 2-3 hvide LEDs på det store joystik
    if (millis() - _da71Last >= 800) {
        _da71Last = millis();
        clearAllJoyLeds();
        int n = 2 + random(2);   // 2-3 aktive
        for (int k = 0; k < n; k++) {
            int id = _DA_UWAM_LED[random(_DA_NUWAM)];
            forceSendLed(JOY_A, id, CRGB::White);
            forceSendLed(JOY_B, id, CRGB::White);
        }
    }

    _daDrawWamBars(leds, screenLen, firstLed, (float)elapsed / CYCLE);
    showLeds();
}

// ─── 81 : Lyd-demo ────────────────────────────────────────────────────────────
// "spil slut" spilles ved indgang (i _daChange). Gul = spil start, grøn = spil slut.
static void _daAnim81(CRGB* leds, int screenLen, int firstLed, const bool just[4]) {
    if (just[_DB_YELLOW]) sendSoundBoth(GameStart_id, GameStart_vol);  // spil start
    if (just[_DB_GREEN])  sendSoundBoth(GameSlut_id,   GameSlut_vol);   // spil slut

    // Genopfrisk knap-hints (state-tracked → ingen redundante sends)
    setButtonLed(JOY_A, _DB_YELLOW, CRGB::Yellow); setButtonLed(JOY_A, _DB_GREEN, CRGB::Green);
    setButtonLed(JOY_B, _DB_YELLOW, CRGB::Yellow); setButtonLed(JOY_B, _DB_GREEN, CRGB::Green);

    // Strip-hint: gul blok = start, grøn blok = slut
    fill_solid(leds + firstLed, screenLen, CRGB::Black);
    int mid = firstLed + screenLen / 2;
    for (int i = 1; i <= 10; i++) leds[mid - i] = _daDim(CRGB::Yellow);
    for (int i = 0; i <  10; i++) leds[mid + i] = _daDim(CRGB::Green);
    showLeds();
}

// =============================================================================
//  Offentlig API
// =============================================================================

// ─── Screensaver-API (bruges af lobby i Gamebox.ino) ─────────────────────────
// Kør én valgt animation uden knap-navigation.
// demoAnimStart() initialiserer state (via _daChange); demoAnimFrame() tegner.
void demoAnimStart(int anim, CRGB* leds, int screenLen, int firstLed) {
    _daChange(leds, screenLen, firstLed, anim);
}

void demoAnimFrame(CRGB* leds, int screenLen, int firstLed) {
    switch (_da) {
        case 11: _daAnim11(leds, screenLen, firstLed); break;
        case 21: _daAnim21(leds, screenLen, firstLed); break;
        case 22: _daAnim22(leds, screenLen, firstLed); break;
        case 32: _daAnim32(leds, screenLen, firstLed); break;
        case 42: _daAnim42(leds, screenLen, firstLed); break;
        case 51: _daAnim51(leds, screenLen, firstLed); break;
        case 61: _daAnim61(leds, screenLen, firstLed); break;
        case 71: _daAnim71(leds, screenLen, firstLed); break;
        default: _daAnim11(leds, screenLen, firstLed); break;
    }
}

void initDemoGame(int /*screenLen*/) {
    _da           = 11;
    _daStart      = millis();
    _daScoreGreen = 0;
    _daScoreRed   = 0;
    _daBallsInited= false;
}

void runDemoGame(CRGB* leds, int screenLen, int firstLed,
                 const int* /*pinsA*/, const int* /*pinsB*/) {
    // Knap-kant-detektion – kun JOY_B (rødt joystik)
    static bool prevBtn[4] = {};
    bool just[4];
    for (int b = 0; b < 4; b++) {
        just[b]    = (bool)joyBtnPressed[JOY_B][b] && !prevBtn[b];
        prevBtn[b] = (bool)joyBtnPressed[JOY_B][b];
    }

    // Global navigation
    if (just[_DB_BLUE]) { _daChange(leds, screenLen, firstLed, _daNext(_da)); return; }
    if (just[_DB_RED])  { _daChange(leds, screenLen, firstLed, _daPrev(_da)); return; }

    switch (_da) {
        case 11: _daAnim11(leds, screenLen, firstLed);          break;
        case 12: _daAnim12(leds, screenLen, firstLed, just);    break;
        case 13: _daAnim13(leds, screenLen, firstLed, just);    break;
        case 14: case 23: case 33: case 43:
        case 52: case 62: case 72: _daAnim14(leds, screenLen, firstLed); break;
        case 15: case 24: case 34: case 44:
        case 53: case 63: case 73: _daAnim15(leds, screenLen, firstLed); break;
        case 21: _daAnim21(leds, screenLen, firstLed);          break;
        case 22: _daAnim22(leds, screenLen, firstLed);          break;
        case 31: _daAnim31(leds, screenLen, firstLed);          break;
        case 32: _daAnim32(leds, screenLen, firstLed);          break;
        case 41: _daAnim41(leds, screenLen, firstLed);          break;
        case 42: _daAnim42(leds, screenLen, firstLed);          break;
        case 51: _daAnim51(leds, screenLen, firstLed);          break;
        case 61: _daAnim61(leds, screenLen, firstLed);          break;
        case 71: _daAnim71(leds, screenLen, firstLed);          break;
        case 81: _daAnim81(leds, screenLen, firstLed, just);    break;
    }
}

#endif // DEMO_GAME_H
