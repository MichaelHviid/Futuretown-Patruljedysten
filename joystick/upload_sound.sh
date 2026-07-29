#!/bin/bash
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
ESPTOOL=~/Library/Arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool
FQBN="esp32:esp32:adafruit_qtpy_esp32s3_n4r2:PartitionScheme=huge_app"
PORT="/dev/cu.usbmodem2101"
SKETCH="$(dirname "$0")"
BUILD="$SKETCH/build"
FS_OFFSET="0x310000"

if [ ! -d "$BUILD" ]; then
    echo "FEJL: Ingen build-mappe fundet. Kør ./build.sh først."
    exit 1
fi

echo "=== 1/2 Uploader sketch ==="
#"$CLI" upload --fqbn "$FQBN" --port "$PORT" --input-dir "$BUILD" || exit 1

echo ""
echo "Venter 12 sekunder på at enheden genstarter..."
#sleep 12

echo "=== 2/2 Uploader lydfiler (LittleFS) ==="
"$ESPTOOL" --chip esp32s3 --port "$PORT" --baud 921600 \
    --before default_reset --after hard_reset \
    write-flash -z --flash-mode dio --flash-freq 80m --flash-size detect \
    "$FS_OFFSET" "$BUILD/littlefs.bin"

echo ""
echo "=== Færdig ==="
