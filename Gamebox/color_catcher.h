#ifndef COLOR_CATCHER_H
#define COLOR_CATCHER_H

#include <FastLED.h>
#include "GameManager.h"
#include "hw_utils.h"

extern GameManager gameManager;
extern int scoreGreen;
extern int scoreRed;
extern const int JOYSTICK_LED_MAP[4];
extern const int JOYSTICK_SCORE_MAP[3];
extern byte BackBrightness;
extern int CCatcherTime;
extern int CCatcherPixTime;
extern int CCatcherPixNum;
extern int CCatcherHomePix;

extern byte CCHit_id, CCHit_vol;
extern byte CCMiss_id, CCMiss_vol;

// --- DEFINITIONER ---
#define MAX_DOTS 40

struct Dot {
    float pos;
    int colorIdx;
    bool active;
};

static Dot dotsA[MAX_DOTS];
static Dot dotsB[MAX_DOTS];

const CRGB CatcherColors[] = {CRGB::Blue, CRGB::Green, CRGB(255, 100, 0), CRGB::Red};
const char* ColorNames[] = {"Blå", "Grøn", "Orange", "Rød"};

static unsigned long lastSpawnTime = 0;
static unsigned long roundStartTime = 0;
static int missesA = 0;
static int missesB = 0;
static bool waitingForStart = true;

static int dotsInCurrentWave = 0;
static int currentWaveLimit = 3;

static bool _ccBtnStateA[4] = {false, false, false, false};
static bool _ccBtnStateB[4] = {false, false, false, false};

static bool isZoneA[300];
static bool isZoneB[300];

CRGB dimColor(CRGB color) {
    uint8_t dim = map(BackBrightness, 0, 100, 0, 255);
    return color.nscale8_video(dim);
}

void _resetRound() {
    missesA = 0; missesB = 0;
    dotsInCurrentWave = 0;
    currentWaveLimit = 3;
    waitingForStart = true;
    for(int i=0; i<MAX_DOTS; i++) {
        dotsA[i].active = false;
        dotsB[i].active = false;
    }
    clearAllButtonStates();
    for(int i=0; i<4; i++) { _ccBtnStateA[i] = false; _ccBtnStateB[i] = false; }
}

static int _ccInitFrames = 0;

void initColorCatcher(int screenLen) {
    scoreGreen = 0; scoreRed = 0;
    _ccInitFrames = 5;   // Force-send LED de første 5 frames
    _resetRound();
}

void runColorCatcher(CRGB* leds, int screenLen, int firstScreen, int* pinsA, int* pinsB) {
    unsigned long now = millis();
    int mid = firstScreen + (screenLen / 2);
    int halfLen = screenLen / 2;

    FastLED.clear();
    for(int i=0; i<300; i++) { isZoneA[i] = false; isZoneB[i] = false; }

    // Zone beregning
    int zoneA_Start = firstScreen + missesA;
    int zoneA_End = zoneA_Start + CCatcherHomePix - 1;
    int zoneB_Start = firstScreen + screenLen - 1 - missesB;
    int zoneB_End = zoneB_Start - (CCatcherHomePix - 1);

    for(int i=0; i < missesA; i++) leds[firstScreen + i] = dimColor(CRGB::Red);
    for(int i=zoneA_Start; i <= zoneA_End && i < mid-1; i++) {
        leds[i] = dimColor(CRGB::DarkGreen);
        isZoneA[i] = true;
    }
    for(int i=0; i < missesB; i++) leds[firstScreen + screenLen - 1 - i] = dimColor(CRGB::Red);
    for(int i=zoneB_Start; i >= zoneB_End && i >= mid; i--) {
        leds[i] = dimColor(CRGB::DarkGreen);
        isZoneB[i] = true;
    }

    // Midter-lys
    if (waitingForStart && (now % 1000 < 500)) {
        leds[mid] = CRGB(80, 0, 80); leds[mid-1] = CRGB(80, 0, 80);
    } else {
        leds[mid] = CRGB(30, 0, 30); leds[mid-1] = CRGB(30, 0, 30);
    }

    // Input & Start logik – brug joyRead i stedet for digitalRead
    int pressedBtnA = -1; int pressedBtnB = -1;
    bool startPressed = false;

    for(int c=0; c<4; c++) {
        bool pA = (joyRead(pinsA[c]) == LOW);
        if(pA && !_ccBtnStateA[c]) {
            if(waitingForStart) {
                waitingForStart = false; roundStartTime = now; lastSpawnTime = now;
                sendSoundOrBroadcast(JOY_A, CCHit_id, CCHit_vol);
                startPressed = true;
            } else { pressedBtnA = c; }
        }
        _ccBtnStateA[c] = pA;

        bool pB = (joyRead(pinsB[c]) == LOW);
        if(pB && !_ccBtnStateB[c]) {
            if(waitingForStart) {
                waitingForStart = false; roundStartTime = now; lastSpawnTime = now;
                sendSoundOrBroadcast(JOY_B, CCHit_id, CCHit_vol);
                startPressed = true;
            } else { pressedBtnB = c; }
        }
        _ccBtnStateB[c] = pB;
    }

    if (startPressed) { showLeds(); return; }
    // Force-send de første 5 frames så pakketab ved opstart ikke giver mørke LEDs
    bool _ccForce = (_ccInitFrames > 0);
    if (_ccForce) _ccInitFrames--;

    if (waitingForStart) {
        {   // Byg batch med 4 knap-farver – 1 pakke i stedet for 4
            EspNowLedBatch cb; cb.count = 4;
            for(int i=0; i<4; i++) { CRGB c=dimColor(CatcherColors[i]); cb.leds[i]={(byte)JOYSTICK_LED_MAP[i],c.r,c.g,c.b}; }
            if (_ccForce) { forceSendLedBatch(JOY_A,cb); forceSendLedBatch(JOY_B,cb); }
            else          { sendLedBatch(JOY_A,cb);      sendLedBatch(JOY_B,cb); }
        }
        setScoreLeds(JOY_A, scoreGreen, CRGB::Green);  setScoreLeds(JOY_B, scoreRed, CRGB::Red);
        showLeds(); return;
    }

    // Fart og Spawning
    unsigned long elapsed = now - roundStartTime;
    float speedFactor = 0.15;
    float pixTimeMs = (float)CCatcherPixTime * 1000.0;
    float totalGameMs = (float)CCatcherTime * 1000.0;

    if (elapsed < pixTimeMs) {
        currentWaveLimit = map(elapsed, 0, (long)pixTimeMs, 3, CCatcherPixNum);
        speedFactor = 0.15;
    } else {
        currentWaveLimit = CCatcherPixNum;
        float remainingFactor = (float)(elapsed - pixTimeMs) / (totalGameMs - pixTimeMs);
        if (remainingFactor > 1.0f) remainingFactor = 1.0f;
        speedFactor = 0.15 + (remainingFactor * 1.5);
    }

    float pixelSpacing = (float)(halfLen - CCatcherHomePix) / (float)currentWaveLimit;
    if (pixelSpacing < 4.0f) pixelSpacing = 4.0f;

    if (dotsInCurrentWave < currentWaveLimit) {
        float lastPos = 999.0f;
        for(int i=0; i<MAX_DOTS; i++) { if(dotsA[i].active && dotsA[i].pos < lastPos) lastPos = dotsA[i].pos; }
        if (lastPos == 999.0f || lastPos >= pixelSpacing) {
            int color = random(0, 4);
            for(int i=0; i<MAX_DOTS; i++) {
                if(!dotsA[i].active && !dotsB[i].active) {
                    dotsA[i].active = true; dotsA[i].pos = 0.0f; dotsA[i].colorIdx = color;
                    dotsB[i].active = true; dotsB[i].pos = 0.0f; dotsB[i].colorIdx = color;
                    dotsInCurrentWave++; lastSpawnTime = now; break;
                }
            }
        }
    } else if (now - lastSpawnTime > 2000) { dotsInCurrentWave = 0; }

    bool isHitA = false; bool isHitB = false;

    // Pixel bevægelse og tjek
    // graceA/B: 2-pixel nådezone ud over hjemmezonen – fanger timing-forsinkelse
    // Uopfangede bolde koster +2 (≈50% mere end forkert tryk, som koster +1)
    for(int i=0; i<MAX_DOTS; i++) {
        if(dotsA[i].active) {
            dotsA[i].pos += speedFactor;
            int ledPos    = mid - 1 - (int)dotsA[i].pos;
            int missLineA = firstScreen + missesA;   // Grænse for rød zone
            int graceA    = missLineA - 2;           // 2 pixels nåde ind i rød

            if (ledPos < graceA) {
                // Forbi nådezon → miss (+2 = 50% hårdere straf end forkert tryk)
                dotsA[i].active = false; missesA += 2;
                sendSoundOrBroadcast(JOY_A, CCMiss_id, CCMiss_vol);
            } else if (ledPos < firstScreen + screenLen) {
                leds[ledPos] = CatcherColors[dotsA[i].colorIdx];
                // Hit: i hjemmezonen ELLER i 2-pixel nådezon
                bool inCatch = (ledPos >= graceA && ledPos <= zoneA_End);
                if (pressedBtnA == dotsA[i].colorIdx && inCatch) {
                    dotsA[i].active = false; isHitA = true;
                    sendSoundOrBroadcast(JOY_A, CCHit_id, CCHit_vol);
                }
            }
        }
        if(dotsB[i].active) {
            dotsB[i].pos += speedFactor;
            int ledPos    = mid + (int)dotsB[i].pos;
            int missLineB = firstScreen + screenLen - 1 - missesB;
            int graceB    = missLineB + 2;           // 2 pixels nåde ind i rød

            if (ledPos > graceB) {
                dotsB[i].active = false; missesB += 2;
                sendSoundOrBroadcast(JOY_B, CCMiss_id, CCMiss_vol);
            } else if (ledPos >= 0) {
                leds[ledPos] = CatcherColors[dotsB[i].colorIdx];
                bool inCatch = (ledPos >= zoneB_End && ledPos <= graceB);
                if (pressedBtnB == dotsB[i].colorIdx && inCatch) {
                    dotsB[i].active = false; isHitB = true;
                    sendSoundOrBroadcast(JOY_B, CCHit_id, CCHit_vol);
                }
            }
        }
    }

    if (pressedBtnA != -1 && !isHitA) { missesA++; sendSoundOrBroadcast(JOY_A, CCMiss_id, CCMiss_vol); }
    if (pressedBtnB != -1 && !isHitB) { missesB++; sendSoundOrBroadcast(JOY_B, CCMiss_id, CCMiss_vol); }

    for(int i=0; i<4; i++) { sendLed(JOY_A, JOYSTICK_LED_MAP[i], dimColor(CatcherColors[i])); sendLed(JOY_B, JOYSTICK_LED_MAP[i], dimColor(CatcherColors[i])); }
    setScoreLeds(JOY_A, scoreGreen, CRGB::Green);  setScoreLeds(JOY_B, scoreRed, CRGB::Red);

    bool deadA = (missesA + CCatcherHomePix >= halfLen);
    bool deadB = (missesB + CCatcherHomePix >= halfLen);

    if (deadA || deadB) {
        // ── Eksplosions-animation + Miss-lyd på begge joystiks ─────────────
        // Tabende sides halve strip blinker hvidt → sort → hvidt (3 gange)
        sendSoundBoth(MissTune_id, MissTune_vol);
        for (int f = 0; f < 3; f++) {
            // Tænd: tabende halvdel lyser hvidt
            if (deadA) {
                for (int j = 0; j < halfLen; j++)
                    leds[firstScreen + j] = (f % 2 == 0) ? CRGB::White : CRGB(120, 0, 0);
            }
            if (deadB) {
                for (int j = 0; j < halfLen; j++)
                    leds[firstScreen + screenLen - 1 - j] = (f % 2 == 0) ? CRGB::White : CRGB(120, 0, 0);
            }
            showLeds(); delay(120);
            // Sluk
            if (deadA)  for (int j = 0; j < halfLen; j++) leds[firstScreen + j] = CRGB::Black;
            if (deadB)  for (int j = 0; j < halfLen; j++) leds[firstScreen + screenLen - 1 - j] = CRGB::Black;
            showLeds(); delay(80);
        }

        if (deadA && deadB) {
            // Begge løb tør – den med færreste misses vinder runden
            if (missesA > missesB) {
                // Grøn missede mest → Rød vinder runden
                scoreRed++;
                sendSoundOrBroadcast(JOY_B, CCHit_id, CCHit_vol);
            } else {
                // Rød missede mest → Grøn vinder runden
                scoreGreen++;
                sendSoundOrBroadcast(JOY_A, CCHit_id, CCHit_vol);
            }
            Serial.printf("Begge dead - GREEN: %d, RED: %d\n", scoreGreen, scoreRed);
        }
        else if (deadB) {
            // Rødt hold løb tør → Grønt hold scorer
            scoreGreen++;
            sendSoundOrBroadcast(JOY_A, CCHit_id, CCHit_vol);
            Serial.printf("Grønt hold scorer - GREEN: %d, RED: %d\n", scoreGreen, scoreRed);
        }
        else if (deadA) {
            // Grønt hold løb tør → Rødt hold scorer
            scoreRed++;
            sendSoundOrBroadcast(JOY_B, CCHit_id, CCHit_vol);
            Serial.printf("Rødt hold scorer - GREEN: %d, RED: %d\n", scoreGreen, scoreRed);
        }

        showLeds();
        if (scoreGreen >= 3 || scoreRed >= 3) {
            Serial.printf("VINDER - GREEN: %d, RED: %d\n", scoreGreen, scoreRed);
            gameManager.setWinnerColor((scoreGreen >= 3) ? CRGB::Green : CRGB::Red);
            gameManager.setState(3);
        } else {
            delay(3000); _resetRound();
        }
    }
    showLeds();
}

#endif
