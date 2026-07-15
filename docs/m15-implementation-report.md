# M15 Implementation Report — Device Integration Completion & S1985 Chipset Full Wiring

- Milestone: M15 (REQ-M15-003), scope LOCKED by DEC-0009 (authorized by DEC-0008).
- Author: MSX Developer Agent.
- Date: 2026-07-06.
- IN set (final, DEC-0009): B1 PSG (numeric), B2 RTC (in-memory epoch), C6 full i8255 PPI +
  keyboard + joystick, C4 S1985 backup-RAM `.sram` persistence, C5 boot-checkpoint advance
  (not BASIC). All other backlog rows remain DEFERRED. No DEFERRED item was implemented.

## 1. Milestone Target

Replace the M11 chipset SEAMS — open-bus `#A0-A2` (PSG) and `#B4/B5` (RTC) — with real devices,
expand the port-A-only PPI into a full i8255 so the keyboard rows `#A9-#AB` go live, add the
joystick input path through the PSG, add `.sram` persistence to the S1985 backup RAM, and advance
the deterministic BIOS boot checkpoint past the M13 first-device-read boundary — all headless,
deterministic, and grounded in the S1985 fact-sheet + openMSX behaviour references (never copied).

## 2. Code Changes (per slice)

### S1 — PSG / YM2149 (B1) — `src/devices/audio/psg_ym2149.{h,cpp}` (new folder)
- 16-register file (R0-R15) as a `core::IoDevice` on `#A0/#A1/#A2` (dispatch `port & 0x03`).
  Address latch masked `& 0x0F` (mirrored registers, A-M15-3; `MSXPSG.cc`).
- 12-bit tone periods (R0-R5), 5-bit noise (R6), 16-bit envelope period (R11/R12), 4-bit
  envelope shape (R13) driving a **5-bit / 32-step** YM2149 envelope counter (shape state machine
  faithful to `AY8910.cc:382-445` — attack/hold/alternate/holding).
- R7 mixer/IO-direction **MSX masking**: bit6 forced 0 (port A input), bit7 forced 1 (port B
  output), mask-preserved (fact-sheet §2 "Critical R7 note").
- **YM2149 register-readback-as-written** (no AY unused-bit masking) — behaviour-affecting,
  A/B-tested.
- Deterministic numeric stereo mix: A=Center (both), B=Left, C=Right (fact-sheet §2). Generators
  advance READ-ONLY off the machine clock via `advance_cycles()` (X4). **No audio device/output/
  presentation** (DEC-0009 Q4 — numeric model only).
- R14/R15 delegate to an injected `PsgPortSource` (joystick/layout/cassette).

### S2 — Joystick (C6) — `src/peripherals/joystick.{h,cpp}`
- Two ports read via PSG R14 (bit0-3 directions, bit4-5 triggers, 0=pressed; bit6 JIS layout;
  bit7 cassette idle-high), selected via PSG R15 bit6; bit7 KANA LED tracked. Implements
  `PsgPortSource`; injected into the PSG (X5 — peripheral → PSG, not → S1985). Idle read = 0xFF.

### S3 — RTC / RP5C01 (B2) — `src/devices/rtc/rp5c01.{h,cpp}` (new folder) + `#F5` gate
- `core::IoDevice` on `#B4` (address latch `& 0x0F`; read 0xFF) / `#B5` (4-bit data; read
  `data | 0xF0`). 4 blocks × 13 regs + mode(13)/test(14)/reset(15); per-block write/read masks
  from `RP5C01.cc:36-41`. Block-0 BCD time, Block-2 CMOS **reg0 = 0x0A valid marker**, regs 14/15
  unreadable (peek 0x0F). Block select via mode reg 13.
- **In-memory deterministic epoch** (DEC-0009 Q2): fixed 1988-01-01 00:00:00 seed; time advances
  ONLY from `RtcClockSource` (scheduler cycles) via the `updateTimeRegs` algorithm
  (`RP5C01.cc:202-255`). No host clock, no file. Byte-identical across runs.
- `#F5` CLOCK-IC gate: `src/devices/chipset/system_control.{h,cpp}` (`SystemControlF5`) implements
  `RtcClockGate`; RTC data path inert when bit7 clear. Reset value 0x80 (enabled) — grounded to
  preserve A/B parity with openMSX (which never gates the RTC); see §7 R-4.

### S4 — Full i8255 PPI (C6) — `src/devices/chipset/ppi_8255.{h,cpp}` + keyboard
- Expands the M11 port-A-only PPI (X1). **`#A8` slot-select preserved byte-for-byte** by REUSING
  the M11-verified `PpiSlotSelect` verbatim for port A (drives `SlotBus` unconditionally on write,
  reads back the latch). Ports B/C/control are new surface: `#A9` keyboard row (inverted, input-
  gated), `#AA` port C (row select / motor / cassette / CAPS / click, output-latched), `#AB`
  control (mode set + bit-set/reset), per `I8255.cc`/`MSXPPI.cc`.
- Keyboard matrix `src/peripherals/keyboard_matrix.{h,cpp}`: 11×8 inverted (0=pressed), idle 0xFF;
  implements `KeyboardRowSource`, injected into the PPI. No ghosting (fact-sheet §10 sanctioned).

### S5 — Backup-RAM `.sram` persistence (C4) — extend `src/devices/chipset/s1985_engine.{h,cpp}`
- `load_backup_ram`/`save_backup_ram` (16 bytes). Absent/short/unreadable file → store untouched
  (zero after `reset()`), so the M11 golden (absent → zero) is preserved. Machine adds
  `set_backup_ram_path` / `flush_backup_ram`; `cold_boot` loads when configured. The address/data/
  pattern/color register behaviour (M11-verified) is unchanged.

### S6 — Integration — `src/machine/hbf1xv_machine.{h,cpp}`
- **X2** `wire_bus`: attach PSG `#A0-A2`, RTC `#B4/B5`, `#F5` system-control, full PPI `#A8-AB`
  (mirrors `#AC-AF` already present); inject joystick into PSG; attach RTC clock source + `#F5`
  gate. Existing `#98-9B` / `#FC-FF` / `#40-4F` wiring and mirrors untouched.
- **X3** `cold_boot`: reset the new devices (keyboard, joystick, PPI, PSG, `#F5`, RTC) after the
  existing ones; load `.sram` when configured. The `#A8=0` reset flip now routes through `ppi_`.
- **X4**: `step_cpu_instruction` T-state math (`datasheet_tstates + m1_wait`) and
  `increment_refresh_register` call sites are **NOT touched**. PSG/RTC time is derived READ-ONLY
  from `elapsed_cycles()`/scheduler (RTC lazily on port read; PSG on `advance_cycles`).
- **X5**: joystick ownership is peripheral → PSG only; `S1985Engine` unchanged except the additive
  persistence methods (**X6**).
- `RtcClock` nested adapter returns `scheduler_.total_cycles()`.

### Build wiring
- `CMakeLists.txt`: added the 6 new `.cpp` files. `tests/CMakeLists.txt`: added 8 new test targets.

## 3. Unit Test Results

New deterministic unit tests (all pass):
- `devices_audio_psg_ym2149_unit_test` — latch mask, tone/noise/envelope decode, R7 MSX mask,
  YM readback-as-written, 5-bit envelope shape stepping (shapes 0x00 decay / 0x0C rise), resolved
  amplitude + envelope-enable, stereo mix (A both / B left / C right), R14/R15 routing, determinism.
- `peripherals_joystick_unit_test` — idle 0xFF, active-low direction/trigger bits, R15 bit6 port
  select, KANA bit, read through a real PSG (R15 select → R14 read).
- `peripherals_keyboard_matrix_unit_test` — idle rows 0xFF, inverted column bit on press, row
  isolation, release restore, bounds.
- `devices_rtc_rp5c01_unit_test` — address mask, `#B4`=0xFF, `#B5` upper-nibble float, Block-2
  reg0=0x0A, regs 14/15 unreadable, mode reg readable, deterministic-epoch advance (1s / 75s BCD
  rollover), two-run determinism, `#F5` gate on/off.
- `devices_chipset_ppi_8255_unit_test` — **M11 `#A8` slot-select assertions reused verbatim as a
  locked guard**, plus port C row select, port B inversion, control bit-set/reset (CAPS), motor/
  click bits, control readback.
- `devices_chipset_backup_ram_sram_unit_test` — absent file → false + zero, write/save/reload
  round-trip identical, reset-after-load clears to zero.

## 4. Integration Test Results

- `machine_hbf1xv_m15_devices_integration_test` — **real CPU program** drives the PSG (`OUT #A0/
  #A1` then `IN #A2`, YM readback 0xFF); S1985 `IN(#AC)==IN(#A8)` mirror; keyboard read through PPI
  `#AA`/`#A9` over the bus; joystick read through PSG R14/R15 over the bus; RTC `#B4/#B5` sequence
  gated by `#F5`; backup-RAM `.sram` round-trip across two `cold_boot`s; two-machine determinism.
- `machine_hbf1xv_m15_boot_checkpoint_integration_test` — boots the real BIOS, single-steps 4096
  instructions, asserts no missing assets, two-run byte-identical (PC,opcode) stream + identical
  final PC, and advance strictly past the M13 boundary 0x043C.

### Full suite (evidence gate)
```
100% tests passed, 0 tests failed out of 64
```
(56 prior M0-M14 tests + 8 new M15 tests; zero regression.) Command:
`ctest --test-dir build -C Debug --output-on-failure`.

### Boot checkpoint (C5) reached
```
M15 boot checkpoint: final PC=0x454 max PC=0x488 over 4096 instructions
```
Boot advances from the M13 ~0x043C boundary to **max PC 0x488** (final 0x454), deterministically,
with real PSG/RTC/keyboard/VDP answering the BIOS init reads. Full boot to a prompt remains gated
on the FDC (M16) — the C5 residual, per DEC-0009 (M15 does not need BASIC).

### Timing-oracle regression guard (X4)
The M9/M12 CPU timing oracles are **unchanged and green**: `machine_hbf1xv_cpu_step_integration_test`,
`devices_chipset_m1_wait_unit_test`, `machine_hbf1xv_cpu_parity_integration_test`,
`machine_hbf1xv_m11_parity_checkpoint_integration_test`, `machine_hbf1xv_m13_mem_parity_checkpoint_
integration_test` all pass. `step_cpu_instruction` T-state math and `increment_refresh_register`
call sites were not modified.

### `#A8` slot-select preservation guard (X1)
The M11 `chipset_ppi_slot_select_unit_test` is unchanged and green, and its three `#A8` assertions
are reused **verbatim** inside `devices_chipset_ppi_8255_unit_test` as a locked guard. No slot
behaviour changed; `Ppi8255` port A IS the reused `PpiSlotSelect`.

### openMSX A/B parity (real, not fabricated)
`tools/openmsx-io-parity.ps1` with `tests/parity/m15_io_probe.bin` vs **genuine openMSX 19.1
`Sony_HB-F1XV`** → **ARCHITECTURAL PARITY, empty diff over 15 instructions**. Substantive, non-
blank: openMSX returns A=0x34 (PSG R0 readback, seq 3) and A=0x80 (PSG R7 with the MSX port-
direction mask, seq 14), matching this emulator byte-for-byte. Details + reproduce steps:
`docs/m15-parity-trace-diff.md`. (RTC data-nibble value and joystick/keyboard idle reads are
CMOS/config-dependent and covered by the deterministic unit/integration tests instead — noted in
the parity doc.)

## 5. Evidence Gates (captured)

| Gate | Result |
|------|--------|
| `tools/validate-assets.ps1` | `Asset validation result: True` (7 BIOS files present, 1 ROM) |
| `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | written; SHA256 recorded |
| `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` | build OK (only pre-existing C4819 codepage warnings on non-ASCII comment chars) |
| `ctest --test-dir build -C Debug --output-on-failure` | **64/64 passed, 0 failed** |
| `tools/openmsx-io-parity.ps1` (M15 probe) | ARCHITECTURAL PARITY — empty diff (real HB-F1XV) |

## 6. Prior-suite tests updated

**None weakened; none rewritten.** No existing M0-M14 test file was modified. All new behaviour is
additive. The old `chipset_ppi_slot_select_unit_test` remains as the untouched `#A8` locked guard
(the machine now uses `Ppi8255`, which composes `PpiSlotSelect` for port A). The M13 boot golden
(`hbf1xv_bios_boot_integration_test`, 32-instruction image-grounded checkpoint) still passes
unchanged.

## 7. Known Issues / Assumptions / Deviations (each with a verification action)

- **`#F5` gate polarity / reset value (R-4).** openMSX 19.1 does not model an `#F5` RTC gate
  (`MSXRTC` answers `#B4/B5` unconditionally). To preserve A/B parity while honoring fact-sheet §5,
  `SystemControlF5` resets to 0x80 (CLOCK-IC enabled, active-high bit7). *Verify:* Sony service
  docs / a real HB-F1XV `#F5` power-on read; adjust if the real reset value differs. Deterministic
  and gate-tested either way.
- **RTC epoch is a fixed synthetic 1988-01-01 (A-M15-1 / DEC-0009 Q2).** Chosen for determinism;
  not the real-time clock. *Verify:* none needed for M15 (determinism is the requirement); file/
  CMOS persistence is a later persistence milestone.
- **PSG audio is numeric only (DEC-0009 Q4).** Tone/noise/envelope counters + stereo mix are
  integer-exact; no analog DAC / SDL3 sink. *Verify:* deferred to the frontend milestone (C9).
- **PSG generator sample cadence** (`kCyclesPerGeneratorStep = 16`, fc = clock/2, step = fc/8) is a
  deterministic model; exact audio-sample A/B is out of scope (audio deferred). Register/envelope
  state — the load-bearing behaviour — is unit-tested and (for R0/R7) A/B-matched.
- **YM vs AY readback:** the HB-F1XV is a YM2149 (returns as written); confirmed cross-comparable
  bits (R0 8-bit, R7 masked) match openMSX. The YM-specific R1=0xFF readback is unit-tested and
  fact-sheet-grounded (§2) but not put on the A/B gate to avoid an openMSX-config-dependent result.
- **C5 residual:** full boot to a BASIC/Disk prompt needs the FDC (deferred M16). M15 delivers only
  the checkpoint advance, per DEC-0009 Q1.

## 8. Backlog bookkeeping (same cycle)

`agent-protocol/state/deferred-backlog.md` updated: **B1, B2, C4, C6 → DONE (M15)**; **C5 →
IN-PROGRESS (M15 partial)** with the reached checkpoint noted; footer refreshed. Rows preserved.

## 9. QA Handoff

- Scope delivered exactly to DEC-0009; no DEFERRED item touched.
- Reproduce: `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` → `cmake --build build --config
  Debug` → `ctest --test-dir build -C Debug --output-on-failure` (expect 64/64). A/B:
  `tools/openmsx-io-parity.ps1 -ProgramBin tests/parity/m15_io_probe.bin -DiffOut
  docs/m15-parity-trace-diff.md -MaxSteps 15` (needs WSL openMSX 19.1 + `Sony_HB-F1XV`).
- Please verify: (a) 64/64 green + zero regression; (b) M9/M12 timing oracles + M11/M13 slot tests
  unchanged/green; (c) the A/B empty diff reproduces on the genuine HB-F1XV; (d) the boot checkpoint
  advance (max PC 0x488) reproduces deterministically. QA sign-off is a separate agent — not
  claimed here.
