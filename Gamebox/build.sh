#!/usr/bin/env bash
# =============================================================================
#  build.sh  –  Kompiler Gamebox sketch + byg LittleFS image
#
#  Brug:    ./build.sh
#  Output:  .build/Gamebox.ino.bin        (firmware)
#           .build/littlefs.bin            (web-interface + assets)
# =============================================================================
set -euo pipefail

# ─── Stier (auto-detekteret) ─────────────────────────────────────────────────
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SKETCH_DIR/.build"
DATA_DIR="$SKETCH_DIR/data"
ARDUINO_DATA="$HOME/Library/Arduino15"

# arduino-cli inde i Arduino IDE bundles
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
    echo "    Sørg for at ESP32-pakken er installeret i Arduino IDE."
    exit 1
fi

# ─── Board-konfiguration (præcis hvad Arduino IDE bruger) ────────────────────
FQBN="esp32:esp32:esp32:UploadSpeed=921600,CPUFreq=240,FlashFreq=80,\
FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,\
PSRAM=disabled,LoopCore=1,EventsCore=1,EraseFlash=none,\
JTAGAdapter=default,ZigbeeMode=default"

# ─── LittleFS partition (default.csv, 4MB flash) ─────────────────────────────
FS_SIZE_BYTES=$((0x160000))   # 1.375 MB  (se build/esp32.esp32.esp32/partitions.csv)
FS_BLOCK=4096
FS_PAGE=256

# ─────────────────────────────────────────────────────────────────────────────

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
step() { echo -e "\n${YELLOW}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
err()  { echo -e "${RED}✗ $1${NC}" >&2; exit 1; }
info() { echo -e "${CYAN}  $1${NC}"; }

# ─── Læs versionsnummer fra Gamebox.ino ──────────────────────────────────────
VERSION=$(grep -m1 'FirmwareVersion' "$SKETCH_DIR/Gamebox.ino" \
    | grep -oE '"[^"]+"' | tr -d '"')
[ -z "$VERSION" ] && VERSION="ukendt"

mkdir -p "$BUILD_DIR"

echo ""
echo "═══════════════════════════════════════════════"
echo "  Gamebox build"
info "Version: $VERSION"
echo "═══════════════════════════════════════════════"

# ─── 1. Kompiler sketch ───────────────────────────────────────────────────────
step "Kompilerer sketch  (FQBN: esp32:esp32:esp32 / default partitions)…"
"$ARDUINO_CLI" compile \
    --config-file "$HOME/.arduinoIDE/arduino-cli.yaml" \
    --fqbn "$FQBN" \
    --output-dir "$BUILD_DIR" \
    "$SKETCH_DIR"
ok "Firmware bygget → $BUILD_DIR/Gamebox.ino.bin"

# ─── 2. Byg admin-frontend og kopier til data/ ───────────────────────────────
ADMIN_DIR="$SKETCH_DIR/gamebox-admin"
if [ -f "$ADMIN_DIR/package.json" ]; then
    step "Bygger admin-frontend (npm run build)…"
    (cd "$ADMIN_DIR" && npm run build --silent)
    rm -rf "$DATA_DIR"
    cp -r "$ADMIN_DIR/dist" "$DATA_DIR"
    ok "Admin kopieret → data/"
fi

# ─── 3. Byg LittleFS image ───────────────────────────────────────────────────
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
info "Version: $VERSION"
echo "═══════════════════════════════════════════════"
ls -lh "$BUILD_DIR"/*.bin 2>/dev/null | awk '{print "  " $5 "  " $NF}'
echo ""
echo "  Upload firmware:  ./upload.sh /dev/cu.XXXX"
echo "  Upload alt:       ./upload.sh /dev/cu.XXXX --data"
echo "  OTA (kun FW):     ./upload.sh --ota 192.168.x.x"
echo "  OTA (alt):        ./upload.sh --ota 192.168.x.x --data"
echo "  OTA (kun admin):  ./upload.sh --ota 192.168.x.x --data-only"
echo "═══════════════════════════════════════════════"
