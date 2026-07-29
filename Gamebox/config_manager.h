#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>

extern const char* FirmwareVersion;


// Globale variabler (brug extern i andre filer hvis nødvendigt)
Preferences preferences;
String wifi_ssid, wifi_pass, Gamebox_SSID, Gamebox_pass, Red_mac, Green_mac;
String GameboxNavn;
String game_list_str = "0,1,2,3,4,5,6,7";

// Default gamebox-navn: "Gamebox#" + sidste 4 hex-cifre af MAC-adressen.
// Bruger efuse-MAC → virker også før WiFi er initialiseret.
String _defaultNavn() {
  uint64_t chip = ESP.getEfuseMac();
  uint8_t b4 = (chip >> 32) & 0xFF;   // næstsidste MAC-oktet
  uint8_t b5 = (chip >> 40) & 0xFF;   // sidste MAC-oktet
  char buf[16];
  snprintf(buf, sizeof(buf), "Gamebox#%02X%02X", b4, b5);
  return String(buf);
}
int selected_game, wifi_channel, red_pin, green_pin;
byte LED_PIN, LEDBRIGHTNESS, BackBrightness, Volume, BatteryPin, BuzzerPin1, BuzzerPin2, Player1Pin, Player2Pin, TugEnergyCost;
int FirstPlayer1Led, FirstPlayer2Led, NUM_LEDS, SCREEN_LEDS, FIRST_SCREEN_LED, STATUS_LED, TugRestorationTime, TugPointToWin;
int SimonBlinkTime, SimonPauseTime, SimonSpeedCount;
int CCatcherTime, CCatcherPixTime, CCatcherPixNum, CCatcherHomePix;
int SB_BaseSpeed, SB_HomeSize, SB_WhiteGrow;
int WAM_MaxP, WAM_Pts, WAM_Bonus, UWAM_MaxP, UWAM_Pts, UWAM_Bonus, CHA_StartSpd,CHA_Accel,CHA_Zone;
float TugPullStrength;
byte GameStart_id, GameStart_vol;
byte GameSlut_id,  GameSlut_vol;
byte WinnerTune_id, WinnerTune_vol;
byte HitTune_id,   HitTune_vol;
byte MissTune_id,  MissTune_vol;
// Simon Battle – 4 farvelyde + egne hit/miss
byte SimonBlue_id,   SimonBlue_vol;
byte SimonGreen_id,  SimonGreen_vol;
byte SimonYellow_id, SimonYellow_vol;
byte SimonRed_id,    SimonRed_vol;
byte SimonHit_id,    SimonHit_vol;
byte SimonMiss_id,   SimonMiss_vol;
// Color Catcher – egne hit/miss
byte CCHit_id,  CCHit_vol;
byte CCMiss_id, CCMiss_vol;
// Speed Ball – egne hit/miss
byte SBHit_id,  SBHit_vol;
byte SBMiss_id, SBMiss_vol;
// Whac A Mole – egne hit/miss
byte WAMHit_id,  WAMHit_vol;
byte WAMMiss_id, WAMMiss_vol;
// Chain Reaction – egne hit/miss
byte CHAHit_id,  CHAHit_vol;
byte CHAMiss_id, CHAMiss_vol;



int pinsTeamA[5], pinsTeamB[5];
String Remote_mac;
byte GlobalVolume;     // 0-100 % – global volumen-skalering
byte JoystickTypeA;   // Grønt joystik: 0=lille/4-knapper/lyd  1=stor/11-knapper/ingen lyd
byte JoystickTypeB;   // Rødt joystik:  0=lille/4-knapper/lyd  1=stor/11-knapper/ingen lyd
byte SleepMin;        // Dvale-timeout i minutter (0=slukket, default=10)

// Mapping af de 9 pixels på et joystick (0-8)
// 0: Knaplys (Blå/Energi), 1-3: Score-pixels, 4-8: Andet/Reserve
const int JOYSTICK_LED_MAP[4] = {0, 4, 6, 8};
const int JOYSTICK_SCORE_MAP[3] = {1, 2, 3} ;

std::vector<const char*> byteKeys = {"LED_PIN","LEDBRIGHTNESS","BackBrightness","Volume","BatteryPin","BuzzerPin1","BuzzerPin2","Player1Pin","Player2Pin","A_Blue","A_Green","A_Yellow","A_Red","A_Input","B_Blue","B_Green","B_Yellow","B_Red","B_Input","TugEnergyCost",
  "GameStart_id","GameStart_vol","GameSlut_id","GameSlut_vol","WinnerTune_id","WinnerTune_vol",
  "HitTune_id","HitTune_vol","MissTune_id","MissTune_vol",
  "SimonBlue_id","SimonBlue_vol","SimonGreen_id","SimonGreen_vol","SimonYellow_id","SimonYellow_vol","SimonRed_id","SimonRed_vol",
  "SimonHit_id","SimonHit_vol","SimonMiss_id","SimonMiss_vol",
  "CCHit_id","CCHit_vol","CCMiss_id","CCMiss_vol",
  "SBHit_id","SBHit_vol","SBMiss_id","SBMiss_vol",
  "WAMHit_id","WAMHit_vol","WAMMiss_id","WAMMiss_vol",
  "CHAHit_id","CHAHit_vol","CHAMiss_id","CHAMiss_vol",
  "GlobalVolume","JoystickTypeA","JoystickTypeB","SleepMin"};
std::vector<const char*> intKeys = {"FirstP1Led","FirstP2Led","NUM_LEDS","SCREEN_LEDS","FIRST_S_LED","STATUS_LED","selected_game","wifi_channel","red_pin","green_pin","TugRestTime","TugPointToWin","SimonPauseTime","SimonBlinkTime","SimonSpeedCount","CCatcherTime","CCatcherHomePix","CCatcherPixNum","CCatcherPixTime","SB_BaseSpeed","SB_HomeSize","SB_WhiteGrow","WAM_MaxP","WAM_Pts","WAM_Bonus","UWAM_MaxP","UWAM_Pts","UWAM_Bonus","CHA_StartSpd","CHA_Accel","CHA_Zone"};
std::vector<const char*> stringKeys = {"wifi_ssid", "wifi_pass", "Navn", "Gamebox_SSID", "Gamebox_pass", "Red_mac", "Green_mac", "Remote_mac", "game_list"};
std::vector<const char*> floatKeys = {"TugPullStrength"};


// ---------------------------------------------------------------------------
//  ensureDefaultsWritten()
//  Kaldt tidligt i setup(). Skriver kun defaults for nøgler der IKKE allerede
//  eksisterer i preferences – eksisterende værdier (MAC-adresser, kalibrering
//  osv.) overskrives aldrig, heller ikke ved firmware-opdatering.
// ---------------------------------------------------------------------------
void ensureDefaultsWritten() {
  preferences.begin("gamebox", false);

  // Makro: skriv kun hvis nøglen ikke allerede er sat
  #define SET_STR(k, v)    if (!preferences.isKey(k)) preferences.putString(k, v)
  #define SET_INT(k, v)    if (!preferences.isKey(k)) preferences.putInt(k, v)
  #define SET_UCH(k, v)    if (!preferences.isKey(k)) preferences.putUChar(k, v)
  #define SET_FLT(k, v)    if (!preferences.isKey(k)) preferences.putFloat(k, v)
  #define SET_BOOL(k, v)   if (!preferences.isKey(k)) preferences.putBool(k, v)

  bool wasInit = preferences.isKey("_init");
  if (!wasInit) Serial.println("[CONFIG] Nye nøgler mangler – skriver defaults...");

  SET_BOOL("_init", true);

  // Netværk
  SET_STR("wifi_ssid",    "GamersWiFi");
  SET_STR("wifi_pass",    "12345678");
  SET_STR("Gamebox_SSID", "GameboxSetup");
  SET_STR("Gamebox_pass", "12345678");
  SET_STR("Green_mac",    "00:00:00:00:00:00");
  SET_STR("Red_mac",      "00:00:00:00:00:00");
  SET_STR("Remote_mac",   "00:00:00:00:00:00");
  SET_STR("Navn",         _defaultNavn());
  SET_STR("game_list",    "0,1,2,3,4,5,6,7");

  // System
  SET_INT("wifi_channel",  6);
  SET_INT("selected_game", 0);
  SET_INT("red_pin",       4);
  SET_INT("green_pin",     15);
  SET_UCH("GlobalVolume",  75);
  SET_UCH("JoystickTypeA",  0);
  SET_UCH("JoystickTypeB",  0);
  SET_UCH("SleepMin",      10);

  // LED-strip
  SET_UCH("LEDBRIGHTNESS", 50);
  SET_UCH("LED_PIN",        12);
  SET_UCH("BackBrightness", 25);
  SET_INT("NUM_LEDS",      138);
  SET_INT("SCREEN_LEDS",   120);
  SET_INT("FIRST_S_LED",     0);
  SET_INT("STATUS_LED",      5);
  SET_INT("FirstP1Led",      0);
  SET_INT("FirstP2Led",    129);

  // Hardware-pins
  SET_UCH("Volume",      200);
  SET_UCH("BatteryPin",   13);
  SET_UCH("BuzzerPin1",   25);
  SET_UCH("BuzzerPin2",   26);
  SET_UCH("Player1Pin",   31);
  SET_UCH("Player2Pin",   25);
  SET_UCH("A_Blue",       33);
  SET_UCH("A_Green",      32);
  SET_UCH("A_Yellow",     27);
  SET_UCH("A_Red",        14);
  SET_UCH("A_Input",      34);
  SET_UCH("B_Blue",       23);
  SET_UCH("B_Green",      22);
  SET_UCH("B_Yellow",     19);
  SET_UCH("B_Red",        18);
  SET_UCH("B_Input",       5);

  // Globale lyde
  SET_UCH("GameStart_id",   41); SET_UCH("GameStart_vol",  21);
  SET_UCH("GameSlut_id",    40); SET_UCH("GameSlut_vol",   21);
  SET_UCH("WinnerTune_id",   1); SET_UCH("WinnerTune_vol", 21);
  SET_UCH("HitTune_id",      8); SET_UCH("HitTune_vol",    21);
  SET_UCH("MissTune_id",    31); SET_UCH("MissTune_vol",   21);

  // Simon-lyde
  SET_UCH("SimonBlue_id",   50); SET_UCH("SimonBlue_vol",   21);
  SET_UCH("SimonGreen_id",  51); SET_UCH("SimonGreen_vol",  21);
  SET_UCH("SimonYellow_id", 23); SET_UCH("SimonYellow_vol", 21);
  SET_UCH("SimonRed_id",    25); SET_UCH("SimonRed_vol",    21);
  SET_UCH("SimonHit_id",    28); SET_UCH("SimonHit_vol",    21);
  SET_UCH("SimonMiss_id",   12); SET_UCH("SimonMiss_vol",   21);

  // Color Catcher-lyde
  SET_UCH("CCHit_id",  20); SET_UCH("CCHit_vol",  21);
  SET_UCH("CCMiss_id", 21); SET_UCH("CCMiss_vol", 21);

  // SpeedBall-lyde
  SET_UCH("SBHit_id",  20); SET_UCH("SBHit_vol",  21);
  SET_UCH("SBMiss_id", 21); SET_UCH("SBMiss_vol", 21);

  // Whac-a-Mole-lyde
  SET_UCH("WAMHit_id",   20); SET_UCH("WAMHit_vol",  21);
  SET_UCH("WAMMiss_id",  13); SET_UCH("WAMMiss_vol", 21);

  // Chain Reaction-lyde
  SET_UCH("CHAHit_id",   21); SET_UCH("CHAHit_vol",  21);
  SET_UCH("CHAMiss_id",  23); SET_UCH("CHAMiss_vol", 21);

  // Spilparametre
  SET_UCH("TugEnergyCost",   5);
  SET_INT("TugRestTime",    5000);
  SET_FLT("TugPullStrength", 0.5f);
  SET_INT("TugPointToWin",   3);
  SET_INT("SimonBlinkTime",  400);
  SET_INT("SimonPauseTime",  400);
  SET_INT("SimonSpeedCount",   5);
  SET_INT("CCatcherTime",     90);
  SET_INT("CCatcherPixTime",  45);
  SET_INT("CCatcherPixNum",    8);
  SET_INT("CCatcherHomePix",  15);
  SET_INT("SB_BaseSpeed",   3000);
  SET_INT("SB_HomeSize",      25);
  SET_INT("SB_WhiteGrow",    120);
  SET_INT("WAM_MaxP",       2500);
  SET_INT("WAM_Pts",          30);
  SET_INT("WAM_Bonus",       100);
  SET_INT("UWAM_MaxP",      2500);
  SET_INT("UWAM_Pts",        120);
  SET_INT("UWAM_Bonus",       60);
  SET_INT("CHA_StartSpd",     60);
  SET_INT("CHA_Accel",        20);
  SET_INT("CHA_Zone",         15);

  #undef SET_STR
  #undef SET_INT
  #undef SET_UCH
  #undef SET_FLT
  #undef SET_BOOL

  preferences.end();
  if (!wasInit) Serial.println("[CONFIG] Defaults skrevet. Eksisterende værdier bibeholdt.");
}

JsonDocument getCurrentConfig() {
  preferences.begin("gamebox", true);
  wifi_ssid = preferences.getString("wifi_ssid", "GamersWiFi");
  wifi_pass = preferences.getString("wifi_pass", "12345678");
  Gamebox_SSID = preferences.getString("Gamebox_SSID", "GameboxSetup");
  Gamebox_pass = preferences.getString("Gamebox_pass", "12345678");
  Green_mac    = preferences.getString("Green_mac",  "00:00:00:00:00:00");
  Red_mac      = preferences.getString("Red_mac",    "00:00:00:00:00:00");
  Remote_mac    = preferences.getString("Remote_mac", "00:00:00:00:00:00");
  GameboxNavn   = preferences.getString("Navn", _defaultNavn());
  game_list_str = preferences.getString("game_list", "0,1,2,3,4,5,6,7");
  GlobalVolume  = preferences.getUChar("GlobalVolume",   75);
  JoystickTypeA = preferences.getUChar("JoystickTypeA",   0);
  JoystickTypeB = preferences.getUChar("JoystickTypeB",   0);
  SleepMin      = preferences.getUChar("SleepMin",        10);

  selected_game = preferences.getInt("selected_game", 0);
  wifi_channel  = preferences.getInt("wifi_channel",  6);
  red_pin       = preferences.getInt("red_pin",   4);
  green_pin     = preferences.getInt("green_pin", 15);

  GameStart_id  = preferences.getUChar("GameStart_id",  41);
  GameStart_vol = preferences.getUChar("GameStart_vol", 21);
  GameSlut_id   = preferences.getUChar("GameSlut_id",   40);
  GameSlut_vol  = preferences.getUChar("GameSlut_vol",  21);
  WinnerTune_id  = preferences.getUChar("WinnerTune_id",   1);
  WinnerTune_vol = preferences.getUChar("WinnerTune_vol",  21);
  HitTune_id    = preferences.getUChar("HitTune_id",    8);
  HitTune_vol   = preferences.getUChar("HitTune_vol",  21);
  MissTune_id   = preferences.getUChar("MissTune_id",  31);
  MissTune_vol  = preferences.getUChar("MissTune_vol", 21);
  SimonBlue_id    = preferences.getUChar("SimonBlue_id",   50);
  SimonBlue_vol   = preferences.getUChar("SimonBlue_vol",  21);
  SimonGreen_id   = preferences.getUChar("SimonGreen_id",  51);
  SimonGreen_vol  = preferences.getUChar("SimonGreen_vol", 21);
  SimonYellow_id  = preferences.getUChar("SimonYellow_id", 23);
  SimonYellow_vol = preferences.getUChar("SimonYellow_vol",21);
  SimonRed_id     = preferences.getUChar("SimonRed_id",    25);
  SimonRed_vol    = preferences.getUChar("SimonRed_vol",   21);
  SimonHit_id     = preferences.getUChar("SimonHit_id",   28);
  SimonHit_vol    = preferences.getUChar("SimonHit_vol",  21);
  SimonMiss_id    = preferences.getUChar("SimonMiss_id",  12);
  SimonMiss_vol   = preferences.getUChar("SimonMiss_vol", 21);
  CCHit_id        = preferences.getUChar("CCHit_id",      20);
  CCHit_vol       = preferences.getUChar("CCHit_vol",     21);
  CCMiss_id       = preferences.getUChar("CCMiss_id",     21);
  CCMiss_vol      = preferences.getUChar("CCMiss_vol",    21);
  SBHit_id        = preferences.getUChar("SBHit_id",      20);
  SBHit_vol       = preferences.getUChar("SBHit_vol",     21);
  SBMiss_id       = preferences.getUChar("SBMiss_id",     21);
  SBMiss_vol      = preferences.getUChar("SBMiss_vol",    21);
  WAMHit_id       = preferences.getUChar("WAMHit_id",     20);
  WAMHit_vol      = preferences.getUChar("WAMHit_vol",    21);
  WAMMiss_id      = preferences.getUChar("WAMMiss_id",    13);
  WAMMiss_vol     = preferences.getUChar("WAMMiss_vol",   21);
  CHAHit_id       = preferences.getUChar("CHAHit_id",     21);
  CHAHit_vol      = preferences.getUChar("CHAHit_vol",    21);
  CHAMiss_id      = preferences.getUChar("CHAMiss_id",    23);
  CHAMiss_vol     = preferences.getUChar("CHAMiss_vol",   21);

  LEDBRIGHTNESS = preferences.getUChar("LEDBRIGHTNESS", 50);
  LED_PIN = preferences.getUChar("LED_PIN", 12);
  BackBrightness = preferences.getUChar("BackBrightness", 25);
  Volume = preferences.getUChar("Volume", 200);
  BatteryPin = preferences.getUChar("BatteryPin", 13);
  BuzzerPin1 = preferences.getUChar("BuzzerPin1", 25);
  BuzzerPin2 = preferences.getUChar("BuzzerPin2", 26);
  Player1Pin = preferences.getUChar("Player1Pin", 31);
  Player2Pin = preferences.getUChar("Player2Pin", 25);
  FirstPlayer1Led = preferences.getInt("FirstP1Led", 0);
  FirstPlayer2Led = preferences.getInt("FirstP2Led", 129);
  NUM_LEDS = preferences.getInt("NUM_LEDS", 138);
  SCREEN_LEDS = preferences.getInt("SCREEN_LEDS", 120);
  FIRST_SCREEN_LED = preferences.getInt("FIRST_S_LED", 0);
  STATUS_LED = preferences.getInt("STATUS_LED", 5);
  TugEnergyCost = preferences.getUChar("TugEnergyCost", 5);
  TugRestorationTime = preferences.getInt("TugRestTime", 5000);
  TugPointToWin = preferences.getInt("TugPointToWin", 3);
  TugPullStrength = preferences.getFloat("TugPullStrength", 0.5);
  SimonBlinkTime = preferences.getInt("SimonBlinkTime", 400);
  SimonPauseTime = preferences.getInt("SimonPauseTime", 400);
  SimonSpeedCount = preferences.getInt("SimonSpeedCount", 5);
  CCatcherTime = preferences.getInt("CCatcherTime", 90);
  CCatcherPixTime = preferences.getInt("CCatcherPixTime", 45);
  CCatcherPixNum = preferences.getInt("CCatcherPixNum", 8);
  CCatcherHomePix = preferences.getInt("CCatcherHomePix", 15);
  
  WAM_MaxP = preferences.getInt("WAM_MaxP", 2500);
  WAM_Pts = preferences.getInt("WAM_Pts", 30);
  WAM_Bonus = preferences.getInt("WAM_Bonus", 100);
  
  UWAM_MaxP = preferences.getInt("UWAM_MaxP", 2500);
  UWAM_Pts = preferences.getInt("UWAM_Pts", 120);
  UWAM_Bonus = preferences.getInt("UWAM_Bonus", 60);
  
  SB_BaseSpeed = preferences.getInt("SB_BaseSpeed", 3000);
  SB_HomeSize = preferences.getInt("SB_HomeSize", 25);
  SB_WhiteGrow = preferences.getInt("SB_WhiteGrow", 120);
  
  CHA_StartSpd = preferences.getInt("CHA_StartSpd", 60);
  CHA_Accel = preferences.getInt("CHA_Accel", 20);
  CHA_Zone = preferences.getInt("CHA_Zone", 15);
  
  pinsTeamA[0] = preferences.getUChar("A_Blue", 33);
  pinsTeamA[1] = preferences.getUChar("A_Green", 32);
  pinsTeamA[2] = preferences.getUChar("A_Yellow", 27);
  pinsTeamA[3] = preferences.getUChar("A_Red", 14);
  pinsTeamA[4] = preferences.getUChar("A_Input", 34);
  
  pinsTeamB[0] = preferences.getUChar("B_Blue", 23);
  pinsTeamB[1] = preferences.getUChar("B_Green", 22);
  pinsTeamB[2] = preferences.getUChar("B_Yellow", 19);
  pinsTeamB[3] = preferences.getUChar("B_Red", 18);
  pinsTeamB[4] = preferences.getUChar("B_Input", 5);

  preferences.end();
  
  JsonDocument doc;
  doc["FirmwareVersion"] = FirmwareVersion; // Kan også trækkes fra en define
  doc["LEDBRIGHTNESS"] = LEDBRIGHTNESS;
  doc["BackBrightness"] = BackBrightness;
  doc["wifi_ssid"] = wifi_ssid;
  doc["Navn"]      = GameboxNavn;
  doc["game_list"] = game_list_str;
  doc["Gamebox_SSID"] = Gamebox_SSID;
  doc["Gamebox_pass"] = Gamebox_pass;
  doc["Green_mac"]    = Green_mac;
  doc["Red_mac"]      = Red_mac;
  doc["Remote_mac"]   = Remote_mac;
  doc["GlobalVolume"]  = GlobalVolume;
  doc["JoystickTypeA"] = JoystickTypeA;
  doc["JoystickTypeB"] = JoystickTypeB;
  doc["SleepMin"]      = SleepMin;
  doc["selected_game"]  = selected_game;
  doc["wifi_channel"]   = wifi_channel;
  doc["red_pin"]        = red_pin;
  doc["green_pin"]      = green_pin;
  doc["GameStart_id"]   = GameStart_id;
  doc["GameStart_vol"]  = GameStart_vol;
  doc["GameSlut_id"]    = GameSlut_id;
  doc["GameSlut_vol"]   = GameSlut_vol;
  doc["WinnerTune_id"]  = WinnerTune_id;
  doc["WinnerTune_vol"] = WinnerTune_vol;
  doc["HitTune_id"]     = HitTune_id;
  doc["HitTune_vol"]    = HitTune_vol;
  doc["MissTune_id"]    = MissTune_id;
  doc["MissTune_vol"]   = MissTune_vol;
  doc["SimonBlue_id"]    = SimonBlue_id;
  doc["SimonBlue_vol"]   = SimonBlue_vol;
  doc["SimonGreen_id"]   = SimonGreen_id;
  doc["SimonGreen_vol"]  = SimonGreen_vol;
  doc["SimonYellow_id"]  = SimonYellow_id;
  doc["SimonYellow_vol"] = SimonYellow_vol;
  doc["SimonRed_id"]     = SimonRed_id;
  doc["SimonRed_vol"]    = SimonRed_vol;
  doc["SimonHit_id"]     = SimonHit_id;
  doc["SimonHit_vol"]    = SimonHit_vol;
  doc["SimonMiss_id"]    = SimonMiss_id;
  doc["SimonMiss_vol"]   = SimonMiss_vol;
  doc["CCHit_id"]        = CCHit_id;
  doc["CCHit_vol"]       = CCHit_vol;
  doc["CCMiss_id"]       = CCMiss_id;
  doc["CCMiss_vol"]      = CCMiss_vol;
  doc["SBHit_id"]        = SBHit_id;
  doc["SBHit_vol"]       = SBHit_vol;
  doc["SBMiss_id"]       = SBMiss_id;
  doc["SBMiss_vol"]      = SBMiss_vol;
  doc["WAMHit_id"]       = WAMHit_id;
  doc["WAMHit_vol"]      = WAMHit_vol;
  doc["WAMMiss_id"]      = WAMMiss_id;
  doc["WAMMiss_vol"]     = WAMMiss_vol;
  doc["CHAHit_id"]       = CHAHit_id;
  doc["CHAHit_vol"]      = CHAHit_vol;
  doc["CHAMiss_id"]      = CHAMiss_id;
  doc["CHAMiss_vol"]     = CHAMiss_vol;
  doc["Volume"] = Volume;
  doc["BatteryPin"] = BatteryPin;
  doc["BuzzerPin1"] = BuzzerPin1;
  doc["BuzzerPin2"] = BuzzerPin2;
  doc["Player1Pin"] = Player1Pin;
  doc["Player2Pin"] = Player2Pin;
  doc["FirstP1Led"] = FirstPlayer1Led;
  doc["FirstP2Led"] = FirstPlayer2Led;   
  doc["NUM_LEDS"] = NUM_LEDS;
  doc["SCREEN_LEDS"] = SCREEN_LEDS;
  doc["FIRST_S_LED"] = FIRST_SCREEN_LED;
  doc["STATUS_LED"] = STATUS_LED;
  doc["TugEnergyCost"] = TugEnergyCost;
  doc["TugRestTime"] = TugRestorationTime;
  doc["TugPullStrength"] = TugPullStrength;
  doc["TugPointToWin"]= TugPointToWin;
  doc["SimonPauseTime"] = SimonPauseTime;
  doc["SimonBlinkTime"] = SimonBlinkTime;
  doc["SimonSpeedCount"] = SimonSpeedCount;
  doc["CCatcherTime"] = CCatcherTime;
  doc["CCatcherPixTime"] = CCatcherPixTime;
  doc["CCatcherPixNum"] = CCatcherPixNum;
  doc["CCatcherHomePix"] = CCatcherHomePix;
  doc["SB_BaseSpeed"] = SB_BaseSpeed;
  doc["SB_HomeSize"] = SB_HomeSize;
  doc["SB_WhiteGrow"] = SB_WhiteGrow;
  doc["WAM_MaxP"] = WAM_MaxP;
  doc["WAM_Pts"] = WAM_Pts;
  doc["WAM_Bonus"] = WAM_Bonus;
  doc["UWAM_MaxP"] = UWAM_MaxP;
  doc["UWAM_Pts"] = UWAM_Pts;
  doc["UWAM_Bonus"] = UWAM_Bonus;
  doc["CHA_StartSpd"] = CHA_StartSpd;
  doc["CHA_Accel"] = CHA_Accel;
  doc["CHA_Zone"] = CHA_Zone;
  doc["A_Blue"] =  pinsTeamA[0];
  doc["A_Green"] = pinsTeamA[1];
  doc["A_Yellow"] =pinsTeamA[2];
  doc["A_Red"] =   pinsTeamA[3];
  doc["A_Input"] = pinsTeamA[4];
  doc["B_Blue"] =  pinsTeamB[0];
  doc["B_Green"] = pinsTeamB[1];
  doc["B_Yellow"] =pinsTeamB[2];
  doc["B_Red"] =   pinsTeamB[3];
  doc["B_Input"] = pinsTeamB[4];

  return doc;
}

#endif
