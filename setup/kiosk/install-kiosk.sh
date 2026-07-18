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
# setup/kiosk/install-kiosk.sh -- start the emulator FULLSCREEN automatically
# when the desktop session logs in (Raspberry Pi OS, or any XDG desktop).
#
# Deliberately the LOW-RISK kiosk: the desktop still boots normally, so your
# mouse, terminal and SSH all keep working -- the emulator simply launches on
# top of it. Nothing outside your own $HOME is touched, no sudo, no boot-config
# or autologin changes, and --uninstall puts it back exactly as it was.
#
# It installs ONE file:
#     ~/.config/autostart/msx-hbf1xv-kiosk.desktop
# whose Exec points at setup/kiosk/launch.sh IN THIS REPO -- so the launch
# behavior is version-controlled and a `git pull` updates it.
#
# Usage:
#   setup/kiosk/install-kiosk.sh                # install (enable autostart)
#   setup/kiosk/install-kiosk.sh --status       # show what is installed
#   setup/kiosk/install-kiosk.sh --uninstall    # remove it
#   setup/kiosk/install-kiosk.sh --dry-run      # print actions, change nothing
#
# To pass extra emulator arguments (boot a disk, set a scale, ...) create an
# untracked args file -- see setup/kiosk/README.md and launch.sh:
#     ~/.config/msx-hbf1xv/kiosk.args

set -euo pipefail

ACTION="install"
DRY=0

while [ $# -gt 0 ]; do
    case "$1" in
        --uninstall|--remove) ACTION="uninstall"; shift ;;
        --status)             ACTION="status";    shift ;;
        --dry-run)            DRY=1;              shift ;;
        -h|--help)            sed -n '15,36p' "$0"; exit 0 ;;
        *) echo "[kiosk] unknown argument: $1" >&2; exit 2 ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/../.." && pwd)"
LAUNCH="$SCRIPT_DIR/launch.sh"

AUTOSTART_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"
ENTRY="$AUTOSTART_DIR/msx-hbf1xv-kiosk.desktop"

run() {  # echo-and-do, or just echo under --dry-run
    if [ "$DRY" -eq 1 ]; then
        echo "[kiosk][dry-run] $*"
    else
        "$@"
    fi
}

case "$ACTION" in

status)
    echo "[kiosk] repo        : $REPO"
    echo "[kiosk] launcher    : $LAUNCH"
    [ -x "$LAUNCH" ] && echo "[kiosk]               (executable)" \
                     || echo "[kiosk]               (NOT executable -- run install to fix)"
    echo "[kiosk] autostart   : $ENTRY"
    if [ -f "$ENTRY" ]; then
        echo "[kiosk] state       : ENABLED"
        echo "---"
        cat "$ENTRY"
    else
        echo "[kiosk] state       : not installed"
    fi
    ARGS_FILE="$HOME/.config/msx-hbf1xv/kiosk.args"
    if [ -f "$ARGS_FILE" ]; then
        echo "---"
        echo "[kiosk] extra args  : $ARGS_FILE"
        sed 's/^/           /' "$ARGS_FILE"
    fi
    ;;

uninstall)
    if [ -f "$ENTRY" ]; then
        run rm -f "$ENTRY"
        echo "[kiosk] removed autostart entry: $ENTRY"
    else
        echo "[kiosk] nothing to remove (no entry at $ENTRY)"
    fi
    echo "[kiosk] the desktop will boot normally on next login."
    echo "[kiosk] note: your args file (if any) and the repo are left untouched."
    ;;

install)
    # The .desktop Exec field does not survive spaces in a path; refuse rather
    # than silently install something that will not launch.
    case "$REPO" in
        *" "*) echo "[kiosk] repo path contains spaces, which a .desktop Exec cannot handle:" >&2
               echo "[kiosk]   $REPO" >&2
               echo "[kiosk] move the checkout to a space-free path and re-run." >&2
               exit 1 ;;
    esac

    [ -f "$LAUNCH" ] || { echo "[kiosk] missing launcher: $LAUNCH" >&2; exit 1; }
    run chmod +x "$LAUNCH"

    if [ ! -x "$REPO/build/sony_msx_sdl3" ]; then
        echo "[kiosk] WARNING: emulator not built yet at $REPO/build/sony_msx_sdl3"
        echo "[kiosk]          autostart is still installed; build before rebooting:"
        echo "[kiosk]            cd $REPO && setup/build.sh"
    fi

    run mkdir -p "$AUTOSTART_DIR"

    if [ "$DRY" -eq 1 ]; then
        echo "[kiosk][dry-run] write $ENTRY (Exec=$LAUNCH)"
    else
        cat > "$ENTRY" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Sony HB-F1XV MSX2+
Comment=Start the MSX2+ emulator fullscreen at login
Exec=$LAUNCH
Path=$REPO
Terminal=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
EOF
    fi

    if [ "$DRY" -eq 1 ]; then
        echo "[kiosk][dry-run] nothing was changed."
    else
        echo "[kiosk] installed autostart entry: $ENTRY"
    fi
    echo "[kiosk]   -> runs: $LAUNCH (fullscreen, working dir $REPO)"
    echo "[kiosk] test it now without rebooting:"
    echo "[kiosk]   $LAUNCH"
    echo "[kiosk] to undo:"
    echo "[kiosk]   $SCRIPT_DIR/install-kiosk.sh --uninstall"
    ;;

esac
