#!/usr/bin/env bash
# =============================================================================
#  upload.sh  –  Upload Joystick firmware og/eller LittleFS
#
#  Brug (seriel):
#    ./upload.sh /dev/cu.usbmodem2101             # kun firmware
#    ./upload.sh /dev/cu.usbmodem2101 --data      # firmware + LittleFS
#    ./upload.sh /dev/cu.usbmodem2101 --data-only # kun LittleFS
#
#  Brug (OTA over WiFi – kræver joysticket er koblet på hjemme-WiFi):
#    ./upload.sh --ota 192.168.1.42               # kun firmware
#    ./upload.sh --ota 192.168.1.42 --data        # firmware + LittleFS
#    ./upload.sh --ota 192.168.1.42 --data-only   # kun LittleFS
#
#  Tip til 30 joysticks via OTA på én gang:
#    for ip in 192.168.1.{10..39}; do ./upload.sh --ota $ip & done; wait
#
#  OBS (seriel): ESP32-S3 QT Py har en UF2-bootloader der midlertidigt
#  overtager USB efter reset. Scriptet venter automatisk på at porten
#  kommer tilbage inden LittleFS uploades.
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
[ -z "$ESPTOOL" ] && { echo "❌  esptool ikke fundet"; exit 1; }

# ─── Flash-adresser (huge_app partition scheme, 4MB) ─────────────────────────
APP_OFFSET="0x10000"
FS_OFFSET="0x310000"   # huge_app.csv: spiffs offset
BAUD="921600"

# ─────────────────────────────────────────────────────────────────────────────

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
step() { echo -e "\n${YELLOW}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
err()  { echo -e "${RED}✗ $1${NC}" >&2; exit 1; }
info() { echo -e "${CYAN}  $1${NC}"; }

# Vent til porten eksisterer OG er stabil (UF2-bootloader er færdig)
wait_for_port() {
    local port="$1" timeout=30 elapsed=0
    while [ ! -e "$port" ] && [ $elapsed -lt $timeout ]; do
        sleep 1; ((elapsed++))
    done
    [ ! -e "$port" ] && return 1
    sleep 2   # ekstra settle-tid efter porten dukker op
    return 0
}

# Kør esptool med automatisk retry ved forbindelsesfejl
esptool_retry() {
    local max=3
    for attempt in $(seq 1 $max); do
        if "$ESPTOOL" "$@"; then
            return 0
        fi
        if [ $attempt -lt $max ]; then
            info "Forbindelsen fejlede – venter 4s og prøver igen (forsøg $attempt/$max)…"
            sleep 4
        fi
    done
    return 1
}

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
        --ota)       shift; OTA_IP="${1:-}"; shift ;;
        --data)      DO_DATA=true; shift ;;
        --data-only) DO_DATA=true; DO_FIRMWARE=false; shift ;;
        /dev/*)      PORT="$1"; shift ;;
        *)           err "Ukendt parameter: $1" ;;
    esac
done

if [ -z "$PORT" ] && [ -z "$OTA_IP" ]; then
    err "Angiv en seriel port (/dev/cu.XXXX) eller --ota <IP>"
fi

FW_BIN="$BUILD_DIR/joystick.ino.bin"
FS_BIN="$BUILD_DIR/littlefs.bin"
[ "$DO_FIRMWARE" = true ]  && [ ! -f "$FW_BIN" ] && \
    err "Ingen firmware fundet: $FW_BIN\nKør ./build.sh først."
[ "$DO_DATA" = true ] && [ ! -f "$FS_BIN" ] && \
    err "Ingen LittleFS image: $FS_BIN\nKør ./build.sh først."

# ─── Seriel upload ────────────────────────────────────────────────────────────
serial_upload() {
    echo ""
    echo "══════════════════════════════════════════════"
    info "Port:  $PORT"
    echo "══════════════════════════════════════════════"

    # Vent på at porten er stabil inden vi starter (UF2-bootloader kan være aktiv)
    info "Kontrollerer port…"
    wait_for_port "$PORT" || err "Port $PORT ikke tilgængelig"

    if [ "$DO_FIRMWARE" = true ]; then
        step "Upload firmware → $PORT  (baud $BAUD, dio)…"
        esptool_retry \
            --chip esp32s3 \
            --port "$PORT" \
            --baud "$BAUD" \
            --before default-reset \
            --after  hard-reset \
            write-flash \
            --flash-mode dio \
            --flash-freq 80m \
            --flash-size detect \
            "$APP_OFFSET" "$FW_BIN"
        ok "Firmware uploadet  ($(du -sh "$FW_BIN" | cut -f1))"
    fi

    if [ "$DO_DATA" = true ]; then
        # Efter firmware-upload: vent på at UF2-bootloader er færdig og porten er klar
        if [ "$DO_FIRMWARE" = true ]; then
            info "Venter på at ESP32-S3 genstarter (UF2-bootloader)…"
            for _ in {1..15}; do ls "$PORT" 2>/dev/null || break; sleep 1; done
            wait_for_port "$PORT" || err "Port $PORT kom ikke tilbage efter reset"
        fi

        step "Upload LittleFS → $PORT  (baud $BAUD, dio, -z)…"
        esptool_retry \
            --chip esp32s3 \
            --port "$PORT" \
            --baud "$BAUD" \
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
    info "OTA til:  $base_url"
    echo "══════════════════════════════════════════════"

    if ! curl -sf --max-time 5 "$base_url/" > /dev/null 2>&1; then
        err "Kan ikke nå $base_url – er joysticket koblet på WiFi? (pakke 250 → WiFi)"
    fi

    _ota_upload() {
        local file="$1" mode="$2" field="$3" label="$4"

        local MD5
        if command -v md5 &>/dev/null; then
            MD5=$(md5 -q "$file")
        else
            MD5=$(md5sum "$file" | cut -d' ' -f1)
        fi
        info "MD5: $MD5"

        local INIT_STATUS
        # Brug IKKE -f: det giver exit 22 ved HTTP 4xx og sammenblander statuskode med "000"
        INIT_STATUS=$(curl -s --max-time 10 \
            -w "%{http_code}" \
            -o /tmp/ota_init.txt \
            "$base_url/ota/start?mode=$mode&hash=$MD5" 2>/dev/null) || INIT_STATUS="000"
        INIT_STATUS="${INIT_STATUS:-000}"
        if [ "$INIT_STATUS" != "200" ]; then
            echo "⚠  OTA start fejlede: HTTP $INIT_STATUS"
            echo "   Response: $(cat /tmp/ota_init.txt 2>/dev/null || echo '(ingen body)')"
            return 1
        fi

        local HTTP_STATUS
        HTTP_STATUS=$(curl -s --max-time 120 \
            -w "%{http_code}" \
            -o /tmp/ota_response.txt \
            -F "$field=@$file" \
            "$base_url/ota/upload" 2>/dev/null) || HTTP_STATUS="000"
        HTTP_STATUS="${HTTP_STATUS:-000}"
        if [ "$HTTP_STATUS" = "200" ]; then
            ok "$label OTA OK  → $ip  ($(du -sh "$file" | cut -f1))"
        else
            echo "⚠  HTTP $HTTP_STATUS – se response:"
            cat /tmp/ota_response.txt 2>/dev/null || true
            return 1
        fi
    }

    if [ "$DO_FIRMWARE" = true ]; then
        step "OTA firmware → $ip  (ElegantOTA v3)…"
        _ota_upload "$FW_BIN" "fr" "file" "Firmware"
    fi

    if [ "$DO_DATA" = true ]; then
        step "OTA LittleFS → $ip  (ElegantOTA v3)…"
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
