# bios/ — Sony HB-F1XV system ROMs

Local, legally-sourced Sony HB-F1XV BIOS assets used for development and validation.
**Third-party IP (Sony); no redistribution rights asserted.** Per **DEC-0047** the repository owner
has made an informed decision to host this repository publicly with these BIOS files included — an
owner accepted-risk hosting decision, not a licensing grant. The files remain Sony's property.

## Required files (all seven)

The machine loader (`src/machine/hbf1xv_machine.cpp`, `load_rom_assets()`) expects exactly
these filenames and sizes; `tools/validate-assets.ps1` gates on all seven being present:

| File           | Size    | Role (slot mapping per the machine composition)        |
| -------------- | ------- | ------------------------------------------------------ |
| `f1xvbios.rom` | 32 KB   | Main BIOS + MSX-BASIC (slot 0-0, pages 0-1)            |
| `f1xvext.rom`  | 16 KB   | SUB ROM, MSX2+ extended BIOS (slot 3-1, page 0)        |
| `f1xvkdr.rom`  | 32 KB   | Kanji driver / KANJI BASIC (slot 3-1, pages 1-2)       |
| `f1xvdisk.rom` | 16 KB   | Disk ROM, WD2793 FDC interface (slot 3-2, page 1)      |
| `f1xvmus.rom`  | 16 KB   | FM-MUSIC / MSX-MUSIC BIOS, "APRLOPLL" (slot 3-3, page 1) |
| `f1xvkfn.rom`  | 256 KB  | Kanji font ROM (I/O ports #D8-#DB)                     |
| `f1xvfirm.rom` | 1 MB    | Halnote / MSX-JE firmware (slot 0-3)                   |

## Behavior notes

- A missing or unreadable file is **0xFF-filled** by `RomAssetLoader` and recorded in
  `Hbf1xvMachine::rom_diagnostics()` — the machine still constructs, but boot fails
  visibly. Tests asserting real ROM content must call
  `machine.set_asset_root(SONY_MSX_BIOS_DIR)` before `cold_boot()` (DEC-0016), or they
  silently run against 0xFF fill under ctest.
- Checksums are captured reproducibly via
  `./tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`.
- Keep files legally sourced; treat as local development assets only; reference explicit
  file paths in tests/scripts — never invent missing assets.
