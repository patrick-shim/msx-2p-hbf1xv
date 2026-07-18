# FM-PAC Firmware and SRAM Directory

This directory holds the FM-PAC peripheral's firmware and its battery-SRAM
save file for local emulator operation and testing. The **firmware is
user-supplied**; the **SRAM save is created automatically by the emulator**
(see "FM-PAC SRAM" below) — you do not provide it.

## Third-Party Firmware

The FM-PAC firmware is third-party proprietary software and is not
included, distributed, licensed, or sublicensed by this project.

Users must provide any required FM-PAC firmware separately and are
solely responsible for obtaining, possessing, copying, and using it
lawfully and in accordance with applicable law and any licence terms
imposed by the relevant rights holders.

References to firmware filenames, versions, sizes, memory layouts, or
cryptographic hashes are provided solely for compatibility
identification and reproducible testing. Such references do not include
the underlying firmware and do not grant any right to obtain, copy,
modify, or redistribute it.

No licence, ownership interest, authorization, waiver, endorsement, or
other right in the FM-PAC firmware or any other third-party intellectual
property is granted, expressed, or implied by this project.

## Local File Placement

Place the FM-PAC firmware here using the filename the emulator expects:

    roms/
    └── fmpac.rom          <- YOU provide this (required for FM-PAC audio + saves)

`fmpac.rom.sram` is **not** something you supply — the emulator's built-in
FM-PAC SRAM feature creates and maintains it for you (see below):

    roms/
    └── fmpac.rom.sram     <- created + written by the emulator (your local saves)

## FM-PAC SRAM (battery save)

This emulator implements the FM-PAC's **8 KB battery-backed SRAM** — the store
behind `CALL FMPAC` and in-game FM-PAC saves. How it behaves:

- If `fmpac.rom.sram` is **absent, the SRAM starts blank** — a fresh boot has no
  saves, exactly like a brand-new FM-PAC cartridge. Nothing to provide.
- When a program writes to FM-PAC SRAM, the emulator persists it to
  `fmpac.rom.sram` in the **openMSX-compatible `.sram` format** (an older
  raw-8192-byte save is migrated losslessly on first write), and reads it back
  on subsequent runs.
- This file is **your own local save data** — runtime state produced by the
  emulator, **not** third-party firmware. Like the firmware, it is never tracked
  or distributed by this project (it stays local). Deleting it simply resets
  your FM-PAC saves.