# setup/kiosk/ — start the emulator automatically at login

Turns a Raspberry Pi (or any Linux desktop) into an MSX appliance: log in, and the
emulator comes up **fullscreen** on its own — no file manager, no clicking.

This is the **low-risk** kiosk on purpose. The desktop still boots normally, so your
mouse, terminal and SSH all keep working; the emulator just launches on top of it.
Nothing outside your `$HOME` is touched — **no `sudo`, no boot-config edits, no
autologin changes** — and uninstalling puts everything back.

## Install

```bash
cd ~/sony-msx-hbf1xv
setup/kiosk/install-kiosk.sh
```

Then reboot (or log out and back in). To try it immediately without rebooting:

```bash
setup/kiosk/launch.sh
```

## Manage

```bash
setup/kiosk/install-kiosk.sh --status      # is it enabled? what will it run?
setup/kiosk/install-kiosk.sh --uninstall   # back to a normal desktop
setup/kiosk/install-kiosk.sh --dry-run     # show actions, change nothing
```

It installs exactly one file — `~/.config/autostart/msx-hbf1xv-kiosk.desktop` — whose
`Exec` points at [`launch.sh`](launch.sh) **in this repo**, so the launch behavior is
version-controlled and `git pull` keeps it current.

## Passing extra emulator arguments

Don't edit the tracked scripts. Create an untracked args file instead:

```bash
mkdir -p ~/.config/msx-hbf1xv
cat > ~/.config/msx-hbf1xv/kiosk.args <<'EOF'
# one or more arguments per line; '#' starts a comment
# boot straight into a game disk:
--disk
games/disks/F1/f1-d2.dsk
EOF
```

Paths are relative to the repo root. `--fullscreen` is always applied first, so you only
list what you want to add (`--scale`, `--cart1`, `--volume`, …).

## What `launch.sh` handles for you

- **Working directory.** As of v1.7.0 the emulator locates the project root from its own
  executable, so it finds `bios/`, `roms/`, `disks/` and the config wherever it is launched
  from — autostart included. The launcher still `cd`s to the repo root first, which keeps any
  relative path you pass in an args file (e.g. `--disk games/disks/...`) resolving as written.
- **Missing BIOS.** This project ships no proprietary BIOS (you supply your own — see
  [`../../bios/README.md`](../../bios/README.md)). Without it the machine boots a
  `0xFF`-filled ROM and shows a black screen, which is baffling on a kiosk with no
  terminal in sight. The launcher checks first and fails with a clear message.
- **Not built yet.** Reports the exact `setup/build.sh` command instead of dying silently.

## Notes

- On the official 7″ touchscreen, touch maps to mouse — so the in-window menu bar
  (File / Machine / Disk …) is fully usable for swapping disks and cartridges with no
  keyboard attached.
- Quit the emulator with **File ▸ Exit** (or `Alt+F4`); you land back on the desktop.
  **Alt+Enter** toggles fullscreen if you want a window.
- To stop the display blanking after ~10 minutes of no input, disable screen blanking
  in **Preferences ▸ Screensaver** (or `xset s off -dpms` in your session autostart).

## Going further (not installed here)

A *true* kiosk — no desktop at all, SDL3 driving the display directly via KMSDRM after a
console autologin — boots faster and feels more like an appliance, but it removes your
graphical safety net and needs `sudo` plus boot-configuration changes. It is deliberately
**not** automated by this script. If you ever want it, the shape is: `raspi-config` →
console autologin, add yourself to the `video`, `render` and `input` groups, and invoke
`launch.sh` from `~/.bash_profile` guarded by `[ -z "$SSH_TTY" ] && [ "$(tty)" = /dev/tty1 ]`
so SSH sessions never launch the emulator. Verify SSH works **before** enabling it.
