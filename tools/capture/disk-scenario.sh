#!/usr/bin/env bash
# M41-S4 disk-scenario A/B helper (measurement only; no src touched).
# Types an identical matrix-event sequence at the Disk-BASIC Ok prompt on BOTH
# engines (ours via --input-script, openMSX via keymatrixdown/up from
# tools/gen/typed-basic-program.py), on a writable copy of a genuinely-bootable empty
# Disk-BASIC image, then A/B-diffs the settled screen. Optionally re-reads a
# named file from BOTH written .dsk images to prove the on-disk bytes agree.
#
# Usage:
#   tools/capture/disk-scenario.sh <id> <text> <startframe> <omxstart> <frames> [readfile]
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$(pwd)"
ID="$1"; TEXT="$2"; SF="$3"; OS="$4"; FR="$5"; READFILE="${6:-}"
# openMSX realtime injection needs slow, well-spaced keys (esp. after disk ops,
# which stall the BIOS keyboard scan) or it drops keystrokes. Ours is
# deterministic and uses gen-typed-ab defaults. These are the openMSX-side only.
OMXHOLD="${7:-0.14}"; OMXGAP="${8:-0.26}"
SDIR="debug/m41/s4/$ID"
mkdir -p "$SDIR/frames"
cp debug/m41/s4/bootblank.dsk "$SDIR/ours.dsk"
cp debug/m41/s4/bootblank.dsk "$SDIR/omx.dsk"
python tools/gen/typed-basic-program.py --text "$TEXT" \
  --out-script "$SDIR/t.script" --out-tcl "$SDIR/t.tcl" \
  --start-frame "$SF" --omx-start "$OS" \
  --omx-hold "$OMXHOLD" --omx-gap "$OMXGAP" >/dev/null

# OURS (writable)
build/Debug/sony_msx_headless.exe --debug-session bios 99999999 --stock \
  --disk "$SDIR/ours.dsk" --input-script "$SDIR/t.script" \
  --frames "$FR" --disk-writable \
  --debug-root "$SDIR" --dump-frame frame 2>"$SDIR/ours.err" 1>/dev/null
tail -1 "$SDIR/ours.err"
python tools/convert/frame-to-png.py "$SDIR/frames/frame" -o "$SDIR/ours.png" 2>&1 | grep -oE "sha256=[0-9a-f]{12}" || true

# openMSX (realtime; -diska writes back)
OMXDSK=$(wsl -e wslpath -a "$REPO/$SDIR/omx.dsk")
TCLW=$(wsl -e wslpath -a "$REPO/$SDIR/t.tcl")
SDIRW=$(wsl -e wslpath -a "$REPO/$SDIR")
# openMSX runs the ~30s MSX2+ boot logo at ~0.3x realtime in WSL (CPU-bound, no
# GPU), so give a generous wall timeout; the guest still runs at correct EMU time
# so key events + the screenshot fire at their scripted emu-times.
wsl -e bash -lc "cp '$TCLW' /tmp/m41s.tcl; rm -f /tmp/m41typed.png; timeout -k 3 400 openmsx -machine Sony_HB-F1XV -diska '$OMXDSK' -script /tmp/m41s.tcl 2>&1 | grep -aiE 'parse|error:' | grep -aviE 'sequencer|midi' | head; cp /tmp/m41typed.png '$SDIRW/omx_raw.png' && echo OMX_COPIED || echo OMX_NO_SHOT" 2>&1 | tail -2

# screen diff
python tools/analyze/scroll-ab-diff.py --ours "$SDIR/ours.png" --omx "$SDIR/omx_raw.png" \
  --ox 32 --oy 24 --out "$SDIR/${ID}_ab.png" --label "$ID" 2>&1 | tail -1

# optional on-disk byte round-trip
if [ -n "$READFILE" ]; then
  python tools/gen/basic-to-disk.py --read "$READFILE" --image "$SDIR/ours.dsk" --read-out "$SDIR/ours_$READFILE" 2>&1 | tail -1 || echo "OURS: $READFILE NOT FOUND"
  python tools/gen/basic-to-disk.py --read "$READFILE" --image "$SDIR/omx.dsk"  --read-out "$SDIR/omx_$READFILE"  2>&1 | tail -1 || echo "OMX: $READFILE NOT FOUND"
fi
