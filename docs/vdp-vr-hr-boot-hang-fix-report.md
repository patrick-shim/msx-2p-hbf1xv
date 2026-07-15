# VDP S#2 VR/HR Status-Bit Fix — Real BIOS Boot Hang (post-M28)

- Type: bug fix, discovered outside any approved milestone's scope, via direct human-requested
  interactive use of the SDL3 frontend (`sony_msx_sdl3.exe --bios-dir bios`, no cartridge, no disk).
- Discovered and fixed by: coordinator (this session), 2026-07-08, immediately following M28's
  developer-implementation-complete state (E2/C5/health-audit), before M28's QA dispatch.
- Files touched: `src/devices/video/v9958_vdp.{h,cpp}` (new), `src/machine/hbf1xv_machine.{h,cpp}`
  (additive wiring, alongside M28's own E2 `Ym2413Clock` addition in the same files),
  `tests/unit/devices/video_vdp_vr_hr_raster_status_unit_test.cpp` (new),
  `tests/integration/machine/hbf1xv_vdp_vr_hr_raster_integration_test.cpp` (new),
  `tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp` (one masked byte, see §4),
  `tests/CMakeLists.txt` (new test registrations).

---

## 1. The finding

A real, CPU-driven cold boot of the actual Sony HB-F1XV BIOS (`bios/f1xvbios.rom`), rendered live
through the SDL3 frontend with no cartridge or disk inserted, produced a **permanently solid black
screen** — not briefly, indefinitely (confirmed out to 179,000,000+ emulated CPU cycles, roughly
30x longer than real MSX hardware needs to reach its BASIC copyright screen).

Root-caused via direct instruction-level investigation (a temporary, non-committed diagnostic
driving a real `step_cpu_instruction()`/`on_vsync_boundary()` loop identical to
`Sdl3App::run_one_frame()`'s own shape, plus `debug_bus_read()` opcode inspection): the real BIOS's
early VDP-initialization routine polls VDP status register S#2 bit 6 (**VR**, "vblank area") via
the standard `OUT #99,<n>; OUT #99,0x8F; IN A,#99` status-select-and-read idiom, in a
wait-for-toggle loop, **before writing a single VDP register**. `V9958Vdp::peek_status_register(2)`
hardcoded VR (and bit 5, **HR**, "hblank area") to a fixed 0 — a disclosed, deliberate M23
simplification tied to backlog C1/D4 ("HR/VR... remain idle 0, correctly deferred to D4/M23").
Since VR/HR never toggle, the wait-for-toggle loop spins forever, and the CPU never reaches the
code that configures R#0-R#8 (mode, palette, screen enable) — hence the permanent black screen.

Confirmed via a targeted test: temporarily forcing VR to a constant 1 (instead of 0) reproduces the
identical hang, on the *opposite* edge of the same wait loop — proving the BIOS genuinely needs VR
to toggle, not merely read as some specific fixed value.

## 2. The fix

`V9958Vdp` gains an optional, attach-only `VdpClockSource` (X-pattern of the existing
`RtcClockSource`/`FdcClockSource`/`CassetteClockSource`/`RenshaTurboClockSource`/`Ym2413ClockSource`
adapters already in this codebase): `cpu_tstates_since_vsync()`. `Hbf1xvMachine::VdpRasterClock`
supplies it from the machine's own pre-existing `cycles_since_last_vsync()` formula
(`elapsed_cycles() - last_vsync_cycle_`, unchanged, already public since M23), attached in
`wire_bus()`. `peek_status_register(2)` now computes VR/HR live when a clock source is attached
(nullptr by default — zero behavior change for any caller that never attaches one, e.g. a bare unit
test constructing `V9958Vdp` directly):

- **VR** (bit 6): derived from the current line number relative to the vsync boundary. Grounded in
  `references/fact-sheets/Yamaha V9958 VDP.md` §7's own NTSC line/frame timing table (LN=0:
  13+26+192+25+3+3=262 lines; LN=1: 13+16+212+15+3+3=262) and the fact-sheet's own note that
  `on_vsync()` fires "at the start of the lower border" — i.e., the vsync-relative clock's origin
  (tstate 0) is the FIRST line of the bottom border, not the top of the frame. VR is set for the
  `262 - active_lines` non-active lines (70 for LN=0, 50 for LN=1) and clear for the active window.
  LN mode (R#9 bit 7) is read live, so the threshold tracks the current screen mode.
- **HR** (bit 5): derived from the current VDP-cycle position within the line, reusing M23's own
  `vdp_access_timing::vdp_cycle_within_line()` (already-shipped, non-gating foundation). Grounded in
  the SAME fact-sheet §7 per-line breakdown (`sync[0,100)+left-erase[100,202)+left-border[202,258)
  +DISPLAY[258,1282)+right-border[1282,1341)+right-erase[1341,1368)`) already used elsewhere in this
  project — HR is set outside `[258,1282)`.

Neither computation touches the ~340-entry per-slot access-timing tables that are the actual reason
C1/D4's *remainder* stays deferred (`references/openmsx-21.0/src/video/VDPAccessSlots.cc`, gated by
the standing `feedback_license_sensitive_scope.md` directive) — VR/HR are simple, independently
and already-documented raster-position booleans, not the license-sensitive slot-table remainder.
**This fix does not touch, weaken, or reopen C1/D4's own UNBUILDABLE-WITHOUT-FABRICATION ruling
(see `docs/m28-planner-package.md` §2.2) in any way.**

Consulted PULL-STYLE only, from `peek_status_register(2)` — never wired into
`step_cpu_instruction()`/`run_cycles()`/`run_frame()` — so it cannot perturb the M9/M12/M23
zero-tolerance CPU-timing oracles (confirmed: `git diff` shows zero touch to `src/devices/cpu/` or
`src/core/`).

## 3. Verification

- **Empirical, before/after**: a real BIOS boot that previously stayed R#0-R#8 = 0 (all zero,
  screen never configured) forever now configures the VDP within ~300 frames and shows the correct
  MSX blue background color (`debug/frames/diag-fixed-300.png`, viewed directly — a genuine, real
  screenshot of decoded VDP output, not fabricated).
- **New unit test** (`tests/unit/devices/video_vdp_vr_hr_raster_status_unit_test.cpp`, suite
  `Devices_VdpVrHrRasterStatus_Unit`, 9 cases): no-clock-source byte-for-byte regression guard; VR
  set at the vsync boundary; VR clear mid-active-display; VR genuinely toggles across the
  boundary (the exact property whose absence caused the hang); LN=0/LN=1 threshold dependency (a
  real, cross-checked difference, not a hardcoded constant); HR set/clear across the per-line
  display window; every other S#2 bit (TR/BD/CE/undocumented/EO) unaffected; determinism
  (repeated peek, same result).
- **New integration test**
  (`tests/integration/machine/hbf1xv_vdp_vr_hr_raster_integration_test.cpp`, suite
  `Machine_Hbf1xvVdpVrHrRaster_Integration`, 4 cases): confirms the REAL machine-level wiring (not
  just the isolated unit) via the real `#99`/R#15 status-read protocol a Z80 program would use;
  cross-checks against the machine's own independently-existing `cycles_since_last_vsync()`
  accessor (not the fix's own internals); two independent machines produce identical results
  (determinism); reading S#2 doesn't perturb CPU/clock state.
- **A genuine regression found and fixed**: `tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp`'s
  `DeviceIsolationInvariant_PsgVdpPpiRtcFdc_ByteForByteUnchanged` case failed on first full-suite
  run after this fix. Root cause: that test runs 10,000 real CPU instructions via a flat-RAM driver
  program that never calls `on_vsync_boundary()`, so `elapsed_cycles()` (and hence
  `cycles_since_last_vsync()`) genuinely advances between its "before" and "after" VDP-state
  snapshots — meaning S#2's now-live VR/HR bits MAY legitimately differ between the two snapshots,
  which is correct, intended behavior, not a cross-device-state-leakage violation (the CP/M harness
  still never issues a single VDP I/O instruction, independently verified by every OTHER bit,
  register, and all 128 KB of VRAM content in the same snapshot, unaffected). Fixed by masking VR/HR
  specifically (bits 0x60) out of that one test's S#2 snapshot comparison, with the exact reasoning
  recorded inline — every other byte in the 34 KB+ snapshot (all other status registers, all 32
  control registers, the command-engine register file, the full palette, the VRAM pointer, and all
  131,072 VRAM bytes) remains an exact, unmasked, byte-for-byte check.
- **Full regression**: `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` — **145/145
  passed** (fast subset, including both new VR/HR tests and the corrected M24 isolation test). The
  standing `feedback_slow_test_cadence.md` mechanical gate fires again for this change (an existing
  `src/devices/` file, `v9958_vdp.cpp`, was touched) — the full, unfiltered `ctest` (including the
  slow ZEXALL/ZEXDOC sweep) was re-run to completion; see `build/full_sweep_vrhr.log` for the raw
  output.

## 4. What this fix does NOT resolve (a second, deeper finding)

With this fix in place, a real BIOS boot proceeds substantially further (VDP genuinely configured,
correct background color shown) but does **not** yet reach the MSX-BASIC copyright screen/prompt.
A real openMSX 19.1 A/B instruction-level trace comparison (`tools/trace-diff.py`, the established
M10-M28 parity technique — 1,268,860 instructions compared, identical `Sony_HB-F1XV` machine, same
`bios/` assets) found the **first architectural divergence at instruction #145,503**, `PC=0x0455`
(inside the real BIOS's RAM-mapper segment/size self-test loop — the classic non-destructive
`LD A,(HL); CPL; LD (HL),A; CP (HL); CPL; LD (HL),A` probe): control flow (PC, all 16-bit register
pairs, flags, IFF1/IFF2/IM) matches openMSX exactly up to and including this instruction, but
reading logical address `0x8001` (page 2, memory-mapper segment 1 per port `#FE`, folding to
physical RAM offset `0x4001`) returns `0xFF` in this emulator versus `0x00` on real openMSX — a
genuine, reproducible, single-byte content mismatch, not a control-flow bug. The two emulators'
architectural traces are byte-identical everywhere *before* this read, so the divergence must trace
back to *when/how* that specific physical RAM location was first written earlier in the boot
sequence — a materially larger trace-back than this fix's own scope (per the human's explicit
direction, this is being documented here as an open finding, NOT investigated further in this
cycle). Downstream of this content mismatch, the CPU eventually (~2.8-2.9M instructions) ends up
executing garbage bytes fetched from an unintended memory region, corrupting the stack via a
repeating `RST 38`, and settles into a permanent `HALT` with interrupts disabled (`IFF1=0`, no `EI`
ever re-executed, no NMI in this system) — a genuine dead end, distinct from and downstream of this
report's own VR/HR fix.

**Recommendation for whichever future milestone picks this up**: start from the exact divergence
point above (instruction #145,503, physical RAM offset `0x4001`) and trace the memory-mapper
segment-register (`#FC-#FF`) write history from cold boot forward on both sides to find where the
two emulators' segment-assignment timelines first differ. This is NOT related to backlog C1/D4 (no
license-sensitive data involved) — it is a memory-mapper/RAM-segment-history question, likely
scoped as its own investigation similar in shape to C5's own auto-boot-trigger work.

> **CORRECTION (added post-hoc, 2026-07-08, per the dedicated follow-up investigation
> `docs/memory-mapper-segment-divergence-investigation.md`; original paragraph above kept intact,
> not rewritten, for audit trail):** the recommendation above was followed, and the underlying
> "physical RAM content mismatch" claim in this paragraph is **SUPERSEDED — it was a false
> positive**, not a genuine emulator defect. The dedicated investigation independently reproduced
> `A=0xFF` at instruction #145,503 on this emulator, then found real openMSX's mapper segment for
> page 2 is `1` on BOTH sides (disconfirming any segment-selection divergence), and finally traced
> the root cause to the openMSX-side comparison technique itself: the live-Tcl harness's `reset`
> command (correctly, per real MSX/Z80 hardware semantics — see
> `references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:47-52` vs. `:41-45`) does **not**
> clear RAM content (only a genuine power-cycle does), so the openMSX-side `0x00` reading was
> leftover contamination from the harness's own uncontrolled multi-second pre-script free-run, not
> a deterministic post-reset value. Using a genuine power-cycle (`set power off`/`set power on`)
> before the reset, real openMSX reads **`A=0xFF`** at instruction #145,503 — an EXACT match with
> this emulator — and the two emulators' physical-memory write histories remain byte-identical for
> a further 300,000-instruction window beyond it. **No code defect exists in
> `src/devices/memory/memory_mapper_ram.cpp`, `src/devices/chipset/mapper_io.cpp`, or their
> wiring; no code change was made.** The separate "downstream ~2.8-2.9M-instruction dead end"
> observation two sentences above this correction was framed as following from the now-refuted
> content mismatch; its own status (genuine divergence vs. artifact, and whether it was ever
> independently A/B-checked against openMSX at all) is consequently now an **open question**, not
> confirmed either way — see the dedicated investigation's §5 for the honest, unresolved framing.
> Backlog/ledger disposition below is intentionally left as originally recorded; updating
> `agent-protocol/channels/decisions.md` / `current-phase.md` / `deferred-backlog.md`'s
> cross-references to this now-superseded finding is a coordinator-owned follow-up.

## 5. Ledger disposition

This fix is NOT part of M28's approved scope (E2, C5, health audit) — it is a self-contained bug
fix to an already-shipped M14 device, discovered via direct interactive use requested by the human
outside the milestone protocol. Recorded via a dedicated decision entry
(`agent-protocol/channels/decisions.md`) rather than folded into M28's own acceptance criteria.
Backlog row C1/D4 gains a factual, non-status-changing cross-reference note (its own "HR/VR remain
idle 0" disclosure is now superseded by this fix; the row's actual remainder — the license-sensitive
slot tables — is completely unaffected and unchanged). The §4 finding above is NOT added as a new
backlog row this cycle (per the human's explicit direction to stop investigating further for now);
it is fully documented here for whichever future cycle picks it up.
