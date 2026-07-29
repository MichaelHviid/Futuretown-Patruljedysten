#!/usr/bin/env bash
# =============================================================================
#  upload.sh  –  Upload Gamebox firmware og/eller LittleFS
#
#  Brug (seriel):
#    ./upload.sh /dev/cu.SLAB_USBtoUART             # kun firmware
#    ./upload.sh /dev/cu.SLAB_USBtoUART --data      # firmware + LittleFS
#    ./upload.sh /dev/cu.SLAB_USBtoUART --data-only # kun LittleFS
#
#  Brug (OTA over WiFi):
#    ./upload.sh --ota 192.168.1.42                 # kun firmware
#    ./upload.sh --ota 192.168.1.42 --data          # firmware + LittleFS
#    ./upload.sh --ota 192.168.1.42 --data-only     # kun LittleFS
#
#  Tip til 15 Gameboxes – upload alle via OTA på én gang:
#    for ip in 192.168.1.{40..54}; do ./upload.sh --ota $ip & done; wait
# =============================================================================
set -euo pipefail

# ─── Stier ───────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/.build"
ARDUINO_DATA="$HOME/Library/Arduino15"

# esptool fra ESP32-pakken
ESPTOOL=$(find "$ARDUINO_DATA/packages/esp32/tools/esptool_py" \
    -name "esptool" -not -name "*.py" -type f 2>/dev/null | sort -V | tail -1)
if [ -z "$ESPTOOL" ]; then
    ESPTOOL=$(find "$ARDUINO_DATA/packages/esp32/tools/esptool_py" \
        -name "esptool.py" -type f 2>/dev/null | sort -V | tail -1)
fi
if [ -z "$ESPTOOL" ]; then
    ESPTOOL=$(which esptool.py 2>/dev/null || which esptool 2>/dev/null || true)
fi
[ -z "$ESPTOOL" ] && { echo "❌  esptool ikke fundet"; exit 1; }

# ─── Flash-adresser (default partition scheme, 4MB) ──────────────────────────
APP_OFFSET="0x10000"
OTA_DATA_OFFSET="0xE000"   # OTA-data partition – nulstiller boot-valg til app0
FS_OFFSET="0x290000"       # 2686976 decimal – matches Arduino IDE output
FW_BAUD="921600"           # Firmware upload (fra FQBN: UploadSpeed=921600)
FS_BAUD="115200"           # LittleFS upload (hvad Arduino IDE faktisk bruger)

# boot_app0.bin nulstiller OTA-boot-flag → ESP32 booter fra app0 (0x10000) efter serial flash
BOOT_APP0=$(find "$ARDUINO_DATA/packages/esp32/hardware/esp32" \
    -name "boot_app0.bin" -type f 2>/dev/null | sort -V | tail -1)
[ -z "$BOOT_APP0" ] && { echo "⚠  boot_app0.bin ikke fundet – serial flash nulstiller ikke OTA-flag"; }

# ─────────────────────────────────────────────────────────────────────────────

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
step() { echo -e "\n${YELLOW}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
err()  { echo -e "${RED}✗ $1${NC}" >&2; exit 1; }
info() { echo -e "${CYAN}  $1${NC}"; }

# ─── Læs versionsnummer fra Gamebox.ino ──────────────────────────────────────
VERSION=$(grep -m1 'FirmwareVersion' "$SCRIPT_DIR/Gamebox.ino" \
    | grep -oE '"[^"]+"' | tr -d '"')
[ -z "$VERSION" ] && VERSION="ukendt"

# ─── Argument-parsing ────────────────────────────────────────────────────────
PORT=""
OTA_IP=""
DO_FIRMWARE=true
DO_DATA=false

if [ $# -eq 0 ]; then
    echo "Brug: $0 /dev/cu.XXXX [--data] [--data-only]"
    echo "      $0 --ota <IP>   [--data] [--data-only]"
    exit 1
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ota)      shift; OTA_IP="${1:-}"; shift ;;
        --data)     DO_DATA=true; shift ;;
        --data-only) DO_DATA=true; DO_FIRMWARE=false; shift ;;
        /dev/*)     PORT="$1"; shift ;;
        *)          err "Ukendt parameter: $1" ;;
    esac
done

# Valider
if [ -z "$PORT" ] && [ -z "$OTA_IP" ]; then
    err "Angiv en seriel port (/dev/cu.XXXX) eller --ota <IP>"
fi

# Tjek at build-output eksisterer
FW_BIN="$BUILD_DIR/Gamebox.ino.bin"
FS_BIN="$BUILD_DIR/littlefs.bin"
[ "$DO_FIRMWARE" = true ] && [ ! -f "$FW_BIN" ] && \
    err "Ingen firmware fundet: $FW_BIN\nKør ./build.sh først."
[ "$DO_DATA" = true ] && [ ! -f "$FS_BIN" ] && \
    err "Ingen LittleFS image: $FS_BIN\nKør ./build.sh først."

# ─── Seriel upload ────────────────────────────────────────────────────────────
serial_upload() {
    echo ""
    echo "══════════════════════════════════════════════"
    info "Version: $VERSION"
    info "Port:    $PORT"
    echo "══════════════════════════════════════════════"

    if [ "$DO_FIRMWARE" = true ]; then
        step "Upload firmware → $PORT  (baud $FW_BAUD, qio)…"
        # Flash firmware + boot_app0.bin i én kommando så ESP32 booter fra app0
        if [ -n "$BOOT_APP0" ]; then
            "$ESPTOOL" \
                --chip esp32 \
                --port "$PORT" \
                --baud "$FW_BAUD" \
                --before default-reset \
                --after  hard-reset \
                write-flash \
                --flash-mode qio \
                --flash-freq 80m \
                --flash-size detect \
                "$OTA_DATA_OFFSET" "$BOOT_APP0" \
                "$APP_OFFSET"      "$FW_BIN"
        else
            "$ESPTOOL" \
                --chip esp32 \
                --port "$PORT" \
                --baud "$FW_BAUD" \
                --before default-reset \
                --after  hard-reset \
                write-flash \
                --flash-mode qio \
                --flash-freq 80m \
                --flash-size detect \
                "$APP_OFFSET" "$FW_BIN"
        fi
        ok "Firmware uploadet  ($(du -sh "$FW_BIN" | cut -f1))"
    fi

    if [ "$DO_DATA" = true ]; then
        step "Upload LittleFS → $PORT  (baud $FS_BAUD, dio, -z)…"
        # Parametre matcher præcis hvad Arduino IDE bruger til LittleFS:
        #   baud 115200, flash-mode dio, -z (compress), flash-size detect
        "$ESPTOOL" \
            --chip esp32 \
            --port "$PORT" \
            --baud "$FS_BAUD" \
            --before default-reset \
            --after  hard-reset \
            write-flash -z \
            --flash-mode dio \
            --flash-freq 80m \
            --flash-size detect \
            "$FS_OFFSET" "$FS_BIN"
        ok "LittleFS uploadet  ($(du -sh "$FS_BIN" | cut -f1))"
    fi
}

# ─── OTA upload ───────────────────────────────────────────────────────────────
ota_upload() {
    local ip="$1"
    local base_url="http://$ip"

    echo ""
    echo "══════════════════════════════════════════════"
    info "Version: $VERSION"
    info "OTA til: $base_url"
    echo "══════════════════════════════════════════════"

    # Tjek at Gameboxen svarer
    if ! curl -sf --max-time 5 "$base_url/" > /dev/null 2>&1; then
        err "Kan ikke nå $base_url – er Gameboxen tændt og på netværket?"
    fi

    # ElegantOTA v3 bruger to trin:
    #   1. GET /ota/start?mode=<fr|fs>&hash=<md5>   – initierer session
    #   2. POST /ota/upload  (multipart/form-data)   – uploader filen
    _ota_upload() {
        local file="$1" mode="$2" field="$3" label="$4"

        # Beregn MD5 (kræves af ElegantOTA v3 til /ota/start)
        local MD5
        if command -v md5 &>/dev/null; then
            MD5=$(md5 -q "$file")                        # macOS
        else
            MD5=$(md5sum "$file" | cut -d' ' -f1)        # Linux
        fi
        info "MD5: $MD5"

        # Trin 1: Start OTA-session
        local INIT_STATUS
        INIT_STATUS=$(curl -sf --max-time 10 \
            --write-out "%{http_code}" \
            --output /tmp/ota_init.txt \
            "$base_url/ota/start?mode=$mode&hash=$MD5" || echo "000")
        if [ "$INIT_STATUS" != "200" ]; then
            echo "⚠  OTA start fejlede: HTTP $INIT_STATUS"
            cat /tmp/ota_init.txt 2>/dev/null || true
            return 1
        fi

        # Trin 2: Upload fil (multipart/form-data – præcis som browseren gør)
        # -f udelades: ESP32 sender HTTP 100 Continue undervejs, som får curl
        # til at fejle med -f selvom upload faktisk lykkedes. Vi tjekker body i stedet.
        # Timeout 60s: tilstrækkeligt til selv store filer – og vi venter ikke 120s
        # hvis ESP32 genstarter og dropper forbindelsen uden svar (HTTP 000).
        local HTTP_STATUS
        HTTP_STATUS=$(curl -s --max-time 60 \
            --write-out "%{http_code}" \
            --output /tmp/ota_response.txt \
            -F "$field=@$file" \
            "$base_url/ota/upload" 2>/dev/null || echo "000")
        local BODY
        BODY=$(cat /tmp/ota_response.txt 2>/dev/null || true)
        if [ "$HTTP_STATUS" = "200" ] || [ "$BODY" = "OK" ]; then
            ok "$label OTA OK  → $ip  ($(du -sh "$file" | cut -f1))"
        elif [ "$HTTP_STATUS" = "000" ]; then
            # Forbindelsen droppet – ESP32 genstartede efter upload (forventet adfærd)
            ok "$label OTA OK  → $ip  ($(du -sh "$file" | cut -f1))  [genstart]"
        else
            echo "⚠  HTTP $HTTP_STATUS – se response: $BODY"
            return 1
        fi
    }

    if [ "$DO_FIRMWARE" = true ]; then
        step "OTA firmware → $ip  (ElegantOTA v3: start + upload)…"
        _ota_upload "$FW_BIN" "fr" "file" "Firmware"
    fi

    if [ "$DO_DATA" = true ]; then
        step "OTA LittleFS → $ip  (ElegantOTA v3: start + upload)…"
        [ "$DO_FIRMWARE" = true ] && { info "Venter 8s på genstart…"; sleep 8; }
        _ota_upload "$FS_BIN" "fs" "file" "LittleFS"
    fi
}

# ─── Kør ─────────────────────────────────────────────────────────────────────
if [ -n "$OTA_IP" ]; then
    ota_upload "$OTA_IP"
else
    serial_upload
fi

echo ""
ok "Alt færdigt."
