#ifndef TUG_OF_WAR_H
#define TUG_OF_WAR_H

#include <FastLED.h>
#include "GameManager.h"
#include "hw_utils.h"

extern GameManager gameManager;
extern int scoreGreen;
extern int scoreRed;

// Eksterne variable fra config
extern float TugPullStrength;
extern byte TugEnergyCost;
extern int TugRestorationTime;
extern int TugPointToWin;
extern const int JOYSTICK_LED_MAP[4];
extern const int JOYSTICK_SCORE_MAP[3];

static float tugRopePos;
static float tugPlayerEnergy[8];
static bool lastTugBtnState[8];
static unsigned long lastTugTapTime[8];
static unsigned long stunEndTimeA = 0;
static unsigned long stunEndTimeB = 0;
static bool tugIsInitializing = true;
static unsigned long lastTugUpdateMicros = 0;
static int _tugInitFrames = 0;  // >0: force-send LED de første N frames (robust mod pakketab)

const unsigned long STUN_DURATION = 1000;

// --- Hjælpefunktioner ---
static CRGB _getTugEnergyColor(float energy) {
    if (energy > 50.0f) return CRGB(map((int)energy, 100, 50, 0, 255), 255, 0);
    return CRGB(255, map((int)energy, 50, 0, 255, 0), 0);
}

void resetTugRope(int screenLen) {
    tugRopePos = (float)screenLen / 2.0f;
    tugIsInitializing = true;
    for(int i=0; i<8; i++) {
        tugPlayerEnergy[i] = 100.0f;
    }
    stunEndTimeA = 0; stunEndTimeB = 0;
    lastTugUpdateMicros = micros();
}

void initTugOfWar(int screenLen) {
    scoreGreen = 0;
    scoreRed = 0;
    _tugInitFrames = 5;  // Force-send de første 5 frames (robust mod pakketab)
    resetTugRope(screenLen);
}

void runTugOfWar(CRGB* leds, int screenLen, int firstScreen, int* pinsA, int* pinsB) {
    unsigned long now = millis();
    unsigned long currentMicros = micros();

    // Beregn Delta Time for jævn energi-genopladning
    if (lastTugUpdateMicros == 0) lastTugUpdateMicros = currentMicros;
    float deltaTime = (currentMicros - lastTugUpdateMicros) / 1000000.0f;
    lastTugUpdateMicros = currentMicros;

    float recoveryPerSecond = 100.0f / ((float)TugRestorationTime / 1000.0f);
    int endZoneLen = max(1, (int)(screenLen * 0.05));

    FastLED.clear();

    // 1. End-zoner – grøn ved grønt joystik, rød ved rødt joystik
    //    (showLeds() vender strimmelen: kode-venstre = fysisk grøn ende)
    for(int i=0; i<endZoneLen; i++) {
        leds[firstScreen + i]                  = CRGB(0, 40, 0);  // Grøn side
        leds[firstScreen + screenLen - 1 - i]  = CRGB(40, 0, 0);  // Rød side
    }

    // 2. LILLA MIDTER-ZONE (2 pixels)
    int midPoint = firstScreen + (screenLen / 2);
    leds[midPoint - 1] = CRGB::Purple;
    leds[midPoint] = CRGB::Purple;

    // 3. Input, Energi og Score
    float pullA = 0, pullB = 0;
    int defendersA = 0, defendersB = 0;

    // LED-opdatering: maks 10 FPS, ELLER force de første 5 frames efter init
    static unsigned long lastTugLedTime = 0;
    bool forceUpdate = (_tugInitFrames > 0);
    bool doLedUpdate = forceUpdate || (now - lastTugLedTime >= 100);
    if (doLedUpdate) { lastTugLedTime = now; if (forceUpdate) _tugInitFrames--; }

    for (int i = 0; i < 8; i++) {
        byte joyId = (i < 4) ? JOY_A : JOY_B;
        // Brug joyRead i stedet for digitalRead
        bool isPressed = (joyRead((i < 4) ? pinsA[i] : pinsB[i-4]) == LOW);

        // Energi genopfyldning (Tidsbaseret)
        float currentRecovery = isPressed ? (recoveryPerSecond * 1.2f) : recoveryPerSecond;
        tugPlayerEnergy[i] = constrain(tugPlayerEnergy[i] + (currentRecovery * deltaTime), 0.0f, 100.0f);

        if (isPressed) { if (i < 4) defendersA++; else defendersB++; }
        bool isStunned = (i < 4) ? (now < stunEndTimeA) : (now < stunEndTimeB);

        // Træk-logik
        if (!isStunned && isPressed && !lastTugBtnState[i] && tugPlayerEnergy[i] >= (float)TugEnergyCost) {
            tugPlayerEnergy[i] -= (float)TugEnergyCost;
            lastTugTapTime[i] = now;
            if (i < 4) pullA += TugPullStrength; else pullB += TugPullStrength;
        }
        lastTugBtnState[i] = isPressed;

        // Joystik-LED feedback (throttlet til 10 FPS)
        if (doLedUpdate) {
            byte pIdx = JOYSTICK_LED_MAP[i % 4];
            CRGB ledCol = isStunned && (now / 100 % 2 == 0)
                          ? CRGB::White
                          : _getTugEnergyColor(tugPlayerEnergy[i]);
            if (forceUpdate) forceSendLed(joyId, pIdx, ledCol);
            else             sendLed(joyId, pIdx, ledCol);

            // Score-LEDs: hvert joystik viser SIN EGEN score i SIN EGEN farve
            if (i == 0) setScoreLeds(JOY_A, scoreGreen, CRGB::Green);  // Grønt joystik → grøn score
            if (i == 4) setScoreLeds(JOY_B, scoreRed,   CRGB::Red);    // Rødt joystik  → rød score
        }
    }

    // --- 4. COMBO TJEK (One-shot trigger) ---
    bool comboA = true; for(int i=0; i<4; i++) if(now - lastTugTapTime[i] > 300) comboA = false;
    bool comboB = true; for(int i=4; i<8; i++) if(now - lastTugTapTime[i] > 300) comboB = false;

    // Team A Combo – stun Team B (ingen lyd)
    if(comboA && (now >= stunEndTimeB) && (now > stunEndTimeA + 500)) {
        pullA += (TugPullStrength * 10.0f);
        stunEndTimeB = now + STUN_DURATION;
    }

    // Team B Combo – stun Team A (ingen lyd)
    if(comboB && (now >= stunEndTimeA) && (now > stunEndTimeB + 500)) {
        pullB += (TugPullStrength * 10.0f);
        stunEndTimeA = now + STUN_DURATION;
    }

    // --- 5. TOV-BEVÆGELSE ---
    tugRopePos += ((now < stunEndTimeB ? 0 : pullB * (1.0f - defendersA * 0.15f)) -
                   (now < stunEndTimeA ? 0 : pullA * (1.0f - defendersB * 0.15f)));

    tugRopePos = constrain(tugRopePos, 0, screenLen - 1);

    // Tegn tovet (Hvidt, 4 pixels bredt)
    int rPos = (int)round(tugRopePos);
    for(int i = -2; i <= 1; i++) {
        if (rPos + i >= 0 && rPos + i < screenLen) {
            leds[firstScreen + rPos + i] = CRGB::White;
        }
    }

    // --- 6. SEJRSTJEK ---
    if (!tugIsInitializing) {
        bool redWonRound = false;
        bool greenWonRound = false;

        // code-left (lav indeks) = fysisk grøn ende  → GRØN trak rød over = GRØN vinder
        // code-right (høj indeks) = fysisk rød ende  → RØD trak grøn over = RØD vinder
        if (tugRopePos <= (float)endZoneLen) {
            scoreGreen++;
            greenWonRound = true;
        }
        else if (tugRopePos >= (float)(screenLen - endZoneLen - 1)) {
            scoreRed++;
            redWonRound = true;
        }

        if (redWonRound || greenWonRound) {
            CRGB winCol = redWonRound ? CRGB::Red : CRGB::Green;

            // Visuelt blink (3 gange, ingen lyd)
            for(int b = 0; b < 3; b++) {
                int rPos2 = (int)round(tugRopePos);
                for(int i = -2; i <= 1; i++) {
                    if (rPos2 + i >= 0 && rPos2 + i < screenLen) {
                        leds[firstScreen + rPos2 + i] = winCol;
                    }
                }
                showLeds();
                delay(100);
                for(int i = -2; i <= 1; i++) {
                    if (rPos2 + i >= 0 && rPos2 + i < screenLen) {
                        leds[firstScreen + rPos2 + i] = CRGB::Black;
                    }
                }
                showLeds();
                delay(100);
            }

            if (scoreGreen >= TugPointToWin || scoreRed >= TugPointToWin) {
                gameManager.setWinnerColor(scoreGreen >= TugPointToWin ? CRGB::Green : CRGB::Red);
                gameManager.setState(3);
            } else {
                resetTugRope(screenLen);
            }
        }
    } else {
        if (abs(tugRopePos - (screenLen / 2.0f)) > 2.0f) tugIsInitializing = false;
    }

    showLeds();
}

#endif
