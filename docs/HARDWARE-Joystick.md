# Hardware – Joystick

Stykliste og wirediagram for de **trådløse joysticks** (ét pr. hold).

> Board: **Adafruit QT Py ESP32-S3 (N4R2)** – 4 MB flash, 2 MB PSRAM
> (`esp32:esp32:adafruit_qtpy_esp32s3_n4r2`, `huge_app`-partition).
> ESP32-S3 er valgt fordi GPIO 7-10 bruges til knapper – de er optaget af flash på en klassisk ESP32-WROOM.

Der findes **to varianter** (vælges pr. joystick i gameboxens web-admin, `JoystickType`):

| Type | Knapper | Lyd |
|:---:|---|:---:|
| **0** – lille | 4 (Blå · Grøn · Gul · Rød) | ✅ ja |
| **1** – stor | op til 11 (P1–P11) | ❌ nej |

Firmwaren er den samme; forskellen er antal knapper (`BtnCount`) og om lyd er slået til (`HasAudio`).

---

## Stykliste (lille joystick – 4 knapper + lyd, default)

| Antal | Komponent | Detalje |
|:---:|---|---|
| 1 | Adafruit QT Py ESP32-S3 (N4R2) | 4 MB flash, 2 MB PSRAM, USB-C |
| 4 | Arcade-trykknapper | Blå, Grøn, Gul, Rød – momentan (NO) |
| 1 | WS2812B LED-kæde, **9 pixels** | Knap-/score-lys, farverækkefølge RGB (stor model: 15 pixels) |
| 1 | I2S-forstærker | Klasse-D med I2S-indgang, fx **MAX98357A** |
| 1 | Højttaler | Lille 4–8 Ω |
| 1 | Elektrolyt-kondensator **100 µF** | Over +5V/GND – buffer |
| 1 | Keramisk kondensator **100 nF** (orange) | På tværs af forsyningen – dæmper støj |
| 1 | Strømforsyning | USB-C eller batteri (dvale/deep-sleep understøttes) |

> **Stor joystick (Type 1):** samme board og status-pixel, men **15-pixels** LED-kæde, **11 knapper** og **ingen** forstærker/højttaler (`HasAudio = false`). De 11 knapper sidder på **GPIO 1–11** (P1–P11 i web-adminen), og LED-kæden på GPIO 12.
>
> Firmwaren driver op til 15 pixels (`LED_COUNT`); på den lille model er kun de første **9** monteret.

---

## Wiring (pin-oversigt)

**Knapper – lille model (Type 0, 4 knapper):**

| Pin | Knap |
|---|---|
| **GPIO 7** | Knap 1 – Blå |
| **GPIO 8** | Knap 2 – Grøn |
| **GPIO 9** | Knap 3 – Gul |
| **GPIO 10** | Knap 4 – Rød |

**Knapper – stor model (Type 1, 11 knapper):**

| Pin | Knap |
|---|---|
| **GPIO 1 – GPIO 11** | Knap 1 – 11 (én GPIO pr. knap) |

**Fælles for begge modeller:**

| Pin | Forbindes til | Note |
|---|---|---|
| **GPIO 12** | LED-kæde **DIN** | 9 px (lille) / 15 px (stor) |
| **GPIO 21** | Status-pixel | Onboard NeoPixel – ingen ekstra ledning |
| **5V** | LED +5V (+ forstærker Vin på lille model) | |
| **GND** | Fælles stel | |

**Kun lille model (lyd via I2S):**

| Pin | I2S |
|---|---|
| **GPIO 2** | DIN (data) |
| **GPIO 3** | BCLK (bit-clock) |
| **GPIO 4** | LRC / WS (word-select) |

Alle knapper sidder mellem deres GPIO og **GND** (intern pull-up, aktiv-lav). Med lyd (lille model) vækkes boardet fra dvale via knap 1–4; uden lyd (stor model) er I2S-pinnene frie, så flere knapper kan bruges som wake-kilde. Forstærkerens `SD`/`GAIN` sættes efter dens eget datablad (fx `GAIN` uforbundet = 9 dB).

---

## Wirediagram

### Lille model (Type 0 – 4 knapper + lyd)

```mermaid
flowchart LR
  ESP["QT Py ESP32-S3"]
  B1(["Knap 1 · Blå"])
  B2(["Knap 2 · Grøn"])
  B3(["Knap 3 · Gul"])
  B4(["Knap 4 · Rød"])
  LEDS["WS2812B kæde<br/>9 pixels"]
  STAT{{"Status-pixel (onboard)"}}
  AMP["I2S-forstærker<br/>(MAX98357A)"]
  SPK["Højttaler"]

  B1 ---|"GPIO 7 ↔ GND"| ESP
  B2 ---|"GPIO 8 ↔ GND"| ESP
  B3 ---|"GPIO 9 ↔ GND"| ESP
  B4 ---|"GPIO 10 ↔ GND"| ESP
  ESP -->|"GPIO 12 → DIN"| LEDS
  ESP -->|"GPIO 21 → DIN"| STAT
  ESP -->|"GPIO 2 (DIN)"| AMP
  ESP -->|"GPIO 3 (BCLK)"| AMP
  ESP -->|"GPIO 4 (LRC)"| AMP
  AMP -->|"+ / −"| SPK
```

### Stor model (Type 1 – 11 knapper, ingen lyd)

```mermaid
flowchart LR
  ESP2["QT Py ESP32-S3"]
  BTN(["Knap 1 – 11"])
  LEDS2["WS2812B kæde<br/>15 pixels"]
  STAT2{{"Status-pixel (onboard)"}}

  BTN  ---|"GPIO 1–11 ↔ GND<br/>(én pr. knap)"| ESP2
  ESP2 -->|"GPIO 12 → DIN"| LEDS2
  ESP2 -->|"GPIO 21 → DIN"| STAT2
```

---

## Config-mode

Hold **knap 1 + knap 2** nede ved opstart → joysticket starter access point `JoystickSetup` (åbn `192.168.4.1` for at sætte navn, kanal, gamebox-MAC, antal knapper, lyd m.m.).

---

*Knapfarverne følger kappen; firmwaren sender kun knap-indeks (1–4 på lille, 1–11 på stor). Forstærker-model (MAX98357A) er ikke fastlagt i koden – enhver I2S-klasse-D-forstærker med BCLK/LRC/DIN passer til pin-opsætningen.*
