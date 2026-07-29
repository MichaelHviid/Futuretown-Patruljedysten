#ifndef HW_UTILS_H
#define HW_UTILS_H

#include <FastLED.h>

// Stub: Denne Gamebox har ingen lokal DAC-lyd.
// Spil-filer kalder playDacAsync() – vi ignorerer det stille.
inline void playDacAsync(byte /*pin*/, int /*freq*/, int /*duration*/) {}


// Hjælpefunktion: RGB til GRB (da dine joysticks er GRB)
inline CRGB _toGRB(CRGB color) {
  return CRGB(color.g, color.r, color.b);
}

// Hjælpefunktion til lyd (hvis begge filer bruger den)
inline void _playTestTone(int dacPin, int freq) {
  if (dacPin == 0) return;
  int period = 1000000 / freq / 2;
  for (int i = 0; i < 20; i++) {
    dacWrite(dacPin, 200); delayMicroseconds(period);
    dacWrite(dacPin, 0); delayMicroseconds(period);
  }
}

void playDualTone(int freq, int duration) {
    extern byte BuzzerPin1; // Referer til dine globale pins
    extern byte BuzzerPin2;
    
    if (BuzzerPin1 == 0 && BuzzerPin2 == 0) return;
    unsigned long start = millis();
    long period = 1000000L / freq;
    long halfPeriod = period / 2;
    
    while (millis() - start < (unsigned long)duration) {
        if (BuzzerPin1 != 0) dacWrite(BuzzerPin1, 255);
        if (BuzzerPin2 != 0) dacWrite(BuzzerPin2, 255);
        delayMicroseconds(halfPeriod);
        if (BuzzerPin1 != 0) dacWrite(BuzzerPin1, 0);
        if (BuzzerPin2 != 0) dacWrite(BuzzerPin2, 0);
        delayMicroseconds(halfPeriod);
    }
}

void playDacTone(int pin, int freq, int duration) {
    unsigned long start = millis();
    // En simpel bølgeform generator til DAC
    while (millis() - start < duration) {
        dacWrite(pin, 128 + 127 * sin(2 * PI * freq * (millis() - start) / 1000.0));
    }
    dacWrite(pin, 0); // Sluk efter tonen
}

// Fælles funktion til begge DACs
void playStartTone(int bz1, int bz2) {
    // Lav tone (Rød/Gul fase)
    playDacTone(bz1, 440, 200);
    playDacTone(bz2, 440, 200);
    delay(100);
    // Høj tone (Grøn "GO!" fase)
    playDacTone(bz1, 880, 500);
    playDacTone(bz2, 880, 500);
}

// ─── RTC-memory: overlever deep sleep ────────────────────────────────────────
// Bruges til at huske spil-valg og WiFi-status på tværs af dvale/opvågning
RTC_DATA_ATTR bool    _rtc_wokeFromSleep    = false;
RTC_DATA_ATTR int     _rtc_selectedGame     = 0;
RTC_DATA_ATTR bool    _rtc_wifiDisconnected = false;
RTC_DATA_ATTR uint8_t _rtc_espNowChannel    = 1;   // Aktiv kanal på sleeptidspunkt

/**
 * Sender Gamebox i deep sleep.
 * Vågner op når rød ELLER grøn knap trykkes (ext1, ANY_LOW).
 * Husker det valgte spil i RTC-memory.
 *
 * @param redPin   GPIO for fysisk rød knap
 * @param greenPin GPIO for fysisk grøn knap
 * @param selectedGame  Det aktuelle spilvalg (gemmes til opvågning)
 * @param wifiDisconnected  Om WiFi allerede er frakoblet (gemmes til opvågning)
 */
void enterDeepSleep(int redPin, int greenPin, int selectedGame, bool wifiDisconnected) {
    Serial.println("[DVALE] Gamebox går i deep sleep – tryk rød eller grøn knap for at vågne.");

    // Gem tilstand i RTC-memory (overlever deep sleep)
    _rtc_wokeFromSleep    = true;
    _rtc_selectedGame     = selectedGame;
    _rtc_wifiDisconnected = wifiDisconnected;
    // Gem den FAKTISK aktive kanal – ikke preference-værdien.
    // Joystikkene kommunikerer på routerens kanal; denne gemmes her
    // så vi vågner op på præcis den samme kanal.
    _rtc_espNowChannel    = (uint8_t)WiFi.channel();
    if (_rtc_espNowChannel == 0) _rtc_espNowChannel = 1;  // Fallback
    Serial.printf("[DVALE] Gemmer kanal %d til opvågning.\n", _rtc_espNowChannel);

    // Sluk LED-strip
    FastLED.clear();
    FastLED.show();
    delay(50);

    // Original ESP32 understøtter ikke ESP_EXT1_WAKEUP_ANY_LOW.
    // Løsning: ext0 på rød knap + ext1 på grøn knap – begge kan være aktive
    // simultant og chippen vågner ved *enten* trigger (LOW = knap trykket).
    esp_sleep_enable_ext0_wakeup((gpio_num_t)redPin,   0); // Rød knap → LOW
    esp_sleep_enable_ext1_wakeup(1ULL << greenPin,
                                 ESP_EXT1_WAKEUP_ALL_LOW); // Grøn knap → LOW

    esp_deep_sleep_start();
    // Koden herunder eksekveres aldrig
}

// ─── showLeds() – vend strip og vis ───────────────────────────────────────────
//
//  Den fysiske LED-strip starter ved det RØDE joystik (FIRST_SCREEN_LED = 0
//  i koden, men første LED sidder ved rød ende i virkeligheden).
//  Alle spil tegner "rød side" ved høje indekser og "grøn side" ved indeks 0.
//  Vi vender strimlen lige inden FastLED.show() – og vender den tilbage bagefter
//  så efterfølgende tegning stadig kan bruge de originale koordinater.
//
//  Brug showLeds() ALLE steder i stedet for FastLED.show() (undtagen setup).
// ─────────────────────────────────────────────────────────────────────────────
extern CRGB leds[];
extern int  SCREEN_LEDS;
extern int  FIRST_SCREEN_LED;

inline void showLeds() {
  int n = SCREEN_LEDS;
  // Flip game-sektionen
  for (int i = 0; i < n / 2; i++) {
    CRGB tmp = leds[FIRST_SCREEN_LED + i];
    leds[FIRST_SCREEN_LED + i]           = leds[FIRST_SCREEN_LED + n - 1 - i];
    leds[FIRST_SCREEN_LED + n - 1 - i]  = tmp;
  }
  FastLED.show();
  // Vend tilbage – koordinater gælder igen for tegning der sker efter show
  for (int i = 0; i < n / 2; i++) {
    CRGB tmp = leds[FIRST_SCREEN_LED + i];
    leds[FIRST_SCREEN_LED + i]           = leds[FIRST_SCREEN_LED + n - 1 - i];
    leds[FIRST_SCREEN_LED + n - 1 - i]  = tmp;
  }
}

/**
 * Justerer CPU hastigheden for at spare strøm
 * @param highSpeed true for 240MHz, false for 80MHz
 */
void setPowerMode(bool highSpeed) {
    uint32_t targetFreq = highSpeed ? 240 : 80;
    if (getCpuFrequencyMhz() != targetFreq) {
        setCpuFrequencyMhz(targetFreq);
        Serial.printf("CPU hastighed ændret til: %d MHz\n", targetFreq);
    }
}

void enterDeepSleepWithReport(const int* teamA, const int* teamB) {
    Serial.println("\n--- FORBEREDER DVALE ---");
    
    // Vi opbygger en "maske" (en liste over pins)
    uint64_t pin_mask = 0;
    String wakeButtons = "";

    // Liste over gyldige RTC GPIOs på ESP32:
    // 0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 39
    auto isRTC = [](int pin) {
        return (pin == 0 || pin == 2 || pin == 4 || (pin >= 12 && pin <= 15) || 
                (pin >= 25 && pin <= 27) || (pin >= 32 && pin <= 39));
    };

    // Tjek Team A (Rødt joystick)
    for (int i = 0; i < 4; i++) {
        if (isRTC(teamA[i])) {
            pin_mask |= (1ULL << teamA[i]);
            wakeButtons += "Rødt Joystick knap " + String(i + 1) + " (GPIO " + String(teamA[i]) + ")\n";
        }
    }

    // Tjek Team B (Grønt joystick)
    for (int i = 0; i < 4; i++) {
        if (isRTC(teamB[i])) {
            pin_mask |= (1ULL << teamB[i]);
            wakeButtons += "Grønt Joystick knap " + String(i + 1) + " (GPIO " + String(teamB[i]) + ")\n";
        }
    }

    if (pin_mask == 0) {
        Serial.println("FEJL: Ingen af dine knapper sidder på RTC-pins! Maskinen kan ikke vækkes.");
        return; 
    }

    Serial.println("Følgende knapper kan vække maskinen:");
    Serial.print(wakeButtons);
    Serial.println("Går i dvale om 2 sekunder...");
    
    FastLED.clear();
    FastLED.show();
    delay(2000);

    // Aktiver alle fundne pins i masken. 
    // ESP_EXT1_WAKEUP_ALL_LOW betyder: Vågn hvis EN af dem går i bund (LOW)
    esp_sleep_enable_ext1_wakeup(pin_mask, ESP_EXT1_WAKEUP_ALL_LOW);

    esp_deep_sleep_start();
}


#endif
