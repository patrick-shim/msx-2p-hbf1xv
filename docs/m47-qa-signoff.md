# M47 — QA Sign-off: DEF-M47-DISKWRITE (WD2793 disk-WRITE corruption)

- **Milestone ID:** M47 / DEF-M47-DISKWRITE (DEC-0072)
- **Commit under review:** `a27be57` (main, HEAD; working tree clean)
- **Investigation reference:** `docs/m47-fdc-write-investigation.md`
- **QA date:** 2026-07-13 (independent re-verification on the developer's existing `build/`, SDL3=ON, VS multi-config; incremental build only — no clean/from-scratch rebuild)
- **Sign-off Decision:** **CONDITIONAL PASS** (automated acceptance fully met; sole open condition is the owner-gated live YS II re-save-and-load, AC-10-equivalent)

---

## 1. Regression Scope

Fix reviewed: WD2793 Write Sector byte-position decoupled from CPU-write timing
(0x00-substitute-**never-drop**) + H4 CHS latch.

- **Root cause (confirmed):** `Wd2793::write_data` committed a Write Sector byte ONLY when
  `transfer_drq(t)` was true and **silently dropped** it otherwise (data register updated but
  `buffer_`/`data_index_`/`data_available_` not advanced). Under any non-strictly-polled cadence
  (early write / 2-bytes-per-DRQ burst / fixed cadence ahead of the emulator's rotational
  first-DRQ) the drop shifted the fully-committed sector → the sporadic YS II save corruption
  (3 coherent-shifted side-1 sectors).
- **Fix:** `write_data` → `commit_write_sector_byte` latches + commits every CPU byte in-order
  ALWAYS (never dropped); un-serviced-slot `0x00 + LostData` substitution moved to `sync()` and
  fires only on a **mid-transfer full-revolution abandonment** (`data_index_ > 0`), gated behind the
  preserved M45 first-byte CHECK_WRITE abort (`data_index_ == 0`). H4: `(track, side)` latched at
  `begin_write_sector` / `start_type3`, committed via `DiskDrive::write_sector_at` to the latched
  CHS so a mid-transfer `0x7FFC` side write can't redirect the sector.
- **Files touched:** `src/devices/fdc/wd2793.{h,cpp}`, `src/devices/fdc/disk_drive.{h,cpp}`,
  `src/machine/hbf1xv_machine.{h,cpp}`. (Non-perturbing Write Sector trace observer +
  `stream_<id>_fdcwrite.log`.)
- **Blast radius assessed:** FDC Write Sector / Write Track paths; disk save persistence; the
  CHS-latch drive path; machine stream-capture wiring. Read path, boot path, CPU, core scheduler
  explicitly out of change scope (verified below).

## 2. Regression Matrix Status

**Test Matrix Reference:** the two milestone gate suites + the new anti-regression unit test +
targeted adversarial mutation + read-path/CPU byte-identity diffs.

| Check | Command / method | Result |
| --- | --- | --- |
| Gate suite 1 (fdc/disk/machine/boot) | `ctest -R "wd2793|fdc|disk|m4[0-9]|machine_hbf1xv|boot" -E zexall` | **71/71 PASS** |
| Gate suite 2 (frontend/machine/cartridge/slot/mapper/ram/fmpac/sram/fdc/disk/boot/cli/input_script) | `ctest -R "frontend|machine|hbf1xv_m|cartridge|slot|mapper|_ram|fmpac|sram|fdc|disk|boot|cli|input_script|m4[0-9]" -E zexall` | **144/144 PASS** |
| New unit test present + registered | `tests/CMakeLists.txt:513-517`, exe built | present, `add_test` registered |
| `devices_fdc_wd2793_write_earlybyte_unit_test` (Cases A–H) | direct run | **all cases PASS** |
| M45 late/stall/drift byte-perfect | `wd2793_write_stall` | PASS (byte_exact, NoLostData under 12k-cycle mid-stall + steady drift) |
| M45b first-byte gate (slow-present writes / absent-first aborts) | `wd2793_write_firstbyte` | PASS |
| Type II/III/I/IV + fast-disk | `wd2793_type2/3/1/4`, `wd2793_fastdisk` | PASS |
| Disk boot + save-persist + multi-disk | `hbf1xv_m28_c5_disk_boot_investigation_system_test`, `machine_hbf1xv_m36_disk_save_persist_integration_test`, `frontend_sdl3_app_multi_disk_integration_test`, `machine_hbf1xv_boot_logo_integration_test` | PASS (within gates) |
| Read path byte-identity | function-body diff `v1.1.2..HEAD` of `read_data` / `begin_read_sector` / `finish_read_sector` | **IDENTICAL** (37 / 36 / 19 lines each) |
| AC-22: `git diff v1.1.2..HEAD -- src/core src/devices/cpu` | git | **EMPTY** |
| Final consistent green state (post-mutation restore) | both gate suites re-run | 71/71 + 144/144 PASS |

### Non-tautology (the crux) — verified by adversarial mutation (DEC-0049 non-destructive: cp-backup → mutate → build → run → restore → `git diff` byte-identical)

- **Else-drop revert** (`if (phase_ == WriteSector && transfer_drq(t))` restored): the new test
  fails **exactly** as predicted — **11 case failures** (Burst ×2-modes, EarlyInject ×2, Multi
  Sec7/Sec8 ×2-modes) + **AllChsSweep: 1440 sector(s) wrong**. Cases A/B/C/D genuinely exercise the
  bug; the fix is load-bearing, not tautological.
- **H4 revert** (`drive_->write_sector(...)` live-CHS restored): **Case E fails** in both modes
  (`H4_CommittedToLatchedSide0`, `H4_Side1_NotRedirected`). The CHS latch is load-bearing.
- Both reverts restored byte-identical (`git diff` empty); tree confirmed clean before re-running.

### All-CHS byte-perfect proof (Case D) — genuine, not tautological

Case D writes a **guaranteed-non-zero, `(lba, offset)`-keyed unique** payload to every
`(track 0..79, side 0..1, sector 1..9)` = **1440 sectors** under the adversarial early/burst cadence
on one shared image, then reads every sector back and asserts byte-equality (a wrong/substituted/
leaked byte surfaces as a mismatch and cannot alias another sector). Passes with the fix; fails on
all 1440 under the else-drop revert. This is the primary offline acceptance for "byte-perfect across
all CHS."

### Deviation from the investigation's literal pseudocode — assessed SOUND and hardware-faithful

The investigation's literal step 2 was "zero-substitute + advance on **every** late DRQ slot in
`sync()`." The developer instead used **commit-on-CPU-write (never drop)** + **0x00-substitute only
on a full-revolution mid-transfer abandonment**. Assessment: this is the correct model for a
CPU-gated controller and is grounded in all three references:

- **openMSX** `references/openmsx-21.0/src/fdc/WD2793.cc:746` lays a byte and advances
  `dataCurrent++` **every** byte-period, using the CPU's `dataReg` if `dataRegWritten` else
  `dataOutReg = 0; statusReg |= LOST_DATA` (:751-757) — **never a drop**; first-byte absence
  terminates (:678-679).
- **fMSX** `references/fmsx-60/source/EMULib/WD1793.c:351` `*D->Ptr++=V;` stores **every** CPU write
  unconditionally and advances — no timing gate at all; abandonment handled by a timeout watchdog
  (:134-139), conceptually identical to the developer's full-revolution grace.
- **Fact sheet** `references/fact-sheets/FDC for Sony HB-F1XV.md` §8 line 168 / line 261: "missed DRQ
  mid-write → Lost Data and a 0x00 byte substituted (command continues)"; "a missed DRQ *before* the
  first data byte terminates the command. **Model both paths.**"

The two behavior references differ in *mechanism* (openMSX disk-cadence-driven; fMSX
CPU-write-driven) but **agree on the load-bearing invariant: a Write Sector byte is never dropped**.
The developer's hybrid honors that invariant and splits abort (first byte absent) vs
0x00-substitute (mid-transfer) exactly as the fact sheet's "model both paths" directs. Because the
emulator's rotational DRQ deadline is an approximation, per-slot substitution on the emulator's
clock is precisely what produced the M45/YS II false-drop; committing each CPU byte in-order
eliminates the root cause rather than masking it (proven by the non-tautology). The full-revolution
grace (`kWriteFirstByteWindowCycles = kSystemClockHz/5` ≈ 715,909 cycles = one 300 RPM revolution)
is a defensible "genuinely abandoned" threshold and is directly corroborated by fMSX's watchdog.

## 3. Failures and Risk Ranking

**No test failures.** No blocker- or high-severity defects found. Residual risks:

- **[LOW — coverage gap]** The Write Track (Type III) path received the same never-drop decoupling
  (`write_data` WriteTrack branch always parses + advances), but the new suite adds **no**
  adversarial early/burst test for Write Track — only `wd2793_type3` (normal cadence) covers it.
  Mitigation: game saves use Write Sector (DSKIO), not Write Track; formatting (the only Write Track
  user) is not exercised during play, and `type3` proves normal-cadence identity. Not a blocker.
- **[LOW — accepted fidelity trade-off]** The decoupled model will not reproduce a genuine
  real-hardware **mid-write LOST_DATA corruption for a merely-slow (sub-full-revolution) CPU** — it
  commits the late byte in position instead. This is consistent with the standing M45 decision and
  the read-path fast-disk CPU-gated model, and is directly corroborated by fMSX (commit-every-write
  + watchdog-only timeout). No known MSX software depends on observing mid-write LOST_DATA. Accepted.
- **[LOW — pre-existing, out of M47 scope]** An abandoned **Write Track** (CPU stops mid-format) has
  no `sync()` completion/timeout path (the abandonment loop and first-byte gate are
  `phase_ == WriteSector`-gated), so it could hold BUSY. This predates M47 (the old Write Track path
  also only completed on CPU writes) and does not occur with real FORMAT. Noted, not attributed to
  M47.
- **[Owner-gated — not a defect]** The AC-10-equivalent live acceptance (YS II 274-read boot
  byte-identity + in-game re-save-and-load in both `--fast-disk` modes, byte-traced via the new
  `stream_<id>_fdcwrite.log`) is the human's final gate. **Assumption:** the developer verified this
  locally per the investigation's stated acceptance gates; **no captured live `*_fdcwrite.log` trace
  is present in-repo** (`debug/` search empty). **Verification action:** the human runs the live YS
  II re-save/reload before any release tag.

**Risk Ranking:** Low (no Critical/High/Medium items).

## 4. Required Fixes

None blocking. Recommended (non-blocking) follow-ups:

1. Add a Write Track early/burst adversarial-cadence unit case (parity with Cases A/D for Write
   Sector) to close the LOW coverage gap.
2. Capture and archive the live YS II re-save `stream_<id>_fdcwrite.log` + byte-diff-vs-pristine as
   the AC-10 evidence artifact when the owner runs the live gate.

## 5. Sign-off Decision

**CONDITIONAL PASS.**

- Automated acceptance is fully met: both gates green (71/71, 144/144); the new anti-regression test
  passes and is proven **non-tautological** (else-drop revert → 11 fails + 1440-sector AllChsSweep
  failure; H4 revert → Case E fails); the all-CHS byte-perfect proof is genuine; M45/M45b are a
  strict superset (not weakened); the read path and CPU/core are byte-identical (AC-22 empty); the
  deviation from the literal pseudocode is sound and triangulated across openMSX + fMSX + fact sheet.
- **Sole condition:** the owner-gated live YS II 274-read boot byte-identity + in-game
  re-save-and-load (both `--fast-disk` modes). Per the task framing this is graded Conditional
  (owner-gated), **not Fail**. Once the human confirms the live re-save reloads byte-identical, M47
  is a clean Pass.

**Sign-off Conditions:** human executes and confirms the live YS II re-save/reload gate before the
release tag; (recommended) archive the live `_fdcwrite.log` evidence and add the Write Track
adversarial-cadence case.
