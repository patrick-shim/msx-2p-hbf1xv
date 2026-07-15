# Audio latency investigation — PSG sound 0.5-1 s late in the SDL3 frontend

Date: 2026-07-08. Coordinator-authorized targeted fix cycle (DEC-0026..DEC-0032 precedent).
Human-reported bug: in live play (`sony_msx_sdl3.exe --cart1 roms\metalgear.rom --cart1-type
Konami`), PSG sound effects arrive 0.5-1 second after the triggering action. Video and input
are real-time; only audio lags, and the lag grows the longer the session runs.

## 1. Pre-fix measurement (real WASAPI driver, hidden window, BIOS boot + idle)

Temporary once-per-second `SDL_GetAudioStreamQueued()` logging in `run_interactive()`
(instrumentation reverted after measurement), 11,000-frame bounded run:

| Metric | Value |
| --- | --- |
| Measured frame rate | **61.61 fps** (correct cadence is 59.9227 Hz) |
| Queue at t=1 s | 1,866 samples (42.3 ms) |
| Queue at t=178 s | 233,459 samples (**5,293.9 ms**) |
| Growth | **+1,307.6 samples/s = +29.7 ms of audio latency per second of play**, monotonic, unbounded |

The human's "0.5-1 s late" is reached after only 17-34 seconds of play; a 3-minute session
already sits at ~5.3 s of lag.

## 2. Root cause (exact arithmetic)

Three compounding defects, all in the frontend presentation layer (no device involvement):

1. **Wall-clock pacing truncation** (`sdl3_app.cpp run_interactive()`): the exact frame
   period is `59736 cycles / 3,579,545 Hz = 16.688154 ms`, but the loop computed
   `target_ms = static_cast<Uint64>(16.688...) = 16 ms`. With per-frame `SDL_Delay`
   granularity the session ran at a measured 61.61 fps instead of 59.9227 — the whole
   emulator ~2.8-4.3% fast. Audio production scaled with it:
   `(61.61 − 59.9227) × 737 ≈ +1,244 samples/s`.
2. **Per-frame sample-count rounding** (`sdl3_app.cpp run_one_frame()`): production was
   `59736 / kCyclesPerSample = 59736 / 81 = 737` samples/frame, but the exact figure is
   `59736 × 44100 / 3,579,545 = 735.948` samples/frame. Even under perfect pacing this
   overproduces `+1.052 samples/frame ≈ +63 samples/s` (1 s of added latency per ~11.7 min).
   At the measured 61.61 fps: `61.61 × 1.052 ≈ +65 samples/s`.
3. **No backpressure**: `SDL_PutAudioStreamData` queues without limit and nothing ever
   read `SDL_GetAudioStreamQueued`, so every excess sample became permanent latency.

Predicted total: `1,244 + 65 ≈ +1,309 samples/s`. Measured: `+1,307.6 samples/s`. The model
matches the measurement to 0.1%.

## 3. The fix (universal, presentation-level; zero device changes, zero game-specific logic)

New SDL-free policy class **`src/frontend/audio_pacer.h/.cpp`** (`AudioPacer`), wired into
`Sdl3AudioPresenter::pump_and_push_paced()` and `Sdl3App`:

1. **Exact-accounting production** — cumulative samples due are derived from the machine's
   cumulative elapsed cycles: `total_due = floor(elapsed_cycles × 44100 / 3,579,545)`,
   integer math via an overflow-safe split (`AudioPacer::scale_cycles`), so production
   matches emulated time exactly over any session length (per-frame deltas are only ever
   735 or 736 — proven by unit test over a simulated 4-hour session).
2. **Backpressure/latency cap** — each frame reads the live queue depth
   (`SDL_GetAudioStreamQueued`, `references/sdl3/include/SDL3/SDL_audio.h`) and never pushes
   past **`kMaxQueuedSamples` = 2,954 samples ≈ 67 ms (~4 video frames)**. Excess production
   after a host stall is trimmed (pushed-output dropped) so latency re-converges instead of
   accumulating. *Policy grounding (frontend policy, not chip behavior):* openMSX's own SDL
   sound driver bounds its ring at 3 fragments of the default 1,024 samples ≈ 69.7 ms @
   44.1 kHz and drops excess samples when full
   (`references/openmsx-21.0/src/sound/SDLSoundDriver.cc:43,152-155`,
   `references/openmsx-21.0/src/sound/Mixer.cc:21-23`).
3. **Low-water re-prime** — only when the queue is found **empty** (session start, or fully
   drained after a stall — an underrun boundary where the device already played silence),
   silence is pushed to restore **`kLowWaterSamples` = 1,455 samples ≈ 33 ms (~2 frames)**
   of base latency, keeping playback contiguous instead of riding the empty-queue edge.
   Ordinary jitter dips never trigger it.
4. **Exact-nanosecond frame pacing** (`run_interactive()`) — absolute deadlines from
   cumulative emulated cycles via the same `scale_cycles` primitive
   (`16,688,154 ns/frame`, no truncation, no per-frame rounding drift), using
   `SDL_GetTicksNS()`/`SDL_DelayNS()` (`references/sdl3/include/SDL3/SDL_timer.h:213,281`).
   Backlogs ≤ 100 ms are caught up (refilling the audio queue, bounded by the cap); larger
   backlogs (debugger pause, laptop sleep) are forgiven by sliding the baseline rather than
   fast-forwarding the machine.

### PSG generator-time policy (explicit decision)

- `samples_to_pump` — the only pacer output that touches emulated device state — is a **pure
  function of elapsed cycles**. Host-queue state only ever affects what is *pushed* to SDL.
  Backpressure trimming drops **pushed** samples while the pump still advances the PSG
  generator for every produced sample, so the pump's notion of time never desyncs from the
  machine's.
- The pre-existing, already-disclosed simplification that each sample advances the generator
  by `kCyclesPerSample = floor(3,579,545/44,100) = 81` cycles (vs the exact 81.1688) is kept:
  the PSG generator experiences 99.792% of machine time — a constant **−3.6 cent** pitch
  offset, now explicitly documented in `sdl3_audio_presenter.h`. The *sample count* tracks
  machine time exactly.
- Determinism: ctest-facing paths are unaffected by wall clock — the plan's pump count
  depends only on cycle counts, and backpressure is a function of queue state only. The
  legacy raw `pump_and_push()` plumbing primitive is retained for tests.

## 4. Post-fix measurement (same protocol: real WASAPI driver, 11,000 frames, ~183 s)

| Metric | Pre-fix | Post-fix |
| --- | --- | --- |
| Frame rate | 61.61 fps | **59.9229 fps** (target 59.9227) |
| Queue depth trend | +29.7 ms/s, monotonic, unbounded | **bounded 30.4-50.9 ms** for the entire run |
| Steady-state stream latency | 5,293.9 ms at t=178 s and still climbing | **mean 41.1 ms** (min 30.4 / max 50.9 ms), hard-capped at 67 ms |
| Net drift | +1,307.6 samples/s | +3.3 samples/s residual (≈0.0075% host clock-domain skew, WASAPI crystal vs QPC ticks — absorbed by the cap; latency can never exceed 67 ms regardless of run length) |

## 5. Test evidence

- `frontend_audio_pacer_unit_test` (new, headless, SDL-free): exact-accounting no-drift proof
  over a simulated 4-hour session against an independent incremental-numerator oracle;
  735/736-only per-frame deltas (never the legacy 737); exact 16,688,154 ns frame-period
  scaling; overflow safety to one emulated year; backpressure cap, exact-room push,
  drop accounting, monotonic guard, 2-second-stall re-convergence, low-water clamp.
- `frontend_sdl3_audio_presenter_integration_test` (extended, SDL3-gated, dummy driver):
  paced path pushes 120 frames faster than real time — queue depth never exceeds
  `kMaxQueuedSamples` in bytes, cumulative production is exactly
  `floor(cycles × 44100 / 3,579,545)`, repeated cycle counts produce nothing.
- `hbf1xv_m26_sdl3_system_test`: pre-existing queue-after-frame-1 assertion still green on
  the paced path.
- Full fast suites: headless `ctest -LE m24_slow_full_sweep` **154/154 passed**;
  SDL3-ON (dummy drivers) **163/163 passed**. No CPU/core change → slow ZEXALL sweep not
  run (per standing cadence rule).

## 6. Files changed

- `src/frontend/audio_pacer.h`, `src/frontend/audio_pacer.cpp` (new)
- `src/frontend/sdl3_audio_presenter.h/.cpp` (paced push path + policy constants)
- `src/frontend/sdl3_app.h/.cpp` (paced production from cumulative cycles; exact-ns pacing)
- `CMakeLists.txt`, `tests/CMakeLists.txt` (new source + test registration)
- `tests/unit/frontend/audio_pacer_unit_test.cpp` (new)
- `tests/integration/frontend/sdl3_audio_presenter_integration_test.cpp` (extended)

Temporary queue-depth instrumentation used for §1/§4 was reverted before completion
(verified: no `TEMP-INSTRUMENTATION`/`AUDIOQ` markers remain in `src/` or `tests/`).
Changes left uncommitted for coordinator/QA review.
