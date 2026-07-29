# Futuretown – Patruljedysten, Gamers Edition

Et ESP32-baseret spilsystem bygget til **Patruljedysten** på et spejder-event: en stor **gamebox** med LED-strip afvikler holdspil, styret af **trådløse joysticks** – og en **remote** kan fjernstyre flere gameboxe på én gang. Alt taler sammen trådløst via **ESP-NOW** (ingen router nødvendig ude på pladsen).

Repo'et er delt så andre kan bygge deres eget system. Alt hardware er hjemmeloddet ESP32-elektronik.

**Udviklet og sponsoreret af [WeDoMobility ApS](https://www.wedomobility.com/).**

> **Kom hurtigt i gang:** byg firmwaren i hver mappe med `./build.sh`, flash med `./upload.sh`, og konfigurér resten via det indbyggede web-interface. Se detaljer nedenfor.

---

## Hvad er det?

To hold dyster mod hinanden i en række minispil, der vises på en lang LED-strip mellem holdene. Hvert hold har et joystick med farvede knapper. En gamebox styrer det hele; en valgfri remote lader en arrangør skifte spil, justere lys/lyd og starte runder på afstand – for mange gameboxe samtidig.

### Spillene
| # | Spil | Kort |
|---|------|------|
| 1 | **Tug of War** | Tovtrækning – hamre på knappen for at trække midten over på modstanderen |
| 2 | **Simon Duel** | Husk og gentag den voksende farvesekvens |
| 3 | **Color Catcher** | Ram den rigtige farve-knap når bolden er i det grønne felt |
| 4 | **Speed Ball** | Returnér bolden i det farvede felt – hurtigere zoner giver mere fart |
| 5 | **Whac-A-Mole** | Tryk den farve der lyser, hurtigst muligt |
| 6 | **Chain Reaction** | Fang bolden i din zone og send den retur |
| 7 | **Ultimate WAM** | Whac-A-Mole på ét stort joystik med mange knapper |

(Spil 0 = knaptest, spil 8 = demo/screensaver med animationer af alle spil.)

---

## Systemets tre dele

```
Futuretown-Patruljedysten/
├─ Gamebox/        Hjernen: LED-strip, spil-logik, web-admin
│  └─ gamebox-admin/   Vue-baseret konfigurations-UI (bygges til LittleFS)
├─ Remote/         Fjernbetjening af flere gameboxe (ESP-NOW)
├─ joystick/       Firmware til de trådløse joysticks
└─ docs/           Vejledning + billeder
```

### 🎮 Gamebox
Hjertet i systemet. Driver WS2812B LED-strippen, kører spillene, og modtager knaptryk fra joystickene via ESP-NOW. Har et web-admin (Vue) til opsætning af WiFi, kanal, joystick-MACs, lyde, lysstyrke m.m. Understøtter trådløs firmware-opdatering (OTA).

### 🕹️ Joystick
Selvstændig ESP32 med farvede knapper og NeoPixel-LEDs (og valgfri lyd). Sender knaptryk til gameboxen via ESP-NOW og melder sig automatisk ("hello") ved opstart.

### 📡 Remote
En ESP32 med web-interface til at fjernstyre **flere gameboxe** samtidig: skift spil, start runder, og justér lysstyrke/volumen med skydere – live. Gameboxe dukker automatisk op i listen når de melder sig.

---

## Sådan hænger det sammen

Alt kører på **ESP-NOW** (Espressifs letvægts-trådløse protokol) på en **fælles kanal** (default: 6) – der skal *ikke* være en router til stede under afvikling.

- **Gameboxen** fungerer som "kanal-anker": den starter et lille access point så joystickene kan finde den rigtige kanal.
- **Joystickene** finder kanalen, låser sig fast og sender knaptryk.
- **Remoten** lytter på samme kanal og styrer gameboxene direkte via deres MAC-adresse.

Vigtigt: **alle enheder skal være på samme ESP-NOW-kanal** for at kunne tale sammen.

---

## Hardware

Grundopskrift pr. enhed (alt bygget på klassisk **ESP32 / ESP-32S, 4 MB flash**):

| Del | Detaljer (defaults) |
|-----|---------------------|
| Mikrocontroller | ESP32 (NodeMCU-32S el. lign.) |
| LED-strip (gamebox) | WS2812B, 120 pixels, data på **GPIO 12** |
| Fysiske knapper (gamebox) | Rød på **GPIO 4**, grøn på **GPIO 15** |
| Joystick-LEDs | WS2812B / NeoPixel |
| ESP-NOW-kanal | 6 (skal matche på tværs af alle enheder) |

Pins og antal LEDs kan ændres i web-adminen / `config_manager.h`.

---

## Byg og flash

Hver mappe har sine egne scripts (bygget til macOS + `arduino-cli`, ESP32 core 3.x):

```bash
# Eksempel: Gamebox
cd Gamebox
./build.sh                          # kompilér firmware
./upload.sh /dev/cu.usbserial-XXXX  # flash via USB (seriel)
./upload.sh --ota 192.168.x.x       # eller trådløst (OTA), når enheden er sat op
```

- **Gamebox** har også et web-admin (Vue). Byg det med `npm run build` i `Gamebox/gamebox-admin/`, kopiér output til `Gamebox/data/`, og flash filsystemet med `./upload.sh --data`.
- **Remote** og **joystick** flashes på samme måde med deres egne `build.sh` / `upload.sh`.

> **Bemærk:** Bygge-output (`build/`, `node_modules/`, `dist/` osv.) er bevidst holdt ude af repo'et via `.gitignore` – det genskabes lokalt.

---

## 🔧 Byggenoter – "Sketch too big"

Gamebox-firmwaren ligger **tæt på grænsen** for standard-partitionens app-plads: ca. **1,31 MB** (`0x140000`) på en 4 MB ESP32. Samme kildekode kan derfor pludselig fejle at bygge, hvis en **biblioteks- eller core-opdatering** puster binæren op:

```
Sketch too big / text section exceeds available space in board   (>100%)
```

Konkret oplevede vi at nøjagtig samme kode gik fra **~1,30 MB (passede)** til **~1,54 MB (for stor)** efter en automatisk biblioteks-opdatering – uden at en linje kode var ændret.

**Hvis du rammer det, prøv i denne rækkefølge:**

1. **Pin bibliotekerne** til versioner der bygger under grænsen. Den mest sandsynlige synder er **ESP Async WebServer** (vi så den skifte `3.11.0 → 3.11.2`) – prøv at gå tilbage til en tidligere version:
   ```bash
   arduino-cli lib install "ESP Async WebServer@3.11.0"
   ```
   Den **største** knap på binær-størrelsen er dog **ESP32-core-versionen**: core **2.0.x** giver markant mindre binære end 3.x. Passer det stadig ikke, så prøv en 2.0.x-core.

2. **Større app-partition** (Arduino IDE → Tools → Partition Scheme → fx "Minimal SPIFFS (1.9MB APP)").
   ⚠️ På 4 MB skrumper det filsystemet til ~190 KB, så det fulde **web-admin (LittleFS) ikke længere kan være der**. Denne vej kræver altså at man dropper eller skrumper adminen — derfor er løsning 1 som regel at foretrække.

**Reference-toolchain** (det vi byggede med – juli 2026):
`esp32` core 3.3.10 · ESP Async WebServer 3.11.x · Async TCP 3.4.10 · ArduinoJson 7.4.3 · FastLED 3.10.5 · ElegantOTA 3.1.7

---

## Opsætning (boot-tilstande)

**Gamebox** (aflæses ved opstart):
| Knapper holdt | Tilstand |
|---|---|
| Ingen | Spil-mode – kun ESP-NOW, hurtig boot |
| Grøn | WiFi-mode – forbinder til router + web-admin |
| Rød + Grøn | Config-AP `GameboxSetup` (åbn `192.168.4.1`) |

Under spil styrer de fysiske knapper: **rød** = skift spil (i lobby) / afbryd (under spil), **grøn** = start runde. Hold rød i 3 sek. for screensaver.

**Joystick:** hold **knap 1 + knap 2** ved opstart → config-AP `JoystickSetup`.

**Remote:** forbinder til konfigureret WiFi, ellers eget access point `GamersRemote`. Åbn dens IP i en browser for at styre gameboxene.

---

## ⚠️ Skift kodeordene!

Alle kodeord i kildekoden er **pladsholdere** (`12345678`). Sæt dine egne via web-adminen (de gemmes i enhedens NVS, ikke i koden) inden brug.

---

## Vejledning

Se `docs/` for den fulde vejledning (`Patruljedyst vejledning.pptx`). Billeder af det loddede hardware tilføjes samme sted.

*(Tip: eksportér gerne PowerPointen til PDF – så kan den vises direkte på GitHub.)*

---

## Licens

Lavet **til spejdere og spejdergrupper** og delt under **Creative Commons BY-NC 4.0** (Kreditering–IkkeKommerciel).

Du må frit bygge, bruge, dele og tilpasse systemet til spejderbrug — så længe du **krediterer [FutureTown.dk](https://www.futuretown.dk)**. Det må **ikke bruges kommercielt**: hverken boksene, koden eller konceptet må sælges eller ændres for at tjene penge på det.

Se [LICENSE](LICENSE) · [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/deed.da)

Bygget til Patruljedysten · FutureTown · Explore Together 🎮
