#!/usr/bin/env bash
# =============================================================================
#  build.sh  –  Kompiler Remote sketch (ESP32-WROOM-DA)
#
#  Brug:    ./build.sh
#  Output:  .build/Remote.ino.bin
# =============================================================================
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SKETCH_DIR/.build"
ARDUINO_DATA="$HOME/Library/Arduino15"

# Udlæs firmware-version fra Remote.ino
VERSION=$(grep -oE 'RemoteVersion[[:space:]]*=[[:space:]]*"[^"]+"' "$SKETCH_DIR/Remote.ino" | grep -oE '"[^"]+"' | tr -d '"')
[ -z "$VERSION" ] && VERSION="ukendt"

ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
if [ ! -x "$ARDUINO_CLI" ]; then
    ARDUINO_CLI=$(which arduino-cli 2>/dev/null || true)
fi
[ -z "$ARDUINO_CLI" ] || [ ! -x "$ARDUINO_CLI" ] && { echo "❌  arduino-cli ikke fundet."; exit 1; }

# ESP32-WROOM-DA bruger samme chip som standard ESP32
# Eneste forskel er dual-antenna modulet – samme FQBN
FQBN="esp32:esp32:esp32:UploadSpeed=921600,CPUFreq=240,FlashFreq=80,\
FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,\
PSRAM=disabled,LoopCore=1,EventsCore=1,EraseFlash=none,\
JTAGAdapter=default,ZigbeeMode=default"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
step() { echo -e "\n${YELLOW}▶ $1${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }

mkdir -p "$BUILD_DIR"

step "Kompilerer Remote sketch v$VERSION (ESP32-WROOM-DA)…"
"$ARDUINO_CLI" compile \
    --config-file "$HOME/.arduinoIDE/arduino-cli.yaml" \
    --fqbn "$FQBN" \
    --output-dir "$BUILD_DIR" \
    "$SKETCH_DIR"
ok "Firmware bygget → $BUILD_DIR/Remote.ino.bin"

echo ""
echo "═══════════════════════════════════════════════"
echo "  Build færdig  ·  version $VERSION"
echo "═══════════════════════════════════════════════"
ls -lh "$BUILD_DIR"/*.bin 2>/dev/null | awk '{print "  " $5 "  " $NF}'
echo ""
echo "  Serial:  ./upload.sh /dev/cu.XXXX"
echo "  OTA:     ./upload.sh --ota 192.168.x.x"
echo "═══════════════════════════════════════════════"
