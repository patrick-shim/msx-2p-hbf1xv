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
- Per **AMENDMENT-C** (2026-07-15, owner request) `fmpac.rom.sram` is **frozen** on the remote at
  its last-pushed state (`d0eb7f9`): it stays tracked/published there, but local save-state churn
  is no longer committed or pushed (enforced locally via
  `git update-index --skip-worktree roms/fmpac.rom.sram` — a per-clone flag; re-apply after a
  fresh clone). To deliberately publish a new save state: `--no-skip-worktree`, commit, push,
  re-apply the flag.
