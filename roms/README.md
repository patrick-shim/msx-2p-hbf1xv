# ROMs Directory for FMPAC ROM and SRAM FILE (NOT TO BE ALTERED)

FM-PAC ROM assets used for emulator bring-up and testing.

Usage notes:

- Keep ROM files legally sourced.
- Treat these files as third-party development assets.
- Do not assume redistribution rights.
- Test plans should reference exact ROM paths used for reproducibility.
- Per **DEC-0047** (+ **AMENDMENT-B**, 2026-07-14) the repo is hosted on a PUBLIC remote by the
  owner's decision. The game cartridge ROMs were moved to `games/` (untracked, local-only); this
  directory now holds ONLY the FM-PAC peripheral firmware `fmpac.rom` + its battery-save
  `fmpac.rom.sram`, which are **tracked and published** here — the same owner-accepted-risk
  category as `bios/` (proprietary peripheral firmware; no redistribution rights asserted). Any
  other `roms/` content stays untracked (provision locally). ROM binaries committed before
  `b5efd29` also remain in git history.
