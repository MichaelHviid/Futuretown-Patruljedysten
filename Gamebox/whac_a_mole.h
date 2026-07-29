#ifndef WHAC_A_MOLE_H
#define WHAC_A_MOLE_H

#include <FastLED.h>
#include "GameManager.h"
#include "hw_utils.h"

extern GameManager gameManager;
extern int scoreGreen;
extern int scoreRed;
extern const int JOYSTICK_LED_MAP[4];
extern const int JOYSTICK_SCORE_MAP[3];
extern byte BackBrightness;

extern int WAM_MaxP;
extern int WAM_Pts;
extern int WAM_Bonus;

extern byte WAMHit_id,   WAMHit_vol;
extern byte WAMMiss_id,  WAMMiss_vol;
extern byte MissTune_id, MissTune_vol;   // Global miss-lyd til eksplosionsfejring

static int wam_activeBtn    = -1;
static int wam_prevActiveBtn = -1;   // Tracker knap-skift → force-send til rydning
static int wam_ledRetry     =  0;   // Frames der force-sendes efter skift
static int wam_activeColIdx = -1;
static unsigned long wam_nextSpawn = 0;
static unsigned long wam_spawnTime = 0;
static bool wam_active = false;

static int wam_scoreA = 0;
static int wam_scoreB = 0;
static unsigned long wam_lockA = 0;
static unsigned long wam_lockB = 0;
static int wam_lastErrorBtnA = -1;
static int wam_lastErrorBtnB = -1;

static unsigned long lastInputA_WAM = 0;
static unsigned long lastInputB_WAM = 0;
const int inputCooldown_WAM = 150;

const CRGB wamColors[] = {CRGB::Blue, CRGB::Green, CRGB(255, 100, 0), CRGB::Red};
const char* wamColorNames[] = {"Blå", "Grøn", "Orange", "Rød"};

CRGB dimWam(CRGB color) {
    uint8_t dim = map(BackBrightness, 0, 100, 0, 255);
    return color.nscale8_video(dim);
}

void _resetWam() {
    wam_scoreA = 0; wam_scoreB = 0;
    wam_active = false;
    wam_activeBtn = -1;
    wam_prevActiveBtn = -1;
    wam_ledRetry = 5;   // Force-ryd alle LEDs ved ny runde
    wam_nextSpawn = millis() + 1500;
    wam_lockA = 0; wam_lockB = 0;
    wam_lastErrorBtnA = -1; wam_lastErrorBtnB = -1;
}

void initWhacAMole(int screenLen) {
    scoreGreen = 0; scoreRed = 0;
    _resetWam();
}

void runWhacAMole(CRGB* leds, int screenLen, int firstScreen, int* pinsA, int* pinsB) {
    unsigned long now = millis();
    FastLED.clear();

    int mid = firstScreen + (screenLen / 2);
    float pX = (float)(screenLen / 2) / (float)WAM_Pts;

    // 1. TEGN POINT-STRIPS – vokser fra enderne mod midten
    // Grønt hold (A) → grøn streg fra kode-venstre ende (fysisk grøn joystik) mod midten
    // Rødt hold (B)  → rød streg fra kode-højre ende (fysisk rød joystik) mod midten
    {
        int barA = (int)(wam_scoreA * pX);
        int barB = (int)(wam_scoreB * pX);
        for(int i=0; i < barA; i++) { int p = firstScreen + i;           if (p < mid)                   leds[p] = CRGB::Green; }
        for(int i=0; i < barB; i++) { int p = firstScreen+screenLen-1-i; if (p >= mid)                  leds[p] = CRGB::Red;   }
    }
    // Hvide midter-markering (tegnes sidst – overskrives ikke af bars)
    leds[mid-1] = CRGB::White;
    leds[mid]   = CRGB::White;

    // 2. SPAWN LOGIK
    if (!wam_active && now > wam_nextSpawn) {
        wam_activeBtn = random(0, 4);
        wam_activeColIdx = random(0, 4);
        wam_spawnTime = now;
        wam_active = true;
    }

    // 3. VIS PÅ JOYSTICKS – med retry-mekanisme mod tabte pakker
    bool wam_btnChanged = (wam_activeBtn != wam_prevActiveBtn);
    if (wam_btnChanged) wam_ledRetry = 5;   // 5 frames force-send ved knap-skift
    bool doForce = (wam_ledRetry > 0);
    if (doForce) wam_ledRetry--;

    for (int i = 0; i < 4; i++) {
        CRGB col = (wam_active && i == wam_activeBtn)
                   ? wamColors[wam_activeColIdx] : CRGB::Black;
        if (doForce) { forceSendLed(JOY_A, JOYSTICK_LED_MAP[i], col);
                       forceSendLed(JOY_B, JOYSTICK_LED_MAP[i], col); }
        else         { sendLed(JOY_A, JOYSTICK_LED_MAP[i], col);
                       sendLed(JOY_B, JOYSTICK_LED_MAP[i], col); }
    }
    wam_prevActiveBtn = wam_activeBtn;

    setScoreLeds(JOY_A, scoreGreen, CRGB::Green);
    setScoreLeds(JOY_B, scoreRed,   CRGB::Red);

    // --- 4. INPUT HÅNDTERING – brug joyRead i stedet for digitalRead ---
    bool hitRegistered = false;
    int flashGreenA = -1; int flashGreenB = -1;

    // Tjek Rød side (A)
    for(int i=0; i<4; i++) {
        bool isDown = (joyRead(pinsA[i]) == LOW);

        if(isDown && (now - lastInputA_WAM > inputCooldown_WAM)) {
            lastInputA_WAM = now;

            if (now < wam_lockA) continue;

            if (wam_active && i == wam_activeColIdx) {
                int p = (WAM_Bonus > 0 && (now - wam_spawnTime < 500) && random(0, 101) <= WAM_Bonus) ? 2 : 1;
                wam_scoreA += p;
                sendSoundOrBroadcast(JOY_A, WAMHit_id, WAMHit_vol);
                flashGreenA = i;
                hitRegistered = true;
            } else if (wam_active) {
                wam_lockA = now + 1000;
                wam_lastErrorBtnA = i;
                sendSoundOrBroadcast(JOY_A, WAMMiss_id, WAMMiss_vol);
            }
        }

        if (isDown && now < wam_lockA && i == wam_lastErrorBtnA) {
            sendLed(JOY_A, JOYSTICK_LED_MAP[i], CRGB::Red);
        }
    }

    // Tjek Grøn side (B)
    for(int i=0; i<4; i++) {
        bool isDown = (joyRead(pinsB[i]) == LOW);

        if(isDown && (now - lastInputB_WAM > inputCooldown_WAM)) {
            lastInputB_WAM = now;

            if (now < wam_lockB) continue;

            if (wam_active && i == wam_activeColIdx) {
                int p = (WAM_Bonus > 0 && (now - wam_spawnTime < 500) && random(0, 101) <= WAM_Bonus) ? 2 : 1;
                wam_scoreB += p;
                sendSoundOrBroadcast(JOY_B, WAMHit_id, WAMHit_vol);
                flashGreenB = i;
                hitRegistered = true;
            } else if (wam_active) {
                wam_lockB = now + 1000;
                wam_lastErrorBtnB = i;
                sendSoundOrBroadcast(JOY_B, WAMMiss_id, WAMMiss_vol);
            }
        }

        if (isDown && now < wam_lockB && i == wam_lastErrorBtnB) {
            sendLed(JOY_B, JOYSTICK_LED_MAP[i], CRGB::Red);
        }
    }

    // --- 5. FEEDBACK EFFEKTER ---
    if (flashGreenA != -1) sendLed(JOY_A, JOYSTICK_LED_MAP[flashGreenA], CRGB::Green);
    if (flashGreenB != -1) sendLed(JOY_B, JOYSTICK_LED_MAP[flashGreenB], CRGB::Green);

    if (hitRegistered) {
        // Sluk aktiv knap øjeblikkeligt (force, ikke state-tracked)
        // og trigger 5 retries mod pakketab
        forceSendLed(JOY_A, JOYSTICK_LED_MAP[wam_activeBtn], CRGB::Black);
        forceSendLed(JOY_B, JOYSTICK_LED_MAP[wam_activeBtn], CRGB::Black);
        wam_active = false;
        wam_ledRetry = 5;
        wam_nextSpawn = now + random(200, WAM_MaxP + 1);
        showLeds();
    }

    // Straf indikator (Hvid ved slip) – brug joyRead til at tjekke om sluppet
    if (now < wam_lockA && wam_lastErrorBtnA != -1) {
        if (joyRead(pinsA[wam_lastErrorBtnA]) == HIGH) {
            sendLed(JOY_A, JOYSTICK_LED_MAP[wam_lastErrorBtnA], CRGB::White);
        }
    }
    if (now < wam_lockB && wam_lastErrorBtnB != -1) {
        if (joyRead(pinsB[wam_lastErrorBtnB]) == HIGH) {
            sendLed(JOY_B, JOYSTICK_LED_MAP[wam_lastErrorBtnB], CRGB::White);
        }
    }

    // --- 6. RUNDE AFSLUTNING ---
    if (wam_scoreA >= WAM_Pts || wam_scoreB >= WAM_Pts) {
        bool teamAWon = (wam_scoreA >= WAM_Pts);

        // 1. Sluk joystik-LEDs øjeblikkeligt
        clearAllJoyLeds();

        // 2. Ryd LED-strip og vis
        FastLED.clear(); showLeds();
        delay(100);

        // 3. Tildel point
        if (teamAWon) scoreGreen++; else scoreRed++;

        // 4. Point-lyd
        sendSoundBoth(MissTune_id, MissTune_vol);
        delay(40);
        sendSoundOrBroadcast(teamAWon ? JOY_A : JOY_B, WAMHit_id, WAMHit_vol);

        // Score-LEDs: force-batch (3 LEDs i én pakke per joystik – ingen delays nødvendige)
        forceSetScoreLeds(JOY_A, scoreGreen, CRGB::Green);
        forceSetScoreLeds(JOY_B, scoreRed,   CRGB::Red);

        // 5. Animation: 2 hvide prikker flyver fra enderne ind mod midten (2 sek)
        int steps = screenLen / 2;
        int stepMs = 2000 / (steps > 0 ? steps : 1);
        float pXf = (float)(screenLen / 2) / (float)(WAM_Pts > 0 ? WAM_Pts : 1);
        for (int s = 0; s <= steps; s++) {
            FastLED.clear();
            // Sub-runde score-bars (forsvinder når prikkerne mødes)
            int barA = (int)(wam_scoreA * pXf);
            int barB = (int)(wam_scoreB * pXf);
            for(int i=0;i<barA;i++) { int p=firstScreen+i;          if(p<mid)  leds[p]=CRGB::Green; }
            for(int i=0;i<barB;i++) { int p=firstScreen+screenLen-1-i; if(p>=mid) leds[p]=CRGB::Red;   }
            // Hvide prikker bevæger sig fra enderne mod centrum
            leds[firstScreen + s]                 = CRGB::White;
            leds[firstScreen + screenLen - 1 - s] = CRGB::White;
            leds[mid-1] = CRGB::White;
            leds[mid]   = CRGB::White;
            showLeds();
            delay(stepMs);
        }

        if (scoreGreen >= 3 || scoreRed >= 3) {
            gameManager.setWinnerColor(scoreGreen >= 3 ? CRGB::Green : CRGB::Red);
            gameManager.setState(3);
        } else {
            _resetWam();
        }
    }
    showLeds();
}

#endif
