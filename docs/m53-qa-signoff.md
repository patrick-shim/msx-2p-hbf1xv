# M53 QA Sign-off — Standalone MSX Disk Utility (`msx-disk`)

- Milestone ID: M53 (DEC-0080)
- Request: REQ-M53-003 · QA Owner: MSX QA Agent · Date: 2026-07-15
- Charter: `docs/m53-planner-package.md` · Developer report: `docs/m53-implementation-report.md`
- Build tree: the ONE canonical `build/` (DEC-0041), VS 2026 generator, `SONY_MSX_ENABLE_SDL3=ON`
- Tree state at QA: UNCOMMITTED. `git status` = ` M .gitignore`, ` M CMakeLists.txt`,
  `?? diskutils/`, `?? src/diskutils/` (unchanged before/after QA; no foreign writers observed).

**Sign-off Decision: PASS.** All AC-1..AC-7 independently reproduced; the full fast suite is
253/253 with the 247 pre-M53 tests name-for-name unchanged; build isolation is proven at the
CMake link edge and by an empty emulator-source diff. No blocker- or high-severity gaps. NORMAL
human-release gate (owner user-test recipe in §7) precedes any release.

---

## 1. Regression Scope

M53 adds a build-isolated, self-contained host-side executable `msx-disk` (`--create` / `--format`
/ `--read`) whose 720 KB MSX-DOS FAT12 blank image is byte-exact for the HB-F1XV
(WD2793 / Sony Disk ROM). Regression surface assessed:

- **Emulator suite integrity** — the pre-M53 247 fast-subset must be byte-identical (no
  removals/renames/re-outcomes) and the 6 new tool tests green.
- **Build isolation (one-way)** — the shipped `msx_disk` binary must link NO emulator library;
  `src/core|devices|machine|frontend` must show an EMPTY diff (AC-6 / DEC-0080 R4).
- **Byte-exactness + determinism** — `--create` SHA256 == the M41-A/B-validated
  `tools/format-blank-disk.ps1` blank (AC-2).
- **CLI safety contract** — exit-code matrix 0/1/2/3, overwrite/size guards (AC-5 / R3).
- **Machine parity** — the tool's boot/BPB/FAT-seed == `DiskImage::synthesize()`, and a
  tool image mounts + reads back through the real FDC datapath (AC-3/AC-4).
- **Structure/packaging** — POST_BUILD copy re-lands the binary; `.gitignore` covers it;
  `diskutils/.gitkeep` keeps the folder.

Not in scope (correctly): any emulator behavior. openMSX A/B and ZEXALL are N/A per DEC-0080 —
justified here by the independently-verified EMPTY diff over the entire emulator source
(`src/core|devices|machine|frontend`), which proves the Z80 core and every device are byte-unchanged
so an A/B or ZEXALL result cannot differ from the pre-M53 baseline.

## 2. Regression Matrix Status

| # | Item (task) | Method | Result |
| --- | --- | --- | --- |
| a1 | Clean build all targets | `cmake --build build --config Debug` | PASS — exit 0; `sony_msx_headless.exe` + `sony_msx_sdl3.exe` + `msx_diskutil.lib` + `msx-disk.exe` (907,776 B) all built |
| a2 | Full fast ctest (ZEXALL excluded) | `ctest -C Debug -E hbf1xv_m24_zexall_system_test` | PASS — **253/253 passed, 0 failed**, 45.97 s |
| a3 | Pre-M53 247 name-for-name unchanged | `ctest -N -C Debug` full name audit | PASS — 254 registered − 6 diskutil − 1 ZEXALL = **247** pre-M53; zero removals/renames; no duplicate names; M52 tests (`frontend_master_volume_*`, `frontend_disk_writable_toggle_flush_*`) intact |
| b1 | `msx_disk` links only `msx_diskutil` | `CMakeLists.txt:125-134` inspection + build log | PASS — `msx_diskutil` STATIC lib has NO `target_link_libraries` (include dirs only); `msx_disk` links `PRIVATE msx_diskutil` only; build log: `msx-disk.exe` from `msx_diskutil.lib` only |
| b2 | Emulator-src diff EMPTY | `git diff --stat -- src/core src/devices src/machine src/frontend` | PASS — empty (re-checked twice) |
| b3 | Tracked mods == `.gitignore` + `CMakeLists.txt` | `git status --porcelain` | PASS — exactly ` M .gitignore` + ` M CMakeLists.txt`; `src/diskutils/` + `diskutils/` untracked-new |
| c | AC-2 SHA256 byte-exactness | fresh `--create` + `format-blank-disk.ps1 -ExecutionPolicy Bypass` | PASS — both `6f1a79835e0421178b01207b196fa245c127c976fa0c6abc3aaa57e6b0ce5188`, 737,280 B; == golden constant |
| d | CLI exit-code + read matrix (8 cases) | live `msx-disk.exe` invocations | PASS — see §2.1 |
| e | Non-tautology review (6 tests) | source read | PASS — see §2.2 |
| f | POST_BUILD re-copy after delete | delete `diskutils/msx-disk.exe` → `--target msx_disk` | PASS — re-landed (907,776 B, fresh timestamp) |
| g | `.gitignore` covers binary; `.gitkeep` keeps folder | `git check-ignore -v` + `git add --dry-run` | PASS — binary ignored at `.gitignore:27` (exit 0); `git add --dry-run diskutils/` lists ONLY `.gitkeep` |
| h | ZEXALL | skipped per DEC-0080 | N/A — empty-emulator-diff proven (see §4) |

### 2.1 Live CLI matrix (task d) — all correct

| Invocation | Expected | Observed |
| --- | --- | --- |
| (no args) | exit 1 | exit 1 |
| `--create <existing>` (no `--force`) | exit 3 | exit 3 |
| `--create <existing> --force` | exit 0 | exit 0 |
| `--format <missing>` | exit 2 | exit 2 |
| `--format <wrong-size>` (no `--force`) | exit 3 | exit 3 |
| `--read <p> --sector 1 --range 0-10` | exit 1 (mutually exclusive) | exit 1 |
| `--read <p> --sector 1` | F9 FF FF at 0x200 | `00000200: f9 ff ff 00 ...` |
| `--read <p> --range 1fe-200` | 55 AA at 0x1FE | `000001fe: 55 aa    \|U.\|` |

### 2.2 Non-tautology review (task e)

- **`diskutils_bpb_matches_machine_unit_test`** — links both `msx_diskutil` + `sony_msx_core`.
  Compares real byte ranges `tool[i]==mach[i]` for `i∈[0..29]∪{510,511}` and FAT-seed offsets
  `{512..514, 2048..2050}` against `DiskImage::synthesize()` (lines 70-92): a wrong BPB byte would
  make it FAIL. Crucially, line 98 asserts `tool != mach` — proving the data areas genuinely
  DIVERGE (tool zero-filled vs `synthesize()`'s per-sector test pattern), so the comparison is NOT
  the trivial "images are identical." Grounded: the machine oracle is `src/devices/fdc/disk_image.cpp:28-88`;
  the tool layout matches `tools/format-blank-disk.ps1:82-133`. Non-tautological — CONFIRMED.
- **`machine_diskutil_fdc_mount_integration_test`** — LBA 0 is read back **through the real WD2793
  datapath**: `build_restore_read_sector_probe()` assembles a Z80 program that drives the FDC via
  I/O port writes (`0x7FF8`-`0x7FFD`: drive/side select, Restore Type-I, Read-Sector Type-II),
  runs the CPU to HALT, and reads the streamed 512 B from the CPU's own buffer at `0xC200`
  (lines 81-156) — NOT a direct memcmp of the attached image buffer. It then asserts the streamed
  bytes equal the mounted image AND the FAT12 markers (EB FE 90 / F9 / 55 AA). LBA 1 (FAT seed) and
  LBA 7 (empty root) use `read_chs` as the deterministic oracle, per planner §6.6 point 3 — the
  M16 probe pattern is cited (lines 28-29, `hbf1xv_m16_fdc_integration_test.cpp:94-167`).
  Reads-through-the-datapath — CONFIRMED.
- The other four (`blank_layout`, `create_format_roundtrip`, `hexdump`, `cli_validation`) are real
  byte/exit-code oracles: exact-value BPB field table, garbage→blank round-trip through the real
  `--format` path, an exact golden hex-dump string, and the full 0/1/2/3 exit matrix. No weakened
  or self-referential assertions found.

## 3. Failures and Risk Ranking

No failures. Residual risks (all LOW/INFO, none blocking):

- **R-A (LOW, by-design, documented) — non-bootable medium.** `--create`/`--format` produce a
  genuinely EMPTY, DOS-recognizable data/files disk (empty root ⇒ zero files; all-zero boot-code
  region), NOT a bootable OS disk. Writing `MSXDOS.SYS`/`COMMAND.COM` would be a redistribution
  violation and is out of scope (planner §1.2). Honest and correct — not a defect.
- **R-B (LOW) — AC-4 is FDC sector-mount + FAT12 readback, not a full Disk-BASIC boot.** The M28/R6
  finding is that a headless DOS boot is not reliably reached in-budget. The deterministic gate is
  the sector-level mount + structure readback (green). The live Disk-BASIC recognition is deferred
  to the owner user-test (§7).
- **R-C (INFO, repo-wide standing condition, NOT M53-specific) — test + CLAUDE.md evidence is
  git-LOCAL-ONLY.** Per DEC-0047 the entire `/tests/` tree (`.gitignore:219`) and `/src/CLAUDE.md`
  (`.gitignore:224`) are gitignored, so the 6 new test sources + `tests/CMakeLists.txt` registration
  and the `src/diskutils/` responsibility line will NOT travel with a commit. The committed artifact
  set (`git add -A --dry-run`) is exactly: `.gitignore`, `CMakeLists.txt`, `diskutils/.gitkeep`, and
  `src/diskutils/{cli,hex_dump,msx_disk_format}.{h,cpp}` + `main.cpp` (7 source files — trackable,
  `git check-ignore` exit 1). This matches every prior milestone's convention; flagged only so the
  coordinator/owner are aware the tests are local-only regression evidence.

## 4. Required Fixes

None. No blocker-level gaps. ZEXALL and openMSX A/B are legitimately waived (DEC-0080) and the
waiver is independently corroborated: the EMPTY diff over `src/core|devices|machine|frontend` proves
zero change to the Z80 core and all devices, so no CPU-exerciser or A/B-parity result can differ
from the established baseline. This satisfies the QA "behavior-affecting" review obligation — M53 is
provably non-behavior-affecting.

## 5. Sign-off Decision

**PASS.**

Sign-off conditions (all met at QA):
1. Build green (all 3 exes) from the ONE `build/` tree. ✅
2. 253/253 fast ctest; 247 pre-M53 name-for-name unchanged; 6 new tool tests green. ✅
3. Build isolation proven: shipped `msx_disk` links only `msx_diskutil`; emulator-src diff EMPTY;
   tracked mods == `.gitignore` + `CMakeLists.txt`. ✅
4. AC-2 SHA256 (`6f1a79…0ce5188`) reproduced independently from both the tool and the PS1 blank. ✅
5. CLI exit-code matrix + `--read` slice correctness verified live. ✅
6. Test oracles reviewed as non-tautological (BPB-vs-machine divergence assert; FDC read-through). ✅
7. POST_BUILD re-copy verified; binary gitignored; `.gitkeep` stage-able. ✅

Standing gate: **NORMAL human-release** (owner user-test, §7) precedes any release/close — no
auto-close (DEC-0080, LOW blast radius).

## 6. Regression Report (template fields)

- Milestone ID: M53 (DEC-0080)
- Regression Scope: standalone `msx-disk` tool + build isolation + emulator-suite integrity (§1)
- Test Matrix Reference: `docs/m53-planner-package.md` §6 (5 unit + 1 integration) + §2 above
- Executed Tests: full fast ctest 253/253 (ZEXALL #141 excluded per DEC-0080); 6 new diskutil
  tests #248-253 all Passed; live CLI matrix (8 cases); AC-2 SHA256 dual-source reproduction
- Failures: none
- Risk Ranking: Low (R-A/R-B by-design/deferred-to-owner) · Info (R-C repo-wide git-local tests)
- Blocking Issues: none
- Recommended Fixes: none
- Sign-off Decision: **Pass**
- Sign-off Conditions: owner user-test (§7) as the NORMAL human-release gate before release/close

## 7. Owner User-Test Recipe (NORMAL human-release gate)

Run from the repo root after `cmake --build build --config Debug` (the binary lands at
`diskutils/msx-disk.exe`). Use ONLY scratch copies — never an original game/OS disk.

1. **Create a fresh disk and confirm size/exit.**
   ```
   diskutils\msx-disk.exe --create scratch\new.dsk
   ```
   Expect exit 0 and a 737,280-byte file. (SHA256 must be
   `6f1a79835e0421178b01207b196fa245c127c976fa0c6abc3aaa57e6b0ce5188`.)

2. **Hex-read it and eyeball the boot sector / FAT.**
   ```
   diskutils\msx-disk.exe --read scratch\new.dsk --sector 0
   diskutils\msx-disk.exe --read scratch\new.dsk --sector 1
   ```
   Sector 0 line 0 begins `eb fe 90 53 4f 4e 59 4d  53 58 20 …` (JMP + `SONYMSX `), byte 0x15 = `f9`,
   offset 0x1FE = `55 aa`. Sector 1 begins `f9 ff ff` (FAT seed) then zeros.

3. **Format a SCRATCH COPY of a real game-disk BACKUP (never the original).**
   Copy a backup to a scratch path first, then:
   ```
   copy games\disks\<title>\d1.dsk scratch\copy.dsk
   diskutils\msx-disk.exe --format scratch\copy.dsk
   ```
   Expect exit 0 (the backup copy is 737,280 B, so no `--force` needed). Re-hex-read to confirm it
   is now the empty blank. This proves `--format` wipes a real-geometry disk to a clean MSX medium
   without touching the original. (A wrong-size file safely refuses with exit 3 unless `--force`.)

4. **Mount the created disk in the emulator and confirm Disk BASIC recognizes it.**
   Boot the emulator with the tool-created disk mounted, e.g.:
   ```
   build\Debug\sony_msx_sdl3.exe --disk scratch\new.dsk
   ```
   At the MSX prompt, `CALL SYSTEM` / Disk BASIC `FILES` should show an empty directory (zero files)
   — i.e. the medium mounts and is directory-readable. This is the live confirmation of the AC-4
   "mount + directory-readable" gate that the headless FDC test proves deterministically; it is NOT
   expected to boot an OS (the disk is intentionally non-bootable, R-A).

---
QA sign-off path: `docs/m53-qa-signoff.md`.
