# M26 Planner Package — SDL3 Frontend (closes C9)

- Milestone ID: M26
- Title: SDL3 Frontend — Windowing, Real-Time Loop, Video/Audio Presentation, Input Mapping,
  Decoded-Frame PNG Capture
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M26-001 (kickoff, planner-first, no production code;
  `agent-protocol/channels/requests.md:1290-1301`)
- Decisions in force: DEC-0005 (backlog disposition discipline — every planner package restates
  all 34 rows), the human's 2026-07-08 "zero license-sensitive future work" standing directive
  (`agent-protocol/state/current-phase.md:3`, project memory `feedback_license_sensitive_scope.md`
  — never reproduce openMSX's own large data tables under `src/`, even "independently
  re-derived"; small discrete facts/constants remain the established, accepted pattern), and the
  coordinator's explicit scope-boundary decision recorded in REQ-M26-001 / `current-phase.md:7-16`
  (M26 = SDL3 frontend (C9) + ONLY the decoded-FrameBuffer-to-PNG capture capability; everything
  else from the human's debug/-tooling request is deferred whole-cloth to a new M27 "Production
  Hardening + Debug/Test Tooling" milestone, named as a forward-looking note only in §7 below, not
  designed here).
- Backlog effect this cycle: **C9 (SDL3 frontend) → target: closes IN FULL**, subject to one
  honestly-disclosed, non-blocking dependency-boundary finding in §2.6 (YM2413/FM-PAC audio
  synthesis does not exist yet — backlog **E1**, already explicitly annotated
  "paired with C9" by the M17 planner — stays OPEN; M26 binds real audio only to what the PSG
  genuinely can produce and discloses the YM2413 gap rather than fabricating FM synthesis). No
  other backlog row is touched. All 34 rows re-affirmed in §3.9.
- Gate: this is the first frontend/presentation-layer milestone in the project and touches new
  architectural territory (real-time pacing, real OS window/audio-device APIs, live input) — per
  the standing project discipline ("STOP and consult the human if QA does not reach a clean PASS"),
  the coordinator should treat a clean QA PASS as the bar for uninterrupted progression to a
  release decision; anything short of that is a real blocker requiring human consultation, not an
  auto-close candidate.

> Grounding rule: every claim below about SDL3 API/environment mechanics was independently
> verified this cycle by directly reading the vendored `references/sdl3/` source (never assumed
> from SDL2-era memory or general SDL knowledge — the task's explicit instruction, and this
> package's §2.4 records a real instance where that discipline caught a genuine SDL2→SDL3 naming
> change). `src/machine/hbf1xv_machine.{h,cpp}` was read in full to ground the real-time-loop
> architecture decision in §2.3 against the actual, already-working `run_frame()`/
> `step_cpu_instruction()` design (mirroring the M25 planner's own discipline of reading the
> machine before proposing a design). `src/devices/video/vdp_frame_renderer.{h,cpp}`/
> `frame_buffer.h` (M21), `src/devices/audio/psg_ym2149.h`/`.cpp` and `ym2413_opll.h` (M15/M17),
> `src/peripherals/keyboard_matrix.h`/`joystick.h` (M15) and `src/devices/chipset/
> mb670836_pause.h` / `src/peripherals/rensha_turbo.h` (M25) were all read in full before this
> design was proposed.

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) Windowing + real-time loop.** A genuine SDL3 application (`sony_msx_sdl3` — the CMake
  target already exists, §2.1) that opens a real window, drives the existing
  `Hbf1xvMachine::step_cpu_instruction()` core at real (or throttled) wall-clock pacing, and shuts
  down cleanly.
- **(b) Video presentation.** The existing M21 `VdpFrameRenderer`/`FrameBuffer` RGB555 pixel
  output, blitted to the real window once per emulated frame.
- **(c) Audio output.** Real, non-fabricated audio synthesis from whichever existing device
  genuinely supports it — per §2.6's honest finding, this is the PSG (YM2149) only; the YM2413
  (OPLL/FM-PAC) has zero waveform-synthesis capability today (backlog **E1**, still open) and is
  disclosed as silent in the mix, not faked.
- **(d) Input mapping.** Real SDL3 keyboard/joystick events mapped into the existing M15
  `KeyboardMatrix`/`JoystickPorts` APIs, plus the M25 PAUSE/Speed-Controller/Ren-Sha-Turbo API
  surfaces (`Mb670836PauseController::press_pause_button()`/`set_speed_level()`,
  `RenshaTurbo::set_speed()`) via new, disclosed, first-principles PC-keybinding choices (§2.7).
- **(e) The ONE new debug/testing capability** the coordinator authorized: a decoded-`FrameBuffer`
  (RGB555, from M21, **not** raw VRAM bytes) → real color PNG capture path, with at least one real
  committed example PNG under `debug/frames/` (§2.5). This is the sole new debug/tooling scope
  item; everything else from the human's original debug/-folder request is out (§1.2).
- **(f) CLI/asset wiring** for the SDL3 executable: BIOS asset root, cartridge loading (reuse the
  existing M19 `cartridge_cli` parser — do not reimplement), and — since the mechanism already
  exists and is trivial to expose (§2.8) — real disk-image loading (`DiskImage(bytes)` already
  accepts an arbitrary byte vector; there is simply no CLI flag anywhere in this project that
  calls it yet). CLAUDE.md's own baseline text names "load cartridges, load disks... in headless
  or SDL3 (windows)" as a single, joint requirement — this milestone is the natural, low-risk place
  to close the disk half of that gap for the SDL3 executable (and, at the developer's discretion,
  symmetrically for the pre-existing headless executable too).
- **(g) A deterministic, fully headless `ctest` suite** exercising the real SDL3 API (not a mock)
  under the "dummy" video/audio drivers (§2.4), plus a separate, honestly-named,
  non-`ctest`-gated human-observed verification category (§6).
- **(h) Full deferred-backlog review** — all 34 rows re-affirmed (§3.9).
- **(i) Zero regression** across the full M1-M25 suite (130 tests, headless `-DSONY_MSX_ENABLE_
  SDL3=OFF` configuration — the default and authoritative regression gate per CLAUDE.md).

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M26 | Owning milestone (candidate) |
|---|---|---|
| **Audio capture to file** (`tools/mem-to-audio.py` integration or equivalent) | Explicitly, deliberately deferred by the coordinator's REQ-M26-001 scope-boundary decision to a new M27 "Production Hardening + Debug/Test Tooling" milestone. | M27 |
| **Full CPU/memory/VRAM state-dump CLI wiring** (`write_state_dump()`/`write_cpu_trace()`/`write_event_log()`, M10-S3, has never had a CLI flag) | Same explicit coordinator deferral. | M27 |
| **Keystroke-sequencing/scripted-input automation** (a macro/scripting layer over `KeyboardMatrix::set_key()`) | Same explicit coordinator deferral; does not exist anywhere in this project today. | M27 |
| **Broader production-readiness testing** (performance/timing validation under real wall-clock constraints, stress testing, cross-platform build validation, replay-determinism validation via the debug event log) | Same explicit coordinator deferral. | M27 |
| **YM2413/FM-PAC audio-waveform synthesis (DSP depth)** | Genuinely does not exist in this codebase (§2.6) — backlog **E1**, already OPEN since M17, already annotated "paired with C9" by that milestone's own planner. M26 binds real audio to what exists (PSG); fabricating FM synthesis ad hoc, ungrounded in the fact-sheet's DSP pipeline (log-sin/exp operator tables, EG rate mechanism, etc. — YM2413 fact-sheet §4/§5/§7/§9), would be exactly the kind of unfounded shortcut this project's culture explicitly disallows. | A future dedicated audio-rendering milestone (E1's own candidate-owner note) |
| **Interlaced-field (`Field::Even`/`Field::Odd`) real-time presentation** | `VdpFrameRenderer::render_frame()` already supports it, but full interlace/raster-timing depth is backlog **D4** (still IN-PROGRESS, M23 partial) — M26's presenter always requests `Field::Progressive`, matching how every prior milestone's tooling (`run_vdp_render_parity`, etc.) already defaults. Not a new limitation. | D4 (VDP-timing-depth milestone) |
| **Any CPU-execution-gating change, or any change to `src/devices/cpu/*`** | The real-time loop is a NEW CONSUMER of the existing `step_cpu_instruction()` contract, not a change to it. Zero edits to the Z80 core — protects the M9/M12/M23 zero-tolerance CPU-timing oracles, the single highest-sensitivity surface in this project. | N/A |
| **Full production keyboard/joystick remapping UI, config files, or persisted keybindings** | A UX-polish feature with no hardware-fidelity content; the milestone's job is a genuine, working, hard-coded default mapping (§2.7), not a configuration system. | A future UX milestone, if ever requested |
| **A literal Speed-Controller slider / Ren-Sha-Turbo slider UI widget** | No slider hardware exists on a PC; M26 binds two discrete keys per control (§2.7), a disclosed first-principles UX choice, not a hardware fact. | N/A — deliberate design decision |
| **Any change to `agent-protocol/state/deferred-backlog.md` row E1's numeric content** (only its cross-reference to M26 is added) | E1 remains genuinely unbuilt scope; M26 must not silently claim partial credit for it. | Future audio-rendering milestone |

### 1.3 Assumptions (each grounded, each with a verification action)

- **A-M26-1 (`SONY_MSX_ENABLE_SDL3` CMake wiring already exists and is functional — verified by
  direct read, not assumed).** `CMakeLists.txt:5,74-79` (read this cycle): the `option(...)`, the
  `find_package(SDL3 CONFIG REQUIRED)`, and the `sony_msx_sdl3` executable target linking
  `sony_msx_core` + `SDL3::SDL3` all already exist and are architecturally correct — this is
  **not** a from-scratch CMake task. The ONLY stub is the *source file*
  `src/frontend/sdl3_main.cpp` itself (`src/frontend/sdl3_main.cpp:1-6`, read this cycle): 4 lines,
  a single `std::cout` print, zero SDL3 API calls (does not even call `SDL_Init`). *Verify:* the
  developer confirms `-DSONY_MSX_ENABLE_SDL3=ON` actually configures successfully in the target
  build environment (an SDL3 package providing `SDL3Config.cmake` must be discoverable — a
  pre-existing project prerequisite documented in `CLAUDE.md`/`project-baseline.md`, not new to
  M26) before relying on it; if SDL3 is not installed in the current environment, this is a real
  blocker to report honestly, not to route around.
- **A-M26-2 (SDL3's real, concrete headless mechanism is the "dummy" video driver and the
  "dummy" audio driver — confirmed by reading the vendored source, not general SDL/SDL2-era
  knowledge).** `references/sdl3/src/video/dummy/SDL_nullvideo.c:51,123` (read this cycle):
  `#define DUMMYVID_DRIVER_NAME "dummy"`, registered as `VideoBootStrap DUMMY_bootstrap`.
  `references/sdl3/src/audio/dummy/SDL_dummyaudio.c:130-131` (read this cycle):
  `AudioBootStrap DUMMYAUDIO_bootstrap = { "dummy", "SDL dummy audio driver", ... }`. Both are
  selected by setting the driver **before** `SDL_Init()`, via the hints
  `SDL_HINT_VIDEO_DRIVER`/`SDL_HINT_AUDIO_DRIVER` (`references/sdl3/include/SDL3/
  SDL_hints.h:3871,507`) whose underlying environment-variable strings are, **in SDL3
  specifically**, `SDL_VIDEO_DRIVER` and `SDL_AUDIO_DRIVER` — **with an underscore between the
  words**, genuinely different from the classic SDL2 `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER` (no
  underscore). This is confirmed as a real, deliberate SDL2→SDL3 rename, not a guess: SDL3's own
  migration guide states it explicitly (`references/sdl3/docs/README-migration.md:802,857-860`,
  read this cycle): *"The environment variables SDL_VIDEODRIVER and SDL_AUDIODRIVER have been
  renamed to SDL_VIDEO_DRIVER and SDL_AUDIO_DRIVER, but the old names are still supported as a
  fallback."* M26's own tooling/tests should use the new, documented SDL3 names
  (`SDL_VIDEO_DRIVER=dummy`, `SDL_AUDIO_DRIVER=dummy`) rather than relying on the undocumented
  legacy fallback. *Verify:* the developer confirms `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)`
  genuinely succeeds under these two env vars in the actual CI/agent execution environment (no
  real display/audio hardware) before relying on it as the `ctest` mechanism.
- **A-M26-3 (`FrameBuffer`'s RGB555 pixel layout is bit-for-bit identical to SDL3's
  `SDL_PIXELFORMAT_XRGB1555` — confirmed by reading both headers, not assumed).**
  `src/devices/video/frame_buffer.h:28-30` (read this cycle): *"pixels is a flat, row-major buffer
  of width\*height native RGB555 values (bits 14-10=R, 9-5=G, 4-0=B)."*
  `references/sdl3/include/SDL3/SDL_pixels.h:572-573` (read this cycle):
  `SDL_PIXELFORMAT_XRGB1555 = SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16,
  SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_1555, 15, 2)` — bit 15 unused/X, bits 14-10 R, 9-5 G, 4-0
  B. This is an EXACT match, permitting a direct `SDL_UpdateTexture` blit of `FrameBuffer::pixels`
  with **zero per-pixel conversion**. *Verify:* the developer confirms no byte-swap is needed (both
  are plain host-native `uint16_t` values in memory for a 16-bit packed format — standard SDL
  behavior, not something this project's own reference material needs to additionally document,
  but worth a one-line runtime sanity check e.g. rendering a known-color synthetic frame and
  reading the presented pixel back).
- **A-M26-4 (the PSG's own sample-generation function, `PsgYm2149::advance_cycles()`, is currently
  never called anywhere in `src/` — a genuine, load-bearing finding from a direct repo-wide
  search, not an assumption).** `grep -rn "advance_cycles" src/` (run this cycle) returns only the
  function's own declaration/definition (`psg_ym2149.h:81`, `psg_ym2149.cpp:293`) — **zero call
  sites** anywhere in `src/machine/hbf1xv_machine.cpp` or any other machine/frontend file. The M15
  PSG device is fully register-accurate and its tone/noise/envelope generator counters are
  fully implemented and unit-tested (`docs/m15-*`), but the generator counters are **never
  actually advanced by real elapsed emulated time** in the shipped machine today — only
  `PsgYm2149::debug_step_envelope()` (a manual, clock-bypassing test-only stepper) exercises the
  generator logic at all pre-M26. §2.6 designs where M26 wires this. *Verify:* the developer
  independently re-runs this grep before relying on the finding.
- **A-M26-5 (a new machine-level bookkeeping seam is genuinely required for a real-time driver
  that steps the CPU itself — not avoidable by simply calling the existing `run_frame()`).**
  `run_frame()` (`src/machine/hbf1xv_machine.cpp:365-388`) does `scheduler_.tick(kFrameCycles)` —
  it advances the clock directly and **never calls `cpu_.step()` at all**. A real-time SDL3 loop
  MUST drive actual CPU execution (fetching real BIOS/game code) via repeated
  `step_cpu_instruction()` calls; calling `run_frame()` in the same session would additionally
  tick the scheduler by `kFrameCycles`, **double-counting elapsed cycles** — the exact hazard this
  project's own M25 planner already named and guarded against at the *test* level (R-M25-5:
  *"never call `run_frame()` and `step_cpu_instruction()` in the same test"*). §2.3 designs a
  small, additive, zero-risk machine-level extraction that resolves this cleanly for a *session*
  (not just a test). *Verify:* the developer confirms the new method is a pure, behavior-preserving
  extraction (§2.3) and that `run_frame()` itself remains textually/behaviorally unchanged from
  every M1-M25 caller's perspective.
- **A-M26-6 (real disk-image loading is already mechanically possible via the existing public API
  — confirmed by reading `disk_image.h`, not assumed).** `src/devices/fdc/disk_image.h:32` (read
  this cycle): `explicit DiskImage(std::vector<std::uint8_t> bytes);` already exists, and
  `Hbf1xvMachine::disk_image()`/`disk_drive()` (`hbf1xv_machine.h:235-236`) are already public,
  non-const accessors. A CLI flag that reads a file's bytes, constructs a `DiskImage`, assigns it
  via `machine.disk_image() = DiskImage(bytes)`, and re-attaches it
  (`machine.disk_drive().attach_image(&machine.disk_image())`) requires **zero machine-level
  change** — it is a pure, additive CLI/frontend-layer task. `grep -n "\-\-disk" src/main.cpp` (run
  this cycle) confirms no such flag exists anywhere in this project today, in either executable.
  *Verify:* the developer confirms this wiring compiles and a real `.dsk` file (e.g.
  `disks/msxdos22.dsk`, already present per the M24/M25 "Standing watch-items" note) loads and is
  byte-readable through the FDC.
- **A-M26-7 (PAUSE/Speed-Controller/Ren-Sha-Turbo PC-keybinding CHOICES are first-principles UX
  decisions, not hardware facts — same category as A-M25-1/A-M25-3, extended now to the physical
  PC key, not just the internal numeric model).** No PC keyboard has a physical PAUSE-toggle
  button, slow-motion slider, or autofire slider in the Sony sense; M26 must invent a reasonable
  binding (§2.7). *Verify:* if this project ever gains a configuration/remapping layer (explicitly
  out of scope this cycle, §1.2), these defaults become the initial preset, not a permanent
  hard-coded contract.
- **A-M26-8 (this milestone introduces the project's first genuinely real-time, wall-clock-paced
  code path — a categorically different kind of "input" than any prior milestone's determinism
  claims covered, and this package explicitly does not overclaim determinism it cannot deliver).**
  Every M1-M25 determinism claim concerned a FIXED, reproducible instruction/event trace producing
  bit-exact machine state. A live, human-interactive SDL3 session's WALL-CLOCK pacing (how many
  real milliseconds elapse between frames on a given host) is inherently host-speed-dependent —
  exactly as with any real-time emulator or real hardware, not a defect. §2.3/§4 draw a hard,
  explicit line: the underlying MACHINE STATE advancement (the exact same
  `step_cpu_instruction()`/`cold_boot()` calls used everywhere else) remains exactly as
  deterministic as ever; only the *host-wall-clock pacing decision* ("how long to `SDL_Delay`
  before the next frame") is real-time and intentionally not gated by `ctest`. *Verify:* §4's
  acceptance criteria require the core stepping logic to be structurally separable from the
  real-time pacing wrapper (§2.3), so `ctest` can exercise the former without the latter.

---

## 2. Spec Summary

### 2.1 CMake/build state — direct findings (per the coordinator's explicit question)

Read directly, not assumed:

- `CMakeLists.txt:5`: `option(SONY_MSX_ENABLE_SDL3 "Build SDL3 frontend" OFF)` — exists.
- `CMakeLists.txt:74-79`:
  ```cmake
  if(SONY_MSX_ENABLE_SDL3)
      find_package(SDL3 CONFIG REQUIRED)
      add_executable(sony_msx_sdl3 src/frontend/sdl3_main.cpp)
      target_link_libraries(sony_msx_sdl3 PRIVATE sony_msx_core SDL3::SDL3)
  endif()
  ```
  This is real, architecturally correct CMake wiring — **not** a from-scratch task. It already
  correctly discovers an installed SDL3 package and links the real `SDL3::SDL3` target.
- `src/frontend/sdl3_main.cpp` (in full):
  ```cpp
  #include <iostream>
  int main() {
      std::cout << "sony-msx-hbf1xv SDL3 frontend scaffold\n";
      return 0;
  }
  ```
  This IS the stub — a placeholder application with zero SDL3 API usage. **This is what M26
  builds**, not the CMake plumbing around it.
- `tests/CMakeLists.txt`: `grep -n "SDL3" tests/CMakeLists.txt` (run this cycle) returns **zero
  matches** — there is no existing SDL3-conditioned test registration anywhere. M26 must add a new
  `if(SONY_MSX_ENABLE_SDL3)` block to `tests/CMakeLists.txt` (mirroring the root file's own
  pattern) so the new SDL3-dependent tests are compiled/registered **only** in the `ON`
  configuration, keeping the headless `OFF` configuration's `ctest` suite (the default, per
  `CLAUDE.md`'s authoritative build flow) completely unaffected — 130/130, byte-for-byte, zero
  regression risk from anything this milestone adds.

### 2.2 Existing seams this milestone binds to (read in full before design)

| Existing API | Source | Role in M26 |
|---|---|---|
| `Hbf1xvMachine::step_cpu_instruction()` | `hbf1xv_machine.h:84` | The real-time loop's CPU-driving primitive (never `run_frame()` — see A-M26-5). |
| `Hbf1xvMachine::cold_boot()` | `hbf1xv_machine.h:57` | Session start. |
| `Hbf1xvMachine::render_frame(Field)` → `FrameBuffer` | `hbf1xv_machine.h:177-178` | Pull-model video source (M21); zero clock cost. |
| `Hbf1xvMachine::psg()` → `PsgYm2149&` | `hbf1xv_machine.h:183-184` | Audio source (§2.6). |
| `Hbf1xvMachine::ym2413()` → `Ym2413Opll&` | `hbf1xv_machine.h:190-191` | Register-accurate only — no synthesis (§2.6). |
| `Hbf1xvMachine::keyboard()`/`joystick()` | `hbf1xv_machine.h:196-199` | Input targets (§2.7). |
| `Hbf1xvMachine::pause_controller()`/`rensha_turbo()` | `hbf1xv_machine.h:222-225` | M25 API surfaces, purpose-built for this binding. |
| `Hbf1xvMachine::vdp()`, `V9958Vdp::on_vsync()` | `hbf1xv_machine.h:169-170`; called directly at `src/main.cpp:406` | Confirmed public and independently callable (§2.3). |
| `Hbf1xvMachine::load_cartridge()`, `machine::parse_cartridge_cli` | `hbf1xv_machine.h:247-249`; `src/machine/cartridge_cli.h` | Reused, not reimplemented (§2.8). |
| `devices::fdc::DiskImage(std::vector<uint8_t>)` | `disk_image.h:32` | Real disk loading, mechanically ready (A-M26-6). |
| `Hbf1xvMachine::debug_root()` | `hbf1xv_machine.h:298-300` | The existing `debug/` root convention the new frame-dump reuses (§2.5). |

### 2.3 Architecture decision — the real-time loop and the new `on_vsync_boundary()` seam

**Frame-cadence arithmetic (computed this cycle from existing constants, not a new fact):**
`kFrameCycles = 228 * 262 = 59,736` T-states/frame (`hbf1xv_machine.cpp:14`) at the
already-established `kSystemClockHz = 3,579,545` Hz system clock (independently declared per-file
in `wd2793.h`/`disk_drive.h`/`rp5c01.h`/`rensha_turbo.h` — the same real MSX NTSC clock this
project has used since M9): `59736 / 3,579,545 ≈ 16.6869 ms/frame ≈ 59.92 Hz` — close to, but not
exactly, real NTSC 59.94 Hz; a pre-existing scheduling characteristic of this project's frame
model, unrelated to and unchanged by M26.

**Decision: the SDL3 main loop drives the CPU purely via `step_cpu_instruction()` in a sub-loop
until `elapsed_cycles()` has advanced by `frame_cycles_per_frame()` since the last frame boundary,
then performs frame-boundary bookkeeping via a NEW, small, additive `Hbf1xvMachine::
on_vsync_boundary()` method — never `run_frame()` (A-M26-5).**

```cpp
// Hbf1xvMachine (M26-S1, additive):
//
// Frame-boundary bookkeeping for a REAL-TIME driver (the SDL3 frontend) that
// steps the CPU itself via step_cpu_instruction() in a sub-loop until
// elapsed_cycles() reaches a frame_cycles_per_frame() boundary, rather than
// using the coarse run_frame() tick (which does NOT drive the CPU at all --
// see run_frame()'s own body). Performs EXACTLY run_frame()'s
// non-scheduler-tick side effects (frame counter, VDP on_vsync(),
// pause-controller on_vsync(), last-vsync bookkeeping) but explicitly does
// NOT call scheduler_.tick(kFrameCycles) -- the caller's own
// step_cpu_instruction() loop has ALREADY advanced the scheduler by that
// amount. Calling this AND run_frame() for the same frame boundary in the
// same session would double-count elapsed cycles (the same class of hazard
// already documented at the test level, R-M25-5) -- a session must pick ONE
// driving model and never mix them.
void on_vsync_boundary();
```

Implementation is a **pure, behavior-preserving extraction** of `run_frame()`'s existing body:

```cpp
void Hbf1xvMachine::on_vsync_boundary() {
    ++frame_count_;
    vdp_.on_vsync();
    pause_controller_.on_vsync();
    last_vsync_cycle_ = elapsed_cycles();
}

void Hbf1xvMachine::run_frame() {
    scheduler_.tick(kFrameCycles);
    on_vsync_boundary();
}
```

This is textually different but **behaviorally identical** to the pre-M26 `run_frame()` for every
existing caller — same four operations, same order, same side effects. `git diff` on this function
should show only the mechanical extraction; every M1-M25 `run_frame()`-consuming test remains green
unchanged. `V9958Vdp::on_vsync()` is confirmed public and independently callable without this
extraction too (already exercised directly at `src/main.cpp:406`,
`machine.vdp().on_vsync();`, in the existing M22 sprite/command-engine A/B harness) — but
`frame_count()`/`cycles_since_last_vsync()`/`vdp_cycle_position()` (all pre-existing, public,
read-only introspection accessors used by the M23 VDP-access-timing foundation) would otherwise go
permanently stale during a real-time session if the frontend only called `vdp_.on_vsync()`/
`pause_controller_.on_vsync()` piecemeal — `on_vsync_boundary()` keeps them meaningful for free,
at zero extra risk.

**The real-time frame loop, structurally separated from wall-clock pacing (A-M26-8):**

```cpp
// src/frontend/sdl3_app.h (illustrative sketch, not literal final code --
// developer's judgment on exact shape per src/CLAUDE.md):
class Sdl3App {
public:
    // The DETERMINISTIC core step: poll+apply pending input, step the CPU
    // until the next frame boundary, call on_vsync_boundary(), pump audio
    // samples, and blit one presented frame. Contains ZERO wall-clock
    // delay/pacing logic -- directly, repeatedly callable from a ctest
    // integration test with fully deterministic, assertable results.
    void run_one_frame();

    // The REAL-TIME outer loop: run_one_frame() + SDL_Delay-based pacing to
    // ~16.69ms/frame + SDL_PollEvent-driven quit handling. NEVER called from
    // ctest.
    int run_interactive();
};
```

`run_one_frame()`'s CPU sub-loop:

```cpp
while (machine.elapsed_cycles() - frame_start_cycle < machine.frame_cycles_per_frame()) {
    machine.step_cpu_instruction();
}
machine.on_vsync_boundary();
```

### 2.4 Headless testability — the real mechanism, verified in source (per the task's explicit,
central instruction)

Confirmed this cycle (A-M26-2 §1.3, cites reproduced here for the spec record):

- **Video:** SDL3's real "dummy" video driver, `references/sdl3/src/video/dummy/
  SDL_nullvideo.c:51` (`DUMMYVID_DRIVER_NAME = "dummy"`), selected via the env var
  `SDL_VIDEO_DRIVER=dummy` (SDL3's own renamed hint — `SDL_hints.h:3871`).
- **Audio:** SDL3's real "dummy" audio driver, `references/sdl3/src/audio/dummy/
  SDL_dummyaudio.c:130-131` (`AudioBootStrap DUMMYAUDIO_bootstrap = {"dummy", ...}`), selected via
  `SDL_AUDIO_DRIVER=dummy` (`SDL_hints.h:507`).
- **A genuine, non-mocked technique for exercising the real input-event path headlessly:**
  `SDL_PushEvent` (`references/sdl3/include/SDL3/SDL_events.h:1449`, confirmed present) lets a
  test inject a **real** `SDL_Event` (e.g. a synthetic `SDL_EVENT_KEY_DOWN` with a specific
  `scancode`) directly into SDL3's own event queue, then call the app's real, unmodified
  `SDL_PollEvent`-based dispatch code — proving the actual input-mapping code path end-to-end,
  entirely under the dummy video driver, with no real keyboard/display hardware anywhere in the
  loop. This is the concrete mechanism §3.8's ctest suite uses for input-mapping tests — a real
  exercise of real SDL3 machinery, not a hand-rolled stand-in.

**The resulting, explicit `ctest`/human split (also see §6):**

| Category | Mechanism | Gated by `ctest`? |
|---|---|---|
| App init, window/renderer/texture/audio-stream creation succeed | `SDL_VIDEO_DRIVER=dummy`, `SDL_AUDIO_DRIVER=dummy`, `SDL_Init(SDL_INIT_VIDEO\|SDL_INIT_AUDIO)` | **Yes** |
| A bounded N-frame session advances machine state deterministically (registers/PC/`elapsed_cycles()`/`frame_count()`) | `Sdl3App::run_one_frame()` called N times directly, zero real-time pacing | **Yes** |
| Input-mapping table correctness (SDL scancode → matrix row/col; joystick button → port bit) | `SDL_PushEvent` synthetic injection + real dispatch, under the dummy driver | **Yes** |
| PSG audio-sample generation is genuinely non-silent/deterministic for a known register program | Direct `machine.psg().sample()` assertions after a deterministic `advance_cycles()` sequence (no real audio HARDWARE playback needed to test the numbers) | **Yes** |
| Decoded-`FrameBuffer`→PNG capture produces the documented, deterministic dump format | Direct C++ dump + a hermetic Python `--self-check` (mirrors the existing `tools/mem-to-png.py --self-check` precedent) | **Yes** |
| "Does a human actually SEE a correct picture in a real window and HEAR correct audio on real speakers, with real keyboard/joystick response, on a real machine" | Human launches the built `sony_msx_sdl3` executable normally (no dummy drivers) and observes it | **No — explicitly NOT `ctest`-gated; a separate, disclosed, human-observed check (§6)** |

### 2.5 Design — decoded-FrameBuffer → PNG capture (the ONE new debug capability)

Per the coordinator's explicit scope grant: capture the **decoded** `FrameBuffer` (RGB555, from
`VdpFrameRenderer`), not raw VRAM bytes — `tools/mem-to-png.py` (M10-S5) is explicitly
insufficient for this (it visualizes raw memory bytes as grayscale noise; it has no VDP-mode/
palette awareness at all, confirmed by reading it in full this cycle — `tools/mem-to-png.py:1-9`'s
own doc comment: *"It does NOT perform any real V9958 rendering, tile/sprite decoding, or palette
lookup... The image is a faithful, reversible picture of the raw bytes and nothing more."*).

**Two-stage design, mirroring the EXACT M10-S3→M10-S5 precedent** (C++ deterministically dumps
data; a separate Python tool renders the image — keeping image-format concerns out of the C++
core, per `CLAUDE.md`'s own explicit tools mandate: *"Generate all needed tools in python or
powershell in tools/... including... memory to content to png... convert"*):

1. **New C++ machine-level dump, mirroring `write_state_dump()`/`write_cpu_trace()`/
   `write_event_log()` exactly** (same directory-creation/error-handling pattern, same
   `debug_root_`-relative convention):
   ```cpp
   // Hbf1xvMachine (M26-S4, additive):
   [[nodiscard]] std::string serialize_frame_dump(
       devices::video::Field field = devices::video::Field::Progressive) const;  // pure/const
   bool write_frame_dump(const std::string& filename,
                          devices::video::Field field = devices::video::Field::Progressive);
   // writes to <debug_root_>/frames/<filename>
   ```
   Format: a small ASCII header (`[FRAME] width=W height=H border=HHHH`, mirroring the exact
   `RENDER width=.../BORDER=...` labels `run_vdp_render_parity` already uses at
   `src/main.cpp:336-337`) followed by the pixel buffer reinterpreted as raw bytes and serialized
   with the **existing, already-proven** `debug_dump::serialize_region()` folded-hex routine
   (`src/machine/debug_dump.{h,cpp}`, already used for DRAM/SRAM/VRAM) — genuine reuse, not a
   parallel reimplementation.
2. **New `tools/frame-to-png.py`** (sibling of `tools/mem-to-png.py`, same determinism discipline
   — DEFLATE *stored* blocks only, no timestamps/metadata chunks, a `--self-check` hermetic mode):
   parses the new dump format (reusing/adapting `mem-to-png.py`'s already-proven
   `parse_region_from_dump()` folding logic), expands each RGB555 pixel to 24-bit RGB8 via
   standard 5-bit→8-bit bit-replication (`(v << 3) | (v >> 2)`), and emits a **real, decoded,
   truecolor PNG** (color type 2), not grayscale. The developer should factor the shared,
   already-vetted `_chunk`/`_zlib_stored` PNG-encoding helpers out of `mem-to-png.py` into a small
   shared module (e.g. `tools/png_encode_common.py`) imported by both scripts — avoiding
   duplicated, unreviewed logic (guardrails: *"avoid duplicating script logic inline"*) — or, if
   the developer judges the coupling not worth it, duplicate the ~30-line, already-hermetically-
   tested encoder locally; either is acceptable, developer's call.
3. **At least one real, committed example PNG under `debug/frames/`** (hard acceptance criterion,
   §4): produced by driving a real, deterministic scene through the actual pipeline — e.g. cold
   boot + a bounded number of real BIOS-boot frames (reusing the existing M13/M15/M16 boot
   checkpoint precedent) or a synthetic VDP test pattern (reusing the existing M21/M22
   `gen-m21-vdp-render-probe.py`-style driver-program technique) — `render_frame()` →
   `write_frame_dump()` → `tools/frame-to-png.py` → commit the PNG. This does **not** require the
   SDL3 window to be running at all; the capture point is at the `FrameBuffer`/`VdpFrameRenderer`
   level, fully headless-reachable, exactly matching the coordinator's own framing.
4. **Evidence discipline:** the C++ dump-generation step is `ctest`-exercised (deterministic
   header/content assertions against a hand-computed oracle, mirroring every other debug-dump
   test in this project); the Python PNG-conversion step's evidence is captured via a direct,
   documented script invocation (`python tools/frame-to-png.py ... --self-check`) at
   implementation/QA time, mirroring how `tools/mem-to-png.py --self-check` is evidenced today —
   **not** claimed as a registered `ctest` case (this project's `ctest` suite is C++-only per
   `tests/CLAUDE.md`'s naming convention; do not overclaim CTest integration for a Python tool).

### 2.6 Design — audio output, and the honest PSG-vs-YM2413 finding

**The finding (A-M26-4, restated for the spec record):** `PsgYm2149::advance_cycles()`
(`psg_ym2149.h:81`, doc comment: *"Deterministic generator advance, driven READ-ONLY off the
machine clock... delta_cpu_cycles is the number of 3.58 MHz system cycles elapsed since the last
advance"*) and `PsgYm2149::sample()` (`psg_ym2149.h:83-89`, returns a numeric
`StereoSample{left, right}` — the resolved 0-31 amplitude of each of the 3 tone/noise channels,
mixed per the fact-sheet's documented A=Center/B=Left/C=Right stereo assignment) are **real,
already-implemented, already-unit-tested (M15) sample-generation machinery** — but
`advance_cycles()` has **zero call sites anywhere in `src/` today** (confirmed by a repo-wide
grep this cycle). The generator counters (tone/noise/envelope) have literally never been advanced
by real elapsed emulated time in the shipped machine; only a manual, clock-bypassing test-only
stepper (`debug_step_envelope()`) exercises them at all pre-M26.

`Ym2413Opll` (`ym2413_opll.h`, read in full this cycle) has **no `sample()`/`advance_cycles()`
method, no audio-generation code path whatsoever** — its own header doc comment says so explicitly
(`ym2413_opll.h:26-29`): *"...it carries NO bank-register / SRAM-handshake / ID-string-detection
logic... and NO audio waveform synthesis / DSP (backlog E1, explicitly deferred..."*. This is not
an oversight; it is the M17 planner's own deliberate, disclosed contract-vs-depth split (mirroring
the identical D1-vs-D-series and B3-vs-E1 precedent this project has used since M14), and backlog
row **E1** (`agent-protocol/state/deferred-backlog.md`, Section D) already explicitly names its
candidate owner as *"Future audio-rendering milestone, paired with C9 (SDL3 frontend)"* — meaning
the M17 planner already anticipated this exact tension a year of milestones ago.

**Decision: M26 wires real audio for the PSG only, from the frontend layer, and honestly discloses
YM2413 silence — it does not touch `Ym2413Opll` and does not fabricate FM synthesis.**

1. **PSG generator-advance wiring lives in the FRONTEND, not the machine core.** A new
   `src/frontend/sdl3_audio_presenter.{h,cpp}` tracks `machine.elapsed_cycles()` deltas itself and
   calls `machine.psg().advance_cycles(delta)` once per audio-sample tick (sample-rate-paced, not
   frame-paced — see point 3), then `machine.psg().sample()` for the instantaneous amplitude.
   **Considered and rejected alternative:** wiring `psg_.advance_cycles()` directly into
   `step_cpu_instruction()` (core-level, always-on for every build). Rejected because (a) it would
   touch the single most sensitivity-sensitive function in the project (the M9/M12/M23
   zero-tolerance CPU-timing-oracle path) for a benefit (audio sample timing) that only the SDL3
   frontend consumes; (b) a headless (non-SDL3) build has no use for PSG generator state at all —
   wiring it unconditionally would be pure dead weight with non-zero regression risk; (c) it does
   not match `src/CLAUDE.md`'s own explicit boundary ("`src/frontend/` — SDL3 application layer,
   input mapping, **audio/video presentation**" — presentation-layer sample generation belongs
   here). If a future milestone wants headless PSG audio capture without a live SDL3 session
   (e.g. paired with M27), it can call `machine.psg().advance_cycles()` from its OWN driver code
   the same way, with zero core change required — the accessor already exists.
2. **YM2413 is honestly silent, not silently pretended-correct.** The audio mix documents this
   explicitly in code comments and in the implementation report: *"YM2413/FM-PAC channels
   contribute silence — this device has register-file/channel/rhythm-decode fidelity (M17,
   backlog B3) but zero waveform synthesis (backlog E1, still OPEN); binding it to real audio
   output here would require inventing an ungrounded DSP pipeline, which this project's culture
   explicitly disallows."* This is presented to a human user (README/`--help` text, developer's
   judgment on exact wording) so nobody mistakes "the SDL3 app is silent for FM-PAC music" for a
   bug rather than a disclosed, tracked scope boundary.
3. **Sample-rate pacing.** A standard rate (44,100 Hz or 48,000 Hz, developer's choice) is used for
   the `SDL_AudioSpec` (`SDL_audio.h:405-410`: `{format, channels, freq}`), opened via
   `SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, userdata)`
   (`SDL_audio.h:2058`, confirmed present) and started via `SDL_ResumeAudioStreamDevice`
   (`SDL_audio.h:1767`); samples are pushed via `SDL_PutAudioStreamData`
   (`SDL_audio.h:1453`). Per-sample cycle delta = `kSystemClockHz / sample_rate` (≈81.2 cycles/
   sample at 44,100 Hz), computed once at startup. This produces a genuine, if intentionally
   simple, nearest-neighbor sampling of the PSG's real square-wave/noise output — a documented
   simplification (no anti-aliasing/interpolation), consistent with this project's established
   "documented simplification, not a fabricated fact" standard (mirrors the M21 renderer's own
   several disclosed simplifications).
4. **Deterministic-vs-real-time split (A-M26-8) applies to audio too:** `ctest` exercises the
   NUMERIC sample generation (call `advance_cycles()` a known number of times with a known
   register program, assert `sample()` against a hand-computed oracle — a pure, deterministic,
   `SDL_AudioStream`-independent check) — it does **not** attempt to assert anything about actual
   host audio-hardware playback, which is correctly out of `ctest`'s reach entirely (§6).

### 2.7 Design — input mapping

**Keyboard.** `SDL_KeyboardEvent::scancode` (`SDL_events.h:380`, doc: *"SDL physical key code"*)
is the correct field to key off — a **physical**, layout-independent key identity, matching this
project's own `KeyboardMatrix` design (row/column physical-matrix semantics, not a
locale-dependent character). A new lookup table (`SDL_Scancode` → `(row, column)`) is built by the
developer directly from the ALREADY-ESTABLISHED grounding this project used to build and test
`KeyboardMatrix` itself in M15 (S1985 fact-sheet §10's own cited "MSX Assembly Page (Grauw)
Keyboard matrices" primary source) — this table is genuinely new to M26 (an SDL-scancode-keyed
lookup on top of already-correct, already-tested row/column semantics), not a re-derivation of the
matrix itself. `references/openmsx-21.0/src/input/UnicodeKeymap.hh` (confirmed present this
cycle) is openMSX's own generic host-key→MSX-matrix translation mechanism and may be consulted as
an **implementation-technique** reference (how such a table is typically shaped) — never copied
verbatim (GPL isolation, per this project's standing discipline).

**Joystick/gamepad.** SDL3's `SDL_joystick.h` (confirmed present) provides device
enumeration/open/button/axis APIs; mapped into `JoystickPorts::PortState`/`set_port(index, state)`
(`joystick.h:66`) for up/down/left/right/trigger-A/trigger-B.

**PAUSE / Speed Controller / Ren-Sha Turbo (A-M26-7 — new, disclosed, first-principles PC-key
bindings, extending the M25 numeric-model design defaults to a physical PC key):**
- PAUSE button → `SDL_SCANCODE_PAUSE` (a genuine, dedicated PC keyboard key with an obviously
  matching name) calls `machine.pause_controller().press_pause_button()`.
- Speed Controller (no physical slider on a PC) → two discrete keys (developer's choice of exact
  scancodes, e.g. a bracket pair) step `set_speed_level()` up/down, clamped `[0, kMaxSpeedLevel]`.
- Ren-Sha Turbo (no physical slider on a PC) → two discrete keys similarly step
  `rensha_turbo().set_speed()` up/down, clamped `[0, 100]`.
- All three are clearly documented as first-principles UX defaults (mirrors A-M25-1/A-M25-3's own
  disclosure style), not a hardware fact, and named explicitly as revisable if this project ever
  adds a remapping layer (out of scope, §1.2).

### 2.8 Design — CLI/asset wiring

- **Cartridge loading:** reuse `sony_msx::machine::parse_cartridge_cli`
  (`src/machine/cartridge_cli.h`) — the SAME parser `src/main.cpp`'s `load_cartridges_from_args()`
  already uses (M19). The developer should factor a small, shared helper (or call the parser
  directly from the SDL3 entry point) rather than duplicating the load/error-reporting logic
  inline (guardrails: avoid duplicated script/logic).
- **BIOS asset root:** reuse `Hbf1xvMachine::set_asset_root()` (already exists, M13) — a
  `--bios-dir <path>` flag, mirroring the existing headless conventions.
- **Disk loading (A-M26-6):** a new `--disk <path>` flag reads the file's bytes, constructs
  `devices::fdc::DiskImage(bytes)`, and assigns it via the existing public accessors — a small,
  low-risk, purely additive CLI task, genuinely unblocked by anything already built. The developer
  may, at their discretion, symmetrically add the same flag to the existing headless
  `sony_msx_headless` executable (`src/main.cpp`) for consistency — not a hard requirement of this
  milestone (the milestone's graded deliverables are the SDL3 app itself), but a low-risk,
  well-grounded, opportunistic fix directly serving `CLAUDE.md`'s explicit "load cartridges, load
  disks... in headless or SDL3" mandate.

---

## 3. Milestones — Slice Plan (M26-S1 … S8)

Given this is the first, largest-scope, most architecturally novel frontend milestone to date, the
coordinator explicitly invited more slices than M25's 5. Every slice runs the full evidence-gate
set (§4 Acceptance Criterion 13) and leaves the **headless** (`-DSONY_MSX_ENABLE_SDL3=OFF`) `ctest`
suite green across the entire M1-M25 suite (130 tests) at every step — the headless configuration
is untouched by any of `src/frontend/`'s new files (they are compiled ONLY under `ON`).

### M26-S1 — Machine-core seam: `on_vsync_boundary()` (behavior-preserving extraction)

- Additive edit to `src/machine/hbf1xv_machine.{h,cpp}` per §2.3: extract `run_frame()`'s existing
  4-line body into the new public `on_vsync_boundary()`, call it from the unchanged
  `run_frame()`. Zero new behavior, zero new state.
- New/updated `tests/integration/machine/hbf1xv_m26_vsync_boundary_integration_test.cpp`:
  (a) REGRESSION GUARD — `run_frame()`'s observable behavior (scheduler tick, `frame_count()`,
  `vdp().irq_active()` semantics, `cycles_since_last_vsync()`) is byte-for-byte identical to
  pre-M26 for every representative existing scenario; (b) a NEW scenario drives the CPU purely via
  `step_cpu_instruction()` to a `frame_cycles_per_frame()` boundary, calls
  `on_vsync_boundary()` directly (never `run_frame()`), and asserts `frame_count()`/
  `cycles_since_last_vsync()`/VDP-vsync-driven interrupt behavior match what `run_frame()` would
  have produced for an equivalent cycle count.
- **Gates:** build + `ctest` green, headless config (130 prior + new).

### M26-S2 — SDL3 application skeleton (windowing + real-time loop, no content yet)

- New `src/frontend/sdl3_app.{h,cpp}`: `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)`, window +
  renderer creation, `run_one_frame()` (the deterministic core step, §2.3 — CPU sub-loop to the
  frame boundary + `on_vsync_boundary()`; no video/audio/input content wired yet, this slice
  proves the loop structure and pacing separation only), `run_interactive()` (the real-time outer
  loop: `SDL_PollEvent` for `SDL_EVENT_QUIT` + `SDL_Delay`-based pacing toward ~16.69 ms/frame),
  clean `SDL_Quit()` shutdown.
- New `src/frontend/sdl3_main.cpp` (replaces the 4-line stub): argv parsing scaffold (bios-dir,
  cartridge flags reused per §2.8, a `--max-frames N` bounded-run flag for headless/test use),
  constructs `Sdl3App`, calls `run_interactive()` (or a bounded loop when `--max-frames` is set).
- New CMake wiring: `tests/CMakeLists.txt` gains its first `if(SONY_MSX_ENABLE_SDL3)` block,
  linking new test executables against `sony_msx_core` + `SDL3::SDL3`.
- New `tests/integration/frontend/sdl3_app_headless_integration_test.cpp` (built ONLY under
  `SONY_MSX_ENABLE_SDL3=ON`): sets `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy` via
  `setenv`/`_putenv_s` (platform-appropriately) BEFORE constructing `Sdl3App`; asserts
  `SDL_Init`/window/renderer creation succeeds; calls `run_one_frame()` a bounded, fixed number of
  times; asserts `machine.elapsed_cycles()`/`frame_count()` advance by the exact expected amounts
  (a deterministic, hand-computable oracle — `N * frame_cycles_per_frame()`); asserts clean
  shutdown leaves no dangling SDL resources (`SDL_Quit()` returns cleanly / no leaked handles per
  whatever introspection SDL3 exposes, developer's call on exact assertion depth).
- **Gates:** headless (`OFF`) `ctest` unaffected (`src/frontend/` not compiled); a SECOND,
  documented build+test pass with `-DSONY_MSX_ENABLE_SDL3=ON` (prerequisite: SDL3 discoverable,
  A-M26-1) shows the new SDL3-gated tests green under the dummy drivers.

### M26-S3 — Video presentation

- New `src/frontend/sdl3_video_presenter.{h,cpp}`: owns the `SDL_Texture*` (created with
  `SDL_PIXELFORMAT_XRGB1555`, per A-M26-3's exact-match finding — zero conversion), recreates it
  when `FrameBuffer::width()`/`height()` change (mode switch), blits via `SDL_UpdateTexture` +
  `SDL_RenderTexture` + `SDL_RenderPresent` once per `run_one_frame()` call. Always requests
  `Field::Progressive` (§1.2).
- `Sdl3App::run_one_frame()` extended to call `machine.render_frame()` and hand it to the
  presenter.
- New test (SDL3-gated, dummy video driver): renders a KNOWN synthetic VDP scene (reusing the
  existing M21/M22 driver-program technique), calls the presenter's blit path, and asserts (via
  `SDL_RenderReadPixels` or an equivalent SDL3 introspection call, developer's choice) that the
  PRESENTED pixel data matches the `FrameBuffer`'s own values bit-for-bit — proving the "zero
  conversion" claim (A-M26-3) is genuinely true, not merely plausible.
- **Gates:** headless unaffected; SDL3-ON `ctest` green.

### M26-S4 — Decoded-FrameBuffer → PNG capture (the ONE new debug capability)

- New `src/machine/frame_dump.{h,cpp}` + additive `Hbf1xvMachine::serialize_frame_dump()`/
  `write_frame_dump()` per §2.5.
- New `tools/frame-to-png.py` (+ optional shared `tools/png_encode_common.py`) per §2.5.
- New `tests/unit/machine/frame_dump_unit_test.cpp` (headless-buildable — this capability does
  NOT depend on SDL3 at all, per the coordinator's own framing; register it OUTSIDE the
  `SONY_MSX_ENABLE_SDL3` guard so it is exercised by the DEFAULT headless evidence gate too):
  deterministic header/content assertions for a known synthetic `FrameBuffer` against a
  hand-computed oracle; a real round-trip (`write_frame_dump` → re-parse → byte-identical pixel
  recovery).
- At least one real, committed example PNG under `debug/frames/` (hard acceptance criterion),
  produced via the documented pipeline (§2.5 point 3) and referenced by path in the implementation
  report.
- **Gates:** headless `ctest` green WITH the new test included (130 prior + new); Python
  `--self-check` hermetic pass documented as evidence.

### M26-S5 — Audio output

- New `src/frontend/sdl3_audio_presenter.{h,cpp}` per §2.6: `SDL_AudioSpec` setup,
  `SDL_OpenAudioDeviceStream`, the per-sample `advance_cycles()`+`sample()` pump, PSG-only real
  synthesis, disclosed YM2413 silence.
- `Sdl3App` wires the presenter into `run_one_frame()`'s sample-rate-paced tick (independent of
  the CPU/frame cadence — audio samples occur far more often than once per video frame).
- New test (headless-buildable, does NOT need a real audio device — pure numeric assertion):
  drive a known PSG register program (e.g. set a known tone period + volume), call
  `advance_cycles()`/`sample()` the expected number of times, assert the resulting amplitude
  sequence against a hand-computed square-wave oracle (mirrors the deterministic-oracle discipline
  `tests/CLAUDE.md` requires for every new integration/system test). A SEPARATE SDL3-gated test
  confirms `SDL_OpenAudioDeviceStream` succeeds and accepts pushed data under `SDL_AUDIO_DRIVER=
  dummy` (proving the SDL3 audio PATH works headlessly, not the acoustic content).
- **Gates:** headless `ctest` green (the numeric PSG oracle test, if it needs no SDL3 types, may
  live outside the SDL3 guard — developer's call on exact placement); SDL3-ON `ctest` green.

### M26-S6 — Input mapping

- New `src/frontend/sdl3_input_mapper.{h,cpp}` per §2.7: the `SDL_Scancode`→`(row,col)` table,
  joystick/gamepad → `JoystickPorts` mapping, PAUSE/Speed-Controller/Ren-Sha-Turbo key bindings.
- `Sdl3App::run_one_frame()`'s event-poll step calls the mapper for every dequeued
  `SDL_EVENT_KEY_DOWN`/`SDL_EVENT_KEY_UP`/joystick event.
- New tests (SDL3-gated, dummy video driver, using the real `SDL_PushEvent` injection technique
  per §2.4): inject a synthetic key-down for a specific mapped scancode, run one poll+dispatch
  cycle, assert `machine.keyboard().key(row, col)` is now pressed at the EXACT expected coordinate
  (a deterministic, table-driven assertion — every mapped key gets a case, mirroring this
  project's exhaustive-coverage discipline for register/bit-mapping tests elsewhere); inject
  key-up, assert release; inject the PAUSE scancode, assert
  `machine.pause_controller().button_engaged()` toggled; a REGRESSION-GUARD case confirms an
  UNMAPPED scancode produces zero observable matrix change.
- **Gates:** headless unaffected; SDL3-ON `ctest` green.

### M26-S7 — CLI + asset/session wiring

- `--disk <path>` (A-M26-6), `--bios-dir`, reused cartridge flags (§2.8), wired into
  `sdl3_main.cpp`'s argv parsing.
- New test (SDL3-gated or a plain unit test if the parsing logic is factored SDL3-independent,
  developer's call): parses a representative flag combination, asserts the resulting
  `Hbf1xvMachine` state (asset root, loaded cartridge, loaded disk image) matches expectations;
  a negative case (bad path) produces the SAME loud, non-zero-exit failure discipline the M19
  cartridge CLI already established (`src/main.cpp`'s `load_cartridges_from_args` precedent) —
  not a silent fallback.
- **Gates:** headless unaffected (unless the developer symmetrically extends `src/main.cpp`, in
  which case the EXISTING headless suite must remain green plus any new coverage); SDL3-ON `ctest`
  green.

### M26-S8 — Dedicated system integration test + A/B evidence plan + documentation/backlog closure

- New `tests/system/hbf1xv_m26_sdl3_system_test.cpp` (SDL3-gated, dummy drivers): the milestone's
  "dedicated system integration test" — combines ALL of windowing/pacing-loop-structure,
  video-texture blit, audio-stream numeric pump, input-mapping (`SDL_PushEvent`-injected), PAUSE/
  Speed-Controller/Ren-Sha-Turbo bindings, and cartridge/disk CLI loading in ONE deterministic,
  bounded-frame scenario — mirroring the M25 `hbf1xv_m25_speed_pause_rensha_system_test.cpp`
  precedent's "everything together" structure, scaled to this milestone's larger surface.
- `docs/m26-implementation-report.md`: full slice-by-slice summary.
- `docs/m26-frontend-evidence.md` (or a section of the implementation report, developer's call):
  the honest A/B disposition (§4 Acceptance Criterion 10 — mostly BLOCKED/N-A, reasoned below) and
  the explicit human-observed-verification checklist (§6).
- Full 34-row backlog review written into `agent-protocol/state/deferred-backlog.md` (§3.9's
  disposition; C9 → DONE (M26); a cross-reference note added to E1's own row, per §2.6, without
  altering E1's substantive OPEN status).
- **Gates:** all of the above green; the FULL headless M1-M25 suite (130 tests) re-confirmed green
  one final time; the FULL SDL3-ON suite green; QA sign-off required before closure.

### 3.9 Full Deferred-Backlog Review (all 34 rows re-affirmed)

Per DEC-0005, every planner package must consult `agent-protocol/state/deferred-backlog.md` in
full (read in full this cycle, 447 lines) and restate every row.

**Section A (human-directive-tracked, 2026-07-06):**
- **B1** PSG/YM2149 internals — DONE (M15), re-affirmed unchanged in REGISTER/GENERATOR-LOGIC
  terms; M26 is the FIRST milestone to actually DRIVE `advance_cycles()` off real elapsed time
  (§2.6), but does so from `src/frontend/` only — zero edit to `psg_ym2149.{h,cpp}` itself.
- **B2** RTC/RP5C01 internals — DONE (M15), re-affirmed unchanged; M26 touches no RTC device.
- **B3** FM-PAC/YM2413 device — DONE (M17), re-affirmed unchanged; M26 touches no `ym2413_opll.*`
  file (§2.6 — real audio binds to PSG only, YM2413 stays register-contract-only as M17 shipped
  it).
- **B4** MSX-JE 16 KB SRAM — DONE (M20), re-affirmed unchanged.
- **B5** Kanji font ROM I/O — DONE (M18), re-affirmed unchanged.
- **B6** Halnote/MSX-JE firmware mapping — DONE (M20), re-affirmed unchanged.
- **B7** Cartridge loading — DONE (M19), re-affirmed unchanged; M26 REUSES (not reimplements) the
  existing `cartridge_cli` parser for the new SDL3 executable (§2.8).
- **B8** FDC drive mechanics — DONE (M16), re-affirmed unchanged; M26 adds real disk-IMAGE LOADING
  via the CLI (A-M26-6), using the existing, unmodified `DiskImage(bytes)` constructor — zero edit
  to any FDC device file.
- **B9** VRAM/V9958 VDP contract — DONE (M14), re-affirmed unchanged; M26 touches no VDP device
  file (only consumes the existing M21 `render_frame()`/`FrameBuffer` output and calls the
  already-public `vdp_.on_vsync()` via the new `on_vsync_boundary()` extraction, S1).

**Section B (other known deferrals):**
- **C1** Exact cycle/T-state timing parity — re-affirmed **IN-PROGRESS (M23 partial)**, unchanged;
  M26 adds no CPU-timing-affecting code inside `Z80aCpu` or the VDP-access-timing foundation — the
  real-time loop is a new EXTERNAL CONSUMER of `step_cpu_instruction()`'s existing contract, never
  a change to it.
- **C2** Z80 HALT-R increment — DONE (M23), re-affirmed unchanged; M26 touches no CPU-core file.
- **C3** ZEXDOC/ZEXALL full parity sweep — DONE (M24), re-affirmed unchanged; M26 touches no
  CPU-core file, so the M24 sweep's own findings remain valid; the slow system test itself must
  still pass as part of the headless full-suite regression gate (§4 Acceptance Criterion 6).
- **C4** S1985 backup-RAM `.sram` persistence — DONE (M15), re-affirmed unchanged.
- **C5** Full boot past first device read — re-affirmed **IN-PROGRESS (M16 partial)**, unchanged;
  unrelated to M26 (no boot-sequence code touched); NOTE for a future C5 milestone: the SDL3
  frontend's new `--disk` flag (A-M26-6) is a genuinely useful NEW tool for whoever eventually
  investigates the real auto-disk-boot trigger interactively, but M26 itself does not attempt
  that investigation.
- **C6** Peripherals (keyboard/joystick) — DONE (M15), re-affirmed unchanged; M26 ADDS a NEW
  CONSUMER (the SDL3 input mapper, §2.7) calling the EXISTING, unmodified `set_key()`/`set_port()`
  APIs — zero edit to `keyboard_matrix.*`/`joystick.*` themselves.
- **C7** Printer + cassette — DONE (M18), re-affirmed unchanged; M26 touches neither device (no
  printer/cassette SDL3 binding is in scope this cycle — not named in the REQ-M26-001 scope, and
  not added speculatively).
- **C8** Sony speed-controller/PAUSE + Ren-Sha Turbo — DONE (M25), re-affirmed unchanged in
  INTERNAL MODEL terms; M26 is the FIRST milestone to bind these APIs to a real physical input
  (§2.7) — zero edit to `mb670836_pause.*`/`rensha_turbo.*` themselves.
- **C9** SDL3 frontend — **THIS MILESTONE.** Target: closes IN FULL (§4). E1's own row (below) gets
  a cross-reference note only, not a status change.
- **C10** FDC flux-level/DMK disk fidelity — re-affirmed OPEN; unrelated to this milestone (M26's
  new `--disk` flag loads sector-level `.dsk` images only, matching M16's existing scope exactly —
  no flux-level fidelity implied or added).

**Section C (M14 VDP-depth deferrals):**
- **D1** Pixel-accurate raster rendering pipeline — DONE (M21), re-affirmed unchanged; M26 is
  purely a NEW CONSUMER of `render_frame()`'s existing output.
- **D2** Sprite rendering + collision — DONE (M22), re-affirmed unchanged.
- **D3** VDP command engine — DONE (M22), re-affirmed unchanged.
- **D4** Cycle-accurate VDP access-slot/command timing — re-affirmed **IN-PROGRESS (M23 partial)**,
  unchanged; M26 touches no VDP-timing file; interlaced-field real-time presentation is explicitly
  deferred alongside this row (§1.2), not attempted this cycle.
- **D5** YJK/YJK+YAE color decode — DONE (M21), re-affirmed unchanged.
- **D6** Scroll/interlace/blink/superimpose — DONE (M21), re-affirmed unchanged.
- **D7** G6/G7 planar interleave — DONE (M22, closed in full), re-affirmed unchanged.

**Section D (M17 YM2413 depth deferrals):**
- **E1** YM2413 FM-synthesis DSP/audio depth — re-affirmed **OPEN**, unchanged in substance;
  §2.6's finding CONFIRMS this row's own candidate-owner note ("paired with C9") was prescient —
  M26 deliberately does NOT attempt to close it (no DSP pipeline is grounded/built this cycle);
  a documentation cross-reference to `docs/m26-planner-package.md` §2.6 may be added to this row
  at closure time (developer/coordinator judgment), without altering its OPEN status or numeric
  content.
- **E2** YM2413 register-write timing constraint — re-affirmed OPEN; unrelated to this milestone
  (M26 adds no register-write traffic pattern of its own beyond what a real user's own key
  presses/game software would already generate through the existing, unmodified device).

**Section E (M18 printer/cassette depth deferrals):**
- **F1** Cassette tape image-format/signal fidelity — re-affirmed OPEN; unrelated to this
  milestone (no cassette SDL3 binding in scope, §1.2's implicit boundary — not named in
  REQ-M26-001).
- **F2** Printer image/ESC-sequence rendering depth — re-affirmed OPEN; unrelated to this
  milestone.

**Section F (M19 cartridge-mapper depth deferrals):**
- **G1** KonamiSCC + embedded SCC chip — re-affirmed OPEN; unrelated to this milestone (M26's
  audio scope is PSG-only, §2.6; a future KonamiSCC audio device would face the identical
  "does it have a real `sample()` method" question this package raised for YM2413).
- **G2** ROM-database/heuristic mapper auto-detection — re-affirmed OPEN; unrelated to this
  milestone.
- **G3** Full runtime cartridge hot-plug — re-affirmed OPEN; unrelated to this milestone (M26's
  cartridge/disk loading is load-at-startup only, via CLI flags, matching every prior milestone's
  fixed-at-construction-or-cold-boot composition model — not live insert/eject during a running
  SDL3 session).
- **G4** Long tail of ~80 other mapper types — re-affirmed OPEN; unrelated to this milestone.

No new backlog rows are proposed this cycle. C9 is the only row this milestone closes.

---

## 4. Acceptance Criteria

1. **A genuine, working SDL3 application** (`sony_msx_sdl3`) that builds under
   `-DSONY_MSX_ENABLE_SDL3=ON` (an SDL3 package providing `SDL3Config.cmake` must be discoverable —
   a pre-existing project prerequisite, A-M26-1) and, when launched normally on a real machine,
   opens a real window, presents real decoded video, plays real PSG audio, and responds to real
   keyboard/joystick input.
2. **Video presentation is genuinely correct, not merely "compiles"**: the S3 test proves the
   PRESENTED pixel data matches `FrameBuffer`'s own values bit-for-bit (A-M26-3's "zero
   conversion" claim independently verified, not assumed).
3. **Audio output is genuinely real for what exists, and honestly silent for what doesn't**: PSG
   samples are driven by the actual `advance_cycles()`/`sample()` machinery against a real,
   deterministic register program (not a synthetic sine wave standing in for it); YM2413 is
   explicitly, visibly disclosed as silent (not faked), with the exact reasoning (§2.6) recorded
   in code comments and the implementation report.
4. **Input mapping is exhaustively, deterministically verified**: every mapped `SDL_Scancode`
   (keyboard) and joystick control has a dedicated test asserting the EXACT expected
   `KeyboardMatrix`/`JoystickPorts` effect via real `SDL_PushEvent` injection (§2.4); an unmapped
   scancode produces zero observable effect (regression guard).
5. **The decoded-`FrameBuffer`→PNG capture capability is real, deterministic, and evidenced with a
   genuine committed example**: at least one real PNG under `debug/frames/`, produced via the
   documented pipeline (§2.5), referenced by path in the implementation report — not merely
   described.
6. **Zero regression on the headless (`-DSONY_MSX_ENABLE_SDL3=OFF`) configuration**: the FULL
   M1-M25 suite (130 tests, including the ~24-27-minute `hbf1xv_m24_zexall_system_test`) passes
   100%, independently reproduced by the developer and QA from clean rebuilds; `git diff` against
   `v1.0.25` shows NO change to any `src/devices/`, `src/peripherals/`, or `src/core/` file's
   BEHAVIOR (the sole exception being the S1 `on_vsync_boundary()` extraction in
   `src/machine/hbf1xv_machine.{h,cpp}`, confirmed behavior-preserving per §2.3/A-M26-5).
7. **`ctest` runs entirely headlessly under `-DSONY_MSX_ENABLE_SDL3=ON` too** — every new SDL3-
   gated test succeeds under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy` (A-M26-2), with NO
   real display/audio hardware required anywhere in the automated suite.
8. **The `ctest`-gated vs. human-observed-only evidence categories are explicitly, honestly
   separated** (§2.4's table, §6) — no claim of "verified" for anything only a human eye/ear could
   actually confirm (a real picture looking correct, real audio sounding correct, a real keyboard
   feeling responsive).
9. **Every named M9/M12/M23 zero-tolerance CPU-timing-oracle test file is confirmed byte-for-byte
   UNCHANGED** via direct `git diff`, independently re-confirmed by both the developer and QA.
10. **The A/B evidence disposition is honest, not fabricated or silently skipped**: this
    milestone's own §2.4 finding stands — this project's established openMSX Tcl-based A/B
    technique compares EMULATOR STATE (registers/VRAM/memory), not SDL rendering/audio-hardware
    output of either engine, and openMSX's own SDL layer is not remotely comparable via this
    project's existing harness. The genuinely checkable sub-claim (the input-mapping TABLE's
    row/column correctness) is ALREADY fully covered by the M15 `KeyboardMatrix`/`JoystickPorts`
    contract tests plus M26's own exhaustive `SDL_PushEvent`-injection tests (Acceptance Criterion
    4) — no NEW cross-engine A/B technique is invented or claimed for the SDL3-specific surface
    (video/audio/input-device plumbing itself), and this is reported as an honest, reasoned
    BLOCKED/N-A disposition in `docs/m26-implementation-report.md`, mirroring the established
    M21-computed-pixel-color / M25-PAUSE-Speed-Controller precedent of a disclosed, non-fabricated
    BLOCKED sub-claim that does not gate an otherwise-complete milestone's closure.
11. **The dedicated system integration test** (S8) exercises windowing/pacing, video, audio,
    input, PAUSE/Speed-Controller/Ren-Sha-Turbo bindings, and CLI/asset loading together in one
    deterministic, bounded-frame scenario.
12. **Full 34-row deferred-backlog review** completed and written into `deferred-backlog.md`
    (§3.9) — C9 dispositioned DONE (M26); E1 re-affirmed OPEN with an honest cross-reference note;
    all other 32 rows re-affirmed with a one-line justification, no silent drift.
13. **Evidence gates executed and captured**: `tools/validate-assets.ps1`;
    `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; `cmake --build build --config
    Debug` (headless AND SDL3-ON configurations, each documented separately); `ctest --test-dir
    build -C Debug --output-on-failure` (both configurations; headless full suite including the
    slow zexall system test at least once); `python tools/frame-to-png.py ... --self-check`.
14. **M27's scope is named only as a brief forward-looking note** (§7), not designed in detail —
    matching the coordinator's explicit instruction.

---

## 5. Risks (each with a verification action)

- **R-M26-1 (this is the first milestone touching real OS-level window/audio-device APIs — a
  categorically higher-uncertainty surface than any prior device milestone).** *Verification:*
  A-M26-1/A-M26-2's grounded, source-verified mechanics reduce guesswork; the developer must still
  empirically confirm `-DSONY_MSX_ENABLE_SDL3=ON` actually configures/builds/runs under the
  dummy drivers in the REAL target environment before claiming success (guardrails: never claim an
  unexecuted build/test result) — if SDL3 is genuinely unavailable in this environment, that is a
  real blocker to report honestly to the human, not to fabricate around.
- **R-M26-2 (real-time wall-clock pacing is inherently non-reproducible across host machines/
  runs — a genuine, new-to-this-project category of "non-determinism" that must not be either
  hidden or allowed to leak into the project's determinism claims for the underlying core).**
  *Verification:* A-M26-8/§2.3's structural separation (`run_one_frame()` vs.
  `run_interactive()`) is a hard, non-negotiable architecture requirement (§4 Acceptance Criterion
  8), not a nice-to-have — QA should specifically verify `ctest` never calls `SDL_Delay`-based
  pacing anywhere in its own execution path.
- **R-M26-3 (the new `on_vsync_boundary()` extraction touches `src/machine/hbf1xv_machine.cpp`,
  which every M1-M25 device ultimately wires through — the single highest-blast-radius file in
  the project).** *Verification:* §2.3's design is DELIBERATELY a pure, mechanical,
  behavior-preserving extraction (same 4 operations, same order) — the developer must confirm via
  `git diff` that `run_frame()`'s own observable behavior is unchanged for every existing caller,
  and QA independently re-derives this by inspection, mirroring the M23-S2 "additive,
  non-perturbing bookkeeping-only" precedent's own review rigor.
- **R-M26-4 (the PSG-generator-advance wiring, even though frontend-only, is still a genuinely
  NEW real-time-driven consumer of a device that has NEVER been driven by real elapsed cycles
  before — an untested-in-anger code path, even though the underlying `advance_cycles()`/
  `sample()` functions themselves are already unit-tested in isolation since M15).**
  *Verification:* §3's S5 numeric-oracle test (a known register program, hand-computed expected
  amplitude sequence) is a hard, non-negotiable gate proving the WIRING is correct, not just that
  the underlying functions individually work.
- **R-M26-5 (temptation to fabricate or approximate YM2413/FM-PAC audio rather than honestly
  disclosing silence — the single highest scope-integrity risk this milestone carries, given the
  Target Machine Specification's own SOUND field names "FM-PAC (OPLL YM-2413) – 9-channel FM
  synthesizer" as the headline sound source).** *Verification:* §2.6/§4 Acceptance Criterion 3 are
  explicit, hard gates — the developer must NOT add any ad hoc waveform/envelope approximation
  for YM2413 channels; QA should specifically check for this (grep for any new
  synthesis-shaped code touching `ym2413_opll.*` or a parallel YM2413-audio file, which should not
  exist).
- **R-M26-6 (SDL3 API-signature/behavior drift from SDL2-era assumptions — already caught once
  concretely this cycle, A-M26-2's env-var-rename finding, but other similar traps may exist
  elsewhere in the API surface this milestone touches, e.g. audio stream semantics, texture
  access modes, event struct field names).** *Verification:* the developer must read the ACTUAL
  vendored `references/sdl3/include/SDL3/*.h` signatures for every SDL3 call site added, not
  transcribe from memory/tutorials/SDL2 experience — mirroring this package's own §2's discipline
  throughout.
- **R-M26-7 (the `--disk`/A-M26-6 addition, while low-risk, is new CLI surface area not explicitly
  named word-for-word in REQ-M26-001's scope text — a mild scope-boundary judgment call this
  planner made, grounded in CLAUDE.md's own explicit "load cartridges, load disks" joint mandate).**
  *Verification:* if the coordinator judges this addition out of M26's intended scope on review,
  it can be trivially deferred to M27 or a later milestone with zero disruption to the rest of the
  plan (it is a small, self-contained, purely additive slice, S7).
- **R-M26-8 (scope-creep risk generally: this is the first frontend milestone, and the temptation
  to over-build — a full remapping UI, a full audio mixer with EQ, multi-window support, etc. — is
  real).** *Verification:* §1.2's explicit out-of-scope table is a hard boundary; the developer
  does not add any of the named out-of-scope items this cycle.

---

## 6. Developer Handoff

**Build order:** M26-S1 (`on_vsync_boundary()` — do this FIRST and run the FULL M9/M12/M23
CPU-timing-oracle `git diff` check immediately after, before proceeding, mirroring the M25-S4
precedent for the highest-blast-radius slice) → M26-S2 (app skeleton + the first SDL3-ON
`ctest` proof-of-headless-viability — do this EARLY so any real environment blocker (A-M26-1) is
discovered before the rest of the plan is built on top of it) → M26-S3 (video) → M26-S4 (PNG
capture — headless-buildable, can proceed in parallel with S3 if useful) → M26-S5 (audio) → M26-S6
(input) → M26-S7 (CLI/assets) → M26-S8 (system test + evidence + backlog closure).

**Hard constraints (do not deviate without a `decisions.md` entry):**
- Do NOT touch `src/devices/cpu/*` — the real-time loop is a new CONSUMER of
  `step_cpu_instruction()`, never a change to it.
- Do NOT call `run_frame()` and `step_cpu_instruction()`/`on_vsync_boundary()` in the same session
  or the same test (A-M26-5) — the double-count hazard is real and already burned this project
  once at the test level (R-M25-5).
- Do NOT add ANY waveform/DSP synthesis for YM2413 — disclose silence honestly instead (§2.6,
  R-M26-5).
- Do NOT use `SDL_Delay`/real-time pacing anywhere inside `ctest`'s own execution path — every
  SDL3-gated test drives `run_one_frame()` directly, a bounded number of times (A-M26-8).
- Do NOT transcribe SDL3 API signatures from memory/SDL2 experience — read the actual vendored
  header for every new call site (R-M26-6).
- Do NOT add a keybinding/remapping configuration system, multi-window support, or any other
  named out-of-scope item (§1.2).
- Do NOT fabricate an A/B PASS for anything this milestone cannot actually cross-check — report
  the honest BLOCKED/N-A disposition per §4 Acceptance Criterion 10.
- Run the FULL M1-M25 headless suite (130 tests, including the slow zexall system test at least
  once before requesting QA) AND the full SDL3-ON suite, not a subset of either.

**Files to create (indicative, developer's judgment on exact shape per `src/CLAUDE.md`):**
`src/frontend/sdl3_app.h`/`.cpp`, `src/frontend/sdl3_video_presenter.h`/`.cpp`,
`src/frontend/sdl3_audio_presenter.h`/`.cpp`, `src/frontend/sdl3_input_mapper.h`/`.cpp`,
`src/machine/frame_dump.h`/`.cpp`, `tools/frame-to-png.py` (+ optional
`tools/png_encode_common.py`), `tests/integration/machine/
hbf1xv_m26_vsync_boundary_integration_test.cpp`, `tests/integration/frontend/
sdl3_app_headless_integration_test.cpp` (+ per-slice SDL3-gated tests for S3/S5/S6/S7),
`tests/unit/machine/frame_dump_unit_test.cpp`, `tests/system/hbf1xv_m26_sdl3_system_test.cpp`,
at least one committed PNG under `debug/frames/`, `docs/m26-implementation-report.md`,
`docs/m26-frontend-evidence.md` (or folded into the implementation report).

**Files to edit (additive only):** `src/frontend/sdl3_main.cpp` (full rewrite of the 4-line
stub — this IS the point of the milestone, not a violation of "additive only" in spirit),
`src/machine/hbf1xv_machine.h`/`.cpp` (the S1 `on_vsync_boundary()` extraction — the ONLY
core-touching edit), `tests/CMakeLists.txt` (new `if(SONY_MSX_ENABLE_SDL3)` block),
`agent-protocol/state/deferred-backlog.md` (§3.9 dispositions, C9 → DONE (M26)),
`agent-protocol/state/milestones.md`, `agent-protocol/state/definition-of-done.yaml`,
`agent-protocol/state/current-phase.md`.

**Evidence to capture before requesting QA:** full headless `ctest` output (130 prior + new S1/S4
tests, 0 failed); full SDL3-ON `ctest` output (all new SDL3-gated tests, 0 failed, explicitly
confirmed run under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`); the `git diff v1.0.25`
confirmation that no CPU/VDP/audio/RTC/FDC/cartridge/memory/halnote/kanji/core file changed
behaviorally (only the disclosed `on_vsync_boundary()` extraction); the committed example PNG's
path and the `tools/frame-to-png.py --self-check` output; the PSG numeric-oracle test's
hand-computed values cross-checked against actual test output; the honest A/B/human-observed
evidence split (§2.4's table, §6 below); the four asset/build/test evidence-gate script outputs
for BOTH build configurations.

### The explicit, honest evidence-category split (mirrors §2.4's table)

**`ctest`-gated (automated, reproducible, required before QA sign-off):** app init under dummy
drivers; bounded-frame deterministic machine-state advancement; input-mapping table correctness
via `SDL_PushEvent` injection; PSG numeric sample-generation correctness; PNG-capture format
correctness; the dedicated system test combining all of the above.

**Human-observed only (NOT `ctest`-gated, NOT claimed as "verified" by any automated evidence —
a separate, disclosed check a human should perform on a real machine before treating the SDL3
frontend as genuinely production-usable):** does a real window actually appear and show a
correctly-colored, non-garbled picture; does real audio actually play through real speakers/
headphones and sound like plausible PSG chiptune audio (not silence, not noise); does a real
keyboard/joystick actually feel responsive with no perceptible input lag; does the PAUSE key/
Speed-Controller/Ren-Sha-Turbo keybindings feel usable in practice. This category is named
explicitly, not hidden, mirroring this project's established discipline of never fabricating
evidence for something that genuinely cannot be checked by an agent in this environment.

---

## 7. Forward-looking note — M27 "Production Hardening + Debug/Test Tooling" (named only, not
designed here, per the coordinator's explicit instruction)

M27 is expected to absorb, whole-cloth, everything the human's original debug/-tooling request
named beyond M26's one authorized PNG-capture item (§1.2's out-of-scope table): audio capture to
file, full CPU/memory/VRAM state-dump CLI wiring (the M10-S3 API has never had a CLI flag),
keystroke-sequencing/scripted-input automation, and broader production-readiness testing
(performance/timing validation under real wall-clock constraints, stress testing, cross-platform
build validation, replay-determinism validation via the debug event log). M26's own new
`Sdl3App::run_one_frame()`/`run_interactive()` split (§2.3) and the new `--disk`/`--bios-dir`/
cartridge CLI surface (§2.8) are natural building blocks a future M27 scripted-input/automation
layer could build on top of — noted here for forward visibility only, not designed.
