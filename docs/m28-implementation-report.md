# M28 Implementation Report — Release Candidate: Backlog Closure Sweep + Full-Project Health Audit

- Milestone: M28
- Request: REQ-M28-002 (`agent-protocol/channels/requests.md:1445-1458`), implementing
  `docs/m28-planner-package.md` §3.1 slices S1-S4.
- Developer: MSX Developer Agent
- Date: 2026-07-08

---

## 1. Milestone Target

Exactly three work items, per the developer handoff hard constraints (`docs/m28-planner-package.md`
§6):

1. **E2** — YM2413 register-write timing constraint (12/84-master-cycle minimum spacing).
2. **C5** — full-boot / automatic disk-ROM boot-handshake trigger investigation (dual-outcome
   honest acceptance).
3. **Release-candidate health audit** — `docs/m28-release-candidate-audit.md` (four parts), run
   LAST.

Plus **S4**: `agent-protocol/state/deferred-backlog.md` ledger closure (E2→DONE; C5 per the honest
outcome reached; C1/D4's UNBUILDABLE-WITHOUT-FABRICATION reasoning recorded verbatim; G1/E1/C10/
F1/F2 → DEFERRED with named owners and verbatim scoping caveats; G2/G3/G4 re-affirmed unchanged).

**Explicitly NOT touched** (hard constraint, verified below): C1/D4's remaining VDP-timing depth,
and E1/C10/F1/F2/G1's production code — all DEFERRED this cycle per the planner package, zero
production code written for any of them.

---

## 2. Code Changes

### 2.1 E2 — YM2413 register-write timing constraint

**Files changed:**
- `src/devices/audio/ym2413_opll.h` — added `Ym2413ClockSource` (X-pattern of
  `rtc::RtcClockSource`/`fdc::FdcClockSource`/`peripherals::CassetteClockSource`/
  `peripherals::RenshaTurboClockSource`, mirrors `src/devices/rtc/rp5c01.h:14-18` exactly);
  `kAddressWriteMinCycles = 12`, `kDataWriteMinCycles = 84`; `attach_clock_source()`,
  `set_write_timing_enforced()`, `write_timing_enforced()`; private `gate_allows_write()` +
  tracking state (`clock_source_`, `write_timing_enforced_`, `has_last_write_`,
  `last_write_was_address_`, `last_write_cycle_`).
- `src/devices/audio/ym2413_opll.cpp` — `gate_allows_write()` implementation (required spacing keyed
  on what the PREVIOUS write was, per the fact-sheet's exact wording: "after an address write...
  before an data write... after a data write... before the next write"); `write_address()`/
  `write_data()` now consult the gate before mutating `latch_`/`regs_`; `reset()` clears the
  write-timing TRACKER state only (`has_last_write_`/`last_write_was_address_`/`last_write_cycle_`),
  leaving `clock_source_`/`write_timing_enforced_` untouched (mirrors `Rp5c01::reset()` leaving
  `clock_source_`/`clock_gate_` untouched).
- `src/machine/hbf1xv_machine.h` — new nested `Ym2413Clock` class (X-pattern of `RtcClock`/
  `FdcClock`/`CassetteClock`/`RenshaTurboClock`); new `ym2413_clock_` member, declared immediately
  before `ym2413_`.
- `src/machine/hbf1xv_machine.cpp` — `ym2413_.attach_clock_source(&ym2413_clock_)` added to
  `wire_bus()`; `Ym2413Clock::cpu_cycles()` implementation (`return scheduler_.total_cycles();`).

**Grounding**: `references/fact-sheets/Yamaha YM2413 FM Chip.md` §8 ("Register write timing (real
hardware constraint): after an address write, wait ≥12 master cycles (~3.36 µs); after a data
write, wait ≥84 master cycles (~23.52 µs) before the next write... Violating the 84-cycle rule
causes dropped/wrong register writes on real hardware" / "openMSX (Nuked-OPLL core) currently has
the too-fast-access-timing emulation *disabled*"). Independently sourced from the Yamaha
Application Manual + andete's hardware measurements — no numeric-table risk (only two scalar
constants), consistent with `docs/m28-planner-package.md` §2.1b.

**Mandatory regression pre-check (R-M28-1), performed BEFORE writing any gate-enabling code:**

```
$ rg "0x7C|0x7D|#7C|#7D" tests/unit/devices/audio_ym2413_opll_unit_test.cpp
$ rg "0x7C|0x7D|#7C|#7D" tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp
```

Found:
- The unit test file (`audio_ym2413_opll_unit_test.cpp`) issues back-to-back `write_address()`/
  `write_data()` calls with ZERO calls to `attach_clock_source()` anywhere — it never attaches a
  clock source, so `gate_allows_write()`'s `clock_source_ == nullptr` short-circuit makes the gate a
  permanent no-op there, REGARDLESS of the enforced-flag's default. Zero risk from this file either
  way.
- The integration test's `FmMusicRomGuard` case (`Case 2`) drives writes via
  `Hbf1xvMachine::debug_io_write()` — confirmed by reading `hbf1xv_machine.cpp:511-513`
  (`void Hbf1xvMachine::debug_io_write(...) { bus_.io_write(port, value); }`) to be a raw,
  zero-cycle-advance pass-through (no CPU stepping occurs between calls, so `elapsed_cycles()` does
  not change between consecutive `debug_io_write()` invocations). A gate defaulting ON would see
  0-cycle spacing between every consecutive write in that loop and DROP nearly all of them —
  silently changing this existing test's observable behaviour (though the specific assertions in
  that case do not check register VALUES, only ROM-content-unchanged, so it would not have failed
  outright — but it would be a silent, undisclosed behaviour change, exactly the class of regression
  risk R-M28-1 exists to catch).

**Resolution chosen: (b)** — the gate **defaults OFF** (`write_timing_enforced_ = false` at
construction), matching openMSX's own documented default-disabled stance and the fact-sheet's own
Recommendation 3 ("Model timing constraints as an option... flag-gated, since openMSX disables
this"). Every existing M17 test file (`audio_ym2413_opll_unit_test.cpp`,
`hbf1xv_m17_ym2413_integration_test.cpp`) is left **byte-for-byte unmodified** — confirmed via
`git diff v1.0.27 -- tests/unit/devices/audio_ym2413_opll_unit_test.cpp
tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp` (empty diff).

### 2.2 C5 — full-boot / auto-disk-boot-trigger investigation

**Files added:**
- `tests/system/hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp` — cold-boots with a real
  `disks/msxdos22.dsk` mounted, with/without a scripted `SPACE` keypress
  (`sony_msx::machine::InputScriptPlayer`), tracking on every instruction boundary whether page 1's
  resolved sub-slot for primary 3 ever becomes `2` (the disk-ROM window,
  `Sony_HB-F1XV.xml:161-176`) over a 20,000,000-instruction budget.
- `tools/openmsx-m28-c5-boot-parity.ps1` — A/B harness (this emulator's `--debug-session --disk
  --trace-cpu` vs. a live-Tcl openMSX session), extending `tools/openmsx-m16-boot-parity.ps1`'s
  Subject 1 with a real, bootable disk mounted on both sides.
- `docs/m28-parity-trace-diff.md` — the full write-up (see §3 below for the summarized outcome).

**Zero production code changed for C5** — this was a pure investigation; no genuine defect was
found in `src/devices/fdc/*`/`src/machine/hbf1xv_machine.*` requiring a fix (per the planner's
explicit allowance: "any production change only if a genuine defect is found — pure observation
requires no production edit").

### 2.3 Health audit — small, safe, mechanically-verified fixes

- Removed `src/devices/device_placeholder.h`, `src/peripherals/peripheral_placeholder.h` (dead
  M0-era scaffolding; R-M28-7 pre-check confirmed zero CMake glob dependency and zero consumers
  anywhere; build reconfigures and compiles cleanly after removal).
- `README.md` — removed the two now-nonexistent lines from the (already stale, M0-era) project
  scaffold tree snippet.
- `agent-protocol/state/deferred-backlog.md` — fixed one stale path citation in the C7 row
  (`references/openmsx-21.0/src/CassettePort.hh/.cc` → `references/openmsx-21.0/src/cassette/
  CassettePort.hh/.cc`, the real vendored location).

No behavior-affecting change was made to any already-QA-signed device (verified: the only
behavior-affecting source diff this cycle is the E2 addition to `ym2413_opll.{h,cpp}`/
`hbf1xv_machine.{h,cpp}`, and that addition is a no-op by default, see §2.1).

### 2.4 Ledger closure (S4)

`agent-protocol/state/deferred-backlog.md`: E2 → `DONE (M28)`; C5 → `IN-PROGRESS (M28 partial)`
with the new finding appended; C1 and D4 rows both carry the UNBUILDABLE-WITHOUT-FABRICATION
reasoning **verbatim** from `docs/m28-planner-package.md` §2.2; E1/C10/F1/F2/G1 rows carry their
`docs/m28-planner-package.md` §2.3(a)-(e) scoping-caveat findings **verbatim**, with DEFERRED
owners M30/M31/M32/M33/M29 respectively; G2/G3/G4 rows left **byte-for-byte unchanged** (no edit
applied). A new "Last updated" trailer summary block added at the top of the reverse-chronological
history section. `agent-protocol/state/milestones.md`, `agent-protocol/state/definition-of-done.yaml`,
`agent-protocol/state/current-phase.md` all updated to reflect developer-implementation-complete
status (not QA sign-off, which this agent cannot self-grant).

---

## 3. C5 Outcome — (b), Investigation Advances, Does Not Close

Full write-up: `docs/m28-parity-trace-diff.md`. Summary:

- **New, stronger finding**: with a REAL, bootable MSX-DOS 2.2 system disk (`disks/msxdos22.dsk`)
  mounted — not merely the FDC's default synthesized (non-bootable) medium M16 used — and with or
  without a scripted `SPACE` key held across a wide early-boot window, the disk-ROM window (primary
  3, sub 2) is **NEVER paged into page 1** within 20,000,000 instructions
  (`hbf1xv_m28_c5_disk_boot_investigation_system_test`, `real 0m8.032s`). This is a stronger, earlier
  signal than M16's own residual (which only established no Read-Sector *command* was ever
  observed) — it establishes the disk-ROM's memory window is never even reachable in the first
  place, a necessary precondition M16 had not directly measured.
- **Real openMSX A/B evidence**: this emulator's boot trajectory shows genuine ARCHITECTURAL PARITY
  (empty diff, every architectural field) against real openMSX 19.1, with the SAME real disk
  mounted on both sides, over a 3000-instruction canonical window AND an extended
  100,000-instruction window (`tools/openmsx-m28-c5-boot-parity.ps1`,
  `docs/m28-parity-trace-diff.md` Results 1-2) — mounting a real disk does not perturb this
  emulator's trajectory relative to genuine openMSX; the disk-ROM-engagement gap is not attributable
  to a defect in this emulator's slot-decode/boot path.
- **Adversarial comparator self-check** (empty B → BLOCKED exit 2; corrupted field → DIVERGENCE
  exit 1) confirms the comparator is trustworthy, mirroring the M10-M16 precedent.
- **Cross-checked with `disks/msxdos23.dsk`** (different DOS version): identical `final_pc=0x2BF7`
  at 3,000,000 instructions as the `msxdos22.dsk` idle-boot run, consistent with the engagement
  decision (or lack thereof) being made at a BIOS level preceding any DOS-version-specific code.
- **`disks/msxdos24/`** is a raw directory tree, not a mountable `.dsk`/`.img` file — correctly NOT
  converted to an image ad hoc (that capability is C10/M31 territory); the two flat `.dsk` images
  were sufficient for a thorough, genuine investigation.

**Outcome (b), per the dual-outcome acceptance criterion**: C5 stays `IN-PROGRESS (M28 partial)` —
not force-closed. No "reached DOS prompt" claim is made (none was genuinely observed). A future
investigation would need independent grounding of the real BIOS/disk-ROM ROM-scan/cold-start
decision sequence (not present in `references/` today) to determine WHY the window is never mapped
in during an otherwise-faithful, openMSX-matching boot.

---

## 3. Unit Test Results

```
$ ctest --test-dir build -C Debug -R "devices_audio_ym2413_write_timing_unit_test" --output-on-failure
1/1 Test #142: devices_audio_ym2413_write_timing_unit_test ... Passed   0.02 sec
100% tests passed, 0 tests failed out of 1
```

`devices_audio_ym2413_write_timing_unit_test` (new, 10 cases): default-off confirmation;
gate-disabled zero-spacing writes still land; gate-enabled-no-clock-source passthrough; first-write-
always-accepted; address-write 12/11-cycle boundary; data-write 84/83-cycle boundary; dropped-write
does-not-reset-timing-reference; `reset()` clears tracker but not the enforced toggle; two-run
determinism (with a deliberate landed/dropped mix, positively confirmed non-degenerate).

Existing M17 unit test (`devices_audio_ym2413_opll_unit_test`, unmodified) re-run for regression:

```
$ ./build/tests/Debug/devices_audio_ym2413_opll_unit_test.exe
All Devices_AudioYm2413Opll_Unit cases passed
```

---

## 4. Integration Test Results

`machine_hbf1xv_m28_ym2413_write_timing_integration_test` (new, 4 cases): gate-off tight sequence
reproduces the exact M17 baseline (all writes land); gate-on tight vs. generously-NOP-padded
sequences (real CPU `OUT` execution) — measured `tight=2 landed=4` (of 4) vs. `padded=4/4`,
empirically confirming the gate drops real, CPU-driven, too-fast writes and accepts sufficiently
spaced ones; two-machine determinism under the gated program.

```
$ ./build/tests/Debug/machine_hbf1xv_m28_ym2413_write_timing_integration_test.exe
[evidence] gate-enabled landed counts: tight=2 padded=4 (of 4)
All Machine_Hbf1xvM28Ym2413WriteTiming_Integration cases passed
```

Existing M17 integration test (`machine_hbf1xv_m17_ym2413_integration_test`, unmodified) re-run for
regression:

```
$ ./build/tests/Debug/machine_hbf1xv_m17_ym2413_integration_test.exe
All Machine_Hbf1xvM17Ym2413_Integration cases passed
```

`hbf1xv_m28_c5_disk_boot_investigation_system_test` (new system test, see §3 above):

```
$ ctest --test-dir build -C Debug -R hbf1xv_m28_c5_disk_boot_investigation_system_test --output-on-failure
1/1 Test #144: hbf1xv_m28_c5_disk_boot_investigation_system_test ... Passed   8.00 sec
100% tests passed, 0 tests failed out of 1
```

### Full regression (evidence gates)

```
$ tools/validate-assets.ps1
Asset validation result: True
(7 BIOS files present; 2 ROM files present)

$ tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
Checksum report written to: docs/asset-checksums.txt

$ git diff v1.0.27 --name-only -- src/devices/ src/peripherals/ src/core/
src/devices/audio/ym2413_opll.cpp
src/devices/audio/ym2413_opll.h
src/devices/device_placeholder.h        (removed)
src/peripherals/peripheral_placeholder.h (removed)
```

This confirms the `feedback_slow_test_cadence.md` mechanical gate (A-M28-3): E2 touches the
EXISTING `src/devices/audio/ym2413_opll.{h,cpp}` — the full, unfiltered `ctest` was run at least
once this cycle, per criterion 8.

```
$ cmake --build build --config Debug          (headless, full rebuild)
(zero errors)

$ cmake -S . -B build/sdl3-on -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install
$ cmake --build build/sdl3-on --config Debug   (SDL3-ON, full rebuild, reusing the pre-existing
                                                 vendored SDL3 install per the request's instruction)
(zero errors)

$ ctest --test-dir build -C Debug --output-on-failure       (FULL, UNFILTERED — not -LE filtered)
144/144 tests passed, 0 tests failed
ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
hbf1xv_m24_zexall_system_test: Test Passed, Test time = 1498.17 sec
Total Test time (real) confirmed via build/Testing/Temporary/LastTest.log (background run,
completed after this report's initial draft — coordinator independently confirmed the raw log
directly, not merely trusting a self-report, per this project's standing verification discipline).
(Note: a SEPARATE, later coordinator-landed bug fix — DEC-0026, docs/vdp-vr-hr-boot-hang-fix-report.md,
outside this milestone's own E2/C5/audit scope — required one further full unfiltered ctest re-run,
independently confirmed green including the slow sweep; see that report for its own evidence.)

$ SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ctest --test-dir build/sdl3-on -C Debug -LE m24_slow_full_sweep --output-on-failure
100% tests passed, 0 tests failed out of 152
Total Test time (real) = 19.71 sec
```

(SDL3-ON's own copy of the slow ZEXALL/ZEXDOC sweep was intentionally not re-run a second time in
this same cycle — running it once, in the primary `build` directory, already satisfies criterion 8;
the SDL3-ON configuration's fast-subset re-confirms zero regression there, including all 3 new M28
tests, which are registered unconditionally outside the SDL3 guard.)

---

## 5. Known Issues

- **C5 stays `IN-PROGRESS (M28 partial)`** — the real auto-disk-boot trigger condition remains
  unexplained; a future investigation needs an independent MSX BIOS/disk-ROM technical reference
  (not disassembly of the GPL/proprietary ROM images themselves) to progress further.
- **`disks/msxdos24/`** could not be used directly (raw directory tree, not a mountable image) —
  this is a real, disclosed limitation, not a defect; building directory→image conversion is C10/M31
  territory.
- **E2's A/B disposition is honestly N/A** for the drop-behaviour comparison (openMSX disables this
  emulation by default) — this is not a gap, it is the expected, disclosed shape of this specific
  claim (mirrors the fact-sheet's own framing).
- **Two historical ledger findings reported, not fixed** (per the fix-scope rule — see
  `docs/m28-release-candidate-audit.md` "Fix-scope compliance summary"): `definition-of-done.yaml`'s
  M10 `status: in_progress` stale field; `deferred-backlog.md`'s missing M27 top-level trailer
  summary (row-level content is correct).
- **C1/D4, E1, C10, F1, F2, G1 remain unimplemented this cycle** — by design, per the hard
  constraints; each has a named (or, for C1/D4, explicitly unnamed) future owner.

---

## 6. QA Handoff

- **Scope to verify**: E2's gate mechanism (unit + integration tests, both new files); the R-M28-1
  regression pre-check's conclusion (re-run the same `rg` sweep, confirm the existing M17 test files
  are byte-for-byte unmodified via `git diff v1.0.27`); C5's investigation (re-run
  `hbf1xv_m28_c5_disk_boot_investigation_system_test` and `tools/openmsx-m28-c5-boot-parity.ps1`
  independently, confirm the same negative finding and the same A/B parity result); the health
  audit's concrete command outputs (re-run the `rg`/`Test-Path`-equivalent sweeps, re-launch both
  executables); the full, unfiltered `ctest` run (both suites' `error_markers == 0` hard assertions
  must still hold).
- **Do NOT re-open**: C1/D4 (UNBUILDABLE-WITHOUT-FABRICATION, no milestone assigned), E1/C10/F1/F2/G1
  (DEFERRED with named owners) — these are out of scope for M28 QA review; QA should confirm they
  were NOT touched (`git diff v1.0.27` shows zero changes outside `src/devices/audio/
  ym2413_opll.{h,cpp}`, `src/machine/hbf1xv_machine.{h,cpp}`, the two removed placeholder files, and
  the new test/tool/doc files listed in this report).
- **Artifacts for QA**: `docs/m28-release-candidate-audit.md`, `docs/m28-parity-trace-diff.md`, this
  report, `agent-protocol/state/deferred-backlog.md` (updated rows), `docs/asset-checksums.txt`.
