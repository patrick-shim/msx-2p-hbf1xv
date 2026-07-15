# M51 Planner Package — Root-cause + fix the sprite disappearance/flicker-during-scroll defect

- Milestone ID: M51
- Title: Sprite disappearance/flicker during scroll (Aleste 2 / Firebird / Laydock 2) — diagnostic-first root cause, then a universal spec-modeling fix
- Objective: pin the CRITICAL, gameplay-breaking sprite defect (DEC-0078) to a concrete mechanism by evidence (deterministic repro → milestone bisect → instrumented trace), then fix it universally, keeping every existing sprite/collision/screen oracle green and 13-mode openMSX parity at 0.000%.
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent (handoff §6)
- QA Owner: MSX QA Agent
- Opened by: DEC-0078 (2026-07-15, `agent-protocol/channels/decisions.md:999-1010`), PLANNING phase, planner-first; owner pre-authorized a fully autonomous run.
- Reference precedence (DEC-0073): tier-1 real-hardware spec > tier-2 openMSX + spec-disciplined reasoning > tier-3 fMSX. On a confirmed conflict the hardware spec wins and the openMSX divergence is documented. Never copy openMSX/fMSX code into `src/`.
- Structure map (REQ-M51-001 required elements → this package): defect statement + repro matrix → §1.1; grounding with tier-1/2/3 citations → §2.2-§2.3; attribution/diagnostic plan (repro slice, bisect slice, expected-signal table) → §3 Tasks 0-2 + the signal table after Task 2; fix-slice decision tree → §3.4; test obligations → §4 (AC-F3, EG-2); evidence gates → §4 EG-1..EG-11; risks → §5; non-goals + authentic-flicker guard → §1.5 (incl. the binding guard block); dependencies/assets → §2.4-§2.5 + §6.

---

## 1. Scope and Assumptions

### 1.1 Defect record (three independent owner live repros, current main @ 0fd9690 / v1.2.0-era)

1. **Aleste 2** (`games/roms/Aleste 2/aleste2.rom`, sha1 `e93d0840c59c6eba273df546d22148d486a150a6`, KonamiSCC): the player's plane sprite completely DISAPPEARS and RE-APPEARS **correlated with game scrolling**. Owner launch: `build/Debug/sony_msx_sdl3.exe --bios-dir bios/ --slot1 <rom> --disk-writable` (convenience defaults: 512 KB modded RAM, FM-PAC auto slot 2, fast-disk ON).
2. **Laydock 2** (disk set `games/disks/laydock_2/laydock2-1.dsk` + `laydock2-2.dsk`): same disappearance symptom per owner.
3. **Firebird / Hi no Tori Hououhen** (`games/roms/firebird/firebird.rom`, sha1 `6b9263e6c5047b70d2c6e6ad896262aae73a3447`, KonamiSCC): player character flickers CONSTANTLY; flying enemy sets flicker severely; **ground-locked objects moving with the scrolling ground do NOT flicker**. This cleanly discriminates layers: the background pipeline is healthy, the sprite subsystem is the defect locus.

### 1.2 Timeline discipline (verified in ledger — do not re-derive)

- **M48** (v1.1.5; ledger anchor hash `e7a7a41` per REQ-M51-001, `agent-protocol/channels/requests.md:1836`): V9958 access-slot contention overlay (`agent-protocol/state/milestones.md:1429`).
- **M49** (v1.1.6; ledger anchor hash `0d84451` per REQ-M51-001): per-line-LIVE sprite RENDERING split (backlog D9), collision kept frame-atomic (`milestones.md:1431-1443`; planner package `docs/m49-planner-package.md`). Ledger honesty notes that matter here:
  - M49 closed on **owner live sign-off with no separate QA sign-off doc** (`milestones.md:1443`) — its EG-3 13-mode A/B is therefore re-run by M51 regardless of outcome.
  - An early M49 wiring of collision through the per-line pass "diverged Laydock 2 and was corrected" (`milestones.md:1443`) — the sprite dynamic path has already produced one Laydock-visible regression during development.
- **v1.1.7** (ledger corroboration `milestones.md:1443`; no commit hash recorded in the ledger): phosphor-persistence blend shipped explicitly as a **sprite-flicker softener**, with the flicker attributed at the time to "genuine 8-sprites-per-line hardware behavior". That attribution is **re-tested** by M51's oracle — the softener is presentation-only (`src/frontend/phosphor_blend.h`, `sdl3_video_presenter.*`), so all headless frame dumps used by this milestone are persistence-free by construction.
- **M50** (v1.2.0): config externalization; its own gate proved an **empty diff** over `src/devices/video/{v9958_vdp,sprite_engine}.*` (`decisions.md:986`, closure `:997`) — M50 is excluded as an origin candidate for a sprite-pixel defect; it stays in the bisect range only to confirm that.

### 1.3 Candidate attributions (hypotheses — NONE pre-decided; developer attributes by evidence)

Per DEC-0078: (a) an M49 per-line-live watermark/segment-lifecycle regression; (b) an M48 timing-overlay interaction shifting WHEN game ISR writes land relative to sprite segments; (c) the honestly-documented M49 residual (1-frame lag + cycle-timed intra-line ceiling, `docs/m49-planner-package.md` §5) now gameplay-breaking; (d) a distinct latent sprite/R#23 defect. Coordinator recon already established the STATIC sprite-line formula is at tier-2 parity (§2.2), so the DYNAMIC path is the suspect surface.

### 1.4 Assumptions (each carries a verification action)

- **A-M51-1 (verify in Task 0):** the defect reproduces headlessly and deterministically via `--debug-session --frames N --input-script --dump-frame` — the SDL3 window is not a factor. Verify: the Task 0 oracle flags the defect on headless captures of at least Aleste 2 and Firebird.
- **A-M51-2 (verify in Task 0):** part of what the owner sees MAY be authentic MSX sprite-multiplex flicker (real mode-2 hardware shows max 8 sprites/line — tier-1 `references/fact-sheets/Yamaha V9958 VDP.md` §6 line 117 — and games rotate SAT priority to multiplex, producing genuine ~30 Hz flicker). The oracle MUST discriminate: openMSX on equivalent inputs is the arbiter (flicker present in openMSX too = authentic, MUST NOT be "fixed"; flicker/absence only in ours = defect). Note openMSX's own `limitSprites` setting exists (`references/openmsx-21.0/src/video/SpriteChecker.cc:110`) — the A/B leg must run openMSX at its default (limit ON) or the comparison is invalid.
- **A-M51-3 (verify in Task 1):** cross-VERSION comparison cannot use frame-hash equality (M48/M49 legitimately changed timing, so identical scripts reach identical STATES but not identical cycle streams). The bisect verdict is delivered by the ORACLE METRIC (presence/transition statistics over a frame window), with frame-hash equality used only WITHIN a version (determinism gate). Verify: each bisect leg's capture is eyeball-confirmed to have reached gameplay before its metric is trusted.
- **A-M51-4 (verify in Task 2):** instrumentation (env-gated trace) is byte-identical when disabled. Verify: a with/without-env dual run of one capture is hash-equal when the env var is unset.
- **A-M51-5 (assumption, bounded):** the fix lands inside `src/devices/video/{sprite_engine,v9958_vdp}.*` + possibly `src/machine/hbf1xv_machine.cpp` (DEC-0078 expected-edits clause). If attribution forces anything wider, STOP and escalate for a new decision entry before editing.

### 1.5 Non-goals (no scope expansion without a new DEC)

- NO cycle-exact intra-line sprite timing (the C1/D4 banned openMSX per-slot POSITION table remains UNBUILDABLE-WITHOUT-FABRICATION; the honest ceiling of `docs/m49-planner-package.md` §5 stands).
- NO change to M48's tier-1-grounded 154/88/31 slot constants, and NO "fixing" the sprite defect by re-tuning command-engine timing to mask it (see fix branch (b), §3.4).
- NO removal of authentic >8-per-line (mode 2) / >4-per-line (mode 1) sprite multiplex flicker — fact-sheet §6 lines 116-117 makes the per-line limits real hardware behavior.
- NO game-specific hacks — universal spec modeling only (DEC-0078).
- NO `src/core` / `src/devices/cpu` edits expected (ZEXALL then correctly not run; EG-10).
- The v1.1.7 phosphor softener (frontend) is out of scope; it neither causes nor may hide the defect in this milestone's headless evidence.

### 1.6 Authentic-hardware-flicker guard (BINDING on developer and QA)

Real V9958 hardware draws at most 8 sprites per line in mode 2 / 4 per line in mode 1 (tier-1
fact-sheet §6 lines 116-117); real games defeat the budget by rotating SAT priority across frames,
producing genuine on-hardware flicker. That behavior is CORRECT and must survive M51 untouched.
Before classifying ANY observed flicker as our defect, all three prongs:

1. **SAT census:** from the deterministic replay, count sprites per affected line in the live
   attribute table. Affected lines with ≤8 (mode 2) sprites that still vanish → DEFECT. >8 with
   visible priority rotation → candidate AUTHENTIC.
2. **openMSX rate (A/B arbiter):** equivalent inputs, openMSX at default `limitsprites`
   (`SpriteChecker.cc:110`). openMSX alternating the same sprite subsets at the same cadence →
   AUTHENTIC (do not fix). Stable in openMSX where we alternate → DEFECT.
3. **Band shape:** the per-line limit drops only the LOWEST-priority overflow sprites on the
   affected lines; it never empties a whole multi-line band of ALL sprites. Whole-band emptiness
   is always DEFECT.

Firebird likely MIXES both (a shmup wave can genuinely exceed 8/line). The fix target is "our
alternation matches openMSX's", never "no flicker at all"; the fix must not reduce our transition
rate BELOW openMSX's reference rate (R1).

---

## 2. Spec Summary

### 2.1 Behavior contract (target)

For every active-display output line L that ends up in the presented frame, the composited sprite set for L must be the sprite set the V9958 would have fetched for L — from the register/SAT state at L's raster point (per-line-live, tier-1 fact-sheet §6 line 120: "sprite data for line N is fetched during line N−1"), and **never an EMPTY placeholder caused by internal pipeline lifecycle**. A sprite that real hardware displays on line L in frame F must be present in our frame F on line L (up to the accepted, documented M49 residual: +2-line commit quantization and, where the engine legitimately falls back, at worst the pre-M49 1-frame-stale sprite set — never absence). Authentic per-line-limit multiplex dropping (4/line mode 1, 8/line mode 2, fact-sheet §6 lines 116-117) is preserved exactly.

### 2.2 Grounding (read this cycle; cite in the implementation report)

- **Tier-1** `references/fact-sheets/Yamaha V9958 VDP.md`: §6 lines 116-120 (mode-1 max 4/line; mode-2 max 8/line + 5S/C semantics; SAT/colour-table layout + bit-9 AND-mask; colour-0 transparency; pitfalls: fetch pattern always runs, **line N fetched during N−1**, 1-pixel vertical shift); §5 line 77 (R#23 = vertical scroll display offset); §10 line 178 (scroll-exactness reliance — Space Manbow, F1 Spirit 3D class); §8 line 163 (154/88/31 slots — the M48 surface, context for attribution (b)).
- **Tier-2** openMSX (EFFECT only — never copy):
  - `references/openmsx-21.0/src/video/SpriteChecker.cc:104-127` — `displayDelta = verticalScroll − lineZero`; `spriteLine = (displayLine − y) & 0xFF`; sprites "checked at" one line above display; sentinel 208 (mode 1; 216 mode 2 elsewhere in the file). This MATCHES our static formula `((line - 1) + r23 - y) & 0xFF` at `src/devices/video/sprite_engine.cpp:263-267` (R#23 participation, checked-one-earlier, sentinel) — the static path is at parity, so M51's suspect surface is the DYNAMIC lifecycle below. Also `SpriteChecker.cc:110` (`limitSprites` setting — A/B knob, A-M51-2).
  - **The producer-side sync protocol:** `SpriteChecker.hh:78-102` (`sync(time)` = VRAM sync + `checkUntil(time)`); `:137-201` — every sprite-relevant update (`updateDisplayEnabled` / `updateSpritesEnabled` / `updateSpriteSizeMag` / `updateTransparency` / `updateVerticalScroll`) syncs sprites to NOW with the OLD value before the change applies; `:269-271` — every VRAM write drives `checkUntil(time)`. Our M49 seam implements this side.
  - **The CONSUMER-side sync contract (key tier-2 finding of this planning pass — the side our M44 command-row sink does NOT implement):** `SpriteChecker.hh:242-247` — `getSprites(line)` "returns the contents of the line the last time it was sprite checked; **before getting the sprites, you should sync to a moment in time after the sprites are checked, or you'll get last frame's sprites**" — openMSX makes the CONSUMER responsible for pacing the checker; and `references/openmsx-21.0/src/video/PixelRenderer.cc:580-584` — `renderUntil()` calls `spriteChecker.checkUntil(time)` ("Update sprite checking, so that rasterizer can call getSprites") BEFORE rasterizing, i.e. **every render advance paces sprites first, regardless of what triggered the render** (register write, VRAM write, command engine, frame end). Directly relevant to H1 (§2.3) and fix shape (a)-(i) (§3.4).
- **Tier-3** fMSX: per-scanline sprite selection is fused with line rendering — `Sprites(Y, Line)` / `ColorSprites(Y, ZBuf)` (`references/fmsx-60/source/fMSX/Common.h:18-19`, `:99-155`, `:247-284`) select and draw sprites at the moment each scanline is refreshed, reading the live attribute table and `VScroll` (`:281`); the per-scanline pipeline is already cited by `src/devices/video/vdp_scanline_accumulator.h:36-38` → `references/fmsx-60/source/fMSX/MSX.c:2149-2155`. A consumer can therefore never outrun the checker in fMSX — independent triangulation of the §2.1 invariant.

### 2.3 The dynamic path as it exists today (planner code reading — the suspect surface, all verified file:line)

1. **Per-frame lifecycle:** `V9958Vdp::on_vsync()` (`src/devices/video/v9958_vdp.cpp:724-765`) ALWAYS runs the frame-atomic `SpriteEngine::recompute_frame()` (`:761`) — collision/S#0 stay frame-atomic (DEC-0031) — and rolls `sprite_split_active_ = false` (`:762`).
2. **Lazy-open render split:** the FIRST active-display VDP write of a frame reaches `Hbf1xvMachine::VdpRenderSyncAdapter::on_before_state_change()` (`src/machine/hbf1xv_machine.cpp:53-136`), which calls `commit_sprite_split(target = line + 2, wrap)` (`:113`) BEFORE the background `sync_to_line(target)` (`:130-134`). `commit_sprite_split` (`v9958_vdp.cpp:698-720`) lazy-opens via `begin_sprite_frame()` → `SpriteEngine::begin_frame()` — which **clears ALL per-line visible buffers** (`sprite_engine.cpp:93`, `lines_.assign(height, {})`) — then `check_until_visible_only(target - 1)` repopulates only `[1, target)` from LIVE state (`sprite_engine.cpp:158-187`), advance-only watermark.
3. **Compositing point:** sprites are composited INTO each background row at the moment that row is rendered/committed — `VdpFrameRenderer::render_line()` → `composite_sprites()` → `sprite_engine().visible_sprites(line)` (`src/devices/video/vdp_frame_renderer.cpp:519-520`, `:381-392`). A row committed while its sprite line-buffer is empty is sealed sprite-less for the whole frame (the accumulator store never re-renders a committed row).
4. **The M44 command-row sink:** `on_commit_up_to()` (`hbf1xv_machine.cpp:138-158`) — driven per VDP-command destination row from `command_row_sync` (`v9958_vdp.cpp:667-671`) — advances the background accumulator **past the beam** and commits those rows, but **does NOT drive the sprite pass** (`v9958_vdp.cpp:714-716`: "no command-row sink drives the sprite watermark") and does not update `last_beam_commit_target_`.
5. **Vsync finalize:** machine `on_vsync_boundary()` runs `vdp_.on_vsync()` (atomic recompute repopulates the buffers) BEFORE the accumulator `finalize()` — so only rows NOT yet committed render from the fresh atomic result (`v9958_vdp.cpp:750-762` comment).

**Named mechanism hypotheses inside this path (hypothesis-grade; Task 2 confirms/eliminates by trace — none pre-decided):**

- **H1 — command-sink / sprite-watermark gap (primary named suspect, attribution (a)).** After the lazy-open CLEAR (step 2), any command-row commit (step 4) whose destination row lies ABOVE the sprite watermark composites that row from an **empty** sprite line-buffer and seals it sprite-less for the frame (step 3). Scroll-blit-heavy titles (exactly these three KonamiSCC/disk shooters) commit playfield rows via VDP commands every frame; whether the player-sprite rows are swept by a blit before the beam reaches them varies frame-to-frame → wholesale sprite absence that toggles with scrolling. Predicts: disappearance FIRST at v1.1.6; correlation with command activity, not sprite count; ground-locked (background) objects clean. Pre-M49 builds cannot show it (buffers were always the fully-populated vsync recompute). Note Aleste 2 opens the split EVERY gameplay frame (it drives R#1 BL low at display line 13 each frame — M39 calibration `hbf1xv_machine.cpp:85-95`, corroborated by `tools/capture-aleste-play-evidence.ps1:49-51`), so the CLEAR precondition is permanently armed in this title. This is exactly the CONSUMER-side sync contract openMSX documents and implements (`SpriteChecker.hh:242-247`, `PixelRenderer.cc:580-584` — §2.2) and fMSX makes structural (§2.2 tier-3): all three reference tiers agree a consumer must never read an unchecked sprite line. **Still a hypothesis until the Task 2 trace shows the empty-composite event on a failing frame.**
- **H2 — segment-gate quantization (attribution (a)).** `check_until_visible_only` applies ONE live R#1 BL / R#8 SPD / mode gate to the whole segment `[lo, end)` (`sprite_engine.cpp:179-183`). Segments are delimited by VDP writes and evaluated pre-write, so this is correct-by-construction for write-paced toggles — but a mis-ordered gate window would blank whole segments. Trace decides; lower prior than H1.
- **H3 — spurious wrap re-open (attribution (a)/(d)).** `commit_sprite_split(target, target < last_beam_commit_target_)` re-opens `begin_frame` (full clear) on a raster DECREASE. A mid-frame `raster_display_line()` jump (e.g. an R#9 LN toggle moves `non_active_lines` 70↔50, `v9958_vdp.cpp:816-818`) could fake a wrap and clear mid-frame. Trace decides; low prior (no known LN-toggling in these titles).
- **H4 — per-line-limit misfire across segments (attribution (a)).** The `max_per_line` cap and 5th-sprite tracking under segmented checking (`sprite_engine.cpp:272-279`). Advance-only watermark means each line is checked in exactly one segment, so no double-count path is visible statically; trace confirms. Low prior.
- **H5 — M48 phase shift (attribution (b)).** M48's CE-window changes shift when game ISR polling loops resume, moving WHERE writes (and command-row commits) land relative to the beam — which modulates H1's gap geometry but should not itself empty a buffer. If the bisect flags v1.1.5 as first-bad under the oracle, this branch takes over.

### 2.4 Dependency map (boundaries touched)

| Layer | File(s) | Role in M51 |
|---|---|---|
| `src/devices/video` | `sprite_engine.{h,cpp}` | PRIMARY suspect + likely fix site (per-line buffer lifecycle, `check_until_visible_only`, `begin_frame` clear discipline). |
| `src/devices/video` | `v9958_vdp.{h,cpp}` | Split lifecycle (`begin_sprite_frame`/`commit_sprite_split`/`on_vsync`), `command_row_sync`; possible fix site + Task 2 trace hooks. |
| `src/machine` | `hbf1xv_machine.cpp` | M32 seam + M44 command-row sink (`on_before_state_change` / `on_commit_up_to`); possible fix site (driving the sprite pass from the sink). |
| `src/devices/video` | `vdp_frame_renderer.*`, `vdp_scanline_accumulator.*` | READ-ONLY consumers/reference — NOT expected to change; any need to edit them is an escalation (A-M51-5). |
| `src/frontend`, `src/core`, `src/devices/cpu` | — | ZERO edits expected (EG-10 relies on the empty cpu/core diff). |
| `tools/` | new `m51-*` capture/oracle scripts (PowerShell + Python) | Task 0/1 deliverables (allowed per CLAUDE.md tools scope). |
| `debug/m51/` | captures, traces, bisect legs | Evidence root (create subfolders as needed). |

### 2.5 Existing harness inventory (verified present; reuse, do not reinvent)

- Deterministic Aleste 2 gameplay recipe: `tools/capture-aleste-play-evidence.ps1` + `tools/aleste-play-evidence-input.script` (SPACE holds at frames 600/1500/2100/2700 → gameplay by ~f2900; double-run byte-identity pattern built in). NOTE: its default `-RomPath roms/aleste.rom` is stale — pass `games/roms/Aleste 2/aleste2.rom`.
- Headless drive/capture: `--debug-session <bios> <steps> --slot1 <rom> [--slot1-type KonamiSCC] --frames N --input-script <s> --dump-frame <name>` (`src/main.cpp:735-745`, `:919-931`); disk legs via `--disk` + `--swap-disk-frame` (`:772-777`); consecutive frames = one deterministic run per frame index (the established recipe pattern — determinism makes runs prefix-identical). `--input-frame-start` (`:830-840`) exists for live-recording-faithful replay; hand-authored coarse scripts do not need it.
- Frame → image/analysis: `tools/frame-to-png.py`, `tools/frame-zoom-crop.py`, `tools/frame-row-ascii.py`.
- openMSX A/B: `tools/m41-run.ps1` (WSL `openmsx -machine Sony_HB-F1XV`, screenshot capture, diff via `tools/m38-ab-diff.py`) — the 13-mode M41 screen A/B harness (manifest `tools/m41-scenarios.json`); `tools/openmsx-ab-smoke.ps1` for the standing smoke gate. Laydock precedent: `tools/laydock-sweep.sh`; Aleste openMSX HUD-capture precedent cited at `hbf1xv_machine.cpp:85-86` (`debug/m39-hud/omsx_hud_*.png`).
- Single-build-tree rebuilds: `tools/bootstrap-build.ps1` (M33/DEC-0041 — bisect legs rebuild `build/` sequentially, NEVER parallel trees).
- Instrumentation precedent: the M47 env-gated per-byte FDC write logger (DEC-0072 closure, `current-phase.md:129-133`) — default-off, byte-identical when unset.

---

## 3. Milestones (diagnostic-first task/slice breakdown)

Sequencing is HARD: Task 0 → Task 1 → Task 2 → (conditional) Fix slices F1-F3 → closure. No fix code before Task 2's attribution verdict is written down.

### Task 0 — deterministic repro + objective sprite-presence/flicker oracle (NO src edits)

- **T0.1 Captures.** For each title, a deterministic headless capture of **N ≥ 30 consecutive gameplay frames** during scroll (per-frame runs at increasing `--frames`, `--dump-frame`, first+last frame double-run hash-asserted):
  - Aleste 2 (MUST): reuse the existing recipe/script; window ≈ f2900-f3600 (gameplay, plane idle at spawn — no movement input after entry).
  - Firebird (MUST): author `tools/m51-firebird-input.script` (title → start → idle gameplay; discovery in dev); pick a window where the player character is stationary.
  - Laydock 2 (SHOULD — "if workable" per DEC-0078): disk boot from `games/disks/laydock_2/laydock2-1.dsk`; reuse `tools/laydock-sweep.sh` learnings.
  - Deliverable: `tools/m51-capture-frames.ps1` (parameterized wrapper: rom/disk, script, F0, N, outdir `debug/m51/<title>/`).
- **T0.2 Oracle.** `tools/m51-sprite-oracle.py`: given the frame sequence + a pinned bounding box + a pinned sprite-colour signature (both derived once from a verified present-frame and recorded as constants in the tool), emit per-frame `presence(f)` (fraction of box pixels matching the signature), classify present/absent by a fixed threshold, and report: absent-frame count, longest absent run, present↔absent transition count per window. Output CSV + verdict line.
- **T0.3 Adversarial oracle checks (the oracle must be falsifiable both ways):**
  1. a known-stable scene (Aleste 2 weapon-select f2600 — byte-stable per `tools/capture-aleste-play-evidence.ps1` history note) yields 0 transitions;
  2. the current build (v1.2.0-era main) yields a FAILING verdict on at least Aleste 2 or Firebird (else the repro does not capture the owner's defect — STOP, escalate, do not proceed to a fix);
  3. the openMSX leg (T0.4) is run through the SAME oracle unmodified.
- **T0.4 openMSX discriminator leg (authentic-multiplex arbiter, A-M51-2).** `tools/m51-openmsx-frames.ps1`: WSL openMSX `-machine Sony_HB-F1XV`, equivalent coarse timed inputs, per-frame `screenshot -raw` sequence over an equivalent gameplay window (m41-run.ps1 + m39-hud precedent), same oracle box/signature (rescaled as needed). Records openMSX's own transition rate per title/scene — the reference rate that separates authentic multiplex flicker from our pathological absence.

### Task 1 — attribution bisect (v1.1.4 → v1.1.5 → v1.1.6 → v1.2.0; NO src edits)

- Legs: tags **v1.1.4** (`2e09040`, pre-M48) → **v1.1.5** (`e7a7a41`, M48) → **v1.1.6** (`0d84451`, M49) → **v1.2.0** (`0bf3285`; M50 — expected identical to v1.1.6 for sprites: its video diff was gate-proven empty, §1.2). Hashes of record = REQ-M51-001 (`agent-protocol/channels/requests.md:1836`). **Pre-flight (mandatory):** `git rev-parse <tag>^{commit}` for all four tags; on any mismatch with the ledger hashes, the TAG is authoritative — record the discrepancy in the implementation report and proceed on tags. Also verify `git diff v1.1.6..v1.2.0 -- src/devices/video src/machine` is sprite-neutral before treating v1.2.0 as the M49-equivalent leg. Add intermediate commits only if adjacent legs disagree unexpectedly.
- Per leg: clean detached-HEAD checkout → `tools/bootstrap-build.ps1` rebuild of the ONE `build/` tree (single-build policy, DEC-0041 — sequential rebuilds, never parallel trees) → run the SAME Task 0 script+capture+oracle → copy outputs to `debug/m51/bisect/<tag>/` BEFORE switching → restore `main` at the end. Worktree discipline: no stray edits, no `git checkout -- .` on tracked state (M35 QA-clobber incident precedent), verify `git status` clean before each switch.
- Cross-version comparison by ORACLE METRIC only (A-M51-3); eyeball-confirm each leg reached gameplay.
- **Verdict (written into the implementation report):** first-bad milestone per title, or "pre-existing at v1.1.4" — mapping onto attribution (a)/(b)/(c)/(d). Expectation table (falsifiable): H1 predicts clean v1.1.4/v1.1.5 and first-bad v1.1.6; H5 predicts first-bad v1.1.5; pre-existing-at-v1.1.4 pushes toward (c)/(d).

### Task 2 — root-cause instrumentation (env-gated trace; first allowed src edits, additive only)

- Add an env-gated trace (`SONY_MSX_M51_SPRITE_TRACE=<path or 1>`; M47 logger precedent) confined to `sprite_engine.*` / `v9958_vdp.*` / `hbf1xv_machine.cpp`, default-off and byte-identical when unset (A-M51-4 dual-run proof). Per frame, log:
  1. `begin_sprite_frame` lazy-opens/wrap-re-opens (raster line, target, `wrap` flag, prior watermark);
  2. every `check_until_visible_only` segment `[lo, end)` with the live gate states (R#1 BL, R#8 SPD, sprite mode) and R#23;
  3. every `on_commit_up_to` background commit `[acc_watermark, display_line)` **with the sprite watermark at that instant**;
  4. at each committed row in a designated line band (the title's player rows), `visible_sprites(line).size()` at composite time;
  5. the `on_vsync` recompute + accumulator-finalize boundary.
- Run one failing capture per affected title; the trace must name the concrete mechanism (e.g. for H1: "frame F: row 121 committed by on_commit_up_to with sprite watermark 17 → composited 0 sprites"). Confirm or eliminate H1-H5 explicitly; if all five die, widen the trace (SAT write classification, R#23 values per segment) until the mechanism is concrete. Trace outputs under `debug/m51/traces/`.
- **Exit:** a written attribution — mechanism, file:line, first-bad milestone, per-title mapping, and which fix branch (§3.4) applies. Also disposition per title how much of the observed flicker is authentic multiplex (openMSX leg): that portion is documented, not fixed.

### Expected-signal table (what each outcome implies — the attribution OUTPUT selects the fix slice)

| Signal (Task 1 bisect × Task 2 trace) | Implication | Fix branch (§3.4) |
|---|---|---|
| First-bad **v1.1.6**; trace shows empty-composite events (`on_commit_up_to` rows past the sprite watermark after a lazy-open clear) coinciding with vanish frames | **H1 confirmed** — M49 wiring gap (attribution (a)) | **(a)**, shape (i)/(ii) |
| First-bad **v1.1.6**; trace shows NO command-sink outrun, but segment gates blanking whole segments / spurious wrap re-opens / per-line-limit misfires | **H2/H3/H4** — other M49 lifecycle defect (attribution (a)) | **(a)**, shape per trace |
| First-bad **v1.1.5** (flicker/misposition — full disappearance is NOT expected pre-M49, buffers were always populated; disappearance at v1.1.5 means the oracle or capture is suspect — re-verify before concluding) | **H5** — M48 timing-overlay interaction (attribution (b)) | **(b)** |
| Symptom already present at **v1.1.4** | Latent defect (attribution (d)), or the (c) residual if behavior matches the documented ceiling | **(d)** (or **(c)**) |
| v1.1.6 first-bad AND v1.1.5 measurably worse than v1.1.4 on the transition metric | Mixed cause — M48 shifted write-landing geometry, M49 added the absence mechanism | **(a) first**, re-run Task 0, then **(b)** only on the proven residual |
| Post-fix residual bounded to ±2-line quantization jitter, openMSX-A/B-equivalent, AND owner live play is clean | **(c)** — the accepted M44 Phase 2b / M49 ceiling | **(c)** documentation close (no code) |
| openMSX leg shows the SAME alternation at the same cadence for a title/scene | That portion is AUTHENTIC multiplex (§1.6) | none — document, do not fix |

### 3.4 Fix slices (CONDITIONAL on the Task 2 verdict — universal spec modeling only, never game-specific)

**Branch (a) — M49 lifecycle defect confirmed (e.g. H1/H2/H3/H4):**

- **F1(a) — engine/lifecycle correction.** Invariant to establish: *no framebuffer row is ever composited from a sprite line-buffer that is empty purely because of pipeline lifecycle.* Candidate universal shapes (developer selects by trace evidence, may combine; document the choice + grounding):
  - (i) drive the sprite visible-only pass from the command-row sink too — `on_commit_up_to` advances sprites to its destination row before the accumulator commits (mirrors M44's already-accepted background approximation; keeps sprite and background state co-timed per committed row; this is openMSX's own consumer-side protocol — every render advance paces the sprite checker first, `PixelRenderer.cc:580-584` / `SpriteChecker.hh:242-247`, §2.2 — re-expressed at our seam, never transcribed). Decide + document whether a command commit may LAZY-OPEN the split when it is not yet active (in the real CPU path the triggering `OUT` already opened it via `on_before_state_change`, so advance-only-when-active is expected sufficient; prove with the Task 2 trace). The same invariant guard belongs on the `render_frame()` mid-display/step-only sync paths (`hbf1xv_machine.cpp:1022-1036`) so capture/debug callers obey it too;
  - (ii) replace `begin_frame`'s clear-all-at-open (`sprite_engine.cpp:93`) with clear-per-line-at-check, so lines not yet re-checked retain the last vsync recompute — at worst the pre-M49 1-frame-stale set, NEVER empty (the documented fallback contract of §2.1);
  - (iii) for H3: harden wrap detection against non-wrap raster jumps.
  May touch ONLY `sprite_engine.*`, `v9958_vdp.*`, `hbf1xv_machine.cpp`. MUST NOT touch: the DEC-0031 frame-atomic collision path (`recompute_frame` at `on_vsync`, S#0/S#3-S#6, `sync_collision_to_raster`), the static per-line formulas (`sprite_engine.cpp:263-307`), mode-2 base-mask/planar addressing, the M44 background watermark discipline, any M48 timing constant.
- **F2(a) — deterministic tests.** New unit/integration oracles reproducing the mechanism pre-fix and locking the fix (see §4 test obligations), plus adversarial-revert proof (re-introducing the defect fails the new oracle AND the Task 0 title oracle).
- **F3(a) — title evidence + full gates.** Before/after Task 0 captures for Aleste 2 + Firebird (+ Laydock 2 if workable) under `debug/m51/`; all EGs (§4).

**Branch (b) — M48 interaction confirmed (first-bad v1.1.5):** the fix still lands on the SPRITE side — the sprite pipeline must be robust to legitimate, tier-1-grounded command timing. FORBIDDEN: changing 154/88/31, `vdp_access_timing.h`, or any DEC-0069/M44/M48 pacing value to mask the symptom. Slices mirror F1-F3(a) with the mechanism restated as "timing-exposed sprite-lifecycle sensitivity"; if the trace instead proves an M48 arithmetic BUG (not a sensitivity), escalate to the coordinator for a decision entry before touching M48 code (outside DEC-0078's expected-edit clause).

**Branch (c) — M49 residual as-designed proving gameplay-breaking (defect present at v1.1.4 or behavior matches the documented ceiling):** allowed remedy = tighter line-granular fetch discipline within the EXISTING seams (e.g. denser check_until driving), staying line-quantized; the cycle-exact per-slot model stays banned (C1/D4). If no in-ceiling remedy exists, M51 closes as diagnostic-only with an honest verdict doc + backlog row and a coordinator escalation — do NOT ship a fake fix.

**Branch (d) — distinct latent defect:** scope confined to the same three files; the concrete mechanism from Task 2 defines the slice; same test/gate obligations as branch (a). If the mechanism lives outside the expected-edit files, STOP and escalate (A-M51-5).

---

## 4. Acceptance Criteria

### Per-task acceptance

- **AC-T0:** deterministic repro exists per title (Aleste 2 + Firebird MUST; Laydock 2 SHOULD): scripts + capture tool + oracle committed under `tools/`, captures under `debug/m51/`; the oracle FAILS on current main for ≥1 title (repro genuinely captures the defect) and passes the adversarial checks §3 T0.3 (stable-scene zero-transitions + openMSX leg processed by the identical oracle). Double-run determinism (same script → byte-identical first/last frame dumps) proven per title.
- **AC-T1:** bisect verdict recorded with per-leg oracle CSVs under `debug/m51/bisect/<tag>/`: first-bad milestone per title (or pre-existing), each leg eyeball-confirmed in-gameplay, `main` restored, single `build/` tree used throughout.
- **AC-T2:** written root-cause attribution naming the concrete mechanism (file:line), confirming/eliminating H1-H5 explicitly, mapping to branch (a)/(b)/(c)/(d), and dispositioning the authentic-multiplex share per title via the openMSX rate. Trace is env-gated and proven byte-identical when off (A-M51-4).

### Fix acceptance (branch-generic)

- **AC-F1 (defect resolved):** post-fix, the Task 0 oracle over the same windows shows: Aleste 2 — zero absent frames and zero wholesale-absence runs for the player plane during scroll; Firebird — player-character transitions reduced to ≤ the openMSX reference rate for the equivalent scene (authentic multiplex may remain; pathological absence may not); Laydock 2 (if repro'd) — same criterion as Aleste 2.
- **AC-F2 (no behavioral collateral):** for a frame with no mid-frame sprite-relevant activity the per-line visible buffers are byte-identical to pre-fix (the M49 AC-S1 invariant re-held); the sprite/background split still coincides at the +2 boundary (M49 AC-S2 oracle green); DEC-0031 collision/5th-sprite/S#0/S#3-S#6 byte-identical.
- **AC-F3 (tests):** new deterministic oracles lock the mechanism: (unit) a synthetic sequence reproducing the attributed mechanism — e.g. for H1: lazy-open at line L, command-row commit at row R > sprite watermark, assert `visible_sprites(R)` equals the hardware-expected set, failing pre-fix; (integration) a machine-level scroll scenario (mid-frame R#23/SAT writes + VDP command rows across ≥3 frames) asserting stable sprite presence + frame-hash determinism. Non-tautology: an adversarial revert of the fix fails ≥1 new oracle.

### Evidence gates (run and capture — never fabricate)

- **EG-1 (build):** `cmake --build build --config Debug` succeeds for BOTH executables (SDL3=ON superset).
- **EG-2 (full fast-subset ctest — MANDATORY):** `ctest --test-dir build -C Debug --output-on-failure` fully green; EVERY existing sprite/M22/M32/M44/M48/M49 oracle green and UNMODIFIED (`video_sprite_engine_mode1/mode2/mode2_attribute_masking/collision_relatch`, `video_vdp_frame_renderer_sprites`/`_sprite_edge_clip`, `hbf1xv_m22_sprites_integration`, `hbf1xv_m22_sprites_command_engine_system`, `hbf1xv_m32_*`, the M44 command-row and M48 command-timing oracles, the M49 no-mid-write/split-coincidence oracles) — NONE weakened; any re-derivation requires written justification in the implementation report, never a silent rebaseline.
- **EG-3 (13-mode 0.000% screen A/B vs openMSX — MANDATORY):** the full M41 harness (`tools/m41-run.ps1` + `tools/m41-scenarios.json` + `tools/m38-ab-diff.py`); all 13 modes 0.000%. (M49 closed without re-running this gate — `milestones.md:1443` — so M51 owes it unconditionally.)
- **EG-4 (DEC-0031 collision byte-identity — MANDATORY):** boot-logo collision-relatch path + collision/S#0/S#3-S#6 byte-identical to pre-M51.
- **EG-5 (title before/after captures — MANDATORY):** deterministic before/after frame sequences + oracle CSVs for Aleste 2 and Firebird (+ Laydock 2 if workable) under `debug/m51/`, plus the openMSX discriminator leg outputs.
- **EG-6 (determinism):** the same script → byte-identical frame hashes across two runs, per title, post-fix.
- **EG-7 (asset + checksum):** `tools/validate-assets.ps1` PASS; `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` refreshed.
- **EG-8 (instrumentation hygiene):** trace env-gated; unset-env dual run byte-identical (A-M51-4).
- **EG-9 (ban attestation):** QA greps `src/` — no openMSX `SpriteChecker`/`VDPAccessSlots` code or table transcription (the standing never-copy rule; the C1/D4 table ban).
- **EG-10 (ZEXALL cadence):** NOT run unless `src/devices/cpu`/`src/core` are touched (not expected); QA attests the empty diff as substitution rationale.
- **EG-11 (scope attestation):** `git diff` confined to `src/devices/video/{sprite_engine,v9958_vdp}.*`, `src/machine/hbf1xv_machine.cpp`, `tools/m51-*`, `tests/` additions; anything wider carries a coordinator decision entry.

### Exit criteria

All AC-T*/AC-F* met (or the branch-(c) diagnostic-only closure honestly recorded), EG-1..EG-11 captured, QA sign-off, then the **NORMAL human-release gate** (HIGH blast radius — all sprite rendering, all 13 modes; NO auto-close). Owner live spot-check of all three titles recommended, ideally with `--persistence off` (or persistence-mode default noted) so the softener cannot mask residuals.

---

## 5. Risks

- **R1 — Misclassifying authentic multiplex flicker (both directions).** Real mode-2 hardware drops >8 sprites/line (fact-sheet §6 line 117) and games multiplex — some Firebird enemy flicker may be genuine, and v1.1.7's ledger already once attributed the symptom to genuine behavior (`milestones.md:1443`). Mitigation: the openMSX-rate discriminator (A-M51-2, openMSX `limitsprites` at default), per-title disposition in the Task 2 verdict; the fix must not reduce our transition rate BELOW openMSX's.
- **R2 — Cross-version bisect desync.** M48/M49 changed timing; scripts may reach different states on old tags. Mitigation: coarse input-gated menu scripts, oracle-metric (not hash) comparison, per-leg eyeball confirmation (A-M51-3).
- **R3 — HIGH blast radius of any fix.** Mitigation: the AC-F2 byte-identity invariants, EG-2 unmodified oracle wall, EG-3 13-mode A/B, EG-4 collision identity, normal human-release gate.
- **R4 — Collision regression (repeat offender).** M49's own dev history already diverged Laydock 2 once via collision wiring (`milestones.md:1443`). Mitigation: the frame-atomic collision path is on the F1 MUST-NOT-TOUCH list unless the attribution demands it (then EG-4 still binds byte-identity or an honestly-justified re-derivation with coordinator sign-off).
- **R5 — Performance.** Driving the sprite pass from the command sink (fix shape (a)-(i)) adds per-row sprite sweeps in command-heavy frames. Mitigation: advance-only watermark bounds total work to ≤ one full-frame sweep per frame; profile if a slowdown appears.
- **R6 — Oracle brittleness.** Colour-signature presence detection can drift with palette/scene changes. Mitigation: pinned boxes/signatures from verified frames, stable-scene adversarial check, thresholds recorded in the tool, double-run determinism.
- **R7 — Laydock 2 repro cost (disk boot, possible swap).** It is the SHOULD leg per DEC-0078 ("if workable"); Aleste 2 + Firebird suffice for attribution. If skipped, record why + owner live-check covers it at release.
- **R8 — Instrumentation perturbation.** Mitigation: env-gated default-off + EG-8 byte-identity proof.
- **R9 — Fix ambiguity if multiple mechanisms co-exist** (e.g. H1 plus a genuine residual). Mitigation: fix one mechanism per slice with its own adversarial oracle; re-run the title oracle after each; do not blend uncommitted hypotheses into one edit.
- **R10 — Wall-clock cost of per-frame capture runs** (~30-40 runs × ~3-6k frames per title per leg). Mitigation: N=16 minimum for bisect legs, N≥30 for before/after evidence; captures are embarrassingly scriptable and re-runnable.

### Honest residuals (stated up-front so QA does not misfile them)

- The M49 line-granular ceiling stands: splits stay +2-line-quantized; cycle-timed intra-line effects stay line-quantized (human-accepted M44 Phase 2b boundary; cycle-exact needs the banned slot table). M51 does not exceed it.
- If fix shape (a)-(ii) (stale-fallback) is chosen, rows can render a 1-frame-stale sprite set in corner cases — the exact pre-M49 behavior, visible at worst as lag, never absence. Document if adopted.
- Authentic multiplex flicker remains after the fix wherever openMSX shows it too — that is the hardware.

---

## 6. Developer Handoff

**Milestone:** M51 (DEC-0078) — diagnostic-first. PLANNING → implement. Reference precedence DEC-0073. Expected edits confined to `src/devices/video/{sprite_engine,v9958_vdp}.*` + possibly `src/machine/hbf1xv_machine.cpp`, plus `tools/m51-*` and new tests. ZERO `src/core`, ZERO `src/devices/cpu`. Never copy openMSX/fMSX code; read `SpriteChecker.cc` for effect only.

**Entry point — Task 0 first (no src edits):**
1. Build current main (`tools/bootstrap-build.ps1`; SDL3=ON superset).
2. Aleste 2 captures: adapt `tools/capture-aleste-play-evidence.ps1` (+ its input script) with `games/roms/Aleste 2/aleste2.rom`; capture ≥30 consecutive gameplay frames ~f2900+ into `debug/m51/aleste2/`.
3. Author the Firebird script + captures; Laydock 2 if workable.
4. Write `tools/m51-sprite-oracle.py` + `tools/m51-capture-frames.ps1`; run the adversarial checks (§3 T0.3) — the oracle MUST fail on current main for ≥1 title before anything else proceeds.
5. openMSX discriminator leg (`tools/m51-openmsx-frames.ps1`, WSL, machine Sony_HB-F1XV, default `limitsprites`).

**Then Task 1 (bisect: v1.1.4 `2e09040` → v1.1.5 `e7a7a41` → v1.1.6 `0d84451` → v1.2.0 `0bf3285` — REQ-M51-001 hashes of record, tags authoritative on mismatch; sequential rebuilds of the ONE `build/` tree, DEC-0041 — never a parallel build tree in the repo; outputs to `debug/m51/bisect/<tag>/`; restore `main`).**

**Then Task 2 (env-gated `SONY_MSX_M51_SPRITE_TRACE`, log the five event classes of §3 Task 2, name the mechanism, confirm/eliminate H1-H5, write the attribution).** The primary named suspect to check FIRST against the trace: **H1** — `on_commit_up_to` (`hbf1xv_machine.cpp:138-158`) commits background rows past the sprite watermark after `begin_frame`'s clear (`sprite_engine.cpp:93`), so `composite_sprites` (`vdp_frame_renderer.cpp:519-520`, `:381-392`) seals those rows sprite-less (`v9958_vdp.cpp:714-716` documents that the sink deliberately does not drive the sprite watermark). It is a hypothesis until the trace shows the empty-composite event on a failing frame.

**Then the conditional fix branch (§3.4) selected by the verdict, one mechanism per slice, with the AC-F oracles and EG-1..EG-11.** Closure: QA sign-off → NORMAL human-release gate (no auto-close); backlog D9's disposition is amended per findings (an M49-regression verdict REPAIRS D9's closure, it does not reopen scope — DEC-0078).

**Grounding index (all paths opened during planning):**
- Ledger: `agent-protocol/channels/decisions.md:999-1010` (DEC-0078); `agent-protocol/state/current-phase.md:9-31` (M51 active block); `agent-protocol/state/milestones.md:1429` (M48), `:1431-1443` (M49 + v1.1.7 note), `:1445-1457` (M50 empty-video-diff), `:1459-1471` (M51 entry).
- Prior package: `docs/m49-planner-package.md` (§5 residuals, A-M49-5 Laydock sub-question, EG list reused).
- Code (dynamic path): `src/devices/video/sprite_engine.cpp:74-115` (begin_frame/clear), `:117-187` (check_until / visible-only), `:189-369` (process_segment; formula `:263-267`; per-line cap `:272-279`), `:410-433`; `src/devices/video/sprite_engine.h:78-129`, `:212-225`; `src/devices/video/v9958_vdp.cpp:667-671` (command_row_sync), `:674-720` (sprite split lifecycle), `:724-765` (on_vsync), `:805-819` (raster_display_line); `src/machine/hbf1xv_machine.cpp:53-136` (seam, +2 at `:101`, sprite drive `:113`), `:138-158` (command sink); `src/devices/video/vdp_frame_renderer.cpp:381-392`, `:519-520`; `src/devices/video/vdp_scanline_accumulator.h:36-119`.
- Tier-1: `references/fact-sheets/Yamaha V9958 VDP.md` §5 line 77, §6 lines 116-120, §7 lines 124-126, §8 line 163, §10 line 178.
- Tier-2: `references/openmsx-21.0/src/video/SpriteChecker.cc:104-127` (+ `:110` limitSprites); `SpriteChecker.hh:78-102`, `:137-201`, `:242-247` (consumer-side sync contract), `:269-271`; `references/openmsx-21.0/src/video/PixelRenderer.cc:580-584` (renderer paces sprite checking before rasterizing).
- Tier-3: `references/fmsx-60/source/fMSX/Common.h:18-19`, `:99-155`, `:247-284` (`Sprites()`/`ColorSprites()` per-scanline selection at line-render time), `references/fmsx-60/source/fMSX/MSX.c` per-scanline pipeline (via `vdp_scanline_accumulator.h:36-38`).
- Harness: `tools/capture-aleste-play-evidence.ps1`, `tools/aleste-play-evidence-input.script`, `tools/m41-run.ps1`, `tools/m41-scenarios.json`, `tools/m38-ab-diff.py`, `tools/frame-to-png.py`, `tools/laydock-sweep.sh`, `tools/bootstrap-build.ps1`, `tools/validate-assets.ps1`, `tools/checksum-assets.ps1`; CLI surface `src/main.cpp:735-745`, `:772-777`, `:830-840`, `:919-931`.
