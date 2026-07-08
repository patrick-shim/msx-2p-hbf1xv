# M26 Implementation Report — SDL3 Frontend (closes C9)

- Milestone ID: M26
- Developer: MSX Developer Agent
- Planner package: `docs/m26-planner-package.md`
- Status recorded here: developer implementation COMPLETE, evidence captured. NOT yet closed —
  QA sign-off + coordinator release decision are the remaining gates. The `deferred-backlog.md`
  status-column ledger flip to DONE and any `v1.0.26` tag are left to the coordinator at closure
  time, per the dispatch's explicit instruction.

---

## 1. Milestone Target

Implement deferred-backlog row **C9** — the project's first frontend/presentation-layer
milestone: a genuine, working SDL3 application (window, real-time loop, video/audio
presentation, keyboard/joystick input) driving the existing, unmodified `Hbf1xvMachine` core —
plus the ONE new debug/testing capability the coordinator explicitly authorized: capturing the
*decoded* `FrameBuffer` (RGB555 pixels, M21) to a real color PNG under `debug/frames/`.

Slice plan followed exactly as scoped (`docs/m26-planner-package.md` §3):

- **S1** — `Hbf1xvMachine::on_vsync_boundary()`: a pure, mechanical extraction of `run_frame()`'s
  pre-M26 body, immediately followed by the CPU-timing-oracle `git diff` confirmation, before
  proceeding to anything else.
- **S2** — SDL3 application skeleton (`Sdl3App`: window/renderer/audio-stream lifecycle, the
  deterministic `run_one_frame()` core step vs. the real-time `run_interactive()` wrapper) + the
  first SDL3-ON `ctest` proof that the dummy-driver mechanism genuinely works in this real
  environment.
- **S3** — Video presentation (`Sdl3VideoPresenter`, zero-conversion `SDL_PIXELFORMAT_XRGB1555`
  texture blit), with a real pixel-readback proof.
- **S4** — Decoded-`FrameBuffer` → PNG capture (`src/machine/frame_dump.*` +
  `tools/frame-to-png.py`), headless-buildable.
- **S5** — Audio (`Sdl3AudioPresenter` + the SDL3-independent `PsgAudioPump`): real PSG
  synthesis wired from the frontend layer; YM2413/FM-PAC honestly, deliberately left silent.
- **S6** — Input mapping (`Sdl3InputMapper`): keyboard matrix, joystick, PAUSE/Speed-Controller/
  Ren-Sha-Turbo bindings, exhaustively tested via real `SDL_PushEvent` injection.
- **S7** — CLI/asset wiring (`src/frontend/sdl3_cli.h`): `--bios-dir`, `--disk` (A-M26-6), reused
  M19 cartridge flags, `--max-frames`.
- **S8** — Dedicated system test + A/B evidence + full backlog review + documentation.

## 2. A real, pre-existing environment finding: SDL3 was not installed — resolved by building the vendored source

Per the dispatch's explicit instruction to report this honestly rather than route around it: a
direct `cmake -S . -B build_probe -DSONY_MSX_ENABLE_SDL3=ON` attempt (before any other work)
failed —

```
CMake Error at CMakeLists.txt:75 (find_package):
  Could not find a package configuration file provided by "SDL3" with any of
  the following names:
    SDL3.cps
    sdl3.cps
    SDL3Config.cmake
    sdl3-config.cmake
```

No system-wide SDL3 package, vcpkg installation, or `CMAKE_PREFIX_PATH` hint existed in this
execution environment. However, `references/sdl3/` (the project's own vendored grounding source)
is a **complete, buildable SDL3 3.5.0 source distribution**, zlib-licensed (`references/sdl3/
LICENSE.txt` — permissive, not GPL; distinct from the openMSX GPL-isolation concern, since
*building and linking against* SDL3 as a binary dependency is the normal, intended use of a
zlib-licensed library, and is categorically different from copying its source into `src/`, which
this milestone does not do anywhere). This was built once, locally, and installed to
`build/_sdl3_install/` (gitignored, not committed, reproducible):

```
cmake -S references/sdl3 -B build/_sdl3_src_build \
    -DCMAKE_INSTALL_PREFIX=build/_sdl3_install \
    -DSDL_SHARED=ON -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF -DSDL_TESTS=OFF \
    -DSDL_INSTALL=ON
cmake --build build/_sdl3_src_build --config Debug --target SDL3-shared SDL3-static
cmake --install build/_sdl3_src_build --config Debug
```

Configure-time output confirmed the "dummy" video driver and the "disk"/"dummy" audio drivers are
genuinely compiled in (`-- Enabled backends: Video drivers: dummy offscreen windows ... Audio
drivers: disk dsound dummy wasapi`) — matching A-M26-2's source-read claim exactly. With
`-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install`, `-DSONY_MSX_ENABLE_SDL3=ON` now configures
cleanly and `sony_msx_sdl3.exe` builds and links successfully. **This resolves R-M26-1's real
environment risk without copying any SDL3 source into `src/`** — the only artifacts produced are
a local build/install tree under the already-gitignored `build/` directory. This is documented
here as the reproducible path a future agent/CI run in a similarly bare environment should follow;
it is not a permanent repository change (no CMakeLists.txt `find_package` behavior was altered,
per the dispatch's explicit "you are NOT building that from scratch" instruction).

A real, end-to-end smoke confirmation followed immediately (before any other SDL3 code was
written), directly answering the dispatch's central open question:

```
$ SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./build/sdl3-on/Debug/sony_msx_sdl3.exe \
    --hidden-window --max-frames 5 --bios-dir bios
sony-msx-hbf1xv SDL3 frontend: window opened, entering real-time loop (PSG audio live; YM2413/FM-PAC intentionally silent -- backlog E1, still open)
EXIT=0
```

**SDL3 IS discoverable/buildable in this environment (via the vendored source), and the "dummy"
video/audio driver mechanism genuinely works end to end** — a real `SDL_Init(SDL_INIT_VIDEO |
SDL_INIT_AUDIO)`, real window/renderer/audio-stream creation, and 5 real emulated frames, with zero
real display/audio hardware. This is not a hypothetical/assumed result — it was empirically
confirmed at S2, before the rest of the plan was built on top of it, per the dispatch's own
risk-sequencing instruction.

## 3. Code Changes

### 3.1 The one behavior-affecting core change (S1)

**`src/machine/hbf1xv_machine.h`/`.cpp`** — new public `Hbf1xvMachine::on_vsync_boundary()`. A
pure, mechanical extraction of `run_frame()`'s pre-M26 body (frame counter, `vdp_.on_vsync()`,
`pause_controller_.on_vsync()`, `last_vsync_cycle_` bookkeeping), EXCLUDING the
`scheduler_.tick(kFrameCycles)` call. `run_frame()` itself becomes:

```cpp
void Hbf1xvMachine::run_frame() {
    scheduler_.tick(kFrameCycles);
    on_vsync_boundary();
}
```

Textually different, behaviorally IDENTICAL for every pre-M26 caller (confirmed by the new
regression-guard test, §5.1, and by `git diff` showing zero change to `src/devices/`,
`src/peripherals/`, `src/core/`, §4). A real-time driver (`Sdl3App::run_one_frame()`) instead
steps the CPU purely via `step_cpu_instruction()` in a sub-loop until the frame-cycle boundary,
then calls `on_vsync_boundary()` directly — never `run_frame()` in the same session (A-M26-5,
the double-count hazard already named at the test level by M25's R-M25-5).

Also additive to the same two files (S4, non-behavior-affecting introspection, mirroring the
`write_state_dump()`/`write_cpu_trace()`/`write_event_log()` precedent): `serialize_frame_dump()`
(pure/const) and `write_frame_dump()`, delegating to the new `src/machine/frame_dump.*`. This is
the second, disclosed edit to `hbf1xv_machine.{h,cpp}` this milestone — the dispatch text's
framing of `on_vsync_boundary()` as "the ONLY change to `src/machine/`" is read here as referring
to the sole BEHAVIOR-affecting change (matching planner §4 Acceptance Criterion 6's own precise
wording, which scopes the "no behavior change" claim to `src/devices/`, `src/peripherals/`,
`src/core/` — never claiming zero change to `src/machine/` as a whole); the frame-dump addition is
purely additive, non-perturbing (does not mutate CPU/memory/clock state), and exercised by a
dedicated headless unit test (§5.1).

### 3.2 New production files, `src/frontend/` (SDL3-gated, compiled only under `-DSONY_MSX_ENABLE_SDL3=ON`)

| File | Lines | Role |
|---|---|---|
| `sdl3_app.h`/`.cpp` | 120+206 | `Sdl3App`: SDL_Init/window/renderer/audio-stream lifecycle, machine ownership + asset loading, the deterministic `run_one_frame()` core step (poll events → CPU sub-loop to frame boundary → `on_vsync_boundary()` → video blit → audio pump), and the real-time `run_interactive()` wrapper (`SDL_Delay` pacing to ~16.6869 ms/frame + `SDL_EVENT_QUIT` handling — NEVER exercised by `ctest`). |
| `sdl3_video_presenter.h`/`.cpp` | 58+68 | `Sdl3VideoPresenter`: owns the `SDL_PIXELFORMAT_XRGB1555` `SDL_Texture` (recreated on mode switch), `blit_frame()`/`present()` split so a test can read back presented pixels between the two steps (SDL3's own documented recommendation). |
| `sdl3_audio_presenter.h`/`.cpp` | 90+61 | `Sdl3AudioPresenter`: `SDL_OpenAudioDeviceStream`/`SDL_ResumeAudioStreamDevice`/`SDL_PutAudioStreamData` wiring around `PsgAudioPump`; YM2413 honestly silent (§3.4). |
| `sdl3_input_mapper.h`/`.cpp` | 102+210 | `Sdl3InputMapper`: the 71-entry `SDL_Scancode`→(row,column) table (rows 0-8 of the standard MSX 11×8 matrix), joystick digital mapping, PAUSE/Speed-Controller/Ren-Sha-Turbo PC-keybindings (A-M26-7). |

### 3.3 New production files, headless-buildable (compiled into `sony_msx_core` unconditionally — zero SDL3 dependency)

| File | Lines | Role |
|---|---|---|
| `src/machine/frame_dump.h`/`.cpp` | 48+163 | Deterministic decoded-`FrameBuffer` dump serializer/parser (S4), mirroring `debug_dump.h`'s exact ASCII discipline and REUSING its `serialize_region()` folded-hex routine (genuine reuse, not a parallel reimplementation). |
| `src/frontend/sdl3_cli.h`/`.cpp` | 36+66 | `parse_sdl3_cli()`: pure argv parser for `--bios-dir`/`--disk`/`--max-frames`/`--hidden-window`, delegating to the existing, unmodified `sony_msx::machine::parse_cartridge_cli` for the reused M19 cartridge flags. |
| `src/frontend/psg_audio_pump.h`/`.cpp` | 50+22 | `PsgAudioPump`: the SDL3-INDEPENDENT numeric core of the audio wiring — advances a `PsgYm2149` by a fixed cycles-per-sample delta and collects `StereoSample`s. Factored out specifically so the "wiring" (not just the underlying, already-unit-tested-since-M15 `advance_cycles()`/`sample()` functions) is headlessly `ctest`-provable (discharges R-M26-4). |

### 3.4 The honest YM2413 non-change (R-M26-5, a hard constraint)

Per the planner's design and the dispatch's explicit hard constraint, **zero code was added
anywhere that synthesizes YM2413/FM-PAC audio.** Verified this cycle:

```
$ git diff --stat -- src/devices/audio/ym2413_opll.h src/devices/audio/ym2413_opll.cpp
(empty — zero changes)

$ find src -iname "*ym2413*"
src/devices/audio/ym2413_opll.cpp
src/devices/audio/ym2413_opll.h
(only the two pre-existing M17 files — no new parallel YM2413-audio file)
```

`Sdl3AudioPresenter`'s own header doc comment explicitly discloses the silence and the reasoning
(inventing a DSP pipeline — log-sin/exp operator tables, the 128-level EG rate mechanism, etc. —
would be exactly the kind of unfounded shortcut this project's culture explicitly disallows); the
same disclosure is printed to the user at SDL3-app startup (`sdl3_main.cpp`'s startup message and
`--help` text).

### 3.5 Edited (additive only)

- **`src/main.cpp`** — new `run_frame_dump_demo()` (S4 evidence-generation helper) + a new
  `--frame-dump-demo <out_filename>` CLI mode. Drives a colorful, deterministic GRAPHIC4 test scene
  directly through the real `#98`/`#99`/`#9A` VDP port protocol via the existing, non-perturbing
  `debug_io_write()` seam (M13) — zero CPU driver program needed (independently re-expressing the
  exact port-write sequences `tools/gen-m21-vdp-render-probe.py` already established, never a Z80
  assembly round-trip). Used to produce the committed example PNG (§6). No other line of
  `src/main.cpp`'s pre-existing behavior is touched.
- **`src/frontend/sdl3_main.cpp`** — full rewrite of the 4-line stub (the explicit point of this
  milestone, not a scope violation): argv parsing via `sdl3_cli.h`, `--help`, `Sdl3App`
  construction/`init()`/`run_interactive()`/`shutdown()`.
- **`CMakeLists.txt`** — `frame_dump.cpp`, `sdl3_cli.cpp`, `psg_audio_pump.cpp` added to the
  always-built `sony_msx_core` sources (all three are genuinely SDL3-independent — verified by
  reading their own `#include` lists); a new `sony_msx_sdl3_frontend` static library (SDL3-gated)
  linking the four `src/frontend/sdl3_{app,video_presenter,audio_presenter,input_mapper}.cpp`
  files, so the SDL3-gated test executables can link it directly without recompiling per-test.
- **`tests/CMakeLists.txt`** — 4 new headless (unguarded) test registrations (S1/S4/S5/S7's
  headless-buildable pieces) + a new `if(SONY_MSX_ENABLE_SDL3)` block (mirroring the root
  `CMakeLists.txt`'s own pattern) registering 6 SDL3-gated tests, none of which are ever reached
  when the option is `OFF`.
- **`.gitignore`** — two explicit `!` exceptions so the one committed example PNG/dump under
  `debug/frames/` are NOT swallowed by the pre-existing `/debug/**/*.png` blanket-ignore rule.

### 3.6 Unmodified (verified, not merely asserted)

```
$ git diff --stat src/devices src/peripherals src/core
(empty — zero changes)
```

Zero changes to any CPU/VDP/audio/RTC/FDC/cartridge/memory/halnote/kanji/core file. The ONLY
`src/machine/` edits are the two disclosed, additive changes in §3.1.

### 3.7 New test files

| File | Lines | Gated? |
|---|---|---|
| `tests/integration/machine/hbf1xv_m26_vsync_boundary_integration_test.cpp` | 152 | Headless |
| `tests/unit/machine/frame_dump_unit_test.cpp` | 148 | Headless |
| `tests/unit/frontend/psg_audio_pump_unit_test.cpp` | 134 | Headless |
| `tests/unit/frontend/sdl3_cli_unit_test.cpp` | 91 | Headless |
| `tests/integration/frontend/sdl3_app_headless_integration_test.cpp` | 116 | SDL3-ON |
| `tests/integration/frontend/sdl3_video_presenter_pixel_integration_test.cpp` | 170 | SDL3-ON |
| `tests/integration/frontend/sdl3_audio_presenter_integration_test.cpp` | 93 | SDL3-ON |
| `tests/integration/frontend/sdl3_input_mapper_integration_test.cpp` | 327 | SDL3-ON |
| `tests/integration/frontend/sdl3_cli_session_integration_test.cpp` | 165 | SDL3-ON |
| `tests/system/hbf1xv_m26_sdl3_system_test.cpp` | 156 | SDL3-ON |

### 3.8 New tools

- **`tools/frame-to-png.py`** (308 lines) — converts a `write_frame_dump()` output into a real,
  decoded, 24-bit truecolor PNG (color type 2), mirroring `tools/mem-to-png.py`'s exact
  determinism discipline (DEFLATE stored blocks only, `--self-check` hermetic mode with a golden
  SHA-256). `tools/mem-to-png.py` was NOT modified (it remains the raw-VRAM-bytes-as-grayscale
  tool it always was — genuinely insufficient for decoded color output, per its own doc comment).

## 4. Unit Test Results

Headless-buildable new unit tests (`-DSONY_MSX_ENABLE_SDL3=OFF`, the default):

```
$ ctest --test-dir build -C Debug -R "frame_dump_unit_test|psg_audio_pump_unit_test|sdl3_cli_unit_test" --output-on-failure
    Start 132: machine_frame_dump_unit_test
1/3 Test #132: machine_frame_dump_unit_test .......................................   Passed    0.03 sec
    Start 133: frontend_psg_audio_pump_unit_test
2/3 Test #133: frontend_psg_audio_pump_unit_test ..................................   Passed    0.02 sec
    Start 134: frontend_sdl3_cli_unit_test
3/3 Test #134: frontend_sdl3_cli_unit_test ........................................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 3
```

(A genuine, self-caught bug in `frame_dump_unit_test.cpp` during development — see §7.)

## 5. Integration Test Results

### 5.1 Immediate post-S1 CPU-timing-oracle confirmation (before proceeding to anything else)

Per the dispatch's explicit instruction, run IMMEDIATELY after the S1 `on_vsync_boundary()` edit:

```
$ git diff --stat src/devices src/peripherals src/core
(empty — zero diff)
```

Zero changes to any file in `src/devices/`, `src/peripherals/`, or `src/core/` — including every
CPU-core file the M9/M12/M23 zero-tolerance oracle suites depend on. The new S1 regression-guard
test (`machine_hbf1xv_m26_vsync_boundary_integration_test.cpp`) additionally proves, at the
machine-behavior level, that `run_frame()` produces byte-for-byte identical `elapsed_cycles()`/
`frame_count()`/`cycles_since_last_vsync()`/`vdp().irq_active()`/full-CPU-register-state results
to a second machine driven via the machine's own `run_cycles(frame_cycles_per_frame())` +
`on_vsync_boundary()` primitives (never a hardcoded literal), AND that a real-time-driver-style
scenario (CPU stepped purely via `step_cpu_instruction()` across a real 7-byte NOP/JP loop program
to the frame boundary, then `on_vsync_boundary()` called directly) produces the same class of
VBlank-interrupt-delivery/frame-count-increment side effects `run_frame()` would have produced:

```
$ ctest --test-dir build -C Debug -R hbf1xv_m26_vsync_boundary --output-on-failure
    Start 131: machine_hbf1xv_m26_vsync_boundary_integration_test
1/1 Test #131: machine_hbf1xv_m26_vsync_boundary_integration_test .................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 1
```

Acceptance Criterion 9's explicit, named 12-file CPU-timing-oracle test-file diff (the same file
list M23/M24/M25 each independently re-ran):

```
$ git diff --stat -- \
    tests/unit/devices/z80a_unprefixed_unit_test.cpp \
    tests/unit/devices/z80a_trace_export_unit_test.cpp \
    tests/integration/machine/hbf1xv_cpu_parity_integration_test.cpp \
    tests/integration/machine/hbf1xv_m11_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_parity_checkpoint_integration_test.cpp \
    tests/integration/machine/hbf1xv_indexed_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_cb_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_ldir_program_integration_test.cpp \
    tests/integration/machine/hbf1xv_interrupt_ack_integration_test.cpp \
    tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp \
    tests/integration/machine/hbf1xv_m23_halt_r_integration_test.cpp
(no output -- empty diff, exit code 0)
```

All 12 named zero-tolerance CPU-timing-oracle test files confirmed **byte-for-byte unchanged**.

### 5.2 SDL3-ON: all 6 new SDL3-gated tests + the reused headless CLI-parser test

Built and run under a SECOND, dedicated build directory (`build/sdl3-on/`, `-DSONY_MSX_ENABLE_
SDL3=ON`, `-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install` per §2), with `SDL3.dll` copied
alongside the test executables (the shared-library runtime dependency of the locally-built SDL3):

```
$ ctest --test-dir build/sdl3-on -C Debug -R "frontend_sdl3|hbf1xv_m26_sdl3" --output-on-failure
    Start 134: frontend_sdl3_cli_unit_test
1/7 Test #134: frontend_sdl3_cli_unit_test ............................   Passed    0.01 sec
    Start 135: frontend_sdl3_app_headless_integration_test
2/7 Test #135: frontend_sdl3_app_headless_integration_test ............   Passed    0.45 sec
    Start 136: frontend_sdl3_video_presenter_pixel_integration_test
3/7 Test #136: frontend_sdl3_video_presenter_pixel_integration_test ...   Passed    0.35 sec
    Start 137: frontend_sdl3_audio_presenter_integration_test
4/7 Test #137: frontend_sdl3_audio_presenter_integration_test .........   Passed    0.06 sec
    Start 138: frontend_sdl3_input_mapper_integration_test
5/7 Test #138: frontend_sdl3_input_mapper_integration_test ............   Passed    0.01 sec
    Start 139: frontend_sdl3_cli_session_integration_test
6/7 Test #139: frontend_sdl3_cli_session_integration_test .............   Passed    0.99 sec
    Start 140: hbf1xv_m26_sdl3_system_test
7/7 Test #140: hbf1xv_m26_sdl3_system_test ............................   Passed    0.49 sec

100% tests passed, 0 tests failed out of 7
Total Test time (real) =   2.37 sec
```

Every one of these 6 new SDL3-gated tests (plus the shared, headless-buildable
`frontend_sdl3_cli_unit_test`) ran entirely under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=
dummy`, set via `_putenv_s`/`setenv` BEFORE any SDL3 call, with `SDL_GetCurrentVideoDriver()`/
`SDL_GetCurrentAudioDriver()` independently asserted to genuinely report `"dummy"` (not merely
assumed from a successful `SDL_Init()` — a driver could in principle have silently fallen back to
something else). Zero `SDL_Delay` calls anywhere in any of these test files (grep-confirmed);
`Sdl3App::run_interactive()` is never called from any test.

### 5.3 Full sdl3-on suite (all shared M1-M25 + M26 headless tests, plus the 6 new SDL3-gated tests)

```
$ ctest --test-dir build/sdl3-on -C Debug -LE m24_slow_full_sweep --output-on-failure
...
139/139 Test #140: hbf1xv_m26_sdl3_system_test ........................................   Passed    0.48 sec

100% tests passed, 0 tests failed out of 139
Total Test time (real) =   9.62 sec
```

All 139 fast tests (133 shared headless-buildable tests, since `hbf1xv_m24_zexall_system_test` is
excluded by the label filter here, + 6 new SDL3-gated tests) pass in the SDL3-ON build tree too —
confirming zero regression from the SDL3 wiring for every prior milestone's own test, compiled a
second time under this configuration.

### 5.4 A real, discovered SDL3 mechanism worth recording (grounded, not assumed)

While building `sdl3_input_mapper_integration_test.cpp`'s exhaustive `SDL_PushEvent`-injection
loop, a genuine SDL3 event-queue nuance was discovered empirically and traced to source
(`references/sdl3/src/events/SDL_events.c:1709-1750`, `SDL_WaitEventTimeoutNS`): `SDL_PollEvent`
implements internal "poll batching" via a `SDL_EVENT_POLL_SENTINEL` marker event — a single
`SDL_PollEvent` call can return `false` merely because it reached the end of the CURRENT batch,
even though a just-`SDL_PushEvent`'d real event is queued immediately behind that boundary. A
naive "push, then drain-until-type-matches" test helper is therefore unsound: it can (a) give up
too early, leaving the real event still queued, and (b) on a LATER call, dequeue that now-stale
leftover and falsely type-match it against an unrelated search. The final `push_and_poll()` helper
(i) retries across multiple poll batches (bounded, cheap) and (ii) matches on exact event content
(scancode+down-state for keys; which+button+down for joystick buttons; which+axis+value for
joystick axes), never type alone — eliminating both failure modes. This is disclosed here per
R-M26-6's own discipline (read the actual vendored source for every SDL3 mechanic touched, rather
than assume) and because it is a genuinely reusable finding for any future milestone that also
needs to inject synthetic SDL3 events in a test.

### 5.5 Evidence gates

```
$ tools/validate-assets.ps1
Asset validation result: True
BIOS directory: bios/ -- 7 files present
ROM directory: roms/ -- 2 files present (aleste.rom, metalgear.rom)

$ tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
Checksum report written to: docs/asset-checksums.txt (unchanged asset set/hashes from prior milestones)

$ cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF
$ cmake --build build --config Debug
(clean full build succeeds -- sony_msx_headless.exe + sony_msx_core + all 134 headless test executables)

$ cmake -S . -B build/sdl3-on -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install
$ cmake --build build/sdl3-on --config Debug
(clean full build succeeds -- sony_msx_sdl3.exe + sony_msx_sdl3_frontend + all 140 test executables)

$ python tools/frame-to-png.py --self-check
  [PASS] parse_frame_dump: width matches
  [PASS] parse_frame_dump: height matches
  [PASS] parse_frame_dump: border matches
  [PASS] parse_frame_dump: pixel bytes match
  [PASS] reproducible: two encodes byte-identical
  [PASS] signature is the 8-byte PNG magic
  [PASS] IHDR width == 4
  [PASS] IHDR height == 3
  [PASS] IHDR bit depth == 8
  [PASS] IHDR color type == 2 (truecolor)
  [PASS] ends with IEND chunk
  [PASS] rgb555_to_rgb888(0x0100) hand-computed oracle
  self-check PNG sha256 = c4c52f8f418883178d7327ef9c1d4a5350f263d83ffce6019d9aa0f5dc0e5503
  self-check PNG bytes  = 107
  [PASS] golden sha256 matches
SELF-CHECK: OK
```

### 5.6 Full headless suite (all M1-M25 + M26, including the slow M24 ZEXALL/ZEXDOC sweep)

Run directly/synchronously per the dispatch's explicit instruction (not nested behind an
abandoned background wait — the tool auto-backgrounded the actual `ctest` invocation because it
genuinely exceeds this environment's ~10-minute single-call foreground limit, a real platform
constraint the M24/M25 precedent did not have to contend with; the developer explicitly stayed on
this task, monitored the background process's live CPU usage independently as a sanity check while
it ran, and captured its real, completed output below — never proceeding on an assumed result):

```
$ ctest --test-dir build -C Debug --output-on-failure
...
        Start 124: hbf1xv_m24_zexall_system_test
124/134 Test #124: hbf1xv_m24_zexall_system_test ......................................   Passed  1935.19 sec
        Start 125: devices_chipset_mb670836_pause_unit_test
125/134 Test #125: devices_chipset_mb670836_pause_unit_test ...........................   Passed    0.01 sec
        Start 126: peripherals_rensha_turbo_unit_test
126/134 Test #126: peripherals_rensha_turbo_unit_test .................................   Passed    0.08 sec
        Start 127: peripherals_rensha_turbo_integration_test
127/134 Test #127: peripherals_rensha_turbo_integration_test ..........................   Passed    0.01 sec
        Start 128: machine_hbf1xv_m25_pause_integration_test
128/134 Test #128: machine_hbf1xv_m25_pause_integration_test ..........................   Passed    0.02 sec
        Start 129: machine_hbf1xv_m25_speed_controller_integration_test
129/134 Test #129: machine_hbf1xv_m25_speed_controller_integration_test ...............   Passed    0.02 sec
        Start 130: hbf1xv_m25_speed_pause_rensha_system_test
130/134 Test #130: hbf1xv_m25_speed_pause_rensha_system_test ..........................   Passed    0.02 sec
        Start 131: machine_hbf1xv_m26_vsync_boundary_integration_test
131/134 Test #131: machine_hbf1xv_m26_vsync_boundary_integration_test .................   Passed    0.02 sec
        Start 132: machine_frame_dump_unit_test
132/134 Test #132: machine_frame_dump_unit_test .......................................   Passed    0.03 sec
        Start 133: frontend_psg_audio_pump_unit_test
133/134 Test #133: frontend_psg_audio_pump_unit_test ..................................   Passed    0.01 sec
        Start 134: frontend_sdl3_cli_unit_test
134/134 Test #134: frontend_sdl3_cli_unit_test ........................................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 134

Label Time Summary:
m24_slow_full_sweep    = 1935.19 sec*proc (1 test)

Total Test time (real) = 1939.27 sec
```

**134/134 (130 prior M1-M25 + 4 new M26 headless tests), 0 failed, zero regression.** The slow
`hbf1xv_m24_zexall_system_test` genuinely re-ran to completion (1935.19s ≈ 32.25 minutes — longer
than the ~22-27-minute range M24/M25 recorded, consistent with this environment being under
heavier concurrent load this cycle from the SDL3 source build/second build-tree work happening
earlier in the same session, not a correctness concern: the instruction count and pass/fail
verdict are fully deterministic and independent of host speed) and PASSED its own hard
`error_markers == 0` gates for both ZEXALL and ZEXDOC (the M24-closure hardening fix, DEC-0022) —
confirming the CPU core's own already-QA-verified correctness is completely unaffected by this
milestone (expected, since `git diff --stat src/devices src/peripherals src/core` already
independently confirmed zero changes there; this test provides the strongest possible empirical
confirmation of that same fact, exercising 5,764,169,474 real instructions per suite through the
entire M1-M25 device/scheduling stack, untouched by any M26 change).

## 6. Decoded-FrameBuffer → PNG Capture: Real Evidence

A real, committed example under `debug/frames/`:

- `debug/frames/m26-example-boot.frame` (181,391 bytes) — the C++ dump, produced by:
  ```
  $ ./build/Debug/sony_msx_headless.exe --frame-dump-demo m26-example-boot.frame
  frame-dump-demo: wrote debug/frames/m26-example-boot.frame
  ```
  A cold-booted machine driven directly through the real `#98`/`#99`/`#9A` VDP port protocol
  (GRAPHIC4/SCREEN5 mode, a 16-entry vivid palette, 16 vertical 16px-wide color bars spanning the
  full 256×192 canvas) via the existing, non-perturbing `debug_io_write()` seam — zero CPU
  execution needed.
- `debug/frames/m26-example-boot.png` (147,726 bytes, 256×192, 24-bit truecolor) — produced by:
  ```
  $ python tools/frame-to-png.py debug/frames/m26-example-boot.frame -o debug/frames/m26-example-boot.png
  frame-to-png: image=256x192 border=0000 png_bytes=147726 sha256=c52af5cd056308dab0aa78933a3733f95f883a58ee636838cd0713bdbbacf5c6 out=debug/frames/m26-example-boot.png
  ```
  Visually confirmed (read directly as an image): 16 distinct, correctly-colored vertical bars —
  a genuine, decoded, human-viewable color picture, not raw-byte noise. This is the concrete
  artifact `tools/mem-to-png.py` could never have produced (no VDP-mode/palette awareness).

Both files are exempted from the pre-existing `/debug/**/*.png`/blanket-ignore `.gitignore` rules
via two new, explicit `!` exceptions (§3.5).

## 7. Known Issues / Residual Risks

- **Two self-caught bugs, fixed before requesting QA (both honestly disclosed, neither a
  production-code defect):**
  1. `tests/unit/machine/frame_dump_unit_test.cpp`'s Case 4 originally called
     `std::filesystem::remove_all(temp_root)` while an `std::ifstream` handle into that same
     directory tree was still open (in scope) — Windows refuses to delete a directory while a file
     handle inside it is open, throwing an uncaught `std::filesystem::filesystem_error` (visible
     as an unexplained `exit code 3`/abort). Fixed by scoping the `ifstream` to close before
     `remove_all()`, and switching to the non-throwing `std::error_code` overload of `remove_all()`
     defensively. Caught via direct instrumentation (temporary `std::cerr` trace points), not
     guessed.
  2. `tests/integration/frontend/sdl3_input_mapper_integration_test.cpp`'s original
     `push_and_poll()` test helper used a "push one event, then drain-until-type-matches" pattern
     that is unsound against SDL3's real internal poll-batching/sentinel mechanism (§5.4) — all 71
     exhaustive scancode-map cases initially failed. Root-caused via direct source reading of
     `references/sdl3/src/events/SDL_events.c` (not assumed/worked around blindly) and fixed by
     retrying across multiple poll batches and matching on exact event content. Zero production
     code was affected — this was purely a test-harness correctness issue.
- **`--disk` was NOT symmetrically added to the headless `sony_msx_headless` executable this
  cycle** — the planner named this as developer's discretion, not a hard requirement (§2.8's own
  "not a hard requirement of this milestone" framing). Given the scope already delivered
  (10 new files, ~1,700 production lines, ~1,550 test lines, both build configurations, the full
  regression suite twice), this was deliberately deferred rather than risk rushing an
  under-tested addition. A clean, low-risk candidate for M27 or a quick follow-up.
- **The SDL3 input-mapping table (`Sdl3InputMapper::scancode_map()`) covers the standard MSX
  matrix rows 0-8 only** (digits, punctuation, letters, SHIFT/CTRL/GRAPH/CAPS/CODE/F1-F5,
  F4/F5/ESC/TAB/STOP/BS/SELECT/RETURN, SPACE/HOME/INS/DEL/arrows) — the two numeric-keypad rows
  (9-10) are not mapped, a disclosed, bounded-scope simplification (§2.7 of the planner package
  named this as acceptable). MSX row 2 column 0 (":") is also intentionally left unmapped (no
  dedicated physical PC scancode exists for a bare colon key on a standard layout).
- **The video-presenter pixel-exact test (S3) normalizes the `SDL_RenderReadPixels()` readback
  surface to `SDL_PIXELFORMAT_XRGB1555` via SDL3's own `SDL_ConvertSurface()`** before the
  byte-for-byte comparison, since `SDL_RenderReadPixels` documents that it returns whatever format
  the CURRENT RENDER TARGET uses (confirmed empirically: the software renderer's backbuffer is
  `SDL_PIXELFORMAT_XRGB8888`, not the uploaded texture's own `XRGB1555` format). This conversion
  step is SDL3's own trusted, general-purpose color-format converter — not any per-pixel logic
  added by this project's own C++ code, which is independently confirmed (by direct source
  inspection of `Sdl3VideoPresenter::blit_frame()`) to hand `FrameBuffer::pixels.data()` to
  `SDL_UpdateTexture` with zero per-pixel transform.
- **Human-observed-only verification (§8 of `docs/m26-frontend-evidence.md`) was NOT performed by
  the developer this cycle** — genuinely cannot be, in a non-interactive agent environment with no
  real display/audio hardware. Named explicitly, not silently skipped, per the planner's own
  `ctest`-gated vs. human-observed-only split (§2.4).
- **A/B evidence** — see `docs/m26-frontend-evidence.md` for the full, honest disposition (mostly
  BLOCKED/N-A, per the planner's own Acceptance Criterion 10 reasoning; does not gate closure).

## 8. QA Handoff

- Headless full regression: **134/134** (130 prior M1-M25 + 4 new M26 headless tests), 0 failed,
  independently reproducible from a clean rebuild (`cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF`
  then `cmake --build build --config Debug` then `ctest --test-dir build -C Debug
  --output-on-failure`; this cycle's own run took 1939.27s / ~32.3 minutes total under this
  environment's concurrent load — budget generously, up to ~35 minutes, for the slow zexall
  system test alone).
- SDL3-ON regression (a SECOND, separate build directory,
  `-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install` if SDL3 is not otherwise installed — see §2):
  **139/139** fast tests (excluding the shared, already-verified slow zexall test) + the 6 new
  SDL3-gated tests, all passing, entirely under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`.
- `git diff` confirms zero changes to `src/devices/`, `src/peripherals/`, `src/core/`, and
  specifically zero changes to `src/devices/audio/ym2413_opll.*` (R-M26-5's hard constraint) — QA
  should independently re-run `git diff --stat src/devices src/peripherals src/core` and `find src
  -iname "*ym2413*"`.
- `Hbf1xvMachine::on_vsync_boundary()` is confirmed behavior-preserving both by direct code
  inspection (§3.1) and by the new regression-guard test (§5.1) — QA should independently read
  `src/machine/hbf1xv_machine.cpp`'s `run_frame()`/`on_vsync_boundary()` pair and confirm the
  four-operation extraction is genuinely mechanical.
- The one real environment finding (§2) — SDL3 not pre-installed, resolved by building the
  vendored `references/sdl3/` source locally — should be independently reproduced by QA to confirm
  the documented build sequence is genuinely sufficient, not merely reported as working.
- Video pixel-exactness (A-M26-3), audio-wiring numeric correctness (R-M26-4), exhaustive
  input-mapping coverage (Acceptance Criterion 4), and the YM2413-silence hard constraint
  (R-M26-5) are all independently, deterministically tested (§5) — QA should independently
  re-run the full SDL3-ON suite and spot-check at least the video-presenter and input-mapper
  tests' assertions directly against their source.
- The committed PNG evidence (§6) should be independently viewed by QA (read the PNG file
  directly) to confirm it is genuinely a decoded, multi-color image, not a placeholder.
- `docs/m26-frontend-evidence.md` records the honest A/B disposition and the explicit
  human-observed-only verification checklist — QA should assess whether the BLOCKED/N-A
  disposition is reasoned soundly (mirroring the M21/M25 precedent of a disclosed BLOCKED
  sub-claim not gating closure) and should NOT attempt to fabricate evidence for the
  human-observed-only category itself.
- Full 34-row deferred-backlog review completed (`agent-protocol/state/deferred-backlog.md`) —
  C9 → IN-PROGRESS (M26 implementation complete, pending QA); E1 re-affirmed OPEN with an honest
  cross-reference note; all other 32 rows re-affirmed unchanged.
