## Legal and Intellectual Property Notice

This project is an independent and unofficial emulator developed for
educational, research, preservation, interoperability, and personal-use
purposes. It is not affiliated with, authorized by, endorsed by, or
sponsored by Sony Group Corporation, Microsoft Corporation, ASCII
Corporation, or any other rights holder associated with the original
hardware or the MSX platform.

Sony, HitBit, HB-F1XV, MSX, MSX2+, and all other third-party names,
product designations, trademarks, service marks, and logos referenced
by this project remain the property of their respective owners, where
applicable. Such references are used solely to identify and describe
the hardware and software environment that this emulator is designed
to reproduce.

This repository and its release packages do not contain or distribute
any proprietary Sony BIOS, firmware, ROM image, system software,
encryption key, copyrighted documentation, artwork, logo, font, or
other third-party asset.

The emulator requires users to provide any necessary BIOS or firmware
images separately. Users are solely responsible for obtaining,
possessing, and using such materials lawfully and in accordance with
applicable law and any licence terms imposed by their respective rights
holders.

No licence, ownership interest, endorsement, waiver, authorization, or
other right from any third-party rights holder is granted, expressed,
or implied by this project.

The foregoing notice concerns third-party intellectual property only.
Rights in the independently developed emulator source code are governed
separately by the project's LICENSE file and applicable source-file
copyright notices.

## Required files (all seven)

The machine loader (`src/machine/hbf1xv_machine.cpp`, `load_rom_assets()`) expects exactly
these filenames and sizes; `tools/gates/validate-assets.ps1` gates on all seven being present:

| File           | Size    | Role (slot mapping per the machine composition)        |
| -------------- | ------- | ------------------------------------------------------ |
| `f1xvbios.rom` | 32 KB   | Main BIOS + MSX-BASIC (slot 0-0, pages 0-1)            |
| `f1xvext.rom`  | 16 KB   | SUB ROM, MSX2+ extended BIOS (slot 3-1, page 0)        |
| `f1xvkdr.rom`  | 32 KB   | Kanji driver / KANJI BASIC (slot 3-1, pages 1-2)       |
| `f1xvdisk.rom` | 16 KB   | Disk ROM, WD2793 FDC interface (slot 3-2, page 1)      |
| `f1xvmus.rom`  | 16 KB   | FM-MUSIC / MSX-MUSIC BIOS, "APRLOPLL" (slot 3-3, page 1) |
| `f1xvkfn.rom`  | 256 KB  | Kanji font ROM (I/O ports #D8-#DB)                     |
| `f1xvfirm.rom` | 1 MB    | Halnote / MSX-JE firmware (slot 0-3)                   |

- `Primary reference URL: https://download.file-hunter.com/`

## Behavior notes

- A missing or unreadable file is **0xFF-filled** by `RomAssetLoader` and recorded in
  `Hbf1xvMachine::rom_diagnostics()` — the machine still constructs, but boot fails
  visibly. Tests asserting real ROM content must call
  `machine.set_asset_root(SONY_MSX_BIOS_DIR)` before `cold_boot()` (DEC-0016), or they
  silently run against 0xFF fill under ctest.
- Checksums are captured reproducibly via
  `./tools/gates/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`.
- Keep files legally sourced; treat as local development assets only; reference explicit
  file paths in tests/scripts — never invent missing assets.
