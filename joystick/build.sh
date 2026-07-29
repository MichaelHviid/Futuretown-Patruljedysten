#!/usr/bin/env bash
# =============================================================================
#  build.sh  –  Kompiler Joystick sketch + byg LittleFS image
#
#  Brug:    ./build.sh
#  Output:  .build/joystick.ino.bin   (firmware)
#           .build/littlefs.bin        (lydfiler + web-interface)
# =============================================================================
set -euo pipefail

# ─── Stier (auto-detekteret) ─────────────────────────────────────────────────
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SKETCH_DIR/.build"
DATA_DIR="$SKETCH_DIR/data"
ARDUINO_DATA="$HOME/Library/Arduino15"

# arduino-cli inde i Arduino IDE bundle
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
if [ ! -x "$ARDUINO_CLI" ]; then
    ARDUINO_CLI=$(which arduino-cli 2>/dev/null || true)
fi
if [ -z "$ARDUINO_CLI" ] || [ ! -x "$ARDUINO_CLI" ]; then
    echo "❌  arduino-cli ikke fundet. Installer Arduino IDE 2 eller arduino-cli."
    exit 1
fi

# Seneste mklittlefs fra ESP32-pakken
MKLITTLEFS=$(find "$ARDUINO_DATA/packages/esp32/tools/mklittlefs" \
    -name "mklittlefs" -type f 2>/dev/null | sort -V | tail -1)
if [ -z "$MKLITTLEFS" ]; then
    echo "❌  mklittlefs ikke fundet under $ARDUINO_DATA/packages/esp32/tools/mklittlefs"
    exit 1
fi

# ─── Board-konfiguration ─────────────────────────────────────────────────────
FQBN="esp32:esp32:adafruit_qtpy_esp32s3_n4r2:PartitionScheme=huge_app"

# ─── LittleFS partition (huge_app.csv, 4MB flash) ────────────────────────────
# spiffs: 0x310000, size 0xE0000 (917504 bytes = ~896 KB)
FS_SIZE_BYTES=$((0xE0000))
FS_BLOCK=4096
FS_PAGE=256

# ─────────────────────────────────────────────────────────────────────────────

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
step() { echo -e "\n${YELLOW}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
err()  { echo -e "${RED}✗ $1${NC}" >&2; exit 1; }

mkdir -p "$BUILD_DIR"

# ─── 1. Kompiler sketch ───────────────────────────────────────────────────────
step "Kompilerer sketch  (FQBN: adafruit_qtpy_esp32s3_n4r2 / huge_app)…"
"$ARDUINO_CLI" compile \
    --fqbn "$FQBN" \
    --output-dir "$BUILD_DIR" \
    "$SKETCH_DIR"
ok "Firmware bygget → $BUILD_DIR/joystick.ino.bin"

# ─── 2. Byg LittleFS image ───────────────────────────────────────────────────
if [ -d "$DATA_DIR" ] && [ -n "$(ls -A "$DATA_DIR" 2>/dev/null)" ]; then
    step "Bygger LittleFS image af $DATA_DIR …"
    "$MKLITTLEFS" \
        -c "$DATA_DIR" \
        -s "$FS_SIZE_BYTES" \
        -b "$FS_BLOCK" \
        -p "$FS_PAGE" \
        "$BUILD_DIR/littlefs.bin"
    FS_USED=$(du -sh "$DATA_DIR" | cut -f1)
    ok "LittleFS image bygget → $BUILD_DIR/littlefs.bin  (data: $FS_USED)"
else
    echo "⚠  Ingen data/ mappe – springer LittleFS over."
fi

# ─── Opsummering ─────────────────────────────────────────────────────────────
echo ""
echo "═══════════════════════════════════════════════"
echo "  Build færdig"
echo "═══════════════════════════════════════════════"
ls -lh "$BUILD_DIR"/*.bin 2>/dev/null | awk '{print "  " $5 "  " $NF}'
echo ""
echo "  Upload firmware:  ./upload.sh /dev/cu.XXXX"
echo "  Upload alt:       ./upload.sh /dev/cu.XXXX --data"
echo "  OTA (kun FW):     ./upload.sh --ota 192.168.x.x"
echo "  OTA (alt):        ./upload.sh --ota 192.168.x.x --data"
echo "═══════════════════════════════════════════════"
