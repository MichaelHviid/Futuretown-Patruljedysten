#ifndef SIMON_DUEL_H
#define SIMON_DUEL_H

#include <FastLED.h>
#include "GameManager.h"
#include "hw_utils.h"

extern GameManager gameManager;
extern int scoreGreen;
extern int scoreRed;
extern const int JOYSTICK_LED_MAP[4];
extern const int JOYSTICK_SCORE_MAP[3];

extern int SimonBlinkTime;
extern int SimonPauseTime;
extern int SimonSpeedCount;

// Simon farvelyde – fra config_manager.h
extern byte SimonBlue_id,   SimonBlue_vol;
extern byte SimonGreen_id,  SimonGreen_vol;
extern byte SimonYellow_id, SimonYellow_vol;
extern byte SimonRed_id,    SimonRed_vol;
extern byte SimonHit_id,    SimonHit_vol;
extern byte SimonMiss_id,   SimonMiss_vol;

#define MAX_SEQUENCE 50
const CRGB SimonColors[] = {CRGB::Blue, CRGB::Green, CRGB(255, 100, 0), CRGB::Red};

// ─── Farvelyde ────────────────────────────────────────────────────────────────
// Farvelyd til BEGGE joystiks (bruges under sekvensvisning)
static inline void _simonPlayColor(int colorIdx) {
    switch (colorIdx) {
        case 0: sendSoundBoth(SimonBlue_id,   SimonBlue_vol);   break;
        case 1: sendSoundBoth(SimonGreen_id,  SimonGreen_vol);  break;
        case 2: sendSoundBoth(SimonYellow_id, SimonYellow_vol); break;
        case 3: sendSoundBoth(SimonRed_id,    SimonRed_vol);    break;
    }
}

// Farvelyd til ÉT joystik (bruges når spiller trykker på en knap)
static inline void _simonPlayColorJoy(int colorIdx, byte joyId) {
    switch (colorIdx) {
        case 0: sendSoundOrBroadcast(joyId, SimonBlue_id,   SimonBlue_vol);   break;
        case 1: sendSoundOrBroadcast(joyId, SimonGreen_id,  SimonGreen_vol);  break;
        case 2: sendSoundOrBroadcast(joyId, SimonYellow_id, SimonYellow_vol); break;
        case 3: sendSoundOrBroadcast(joyId, SimonRed_id,    SimonRed_vol);    break;
    }
}

enum SimonState { SIMON_GENERATE, SIMON_SHOW, SIMON_INPUT, SIMON_CHECK_ROUND, SIMON_PAUSE };
static SimonState simonState = SIMON_GENERATE;

static int sequence[MAX_SEQUENCE];
static int currentLevel = 0;
static int playerStepA = 0;
static int playerStepB = 0;
static bool failedA = false;
static bool failedB = false;
static unsigned long nextActionTime = 0;
static int showIdx = 0;
static bool isShowingColor = false;

static bool _simonBtnStateA[4] = {false, false, false, false};
static bool _simonBtnStateB[4] = {false, false, false, false};

// Force-send tæller: sikrer LED-opdatering selv ved pakketab ved spilstart
static int _simonInitFrames = 0;

// ─── Log-hjælpere (placeret efter variabel-deklarationer) ─────────────────────
static const char* _simonColorName(int idx) {
    switch(idx) { case 0: return "Blaa"; case 1: return "Groen"; case 2: return "Gul"; case 3: return "Roed"; }
    return "?";
}
static void _simonPrintSeq() {
    Serial.printf("[SIMON] Niveau %d  (%d farver):", currentLevel, currentLevel + 1);
    for(int i = 0; i <= currentLevel; i++) Serial.printf("  %d:%s", i + 1, _simonColorName(sequence[i]));
    Serial.println();
}

void _updateScoreLeds() {
    // scoreRed = grønt holds point (JOY_A), scoreGreen = rødt holds point (JOY_B)
    setScoreLeds(JOY_A, scoreRed,   CRGB::Green);  // Grønt joystik → grønne point
    setScoreLeds(JOY_B, scoreGreen, CRGB::Red);    // Rødt joystik  → røde point
}

void _setJoystickColors() {
    bool force = (_simonInitFrames > 0);
    if (force) _simonInitFrames--;
    // Byg batch med alle 4 knap-farver – 1 pakke i stedet for 4
    EspNowLedBatch b;
    b.count = 4;
    for(int i = 0; i < 4; i++)
        b.leds[i] = {(byte)JOYSTICK_LED_MAP[i], SimonColors[i].r, SimonColors[i].g, SimonColors[i].b};
    if (force) { forceSendLedBatch(JOY_A, b); forceSendLedBatch(JOY_B, b); }
    else        { sendLedBatch(JOY_A, b);      sendLedBatch(JOY_B, b); }
    _updateScoreLeds();
}

void initSimonDuel(int screenLen) {
    _simonInitFrames = 5;  // Force-send de første 5 frames → robust mod pakketab ved WiFi-disconnect
    simonState = SIMON_GENERATE;
    currentLevel = 0;
    scoreGreen = 0; scoreRed = 0;
    randomSeed(analogRead(0) + millis());
    sequence[0] = random(0, 4);
    failedA = false; failedB = false;
}

void runSimonDuel(CRGB* leds, int screenLen, int firstScreen, int* pinsA, int* pinsB) {
    unsigned long now = millis();
    FastLED.clear();
    _setJoystickColors();

    switch (simonState) {
        case SIMON_GENERATE:
            if (currentLevel == 0) sequence[0] = random(0, 4);
            else if (currentLevel < MAX_SEQUENCE - 1) sequence[currentLevel] = random(0, 4);
            showIdx = 0;
            isShowingColor = false;
            nextActionTime = now + 800;
            simonState = SIMON_SHOW;
            Serial.printf("[SIMON] ------------------------------------------\n");
            _simonPrintSeq();
            break;

        case SIMON_SHOW:
            if (now > nextActionTime) {
                if (showIdx <= currentLevel) {
                    if (!isShowingColor) {
                        isShowingColor = true;
                        int currentBlink = SimonBlinkTime;
                        int currentPause = SimonPauseTime;

                        if (showIdx < currentLevel && SimonSpeedCount > 0) {
                            int speedMultiplier = currentLevel / SimonSpeedCount;
                            for(int m=0; m < speedMultiplier; m++) {
                                currentBlink = currentBlink * 0.8;
                                currentPause = currentPause * 0.8;
                            }
                        }

                        // Tegn farven på midten af skærmen
                        int mid = firstScreen + (screenLen / 2);
                        for(int i=-15; i<15; i++) leds[mid + i] = SimonColors[sequence[showIdx]];
                        showLeds();
                        Serial.printf("[SIMON] SHOW  %d/%d  %s\n",
                                      showIdx + 1, currentLevel + 1,
                                      _simonColorName(sequence[showIdx]));

                        // Send farvelyd til begge joysticks
                        _simonPlayColor(sequence[showIdx]);
                        delay(currentBlink);  // Vis farven i blink-tid

                        isShowingColor = false;
                        showIdx++;
                        nextActionTime = millis() + currentPause;
                    }
                } else {
                    playerStepA = 0; playerStepB = 0;
                    failedA = false; failedB = false;
                    clearAllButtonStates();
                    for(int j=0; j<4; j++) { _simonBtnStateA[j] = false; _simonBtnStateB[j] = false; }
                    simonState = SIMON_INPUT;
                    Serial.printf("[SIMON] INPUT klar  (knaptilstande nulstillet)\n");
                }
            }
            break;

        case SIMON_INPUT:
        {
            // Niveau 0: hold farven synlig mens begge hold trykker
            if (currentLevel == 0) {
                int mid = firstScreen + (screenLen / 2);
                for(int i = -15; i < 15; i++) leds[mid + i] = SimonColors[sequence[0]];
            }

            // TEAM A – kant-baseret: registrer kun ved overgang ikke-trykket → trykket
            for(int i=0; i<4; i++) {
                bool pressed = (joyRead(pinsA[i]) == LOW);
                if(pressed && !_simonBtnStateA[i] && !failedA && playerStepA <= currentLevel) {
                    if(i == sequence[playerStepA]) {
                        _simonPlayColorJoy(i, JOY_A);
                        playerStepA++;
                        Serial.printf("[SIMON]  Groent  OK   %d/%d  %s\n",
                                      playerStepA, currentLevel + 1, _simonColorName(i));
                    } else {
                        Serial.printf("[SIMON]  Groent  FEJL %d/%d  forventede %s  fik %s\n",
                                      playerStepA + 1, currentLevel + 1,
                                      _simonColorName(sequence[playerStepA]), _simonColorName(i));
                        failedA = true;
                        sendSoundOrBroadcast(JOY_A, SimonMiss_id, SimonMiss_vol);
                    }
                }
                _simonBtnStateA[i] = pressed;
            }

            // TEAM B – kant-baseret: registrer kun ved overgang ikke-trykket → trykket
            for(int i=0; i<4; i++) {
                bool pressed = (joyRead(pinsB[i]) == LOW);
                if(pressed && !_simonBtnStateB[i] && !failedB && playerStepB <= currentLevel) {
                    if(i == sequence[playerStepB]) {
                        _simonPlayColorJoy(i, JOY_B);
                        playerStepB++;
                        Serial.printf("[SIMON]  Roedt   OK   %d/%d  %s\n",
                                      playerStepB, currentLevel + 1, _simonColorName(i));
                    } else {
                        Serial.printf("[SIMON]  Roedt   FEJL %d/%d  forventede %s  fik %s\n",
                                      playerStepB + 1, currentLevel + 1,
                                      _simonColorName(sequence[playerStepB]), _simonColorName(i));
                        failedB = true;
                        sendSoundOrBroadcast(JOY_B, SimonMiss_id, SimonMiss_vol);
                    }
                }
                _simonBtnStateB[i] = pressed;
            }

            // Vis progress – grønt hold grøn farve, rødt hold rød farve
            for(int i=0; i<playerStepA; i++) leds[firstScreen + 2 + i*2] = CRGB::Green;
            if(failedA) leds[firstScreen + 2 + playerStepA*2] = CRGB::White;
            for(int i=0; i<playerStepB; i++) leds[firstScreen + screenLen - 3 - i*2] = CRGB::Red;
            if(failedB) leds[firstScreen + screenLen - 3 - playerStepB*2] = CRGB::White;

            bool doneA = (failedA || playerStepA > currentLevel);
            bool doneB = (failedB || playerStepB > currentLevel);

            if (doneA && doneB) {
                Serial.printf("[SIMON] Runde slut:  Groent:%s  Roedt:%s\n",
                              failedA ? "FEJL" : "OK", failedB ? "FEJL" : "OK");
                // Niveau 0, begge korrekte: ingen 800ms delay – SIMON_PAUSE håndterer 2 sek mørkt
                if (currentLevel == 0 && !failedA && !failedB) {
                    simonState = SIMON_CHECK_ROUND;
                } else {
                    delay(800);
                    simonState = SIMON_CHECK_ROUND;
                }
            }
        }
        break;

        case SIMON_CHECK_ROUND:
            if (failedA && failedB) {
                // Begge fejlede – ingen scorer
                Serial.printf("[SIMON] Ingen scorer  niveau -> 0  (Gr:%d  Ro:%d)\n", scoreRed, scoreGreen);
                currentLevel = 0;
                sendSoundBoth(SimonMiss_id, SimonMiss_vol);
                delay(800);
            } else if (!failedA && failedB) {
                // Grønt hold scorer – spil hit til det grønne joystik
                scoreRed++;
                Serial.printf("[SIMON] Groent scorer!  Gr:%d  Ro:%d\n", scoreRed, scoreGreen);
                sendSoundOrBroadcast(JOY_A, SimonHit_id, SimonHit_vol);
                currentLevel = 0;
            } else if (failedA && !failedB) {
                // Rødt hold scorer – spil hit til det røde joystik
                scoreGreen++;
                Serial.printf("[SIMON] Roedt scorer!   Gr:%d  Ro:%d\n", scoreRed, scoreGreen);
                sendSoundOrBroadcast(JOY_B, SimonHit_id, SimonHit_vol);
                currentLevel = 0;
            } else {
                // Begge rigtige – level op, ingen point
                currentLevel++;
                Serial.printf("[SIMON] Begge OK  -> niveau %d\n", currentLevel);
            }

            failedA = false; failedB = false;
            playerStepA = 0; playerStepB = 0;

            if (scoreGreen >= 3 || scoreRed >= 3) {
                Serial.printf("[SIMON] SPIL SLUT  Groent:%d  Roedt:%d  -> %s vinder\n",
                              scoreRed, scoreGreen, scoreGreen >= 3 ? "Roedt" : "Groent");
                gameManager.setWinnerColor(scoreGreen >= 3 ? CRGB::Red : CRGB::Green);
                gameManager.setState(3);
            } else if (currentLevel == 1) {
                // Netop avanceret fra niveau 0 → 1: 2 sek mørkt før næste sekvens
                nextActionTime = millis() + 2000;
                simonState = SIMON_PAUSE;
            } else {
                simonState = SIMON_GENERATE;
            }
            break;

        case SIMON_PAUSE:
            // Skærmen er mørk (FastLED.clear() kørte øverst) – vent til tid
            if (now >= nextActionTime) {
                simonState = SIMON_GENERATE;
            }
            break;
    }
    showLeds();
}

#endif
