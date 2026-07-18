# games/ — Local Game Library

This folder holds the **owner-provisioned game library** for the Sony HB-F1XV emulator.
Only the **directory skeleton and this README** are published; every game file is
third-party intellectual property and is **never tracked, committed, or redistributed**
(standing repo policy, DEC-0047). Populate it locally with your own legally-sourced files.

## Directory structure

```
games/
├── disks/                  Floppy-disk games (.dsk images)
│   └── <title>/            One folder per title — multi-disk sets live together
│       ├── <title>-d1.dsk  Disk 1, 2, 3 … (any naming; you pick the order on the CLI)
│       ├── <title>-d2.dsk
│       └── <title>-save.dsk   Optional dedicated save disk
└── roms/                   Cartridge games (.rom images)
    └── <title>/            One folder per title
        └── <title>.rom
```

Folder names may contain spaces (e.g. `Metal Gear 2/`) — **always quote paths** in the shell.

## Supported file types

| Type | Format | Notes |
|------|--------|-------|
| `.dsk` | 720 KB 3.5" DD, exactly **737,280 bytes** (80 tracks × 2 sides × 9 sectors × 512 B), MSX-DOS FAT12 | The HB-F1XV's native floppy format. Images of other sizes are rejected. Uppercase `.DSK` works too. |
| `.rom` | Raw MSX cartridge dump | Mapper is **auto-detected** (software-database SHA-1 match, then a bank-write heuristic). Supported mappers: `KonamiSCC`, `Konami`, `ASCII8`, `ASCII16`, `FMPAC` — force one with `--slotN-type <name>` only if a game misdetects. |
| `<rom>.sram` | 8 KB FM-PAC battery save (openMSX-compatible format) | Auto-created/persisted next to an FM-PAC cartridge; not a game file you provide. |

## Launching games

```powershell
# Cartridge (slot 1; the FM-PAC auto-loads into slot 2 for SRAM saves):
build\Debug\sony_msx_sdl3.exe --slot1 "games\roms\Metal Gear\metal_gear.rom"

# Single-disk game:
build\Debug\sony_msx_sdl3.exe --disk "games\disks\undeadline\undeadline.dsk"

# Multi-disk set (F11 cycles through the list at runtime):
build\Debug\sony_msx_sdl3.exe --disk "games\disks\ys2\ys2-d1.dsk" --disk "games\disks\ys2\ys2-d2.dsk" --disk "games\disks\ys2\ys2-save.dsk"
```

Or at runtime via the in-window menu: **File ▸ Open Cartridge…** / **File ▸ Open Disk(s)…**
(multi-select replaces the F11 rotation). Some titles need **CTRL held during boot**
(single-drive mode) — hold it from power-on until loaded.

## Saves

- **Disk saves** write back to the `.dsk` file by default (`--no-disk-writable` or Alt+S for
  read-only). Create a fresh blank save disk with `utils\msx-diskutil.exe --create save.dsk`
  or the menu's **Disk ▸ New Blank Disk…**.
- **FM-PAC SRAM saves** persist automatically to `<cartridge>.sram`.

## Legal

Game images remain the property of their respective rights holders. This repository
asserts no redistribution rights and publishes none of their content — the folder names
here exist purely as local organizational scaffolding.
