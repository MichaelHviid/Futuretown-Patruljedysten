#!/usr/bin/env bash
# =============================================================================
#  monitor.sh  –  Serial monitor uden Arduino IDE
#
#  Brug:
#    ./monitor.sh                    # lokal (kun denne Mac)
#    ./monitor.sh --net              # over netværk via TCP port 2323
#                                    # Kræver: brew install socat
#
#  Tilslut fra anden maskine:
#    telnet <denne-macs-ip> 2323
#
#  Afslut screen:  Ctrl+A  derefter  K
#  Afslut socat:   Ctrl+C
# =============================================================================

PORT="/dev/cu.usbmodem2101"
BAUD="115200"
TCP_PORT="2323"

if [ ! -e "$PORT" ]; then
    echo "❌  $PORT ikke fundet – er joysticket tilsluttet?"
    exit 1
fi

if [ "${1:-}" = "--net" ]; then
    if ! command -v socat &>/dev/null; then
        echo "❌  socat ikke fundet. Installer med:  brew install socat"
        exit 1
    fi
    MY_IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || echo "?")
    echo "📡  Serial bridge kører på TCP port $TCP_PORT"
    echo "    Tilslut fra netværket:  telnet $MY_IP $TCP_PORT"
    echo "    Stop med Ctrl+C"
    echo ""
    socat TCP-LISTEN:$TCP_PORT,reuseaddr /dev/"${PORT##*/}",b$BAUD
else
    echo "🖥️   Åbner serial monitor ($PORT @ $BAUD baud)"
    echo "    Afslut:  Ctrl+A  derefter  K"
    echo ""
    screen "$PORT" "$BAUD"
fi
