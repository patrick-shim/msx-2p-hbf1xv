# M26 Frontend Evidence — openMSX A/B Disposition + Human-Observed Verification Checklist

- Milestone ID: M26
- Companion to: `docs/m26-implementation-report.md`
- Purpose: an honest, explicit accounting of what genuinely CAN and CANNOT be cross-checked
  against real openMSX for this milestone's SDL3-specific surface (video/audio/input-device
  plumbing), per `docs/m26-planner-package.md` §4 Acceptance Criterion 10, plus the explicit,
  disclosed `ctest`-gated vs. human-observed-only evidence split named in §2.4/§6 of that same
  package.

---

## 1. Why this milestone's A/B disposition is different from every prior device milestone

Every M11-M25 openMSX A/B check compared **emulator architectural STATE** — CPU registers, VRAM
bytes, palette entries, I/O-port register files — read back via openMSX's own Tcl `debug`/`reg`
introspection commands. That technique is well-proven and genuinely comparable because both
engines expose the SAME kind of introspection point: raw device state.

M26's new surface (a real SDL3 window, a real audio device stream, real keyboard/joystick event
handling) has **no equivalent introspection point in either engine**:

- openMSX's own SDL rendering output (actual pixels drawn to an actual window) is not exposed via
  any Tcl `debug` command this project's existing harness (or any prior milestone's) has ever
  used or found. There is no "read the presented window's pixels" Tcl primitive.
- openMSX's own audio-hardware output (actual samples sent to an actual sound device) is likewise
  not comparable via this project's Tcl-based harness — there is no "read the audio device's
  output buffer" primitive either.
- SDL3-level plumbing itself (window creation, texture upload, audio-stream lifecycle,
  `SDL_PushEvent`/`SDL_PollEvent` dispatch) is this project's OWN new frontend code, not a
  cross-engine-comparable emulator behavior at all — there is nothing in openMSX to diff it
  against, by definition (it is a *presentation-layer* concern, not a *device-behavior* concern).

This is the SAME class of honest limitation this project has already disclosed twice before (the
M21 "no computed-pixel-color introspection point exists in openMSX" finding, and the M25 "openMSX
genuinely does not model this Sony-specific hardware anywhere" finding for hardware PAUSE) — this
milestone's finding is a natural continuation of that established, disclosed-limitation pattern,
not a new kind of shortfall.

## 2. What genuinely CAN be cross-checked (and already is, without a new technique)

The one sub-claim within M26's scope that IS, in principle, cross-engine-comparable is the
**input-mapping TABLE's row/column correctness** — i.e., "does pressing physical key X really
correspond to MSX matrix position (row, column) Y on the real hardware." This is fully covered
by TWO existing layers, requiring no new A/B technique:

1. **The M15 `KeyboardMatrix`/`JoystickPorts` contract tests** (`tests/unit/peripherals/
   keyboard_matrix_unit_test.cpp`, `tests/unit/peripherals/joystick_unit_test.cpp`) already
   established the matrix's own row/column read/write semantics are correct (inverted encoding,
   idle=0xFF, PPI port B/C row-select wiring) — unchanged by M26 (`git diff --stat src/peripherals`
   is empty, per the implementation report §3.6).
2. **M26's own exhaustive `SDL_PushEvent`-injection tests**
   (`tests/integration/frontend/sdl3_input_mapper_integration_test.cpp`) prove, for EVERY one of
   the 71 mapped `SDL_Scancode` entries, that pressing/releasing it produces EXACTLY the documented
   `(row, column)` matrix effect — a real, deterministic, exhaustive proof of the NEW lookup table
   `Sdl3InputMapper::scancode_map()` itself.

Together, these two layers fully discharge the "is the mapping table correct" question without
needing (or fabricating) a new cross-engine technique — `Sdl3InputMapper::scancode_map()`'s
row/column assignments are independently cross-checked against this project's own
already-established ground truth (`peripherals::RenshaTurbo`'s doc comment, M25, citing "keyboard
row 8 bit 0 (SPACE)" as the real autofire attach point — matching `scancode_map()`'s own
row=8/column=0/`SDL_SCANCODE_SPACE` entry exactly, a genuine, load-bearing self-consistency check
within this project's own accumulated knowledge).

## 3. Formal disposition (per Acceptance Criterion 10)

| Sub-claim | Disposition | Reasoning |
|---|---|---|
| Video rendering pixel content, presented in a real SDL3 window | **BLOCKED (N/A)** | No openMSX introspection point for presented-window pixels exists; not attempted. |
| Audio output content, sent to a real audio device | **BLOCKED (N/A)** | No openMSX introspection point for audio-device output exists; not attempted. |
| SDL3 windowing/event/audio-stream plumbing itself | **N/A by definition** | This is new, project-own frontend code with no openMSX equivalent to diff against — not a device-behavior claim at all. |
| Input-mapping TABLE row/column correctness | **Covered without a new A/B technique** | Discharged by the existing M15 contract tests + M26's own exhaustive `SDL_PushEvent`-injection tests (§2) — a real, deterministic, non-fabricated proof. |
| PSG (YM2149) sample-generation numeric correctness | **Covered without a new A/B technique** | `PsgYm2149`'s own register/generator logic was already A/B-verified at M15 (R0/R7 empty diff vs. openMSX); M26 only adds NEW WIRING (real-time driving of the already-verified `advance_cycles()`/`sample()` functions) — proven correct via a hand-computed numeric oracle (`frontend_psg_audio_pump_unit_test`), not a new cross-engine claim. |
| YM2413/FM-PAC synthesis | **Honestly out of scope, not attempted, not faked** | Zero waveform synthesis exists in this project (backlog E1, still OPEN) — there is nothing to A/B in the first place. |

**This disposition does NOT gate M26's closure**, per Acceptance Criterion 10's own explicit
language — mirroring the established M21-computed-pixel-color and M25-PAUSE-Speed-Controller
precedent of a disclosed, non-fabricated BLOCKED sub-claim that does not block an otherwise-
complete milestone.

## 4. The `ctest`-gated vs. human-observed-only evidence split (§2.4/§6 of the planner package)

| Category | Mechanism | Gated by `ctest`? | Evidence location |
|---|---|---|---|
| App init (SDL_Init, window/renderer/audio-stream creation) succeeds | `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy` | **Yes** | `frontend_sdl3_app_headless_integration_test` |
| A bounded N-frame session advances machine state deterministically | `Sdl3App::run_one_frame()` called N times directly | **Yes** | `frontend_sdl3_app_headless_integration_test`, `hbf1xv_m26_sdl3_system_test` |
| Input-mapping table correctness | `SDL_PushEvent` synthetic injection + real dispatch | **Yes** | `frontend_sdl3_input_mapper_integration_test` (71/71 mapped keys + PAUSE/Speed/Ren-Sha bindings + regression guard) |
| PSG audio-sample generation is genuinely non-silent/deterministic | Direct `PsgAudioPump`/`PsgYm2149` numeric assertions | **Yes** | `frontend_psg_audio_pump_unit_test` (headless, no SDL3 needed) |
| Real SDL3 audio-stream path accepts pushed data under the dummy driver | `SDL_OpenAudioDeviceStream`/`SDL_PutAudioStreamData`/`SDL_GetAudioStreamQueued` | **Yes** | `frontend_sdl3_audio_presenter_integration_test` |
| Video texture blit is pixel-exact | `SDL_RenderReadPixels` readback vs. `FrameBuffer` | **Yes** | `frontend_sdl3_video_presenter_pixel_integration_test` |
| Decoded-`FrameBuffer`→PNG capture format correctness | Direct C++ dump + hermetic Python `--self-check` | **Yes** (C++ side); self-check documented as evidence (Python side) | `machine_frame_dump_unit_test`; `tools/frame-to-png.py --self-check` |
| Real cartridge/disk/BIOS CLI loading | Real committed assets (`roms/aleste.rom`, `disks/msxdos22.dsk`) | **Yes** | `frontend_sdl3_cli_session_integration_test`, `hbf1xv_m26_sdl3_system_test` |

**Human-observed-only (NOT `ctest`-gated, NOT claimed as "verified" by any automated evidence —
genuinely cannot be checked by an agent in this non-interactive environment with no real
display/audio hardware):**

- [ ] Does a real window actually appear on screen, sized correctly, and show a correctly-colored,
  non-garbled picture during real gameplay/BIOS boot (not just the synthetic test pattern used for
  the committed PNG evidence)?
- [ ] Does real audio actually play through real speakers/headphones and sound like plausible PSG
  chiptune audio (correct pitch, correct stereo A=center/B=left/C=right separation, no audible
  glitches/pops/dropouts from the sample-rate-paced batching)?
- [ ] Does a real keyboard feel responsive with no perceptible input lag, and does every key in
  `Sdl3InputMapper::scancode_map()` genuinely correspond to the intended MSX key when typed on a
  real physical keyboard (not just verified via synthetic `SDL_PushEvent` injection)?
- [ ] Does a real joystick/gamepad's digital directions and trigger buttons feel correct and
  responsive?
- [ ] Does the PAUSE key (`SDL_SCANCODE_PAUSE`) visibly freeze the displayed picture/audio when
  pressed, and resume cleanly when pressed again?
- [ ] Do the Speed-Controller (F6/F7) and Ren-Sha Turbo (F8/F9) keybindings feel usable/sensible in
  practice, at their default step size?
- [ ] Does the real-time pacing (`run_interactive()`'s `SDL_Delay`-based ~16.69 ms/frame loop) feel
  like plausible real-time speed, not obviously too fast/slow, on real target hardware?
- [ ] Does `sony_msx_sdl3.exe --help` print correctly-formatted, legible usage text in a real
  terminal?

**This checklist is explicitly named here, not hidden or silently claimed as passing** — no item
above is asserted true/false by the developer; each is a genuine open question for whichever human
next launches the built `sony_msx_sdl3.exe` executable normally (no dummy drivers) on a real
machine with a real display/audio device, per the milestone's own explicit scope framing.

## 5. What IS empirically confirmed about the real, non-dummy path (without claiming full human verification)

One piece of real-environment evidence beyond the dummy-driver `ctest` suite: `docs/m26-
implementation-report.md` §2 documents a real `sony_msx_sdl3.exe --hidden-window --max-frames 5
--bios-dir bios` invocation under the dummy drivers that opened a real SDL3 window/renderer/audio
stream and ran 5 real emulated frames to completion with exit code 0 — genuine confirmation that
the application's INITIALIZATION and FRAME-STEPPING machinery work end to end, in this real
process, on this real machine. This is NOT a substitute for the human-observed checklist above
(a hidden window under the dummy video driver proves nothing about what a REAL window/display
would visually show, and the dummy audio driver produces no real sound) — it is named here only
to be precise about exactly how far the automated/semi-automated evidence goes before the
human-observed boundary begins.
