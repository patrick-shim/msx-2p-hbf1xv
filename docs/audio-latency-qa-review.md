# QA Review — Audio Latency Targeted Defect Cycle (PSG sound 0.5-1 s late)

- Date: 2026-07-09
- Reviewer: MSX QA Agent
- Scope under review: the uncommitted working-tree changes documented in
  `docs/audio-latency-investigation.md` (diff vs HEAD `894be88`, DEC-0032).
- Precedent: coordinator-authorized targeted defect cycle (DEC-0026..DEC-0032 discipline).
- Cadence: per the standing rule, the ZEXALL/ZEXDOC slow sweep was NOT run — this cycle is
  frontend-only (verified: `git diff HEAD -- src/devices src/machine src/core` is empty).
  The two fast subsets plus real-driver verification are the regression gate.

## 1. Regression Scope

Files under review (every diff hunk read):

- `src/frontend/audio_pacer.{h,cpp}` (new, SDL-free) — exact-accounting production
  (`total_due = floor(elapsed_cycles * 44100 / 3579545)` via overflow-safe `scale_cycles`),
  67 ms backpressure cap with trim-to-reconverge, 33 ms low-water silence re-prime at
  underrun boundaries only.
- `src/frontend/sdl3_audio_presenter.{h,cpp}` — new `pump_and_push_paced()`; policy
  constants `kLowWaterSamples`=1455, `kMaxQueuedSamples`=2954, `kBytesPerSampleFrame`=4;
  legacy raw `pump_and_push()` retained for plumbing tests.
- `src/frontend/sdl3_app.{h,cpp}` — `run_one_frame()` audio production now from
  `machine_.elapsed_cycles()`; `run_interactive()` exact-ns absolute deadlines
  (16,688,154 ns/frame via `scale_cycles`, `SDL_GetTicksNS`/`SDL_DelayNS`, 100 ms backlog
  forgiveness).
- `CMakeLists.txt`, `tests/CMakeLists.txt`, new `tests/unit/frontend/audio_pacer_unit_test.cpp`,
  extended `tests/integration/frontend/sdl3_audio_presenter_integration_test.cpp`.

Affected flows: SDL3 interactive audio/video pacing only. Headless emulated state is
unaffected (verified below). No device, machine, core, peripheral, or tool changes.

## 2. Regression Matrix Status (all evidence from my own runs, 2026-07-08/09)

| Gate | Result |
| --- | --- |
| Arithmetic re-derivation (independent Python) | fps = 3,579,545/59,736 = **59.92274**; frame period = 16,688,154.500 ns -> floor **16,688,154**; samples/frame = **735.9476** (old `floor(59736/81)` = 737); `scale_cycles` split == direct product over 200,000 randomized values + edges; overflow headroom ~585 emulated years at ns scaling; low-water 1455 / cap 2954 (66.98 ms); pre-fix defect model reproduces the measured +1,307.6 samples/s to 0.06% |
| Headless rebuild (`build-qa`, from working tree) | exit 0 |
| Headless fast subset `ctest -C Debug -LE m24_slow_full_sweep` | **154/154 passed** (17.53 s), incl. new `frontend_audio_pacer_unit_test` ("All Frontend_AudioPacer_Unit cases passed") |
| SDL3-ON rebuild (`build/sdl3-on`, from working tree) | exit 0 |
| SDL3-ON fast subset (dummy video/audio drivers) | **163/163 passed** (17.36 s) |
| Extended presenter integration test on **real WASAPI** (SDL_AUDIO_DRIVER unset) | exit 0 — byte-level queue cap + exact cumulative accounting hold on a genuine device stream |
| Real-audio interactive run (temporary `QA-TEMP-PROBE`, reverted after; real WASAPI, `--hidden-window --cart1 roms/metalgear.rom --cart1-type Konami --max-frames 11000`, ~183 s) | exit 0; **59.9226 fps** measured in-loop over 10,920 frames (target 59.9227; pre-fix 61.61); queue bounded **25.8–49.4 ms, mean 37.6 ms**, cap never exceeded, no negative/error reads; residual drift **+3.85 samples/s** (host clock-domain skew, same order as developer's +3.3; absorbed by the cap) |
| Pristine-binary spot check after probe revert (660 frames, real audio, Metal Gear) | exit 0 (11.44 s wall incl. ~0.4 s process startup — consistent with 16.688 ms/frame in-loop pacing) |
| Adversarial test-oracle check (analytical) | Case 1 oracle (`numerator += 59736*44100; due = numerator/3579545`) is exactly `floor(f*59736*44100/3579545)`, overflow-free to 862k frames (2.27e15 << 2^64); a 737-per-frame implementation fails at frame 1 (737 vs 735) and fails Case 2 outright (delta set over 4 h is exactly {735,736}; 737-style gives {737}); the ns case fails any 16 ms-truncation scheme |
| Determinism check (code-level, item below) | pass |
| Scope check: `git diff HEAD -- src/devices src/machine src/core` | **empty**; only untracked src files are `src/frontend/audio_pacer.{h,cpp}` |
| Instrumentation scaffolding | no `TEMP-INSTRUMENTATION`/`AUDIOQ` markers in `src/` or `tests/`; `SDL_GetAudioStreamQueued` appears only in the paced presenter path (by design) and the integration test; my own `QA-TEMP-PROBE` fully reverted (marker scan clean, diff stat byte-identical to the developer's fix) |
| Game-specific keying | none — game titles appear only in doc/command examples |
| Grounding citations | verified: `references/openmsx-21.0/src/sound/SDLSoundDriver.cc:43` (ring = 3 fragments), `:152-155` (literal "drop excess samples"), `references/openmsx-21.0/src/sound/Mixer.cc:21-23` (defaultSamples 1024 non-Win32); second reference `references/fmsx-60/source/EMULib/Unix/SndUnix.c:180` (latency-parameterized bounded ring) independently confirms the bounded-host-buffer policy shape |

### Determinism verdict (checklist item 2)

**Pass.** In `AudioPacer::plan()` (`src/frontend/audio_pacer.cpp:21-25`), `samples_to_pump`
is computed solely from `total_elapsed_cycles` and `samples_produced_` (itself a pure
function of the cycle-count history). `queued_samples` enters only at step 2/3 —
`samples_to_push` and `silence_samples_to_push`, both host-push-only outputs. The call site
passes `machine_.elapsed_cycles()` (`sdl3_app.cpp:235`). Emulated state therefore never
depends on wall clock or host-queue state.

### PSG generator-desync verdict (checklist item 3)

**Pass.** In `pump_and_push_paced()` (`src/frontend/sdl3_audio_presenter.cpp`),
`pump_.pump_samples(psg, samples_to_pump)` executes for the **full** exact delta before the
`samples_to_push == 0` early-return; trimming only limits the PCM conversion/push loop over
the first `samples_to_push` samples. Confirmed by unit Cases 4/6
(`samples_produced() == cycles_to_samples(...)` under queue-at-cap and 2-second-stall) and
by the integration test's cumulative-production assertion on a real stream.

## 3. Failures and Risk Ranking

No blocker, major, or minor functional failures found. Informational observations:

1. **(Info)** `.claude/settings.json` carries an unrelated `"sonnect"` -> `"sonnet"` typo
   fix in the same working tree. Out of scope for this cycle; should be committed
   separately (or explicitly noted by the coordinator), not silently bundled.
2. **(Info)** Untracked `build-qa/`, `build-qa2/`, `disks/games/`, `debug/*` outputs, and
   stray `references/sdl3/...` dirs exist; a careless `git add -A` would sweep them into
   the fix commit. Stage the eight fix paths explicitly.
3. **(Info)** Grounding nuance: openMSX's *throttled* path blocks (sleeps) instead of
   dropping (`SDLSoundDriver.cc:143-151`); the cited drop branch is its unthrottled
   fallback. Here `run_interactive()`'s own deadline pacing plays the throttle role, so
   trim-as-fallback is the correct analogue; fMSX's bounded ring corroborates the shape.
4. **(Info)** `SDL_GetAudioStreamQueued` error (−1) is treated as an empty queue, which
   would trigger a one-shot ≤33 ms silence re-prime — benign, SDL-error-only path.
5. **(Info)** With the measured +3.85 samples/s host clock-domain skew, the queue walks
   from the ~33 ms low-water toward the 67 ms cap over ~5-6 minutes, then holds there with
   periodic few-sample trims. Steady-state long-session latency ≈67 ms — bounded by design
   and comparable to openMSX's own ~69.7 ms (non-Win32) / ~139 ms (Win32 default) ring.
6. **(Info, pre-existing)** The disclosed 81-vs-81.1688 generator-step simplification
   (constant −3.6 cent PSG pitch offset) is retained and now explicitly documented in
   `sdl3_audio_presenter.h`; sample *count* now tracks machine time exactly.

## 4. Required Fixes

None required for sign-off. Housekeeping at commit time: keep the commit to the eight fix
paths listed in `docs/audio-latency-investigation.md` §6 plus that doc and this review;
handle the `.claude/settings.json` typo separately.

## 5. Sign-off Decision

**PASS.**

The human-reported defect (audio 0.5-1 s late, growing) is fixed at its measured root
causes, verified independently: frame cadence 59.9226 fps (was 61.61), host audio queue
bounded 25.8-49.4 ms over a ~3-minute real-WASAPI Metal Gear session (was +29.7 ms/s
unbounded), exit 0, headless 154/154 and SDL3-ON 163/163 reproduced from my own builds of
the working tree, zero device/machine/core changes, zero game-specific logic, and the
emulated-state path provably independent of wall clock and host-queue state.
