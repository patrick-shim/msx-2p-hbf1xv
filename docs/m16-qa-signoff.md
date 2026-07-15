# M16 QA Sign-off — FDC Drive Mechanics (Fujitsu MB89311 / WD2793, Sony HB-F1XV)

- Milestone: M16 (REQ-M16-003).
- QA Owner: MSX QA Agent.
- Date: 2026-07-07.
- Gate: M16 retains the NORMAL human-release-decision gate. A QA Pass lets the coordinator
  PRESENT M16 to the human for the release decision + tag; it does NOT auto-close the milestone.
- Verdict: **PASS** (B8 fully and correctly delivered with strong evidence; the C5 residual is
  honestly reported, planner-anticipated, and non-blocking for THIS milestone's closure — see
  §6 for the full reasoning).

All results below were produced by QA independently (fresh reconfigure, rebuilt, re-ran ctest, read
every changed source file, verified the two openMSX-grounded bug-fix citations against the actual
`references/openmsx-21.0/src/fdc/WD2793.cc` / `PhilipsFDC.cc` line ranges, and inspected the raw
captured A/B trace files under `build/`) — developer and coordinator numbers were not trusted at
face value.

---

## 1. Regression Scope

Affected surface for M16 (closes backlog B8; advances C5): a wholly new `src/devices/fdc/`
subsystem (WD2793 core, disk drive, disk image, Sony connection-style decode, FDC clock source)
plus additive wiring in `src/machine/hbf1xv_machine.{h,cpp}` that replaces the M13 bare DISK-ROM
attach at slot (3,2,1) with the new `SonyFdc` device.

New/changed source verified:
- `src/devices/fdc/wd2793.{h,cpp}` (new) — WD2793 core: 5 registers, Type I/II/III/IV, context-
  sensitive status, INTRQ/DRQ, HLD, deterministic timing.
- `src/devices/fdc/disk_drive.{h,cpp}` (new) — drive mechanism, motor/motor-off, index-pulse
  scheduling helper.
- `src/devices/fdc/disk_image.{h,cpp}` (new) — deterministic 737,280-byte 2DD image.
- `src/devices/fdc/sony_fdc.{h,cpp}` (new) — Sony connection-style register decode wrapping
  `disk_rom_`.
- `src/devices/fdc/fdc_clock_source.h` (new) — cycle-clock interface (mirrors `RtcClockSource`).
- `src/machine/hbf1xv_machine.{h,cpp}` (modified, additive) — FDC composition, slot re-attach,
  `cold_boot()` FDC reset/remount, `debug_sub_slot_register` diagnostic accessor.
- `CMakeLists.txt`, `tests/CMakeLists.txt` (modified, additive) — new source/test targets.
- Test/tooling additions: `tests/unit/devices/fdc/*`, `tests/integration/machine/hbf1xv_m16_*`,
  `tools/gen-m16-boot-disk.py`, `tools/gen-m16-fdc-probe.py`, `tools/openmsx-m16-boot-parity.ps1`,
  `tests/parity/m16_boot.dsk`, `tests/parity/m16_fdc_probe.bin`.

Regression-critical protected areas checked: X4 (CPU T-state math / M9-M12 timing oracles), X1
(`#A8` slot-select, unaffected — no PPI changes this cycle), M11/M13 slot tests, M13/M15 boot
goldens, M14 VDP (untouched — no VDP files in this cycle's diff), M15 device suite (PSG/RTC/PPI/
backup-RAM, untouched).

## 2. Regression Matrix Status

| Area | Coverage | QA result |
|------|----------|-----------|
| Build (headless, SDL3 OFF) | fresh `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` | **PASS — 0 errors** (only pre-existing C4819 codepage warnings) |
| Asset validation | `tools/validate-assets.ps1` | **PASS — True**, all 7 BIOS files present incl. `f1xvdisk.rom` |
| Checksum refresh | `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | **PASS — ran clean**, only the timestamp line changed; BIOS/ROM hashes unchanged |
| Full deterministic suite | `ctest --test-dir build -C Debug --output-on-failure` | **PASS — 100% tests passed, 0 failed out of 72** (QA-executed, fresh tree) |
| Disk image geometry (S1) | `devices_fdc_disk_image_unit_test` | PASS |
| WD2793 Type I (S2) | `devices_fdc_wd2793_type1_unit_test` | PASS |
| WD2793 Type II (S3) | `devices_fdc_wd2793_type2_unit_test` | PASS |
| WD2793 Type III (S3, test-fixture bug fixed this cycle) | `devices_fdc_wd2793_type3_unit_test` | PASS + source-verified fix is genuine (§3.2) |
| WD2793 Type IV (S3, **new this cycle**) | `devices_fdc_wd2793_type4_unit_test` | PASS + source-verified grounding (§3.1) |
| Sony connection-style decode (S4) | `devices_fdc_sony_fdc_unit_test` | PASS + source-verified against `PhilipsFDC.cc` line-by-line (§3.3) |
| Machine wiring + CPU-driven FDC probe (S5) | `machine_hbf1xv_m16_fdc_integration_test` | PASS |
| Boot-checkpoint advance (S6, **new this cycle**) | `machine_hbf1xv_m16_boot_checkpoint_integration_test` | PASS + independently re-run (§5) |
| X4 timing oracles (M9/M12) | cpu_step / m11_parity / m13_mem_parity / m15_boot_checkpoint | PASS — expected values UNCHANGED (§4) |
| M13/M15 boot goldens | `machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`, `machine_hbf1xv_m15_boot_checkpoint_integration_test` | PASS — byte-identical to their prior recorded values (PC 0x454/max 0x488 reproduced verbatim) |
| openMSX A/B — Subject 1 (real boot, FDC live) | `tools/openmsx-m16-boot-parity.ps1` vs genuine openMSX 19.1 `Sony_HB-F1XV` | **PASS — empty diff, 3000/3000 instructions, QA-inspected raw traces** (§5) |
| openMSX A/B — Subject 2 (FDC register/command probe, identical disk image) | same harness | **PASS (functional)** — empty diff instructions 0-37 (register/command sequence incl. the HLD fix), documented cycle-cadence divergence in the DRQ-wait loop (pre-declared risk R-M16-2, not gated), identical terminal register file at HALT, 512-byte buffer byte-identical to the mounted image (QA-verified directly, §5) |
| Adversarial comparator self-check | empty-B / corrupted-field re-run | **PASS** — QA re-inspected `build/m16_adversarial_empty.md` (BLOCKED) and `build/m16_adversarial_corrupt.md` (DIVERGENCE at the exact corrupted instruction 10) |

The 8 new/updated M16 tests are additive; the 64 prior M0-M15 tests are unchanged and green (zero
regression). QA counted 72 in its own fresh `ctest` run, not from the developer's report.

## 3. Source-Level Verification (genuine, not stub)

**WD2793 core (`wd2793.{h,cpp}`) — GENUINE.** Five registers (TR=0xFF at reset); Type I
Restore/Seek/Step(-In/-Out) with step-rate/settle timing in absolute emulated cycles; Type II
Read/Write Sector with DRQ cadence (114 cycles/byte) and Lost-Data semantics that differ read vs
write (byte lost vs 0x00-substituted-and-continues), matching
`references/openmsx-21.0/src/fdc/WD2793.cc:261-267,751-757`; Type III Read Address/Read
Track/Write Track with the CRC-16-CCITT and 0xF5/0xF6/0xF7 escape-byte handling; Type IV Force
Interrupt (i3 immediate, i2 index-pulse-scheduled, flags=0 non-forcing). Context-sensitive status
via `is_type1_status()` — QA compared this bit-for-bit against openMSX's `getStatusReg`
(`WD2793.cc:161`: `((commandReg & 0x80) == 0) || ((commandReg & 0xF0) == 0xD0)`) — **identical
condition**.

**Sony connection-style decode (`sony_fdc.cpp`) — GENUINE, verified line-by-line against
`references/openmsx-21.0/src/fdc/PhilipsFDC.cc:1-175`.** QA read the actual openMSX source (not
just the planner's transcription) and confirms: `0x3FF8-0x3FFB` register passthrough; `0x3FFC` side
register (peek, bit0 -> `drive_.set_side`); `0x3FFD` drive-select (`00`/`10` -> A, `01` -> B, `11`
-> NONE) + motor (bit7) + DSKCHG at bit2 (active-low, 0=changed); `0x3FFE` reads `0xFF`; `0x3FFF`
active-low INTRQ(bit6)/DTRQ(bit7) starting from `0xFF`; default branch falls through to the wrapped
ROM. This is an exact match to `PhilipsFDC.cc`'s `readMem`/`writeMem`/`peekMem`, not the
fact-sheet's inferred (partly wrong) table — the load-bearing correction the planner flagged
(§3.2) is faithfully implemented.

**Disk image / drive (`disk_image.cpp`, `disk_drive.{h,cpp}`) — GENUINE, DETERMINISTIC.** No
`<chrono>`, no host-clock call, no `std::filesystem` read in the emulation path (`grep` over
`src/devices/fdc/` returns zero hits for wall-clock/filesystem APIs). All Busy/DRQ/step/settle/
index/motor timing is expressed as absolute emulated-cycle deadlines derived from
`FdcClockSource::cpu_cycles()`, which in the machine is wired to `scheduler_.total_cycles()`
(`FdcClock::cpu_cycles()`, additive, mirrors the M15 `RtcClock` pattern exactly).

## 4. X4 (CRITICAL) — CPU Timing Not Perturbed: CONFIRMED CLEAN

- `git diff --stat` over `src/`, `tests/CMakeLists.txt`, `CMakeLists.txt` shows changes confined to
  `src/devices/fdc/` (new), `src/machine/hbf1xv_machine.{h,cpp}` (additive only — the FDC
  composition, slot re-attach at `(3,2,1)`, `cold_boot()` FDC reset lines, and the new
  `debug_sub_slot_register` accessor), and the two CMake files. **`src/devices/cpu/*` is absent
  from the diff entirely** — `step_cpu_instruction`'s T-state formula and
  `increment_refresh_register()` were not touched, confirmed by direct inspection (not just the
  developer's claim).
- Re-ran the M9/M12 timing-oracle-bearing tests directly: `machine_hbf1xv_cpu_step_integration_test`
  passes; `machine_hbf1xv_m15_boot_checkpoint_integration_test` reproduces **byte-identical**
  "final PC=0x454 max PC=0x488 over 4096 instructions" verbatim against the M15 QA sign-off record;
  `machine_hbf1xv_m11_parity_checkpoint_integration_test` and
  `machine_hbf1xv_m13_mem_parity_checkpoint_integration_test` both pass (silent = deterministic
  match against their pinned goldens).
- All new FDC timing (Busy/DRQ/step/settle/index/motor/HLD-idle) is fed exclusively by
  `FdcClockSource::cpu_cycles()` (a thin read-only wrapper over `scheduler_.total_cycles()`), never
  the CPU T-state accumulator and never wall-clock. Confirmed by source read, not assumption.

## 5. openMSX A/B Adversarial Validation — QA-Reproduced Directly From Raw Traces

QA did not just read `docs/m16-parity-trace-diff.md`; it opened the raw captured trace files under
`build/` and independently checked the substantive claims:

- **Subject 1 (real BIOS boot, FDC live, 3000 instructions).** `build/m16_boot_A.txt` (this
  emulator) and `build/m16_boot_B.txt` (openMSX 19.1) both terminate at the identical final
  instruction: `SEQ=2999/seq=2999 PC=0455 OP=2F [CPL] AF=FF20 SP=0000` on both sides, matching
  `build/m16_boot_diff.md`'s "ARCHITECTURAL PARITY — EMPTY DIFF" claim. Non-trivial, non-zero
  register content on both sides (AF/BC/DE/HL diverge meaningfully from reset default over the
  window and still match) — this is a genuine parity result, not two blank/degenerate traces.
- **Subject 2 (FDC register/command probe, identical `tests/parity/m16_boot.dsk` mounted both
  sides).** `build/m16_fdc_probe_diff.md` literally reports "ARCHITECTURAL DIVERGENCE" starting at
  instruction 38 (the DRQ-wait polling loop) with a length mismatch (A=6253, B=8290) — the
  implementation report's characterization of this as a documented/expected cycle-cadence
  difference (not an empty-diff claim) is **honest and matches the literal comparator output**, not
  spun. QA directly inspected the tails of `build/m16_fdc_probe_A.txt` and
  `build/m16_fdc_probe_B.txt`: both terminate at `PC=C041 OP=76 [HALT] AF=0044 BC=0000 DE=0000
  HL=C400 SP=FFFF IM=1 IFF1=IFF2=0` — **identical terminal architectural state** despite the
  differing iteration counts, confirming functional (not merely claimed) parity.
- **Buffer content match.** QA extracted `build/m16_fdc_probe_B_buf.hex` (512 bytes, the openMSX
  RAM buffer at `0xC200`) and diffed it byte-for-byte in Python against the first 512 bytes of
  `tests/parity/m16_boot.dsk`: **exact match** (`data == buf` → `True`). This independently confirms
  the "actual 512 transferred bytes match byte-for-byte" claim rather than accepting it on faith.
- **Adversarial comparator self-check, QA-reproduced.** `build/m16_adversarial_empty.md` shows
  `Result: BLOCKED -- a trace side is empty` (openMSX side genuinely 0 rows);
  `build/m16_adversarial_corrupt.md` shows `ARCHITECTURAL DIVERGENCE` with the very first mismatch
  row reading `| 10 | C019 | af | 0054 | DEAD |` — exactly the hand-corrupted instruction 10 value,
  confirming the comparator genuinely detects a single-field corruption rather than silently
  passing. The empty-diff / functional-match claims above are therefore trustworthy.
- **Grounding citations verified against the actual reference file, not the planner's transcription
  alone.** QA read `references/openmsx-21.0/src/fdc/WD2793.cc:420-433` (`startType1Cmd` — H_FLAG
  gates `hldTime`), `:522-533` (`startType2Cmd` — `hldTime = time` unconditional on acceptance),
  `:820-834` (`startType3Cmd` — same pattern), `:1062-1073` (`endCmd` — HLD idle-timeout restart),
  and `:159-197` (`getStatusReg` — the exact `(hldTime <= time) && (time < hldTime + IDLE)` window,
  `IDLE = EmuDuration::sec(3)` at line 42) and confirms every cited line range says precisely what
  the implementation report and source comments claim. Same for Type IV i2
  (`WD2793.cc:1035-1060`, `irqTime = drive.getTimeTillIndexPulse(time)` at :1049-1050) and the
  Write-Track escape-byte handling (`WD2793.cc:960-999`, unconditional 0xF5/0xF6/0xF7 special
  handling). **No fabricated grounding found.**

## 6. Boot-Checkpoint (C5) Genuineness and Acceptability — Core Judgment Call

QA ran `machine_hbf1xv_m16_boot_checkpoint_integration_test.exe` directly (not via ctest's summary
line alone) and observed:

```
M16 boot checkpoint: final PC=0x7d60 max PC=0x7d6f over 400000 instructions (M15 max PC was 0x488);
FDC read_sector_commands_accepted=0 (residual: disk-ROM auto-boot handshake not reached in this
budget, see docs/m16-implementation-report.md / docs/m16-parity-trace-diff.md)
```

This is **exactly** what the implementation report and parity trace-diff claim: max PC 0x7D6F (far
past the M15 checkpoint 0x488), and the FDC diagnostic counters (`read_sector_commands_accepted`,
`read_sector_bytes_transferred`, `read_sector_completions_ok`) all genuinely zero — the CPU never
pages slot (3,2) into page 1 during this unattended run. The test itself asserts this residual as a
named, honest expectation (`BootCheckpoint_FdcReadSectorNeverAccepted_HonestResidual`, etc.) rather
than omitting or fabricating a passing assertion — QA regards this as a model of transparent
reporting, not evasion.

**Is the residual acceptable to defer for THIS milestone's closure?** QA's judgment: **yes**, for
the following reasons, weighed independently (not simply accepting the developer's or coordinator's
framing):

1. **The planner pre-authorized exactly this contingency, in writing, before implementation
   began.** `docs/m16-planner-package.md` §6.3 states verbatim: "If, empirically, the Sony disk ROM
   stops earlier ... the checkpoint is set at the furthest deterministic, A/B-matched point — still
   strictly past 0x454 — and any residual is documented (not fabricated)." That is precisely what
   happened: max PC 0x7D6F is far past 0x454/0x488, it is deterministic (two-run byte-identical),
   and it is A/B-matched over the portion that could be compared (Subject 1, 3000 instructions,
   empty diff). This is not a post-hoc excuse invented after the fact to explain away a miss; it is
   the exact fallback the plan itself specified as an acceptable outcome.
2. **M16's own stated closure criterion is B8, not C5.** Both the planner package (§9) and the
   deferred-backlog ledger state M16 **closes B8** ("FDC drive mechanics") and merely **advances**
   C5 ("full boot past first device read"), explicitly re-affirming C5 as "IN-PROGRESS unless/until
   a full prompt is reached and A/B-confirmed." The developer and the backlog entry both correctly
   leave C5 IN-PROGRESS, not DONE — no false claim of closure is made anywhere in the artifacts.
   B8's actual scope (WD2793 core, Sony decode, deterministic image, motor-off, DSKCHG,
   unit/integration tests, real A/B evidence) is fully, correctly, and verifiably delivered (§2-§5
   above). M15 received a PASS under the identical structural situation (C5 IN-PROGRESS, not
   blocking) for the same reason: C5 is tracked, honestly scoped, and not the milestone's own
   closure gate.
3. **The FDC device itself — the actual subject of B8 — is independently and positively verified
   correct**, via two independent lines of evidence that do not depend on the real, uncertain
   BIOS auto-boot trigger: (a) S1-S6 deterministic unit/integration tests that explicitly drive the
   controller through a CPU program navigating to slot (3,2), and (b) a genuine openMSX A/B
   comparison using the identical disk image, matching functionally (terminal register file +
   byte-identical 512-byte transfer) after a real, cited, grounded bug fix (HLD) was found and
   corrected. The residual is specifically in **automatically reaching** FDC engagement via an
   unattended, keyboard-less cold boot — not in the FDC's own modeled behavior.
4. **The residual is well-bounded and actionable, not an open-ended mystery.** The gap is
   attributed to a specific, named cause (the real MSX auto-disk-boot trigger requires a BASIC
   cold-start decision path this investigation could not pin down without a disk-ROM/SUB-ROM
   disassembly — explicitly out of M16's scope), with a concrete verification action recorded in
   `agent-protocol/state/deferred-backlog.md` C5's row for a future milestone.
5. **No fabrication anywhere in the chain.** QA's independent re-run of the boot-checkpoint test,
   the raw A/B traces, and the source-level grounding all corroborate the developer's own account
   verbatim — nothing was found to be overstated, and the one place a claim could have been
   overstated (Subject 2's "empty diff") is instead reported with the literal, harsher "ARCHITECTURAL
   DIVERGENCE" comparator output, with the actual functional-parity claim scoped honestly beneath it.

**Conclusion on this point**: the C5 residual is a genuine, disclosed, planner-anticipated gap in a
*different*, explicitly-not-being-closed backlog item, not an unmet acceptance criterion of the
milestone M16 is actually gated on (B8). QA does not treat it as blocking or as requiring a
Conditional Pass; it is carried forward as an explicit condition below (§8) rather than a defect.

## 7. Failures and Risk Ranking

No failures found. No test was weakened, deleted, or rewritten to accommodate incorrect behavior.

### Test-fixture fix (`wd2793_type3_unit_test.cpp` Write-Track pattern) — VERIFIED LEGITIMATE, not a masked bug
QA independently confirmed against `references/openmsx-21.0/src/fdc/WD2793.cc:960-999`
(`writeTrackData`) that openMSX treats `0xF5`/`0xF6`/`0xF7` as reserved Write-Track escape codes
**unconditionally**, wherever they occur in the streamed byte sequence — there is no
"literal-data-if-not-preceded-by-sync" exception in real WD2793/openMSX behavior. The old test
pattern (`(j*3+7)&0xFF`) could and did emit these values as ordinary sector-data bytes, which is
invalid input for a real Write-Track stream (real formatting software never embeds them as literal
data). The fix (`(j*3+7) % 0xF5`) constrains the generated pattern to `[0x00,0xF4]`, and the test's
own byte-for-byte content assertions (`WriteTrack_FormattedSector_ContentMatchesStream`,
`WriteTrack_ThenReadSector_MatchesFormattedPattern`) were **not weakened or loosened** — they still
require exact byte equality against the (now-valid) pattern. This is a genuine fixture-input
correction, not a production-code workaround or a lowered bar.

### HEAD_LOADED (HLD) fix — VERIFIED GENUINE, correctly grounded
QA read `WD2793.cc:420-433` (Type I, H-flag-gated), `:522-533` and `:820-834` (Type II/III,
unconditional activation "see comment in startType1Cmd"), `:159-197` (`getStatusReg`'s
`hldTime <= time && time < hldTime + IDLE` window, `IDLE = 3s`), and `:1062-1073` (`endCmd`'s
idle-timeout restart) — all match the implementation's `wd2793.cpp` `start_type1`/`start_type2`/
`start_type3`/`peek_status`/`end_command` logic line-for-line in behavior (not copied code — the
implementation is an independent re-expression, satisfying GPL isolation). No pre-existing test
asserted the old (incorrect, motor-tied) HLD behavior — the fix is additive/corrective, confirmed
by `ctest` staying green (72/72) both structurally (no removed assertions found in the diff) and by
direct reading of `wd2793_type1_unit_test.cpp`/`sony_fdc_unit_test.cpp` (neither test asserts on
HEAD_LOADED at all, so neither could have been "weakened" to hide the old bug).

### Type IV i2 (index-pulse-scheduled INTRQ) fix — VERIFIED GENUINE, correctly grounded
QA read `WD2793.cc:1035-1060` (`startType4Cmd`, `irqTime = drive.getTimeTillIndexPulse(time)` at
`:1049-1050`) and confirms the fix (`index_irq_armed_`/`index_irq_deadline_`, resolved in `sync()`)
matches openMSX's scheduled (not immediate) INTRQ-on-i2 semantics. The new
`wd2793_type4_unit_test.cpp` is the **first** test to ever exercise this path (it did not exist
before this cycle, so it could not have "weakened" a prior assertion) — genuinely new coverage of a
previously-untested command variant, not a retroactive fix-then-relax cycle.

### Risk ranking (residual, non-blocking)
- **R-M16-3 residual (C5 auto-boot trigger not reached)** — MEDIUM priority, non-blocking per §6.
  Tracked in `agent-protocol/state/deferred-backlog.md` C5 row with a concrete next-step
  (disk-ROM/SUB-ROM disassembly, or a simulated-keypress/DiskBASIC-auto-run investigation).
- **DRQ-cadence cycle-count divergence vs openMSX (Subject 2, R-M16-2 anticipated risk)** — LOW
  priority, explicitly out of the architectural gate since M10 (T-state/cycle fields reported, not
  gated); functional correctness independently confirmed instead (§5). Non-blocking.
- **Write Track / flux-level fidelity (backlog C10, ex-planner-"B10")** — LOW priority, explicitly
  deferred and re-affirmed OPEN; standard 9-sector reformat verified round-trip; arbitrary flux/DMK
  fidelity was never in M16's Stage 1-2 scope. Non-blocking.
- **Second/phantom drive, real-`.dsk` host-path mounting, SDL3/CLI disk-insert UX** — explicitly out
  of scope per the planner §1.2; untouched, no regression risk introduced.

## 8. Required Fixes

None blocking. No code changes required before the human release decision. One recommended
follow-up condition, non-blocking, for the coordinator/next-milestone owner to track (not a defect
in this milestone's own deliverable):

- **Condition C-M16-1**: before any future milestone claims C5 fully closed ("full boot to a
  BASIC/DiskBASIC/DOS prompt"), it must specifically re-attempt the boot-checkpoint acceptance
  signal against the now-identified real trigger condition (disk-ROM/SUB-ROM disassembly or a
  simulated keypress / auto-run flag), per the verification action already recorded in
  `agent-protocol/state/deferred-backlog.md` C5's row. This is carried forward, not newly invented
  by QA.
- Ledger check: `agent-protocol/state/deferred-backlog.md` (B8 → DONE (M16), C5 advanced/IN-PROGRESS
  with an honest residual, C10 re-affirmed OPEN) and `agent-protocol/state/definition-of-done.yaml`
  M16 block (`implementation.*` true, `regression_qa.signoff_decision_recorded`/`overall_done` left
  false pending this sign-off) are both internally consistent with the artifacts QA reviewed. No
  drift found this cycle (unlike M15, which had a bookkeeping lag QA had to flag).

## 9. Boundary + Backlog Check

- No DEFERRED item was implemented. Verified absent/untouched this cycle: FM-PAC/OPLL internals
  (B3), MSX-JE SRAM (B4), Kanji-font I/O (B5), Halnote firmware (B6), cartridge loading (B7), VDP
  rendering depth (D1-D7), printer/cassette (C7), Sony speed/pause (C8), SDL3 frontend (C9), exact
  cycle timing (C1/C2), ZEXALL (C3).
- `deferred-backlog.md` correctly records **B8 → DONE (M16)** and **C5 advanced, IN-PROGRESS** with
  the reached checkpoint and an honest residual; the new C10 row (flux/DMK fidelity) is present and
  re-affirmed OPEN (already existed from the prior session, not duplicated). All other rows
  re-affirmed OPEN with named owners; footer updated same-cycle. Consistent with the planner
  package and the developer's own bookkeeping.

## 10. Sign-off Decision

**PASS.**

QA-executed evidence: fresh build clean (0 errors); asset validation True; checksum gate refreshed;
**ctest 72/72 (0 failed)**, independently re-run from a clean tree; all new FDC source
(`wd2793.{h,cpp}`, `sony_fdc.{h,cpp}`, `disk_drive.{h,cpp}`, `disk_image.{h,cpp}`,
`fdc_clock_source.h`) read and verified GENUINE (non-stub) and grounded line-for-line against the
actual `references/openmsx-21.0/src/fdc/WD2793.cc`/`PhilipsFDC.cc` (not just the planner's or
developer's transcription); X4 (CPU T-state math untouched, all timing-oracle-bearing tests
reproduce their pinned prior values byte-for-byte) confirmed clean by direct diff inspection and
re-run; openMSX A/B evidence independently re-inspected from the raw trace files under `build/`
(Subject 1 empty-diff terminal match; Subject 2 functional terminal-state + byte-identical
512-byte buffer match, with the comparator's literal "DIVERGENCE" wording over the DRQ-cadence
portion honestly preserved rather than mischaracterized as an empty diff) and the adversarial
BLOCKED/DIVERGENCE self-checks reproduced; the two mid-cycle bug fixes (HLD, Type-IV i2) are real,
correctly grounded, and did not weaken any existing test assertion (verified by reading every
affected test file, not just trusting the report); the Write-Track test-fixture fix is a legitimate
input-data correction, not a masked production defect.

The C5 residual (real, unattended auto-disk-boot handshake not reached) is genuinely investigated,
honestly reported, matches the planner's own pre-declared fallback contingency (§6.3), does not
block M16's actual closure criterion (B8, which is fully and independently verified correct), and
is already tracked with a concrete next step in the backlog. No blocker-level gaps remain in this
milestone's own deliverable.

Per the milestone rule, this PASS authorizes the coordinator to PRESENT M16 for the human release
decision (normal gate); it does not auto-close M16. Recommended condition at closure (non-blocking,
carried forward, not new): C-M16-1 above — any future "C5 fully closed" claim must specifically
re-attempt the boot-checkpoint acceptance signal against the now better-understood real auto-boot
trigger condition.
