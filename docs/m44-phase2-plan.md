# M44 Phase 2 — V9958 command-engine cycle-timing (deferred; resumable plan)

Status: **DEFERRED by the human (2026-07-12).** Phase 1 (`a27a1c4`, the render-observation-timing
foundation) is committed and no-regression. This document captures everything needed to resume
Phase 2 cold. Context: DEC-0065 (phasing decision), DEC-0066 (Phase-1 close + Phase-2 deferral),
`docs/m44-planner-package.md` (the Phase-1 design + §2.5 R-1 residual), and the investigation
evidence under `debug/laydock2-flicker/`, `debug/m44-laydock/`, `debug/m44-ab/`.

## Why Phase 2 exists (the actual Laydock 2 flicker cause — instrumented)

DEF-M44-CMDSYNC opened from the human's Laydock 2 report: a fast-flickering scrambled band in the
top status-HUD (around SCORE), present on our engine but not openMSX. Phase 1 assumed the cause was
render-*observation* timing (the command engine writing raster-visible VRAM outside the render-sync
seam → a patchwork tear). **Phase-1 implementation + the `.omr` A/B DISPROVED that as the Laydock
cause** (`debug/m44-laydock/diag2.log`):

- Laydock issues its HUD-updating commands while the **raster is at lines 109–181** — far *below*
  the HUD (display lines 0–47).
- **15,819 of ~16,001** HUD-line commits are **already-swept** (the beam has already passed the HUD
  when the commands run). Phase 1 correctly wrap-guard-skips these — real hardware physically
  cannot repaint an already-drawn scanline. Only ~182 are effective. So Phase-1 observation timing
  cannot reach the HUD.
- The HUD is frozen by the existing io_write seam at an intermediate VRAM state, and it **jitters
  frame-to-frame because the game's blit loop is UNPACED**: our command engine clears its `CE`
  status bit (S#2 bit 0) in **0 cycles**, so the game's `busy-wait-on-CE` HUD-composite loop runs at
  the wrong rate → its subsequent register/blit writes land at a drifting beam phase → the frozen
  HUD state jitters.

**Conclusion:** the Laydock flicker is the **CE-pacing / command-duration** gap, NOT observation
timing. Fixing it requires giving VDP commands real duration so `CE`/S#2-bit0 stays busy for the
command's true length — which paces the game's blit loop and makes the beam phase deterministic.

## Scope — two tiers

### Phase 2a (minimal — the actual Laydock fix; recommended first)
Model the **`CE` / S#2-bit-0 busy-wait DURATION**: when a command starts, compute an
estimated finish time (proportional to the command's work — `NX*NY` for rectangle ops, length for
LINE/SRCH, per the V9958 access-slot cost per mode) and hold `CE=1` / S#2 bit0 set until that
emulated time. Games that busy-wait on `CE` then get paced correctly → the Laydock HUD stabilizes.
This does NOT require the full incremental executor — the command may still compute its final VRAM
in a burst (Phase 1 handles the observation timing for the ahead-of-beam case); only the *reported
duration* becomes non-zero.
- Grounding: openMSX `VDPCmdEngine::calcFinishTime` / the per-command `getSlotCalculator` cost
  (`references/openmsx-21.0/src/video/VDPCmdEngine.cc:985-1090`), the S#2 CE bit in
  `peekStatus2`/`sync` (`VDPCmdEngine.hh:53-142`).
- Risk: getting the duration *approximately* right paces the loop; getting it *cycle-exact* is 2b.
  A too-short estimate under-paces (residual jitter); too-long over-paces (slowdown). Calibrate
  against the `.omr` A/B until the HUD is stable.

### Phase 2b (full — complete command-timing bit-parity)
Make the 10 commands execute **incrementally over emulated cycles**: `execute*(EmuTime limit)` +
the access-slot calculator + per-mode timing, writing pixels one VDP access slot at a time with
each write timestamped, and `sync(time)` forced on every status/register/CPU touch and on
`stealAccessSlot` for CPU VRAM access during a command. This gives cycle-accurate command timing
(the true bit-parity goal) and subsumes 2a. It is a substantial VDP rewrite — the reason the human
chose the phased approach.
- Grounding: `VDPCmdEngine.hh:53-142` (`sync`/`sync2`/`stealAccessSlot`), `VDPCmdEngine.cc:985-1090`
  (`executeLmmv` etc.), `VDPVRAM.hh:428,575-593` (`cmdWrite`→`writeCommon`→notify).

## The Phase-1 foundation Phase 2 builds on (already committed, `a27a1c4`)

- `VdpCommandEngine`: a default-null `CommandRowSink` + `set_command_row_sink()` + `notify_dest_row(dy)`
  called per destination row (before that row's writes) in the 7 atomic writers
  (`run_lmmv/lmmm/hmmv/hmmm/ymmm/pset/line`). `run_point/run_srch` + event-driven transfers untouched.
- `V9958Vdp`: `VdpRenderSyncListener::on_commit_up_to(int line)` (default-empty); a private
  `CommandRowSyncAdapter` installed in the ctor that computes `L=((dy&0xFF)-R#23)&0xFF` and applies
  the four gates (bitmap+visible-page [multi-page-aware], active-display/vblank-suppress, then
  forward to `render_sync_->on_commit_up_to(L)`).
- `Hbf1xvMachine::VdpRenderSyncAdapter::on_commit_up_to(int)` → advance-only `sync_to_line` with the
  wrap guard (`L<=watermark ⇒ skip`; never trips the accumulator's mid-command finalize), honoring
  `suspended_`/`mark_completed_frame_stale()`; plus the `last_beam_commit_target_` guard on
  `on_before_state_change` that distinguishes a genuine frame-wrap (raster decreases → keep finalize)
  from monotonic advance (skip) — proven byte-identical for all pre-M44 paths.

Phase 2 adds command duration/timing ON TOP of this seam; the seam itself needs no change.

## Acceptance (Phase 2)

- **Laydock 2 HUD flicker GONE** and stable, matching openMSX — the `.omr` A/B (`debug/m44-laydock/`
  + `tools/omr-to-input-script.py`; the human's live confirmation is the authoritative gate).
- Busy-wait-on-`CE` games are paced correctly (a deterministic CE-duration test).
- NO regression: all 13 M41 screen-mode A/Bs still 0.000%; M22 sprite+command-parity + M32
  byte-identity oracles green or re-derived with justification (no weakening); non-tautology proven.
- `src/devices/video` only expected; if the timing needs the scheduler/clock (`src/core`), that
  fires the mechanical ZEXALL sweep — re-confirm blast radius before implementing.
- openMSX A/B required (behavior-affecting).

## Resume checklist

1. Read this doc + `docs/m44-planner-package.md` (Phase-1 design + §2.5 R-1) + DEC-0065/0066.
2. Re-run the Laydock repro: `debug/m44-laydock/` capture loop (see `diag2.log` for the raster-position
   proof and the exact frames 10296–10308). The HUD-band frame numbers should match
   `debug/laydock2-flicker/seq/`.
3. Start with Phase 2a (CE-duration); calibrate the duration estimate against the `.omr` A/B until the
   HUD is stable; then decide whether 2b (full cycle accuracy) is warranted for the bit-parity goal.
4. Evidence to reuse: `debug/laydock2-flicker/` (the original A/B triptych + openMSX clean frames),
   `debug/m44-ab/` (the 13-mode + pre/post byte-identity proof), `debug/m44-laydock/` (post-Phase-1
   HUD + diag2.log).

## Separate, pre-existing item found during M44 (NOT part of M44)

The 13-mode A/B surfaced a **pre-existing** openMSX divergence in two sprite modes: `sp2_double`
(~8.6%) and `sp3_collision` (~6.4%). Proven unrelated to M44 (byte-identical before/after `a27a1c4`).
This is a latent V9958 sprite-rendering bit-parity gap (double-size / collision) worth its own
investigation — evidence in `debug/m44-ab/m44-13mode-ab.md`.
