# M15 QA Sign-off — Device Integration Completion & S1985 Chipset Full Wiring

- Milestone: M15 (REQ-M15-004), scope LOCKED by DEC-0009 (authorized by DEC-0008).
- QA Owner: MSX QA Agent.
- Date: 2026-07-06.
- Gate: M15 retains the NORMAL human-release-decision gate. A QA Pass lets the coordinator
  PRESENT M15 to the human; it does NOT auto-close the milestone.
- Verdict: **PASS** (no regression or weakening found; one non-blocking ledger drift for the
  coordinator to correct at closure).

All results below were produced by QA independently (re-built, re-ran ctest, re-ran the openMSX
A/B harness, re-ran the boot checkpoint, and read the source) — developer numbers were not trusted.

---

## 1. Regression Scope

Affected surface for M15 (DEC-0009 IN set): B1 PSG/YM2149 (numeric), B2 RTC/RP5C01 (in-memory
epoch), C6 full i8255 PPI + keyboard + joystick, C4 backup-RAM `.sram` persistence, C5 boot-advance
checkpoint. New/changed source verified:

- `src/devices/audio/psg_ym2149.{h,cpp}` (new)
- `src/peripherals/{joystick,keyboard_matrix}.{h,cpp}` (new)
- `src/devices/rtc/rp5c01.{h,cpp}` (new)
- `src/devices/chipset/{system_control,ppi_8255}.{h,cpp}` (new; `#F5` gate + full PPI)
- `src/devices/chipset/s1985_engine.{h,cpp}` (additive `.sram` load/save)
- `src/machine/hbf1xv_machine.{h,cpp}` (wiring X2/X3, cold_boot ordering, backup-RAM path)

Regression-critical protected areas: X4 (CPU T-state math / M9-M12 timing oracles), X1 (`#A8`
slot-select), M11/M13 slot tests, M13 boot golden, M14 VDP.

## 2. Regression Matrix Status

| Area | Coverage | QA result |
|------|----------|-----------|
| Build (headless, SDL3 OFF) | `cmake --build build --config Debug` | PASS — clean (only pre-existing C4819 codepage warnings) |
| Full deterministic suite | `ctest --test-dir build -C Debug --output-on-failure` | **PASS — 100% tests passed, 0 failed out of 64** (QA-executed) |
| PSG YM2149 (B1) | `devices_audio_psg_ym2149_unit_test` (#57) | PASS + source-verified genuine |
| Joystick (C6) | `peripherals_joystick_unit_test` (#58) | PASS + source-verified |
| Keyboard matrix (C6) | `peripherals_keyboard_matrix_unit_test` (#59) | PASS + source-verified |
| RTC RP5C01 (B2) | `devices_rtc_rp5c01_unit_test` (#60) | PASS + source-verified |
| Full i8255 PPI (C6) | `devices_chipset_ppi_8255_unit_test` (#61, reuses M11 `#A8` asserts) | PASS + source-verified |
| Backup-RAM `.sram` (C4) | `devices_chipset_backup_ram_sram_unit_test` (#62) | PASS + source-verified |
| M15 device integration | `machine_hbf1xv_m15_devices_integration_test` (#63) | PASS |
| Boot checkpoint (C5) | `machine_hbf1xv_m15_boot_checkpoint_integration_test` (#64) | PASS + reproduced |
| X4 timing oracles (M9/M12) | cpu_step / cb / ldir / indexed / IM2 / IM0 ack | PASS — expected values UNCHANGED (see §3) |
| X1 `#A8` slot-select (M11/M13) | `chipset_ppi_slot_select_unit_test` + reused guard | PASS — byte-for-byte preserved |
| openMSX A/B parity | `tools/openmsx-io-parity.ps1` vs real `Sony_HB-F1XV` | PASS — empty diff 15/15, QA-REPRODUCED (§5) |

The 8 new tests (#57-#64) are additive; the 56 prior M0-M14 tests are unchanged and green (zero
regression). QA counted 64 in the ctest run itself, not from the report.

### Per-device genuine (non-stub) verification + two hand-checks

**PSG YM2149 (`psg_ym2149.cpp`) — GENUINE, NUMERIC-ONLY.**
- 16-register file R0-R15; address latch masked `& 0x0F` (mirrored registers).
- 5-bit / 32-step envelope: `step` initialized `0x1F`, `set_shape`/`do_step` faithful to the
  attack/hold/alternate/holding state machine; `volume() = step ^ attack`.
- R7 MSX masking on write: `value = (value & ~0x40) | 0x80` — bit6 forced 0 (port A input),
  bit7 forced 1 (port B output), mask-preserved.
- YM2149 read-as-written: `read_register` returns the raw store (no AY unused-bit masking).
- Stereo recorded numerically: `sample()` returns `left = A+B`, `right = A+C` (A=both, B=Left,
  C=Right) — integer only.
- **NUMERIC ONLY CONFIRMED:** no `#include` of any audio device/SDL/output/DAC; `sample()` returns
  a plain `{int32,int32}` struct; generators advance via `advance_cycles()` which is not called
  from the CPU step path. No audio presentation is pulled in.
- **HAND-CHECK #1 (PSG R7 mask, fact-sheet §2 "Critical R7 note"):** writing 0x00 to R7 yields
  `(0x00 & ~0x40) | 0x80 = 0x80`. Matches the fact-sheet and the A/B captured A=0x80. VERIFIED.

**Joystick (`joystick.cpp`) — GENUINE.** Read via PSG R14 (`read_port_a`/`encode`, active-low
directions bit0-3, triggers bit4-5, 0=pressed), port select via PSG R15 (`write_port_b` bit6),
KANA bit7. Injected into the PSG via `attach_port_source(&joystick_)` — hangs off the PSG, NOT the
S1985 engine (X5 confirmed in `wire_bus`; `S1985Engine` is not referenced by the joystick).

**RTC RP5C01 (`rp5c01.cpp`) — GENUINE, DETERMINISTIC.**
- `#B4` latch (`& 0x0F`, read 0xFF); `#B5` data (read `peek_port | 0xF0`).
- `#F5` gate: `io_read`/`io_write` early-out when `clock_gate_->clock_ic_enabled()` is false
  (bit7 of `SystemControlF5`).
- 4 blocks × 13 regs + mode(13)/test(14)/reset(15); per-block BCD write/read masks; Block-2 reg0
  stamped `kValidCmosMarker`; regs 14/15 return `0x0F` (unreadable).
- **IN-MEMORY DETERMINISTIC EPOCH CONFIRMED:** `seed_epoch()` sets a fixed 1988-01-01 00:00:00;
  `sync_time()` derives elapsed ticks ONLY from `clock_source_->cpu_cycles()` (scheduler). No
  `<chrono>`/host-clock call and no file read drives RTC time. Determinism preserved.
- **HAND-CHECK #2 (Block-2 reg0 CMOS-valid, fact-sheet §5):** `kValidCmosMarker = 0x0A`; `reset()`
  writes it to Block-2 reg0; Block-2 mask is `0xF`, so `peek` returns `0x0A & 0x0F = 0x0A`. The BIOS
  therefore takes the valid-CMOS path. VERIFIED.

**i8255 PPI (`ppi_8255.cpp`) — GENUINE.** `#A8` port A delegates to a composed `PpiSlotSelect`
(slot select); `#A9` returns the selected keyboard row (input-gated); `#AA` port C
(row-select/motor/CAPS/click latch); `#AB` control (mode set + bit-set/reset). Keyboard matrix is
11×8 inverted.
- **HAND-CHECK #3 (keyboard inversion, fact-sheet §3/§10):** `KeyboardMatrix::reset()` fills rows
  with `0xFF` (idle); `set_key(pressed)` clears the column bit (`&= ~mask`) → 0 = pressed. VERIFIED.

**Backup-RAM `.sram` (C4, `s1985_engine.cpp`) — GENUINE, DETERMINISTIC.** `load_backup_ram` returns
false and leaves the store untouched on absent/unreadable/short file, so after `reset()` the state
is deterministic zero — the M11 golden (absent → zero) is intact. `save_backup_ram` writes the
16 bytes verbatim.

## 3. Failures and Risk Ranking

No failures. No test was weakened, rewritten, or deleted; all M15 coverage is additive.

### X4 (CRITICAL) — CPU timing not perturbed: CONFIRMED CLEAN
- `Hbf1xvMachine::step_cpu_instruction` still computes `tstates = cpu_.step() (datasheet) +
  s1985_engine_.m1_wait_tstates(...)` — unchanged formula; no PSG/RTC advance in the CPU path.
- PSG generators advance READ-ONLY via `advance_cycles()` (off the machine clock, not called during
  the CPU step); RTC advances lazily in `sync_time()` off `scheduler total_cycles()`. Neither feeds
  back into T-state accounting. `increment_refresh_register()` and all datasheet T-state constants
  live in `src/devices/cpu/*`, which M15 did not touch.
- Timing oracles re-verified GREEN with UNCHANGED expected values: cpu_step **22**
  (`hbf1xv_cpu_step_integration_test:70`), cb **102**, ldir **102**, indexed **105**, IM2 ack **49**,
  IM0 ack **38**. All present in the 64/64 run.

### X1 (CRITICAL) — `#A8` slot-select preserved: CONFIRMED CLEAN
- `Ppi8255` composes `PpiSlotSelect port_a_` and, on `port & 0x03 == 0`, delegates read/write to it
  verbatim. `PpiSlotSelect::io_write` still latches `port_a_` and calls
  `slot_bus_.set_primary_select(value)`; `io_read` returns the latch. Byte-for-byte identical to
  M11. `Ppi8255` port A behavior == the old `PpiSlotSelect`. The M11 `#A8` unit test is unchanged
  and reused as a locked guard inside `devices_chipset_ppi_8255_unit_test`.

## 4. Required Fixes

None blocking. One process-integrity follow-up (below) for the coordinator to correct at closure.

### Ledger / process-integrity findings
- Requests REQ-M15-001..004 and responses RESP-M15-002/002b/003 are present; DEC-0008 and DEC-0009
  are recorded; `deferred-backlog.md` was updated same-cycle (B1/B2/C4/C6 → DONE (M15); C5 →
  IN-PROGRESS); milestone numbering is consistent (M15 = device integration, M16 = FDC next).
  `current-phase.md` accurately reflects "M15 Regression/QA … ctest 64/64".
- **DRIFT (non-blocking):** `agent-protocol/state/milestones.md` M15 still reads
  `Status: … implementation HELD pending human go-ahead` and `Details: … Implementation not
  started`, and `definition-of-done.yaml` M15 `notes` still says "PLANNING ONLY this cycle". These contradict the completed implementation (RESP-M15-003,
  `docs/m15-implementation-report.md`, ctest 64/64) and `current-phase.md`. The coordinator should
  refresh the M15 `milestones.md` Status/Details and the DoD `notes` to "implementation complete →
  regression_qa" when recording this sign-off. This is a bookkeeping lag, not a code or evidence
  gap.

## 5. openMSX A/B Adversarial Validation

QA independently re-ran `tools/openmsx-io-parity.ps1 -ProgramBin tests/parity/m15_io_probe.bin
-MaxSteps 15` against genuine **openMSX 19.1** with the installed `Sony_HB-F1XV` machine
(`/usr/share/openmsx/machines/Sony_HB-F1XV.xml`, real S1985 + YM2149 + RP5C01).

- **Result: ARCHITECTURAL PARITY — empty diff, 15/15 instructions captured on both sides.**
- **Substantive (non-trivial) values present on BOTH sides:**
  - seq 3, after `IN A,(#A2)` reading PSG R0 → `AF=3400` (A=0x34) — emulator and openMSX.
  - seq 14, after `IN A,(#A2)` reading PSG R7 with the MSX port-direction mask → `AF=8000`
    (A=0x80) — emulator and openMSX. This confirms the R7 MSX masking on the real HB-F1XV.
- **Comparator adversarially validated:** corrupting the openMSX `af=8000` field to `af=8001` made
  `tools/trace-diff.py` report DIVERGENCE (exit 1); an empty B side reported BLOCKED (exit 2); the
  genuine capture reports PARITY (exit 0). The empty diff is therefore a real match, not two blank
  or mis-compared traces.
- RTC data-nibble and joystick/keyboard idle reads are CMOS/config-dependent and are (correctly)
  NOT on the A/B gate; they are covered by the deterministic unit + integration tests. Documented in
  `docs/m15-parity-trace-diff.md` — no fabricated parity.

## 6. Boot Checkpoint (C5) Genuineness

QA ran `machine_hbf1xv_m15_boot_checkpoint_integration_test` directly:
`M15 boot checkpoint: final PC=0x454 max PC=0x488 over 4096 instructions`.
- GENUINE: the test boots the real BIOS (`rom_diagnostics().empty()` asserted — real ROMs present),
  single-steps real fetched opcodes, and asserts `max_pc > 0x043C` (the M13 boundary). Max PC 0x488
  strictly exceeds 0x043C — boot advanced past the M13 first-device-read boundary.
- DETERMINISTIC: two cold_boot+run cycles produce a byte-identical (PC,opcode) stream and identical
  final PC.
- SELF-DERIVED GOLDEN: the test does not hardcode 0x454/0x488; it derives and emits them and asserts
  only determinism + the `> 0x043C` inequality. Not a fabricated constant.
- Residual (accepted, DEC-0009 Q1): full boot to a BASIC/Disk prompt needs the FDC (M16). M15
  delivers only the checkpoint advance.

## 7. Boundary + Backlog Check

- No DEFERRED item was implemented. Verified absent: FDC drive mechanics (B8), FM-PAC/OPLL internals
  (B3), MSX-JE SRAM (B4), Kanji-font I/O `#D8-DB` (B5), Halnote firmware (B6), cartridge loading
  (B7), VDP rendering depth (D1-D7). The DISK/FM-MUSIC ROMs remain PRESENCE-only (M13), unchanged.
- `deferred-backlog.md` correctly records **B1, B2, C4, C6 → DONE (M15)** and **C5 → IN-PROGRESS
  (M15 partial)** with the reached checkpoint; all other rows re-affirmed OPEN with named owners;
  footer updated same-cycle. Consistent with DEC-0009 and the M16-FDC-next sequence.

## 8. Assumption Disposition

- **`#F5` gate reset = 0x80 (CLOCK-IC enabled).** ACCEPTED. openMSX 19.1 does not model an `#F5`
  RTC gate (its `MSXRTC` answers `#B4/#B5` unconditionally), so enabling the gate at reset preserves
  A/B boot parity while honoring fact-sheet §5; Sony's non-inverted system-flag polarity (§9)
  supports active-high. Deterministic and gate-tested either way. Verification action retained: a
  real HB-F1XV `#F5` power-on read; adjust if hardware differs. Non-blocking for M15.
- **RTC fixed synthetic 1988-01-01 epoch.** ACCEPTED per DEC-0009 Q2 — determinism is the M15
  requirement, and the fixed epoch guarantees byte-identical runs. Real-time/CMOS file persistence
  is explicitly a later persistence milestone. Correct for M15.

## 9. Residual Risk Disposition

- PSG audio presentation (analog/DAC/SDL sink) — DEFERRED to the frontend milestone (DEC-0009 Q4);
  M15 numeric model is complete and unit- + A/B-tested. Non-blocking.
- PSG exact audio-sample cadence A/B — out of scope (audio deferred); register/envelope state is the
  load-bearing behavior and is verified. Non-blocking.
- Cross-emulator cycle/T-state parity — ungated (backlog C1/D4). Non-blocking.
- C5 full boot to a prompt — owned by M16 (FDC). Non-blocking, tracked.
- Ledger drift in `milestones.md`/DoD `notes` (§4) — bookkeeping only; coordinator to refresh at
  closure. Non-blocking.

## 10. Sign-off Decision

**PASS.**

QA-executed evidence: build clean; **ctest 64/64 (0 failed)**; all 8 new devices verified GENUINE
(non-stub) and correct in source with three fact-sheet hand-checks passing; X4 (CPU T-state math +
all six timing oracles unchanged/green) and X1 (`#A8` byte-for-byte preserved) regression guards
confirmed clean; openMSX A/B empty diff INDEPENDENTLY REPRODUCED over substantive PSG R0=0x34 /
R7=0x80 values with the comparator adversarially validated (divergence + blocked modes); boot
checkpoint reproduced (final PC 0x454, max PC 0x488 > 0x043C, deterministic, self-derived); no
deferred item implemented; backlog/requests/responses/decisions consistent apart from one
non-blocking bookkeeping lag.

No blocker-level gaps remain. Per the milestone rule, this PASS authorizes the coordinator to
PRESENT M15 for the human release decision (normal gate); it does not auto-close M15. Recommended
follow-up at closure: refresh the `milestones.md` M15 Status/Details and DoD M15 `notes` to reflect
completed implementation + QA sign-off.
