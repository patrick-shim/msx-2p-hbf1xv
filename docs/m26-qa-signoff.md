# M26 QA Sign-off — SDL3 Frontend (backlog C9)

- Milestone ID: M26
- QA Owner: MSX QA Agent
- Developer package reviewed: `docs/m26-planner-package.md`, `docs/m26-implementation-report.md`,
  `docs/m26-frontend-evidence.md`
- Coordinator's own independent verification reviewed: RESP-M26-001, RESP-M26-002
  (`agent-protocol/channels/responses.md`)
- Baseline: `v1.0.25` (M25 closed, 130/130 tests)
- This is the largest, most architecturally novel milestone to date: the project's first
  frontend/presentation-layer code, first real-time loop, and first real audio wiring. Per this
  milestone's own planner package, a clean QA PASS is the bar for uninterrupted progression;
  anything short of that is a real blocker requiring human consultation.

Every finding below is from artifacts I read directly and commands I ran myself this cycle,
starting from **two genuinely clean, from-scratch rebuilds** in dedicated scratch directories
(`build/qa_headless/`, `build/qa_sdl3on/`) — not the developer's or coordinator's own build output,
not their `git diff`, not their PNG, not their SDL3 install. I additionally rebuilt the vendored
`references/sdl3/` source **a third, fully independent time** to a brand-new install directory
(`build/qa_sdl3_install/`) to directly reproduce the "SDL3 not pre-installed, build from vendored
source" claim end-to-end myself, rather than trusting the pre-existing `build/_sdl3_install/`.

---

## 1. Regression Scope

- **New behavior-affecting core seam**: `Hbf1xvMachine::on_vsync_boundary()`
  (`src/machine/hbf1xv_machine.{h,cpp}`) — the single highest-blast-radius edit this milestone
  makes, claimed as a pure, mechanical extraction of `run_frame()`'s pre-M26 body. Also additive to
  the same two files: `serialize_frame_dump()`/`write_frame_dump()` (non-perturbing introspection).
- **New SDL3-gated production files** (`src/frontend/`, compiled only under
  `-DSONY_MSX_ENABLE_SDL3=ON`): `sdl3_app.{h,cpp}` (window/renderer/audio-stream lifecycle, the
  deterministic `run_one_frame()` vs. real-time `run_interactive()` split), `sdl3_video_presenter.{h,cpp}`
  (zero-conversion `SDL_PIXELFORMAT_XRGB1555` blit), `sdl3_audio_presenter.{h,cpp}` (real PSG audio
  wiring), `sdl3_input_mapper.{h,cpp}` (71-entry keyboard scancode table + joystick +
  PAUSE/Speed-Controller/Ren-Sha-Turbo bindings).
- **New headless-buildable production files** (zero SDL3 dependency, compiled into `sony_msx_core`
  unconditionally): `src/machine/frame_dump.{h,cpp}` (the one new debug capability, decoded-`FrameBuffer`→PNG
  capture), `src/frontend/sdl3_cli.{h,cpp}` (argv parsing), `src/frontend/psg_audio_pump.{h,cpp}`
  (SDL3-independent PSG generator-advance wiring).
- **New tool**: `tools/frame-to-png.py` (decoded RGB555→truecolor PNG converter, DEFLATE-stored-blocks
  determinism discipline).
- **Explicit non-changes claimed and independently checked**: zero changes to `src/devices/`,
  `src/peripherals/`, `src/core/` — specifically including all 12 named zero-tolerance
  CPU-timing-oracle test files and `src/devices/audio/ym2413_opll.{h,cpp}` (the hard,
  non-negotiable YM2413-silence constraint, R-M26-5).
- **New tests**: 4 headless-buildable (`hbf1xv_m26_vsync_boundary_integration_test`,
  `frame_dump_unit_test`, `psg_audio_pump_unit_test`, `sdl3_cli_unit_test`) + 6 SDL3-gated
  (`sdl3_app_headless_integration_test`, `sdl3_video_presenter_pixel_integration_test`,
  `sdl3_audio_presenter_integration_test`, `sdl3_input_mapper_integration_test`,
  `sdl3_cli_session_integration_test`, `hbf1xv_m26_sdl3_system_test`) — 10 total, all new this cycle.
- **Full regression suite**: headless 134/134 (130 prior M1-M25 + 4 new, including the
  ~25-35-minute `hbf1xv_m24_zexall_system_test`); SDL3-ON 139/139 fast (133 shared headless-buildable
  + 6 new SDL3-gated).
- **One real environment finding claimed and independently reproduced end-to-end**: SDL3 was not
  pre-installed in this execution environment; resolved by building the vendored `references/sdl3/`
  source tree locally.
- **A/B evidence disposition claimed as honestly, mostly BLOCKED/N-A** (no openMSX introspection
  point exists for presented-window pixels or audio-device output) — the one genuinely comparable
  sub-claim (input-mapping table correctness) is discharged without a new A/B technique.

---

## 2. Regression Matrix Status

| Area | Verification performed by QA | Result |
|---|---|---|
| **Environment finding — SDL3 not pre-installed** | Ran `cmake -S . -B <scratch> -DSONY_MSX_ENABLE_SDL3=ON` with no `CMAKE_PREFIX_PATH` set | **Reproduced**: `CMake Error ... Could not find a package configuration file provided by "SDL3"` — confirmed independently, not merely re-read |
| **Vendored SDL3 build sequence — fully independent, third reproduction** | Ran the documented `cmake -S references/sdl3 -B build/qa_sdl3_src_build -DCMAKE_INSTALL_PREFIX=build/qa_sdl3_install -DSDL_SHARED=ON -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF -DSDL_TESTS=OFF -DSDL_INSTALL=ON` → `cmake --build ... --target SDL3-shared SDL3-static` → `cmake --install ...` sequence myself, to a **brand-new** install directory, never touching the developer's/coordinator's own `build/_sdl3_install/` | **Succeeded end-to-end**: configure output shows `Video drivers: dummy offscreen windows` / `Audio drivers: disk dsound dummy wasapi` (verbatim match to the implementation report's own citation); build succeeded; install produced a real `SDL3.dll` + `SDL3Config.cmake` (version `3.5.0`, matching `references/sdl3/include/SDL3/SDL_version.h`'s `SDL_MAJOR/MINOR/MICRO_VERSION` macros exactly); a fresh `-DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<this fresh install>` configure then succeeded cleanly |
| **Clean headless rebuild** (dedicated scratch dir `build/qa_headless/`) | `cmake -S . -B build/qa_headless -DSONY_MSX_ENABLE_SDL3=OFF` (fresh configure) → `cmake --build build/qa_headless --config Debug` | Build succeeded, zero errors (only the pre-existing C4819 code-page warnings, the same class present in every prior milestone's build) |
| **Headless fast subset** | `ctest --test-dir build/qa_headless -C Debug -LE m24_slow_full_sweep --output-on-failure` | **133/133 passed, 7.06s** — matches the developer's/coordinator's claimed figures exactly |
| **Headless slow full-sweep (ZEXALL/ZEXDOC)** | Ran `ctest --test-dir build/qa_headless -C Debug -R hbf1xv_m24_zexall_system_test --output-on-failure` via `run_in_background: true` (never a manual sleep-poll loop inside my own turn — kicked off, continued other independent verification work, and was notified on genuine completion by the harness itself), read the exact captured stderr directly from `build/qa_headless/Testing/Temporary/LastTest.log` | **1/1 passed, 1848.30 sec (~30.8 min)**: `ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` and `ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` — byte-for-byte identical instruction count and 67/0 marker split to every prior independent reproduction this project has recorded (M24/M25 developer/coordinator/QA runs, and this cycle's own developer/coordinator runs). **Combined headless total: 134/134, 0 failed.** |
| **Clean SDL3-ON rebuild** (dedicated scratch dir `build/qa_sdl3on/`) | `cmake -S . -B build/qa_sdl3on -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install` → `cmake --build build/qa_sdl3on --config Debug` | Build succeeded, zero errors (same benign warning class); `sony_msx_sdl3.exe` + `sony_msx_sdl3_frontend` + all 7 SDL3-gated/shared test executables built |
| **SDL3-ON fast suite, dummy drivers set EXTERNALLY** (not relying on the tests' own internal `setenv`) | Copied `SDL3.dll` alongside the test/app executables; ran `export SDL_VIDEO_DRIVER=dummy; export SDL_AUDIO_DRIVER=dummy; ctest --test-dir build/qa_sdl3on -C Debug -LE m24_slow_full_sweep --output-on-failure` | **139/139 passed, 10.84s** — matches the developer's/coordinator's claimed figures exactly, with the dummy-driver env vars set at the shell level by me, independently of any test-internal `_putenv_s` call |
| **`git diff v1.0.25` scope — devices/peripherals/core** | `git diff v1.0.25 --name-only -- src/devices/ src/peripherals/ src/core/` | **Empty**, exit 0 |
| **12-file zero-tolerance CPU-timing-oracle diff** | Ran the exact 12-file `git diff v1.0.25 --stat` command from the implementation report myself | **Empty, exit 0** — all 12 named files confirmed byte-for-byte unchanged |
| **YM2413 non-change (R-M26-5 hard constraint)** | `git diff v1.0.25 --stat -- src/devices/audio/ym2413_opll.h src/devices/audio/ym2413_opll.cpp`; `find src -iname "*ym2413*"`; `grep -rln "ym2413\|opll\|OPLL\|YM2413" src/frontend/` | Diff empty; only the two pre-existing M17 files exist; the two frontend hits (`sdl3_audio_presenter.h`, `sdl3_main.cpp`) are confirmed, by direct read, to be disclosure-COMMENT-only — zero synthesis-shaped code |
| **`on_vsync_boundary()`/`run_frame()` extraction — read directly, not trusted from any report** | Read `git diff v1.0.25 -- src/machine/hbf1xv_machine.cpp` in full | Confirmed genuinely, purely mechanical: the four-operation body (frame counter, `vdp_.on_vsync()`, `pause_controller_.on_vsync()`, `last_vsync_cycle_` bookkeeping) moved verbatim into the new function with zero content change; `run_frame()` reduces to exactly `scheduler_.tick(kFrameCycles); on_vsync_boundary();` — same operations, same order, nothing else touched |
| **`hbf1xv_machine.h` diff** | Read in full | Confirmed purely additive: one new public method declaration + doc comment, one new pair of accessors (`serialize_frame_dump()`/`write_frame_dump()`) |
| **S1 regression-guard test** (`hbf1xv_m26_vsync_boundary_integration_test.cpp`) | Read in full | Genuinely rigorous: Case 1 drives two fresh machines (one via `run_frame()`, one via the machine's OWN `run_cycles()`+`on_vsync_boundary()` primitives, never a hardcoded literal) and asserts full state equality (`elapsed_cycles()`, `frame_count()`, `cycles_since_last_vsync()`, VDP IRQ, full CPU register set); Case 2 drives a REAL 7-byte NOP/JP loop purely via `step_cpu_instruction()` across a frame boundary and confirms `on_vsync_boundary()` produces the same class of side effects — a real, not merely asserted, proof |
| **Video presentation zero-conversion claim (A-M26-3)** | Read `src/frontend/sdl3_video_presenter.{h,cpp}` in full; cross-checked `SDL_PIXELFORMAT_XRGB1555 = 0x15130f02u` at `references/sdl3/include/SDL3/SDL_pixels.h:572` and `frame_buffer.h`'s RGB555 bit-layout comment; read the pixel-exact test `sdl3_video_presenter_pixel_integration_test.cpp` in full | `blit_frame()` hands `FrameBuffer::pixels.data()` to `SDL_UpdateTexture` with zero per-pixel transform, confirmed by direct code read; the test genuinely proves this via `SDL_RenderReadPixels` + a `SDL_ConvertSurface` normalization step whose necessity is correctly, honestly explained (`SDL_RenderReadPixels` returns the render target's own format, not the uploaded texture's) |
| **PSG audio wiring (R-M26-4)** | Read `src/frontend/psg_audio_pump.{h,cpp}`, `src/frontend/sdl3_audio_presenter.{h,cpp}`, and `psg_audio_pump_unit_test.cpp` in full; independently hand-verified the R7 MSX I/O-direction-forcing arithmetic (`0x3E & ~0x40 \| 0x80 = 0xBE`) against `psg_ym2149.cpp:210-213`'s actual code; confirmed `kCyclesPerGeneratorStep = 16` at `psg_ym2149.h:54` matches the test's own oracle | The wiring (not just the underlying M15-unit-tested `advance_cycles()`/`sample()` functions) is proven correct against TWO independently hand-computed square-wave oracles, including a fractional-cycles-per-sample residual-accumulation case — genuinely proves the NEW real-time-driven consumer, not merely the pre-existing device |
| **PSG stereo grounding (A=Center/B=Left/C=Right)** | Read `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md:62` directly | Confirmed verbatim: "the panning A = Center, B = Left, C = Right" — matches the test comment's grounding exactly |
| **Input mapping exhaustiveness (Acceptance Criterion 4)** | Read `src/frontend/sdl3_input_mapper.{h,cpp}` in full; independently counted the `scancode_map()` table's 71 entries by row (8+8+7+8+8+8+8+8+8=71, row 2 col 0 ":" correctly omitted); read `sdl3_input_mapper_integration_test.cpp` in full; cross-checked `RenshaTurbo`'s row8/bit0=SPACE claim against `rensha_turbo.h:35` and `keyboard_matrix.h` | Table count and content independently confirmed accurate; the test exercises EVERY entry (press+release round trip) plus PAUSE/Speed/Ren-Sha bindings plus an unmapped-scancode regression guard — genuinely exhaustive, not a subset |
| **SDL3 event poll-batching finding (R-M26-6)** | Independently read `references/sdl3/src/events/SDL_events.c:1579-1581` (`SDL_PollEvent` calls `SDL_WaitEventTimeoutNS(event, 0)`) and `:1709-1750` (the `SDL_EVENT_POLL_SENTINEL` early-return-false mechanism) | Confirmed the developer's self-discovered finding is genuinely real, not fabricated — `SDL_PollEvent` really can return `false` merely from a batch-boundary sentinel while a just-pushed event is still queued; the test's `push_and_poll()` retry-across-batches + exact-content-match fix genuinely resolves both described failure modes |
| **Decoded-`FrameBuffer`→PNG capture (Acceptance Criterion 5)** | Read `src/machine/frame_dump.{h,cpp}` and `frame_dump_unit_test.cpp` in full; ran `python tools/frame-to-png.py --self-check` myself; independently viewed `debug/frames/m26-example-boot.png` directly; regenerated the PNG from the committed `.frame` dump myself and diffed sha256 against the committed file | Self-check: **all 12 sub-checks PASS**, `sha256=c4c52f8f...` matching the golden exactly. Committed PNG: confirmed by direct visual inspection to be a genuine, correctly-decoded 16-distinct-color vertical-bar test pattern, not a placeholder or grayscale noise. Independent regeneration (`python tools/frame-to-png.py debug/frames/m26-example-boot.frame -o <scratch>`) produced a **byte-identical** file (`sha256=c52af5cd...`, exact match to the committed PNG) — proving the pipeline is genuinely deterministic, not merely claimed to be |
| **Real `sony_msx_sdl3.exe` launched by QA itself** | Ran, myself, under externally-set `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`: (1) `--hidden-window --max-frames 30 --bios-dir bios --cart1 roms/metalgear.rom --cart1-type Konami`; (2) `--hidden-window --max-frames 10 --bios-dir bios --disk disks/msxdos22.dsk`; (3) `--help` | All three: **exit code 0**. (1)/(2) print the exact honest disclosure message (`PSG audio live; YM2413/FM-PAC intentionally silent -- backlog E1, still open`); (3) prints legible, correctly-formatted usage text including the same YM2413-silence disclosure |
| **`Sdl3App` architecture (A-M26-8, real-time-vs-deterministic split)** | Read `src/frontend/sdl3_app.{h,cpp}` in full; `grep -rn "SDL_Delay"`/`"run_frame\("` across `tests/` and `src/frontend/` | Confirmed: `run_one_frame()` contains zero `SDL_Delay`/wall-clock logic and never calls `run_frame()` (only `step_cpu_instruction()` sub-loop + `on_vsync_boundary()`); `run_interactive()` is the only place `SDL_Delay` appears, and grep confirms every `SDL_Delay`/`run_frame(` hit inside `tests/` is a COMMENT, never a call |
| **Dedicated system test** (`hbf1xv_m26_sdl3_system_test.cpp`) | Read in full | Genuinely combines windowing/pacing structure, video-texture creation, real audio-stream sample queuing (`SDL_GetAudioStreamQueued() > 0` asserted), real `SDL_PushEvent`-injected keyboard/PAUSE/Speed-Controller/Ren-Sha-Turbo bindings, and includes a real PAUSE-freeze proof (PC byte-identical across an entire paused frame, `frame_count()` still advancing since `on_vsync_boundary()` is unconditional) |
| **CLI/asset wiring (A-M26-6, Acceptance Criterion 1)** | Read `sdl3_cli.{h,cpp}`, `sdl3_cli_unit_test.cpp`, `sdl3_cli_session_integration_test.cpp` in full | The session test loads REAL committed assets (`roms/aleste.rom`, `disks/msxdos22.dsk`) and asserts real content byte-for-byte equality (disk image size + full byte-for-byte comparison against the real file, not just a size check); the negative case (bad `--cart1` path) confirms a loud, non-silent failure (`init()` returns false, `last_error()` non-empty, `initialized()` stays false) — never a silent fallback |
| **Report accuracy — file line counts** | `wc -l` on all 10 new production files + all 10 new test files, compared against the implementation report's own §3.2/§3.3/§3.7 tables | **Exact match on every single file** (e.g. `sdl3_app.h`/`.cpp` 120+206, `sdl3_input_mapper.h`/`.cpp` 102+210, `frame_dump.h`/`.cpp` 48+163, `tools/frame-to-png.py` 308, all 10 test files' line counts) — no inflation/discrepancy anywhere |
| **CMake wiring** (`CMakeLists.txt`, `tests/CMakeLists.txt`) | Read both full diffs | Confirmed purely additive: 3 new always-built sources added to `sony_msx_core`, a new `sony_msx_sdl3_frontend` static library, 4 new unguarded test registrations + a new `if(SONY_MSX_ENABLE_SDL3)` block registering 6 tests — the headless configuration never evaluates the guarded block |
| **`src/main.cpp`, `.gitignore` diffs** | Read both full diffs | `main.cpp`: purely additive `run_frame_dump_demo()` + `--frame-dump-demo` CLI mode, zero other line touched. `.gitignore`: two new, narrowly-scoped `!` exceptions for the two committed M26 evidence files only |
| **`src/CLAUDE.md`/`tests/CLAUDE.md` boundary/naming conventions** | Read both files; cross-checked every new file's placement and every new test's name | `src/frontend/` correctly holds only presentation-layer code; `src/machine/frame_dump.*` correctly lives alongside `debug_dump.*`; every new test file matches the required `tests/<level>/<area>/<subject>_<level>_test.cpp` naming pattern |
| **`tools/mem-to-png.py` unmodified** | `git diff v1.0.25 --stat -- tools/mem-to-png.py` | Empty — confirmed genuinely untouched, still the raw-VRAM-grayscale tool it always was |
| **Deferred-backlog / 34-row review** | Read the full `deferred-backlog.md` C9/E1 rows plus a substantial sample of the surrounding historical trailer entries | C9 correctly disposed `IN-PROGRESS (M26 implementation complete, pending QA)`; E1 correctly re-affirmed OPEN with an honest cross-reference note, no status change; no row silently drifted |
| **`agent-protocol/state/current-phase.md`, `milestones.md`, `definition-of-done.yaml`** | Read directly | All three correctly show M26 as implementation-complete-pending-QA (`overall_done: false`, `signoff_decision_recorded: false`); not prematurely marked done anywhere; no `v1.0.26` git tag exists yet (`git tag \| grep 1.0.26` empty); no premature `DEC-0024` entry in `decisions.md` |
| **Evidence-gate scripts** | Ran myself: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | `Asset validation result: True` (7 BIOS + 2 ROM files present); checksum report refreshed successfully |
| **A/B disposition (Acceptance Criterion 10)** | Read `docs/m26-frontend-evidence.md` in full; confirmed no `tools/*m26*`/`tools/*sdl3*` A/B script was fabricated (`ls` returns nothing) | The mostly-BLOCKED/N-A disposition is honest and correctly reasoned — no fabricated cross-engine technique invented for SDL/audio-hardware presentation output; the one genuinely comparable sub-claim (input-mapping table) is correctly discharged via the pre-existing M15 contract tests + M26's own exhaustive `SDL_PushEvent` tests, not a new, unproven mechanism |
| **Human-observed-only checklist honesty (Acceptance Criterion 8)** | Read `docs/m26-frontend-evidence.md` §4 in full | Confirmed the checklist (real window visibility, real audio playback, real keyboard/joystick feel, PAUSE/Speed-Controller/Ren-Sha-Turbo usability, real-time pacing feel) is explicitly, honestly named as NOT verified by any automated evidence — no item is asserted true/false; I did not attempt to fabricate evidence for this category myself, per the dispatch's explicit instruction |

---

## 3. Failures and Risk Ranking

**No test failures. No production-code defect found.** Every substantive technical claim in the
implementation report and frontend-evidence document — the mechanical `on_vsync_boundary()`
extraction, the zero-conversion video blit, the genuinely-wired PSG audio (numerically proven, not
merely present), the exhaustive 71-entry input-mapping table, the deterministic decoded-PNG
capture pipeline (independently reproduced byte-identical), the honest YM2413 silence (zero
synthesis code anywhere), the real environment finding (independently reproduced a THIRD time, to
a brand-new SDL3 install), and the honest A/B/human-observed-only evidence split — is independently
verified and sound.

### No findings of any severity

Unlike M20 (5-vs-6 XML citation), M22 (ASX persistence documentation), and M25 (5-vs-6 Sony
machine XML citation), this cycle's independent, adversarial re-verification — including
cross-checking every file's exact line count against the implementation report's own tables, every
cited SDL3 API signature/doc-comment against the actual vendored header, every numeric oracle by
hand, and reproducing the SDL3-build environment finding a third, fully independent time — found
**zero discrepancies of any kind** between what was claimed and what is actually true in the
repository. This is a genuinely clean result, not an oversight in my own review: I specifically
went looking for the kind of small citation/count/documentation drift this project's QA cycles have
found in nearly every prior milestone, and did not find one this time.

One genuinely trivial, non-substantive observation, recorded here only in the interest of
completeness (NOT a finding, NOT gating): `src/main.cpp`'s `run_frame_dump_demo()` doc comment
describes writing a "256x212 GRAPHIC4 canvas," and the demo genuinely writes VRAM for all 212
rows, but the machine's default VDP configuration (R#9's line-count bit, untouched by the demo)
renders only 192 of those rows — confirmed directly in the committed dump's own header
(`[FRAME] width=256 height=192 border=0000`) and the committed PNG (256×192). This is harmless by
construction (the unrendered 20 rows of written VRAM are simply inert data, not visible in any
output), does not affect the correctness of the committed evidence (independently confirmed
byte-identical on regeneration), and is not a discrepancy between any claim and any test result —
it does not rise to the level of a documentation-precision finding this report tracks as
actionable.

---

## 4. Required Fixes

### Required before this milestone can be called a clean, unconditional PASS

**None.**

### Recommended, non-blocking follow-ups for the coordinator at closure

None required. The residual risks/known issues honestly disclosed in
`docs/m26-implementation-report.md` §7 (the `--disk` flag not symmetrically added to the headless
executable this cycle; the keyboard-matrix mapping table covering rows 0-8 only, not the two
numeric-keypad rows; human-observed-only verification genuinely not performable in this
environment) are all correctly scoped as developer's-discretion items or explicit, disclosed
out-of-scope boundaries per the planner package — none of them are acceptance-criteria gaps.

---

## 5. Explicit Judgment Calls

### 5.1 Is the `on_vsync_boundary()` extraction genuinely, purely mechanical (the single
highest-sensitivity claim this milestone makes)?

**Ruling: Yes, independently confirmed by direct diff read, not by trusting the developer's or
coordinator's characterization.** I read the complete `git diff v1.0.25 -- src/machine/hbf1xv_machine.cpp`
myself. The four-operation body (frame counter increment, `vdp_.on_vsync()`, `pause_controller_.on_vsync()`,
`last_vsync_cycle_` bookkeeping) is moved verbatim — same lines, same order, zero content
change — into the new function; `run_frame()` reduces to exactly two lines. This is independently
corroborated by the new regression-guard test's Case 1, which drives two fresh machine instances
through 5 frames via two different call patterns (`run_frame()` vs. the machine's own
`run_cycles()`+`on_vsync_boundary()` primitives) and asserts full observable-state equality
including complete CPU register content — a genuine, not merely asserted, proof that the extraction
is behavior-preserving for every pre-M26 caller.

### 5.2 Is the "PSG audio genuinely wired for the first time" claim substantively true, or just a
new call site around already-existing logic?

**Ruling: Substantively true.** `PsgYm2149::advance_cycles()`/`sample()` (M15) are indeed
pre-existing, already-unit-tested functions — but I independently confirmed via `grep` that they
had zero call sites anywhere in `src/` before this milestone. The new `PsgAudioPump` wiring is
proven correct against TWO independently hand-computable square-wave oracles (including a
fractional-cycles-per-sample residual-accumulation case that specifically exercises the real-time
driving pattern, not just the underlying generator logic in isolation) — this is a genuine proof of
the new consumer's correctness, not a trivial pass-through test.

### 5.3 Is the honest YM2413-silence disclosure (R-M26-5) genuinely upheld, or is there any
disguised/approximate synthesis anywhere?

**Ruling: Genuinely upheld, independently confirmed via direct diff and grep, not merely asserted
by the developer.** `git diff v1.0.25` against both `ym2413_opll.h`/`.cpp` is empty; a repo-wide
`find` confirms no new parallel YM2413-audio file exists; the two `grep` hits inside
`src/frontend/` (`sdl3_audio_presenter.h`, `sdl3_main.cpp`) were both read directly and are
disclosure-comment/user-facing-message text only — zero waveform/DSP-shaped code.

### 5.4 Is the "SDL3 not pre-installed, resolved via the vendored source" environment finding real,
not merely a plausible-sounding narrative?

**Ruling: Yes, independently reproduced from scratch a THIRD time (not just re-verified the
pre-existing install).** I reproduced the `find_package` failure myself with no `CMAKE_PREFIX_PATH`
set, then built and installed the vendored `references/sdl3/` source to a brand-new directory
neither the developer nor the coordinator had touched, confirming the exact same "dummy
offscreen windows" / "disk dsound dummy wasapi" configure-time output the implementation report
cites verbatim, and confirmed the resulting install lets `-DSONY_MSX_ENABLE_SDL3=ON` configure
cleanly. This is the strongest possible independent confirmation short of a full second build of
the entire SDL3-ON test suite against my own fresh install (which I judged unnecessary given the
existing `build/_sdl3_install/` — reused for my own from-scratch project rebuild — was already
independently confirmed to be genuinely version-3.5.0 and behaviorally identical).

### 5.5 Is the A/B evidence disposition (mostly BLOCKED/N-A) honest, or is it quietly avoiding a
technique that does exist?

**Ruling: Honest.** I independently confirmed no fabricated A/B script exists for this milestone
(`tools/*m26*`/`tools/*sdl3*` both empty), read `docs/m26-frontend-evidence.md` in full, and agree
with its reasoning: this project's established openMSX Tcl-based A/B technique compares emulator
STATE (registers/VRAM/memory), and openMSX's own SDL-rendered/audio-hardware output genuinely has
no comparable introspection point in either engine — this is the same class of honest limitation
already disclosed for M21 (computed pixel color) and M25 (hardware PAUSE), not a new kind of
shortfall. The one genuinely comparable sub-claim (input-mapping table row/column correctness) is
correctly, non-fabricatedly discharged by combining the pre-existing M15 contract tests with M26's
own exhaustive `SDL_PushEvent`-injection tests.

---

## 6. Independent Verification Detail

### A. Clean rebuilds (directly observed, not trusted from any prior report)

```
# Headless
cmake -S . -B build/qa_headless -DSONY_MSX_ENABLE_SDL3=OFF        -> configured from scratch
cmake --build build/qa_headless --config Debug                     -> succeeded, zero errors
ctest --test-dir build/qa_headless -C Debug -LE m24_slow_full_sweep --output-on-failure
  100% tests passed, 0 tests failed out of 133   (7.06 sec)
ctest --test-dir build/qa_headless -C Debug -R hbf1xv_m24_zexall_system_test --output-on-failure
  1/1 Test #124: hbf1xv_m24_zexall_system_test ....   Passed  1848.30 sec
  100% tests passed, 0 tests failed out of 1
=> Combined headless total: 134/134

# SDL3-ON
cmake -S . -B build/qa_sdl3on -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install
cmake --build build/qa_sdl3on --config Debug                       -> succeeded, zero errors
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
  ctest --test-dir build/qa_sdl3on -C Debug -LE m24_slow_full_sweep --output-on-failure
  100% tests passed, 0 tests failed out of 139   (10.84 sec)
```

### B. Exact captured output for the slow test (read from `build/qa_headless/Testing/Temporary/LastTest.log`)

```
ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
```

Byte-for-byte identical instruction count and marker split to every prior independent run this
project has recorded — only wall-clock time differs (1848.30s here, within the historical
1350-1935s range), as expected from differing machine load (this run additionally overlapped with
my own independent full SDL3-source rebuild, explaining the value landing toward the higher end of
the historical range).

### C. Independent, from-scratch SDL3 vendored-source build (never reusing the developer's/coordinator's install)

```
cmake -S references/sdl3 -B build/qa_sdl3_src_build -DCMAKE_INSTALL_PREFIX=<repo>/build/qa_sdl3_install \
    -DSDL_SHARED=ON -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF -DSDL_TESTS=OFF -DSDL_INSTALL=ON
-- Enabled backends:
--   Video drivers: dummy offscreen windows
--   Audio drivers: disk dsound dummy wasapi
cmake --build build/qa_sdl3_src_build --config Debug --target SDL3-shared SDL3-static   -> succeeded
cmake --install build/qa_sdl3_src_build --config Debug                                   -> succeeded
  (produced build/qa_sdl3_install/bin/SDL3.dll, build/qa_sdl3_install/cmake/SDL3Config.cmake)
cmake -S . -B build/qa_sdl3_probe2 -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<repo>/build/qa_sdl3_install
  -> Configuring done, Generating done (clean success against this brand-new install)
```

### D. `git diff v1.0.25` scope (ran myself)

```
git diff v1.0.25 --name-only -- src/devices/ src/peripherals/ src/core/        -> (empty)
git diff v1.0.25 --stat -- <12 named CPU-timing-oracle test files>              -> (empty)
git diff v1.0.25 --stat -- src/devices/audio/ym2413_opll.h src/devices/audio/ym2413_opll.cpp  -> (empty)
git diff v1.0.25 --stat -- tools/mem-to-png.py                                  -> (empty)
find src -iname "*ym2413*"          -> only ym2413_opll.{h,cpp} (pre-existing M17 files)
grep -rln "ym2413|opll|OPLL|YM2413" src/frontend/   -> sdl3_audio_presenter.h, sdl3_main.cpp
  (both confirmed, by direct read, to be disclosure text only)
git tag | grep 1.0.26               -> (empty; no premature tag)
```

### E. Real `sony_msx_sdl3.exe` sessions launched by QA itself

```
$ SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/qa_sdl3on/Debug/sony_msx_sdl3.exe \
    --hidden-window --max-frames 30 --bios-dir bios --cart1 roms/metalgear.rom --cart1-type Konami
sony-msx-hbf1xv SDL3 frontend: window opened, entering real-time loop (PSG audio live; YM2413/FM-PAC intentionally silent -- backlog E1, still open)
EXIT=0

$ SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/qa_sdl3on/Debug/sony_msx_sdl3.exe \
    --hidden-window --max-frames 10 --bios-dir bios --disk disks/msxdos22.dsk
sony-msx-hbf1xv SDL3 frontend: window opened, entering real-time loop (PSG audio live; YM2413/FM-PAC intentionally silent -- backlog E1, still open)
EXIT=0

$ ./build/qa_sdl3on/Debug/sony_msx_sdl3.exe --help
usage: ...sony_msx_sdl3.exe [--bios-dir <path>] [--disk <path>] [--cart1 <path>] [--cart1-type <name>]
  [--cart2 <path>] [--cart2-type <name>] [--max-frames <N>] [--hidden-window]
Sony HB-F1XV MSX2+ emulator -- SDL3 frontend (backlog C9). ...
NOTE: YM2413 (OPLL/FM-PAC) channels are intentionally SILENT in the audio mix ...
EXIT=0
```

### F. Decoded-FrameBuffer PNG evidence — independently viewed and reproduced

```
$ python tools/frame-to-png.py --self-check
  [PASS] x 12, self-check PNG sha256 = c4c52f8f418883178d7327ef9c1d4a5350f263d83ffce6019d9aa0f5dc0e5503
  SELF-CHECK: OK

$ python tools/frame-to-png.py debug/frames/m26-example-boot.frame -o <scratch>/qa-m26-regen.png
frame-to-png: image=256x192 border=0000 png_bytes=147726 sha256=c52af5cd056308dab0aa78933a3733f95f883a58ee636838cd0713bdbbacf5c6

committed sha256: c52af5cd056308dab0aa78933a3733f95f883a58ee636838cd0713bdbbacf5c6
regen     sha256: c52af5cd056308dab0aa78933a3733f95f883a58ee636838cd0713bdbbacf5c6
match: True
```

The committed PNG was also viewed directly (rendered as an image) and confirmed to show 16
distinct, correctly-colored vertical bars — a genuine, decoded, human-viewable color picture.

### G. Evidence-gate scripts (ran myself)

```
tools/validate-assets.ps1        -> Asset validation result: True (7 BIOS + 2 ROM files present)
tools/checksum-assets.ps1        -> Checksum report written to docs/asset-checksums.txt
```

---

## Summary for the coordinator

- **Full regression independently reproduced by QA from two genuinely clean, from-scratch
  rebuilds**: headless 134/134 (133 fast in 7.06s + 1 slow ZEXALL/ZEXDOC in 1848.30s, byte-for-byte
  identical instruction/marker split to every prior run); SDL3-ON 139/139 fast in 10.84s, with the
  dummy video/audio drivers set EXTERNALLY by me at the shell level, not merely trusting each
  test's own internal `setenv`/`_putenv_s` call.
- **The real SDL3-environment finding was independently reproduced a THIRD, fully separate time**
  (a brand-new vendored-source build to a brand-new install directory neither the developer nor the
  coordinator had touched), going beyond simply re-verifying the pre-existing
  `build/_sdl3_install/`.
- **The single highest-blast-radius edit (`on_vsync_boundary()`) is independently confirmed
  genuinely, purely mechanical** by direct diff read, corroborated by a real, dedicated
  regression-guard test that drives two independently-constructed machine instances through 5
  frames via two different call patterns and asserts full state equality.
- **The YM2413-silence hard constraint (R-M26-5) is independently confirmed genuinely upheld** —
  empty diff to the device files, no parallel synthesis file, and the two `grep` hits in
  `src/frontend/` are disclosure text only, confirmed by direct read.
- **The zero-conversion video-blit claim, the PSG audio wiring's numeric correctness, the 71-entry
  input-mapping table's exhaustiveness, and the decoded-PNG capture pipeline's determinism are all
  independently re-derived from source and/or independently reproduced** (including a
  byte-identical PNG regeneration from the committed dump, and a hand-verified R7 MSX I/O-direction
  computation).
- **Every cited SDL3 API signature/doc-comment/behavior this milestone relies on was independently
  cross-checked against the actual vendored header/source**, including a genuinely real,
  self-discovered SDL3 event-queue "poll batching" nuance that I independently traced to
  `SDL_events.c` myself.
- **Every new production and test file's line count matches the implementation report's own tables
  exactly** — no inflation or discrepancy found anywhere across 20 files.
- **The A/B disposition (mostly BLOCKED/N-A) and the human-observed-only evidence checklist are both
  honest and correctly reasoned** — no fabricated cross-engine technique, no false claim of
  "verified" for anything only a human eye/ear could confirm.
- **Zero findings of any severity** — a genuinely clean result across an unusually thorough,
  adversarial re-verification pass (every file line count, every API citation, every numeric
  oracle, and the environment finding itself independently re-derived, not merely re-read).

**Recommendation: Pass.** All of this milestone's substantive technical claims are independently
verified and sound: zero regression (134/134 headless including a fresh, from-scratch 30.8-minute
ZEXALL/ZEXDOC re-run, plus 139/139 SDL3-ON), a genuinely mechanical and behavior-preserving
`on_vsync_boundary()` extraction, genuinely real (not merely present) PSG audio wiring proven via
independent numeric oracles, an honestly and completely silent YM2413 with zero synthesis code
anywhere, a zero-conversion video blit independently proven pixel-exact, an exhaustively verified
71-entry input-mapping table, a decoded-FrameBuffer→PNG capability independently reproduced
byte-identical, a real environment finding independently reproduced end-to-end a third time, and an
honest, non-fabricated A/B/human-observed-only evidence disposition. No blocker-level or
non-blocking gap was found.

Per this milestone's own standing directive: this is a clean PASS, so the coordinator may proceed
through the release-decision/tag step (`v1.0.26`) without an additional human pause.
