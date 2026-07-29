#ifndef CHAIN_REACTION_H
#define CHAIN_REACTION_H

#include <FastLED.h>
#include "GameManager.h"
#include "hw_utils.h"

extern GameManager gameManager;
extern int scoreGreen;
extern int scoreRed;
extern const int JOYSTICK_LED_MAP[4];
extern const int JOYSTICK_SCORE_MAP[3];
extern byte BackBrightness;

extern int CHA_StartSpd;
extern int CHA_Accel;
extern int CHA_Zone;

extern byte CHAHit_id,   CHAHit_vol;
extern byte MissTune_id, MissTune_vol;  // Global miss-lyd til eksplosion
extern byte CHAMiss_id, CHAMiss_vol;

// Interne spil-variable
static float cha_pos = 0;
static float cha_speed = 0;
static int cha_direction = 1;
static int cha_colorIdx = 0;
static bool cha_active = false;
static bool cha_waitingForStart = true;

// LED failsafe: init-frames + periodisk refresh (knap-farver ændres aldrig → ingen naturlig retrigger)
static int  _chaInitFrames   = 0;
static unsigned long _chaLastLedSend = 0;

// Straf-system: 1000ms lock ved forkert tryk
static unsigned long cha_lockA = 0, cha_lockB = 0;
static int cha_errorBtnA = -1, cha_errorBtnB = -1;

const CRGB chaColors[] = {CRGB::Blue, CRGB::Green, CRGB(255, 100, 0), CRGB::Red};

static unsigned long lastInputA_CHA = 0;
static unsigned long lastInputB_CHA = 0;
const int inputCooldown_CHA = 200;

CRGB dimCha(CRGB color) {
    uint8_t dim = map(BackBrightness, 0, 100, 0, 255);
    return color.nscale8_video(dim);
}

void _resetChaRound() {
    cha_pos = 0;
    cha_speed = (float)CHA_StartSpd / 100.0;
    cha_direction = (random(0, 2) == 0) ? 1 : -1;
    cha_colorIdx = random(0, 4);
    cha_active = true;
    cha_waitingForStart = true;
    cha_lockA = 0; cha_lockB = 0;
    cha_errorBtnA = -1; cha_errorBtnB = -1;
}

void initChainReaction(int screenLen) {
    scoreGreen = 0; scoreRed = 0;
    _chaInitFrames = 5;      // Force-send de første 5 frames
    _chaLastLedSend = 0;     // Trigger straks periodisk refresh
    _resetChaRound();
}

void runChainReaction(CRGB* leds, int screenLen, int firstScreen, int* pinsA, int* pinsB) {
    unsigned long now = millis();
    FastLED.clear();

    int mid = firstScreen + (screenLen / 2);
    int currentZone = CHA_Zone;

    // --- 1. TEGN BANEN ---
    leds[mid] = CRGB(30, 0, 30);
    leds[mid-1] = CRGB(30, 0, 30);

    CRGB hZoneCol = dimCha(CRGB::DarkGreen);
    for(int i=0; i < currentZone; i++) {
        leds[firstScreen + i] = hZoneCol;
        leds[firstScreen + screenLen - 1 - i] = hZoneCol;
    }

    // Joystik-LEDs: knap-farver ændres aldrig → force hvert 2. sek som failsafe
    {   bool chaForce = (_chaInitFrames > 0) || (now - _chaLastLedSend >= 2000);
        if (chaForce) { _chaLastLedSend = now; if (_chaInitFrames > 0) _chaInitFrames--; }
        EspNowLedBatch cb; cb.count=4;
        for(int i=0;i<4;i++){CRGB c=dimCha(chaColors[i]);cb.leds[i]={(byte)JOYSTICK_LED_MAP[i],c.r,c.g,c.b};}
        if (chaForce) { forceSendLedBatch(JOY_A,cb); forceSendLedBatch(JOY_B,cb); }
        else          { sendLedBatch(JOY_A,cb);      sendLedBatch(JOY_B,cb); }
    }
    setScoreLeds(JOY_A, scoreGreen, CRGB::Green);
    setScoreLeds(JOY_B, scoreRed,   CRGB::Red);

    // --- 2. INPUT HÅNDTERING – brug joyRead i stedet for digitalRead ---
    int pressedA = -1; int pressedB = -1;
    bool startTriggered = false;

    for(int i=0; i<4; i++) {
        if(joyRead(pinsA[i]) == LOW && (now - lastInputA_CHA > inputCooldown_CHA)) {
            lastInputA_CHA = now;
            if(cha_waitingForStart) { cha_waitingForStart = false; startTriggered = true; }
            else if (now >= cha_lockA) pressedA = i;   // Ignorér tryk under straf-lock
        }
        if(joyRead(pinsB[i]) == LOW && (now - lastInputB_CHA > inputCooldown_CHA)) {
            lastInputB_CHA = now;
            if(cha_waitingForStart) { cha_waitingForStart = false; startTriggered = true; }
            else if (now >= cha_lockB) pressedB = i;
        }
    }

    // Straf-indikator: hvid LED mens lock er aktiv, sluk når lock udløber
    if (cha_errorBtnA >= 0) {
        if (now < cha_lockA) {
            sendLed(JOY_A, JOYSTICK_LED_MAP[cha_errorBtnA], CRGB::White);
        } else {
            forceSendLed(JOY_A, JOYSTICK_LED_MAP[cha_errorBtnA], CRGB::Black);
            cha_errorBtnA = -1;
        }
    }
    if (cha_errorBtnB >= 0) {
        if (now < cha_lockB) {
            sendLed(JOY_B, JOYSTICK_LED_MAP[cha_errorBtnB], CRGB::White);
        } else {
            forceSendLed(JOY_B, JOYSTICK_LED_MAP[cha_errorBtnB], CRGB::Black);
            cha_errorBtnB = -1;
        }
    }

    if (startTriggered) { showLeds(); return; }
    if (cha_waitingForStart) {
        if (now % 1000 < 500) { leds[mid] = CRGB(80, 0, 80); leds[mid-1] = CRGB(80, 0, 80); }
        showLeds(); return;
    }

    // --- 3. BOLD LOGIK ---
    if (cha_active) {
        cha_pos += (cha_speed * cha_direction);
        int ledPos = mid + (int)cha_pos;

        // TJEK FOR MISS + EKSPLOSION
        // code-RIGHT (høj ledPos) = fysisk RØD ende → GRØN scorer
        // code-LEFT  (lav ledPos) = fysisk GRØN ende → RØD scorer
        auto _chaExplosion = [&](bool redEnd) {
            clearAllJoyLeds();
            sendSoundBoth(MissTune_id, MissTune_vol);
            for (int f = 0; f < 3; f++) {
                CRGB fc = (f % 2 == 0) ? CRGB::White : (redEnd ? CRGB(120,0,0) : CRGB(0,120,0));
                if (redEnd) for (int j=0;j<screenLen/2;j++) leds[firstScreen+screenLen-1-j]=fc;
                else        for (int j=0;j<screenLen/2;j++) leds[firstScreen+j]=fc;
                showLeds(); delay(120);
                if (redEnd) for (int j=0;j<screenLen/2;j++) leds[firstScreen+screenLen-1-j]=CRGB::Black;
                else        for (int j=0;j<screenLen/2;j++) leds[firstScreen+j]=CRGB::Black;
                showLeds(); delay(80);
            }
        };

        if (ledPos >= firstScreen + screenLen) {
            // Bold ud ved RØD ende → GRØN scorer
            cha_active = false;
            _chaExplosion(true);
            scoreGreen++;
        }
        else if (ledPos < firstScreen) {
            // Bold ud ved GRØN ende → RØD scorer
            cha_active = false;
            _chaExplosion(false);
            scoreRed++;
        }

        // Tegn bold
        if (cha_active) {
            CRGB ballCol = chaColors[cha_colorIdx];
            if (ledPos >= firstScreen && ledPos < firstScreen + screenLen) {
                leds[ledPos] = ballCol;
                if (ledPos + 1 < firstScreen + screenLen) leds[ledPos + 1] = ballCol;
                if (ledPos - 1 >= firstScreen) leds[ledPos - 1] = ballCol;
            }
        }

        // --- 4. HIT OG STRAF ---
        // Player A (Rød) – forkert tryk → 1000ms straf + hvid LED
        if (pressedA != -1) {
            int dist = ledPos - firstScreen;
            if (dist >= 0 && dist < currentZone && pressedA == cha_colorIdx) {
                // Korrekt farve i zonen → hit
                cha_direction = 1; cha_colorIdx = random(0, 4);
                cha_speed += ((float)CHA_Accel / 100.0);
                sendSoundOrBroadcast(JOY_A, CHAHit_id, CHAHit_vol);
            } else {
                // Forkert eller udenfor zone → straf
                cha_speed += 0.05;
                cha_lockA = now + 1000;
                cha_errorBtnA = pressedA;
                sendSoundOrBroadcast(JOY_A, CHAMiss_id, CHAMiss_vol);
            }
        }
        // Player B (Grøn) – forkert tryk → 1000ms straf + hvid LED
        if (pressedB != -1) {
            int dist = (firstScreen + screenLen - 1) - ledPos;
            if (dist >= 0 && dist < currentZone && pressedB == cha_colorIdx) {
                // Korrekt farve i zonen → hit
                cha_direction = -1; cha_colorIdx = random(0, 4);
                cha_speed += ((float)CHA_Accel / 100.0);
                sendSoundOrBroadcast(JOY_B, CHAHit_id, CHAHit_vol);
            } else {
                // Forkert eller udenfor zone → straf
                cha_speed += 0.05;
                cha_lockB = now + 1000;
                cha_errorBtnB = pressedB;
                sendSoundOrBroadcast(JOY_B, CHAMiss_id, CHAMiss_vol);
            }
        }
    } else {
        if (scoreGreen < 3 && scoreRed < 3) {
            delay(500);   // Eksplosionen varede ~600ms – kort ekstra pause inden næste runde
            _resetChaRound();
        }
    }

    if (scoreGreen >= 3 || scoreRed >= 3) {
        gameManager.setWinnerColor(scoreGreen >= 3 ? CRGB::Green : CRGB::Red);
        gameManager.setState(3);
    }

    showLeds();
}

#endif
