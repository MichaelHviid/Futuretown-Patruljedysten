#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char index_html[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="da">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Remote</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; background: #f0f2f5; padding: 30px; display: flex; flex-direction: column; align-items: center; }
        h1 { color: #333; margin-bottom: 5px; }
        .gateway-info { background: #e3faf2; color: #0ca678; border: 1px solid #96f2d7; padding: 12px; border-radius: 6px; font-weight: bold; margin-bottom: 20px; text-align: center; width: 100%; max-width: 900px; box-sizing: border-box; }

        /* ── GAMEBOX REMOTE SEKTION (mørkelilla) ── */
        #gbBoxes { width:100%; display:flex; flex-direction:column; align-items:center; }
        .gamebox-section { background: #2a1a45; border: 2px solid #5a3a8a; border-radius: 14px; padding: 20px 24px; width: 100%; max-width: 900px; margin-bottom: 18px; box-sizing: border-box; display: flex; flex-direction: column; gap: 14px; box-shadow: 0 4px 18px rgba(120,60,200,0.3); }
        .gamebox-section h2 { color: #b088ff; margin: 0; font-size: 1.1rem; }
        .gb-row { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
        .gb-label { color: rgba(190,160,255,0.75); font-size: 0.72rem; text-transform: uppercase; letter-spacing: 0.05em; min-width: 70px; }
        .gb-input { padding: 6px 10px; border-radius: 6px; border: 1px solid #6a48a8; background: #1c1030; color: #c9a3ff; font-family: monospace; font-size: 0.85rem; flex: 1; max-width: 240px; }
        .gb-mac-ro { font-family: monospace; font-size: 0.9rem; color: #c9a3ff; flex: 1; }
        .gb-save  { background: #4a2f7a; color: #c9a3ff; border: 1px solid #6a48a8; border-radius: 6px; padding: 5px 12px; cursor: pointer; font-size: 0.9rem; }
        .gb-save:hover { background: #5e3d9a; }
        .gb-del   { background: #5a1515; color: #ff9090; border: 1px solid #aa3333; border-radius: 6px; padding: 5px 12px; cursor: pointer; font-size: 0.9rem; }
        .gb-del:hover { background: #7a2020; }
        .hello-add { background: #4a2f7a; color:#e0ccff; padding:4px 10px; font-size:12px; border:none; border-radius:5px; cursor:pointer; }
        .hello-add:hover { background:#5e3d9a; }
        .gb-btns  { display: flex; gap: 10px; flex-wrap: wrap; }
        .gb-btn   { border: none; border-radius: 8px; padding: 9px 18px; font-size: 0.85rem; font-weight: bold; cursor: pointer; transition: opacity 0.1s; }
        .gb-btn:hover  { opacity: 0.8; }
        .gb-btn:active { opacity: 0.6; transform: scale(0.97); }
        .gb-red    { background: #5a1515; color: #ff9090; border: 1px solid #aa3333; }
        .gb-green  { background: #155a20; color: #90ff90; border: 1px solid #33aa44; }
        .gb-query  { background: #1a3a5c; color: #90d0ff; border: 1px solid #2266aa; }
        .gb-setcfg { background: #2a1a5c; color: #c090ff; border: 1px solid #6644aa; }
        .gb-state  { background: #0a1e38; border: 1px solid #2266aa; border-radius: 8px; padding: 10px 16px; color: #80c8ff; font-size: 0.9rem; display: flex; gap: 20px; flex-wrap: wrap; }
        .gb-state span { opacity: 0.7; font-size: 0.75rem; display: block; }
        .gb-state strong { font-size: 1.1rem; }

        /* ── ORIGINAL STYLING (bevaret) ── */
        .config-container { display: flex; gap: 20px; width: 100%; max-width: 900px; margin-bottom: 30px; flex-wrap: wrap; }
        .config-card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); flex: 1; min-width: 280px; display: flex; flex-direction: column; gap: 10px; }
        .config-card h3 { margin: 0; border-bottom: 2px solid #eee; padding-bottom: 5px; }
        .input-group { display: flex; flex-direction: column; gap: 5px; font-size: 14px; }
        .input-group input { padding: 8px; border: 1px solid #ccc; border-radius: 4px; }
        button { padding: 10px; font-size: 15px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }
        button:hover { background: #0056b3; }
        .btn-success { background: #2b8a3e; } .btn-success:hover { background: #237032; }
        .btn-danger  { background: #e03131; margin-top: 15px; width: 100%; max-width: 900px; padding: 15px; font-size: 16px; }
        .btn-danger:hover { background: #c92a2a; }
        .config-toggle { width:100%; max-width:900px; margin-top:20px; background:#495057; padding:14px; font-size:16px; }
        .config-toggle:hover { background:#343a40; }
        #configPanel { width:100%; display:flex; flex-direction:column; align-items:center; margin-top:15px; }
        .container { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 20px; width: 100%; max-width: 900px; margin-bottom: 30px; }
        .box { background: #7f8c8d; border-radius: 12px; display: flex; flex-direction: column; justify-content: space-between; align-items: center; color: white; font-weight: bold; font-size: 16px; cursor: pointer; text-align: center; padding: 15px; box-sizing: border-box; min-height: 160px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); transition: transform 0.2s; }
        .box:hover { transform: translateY(-3px); }
        .box-red.active    { background: #ee5253 !important; } .box-red    { background: #bdc3c7; } .box-red:hover    { background: #ff6b6b; }
        .box-yellow.active { background: #ff9f43 !important; color:#333; } .box-yellow { background: #bdc3c7; } .box-yellow:hover { background: #feca57; color:#333; }
        .box-green.active  { background: #10ac84 !important; } .box-green  { background: #bdc3c7; } .box-green:hover  { background: #1dd1a1; }
        .box-blue.active   { background: #2e86de !important; } .box-blue   { background: #bdc3c7; } .box-blue:hover   { background: #54a0ff; }
        .box-grey.active   { background: #2c3e50 !important; } .box-grey   { background: #95a5a6; } .box-grey:hover   { background: #7f8c8d; }
        .slider-container { width:100%; margin-top:10px; background:rgba(0,0,0,0.15); padding:5px; border-radius:6px; box-sizing:border-box; }
        .slider-container label { font-size:11px; display:block; margin-bottom:2px; font-weight:normal; }
        .slider-container input { width:100%; cursor:pointer; margin:0; }
        .led-section { background:white; width:100%; max-width:900px; padding:25px; border-radius:8px; box-shadow:0 4px 6px rgba(0,0,0,0.05); box-sizing:border-box; margin-bottom:30px; }
        .led-section h2 { margin-top:0; border-bottom:2px solid #eee; padding-bottom:10px; color:#333; }
        .led-grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(150px,1fr)); gap:15px; margin-bottom:25px; }
        .led-btn { color:white; text-shadow:0 1px 2px rgba(0,0,0,0.4); padding:15px; font-size:14px; border-radius:6px; box-shadow:0 2px 5px rgba(0,0,0,0.1); border:none; cursor:pointer; font-weight:bold; transition:transform 0.1s; }
        .led-btn:hover { transform:translateY(-2px); filter:brightness(1.1); }
        .master-grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(160px,1fr)); gap:15px; }
        .log-section { background:white; width:100%; max-width:900px; padding:20px; border-radius:8px; box-shadow:0 4px 6px rgba(0,0,0,0.05); box-sizing:border-box; }
        .log-table { width:100%; border-collapse:collapse; font-size:13px; }
        .log-table th, .log-table td { padding:10px; border-bottom:1px solid #eee; text-align:left; }
        .badge { padding:3px 6px; border-radius:4px; font-weight:bold; color:white; font-size:11px; }
        .badge-this { background:#40c057; }
        .status { margin-top:20px; font-size:14px; color:#666; }
        .gb-slider { flex:1; max-width:240px; }
        /* ── Mobil: fyld skærmen, undgå spildt plads i siderne ── */
        @media (max-width: 640px) {
            body { padding: 10px; }
            h1 { font-size: 1.4rem; text-align: center; }
            .gamebox-section { padding: 16px; gap: 10px; }
            .gb-label { min-width: 56px; }
            .gb-input, .gb-num, .gb-slider { max-width: none; }
            .config-container { flex-direction: column; }   /* stabl kort lodret */
            .config-card { min-width: 0; }
            .led-section { padding: 16px; }
            .master-grid { grid-template-columns: repeat(auto-fill, minmax(120px, 1fr)); }
        }
    </style>
</head>
<body>
<div id="app">
    <h1>ESP32 Remote & Joystik Gateway</h1>
    <div class="gateway-info">
        Gateway MAC: <strong id="myMac">—</strong> &nbsp;·&nbsp;
        Aktiv kanal: <strong id="myChannel" style="color:#d9480f;">—</strong> &nbsp;·&nbsp;
        Version: <strong id="fwVersion">—</strong>
    </div>

    <!-- ══════════════════════════════════════════
         GAMEBOX REMOTE SEKTION
    ══════════════════════════════════════════ -->
    <!-- Gamebox-bokse (fyldes af JS fra gb_list) -->
    <div id="gbBoxes"></div>

    <!-- ══════════════════════════════════════════
         JOYSTIK KONFIG (uændret fra original)
    ══════════════════════════════════════════ -->
    <!-- LED-sektion -->
    <div class="led-section">
        <h2>Styr Alle Dioder (ID 255)</h2>
        <div class="master-grid">
            <button class="led-btn" style="background:#ff4d4d;padding:15px" onclick="clickLed(255,255,0,0)">🔴 Alle Rød</button>
            <button class="led-btn" style="background:#ffca28;color:#333;padding:15px" onclick="clickLed(255,255,200,0)">🟡 Alle Gul</button>
            <button class="led-btn" style="background:#4caf50;padding:15px" onclick="clickLed(255,0,255,0)">🟢 Alle Grøn</button>
            <button class="led-btn" style="background:#2196f3;padding:15px" onclick="clickLed(255,0,0,255)">🔵 Alle Blå</button>
            <button class="led-btn" style="background:#37474f;padding:15px" onclick="clickLed(255,0,0,0)">⚫ Sluk Alle</button>
        </div>
    </div>

    <!-- Gamebox Hello-log -->
    <div class="log-section" style="margin-bottom:30px">
        <h3>📢 Gameboxe der har meldt sig (Hello)</h3>
        <table class="log-table">
            <thead><tr><th>Navn</th><th>MAC</th><th>Antal</th><th>Sidst set</th><th>Styr</th></tr></thead>
            <tbody id="helloBody"></tbody>
        </table>
        <p id="helloEmpty" style="color:#999;font-size:13px">Ingen gameboxe har meldt sig endnu…</p>
    </div>

    <!-- Log -->
    <div class="log-section">
        <h3>System Log</h3>
        <table class="log-table">
            <thead><tr><th>Type</th><th>Fra → Til</th><th>Data</th></tr></thead>
            <tbody id="logBody"></tbody>
        </table>
    </div>

    <!-- Config-knap + skjult panel (i bunden) -->
    <button id="btnConfigToggle" class="config-toggle">⚙️ Config</button>
    <div id="configPanel" style="display:none">
        <div class="config-container">
            <div class="config-card">
                <h3>ESP-NOW Enhed (Joystik / Afspiller)</h3>
                <div class="input-group">
                    <label>Mål-ESP MAC-adresse:</label>
                    <input id="targetMac" placeholder="AA:BB:CC:DD:EE:FF" maxlength="17"
                           style="text-transform:uppercase; text-align:center;" />
                </div>
                <button id="btnSaveMac">Gem MAC i ESP32</button>
            </div>
            <div class="config-card">
                <h3>ESP-NOW Kanal</h3>
                <div class="input-group">
                    <label>Kanal (1-13) – skal matche dine gameboxe:</label>
                    <input id="savedChannel" type="number" min="1" max="13"
                           style="text-align:center;" />
                </div>
                <p style="font-size:0.75rem; color:rgba(100,160,220,0.6); margin:4px 0 10px">
                    Bruges når der ikke er router (AP-mode). Uden fælles kanal kan Remoten ikke høre gameboxene.
                </p>
                <button class="btn-success" id="btnSaveChannel">Gem Kanal & Genstart</button>
            </div>
            <div class="config-card">
                <h3>Router Netværksindstillinger</h3>
                <div class="input-group">
                    <label>Wi-Fi Navn (SSID):</label>
                    <input id="wifiSsid" placeholder="Dit netværksnavn" />
                </div>
                <div class="input-group">
                    <label>Wi-Fi Kodeord:</label>
                    <input id="wifiPassword" type="password" placeholder="Dit kodeord" />
                </div>
                <button class="btn-success" id="btnSaveWifi">Gem Wi-Fi & Genstart</button>
            </div>
            <div class="config-card">
                <h3>Remote Access Point</h3>
                <div class="input-group">
                    <label>AP-navn (SSID):</label>
                    <input id="apSsid" placeholder="GamersRemote" maxlength="32" />
                </div>
                <div class="input-group">
                    <label>AP-kodeord (min. 8 tegn, tom = åbent):</label>
                    <input id="apPass" type="password" placeholder="mindst 8 tegn" maxlength="63" />
                </div>
                <button class="btn-success" id="btnSaveAp">Gem AP & Genstart</button>
            </div>
        </div>
        <button class="btn-danger" id="btnTurbo">🚀 AKTIVER ESP-NOW ONLY MODE</button>
    </div>

    <div class="status">WebSocket: <span id="wsStatus">Afbrudt</span></div>
</div>

<script>
// ── Vanilla JS (ingen eksterne afhængigheder – virker offline) ──
const $ = id => document.getElementById(id);

// ── WebSocket ──
const socket = new WebSocket(`ws://${window.location.host}/ws`);
const send = (obj) => { try { socket.send(JSON.stringify(obj)); } catch(e){ alert('Ikke forbundet endnu.'); } };
socket.onopen  = () => { $('wsStatus').textContent = 'Forbundet'; };
socket.onclose = () => { $('wsStatus').textContent = 'Afbrudt'; };

function clickLed(id, r, g, b) { send({ action:'trigger_led_struct', ledId:id, r, g, b }); }

// ══ Gamebox-bokse (flere, per-MAC) ══════════════════════════════════════════
let boxes = [];                 // fra gb_list: [{mac, name}, ...]
const boxThrottle = {};         // mac → sidste config-send (ms)
function boxHeading(b) { return (b.name && b.name.trim()) ? b.name : (b.mac || 'Ny gamebox'); }
function esc(s) { return String(s).replace(/[<>&"']/g, ''); }
function isControlled(mac) { return boxes.some(b => b.mac && b.mac.toUpperCase() === String(mac).toUpperCase()); }

function makeBox(b, isFirst) {
    const el = document.createElement('div');
    el.className = 'gamebox-section';
    el.dataset.mac = b.mac || '';
    const macCtrl = isFirst
        ? `<input class="gb-mac gb-input" placeholder="AA:BB:CC:DD:EE:FF"
                  style="flex:0 0 auto; width:168px; box-sizing:border-box; text-transform:uppercase"
                  maxlength="17" value="${esc(b.mac||'')}">
           <button class="gb-save gb-gem">💾 Gem</button>`
        : `<span class="gb-mac-ro">${esc(b.mac)}</span>
           <button class="gb-del">🗑 Slet</button>`;
    const nameRow = isFirst
        ? `<div class="gb-row"><span class="gb-label">Navn</span>
              <input class="gb-name gb-input" placeholder="fx Gamebox#12 eller Alle" maxlength="20"
                     style="flex:1; max-width:240px" value="${esc(b.name||'')}"></div>`
        : '';
    el.innerHTML =
        `<h2>📡 <span class="gb-title">${esc(boxHeading(b))}</span></h2>
         ${nameRow}
         <div class="gb-row"><span class="gb-label">Mac</span>${macCtrl}</div>
         <div class="gb-row"><span class="gb-label">Brightness</span>
             <input class="gb-bright gb-slider" type="range" min="0" max="255" value="100">
             <strong class="gb-bright-val" style="color:#c9a3ff; min-width:38px; text-align:right">100</strong></div>
         <div class="gb-row"><span class="gb-label">Volume %</span>
             <input class="gb-vol gb-slider" type="range" min="0" max="100" value="100">
             <strong class="gb-vol-val" style="color:#c9a3ff; min-width:38px; text-align:right">100</strong></div>
         <div class="gb-btns">
             <button class="gb-btn gb-red">🔴 Skift spil</button>
             <button class="gb-btn gb-green">🟢 Start</button></div>
         <div class="gb-state-line" style="color:rgba(200,160,255,0.5); font-size:0.8rem">Status hentes automatisk når gameboxen melder sig…</div>`;

    const bright = el.querySelector('.gb-bright'), vol = el.querySelector('.gb-vol');
    const brightVal = el.querySelector('.gb-bright-val'), volVal = el.querySelector('.gb-vol-val');
    const macOf = () => el.dataset.mac;
    function sendCfg(force) {
        const m = macOf(); if (!m) return;
        const now = Date.now();
        if (!force && now - (boxThrottle[m]||0) < 150) return;
        boxThrottle[m] = now;
        send({ action:'gb_config', mac:m, brightness: parseInt(bright.value), volume: parseInt(vol.value) });
    }
    bright.addEventListener('input',  () => { brightVal.textContent = bright.value; sendCfg(false); });
    bright.addEventListener('change', () => sendCfg(true));
    vol.addEventListener('input',     () => { volVal.textContent = vol.value; sendCfg(false); });
    vol.addEventListener('change',    () => sendCfg(true));
    el.querySelector('.gb-red').onclick   = () => { if (macOf()) send({ action:'gb_red',   mac: macOf() }); };
    el.querySelector('.gb-green').onclick = () => { if (macOf()) send({ action:'gb_green', mac: macOf() }); };
    if (isFirst) {
        el.querySelector('.gb-gem').onclick = () => {
            const v = el.querySelector('.gb-mac').value.trim();
            const nm = el.querySelector('.gb-name') ? el.querySelector('.gb-name').value.trim() : '';
            if (v.length === 17) send({ action:'gb_save_first', mac: v, name: nm });
            else alert('Indtast en gyldig MAC (AA:BB:CC:DD:EE:FF).');
        };
    } else {
        el.querySelector('.gb-del').onclick = () => {
            if (confirm('Fjern ' + boxHeading(b) + ' fra listen?')) send({ action:'gb_remove', mac: b.mac });
        };
    }
    return el;
}

function renderBoxes() {
    const cont = $('gbBoxes');
    cont.innerHTML = '';
    let list;
    if (!boxes.length) {
        list = [{ mac:'', name:'' }];                 // altid mindst én redigerbar boks
    } else {
        // Øverste (redigerbare) boks bliver i toppen; resten sorteres alfabetisk efter navn
        const rest = boxes.slice(1).sort((a, b) =>
            boxHeading(a).localeCompare(boxHeading(b), 'da', { sensitivity: 'base', numeric: true }));
        list = [boxes[0], ...rest];
    }
    list.forEach((b, i) => cont.appendChild(makeBox(b, i === 0)));
    renderHellos();   // opdatér "Opret"-knapper (skjul for allerede-tilføjede)
}

function addGb(mac, name) { send({ action:'gb_add', mac, name }); }

// ── Gamebox Hello-log (dedupliceret pr. MAC) ──
const helloMap = {};   // mac → { name, mac, count, baseAgo, seen }
function fmtAgo(sec) {
    if (sec < 5)    return 'lige nu';
    if (sec < 60)   return sec + 's siden';
    if (sec < 3600) return Math.floor(sec/60) + 'm siden';
    return Math.floor(sec/3600) + 't siden';
}
function renderHellos() {
    const keys = Object.keys(helloMap);
    $('helloEmpty').style.display = keys.length ? 'none' : 'block';
    const rows = keys.map(k => helloMap[k])
        .sort((a,b) => b.seen - a.seen)   // seneste øverst
        .map(h => {
            const age = h.baseAgo + Math.floor((Date.now() - h.seen) / 1000);
            const ctrl = isControlled(h.mac)
                ? '<span style="color:#40c057">✓ tilføjet</span>'
                : `<button class="hello-add" onclick="addGb('${esc(h.mac)}','${esc(h.name)}')">➕ Opret</button>`;
            return `<tr><td><b>${esc(h.name)}</b></td>` +
                   `<td style="font-family:monospace;font-size:11px">${esc(h.mac)}</td>` +
                   `<td>×${h.count}</td><td>${fmtAgo(age)}</td><td>${ctrl}</td></tr>`;
        }).join('');
    $('helloBody').innerHTML = rows;
}
setInterval(renderHellos, 3000);   // hold "sidst set" opdateret

// ── Log ──
const logRows = [];
function addLog(e) {
    logRows.unshift(e);
    if (logRows.length > 15) logRows.pop();
    $('logBody').innerHTML = logRows.map(x =>
        `<tr><td><span class="badge badge-this">${x.targetType||''}</span></td>` +
        `<td style="font-family:monospace;font-size:11px">Fra: ${x.sender||''}<br>Til: ${x.receiver||''}</td>` +
        `<td>${x.details||''}</td></tr>`).join('');
}

// ── Indgående beskeder ──
socket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.type === 'config') {
        $('targetMac').value    = data.savedMac || '';
        $('myMac').textContent  = data.myMac || '—';
        $('myChannel').textContent = (data.myChannel ?? '—');
        $('fwVersion').textContent = data.version || '—';
        $('savedChannel').value = data.savedChannel || 1;
        $('apSsid').value       = data.apSsid || '';
        $('apPass').value       = data.apPass || '';
        $('wifiSsid').value     = data.wifiSsid || '';
    }
    else if (data.type === 'gb_list') {
        boxes = data.boxes || [];
        renderBoxes();
    }
    else if (data.type === 'gamebox_state') {
        const el = [...document.querySelectorAll('#gbBoxes .gamebox-section')]
                   .find(e => e.dataset.mac.toUpperCase() === String(data.mac||'').toUpperCase());
        if (el) {
            el.querySelector('.gb-bright').value     = data.brightness;
            el.querySelector('.gb-vol').value        = data.volume;
            el.querySelector('.gb-bright-val').textContent = data.brightness;
            el.querySelector('.gb-vol-val').textContent    = data.volume;
            el.querySelector('.gb-state-line').textContent =
                `Spil ${data.game_id} · Brightness ${data.brightness} · Volume ${data.volume}%`;
        }
    }
    else if (data.type === 'gamebox_hello') {
        helloMap[data.sender] = {
            name: data.name || '?', mac: data.sender,
            count: data.count || 1, baseAgo: data.agoSec || 0, seen: Date.now()
        };
        renderHellos();
        // Auto-hent status hvis gameboxen allerede er på styringslisten
        if (isControlled(data.sender)) send({ action:'gb_query', mac: data.sender });
    }
    else if (data.type === 'joystick_ping' || data.type === 'joystick_wifi' || data.type === 'esp_now_recv') {
        if (data.type === 'joystick_ping') { data.targetType = 'Ping'; data.receiver = '(Remote)'; }
        else if (data.type === 'joystick_wifi') { data.targetType = 'WiFi'; data.receiver = '(Remote)'; }
        addLog(data);
    }
};

// ── Knap-handlere ──
$('btnSaveMac').onclick    = () => { send({ action:'save_mac', mac: $('targetMac').value }); alert('Joystik MAC gemt!'); };
$('btnSaveWifi').onclick   = () => send({ action:'save_wifi', ssid: $('wifiSsid').value, pass: $('wifiPassword').value });
$('btnSaveAp').onclick     = () => { const p = $('apPass').value; if (p.length > 0 && p.length < 8) { alert('Kodeord skal være mindst 8 tegn (eller tomt for åbent AP).'); return; } if (confirm('Gem AP-indstillinger og genstart?')) send({ action:'save_ap', ssid: $('apSsid').value, pass: p }); };
$('btnSaveChannel').onclick= () => { const c = parseInt($('savedChannel').value); if (confirm('Gem kanal ' + c + ' og genstart?')) send({ action:'save_channel', channel: c }); };
$('btnTurbo').onclick      = () => { if (confirm('Advarsel: lukker websiden ned. Fortsæt?')) send({ action:'go_turbo' }); };

// Config-panel: vis/skjul
$('btnConfigToggle').onclick = () => {
    const p = $('configPanel');
    const show = (p.style.display === 'none');
    p.style.display = show ? 'flex' : 'none';
    $('btnConfigToggle').textContent = show ? '⚙️ Skjul config' : '⚙️ Config';
};

renderBoxes();   // vis den tomme redigerbare boks indtil gb_list ankommer
</script>
</body>
</html>
)rawhtml";

#endif // WEB_PAGE_H
