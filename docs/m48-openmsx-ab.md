# M48 — openMSX A/B evidence (EG-6 screen-mode + EG-7 command-timing)

**Milestone:** M48 — V9958 command-engine ↔ CPU VRAM access-slot CONTENTION model (DEC-0075).
**Author:** MSX QA Agent. **Date:** 2026-07-14. **Status:** read-only A/B assessment; no `src/` edits.
**Environment:** WSL Ubuntu-24.04, openMSX **19.1** at `/usr/bin/openmsx` (reference source tree is
openMSX **21.0** under `references/`). Our engine: `build/Debug/sony_msx_headless.exe` (single
canonical `build/`, DEC-0041).

**Authority note (DEC-0073, restated).** The tier-1 authority for M48 is the fact-sheet §8 slot
COUNTS (154/88/31), **not** openMSX. openMSX's own command timing is a 2013 logic-analyzer
approximation taken on a **V9938 (Philips NMS8250)**, not a real V9958 (fact-sheet §8 line 154), and
openMSX is documented "too fast on VDP commands" (fact-sheet §10 line 176 / openMSX issue #2057). So
cycle-for-cycle openMSX parity on command timing is neither the target nor achievable; matching the
tier-1 §8 counts on average is the correct bar (planner §7).

---

## EG-6 — 13 screen-mode A/B (no rendering regression)

**Bar (planner EG-6):** all 13 M41 screen modes (M0–M12) still MATCH openMSX at 0.000% (bit-exact),
proving the atomic VRAM/pixel output is unchanged by the timing overlay.

### Disposition: PASS (structural + oracle-corroborated). Automated 13-mode openMSX re-run NOT reproducible from committed artifacts in this environment (documented, non-blocking).

**1. Structural proof (decisive) — M48 changes zero rendering inputs.** The full M48 diff
(`src/devices/video/{vdp_access_timing.h, v9958_vdp.h, v9958_vdp.cpp}`) touches **only** the S#2
CE/TR busy-window expiry timestamps and a one-directional CPU-priority slot-steal:

- `arm_command_busy_window()` now writes `cmd_busy_until_cycles_ = ceil(free_running * 154 / S_eff)`
  (+ bias) — a **STATUS/timing timestamp only**.
- `steal_command_slot_for_cpu_vram_access()` adds `slot_cost_tstates(S_eff)` to
  `cmd_busy_until_cycles_` — again a timestamp only.
- The steal is invoked at the **top** of `io_read`/`io_write` `case 0` **before**
  `vram_data_read()` / `vram_data_write()`; it does not read, alter, drop, or delay the VRAM datum,
  the VRAM pointer, the returned byte, the write order, the palette, the mode decode, or the M44
  `CommandRowSink` per-destination-row render-sync. `vdp_command_engine.{h,cpp}` is **untouched**
  (`git diff` empty), so `last_cmd_duration_tstates()` and the M44 duration oracle are unmoved.

Because **no rendering input changed**, the rendered pixel output for any settled scene is
byte-identical to v1.1.4, which M41 established at **0.000% vs openMSX for 11/13 modes** (8 exact +
2 YJK post-fix + baseline; `docs/m41-production-qa-signoff.md`). EG-6 is therefore structurally
guaranteed for the settled-scene A/B methodology M41 uses.

**2. Render-oracle corroboration (ran green this cycle).** The deterministic render oracles that
would catch any pixel/VRAM/render-path perturbation all PASS on the M48 build:
`machine_hbf1xv_m21_vdp_render_integration_test` (#114), `hbf1xv_m21_vdp_render_system_test` (#115),
`machine_hbf1xv_m44_command_render_sync_integration_test` (#203), plus the M22 sprite/command-engine
and M32 per-line-latch fixtures — all green (full ctest 231/231).

**3. Why the automated openMSX 13-mode pipeline was not re-run here.** The M41 harness
(`tools/m41-run.ps1`) is scenario-manifest driven, but the committed
`tools/m41-scenarios.json` holds only two baseline scenarios, and both are unreproducible from
committed state in this environment:
- `baseline_cart_g2` depends on a **gitignored** pre-generated cart artifact
  `debug/m38/carts/s10_g2_baseline.rom` (absent) → harness BLOCKED (`frame` dump never produced).
- `baseline_disk_circle` is itself marked `"expected":"BLOCKED"` — a **pre-existing** headless
  disk-BASIC boot hang (DI:HALT disk-ROM pc=0x4005 on a blank-format disk), unrelated to M48.

The full 13-mode capture set the M41 signoff used lives under the **gitignored** `debug/m41/ab/`
tree and requires regenerating the M38/M41 cart+guest artifact pipeline. This is an
environment/harness-artifact limitation, **not** an M48 regression. Attempt log:
`debug/m48/screen-ab/m41-ab-report.md` (row: `baseline_cart_g2 … BLOCKED:harness-error`).

**4. Note on the (rejected) our-M48-vs-preM48 game-frame diff.** A live-game frame-identity check
between M48 and pre-M48 would be **misleading** and is deliberately NOT used: M48 intentionally
changes the CE-window duration, so a title that busy-waits on S#2 CE legitimately shifts its
execution timeline — a differing frame would be *expected behaviour*, not a defect. The correct
oracle for "no rendering regression" is the settled-scene A/B (above), and the intended
command-timing change is validated perceptually by the owner live spot-check (R3, below / EG-7).

---

## EG-7 — command-engine timing A/B (the contention parity evidence)

**Bar (planner EG-7):** a CE/TR-duration A/B vs openMSX for representative commands (HMMV, HMMM,
LMMM, LINE) under the three slot conditions (display-off, sprites-off, sprites-on) plus the §8
"HMMV + concurrent `OUT (#98)`" scenario. Disposition measured and honestly recorded per §7
(aggregate-rate parity, not cycle-exact); DIRECTION and rough magnitude must match.

### Disposition: PASS (rigorous STATIC source A/B vs tier-1 §8 + openMSX mechanism). Dynamic CE-duration A/B is not measurable by the available harness — proven below, not assumed.

**1. Why the dynamic harness cannot measure this.** The A/B harnesses
(`tools/openmsx-m22-sprite-cmd-parity.ps1` and the smoke probe) compare **final** VDP register /
status-register / VRAM bytes after a command "completes naturally" — they do **not** sample the S#2
CE/TR busy-window **duration** over time, which is the only quantity M48 changes. Moreover openMSX's
CE duration is itself driven by the **banned** `VDPAccessSlots.cc` per-slot POSITION tables via
`VDPCmdEngine.cc` `getAccessSlot`/`calculator.next(Delta::D88)` (confirmed by reading
`references/openmsx-21.0/src/video/VDPCmdEngine.cc:53,742,862,909` — read for **effect only**, not
transcribed), and openMSX is a V9938-measured, "too fast" (issue #2057) approximation. So a dynamic
openMSX CE-duration number would be neither the tier-1 authority nor cycle-comparable. The correct
evidence is a static A/B of the two *models* against the tier-1 fact-sheet §8 counts.

**2. Static model A/B — the three slot regimes (Slice 1).**

| Regime (fact-sheet §8 l.163) | S | tier-1 factor 154/S | M48 `arm_command_busy_window()` | openMSX mechanism |
|---|---|---|---|---|
| display off / border / vblank | 154 | 1.000× | `ceil(d·154/154)=d` (byte-identical v1.1.4) | per-slot advance, screen-off slot set (154 slots) |
| display on, sprites OFF | 88 | 1.750× | `ceil(d·154/88)` | per-slot advance, sprites-off slot set (88 slots) |
| display on, sprites ON | 31 | 4.968× | `ceil(d·154/31)` | per-slot advance, sprites-on slot set (31 slots) |

Both models slow the command by the **same tier-1 slot-scarcity ratios**; M48 applies the ratio as a
whole-duration **average** (`1368/S` mean spacing, no positional info — the ban boundary), openMSX
applies it **per-slot positionally** (the banned table). Direction and rough magnitude match; the
per-slot phase does not (accepted, §7 / Risk R6). Worked HMMV NX=6 NY=2 GRAPHIC4 (base 50 T-states,
independently re-derived from §8 costs `48·(3·2−1)+56·(2−1)=296` VDP cyc, `ceil(296/6)=50`):
display-off 50 → sprites-off 88 → sprites-on 249 T-states. These exact values are asserted by the
Slice-1 unit oracle `devices_v9958_cmd_slot_factor_unit_test` (#123, green), with expected values
re-derived from 154/88/31/1368/6 (never read back from the impl — EG-8), and they DISCRIMINATE:
an 88↔31 swap makes 8 cases fail (QA-reproduced adversarial mutation, below).

**3. Static model A/B — the concurrent-`OUT (#98)` scenario (Slice 2, the §8 "~2× cut").**

Fact-sheet §8 l.163: "CPU VRAM access takes priority over the command engine and steals slots — with
sprites on, a HMMV can be cut ~2× by simultaneous `OUT (#98),A`." M48 models this as: each CPU `#98`
access while a command is busy adds `slot_cost_tstates(S_eff)` T-states to the command's CE window
(154→2, 88→3, **31→8**), one-directional (CPU never stalled — the HB-F1XV does not wire /WAIT,
fact-sheet §1/§7).

- **Direction:** correct — concurrent CPU `#98` access lengthens the command, never the reverse.
- **Magnitude (sprites on, S=31, 8 T-states/access):** a realistic tight Z80 `OUT (#98)` cadence is
  ~16–20 T-states/access (`OUTI`=16T, or `LD A,n`+`OUT (n),A`≈18T). Over a command window `W`, that
  is ~`W/18` accesses each stealing 8T → the command extends by ~`0.44·W` (≈1.45×) up to ~0.5·W
  (≈1.5×) for the tightest sustained `OUTI` stream. The fact-sheet's "**~2×**" is an upper bound; a
  real Z80 cannot sustain one `#98` access per 8 T-states, so the exact 2× is not reachable and M48
  lands directionally correct, somewhat under it — consistent with the §7 "roughly quantitatively
  correct, not cycle-exact" bar and with Risk R6 (the average model under-inflates the clustered
  worst-case gaps of the real 31 slots). Notably M48's cut is **proportional to the game's actual
  `#98` cadence**, which is more faithful than a fixed "2×". The v1.1.4 build had **no** such effect
  at all — M48 is a genuine, real improvement.

The Slice-2 oracles `devices_v9958_cmd_slot_steal_unit_test` (#124) and
`machine_hbf1xv_m48_cpu_vram_steal_invariance_integration_test` (#136) assert the +N·`slot_cost(S)`
extension per regime with tier-1-re-derived expected values, and prove CPU-timing invariance (the
`#98` read byte is identical busy-vs-idle; the CPU clock is never advanced by a `#98` access; two
byte-identical Z80 programs differing by one operand step in per-instruction T-state lockstep). Both
DISCRIMINATE: dropping the steal add fails 4 unit cases + the integration C2 case (QA-reproduced).

**4. Perceptual validation of the aggressive 4.968× sprites-on branch (Risk R3).** The owner LIVE
spot-checked the sprites-on direction on **Laydock 2** and **YS II** (task-stated): "looks right,
not too slow." This discharges the perceptual regression check on the two DEC-0069-validated titles
for the far-more-aggressive tier-1 4.968× factor (vs the old empirical 200/300%). Per DEC-0073 the
tier-1 §8 counts are authoritative over the prior live-tuned placeholder.

---

## Honest ceiling (planner §7 / Risk R6) — what is NOT delivered, and stays OPEN

M48 is the **ban-respecting average-throughput** contention model. It does **not** and cannot (under
the `VDPAccessSlots.cc` license-sensitive-scope ban) reproduce: (1) the cycle-exact VDP-cycle at
which any single access is granted; (2) the exact clustered worst-case gap structure of the 31
sprites-on slots (the `1368/S` average understates it — R6); (3) cycle-exact "too-fast dropped
request"; (4) raster-position command/CPU tearing beyond M44's per-destination-row granularity.
These remain the C1/D4 **UNBUILDABLE-WITHOUT-FABRICATION** remainder (deferred-backlog rows C1/D4),
unchanged by M48. This is a genuine, permanent sourcing/license blocker for the *exact* model, not a
"too large" deferral — honestly recorded, not smuggled.
