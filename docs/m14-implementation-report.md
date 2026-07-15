# M14 Implementation Report — Yamaha V9958 VDP (register/VRAM/status/interrupt contract)

- Milestone: M14 (Yamaha V9958 VDP incl. 128 KB VRAM ownership). Request: REQ-M14-003.
- Author: MSX Developer Agent. Date: 2026-07-06.
- Scope delivered: the V9958 register/VRAM/status/interrupt CONTRACT (device-level, unit- +
  A/B-verifiable), slices S1–S6 per `docs/m14-planner-package.md`. Explicitly EXCLUDED and deferred
  to `agent-protocol/state/deferred-backlog.md` rows D1–D7: pixel raster rendering, sprites/collision,
  command engine R#32–46, cycle-accurate access-slot/command timing + exact sub-frame IRQ position,
  YJK/YAE decode, visual scroll/interlace/blink/superimpose, G6/G7 planar interleave.
- Note: this file was reconstructed by the coordinator from the developer's RESP-M14-003 evidence to
  keep the `docs/mNN-implementation-report.md` set consistent; all numbers are from developer-run and
  are independently re-verified by QA in `docs/m14-qa-signoff.md`.

## VRAM migration outcome

`MemoryRegion vram_{kVramBytes}`, `machine::kVramBytes`, and the `vram()`/`vram_size()` accessors are
REMOVED from `Hbf1xvMachine`. VRAM now lives in `VdpVram` owned by the `V9958Vdp vdp_` member and is
reachable only via ports #98/#99 (+ the pre-existing S1985 #9C/#9D mirror) or, for debug/tests,
`machine.vdp().vram()`. No CPU-addressable VRAM region remains in the machine. `cold_boot()` calls
`vdp_.reset()` which zero-clears VRAM (matching the retired `vram_.clear()`), so the M10 debug-dump VRAM
golden is byte-unchanged.

## Files

New (`src/devices/video/`):
- `vdp_vram.{h,cpp}` — 128 KB flat VRAM store (`VdpVram`, `kVramBytes` authority), 17-bit addressing,
  size/clear/read/write/load/dump/data.
- `vdp_mode.h` — `VdpMode` enum + `decode_vdp_mode(reg0,reg1,reg25)` (M1–M5/YJK/YAE) + R#14-carry gate.
- `irq_line.h` — abstract level-held `IrqLine` sink (VDP owns the line; machine adapts to the CPU).
- `v9958_vdp.{h,cpp}` — the `core::IoDevice` (decodes `port & 0x03`): #98 VRAM data (shared read/write
  latch, read-ahead, auto-increment with R#14 carry + legacy 16 KB wrap), #99 two-write register/address
  protocol + status read + port-1 abort, #9A palette (9-bit GRB two-write), #9B indirect + AII, S#0..S#9,
  VBlank/line interrupts, V9938 boot palette, mode decode.

Changed:
- `src/machine/hbf1xv_machine.{h,cpp}` — vram_ migration; added `V9958Vdp vdp_` + nested `CpuIrqAdapter`;
  wired VDP on #98–#9B (mirror #9C–#9F pre-existing); `on_vsync()` per frame in `run_frame`; level-sample
  the VDP's held /INT in `step_cpu_instruction`; repointed debug dump VRAM block to `vdp_.vram()`.
- `src/devices/cpu/z80a_cpu.{h,cpp}` — thin `clear_maskable_interrupt()` pass-through; the M12 IM1 accept
  logic is UNCHANGED.
- `src/main.cpp` — `--vdp-parity` dump mode for the A/B harness. Root `CMakeLists.txt` — registered the
  two new .cpp files.

## Interrupt seam (risk R-1)

`service_maskable_interrupt()` clears the CPU pending flag on accept (`z80a_cpu.cpp:1769`; QA-verified), so the VDP
owns the level. The machine re-asserts from `vdp_.irq_active()` each step (assert-only; never clobbers an
externally-injected interrupt); the VDP releases via `set_irq(false)` on the S#0/S#1 read — a faithful
level-hold-until-status-read model. Integration test confirms exactly one accept and `/INT` released
after the S#0 read.

## Tests

New: 5 unit suites (`devices_vdp_vram_unit_test`, `devices_v9958_ports_unit_test`,
`devices_v9958_registers_unit_test`, `devices_v9958_palette_mode_unit_test`,
`devices_v9958_status_irq_unit_test`) + 1 integration (`machine_hbf1xv_vdp_io_integration_test`).
Total ctest: 56/56 passed, 0 failed. Build exit 0 (pre-existing C4819 code-page warnings only).

Prior suites updated (authentic, not weakened): `hbf1xv_memory_regions` unit + integration repointed VRAM
assertions from `machine.vram()`/`vram_size()` (removed) to `machine.vdp().vram()`; DRAM/SRAM assertions
unchanged. `hbf1xv_debug_dump` golden unchanged (VRAM still all-zero at boot).

## Evidence gates

- `tools/validate-assets.ps1` → result True (7 BIOS, 1 ROM).
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` → written, exit 0.
- `cmake --build build --config Debug` → success (0 errors).
- `ctest --test-dir build -C Debug --output-on-failure` → 56/56 passed.

## A/B parity

Genuine capture vs openMSX 19.1 (WSL) genuine `Sony_HB-F1XV` V9958 over `tests/parity/m14_vdp_probe.bin`
via `tools/openmsx-vdp-parity.ps1`. Result in `docs/m14-parity-trace-diff.md`: ARCHITECTURAL PARITY
(empty diff) — VRAM read-back [0..255] ramp byte-identical (now comparable; excluded in M13), VRAM pointer
0x0002 identical (read-ahead + auto-increment), R#0/1/7/14/17 identical, adversarial corruption at 0x0000
detected. BIOS-preset registers and frame-timing S#0 F / S#2 EO excluded from the gate (deferred D4;
verified in-emulator by deterministic tests).

## Boundary + assumptions

- Stayed strictly within the register/VRAM/status/interrupt contract; NO D1–D7. Flat VRAM store + linear
  CPU-port addressing (planar interleave deferred D7); interrupts at frame/line-count granularity
  (sub-frame position deferred D4).
- Assumption: control registers store raw written bytes (openMSX applies `controlValueMasks`). Not
  cross-compared for BIOS-preset registers; the explicitly-written gate registers use clean values that
  survive masking and matched openMSX exactly. Verification action: revisit register masking when the
  command-engine/rendering milestone needs exact readback (tracked with D3/D1).
- R#25 VDS bit stored only (never enables the Z80-clock-stop hazard) — matches fact-sheet §10.
