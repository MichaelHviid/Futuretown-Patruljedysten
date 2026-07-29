<script setup>

</script>

<template>
  <div class="topnav" style="display:flex">
    <div style="margin:auto auto auto 10px">
      <h1>Futuretown Gamebox</h1>
    </div>
    <div style="margin: auto 20px auto auto;color: white;text-align: center ">
      <a href="update"><button class="button-on">Firmware</button></a>
      <br><br>
      <div style="width:fit-content; margin: auto 10px auto auto;text-align: right;">
        Firmware : {{ config.FirmwareVersion }}<br>
        Admin : {{wdm.version}}
      </div>
    </div>
  </div>

  <div class="content">

    <!-- ═══════════════════════════════════════════════════
         LIVE JOYSTICK STATUS
    ═══════════════════════════════════════════════════ -->
    <!-- ═══════════════════════════════════════════════════
         GAMEBOX IDENTITET
    ═══════════════════════════════════════════════════ -->
    <div class="gamebox-identity">
      <span class="identity-label">Gamebox MAC</span>
      <span class="identity-mac">{{ ownMac }}</span>
      <span class="identity-sep">·</span>
      <span class="identity-label">Kanal</span>
      <span class="identity-ch">{{ ownChannel }}</span>
    </div>

    <h2>Joystick Status
      <span class="ws-badge" :class="wsConnected ? 'ws-on' : 'ws-off'">
        {{ wsConnected ? '● Live' : '○ Offline' }}
      </span>
    </h2>

    <div class="joystick-row">
      <!-- GREEN joystick (Team A) -->
      <div class="joystick joy-green">
        <div class="joy-header">
          <div class="joy-label">🟢 Grønt joystick (Team A)</div>
          <div class="joy-pts">
            <div class="joy-pt" title="ID 3">●</div>
            <div class="joy-pt" title="ID 2">●</div>
            <div class="joy-pt pt-lit" title="ID 1">●</div>
          </div>
        </div>
        <div class="joy-mac-row">
          <span class="joy-mac">{{ config.Green_mac || '—' }}</span>
          <div class="joy-type-mini">
            <button :class="['jtm-btn', config.JoystickTypeA === 0 ? 'jtm-active' : '']"
                    @click="config.JoystickTypeA = 0; postConfig()" title="Lille joystik (4 knapper, lyd)">4</button>
            <button :class="['jtm-btn', config.JoystickTypeA === 1 ? 'jtm-active' : '']"
                    @click="config.JoystickTypeA = 1; postConfig()" title="Stort joystik (11 knapper, ingen lyd)">11</button>
          </div>
        </div>
        <div v-if="joyIpA" class="joy-ip">📡 {{ joyIpA }}</div>
        <!-- Lille joystik: 4 runde knapper -->
        <div v-if="config.JoystickTypeA !== 1" class="joy-buttons">
          <div class="joy-btn btn-red"    :class="{ active: joyState[0][0] }">Rød</div>
          <div class="joy-btn btn-yellow" :class="{ active: joyState[0][1] }">Gul</div>
          <div class="joy-btn btn-green"  :class="{ active: joyState[0][2] }">Grøn</div>
          <div class="joy-btn btn-blue"   :class="{ active: joyState[0][3] }">Blå</div>
        </div>
        <!-- Stort joystik: 7-kolonne layout -->
        <div v-else class="joy-buttons-large">
          <div v-for="col in bigJoyCols" :key="col.buttons[0].label" class="jbl-col">
            <button v-for="btn in col.buttons" :key="btn.idx"
                    class="jbl-btn"
                    :class="{ 'jbl-active': joyState[0][btn.idx] }"
                    :style="joyState[0][btn.idx]
                      ? { background: btn.hex, boxShadow: '0 0 10px ' + btn.hex }
                      : { background: 'rgba(0,0,0,0.35)', borderColor: btn.hex }"
                    @click="sendLedCmd(0, btn.id, btn.r, btn.g, btn.b)"
                    :title="'Knap ' + btn.label">{{ btn.label }}</button>
            <!-- Fylder plads i kolonner med kun 1 knap -->
            <div v-if="col.buttons.length === 1" class="jbl-spacer"></div>
          </div>
          <button class="jbl-off" @click="sendLedCmd(0,255,0,0,0)" title="Sluk alle">✕</button>
        </div>
        <!-- Sound-row: lyd kun for lille joystik, WiFi altid -->
        <div class="sound-row">
          <span class="sound-label">Lyd →</span>
          <template v-if="config.JoystickTypeA !== 1">
            <button v-for="n in 4" :key="n" class="snd-btn" @click="sendSound(0, n)">{{ n }}</button>
            <button class="snd-btn snd-special" @click="sendSound(0, 99)">♪</button>
          </template>
          <button class="snd-btn snd-wifi"  @click="sendSound(0, 250)" title="Tænd WiFi (ID 250)">📶</button>
          <button class="snd-btn snd-sleep" @click="sendSound(0, 251)" title="Joystik i dvale (ID 251)">💤</button>
        </div>
      </div>

      <!-- RED joystick (Team B) -->
      <div class="joystick joy-red">
        <div class="joy-header">
          <div class="joy-label">🔴 Rødt joystick (Team B)</div>
          <div class="joy-pts">
            <div class="joy-pt" title="ID 3">●</div>
            <div class="joy-pt" title="ID 2">●</div>
            <div class="joy-pt pt-lit" title="ID 1">●</div>
          </div>
        </div>
        <div class="joy-mac-row">
          <span class="joy-mac">{{ config.Red_mac || '—' }}</span>
          <div class="joy-type-mini">
            <button :class="['jtm-btn', config.JoystickTypeB === 0 ? 'jtm-active' : '']"
                    @click="config.JoystickTypeB = 0; postConfig()" title="Lille joystik (4 knapper, lyd)">4</button>
            <button :class="['jtm-btn', config.JoystickTypeB === 1 ? 'jtm-active' : '']"
                    @click="config.JoystickTypeB = 1; postConfig()" title="Stort joystik (11 knapper, ingen lyd)">11</button>
          </div>
        </div>
        <div v-if="joyIpB" class="joy-ip">📡 {{ joyIpB }}</div>
        <!-- Lille joystik: 4 runde knapper -->
        <div v-if="config.JoystickTypeB !== 1" class="joy-buttons">
          <div class="joy-btn btn-red"    :class="{ active: joyState[1][0] }">Rød</div>
          <div class="joy-btn btn-yellow" :class="{ active: joyState[1][1] }">Gul</div>
          <div class="joy-btn btn-green"  :class="{ active: joyState[1][2] }">Grøn</div>
          <div class="joy-btn btn-blue"   :class="{ active: joyState[1][3] }">Blå</div>
        </div>
        <!-- Stort joystik: 7-kolonne layout -->
        <div v-else class="joy-buttons-large">
          <div v-for="col in bigJoyCols" :key="col.buttons[0].label" class="jbl-col">
            <button v-for="btn in col.buttons" :key="btn.idx"
                    class="jbl-btn"
                    :class="{ 'jbl-active': joyState[1][btn.idx] }"
                    :style="joyState[1][btn.idx]
                      ? { background: btn.hex, boxShadow: '0 0 10px ' + btn.hex }
                      : { background: 'rgba(0,0,0,0.35)', borderColor: btn.hex }"
                    @click="sendLedCmd(1, btn.id, btn.r, btn.g, btn.b)"
                    :title="'Knap ' + btn.label">{{ btn.label }}</button>
            <div v-if="col.buttons.length === 1" class="jbl-spacer"></div>
          </div>
          <button class="jbl-off" @click="sendLedCmd(1,255,0,0,0)" title="Sluk alle">✕</button>
        </div>
        <div class="sound-row">
          <span class="sound-label">Lyd →</span>
          <template v-if="config.JoystickTypeB !== 1">
            <button v-for="n in 4" :key="n" class="snd-btn" @click="sendSound(1, n)">{{ n }}</button>
            <button class="snd-btn snd-special" @click="sendSound(1, 99)">♪</button>
          </template>
          <button class="snd-btn snd-wifi"  @click="sendSound(1, 250)" title="Tænd WiFi (ID 250)">📶</button>
          <button class="snd-btn snd-sleep" @click="sendSound(1, 251)" title="Joystik i dvale (ID 251)">💤</button>
        </div>
      </div>
    </div>

    <!-- ═══════════════════════════════════════════════════
         OPDAGEDE ENHEDER (auto-discovery)
    ═══════════════════════════════════════════════════ -->
    <h2>Opdagede enheder <span style="font-size:0.7rem;font-weight:normal;color:#666">via ESP-NOW broadcast</span></h2>
    <div class="log-table-wrap">
      <table class="log-table" v-if="discoveredDevices.length">
        <thead>
          <tr>
            <th>Navn</th>
            <th>MAC-adresse</th>
            <th>IP</th>
            <th>Pakker</th>
            <th>Sidst set</th>
            <th colspan="2">Sæt som</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="d in discoveredDevices" :key="d.mac">
            <td>
              <span v-if="d.name" class="dev-name">{{ d.name }}</span>
              <span v-else class="dev-name-unknown">—</span>
            </td>
            <td class="mono">{{ d.mac }}</td>
            <td class="mono">{{ d.ip || '—' }}</td>
            <td>{{ d.count }}</td>
            <td class="mono">{{ d.lastSeen }}</td>
            <td><button class="set-btn set-green"  @click="setMac('green',  d.mac)">→ Grøn A</button></td>
            <td><button class="set-btn set-red"    @click="setMac('red',    d.mac)">→ Rød B</button></td>
            <td><button class="set-btn set-remote" @click="setMac('remote', d.mac)">→ Remote</button></td>
          </tr>
        </tbody>
      </table>
      <p v-else class="log-empty">Ingen enheder opdaget endnu — joystick-ESPs sender automatisk broadcasts.</p>
    </div>

    <!-- ═══════════════════════════════════════════════════
         BUTTON LOG
    ═══════════════════════════════════════════════════ -->
    <h2>Knap Log <span style="font-size:0.7rem;font-weight:normal;color:#666">(seneste 10)</span></h2>
    <div class="log-table-wrap">
      <table class="log-table" v-if="buttonLog.length">
        <thead>
          <tr>
            <th>Tid</th>
            <th>Joystick</th>
            <th>MAC</th>
            <th>Knap</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(entry, i) in buttonLog" :key="i">
            <td class="mono">{{ entry.time }}</td>
            <td>
              <span class="joy-tag" :class="entry.joystick === 0 ? 'tag-green' : entry.joystick === 1 ? 'tag-red' : 'tag-unknown'">
                {{ entry.joystick === 0 ? 'Grøn A' : entry.joystick === 1 ? 'Rød B' : 'Ukendt' }}
              </span>
            </td>
            <td class="mono">{{ entry.mac }}</td>
            <td><strong>{{ entry.btnName }}</strong></td>
            <td>
              <span class="state-tag" :class="entry.isPressed ? 'tag-pressed' : 'tag-released'">
                {{ entry.isPressed ? '▼ Trykket' : '▲ Sluppet' }}
              </span>
            </td>
          </tr>
        </tbody>
      </table>
      <p v-else class="log-empty">Ingen knapaktivitet endnu — forbind joysticks via ESP-NOW.</p>
    </div>

    <!-- ═══════════════════════════════════════════════════
         CONFIG SECTIONS (uændret + tilføjet MAC-felter)
    ═══════════════════════════════════════════════════ -->
    <h2>Gamebox knapper</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.red_pin"   icon="fa-circle" title="Rød knap (GPIO)"   :min="0" :max="39" :oldsettings="config.red_pin"   @save="postConfig"/>
      <oneConfig v-model="config.green_pin" icon="fa-circle" title="Grøn knap (GPIO)"  :min="0" :max="39" :oldsettings="config.green_pin" @save="postConfig"/>
    </div>

    <!-- ═══════════════════════════════════════════════════
         REMOTE CONTROL
    ═══════════════════════════════════════════════════ -->
    <h2>Remote Control</h2>
    <div class="remote-row" v-if="!loading">
      <div class="remote-card">
        <div class="joy-header">
          <div class="joy-label">📡 Remote ESP</div>
        </div>
        <div class="remote-mac-row">
          <label class="remote-label">MAC</label>
          <input v-model="config.Remote_mac" class="remote-mac-input" placeholder="XX:XX:XX:XX:XX:XX" />
          <button class="remote-mac-save" @click="postConfig" title="Gem Remote MAC">💾</button>
        </div>
        <div class="remote-fields">
          <div class="remote-field">
            <label class="remote-label">Brightness (0-255)</label>
            <input v-model.number="config.LEDBRIGHTNESS" type="number" :min="0" :max="255"
                   class="remote-input" @change="postConfig" />
          </div>
          <div class="remote-field">
            <label class="remote-label">Global Volume (0-100%)</label>
            <input v-model.number="config.GlobalVolume" type="number" :min="0" :max="100"
                   class="remote-input" @change="postConfig" />
          </div>
          <div class="remote-field">
            <label class="remote-label">Dvale (min, 0=slukket)</label>
            <input v-model.number="config.SleepMin" type="number" :min="0" :max="240"
                   class="remote-input" @change="postConfig"
                   :title="config.SleepMin > 0 ? 'LED slukker efter ' + Math.round(config.SleepMin/2) + ' min, dvale efter ' + config.SleepMin + ' min' : 'Dvale deaktiveret'" />
          </div>
        </div>
        <div class="remote-btns">
          <button class="remote-action-btn remote-red"   @click="remoteAction('remote_red_btn')"
                  title="Skift spil (samme som fysisk rød knap)">🔴 Skift spil</button>
          <button class="remote-action-btn remote-green" @click="remoteAction('remote_green_btn')"
                  title="Start klar-fase (samme som fysisk grøn knap)">🟢 Start</button>
          <button class="remote-action-btn remote-send"  @click="remoteAction('send_to_remote')"
                  title="Send brightness, volume og aktuelt spil til remote-ESP">📡 Send til remote</button>
        </div>
        <p class="remote-note">Kun aktiv i lobbyen · Kræver gamebox i STATE_WAITING</p>
      </div>
    </div>

    <h2>Lyde (ESP-NOW til joystiks)</h2>
    <div class="card-grid" v-if="!loading">
      <soundConfig title="Spilstart"    v-model:id="config.GameStart_id"   v-model:vol="config.GameStart_vol"  @save="postConfig" @test="testSound"/>
      <soundConfig title="Spil slut"    v-model:id="config.GameSlut_id"    v-model:vol="config.GameSlut_vol"   @save="postConfig" @test="testSound"/>
      <soundConfig title="Vindertune"   v-model:id="config.WinnerTune_id"  v-model:vol="config.WinnerTune_vol" @save="postConfig" @test="testSound"/>
      <soundConfig title="Hit (global)" v-model:id="config.HitTune_id"     v-model:vol="config.HitTune_vol"    @save="postConfig" @test="testSound"/>
      <soundConfig title="Miss (global)"v-model:id="config.MissTune_id"    v-model:vol="config.MissTune_vol"   @save="postConfig" @test="testSound"/>
    </div>

    <h2>Wifi og spilvalg</h2>
    <div class="card-grid" v-if="!loading">
      <wifiConfig title="WiFi" v-model:ssid="config.wifi_ssid" v-model:pass="config.wifi_pass"
                  :oldsettings="config.wifi_ssid" @save="postWiFiconfig"/>
      <wifiConfig title="AP-WiFi" v-model:ssid="config.Gamebox_SSID" v-model:pass="config.Gamebox_pass"
                  :oldsettings="config.Gamebox_SSID" @save="postAPWiFiconfig"/>
      <oneConfig v-model="config.selected_game" icon="fa-gamepad" title="Game (0=test)"
                 :min="0" :max="255" :oldsettings="config.selected_game" @save="postConfig" @test="testSound"/>
      <oneConfig v-model="config.wifi_channel" icon="fa-wifi" title="ESP-NOW kanal (1-13)"
                 :min="1" :max="13" :oldsettings="config.wifi_channel" @save="postConfig" @test="testSound"/>
      <div class="card">
        <p class="card-title"><font-awesome-icon icon="fa-tag"/> Gamebox navn</p>
        <font-awesome-icon icon="fa-signature"/>
        <input v-model="config.Navn" placeholder="Gamebox#XXXX" maxlength="20"
               style="width:160px; text-align:center"/>
        <p><button class="button-on" @click="postConfig">Gem navn</button></p>
        <p style="font-size:0.7rem; opacity:0.6">Annonceres som WiFi-SSID i spil-mode og i hello-pakken til remoten</p>
      </div>
      <div class="card">
        <p class="card-title"><font-awesome-icon icon="fa-gamepad"/> Aktive spil</p>
        <font-awesome-icon icon="fa-list-ol"/>
        <input v-model="config.game_list" placeholder="0,1,2,3,4,5,6,7"
               style="width:160px; font-family:monospace; letter-spacing:1px"/>
        <p><button class="button-on" @click="postConfig">Gem spilliste</button></p>
        <p class="state">Gemt: {{ config.game_list }}</p>
      </div>
    </div>

    <h2>Joystick MAC-adresser</h2>
    <div class="card-grid" v-if="!loading">
      <macConfig title="Grønt joystick (Team A)" v-model="config.Green_mac"
                 :oldsettings="config.Green_mac" @save="postMacConfig"/>
      <macConfig title="Rødt joystick (Team B)"  v-model="config.Red_mac"
                 :oldsettings="config.Red_mac"   @save="postMacConfig"/>
    </div>

    <h2>Tug of War (1)</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.TugEnergyCost" icon="fa-gamepad" title="TOW Energy" :min="0" :max="255" :oldsettings="config.TugEnergyCost" @save="postConfig"/>
      <oneConfig v-model="config.TugRestTime"   icon="fa-gamepad" title="TOW RestTime" :min="0" :max="10000" :oldsettings="config.TugRestTime" @save="postConfig"/>
      <oneConfig v-model="config.TugPullStrength" icon="fa-gamepad" title="TOW Strength" :min="0" :max="1" :oldsettings="config.TugPullStrength" @save="postConfig"/>
      <oneConfig v-model="config.TugPointToWin" icon="fa-gamepad" title="TOW Points" :min="1" :max="3" :oldsettings="config.TugPointToWin" @save="postConfig"/>
    </div>

    <h2>Simon battle (2)</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.SimonBlinkTime"  icon="fa-clock" title="SIMON Blink"  :min="0" :max="10000" :oldsettings="config.SimonBlinkTime"  @save="postConfig"/>
      <oneConfig v-model="config.SimonPauseTime"  icon="fa-clock" title="SIMON Pause"  :min="0" :max="10000" :oldsettings="config.SimonPauseTime"  @save="postConfig"/>
      <oneConfig v-model="config.SimonSpeedCount" icon="fa-clock" title="count to new level" :min="0" :max="10000" :oldsettings="config.SimonSpeedCount" @save="postConfig"/>
      <soundConfig title="Simon Blå"    v-model:id="config.SimonBlue_id"    v-model:vol="config.SimonBlue_vol"   @save="postConfig" @test="testSound"/>
      <soundConfig title="Simon Grøn"   v-model:id="config.SimonGreen_id"   v-model:vol="config.SimonGreen_vol"  @save="postConfig" @test="testSound"/>
      <soundConfig title="Simon Gul"    v-model:id="config.SimonYellow_id"  v-model:vol="config.SimonYellow_vol" @save="postConfig" @test="testSound"/>
      <soundConfig title="Simon Rød"    v-model:id="config.SimonRed_id"     v-model:vol="config.SimonRed_vol"    @save="postConfig" @test="testSound"/>
      <soundConfig title="Simon Hit"    v-model:id="config.SimonHit_id"     v-model:vol="config.SimonHit_vol"    @save="postConfig" @test="testSound"/>
      <soundConfig title="Simon Miss"   v-model:id="config.SimonMiss_id"    v-model:vol="config.SimonMiss_vol"   @save="postConfig" @test="testSound"/>
    </div>

    <h2>Color Catcher (3)</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.CCatcherTime"    icon="fa-clock" title="Time to insane"   :min="0" :max="255"   :oldsettings="config.CCatcherTime"    @save="postConfig"/>
      <oneConfig v-model="config.CCatcherPixTime" icon="fa-clock" title="Time to maxPixels" :min="0" :max="255"  :oldsettings="config.CCatcherPixTime" @save="postConfig"/>
      <oneConfig v-model="config.CCatcherPixNum"  icon="fa-clock" title="maxPixels"         :min="0" :max="255"  :oldsettings="config.CCatcherPixNum"  @save="postConfig"/>
      <oneConfig v-model="config.CCatcherHomePix" icon="fa-clock" title="HomeBase count"    :min="0" :max="255"  :oldsettings="config.CCatcherHomePix" @save="postConfig"/>
      <soundConfig title="CC Hit"  v-model:id="config.CCHit_id"  v-model:vol="config.CCHit_vol"  @save="postConfig" @test="testSound"/>
      <soundConfig title="CC Miss" v-model:id="config.CCMiss_id" v-model:vol="config.CCMiss_vol" @save="postConfig" @test="testSound"/>
    </div>

    <h2>Speed Ball (4)</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.SB_BaseSpeed" icon="fa-clock" title="BallSpeed"     :min="0" :max="10000" :oldsettings="config.SB_BaseSpeed" @save="postConfig"/>
      <oneConfig v-model="config.SB_HomeSize"  icon="fa-clock" title="HomeField%"    :min="0" :max="100"   :oldsettings="config.SB_HomeSize"  @save="postConfig"/>
      <oneConfig v-model="config.SB_WhiteGrow" icon="fa-clock" title="Time to insane":min="0" :max="10000" :oldsettings="config.SB_WhiteGrow" @save="postConfig"/>
      <soundConfig title="SB Hit"  v-model:id="config.SBHit_id"  v-model:vol="config.SBHit_vol"  @save="postConfig" @test="testSound"/>
      <soundConfig title="SB Miss" v-model:id="config.SBMiss_id" v-model:vol="config.SBMiss_vol" @save="postConfig" @test="testSound"/>
    </div>

    <h2>Whac A Mole (5)</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.WAM_MaxP"  icon="fa-clock" title="Max Random pause ms" :min="0" :max="10000" :oldsettings="config.WAM_MaxP"  @save="postConfig"/>
      <oneConfig v-model="config.WAM_Pts"   icon="fa-clock" title="Point to win"         :min="0" :max="100"   :oldsettings="config.WAM_Pts"   @save="postConfig"/>
      <oneConfig v-model="config.WAM_Bonus" icon="fa-clock" title="Bonus %"              :min="0" :max="10000" :oldsettings="config.WAM_Bonus" @save="postConfig"/>
      <soundConfig title="WAM Hit"  v-model:id="config.WAMHit_id"  v-model:vol="config.WAMHit_vol"  @save="postConfig" @test="testSound"/>
      <soundConfig title="WAM Miss" v-model:id="config.WAMMiss_id" v-model:vol="config.WAMMiss_vol" @save="postConfig" @test="testSound"/>
    </div>

    <h2>Chain Reaction (6)</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.CHA_StartSpd" icon="fa-clock" title="Start Speed"     :min="0" :max="10000" :oldsettings="config.CHA_StartSpd" @save="postConfig"/>
      <oneConfig v-model="config.CHA_Accel"    icon="fa-clock" title="Accel"           :min="0" :max="100"   :oldsettings="config.CHA_Accel"    @save="postConfig"/>
      <oneConfig v-model="config.CHA_Zone"     icon="fa-clock" title="Pixels in Hitzone":min="0" :max="10000":oldsettings="config.CHA_Zone"     @save="postConfig"/>
      <soundConfig title="CHA Hit"  v-model:id="config.CHAHit_id"  v-model:vol="config.CHAHit_vol"  @save="postConfig" @test="testSound"/>
      <soundConfig title="CHA Miss" v-model:id="config.CHAMiss_id" v-model:vol="config.CHAMiss_vol" @save="postConfig" @test="testSound"/>
    </div>

    <h2>ULTIMATE - Whac A Mole (7)</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.UWAM_MaxP"  icon="fa-clock" title="Max Random pause ms" :min="0" :max="10000" :oldsettings="config.UWAM_MaxP"  @save="postConfig"/>
      <oneConfig v-model="config.UWAM_Pts"   icon="fa-clock" title="Point to win"         :min="0" :max="100"   :oldsettings="config.UWAM_Pts"   @save="postConfig"/>
      <oneConfig v-model="config.UWAM_Bonus" icon="fa-clock" title="Bonus %"              :min="0" :max="10000" :oldsettings="config.UWAM_Bonus" @save="postConfig"/>
    </div>

    <h2>LED-strip indstillinger</h2>
    <div class="card-grid" v-if="!loading">
      <oneConfig v-model="config.LEDBRIGHTNESS"  icon="fa-lightbulb"     title="Led Brightness"     :min="0" :max="255" :oldsettings="config.LEDBRIGHTNESS"  @save="postConfig"/>
      <oneConfig v-model="config.BackBrightness" icon="fa-lightbulb"     title="Background % bright" :min="1" :max="100" :oldsettings="config.BackBrightness" @save="postConfig"/>
      <oneConfig v-model="config.SCREEN_LEDS"    icon="fa-traffic-light" title="LEDs i strip"        :min="1" :max="500" :oldsettings="config.SCREEN_LEDS"    @save="postConfig"/>
      <oneConfig v-model="config.FIRST_S_LED"    icon="fa-traffic-light" title="First Screen LED"    :min="0" :max="50"  :oldsettings="config.FIRST_S_LED"    @save="postConfig"/>
    </div>

  </div><!-- /content -->
</template>

<script>
import oneConfig   from '@/components/oneConfig.vue'
import wifiConfig  from '@/components/wifiConfig.vue'
import macConfig   from '@/components/macConfig.vue'
import soundConfig from '@/components/soundConfig.vue'

// Button index: buttonId 1 = Blue (idx 0), 2 = Green (idx 1), 3 = Yellow (idx 2), 4 = Red (idx 3)
const BTN_COLORS = ['Blå', 'Grøn', 'Gul', 'Rød']

function nowStr() {
  const d = new Date()
  return d.toTimeString().slice(0, 8)
}

export default {
  components: { oneConfig, wifiConfig, macConfig, soundConfig },
  emits: ['showError'],
  inject: ['wdm'],

  data() {
    return {
      loading: true,
      config: {},
      // WebSocket
      wsConnected: false,
      _ws: null,
      // Live joystick state: [joystickId 0/1][buttonIndex 0-10]
      joyState: [
        Array(11).fill(false),
        Array(11).fill(false),
      ],
      // 7-kolonne layout for det store joystik (venstre=rød, højre=blå)
      bigJoyCols: [
        { buttons: [{idx:10,label:'11',hex:'#e53935',r:255,g:0,b:0,   id:12},
                    {idx: 9,label:'10',hex:'#e53935',r:255,g:0,b:0,   id:11}] },
        { buttons: [{idx: 8,label:'9', hex:'#cfd8dc',r:255,g:255,b:255,id:10}] },
        { buttons: [{idx: 7,label:'8', hex:'#f77f00',r:255,g:100,b:0, id:8},
                    {idx: 6,label:'7', hex:'#f77f00',r:255,g:100,b:0, id:7}] },
        { buttons: [{idx: 5,label:'6', hex:'#cfd8dc',r:255,g:255,b:255,id:6}] },
        { buttons: [{idx: 4,label:'5', hex:'#4caf50',r:0,g:255,b:0,  id:4},
                    {idx: 3,label:'4', hex:'#4caf50',r:0,g:255,b:0,  id:3}] },
        { buttons: [{idx: 2,label:'3', hex:'#cfd8dc',r:255,g:255,b:255,id:2}] },
        { buttons: [{idx: 0,label:'1', hex:'#2196f3',r:0,g:0,b:255,  id:0},
                    {idx: 1,label:'2', hex:'#2196f3',r:0,g:0,b:255,  id:1}] },
      ],
      // Button log – last 10 events
      buttonLog: [],
      // Auto-discovery – unikke ESP-NOW afsendere
      discoveredDevices: [],
      // Denne Gamebox's identitet
      ownMac: '—',
      ownChannel: '—',
      // WiFi-IP fra joystiks (udfyldes via wifi_status event)
      joyIpA: '',
      joyIpB: '',
      // LED-map for det store joystik (11 knapper → LED IDs + farver)
      bigLedMap: [
        { id:0,  hex:'#2196f3', r:0,   g:0,   b:255 },  // Knap 1  – Blå
        { id:1,  hex:'#2196f3', r:0,   g:0,   b:255 },  // Knap 2  – Blå
        { id:2,  hex:'#cfd8dc', r:255, g:255, b:255 },  // Knap 3  – Hvid
        { id:3,  hex:'#4caf50', r:0,   g:255, b:0   },  // Knap 4  – Grøn
        { id:4,  hex:'#4caf50', r:0,   g:255, b:0   },  // Knap 5  – Grøn
        { id:6,  hex:'#cfd8dc', r:255, g:255, b:255 },  // Knap 6  – Hvid
        { id:7,  hex:'#f77f00', r:255, g:100, b:0   },  // Knap 7  – Orange
        { id:8,  hex:'#f77f00', r:255, g:100, b:0   },  // Knap 8  – Orange
        { id:10, hex:'#cfd8dc', r:255, g:255, b:255 },  // Knap 9  – Hvid
        { id:11, hex:'#e53935', r:255, g:0,   b:0   },  // Knap 10 – Rød
        { id:12, hex:'#e53935', r:255, g:0,   b:0   },  // Knap 11 – Rød
      ],
    }
  },

  created() {
    this.getConfig()
    this.initWebSocket()
  },

  beforeUnmount() {
    if (this._ws) this._ws.close()
  },

  methods: {
    // ── WebSocket ──────────────────────────────────────────────
    initWebSocket() {
      const proto = location.protocol === 'https:' ? 'wss' : 'ws'
      const url   = `${proto}://${location.host}/ws`
      const ws = new WebSocket(url)
      this._ws = ws

      ws.onopen  = () => { this.wsConnected = true  }
      ws.onclose = () => { this.wsConnected = false; setTimeout(() => this.initWebSocket(), 3000) }
      ws.onerror = () => { ws.close() }

      ws.onmessage = (evt) => {
        try {
          const msg = JSON.parse(evt.data)
          if      (msg.type === 'button_event') this.handleButtonEvent(msg)
          else if (msg.type === 'raw_espnow')   this.handleRawEspNow(msg)
          else if (msg.type === 'ping_event')   this.handlePingEvent(msg)
          else if (msg.type === 'wifi_status')  this.handleWifiStatus(msg)
          else if (msg.type === 'device_info') {
            this.ownMac = msg.mac
            this.ownChannel = msg.channel
          }
        } catch (e) { /* ignore malformed */ }
      }
    },

    handleButtonEvent(msg) {
      const joy = msg.joystick
      const idx = msg.buttonId - 1  // 1-4 → 0-3

      if (msg.buttonId === 255) {
        if (joy >= 0 && joy <= 1) {
          this.joyState[joy] = Array(11).fill(true)
          setTimeout(() => { this.joyState[joy] = Array(11).fill(false) }, 600)
        }
      } else if (joy >= 0 && joy <= 1 && idx >= 0 && idx <= 10) {
        this.joyState[joy][idx] = msg.isPressed
      }

      // Log (maks 10, nyeste øverst)
      const btnLabel = msg.buttonId === 255 ? 'Alle' :
                       msg.btnName || `Knap ${msg.buttonId}`
      this.buttonLog.unshift({
        time:      nowStr(),
        joystick:  joy,
        mac:       msg.mac,
        btnName:   btnLabel,
        isPressed: msg.isPressed,
      })
      if (this.buttonLog.length > 10) this.buttonLog.pop()
    },

    handleRawEspNow(msg) {
      const mode = msg.bcast ? 'BROADCAST' : 'UNICAST  '
      console.log(`[ESP-NOW RX] ${mode}  MAC: ${msg.mac}  len: ${msg.len}  hex: ${msg.hex}`)
      this._upsertDevice({ mac: msg.mac })
    },

    handlePingEvent(msg) {
      console.log(`[PING] Joystik #${msg.joystick_id} '${msg.name}' er online  MAC: ${msg.mac}`)
      this._upsertDevice({ mac: msg.mac, name: msg.name, joyId: msg.joystick_id })
    },

    handleWifiStatus(msg) {
      console.log(`[WiFi Status] Joystik ${msg.joystick}  IP: ${msg.ip}  SSID: ${msg.ssid}`)
      this._upsertDevice({ mac: msg.mac, ip: msg.ip, ssid: msg.ssid })
      // Opdater også den korrekte joystik-kortvisning
      const key = msg.joystick === 0 ? 'joyIpA' : 'joyIpB'
      this[key] = msg.ip
    },

    _upsertDevice(fields) {
      const existing = this.discoveredDevices.find(d => d.mac === fields.mac)
      if (existing) {
        existing.count++
        existing.lastSeen = nowStr()
        if (fields.name  !== undefined) existing.name  = fields.name
        if (fields.joyId !== undefined) existing.joyId = fields.joyId
        if (fields.ip    !== undefined) existing.ip    = fields.ip
        if (fields.ssid  !== undefined) existing.ssid  = fields.ssid
      } else {
        this.discoveredDevices.push({
          mac: fields.mac, count: 1, lastSeen: nowStr(),
          name: fields.name || '', joyId: fields.joyId ?? null,
          ip: fields.ip || '', ssid: fields.ssid || '',
        })
      }
    },

    setMac(color, mac) {
      if      (color === 'green')  this.config.Green_mac  = mac
      else if (color === 'red')    this.config.Red_mac    = mac
      else if (color === 'remote') { this.config.Remote_mac = mac; this.postConfig() }
    },

    remoteAction(action) {
      if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return
      this._ws.send(JSON.stringify({ action }))
    },

    sendLedCmd(joystick, ledId, r, g, b) {
      if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return
      this._ws.send(JSON.stringify({ action: 'send_led', joystick, ledId, r, g, b }))
    },

    sendSound(joystickId, soundId) {
      if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return
      this._ws.send(JSON.stringify({ action: 'send_sound', joystick: joystickId, soundId, volume: 15 }))
    },

    // Afspil lyd fra soundConfig-kortets test-knapper (▶🟢 / ▶🔴)
    testSound({ joystick, id, vol }) {
      if (!this._ws || this._ws.readyState !== WebSocket.OPEN) return
      this._ws.send(JSON.stringify({ action: 'send_sound', joystick, soundId: id, volume: vol }))
    },

    // ── REST config ────────────────────────────────────────────
    async getConfig() {
      const res = await fetch('/config', { headers: { 'Content-Type': 'application/json' } })
      if (res.ok) this.config = await res.json()
      else alert('Kunne ikke hente config')
      this.loading = false
    },

    async postConfig() {
      this.loading = true
      const headers = { 'Content-Type': 'application/json' }
      const cfg = { ...this.config }
      // Fjern read-only og forældede felter – reducerer JSON-størrelsen
      // og undgår at overskride én TCP-pakke (1460 bytes)
      for (const k of ['wifi_ssid','wifi_pass','FirmwareVersion',
                        'BatteryPin','BuzzerPin1','BuzzerPin2',
                        'Player1Pin','Player2Pin',
                        'FirstP1Led','FirstP2Led','NUM_LEDS','STATUS_LED']) {
        delete cfg[k]
      }
      const res = await fetch('/config', { method: 'POST', headers, body: JSON.stringify(cfg) })
      if (res.ok) { const r = await res.json(); this.config = r.config }
      else alert('Gem fejlede')
      this.loading = false
    },

    async postMacConfig() {
      this.loading = true
      const headers = { 'Content-Type': 'application/json' }
      const body = JSON.stringify({ Green_mac: this.config.Green_mac, Red_mac: this.config.Red_mac })
      const res = await fetch('/config', { method: 'POST', headers, body })
      if (res.ok) { const r = await res.json(); this.config = r.config }
      else alert('Gem fejlede')
      this.loading = false
    },

    async postWiFiconfig() {
      this.loading = true
      const headers = { 'Content-Type': 'application/json' }
      const body = JSON.stringify({ wifi_ssid: this.config.wifi_ssid, wifi_pass: this.config.wifi_pass })
      const res = await fetch('/config', { method: 'POST', headers, body })
      if (res.ok) { const r = await res.json(); this.config = r.config }
      else alert('Gem fejlede')
      this.loading = false
    },

    async postAPWiFiconfig() {
      this.loading = true
      const headers = { 'Content-Type': 'application/json' }
      const body = JSON.stringify({ Gamebox_SSID: this.config.Gamebox_SSID, Gamebox_pass: this.config.Gamebox_pass })
      const res = await fetch('/config', { method: 'POST', headers, body })
      if (res.ok) { const r = await res.json(); this.config = r.config }
      else alert('Gem fejlede')
      this.loading = false
    },
  },
}
</script>

<style src="./style.css" lang="css"></style>

<style scoped>
/* ── Gamebox identitet ───────────────────────────────── */
.gamebox-identity {
  display: flex;
  align-items: center;
  gap: 8px;
  background: #0A1128;
  color: #fff;
  border-radius: 10px;
  padding: 10px 20px;
  margin-bottom: 18px;
  font-family: monospace;
  font-size: 1rem;
  letter-spacing: 0.03em;
}
.identity-label { color: rgba(255,255,255,0.55); font-size: 0.78rem; font-family: sans-serif; }
.identity-mac   { font-size: 1.05rem; font-weight: bold; color: #7dd3fc; }
.identity-ch    { font-size: 1.05rem; font-weight: bold; color: #86efac; }
.identity-sep   { color: rgba(255,255,255,0.3); }

/* ── Lydknapper ──────────────────────────────────────── */
.sound-row { display: flex; align-items: center; gap: 6px; margin-top: 4px; }
.sound-label { color: rgba(255,255,255,0.5); font-size: 0.7rem; }
.snd-btn { border: none; border-radius: 6px; padding: 4px 10px; font-size: 0.8rem; cursor: pointer; background: rgba(255,255,255,0.15); color: #fff; font-weight: bold; transition: background 0.1s; }
.snd-btn:hover { background: rgba(255,255,255,0.3); }
.snd-btn:active { background: rgba(255,255,255,0.5); }
.snd-special { background: rgba(255,220,50,0.25); color: #ffe030; }

/* ── WebSocket badge ─────────────────────────────────── */
.ws-badge { font-size: 0.75rem; padding: 2px 10px; border-radius: 12px; font-weight: bold; vertical-align: middle; margin-left: 8px; }
.ws-on  { background: #d3f9d8; color: #2b8a3e; }
.ws-off { background: #ffe3e3; color: #c92a2a; }

/* ── Joystick row ────────────────────────────────────── */
.joystick-row { display: flex; gap: 30px; justify-content: center; flex-wrap: wrap; margin-bottom: 30px; }

.joystick {
  border-radius: 18px;
  padding: 20px 28px;
  min-width: 260px;
  box-shadow: 0 4px 18px rgba(0,0,0,0.15);
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
}
.joy-green { background: #2d6a2d; }
.joy-red   { background: #7a1a1a; }

.joy-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  width: 100%;
}
.joy-label { color: #fff; font-weight: bold; font-size: 1rem; }
.joy-mac   { color: rgba(255,255,255,0.6); font-family: monospace; font-size: 0.75rem; }
.joy-ip    { color: rgba(180,255,180,0.85); font-family: monospace; font-size: 0.72rem;
             background: rgba(0,0,0,0.25); border-radius: 4px; padding: 1px 6px; }

.dev-name         { font-weight: 600; }
.dev-name-unknown { color: #bbb; }
.set-remote { background: #1a3a5c; color: #60b0ff; border: 1px solid #2266aa; }
.set-remote:hover { background: #2255aa; }

/* ── Remote Control ────────────────────────────────────────── */
.remote-row { display: flex; justify-content: center; margin-bottom: 30px; }
.remote-card {
  background: #0d2540;
  border: 2px solid #1e5080;
  border-radius: 18px;
  padding: 20px 28px;
  min-width: 340px;
  display: flex;
  flex-direction: column;
  gap: 12px;
  box-shadow: 0 4px 18px rgba(0,80,160,0.3);
}
.remote-mac-row { display: flex; align-items: center; gap: 10px; }
.remote-label   { color: rgba(150,200,255,0.8); font-size: 0.72rem; text-transform: uppercase; letter-spacing: 0.05em; }
.remote-mac-input {
  flex: 1; padding: 5px 10px; border-radius: 6px;
  border: 1px solid #2266aa; background: #0a1e38; color: #80c8ff;
  font-family: monospace; font-size: 0.85rem;
}
.remote-mac-save {
  background: #1e5080; color: #80c8ff; border: 1px solid #2266aa;
  border-radius: 6px; padding: 4px 10px; cursor: pointer; font-size: 1rem;
}
.remote-mac-save:hover { background: #2266aa; }
.remote-fields { display: flex; gap: 16px; flex-wrap: wrap; }
.remote-field  { display: flex; flex-direction: column; gap: 4px; }
.remote-input  {
  width: 100px; padding: 5px 8px; border-radius: 6px;
  border: 1px solid #2266aa; background: #0a1e38; color: #80c8ff;
  font-size: 0.9rem; text-align: center;
}
.remote-btns { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 4px; }
.remote-action-btn {
  border: none; border-radius: 8px; padding: 7px 14px;
  font-size: 0.82rem; font-weight: bold; cursor: pointer;
  transition: opacity 0.1s;
}
.remote-action-btn:hover  { opacity: 0.8; }
.remote-action-btn:active { opacity: 0.6; transform: scale(0.97); }
.remote-red   { background: #5a1515; color: #ff9090; border: 1px solid #aa3333; }
.remote-green { background: #155a20; color: #90ff90; border: 1px solid #33aa44; }
.remote-send  { background: #1a3a5c; color: #90d0ff; border: 1px solid #2266aa; }
.remote-note  { color: rgba(100,160,220,0.5); font-size: 0.68rem; margin: 0; text-align: center; }

/* Per-joystik type-mini toggle */
.joy-mac-row { display: flex; justify-content: space-between; align-items: center; width: 100%; }
.joy-type-mini { display: flex; gap: 4px; }
.jtm-btn {
  border: 1px solid rgba(255,255,255,0.2); border-radius: 5px;
  background: rgba(0,0,0,0.2); color: rgba(255,255,255,0.5);
  padding: 2px 8px; font-size: 0.72rem; font-weight: bold; cursor: pointer;
}
.jtm-btn:hover { background: rgba(255,255,255,0.15); color: #fff; }
.jtm-active    { background: rgba(255,255,255,0.25) !important; color: #fff !important; border-color: rgba(255,255,255,0.5) !important; }

/* 7-kolonne layout for stort joystik */
.joy-buttons-large {
  display: flex; gap: 6px; align-items: center; justify-content: center;
  padding: 4px 0;
}
.jbl-col    { display: flex; flex-direction: column; gap: 6px; align-items: center; }
.jbl-spacer { width: 36px; height: 36px; }  /* fylder plads for enkelt-knap kolonner */
.jbl-btn {
  width: 36px; height: 36px; border-radius: 50%;
  border: 2px solid; cursor: pointer;
  font-size: 0.68rem; font-weight: bold;
  color: rgba(255,255,255,0.8);
  transition: all 0.1s;
  display: flex; align-items: center; justify-content: center;
}
.jbl-btn:hover  { filter: brightness(1.3); transform: scale(1.08); }
.jbl-btn:active { transform: scale(0.94); }
.jbl-active     { color: #000 !important; }
.jbl-off {
  width: 28px; height: 28px; border-radius: 5px; border: 1px solid rgba(255,255,255,0.2);
  background: rgba(0,0,0,0.3); color: rgba(255,255,255,0.5);
  cursor: pointer; font-size: 0.8rem; margin-left: 4px;
}
.jbl-off:hover { background: rgba(255,255,255,0.15); color: #fff; }

/* Point-LEDs øverste højre hjørne (ID3 venstre, ID2 midt, ID1 højre) */
.joy-pts { display: flex; gap: 5px; align-items: center; }
.joy-pt  { font-size: 1.2rem; color: rgba(255,255,255,0.15); line-height: 1; }
.pt-lit  { color: rgba(255,255,255,0.7); }

.joy-buttons { display: flex; gap: 14px; }

.joy-btn {
  width: 58px;
  height: 58px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 0.65rem;
  font-weight: bold;
  color: rgba(255,255,255,0.5);
  transition: background 0.08s, box-shadow 0.08s, transform 0.08s;
  cursor: default;
  user-select: none;
}

/* Resting (not pressed) – farvet border viser forventet LED-farve */
.btn-red    { background: #5a1515; border: 2px solid #cc2222; }
.btn-yellow { background: #5a4d10; border: 2px solid #ccaa00; }
.btn-green  { background: #155a20; border: 2px solid #22aa44; }
.btn-blue   { background: #15305a; border: 2px solid #2266cc; }

/* Active (pressed) – border lyser fuldt op */
.btn-red.active    { background: #ff3030; color: #fff; border-color: #ff6060; box-shadow: 0 0 18px #ff3030; transform: scale(0.93); }
.btn-yellow.active { background: #ffe030; color: #333; border-color: #ffe880; box-shadow: 0 0 18px #ffe030; transform: scale(0.93); }
.btn-green.active  { background: #30ff60; color: #1a3a1a; border-color: #80ffaa; box-shadow: 0 0 18px #30ff60; transform: scale(0.93); }
.btn-blue.active   { background: #30a0ff; color: #fff;  border-color: #80c8ff; box-shadow: 0 0 18px #30a0ff; transform: scale(0.93); }

.snd-wifi  { background: rgba(30,180,255,0.15); color: #50d0ff; font-size: 1rem; padding: 3px 8px; }
.snd-sleep { background: rgba(100,80,160,0.2);  color: #b090ff; font-size: 1rem; padding: 3px 8px; }

/* ── Button log ──────────────────────────────────────── */
.log-table-wrap {
  max-width: 800px;
  margin: 0 auto 30px;
  background: white;
  border-radius: 8px;
  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.3);
  overflow: hidden;
}
.log-table { width: 100%; border-collapse: collapse; font-size: 0.85rem; }
.log-table th { background: #0A1128; color: white; padding: 10px 14px; text-align: left; }
.log-table td { padding: 9px 14px; border-bottom: 1px solid #eee; text-align: left; }
.log-table tr:last-child td { border-bottom: none; }
.log-empty { color: #999; padding: 20px; }

.mono { font-family: monospace; font-size: 0.8rem; }

.joy-tag  { padding: 2px 8px; border-radius: 10px; font-size: 0.75rem; font-weight: bold; }
.tag-green   { background: #d3f9d8; color: #2b8a3e; }
.tag-red     { background: #ffe3e3; color: #c92a2a; }
.tag-unknown { background: #e9ecef; color: #666; }

.set-btn { border: none; border-radius: 6px; padding: 3px 10px; font-size: 0.78rem; cursor: pointer; font-weight: bold; }
.set-green { background: #d3f9d8; color: #2b8a3e; }
.set-red   { background: #ffe3e3; color: #c92a2a; }

.state-tag { padding: 2px 8px; border-radius: 10px; font-size: 0.75rem; font-weight: bold; }
.tag-pressed  { background: #1864ab; color: white; }
.tag-released { background: #e9ecef; color: #495057; }
</style>
