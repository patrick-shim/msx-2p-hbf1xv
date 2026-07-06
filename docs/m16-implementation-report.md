# M16 Implementation Report — FDC Drive Mechanics (Fujitsu MB89311 / WD2793, Sony HB-F1XV)

- Milestone: M16 (REQ-M16-003), continuing a prior in-flight session (S1-S5 device sources +
  machine wiring already drafted; S6 boot-checkpoint + Type IV unit coverage + evidence gates were
  the outstanding work this cycle).
- Author: MSX Developer Agent.
- Date: 2026-07-07.
- Scope delivered: full M16 slice plan S1-S6 per `docs/m16-planner-package.md`. No DEFERRED item
  was implemented; C10 (flux/DMK fidelity, the planner's proposed "B10") remains OPEN.

## 1. Milestone Target

Implement the HB-F1XV floppy disk controller as a WD2793 core (physical chip Fujitsu MB89311,
register/command-compatible) with the Sony memory-mapped connection style, plus the built-in
single 3.5" 720 KB (2DD) drive and a fully deterministic disk image; wire it onto the DISK ROM
already mapped at slot 3-2 page 1 (M13), replacing the bare ROM attach; close backlog B8; advance
C5 (full-boot checkpoint); produce a genuine openMSX A/B comparison; keep all M1-M15 suites green.

## 2. Code Changes

### 2.1 Already-drafted device/machine wiring (verified this cycle, not re-authored)
- `src/devices/fdc/disk_image.{h,cpp}` — deterministic 737,280-byte 2DD image (80×2×9×512, media
  0xF9), CHS↔LBA, `synthesize()` a pure function of constants (`references/openmsx-21.0/src/fdc/
  DSKDiskImage.cc`/`SectorAccessibleDisk.cc` behaviour reference).
- `src/devices/fdc/disk_drive.{h,cpp}` — drive mechanism: track/side, motor + ~4 s delayed
  motor-off (`RealDrive.cc:263-321`), ready/write-protect/track00/index-pulse/disk-changed sense,
  sector access.
- `src/devices/fdc/fdc_clock_source.h` — `FdcClockSource` (mirrors `RtcClockSource`,
  `src/devices/rtc/rp5c01.h:14-18`): all FDC timing derives read-only from `elapsed_cycles()`.
- `src/devices/fdc/wd2793.{h,cpp}` — the WD2793 core (five registers, Type I/II/III/IV, DRQ/Lost
  Data, CRC-16-CCITT), grounded in `references/openmsx-21.0/src/fdc/WD2793.cc`.
- `src/devices/fdc/sony_fdc.{h,cpp}` — Sony connection-style decode wrapping `disk_rom_`;
  `0x7FF8-0x7FFF` per `references/openmsx-21.0/src/fdc/PhilipsFDC.cc:24-172` (authoritative over
  the fact-sheet's inferred table — DSKCHG at `0x7FFD` bit2, `0x7FFF` active-low).
- `src/machine/hbf1xv_machine.{h,cpp}` — `SonyFdc` composed over `disk_rom_` + `DiskDrive` +
  `DiskImage`; attached at slot (3,2,1) replacing the bare ROM; `FdcClock` cycle-source adapter;
  `cold_boot()` resets the FDC stack and re-mounts the deterministic default image; accessors
  `fdc()`/`disk_drive()`/`disk_image()`.
- `CMakeLists.txt` links the four new `src/devices/fdc/*.cpp` files.

### 2.2 This cycle's new work

**S6 — boot-checkpoint integration test (the missing file that broke `cmake` configure):**
- `tests/integration/machine/hbf1xv_m16_boot_checkpoint_integration_test.cpp` (new) — boots the
  authentic BIOS (`cold_boot()`, no synthetic harness) for 400,000 instructions, asserting: no
  missing-asset diagnostics; two-run byte-identical (PC,opcode) stream + identical final/max PC +
  identical FDC diagnostic counters; max PC strictly > the M15 checkpoint (0x488); and the
  **honestly-observed** FDC-engagement state (see §5).
- `tests/CMakeLists.txt:442-449` already declared this target from the prior session; no CMake
  change was needed once the source file existed.

**Type IV (Force Interrupt) unit coverage + a genuine grounding fix it uncovered:**
- `tests/unit/devices/fdc/wd2793_type4_unit_test.cpp` (new) — i3 (immediate INTRQ), i2
  (index-pulse-scheduled INTRQ, NOT immediate), i2-while-not-ready (never arms), a new command
  superseding a still-pending i2 schedule, and flags=0 (terminate without forcing INTRQ).
- Writing this test exposed that the existing `start_type4`'s i2 handling asserted INTRQ
  **immediately** rather than **scheduling** it at the drive's next index pulse
  (`references/openmsx-21.0/src/fdc/WD2793.cc:1049-1050` `irqTime = drive.getTimeTillIndexPulse
  (time)`). Fixed in `src/devices/fdc/{wd2793,disk_drive}.{h,cpp}`: added
  `DiskDrive::cycles_until_index_pulse()`, and `Wd2793::index_irq_armed_/index_irq_deadline_`
  checked in `sync()`; a new command supersedes a still-armed schedule (matches `write_command`'s
  existing INTRQ-supersede logic).
- Wired into `tests/CMakeLists.txt` (`devices_fdc_wd2793_type4_unit_test`).

**A pre-existing (never previously build-tested) Write-Track test bug, found and fixed:**
- `tests/unit/devices/fdc/wd2793_type3_unit_test.cpp`'s Write-Track case used a synthetic data
  pattern `(j*3+7)&0xFF` that happened to emit the WD2793's own reserved Write-Track escape byte
  values `0xF5`/`0xF7` as literal sector data. Our parser (like openMSX's, `WD2793.cc:972-992`)
  treats these specially **unconditionally** wherever they appear in the streamed bytes, matching
  real hardware (real disk-format software never embeds them as literal data-field content) —
  so the test's own fixture was invalid input, not a production bug. Fixed the test's pattern
  generator to `(j*3+7) % 0xF5` (keeps every byte in `[0x00,0xF4]`, below the first reserved code).
  This was masked in the prior session because `cmake configure` was broken (S6's missing file),
  so `ctest` had never actually run this test until this cycle.

**A genuine, A/B-discovered HEAD_LOADED (HLD) status-bit bug, found and fixed:**
- Building the openMSX A/B FDC probe (§4) surfaced a real divergence at the very first Restore
  status poll: this emulator reported HEAD_LOADED (bit 5) set, genuine openMSX reported it clear.
  Grounding in `references/openmsx-21.0/src/fdc/WD2793.cc:420-433,522-533,820-834,1062-1073`:
  HEAD_LOADED reflects the **H-flag (bit 3, 0x08) of the most recently issued Type-I command**
  (active immediately when H is set; unconditional on Type II/III command acceptance), staying set
  for a ~3 s idle-timeout window after last (re)activated — it is **NOT** tied to the drive motor
  state. The prior model ("HLD/HLT tied to motor-on, no delay") was an incorrect simplification.
  Fixed in `src/devices/fdc/wd2793.{h,cpp}`: `hld_active_`/`hld_since_` state; H-flag-gated
  activation in `start_type1`; unconditional activation (ready path) in `start_type2`/
  `start_type3`; idle-timeout restart in `end_command()` (Type I completion now also routes
  through `end_command()` instead of an inlined duplicate, so the restart logic applies there
  too). No existing test had asserted the old (incorrect) behaviour, so this is a clean, additive
  correction — verified by re-running the A/B probe (§4) and the full `ctest` suite.

**New diagnostic counters on `Wd2793` (non-perturbing, back the boot-checkpoint signal):**
- `read_sector_commands_accepted()`, `read_sector_bytes_transferred()`,
  `read_sector_completions_ok()` — cumulative counters (cleared by `reset()`) incremented at the
  precise points the planner's §6.3 acceptance signal names: Type II Read Sector acceptance,
  each genuine DRQ byte transferred during a Read Sector, and clean (no-error) command completion.
  Pure bookkeeping; zero effect on emulated timing/behaviour.

**New diagnostic accessor on `Hbf1xvMachine` (non-perturbing, used during investigation):**
- `debug_sub_slot_register(int primary)` — direct (non-`#FFFF`-indirected) sub-slot-register
  readback, used to determine definitively whether the CPU ever pages slot (3,2) into page 1
  during an unattended boot, independent of which primary slot currently answers page 3's `0xFFFF`
  indirection. Follows the existing `slot_expanded()`/`debug_bus_read()` debug-seam convention.

**New `tools/` scripts (deterministic, reproducible, per CLAUDE.md preference):**
- `tools/gen-m16-boot-disk.py` — pure-function port of `DiskImage::synthesize()`; produces
  `tests/parity/m16_boot.dsk` (737,280 bytes; verified byte-identical to what this emulator's
  `cold_boot()` mounts by default, same sha256 on both sides).
- `tools/gen-m16-fdc-probe.py` — assembles the exact same Z80 byte sequence as
  `build_restore_read_sector_probe()` in `hbf1xv_m16_fdc_integration_test.cpp`, into
  `tests/parity/m16_fdc_probe.bin` (66 bytes), for the openMSX-side probe.
- `tools/openmsx-m16-boot-parity.ps1` — runs BOTH A/B subjects (§4) end-to-end and writes all
  artifacts; reproducible (re-run during this cycle after a `rm -rf build` + fresh reconfigure and
  reproduced identical results).

## 3. Unit Test Results

All new/updated deterministic unit tests pass:
- `devices_fdc_disk_image_unit_test` (pre-existing, unmodified) — geometry, CHS↔LBA, determinism,
  write-protect.
- `devices_fdc_wd2793_type1_unit_test` (pre-existing, unmodified) — reset TR=0xFF, Restore/Seek/
  Step, Type-I status bits, INTRQ, step-rate timing.
- `devices_fdc_wd2793_type2_unit_test` (pre-existing, unmodified) — Read/Write Sector, DRQ, Lost
  Data, Record Not Found.
- `devices_fdc_wd2793_type3_unit_test` (test-fixture bug fixed this cycle, see §2.2) — Read
  Address, Write Track (reformats + reads back correctly with the corrected pattern), Force
  Interrupt mid-Read-Sector.
- `devices_fdc_wd2793_type4_unit_test` (**new this cycle**) — i3 immediate INTRQ (mid a settling
  Type-I command), i2 index-pulse-scheduled INTRQ (armed, not immediate; fires exactly at the
  computed deadline; no re-arm), i2 with drive not ready (never arms), a superseding command
  clearing a stale i2 schedule, flags=0 (terminate without forcing INTRQ).
- `devices_fdc_sony_fdc_unit_test` (pre-existing, unmodified) — register decode, DSKCHG, active-low
  INTRQ/DTRQ, ROM passthrough, no mirroring.

## 4. Integration Test Results

- `machine_hbf1xv_m16_fdc_integration_test` (pre-existing, unmodified) — real CPU program over the
  full M11 bus drives Restore + Read Sector(LBA 0) via slot (3,2), motor-on/delayed-motor-off,
  DSKCHG readback.
- `machine_hbf1xv_m16_boot_checkpoint_integration_test` (**new this cycle**) — real BIOS boot,
  400,000 instructions, deterministic two-run identical (PC,opcode) stream, max PC 0x7D6F (> the
  M15 checkpoint 0x488), and the honestly-observed FDC-engagement state (zero — see §5).

### Full suite (evidence gate)
```
100% tests passed, 0 tests failed out of 72
```
(64 prior M0-M15 tests + 8 new/M16 tests: `devices_fdc_wd2793_type4_unit_test` +
`machine_hbf1xv_m16_boot_checkpoint_integration_test` are genuinely new this cycle; the other 6
M16 tests existed from the prior session and are now, for the first time, actually build-verified
— one of them (`wd2793_type3`) needed a test-fixture fix, §2.2.) Command:
`ctest --test-dir build -C Debug --output-on-failure`, run against a **fresh** `rm -rf build` +
`cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` (0 errors,
only pre-existing C4819 codepage warnings).

### Timing-oracle / #A8 regression guard
`machine_hbf1xv_cpu_step_integration_test`, `devices_chipset_m1_wait_unit_test`,
`machine_hbf1xv_cpu_parity_integration_test`, `machine_hbf1xv_m11_parity_checkpoint_integration_
test`, `machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`, `machine_hbf1xv_m15_boot_
checkpoint_integration_test` all pass, byte-identical to their prior values. `step_cpu_instruction`
T-state math and `increment_refresh_register()` call sites were **not modified**. All new FDC
timing (Busy/DRQ/step/settle/index/motor/HLD-idle) derives from `FdcClockSource`/
`elapsed_cycles()`, never wall-clock.

### openMSX A/B parity (real, not fabricated) — full detail in `docs/m16-parity-trace-diff.md`
- **Subject 1 (real BIOS boot, 3000 instructions):** ARCHITECTURAL PARITY — empty diff vs genuine
  openMSX 19.1 `Sony_HB-F1XV`, with the FDC now live at slot 3-2. Both traces land in the
  identical final instruction (PC=0x0455).
- **Subject 2 (FDC register/command probe, identical `tests/parity/m16_boot.dsk` mounted on both
  sides):** the register/command-sequence portion (0-37, side/drive/motor/Restore/status-poll/
  Read-Sector-issuance) is an exact empty-diff match (**after** the HLD fix — the FIRST run of
  this probe is what surfaced the HLD bug, §2.2). From the DRQ-wait polling loop onward, the two
  WD2793 models take a different number of loop iterations (documented cycle-cadence difference,
  not gated, matching the established M10+ convention) — but **both emulators reach an identical
  terminal state** (PC/AF/BC/DE/HL/SP all matching at HALT) and **the actual 512 transferred bytes
  match byte-for-byte** against the expected LBA-0 sector content of the identical disk image.
- Adversarial comparator self-check: empty B side → exit 2 BLOCKED; corrupted field → exit 1
  DIVERGENCE. The parity claims above are trustworthy.

## 5. Known Issues / Deviations From the Planner's Assumed Boot Sequence (honest, not fabricated)

- **Boot-checkpoint acceptance signal (planner §6.3), PRIMARY signal (a)-(c) NOT reached.**
  Genuinely investigated (see `docs/m16-parity-trace-diff.md` §4 for the full account): with the
  FDC live at slot 3-2, real-boot PC advancement past the M15 checkpoint IS achieved (max PC
  0x7D6F, deterministic, two-run identical — signal (d)), but the CPU never pages slot (3,2) into
  page 1 during an unattended, keyboard-less cold boot — confirmed with a direct (non-`0xFFFF`-
  indirected) sub-slot-register check on every instruction step, up to a 20,000,000-instruction
  diagnostic run (not committed as a test). The WD2793 command register is consequently never
  written; the new diagnostic counters stay at zero. This is:
  - **Not a missing/malformed asset**: `bios/f1xvdisk.rom` carries a valid `"AB"` expansion-ROM
    header at its base (verified directly).
  - **Not a divergence from real hardware in this emulator's boot trajectory**: Subject 1 above
    shows genuine, empty-diff parity with real openMSX over the comparable window, and the deeper
    (non-A/B-compared) trajectory — a long RAM-sizing-test-shaped loop, then settling into a
    steady low-PC idle loop consistent with a BASIC input-wait prompt — is ordinary, plausible MSX
    cold-boot behaviour, not a defect signature.
  - **A genuine, honestly-recorded residual**: the real MSX auto-disk-boot trigger condition
    requires something an unattended, no-keyboard-input cold boot does not provide. Pinning down
    exactly what (a specific BASIC cold-start decision path, most likely) needs the disk-ROM/
    SUB-ROM disassembly, which is beyond this milestone's scope to reverse-engineer. The FDC
    **device itself** is independently, positively verified correct (S1-S5 tests + Subject 2
    above), so the residual is specifically in **automatically reaching** FDC engagement via the
    unattended real boot path, not in the FDC's own behaviour.
  - **Verification action** (for a future milestone/session): obtain or produce a disk-ROM/SUB-ROM
    disassembly (or trace a REAL Sony HB-F1XV / openMSX run for many more instructions with a
    simulated keypress or a DiskBASIC auto-run flag) to identify the actual auto-boot trigger, then
    re-attempt the S6 checkpoint against that specific, now-known trigger condition.
- **DRQ-cadence cycle-level difference vs openMSX (Subject 2, R-M16-2's anticipated risk)**:
  this emulator's deterministic ~114-cycle/byte model does not match openMSX's real-track-rotation
  `DynamicClock` cadence exactly (expected — never claimed to; T-state/cycle fields are excluded
  from the architectural gate since M10). Functional correctness (final state + transferred data)
  is confirmed instead. Exact cross-emulator cycle parity remains backlog C1/C2 territory.
- **Write Track / flux-level fidelity**: standard 9-sector reformat only (verified round-trip);
  arbitrary flux/DMK images and non-standard track layouts are out of scope (backlog C10, formerly
  proposed as "B10" by the planner — the row already existed in the ledger from the prior session
  under the id C10; re-affirmed OPEN, not duplicated).
- **Second/phantom drive, real-`.dsk` host-path mounting at runtime, SDL3/CLI disk-insert UX**: all
  explicitly out of scope per the planner (§1.2); untouched.

## 6. Backlog Bookkeeping (same cycle)

`agent-protocol/state/deferred-backlog.md` updated: **B8 → DONE (M16)** (FDC drive mechanics: WD2793
core + Sony decode + deterministic image + motor-off + DSKCHG, unit/integration-tested, real A/B
evidence). **C5 advanced** (real-boot max PC 0x7D6F vs the M15 0x488; real openMSX A/B parity
confirmed over both a real-boot window and a dedicated FDC probe) but **remains IN-PROGRESS**,
honestly, because the automatic disk-boot handshake is not yet reached. C10 (flux/DMK fidelity)
re-affirmed OPEN (row already existed from the prior session). All other rows unchanged.
`agent-protocol/state/definition-of-done.yaml` M16 block: `implementation.*` flags set true
(genuinely evidenced this cycle); `regression_qa.signoff_decision_recorded` and `overall_done`
left false (QA sign-off is a separate gate, not claimed here).

## 7. QA Handoff

- Scope delivered: full M16 S1-S6 slice plan. No DEFERRED item touched.
- Reproduce: `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` → `cmake --build build --config
  Debug` → `ctest --test-dir build -C Debug --output-on-failure` (expect 72/72). A/B:
  `tools/openmsx-m16-boot-parity.ps1` (needs WSL openMSX 19.1 + `Sony_HB-F1XV`; regenerates
  `tests/parity/m16_boot.dsk`/`m16_fdc_probe.bin` first via `tools/gen-m16-boot-disk.py` /
  `tools/gen-m16-fdc-probe.py` if not already present).
- Please verify: (a) 72/72 green + zero regression against the M1-M15 baseline; (b) M9/M12 timing
  oracles unchanged; (c) the openMSX A/B results reproduce (Subject 1 empty diff; Subject 2
  register-sequence empty diff pre-DRQ-loop + functional match at completion); (d) the HLD fix and
  the Write-Track test-fixture fix are each independently sound (grounding citations in §2.2); (e)
  the honestly-reported C5 residual (auto-boot handshake not reached) is an acceptable outcome
  given the planner's own explicit contingency language (§6.2/§6.3/R-M16-3) — this developer is
  NOT claiming C5 fully closed, and QA sign-off is a separate gate not claimed here.
