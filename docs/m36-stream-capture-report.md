# M36 Live Stream-Capture (DEC-0052) — Implementation Report

**Date**: 2026-07-10
**Feature**: F10 live stream-capture — a debug tool to capture the temporal trajectory into the YS II building-entry CRASH (a single snapshot shows only the end state).
**Status**: COMPLETE + verified. Uncommitted (coordinator owns closure).

> Authored by the MSX Developer Agent (opus); persisted by the coordinator (developer under a
> no-report-file constraint), with the coordinator verification addendum in §7.

## 1. What it does
- **F10** in the SDL3 frontend TOGGLES stream-capture ON/OFF (host hotkey, same discipline as F11=disk-swap / F12=snapshot; never routed to the input mapper; F10 confirmed otherwise unbound).
- While ON: a **full snapshot every frame** into `debug/snapshot/stream_<id>/f<frame>_c<cycle>/` (the frame-by-frame state evolution).
- A bounded in-memory **per-instruction CPU-trace ring** (`kStreamTraceRingCapacity = 1<<20` = 1,048,576 entries ≈ ~2.8 s of execution) of `PC/opcode/regs/SP + #A8 slot`.
- **AUTO-STOP + finalize on HALT**: the instant the CPU enters HALT (the crash), it dumps the ring to `debug/traces/stream_<id>_trace.txt` (oldest→newest) + a final `stream_<id>/HALT_<frame-id>/` snapshot, then disarms. (Also finalizes on a manual F10-off.)

## 2. Design / reuse
Machine-level mechanism (testable without SDL); the frontend only owns the F10 toggle. `step_cpu_instruction()` captures the per-instruction record (reusing the already-fetched opcode + non-perturbing `debug_io_read(0xA8)`) into the ring and detects the `!halted→halted` transition; `on_vsync_boundary()` writes the per-frame bundle. Reuses `debug_snapshot` (Phase 3), the M10/M27 `Z80aTraceRecord` + `CpuTraceSink::format_record`, the F11/F12 hotkey pattern. New machine API: `set_stream_capture_enabled/stream_capture_active/stream_capture_id/serialize_stream_trace`; SDL seam `request_stream_toggle()`. `StreamTraceEntry` wraps the M27 record + `#A8` alongside (adding the slot to the CPU-core record would touch `src/devices/cpu/` — deliberately avoided).

## 3. Evidence
- New `tests/system/hbf1xv_m36_stream_capture_system_test.cpp` — Passed: auto-stop-on-halt, ≥1 per-frame bundle, HALT bundle written, trace file non-empty + matches ring, ring holds all instructions, newest record is HALT, ring carries #A8, determinism (same id / ring / file set / byte-identical tree across two armed runs), default-off (empty ring, never armed, no dirs, CPU still executed).
- Full suite: build exit 0; `ctest -E hbf1xv_m24_zexall_system_test` → **206/206**, ZEXALL withheld.
- `git status --porcelain src/devices/cpu src/core` → empty (zero cpu/core edits). Default-off no-op proven (every hook behind `if (stream_active_)`; CPU golden trace tests unchanged).

## 4. Known limits
- Per-frame bundles are heavy (~hundreds of KB/frame text) — a deliberately heavy mode for a short toggled window, not steady-state.
- Ring records the first opcode byte only (guarantees non-perturbation by reusing the baseline fetch); registers/PC/SP/#A8 are exact.
- No SDL3-gated F10 integration test (frontend is a thin wrapper proven end-to-end at machine level; `request_stream_toggle()` seam is in place).

## 5. Human usage
Run the SDL3 build, play to the building, press **F10** to arm as you cross the threshold; the capture finalizes itself on the freeze. Collect `debug/traces/stream_<id>_trace.txt` (instruction trajectory ending at the derail) + `debug/snapshot/stream_<id>/HALT_<frame-id>/`.

## 6. Files
`src/machine/hbf1xv_machine.{h,cpp}` (stream API + hooks), `src/frontend/sdl3_app.{h,cpp}` (F10 + seam), `tests/system/hbf1xv_m36_stream_capture_system_test.cpp` (+ `tests/CMakeLists.txt`), `.gitignore`.

## 7. Coordinator Independent Verification Addendum (2026-07-10)
- `git status --porcelain src/devices/cpu src/core` = EMPTY — zero cpu/core edits confirmed.
- `hbf1xv_m36_stream_capture_system_test` re-run green from `build/`.
- `build/Debug/sony_msx_sdl3.exe` (10:57:43) is newer than the modified `hbf1xv_machine.cpp` / `sdl3_app.cpp` — the F10 feature is compiled into the binary the human will run.
- Ready for the human's F10 capture of the Bug B crash trajectory.
