# M11 Implementation Report — S1985 "MSX-ENGINE" Chipset & Full System Bus

- Milestone: M11 (REQ-M11-003)
- Authority: `docs/m11-planner-package.md`; DEC-0002; S1985 fact-sheet.
- Status: implementation complete; all evidence gates green; A/B parity EMPTY diff
  captured vs genuine Sony HB-F1XV. Awaiting QA sign-off (separate agent).

## 1. Slices delivered (S1–S6)

All six slices implemented in order per the planner package, each keeping `ctest`
green before proceeding.

## 2. Files added

Source (contracts + chipset fabric + machine glue):
- `src/core/device_contracts.h` — `MemoryDevice` / `IoDevice` / `SwitchedDevice` contracts.
- `src/devices/chipset/slot_bus.{h,cpp}` — primary `#A8` + secondary `#FFFF` slot decode.
- `src/devices/chipset/ppi_slot_select.{h,cpp}` — PPI port-A `IoDevice` on `#A8`.
- `src/devices/chipset/io_bus.{h,cpp}` — 256-port dispatch + straight-alias mirrors.
- `src/devices/chipset/mapper_io.{h,cpp}` — `#FC-#FF` segment store + `100xxxxx` readback.
- `src/devices/chipset/switched_io.{h,cpp}` — `#40-#4F` switched-I/O controller.
- `src/devices/chipset/s1985_engine.{h,cpp}` — thin S1985: backup RAM (ID 0xFE), mapper
  readback config, `m1_wait_tstates` helper.
- `src/devices/chipset/system_bus.{h,cpp}` — `core::Bus` composing `SlotBus` + `IoBus`.
- `src/machine/ram_slot_backing.{h,cpp}` — DRAM `MemoryRegion` as slot 3-0 `MemoryDevice`.

Tests (10 new): `cpu_io_seam_unit_test`, `chipset_slot_bus_unit_test`,
`chipset_ppi_slot_select_unit_test`, `chipset_io_bus_unit_test`,
`chipset_backup_ram_unit_test`, `chipset_mapper_io_unit_test`,
`chipset_m1_wait_unit_test`, `machine_hbf1xv_slot_map_unit_test`,
`machine_hbf1xv_system_bus_integration_test`,
`machine_hbf1xv_m11_parity_checkpoint_integration_test`.

Tooling/artifacts: `tools/openmsx-io-parity.ps1`, `tests/parity/m11_bus_probe.bin`
(26 bytes, SHA-256 f8be4803533d77147e5ddf398415046f093168dd7bb8796342a51aec8b799455),
`docs/m11-parity-trace-diff.md`, refreshed `docs/asset-checksums.txt`.

## 3. Files changed

- `src/core/bus.h` — added `io_read`/`io_write` virtuals (open-bus 0xFF defaults; keeps
  M0–M10 memory-only fakes compiling).
- `src/devices/cpu/cpu_bus_client.{h,cpp}` — `io_read`/`io_write` seam.
- `src/devices/cpu/z80a_cpu.{h,cpp}` — routed `IN/OUT (n)`, `IN/OUT (C)`, block I/O through
  `bus_.io_read/io_write`; added per-step M1-cycle counter (incremented in the single
  `increment_refresh_register` choke-point; datasheet T-states UNCHANGED).
- `src/machine/hbf1xv_machine.{h,cpp}` — removed flat `MachineBus`; instantiated/ wired the
  fabric + S1985; `cold_boot` sets the M11 slot default; `step_cpu_instruction` adds the M1
  wait to the scheduler.
- `src/devices/chipset/README.md` — populated (architecture + file map + boundaries).
- `CMakeLists.txt` — 9 new `.cpp` added to `sony_msx_core`.
- 8 timing-oracle tests updated for the M1 wait (R-3, see §5).

## 4. Per-slice test evidence

- **S1** `devices_cpu_io_seam_unit_test` — IN/OUT round-trip via a capturing `IoDevice`,
  port formation `(A<<8)|n` and `BC`, unmapped→0xFF, M1-count oracle (NOP=1, LD A,n=1,
  CB=2, ED=2, DD=2, DDCB=2).
- **S2** `devices_chipset_slot_bus_unit_test`, `devices_chipset_ppi_slot_select_unit_test`
  — page→primary routing, `#A8` readback, attach/resolve, expanded-slot-3 `#FFFF` set +
  `0xFF^reg` readback, non-expanded `#FFFF` as normal memory, unmapped→0xFF.
- **S3** `devices_chipset_io_bus_unit_test` — dispatch on `port&0xFF`, `#AC`==`#A8` and
  `#9C`↔`#98` mirror aliases, unmapped open-bus.
- **S4** `devices_chipset_backup_ram_unit_test` (ID-select 0xFE, address/data/pattern-rotate/
  color regs, reset clears), `devices_chipset_mapper_io_unit_test` (0→0x80, 0x1F→0x9F,
  0x25→0x85), `devices_chipset_m1_wait_unit_test` (helper linearity + §8 examples XOR A 4→5,
  BIT 0,(HL) 12→14, OUT (n),A 11→12, cross-checked against the real CPU M1 count).
- **S5** `machine_hbf1xv_slot_map_unit_test` (all 4 pages→slot 3-0 DRAM, `#A8`=0xFF reset
  pin, CPU write→DRAM, `#FFFF`→0xFF^0), `machine_hbf1xv_system_bus_integration_test`
  (end-to-end `#A8`/`#AC`/`#FC`/`#40`/`#FFFF` through the CPU + determinism oracle).
- **S6** `machine_hbf1xv_m11_parity_checkpoint_integration_test` (probe→HALT golden trace,
  B=0x85/C=0x01/D=0x3C) + the A/B artifact below.

## 5. R-3 timing-oracle updates (datasheet + M1 wait, grounded in §8)

The +1-per-M1 wait changes machine `elapsed_cycles`. Every affected oracle was recomputed
(not weakened) to the MSX-accurate datasheet+M1 total:

| Test | Old | New | Derivation |
|---|---|---|---|
| `hbf1xv_cpu_step_integration` | steps 7/4/4/4, elapsed 19 | 8/5/5/4, elapsed 22 | +1 per fetched op; halted idle 0 M1 |
| `cpu_bootstrap_im1_system` | 4/4/13, elapsed 21 | 5/5/14, elapsed 24 | EI/NOP +1; IM1 ack +1 |
| `hbf1xv_cb_program_integration` | 89 | 102 | +13 M1 (RLC=2, DJNZ=1 ×3 etc.) |
| `hbf1xv_trace_program_integration` | elapsed 22 | 26 (CPU TC stays 22) | CPU datasheet vs machine clock diverge by design |
| `hbf1xv_indexed_program_integration` | 94 | 105 | 5 DD ops ×2 + HALT = +11 |
| `hbf1xv_ldir_program_integration` | 92 | 102 | 3 loads + 3×2 (LDIR) + HALT = +10 |
| `hbf1xv_interrupt_ack_integration` | IM2 44 / IM0 34 | 49 / 38 | +M1 per fetch + ack |
| `hbf1xv_debug_event_log_integration` | golden T stamps | +1/op golden | machine-time event log |

CPU-level tests that publish datasheet T-states (`hbf1xv_parity_checkpoint`,
`hbf1xv_debug_dump` TSTATES=22) are unchanged and stayed green — confirming the wait is
isolated to the machine/scheduler (A-4).

## 6. Evidence gates (captured)

- `tools/validate-assets.ps1` → `Asset validation result: True` (7 BIOS files, 1 ROM). exit 0.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` → report written. exit 0.
- `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` →
  build succeeds. exit 0. (Only pre-existing C4819 code-page warnings for non-ASCII comments.)
- `ctest --test-dir build -C Debug --output-on-failure` → **100% tests passed, 38/38, 0 failed.**

## 7. A/B parity (S6) — REAL captured diff, EMPTY

`docs/m11-parity-trace-diff.md`. Subject A = this emulator (`--parity-trace`); Reference B =
openMSX 19.1 on WSL running the **genuine Sony_HB-F1XV machine** whose XML instantiates a real
`<S1985>` (5-bit mapper read-back; `/usr/share/openmsx/machines/Sony_HB-F1XV.xml`, real
HB-F1XV system ROMs present in the WSL install). Result: **ARCHITECTURAL PARITY — EMPTY DIFF**,
all 15 probe instructions match on every architectural field. S1985 behaviours confirmed
against real hardware model: mapper `100xxxxx` readback (0x85), switched-I/O `~ID` (0x01),
16-byte backup-RAM round-trip (0x3C). `trace-diff.py` exit 0 (not fabricated; reproduce with
`tools/openmsx-io-parity.ps1`).

Scope note (honest): the probe is deliberately slot-safe and BIOS-independent. A free-running
slot-manipulating trace is not used for A/B because the M11 bring-up slot default (`#A8`=0xFF,
R-1) differs from a real BIOS boot; the `#AC`/`#9C` mirrors are covered by unit + machine
integration tests (their absolute `#A8`/`#AC` values are BIOS-dependent, only the mirror
invariant is machine-independent).

## 8. Deviations / assumptions (with verification)

- **A-2 / R-1 (tracked, non-silent):** `cold_boot` forces `#A8`=0xFF so all pages resolve to
  slot 3-0 DRAM (bootable without BIOS). Pinned by `machine_hbf1xv_slot_map_unit_test`
  (`ResetPrimarySelect_A8_IsFF_M11BringUpDefault`) and commented in `hbf1xv_machine.cpp`.
  **M12 MUST flip the reset default to `#A8`=0 (slot-0 BIOS)** once ROMs populate slot 0.
- **A-1:** unmapped I/O reads return open-bus 0xFF (bus.h default) — cited §10; no M9 I/O test
  regressed.
- **A-4:** Z80 keeps datasheet T-states; S1985 M1 wait applied by machine — verified by the
  CPU-level tests staying green while machine oracles were recomputed.
- **A-5 / R-6:** backup-RAM `.sram` persistence is out of M11 scope; storage is volatile and
  cleared on reset (verified by `Reset_ClearsBackupRam`). Flag to coordinator if a later
  milestone needs persistence.
- **Scope fence honored:** no PSG/RTC register behavior, no mapper RAM banks, no ROM images,
  no VDP logic — only the seams those milestones plug into.

## 9. QA handoff

Ready for QA regression assessment. Suggested focus: independently re-derive the R-3 oracle
values (§5), confirm the A/B diff is genuinely captured (re-run `tools/openmsx-io-parity.ps1`
if WSL/openMSX available), and confirm the R-1 default is tracked for M12.
