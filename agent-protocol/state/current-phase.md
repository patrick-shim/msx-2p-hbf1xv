# Current Phase

- Objective: **Deliver the production Sony HB-F1XV MSX2+ emulator to end-goal fidelity — every game
  behaving like real hardware** (project memory `project_end_goal_all_games.md`). Current work is
  device-fidelity hardening carried out under the autonomous DEC-0072/M47 investigation. All behavior
  grounding now follows the **reference-precedence directive (DEC-0073)**: real-hardware spec (tier 1)
  > openMSX + own spec-disciplined reasoning (tier 2) > fMSX (tier 3); on a confirmed conflict the
  hardware spec wins and the openMSX divergence is documented.
- Active Phase: **NONE OPEN — M54 DONE (2026-07-16, DEC-0081-CLOSURE) + DEC-0082 PUBLISHING.**
  macOS support landed via the owner's Mac (commit `b669fea`: CMake if(MSVC)/else() dual-toolchain
  branch, .gitattributes binary pins, string-only path neutralization, README dual-platform docs;
  Mac evidence 253/253 + ZEXALL 254/254 on AppleClang + golden SHA match). Windows re-gate
  coordinator-discharged: AC-W1 253/253 clean rebuild, AC-W2 goldens byte-identical (SHA
  6f1a7983…ce5188 identical MSVC x64 ↔ AppleClang arm64), AC-W3 zero churn. ONE codebase, two
  toolchains, auto-detected — no fork. **DEC-0082 as amended (owner-corrected scope):** the
  CROSS-MACHINE COORDINATION set — docs/, agent-protocol/, .claude/ (minus settings.local.json),
  root CLAUDE.md, src/CLAUDE.md — is now TRACKED AND PUBLISHED so both machines coordinate via
  GitHub. references/, tests/, tools/ REMAIN local-only (both machines hold copies; edits
  hand-carried — recorded limitation). debug/, games/, build/ stay ignored. Mac side must run
  the one-time reconciliation (move local untracked copies of the published set aside → pull →
  append its Mac-side M54 ledger entries into the tracked channels). M54 planning record below.
- Prior phase (closed): **M54 — PLANNING (opened DEC-0081, 2026-07-15). macOS support: single codebase,
  MSVC + AppleClang, CMake build-time auto-detect, NO repo split.** PLANNING-ONLY cycle (owner
  directive): the owner will clone on their Mac and continue there — macOS build/test evidence
  CANNOT be fabricated from this Windows machine, so the developer/QA gates are DEFERRED
  (not skipped; orchestration-validated protocol-compliant) and M54 stays OPEN. Coordinator
  recon: src/ has zero Win32 API usage, CMAKE_CXX_EXTENSIONS OFF, MSVC/WIN32 build bits already
  guarded (CMakeLists 29/161/188), no backslash paths, tests pure C++, SDL3 vendored
  (macOS-native), both platforms little-endian. Headline risks: AppleClang missing-include
  strictness, CRLF/LF fixture divergence (.gitattributes decision), SDL3Config discovery,
  filename case, PowerShell tooling on macOS, single-config ctest invocation. Ledger set:
  DEC-0081 (decisions), M54 entry (milestones.md), REQ-M54-001 (requests → planner), this flip.
  NEXT: msx-planner delivers `docs/m54-planner-package.md` (incl. the macOS RUNBOOK) → cycle
  HOLDS with the deferral note → owner continues Mac-side (developer evidence → QA → release).
- Prior phase (closed): **NONE OPEN — M53 DONE, msx-disk SHIPPED (2026-07-15).** Owner user-test passed
  ("works like a charm"); owner-directed doc sweep completed (README published section + bullet;
  root CLAUDE.md layout/scope; src/CLAUDE.md; project-baseline.md; ALL 5 .claude/agents now carry
  the diskutil description + the both-ways build-isolation rule + golden SHA oracle); feat commit
  pushed to origin main (DEC-0080-CLOSURE). NO version tag (tool-only addition; v1.2.2 remains
  the current release). M53 record retained below.
- Prior phase (closed): **M53 — PLANNING (opened DEC-0080, 2026-07-15). Standalone MSX disk utility
  `msx-disk.exe` (--create / --read hex dump / --format).** Owner-requested; owner STRUCTURE
  DIRECTIVE: source in NEW `src/diskutils/`, compiled binary post-build-copied to `diskutils/`
  (owner-created folder, tracked; binary gitignored by default pending owner preference).
  Output must be byte-exact HB-F1XV-compatible: 720 KB 3.5" DD, 80 trk × 2 sides × 9 sec × 512 B,
  MSX-DOS FAT12 + MSX boot sector — grounded in tools/format-blank-disk.ps1 (M36 S-c),
  repo DiskImage code, real disks/msxdos23.dsk oracle, DEC-0073-ranked references. Isolation
  CRITICAL: additive CMake target in the ONE build/ tree; emulator suite (247/247) stays
  byte-identical; emulator never depends on the tool. Cross-check gate: a tool-created image
  boots/mounts in our own FDC headlessly. LOW blast radius → NORMAL human-release gate
  (owner user-test), no auto-close. openMSX A/B + ZEXALL N/A. Orchestration gate PASS
  (follow-up validation, 2026-07-15). Ledger set: DEC-0080 (opening + scope-area ratification),
  M53 entry (milestones.md), REQ-M53-001/002/003 (requests), RESP-M53-001/002/003 (responses),
  this phase flip.
  **STATUS (2026-07-15 17:45): all three gates CLOSED — QA = PASS (`docs/m53-qa-signoff.md`).**
  Planner: `docs/m53-planner-package.md` (self-contained tool, PS1:82-133 layout source of
  truth, all three DEC-0073 tiers agree). Developer: `docs/m53-implementation-report.md` —
  new `src/diskutils/*` + additive CMake + POST_BUILD copy → `diskutils/msx-disk.exe`;
  tracked diff exactly .gitignore + CMakeLists.txt; tests 247→253/253. QA PASS: independently
  rebuilt + 253/253, AC-2 golden SHA 6f1a7983…b0ce5188 three-way reproduced, isolation
  verified (no emulator link, emulator-src diff EMPTY), CLI matrix 8/8, non-tautology
  confirmed, POST_BUILD re-land verified. Residuals LOW: disks are empty non-bootable
  DOS-recognizable media BY DESIGN; live Disk-BASIC recognition = owner user-test.
  Work UNCOMMITTED. NEXT (owner, NORMAL human-release gate): user-test — msx-disk --create →
  --read → --format a SCRATCH COPY → mount created disk in the emulator, Disk BASIC
  recognizes it — then owner-directed commit/push (tag optional, owner decides).
- Prior phase (closed): **M52 DONE + v1.2.2 RELEASED (2026-07-15).** Owner live-tested and
  approved ("very good"), discharging the NORMAL human-release gate; README synced to v1.2.2;
  feat + docs commits, annotated tag v1.2.2, main + tag pushed per DEC-0047-AMENDMENT-A
  (DEC-0079-CLOSURE). Post-QA owner amendment DEC-0079-AMENDMENT-A: volume-up = **Alt+U** (was
  Alt+P; QA re-run waived by owner; rebuild + 10/10 related tests verified). Final M52 hotkeys:
  Alt+D/Alt+U volume down/up (repeat honored), Alt+F fast-disk, Alt+S disk-writable toggle.
  M52 record retained below.
- Prior phase (closed): **M52 — PLANNING (opened DEC-0079, 2026-07-15). SDL3 master volume control +
  hotkey reshuffle + disk-writable default ON.** Owner-requested usability slice: (1) master
  volume — presentation-stage master gain strictly AFTER the DEC-0039 mix (PSG:FM:keyclick
  ratios byte-identical at full volume), hotkeys Alt+D down / Alt+U up, CLI `--volume <0..100>`
  (--persistence pattern), XML `<volume>` via the M50 `emulator_config.*` seam with CLI > XML >
  built-in default precedence; (2) fast-disk toggle hotkey Alt+D → **Alt+F** (owner mnemonic
  "fast disk"; frees Alt+D); (3) `--disk-writable` default **ON** (CLI + XML), new
  `--no-disk-writable` negative flag, **Alt+S** runtime toggle with planner-defined flush
  semantics (M45/M45b/M47 write-integrity must not regress). Hotkey-collision audit clean
  (DEC-0079): Alt+F/Alt+S/Alt+D/Alt+U all free vs F6-F12/Pause/Alt+Enter/Alt+B/Alt+M.
  CRITICAL determinism guard: pre-M52 deterministic suite stays byte-identical; headless
  parity for the disk-writable default is a planner decision under that constraint. Expected
  locus: `src/frontend/*` + `src/machine/emulator_config.*` (+ possibly `src/main.cpp`) + root
  `sony_msx_hbf1xv.xml`; ZERO core/cpu/device-timing → openMSX A/B + ZEXALL not required
  (M50/DEC-0077 disposition), M50-style empty-diff HARD-EXCLUDE accuracy gate mandatory. MED
  blast radius (disk-default behavior change) → NORMAL human-release gate, no auto-close.
  Operational precondition (M51 §8 incident): SINGLE automation session during dev/build.
  Ledger set this cycle: DEC-0079 (decisions), M52 entry (milestones.md), REQ-M52-001/002/003
  (requests), RESP-M52-001/002/003 (responses), this phase flip.
  **STATUS (2026-07-15 15:30): all three gates CLOSED — QA = PASS (`docs/m52-qa-signoff.md`).**
  Planner: `docs/m52-planner-package.md` (volume default 100 unity / linear clamp-not-wrap
  step 10 / key-repeat on volume keys only / Alt+S = host-path bind-unbind / headless
  disk-writable stays OFF). Developer: `docs/m52-implementation-report.md` — 13 tracked files
  +313/−40 + new `src/frontend/master_volume.h`, tests 245→248, full ctest 247/247, the 3
  §4.5 policy flips exactly as enumerated. QA PASS: independently reproduced 247/247,
  byte-identity #183 non-tautological, anti-drift held, Alt+S semantics #184, FDC diff empty
  + write-integrity green, HARD-EXCLUDE empty-diff re-run twice, shipped-XML round-trip green,
  headless smoke green (boot-only disk SHA unchanged — R1 de-risked). Work is UNCOMMITTED in
  the working tree. NEXT (owner, NORMAL human-release gate): live check — Alt+D/Alt+U audible
  volume steps + stderr percent, Alt+F fast-disk message, Alt+S toggle, an in-game disk save
  persisting by DEFAULT across relaunch, `--no-disk-writable` escape hatch — then
  owner-directed commit + README version sync + tag (target v1.2.2) + push.
- Prior phase (closed): **M51 DONE + v1.2.1 RELEASED (2026-07-15).** The owner live-confirmed
  the sprite fix ("near perfect") on the repro titles, discharging the NORMAL human-release gate:
  README synced to v1.2.1 (`b97a048`), annotated tag **v1.2.1** @ `b97a048` + `main` pushed to
  origin and remote-verified (assistant push per DEC-0047-AMENDMENT-A after owner approval).
  DEC-0078 CLOSURE recorded in `channels/decisions.md`; M51 = Done in `state/milestones.md`.
  Evidence-corrected finding carried forward: v1.1.7's phosphor-persistence "genuine 8-sprite
  flicker" attribution was wrong — the flicker was the M49 regression; the feature stays, default
  0/off. Residual watch items (non-blocking): M49 +2-line quantization ceiling; the documented
  1-frame-stale fallback for rows sink-committed before split-open; Laydock 2 deep-gameplay
  scripted repro remains probe-stage (owner live check covered it). QA record retained below:
  QA sign-off `docs/m51-qa-signoff.md` = **PASS**: canonical
  244/244 QA-reproduced; canonical-binary title oracles Aleste 2 f3560-99 + Firebird f2000-39 both
  **present 40/40, 0 transitions** (== openMSX reference rate); canonical/worktree frame hashes
  EQUAL; EG-3 12/12 runnable 0.000% incl. a QA live canonical-vs-WSL-openMSX re-run; EG-8
  trace-neutrality byte-identical; integrity 3/3 files hash-identical to the sha-stamped
  deliverables at clean `278af86` (local-only commit). Remaining: owner live spot-check
  (Aleste 2 + Firebird + Laydock 2; launch WITHOUT `--persistence`, default 0=off, so the phosphor
  softener cannot mask residuals) → owner-directed tag/push (target v1.2.1) + README sync.
  OWNER ADVISED: close the other live automation session on this repo (report §8 incident) before
  any further work. Development record below.
  DEVELOPMENT outcome (REQ-M51-002, `docs/m51-implementation-report.md`): **H1 CONFIRMED — M49
  (v1.1.6) regression, attribution (a).** The M44 command-row sink `on_commit_up_to()` committed
  command-blitted background rows PAST the render-only sprite watermark after `begin_frame()`'s
  clear — sealing those rows with EMPTY sprite buffers; one wrap-space terrain blit could seal the
  whole remaining frame (Aleste 2 plane 40/40-frame absence; Firebird 16 present↔absent
  transitions vs openMSX 0 on identical inputs → flicker was 100% our defect, 0% authentic
  multiplex; SAT census ≤6 sprites/line). Bisect: v1.1.4 PASS, v1.1.5 PASS (M48/"adjustable VDP
  timing" ELIMINATED), **v1.1.6 first-bad both titles**, v1.2.0≡v1.1.6. Fix (shape (i), consumer-
  side pacing contract, openMSX PixelRenderer.cc:580-584/SpriteChecker.hh:242-247 effect-only):
  new `V9958Vdp::commit_sprite_rows()` + `on_commit_up_to()` pacing + render_frame memoization
  guards — `v9958_vdp.{h,cpp}` + `hbf1xv_machine.cpp`, +228/−5, zero core/cpu (ZEXALL correctly
  not run). Worktree-verified gates: fast ctest **244/244** (2 new tests, adversarial-revert
  proven), 12/12 runnable modes 0.000% A/B, DEC-0031 collision byte-identity, post-fix oracles
  Aleste 2 40/40 present + Firebird 40/40 present/0 transitions (== openMSX rate), determinism
  dual-run identical. **INCIDENT (report §8): a SECOND concurrent automation session mutated the
  canonical tree during the run** (moved HEAD mid-bisect, contaminated its own bisect leg —
  quarantined at `debug/m51/quarantine-rogue-session/`, file-locked the exe, and wrote an
  UNVERIFIED shape-(i)+(ii) variant into canonical `src/` + rewrote the new unit test in shared
  `tests/`). Coordinator serialization (2026-07-15 ~03:15): rogue canonical state quarantined
  (`debug/m51/quarantine-rogue-session/canonical-landing-quarantine/` — src diff + both test
  rewrites), `src/` restored to HEAD, the developer's sha-stamped verified set landed EXACTLY
  (3 files +228/−5, sha256 d54b1525…, deliverable-hash-matched; tests CMake block verified
  single). ATTESTATION ESCALATION (03:20-03:35): attempt 1 — 243/244 with ONLY the new M51
  integration test failing in the pre-fix pattern; forensics showed Copy-Item had preserved the
  deliverables' 03:05 mtimes so MSBuild kept the FOREIGN session's newer .obj files (stale-object
  hazard) → mtimes bumped; attempt 2 — the foreign session DELETED build outputs mid-link
  (sony_msx_sdl3_frontend.lib built then vanished, LNK1104; sony_msx_sdl3.exe + SDL3.dll gone
  from build/Debug); attempt 3 (foreground) — C1041 PDB-lock storm = TWO concurrent MSBuild
  fleets in the one canonical tree (the foreign session actively building). Coordinator data-
  safety response: the verified 3-file set COMMITTED locally as **`278af86`** on main (NOT
  pushed; release still QA- and owner-gated) so any further foreign clobber is git-detectable/
  restorable; HEAD+tree re-verified intact/clean; now serializing by waiting for the foreign
  MSBuild fleet to drain (60s-quiet watcher) before the final attestation run. OWNER ACTION
  ADVISED: close/stop any other live Claude session on this repo before further work. Prior
  planning detail:
  PLANNING gate closed 2026-07-15: `docs/m51-planner-package.md` delivered (diagnostic-first: Task 0
  repro+oracle → Task 1 oracle-metric bisect v1.1.4→v1.1.5→v1.1.6→v1.2.0 → Task 2 env-gated
  `SONY_MSX_M51_SPRITE_TRACE` attribution → conditional fix slices F1-F3 per attribution branch →
  EG-1..EG-11). Primary planning hypothesis **H1 (hypothesis-grade, NOT pre-decided)**: the M44
  command-row sink `on_commit_up_to` (`src/machine/hbf1xv_machine.cpp:138-158`) commits
  command-blitted background rows PAST the sprite watermark without driving the sprite pass
  (`v9958_vdp.cpp:714-716`) after `begin_frame()` cleared the per-line sprite buffers
  (`sprite_engine.cpp:93`) — sealing those rows sprite-less for the frame; predicts first-bad at
  v1.1.6 on command-blit-heavy shooters. H2-H5 secondary hypotheses + trace checks in the package.
  Developer entry: Task 0 step 1 (build main, adapt `tools/capture-aleste-play-evidence.ps1` with
  the correct Aleste 2 ROM path, ≥30 consecutive gameplay frames → `debug/m51/aleste2/`, oracle
  must FAIL on current main first). No fix code before the Task 2 written attribution verdict.
  CRITICAL gameplay-breaking sprite defect, three independent live repros (all KonamiSCC carts):
  **Aleste 2** — the player's plane sprite completely DISAPPEARS/RE-APPEARS correlated with game
  scrolling; **Firebird (Hi no Tori Hououhen)** — player character flickers CONSTANTLY + flying
  enemy sets flicker severely while ground-locked (background-layer) objects are clean — a clean
  sprite-vs-background discriminator placing the defect in the SPRITE subsystem; **Laydock 2** —
  same disappearance symptom (owner report). Post-dates BOTH M48 (v1.1.5, access-slot contention =
  the owner's "adjustable VDP timing" suspicion) and M49 (v1.1.6, per-line-live sprite fetch, D9).
  Candidate attributions (hypotheses only — developer attributes by evidence): (a) M49 per-line-live
  watermark/segment regression (visible-buffer lifecycle across mid-frame `check_until` segments,
  5th/8-sprite-limit misfire, segment-boundary frame-to-frame wander), (b) M48 timing-overlay shift
  of where game ISR writes land vs sprite segments, (c) the honest M49 residual (1-frame lag +
  cycle-timed intra-line ceiling) proving gameplay-breaking, (d) a distinct latent sprite/R#23
  defect. Coordinator recon: static sprite-line formula (`sprite_engine.cpp:263-267`) is at
  openMSX parity (R#23 participation + sentinel-216 + checked-one-line-earlier all match
  `SpriteChecker.cc:104-127`) — the DYNAMIC path is the suspect. Plan of record: planner-first
  (docs/m51-planner-package.md), diagnostic-first (deterministic `--input-script`/`--dump-frame`
  repro + git bisect v1.1.4 → v1.1.5 → v1.1.6 → v1.2.0 to pin the regressing milestone), then a
  universal fix + tests + the mandatory HIGH-blast-radius gates (13-mode 0.000% A/B, DEC-0031
  collision byte-identity, before/after captures under `debug/m51/`). Caveat honored: genuine
  >8-sprites-per-line hardware flicker must NOT be "fixed" away. NORMAL human-release gate (no
  auto-close). Ledger set this cycle: DEC-0078 (decisions), M51 entry (milestones.md), REQ-M51-001
  (requests), this phase flip. Prior phase (M50) retained below.
- Prior phase (closed): **M50 DONE + v1.2.0 RELEASED (2026-07-14).**
  M50 (DEC-0077) externalized the strict-XML configuration — QA PASS (`docs/m50-qa-signoff.md`,
  **242/242**), tag **v1.2.0** committed + pushed (assistant push per DEC-0047-AMENDMENT-A). The
  shipped config file was then renamed `hbf1xv-config.xml` → **`sony_msx_hbf1xv.xml`** (root element
  `<hbf1xv-config>` deliberately unchanged; 242/242 still green), and the `build/` + `debug/` trees
  were cleaned to reclaim ~3.4 GB (repo now ~154 MB). No open milestone remains. Milestone scope
  detail retained below: **Externalized strict-XML
  configuration (production hardening).** Move the emulator's DEFAULTS/KNOBS out of hardcoded C++ into
  a single strict `sony_msx_hbf1xv.xml` so the machine is configurable WITHOUT recompiling: config FOUND
  → parse+validate+use; NOT found → ONE WARNING line + built-in defaults, continue (still runs
  standalone). Confirmed scope "Settings + machine config" — EXTERNALIZE (A) CLI-option/session
  defaults (`--ram`, fast-disk, FM-PAC auto-load + slot, border, disk-writable, speed 0..7, and the
  SDL3-presentation knobs scale/filter/persistence/persistence-mode/fullscreen/capture — each an
  existing knob hardcoded in the M46 `resolve_session_defaults()` layer / `Sdl3AppConfig` struct,
  audited in `src/frontend/{sdl3_cli.h,sdl3_app.h,sdl3_main.cpp}` + `src/main.cpp`) and (B) machine
  sizing/paths (main RAM KB, VRAM KB, ROM/BIOS dir + the 7 BIOS filenames `hbf1xv_machine.cpp:513-520`,
  FM-PAC ROM+SRAM, softwaredb path; slot layout as an optional validated-to-builtin echo).
  **HARD-EXCLUDE (stay hardcoded, the immutable silicon spec, tier 1 — a bad XML edit must NEVER break
  accuracy):** Z80A 3.58 MHz + frame cycles, IM1=13T/IM2=19T/NMI=11T, S1985 +1 M1 wait, V9958 154/88/31
  slots + 1368 cyc/line + M48/M49 parity values, WD2793 FDC constants (empty-diff gate over `src/core`,
  `src/devices/cpu`, `vdp_access_timing.h`, `{v9958_vdp,sprite_engine}.*`, `wd2793.cpp`,
  `s1985_engine.*`). **Precedence:** explicit CLI > XML > built-in default, resolved in the M46
  anti-drift Layer-B seam (ctor `kDramBytes` + struct stock defaults stay UNCHANGED). **Graceful
  fallback:** file-not-found → WARNING + defaults; bad value → per-key WARNING naming the key + that
  key's default (rest of file still applies); structurally-broken file → whole-file fallback; never
  crash. **Determinism guard (CRITICAL, keeps the ~235-test suite byte-identical):** headless never
  auto-loads (default scaffold, `--debug-session`, all `*-parity` modes); SDL3 `--hidden-window` never
  auto-loads; auto-load ONLY in a genuinely interactive SDL3 launch; `--config <path>` forces load in
  any mode (no test passes it). **Reuse the existing XML machinery:** extract the tolerant `TagScanner`
  from `src/machine/software_db.cpp:37-136` into a shared `xml_tokenizer.*` (behavior-preserving) +
  new `emulator_config.*`; NO new third-party dependency. **Shipped reference:** tracked,
  fully-commented `sony_msx_hbf1xv.xml` at repo root (verified NOT gitignored) with every knob + default
  + range; shipped values == built-in defaults (round-trip identity). Planner package:
  **`docs/m50-planner-package.md`** (4 slices S1 model+parser/validator · S2 CLI-default resolution +
  determinism rule + `--config` · S3 machine sizing/paths · S4 shipped config + docs; per-slice
  acceptance AC-S1..S4 + milestone AC-1..AC-8; evidence gates §8; risks R1-R7; §3 HARD-EXCLUDE; the
  full CLI/machine externalize/exclude audit §1.2/§2). Ledger set: DEC-0077 (decisions), M50 entry
  (milestones.md), this phase note. openMSX A/B NOT required (pure config plumbing; every settable
  value was already A/B-validated in its origin milestone — RAM M42, FM-PAC/fast-disk M46,
  video/persistence M37/M39; timing-constant diff empty). ZEXALL NOT run (zero cpu/core diff). MED
  blast radius, NORMAL human-release gate (no auto-close). OUTCOME: all 4 slices shipped, QA PASS,
  released **v1.2.0** (then the config file renamed to `sony_msx_hbf1xv.xml`) — see the M50 milestone
  entry. Prior/parallel phase (M49, DEC-0076) retained below; it is DONE (v1.1.6).
- Prior/parallel phase (M49 — PLANNING, opened DEC-0076, 2026-07-14). Per-line LIVE sprite
  attribute/register fetch — closes backlog D9.** The sprite subsystem is FRAME-ATOMIC
  (`SpriteEngine::recompute_frame()` runs ONLY from `V9958Vdp::on_vsync()`,
  `src/devices/video/sprite_engine.cpp:74-301` ← `src/devices/video/v9958_vdp.cpp:685-705`),
  snapshotting the whole control-reg file + all VRAM at end-of-frame; the BACKGROUND is already
  per-line-live via the M32 seam (`src/machine/hbf1xv_machine.cpp:53-124`, +2 margin `:101`). Space
  Manbow + Laydock 2 do CYCLE-TIMED mid-frame VDP reprogramming (NOT line interrupts: R#0 IE1=0,
  R#19=0xDB) → the snapshot races the writes → top-region sprites culled/mis-positioned + flicker.
  Tier-1 (fact-sheet §6 line 120): sprite data for line N is fetched during line N−1 (per-scanline);
  R#23 is a live per-line offset (§7 line 126 / §10 line 178). Fix = give `SpriteEngine` a stateful
  `check_until(line)` watermark pass (mirror `VdpScanlineAccumulator`), drive it from the EXISTING
  M32 seam with the SAME +2 boundary + M44 watermark discipline, feed the DEC-0031 collision/5th-
  sprite machinery byte-identical, keep R#23 per-line-live. ALGORITHM change (D4/C1 table ban does
  NOT apply; the "never copy openMSX `SpriteChecker` code" rule DOES). Planner package:
  **`docs/m49-planner-package.md`** (4 slices S1-S4, per-slice acceptance AC-S1..S4, evidence gates
  EG-1..EG-9 incl. the MANDATORY full 13-mode 0.000% screen A/B + Space Manbow/Laydock 2
  before/after captures + DEC-0031 collision-byte-identity gate, risks R1-R8, honest residual-risk
  note on 1-frame-lag + Aleste-2 cycle-timed intra-line ceiling, and the flagged open sub-question:
  is Laydock 2 scoreboard noise purely sprite or ALSO a background facet — needs a Laydock 2 repro
  in dev). Ledger set: DEC-0076 (decisions), M49 entry (milestones.md), this phase note. HIGH blast
  radius (all sprite rendering, all 13 modes) → NORMAL human-release gate, no auto-close. Expected
  edits confined to `src/devices/video/{sprite_engine,v9958_vdp}.*` + `src/machine/hbf1xv_machine.cpp`;
  ZERO cpu/core (ZEXALL not triggered). NEXT: developer handoff, Task 0 (Laydock 2 repro) then Slice
  S1 (engine) first. NOTE (coordinator to reconcile, independent of M49): `v9958_vdp.h:284-315`
  already references M48 Slice 1/2 access-timing seams while the M48 block below still reads PLANNING
  — a state/code drift outside this M49 planning cycle's evidence; M48's own disposition is unchanged
  by this M49 opening. Prior M48 planning detail retained below.
- Prior/parallel phase (M48, opened DEC-0075, planner package `docs/m48-planner-package.md`): **V9958
  command-engine ↔ CPU VRAM access-slot CONTENTION model (deferred backlog D4/C1 remainder),
  ban-respecting + FORMULA-based** — orthogonal to M49 (command TIMING overlay vs sprite-fetch
  timing); disposition unchanged by the M49 opening.
  This is the next milestone the owner scheduled after DEC-0074 (v1.1.4) landed only the CONTAINED
  per-command cost corrections and explicitly deferred the full slot-contention model. M48 adds, as a
  pure timing/STATUS overlay on the existing S#2 CE/TR busy windows (VRAM mutation stays ATOMIC, M44
  render-sync untouched): **Slice 1** — a per-line command-throughput cap from the three tier-1
  fact-sheet §8 slot COUNTS (154 display-off / 88 sprites-off / 31 sprites-on), sprite-aware,
  superseding the DEC-0069 empirical `kActiveDisplaySlotFactorPercent` placeholder; **Slice 2** —
  CPU-concurrent-access command slowdown (CPU-priority: each CPU `#98` VRAM access during a busy
  command steals one slot, extending the CE window; the CPU access is NEVER stalled/dropped ⇒ CPU
  timing byte-identical). **HARD BAN:** the openMSX `VDPAccessSlots.cc` ~340-entry per-slot POSITION
  tables are BANNED (read for EFFECT only, never transcribed into `src/`); the ONLY legal numeric
  inputs are 154/88/31, 1368, 6. Honest ceiling: the cycle-exact per-slot model needs the banned table
  and stays UNBUILDABLE-WITHOUT-FABRICATION — "parity" here means aggregate CE/TR timing matching §8 ON
  AVERAGE, with pixel + CPU output bit-exact. Planner package: **`docs/m48-planner-package.md`** (scope,
  2-slice breakdown, per-slice acceptance, EG-1..EG-9 incl. the MANDATORY full ZEXALL sweep + 13-mode
  0.000% screen A/B + command-engine timing A/B, risks R1-R7, honest ban-ceiling §7). Ledger set:
  DEC-0075 (decisions), M48 entry (milestones.md), this phase note. NEXT: developer handoff (Slice 1
  first). Prior phase detail (M47, closed) retained below.
- Prior phase (closed): **M47 CLOSED (DEC-0072, 2026-07-14, autonomous run) — the YS II 8-slot disk-save
  crash is INHERENT to the game/disk, NOT an emulator defect. The emulator is VINDICATED.** Decisive,
  independent owner test: the identical save→load crash reproduces on the owner's LOCAL openMSX (an
  independent accuracy-focused emulator) from a FRESH game on a VANILLA disk, across MULTIPLE game
  sources — two accurate emulators failing from a clean slate ⇒ the fault is the software on the disk
  (YS II's own save/load routine mishandling a full 8-slot save area — the buggy routine shared by the
  common crack lineage all these `.dsk` images descend from). PROOF the FDC is clean: an env-gated
  per-byte write logger over the deterministic replay shows **0 substitutions** and a byte stream
  **byte-identical** to what lands on disk (8 Write-Sector × 512 B, no drops/holes/overrun), with
  corruption confined EXACTLY to the 8 save slots (LBA 9-16, no FAT/dir spill) — corroborated by an
  independent WD2793 datasheet audit; and openMSX itself immediately CRASHES loading OUR produced
  disk. An intermediate "our CPU/VDP timing" hypothesis (the crash is timing-*sensitive* per a
  `--vdp-cmd-bias` sweep) was raised then CORRECTED: openMSX at correct timing ALSO crashes, so correct
  timing does not avoid the crash — timing was a distraction, not the cause. Verdict doc:
  `docs/m47-dec0072-timing-fidelity-verdict.md`. Practical guidance (already-working paths): use 1-2
  disk save slots or FM-PAC SRAM saves (100% reliable) — do not fill all 8 disk slots on these images.
  This SUPERSEDES the prior (2026-07-13) "M47 = WD2793 write-fix closes it" understanding: the
  `a27be57` never-drop write model is a real, kept correctness improvement but was NOT the YS II cause.
- Device-fidelity items found during the 4-specialist audit and FIXED as independent correctness wins
  (grounded tier-1 hardware spec, openMSX-A/B tier-2 confirmation per DEC-0073/DEC-0074; NOT the YS II
  cause): **(1) CPU interrupt/NMI-acknowledge timing** — the S1985 +1 M1 wait does NOT apply to the
  interrupt/NMI-ack M1 (it gates on the opcode-fetch /M1+/MREQ, not the ack's /M1+/IORQ): **IM1 = 13T,
  IM2 = 19T, NMI = 11T** (were 14/20/12), via a `tick_refresh_only()` split + NMI R-tick fix; 4 test
  oracles updated; `references/fact-sheets/Zilog Z80A CPU.md` §5 corrected. **(2) V9958 command-engine
  timing** — LINE per-pixel 88→112 (+32 minor Bresenham step); per-line end-of-line overhead
  (HMMV+56/HMMM+64/LMMV+64/LMMM+64/YMMM+0); LMCM/LMMC/HMMC transfer-ready (TR) per-unit pacing (never
  drops data). The bug was matching openMSX's deliberate `calcFinishTime` UNDER-estimate rather than
  its real execution timing. **(3) PSG** — Tone/Noise counter clamps to period-1 on a period-shrinking
  write (was reset to 0), matching openMSX + our own `Envelope`. The full VDP access-slot CONTENTION
  model (154/88/31 slots + CPU↔command slot stealing) stays DEFERRED — HIGH risk + license-sensitive
  ~340-entry tables (the standing C1/D4 UNBUILDABLE-WITHOUT-FABRICATION ban).
- Verification: full deterministic suite **228/228** including the ZEXALL/ZEXDOC sweep after the CPU
  work; VDP suite green after the VDP work. New this session: the reference-precedence directive
  (**DEC-0073**) — propagated across CLAUDE.md, all 5 `.claude/agents`, all 5 `.claude/commands`,
  `agent-protocol/project-baseline.md`, and `agent-protocol/guardrails.md`; the CPU/VDP/PSG
  timing-parity work is recorded as **DEC-0074**.
- **v1.1.4 RELEASED (2026-07-14):** DEC-0074 timing parity (Z80A interrupt-ack 13/19/11T, V9958
  command-engine LINE 112 + per-line overhead + LMCM/LMMC/HMMC TR pacing, PSG counter-phase) +
  FM-PAC firmware published to `roms/` (DEC-0047-AMENDMENT-B). README synced to v1.1.4; tag
  **v1.1.4 @ `2e09040`** and `main` pushed to origin (assistant push per DEC-0047-AMENDMENT-A).
  Clean-rebuild verified **228/228** (full ZEXALL sweep skipped per owner; already green on this
  exact CPU code). v1.1.0–v1.1.3 tags all confirmed on origin.
- NEXT: no open emulator defect remains on the YS II track — DEC-0072 is CLOSED (emulator vindicated).
  The deferred **VDP access-slot CONTENTION model (backlog D4/C1)** is now OPEN as **M48 — PLANNING**
  (DEC-0075; planner package `docs/m48-planner-package.md` written this cycle). Immediate next step:
  developer handoff, Slice 1 (per-line throughput cap from 154/88/31) first, then Slice 2
  (CPU-concurrent slot-steal); QA runs the MANDATORY full ZEXALL/ZEXDOC sweep + the 13-mode screen A/B
  + the command-engine timing A/B. Normal human-release-decision gate applies (HIGH blast radius; no
  auto-close). Optional owner live re-test of the timing-parity build (a FRESH 8-slot save, NOT the old
  cycle-stamped recording — any timing change desyncs it). Prior release detail below.
- Prior phase (closed): **v1.1.2 RELEASED — M46 CLOSED (DEC-0071, 2026-07-13). Convenience-first launch
  defaults + `--stock` + `--slotN` rename + enriched banner + FM-PAC slot-2 auto-load.** Anti-drift
  CLI-resolver seam (machine ctor + Sdl3AppConfig struct stay stock → direct-construction tests
  unaffected; `ram_size` sentinel untouched). QA Conditional Pass (`docs/m46-qa-signoff.md`): trimmed
  gate 136/143 (7 fails = pre-existing asset-absence from the reorg, zero M46 code), anti-drift
  verified 3 levels, empty cpu/core diff (ZEXALL withheld), 9-tool `--stock` migration. AC-10
  (interactive `CALL FMPAC` from slot 2 + in-game SRAM) owner-gated → human live-checked + directed
  release. Commits: `70db39f` (M46 src) + `c00c7d1` (README v1.1.2 doc). Tag **v1.1.2** @ `c00c7d1`,
  owner-run push per DEC-0047. Repo tidied (empty debug/ removed). Deferred local-only hygiene: 7
  stale asset-path test fixtures → skip-when-absent. Prior release detail below.
- Prior phase (closed): **v1.1.1 RELEASE — M44 Phase 2a + M45 (+ M45b regression fix) CLOSED, LIVE
  HUMAN-CONFIRMED (2026-07-13), PUSHED.** The human live-verified end-to-end: YS II in-game SAVE → LOAD →
  play → die → continue works **WITH AND WITHOUT `--fast-disk`**; Laydock 2's severe HUD flicker is
  GONE and cut-scenes now play at NORMAL speed (were fast-forwarded — the CE-pacing win); the
  residual thin-line/label jitter is ACCEPTED ("i'm good with what we have") → **Q1 DECIDED: NO
  Phase 2b** (DEC-0069-OUTCOME). Unpushed release chain toward tag **v1.1.1**: `a27a1c4` (M44 Phase 1)
  → `4a8f8e0` (M45 write DRQ) → `572d9cc` (M44 Phase 2a CE-pacing) → `0f648d7` (M45b CHECK_WRITE
  window rotational) → the README v1.1.1 sync commit. Repo tidied for disk space (build/ + debug/
  removed ~4.53 GB; WSL openMSX screenshots cleared; the ~480 KB openMSX system-ROM setup kept for
  future A/B). Owner-run tag+push per DEC-0047. Closed sub-phase detail:
- Prior phase (closed): **M45b — DONE (DEC-0070, 2026-07-12, `0f648d7`).** The M45 (`4a8f8e0`)
  WriteSector first-byte CHECK_WRITE window (fixed `read_start_cycles()` 228 + 8-byte ~912 ≈ 1140
  cycles) was FAR too short → aborted YS II's legitimate save-buffer-then-write cadence with
  LOST_DATA writing NOTHING → game hang (the human's live "save hangs" report; crash capture
  STATUS=04 / DATA_INDEX=0 / Type-I retry-seek loop). Fix: first-byte DRQ now rotational-search-
  relative (same model as the read path) + a full-revolution CHECK_WRITE window (kSystemClockHz/5 =
  715909). The M45 edge-DRQ-handshake corruption fix is KEPT INTACT (only the first-byte window
  changed). Fixed in an isolated git worktree, cherry-picked to main; NEW `wd2793_write_firstbyte`
  test + adversarial-revert proof (21 assertions fail on the old window); combined main rebuild GREEN
  (FDC/disk 15/15, fast subset 222/222). `src/devices/fdc/wd2793.{h,cpp}` only, zero cpu/core.
- Prior phase (closed): **M44 — Phase 2a DONE (`572d9cc`, DEC-0069 + DEC-0069-OUTCOME); Phase 2b
  DECLINED by the human.** V9958 command-engine CE busy-duration pacing: license-safe duration model
  (ticks_per_unit composed ONLY from the already-blessed VdpAccessDelta values, NOT the banned slot
  table); CE-busy overlay at the S#2 read only (`ce() || cmd_busy_until_cycles_ > cpu_total_cycles()`),
  absolute timebase from scheduler.total_cycles() — ZERO src/core edit. Severe Laydock flicker gone +
  cut-scene timing corrected (human-confirmed); the residual label jitter is the per-raster slot-
  accuracy (Phase 2b) boundary, human-ACCEPTED (no Phase 2b). New CE-duration oracle; 222/222; render
  byte-identical pre==post; all M22/M32/Phase-1/VR-HR oracles unmodified. Phase 1 = `a27a1c4`
  (per-row render-sync foundation, DEC-0065/0066).
- Prior phase (closed): **M45 — DONE (QA CONDITIONAL PASS, DEC-0067/0068, 2026-07-12; `4a8f8e0`; the
  WD2793 write-corruption fix, now human-live-confirmed via the M45b follow-up).** From the human's
  CRITICAL report: an in-game YS II save corrupted disk2 (openMSX crashes
  identically → corruption persisted in the .dsk). Root cause (confirmed via real-object harness +
  forensic diff vs the human's pristine `disks/games/ys2-pristine/`): the WD2793 accurate-mode
  (`!fast_disk_`) WriteSector path modeled DRQ as a rigid absolute-cumulative deadline and injected
  0x00+LOST_DATA (consuming data slots → early finish + dropped tail) when the CPU trailed it —
  corrupting normally-timed game writes (interrupts-enabled loop + mid-transfer stall). **REVERSED
  the --fast-disk suspicion: fast mode was code-immune + byte-perfect.** Fix `4a8f8e0`
  (`src/devices/fdc/wd2793.{cpp,h}` only, zero cpu/core): re-modeled to the openMSX/real-WD2793 edge
  DRQ handshake + one-byte pipeline + first-byte CHECK_WRITE gate; removed the `!fast_disk_` gate
  (identical with/without --fast-disk). QA CONDITIONAL PASS (`docs/m45-qa-signoff.md`): 206/206
  headless (SDL3-gated delta only) + all FDC/disk oracles + new `wd2793_write_stall` test;
  non-tautology + forensic match DEFINITIVE (revert reproduces the exact disk2 105-zero signature vs
  the real 98-110); BIOS-write byte-perfect + mode-independent; openMSX A/B BLOCKED (harness) but
  acceptable (flat-.dsk architecture). SDL3 build restored via bootstrap. Residual (backlogged): the
  fix is CPU-paced (SAFER than openMSX's scheduled-tick LOST_DATA, invisible to real software) — a
  full cycle-accurate write model is a future item. The human's corrupt save is NOT recoverable →
  restore pristine ys2-d2.dsk (pending OK). NEXT: owner live re-save test → tag v1.1.1 @ `4a8f8e0`.
- Prior phase (closed): **M44 — Phase 1 committed (`a27a1c4`, command-engine per-row render-sync
  foundation, no-regression), Phase 2 originally deferred (docs/m44-phase2-plan.md) then RESOLVED
  this session as Phase 2a (`572d9cc`, done) + Phase 2b (declined by the human) — see the active
  block above. DEC-0065/0066/0069.**
- Prior phase (closed): **M43 — DONE (QA FINAL PASS, DEC-0062/0063/0064, 2026-07-12; release target v1.1.0
  @ `9f6eb8c`, owner-run tag+push per DEC-0047; recommended live human re-verify first).** From the
  human's FM-PAC `CALL FMPAC` "Syntax error" report. Slice 1 (no code): DEF-M43-CALLDISPATCH = VOID
  (dispatch works at openMSX parity; the report was a headless frame-capture staleness artifact +
  the human's prior non-canonical 16 KB ROM; slot fabric verified correct + UNMODIFIED — the
  diagnostic-first gate avoided an unnecessary edit to the address fabric; LIVE human-confirmed).
  Slice 2 (`a733d65`, src/devices/video): DEF-M43-FMPAC-SCREEN — V9958 GRAPHIC2/3 table addressing
  corrected additive→openMSX AND-mask (mode-aware; universal SCREEN 2/4 fix); FM-PAC manager UI now
  renders; all 13 M41 screen modes still 0.000% vs openMSX. Slice 3 (`9f6eb8c`, src/devices/cartridge):
  FM-PAC SRAM openMSX-exact 8206-byte format + LOSSLESS migration of legacy raw-8192 saves (verified
  on the human's real save, no data-loss path, hardening exceeds openMSX) + register parity + M36 test
  reconciled to the canonical 64 KB ROM. Gates: clean rebuild both exes, fast ctest 218/218, asset
  gate PASS, commits src-scoped (zero cpu/core, no AI trailer). **ZEXALL full sweep WITHHELD-substituted**
  (coordinator+human): git-diff `v1.0.40..HEAD -- src/devices/cpu src/core` = EMPTY → Z80 core
  byte-identical to the last clean sweep → run only on cpu/core edits or a fresh release baseline.
  QA honesty-corrected the FM-PAC render residual to 0.979% (a golden mid-palette-load artifact, NOT
  a defect — ours renders the correct settled green). NEXT: recommended live re-verify (manager
  renders + a manager-UI save persists) → owner tags v1.1.0 @ `9f6eb8c` + pushes the 2 unpushed commits.
- Prior phase (closed): **M42 — DONE (QA PASS, unconditional; DEC-0061, 2026-07-12; release target v1.0.41
  @ `3f69786`, owner-run tag+push per DEC-0047).** Opt-in `--ram <64|128|256|512>` CLI (headless +
  SDL3), default **64 KB** = stock spec (byte-identical when absent/64); 128/256/512 = opt-in
  non-stock "fully-populated S1985" mods. `MemoryMapperRam` parameterized by the backing region
  (num_segments = ram.size()/16 KB); authentic S1985 5-bit read-back mask UNCHANGED (0x1F) — RAM
  detection works via fold-based segment mirroring. 512 KB = the 5-bit read-back ceiling (32
  segments); >512 KB needs an external RAM cartridge (future). Dev `3f69786` (frontend + machine +
  devices/memory; ZERO src/core & src/devices/cpu → ZEXALL withheld; no AI trailer). QA **PASS**
  (`docs/m42-qa-signoff.md`): clean rebuild both exes; fast ctest 216/216 (217/217 w/ the QA A/B
  test); default-64 byte-identity proven; non-tautology proven (fold mutation → 34 fails, restored
  byte-identical); **end-to-end openMSX A/B PARITY** (`docs/m42-openmsx-ram-ab.md`) — an internal-512
  HBF1XV override vs our engine after a real BIOS boot both detect 32 distinct mapper segments at
  512 KB / 4 at stock 64 KB, byte-for-byte. CLI rejection verified both exes. NEXT: owner tags
  v1.0.41 @ `3f69786` + pushes.
- Prior phase (closed): **M41 — DONE (QA CONDITIONAL PASS, conditions discharged; DEC-0059, 2026-07-11;
  release target v1.0.40, owner-run tag+push per DEC-0047).** The human's "final PRODUCTION QA
  before release": a system-wide openMSX A/B parity sweep over 8 subsystems (keyboard, joystick,
  PSG, FM/OPLL, BASIC exec, screen modes, sprites, disk I/O). Phases S1-S4 measured; **two real
  device defects found + fixed inline** — DEF-M41-YJKOFFSET (`6044c19`, V9958 YJK/YAE 4px-left
  registration; fMSX+fact-sheet corroborated, disagrees w/ openMSX-21.0-source, two-ref rule
  applied) and DEF-M41-PSGENV (`6f6fb39`, PSG YM2149 hardware-envelope half-rate; missing openMSX
  `count+=duration*2`). Enabler `--input-script JOY=` verb (`3976bc4`). Ride-alongs already on main:
  `6ef652b` --fast-disk, `73ce71c` Right-Ctrl `:/*`. QA (msx-qa 2026-07-11): four hard gates GREEN —
  clean rebuild both exes; fast ctest **215/215**; **ZEXALL/ZEXDOC full sweep PASS** (2069 s, 0
  markers, exit 0 — the release-checkpoint CPU sweep, log `debug/m41/zexall/m41-zexall-sweep.log`);
  asset gate PASS. Non-tautology proven (adversarial YJK mutation). Final parity matrix: **30 true
  MATCH + 1 partial (H3) + 4 KNOWN-DELTA (FM sample-exact = the standing license-sensitive-scope
  ban, correctly not claimed) + 3 BLOCKED (joystick-port unscriptable on openMSX headless; decode
  proven A/B-equal via a STICK(0) control) = 38 dispositioned**, every non-MATCH legitimately
  irreducible (no masked defect). Conditions C1 (H3 on-disk-parity overstatement → re-dispositioned
  honestly: our-side-correct + openMSX-on-disk-BLOCKED; write parity proven by H1/H4/H5 SHA-equal),
  C2 (this ledger refresh), C3 (YJK RESOLVED folded into the A/B report) — ALL discharged
  2026-07-11. Sign-off: `docs/m41-production-qa-signoff.md` (local; agent-protocol/docs/debug/tools/
  tests gitignored per DEC-0044). HEAD `6f6fb39` is 5 commits ahead of the pushed v1.0.39 tag.
- Prior phase (closed): **v1.0.39 = M38 + M39 (2026-07-11) — the MSX2+ gameplay-fix cycle, ledger
  BACKFILLED 2026-07-11 by DEC-0060 (blocks added to state/milestones.md).** The ledger had been
  stale since M37 (v1.0.39 shipped without contemporaneous entries); DEC-0060 reconstructed it from
  git history + docs/m39-qa-signoff.md + the human's real requests (NO M40 exists — the "M38-40"
  label was approximate). **M38 (`b0db02b`)** — V9958 bitmap horizontal-scroll fix (R#27 fine
  wrong-sign + multi-page windowing missing; universal G4-G7/YJK/YAE; Laydock+F1 live-validated;
  s09 YJK residual later fixed as M41 DEF-M41-YJKOFFSET). **M39 (`2405a35`/`73968d7`/`53e4eaf`,
  TAG v1.0.39 @53e4eaf)** — digitized voice (PSG `sync_to_cycle` sync-before-change + 1-bit DAC),
  border default reverted to Sony-original edge-to-edge (`--border` opt-in), HUD raster seam
  L+1→L+2 (openMSX PixelRenderer.cc:566-567); QA Conditional Pass (213/213, docs/m39-qa-signoff.md)
  → human live-verify PASSED. Preceded by an unnumbered repo-maintenance sub-phase (comment tidy,
  local-folder untracking, README de-AI rewrite).
- Prior phase (closed): **M37 CLOSED (tag v1.0.38, 2026-07-10, DEC-0058) — post-M36 hardening + two
  live-play feature requests. QA FINAL PASS (docs/m37-qa-signoff.md, pristine rebuild 210/210,
  cpu/core diff EMPTY across the whole cycle, no oracle weakened) + LIVE human validation (DEC-0057:
  "YS II loads and plays perfectly, SRAM works perfectly, screen looks good").** Six slices:
  A (861eec1) VDP-IRQ probe tool; B (85b48aa) FM-PAC OPLL audio mix; C (85827e5) WD2793 read
  rotational latency (openMSX-corroborated, no oracle weakened); D (cd459a3) --speed CLI; E (cd459a3)
  SDL3 window scaling/fullscreen; F (83a0888) --capture on|off F10 gate + default --scale 3/--filter
  linear. Non-blocking follow-on backlog: WD2793 write/read-address/read-track rotational latency
  (fixed-delay remains); optional --capture gating of the F12 snapshot. Historical M37 slice detail
  and prior M36 context retained below. Slices: A (861eec1)
  openMSX VDP-IRQ A/B probe → tools/openmsx-vdp-irq-parity.ps1; B (85b48aa) inserted FM-PAC OPLL
  mixed into machine audio (additive, byte-identical when absent); C (85827e5) WD2793 Read-Sector
  rotational first-DRQ latency (index-pulse-relative; replaces the fixed 228-cycle R-A defect;
  openMSX A/B sawtooth-corroborated; documented sector-vs-track approximation; NO oracle weakened,
  proven live under adversarial mutation); D (cd459a3) `--speed <0..7>` CLI → Mb670836 Speed
  Controller initial level (CPU-slowdown, not turbo); E (cd459a3) SDL3 window scaling (resizable +
  320×240 letterbox + --filter nearest|linear + --scale N + --fullscreen + Alt+Enter toggle).
  Human live-verify gates before the tag: (1) **Slice C** — YS II two-disk boot + interior/disk
  load + save with the new read-timing (blocker-level if it regresses; no automated YS-II regression
  exists); (2) **Slice E** — SDL3 --scale/--filter visual + drag-resize + Alt+Enter fullscreen
  (non-blocking). On both passing → full PASS → coordinator release decision + git tag v1.0.38.
  Prior: **M36 CLOSED (tag v1.0.37, 2026-07-10, DEC-0054). YS II FULLY PLAYABLE like real
  hardware — building interiors LOAD (Bug B FIXED) and saves PERSIST (FM-PAC SRAM auto-save +
  disk write-back), LIVE human-validated on the real SDL3 build.** BUG B was a real V9958
  interrupt-accuracy bug: `change_register` did not re-evaluate /INT on an IE1/IE0 register write,
  so YS II's building-load ISR clearing IE0 left our /INT asserted → the ISR's EI re-fired into a
  nested-VBLANK stack overflow. Fixed per openMSX VDP.cc:1177-1198 (SUPERSEDES DEC-0053's "VDP
  correct" conclusion); pinned by the human's F10 lightweight-watch-mode capture. SDL3 FM-PAC SRAM
  persistence added (auto-save <cart>.sram). QA PASS (docs/m36-qa-signoff-final.md; 207/207 then
  208/208, zero cpu/core, source parity + independent openMSX VDP-IRQ runtime A/B + live human
  validation). R3 A/B waiver recorded (DEC-0054). Residual non-blocking: R-A WD2793 read rotational
  latency (accuracy backlog), R-B commit a tools/ VDP-IRQ probe, R-C/D/E FM-PAC minor. Historical
  M36 detail retained below. Phase 1 (msx-playtest harness) committed d522804. Phase 2 (DEC-0050,
  device-level): S-a agent-frontmatter enabler; S-c disk-save persistence (DiskImage host-file
  flush, opt-in --disk-writable, tools/format-blank-disk.ps1); S-d FM-PAC peripheral cartridge
  (CartridgeMapperType::FmPac per MSXFmPac.cc, loadable via --cart roms/fmpac.rom, functional
  validation, real-ROM integration test); S-e speculative internal sram_ REMOVED (bare machine =
  "NO S-RAM AVAILABLE", correct); S-f R-M35-1 strengthened; PLUS the DSKCHG read-and-clear fix
  (sony_fdc 0x3FFD, openMSX-grounded — a real latent media-change bug, NOT the YS II crash). Phase 3
  (debug tooling, DEC-0051/0052; additive/read-only, zero cpu/core): the comprehensive F12 debug
  snapshot (debug_snapshot.*) + the F10 live stream-capture (per-frame snapshots + CPU-trace ring +
  FDC read log, auto-finalize on HALT/SP-underflow) — the F10 tool is what cracked Bug B open.
  **Bug B: ROOT-CAUSED, OPEN.** A nested VBLANK-interrupt storm — the game runs its minimal ISR
  (0xA5E3) with the sound routine's EI (0xA373) unpatched and never acks S#0, so the VBLANK re-fires
  ~440x mid-ISR → stack runaway → JP(HL) into data → HALT. Our V9958 interrupt path is CORRECT
  (audited vs openMSX); disk reads byte-perfect (12/12 sector CRCs match disk2.dsk); mapper,
  CHS→LBA, DSKCHG all ruled out. The real divergence is UPSTREAM (the full ISR 0xA5F5 via 0xAAB4 is
  never restored before the sound is enabled), driven by a not-yet-pinned device value/timing (top
  suspects: WD2793 completion timing, VDP command-engine S#2). Human chose the openMSX-A/B path +
  the checkpoint, then went AWAY authorizing AUTONOMOUS end-to-end operation (no interrupts); the
  ONE human dependency left is the final end-to-end VERIFY (drive YS II to the building — not
  headlessly reproducible). Running: msx-qa on the committed portion + a read-only Bug-B
  device-timing A/B audit (WD2793 + VDP cmd-engine vs openMSX). Original Phase-1 kickoff intent
  retained below for
  history: Per the human's ratified decisions after live YS II play: (1) BUILD A
  PLAYTEST/LIVE-QA AGENT + COMMAND FIRST — hybrid design: headless `--input-script` drive +
  `--dump-frame` PNG capture read by a vision-capable opus agent (deterministic, regression-able)
  PLUS optional real-window spot-checks; the agent works alongside planner/developer/qa to simulate
  a human player. (2) THEN fix the two bugs the human found: **(A) FM-PAC SRAM "NO S-RAM AVAILABLE" — REFRAMED BY DEC-0050**
  — the HB-F1XV built-in FM is MSX-MUSIC (OPLL + BIOS, NO SRAM), so "NO S-RAM AVAILABLE" on a bare
  machine is CORRECT hardware behavior. The FM-PAC's 8 KB battery SRAM (window 0x4000-0x5FFD,
  0x5FFE/0x5FFF magic unlock, 0x7FF6 enable, 0x7FF7 bank) is a PERIPHERAL CARTRIDGE (new
  CartridgeMapperType::FmPac, loadable via `--cart roms/fmpac.rom`). The speculative internal
  `sram_` (hbf1xv_machine.h:176-179) is an inaccuracy to remove. Save persistence: FM-PAC SRAM save
  (cart inserted) AND/OR disk save (host-file write-back — an implement task); **(B) black screen on building entry** — the active area goes blank while UI
  persists and the game stops, needs deterministic repro (disk-read-fail vs VDP blank). ALL FOUR
  agents now on **opus** (developer/qa/planner/orchestration). Prior: **M35 CLOSED (DEC-0049,
  2026-07-10, tag v1.0.36) — multi-disk hot-swap, LIVE human-validated (F11 swapped disk1→disk2 in
  YS II, game continued); 183/183 + 194/194; residual R-M35-1 (strengthen the multi_disk
  integration test) carried forward; a QA-agent git-checkout clobber incident was caught and
  recovered by the coordinator (checkpoint e360a5b), and the msx-qa agent gained a non-destructive-
  mutation rule.** Prior: **IDLE — M34 CLOSED (DEC-0045, 2026-07-09, tag v1.0.35);
  awaiting the human's live re-check of the two M34 fixes (Aleste 2 transition-beep gone; Metal
  Gear room-transition clean backdrop wipe — R-M34-1 is the final acceptance signal on the 20.7
  kHz residual).** M34 fixed the DEC-0043 playtest defect pair universally: (A) PSG/SCC
  band-limited box-average integration killed the ultrasonic point-sampling alias (Aleste 2's
  period-0 silence idiom that read as the transition beep) — zero post-fix burst blocks,
  residual ~500 RMS at 20.7 kHz sub-threshold, openMSX audio A/B silence-parity; the full
  16-surface audio byte-oracle re-baseline executed under the anti-tautology discipline. (B) an
  R#1 BL (display-enable) render gate fixed Metal Gear's room-transition slow-fill of
  half-rewritten VRAM (both references agree; MG genuinely blanks — S4 outcome (a)). QA
  CONDITIONAL PASS, both committed-evidence escalations independently confirmed; the four
  m32-aleste-play PNGs regenerated from a now-committed recipe. Fresh-tree gate: **183/183
  headless + 192/192 SDL3-ON**. NEW backlog row E4 (true band-limited resampling depth,
  on-demand). R-M32-7 CLOSED PASS (the human: "absolutely gold").
  Remaining open backlog: C1/D4 + E3 + E4 (sourcing/depth-blocked), D8/D9/D10 (renderer
  remainders), C10/F1/F2 (era-labeled, unscheduled), G3/G4/G5/G6 (on-demand; G6 = typed-BASIC
  harness, near-term candidate).
- Prior phase (closed): **M33 — CLOSED (DEC-0042, 2026-07-09, tag v1.0.34)** — repository
  housekeeping (single git stream, ONE canonical build/ via tools/bootstrap-build.ps1, CMake
  SDL3.dll staging killing the modal loader dialogs, DoD M29-M32 records restored, full docs
  truth sweep). Also DEC-0044 (daad6b8): references/tests/tools/docs/debug/.claude added to
  .gitignore for a lean compile-only remote export (already-tracked files stay tracked; new
  files under those trees need `git add -f`; add_subdirectory(tests) EXISTS-guarded).
- Prior phase (closed): **M32 — CLOSED (DEC-0040, 2026-07-09, tag v1.0.33)**; R-M32-7 verified
  PASS by the human on 2026-07-09.
- Prior phase (closed): **M32 — CLOSED (DEC-0040; QA CONDITIONAL PASS RESP-M32-003, all
  conditions discharged; tag v1.0.33)** — both DEC-0039 defects fixed universally per `docs/m32-planner-package.md` S1-S5
  with the RESP-M32-001 D-1/D-2 ratifications applied. **(A) Raster-accurate per-line rendering +
  line-interrupt delivery**: NEW `src/devices/video/vdp_scanline_accumulator.{h,cpp}` + the
  nullable `VdpRenderSyncListener` seam at the top of `V9958Vdp::io_write()` (sync-before-change,
  PixelRenderer.cc:253-394/510-517); machine write-hook adapter (write at display line L takes
  effect from L+1), finalize in `on_vsync_boundary()` (AFTER `vdp_.on_vsync()` — a documented,
  justified ordering deviation in service of the package's own AC-4/AC-5 byte-identity oracles),
  `render_frame(field)` re-routed with its exact signature kept (mutable-accumulator logical
  constness documented); O(1) fingerprint-cached line-interrupt poll in `step_cpu_instruction()`
  firing at display line (R#19 − R#23) & 0xFF (VDP.cc:518-576 + MSX.c:2091-2104, both references
  agree) — `on_line_match()`'s zero-production-call-site gap CLOSED; IE1-off software provably
  unchanged. Synthetic split-screen system oracle green incl. the adversarial IE1-off arm;
  **openMSX A/B PARITY with split-boundary delta 0** (`tools/openmsx-m32-split-ab.ps1` +
  `tools/gen-m32-split-probe.py` + `tools/m32-split-ab-compare.py`, region-structural comparator
  with adversarial self-check; `docs/m32-parity-trace-diff.md`); **Aleste 2 live smoke reaches
  NON-GARBAGE scrolling gameplay past weapon select** (`debug/frames/m32-aleste-play-*`, the
  human's exact repro resolved; A-M32-1 verified: Aleste enables IE1 mid-frame from gameplay
  start, R#19=220 HUD-split protocol). AC-5 committed-evidence sweep: byte-identical across the
  matrix EXCEPT four root-caused raster-truth divergences (bios-f150 wobble tearing;
  metalgear f1100/f1400 + mg2 f1700 D9-class sprite bands) — ESCALATED per AC-6 with evidence
  (`debug/frames/m32-divergence-*`), never rebaselined. **(B) FM mix calibration**:
  `kFmAmplitudeScale` 5 → 21 via the D-2-ratified per-channel derivation (k = round(400×31×(3/7)
  /256)), header clamp math redone (melody 48,384 / rhythm 86,016 / +PSG+2×SCC 125,216 → clamp
  REQUIRED, saturation-tested at both rails); loudness-ratio oracle green (measured 0.4335 vs
  reference 0.42857, +1.2%); zero-YM2413 byte-identity oracle green UNMODIFIED; fmON/fmOFF pair
  re-captured at k=21 (`debug/sounds/m32-fm-aleste-*`; FM contribution peak 3,780 = exactly the
  package-predicted 900×21/5). Evidence: headless fast **177/177** (171 + 6 new), SDL3-ON fast
  **186/186** (180 + 6 new); zero `src/devices/cpu/`/`src/core/` edits (ZEXALL correctly NOT
  run); ledger D8/D9/D10 added + C10/F1/F2 era shifts recorded. All changes UNCOMMITTED for
  QA/coordinator. Handoff: `docs/m32-implementation-report.md`.
- Prior phase state (planning kickoff, retained): **M32 — PLANNING (kickoff 2026-07-09,
  DEC-0039)** — the RC-playtest defect pair,
  human-authorized after first-hand v1.0.32 play of Aleste 2 ("M32: both fixes"). **(A)
  raster-accurate per-line rendering** — `Hbf1xvMachine::render_frame()` renders the whole frame
  from ONE register snapshot, so a mid-frame R#23 rewrite from a line-interrupt handler (the
  standard HUD/playfield screen split — Aleste 2 gameplay) is unrepresentable BY CONSTRUCTION:
  the human sees the HUD correct and the scroll ring-buffer un-rotated (garbage) below it, while
  every non-split screen (cutscene/title/weapon-select/game-over) is pixel-perfect. Universal
  fix: accumulate the frame scanline-by-scanline as the raster advances, reading LIVE VDP
  registers per line (openMSX PixelRenderer model; DEC-0031's line-granular collision +
  VdpRasterClock already exist as groundwork). **(B) FM mix calibration** — `kFmAmplitudeScale=5`
  leaves the (fully working, APRLOPLL-detected, genuinely keyed) YM2413 ~29 dB under the PSG:
  measured from the committed M31 evidence WAVs, FM contribution peak 900 / mean-nonzero 261 on
  ±32,767 vs PSG effects at 24,800; openMSX's own Sony_HB-F1XV.xml balances PSG:MSX-MUSIC at
  21000:9000 (~7 dB). Universal fix: re-derive the scale from the reference ratio + an A/B
  loudness comparison. Tag target v1.0.33. ZEXALL: NOT expected (no cpu/core touch planned;
  fast-subset cadence). The human also CONFIRMED the DEC-0033 sound-delay fix in live play.
- Prior phase (closed): **M31 — CLOSED (DEC-0038, 2026-07-09, tag v1.0.32 = PRODUCTION
  CANDIDATE; QA unconditional PASS with the explicit RC verdict)** — THE RELEASE
  CANDIDATE of the DEC-0035 run. YM2413 (OPLL) FM-synthesis DSP depth,
  backlog E1's formulaically-derivable subset per `docs/m31-planner-package.md` S1-S6:
  NEW `src/devices/audio/ym2413_synth.{h,cpp}` (closed-form logsin/exp tables computed at
  construction; §8 phase generation with the 19-bit A-M31-4 accumulator and the §3 MUL table;
  §5 EG decay/release EXACT — global 18-operator counter, eg_shift/eg_select, first-segment-
  shorter quirk unit-proven, durations tied to §5's own closed form; THE §2.4 ATTACK
  APPROXIMATION prominently disclosed in the mandatory header block — `YM2413NukeYktTables.ii`
  NEVER OPENED, non-opening attestation in the report; 2-deep feedback averaging with the
  FB-doubling property; KSR/Rks per A-M31-3; 9 melody channels + §6 rhythm mode with BD/TOM
  full synthesis, SD/HH/T-CY disclosed approximations, and the ×2 double-output quirk proven
  as an EXACT sample-for-sample factor; AM/VIB formula-constrained LFOs); ADDITIVE
  `Ym2413Opll::advance_cycles/fm_sample` (native tick per 72 cycles = exact 49716 Hz, ZOH,
  the exact 9:8 decimation 648 = 9×72 unit-proven); ADDITIVE third `MachineAudioMixer` source
  (kFmAmplitudeScale=5 documented policy; **the zero-YM2413 byte-identity HARD oracle green**
  — nullptr AND silent-attached both byte-identical to the v1.0.31 arithmetic); Sdl3App passes
  the machine OPLL to the mixer. RC gate: headless FULL UNFILTERED ctest green INCLUDING the
  ZEXALL/ZEXDOC sweep (durable log `docs/m31-rc-zexall-log.txt`, error_markers=0 both suites);
  headless fast 171/171, SDL3-ON fast 180/180 (163/172 baselines + 8 new); 6-item smoke matrix
  with committed artifacts — BIOS logo→BASIC-phase (DEC-0031 intact), Metal Gear GAMEPLAY
  reached, Aleste 2 auto-ID + boots, **metalgear2_scc.rom auto-ID (SHA1 78560a5c…) + boots to
  title with REAL SCC samples through the mixer (first activity frame 1244 — closes A-M29-4)**,
  MSX-DOS disk → the real "MSX BASIC version 3.0 / Disk BASIC version 1.0 / Ok" prompt
  captured, and **REAL-TITLE FM CONFIRMED: Aleste 2 writes YM2413 key-ons (frame ~698) and the
  FM-included mix audibly diverges from the FM-muted mix of the identical deterministic run**
  (`debug/sounds/m31-fm-aleste-fmON/fmOFF.wav`; A-M31-1 "APRLOPLL"@0x18 verified). A/B:
  audio-sample N/A BY DESIGN + CPU-visible surface unchanged with git-diff proof
  (`docs/m31-parity-trace-diff.md`). Ledger: E1 → DONE (M31), NEW row E3 (license/sourcing-
  blocked remainder, C1/D4 standard), G1 cross-note. Health: validate-assets green (3 ROMs incl.
  metalgear2_scc.rom), checksums refreshed, no new TODO/FIXME, both executables launch.
  Closure: QA's F2 cosmetic comment fix applied at commit; committed 00023cd with the curated
  evidence set (5 smoke PNGs + 3 WAVs, .gitignore M31 RC exception block); annotated tag
  v1.0.32. The DEC-0035 run is COMPLETE: M29 v1.0.30 → M30 v1.0.31 → M31 v1.0.32.
- Prior phase (closed): **M30 — CLOSED (DEC-0037, 2026-07-09, tag v1.0.31)**. Universal
  cartridge mapper auto-identification (backlog G2 — the Aleste-2 usability fix, delivered as a
  UNIVERSAL mechanism, nothing game-keyed). Delivered per `docs/m30-planner-package.md` S1-S6:
  clean-room FIPS 180-4/RFC 3174 SHA-1 (`src/machine/sha1.*`, published-vector-gated;
  `references/fmsx-60/source/EMULib/SHA1.c` deliberately never opened — R-M30-2 discipline
  held); tolerant softwaredb.xml subset parser (`src/machine/software_db.*`,
  RomDatabase.cc-grounded semantics, GPL data file stays in references/ and is only parsed at
  runtime, zero real-DB content in any fixture); precedence engine + re-derived guessRomType
  heuristic + verbatim A-E message formatter (`src/machine/cartridge_identifier.*`; the four
  openMSX-vs-fMSX disagreements recorded in-code per DEC-0030, all arbitrated to openMSX);
  ONE shared resolver consumed by BOTH executables (`--softwaredb` additive, `--cartN-type auto`
  accepted, explicit-type invocations byte-for-byte unchanged per the re-run A-M30-3 grep,
  DB-identified-but-unsupported -> loud message B + non-zero exit/startup abort). End-to-end:
  `sony_msx_headless --cart1 roms/aleste.rom` (NO type flag) prints the verbatim message A
  ("Aleste 2" (KonamiSCC) via softwaredb SHA1 match) and boots; the heuristic-only path (DB
  removed) ALSO lands KonamiSCC for the same file (scores KonamiSCC=6 Konami=6, tie-break) — a
  free corroboration. Evidence: headless fast subset **163/163** (159 baseline + 4 new), SDL3-ON
  fast subset **172/172** (168 baseline + 4 new, dummy drivers; slow sweep NOT run per DEC-0035,
  zero CPU/core touch confirmed via `git diff v1.0.30` — empty for src/devices, src/core,
  src/peripherals, audio_pacer.*); A/B **AGREEMENT** (`docs/m30-identification-ab.md`,
  `tools/openmsx-m30-identification-ab.ps1`): openMSX with NO `-romtype` and this emulator with
  NO `--cart1-type` both resolve the same file to KonamiSCC — Side-B queried live via the
  source-verified `machine_info device` mappertype mechanism (R-M30-6 verified in
  MSXMotherBoard.cc/MSXRom.cc/HardwareConfig.cc/RomFactory.cc BEFORE scripting). Ledger: G2 ->
  DONE (M30) with named non-goals (CARTS.SHA/CARTS.CRC skipped with reasoning; PAGE2 subsumed by
  A-M19-8); E1 renumbered M30->M31 THIS cycle per package §2.7 (citing DEC-0035); C10/F1/F2
  numeric-owner shift notes (M32/M33/M34-era). All changes UNCOMMITTED, awaiting QA
  (`docs/m30-implementation-report.md` is the handoff artifact).
- Prior phase (closed): **M29 — CLOSED (DEC-0036, 2026-07-09, tag v1.0.30)**. Delivered per
  `docs/m29-planner-package.md` S1-S6: `CartridgeMapperType::KonamiSCC` + CLI value "KonamiSCC";
  `src/devices/audio/scc_wavetable.*` (plain-SCC Real mode: De Schrijder AmpOut=640+Σ mixing law
  reproduced literally in a unit oracle, Pazos deformation register incl. read-as-write-0xFF,
  NYYRIKKI latching/restart/period<9 stop, enen power-on state; mode-aware-ready for the new G5
  remainder); `src/devices/cartridge/cartridge_konami_scc_rom.*` (opposite-of-plain-Konami
  mirroring, 0x800-wide bank windows, masked (v&0x3F)==0x3F enable incl. 0xBF, both-effects
  0x9000 write, 0x9800-0x9FFF SCC window mirrored via addr&0xFF); machine `scc_chip()` accessor;
  SDL3-independent `src/frontend/machine_audio_mixer.*` wired into the presenter with
  `AudioPacer`/`PsgAudioPump` byte-for-byte untouched (DEC-0033) and the zero-SCC byte-identity
  hard regression oracle unit-proven. Evidence: headless fast subset **159/159**, SDL3-ON fast
  subset **168/168** (dummy drivers; slow sweep NOT run per DEC-0035); openMSX A/B **EMPTY DIFF
  over 140 instructions** (`docs/m29-parity-trace-diff.md`, `tools/openmsx-m29-scc-parity.ps1`,
  A-M29-3 forcing syntax verified in source first); audio-sample A/B recorded N/A by design per
  the package's own §2.5 disposition. BONUS real-ROM smoke (coordinator-directed): `roms/
  aleste.rom` **boots AND starts** under `--cart1-type KonamiSCC` (loader banner "Konami8
  mapper", game intro running after the scripted keypress — `debug/frames/m29-aleste-f*.png`).
  Ledger: G1 → DONE (M29), NEW row G5 (SCC-I) added. All changes UNCOMMITTED, awaiting QA
  (`docs/m29-implementation-report.md` is the handoff artifact). Temporary main.cpp probe
  reverted (`git checkout -- src/main.cpp`).
- Prior objective (closed): The M21-M28 continuation is COMPLETE (v1.0.21..v1.0.28). The human's earlier-deferred
  debug/-folder artifact request (raised 2026-07-08 after M25 completed) was fully discharged across
  M26 (SDL3 frontend) and M27 (Production Hardening + Debug/Test Tooling). M28 ("Release Candidate")
  then swept the remaining backlog and delivered a full-project health audit; a real, human-reported
  interactive-use bug (a permanent black screen on SDL3 boot) was found and fixed mid-cycle as
  DEC-0026, folded into M28's own closure commit/tag per QA's recommendation. Same established
  cadence continued throughout: planner → developer → QA → release decision, proceeding through tag
  without an extra pause on a clean QA PASS, STOP and consult the human otherwise (this fired
  TWICE this session — M24 and M28 — both resolved via the identical "fix, re-confirm, then tag"
  human choice). Standing "zero license-sensitive future work" and "don't run the slow test unless
  necessary" directives remain in force (project memories `feedback_license_sensitive_scope.md` /
  `feedback_slow_test_cadence.md`). No further milestone has been requested by the human yet.
- Prior Phase: M28 ("Release Candidate — Backlog Closure Sweep + Full-Project Health Audit") —
  **CLOSED 2026-07-08 (DEC-0027/REQ-M28-004, git tag v1.0.28)**, folding **DEC-0026** (a separately
  discovered/fixed VDP boot-hang bug, outside M28's own approved scope) into the same closure
  commit/tag per QA's explicit recommendation. Approved scope: IN-M28 = E2 (YM2413 write-timing
  constraint, DONE) + C5 (auto-disk-boot investigation, dual-outcome honest acceptance, reached
  outcome (b), stays IN-PROGRESS) + the mandatory release-candidate health audit. DEFERRED with
  named follow-ons: G1→M29, E1→M30 (scoped subset), C10→M31, F1→M32, F2→M33; G2/G3/G4 indefinite
  (rows left unchanged). **C1/D4 ruled UNBUILDABLE-WITHOUT-FABRICATION** per
  `feedback_license_sensitive_scope.md` — no independent, non-GPL source exists for the ~340-entry
  VDP slot tables; stays OPEN, no milestone number assigned; both rows additionally carry DEC-0026's
  factual cross-reference note.

  **DEC-0026** (`docs/vdp-vr-hr-boot-hang-fix-report.md`): a real, CPU-driven interactive SDL3 boot
  of the actual Sony BIOS produced a PERMANENT black screen — root-caused to `V9958Vdp` status
  register S#2 bits VR/HR being hardcoded to 0 (a disclosed M23 simplification), on which the real
  BIOS's very first VDP-init step polls in a wait-for-toggle loop before writing a single VDP
  register. Fixed via a new, optional `VdpClockSource` (X-pattern of the project's existing clock
  adapters), computing VR/HR live from real elapsed raster time, grounded in the V9958 fact-sheet's
  own NTSC timing tables and reusing M23's own already-shipped raster-position foundation — zero
  license-sensitive data involved, C1/D4's actual remainder untouched. A genuine regression (the M24
  CP/M-isolation test) was found and correctly fixed same-cycle (two specific, now-legitimately-
  time-dependent status bits masked out of ONE test's snapshot comparison, every other byte still
  exact). A SECOND, deeper finding was precisely located via a real openMSX A/B instruction trace
  (exact divergence: instruction #145,503, a single-byte memory-mapper-segment content mismatch) but
  intentionally left OPEN, honestly disclosed, NOT investigated further this cycle per the human's
  own explicit direction.

  **QA (`docs/m28-qa-signoff.md`, RESP-M28-003) returned a Conditional Pass** covering BOTH M28
  and DEC-0026 together — every substantive technical claim independently reproduced from a
  completely fresh build (146/146 full unfiltered `ctest`, ZEXALL/ZEXDOC clean, E2/C5/health-audit
  claims all reconfirmed, zero CPU/core touch) — **including a direct, independent, controlled
  before/after reproduction of DEC-0026's headline claim** (built the pre-fix code in a real `git
  worktree` checkout of v1.0.27, confirmed the VDP genuinely stays all-zero forever there; confirmed
  the post-fix code configures it by frame 74). Two documentation/ledger-only findings gated the
  Conditional Pass (neither a code defect): the C5 trace document's specific figures were stale
  post-DEC-0026 (fixed with an honest superseded-figure caveat, not a silent renumbering — the
  original "legitimate idle behaviour" interpretation was itself corrected); the ledger state files
  hadn't yet been refreshed to reference DEC-0026. Both fixed directly by the coordinator per the
  human's explicit "fix, re-confirm, then tag" choice (mirroring the M24 precedent exactly).

  M27 (`docs/m27-*`, v1.0.27) remains CLOSED. `feedback_slow_test_cadence.md`'s mechanical gate
  fired TWICE this cycle (once for E2's touch to `ym2413_opll.{h,cpp}`, once again for DEC-0026's
  touch to `v9958_vdp.{h,cpp}`) — the full, unfiltered `ctest` ran to completion twice, both green
  (144/144 then 146/146), independently reproduced a third time by QA from a clean rebuild.
- Phase Owner: MSX Master Agent (coordinator)
- Phase Status (M27, closed): implementation complete for all four named items — (1) full
  CPU/memory/VRAM state-dump CLI wiring via a new headless `--debug-session` mode + additive SDL3
  `Sdl3AppConfig`/`sdl3_cli` fields; (2) real, decoded PSG audio capture (`src/frontend/
  psg_audio_dump.*` + `tools/audio-dump-to-wav.py`, genuinely reusing M26's `PsgAudioPump`), a real,
  committed, non-silent example WAV (`debug/sounds/m27-example-tone.wav`); (3) keystroke-sequencing/
  scripted-input automation (`src/peripherals/msx_key_names.*` + `src/machine/input_script.*`,
  `--input-script` on both executables, a hard SDL3-gated cross-consistency test proving the new
  headless key-name table agrees with `Sdl3InputMapper::scancode_map()` exactly); (4) event-log CLI
  wiring + a genuine, adversarially-validated, byte-for-byte replay-determinism system test (two
  independent machines byte-identical event logs; a third, deliberately-diverged machine produces a
  genuinely different one). Zero changes to any pre-existing file under `src/devices/`,
  `src/peripherals/`, or `src/core/` (confirmed via `git diff v1.0.26` at 3 separate gates); zero new
  `Hbf1xvMachine` method needed. Headless fast-subset 140/140; SDL3-ON fast-subset 149/149; the slow
  `hbf1xv_m24_zexall_system_test` not run this cycle per the standing `feedback_slow_test_cadence.md`
  directive (git-diff-confirmed genuinely unnecessary at every gate, by developer, coordinator, AND
  QA independently). This milestone closes zero backlog rows (infrastructure/tooling scope); C5's
  row gained a factual, non-status-changing cross-reference note only; backlog E1 remains OPEN,
  untouched.

  **QA (`docs/m27-qa-signoff.md`, RESP-M27-003) returned a clean, unconditional Pass** — zero
  blocker/high/medium-severity findings; two Low-severity, non-blocking notes, neither requiring a
  fix (a narrative-vs-acceptance-criterion non-discrepancy on which backlog row gets a
  cross-reference note; the audio demo's single-channel design covering half the documented PCM
  range). QA independently rebuilt both configurations from clean, reproduced both fast-subset
  counts exactly, independently launched the real `sony_msx_headless.exe --debug-session` binary
  itself twice (byte-identical event logs/SHA-256), independently decoded the committed WAV via raw
  PCM read, independently confirmed the 71/71 key-name cross-consistency table's completeness, and
  independently judged the flat-RAM-driver negative-control design correction genuinely sound.
  Per the standing continuation cadence and this milestone's own clean PASS, the coordinator
  proceeded to close M27 and tag `v1.0.27` without an additional human pause. Full details in
  `docs/m27-implementation-report.md` / `docs/m27-qa-signoff.md`.
- Phase Status (M26): closed by coordinator release decision (2026-07-08, DEC-0024/REQ-M26-004).
  First frontend/presentation-layer milestone, and the largest/most architecturally novel to date.
  `Hbf1xvMachine::on_vsync_boundary()` — a pure,
  mechanical extraction of `run_frame()`'s pre-M26 body (the ONLY behavior-affecting change to
  `src/machine/`; `git diff --stat src/devices src/peripherals src/core` confirmed EMPTY) — lets a
  real-time SDL3 loop drive the CPU via `step_cpu_instruction()` without the `run_frame()`
  double-count hazard (A-M26-5). New `src/frontend/` (`Sdl3App`, `Sdl3VideoPresenter`,
  `Sdl3AudioPresenter`+`PsgAudioPump`, `Sdl3InputMapper`, `sdl3_cli`): a genuine, working SDL3
  application — window/renderer/audio-stream lifecycle, zero-conversion `SDL_PIXELFORMAT_XRGB1555`
  video blit (independently pixel-readback-proven bit-for-bit), real PSG/YM2149 audio synthesis
  wired from the frontend layer for the FIRST time (`advance_cycles()` had zero call sites in this
  project before M26), a 71-entry keyboard-matrix mapping table + joystick + PAUSE/Speed-Controller/
  Ren-Sha-Turbo bindings (all exhaustively `SDL_PushEvent`-injection tested), and CLI/asset wiring
  including a new `--disk` flag (A-M26-6). YM2413/FM-PAC honestly, deliberately left SILENT —
  backlog **E1** stays OPEN, zero waveform/DSP-shaped code added anywhere (R-M26-5, independently
  grep-verified). The ONE new debug/testing capability the coordinator authorized — decoded-
  `FrameBuffer`→real color PNG capture (`src/machine/frame_dump.*` + `tools/frame-to-png.py`) —
  ships with a real, committed example (`debug/frames/m26-example-boot.png`, a 256×192 16-color-bar
  GRAPHIC4 test scene). A real, empirically-confirmed environment finding: SDL3 was NOT
  pre-installed in this execution environment, but the vendored `references/sdl3/` tree (zlib-
  licensed) was built and locally installed once (`build/_sdl3_install/`, gitignored) to obtain a
  real `SDL3Config.cmake` — resolving the environment risk without copying any SDL3 source into
  `src/`; the "dummy" video/audio driver mechanism was then independently, empirically confirmed to
  work end to end via a real `sony_msx_sdl3.exe --hidden-window --max-frames N` session. Headless
  suite: 134/134 (130 prior + 4 new). SDL3-ON suite (a second, dedicated build directory): 140/140
  (134 shared + 6 new SDL3-gated), all under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`,
  zero `SDL_Delay` anywhere in the `ctest` execution path. Full details, including the slow-sweep
  re-run figure, in `docs/m26-implementation-report.md`; the honest A/B/human-observed disposition
  (mostly BLOCKED/N-A, does not gate closure per Acceptance Criterion 10) is in
  `docs/m26-frontend-evidence.md`.

  **QA (`docs/m26-qa-signoff.md`, RESP-M26-003) returned a clean, unconditional Pass with ZERO
  findings of any severity** — the cleanest QA cycle of this entire session. From two clean,
  from-scratch rebuilds PLUS a third, fully independent rebuild of the vendored SDL3 source to a
  brand-new install directory (confirming the environment finding is genuinely reproducible, not a
  fluke): headless 134/134 (a fresh 30.8-min ZEXALL/ZEXDOC re-run, byte-identical to every prior
  run); SDL3-ON 139/139, dummy drivers set externally. Cross-checked every one of 20 new files'
  line counts against the implementation report (exact match on all), every cited SDL3 API against
  the actual vendored headers, launched the real `sony_msx_sdl3.exe` itself three times (all exit
  0), and regenerated the committed PNG from its dump (byte-identical SHA-256). One trivial,
  non-gating doc-comment imprecision noted for completeness only (`run_frame_dump_demo()` says
  "256×212," actual evidence is 256×192 — harmless). Per the milestone's own standing directive (a
  clean PASS is the bar, given the novel territory), the coordinator proceeded through the
  release-decision/tag step without an additional human pause. Tagged `v1.0.26`.
- Phase Status (M25): closed by coordinator release decision (2026-07-08, DEC-0023/REQ-M25-004).
  `Mb670836PauseController` (`src/devices/chipset/mb670836_pause.{h,cpp}`) — a machine-level
  CPU-execution gate consulted at the very top of `step_cpu_instruction()`, before any opcode
  decode, so PC/R/every register stay completely frozen while engaged (architecturally distinct
  from the Z80's own CPU-internal `HALT`, which keeps incrementing R). The manual PAUSE button
  (toggle semantics) and the Speed Controller's VBlank-synced duty cycle (`kPeriodFrames=8`) OR
  into one combined `cpu_should_pause()` gate. `RenshaTurbo`
  (`src/peripherals/rensha_turbo.{h,cpp}`) — a simpler, peripheral-level autofire signal generator
  grounded in real openMSX behavior and the real per-machine `Sony_HB-F1XV.xml` calibration
  (`min_ints=47`/`max_ints=221`), wired into `KeyboardMatrix`/`JoystickPorts` via additive,
  default-nullptr OR-mask attach points (byte-for-byte regression guard when unattached). 6 new
  dedicated unit/integration/system tests prove hardware PAUSE genuinely freezes CPU state and
  cannot be bypassed via any CPU-visible API, the Speed Controller's duty cycle is
  deterministic/hand-computable, and Ren-Sha Turbo never forces a spurious press (R-M25-6). Real
  openMSX A/B evidence: Ren-Sha Turbo achieved genuine live PARITY against the real `Sony_HB-F1XV`
  openMSX machine (independently reproduced by BOTH the developer and QA, end-to-end, against real
  WSL openMSX); Hardware PAUSE/Speed Controller is honestly BLOCKED (openMSX genuinely does not
  model this Sony-specific hardware anywhere — an exhaustive, cited search independently confirmed
  by the developer, coordinator, AND QA, does not gate C8's closure per Acceptance Criterion 9).
  `git diff v1.0.24` confirms zero changes to any CPU/VDP/audio/RTC/FDC/cartridge/memory/halnote/
  kanji/core file and all 12 named zero-tolerance CPU-timing-oracle test files (byte-for-byte
  unchanged, independently reproduced three times). Full regression: 130/130 (124 prior + 6 new),
  zero regression, including the slow `hbf1xv_m24_zexall_system_test` re-run to completion by the
  developer, the coordinator, AND QA (three separate from-scratch runs this cycle alone).

  **QA returned a clean, unconditional Pass (`docs/m25-qa-signoff.md`, RESP-M25-003)** — the
  standing "STOP if not a clean PASS" condition, which fired once for M24, did NOT fire here. QA's
  sole finding (Low, non-blocking): its own fresh `find` sweep for every `Sony_HB-F1*.xml` file
  found a SIXTH Sony machine (`Sony_HB-F1XDmk2.xml`) missed by every prior search — independently
  confirmed it also wires no Pause/SpeedController device, reinforcing rather than undermining the
  BLOCKED disposition. Coordinator applied this "five"→"six" documentation fix directly across
  `docs/m25-planner-package.md`, `docs/m25-implementation-report.md`, `docs/m25-parity-trace-diff.md`,
  and `tools/openmsx-m25-rensha-parity.ps1` (verifying the PowerShell backtick-escaping renders
  correctly via a standalone parse-and-render check before committing — a direct, applied lesson
  from that same script's own earlier self-caught escaping bug). See
  `docs/m25-implementation-report.md` / `docs/m25-parity-trace-diff.md` / `docs/m25-qa-signoff.md`
  for the full evidence.
- Phase Status (M24, prior): M24 closed by coordinator release decision (2026-07-08, DEC-0022/REQ-M24-005).
  This milestone's own standing directive named a Conditional-Pass stop condition that fired for
  the first time this run: QA (`docs/m24-qa-signoff.md`, RESP-M24-003) independently reproduced
  the full 134/134 ZEXALL/ZEXDOC headline claim from a genuinely clean rebuild (a THIRD from-
  scratch reproduction, byte-for-byte identical to the developer's and coordinator's own),
  confirmed license isolation/device isolation/the adversarial self-check/the combinatorial-total
  arithmetic/openMSX A/B PARITY all independently, and found no CPU-core defect — but returned a
  **Conditional Pass**, not a clean PASS, gated on one hardening fix: `tests/system/
  hbf1xv_m24_zexall_system_test.cpp` checked marker COUNT but never hard-asserted `error_markers ==
  0` for either suite. Per the standing directive, the coordinator stopped and consulted the human,
  who chose "Fix, re-confirm, then tag." The developer added the two hard assertions
  (`Zexall_ZeroErrorMarkers`/`Zexdoc_ZeroErrorMarkers`, purely additive, test-code-only) and
  re-ran the full slow sweep to completion a FOURTH independent time this cycle (1638.22s),
  landing on the identical 5,764,169,474-instruction/67-0-marker result yet again with the new
  hard gates genuinely exercised; the coordinator independently re-verified via direct file read
  plus its own fast-subset re-run. QA's DEC-0021 ruling (disk-boot-A/B sub-claim stays BLOCKED-as-
  recorded; `disks/` reserved for a future dedicated C5 investigation) was ratified as final.
  `references/zexall/` (GPL v2, YAZE-AG) committed as part of the closure commit, ending 5
  milestones (M19-M23) of sitting staged-but-uncommitted. Fourteen milestones M11-M24 now tagged
  (v1.0.11..v1.0.24) at the time M24 closed; FIFTEEN now tagged (v1.0.11..v1.0.25) with M25's
  closure.
- Current full suite (headless, `-DSONY_MSX_ENABLE_SDL3=OFF`, the default): 134/134 (130 prior +
  4 new: `machine_hbf1xv_m26_vsync_boundary_integration_test`, `machine_frame_dump_unit_test`,
  `frontend_psg_audio_pump_unit_test`, `frontend_sdl3_cli_unit_test`), zero regression through M26.
  A SECOND, dedicated build directory with `-DSONY_MSX_ENABLE_SDL3=ON` (+
  `-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install` if SDL3 is not otherwise installed — see M26
  Phase Status above for the reproducible build sequence) runs 140/140 (134 shared + 6 new
  SDL3-gated), entirely headlessly under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`. NOTE:
  `hbf1xv_m24_zexall_system_test` (pre-existing since M24) genuinely takes ~25-35 minutes to run
  (both real ZEXALL/ZEXDOC suites to completion) — registered with the CTest LABEL
  `m24_slow_full_sweep` so the routine evidence-gate cadence can exclude it explicitly via
  `ctest -LE m24_slow_full_sweep` (133/133 in ~4-7s) while the default, unfiltered `ctest`
  invocation still includes it for real. All 4 new M26 headless tests and all 6 new M26 SDL3-gated
  tests are fast (no new slow-sweep-class test was added this cycle).

## M21-M28 run summary

- **M21 (VDP rendering depth, v1.0.21)**: First pixel-rendering output path. `VdpFrameRenderer`/`FrameBuffer` — deterministic, pull-model, frozen-register-snapshot renderer, RGB555 pixels, zero new clock consumer. Every Target-Spec mode byte-exact (GRAPHIC7 GGGRRRBB byte layout; MULTICOLOR's real 256-wide canvas). YJK/YJK+YAE decode byte-exact, incl. an independently-verified floor-vs-truncation finding. G6/G7 planar interleave's CPU-port + display-path pieces closed (command-engine piece carried to M22). `ctest` 106/106. Closes D1/D5/D6; D7 IN-PROGRESS (M21 partial). QA PASS (`docs/m21-qa-signoff.md`).
- **M22 (sprites + VDP command engine, v1.0.22)**: `SpriteEngine` (D2) and `VdpCommandEngine` (D3), both owned inside `V9958Vdp`. Sprite Mode 1/2 byte-exact per `SpriteChecker.cc/.hh` (EC/CC/IC bits; color-0/TP transparency — a fact-sheet-vs-source discrepancy resolved in favor of source). Full R#32-46 register file, all 13 commands via a hybrid execution model (10 atomic; LMCM/LMMC/HMMC event-driven, mirroring the M16 FDC DRQ/INTRQ precedent). D7 closes IN FULL via 5 new coordinate-address functions (confirmed to bypass R#2 entirely — a genuinely new finding). `ctest` 117/117. Closes D2/D3; D7 DONE in full. QA PASS (`docs/m22-qa-signoff.md`) via genuinely independent, adversarial scrutiny — QA hand-verified the raw A/B divergence numbers itself and corrected the developer's causal narrative.
- **M23 (exact cycle timing, v1.0.23)**: Deliberately conservative scope, given this touches the highest-risk surface in the project (CPU-visible timing, adjacent to the zero-tolerance M9/M12 CPU-timing oracles). Closes C2 (Z80 HALT-R) in full: `Z80aCpu::step()`'s halted branch now calls `increment_refresh_register()`, and the already-existing machine-level formula automatically bills the correct 5T. Exactly one golden test deliberately updated (t3 4→5, elapsed_cycles 22→23). C1/D4 advance to IN-PROGRESS (M23 partial) with a real, tested, non-gating VDP access-timing foundation — the full slot tables and any actual CPU-execution gating explicitly declined this cycle, since the M21/M22 system tests already contain back-to-back `OUT (#98),A` writes that real gating could silently corrupt. `ctest` 121/121; all 10 named oracle suites plus all 6 M21/M22 device files confirmed genuinely untouched. QA PASS (`docs/m23-qa-signoff.md`) via exceptional scrutiny — QA independently re-ran the C2 A/B harness and designed its own exploratory probes to refine the developer's divergence hypothesis into a more precise, still-honest explanation.
- **M24 (ZEXDOC/ZEXALL full parity sweep, v1.0.24)**: CPU-validation milestone — a genuine, generic `CpmBdosHarness` (zero zexall-specific knowledge) ran the real, GPL-licensed ZEXALL/ZEXDOC Z80 exerciser suite against the already-QA-verified Z80A CPU core to genuine CP/M warm-boot completion. **134/134 group verdicts PASS**, independently reproduced FOUR separate times from clean rebuilds (developer, coordinator, QA, post-fix confirmation), every time byte-for-byte identical (5,764,169,474 instructions/suite, 67/0 marker split). Adversarial self-check PASSED; openMSX A/B bounded-prefix PARITY independently re-run twice (developer, QA). `ctest` 124/124; zero changes to `src/devices/`/`src/peripherals/`/`src/core/`. QA's first-ever Conditional Pass this run (a real, if narrow, regression-harness gap — see Phase Status above) was fixed and reconfirmed per the human's explicit choice before tagging. Closes C3 in full.
- **M25 (Sony speed-controller + hardware PAUSE + Ren-Sha Turbo, v1.0.25)**: First milestone implementing genuinely new, never-previously-scoped HB-F1XV-specific hardware. New `Mb670836PauseController` — a machine-level CPU-execution gate at the very top of `step_cpu_instruction()`, freezing PC/R/every register while engaged (distinct from the Z80's own `HALT`). New `RenshaTurbo` autofire, wired into `KeyboardMatrix`/`JoystickPorts` via additive OR-mask attach points. Zero changes to any CPU/device-core file; all 12 named zero-tolerance CPU-timing-oracle files confirmed byte-for-byte unchanged, independently reproduced three times (developer, coordinator, QA). `ctest` 130/130, including three separate from-scratch re-runs of the M24 ZEXALL/ZEXDOC sweep. Real, live openMSX A/B PARITY for Ren-Sha Turbo against the actual `Sony_HB-F1XV` machine (reproduced twice, developer and QA); Hardware PAUSE/Speed Controller honestly BLOCKED (openMSX models none of this Sony-specific hardware anywhere, independently confirmed three times). QA returned a **clean, unconditional Pass** — the Conditional-Pass stop condition did NOT fire this time. QA's sole finding (a "five"→"six" Sony-machine-XML citation undercount, Low/non-blocking) was fixed directly by the coordinator per QA's own recommendation. Closes C8 in full. **This closes the full M24-M25 continuation.**
- **M26 (SDL3 Frontend, v1.0.26)**: The largest, most architecturally novel milestone to date — first frontend/presentation-layer code, first real-time loop, first real audio wiring. New `Hbf1xvMachine::on_vsync_boundary()` (a pure, mechanical `run_frame()`-body extraction, the ONLY behavior-affecting `src/machine/` change) lets a real-time SDL3 loop drive the CPU via `step_cpu_instruction()` without the `run_frame()` double-count hazard. Zero changes to any CPU/device/peripheral/core file, independently confirmed three times (developer, coordinator, QA), including all 12 CPU-timing-oracle files and `ym2413_opll.*` specifically. New `src/frontend/` (`Sdl3App`, `Sdl3VideoPresenter`, `Sdl3AudioPresenter`+`PsgAudioPump`, `Sdl3InputMapper`, `sdl3_cli`): zero-conversion `SDL_PIXELFORMAT_XRGB1555` video blit (pixel-readback-proven); real PSG audio wired for the first time in this project's history; a 71-entry keyboard mapping table plus joystick/PAUSE/Speed-Controller/Ren-Sha-Turbo bindings, exhaustively `SDL_PushEvent`-tested. YM2413/FM-PAC honestly, deliberately left silent — backlog E1 stays OPEN. The one authorized new debug capability — decoded-`FrameBuffer`→PNG capture — ships with a real, committed, independently-byte-reproduced example. A real environment finding (SDL3 not pre-installed, resolved by building the vendored source) was independently reproduced end-to-end THREE times (developer, coordinator, QA's own brand-new install). `ctest` 134/134 headless + 139-140/140 SDL3-ON, both under the real "dummy" video/audio drivers. QA returned a **clean, unconditional Pass with ZERO findings of any severity** — the cleanest QA cycle of this session. Closes C9 in full. Names a future **M27 "Production Hardening + Debug/Test Tooling"** milestone for everything else from the human's original debug/-tooling request.
- **M27 (Production Hardening + Debug/Test Tooling, v1.0.27)**: Infrastructure/tooling milestone — closes zero backlog rows outright. Four named items: (1) new headless `--debug-session` mode (`src/main.cpp`, wholly additive) + additive SDL3 fields wiring the pre-existing `write_state_dump()`/`write_cpu_trace()`/`write_event_log()`, a new `--disk` flag, and `--input-script` — zero new `Hbf1xvMachine` method needed; (2) real, decoded PSG audio capture to file (`src/frontend/psg_audio_dump.*`, genuinely reusing M26's `PsgAudioPump`) + `tools/audio-dump-to-wav.py`, shipping a real, committed, non-silent example WAV independently decoded three times; (3) a general-purpose, deterministic keystroke-sequencing/scripted-input mechanism (`src/peripherals/msx_key_names.*` + `src/machine/input_script.*`, 71-entry table cross-consistency-tested against `Sdl3InputMapper`); (4) event-log CLI wiring + an adversarially-validated, byte-for-byte replay-determinism system test (two independent machines identical logs; a third, deliberately-diverged machine — via a genuine, self-caught flat-RAM-driver design correction — produces a different one). A self-caught finding: `PsgYm2149::write_register()` is actually private; the demo correctly uses the public `write_address()`/`write_data()` ports. Zero changes to any pre-existing file under `src/devices/`, `src/peripherals/`, or `src/core/`, confirmed via `git diff v1.0.26` at THREE separate gates (developer, coordinator, QA). Per the human's standing `feedback_slow_test_cadence.md` directive, the slow `hbf1xv_m24_zexall_system_test` was not run at any gate this cycle — confirmed genuinely unnecessary by all three parties independently. Headless fast-subset 140/140; SDL3-ON fast-subset 149/149; every pre-existing M26 SDL3 test remains green. QA returned a **clean, unconditional Pass** — zero blocker/high/medium findings, two Low non-blocking notes. Backlog C5 gains a factual cross-reference note only (status unchanged); E1 remains OPEN, untouched.
- **M28 (Release Candidate — Backlog Closure Sweep + Full-Project Health Audit, v1.0.28)**: Swept the remaining 34-row backlog and delivered a full-project health audit. Planner triaged 11 remaining OPEN/IN-PROGRESS rows: IN-M28 = E2 (YM2413 write-timing gate, DONE) + C5 (auto-disk-boot investigation, honest outcome (b), stays IN-PROGRESS) + the health audit (found+fixed two small things: two dead M0-era placeholder files, one stale citation path; correctly declined two other findings exceeding the fix-scope bar). DEFERRED with named owners: G1→M29, E1→M30 (scoped subset), C10→M31, F1→M32, F2→M33; G2/G3/G4 unchanged. **C1/D4 ruled UNBUILDABLE-WITHOUT-FABRICATION** per the standing license-sensitive-scope directive — no independent source for the ~340-entry VDP slot tables. Mid-cycle, a real, human-reported bug was found and fixed as **DEC-0026**: a permanent black screen on real SDL3 boot, root-caused to VDP status bits VR/HR being hardcoded to 0 (blocking the real BIOS's own VDP-init wait-loop forever) — fixed via a new, optional raster-position clock source, zero license-sensitive data involved, folded into M28's own closure commit/tag per QA's recommendation. A second, deeper finding (an exact, precisely-located memory-mapper segment-content divergence, found via a real openMSX A/B trace) was intentionally left open, honestly disclosed, per the human's explicit direction not to chase it further this cycle. `ctest` 146/146 (independently reproduced three times, including ZEXALL/ZEXDOC clean). QA returned a **Conditional Pass** — the second time this session's stop-and-consult rule fired (after M24) — on two documentation/ledger-only findings (stale C5 trace figures post-fix; ledger state files not yet referencing DEC-0026), both fixed directly by the coordinator per the human's "fix, re-confirm, then tag" choice. Closes E2 in full.

## Standing watch-items (carried forward, none blocking)

1. `references/zexall/` (legally-sourced ZEXALL/ZEXDOC suite) is now COMMITTED as part of M24's closure (DEC-0022) — resolved, no longer a watch-item.
2. C5 (FDC full-boot residual) still needs the real auto-disk-boot trigger investigated. The
   human added real MSX-DOS system-disk images at `disks/msxdos22.dsk` (737,280 bytes),
   `disks/msxdos23.dsk` (737,280 bytes), and an unpacked `disks/msxdos24/` tree (`COMMAND2.COM`,
   `MSXDOS.SYS`, `MSXDOS2.SYS`, `AUTOEXEC.BAT`, `HELP/`, `UTILS/`, version 2.32 per `MSXDOS2.SYS`'s
   embedded string) — all three sized/shaped consistent with the HB-F1XV's documented 720 KB 3.5"
   floppy spec (DEC-0021). QA formally ruled (M24 closure, DEC-0022) that redoing C3/M24's
   disk-boot-A/B sub-claim against this asset would require solving C5's own still-unsolved
   auto-boot-trigger problem plus new keystroke-automation scope — out of scope for a QA-step
   fix, so it stays BLOCKED-as-recorded. `disks/` is reserved for a FUTURE, dedicated C5 milestone.
3. Any test asserting real ROM/asset content must call `machine.set_asset_root(SONY_MSX_BIOS_DIR)` before `cold_boot()` with the matching `tests/CMakeLists.txt` compile-definition wiring, or it silently 0xFF-fills under `ctest` (DEC-0016).
4. M20's disclosed sub-mapper-shadow A/B divergence (openMSX 19.1 runtime-side) could be re-checked against a newer openMSX build if available — not blocking.
5. The interlace/EO hedge (D6, M21) is a documented SIMPLIFICATION of openMSX's own `getEvenOddMask()`, not a bit-for-bit port.
6. M21's A/B evidence could not live-compare computed pixel/color values (no introspection point in openMSX) — an environment limitation.
7. ASX (S#8/S#9, M22) persistence is narrower in scope than `vdp_command_engine.h`'s own header comment states (only SRCH/LMCM persist it, not LINE/LMMM/HMMM) — a documentation-only correction recommended for a future cycle.
8. Future A/B probes for the VDP command engine should explicitly include S#7/S#8/S#9 in their comparison gate or explicitly disclose the exclusion.
9. NEW from M23: the C2 A/B divergence's refined, authoritative causal framing is "emulated time accumulated during the unthrottled pre-measurement boot window, per openMSX's own bulk `advanceHalt` scheduling" — not the implementation report's original, less-precise "wall-clock time between Tcl round-trips" framing.
10. NEW from M23: C1/D4's precisely-named 5-item remainder (full per-mode/per-scanline slot tables + raster-position reconciliation with `run_frame()`'s whole-frame-atomic jump; CPU-priority-steals-command-engine-slot interaction; command engine's real per-pixel/per-line VDP-cycle cost; exact sub-frame IRQ raster position; any actual CPU-execution gating) is genuinely large, exacting, license-sensitive future work (~340 numeric table entries to independently re-derive) — a dedicated future milestone, not a quick add-on.
11. NEW from M23: any future milestone attempting real CPU-execution gating on VDP access timing must first explicitly re-validate the M21/M22 system-test fixtures (`tests/system/hbf1xv_m21_vdp_render_system_test.cpp:56-62`, `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp:58-78`) against real wait/drop behavior — a demonstrated, not hypothetical, regression risk.
12. NEW from M24: `git diff <tag> -- <path>` cannot show content for a wholly untracked (never `git add`ed) file, regardless of what it contains — confirmed via `git cat-file -e <tag>:<path>` returning "exists on disk, but not in `<tag>`". Verifying whether a new, not-yet-committed file contains an expected change requires a direct file read, not a tag-diff, or it can produce a false "not applied" conclusion.
13. NEW from M24: `hbf1xv_m24_zexall_system_test` now hard-asserts `error_markers == 0` for both suites (post-DEC-0022 hardening fix) — the project's single most valuable CPU-regression oracle is now a proper zero-tolerance gate, not observe-and-report. Any future milestone adding a similarly expensive, high-value regression test should default to a hard-gate design from the start, per QA's M24 finding.
14. NEW from M25: hardware PAUSE (`Mb670836PauseController`), the Speed Controller, and Ren-Sha Turbo now have a complete machine/peripheral-level API surface (`press_pause_button()`, `set_speed_level()`, `RenshaTurbo::set_speed()`) ready for a future keyboard/input binding — deliberately no CLI/SDL3 wiring this cycle (out of scope, per the planner's §1.2 decision). Ready for whichever future milestone tackles C9 (SDL3 frontend) to bind.
15. NEW from M25: an optional, planner's-judgment-only `DebugEventType::Pause`/`Resume` + state-dump-section extension was named but not implemented this cycle (not a hard acceptance criterion) — a candidate for a future cycle, likely alongside C9.
16. NEW from M25: PAUSE's idle T-state charge (1 T-state per paused `step_cpu_instruction()` call) is a documented MODELING CHOICE (R-M25-7), not a hardware-quantized fact — real hardware's WAIT-line hold is not naturally discretized. Revisit only if real Sony MB670836 measurement data ever surfaces.
17. NEW from M25: the PAUSE-button toggle-vs-hold semantics (A-M25-1) and the Speed-Controller's `kPeriodFrames=8` duty-cycle range (A-M25-3) are first-principles design defaults, independently judged reasonable by QA, but explicitly revisable if a genuine Sony service-manual/community measurement ever becomes available.
18. NEW from M28/DEC-0026: `V9958Vdp` status register S#2 bits VR/HR are now LIVE (raster-position-derived via a new `VdpClockSource`), not a fixed constant — any future test/tool that assumes a frozen S#2 value across a time-advancing operation must mask those two bits out (0x60) explicitly, exactly as `tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp` now does.
19. SUPERSEDED (DEC-0028, 2026-07-08): the M28/DEC-0026 "instruction #145,503 memory-mapper content divergence" finding was investigated (`docs/memory-mapper-segment-divergence-investigation.md`) and proven a FALSE POSITIVE — an artifact of the live-Tcl comparison methodology (a bare openMSX Tcl `reset` does not clear RAM; only a genuine power-cycle does; the harness's pre-script free-run had contaminated the compared byte). Under the corrected power-cycle methodology, both emulators are byte-identical (write history included) over a 300,000-instruction window. The memory-mapper/RAM subsystem is positively confirmed correct. **Standing methodology rule from this finding: any future memory-CONTENT A/B against live openMSX MUST use `set power off/on`, never a bare `reset`** (CPU-register-only comparisons are unaffected). One genuinely open remainder: the separate "~2.8-2.9M instruction dead-end" observation (RST 38 stack corruption → permanent HALT), whose causal framing was anchored to the now-refuted mismatch — its real cause/AB-status is an open question for a future check using the corrected methodology.
20. NEW from M28: the Conditional-Pass-stop-and-consult rule has now fired TWICE this session (M24, M28), both resolved via the identical human choice ("fix, re-confirm, then tag") — this is now a well-established, low-friction pattern, not a one-off.

## Live-playtesting hardening arc (2026-07-08/09, DEC-0028..DEC-0034, tag v1.0.29) — CLOSED

The project's first sustained human-in-the-loop QA phase, and its most productive defect-finding
method to date. The human played the real SDL3 frontend (BASIC, Metal Gear, MSX-DOS disks) and
every finding was root-caused, fixed universally (never game-keyed), QA'd, and landed same-day:

- **DEC-0028**: the #145,503 memory-mapper "divergence" proven a comparison-methodology false
  positive (bare openMSX Tcl `reset` does not clear RAM — power-cycle rule codified); mapper/RAM
  subsystem positively confirmed byte-identical to openMSX.
- **DEC-0029**: sprite mode 2 attribute-table AND-mask addressing — all MSX2 sprites had been
  invisible; also retroactively explained M22's `sprite_mode2_ninth_ic` divergence.
- **DEC-0030**: fMSX 6.0 registered as the independent second behavior reference (paid off
  same-day: corroborated the COL fix, and its two recorded disagreements — collision granularity,
  color-0 TP conditioning — were arbitrated to the data-book model).
- **DEC-0031**: the MSX2+ boot logo restored (three stacked causes: missing #F4 reset-status
  register; frame-vs-line collision granularity; missing pre-armed COL transfer unit — plus a
  latent G5/G6 sprite-compositing OOB crash); border/backdrop composition added.
- **DEC-0032**: V99x8 color-0 transparency (backdrop show-through) — a pre-existing universal
  renderer gap the new border made visible (the suspected DEC-0031 regression was forensically
  disproven, 215/215 frames byte-identical); border made opt-in (`--border`) per human preference.
- **DEC-0033**: exact audio pacing — the frame loop had run ~3% fast (16 ms truncation) and the
  unbounded audio queue accumulated +29.7 ms lag per second; now authentic 59.9227 Hz with a
  bounded ~40 ms mean audio latency (measured independently twice).
- **DEC-0034**: **backlog C5 ("full boot") VERIFIED CLOSED after 12 milestones** — unattended cold
  boot with a real MSX-DOS disk now pages the disk-ROM in (frame 300) and executes 11 real FDC
  sector reads to a Disk-BASIC-ready prompt (`debug/frames/c5-verify-settled.png`); the "missing
  auto-boot trigger" was never missing — the arc's boot fixes unblocked it, and the old negative
  diagnostics were vsync-less loop-shape artifacts. `disks/games/` (YS II two-disk set) registered.

Process changes ratified during the arc: universal-fixes-only (memory); ZEXALL at RC checkpoints
only, sole cpu/core carve-out (memory, third revision); openMSX-vs-fMSX disagreement-arbitration
discipline (governance files). Named residuals carried forward: MSXDOS.SYS→`A>` handoff question
(future M31 candidate); M28 C5 test re-baseline/rename hygiene; multi-disk swap UI (YS II will
need it); the "~2.8-2.9M boot dead-end" note is OBSOLETE (superseded by the arc's boot fixes —
the machine demonstrably boots to prompt).

## Indicative follow-on order (per `agent-protocol/state/deferred-backlog.md`)

Remaining open backlog (post-DEC-0035 renumber, applied to the ledger in the M30 cycle):
**C1/D4** (blocked pending an independent numeric source), **E1→M31** (YM2413 FM synthesis —
the biggest audible gap; the DEC-0035 release candidate, where the RC-checkpoint ZEXALL sweep
gate applies), **C10→M32-era** (FDC flux/DMK), **F1→M33-era** (cassette), **F2→M34-era**
(printer), **G3/G4/G5** (indefinite/on-demand). G1 closed (M29, v1.0.30); G2 closed (M30,
pending QA/tag v1.0.31).

- Remote/hosting (**DEC-0047**, 2026-07-09): `origin` = **PUBLIC** github.com/patrick-shim/msx-2p-hbf1xv,
  one-way LOCAL → REMOTE (no fetch/pull/merge inbound). Owner's informed decision to publish the
  full repo, `bios/` included (coordinator warned twice re: third-party-asset exposure; owner
  accepted). `roms/`+`disks/` CONTENT untracked as of commit **b5efd29** but their binaries remain
  in pre-b5efd29 history. Config/asset commit b5efd29 landed; the PUSH ITSELF is owner-run — the
  auto-mode safety classifier blocks the assistant from pushing proprietary assets to a public
  remote. Decision propagated across .gitignore / README.md / bios/README.md / roms/README.md /
  CLAUDE.md / guardrails.md / .claude/agents/msx-developer.md + a project memory. Standing push
  sequence: `git remote add origin <url>` → `git push -u origin main` → `git push origin --tags`.
- Updated At: 2026-07-09 (M34 CLOSED per DEC-0045, tag v1.0.35 — the DEC-0043 playtest defect
  pair fixed universally: PSG/SCC band-limited box-average integration killed the Aleste 2
  transition-beep alias; an R#1 BL render gate fixed the Metal Gear room-transition slow-fill.
  QA Conditional Pass fully discharged; both evidence escalations independently confirmed; 4
  Aleste PNGs regenerated from a committed recipe. 183/183 + 192/192. R-M32-7 closed PASS.
  Coordinator idle awaiting the human's M34 live re-check. DEC-0047 public-remote decision
  recorded + propagated; push is owner-run under the auto-mode constraint.)
