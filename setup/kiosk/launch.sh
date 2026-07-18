#!/usr/bin/env bash
# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator
#  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
#
#  LEGAL NOTICE - Personal reference only.
#  This source code is made available solely for personal, non-commercial
#  reference and educational study. Commercial use, sale, or redistribution
#  for profit is not permitted without the author's written consent.
#  Provided "AS IS", without warranty of any kind.
#  Proprietary BIOS/ROM/disk assets remain the property of their respective
#  rights holders and are NOT licensed by this notice.
# ============================================================================
#
# setup/kiosk/launch.sh -- the ONE entry point both kiosk modes invoke.
#
# Why a launcher instead of putting the command straight into the autostart
# entry: the emulator resolves bios/ , roms/ , disks/ and sony_msx_hbf1xv.xml
# RELATIVE TO THE WORKING DIRECTORY. An autostart entry or a systemd unit starts
# in / (or $HOME), so a bare command would boot with no BIOS and fail. This
# script always cd's to the repo root first. It also lives IN the repo, so
# `git pull` updates the kiosk launch behavior -- nothing is frozen into a
# hand-edited dotfile on one SD card.
#
# Extra emulator arguments (e.g. --disk, --cart1, --scale) can be supplied
# WITHOUT editing any tracked file, via an optional untracked args file:
#     ~/.config/msx-hbf1xv/kiosk.args
# one or more arguments per line; '#' starts a comment. Example:
#     # boot straight into a game disk
#     --disk
#     games/disks/F1/f1-d2.dsk
# Anything passed on this script's own command line is appended after those.

set -euo pipefail

# Repo root = two levels up from setup/kiosk/ (works regardless of caller CWD).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO"

BIN="$REPO/build/sony_msx_sdl3"
if [ ! -x "$BIN" ]; then
    echo "[kiosk] emulator not built at: $BIN" >&2
    echo "[kiosk] build it first:  cd $REPO && setup/build.sh" >&2
    exit 1
fi

# Fail early and LOUDLY on a missing BIOS -- otherwise the machine boots to a
# 0xFF-filled ROM and just shows a black screen, which is baffling on a kiosk
# with no terminal in sight. (bios/ is user-supplied; see bios/README.md.)
BIOS_DIR="${MSX_KIOSK_BIOS_DIR:-$REPO/bios}"
if [ -z "$(find "$BIOS_DIR" -maxdepth 1 -name '*.rom' -print -quit 2>/dev/null)" ]; then
    echo "[kiosk] no BIOS ROMs found in: $BIOS_DIR" >&2
    echo "[kiosk] this project ships no proprietary BIOS -- supply your own." >&2
    echo "[kiosk] required filenames are listed in: $REPO/bios/README.md" >&2
    exit 1
fi

# Optional untracked args file (see header).
EXTRA=()
ARGS_FILE="${MSX_KIOSK_ARGS_FILE:-$HOME/.config/msx-hbf1xv/kiosk.args}"
if [ -f "$ARGS_FILE" ]; then
    # Word-splitting is INTENTIONAL here: each whitespace-separated token is one
    # argument. Comments stripped first.
    # shellcheck disable=SC2207
    EXTRA=($(sed 's/#.*//' "$ARGS_FILE"))
fi

# --fullscreen is the kiosk default; a caller (or the args file) can still pass
# --windowed-style flags after it if they want.
# The ${arr[@]+"${arr[@]}"} guard is required under `set -u` for empty arrays.
exec "$BIN" --fullscreen ${EXTRA[@]+"${EXTRA[@]}"} "$@"
