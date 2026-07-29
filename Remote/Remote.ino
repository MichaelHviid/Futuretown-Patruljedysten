// ─────────────────────────────────────────────────────────────────────
//  Futuretown – Patruljedysten, Gamers Edition · Remote
//  Udviklet & sponsoreret af WeDoMobility ApS · wedomobility.com
//  © FutureTown · Licens: CC BY-NC 4.0 (Kreditering–IkkeKommerciel)
//  Fri brug og tilpasning for spejdere og spejdergrupper.
//  MÅ IKKE bruges kommercielt – bokse, kode eller koncept må ikke
//  sælges eller ændres for at tjene penge. Se LICENSE.
//  https://creativecommons.org/licenses/by-nc/4.0/
// ─────────────────────────────────────────────────────────────────────
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <Preferences.h>

const char* RemoteVersion    = "0.607.13-01";   // Remote firmware-version
const char* default_ssid     = "GamersWiFi";
const char* default_password = "12345678";
const char* default_ap_ssid  = "GamersRemote";
const char* default_ap_pass  = "12345678";   // AP-kodeord (min. 8 tegn for WPA2 – skift dette!)

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Preferences preferences;
uint8_t targetMac[6]    = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};  // Default = broadcast
uint8_t gameboxMac[6]   = {0};   // Gamebox-MAC
char gatewayMacStr[18];
uint8_t gatewayMacByte[6];
int wifiChannel   = 1;    // Aktiv kanal (router-kanal ved STA, ellers gemt kanal)
int storedChannel = 6;    // Gemt ESP-NOW-kanal – bruges i AP-mode / uden router
bool isAPMode     = false;
bool espNowOnlyMode = false;

String current_ssid;
String current_password;
String apSsid;              // Remotens eget AP-SSID (redigerbart i web)
String apPass;              // Remotens eget AP-kodeord (redigerbart i web)

// ─── STRUCTS ─────────────────────────────────────────────────────────────────

typedef struct struct_message { byte id; byte button; bool pressed; } struct_message;

// 21-byte ping fra joystik ved opstart
typedef struct struct_ping { byte id; char name[20]; } struct_ping;

// 49-byte WiFi-status svar fra joystik (efter lyd-ID 250)
typedef struct struct_wifi_status { char ip[16]; char ssid[33]; } struct_wifi_status;
struct_message incomingData;

typedef struct struct_cmd { byte soundId; byte volume; } struct_cmd;
struct_cmd outgoingCmd;

typedef struct struct_led { byte ledId; byte r; byte g; byte b; } struct_led;
struct_led outgoingLed;

// 8-byte kommando til gamebox (marker=0x52)
typedef struct struct_remote_cmd {
    byte marker;     // = 0x52
    byte cmd;
    byte param1;
    byte param2;
    byte reserved[4];
} struct_remote_cmd;

// 7-byte state-info fra gamebox (marker=0x53)
typedef struct struct_state_info {
    byte marker;     // = 0x53
    byte game_id;
    byte brightness;
    byte volume;
    byte reserved[3];
} struct_state_info;

// 22 bytes = "Hello"-pakke fra gamebox (marker 0x48 + navn)
typedef struct struct_hello {
    byte marker;      // = 0x48 ('H')
    char name[21];    // null-termineret gamebox-navn
} struct_hello;

// ── Hello-register: huskes i RAM så en nyåbnet webside ser tidligere hello'er ──
#define MAX_HELLO 16
struct HelloEntry { uint8_t mac[6]; char name[21]; uint32_t count; uint32_t lastMs; bool used; };
HelloEntry helloLog[MAX_HELLO] = {};

// Registrér/opdatér en hello. Returnerer indeks i helloLog.
int registerHello(const uint8_t* mac, const char* name) {
    int free = -1, oldest = 0;
    for (int i = 0; i < MAX_HELLO; i++) {
        if (helloLog[i].used && memcmp(helloLog[i].mac, mac, 6) == 0) {
            strncpy(helloLog[i].name, name, 20); helloLog[i].name[20] = 0;
            helloLog[i].count++; helloLog[i].lastMs = millis();
            return i;
        }
        if (!helloLog[i].used && free < 0) free = i;
        if (helloLog[i].lastMs < helloLog[oldest].lastMs) oldest = i;
    }
    int idx = (free >= 0) ? free : oldest;   // fyldt → overskriv ældste
    memcpy(helloLog[idx].mac, mac, 6);
    strncpy(helloLog[idx].name, name, 20); helloLog[idx].name[20] = 0;
    helloLog[idx].count = 1; helloLog[idx].lastMs = millis(); helloLog[idx].used = true;
    return idx;
}

// Send én hello-entry til web (client=nullptr → alle klienter)
void sendHelloJson(int idx, AsyncWebSocketClient* client) {
    char macStr[18];
    snprintf(macStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             helloLog[idx].mac[0], helloLog[idx].mac[1], helloLog[idx].mac[2],
             helloLog[idx].mac[3], helloLog[idx].mac[4], helloLog[idx].mac[5]);
    JsonDocument doc;
    doc["type"]   = "gamebox_hello";
    doc["sender"] = macStr;
    doc["name"]   = helloLog[idx].name;
    doc["count"]  = helloLog[idx].count;
    doc["agoSec"] = (millis() - helloLog[idx].lastMs) / 1000;
    String json; serializeJson(doc, json);
    if (client) client->text(json); else ws.textAll(json);
}

#define REMOTE_CMD_NEXT_GAME   0x01
#define REMOTE_CMD_START       0x02
#define REMOTE_CMD_SET_GAME    0x03
#define REMOTE_CMD_SET_CONFIG  0x04
#define REMOTE_CMD_QUERY       0x05

// ─── HJÆLPERE ────────────────────────────────────────────────────────────────

void registerPeer(uint8_t* mac) {
    if (!espNowOnlyMode && WiFi.status() == WL_CONNECTED) wifiChannel = WiFi.channel();
    esp_now_peer_info_t p; memset(&p, 0, sizeof(p));
    memcpy(p.peer_addr, mac, 6);
    p.channel = 0;
    p.encrypt = false;
    { wifi_mode_t m; esp_wifi_get_mode(&m);
      p.ifidx = (m==WIFI_MODE_AP||m==WIFI_MODE_APSTA) ? WIFI_IF_AP : WIFI_IF_STA; }
    if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);
    esp_now_add_peer(&p);
}

bool parseMac(const char* str, uint8_t* mac) {
    int v[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)v[i];
    return true;
}

// ── Gamebox-liste (persisteret: op til 20 fjernbetjente gameboxe) ─────────────
#define MAX_GB 20
struct GbEntry { char mac[18]; char name[24]; bool used; };
GbEntry gbList[MAX_GB] = {};

void loadGbList() {
    preferences.begin("esp_now_pref", true);
    String js = preferences.getString("gb_list", "[]");
    preferences.end();
    for (int i = 0; i < MAX_GB; i++) { gbList[i].used = false; gbList[i].mac[0] = 0; gbList[i].name[0] = 0; }
    JsonDocument doc;
    if (deserializeJson(doc, js)) return;
    int i = 0;
    for (JsonObject o : doc.as<JsonArray>()) {
        if (i >= MAX_GB) break;
        const char* m = o["mac"] | "";
        if (strlen(m) < 17) continue;
        strncpy(gbList[i].mac,  m,             17); gbList[i].mac[17] = 0;
        strncpy(gbList[i].name, o["name"] | "", 23); gbList[i].name[23] = 0;
        gbList[i].used = true;
        i++;
    }
}

void saveGbList() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_GB; i++) if (gbList[i].used) {
        JsonObject o = arr.add<JsonObject>();
        o["mac"]  = gbList[i].mac;
        o["name"] = gbList[i].name;
    }
    String js; serializeJson(doc, js);
    preferences.begin("esp_now_pref", false);
    preferences.putString("gb_list", js);
    preferences.end();
}

int gbFind(const char* mac) {
    for (int i = 0; i < MAX_GB; i++)
        if (gbList[i].used && strcasecmp(gbList[i].mac, mac) == 0) return i;
    return -1;
}

int gbCount() { int n = 0; for (int i = 0; i < MAX_GB; i++) if (gbList[i].used) n++; return n; }

void gbRegisterAll() {
    for (int i = 0; i < MAX_GB; i++) if (gbList[i].used) {
        uint8_t m[6]; if (parseMac(gbList[i].mac, m)) registerPeer(m);
    }
}

// Send hele gamebox-listen til web (client=nullptr → alle klienter)
void sendGbList(AsyncWebSocketClient* client) {
    JsonDocument doc;
    doc["type"] = "gb_list";
    JsonArray arr = doc["boxes"].to<JsonArray>();
    for (int i = 0; i < MAX_GB; i++) if (gbList[i].used) {
        JsonObject o = arr.add<JsonObject>();
        o["mac"]  = gbList[i].mac;
        o["name"] = gbList[i].name;
    }
    String js; serializeJson(doc, js);
    if (client) client->text(js); else ws.textAll(js);
}

// Send en remote-kommando til en specifik gamebox-MAC (streng)
void sendToMac(const char* macStr, byte cmd, byte p1 = 0, byte p2 = 0) {
    uint8_t m[6];
    if (!parseMac(macStr, m)) return;
    if (!esp_now_is_peer_exist(m)) registerPeer(m);
    struct_remote_cmd rc;
    rc.marker = 0x52; rc.cmd = cmd; rc.param1 = p1; rc.param2 = p2;
    memset(rc.reserved, 0, 4);
    esp_now_send(m, (uint8_t*)&rc, sizeof(rc));
}

void sendToGamebox(byte cmd, byte p1 = 0, byte p2 = 0) {
    if (!esp_now_is_peer_exist(gameboxMac)) registerPeer(gameboxMac);
    struct_remote_cmd rc;
    rc.marker = 0x52; rc.cmd = cmd; rc.param1 = p1; rc.param2 = p2;
    memset(rc.reserved, 0, 4);
    esp_now_send(gameboxMac, (uint8_t*)&rc, sizeof(rc));
}

// game_id/brightness/volume + hvilken gamebox (MAC) det kom fra
void broadcastState(const char* mac, byte game_id, byte brightness, byte volume) {
    JsonDocument doc;
    doc["type"]       = "gamebox_state";
    doc["mac"]        = mac;
    doc["game_id"]    = game_id;
    doc["brightness"] = brightness;
    doc["volume"]     = volume;
    String json; serializeJson(doc, json);
    ws.textAll(json);
}

// ─── ESP-NOW MODTAGER ────────────────────────────────────────────────────────

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {

    char senderMac[18];
    snprintf(senderMac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             info->src_addr[0],info->src_addr[1],info->src_addr[2],
             info->src_addr[3],info->src_addr[4],info->src_addr[5]);

    // 21 bytes = ping-pakke fra joystik ved opstart
    if (len == 21) {
        struct_ping* p = (struct_ping*)data;
        p->name[19] = '\0';
        Serial.printf("[PING] Joystik #%d '%s'  MAC: %s\n", p->id, p->name, senderMac);
        JsonDocument doc;
        doc["type"]    = "joystick_ping";
        doc["sender"]  = senderMac;
        doc["joy_id"]  = p->id;
        doc["joy_name"]= p->name;
        doc["details"] = String("🟢 <b>Joystik #") + p->id + " '" + p->name + "' er online</b><br>"
                       + "<span style='font-family:monospace;font-size:11px'>" + senderMac + "</span>";
        String json; serializeJson(doc, json); ws.textAll(json);
        return;
    }

    // 49 bytes = WiFi-status svar (efter lyd-ID 250)
    if (len == 49) {
        struct_wifi_status* ws_data = (struct_wifi_status*)data;
        ws_data->ip[15] = '\0'; ws_data->ssid[32] = '\0';
        Serial.printf("[WIFI STATUS] IP: %s  SSID: %s  MAC: %s\n", ws_data->ip, ws_data->ssid, senderMac);
        JsonDocument doc;
        doc["type"]    = "joystick_wifi";
        doc["sender"]  = senderMac;
        doc["ip"]      = ws_data->ip;
        doc["ssid"]    = ws_data->ssid;
        doc["details"] = String("📡 <b>WiFi tændt</b>  IP: <b>") + ws_data->ip + "</b><br>"
                       + "SSID: " + ws_data->ssid + "<br>"
                       + "<span style='font-family:monospace;font-size:11px'>" + senderMac + "</span>";
        String json; serializeJson(doc, json); ws.textAll(json);
        return;
    }

    // 7 bytes = EspNowStateInfo fra gamebox
    if (len == 7 && data[0] == 0x53) {
        struct_state_info* si = (struct_state_info*)data;
        Serial.printf("[GAMEBOX STATE] %s  spil=%d bright=%d vol=%d\n", senderMac, si->game_id, si->brightness, si->volume);
        broadcastState(senderMac, si->game_id, si->brightness, si->volume);
        return;
    }

    // 22 bytes = "Hello" fra gamebox (marker 0x48)
    if (len == sizeof(struct_hello) && data[0] == 0x48) {
        struct_hello* h = (struct_hello*)data;
        h->name[20] = '\0';
        int idx = registerHello(info->src_addr, h->name);
        Serial.printf("[HELLO] '%s'  MAC: %s  (#%lu)\n", h->name, senderMac, (unsigned long)helloLog[idx].count);
        sendHelloJson(idx, nullptr);   // send til alle åbne websider
        // Er gameboxen allerede på styringslisten? Opdatér dens navn og gem.
        int g = gbFind(senderMac);
        if (g >= 0 && strcmp(gbList[g].name, h->name) != 0) {
            strncpy(gbList[g].name, h->name, 23); gbList[g].name[23] = 0;
            saveGbList();
            sendGbList(nullptr);
        }
        return;
    }

    // 3 bytes = knap-event fra joystik
    if (len == sizeof(struct_message)) {
        memcpy(&incomingData, data, sizeof(incomingData));

        // Auto-svar til joystik (uændret fra original)
        if (incomingData.button == 4 && incomingData.pressed) {
            outgoingCmd = {16, 15};
            if (!esp_now_is_peer_exist(info->src_addr)) registerPeer((uint8_t*)info->src_addr);
            esp_now_send((uint8_t*)info->src_addr, (uint8_t*)&outgoingCmd, sizeof(outgoingCmd));
        }
        if (incomingData.button == 3 && incomingData.pressed) {
            outgoingCmd = {17, 15};
            if (!esp_now_is_peer_exist(info->src_addr)) registerPeer((uint8_t*)info->src_addr);
            esp_now_send((uint8_t*)info->src_addr, (uint8_t*)&outgoingCmd, sizeof(outgoingCmd));
        }

        if (!espNowOnlyMode) {
            char recvMac[18];
            snprintf(recvMac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                     info->des_addr[0],info->des_addr[1],info->des_addr[2],
                     info->des_addr[3],info->des_addr[4],info->des_addr[5]);
            JsonDocument doc;
            doc["type"] = "esp_now_recv";
            doc["sender"] = senderMac; doc["receiver"] = recvMac;
            doc["targetType"] = (info->des_addr[0] == 0xFF) ? "Broadcast" : "Direkte";
            String knapNavn = String(incomingData.button);
            if (incomingData.button == 1) knapNavn = "1 (Rød)";
            if (incomingData.button == 2) knapNavn = "2 (Gul)";
            if (incomingData.button == 3) knapNavn = "3 (Grøn)";
            if (incomingData.button == 4) knapNavn = "4 (Blå)";
            char buf[128];
            snprintf(buf, 128, "📥 <b>Joystick data</b><br>ID: %d | Knap: %s | %s",
                     incomingData.id, knapNavn.c_str(), incomingData.pressed ? "🔴 TRYKKET" : "⚪ SLUPPET");
            doc["details"] = buf; doc["isJoystickStruct"] = true;
            doc["struct_button"] = incomingData.button; doc["struct_pressed"] = incomingData.pressed;
            String json; serializeJson(doc, json); ws.textAll(json);
        }
    }
}

// ─── WEBSOCKET ────────────────────────────────────────────────────────────────

void handleWsMsg(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client = nullptr) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;
    String action = doc["action"].as<String>();

    // ── Eksisterende joystik-handling ──
    if (action == "save_mac") {
        String ms = doc["mac"].as<String>();
        if (ms == "" || ms == "00:00:00:00:00:00") ms = "FF:FF:FF:FF:FF:FF";
        if (parseMac(ms.c_str(), targetMac)) {
            preferences.begin("esp_now_pref", false);
            preferences.putBytes("target_mac", targetMac, 6);
            preferences.end();
            registerPeer(targetMac);
        }
    }
    else if (action == "save_wifi") {
        preferences.begin("esp_now_pref", false);
        preferences.putString("wifi_ssid", doc["ssid"].as<String>());
        preferences.putString("wifi_pass", doc["pass"].as<String>());
        preferences.end();
        delay(500); ESP.restart();
    }
    else if (action == "save_ap") {
        String s = doc["ssid"].as<String>();
        String p = doc["pass"].as<String>();
        if (s.length() == 0) s = default_ap_ssid;
        preferences.begin("esp_now_pref", false);
        preferences.putString("ap_ssid", s);
        preferences.putString("ap_pass", p);
        preferences.end();
        Serial.printf("[REMOTE] AP gemt: SSID=%s – genstarter\n", s.c_str());
        delay(500); ESP.restart();
    }
    else if (action == "save_channel") {
        int ch = doc["channel"] | 6;
        if (ch < 1)  ch = 1;
        if (ch > 13) ch = 13;
        preferences.begin("esp_now_pref", false);
        preferences.putInt("wifi_channel", ch);
        preferences.end();
        Serial.printf("[REMOTE] ESP-NOW kanal gemt: %d – genstarter\n", ch);
        delay(500); ESP.restart();
    }
    else if (action == "trigger_cmd_struct") {
        outgoingCmd.soundId = doc["soundId"]; outgoingCmd.volume = doc["volume"];
        registerPeer(targetMac);
        esp_now_send(targetMac, (uint8_t*)&outgoingCmd, sizeof(outgoingCmd));
    }
    else if (action == "trigger_led_struct") {
        outgoingLed.ledId = doc["ledId"];
        outgoingLed.r = doc["r"]; outgoingLed.g = doc["g"]; outgoingLed.b = doc["b"];
        registerPeer(targetMac);
        esp_now_send(targetMac, (uint8_t*)&outgoingLed, sizeof(outgoingLed));
    }
    else if (action == "go_turbo") {
        if (WiFi.status() == WL_CONNECTED) wifiChannel = WiFi.channel();
        ws.closeAll(); server.end();
        esp_wifi_disconnect();
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
        espNowOnlyMode = true;
        registerPeer(targetMac);
    }

    // ── Gamebox-liste (flere bokse, per-MAC) ──
    else if (action == "gb_save_first") {          // øverste boks: sæt/ret MAC + navn
        String ms = doc["mac"].as<String>();
        String nm = doc["name"].as<String>();
        uint8_t m[6];
        if (parseMac(ms.c_str(), m)) {
            if (!gbList[0].used) gbList[0].used = true;
            strncpy(gbList[0].mac,  ms.c_str(), 17); gbList[0].mac[17]  = 0;
            strncpy(gbList[0].name, nm.c_str(), 23); gbList[0].name[23] = 0;
            registerPeer(m);
            saveGbList(); sendGbList(nullptr);
            Serial.printf("[REMOTE] Øverste gamebox sat: %s '%s'\n", ms.c_str(), nm.c_str());
        }
    }
    else if (action == "gb_add") {                 // "Opret" fra hello-loggen
        String ms = doc["mac"].as<String>();
        String nm = doc["name"].as<String>();
        uint8_t m[6];
        if (parseMac(ms.c_str(), m) && gbFind(ms.c_str()) < 0) {
            int n = 0; while (n < MAX_GB && gbList[n].used) n++;
            if (n < MAX_GB) {
                strncpy(gbList[n].mac,  ms.c_str(), 17); gbList[n].mac[17] = 0;
                strncpy(gbList[n].name, nm.c_str(), 23); gbList[n].name[23] = 0;
                gbList[n].used = true;
                registerPeer(m);
                saveGbList(); sendGbList(nullptr);
                Serial.printf("[REMOTE] Gamebox tilføjet: %s '%s'\n", ms.c_str(), nm.c_str());
            }
        }
    }
    else if (action == "gb_remove") {              // "Slet" på en auto-boks
        String ms = doc["mac"].as<String>();
        int idx = gbFind(ms.c_str());
        if (idx >= 0) {
            for (int j = idx; j < MAX_GB - 1; j++) gbList[j] = gbList[j + 1];
            gbList[MAX_GB - 1].used = false; gbList[MAX_GB - 1].mac[0] = 0; gbList[MAX_GB - 1].name[0] = 0;
            saveGbList(); sendGbList(nullptr);
            Serial.printf("[REMOTE] Gamebox slettet: %s\n", ms.c_str());
        }
    }
    else if (action == "gb_red")   { sendToMac(doc["mac"].as<String>().c_str(), REMOTE_CMD_NEXT_GAME); }
    else if (action == "gb_green") { sendToMac(doc["mac"].as<String>().c_str(), REMOTE_CMD_START); }
    else if (action == "gb_query") { sendToMac(doc["mac"].as<String>().c_str(), REMOTE_CMD_QUERY); }
    else if (action == "gb_config") {
        byte br = doc["brightness"] | 100;
        byte vo = doc["volume"]     | 100;
        sendToMac(doc["mac"].as<String>().c_str(), REMOTE_CMD_SET_CONFIG, br, vo);
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        preferences.begin("esp_now_pref", false);
        preferences.getBytes("target_mac",  targetMac,  6);
        preferences.end();

        char tStr[18];
        snprintf(tStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                 targetMac[0],targetMac[1],targetMac[2],
                 targetMac[3],targetMac[4],targetMac[5]);

        JsonDocument doc;
        doc["type"]       = "config";
        doc["version"]    = RemoteVersion;
        doc["savedMac"]   = tStr;
        doc["myMac"]        = gatewayMacStr;
        doc["myChannel"]    = wifiChannel;
        doc["savedChannel"] = storedChannel;
        doc["apSsid"]       = apSsid;
        doc["apPass"]       = apPass;
        doc["wifiSsid"]     = current_ssid;
        String json; serializeJson(doc, json); client->text(json);

        // Send gamebox-styringslisten + tidligere hello'er til den nyåbnede side
        sendGbList(client);
        for (int i = 0; i < MAX_HELLO; i++)
            if (helloLog[i].used) sendHelloJson(i, client);

    } else if (type == WS_EVT_DATA) {
        handleWsMsg(arg, data, len, client);
    }
}

// ─── HTML ────────────────────────────────────────────────────────────────────

#include "web_page.h"

// ─── SETUP + LOOP ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200); delay(1000);
    Serial.printf("\n[REMOTE] Firmware-version: %s\n", RemoteVersion);

    preferences.begin("esp_now_pref", false);
    current_ssid     = preferences.getString("wifi_ssid", "");
    current_password = preferences.getString("wifi_pass", "");
    preferences.getBytes("target_mac",  targetMac,  6);
    storedChannel = preferences.getInt("wifi_channel", 6);
    apSsid = preferences.getString("ap_ssid", default_ap_ssid);
    apPass = preferences.getString("ap_pass", default_ap_pass);
    // Sikr at targetMac ikke er alt-nul (erase_flash fjerner preference)
    bool isZero = true;
    for (int i = 0; i < 6; i++) if (targetMac[i]) { isZero = false; break; }
    if (isZero) { memset(targetMac, 0xFF, 6); }  // Fallback til broadcast
    preferences.end();

    if (current_ssid == "" || current_ssid == "DIT_WIFI_NAVN") {
        current_ssid = String(default_ssid); current_password = String(default_password);
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(current_ssid.c_str(), current_password.c_str());
    int timer = 0;
    while (WiFi.status() != WL_CONNECTED && timer < 20) { delay(500); Serial.print('.'); timer++; }

    if (WiFi.status() != WL_CONNECTED) {
        // AP-fallback: APSTA så STA-interfacet er aktivt → gyldig STA-MAC OG
        // ESP-NOW-modtagelse på STA-MAC'en (som gameboxen unicaster hello til).
        // Brug den GEMTE kanal (matcher gameboxene), ikke hardkodet 1.
        WiFi.disconnect(); WiFi.mode(WIFI_AP_STA);
        // WPA2 kræver mindst 8 tegn – ellers åbent AP (NULL)
        const char* apPw = (apPass.length() >= 8) ? apPass.c_str() : NULL;
        WiFi.softAP(apSsid.c_str(), apPw, storedChannel, 0, 8);   // kodeord + op til 8 klienter
        WiFi.setSleep(false);
        isAPMode = true; wifiChannel = storedChannel;
        Serial.println("\n╔══════════════════════════════════════╗");
        Serial.println(  "║  WiFi FEJLEDE – kører som Access Point ║");
        Serial.printf(   "║  SSID:  %-30s ║\n", apSsid.c_str());
        Serial.println(  "║  IP:    192.168.4.1                  ║");
        Serial.printf(   "║  Kanal: %-2d (gemt)                     ║\n", storedChannel);
        Serial.println(  "╚══════════════════════════════════════╝");
    } else {
        wifiChannel = WiFi.channel();
        WiFi.setSleep(false);
        Serial.println("\n╔══════════════════════════════════════╗");
        Serial.println(  "║         WiFi FORBUNDET               ║");
        Serial.printf(   "║  SSID:   %-29s ║\n", current_ssid.c_str());
        Serial.printf(   "║  IP:     %-29s ║\n", WiFi.localIP().toString().c_str());
        Serial.printf(   "║  Kanal:  %-2d                           ║\n", wifiChannel);
        Serial.printf(   "║  Web:    http://%-22s ║\n", WiFi.localIP().toString().c_str());
        Serial.println(  "╚══════════════════════════════════════╝");
    }

    // STA-MAC'en er den Remoten MODTAGER ESP-NOW på (også i AP_STA-fallback).
    // Gameboxenes Remote_mac skal pege på DENNE, så hello'en når frem.
    // (Remoten SENDER fra AP-interfacet, så gameboxen ser STA-MAC + 1 som afsender –
    //  det er normalt og betyder ikke at man skal bruge den adresse.)
    WiFi.macAddress(gatewayMacByte);
    snprintf(gatewayMacStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             gatewayMacByte[0],gatewayMacByte[1],gatewayMacByte[2],
             gatewayMacByte[3],gatewayMacByte[4],gatewayMacByte[5]);
    Serial.printf("[Remote] MAC (modtager): %s\n", gatewayMacStr);

    if (esp_now_init() != ESP_OK) { Serial.println("[ESP-NOW] Init fejl!"); return; }
    esp_now_register_recv_cb(OnDataRecv);

    // Broadcast altid registreret – bruges som fallback og til "alle joystiks"-sends
    uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    registerPeer(bcast);
    // Specifik joystik-MAC (hvis forskellig fra broadcast)
    bool tIsBcast = true;
    for (int i = 0; i < 6; i++) if (targetMac[i] != 0xFF) { tIsBcast = false; break; }
    if (!tIsBcast) registerPeer(targetMac);

    // Indlæs den persisterede gamebox-liste og registrér alle som peers
    loadGbList();
    gbRegisterAll();
    Serial.printf("[REMOTE] %d gemte gameboxe indlæst.\n", gbCount());

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
    ElegantOTA.begin(&server);   // OTA på /update
    server.begin();
    Serial.println("[SYSTEM] Klar – OTA tilgængelig på /update");
}

void loop() {
    ElegantOTA.loop();
    ws.cleanupClients();
    delay(10);
}
