#!/usr/bin/env bash
# =============================================================================
#  upload.sh  –  Upload Remote firmware (ESP32-WROOM-DA)
#
#  Brug (seriel):
#    ./upload.sh /dev/cu.SLAB_USBtoUART
#
#  Brug (OTA – ElegantOTA v3):
#    ./upload.sh --ota 192.168.1.42
#
#  Tip – opdater alle remotes parallelt:
#    for ip in 192.168.1.{50..54}; do ./upload.sh --ota $ip & done; wait
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/.build"
ARDUINO_DATA="$HOME/Library/Arduino15"
FW_BIN="$BUILD_DIR/Remote.ino.bin"

# Udlæs firmware-version fra Remote.ino
VERSION=$(grep -oE 'RemoteVersion[[:space:]]*=[[:space:]]*"[^"]+"' "$SCRIPT_DIR/Remote.ino" | grep -oE '"[^"]+"' | tr -d '"')
[ -z "$VERSION" ] && VERSION="ukendt"

# esptool
ESPTOOL=$(find "$ARDUINO_DATA/packages/esp32/tools/esptool_py" \
    -name "esptool" -not -name "*.py" -type f 2>/dev/null | sort -V | tail -1)
[ -z "$ESPTOOL" ] && ESPTOOL=$(which esptool.py 2>/dev/null || which esptool 2>/dev/null || true)
[ -z "$ESPTOOL" ] && { echo "❌  esptool ikke fundet"; exit 1; }

APP_OFFSET="0x10000"
FW_BAUD="115200"   # Stabilt på alle USB-adaptere (tager ~90s, undgår baud-skift fejl)

# Disse filer sikrer korrekte OTA-partitioner ved seriel flash
BOOT_BIN="$BUILD_DIR/Remote.ino.bootloader.bin"
PART_BIN="$BUILD_DIR/Remote.ino.partitions.bin"
BOOT_APP="$BUILD_DIR/boot_app0.bin"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
step() { echo -e "\n${YELLOW}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
info() { echo -e "${CYAN}  $1${NC}"; }
err()  { echo -e "${RED}✗ $1${NC}" >&2; exit 1; }

# ─── Argument-parsing ────────────────────────────────────────────────────────
PORT="" OTA_IP=""

[ $# -eq 0 ] && { echo "Brug: $0 /dev/cu.XXXX  eller  $0 --ota <IP>"; exit 1; }
while [[ $# -gt 0 ]]; do
    case "$1" in
        --ota)   shift; OTA_IP="${1:-}"; shift ;;
        /dev/*)  PORT="$1"; shift ;;
        *)       err "Ukendt parameter: $1" ;;
    esac
done

[ ! -f "$FW_BIN" ] && err "Ingen firmware: $FW_BIN\nKør ./build.sh først."

# ─── Seriel upload ────────────────────────────────────────────────────────────
if [ -n "$PORT" ]; then
    echo ""
    echo "══════════════════════════════════════════════"
    info "Version:  $VERSION"
    info "Port:     $PORT  (baud $FW_BAUD)"
    echo "══════════════════════════════════════════════"
    # Fuld flash: bootloader + partition-tabel + boot_app + firmware
    # Partition-tabellen SKAL med for at skifte fra no_ota → default (OTA-aktiveret)
    step "Fuld flash → $PORT  (bootloader + partitioner + firmware)…"

    FLASH_ARGS=()
    if [ -f "$BOOT_BIN" ]; then FLASH_ARGS+=(0x1000  "$BOOT_BIN"); fi
    if [ -f "$PART_BIN" ]; then FLASH_ARGS+=(0x8000  "$PART_BIN"); fi
    if [ -f "$BOOT_APP" ]; then FLASH_ARGS+=(0xe000  "$BOOT_APP"); fi
    FLASH_ARGS+=("$APP_OFFSET" "$FW_BIN")

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
        "${FLASH_ARGS[@]}"
    ok "Fuld flash færdig  ($(du -sh "$FW_BIN" | cut -f1))"
    info "OTA er nu aktiveret på næste boot – efterfølgende uploads via ./upload.sh --ota <IP>"
fi

# ─── OTA upload (ElegantOTA v3) ───────────────────────────────────────────────
if [ -n "$OTA_IP" ]; then
    BASE="http://$OTA_IP"
    echo ""
    echo "══════════════════════════════════════════════"
    info "Version:  $VERSION"
    info "OTA til:  $BASE"
    echo "══════════════════════════════════════════════"

    if ! curl -sf --max-time 5 "$BASE/" > /dev/null 2>&1; then
        err "Kan ikke nå $BASE – er Remote'en tændt?"
    fi

    # Beregn MD5 (kræves af ElegantOTA v3 til /ota/start)
    if command -v md5 &>/dev/null; then MD5=$(md5 -q "$FW_BIN")
    else MD5=$(md5sum "$FW_BIN" | cut -d' ' -f1); fi
    info "MD5: $MD5"

    step "OTA trin 1: start session …"
    INIT=$(curl -sf --max-time 10 \
        --write-out "%{http_code}" --output /tmp/ota_init.txt \
        "$BASE/ota/start?mode=fr&hash=$MD5" || echo "000")
    [ "$INIT" != "200" ] && { echo "⚠  Start fejlede: HTTP $INIT"; cat /tmp/ota_init.txt 2>/dev/null; exit 1; }

    step "OTA trin 2: upload firmware …"
    HTTP=$(curl -sf --max-time 120 \
        --write-out "%{http_code}" --output /tmp/ota_resp.txt \
        -F "file=@$FW_BIN" \
        "$BASE/ota/upload" || echo "000")
    if [ "$HTTP" = "200" ]; then
        ok "OTA OK  → $OTA_IP  ($(du -sh "$FW_BIN" | cut -f1))"
    else
        echo "⚠  HTTP $HTTP – se response:"
        cat /tmp/ota_resp.txt 2>/dev/null || true
        exit 1
    fi
fi

echo ""
ok "Færdig  ·  version $VERSION"
