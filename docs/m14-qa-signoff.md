# M14 QA Sign-off — Yamaha V9958 VDP (register/VRAM/status/interrupt contract)

- Milestone: M14 (Yamaha V9958 VDP incl. 128 KB VRAM ownership). Request: REQ-M14-004.
- Author: MSX QA Agent. Date: 2026-07-06.
- Gate: M14 retains the NORMAL human-release-decision gate (DEC-0006). A QA Pass authorizes the
  coordinator to PRESENT M14 to the human; it does NOT auto-close the milestone.
- Method: all evidence below was independently re-run or re-read by QA. Developer-reported numbers
  were not trusted; they were reproduced. Behavior expectations are grounded in
  `references/fact-sheets/Yamaha V9958 VDP.md` and cross-checked against openMSX
  `references/openmsx-21.0/src/video/VDP.cc` (behavior reference only).

---

## 1. Regression Scope

Impacted surface assessed:

- New device subsystem `src/devices/video/` — `vdp_vram.{h,cpp}`, `vdp_mode.h`, `irq_line.h`,
  `v9958_vdp.{h,cpp}`.
- Machine composition `src/machine/hbf1xv_machine.{h,cpp}` — VRAM-ownership migration, VDP wiring on
  the M11 IoBus (`#98-#9B` + S1985 `#9C-#9F` mirror), per-frame VBlank delivery, /INT level-sample,
  debug-dump repoint.
- CPU seam `src/devices/cpu/z80a_cpu.{h,cpp}` — a thin `clear_maskable_interrupt()` pass-through
  (M12 IM1 accept path must be UNCHANGED).
- Prior-suite regression risk: the memory-region unit + integration suites repointed from the
  retired `machine.vram()` to `machine.vdp().vram()`; the M10/M13 debug-dump golden.
- A/B parity: VRAM read-back now cross-comparable (it was excluded in M13).
- Out-of-scope boundary: D1–D7 (rendering/sprites/command/slot-timing/YJK/planar) must NOT be
  implemented; backlog rows must be present.

---

## 2. Regression Matrix Status

### 2.1 QA-executed evidence gates

| Gate | QA result |
|------|-----------|
| `cmake --build build --config Debug` (SDL3 OFF) | SUCCESS, exit 0 (pre-existing C4819 code-page warnings only) |
| `ctest --test-dir build -C Debug --output-on-failure` | **56/56 passed, 0 failed** (0.90 s) — matches developer claim |
| `tools/openmsx-vdp-parity.ps1` (QA re-run vs genuine openMSX Sony_HB-F1XV V9958 on WSL) | **ARCHITECTURAL PARITY (empty diff)**, 16 openMSX VRAM lines captured, adversarial check PASS |

The five new VDP unit suites (`devices_vdp_vram_unit_test`, `devices_v9958_ports_unit_test`,
`devices_v9958_registers_unit_test`, `devices_v9958_palette_mode_unit_test`,
`devices_v9958_status_irq_unit_test`) and the integration suite
`machine_hbf1xv_vdp_io_integration_test` (tests 51–56) all passed under QA execution; the prior
M0–M13 suites (tests 1–50) remained green.

### 2.2 Per-criterion verification (source-grounded, non-stub)

| Acceptance criterion | Status | Evidence (QA-verified) |
|----------------------|--------|------------------------|
| VDP owns 128 KB VRAM; no CPU-addressable VRAM region remains | PASS | `MemoryRegion vram_`, `machine::kVramBytes`, `vram()/vram_size()` are GONE from `hbf1xv_machine.h` (members are only `dram_`, `sram_`, `vdp_` at .h:204/205/236; comment .h:108-112 documents the migration). VRAM authority is `VdpVram::kVramBytes = 128*1024` (`vdp_vram.h:33`). Debug dump repointed to `vdp_.vram()` (`hbf1xv_machine.cpp:424`). Integration case `Vram_SeparateFromDram_NoCpuAddressableWindow` proves a VRAM write leaves DRAM at the same low address untouched. |
| #98 data path: 17-bit `(R#14<<14)\|ptr`, auto-increment + R#14 carry, legacy wrap, read-ahead, shared latch | PASS | `effective_address()` = `((R#14<<14)\|vram_pointer_) & 0x1FFFF` (`v9958_vdp.cpp:98-101`); `advance_vram_pointer()` wraps 14-bit ptr, carries into R#14 only when `is_v9938_mode()` (`.cpp:103-111`); `vram_data_write` sets the shared `cpu_vram_data_` latch then writes (`.cpp:113-119`); `vram_data_read` returns the read-ahead buffer then refills (`.cpp:121-129`). |
| #99 two-write register/address + direction bit + R#0-27; #9B indirect + AII | PASS | `port1_write` two-write latch: `value&0x80` → `change_register(value&0x3F, latch)`, else address setup with bit6 write-access and a read-ahead prefetch when bit6=0 (`.cpp:133-153`); `indirect_write` uses R#17, `change_register(R#17&0x3F,...)`, AII-gated auto-increment (`.cpp:201-211`). A `#98/#99` read aborts a half-written latch (`register_data_stored_=false`, `.cpp:65`). |
| #9A palette 9-bit GRB two-write; mode decode | PASS | `palette_write` masks `((value<<8)\|latch)&0x777`, R#16 pointer auto-increments (`.cpp:183-197`); `decode_vdp_mode` maps the M5..M1 base to the Target-Spec mode set + YJK/YAE overlay on GRAPHIC7 (`vdp_mode.h:66-95`); no 19,268-sized structure exists (palette is 16 entries). |
| S#0..S#9 (S#1 resets 0x04, ID#=2); VBlank/line interrupts | PASS | Reset S#0=0x00, S#1=0x04, S#2=0x0C (`.cpp:33-46`, constants `.h:40/42`); `peek_status_register` models S#1 ID=2 with dead LPS/FL=0, S#2 undoc bits 3/2=1 + EO toggle, S#3–S#9 documented idle masks (`.cpp:294-328`); read-side flag resets on S#0 (F + vertical line) and S#1 (FH when IE1) (`.cpp:155-179`). |
| System integration onto M11 IoBus; mirror equivalence; prior suites green | PASS | VDP attached `#98-#9B` (`hbf1xv_machine.cpp:88-91`); mirror `#9C-#9F` routes to the same device (integration case `Mirror9C9D_RouteToSameVdp_ReadBackRamp`); `io_read/io_write` decode `port & 0x03`. |
| A/B trace-diff (real capture, VRAM read-back included) | PASS | QA reproduced; see §2.3. |

### 2.3 Interrupt-seam verification (critical, R-1)

The V9958 /INT is LEVEL-held and cleared on the status read, wired to the M12 IM1 accept path via
the `IrqLine` adapter WITHOUT touching M12 accept logic:

- `service_maskable_interrupt()` is the M12 code unchanged — it clears the CPU pending flag on
  accept (`z80a_cpu.cpp:1769`, inside the function starting at :1765), sets IFF1/IFF2 false, ticks
  refresh, and dispatches by interrupt mode. QA diffed the accept path (`z80a_cpu.cpp:56-57`
  acceptance test and the `service_maskable_interrupt` body): no M14 change.
- The only M14 CPU addition is `clear_maskable_interrupt()` (`z80a_cpu.cpp:147-152`) — a thin
  `state_.clear_maskable_interrupt()` pass-through, no acceptance logic.
- The machine `CpuIrqAdapter::set_irq()` forwards assert→`request_maskable_interrupt()`,
  release→`clear_maskable_interrupt()` (`hbf1xv_machine.cpp:17-26`). The VDP owns the level; the
  machine re-asserts from `vdp_.irq_active()` each `step_cpu_instruction` (ASSERT-ONLY, so an
  externally injected interrupt is never clobbered; `hbf1xv_machine.cpp:257-259`). The VDP releases
  via `set_irq(false)` on the S#0/S#1 read.
- Single-accept / level-clear confirmed end-to-end by `machine_hbf1xv_vdp_io_integration_test`:
  after a VBlank with IE0 and 40 CPU steps, the ISR acceptance counter is exactly 1
  (`Interrupt_AcceptedExactlyOnce`), the ISR observed S#0=0x80 (`Isr_ReadS0_ObservedFSet`), S#0 then
  read back 0x00 (`Isr_ReadS0_ClearedF`), and `/INT` released (`Interrupt_ReleasedAfterS0Read`).
  No re-trigger loop. The `devices_v9958_status_irq_unit_test` further proves the wired-OR
  (vertical OR horizontal) hold: reading S#0 clears vertical while horizontal still holds the line,
  and reading S#1 releases it.

### 2.4 Two hand-checks against the fact sheet

**Hand-check A — #98 auto-increment across an R#14 boundary.** Fact-sheet §2 line 40: "Auto-increment
on each port-#0 access; R#14 auto-increments on carry from A13 — except in G1/G2/MC/T1 modes … in
new modes it counts full 128 KB." Deriving from the code (`advance_vram_pointer` +
`effective_address`) with R#14=0 in a V9938 mode: ptr 0x3FFE→addr 0x3FFE; access→ptr 0x3FFF (addr
0x3FFF); access→ptr wraps 0x0000 and R#14→1, addr = `(1<<14)|0 = 0x4000`. The sequence crosses the
16 KB page boundary 0x3FFF→0x4000 into the next bank — exactly the fact-sheet's full-128 KB counting.
In a legacy mode (`is_v9938_mode()` false) R#14 does NOT increment, so the address wraps within the
16 KB bank (TMS9918 compat). Both branches present and correct.

**Hand-check B — S#0 after a VBlank with IE0=1, then after an S#0 read.** Fact-sheet lines 91/99/126:
S#0 bit7 F = VBlank flag, cleared on read; VBlank enabled by R#1 IE0. Deriving: `on_vsync()` sets
`status_reg0_ |= 0x80` and, because IE0 (R#1 bit5=0x20) is set, `irq_vertical_=true` and `update_irq`
asserts /INT. First S#0 read → returns 0x80 (peek), then `status_reg0_ &= ~0x80`, `irq_vertical_=false`,
/INT released. Subsequent S#0 read → 0x00. This matches the fact sheet and is confirmed by both the
unit test (`VBlank_ReadS0_ReturnsF`/`ClearsF`/`ReleasesInt`) and the integration ISR.

### 2.5 Regression audit of updated tests (authentic, not weakened)

- `hbf1xv_memory_regions_unit_test` (and its integration sibling): the VRAM assertions were
  repointed from the retired `machine.vram()/vram_size()` to `machine.vdp().vram()`. This is an
  AUTHENTIC repoint — the suite retains the full rigor: exact 128 KB size, cold-boot zero-init
  (`ColdBoot_Vram_ZeroInitialized`), boundary + interior read/write round-trips, region independence
  (DRAM A-5 pattern and SRAM unaffected), and a load→dump→reload byte-identity round-trip. DRAM/SRAM
  assertions are unchanged. No assertion was deleted or relaxed.
- `hbf1xv_debug_dump` golden: VRAM is still all-zero at boot because `vdp_.reset()` zero-clears VRAM
  (`v9958_vdp.cpp:22`, matching the retired `vram_.clear()`), and the dump reads
  `vdp_.vram().data()/size()` (`hbf1xv_machine.cpp:424`). Golden byte-unchanged (R-3 satisfied).

### 2.6 A/B adversarial validation (QA-reproduced, incl. VRAM read-back)

QA re-ran `tools/openmsx-vdp-parity.ps1` end-to-end against genuine openMSX on WSL
(`/usr/bin/openmsx`, machine `Sony_HB-F1XV`, real V9958) over `tests/parity/m14_vdp_probe.bin`
(QA artifacts: `build/qa_m14_vdp_A.txt`, `build/qa_m14_vdp_B.txt`, `build/qa_m14_vdp_diff.md`):

- Result: **ARCHITECTURAL PARITY (empty diff)**. Exit 0.
- VRAM read-back IS genuinely compared (NOT skipped as in M13): openMSX returned a real ramp from
  its `physical VRAM` debuggable — `OMVRAM 0000 00 01 02 03 …`, `OMVRAM 0010 10 11 …` (16 lines =
  256 bytes), byte-identical to the emulator's `--vdp-parity` dump. The gate covers VRAM[0..255].
- Gate registers matched: R#7=0x2A (indirect write via #9B), R#17=0x08 (post-AII pointer),
  R#0/R#1/R#14 = 00; VRAM pointer A=0002 / B=0002 (read-ahead + auto-increment).
- Adversarial comparator check: PASS — corrupting the emulator's byte at offset 0000 (00→01)
  produced a DIVERGENCE against openMSX, proving the comparator can actually report a mismatch (not
  a vacuous empty diff).
- Correctly-excluded fields: BIOS-preset registers (not program-written) and frame-timing S#0 F /
  S#2 EO (deferred D4, verified in-emulator by the deterministic tests). This exclusion is
  legitimate and documented in the harness header.

### 2.7 Boundary + backlog presence check (D1–D7)

- No out-of-scope depth implemented: a grep of `src/devices/video/` for render/framebuffer/sprite/
  command-engine/pixel/scanline/YJK-decode/planar found ONLY deferral comments — no implementation.
  The store is a flat 128 KB buffer (no planar interleave, D7 deferred); interrupts are at
  frame/line-count granularity (sub-frame position D4 deferred); mode decode is bit-level only (no
  pixels, D1/D5/D6 deferred).
- `agent-protocol/state/deferred-backlog.md`: rows D1–D7 present (section C, all OPEN, each grounded
  in `references/`), and B9 (VRAM/V9958 VDP) marked **IN-PROGRESS (M14)**. Correct.

### 2.8 Process-integrity / ledger-consistency check

Cross-checked `state/milestones.md`, `state/definition-of-done.yaml`, `channels/decisions.md`,
`channels/requests.md`, `channels/responses.md`, and `state/deferred-backlog.md` for M14:

- Milestone numbering is consistent everywhere: M12 = Z80A CPU parity, M13 = RAM/ROM memory,
  M14 = V9958 VDP (DEC-0003 renumber honored in all artifacts).
- DEC-0002 (VRAM-owned-by-VDP), DEC-0003 (renumber + M12 insert), DEC-0005 (backlog registry),
  DEC-0006 (M13 close + M14 normal gate) are all present and mutually consistent.
- REQ/RESP-M14-002 (planner), -003 (developer), -004 (QA) present and correctly threaded.
- `definition-of-done.yaml` M14 = `regression_qa` with all regression_qa flags false (correct
  pre-sign-off state); `current-phase.md` = M14 Regression/QA. Consistent.
- No dropped follow-up: B9→M14, D1–D7 recorded; earlier deferrals (B1–B8, C1–C9) untouched.

**Drift found (non-blocking, for coordinator to fix):**

1. `agent-protocol/state/milestones.md` M14 **Status: Planned** (line 320) is STALE — implementation
   is complete and QA is running; it should read "Ready for QA"/"In Progress". Every other artifact
   reflects the real state; only this line lags.
2. Minor line-number drift in `docs/m14-implementation-report.md` and RESP-M14-003: they cite the
   CPU pending-clear at `z80a_cpu.cpp:1762`; the actual `state_.clear_maskable_interrupt()` is at
   `:1769` (function starts at `:1765`). The behavior is correct and present; only the citation is
   off by ~7 lines.

Neither drift affects code correctness or the acceptance criteria.

---

## 3. Failures and Risk Ranking

No blocker or major failures found. Residual risks (all accepted/tracked, none blocking):

| Sev | Item | Disposition |
|-----|------|-------------|
| Low | D1–D7 rendering/sprite/command/slot-timing/YJK/planar depth deferred | Correct milestone boundary; committed scope, tracked in backlog, grounded in `references/`. Not required by M14 acceptance. |
| Low | Interrupts modeled at frame/line-count granularity; exact sub-frame /INT position deferred (D4) | Accepted; A/B checkpoints are frame-aligned so the diff is deterministic. Verified in-emulator. |
| Low | Control registers store raw bytes; openMSX `controlValueMasks` not applied | Assumption; the gate registers use clean values that survive masking and matched openMSX exactly. Revisit at the command-engine/rendering milestone (D1/D3). |
| Info | `milestones.md` M14 Status still "Planned" (stale) | Coordinator to update on transition. |
| Info | Impl-report/RESP line-number citation off by ~7 (`:1762` vs `:1769`) | Documentation nit; behavior correct. |

---

## 4. Required Fixes

None required for a Pass. Recommended (non-blocking) housekeeping for the coordinator:

1. Update `agent-protocol/state/milestones.md` M14 Status from "Planned" to reflect the QA phase
   (and to "Done" on the human release decision).
2. Optionally correct the `z80a_cpu.cpp:1762`→`:1769` citation in the M14 implementation report /
   RESP-M14-003 on the next append.

---

## 5. Sign-off Decision

**PASS.**

Rationale: QA independently reproduced ctest 56/56 (0 failed) and the openMSX A/B (empty diff with a
genuine VRAM read-back and a passing adversarial comparator check). Every M14 acceptance criterion is
verified as genuine, non-stub source: the 128 KB VRAM is owned by the VDP with no CPU-addressable
VRAM region remaining; the #98/#99/#9A/#9B port contract, 17-bit addressing with R#14 carry,
read-ahead/shared latch, palette, indirect+AII, and S#0..S#9 are correct and fact-sheet-grounded; the
/INT is level-held and cleared on the status read, wired to the UNCHANGED M12 IM1 accept path via a
thin pass-through, with exactly one accept and no re-trigger loop. The updated memory-region tests are
authentic repoints (not weakened) and the debug-dump golden is unchanged. The D1–D7 boundary was
respected (no rendering/sprite/command/timing depth implemented) and the backlog records them plus
B9=IN-PROGRESS(M14). The two ledger drifts found are non-blocking status/citation lag, not
correctness or gate issues.

Per DEC-0006, M14 retains the normal human-release-decision gate: this Pass authorizes the
coordinator to present M14 to the human for the release decision (and tag), not to auto-close.
