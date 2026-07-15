# M44 Phase-2a Planner Package — DEF-M44-CMDSYNC Phase 2a (V9958 command-engine `CE`/S#2-bit0 busy-wait DURATION)

- Decision of record: **DEC-0069** (`agent-protocol/channels/decisions.md:823-828`) — M44 Phase 2
  RESUMED; Phase 2a = model the command-engine `CE`/S#2-bit0 busy-wait duration to pace Laydock 2's
  blit loop. Scoped originally by **DEC-0065** (phased), deferred by **DEC-0066**.
- Authoritative resumable plan: `docs/m44-phase2-plan.md` (§Phase 2a). Phase-1 design +
  foundation: `docs/m44-planner-package.md` + committed `a27a1c4`.
- Milestone: **M44 (Phase 2a)**, release bundles into the next release after v1.1.1
  (DEC-0069 / DEC-0068). Owner-run tag+push per DEC-0047.
- Spec owner: MSX Planner. Developer owner: MSX Developer. QA owner: MSX QA.
- Scope class: **VDP command-timing accuracy** — `src/devices/video` only (command engine +
  VDP S#2 read) + the machine's `VdpClockSource` adapter. **NO `src/core`, NO `src/devices/cpu`**
  (the mechanical ZEXALL per-edit trigger does not fire; confirm the empty diff at every gate).
- Discipline: planner-first + **system-wide-review-first**
  (`feedback_system_wide_review_first`) + **universal-fixes-only**
  (`feedback_universal_fixes_only`). This is a generic V9958 CE-duration model, not a
  Laydock-specific patch. License isolation: openMSX `VDPCmdEngine` is a behavior reference
  only; **the ~340-entry access-slot position tables must NOT be transcribed**
  (`feedback_license_sensitive_scope`); Phase 2a uses a coarse re-derived per-command cost model.

This package is **spec + mechanism + calibration + acceptance only**. No production code is
written or modified here.

---

## 1. Scope and Assumptions

### 1.1 In scope (Phase 2a — the CE-pacing fix)

Give each VDP **command** a real reported busy DURATION so the `CE` bit (S#2 bit 0) stays set for
the command's estimated emulated length, pacing software that busy-waits on `CE`. Concretely:

- A **per-command duration estimate** (T-states) computed at command issue from the clipped
  `NX*NY` (rectangle ops), length (LINE), or searched distance (SRCH), times a per-command
  per-mode access-slot cost, grounded in openMSX `VDPCmdEngine::calcFinishTime`
  (`references/openmsx-21.0/src/video/VDPCmdEngine.cc:733-747`) — re-derived, NOT transcribed
  (§2.3).
- A **`CE`-busy mechanism**: a `cmd_busy_until` absolute-cycle timestamp; the S#2-bit0 status read
  reports `CE=1` until `now >= cmd_busy_until`, wired from the command engine's computed duration
  into `V9958Vdp::peek_status_register(2)` (§2.4).
- The command still computes its final VRAM in a **BURST** (unchanged). Phase-1's per-row
  render-sync (`a27a1c4`) handles the ahead-of-beam observation timing; Phase 2a changes ONLY the
  *reported duration*. The two compose without disturbing each other (§2.6).
- **Calibration** of the duration estimate against the reused Laydock 2 `.omr` A/B until the HUD
  is crisp/stable with no visible slowdown (§3).

### 1.2 Out of scope (Phase 2b — deferred, separate milestone; DEC-0065/0069)

- **Incremental** `execute*(EmuTime limit)` command execution (writing one access slot at a time,
  each write timestamped) — the full cycle-accurate write model.
- The exact **per-mode access-slot position table** and `stealAccessSlot` CPU-VRAM contention
  during a command (bit-parity command timing). Phase 2a uses a coarse linear cost, not the slot
  table.
- Any Z80 T-state / `src/core` scheduler / `src/devices/cpu` change.

### 1.3 Non-goals

- No new game-specific code paths; no Laydock detection. The duration keys only on generic
  command state (command code, mode, clipped NX/NY, direction).
- No change to the *VRAM result* of any command (the burst is byte-for-byte identical). No
  weakening of any M22 command-result oracle or M32/M41 render oracle.
- No change to the Phase-1 render-sync seam behavior; no `src/core`/`cpu`/`peripherals`/frontend
  behavior change.

### 1.4 Assumptions (each carries a verification action)

- **A-1 (central):** A deterministic, approximately-hardware-accurate CE duration paces Laydock
  2's busy-wait-on-`CE` blit loop so its subsequent beam-sensitive register writes land at a
  stable beam phase → the HUD stops jittering, matching openMSX. **Verify:** the reused
  `debug/laydock2-flicker/` + `debug/m44-laydock/` `.omr` A/B (developer captures OURS vs
  `omsx/oms_0xx`; the human's live confirmation is the authoritative gate per DEC-0069).
- **A-2:** Laydock's HUD-composite loop busy-waits on **S#2 bit0 (CE)** of an **atomic** command
  (LMMV/LMMM/HMMV/HMMM/YMMM), not an event-driven transfer. Grounded: the Phase-1 instrumentation
  (`debug/m44-laydock/diag2.log`, DEC-0066) proved the loop is unpaced because our CE clears in 0
  cycles; event-driven transfers (LMMC/HMMC/LMCM) already hold `CE` across their multi-step
  handshake (`vdp_command_engine.cpp:126-127,202-219`) and are correctly paced today. **Verify:**
  developer confirms via a command-trace of the repro that the paced loop issues atomic
  fills/copies (the snapshot `debug/laydock2-flicker/.../vdp.txt` = GRAPHIC4, R#23=0xA4).
- **A-3:** An **absolute** monotonic CPU-cycle timebase is available to the VDP without touching
  `src/core`. Grounded: `Hbf1xvMachine::VdpRasterClock::cpu_tstates_since_vsync()` is literally
  `scheduler_.total_cycles() - last_vsync_cycle_` (`hbf1xv_machine.h:857-859`) — the monotonic
  `scheduler_.total_cycles()` exists underneath; only the exposed accessor subtracts the vsync
  epoch. Exposing the absolute value is an additive `VdpClockSource` method implemented in the
  machine adapter (§2.4.1), NOT a `core::Scheduler` edit. **Verify:** the `git diff` shows only
  `src/devices/video` + the one `src/machine` adapter method; `src/core` empty.
- **A-4:** The openMSX-grounded naive underestimate (`calcFinishTime`, which "assumes we never
  have to wait for access slots and no per-line overhead", `VDPCmdEngine.cc:741-746`) is a
  **lower bound** on the real slot-limited duration; Laydock runs its commands during **active
  display** (raster 109–181, DEC-0066) where the real chip is slot-starved and commands run
  *slower*, so the underestimate likely **under-paces** and a calibrated upward correction is
  expected (§2.3.3, §3). **Verify:** the `.omr` A/B calibration ladder (§3.3) — if the base
  estimate leaves residual jitter, apply the single calibrated correction constant and re-A/B.
- **A-5:** No existing oracle asserts atomic-command `CE==0` via the S#2 status-register path with
  the busy window active. Grounded audit (§2.7): the M22 atomic unit oracle reads
  `cmd_engine().ce()` **directly** (engine-level status bit, left unchanged), and every
  peek-S#2 `CE` assertion in the M22 integration/system tests is for an **event-driven** transfer
  (LMMC/HMMC), whose `CE` is unchanged. **Verify:** developer re-greps the full suite before
  implementing and after (AC-6/AC-7); any newly-found atomic+peek-S#2==0 assertion escalates to
  the Q2 decision path (§7).

---

## 2. Spec Summary

### 2.1 Confirmed problem (recap — NOT re-investigated; DEC-0066 + `docs/m44-phase2-plan.md`)

Our command engine sets `CE` (S#2 bit0) at command start and clears it in `command_done()`
**within the same `R#46` write** for the 7 atomic commands
(`vdp_command_engine.cpp:291,314-317`) — a **zero-cycle** reported duration. Laydock 2's HUD
composite loop busy-waits on `CE`; with 0-duration commands the loop is **UNPACED**, so its
subsequent register/blit writes (including the R#23 split scroll that positions the HUD) land at a
**drifting beam phase** frame-to-frame → the io_write-seam-frozen HUD **jitters** (the flicker).
Phase-1 observation timing cannot reach it: the commands run at raster 109–181 (below the HUD
display lines 0–47), so 15,819/16,001 HUD-line commits are already-swept and wrap-guard-skipped
(`debug/m44-laydock/diag2.log`). **The fix is to give commands a real reported duration so the
busy-wait loop is paced deterministically** — which pins the beam phase and stops the jitter.

openMSX ground truth: `startLmmv`/`startHmmv`/… call `calcFinishTime(nx, ny, ticksPerPixel)`
(`VDPCmdEngine.cc:980,1105,1365,1470,1606`), which sets `statusChangeTime = engineTime +
VDPClock::duration(ticksPerPixel) * ((nx*(ny-1)) + (ANX-1))` (`:733-747`); `peekStatus2(time)`
returns `CE` set until `time >= statusChangeTime` (`VDPCmdEngine.hh:76-81`). A new command,
ABRT/`commandDone`, or mode-loss resets `statusChangeTime` (`:753`, `commandDone`).

### 2.2 Why "approximately right" duration fixes the flicker (the "why it works" contract)

The flicker is **frame-to-frame jitter**, not a static artifact. Its driver is that the number of
busy-wait loop iterations per HUD command drifts because `CE` clears instantly (the loop spins at
raw CPU speed and exits at a beam-phase that depends on exactly when the burst happened to run).
Two properties of the fix defeat that:

1. **Determinism** — a duration that is a pure function of the command parameters makes the loop
   exit after the *same* emulated interval every frame (given the same command sequence). Same
   interval → same beam phase for the subsequent scroll/register writes → the HUD sits in the
   **same place every frame** → no jitter.
2. **Magnitude adequacy** — the exit must land *after* the beam has passed the phase-sensitive
   region so the phase is genuinely pinned, not merely repeatable-but-wrong. Too short: the loop
   exits early, the following writes still race a moving beam → **residual jitter**. Too long: the
   loop over-waits, fewer HUD updates per frame → **visible slowdown**. So the estimate must be
   *close to* the real command duration, calibrated against openMSX (§3). This is exactly the
   DEC-0069 / `docs/m44-phase2-plan.md` risk statement ("too-short under-paces; too-long
   over-paces").

### 2.3 The duration model (re-derived per-mode cost; license-safe)

#### 2.3.1 Base formula (openMSX `calcFinishTime`, re-derived)

For an atomic rectangle/line command, at issue compute the work count and multiply by a
per-command **VDP-cycle-per-pixel-unit** cost, then convert to CPU T-states:

```
work_units   = tmp_nx * tmp_ny - 1           # clamp >= 0; degenerate (tmp_nx==0||tmp_ny==0) => 0
vdp_cycles   = ticks_per_unit * work_units    # per-command constant, §2.3.2
duration_ts  = ceil(vdp_cycles / 6)           # 6 = kCpuTstatesPerVdpCycleRatio
```

`tmp_nx`, `tmp_ny` are the **already-computed** clipped counts each `run_*` derives
(`vdp_command_engine.cpp` `clip_nx_*`/`clip_ny_*`), in the command's **native unit** (pixels for
LMMV/LMMM/LINE; **bytes** for HMMV/HMMM/YMMM — the loop iterates bytes, so the byte count is the
correct unit, matching openMSX which passes byte-clipped `tmpNX` for the high-speed commands).
`work_units = tmp_nx*tmp_ny - 1` is the exact reduction of openMSX's `nx*(ny-1)+(ANX-1)` when
`ANX==tmp_nx` at start (the issue-time value). The `/6` ratio and the VDP-cycle unit are the
project's already-established, fact-sheet-grounded constants
(`vdp_access_timing.h:90-103`, `kVdpCyclesPerLine=1368`, `kCpuTstatesPerVdpCycleRatio=6`,
`kCpuTstatesPerLine=228`).

#### 2.3.2 Per-command `ticks_per_unit` (VDP cycles) — re-derived, decomposed into existing D-deltas

| Command | openMSX `ticksPerPixel` (`VDPCmdEngine.cc`) | Decomposition into `vdp_access_timing.h` deltas | Unit |
| --- | --- | --- | --- |
| **HMMV** | 48 (`:1365`) | `D48` | byte |
| **YMMM** | 24 + 40 = 64 (`:1606`) | `D24 + D40` | byte |
| **HMMM** | 24 + 64 = 88 (`:1470`) | `D24 + D64` (= `D88`) | byte |
| **LMMV** | 72 + 24 = 96 (`:980`) | `D72 + D24` | pixel |
| **LMMM** | 64 + 32 + 24 = 120 (`:1105`) | `D64 + D32 + D24` (= `D120`) | pixel |
| **LINE** | ~88 dominant (`executeLine` uses `D88` long-axis, `D32` minor; `startLine` seeds "finish soon" `:878`) | `D88` × line length (major-axis pixel count) | pixel |
| **PSET** | ~D24 (`:804`, "finish soon" `:787`) | ≈ `D24` (single pixel) → effectively instant | pixel |
| **SRCH** | ~D88/searched-pixel (`:862`, "any moment" `:827`) | `D88` × searched distance `|asx_final − sx_|` | pixel |
| **POINT** | single read (`:762-763`, "finish soon") | ≈ 0 → instant | — |
| **ABRT** | `commandDone` immediately (`:751-754`) | 0 → instant | — |

**License-safe note (mandatory).** These are *small, human-meaningful hardware constants* — the
per-command access-slot cost per pixel/byte — every one of which decomposes into the named
`VdpAccessDelta` values **already present and already license-blessed** in
`src/devices/video/vdp_access_timing.h:72-88` (the header explicitly re-derived those deltas as
"hardware fact, not copied code", `:65-71`). Phase 2a is therefore a **composition of existing,
already-re-derived deltas**, NOT a transcription of openMSX's `VDPAccessSlots.cc:14-141`
~340-entry slot-position tables. Those tables stay **out of scope** (they are the Phase-2b
cycle-exact model). Express the per-command costs as sums of the `VdpAccessDelta` enum, with a
citation comment, exactly as the existing header does.

#### 2.3.3 Accuracy requirement + the known bias

`calcFinishTime` is a deliberate **underestimate** (`VDPCmdEngine.cc:741-742`: "assumes we never
have to wait for access slots and no per-line overhead"). The **real** command is slot-limited and
runs longer, especially **during active display** where the renderer steals slots — and Laydock
issues its HUD commands during active display (raster 109–181). So the base estimate is a **lower
bound** and is expected to **under-pace**. The accuracy target is *not* bit-exactness (that is
2b); it is: **land the busy-wait exit late enough that the beam phase of the following writes is
pinned, without over-shooting into visible slowdown.** The permissible band is bounded by the two
failure modes and is resolved empirically by §3 calibration.

### 2.4 The `CE`-busy mechanism + wiring

#### 2.4.1 Timebase — an absolute monotonic accessor (additive, default-safe)

Extend `VdpClockSource` (`src/devices/video/v9958_vdp.h:35-39`) with a **non-pure** absolute
accessor, e.g.:

```cpp
[[nodiscard]] virtual std::uint64_t cpu_total_cycles() const {
    return cpu_tstates_since_vsync();   // default keeps existing mock clocks compiling + inert
}
```

- `Hbf1xvMachine::VdpRasterClock` (`hbf1xv_machine.h:853-864`) overrides it →
  `return scheduler_.total_cycles();` (monotonic, wrap-free at u64; never reset mid-run). **This
  reads an existing public method of `core::Scheduler`; it does NOT edit `src/core`.**
- The two existing test mocks that implement `VdpClockSource`
  (`tests/unit/devices/video_vdp_vr_hr_raster_status_unit_test.cpp:51-53`,
  `tests/unit/devices/video_sprite_engine_collision_relatch_unit_test.cpp:59-61`) implement only
  `cpu_tstates_since_vsync()`; the default keeps them compiling **unedited** and byte-identical
  (they test VR/HR + sprite collision, never CE-duration).

**Why absolute, not the frame-relative `cpu_tstates_since_vsync()`:** a command's busy window can
exceed one frame (a full-screen HMMV ≈ 13,568 byte-writes × 8 T-states ≈ 108k T-states ≈ ~1.8
NTSC frames of 262×228=59,736 T-states), and the frame-relative counter **resets at every
vsync**. An absolute u64 makes `now >= cmd_busy_until` a trivial monotonic comparison with no
wrap/epoch bookkeeping — the direct analog of openMSX's monotonic `EmuTime`.

#### 2.4.2 The engine exposes a PURE duration; the VDP owns the busy timestamp (recommended split)

- **`VdpCommandEngine`** — add `std::uint64_t last_cmd_duration_tstates_ = 0;` set inside
  `execute_command()`/each `run_*` from §2.3 (0 for ABRT, degenerate no-ops, POINT, and the
  event-driven `start_lmcm`/`start_lmmc`/`start_hmmc`). Expose
  `[[nodiscard]] std::uint64_t last_cmd_duration_tstates() const;`. This is a **pure function of
  the command parameters — no clock, no "now"** — so it is unit-testable in complete isolation
  (feed NX/NY/mode/command, assert the T-state count). The engine's low-level `ce()`
  (`status_ & kCe`) stays **exactly as today** (atomic clears immediately in `command_done`;
  event-driven holds across the transfer). `reset()` sets the duration to 0.
- **`V9958Vdp`** — add `std::uint64_t cmd_busy_until_cycles_ = 0;` (cleared in `reset()`). It is
  the natural owner: it owns the clock source and computes S#2. Arm it in `change_register()`
  right after the `R#46` dispatch:

  ```
  cmd_engine_.write_register(reg-32, value);           // runs the burst; sets last_cmd_duration_tstates_
  if (reg == 46 && clock_source_ != nullptr && !command_timing_suspended_) {
      cmd_busy_until_cycles_ = clock_source_->cpu_total_cycles()
                             + cmd_engine_.last_cmd_duration_tstates();   // 0 duration => == now => inert
  }
  ```

  Report `CE` in `peek_status_register(2)` (`v9958_vdp.cpp:650`) as the **union** of the
  event-driven bit and the atomic busy window:

  ```
  const bool ce = cmd_engine_.ce()
      || (clock_source_ != nullptr
          && cmd_busy_until_cycles_ > clock_source_->cpu_total_cycles());
  if (ce) s2 |= 0x01;
  ```

  `peek_status_register(2)` is `const`; reading `clock_source_->cpu_total_cycles()` (const) and
  `cmd_busy_until_cycles_` (a member) from it is const-clean — and it already pulls the clock this
  exact way for the VR/HR bits (`v9958_vdp.cpp:621-646`), so this is the **same proven pull-style
  pattern**.

**Reconciliation with the DEC-0069/task hint ("a `cmd_busy_until_` on the command engine").**
The task suggested the engine own the timestamp. I recommend the split above instead: the engine
owns the **pure duration** (clock-free, isolated unit test) and the **VDP** owns the
**timestamp** (it alone holds the clock and computes S#2). The engine-owned variant is equally
viable — it would need `engine.arm_busy_until(now)` + `engine.ce(now)` with the VDP feeding
`now` — but it pushes clock knowledge into the engine and complicates its unit tests. This is a
minor implementation choice, not a blocker; either satisfies the mechanism. **Recommended:
duration-on-engine, timestamp-on-VDP.**

#### 2.4.3 The `command_timing_suspended_` guard (debug-write exclusion → strict superset)

`Hbf1xvMachine::debug_io_write` brackets its writes with
`render_sync_adapter_.set_suspended(true/false)` (`hbf1xv_machine.cpp:881-891`) so debug-driven
scene building never perturbs the beam/render seam. Arm the busy window under the **same
exclusion** so debug-issued commands stay byte-identical: add a `command_timing_suspended_` flag
to `V9958Vdp` (default false), toggled by the machine around `debug_io_write` exactly as it
toggles the adapter, and gate the arming on `!command_timing_suspended_` (shown in §2.4.2).
Net effect: **only real CPU-driven `OUT` command writes arm a busy window; debug/inspection
writes never do** → every debug-driven test harness is byte-identical. (Audit §2.7 shows no
oracle would break even without this guard; it is the belt-and-suspenders that makes the
strict-superset boundary crisp and future-proof.)

### 2.5 Edge cases (spec)

- **New command / Force-Interrupt / ABRT mid-busy.** Every `R#46` write re-arms
  `cmd_busy_until_cycles_ = now + duration`. ABRT (codes 0x0–0x3) and any degenerate/instant
  command yield duration 0 → `cmd_busy_until = now` → `CE` reads 0 immediately (a busy window is
  *replaced/cleared*, matching openMSX where `commandDone`/a new `start*` resets
  `statusChangeTime`). No stale window can linger past a new command.
- **Mode-loss abort.** `notify_mode_change` already aborts an in-progress command when command
  legality is lost (`vdp_command_engine.cpp:174-182`). Because the atomic burst has already
  completed and `cmd_busy_until` is a VDP-side timestamp, a subsequent mode change does not need
  to touch it; the next command re-arms. (If desired for exactness, the developer may clear
  `cmd_busy_until_cycles_` on the same abort path — optional, low value, note either way.)
- **CPU VRAM access during the busy window.** In our **burst** model the command's VRAM is
  already final when `CE` is still reported busy — so a CPU `#98` read/write during the window
  sees post-command bytes, whereas real hardware (and Phase-2b) would show the command **partway**
  and would `stealAccessSlot` (`VDPCmdEngine.hh:58-68`), lengthening the command. **This
  divergence is invisible to the target software class** (busy-wait-on-`CE` code does not touch
  VRAM until `CE` clears) and is explicitly the Phase-2a/2b boundary. **Spec it, do not fix it in
  2a.**
- **Event-driven transfers (LMMC/HMMC/LMCM).** Duration 0 (they are paced by their own held
  `kCe` + the TR/COL handshake). The union in §2.4.2 leaves them exactly as today; the arming with
  duration 0 also clears any stale atomic window at their `R#46`.
- **`--fast-disk` / speed controls.** N/A to the VDP (that is the FDC). The M25 speed controller
  scales wall-clock pacing, not emulated `scheduler_.total_cycles()`; since `cmd_busy_until` is in
  emulated cycles, speed scaling is transparent (the busy window is the same emulated length at
  any host speed). No interaction — note and move on.
- **Frame-wrap during a busy window.** Handled for free by the absolute u64 timebase (§2.4.1):
  `CE` correctly stays busy across `on_vsync`, matching a real multi-frame fill.
- **`reset()`/cold boot.** Both `last_cmd_duration_tstates_` (engine) and `cmd_busy_until_cycles_`
  (VDP) reset to 0, so no window survives a reset. If `scheduler_.total_cycles()` restarts at 0 on
  cold boot, a 0 `cmd_busy_until` is correctly already-expired.

### 2.6 Composition with the committed Phase-1 seam (`a27a1c4`) — undisturbed

Phase 2a touches **only** (a) the duration computation inside the command engine's `run_*`/
`execute_command`, and (b) the S#2-bit0 report + arming in `V9958Vdp`. It does **not** touch the
Phase-1 render-sync path: `notify_dest_row` → `CommandRowSink::on_dest_row` →
`VdpRenderSyncListener::on_commit_up_to(L)` → `scanline_accumulator_.sync_to_line` are all
unchanged. Grounding that they are orthogonal:

- The Phase-1 integration test
  (`tests/integration/machine/hbf1xv_m44_command_render_sync_integration_test.cpp`) reads **no**
  S#2/CE at all (it asserts per-row commit ordering) — so the CE overlay cannot perturb it.
- The burst still writes identical VRAM in identical order → the per-row sink fires identically →
  Phase-1 behavior is byte-for-byte preserved.

### 2.7 Strict-superset boundary + the oracle audit (evidence-grounded)

The design keeps the engine-level `cmd_engine_.ce()` (`status_ & kCe`) **semantically identical**
to today and adds the busy window **only** at the S#2 report. That makes the change a strict
superset for every command whose duration nothing observes, and confines any behavior delta to
"software that reads S#2-bit0 after an atomic command with a clock attached." The audit of every
oracle that reads bit0:

| Oracle | How it reads CE | Command class | Effect of Phase 2a |
| --- | --- | --- | --- |
| `video_vdp_command_engine_atomic_unit_test.cpp:112,195,204` | `vdp.cmd_engine().ce()` **direct** (engine bit) | atomic HMMV/POINT/ABRT | **Unchanged** — engine bit still clears in `command_done`; the overlay lives only in `V9958Vdp` (not touched by this bare-engine test) |
| `hbf1xv_m22_command_engine_integration_test.cpp:121-127` | `peek_status_register(2) & 0x01` | **LMMC** (event-driven) | **Unchanged** — event-driven `kCe`; no atomic arming |
| `hbf1xv_m22_command_engine_integration_test.cpp:102,144` | reads **VRAM only** (not CE) | atomic HMMV | **Unchanged** — no CE assertion |
| `hbf1xv_m22_sprites_command_engine_system_test.cpp:206` | `peek_status_register(2) & 0x01 == 0` | **LMMC transfer** (`:187`), read after `run_to_halt` | **Unchanged** — event-driven; also the CPU has run far past any earlier HMMV window |

**Result: no existing oracle asserts atomic-`CE==0` via the S#2 path while a window is active → no
current oracle changes; none needs re-derivation.** The developer must re-run this grep-audit
during implementation (AC-6). If a genuine timing oracle is found that *must* change, it requires a
`decisions.md` entry proving **no weakening** (a reported non-zero atomic CE duration is *more*
accurate than the previous instant-clear — the oracle would poll-until-clear rather than assert
instant-0; coverage equal-or-stronger). See Q2 (§7).

---

## 3. Milestone (M44 — Phase 2a) + calibration

- **Milestone ID:** M44 (Phase 2a).
- **Title:** DEF-M44-CMDSYNC Phase 2a — V9958 command-engine `CE`/S#2-bit0 busy-wait duration
  (Laydock 2 HUD flicker, CE-pacing fix).
- **Objective:** Report a deterministic, calibrated non-zero `CE` duration for atomic commands so
  busy-wait-on-`CE` software (Laydock 2's HUD loop) is paced, pinning the beam phase and
  eliminating the frame-to-frame HUD jitter to openMSX parity — without the Phase-2b access-slot
  cycle model.
- **Included Scope:** §1.1; the duration model §2.3; the CE mechanism + wiring §2.4; the edge
  cases §2.5; calibration §3.3. Files §5.
- **Excluded Scope:** §1.2 (all Phase-2b incremental/slot-table work); any `src/core`/
  `src/devices/cpu`/`peripherals`/frontend behavior change.
- **Dependencies:** the committed Phase-1 seam (`a27a1c4`); M22 command engine; the existing
  `VdpClockSource` pull-style timebase + `scheduler_.total_cycles()`; the reused Laydock A/B assets
  (`debug/laydock2-flicker/`, `debug/m44-laydock/`, `tools/omr-to-input-script.py`); the M41
  screen-mode A/B harness (`tools/m41-run.ps1`, `tools/m41-video-ab.py`,
  `tools/m41-scenarios.json`); WSL `/usr/bin/openmsx`.
- **Interfaces Affected:** `VdpCommandEngine` (pure `last_cmd_duration_tstates()` + internal
  compute); `VdpClockSource` (additive default `cpu_total_cycles()`); `V9958Vdp`
  (`cmd_busy_until_cycles_` + `command_timing_suspended_` + arming in `change_register` + the S#2
  union); `Hbf1xvMachine::VdpRasterClock` (override) + `debug_io_write` (toggle the suspend flag).
  No public machine/frontend API change.
- **Acceptance Criteria:** §4.
- **Regression Impact:** §6. Medium-high blast radius (S#2-bit0 is read by any command-polling
  software) mitigated by the engine-bit-unchanged split + the audit §2.7.
- **Exit Criteria:** all §4 gates green; the human's live Laydock confirmation (DEC-0069
  authoritative gate); QA sign-off; owner release decision (bundles after v1.1.1).
- **Status:** Planned.

### 3.1 Recommended slice plan (developer)

- **S1 — Timebase accessor (inert).** Add `VdpClockSource::cpu_total_cycles()` (default →
  `cpu_tstates_since_vsync()`); override in `VdpRasterClock` → `scheduler_.total_cycles()`. No
  behavior change yet (nothing reads it). Gate: builds; the two mock clocks compile unedited;
  full suite byte-identical.
- **S2 — Pure duration in the engine.** Add `last_cmd_duration_tstates_` + the §2.3 compute in
  `execute_command`/`run_*` (rectangle + LINE + SRCH; 0 for ABRT/no-op/POINT/event-driven).
  Unit-test the duration in isolation (AC-5). No CE wiring yet → S#2 still reports the old CE →
  suite byte-identical.
- **S3 — CE overlay + arming + debug guard.** Add `cmd_busy_until_cycles_` +
  `command_timing_suspended_` to `V9958Vdp`; arm in `change_register` (R#46, clock present, not
  suspended); the S#2-bit0 union; the machine toggles the suspend flag around `debug_io_write`;
  clear both on `reset()`. Gate: the §2.7 audit re-run green; M22/M32/M41 oracles green.
- **S4 — Calibrate + evidence.** Run the Laydock `.omr` A/B (§3.3 ladder), tune the single
  correction constant until the HUD is stable with no slowdown, capture the post-fix montage +
  A/B, run all evidence gates, write `docs/m44-phase2a-implementation-report.md` with the final
  calibrated constant and its A/B justification.

### 3.2 Slice-level determinism

Because the duration is a pure function of `(command, mode, tmp_nx, tmp_ny)` and the timebase is
the deterministic emulated `scheduler_.total_cycles()`, the paced loop is **fully deterministic**
run-to-run (satisfies the project determinism non-negotiable). The M36 `.omr` replay
(`tools/omr-to-input-script.py`) reproduces the same frames every run.

### 3.3 Calibration approach (against the `.omr` A/B)

1. **Baseline:** ship the §2.3 base estimate (naive underestimate). Capture OURS vs
   `debug/laydock2-flicker/omsx/oms_0xx` across the known jitter window (frames 10296–10311,
   `debug/laydock2-flicker/seq/` + `debug/m44-laydock/`). Compare the HUD band frame-to-frame.
2. **Classify the residual:** (a) HUD stable + crisp = done; (b) still jittering = **under-paced**
   → increase; (c) HUD stable but the game visibly slower / fewer HUD updates than openMSX =
   **over-paced** → decrease.
3. **Single-knob correction (bounded, license-safe).** Introduce **one** named calibration
   constant — preferred form: a coarse **active-display slot-availability factor** (a per-mode
   scalar ≥ 1 applied to `ticks_per_unit` while the command runs during active display,
   representing the fraction of VDP cycles available as command slots — a *single re-derived
   number per mode*, e.g. from the slots-per-line count, NOT the slot-position table). A simpler
   fallback is a single global scale factor. Tune it against step 1's A/B until class (a).
4. **Record** the final constant, its value, the pre/post montage, and the A/B verdict in the
   implementation report. **The human's live confirmation is the authoritative acceptance gate**
   (DEC-0069). If class (a) is unreachable without the per-position slot table, that is the
   Phase-2a→2b escalation (Q1, §7) — do NOT pull 2b forward silently.
5. **Guard against over-fit:** the constant must be a *generic* per-mode hardware-grounded factor,
   never a Laydock-specific tuning (universal-fixes rule). Verify a second command-polling title
   or the deterministic CE test still behaves sanely at the chosen value.

---

## 4. Acceptance Criteria

### 4.1 Evidence gates (run and capture; never fabricate)

- `tools/validate-assets.ps1` — BIOS present + ≥1 ROM. **PASS.**
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — refreshed.
- `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON && cmake --build build --config Debug` — clean
  rebuild, both executables (`sony_msx_headless.exe`, `sony_msx_sdl3.exe`).
- `ctest --test-dir build -C Debug --output-on-failure` — deterministic suite GREEN (fast subset;
  new count = prior + the new CE-duration test(s), no drop).
- **openMSX A/B REQUIRED** (behavior-affecting) — §4.2 + §4.3.
- **ZEXALL/ZEXDOC:** WITHHELD-substituted — confirm `git diff <base>..HEAD -- src/core
  src/devices/cpu` is **EMPTY** (mechanical per-edit trigger does not fire). Record the empty diff.

### 4.2 Behavior parity — Laydock 2 HUD (the headline fix)

- **AC-1:** The Laydock 2 HUD renders **crisp and frame-to-frame stable**, matching openMSX, via
  the reused `debug/laydock2-flicker/` + `debug/m44-laydock/` `.omr` A/B: OURS (post-fix) vs
  `omsx/oms_0xx` show no scrambled band, no red bars, no blue-noise score digits, no `GAME OVER`
  bleed, and **no per-few-frame boundary jitter** (contrast the pre-fix `hud_montage.png`). Capture
  the post-fix montage + a triptych update. **The human's live confirmation is the authoritative
  gate** (DEC-0069).
- **AC-1-slowdown:** The paced game shows **no visible slowdown** vs openMSX at the calibrated
  duration (the over-pacing failure mode is ruled out by the A/B, §3.3 step 2c).

### 4.3 No-regression parity — every other command/screen (blast radius)

- **AC-2:** ALL 13 M41 screen-mode A/Bs remain **0.000%** vs openMSX
  (`tools/m41-run.ps1` / `tools/m41-video-ab.py` over `tools/m41-scenarios.json`). (These do not
  poll CE, so the duration is invisible to them — a strong superset check.)
- **AC-3:** The M22 command-result oracles stay **GREEN unmodified** — the burst VRAM result is
  byte-for-byte unchanged: `video_vdp_command_engine_{atomic,logic_ops,nonbitmap,pending_col,
  registers,transfer}_unit_test.cpp`; `hbf1xv_m22_command_engine_integration_test.cpp`;
  `hbf1xv_m22_sprites_command_engine_system_test.cpp`. The event-driven CE handshakes
  (LMMC/HMMC) in the latter two stay green (unchanged). If any must be re-derived, it requires a
  written `decisions.md` justification proving **no weakening** (§2.7 / Q2).
- **AC-4:** The M32 oracles stay green (render-sync seam untouched): the zero-listener
  byte-identity unit oracle, `hbf1xv_m32_split_screen_system_test.cpp`,
  `hbf1xv_m32_{line_interrupt,per_line_latch}_integration_test.cpp`.
- **AC-4b:** The **M44 Phase-1** integration test
  (`hbf1xv_m44_command_render_sync_integration_test.cpp`) stays green **unmodified** (Phase-2a is
  orthogonal, §2.6).
- **AC-4c:** The VR/HR raster-status oracle
  (`video_vdp_vr_hr_raster_status_unit_test.cpp`) and the sprite-collision-relatch oracle
  (`video_sprite_engine_collision_relatch_unit_test.cpp`) stay green **unmodified** — the two
  existing `VdpClockSource` mocks compile via the new method's default (§2.4.1).

### 4.4 New deterministic CE-duration test (non-tautology REQUIRED)

- **AC-5 (pure duration, engine-level):** A new unit test constructs a `VdpCommandEngine` (or the
  VDP) in a known mode (e.g. GRAPHIC4), issues an atomic rectangle command with known `NX/NY`, and
  asserts `last_cmd_duration_tstates()` equals the §2.3 formula value
  (`ceil(ticks_per_unit * (tmp_nx*tmp_ny − 1) / 6)`) for at least: one HMMV (byte unit, 48), one
  LMMV (pixel unit, 96), and one LMMM (120). Assert a **degenerate** command (NX or NY = 0) and
  **ABRT** and **event-driven LMMC** all report duration **0**.
- **AC-6 (CE window, VDP-level, with a fake absolute clock):** A new integration/unit test attaches
  a fake `VdpClockSource` whose `cpu_total_cycles()` advances on demand, issues an atomic command
  via the real register path, and asserts: `peek_status_register(2) & 0x01 == 1` while
  `now < cmd_busy_until`, then flips to `0` once `now` is advanced past it. A second arm asserts a
  **busy-wait loop** (poll S#2-bit0, advancing the fake clock per poll) exits after the expected
  number of polls (i.e., it is *paced*).
- **AC-6 non-tautology:** an adversarial arm that forces the duration to 0 (the pre-fix behavior)
  makes the "paced" assertion **FAIL** (CE clears on the first poll → the pacing check reproduces
  the un-paced loop). A second adversarial arm removes the `> now` comparison (constant CE=1) and
  asserts the "clears after the window" assertion **FAILS**. The test must FAIL if either the
  duration or the timestamp comparison is removed/mutated.
- **AC-7 (strict-superset / default-inert):** with **no** clock attached (bare `V9958Vdp`),
  issuing an atomic command and reading S#2-bit0 reproduces the **exact pre-fix** value
  (`cmd_engine_.ce()` only) — CE=0 immediately after the burst. With a clock attached but the
  command issued via `debug_io_write` (suspended), CE also reports the pre-fix instant-clear
  (guards the §2.4.3 debug exclusion). Re-run the §2.7 grep-audit and record it.

---

## 5. File and Blast-Radius Map

All edits confined to `src/devices/video` + the one `src/machine` clock-adapter/override method +
`tests/`. **NO `src/core`, NO `src/devices/cpu`, NO `src/peripherals`, NO frontend behavior
change.**

| File | Change | Risk |
| --- | --- | --- |
| `src/devices/video/vdp_command_engine.h` | Add `std::uint64_t last_cmd_duration_tstates_` + getter; declare a small per-command `ticks_per_unit` table (sums of `VdpAccessDelta`). | Low — additive; pure value. |
| `src/devices/video/vdp_command_engine.cpp` | Compute `last_cmd_duration_tstates_` in `execute_command`/`run_lmmv/lmmm/hmmv/hmmm/ymmm/line/srch` from the already-computed `tmp_nx/tmp_ny` (§2.3); set 0 for ABRT/no-op/POINT/event-driven; `reset()` clears it. **No change to VRAM writes/order.** | **Medium — the model core.** Must not alter burst results. |
| `src/devices/video/v9958_vdp.h` | Add default `VdpClockSource::cpu_total_cycles()`; add `V9958Vdp` members `cmd_busy_until_cycles_`, `command_timing_suspended_` + a `set_command_timing_suspended(bool)` (or reuse an existing suspend hook). | Low — additive; default keeps mocks inert. |
| `src/devices/video/v9958_vdp.cpp` | Arm `cmd_busy_until_cycles_` in `change_register` after the R#46 dispatch (clock present + not suspended); the S#2-bit0 **union** in `peek_status_register(2)`; clear both on `reset()`. | **Medium — the S#2 report.** Covered by AC-5/6/7 + audit §2.7. |
| `src/machine/hbf1xv_machine.h` | `VdpRasterClock::cpu_total_cycles()` override → `scheduler_.total_cycles()`. | Low — one-line, reads existing scheduler API. |
| `src/machine/hbf1xv_machine.cpp` | Toggle `vdp_.set_command_timing_suspended(true/false)` around `debug_io_write` (alongside the existing `render_sync_adapter_.set_suspended`). | Low — mirrors the existing bracket. |
| `src/core/**`, `src/devices/cpu/**` | **UNCHANGED.** | None. |
| `src/devices/video/vdp_frame_renderer.*`, `vdp_scanline_accumulator.*`, `vdp_vram.*` | **UNCHANGED** (Phase-1 seam + VRAM store untouched). | None. |
| `tests/...` (new) | CE-duration unit + CE-window integration tests (AC-5/6/6-nt/7). | Additive. |

Confirm at every gate: `git diff <base>..HEAD --stat` shows only `src/devices/video`, the two
`src/machine` adapter lines, and `tests/` — nothing under `src/core` or `src/devices/cpu`.

---

## 6. Regression Matrix

| Area | Oracle / evidence | Expected | Why at risk |
| --- | --- | --- | --- |
| Command VRAM results | M22 atomic/logic/nonbitmap/pending_col/registers/transfer unit tests | Green unmodified | Duration compute added to `run_*`; results must not change |
| Command + sprite system parity | `hbf1xv_m22_sprites_command_engine_system_test` | Green unmodified | Event-driven CE assertion (`:206`) must stay valid |
| Command engine handshake | `hbf1xv_m22_command_engine_integration_test` | Green unmodified | Event-driven CE via peek-S#2 (`:120-127`) |
| Phase-1 render-sync | `hbf1xv_m44_command_render_sync_integration_test` | Green unmodified | Orthogonality (§2.6) |
| VR/HR + collision mocks | `video_vdp_vr_hr_raster_status`, `video_sprite_engine_collision_relatch` | Green unmodified | New `VdpClockSource` method default must keep them compiling/inert |
| Split-screen / line-int | M32 oracles | Green | Shares the VDP; render-sync untouched |
| Screen modes (13) | M41 A/B | **0.000%** | Any command-drawn screen (CE invisible to them) |
| Laydock 2 HUD | `.omr` A/B, `debug/laydock2-flicker/` + `debug/m44-laydock/` | Crisp/stable = openMSX (AC-1) | The target defect |
| CPU/core | `git diff -- src/core src/devices/cpu` | EMPTY → ZEXALL withheld | Confirms no CPU touch |
| Both executables | headless + SDL3 fast ctest | Green, count = prior + new | Build/link + SDL3 path |

---

## 7. Risks

- **R-1 (Central — calibration reachability):** the naive underestimate under-paces during active
  display (§2.3.3/A-4), so a calibrated upward correction is expected. If **no** coarse per-mode
  factor stabilizes the HUD without slowdown — i.e., Laydock needs *per-raster-position* slot
  accuracy — that is the Phase-2a→2b boundary. **Mitigation:** the §3.3 ladder + the Q1 escalation
  (do NOT pull the slot-position table forward silently; that violates the phased DEC + the
  license-sensitive-scope ban). Document the reached class + evidence.
- **R-2 (License isolation):** the per-command costs must be expressed as sums of the existing
  re-derived `VdpAccessDelta` values (§2.3.2), **not** transcribed from `VDPAccessSlots.cc`.
  **Mitigation:** cite `vdp_access_timing.h:72-88` + `VDPCmdEngine.cc:733-747` for the *formula*
  only; a code review confirms no slot-position table appears; `feedback_license_sensitive_scope`.
- **R-3 (S#2 blast radius):** S#2-bit0 is read by any command-polling software.
  **Mitigation:** the engine-bit-unchanged split (§2.4.2) confines the delta to the reported CE
  window; the evidence-grounded audit (§2.7) shows no oracle regresses; the `command_timing_
  suspended_` guard (§2.4.3) keeps debug harnesses byte-identical; AC-2/3/4 sweep.
- **R-4 (Timebase correctness):** using the absolute `scheduler_.total_cycles()` is required for
  cross-frame windows; the frame-relative accessor would break after vsync. **Mitigation:** the
  additive `cpu_total_cycles()` (§2.4.1) + AC-6 exercises a window that crosses a simulated wrap.
- **R-5 (Interface-break of mock clocks):** a *pure*-virtual add would break the two existing
  mocks. **Mitigation:** ship it **non-pure with a default** (§2.4.1); AC-4c pins that they compile
  unedited and stay green.
- **R-6 (Over-fit / non-universal tuning):** a Laydock-specific constant would violate
  universal-fixes. **Mitigation:** the calibration constant is a generic per-mode
  hardware-grounded factor; §3.3 step 5 verifies it on a second polling path / the CE test.
- **R-7 (Burst-vs-partway divergence):** CPU VRAM access during the window sees final (not
  partway) bytes (§2.5). **Mitigation:** invisible to busy-wait-on-CE software; explicitly the
  2a/2b boundary; specced, not fixed.

---

## 8. Developer Handoff

- **Do:** implement S1→S4 (§3.1) in `src/devices/video` (+ the two `src/machine` adapter lines).
  Mechanism = §2.4 (recommended split: **pure duration on the engine, timestamp on the VDP**).
  Duration model = §2.3 (per-command cost as sums of the existing `VdpAccessDelta`, license-safe).
- **Central invariant:** Phase 2a changes only the **reported CE duration** — never *what* bytes a
  command writes, nor the write order, nor the engine-level `cmd_engine_.ce()` status bit, nor the
  Phase-1 render-sync seam. All M22 content oracles + the Phase-1 test pass **unmodified**.
- **Timebase:** add `VdpClockSource::cpu_total_cycles()` **non-pure with a default** (keeps the two
  mock clocks compiling unedited); override in `VdpRasterClock` → `scheduler_.total_cycles()`
  (reads the scheduler; does **not** edit `src/core`). Arm only real CPU writes (not
  `debug_io_write`) via the `command_timing_suspended_` guard (§2.4.3). Clear both timestamps in
  `reset()`.
- **Grounding to cite (read, never copy):** `references/openmsx-21.0/src/video/VDPCmdEngine.cc:
  733-747` (`calcFinishTime` formula) + `:980,1105,1365,1470,1606` (per-command `ticksPerPixel`) +
  `VDPCmdEngine.hh:76-81` (`peekStatus2` CE-until-`statusChangeTime`). Our re-derived deltas:
  `src/devices/video/vdp_access_timing.h:72-103`. Do **not** read/transcribe
  `VDPAccessSlots.cc:14-141` (the 2b slot table).
- **Calibrate:** run the Laydock `.omr` A/B (§3.3), tune the **single** correction constant to
  class (a) — HUD stable + crisp + no slowdown. Record the final value + pre/post montage + A/B
  verdict. The human's live confirmation is the authoritative gate (DEC-0069).
- **Gates:** §4.1 evidence gates; the Laydock `.omr` A/B (AC-1); the M41 0.000% sweep (AC-2);
  M22/M32/Phase-1/VR-HR oracles unmodified (AC-3/4/4b/4c); the new CE-duration tests with
  non-tautology + inertness (AC-5/6/6-nt/7). Confirm the empty `src/core|cpu` diff.
- **Report:** `docs/m44-phase2a-implementation-report.md` + A/B artifacts under `docs/` (+ refreshed
  montage/triptych under `debug/m44-laydock/` or `debug/laydock2-flicker/`). Keep the Phase-2a/2b
  boundary explicit; do NOT expand into the incremental slot-table model (DEC-0065/0069 boundary).

### Open questions for a decision entry (if hit)

- **Q1 (calibration reachability → Phase-2b):** if the `.omr` A/B cannot reach a stable, no-slowdown
  HUD with any coarse per-mode factor — i.e., Phase 2a's linear model is insufficient and only the
  per-raster-position slot table would stabilize it — **stop and escalate** to the coordinator for a
  DEC to either (a) accept a documented bounded residual as the Phase-2a/2b boundary, or (b) open
  Phase 2b. Do NOT pull the slot-position table forward under Phase 2a (phased-DEC + license ban).
- **Q2 (oracle re-derivation):** if the implementation-time re-audit (§2.7 / AC-6) finds any oracle
  that asserts atomic-`CE==0` via the S#2 path while a window is active, it needs a `decisions.md`
  entry proving **no weakening** (the reported non-zero atomic CE is strictly more accurate;
  convert an instant-0 assertion to a poll-until-clear with equal-or-stronger coverage) **before**
  editing it.
- **Q3 (busy-until ownership):** the recommended split puts the timestamp on `V9958Vdp` (deviating
  from the DEC-0069/task hint of "on the command engine") for const-correctness + clock ownership +
  engine unit-test purity (§2.4.2). This is an implementation-local choice; flag it in the report,
  no separate DEC needed unless the reviewer prefers the engine-owned variant.
