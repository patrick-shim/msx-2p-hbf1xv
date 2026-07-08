# Current Phase

- Objective: The M21-M27 continuation is COMPLETE (v1.0.21..v1.0.27). The human's earlier-deferred
  debug/-folder artifact request (raised 2026-07-08 after M25 completed) was fully discharged
  across two milestones per the human's own delegated scope-boundary call: M26 = SDL3 frontend (C9)
  with ONLY a minimal, new decoded-`FrameBuffer`-to-PNG capture capability folded in; everything
  else (audio capture, full CPU/memory/VRAM dump CLI wiring, keystroke-sequencing automation,
  production/stress testing) went to M27 "Production Hardening + Debug/Test Tooling", now also
  closed. Same established cadence continued throughout: planner → developer → QA → release
  decision, proceeding through tag without an extra pause on a clean QA PASS, STOP and consult the
  human otherwise (this fired once, for M24). Standing "zero license-sensitive future work" and
  "don't run the slow test unless necessary" directives remain in force (project memories
  `feedback_license_sensitive_scope.md` / `feedback_slow_test_cadence.md`). No further milestone has
  been requested by the human yet.
- Active Phase: M27 ("Production Hardening + Debug/Test Tooling") — CLOSED (2026-07-08,
  DEC-0025/REQ-M27-004, git tag v1.0.27). Planner package accepted (`docs/m27-planner-package.md`,
  RESP-M27-001), developer implementation complete (`docs/m27-implementation-report.md`,
  RESP-M27-002), QA returned a clean, unconditional Pass (`docs/m27-qa-signoff.md`, RESP-M27-003).
  M26 (SDL3 frontend, v1.0.26) and M27 both CLOSED. No further milestone kicked off yet — awaiting
  the next human directive.
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

## M21-M27 run summary

- **M21 (VDP rendering depth, v1.0.21)**: First pixel-rendering output path. `VdpFrameRenderer`/`FrameBuffer` — deterministic, pull-model, frozen-register-snapshot renderer, RGB555 pixels, zero new clock consumer. Every Target-Spec mode byte-exact (GRAPHIC7 GGGRRRBB byte layout; MULTICOLOR's real 256-wide canvas). YJK/YJK+YAE decode byte-exact, incl. an independently-verified floor-vs-truncation finding. G6/G7 planar interleave's CPU-port + display-path pieces closed (command-engine piece carried to M22). `ctest` 106/106. Closes D1/D5/D6; D7 IN-PROGRESS (M21 partial). QA PASS (`docs/m21-qa-signoff.md`).
- **M22 (sprites + VDP command engine, v1.0.22)**: `SpriteEngine` (D2) and `VdpCommandEngine` (D3), both owned inside `V9958Vdp`. Sprite Mode 1/2 byte-exact per `SpriteChecker.cc/.hh` (EC/CC/IC bits; color-0/TP transparency — a fact-sheet-vs-source discrepancy resolved in favor of source). Full R#32-46 register file, all 13 commands via a hybrid execution model (10 atomic; LMCM/LMMC/HMMC event-driven, mirroring the M16 FDC DRQ/INTRQ precedent). D7 closes IN FULL via 5 new coordinate-address functions (confirmed to bypass R#2 entirely — a genuinely new finding). `ctest` 117/117. Closes D2/D3; D7 DONE in full. QA PASS (`docs/m22-qa-signoff.md`) via genuinely independent, adversarial scrutiny — QA hand-verified the raw A/B divergence numbers itself and corrected the developer's causal narrative.
- **M23 (exact cycle timing, v1.0.23)**: Deliberately conservative scope, given this touches the highest-risk surface in the project (CPU-visible timing, adjacent to the zero-tolerance M9/M12 CPU-timing oracles). Closes C2 (Z80 HALT-R) in full: `Z80aCpu::step()`'s halted branch now calls `increment_refresh_register()`, and the already-existing machine-level formula automatically bills the correct 5T. Exactly one golden test deliberately updated (t3 4→5, elapsed_cycles 22→23). C1/D4 advance to IN-PROGRESS (M23 partial) with a real, tested, non-gating VDP access-timing foundation — the full slot tables and any actual CPU-execution gating explicitly declined this cycle, since the M21/M22 system tests already contain back-to-back `OUT (#98),A` writes that real gating could silently corrupt. `ctest` 121/121; all 10 named oracle suites plus all 6 M21/M22 device files confirmed genuinely untouched. QA PASS (`docs/m23-qa-signoff.md`) via exceptional scrutiny — QA independently re-ran the C2 A/B harness and designed its own exploratory probes to refine the developer's divergence hypothesis into a more precise, still-honest explanation.
- **M24 (ZEXDOC/ZEXALL full parity sweep, v1.0.24)**: CPU-validation milestone — a genuine, generic `CpmBdosHarness` (zero zexall-specific knowledge) ran the real, GPL-licensed ZEXALL/ZEXDOC Z80 exerciser suite against the already-QA-verified Z80A CPU core to genuine CP/M warm-boot completion. **134/134 group verdicts PASS**, independently reproduced FOUR separate times from clean rebuilds (developer, coordinator, QA, post-fix confirmation), every time byte-for-byte identical (5,764,169,474 instructions/suite, 67/0 marker split). Adversarial self-check PASSED; openMSX A/B bounded-prefix PARITY independently re-run twice (developer, QA). `ctest` 124/124; zero changes to `src/devices/`/`src/peripherals/`/`src/core/`. QA's first-ever Conditional Pass this run (a real, if narrow, regression-harness gap — see Phase Status above) was fixed and reconfirmed per the human's explicit choice before tagging. Closes C3 in full.
- **M25 (Sony speed-controller + hardware PAUSE + Ren-Sha Turbo, v1.0.25)**: First milestone implementing genuinely new, never-previously-scoped HB-F1XV-specific hardware. New `Mb670836PauseController` — a machine-level CPU-execution gate at the very top of `step_cpu_instruction()`, freezing PC/R/every register while engaged (distinct from the Z80's own `HALT`). New `RenshaTurbo` autofire, wired into `KeyboardMatrix`/`JoystickPorts` via additive OR-mask attach points. Zero changes to any CPU/device-core file; all 12 named zero-tolerance CPU-timing-oracle files confirmed byte-for-byte unchanged, independently reproduced three times (developer, coordinator, QA). `ctest` 130/130, including three separate from-scratch re-runs of the M24 ZEXALL/ZEXDOC sweep. Real, live openMSX A/B PARITY for Ren-Sha Turbo against the actual `Sony_HB-F1XV` machine (reproduced twice, developer and QA); Hardware PAUSE/Speed Controller honestly BLOCKED (openMSX models none of this Sony-specific hardware anywhere, independently confirmed three times). QA returned a **clean, unconditional Pass** — the Conditional-Pass stop condition did NOT fire this time. QA's sole finding (a "five"→"six" Sony-machine-XML citation undercount, Low/non-blocking) was fixed directly by the coordinator per QA's own recommendation. Closes C8 in full. **This closes the full M24-M25 continuation.**
- **M26 (SDL3 Frontend, v1.0.26)**: The largest, most architecturally novel milestone to date — first frontend/presentation-layer code, first real-time loop, first real audio wiring. New `Hbf1xvMachine::on_vsync_boundary()` (a pure, mechanical `run_frame()`-body extraction, the ONLY behavior-affecting `src/machine/` change) lets a real-time SDL3 loop drive the CPU via `step_cpu_instruction()` without the `run_frame()` double-count hazard. Zero changes to any CPU/device/peripheral/core file, independently confirmed three times (developer, coordinator, QA), including all 12 CPU-timing-oracle files and `ym2413_opll.*` specifically. New `src/frontend/` (`Sdl3App`, `Sdl3VideoPresenter`, `Sdl3AudioPresenter`+`PsgAudioPump`, `Sdl3InputMapper`, `sdl3_cli`): zero-conversion `SDL_PIXELFORMAT_XRGB1555` video blit (pixel-readback-proven); real PSG audio wired for the first time in this project's history; a 71-entry keyboard mapping table plus joystick/PAUSE/Speed-Controller/Ren-Sha-Turbo bindings, exhaustively `SDL_PushEvent`-tested. YM2413/FM-PAC honestly, deliberately left silent — backlog E1 stays OPEN. The one authorized new debug capability — decoded-`FrameBuffer`→PNG capture — ships with a real, committed, independently-byte-reproduced example. A real environment finding (SDL3 not pre-installed, resolved by building the vendored source) was independently reproduced end-to-end THREE times (developer, coordinator, QA's own brand-new install). `ctest` 134/134 headless + 139-140/140 SDL3-ON, both under the real "dummy" video/audio drivers. QA returned a **clean, unconditional Pass with ZERO findings of any severity** — the cleanest QA cycle of this session. Closes C9 in full. Names a future **M27 "Production Hardening + Debug/Test Tooling"** milestone for everything else from the human's original debug/-tooling request.
- **M27 (Production Hardening + Debug/Test Tooling, v1.0.27)**: Infrastructure/tooling milestone — closes zero backlog rows outright. Four named items: (1) new headless `--debug-session` mode (`src/main.cpp`, wholly additive) + additive SDL3 fields wiring the pre-existing `write_state_dump()`/`write_cpu_trace()`/`write_event_log()`, a new `--disk` flag, and `--input-script` — zero new `Hbf1xvMachine` method needed; (2) real, decoded PSG audio capture to file (`src/frontend/psg_audio_dump.*`, genuinely reusing M26's `PsgAudioPump`) + `tools/audio-dump-to-wav.py`, shipping a real, committed, non-silent example WAV independently decoded three times; (3) a general-purpose, deterministic keystroke-sequencing/scripted-input mechanism (`src/peripherals/msx_key_names.*` + `src/machine/input_script.*`, 71-entry table cross-consistency-tested against `Sdl3InputMapper`); (4) event-log CLI wiring + an adversarially-validated, byte-for-byte replay-determinism system test (two independent machines identical logs; a third, deliberately-diverged machine — via a genuine, self-caught flat-RAM-driver design correction — produces a different one). A self-caught finding: `PsgYm2149::write_register()` is actually private; the demo correctly uses the public `write_address()`/`write_data()` ports. Zero changes to any pre-existing file under `src/devices/`, `src/peripherals/`, or `src/core/`, confirmed via `git diff v1.0.26` at THREE separate gates (developer, coordinator, QA). Per the human's standing `feedback_slow_test_cadence.md` directive, the slow `hbf1xv_m24_zexall_system_test` was not run at any gate this cycle — confirmed genuinely unnecessary by all three parties independently. Headless fast-subset 140/140; SDL3-ON fast-subset 149/149; every pre-existing M26 SDL3 test remains green. QA returned a **clean, unconditional Pass** — zero blocker/high/medium findings, two Low non-blocking notes. Backlog C5 gains a factual cross-reference note only (status unchanged); E1 remains OPEN, untouched.

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

## Indicative follow-on order (per `agent-protocol/state/deferred-backlog.md`)

**M26 (SDL3 frontend) and M27 (Production Hardening + Debug/Test Tooling) are both now CLOSED.** No
further milestone kicked off yet — awaiting the next human directive. Candidates: a future
VDP-timing-depth milestone (C1/D4 remainder) · a future dedicated C5 real-disk-boot-trigger
investigation (now unblocked on assets via `disks/`, DEC-0021, and equipped with M27's new
`--disk`/scripted-input tooling) · a future audio-rendering milestone for E1 (YM2413 DSP/synthesis
depth, still OPEN, "paired with C9" per M17's own note — both C9 (M26) and the audio-capture
tooling (M27) are now done, so this pairing is unblocked whenever prioritized).

- Updated At: 2026-07-08T20:10:00+09:00 (M27 CLOSED, tagged v1.0.27, DEC-0025 — QA clean PASS;
  awaiting next human directive)
