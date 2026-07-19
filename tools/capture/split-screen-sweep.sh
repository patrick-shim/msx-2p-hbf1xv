#!/usr/bin/env bash
# Local M38 validation helper: replays the split-screen title input-script on the
# headless build to a list of frame targets, dumping+converting each to PNG.
# Each run re-emulates deterministically from cold boot with the title disk mounted.
set -euo pipefail
cd "$(dirname "$0")/.."

EXE=build/Debug/sony_msx_headless.exe
BIOS=bios
DISK="disks/games/<title>/game-d1.dsk"
SCRIPT=debug/split-screen-m38/game.script
ROOT="${ROOT:-debug/split-screen-m38}"
PREFIX="${PREFIX:-f}"

for N in "$@"; do
  name="${PREFIX}$(printf '%05d' "$N")"
  "$EXE" --debug-session "$BIOS" 999999999 --stock \
      --frames "$N" --disk "$DISK" --input-script "$SCRIPT" \
      --debug-root "$ROOT" --dump-frame "$name" >/dev/null 2>&1
  python tools/convert/frame-to-png.py "$ROOT/frames/$name" -o "$ROOT/${name}.png" 2>/dev/null \
      | sed 's/^/  /'
  echo "captured frame $N -> $ROOT/${name}.png"
done
