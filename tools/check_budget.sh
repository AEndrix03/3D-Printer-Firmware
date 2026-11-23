#!/usr/bin/env bash
# Compila il firmware per Arduino Uno e verifica che resti nei limiti di memoria.
# Su un ATmega328P (32 KB flash, 2 KB RAM) la RAM non usata dalle variabili globali
# e' tutto lo spazio disponibile per lo stack: se scende troppo il firmware diventa instabile.
#
#   tools/check_budget.sh            verifica i limiti
#   tools/check_budget.sh --symbols  elenca anche le variabili che occupano piu' RAM
set -euo pipefail

MAX_FLASH="${MAX_FLASH:-27500}"   # byte (limite fisico: 32256): soglia stretta, per non perdere i guadagni
MAX_GLOBALS="${MAX_GLOBALS:-1000}" # byte: lascia almeno ~1050 byte allo stack

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build/budget}"
mkdir -p "$BUILD"

arduino-cli compile --fqbn arduino:avr:uno \
    --build-property "compiler.cpp.extra_flags=-DPLATFORM_ARDUINO" \
    --build-path "$BUILD" --warnings all "$ROOT" > "$BUILD/compile.log" 2>&1 || {
    cat "$BUILD/compile.log"
    exit 1
}

if grep -E "warning:" "$BUILD/compile.log" | grep -v -E "cores[/\\]arduino" | grep -q .; then
    echo "Compiler warnings in firmware sources:"
    grep -E "warning:" "$BUILD/compile.log" | grep -v -E "cores[/\\]arduino"
    exit 1
fi

ELF="$(find "$BUILD" -maxdepth 1 -name '*.elf' | head -n 1)"
SIZE_TOOL="$(find "${ARDUINO_DATA:-$HOME/.arduino15}" "$HOME/AppData/Local/Arduino15" \
    -name 'avr-size*' -type f 2>/dev/null | head -n 1 || true)"
if [ -z "$SIZE_TOOL" ] || [ -z "$ELF" ]; then
    echo "avr-size or firmware .elf not found" >&2
    exit 2
fi

read -r text data bss _ < <("$SIZE_TOOL" "$ELF" | tail -n 1)
flash=$((text + data))
globals=$((data + bss))

echo "flash:   $flash / 32256 byte (limite $MAX_FLASH)"
echo "globals: $globals / 2048 byte (limite $MAX_GLOBALS) -> $((2048 - globals)) byte per lo stack"

if [ "${1:-}" = "--symbols" ]; then
    NM_TOOL="$(dirname "$SIZE_TOOL")/avr-nm"
    echo
    echo "Variabili che occupano piu' RAM:"
    "$NM_TOOL" --size-sort --reverse-sort -C "$ELF" | grep -i -E " [bd] " | head -n 15
fi

status=0
[ "$flash" -le "$MAX_FLASH" ] || { echo "ERROR: flash budget exceeded"; status=1; }
[ "$globals" -le "$MAX_GLOBALS" ] || { echo "ERROR: RAM budget exceeded"; status=1; }
exit $status
