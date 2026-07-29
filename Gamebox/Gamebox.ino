// ─────────────────────────────────────────────────────────────────────
//  Futuretown – Patruljedysten, Gamers Edition
//  Udviklet & sponsoreret af WeDoMobility ApS · wedomobility.com
//  © FutureTown · Licens: CC BY-NC 4.0 (Kreditering–IkkeKommerciel)
//  Fri brug og tilpasning for spejdere og spejdergrupper.
//  MÅ IKKE bruges kommercielt – bokse, kode eller koncept må ikke
//  sælges eller ændres for at tjene penge. Se LICENSE.
//  https://creativecommons.org/licenses/by-nc/4.0/
// ─────────────────────────────────────────────────────────────────────
const char* FirmwareVersion = "0.607.05-08";
#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include "config_manager.h"
#include "button_manager.h"   // SKAL inkluderes FØR wifi_manager.h
#include "wifi_manager.h"     // Inkluderer espnow_buttons.h + espnow_sound.h
#include "GameManager.h"
#include "button_test.h"
#include "tug_of_war.h"
#include "simon_duel.h"
#include "color_catcher.h"
#include "speed_ball.h"
#include "whac_a_mole.h"
#include "ultimateWAM.h"
#include "chain_reaction.h"
#include "demo_game.h"
#include "hw_utils.h"

// ─── LED-opsætning ─────────────────────────────────────────────────────────
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define MAX_LEDS    325
#define static_LED_PIN 12

const int NUM_GAMES = 9;

// ─── Spil-filter (game_list preference) ────────────────────────────────────
int  _gameList[9];
int  _gameListLen = 0;

void parseGameList() {
  _gameListLen = 0;
  String s = game_list_str;
  int start = 0;
  while (start <= s.length() && _gameListLen < NUM_GAMES) {
    int comma = s.indexOf(',', start);
    String token = (comma == -1) ? s.substring(start) : s.substring(start, comma);
    token.trim();
    int val = token.toInt();
    if (val >= 0 && val < NUM_GAMES) _gameList[_gameListLen++] = val;
    if (comma == -1) break;
    start = comma + 1;
  }
  if (_gameListLen == 0) {
    for (int i = 0; i < NUM_GAMES; i++) _gameList[i] = i;
    _gameListLen = NUM_GAMES;
  }
  Serial.printf("[GAMES] Aktive spil (%d): ", _gameListLen);
  for (int i = 0; i < _gameListLen; i++) Serial.printf("%d ", _gameList[i]);
  Serial.println();
}

int nextGame() {
  for (int i = 0; i < _gameListLen; i++) {
    if (_gameList[i] == selected_game) return _gameList[(i + 1) % _gameListLen];
  }
  return _gameList[0];  // current ikke i listen → hop til første
}

// ─── Globale variabler ─────────────────────────────────────────────────────
AsyncWebServer server(80);
CRGB leds[MAX_LEDS];
int scoreA     = 0;
int scoreB     = 0;
String jsonConfig;
GameManager gameManager;
int lastGameID  = -1;
int scoreGreen  = 0;
int scoreRed    = 0;
bool gameFinished = false;
bool roundEnded   = false;

unsigned long lastInputTime  = 0;
bool          _joyLedsSilent  = false;
bool          _webServerActive = false;
bool          _screensaverActive = false;   // Lobby-screensaver kører (spil 1-7)

// ─── IP som binær LED-visning (kun i lobby) ─────────────────────────────────
void drawBinaryIP() {
  IPAddress ip = WiFi.localIP();
  if (ip[0] == 0) return;
  int centerStart = (SCREEN_LEDS / 2) + FIRST_SCREEN_LED - 2;
  for (int i = 0; i < 4; i++) leds[centerStart + i] = CRGB::Green;
  int currentPos = centerStart + 4 + 2;
  for (int octet = 0; octet < 4; octet++) {
    byte value = ip[octet];
    for (int bit = 0; bit < 8; bit++) {
      bool isBitSet = (value >> (7 - bit)) & 0x01;
      if (currentPos < MAX_LEDS) leds[currentPos] = isBitSet ? CRGB::White : CRGB(0, 0, 15);
      currentPos++;
    }
    currentPos += 2;
  }
}

// ─── Spil-ID som LED-indikator ─────────────────────────────────────────────
void drawGameIDIndicator() {
  if (selected_game == 0) {
    leds[FIRST_SCREEN_LED]     = CRGB::Yellow;
    leds[FIRST_SCREEN_LED + 1] = CRGB::Yellow;
    leds[FIRST_SCREEN_LED + 2] = CRGB::Yellow;
  } else if (selected_game == 8) {
    // Demo-spil: 3 røde LEDs (spejler spil 0's 3 gule)
    leds[FIRST_SCREEN_LED]     = CRGB::Red;
    leds[FIRST_SCREEN_LED + 1] = CRGB::Red;
    leds[FIRST_SCREEN_LED + 2] = CRGB::Red;
  } else {
    for (int i = 0; i < selected_game; i++) {
      leds[FIRST_SCREEN_LED + (i * 5)]     = CRGB::Blue;
      leds[FIRST_SCREEN_LED + 1 + (i * 5)] = CRGB::Blue;
      leds[FIRST_SCREEN_LED + 2 + (i * 5)] = CRGB::Blue;
    }
  }
}

// ─── Skift aktivt spil ─────────────────────────────────────────────────────
void updateSelectedGame() {
  if (selected_game == lastGameID) return;
  lastGameID = selected_game;
  Serial.printf("Skifter til spil %d\n", selected_game);

  switch (selected_game) {
    case 0:
      gameManager.setup(
        []() { FastLED.clear(); },
        []() { runButtonTest(leds); }
      );
      break;
    case 1:
      gameManager.setup(
        []() { initTugOfWar(SCREEN_LEDS); },
        []() { runTugOfWar(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
    case 2:
      gameManager.setup(
        []() { initSimonDuel(SCREEN_LEDS); },
        []() { runSimonDuel(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
    case 3:
      gameManager.setup(
        []() { initColorCatcher(SCREEN_LEDS); },
        []() { runColorCatcher(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
    case 4:
      gameManager.setup(
        []() { initSpeedBall(SCREEN_LEDS); },
        []() { runSpeedBall(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
    case 5:
      gameManager.setup(
        []() { initWhacAMole(SCREEN_LEDS); },
        []() { runWhacAMole(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
    case 6:
      gameManager.setup(
        []() { initChainReaction(SCREEN_LEDS); },
        []() { runChainReaction(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
    case 7:
      gameManager.setup(
        []() { initUltimateWAM(SCREEN_LEDS); },
        []() { runUltimateWAM(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
    case 8:
      gameManager.setup(
        []() { initDemoGame(SCREEN_LEDS); },
        []() { runDemoGame(leds, SCREEN_LEDS, FIRST_SCREEN_LED, pinsTeamA, pinsTeamB); }
      );
      break;
  }
}

// ─── Hello-annoncering ved opstart ─────────────────────────────────────────
//  Unicaster "Hello – her er gamebox <navn>" til den konfigurerede remote.
//  Hvis rød knap holdes ved boot: broadcaster også til alle på kanalen.
String _gbHelloNavn = "";   // cachet navn til periodisk hello

void announceGamebox() {
  preferences.begin("gamebox", true);
  _gbHelloNavn = preferences.getString("Navn", _defaultNavn());
  preferences.end();

  sendGameboxHello(_gbHelloNavn.c_str(), false);   // unicast → remote (hvis konfigureret)
  if (_bm_redAtBoot) {
    // Broadcast har ingen ACK → send en burst så Remoten næppe misser den
    for (int i = 0; i < 4; i++) { sendGameboxHello(_gbHelloNavn.c_str(), true); delay(200); }
  }
}

// Kaldes fra loop(): gentag broadcast-hello periodisk (hvis rød blev holdt ved boot),
// så en Remote der missede boot-burst'en, eller som lige er kommet online, finder gameboxen.
void periodicHello() {
  if (!_bm_redAtBoot) return;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 30000) return;   // hvert 30. sek
  last = now;
  sendGameboxHello(_gbHelloNavn.c_str(), true);
}

// ─── Setup ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);  // Giv terminal tid til at forbinde
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.printf( "║  Futuretown Gamebox  %-17s║\n", FirmwareVersion);
  Serial.println("╚══════════════════════════════════════╝");

  // Sluk LED-strip øjeblikkeligt – forhindrer støj/garbage under boot
  FastLED.addLeds<WS2812B, 12, GRB>(leds, MAX_LEDS);
  FastLED.clear();
  FastLED.show();

  // Læs pin-præferencer med hardcoded defaults (preferences ikke skrevet endnu)
  preferences.begin("gamebox", true);
  int rPin = preferences.getInt("red_pin",   4);
  int gPin = preferences.getInt("green_pin", 15);
  preferences.end();

  // Initialisér fysiske knapper og registrér boot-mode FØR preferences skrives
  initButtonManager(rPin, gPin);
  checkBootButtons();  // Sætter _bm_forceAPMode og _bm_wifiMode

  // Første opstart: skriv alle defaults til preferences én gang for altid
  ensureDefaultsWritten();

  // ── Opvågning fra deep sleep: skip WiFi, kun ESP-NOW ──────────────────────
  if (_rtc_wokeFromSleep) {
    Serial.println("[OPVÅGNING] Vågnede fra deep sleep – skip WiFi, starter ESP-NOW direkte.");
    selected_game = _rtc_selectedGame;

    if (!LittleFS.begin(true)) Serial.println("LittleFS Error");
    serializeJson(getCurrentConfig(), jsonConfig);

    // Start ESP-NOW efter dvale med APSTA-mode:
    // - AP-interface: sikrer TX-buffere er allokeret (løser NO_MEM)
    // - STA-interface: modtager pakker fra joystiks der sender til gamebox STA-MAC
    //   (joystik-MAC != AP-MAC, så STA skal være aktiv for at modtage)
    Serial.printf("[OPVÅGNING] ESP-NOW kanal: %d (APSTA bootstrap)\n", _rtc_espNowChannel);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("gamebox_wake", "12345678", _rtc_espNowChannel);
    WiFi.setSleep(false);
    delay(200);
    _initEspNow();
    delay(100);

    FastLED.addLeds<WS2812B, 12, GRB>(leds, MAX_LEDS);
    FastLED.setBrightness(LEDBRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // Ingen WiFi efter dvale → ingen webserver (ingen adgang udefra alligevel)
    lastGameID = -1;
    updateSelectedGame();
    lastInputTime = millis();
    _joyLedsSilent = false;
    announceGamebox();   // Hello til remote (+ broadcast hvis rød holdt)
    // gameManager._wifiDisconnected genspejles via RTC flag
    return;  // Spring den normale setup-sti over
  }
  // ──────────────────────────────────────────────────────────────────────────

  // initWiFi sætter _gb_webServerNeeded baseret på boot-mode
  initWiFi();

  // getCurrentConfig() indlæser alle præference-værdier i globale variabler
  serializeJson(getCurrentConfig(), jsonConfig);
  parseGameList();  // Byg aktiv spil-liste fra game_list preference

  // LEDs
  FastLED.addLeds<WS2812B, 12, GRB>(leds, MAX_LEDS);
  FastLED.setBrightness(LEDBRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  // Web-server og LittleFS kun i WiFi-mode og config-mode
  if (_gb_webServerNeeded) {
    if (!LittleFS.begin(true)) Serial.println("LittleFS Error");
    initWebServer(server);
    _webServerActive = true;
    Serial.println("[SETUP] Web-server aktiv.");
  } else {
    Serial.println("[SETUP] Spil-mode – web-server ikke startet.");
  }

  // Vent på at boot-knapper slippes inden main loop starter.
  // Forhindrer at en holdt grøn knap straks trigger spilstart.
  unsigned long btnWait = millis();
  while ((physRedPressed() || physGreenPressed()) && millis() - btnWait < 3000) {
    delay(10);
  }

  lastGameID = -1;
  updateSelectedGame();
  lastInputTime = millis();
  announceGamebox();   // Hello til remote (+ broadcast hvis rød holdt)
}

// ─── Screensaver (lobby) ───────────────────────────────────────────────────
//  Spil 1-7 har hver sin demo-animation. Spil 0 (knaptest) og 8 (demo) har ingen.
static bool _ssEligible(int game) { return game >= 1 && game <= 7; }

static int _ssAnim(int game) {
  switch (game) {
    case 1: return 11;   // Tug of War
    case 2: return 21;   // Simon
    case 3: return 32;   // Color Catcher
    case 4: return 42;   // Speed Ball
    case 5: return 51;   // Whac-A-Mole
    case 6: return 61;   // Chain Reaction
    case 7: return 71;   // Ultimate WAM
  }
  return 11;
}

void startScreensaver() {
  _screensaverActive = true;
  demoAnimStart(_ssAnim(selected_game), leds, SCREEN_LEDS, FIRST_SCREEN_LED);
  Serial.printf("[SCREENSAVER] Start – spil %d → anim %d\n", selected_game, _ssAnim(selected_game));
}

void stopScreensaver() {
  _screensaverActive = false;
  lastInputTime = millis();   // genstart idle-/dvale-timere
  FastLED.clear();
  Serial.println("[SCREENSAVER] Stop → lobby");
}

// ─── Loop ──────────────────────────────────────────────────────────────────
void loop() {
  if (_webServerActive) {
    ElegantOTA.loop();
    loopWebServer();   // WebSocket cleanup + ESP-NOW kø (Core 0 → Core 1)
  } else {
    // Ingen webserver (startet fra dvale) – behandl kun ESP-NOW kø
    processEspNowQueue();
  }

  periodicHello();   // gentag broadcast-hello (hvis rød blev holdt ved boot)

  unsigned long now          = millis();
  int           currentState = gameManager.getState();

  // ── Fysisk rød knap ────────────────────────────────────────────────────
  //   Kort tryk (skifter ved SLIP):  skift spil i lobby / afbryd spil
  //   Hold 3 sek i lobby (spil 1-7): start screensaver
  //   Ethvert tryk mens screensaver kører: afslut screensaver
  static bool          lastRedBtn      = false;
  static unsigned long redPressStart   = 0;
  static unsigned long lastRedTime     = 0;
  static bool          redHoldConsumed = false;
  bool curRed = physRedPressed();

  // Rising edge – rød trykket ned
  if (curRed && !lastRedBtn) {
    redPressStart   = now;
    redHoldConsumed = false;
  }

  // Hold i 3 sek i lobby med spil 1-7 → start screensaver
  if (curRed && !redHoldConsumed && !_screensaverActive &&
      currentState == STATE_WAITING && _ssEligible(selected_game) &&
      (now - redPressStart >= 3000)) {
    startScreensaver();
    redHoldConsumed = true;   // så efterfølgende slip ikke også skifter spil
  }

  // Falling edge – rød sluppet
  if (!curRed && lastRedBtn && (now - lastRedTime > 250)) {
    lastRedTime = now;
    if (redHoldConsumed) {
      // 3-sek-hold startede lige screensaveren → gør intet ved slip
    } else if (_screensaverActive) {
      stopScreensaver();                       // rød afslutter en kørende screensaver
    } else if (currentState == STATE_PLAYING || currentState == STATE_COUNTDOWN) {
      // Afbryd det igangværende spil → tilbage til lobby
      gameManager.setState(STATE_WAITING);
      Serial.println("[KNAP] Rød: spil afbrudt → lobby");
    } else if (currentState == STATE_WAITING || currentState == STATE_VICTORY) {
      // Skift til næste spil (i lobby ELLER vindervisning)
      selected_game = nextGame();
      preferences.begin("gamebox", false);
      preferences.putInt("selected_game", selected_game);
      preferences.end();
      serializeJson(getCurrentConfig(), jsonConfig);
      updateSelectedGame();
      Serial.printf("[KNAP] Rød: spil skiftet til %d\n", selected_game);
    }
  }
  lastRedBtn = curRed;

  // ── Fysisk grøn knap: start klar-fase fra lobby eller vindervisning ────────
  static bool          lastGreenBtn  = false;
  static unsigned long lastGreenTime = 0;
  bool curGreen = physGreenPressed();

  if (curGreen && !lastGreenBtn && (now - lastGreenTime > 250)) {
    lastGreenTime = now;
    if (currentState == STATE_WAITING || currentState == STATE_VICTORY) {
      _screensaverActive = false;   // grøn afslutter evt. screensaver og starter spillet
      clearJoyReady();
      gameManager.setState(STATE_COUNTDOWN);
      sendSoundBoth(GameStart_id, GameStart_vol);  // Spilstart afspilles straks ved grøn knap
      Serial.println("[KNAP] Grøn: klar-fase + spilstart-lyd");
    }
  }
  lastGreenBtn = curGreen;

  // ── Remote control (WebSocket + ESP-NOW) ──────────────────────────────────
  // Kun i lobbyen (STATE_WAITING) – ignorer i andre states
  if (_remoteActionPending) {
    byte act = _remoteActionPending; _remoteActionPending = 0;
    lastInputTime = now;
    _screensaverActive = false;   // remote-aktivitet afslutter screensaver
    if (act == 1 && (currentState == STATE_PLAYING || currentState == STATE_COUNTDOWN)) {
      // Rød under spil → afbryd og gå til lobby (samme som fysisk rød knap)
      gameManager.setState(STATE_WAITING);
      Serial.println("[REMOTE] Rød: spil afbrudt → lobby");
    } else if (act == 1 && (currentState == STATE_WAITING || currentState == STATE_VICTORY)) {
      // Rød i lobby/vinder → skift spil
      selected_game = nextGame();
      preferences.begin("gamebox", false);
      preferences.putInt("selected_game", selected_game);
      preferences.end();
      serializeJson(getCurrentConfig(), jsonConfig);
      updateSelectedGame();
      Serial.printf("[REMOTE] Spil skiftet til %d\n", selected_game);
    } else if (act == 2 && (currentState == STATE_WAITING || currentState == STATE_VICTORY)) {
      // Grøn: start klar-fase
      clearJoyReady();
      gameManager.setState(STATE_COUNTDOWN);
      sendSoundBoth(GameStart_id, GameStart_vol);
      Serial.println("[REMOTE] Grøn: klar-fase startet");
    } else if (act == 3) {
      // Send state til remote
      sendStateToRemote(selected_game, LEDBRIGHTNESS, GlobalVolume);
    }
  }
  if (_remoteCmdPending) {
    byte cmd = _remoteCmdPending; _remoteCmdPending = 0;
    lastInputTime = now;
    _screensaverActive = false;   // remote-aktivitet afslutter screensaver
    switch (cmd) {
      case REMOTE_CMD_NEXT_GAME:
        if (currentState == STATE_PLAYING || currentState == STATE_COUNTDOWN) {
          // Under spil → afbryd til lobby
          gameManager.setState(STATE_WAITING);
          Serial.println("[REMOTE] Rød: spil afbrudt → lobby");
        } else if (currentState == STATE_WAITING || currentState == STATE_VICTORY) {
          // I lobby/vinder → skift spil
          selected_game = nextGame();
          preferences.begin("gamebox", false); preferences.putInt("selected_game", selected_game); preferences.end();
          serializeJson(getCurrentConfig(), jsonConfig); updateSelectedGame();
          Serial.printf("[REMOTE] Spil skiftet til %d\n", selected_game);
        }
        break;
      case REMOTE_CMD_START:
        if (currentState == STATE_WAITING || currentState == STATE_VICTORY) {
          clearJoyReady(); gameManager.setState(STATE_COUNTDOWN); sendSoundBoth(GameStart_id, GameStart_vol);
        }
        break;
      case REMOTE_CMD_SET_GAME:
        if ((currentState == STATE_WAITING || currentState == STATE_VICTORY) && _remoteParam1 < NUM_GAMES) {
          selected_game = _remoteParam1;
          preferences.begin("gamebox", false); preferences.putInt("selected_game", selected_game); preferences.end();
          serializeJson(getCurrentConfig(), jsonConfig); updateSelectedGame();
        }
        break;
      case REMOTE_CMD_SET_CONFIG:
        if (currentState == STATE_WAITING) {
          LEDBRIGHTNESS = _remoteParam1; GlobalVolume = _remoteParam2;
          FastLED.setBrightness(LEDBRIGHTNESS);
          preferences.begin("gamebox", false);
          preferences.putUChar("LEDBRIGHTNESS", LEDBRIGHTNESS);
          preferences.putUChar("GlobalVolume",  GlobalVolume);
          preferences.end();
          serializeJson(getCurrentConfig(), jsonConfig);
        }
        break;
      case REMOTE_CMD_QUERY:
        sendStateToRemote(selected_game, LEDBRIGHTNESS, GlobalVolume);
        break;
    }
  }

  // Registrér aktivitet (til dvale-timer og input-tracking)
  if (curRed || curGreen) lastInputTime = now;
  for (int j = 0; j < 2; j++)
    for (int b = 0; b < 4; b++)
      if (joyBtnPressed[j][b]) lastInputTime = now;

  // CPU kører altid på 80 MHz
  setPowerMode(false);

  // ── Dvale-timer (SleepMin=0 → deaktiveret) ────────────────────────────────
  // lastInputTime nulstilles ved ethvert knapnedtryk – også under spil.
  // Ingen aktivitet i SleepMin minutter → dvale, uanset spil-state.
  if (SleepMin > 0) {
    unsigned long idle     = now - lastInputTime;
    unsigned long sleepMs  = (unsigned long)SleepMin * 60 * 1000UL;
    unsigned long ledOffMs = sleepMs / 2;

    if (idle >= sleepMs) {
      Serial.printf("[DVALE] %d min uden aktivitet – deep sleep.\n", SleepMin);
      clearAllJoyLeds();
      _setWifiLed(false);       // Sluk blå LED inden dvale
      sendSoundBoth(251, 15);   // Send dvale-kommando til begge joystiks (ID 251)
      delay(100);               // Giv ESP-NOW tid til at sende inden radio slukkes
      enterDeepSleep(red_pin, green_pin, selected_game,
                     gameManager.isWifiDisconnected());
    } else if (idle >= ledOffMs && !_joyLedsSilent) {
      Serial.printf("[DVALE] %.1f min uden aktivitet – sender dvale til joystiks.\n", SleepMin/2.0f);
      sendSoundBoth(251, 15);   // Joystiks går selv i dvale og slukker LEDs
      _joyLedsSilent = true;
    }
    if (idle < ledOffMs) _joyLedsSilent = false;
  }

  // ── Screensaver: start efter 60 sek i lobby (kun spil 1-7) ────────────────
  if (!_screensaverActive && currentState == STATE_WAITING &&
      _ssEligible(selected_game) && (now - lastInputTime >= 60000)) {
    startScreensaver();
  }

  // ── Screensaver aktiv: tegn animation i stedet for normal lobby ───────────
  //   Dvale-timeren ovenfor kører fortsat, så gameboxen kan gå i dvale mens
  //   screensaveren kører. Rød/grøn afslutter den (håndteret i knap-blokkene).
  if (_screensaverActive && gameManager.getState() == STATE_WAITING) {
    demoAnimFrame(leds, SCREEN_LEDS, FIRST_SCREEN_LED);   // tegner + showLeds()
    delay(10);
    return;
  }
  _screensaverActive = false;   // ikke længere i lobby → screensaver slukket

  // ── GameManager – alt tegning styres herfra ────────────────────────────
  if (selected_game != lastGameID) updateSelectedGame();
  gameManager.update(leds, SCREEN_LEDS, FIRST_SCREEN_LED);

  // Tegn spil-indikator i lobby OG vindervisning
  if (currentState == STATE_WAITING || currentState == STATE_VICTORY) {
    drawGameIDIndicator();
  }

  // Tegn WiFi-overlay i lobby (IP-adresse eller AP-mode markering)
  if (currentState == STATE_WAITING) {
    if (WiFi.status() == WL_CONNECTED) {
      drawBinaryIP();
    } else if (_gb_isAPMode) {
      // AP-mode: 4 gule LEDs i midten i stedet for IP-adresse
      int centerStart = (SCREEN_LEDS / 2) + FIRST_SCREEN_LED - 2;
      for (int i = 0; i < 4; i++) leds[centerStart + i] = CRGB::Yellow;
    }
  }

  // Dvale-forvarsel: sort strip med én grøn og én rød indikator-pixel i hver ende
  if (_joyLedsSilent) {
    fill_solid(leds + FIRST_SCREEN_LED, SCREEN_LEDS, CRGB::Black);
    uint8_t dim = map(BackBrightness, 0, 100, 0, 255);
    leds[FIRST_SCREEN_LED]                   = CRGB(0, dim, 0);   // Grøn (kode-venstre = fysisk grøn ende)
    leds[FIRST_SCREEN_LED + SCREEN_LEDS - 1] = CRGB(dim, 0, 0);   // Rød  (kode-højre  = fysisk rød ende)
  }
  showLeds();
  delay(10);
}
