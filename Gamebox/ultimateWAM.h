#ifndef ULTIMATE_WAM_H
#define ULTIMATE_WAM_H

#include <FastLED.h>
#include "GameManager.h"

extern GameManager gameManager;
extern int scoreGreen;
extern int scoreRed;
extern volatile bool joyBtnPressed[2][11];
extern int UWAM_MaxP;
extern int UWAM_Pts;
extern int UWAM_Bonus;

const int UWAM_LED_MAP[11] = {0, 1, 2, 3, 4, 6, 7, 8, 10, 11, 12};

// ─── Spil-state ────────────────────────────────────────────────────────────
static int  uwam_scoreGreen     = 0;
static int  uwam_scoreRed       = 0;
static bool uwam_greenLastPenalty = false; // Sidste grønne point = rød (fra røds fejl)
static bool uwam_redLastPenalty   = false; // Sidste røde point = grøn (fra grøns fejl)

static int  uwam_activeBtns[7]  = {};
static int  uwam_activeBtnCount = 0;
static int  uwam_ledRetry       = 0;
static int  uwam_errorBtnA      = -1;
static int  uwam_errorBtnB      = -1;
static bool uwam_prevA[11]      = {};
static bool uwam_prevB[11]      = {};
static unsigned long uwam_removedTime[11] = {}; // Hvornår knap sidst blev fjernet
static unsigned long uwam_pauseEnd  = 0;
static unsigned long uwam_startTime = 0;

#define UWAM_GRACE_MS 25   // ms grace-vindue for timing/latens

// ─── Hjælpefunktioner ──────────────────────────────────────────────────────

static int _uwamLevel() {
    int leading = max(uwam_scoreGreen, uwam_scoreRed);
    float pct   = (float)leading / (float)(UWAM_Pts > 0 ? UWAM_Pts : 1);
    if (pct < 0.20f) return 1;
    if (pct < 0.40f) return 2;
    if (pct < 0.60f) return 3;
    if (pct < 0.80f) return 4;
    return 5;
}

static int _uwamBtnCount(int level) {
    switch (level) {
        case 1: return 1;
        case 2: return random(1, 3);
        case 3: return 2;
        case 4: return random(1, 4);
        case 5: return random(2, 8);
        default: return 1;
    }
}

static void _uwamGenerate(int count) {
    count = constrain(count, 1, 11);
    int pool[11]; for(int i=0;i<11;i++) pool[i]=i;
    for(int i=10;i>0;i--) { int j=random(i+1); int t=pool[i]; pool[i]=pool[j]; pool[j]=t; }
    uwam_activeBtnCount = count;
    for(int i=0;i<count;i++) uwam_activeBtns[i]=pool[i];
    uwam_ledRetry = 5;
}

static bool _uwamIsActive(int idx) {
    for(int j=0;j<uwam_activeBtnCount;j++) if(uwam_activeBtns[j]==idx) return true;
    return false;
}

static void _uwamRemoveOne(int idx, unsigned long now) {
    uwam_removedTime[idx] = now;   // Gem tidspunkt for fjernelse
    for(int j=0;j<uwam_activeBtnCount;j++) {
        if(uwam_activeBtns[j]==idx) {
            for(int k=j;k<uwam_activeBtnCount-1;k++) uwam_activeBtns[k]=uwam_activeBtns[k+1];
            uwam_activeBtnCount--;
            return;
        }
    }
}

static void _uwamClearAllLeds() {
    for(int j=0;j<uwam_activeBtnCount;j++) {
        forceSendLed(JOY_A, UWAM_LED_MAP[uwam_activeBtns[j]], CRGB::Black);
        forceSendLed(JOY_B, UWAM_LED_MAP[uwam_activeBtns[j]], CRGB::Black);
    }
}

// Behandl forkert tryk fra et joystik
// Returnerer true hvis der gives penalty (modstander scorer)
static bool _uwamWrongPress(byte joyId, int btnIdx, unsigned long now) {
    // Er knappen for nylig fjernet (timing/latens)?
    bool recentlyRemoved = (uwam_removedTime[btnIdx] > 0 &&
                            now - uwam_removedTime[btnIdx] <= UWAM_GRACE_MS);

    if (recentlyRemoved) {
        // Inden for grace-vinduet → ingen straf, bare fejl-LED
        Serial.printf("[UWAM] %s knap %d – grace (%lums) – ingen straf\n",
                      joyId==0?"Grønt":"Rødt", btnIdx+1,
                      now - uwam_removedTime[btnIdx]);
        return false;  // Ingen penalty
    }

    // Rigtigt forkert tryk → modstander scorer (med penalty-farve)
    unsigned long elapsed = (uwam_removedTime[btnIdx] > 0)
                            ? now - uwam_removedTime[btnIdx] : 9999;
    if (joyId == 0) {
        // Grøn trykker forkert → Rød scorer (grøn farve i rød bjælke = rød fik et bonuspoint)
        uwam_scoreRed++;
        uwam_redLastPenalty = true;
        Serial.printf("[UWAM] Grønt knap %d ✗ (%lums efter fjernelse)  → Rød +1 (penalty)\n",
                      btnIdx+1, elapsed);
    } else {
        // Rød trykker forkert → Grøn scorer (rød farve i grøn bjælke)
        uwam_scoreGreen++;
        uwam_greenLastPenalty = true;
        Serial.printf("[UWAM] Rødt  knap %d ✗ (%lums efter fjernelse)  → Grøn +1 (penalty)\n",
                      btnIdx+1, elapsed);
    }
    return true;  // Penalty givet
}

// ─── Init ──────────────────────────────────────────────────────────────────
void initUltimateWAM(int /*screenLen*/) {
    scoreGreen = 0; scoreRed = 0;
    uwam_scoreGreen = uwam_scoreRed = 0;
    uwam_greenLastPenalty = uwam_redLastPenalty = false;
    uwam_activeBtnCount = 0;
    uwam_ledRetry  = 0;
    uwam_errorBtnA = uwam_errorBtnB = -1;
    uwam_startTime = millis();
    uwam_pauseEnd  = millis() + random(600, 1800);
    for(int i=0;i<11;i++) {
        uwam_prevA[i]=false; uwam_prevB[i]=false;
        uwam_removedTime[i]=0;
    }
}

// ─── Run ───────────────────────────────────────────────────────────────────
void runUltimateWAM(CRGB* leds, int screenLeds, int firstScreenLed, int* /*pinsA*/, int* /*pinsB*/) {
    unsigned long now = millis();
    FastLED.clear();

    // ── SCORE-BJÆLKER (yderste LED i særlig farve ved penalty) ───────────
    {
        int half = screenLeds / 2;
        int mid  = firstScreenLed + half;
        float pX = (float)half / (float)(UWAM_Pts > 0 ? UWAM_Pts : 1);
        int barG = constrain((int)(uwam_scoreGreen * pX), 0, half);
        int barR = constrain((int)(uwam_scoreRed   * pX), 0, half);

        for(int i=0;i<barG;i++) {
            int p = firstScreenLed+i;
            if(p < mid) {
                // Yderste LED rød hvis penalty, ellers grøn
                leds[p] = (i == barG-1 && uwam_greenLastPenalty) ? CRGB::Red : CRGB::Green;
            }
        }
        for(int i=0;i<barR;i++) {
            int p = firstScreenLed+screenLeds-1-i;
            if(p >= mid) {
                // Yderste LED grøn hvis penalty, ellers rød
                leds[p] = (i == barR-1 && uwam_redLastPenalty) ? CRGB::Green : CRGB::Red;
            }
        }
        leds[mid-1]=CRGB::White; leds[mid]=CRGB::White;
    }

    // ── RISING EDGE DETEKTION ─────────────────────────────────────────────
    bool newA[11]={}, newB[11]={};
    for(int i=0;i<11;i++) {
        bool cA=(bool)joyBtnPressed[0][i], cB=(bool)joyBtnPressed[1][i];
        newA[i]=cA&&!uwam_prevA[i]; newB[i]=cB&&!uwam_prevB[i];
        uwam_prevA[i]=cA; uwam_prevB[i]=cB;
    }

    // ── FEJL-LED: rød indtil slip ─────────────────────────────────────────
    if(uwam_errorBtnA>=0) {
        if(joyBtnPressed[0][uwam_errorBtnA]) forceSendLed(JOY_A,UWAM_LED_MAP[uwam_errorBtnA],CRGB::Red);
        else { forceSendLed(JOY_A,UWAM_LED_MAP[uwam_errorBtnA],CRGB::Black); uwam_errorBtnA=-1; }
    }
    if(uwam_errorBtnB>=0) {
        if(joyBtnPressed[1][uwam_errorBtnB]) forceSendLed(JOY_B,UWAM_LED_MAP[uwam_errorBtnB],CRGB::Red);
        else { forceSendLed(JOY_B,UWAM_LED_MAP[uwam_errorBtnB],CRGB::Black); uwam_errorBtnB=-1; }
    }

    if(uwam_activeBtnCount == 0) {
        // ── PAUSE-FASE ────────────────────────────────────────────────────
        for(int i=0;i<11;i++) {
            if(newA[i] && uwam_errorBtnA<0) {
                bool penalty = _uwamWrongPress(0, i, now);
                uwam_errorBtnA=i; forceSendLed(JOY_A,UWAM_LED_MAP[i],CRGB::Red);
                (void)penalty;
            }
            if(newB[i] && uwam_errorBtnB<0) {
                bool penalty = _uwamWrongPress(1, i, now);
                uwam_errorBtnB=i; forceSendLed(JOY_B,UWAM_LED_MAP[i],CRGB::Red);
                (void)penalty;
            }
        }
        if(now >= uwam_pauseEnd) {
            int lvl=_uwamLevel(), cnt=_uwamBtnCount(lvl);
            _uwamGenerate(cnt);
            Serial.printf("[UWAM] Level %d → %d knap(per): ", lvl, cnt);
            for(int j=0;j<uwam_activeBtnCount;j++) Serial.printf("%d ", uwam_activeBtns[j]+1);
            Serial.println();
        }

    } else {
        // ── AKTIV FASE ────────────────────────────────────────────────────

        bool forceNow=(uwam_ledRetry>0); if(forceNow) uwam_ledRetry--;
        for(int j=0;j<uwam_activeBtnCount;j++) {
            byte led=UWAM_LED_MAP[uwam_activeBtns[j]];
            if(forceNow) { forceSendLed(JOY_A,led,CRGB::White); forceSendLed(JOY_B,led,CRGB::White); }
            else         { sendLed(JOY_A,led,CRGB::White);       sendLed(JOY_B,led,CRGB::White); }
        }

        // A (Grønt hold)
        for(int i=0;i<11;i++) {
            if(newA[i]) {
                if(_uwamIsActive(i)) {
                    Serial.printf("[UWAM] Grønt knap %d ✓  G=%d→%d R=%d  aktive=%d→%d\n",
                                  i+1,uwam_scoreGreen,uwam_scoreGreen+1,
                                  uwam_scoreRed,uwam_activeBtnCount,uwam_activeBtnCount-1);
                    uwam_scoreGreen++;
                    uwam_greenLastPenalty = false;   // Normal scoring nulstiller penalty-farven
                    forceSendLed(JOY_A,UWAM_LED_MAP[i],CRGB::Black);
                    forceSendLed(JOY_B,UWAM_LED_MAP[i],CRGB::Black);
                    _uwamRemoveOne(i, now);
                } else {
                    // Forkert/for sent → modstander scorer (med grace-tjek)
                    bool penalty = _uwamWrongPress(0, i, now);
                    if(uwam_errorBtnA<0) { uwam_errorBtnA=i; forceSendLed(JOY_A,UWAM_LED_MAP[i],CRGB::Red); }
                    (void)penalty;
                }
            }
        }

        // B (Rødt hold)
        for(int i=0;i<11;i++) {
            if(newB[i]) {
                if(_uwamIsActive(i)) {
                    Serial.printf("[UWAM] Rødt  knap %d ✓  G=%d R=%d→%d  aktive=%d→%d\n",
                                  i+1,uwam_scoreGreen,
                                  uwam_scoreRed,uwam_scoreRed+1,uwam_activeBtnCount,uwam_activeBtnCount-1);
                    uwam_scoreRed++;
                    uwam_redLastPenalty = false;
                    forceSendLed(JOY_A,UWAM_LED_MAP[i],CRGB::Black);
                    forceSendLed(JOY_B,UWAM_LED_MAP[i],CRGB::Black);
                    _uwamRemoveOne(i, now);
                } else {
                    bool penalty = _uwamWrongPress(1, i, now);
                    if(uwam_errorBtnB<0) { uwam_errorBtnB=i; forceSendLed(JOY_B,UWAM_LED_MAP[i],CRGB::Red); }
                    (void)penalty;
                }
            }
        }

        if(uwam_activeBtnCount == 0) {
            unsigned long elapsed=(now-uwam_startTime)/1000;
            long maxPause=map(min(elapsed,(unsigned long)UWAM_Bonus),0,UWAM_Bonus,UWAM_MaxP,0);
            uwam_pauseEnd=now+random(400,max(600L,maxPause+600));
        }
    }

    // ── SEJRSTJEK ─────────────────────────────────────────────────────────
    if(uwam_scoreGreen >= UWAM_Pts || uwam_scoreRed >= UWAM_Pts) {
        bool greenWon=(uwam_scoreGreen >= UWAM_Pts);
        CRGB winCol=greenWon?CRGB::Green:CRGB::Red;
        CRGB darkCol=CRGB(winCol.r/6,winCol.g/6,winCol.b/6);
        _uwamClearAllLeds(); clearAllJoyLeds();
        unsigned long t0=millis();
        while(millis()-t0 < 5000) {
            fill_solid(leds+firstScreenLed,screenLeds,darkCol);
            for(int s=0;s<8;s++) leds[firstScreenLed+random(screenLeds)]=winCol;
            showLeds(); delay(80);
        }
        gameManager.setWinnerColor(winCol);
        gameManager.setState(3);
        return;
    }

    showLeds();
}

#endif
