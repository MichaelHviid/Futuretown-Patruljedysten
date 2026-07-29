#ifndef ESPNOW_SOUND_H
#define ESPNOW_SOUND_H

// =============================================================================
//  espnow_sound.h  –  ESP-NOW trådløs lyd-sender
//  Gamebox, Futuretown
//
//  Sender lyd-kommandoer (soundId + volume) til joystick-ESPs via ESP-NOW.
//  Joystick-firmware forventer at modtage: struct { byte soundId; byte volume; }
//
//  Bruges typisk i spilkoden:
//    sendSound(JOY_A, 12);              // Lyd 12 til joystick A, standard-volume
//    sendSound(JOY_B, 7, 18);           // Lyd 7 til joystick B, volume 18
//    sendSoundBoth(1);                  // Lyd 1 til begge joysticks
//    sendSoundOrBroadcast(JOY_A, 99);   // Til A's MAC, eller broadcast hvis ikke konfigureret
//
//  Specielle lyd-ID'er (brugt fra web-admin):
//    99  = Imperial March
//    250 = Aktiver WiFi på joystick
//
//  Kald i setup() EFTER initEspNowButtons():
//    initEspNowSound();
// =============================================================================

#include <Arduino.h>
#include <esp_now.h>

// -----------------------------------------------------------------------------
//  MAC-adresser
//  Genbruges fra espnow_buttons.h hvis den er inkluderet først.
//  Ellers defineres de her.
// -----------------------------------------------------------------------------
#ifndef ESPNOW_BUTTONS_H
  static uint8_t _espnow_joyMAC[2][6] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  // JOY_A – sæt via setJoystickMACFromString()
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  // JOY_B – sæt via setJoystickMACFromString()
  };
  #ifndef JOY_A
    #define JOY_A 0
    #define JOY_B 1
  #endif
#endif

// Broadcast MAC – sender til alle ESP-NOW enheder i nærheden
static const uint8_t _espnow_broadcastMAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// -----------------------------------------------------------------------------
//  Kommando-struct (skal matche joystick-firmware præcist)
//  soundId: 1-200 er lyde, 99 = Imperial March, 250 = aktiver WiFi
// -----------------------------------------------------------------------------
struct EspNowSndCmd {
  byte soundId;  // 0–255  (byte-range, firmware afgør hvad der sker)
  byte volume;   // 0–21
};

#define ESPNOW_SOUND_DEFAULT_VOL 15

// Remote MAC – registreres som peer for at sende state-info tilbage
static uint8_t _espnow_remoteMac[6] = {};

// -----------------------------------------------------------------------------
//  Intern tilstand
// -----------------------------------------------------------------------------
static bool _espnowSoundReady = false;

// Global volumen-skalering (0-100 % af konfigureret volumen)
// Defineret i config_manager.h som byte GlobalVolume
extern byte GlobalVolume;
static inline byte _applyGlobalVolume(byte vol) {
  return (byte)((unsigned int)vol * GlobalVolume / 100);
}

// -----------------------------------------------------------------------------
//  Hjælpefunktioner
// -----------------------------------------------------------------------------

// Returnerer true hvis MAC-adressen er all-zero (dvs. ikke konfigureret)
static inline bool _macIsZero(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) if (mac[i] != 0x00) return false;
  return true;
}

// Intern: registrer én peer med korrekt WiFi-interface
// Bruger WIFI_IF_AP i AP-mode (fx wake-bootstrap), WIFI_IF_STA ellers.
// channel = 0 = følg aktuel WiFi-kanal automatisk.
static void _registerPeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  // I normal STA-mode: brug WIFI_IF_STA.
  // I AP/APSTA wake-mode: brug WIFI_IF_AP for at sende (TX-buffere på AP).
  // Modtagelse sker via STA-interface uanset (joystiks sender til STA-MAC).
  wifi_mode_t mode; esp_wifi_get_mode(&mode);
  peer.ifidx = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) ? WIFI_IF_AP : WIFI_IF_STA;
  esp_err_t res = esp_now_add_peer(&peer);
  Serial.printf("[ESP-NOW PEER] Tilføjet %02X:%02X:%02X:%02X:%02X:%02X (%s)\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                res == ESP_OK ? "OK" : "FEJL");
}

// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------

/**
 * Registrer joystick-peers + broadcast-peer.
 * Kald EFTER initEspNowButtons(). Sikkert at kalde flere gange.
 */
void initEspNowSound() {
  // Broadcast peer – bruges når joystick-MAC ikke er konfigureret
  _registerPeer(_espnow_broadcastMAC);

  // Joystick-peers (hvis konfigurerede)
  for (int j = 0; j < 2; j++) {
    if (!_macIsZero(_espnow_joyMAC[j])) {
      _registerPeer(_espnow_joyMAC[j]);
      Serial.printf("[ESP-NOW LYD] Joystick %d (%02X:%02X:%02X:%02X:%02X:%02X) tilføjet som peer.\n",
                    j,
                    _espnow_joyMAC[j][0], _espnow_joyMAC[j][1], _espnow_joyMAC[j][2],
                    _espnow_joyMAC[j][3], _espnow_joyMAC[j][4], _espnow_joyMAC[j][5]);
    } else {
      Serial.printf("[ESP-NOW LYD] Joystick %d: ingen MAC konfigureret – bruger broadcast.\n", j);
    }
  }

  _espnowSoundReady = true;
  Serial.println("[ESP-NOW LYD] Lyd-sender klar.");
}

/**
 * Opdater en joystick-peers MAC-adresse live (fx efter web-config er gemt).
 * Fjerner den gamle peer og registrerer den nye.
 */
void refreshEspNowPeer(byte joystickId) {
  if (joystickId >= 2) return;
  // Fjern alle nuværende registrerede peers der matcher og tilføj ny
  for (int j = 0; j < 2; j++) {
    if (!_macIsZero(_espnow_joyMAC[j]) && esp_now_is_peer_exist(_espnow_joyMAC[j])) {
      // Kun fjern den specifikke joystick (undgå at røre andre)
    }
  }
  if (!_macIsZero(_espnow_joyMAC[joystickId])) {
    _registerPeer(_espnow_joyMAC[joystickId]);
    Serial.printf("[ESP-NOW LYD] Peer joystick %d opdateret.\n", joystickId);
  }
}

/**
 * Send lyd-ID til ét specifikt joystick (kræver konfigureret MAC).
 * @param joystickId  JOY_A eller JOY_B
 * @param soundId     0–255
 * @param volume      0–21
 */
void sendSound(byte joystickId, byte soundId, byte volume = ESPNOW_SOUND_DEFAULT_VOL) {
  if (!_espnowSoundReady) return;
  if (joystickId >= 2) return;
  if (volume > 21) volume = 21;

  EspNowSndCmd cmd = { soundId, _applyGlobalVolume(volume) };
  esp_err_t result = esp_now_send(_espnow_joyMAC[joystickId], (uint8_t*)&cmd, sizeof(cmd));
  if (result != ESP_OK) {
    Serial.printf("[ESP-NOW LYD] Send fejlede til joystick %d (fejl: %d)\n", joystickId, result);
  }
}

/**
 * Send lyd til joystick – bruger joystick-MAC hvis konfigureret, broadcast ellers.
 * Brug dette fra web-admin og spillogik – aldrig en fejl selv om MAC mangler.
 */
void sendSoundOrBroadcast(byte joystickId, byte soundId, byte volume = ESPNOW_SOUND_DEFAULT_VOL) {
  if (!_espnowSoundReady) return;
  if (joystickId >= 2) return;
  if (volume > 21) volume = 21;

  const uint8_t* target = _macIsZero(_espnow_joyMAC[joystickId])
                          ? _espnow_broadcastMAC
                          : _espnow_joyMAC[joystickId];

  EspNowSndCmd cmd = { soundId, _applyGlobalVolume(volume) };
  esp_err_t result = esp_now_send(target, (uint8_t*)&cmd, sizeof(cmd));
  if (result != ESP_OK) {
    Serial.printf("[ESP-NOW LYD] sendSoundOrBroadcast fejlede (joy=%d, fejl=%d)\n", joystickId, result);
  }
}

/**
 * Send samme lyd til begge joysticks (eller broadcast for ukonfigurerede).
 */
void sendSoundBoth(byte soundId, byte volume = ESPNOW_SOUND_DEFAULT_VOL) {
  sendSoundOrBroadcast(JOY_A, soundId, volume);
  sendSoundOrBroadcast(JOY_B, soundId, volume);
}

// ─── Remote control ──────────────────────────────────────────────────────────

// 7-byte state-info pakke: gamebox → remote (ikke-konflikt med eksisterende størrelser)
struct EspNowStateInfo {
  byte marker;      // = 0x53 ('S')
  byte game_id;
  byte brightness;
  byte volume;      // GlobalVolume (0-100)
  byte reserved[3];
};

// 8-byte remote-kommando: remote → gamebox
struct EspNowRemoteCmd {
  byte marker;      // = 0x52 ('R')
  byte cmd;         // se REMOTE_CMD_* konstanter
  byte param1;
  byte param2;
  byte reserved[4];
};
#define REMOTE_CMD_NEXT_GAME   0x01
#define REMOTE_CMD_START       0x02
#define REMOTE_CMD_SET_GAME    0x03
#define REMOTE_CMD_SET_CONFIG  0x04  // param1=brightness, param2=volume
#define REMOTE_CMD_QUERY       0x05  // Gamebox svarer med EspNowStateInfo

/**
 * Gem remote MAC og registrer som ESP-NOW peer.
 */
void setRemoteMACFromString(const char* macStr) {
  if (!parseMacString(macStr, _espnow_remoteMac)) {
    memset(_espnow_remoteMac, 0, 6);
    return;
  }
  if (!_espnowSoundReady || _macIsZero(_espnow_remoteMac)) return;
  _registerPeer(_espnow_remoteMac);
  Serial.printf("[REMOTE] MAC sat til %02X:%02X:%02X:%02X:%02X:%02X\n",
                _espnow_remoteMac[0], _espnow_remoteMac[1], _espnow_remoteMac[2],
                _espnow_remoteMac[3], _espnow_remoteMac[4], _espnow_remoteMac[5]);
}

/**
 * Send aktuel state til remote (spil, brightness, globalvolume).
 * Bruges ved query-kommando og "Send til Remote" fra web.
 */
void sendStateToRemote(byte game_id, byte brightness, byte volume) {
  if (!_espnowSoundReady || _macIsZero(_espnow_remoteMac)) return;
  if (!esp_now_is_peer_exist(_espnow_remoteMac)) {
    _registerPeer(_espnow_remoteMac);
  }
  EspNowStateInfo info;
  info.marker     = 0x53;
  info.game_id    = game_id;
  info.brightness = brightness;
  info.volume     = volume;
  memset(info.reserved, 0, sizeof(info.reserved));
  esp_now_send(_espnow_remoteMac, (uint8_t*)&info, sizeof(info));
  Serial.printf("[REMOTE] State sendt: spil=%d bright=%d vol=%d\n", game_id, brightness, volume);
}

// ─── Hello-annoncering ────────────────────────────────────────────────────────
// 22-byte hello-pakke: gamebox annoncerer sig ved opstart.
// Størrelse 22 kolliderer ikke med øvrige pakke-typer (2/3/4/7/8/21/49/LED-batch).
struct EspNowHello {
  byte marker;      // = 0x48 ('H')
  char name[21];    // Gamebox-navn, null-termineret (fx "Gamebox #10")
};

/**
 * Send "Hello – her er gamebox <navn>".
 *   broadcast=false → unicast til den konfigurerede remote (springes over hvis ingen)
 *   broadcast=true  → broadcast til alle på kanalen
 */
void sendGameboxHello(const char* name, bool broadcast) {
  if (!_espnowSoundReady) return;

  EspNowHello h;
  h.marker = 0x48;
  memset(h.name, 0, sizeof(h.name));
  strncpy(h.name, name ? name : "", sizeof(h.name) - 1);

  const uint8_t* target;
  if (broadcast) {
    target = _espnow_broadcastMAC;   // FF:FF:FF:FF:FF:FF (peer registreret i initEspNowSound)
  } else {
    if (_macIsZero(_espnow_remoteMac)) return;   // ingen remote konfigureret
    if (!esp_now_is_peer_exist(_espnow_remoteMac)) _registerPeer(_espnow_remoteMac);
    target = _espnow_remoteMac;
  }

  esp_now_send(target, (uint8_t*)&h, sizeof(h));
  Serial.printf("[HELLO] %s: \"%s\"\n", broadcast ? "BROADCAST" : "UNICAST→remote", h.name);
}

#endif // ESPNOW_SOUND_H
