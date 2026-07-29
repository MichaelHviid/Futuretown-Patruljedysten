#ifndef SPEED_BALL_H
#define SPEED_BALL_H

#include <FastLED.h>
#include "GameManager.h"
#include "hw_utils.h"

extern GameManager gameManager;
extern int scoreGreen;
extern int scoreRed;
extern const int JOYSTICK_LED_MAP[4];
extern const int JOYSTICK_SCORE_MAP[3];
extern byte BackBrightness;

extern int SB_BaseSpeed;
extern int SB_HomeSize;
extern int SB_WhiteGrow;

extern byte SBHit_id,   SBHit_vol;
extern byte SBMiss_id,  SBMiss_vol;
extern byte MissTune_id, MissTune_vol;   // Global miss-lyd til eksplosionsfejring

static float sb_ballPos = 0;
static float sb_ballVelocity = 0;
static int sb_activeBtnA = 0;
static int sb_activeBtnB = 0;
static int sb_prevActiveBtnA = -1;   // -1 = tvinger force-send ved første frame
static int sb_prevActiveBtnB = -1;
static int sb_ledRetryCount  =  5;   // Antal frames vi force-sender efter knap-skift
static unsigned long sb_roundStartTime = 0;
static bool sb_gameWaiting = true;

static int sb_bonusA = 0;
static int sb_bonusB = 0;

// Zone-straf ved returneringer
static int   sb_lastHitLevelA = 1;     // Niveau A sidst returnerede fra
static int   sb_lastHitLevelB = 1;     // Niveau B sidst returnerede fra
static float sb_lastReturnSpeedA = 0;  // Minimum hastighed for A's næste retur
static float sb_lastReturnSpeedB = 0;  // Minimum hastighed for B's næste retur

// Konverter multiplikator → zone-niveau (1=Grøn … 5=Hvid)
static int _sbLevel(float m) {
    if (m >= 10.0f) return 5;
    if (m >= 4.0f)  return 4;
    if (m >= 2.0f)  return 3;
    if (m >= 1.5f)  return 2;
    return 1;
}

// Straf-faktor baseret på forskel i zone-niveau
// diff=0-1: 100%  diff=2: 75%  diff=3: 50%  diff≥4: 25%
static float _sbPenalty(int incomingLevel, int outgoingLevel) {
    int diff = incomingLevel - outgoingLevel;
    if (diff <= 1) return 1.0f;
    return max(0.25f, 1.0f - (float)(diff - 1) * 0.25f);
}

static bool sb_btnPressedA[4] = {false, false, false, false};
static bool sb_btnPressedB[4] = {false, false, false, false};

CRGB _sbDim(CRGB color) {
    uint8_t dim = map(BackBrightness, 0, 100, 0, 255);
    return color.nscale8_video(dim);
}

void _resetSpeedBallRound(int screenLen) {
    sb_ballPos = 0;
    sb_ballVelocity = 0;
    sb_activeBtnA = random(0, 4);
    sb_activeBtnB = random(0, 4);
    sb_prevActiveBtnA = sb_prevActiveBtnB = -1;
    sb_ledRetryCount  = 5;
    sb_lastHitLevelA  = sb_lastHitLevelB = 1;
    sb_lastReturnSpeedA = sb_lastReturnSpeedB = 0.0f;
    sb_gameWaiting = true;
    sb_bonusA = 0;
    sb_bonusB = 0;
    for(int i=0; i<4; i++) { sb_btnPressedA[i] = false; sb_btnPressedB[i] = false; }
    sb_roundStartTime = millis();
}

void initSpeedBall(int screenLen) {
    scoreGreen = 0; scoreRed = 0;
    _resetSpeedBallRound(screenLen);
}

void runSpeedBall(CRGB* leds, int screenLen, int firstScreen, int* pinsA, int* pinsB) {
    unsigned long now = millis();
    unsigned long elapsed = now - sb_roundStartTime;

    float f = (float)elapsed / (SB_WhiteGrow * 1000.0);
    if (f > 1.0) f = 1.0;

    int half = screenLen / 2;
    int endA = firstScreen;
    int endB = firstScreen + screenLen - 1;
    int mid = firstScreen + half;

    auto calculateZones = [&](int bonus, int& zW, int& zB, int& zR, int& zY, int& zG) {
        float pG = 0.15 + (f * 0.25);
        float pY = 0.07 + (f * 0.13);
        float pR = 0.03 + (f * 0.07);
        float pB = 0.00 + (f * 0.30);
        float currentHomePct = (SB_HomeSize / 100.0) + (f * (1.0 - (SB_HomeSize / 100.0)));
        int totalHomePixels = half * currentHomePct;
        int available = totalHomePixels - bonus;
        if (available < 0) available = 0;
        zW = bonus;
        float sumP = pG + pY + pR + pB;
        zB = available * (pB / sumP);
        zR = available * (pR / sumP);
        zY = available * (pY / sumP);
        zG = available - zB - zR - zY;
    };

    int wA, bA, rA, yA, gA;
    calculateZones(sb_bonusA, wA, bA, rA, yA, gA);
    int wB, bB, rB, yB, gB;
    calculateZones(sb_bonusB, wB, bB, rB, yB, gB);

    FastLED.clear();
    auto drawHalf = [&](int startIdx, int dir, int w, int b, int r, int y, int g) {
        for (int i = 0; i < half; i++) {
            CRGB col = CRGB::Black;
            if (i < w) col = CRGB::White;
            else if (i < w + b) col = CRGB::Blue;
            else if (i < w + b + r) col = CRGB::Red;
            else if (i < w + b + r + y) col = CRGB(255, 100, 0);
            else if (i < w + b + r + y + g) col = CRGB::Green;
            leds[startIdx + (i * dir)] = _sbDim(col);
        }
    };
    drawHalf(endA, 1, wA, bA, rA, yA, gA);
    drawHalf(endB, -1, wB, bB, rB, yB, gB);

    // Force-send alle 4 knapper i 5 frames efter knap-skift (retry mod pakketab).
    // Ingen periodisk refresh – undgår unødigt netværksstøj med 30 joystiks.
    bool btnChanged = (sb_activeBtnA != sb_prevActiveBtnA) ||
                      (sb_activeBtnB != sb_prevActiveBtnB);
    if (btnChanged) sb_ledRetryCount = 5;   // Nulstil retry-tæller ved skift

    bool doForce = (sb_ledRetryCount > 0);
    if (doForce) sb_ledRetryCount--;

    for(int i=0; i<4; i++) {
        CRGB colA = (i == sb_activeBtnA) ? CRGB::White : CRGB::Black;
        CRGB colB = (i == sb_activeBtnB) ? CRGB::White : CRGB::Black;
        if (doForce) { forceSendLed(JOY_A, JOYSTICK_LED_MAP[i], colA);
                       forceSendLed(JOY_B, JOYSTICK_LED_MAP[i], colB); }
        else         { sendLed(JOY_A, JOYSTICK_LED_MAP[i], colA);
                       sendLed(JOY_B, JOYSTICK_LED_MAP[i], colB); }
    }
    sb_prevActiveBtnA = sb_activeBtnA;
    sb_prevActiveBtnB = sb_activeBtnB;
    setScoreLeds(JOY_A, scoreGreen, _sbDim(CRGB::Green));
    setScoreLeds(JOY_B, scoreRed,   _sbDim(CRGB::Red));

    float startVel = (float)screenLen / (SB_BaseSpeed / 16.0);
    if (!sb_gameWaiting) sb_ballPos += sb_ballVelocity;
    int ballIdx = mid + (int)sb_ballPos;

    if (ballIdx >= firstScreen && ballIdx < firstScreen + screenLen) {
        leds[ballIdx] = CRGB::White;
    }

    auto getMult = [&](int dist, int w, int b, int r, int y, int g) -> float {
        if (dist < w) return 10.0;
        if (dist < w + b) return 4.0;
        if (dist < w + b + r) return 2.0;
        if (dist < w + b + r + y) return 1.5;
        if (dist < w + b + r + y + g) return 1.0;
        return -1.0;
    };

    // --- INPUT LOGIK – brug joyRead i stedet for digitalRead ---
    for(int i=0; i<4; i++) {
        // Team A
        if (joyRead(pinsA[i]) == LOW) {
            if (!sb_btnPressedA[i]) {
                sb_btnPressedA[i] = true;
                if (i == sb_activeBtnA) {
                    if (sb_gameWaiting) {
                        sb_gameWaiting = false;
                        sb_ballVelocity = startVel;
                        sb_ballPos = 0;
                        sendSoundOrBroadcast(JOY_A, SBHit_id, SBHit_vol);
                    } else if (ballIdx >= firstScreen && ballIdx < mid) {
                        float m = getMult(ballIdx - endA, wA, bA, rA, yA, gA);
                        if (m > 0) {
                            float penalty = _sbPenalty(sb_lastHitLevelB, _sbLevel(m));
                            float spd = abs(sb_ballVelocity) * m * penalty;
                            spd = max(spd, sb_lastReturnSpeedA);  // aldrig langsommere end dit eget minimum
                            if (spd > 4.5f) spd = 4.5f;
                            sb_ballVelocity = spd;
                            sb_lastHitLevelA    = _sbLevel(m);
                            sb_lastReturnSpeedA = spd;
                            sb_activeBtnA = (sb_activeBtnA + 1) % 4;
                            if (m == 10.0) sb_bonusA = 0;
                            sendSoundOrBroadcast(JOY_A, SBHit_id, SBHit_vol);
                        } else { sb_bonusB++; sendSoundOrBroadcast(JOY_A, SBMiss_id, SBMiss_vol); }
                    }
                } else { sb_bonusB++; sendSoundOrBroadcast(JOY_A, SBMiss_id, SBMiss_vol); }
            }
        } else sb_btnPressedA[i] = false;

        // Team B
        if (joyRead(pinsB[i]) == LOW) {
            if (!sb_btnPressedB[i]) {
                sb_btnPressedB[i] = true;
                if (i == sb_activeBtnB) {
                    if (sb_gameWaiting) {
                        sb_gameWaiting = false;
                        sb_ballVelocity = -startVel;
                        sb_ballPos = 0;
                        sendSoundOrBroadcast(JOY_B, SBHit_id, SBHit_vol);
                    } else if (ballIdx <= endB && ballIdx >= mid) {
                        float m = getMult(endB - ballIdx, wB, bB, rB, yB, gB);
                        if (m > 0) {
                            float penalty = _sbPenalty(sb_lastHitLevelA, _sbLevel(m));
                            float spd = abs(sb_ballVelocity) * m * penalty;
                            spd = max(spd, sb_lastReturnSpeedB);
                            if (spd > 4.5f) spd = 4.5f;
                            sb_ballVelocity = -spd;
                            sb_lastHitLevelB    = _sbLevel(m);
                            sb_lastReturnSpeedB = spd;
                            sb_activeBtnB = (sb_activeBtnB + 1) % 4;
                            if (m == 10.0) sb_bonusB = 0;
                            sendSoundOrBroadcast(JOY_B, SBHit_id, SBHit_vol);
                        } else { sb_bonusA++; sendSoundOrBroadcast(JOY_B, SBMiss_id, SBMiss_vol); }
                    }
                } else { sb_bonusA++; sendSoundOrBroadcast(JOY_B, SBMiss_id, SBMiss_vol); }
            }
        } else sb_btnPressedB[i] = false;
    }

    // --- MÅL-TJEK ---
    if (!sb_gameWaiting) {
        if (ballIdx < firstScreen || ballIdx > endB) {
            bool goalA = (ballIdx < firstScreen);  // goalA=true → A's mål → B (rød) scorer

            if (goalA) scoreRed++; else scoreGreen++;

            // Global Miss-lyd til begge joystiks
            sendSoundBoth(MissTune_id, MissTune_vol);

            // Eksplosion på tabende sides halvdel (3 blink)
            // goalA=true → A (grønt) tabte → kode-venstre = fysisk grøn ende blinker
            // goalA=false → B (rødt) tabte → kode-højre = fysisk rød ende blinker
            for (int f = 0; f < 3; f++) {
                CRGB flashCol = (f % 2 == 0) ? CRGB::White : CRGB(140, 80, 0);
                if (goalA) {
                    for (int j = 0; j < screenLen / 2; j++) leds[firstScreen + j] = flashCol;
                } else {
                    for (int j = 0; j < screenLen / 2; j++) leds[firstScreen + screenLen - 1 - j] = flashCol;
                }
                showLeds(); delay(120);
                if (goalA) for (int j=0; j<screenLen/2; j++) leds[firstScreen+j] = CRGB::Black;
                else       for (int j=0; j<screenLen/2; j++) leds[firstScreen+screenLen-1-j] = CRGB::Black;
                showLeds(); delay(80);
            }

            // Hit-lyd til vinderen
            sendSoundOrBroadcast(goalA ? JOY_B : JOY_A, SBHit_id, SBHit_vol);

            drawHalf(endA, 1, wA, bA, rA, yA, gA);
            drawHalf(endB, -1, wB, bB, rB, yB, gB);
            setScoreLeds(JOY_A, scoreGreen, _sbDim(CRGB::Green));
            setScoreLeds(JOY_B, scoreRed,   _sbDim(CRGB::Red));
            showLeds();
            delay(500);
            _resetSpeedBallRound(screenLen);
        }
    }

    if (scoreGreen >= 3 || scoreRed >= 3) {
        gameManager.setWinnerColor(scoreGreen >= 3 ? CRGB::Green : CRGB::Red);
        FastLED.clear();
        showLeds();
        gameManager.setState(3);
        return;
    }

    showLeds();
}

#endif
