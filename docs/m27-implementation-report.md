# M27 Implementation Report — Production Hardening + Debug/Test Tooling

- Milestone ID: M27
- Developer: MSX Developer Agent
- Planner package: `docs/m27-planner-package.md`
- Dispatch: REQ-M27-002 (`agent-protocol/channels/requests.md:1372-1384`)
- Implements items 1-4 named in REQ-M27-001: (1) full CPU/memory/VRAM state-dump CLI wiring,
  (2) real decoded PSG audio capture to file, (3) keystroke-sequencing/scripted-input automation,
  (4) event-log CLI wiring + a genuine replay-determinism proof.

---

## 1. Milestone Target

Wire the already-existing, already-tested `Hbf1xvMachine::write_state_dump()`/`write_cpu_trace()`/
`write_event_log()` (M10-S3) into both executables via a new bounded, real-BIOS-asset-loaded,
CPU-driven headless `--debug-session` mode plus additive SDL3-side `Sdl3AppConfig`/`sdl3_cli`
fields; build a new, headless-buildable, genuinely-decoded PSG audio-dump capability
(`src/frontend/psg_audio_dump.{h,cpp}`) reusing M26's `PsgAudioPump`/`PsgYm2149`, plus a new
`tools/audio-dump-to-wav.py` and a real, committed, non-silent example WAV; build a deterministic
keystroke-sequencing/scripted-input mechanism (`src/peripherals/msx_key_names.*` +
`src/machine/input_script.*`) wired into both executables via `--input-script`; and produce a
concrete, adversarially-validated, byte-for-byte replay-determinism system test proving this
project's own core determinism claim end to end via the debug tooling itself.

Followed the planner's 8-slice build order (S1→S8) exactly. No scope change from the accepted
planner package.

---

## 2. Code Changes

### 2.1 New, wholly additive production files

| File | Purpose |
|---|---|
| `src/peripherals/msx_key_names.h`/`.cpp` | Symbolic key-name → (row, column) lookup, 71 entries, every entry copied verbatim from `src/frontend/sdl3_input_mapper.cpp`'s own array literal this cycle (R-M27-4) |
| `src/machine/input_script.h`/`.cpp` | Deterministic timed key-event script format (`HBF1XV-INPUT-SCRIPT v1`) — parse/serialize + a monotonic-cursor `InputScriptPlayer` |
| `src/frontend/psg_audio_dump.h`/`.cpp` | Headless-buildable, decoded-PSG-audio-dump serializer (`HBF1XV-AUDIO-DUMP v1`), genuinely reusing `frontend::PsgAudioPump`/`devices::audio::PsgYm2149` (M26), plus the documented `psg_raw_sum_to_pcm16()` linear scale grounded in `PsgYm2149::sample()`'s real `[0,62]` range |
| `tools/audio-dump-to-wav.py` | New, sibling-of-`frame-to-png.py` WAV converter for the `[AUDIO]`/`[PCM]` dump format — does NOT extend `tools/mem-to-audio.py` |

Zero edits to any pre-existing file under `src/devices/` or `src/peripherals/` — confirmed via
`git diff v1.0.26 --stat -- src/devices/ src/peripherals/` (empty) at every gate (§4 below).

### 2.2 Additive-only edits to pre-existing files

| File | Change |
|---|---|
| `src/main.cpp` | Two wholly new `if` branches in `main()`: `--debug-session <bios_dir> <max_steps> [--disk] [--cart1/-type] [--cart2/-type] [--debug-root] [--dump-state] [--trace-cpu] [--event-log] [--input-script]` and `--audio-dump-demo <name>`. The pre-existing default run path (lines 899-912 pre-cycle) is byte-for-byte unchanged. |
| `src/frontend/sdl3_app.h`/`.cpp` | Additive `Sdl3AppConfig` fields (`dump_state_filename`, `trace_cpu_filename`, `event_log_filename`, `input_script_path`, all `std::nullopt` default); a new public `flush_debug_session_outputs()` method; `input_script_player_` member (default-constructed = genuine no-op); `run_one_frame()`'s CPU sub-loop additively calls `input_script_player_.apply_due(...)` each step; `run_interactive()`'s loop-exit path additively calls `flush_debug_session_outputs()`. |
| `src/frontend/sdl3_cli.h`/`.cpp` | Additive `ParsedSdl3Cli` fields (`dump_state_filename`/`trace_cpu_filename`/`event_log_filename`/`input_script_path`) and their parsing branches. |
| `src/frontend/sdl3_main.cpp` | Copies the four new parsed fields into `Sdl3AppConfig`; usage string extended. |
| `CMakeLists.txt` | Three new source files added to the existing, unguarded `sony_msx_core` list. |
| `tests/CMakeLists.txt` | New test registrations (headless outside the SDL3 guard; SDL3-gated inside it). |
| `.gitignore` | `/debug/**/*.dump` added to the generated-runtime-output ignore list; two new exception entries for the committed `debug/sounds/m27-example-tone.{wav,dump}` evidence pair (mirrors the M26 PNG exception precedent). |

`git diff v1.0.26 --stat -- src/machine/hbf1xv_machine.h src/machine/hbf1xv_machine.cpp` is
**empty** — confirmed zero new machine-level method was needed for item 1 (Acceptance Criterion 2).
`git diff v1.0.26 --stat -- tools/mem-to-audio.py` is **empty** — confirmed genuinely untouched.

### 2.3 A load-bearing self-caught planner-grounding correction

The planner package (§2.3) cited `PsgYm2149::write_register()` as "the ALREADY-existing,
already-tested public API, `psg_ym2149.h:141`". Direct re-read of the header this cycle (per the
planner's own A-M27 "re-verify before implementing" discipline) showed `write_register()` is
declared under the class's **private** section (`psg_ym2149.h:103` `private:` precedes line 141).
The `--audio-dump-demo` mode (and its unit test) instead use the genuinely public API
`write_address()` (#A0) + `write_data()` (#A1) — the exact ports the real CPU/IoBus itself uses —
functionally identical and, if anything, more realistic. Documented in the code comment at the
call site; no behavior change to any device file was needed.

### 2.4 New test files (17 total: 3 unit, 5 integration headless, 2 system headless, 3 integration
    SDL3-gated — plus registration edits)

- `tests/unit/peripherals/msx_key_names_unit_test.cpp` — all 71 documented names resolve correctly;
  unmapped/case-sensitivity/numeric-keypad-out-of-scope regression guards.
- `tests/unit/machine/input_script_unit_test.cpp` — parse/serialize round-trip; 7 malformed-input
  rejection classes (missing tag, bad DOWN/UP token, unrecognized KEY, non-numeric T, out-of-order
  T, missing T token, empty input); `InputScriptPlayer`'s monotonic-cursor discipline (apply exactly
  once, never skipped/re-applied, safe no-op on repeated/stale/past-end `current_tstate`).
- `tests/unit/frontend/psg_audio_dump_unit_test.cpp` — `psg_raw_sum_to_pcm16()`'s documented linear
  scale (hand-computed boundary + midpoint values + clamping); a full-dump byte-for-byte oracle
  built from an independently re-derived `PsgAudioPump` sequence (sample_rate=kSystemClockHz gives
  cycles_per_sample=1, a clean, hand-computable residual-accumulation oracle: 15 silent samples then
  2 audible ones); determinism (two identical runs byte-identical); `write_psg_audio_dump()`
  directory-creation/content-parity contract.
- `tests/integration/machine/hbf1xv_m27_debug_session_integration_test.cpp` — the device-level
  seams `--debug-session` relies on: a bounded, real-BIOS-loaded, halt-respecting
  `step_cpu_instruction()` loop is deterministic (A-M27-1/A-M27-2 resolved); the `--disk` mechanism
  (A-M27-3) mounts a real disk image byte-readable both via `DiskImage::byte_at()` and
  `DiskDrive::read_sector()`.
- `tests/integration/machine/hbf1xv_m27_input_script_integration_test.cpp` — a flat-RAM NOP driver
  program gives an exactly hand-computable T-state schedule (5 T-states/NOP); asserts keyboard-matrix
  transitions at the EXACT expected T-state boundaries.
- `tests/system/hbf1xv_m27_replay_determinism_system_test.cpp` — the item-4 headline proof (§3 below).
- `tests/system/hbf1xv_m27_debug_tooling_system_test.cpp` — the S8 "everything together" scenario:
  state-dump + CPU-trace + event-log + scripted-input driving in one deterministic, bounded,
  real-BIOS-boot run; two independent runs byte-identical across all three output files.
- `tests/integration/frontend/sdl3_app_debug_session_integration_test.cpp` (SDL3-gated) — mirrors
  the headless state-dump/CPU-trace/event-log assertions via bounded `run_one_frame()` calls, PLUS
  the hard regression-guard case (§4 Acceptance Criterion 10): fields left unset → zero observable
  effect, no debug-output directory ever created.
- `tests/integration/frontend/sdl3_input_mapper_key_names_consistency_integration_test.cpp`
  (SDL3-gated) — the HARD cross-consistency gate (R-M27-4): all 71 entries independently re-paired
  (scancode, name) and asserted to resolve to the identical (row, column) via both tables, PLUS a
  completeness check that no `scancode_map()` entry was silently omitted from the pairing.
- `tests/integration/frontend/sdl3_app_input_script_integration_test.cpp` (SDL3-gated) — the SAME
  script text the headless configuration's test uses, driven through `run_one_frame()`, asserting
  the SAME documented final keyboard-matrix state (Acceptance Criterion 7).

---

## 3. The replay-determinism proof — a genuine finding and design correction

The planner's illustrative design (§2.5) used the real BIOS + `roms/aleste.rom` cartridge for BOTH
the positive (two-identical-runs) and negative (adversarial-divergence) cases, with the divergence
injected via a bare `keyboard().set_key()` call. Implementing this literally exposed a real,
load-bearing finding: **the real, unmodified BIOS, driven purely by `step_cpu_instruction()` (no
`on_vsync()`), spends its first >=20,000 instructions entirely inside the well-known MSX BIOS
RAM/slot-detection routine** — confirmed by direct inspection of the loaded BIOS image bytes at the
observed loop PCs (`IN A,(#A8)`, the PPI slot-select readback port, never `#A9`, the keyboard port),
and independently corroborated by this project's own pre-existing backlog **C5** disclosure ("the
auto-boot handshake is genuinely never observed... confirmed up to a 20,000,000-instruction
diagnostic run"). A first implementation of the negative control (bare `set_key()`, no other change)
was run and **failed as expected**: zero observable divergence, because the real BIOS never once
reads the keyboard matrix within any bounded window fast enough for a `ctest` case.

This is an honest architectural fact, not a test bug — and it is itself a real, in-cycle example of
why item 4's "genuinely discriminates" requirement matters: a naively-designed negative control
would have silently passed vacuously here (the two runs WOULD be identical either way, for the
wrong reason).

**Fix applied** (documented in the test file's own header comment): the final test still loads the
real BIOS asset root and the real cartridge (`roms/aleste.rom`, honoring "same BIOS asset root, same
cartridge" literally), then additionally calls `map_flat_ram()` and drives a small, 21-byte,
hand-verifiable Z80 driver program — mirroring `src/main.cpp`'s own pre-existing
`run_parity_trace()` technique — that genuinely reads the keyboard matrix via the real `#AA`
(row-select)/`#A9` (row-read) PPI ports and branches on the result:

```
0000  3E 02        LD A,#02          select row 2 ("A" lives here)
0002  D3 AA        OUT (#AA),A       PPI port C row-select
0004  DB A9        IN A,(#A9)        PPI port B: read row 2 (inverted)
0006  CB 77        BIT 6,A           test column 6 ("A")
0008  28 05        JR Z,+5           Z=1 (bit clear) means "A" pressed
000A  00×5         NOP ×5            "not pressed" path
000F  00×5         NOP ×5            "pressed" path (jump target)
0014  76           HALT
```

- **Case 1 (positive)**: two independent machines, identical setup (real BIOS root, real cartridge,
  event logging enabled BEFORE `cold_boot()`), "A" never pressed → byte-for-byte identical event
  logs (18 lines: 1 Reset + 16 InstructionRetire + 1 Halt, hand-computed and asserted exactly).
  Reset event confirmed present at sequence 0 (R-M27-2's ordering requirement).
- **Case 2 (adversarial negative control)**: a third machine, identical setup, but
  `keyboard().set_key(2, 6, true)` injected after step 2 (before the `IN A,(#A9)` read consumes it)
  → event log genuinely DIFFERS: 13 lines (5 fewer InstructionRetire events, matching the 5 skipped
  NOPs exactly, hand-computed and asserted exactly), and the not-pressed run's trace contains
  `PC=000A` while the pressed run's never does (a real PC-level divergence, not merely a line-count
  difference).

Both the ctest case AND a real, manual two-process launch of the compiled
`sony_msx_headless.exe --debug-session bios 3000 --event-log run1.log` /
`... run2.log` (two separate process invocations) were run — byte-for-byte identical
(`cmp` exit 0; identical SHA-256 `2f7b430925d1e6a8090d0362f49112c00a1a1e17f97c74b3c375b6fb764381c8`
for both, 136,977 bytes each) — see §5.

---

## 4. Unit Test Results

Headless fast-subset (`-DSONY_MSX_ENABLE_SDL3=OFF`, `ctest -LE m24_slow_full_sweep`):

```
100% tests passed, 0 tests failed out of 140
Total Test time (real) =   3.67 sec
```

(133 prior fast-subset tests + 7 new M27 headless tests: `peripherals_msx_key_names_unit_test`,
`machine_input_script_unit_test`, `frontend_psg_audio_dump_unit_test`,
`machine_hbf1xv_m27_debug_session_integration_test`,
`machine_hbf1xv_m27_input_script_integration_test`, `hbf1xv_m27_replay_determinism_system_test`,
`hbf1xv_m27_debug_tooling_system_test`.)

All new unit tests individually re-run and confirmed green:

```
peripherals_msx_key_names_unit_test .................   Passed
machine_input_script_unit_test ......................   Passed
frontend_psg_audio_dump_unit_test ...................   Passed
```

`python tools/audio-dump-to-wav.py --self-check`:

```
[PASS] parse_audio_dump: sample_rate matches
[PASS] parse_audio_dump: channels matches
[PASS] parse_audio_dump: bits matches
[PASS] parse_audio_dump: samples matches
[PASS] parse_audio_dump: pcm bytes match
[PASS] reproducible: two encodes byte-identical
[PASS] RIFF magic / WAVE form type / fmt chunk id / PCM format tag (1)
[PASS] stereo (2 channels) / sample rate 44100 / 16 bits/sample / block align 4
[PASS] byte rate == rate*block_align / data chunk id / data size == 12 / total bytes == 44+12
[PASS] first sample pair decodes to (-32768, -32768)
[PASS] third sample pair decodes to (-1, 12345)
[PASS] golden sha256 matches
SELF-CHECK: OK
```

---

## 5. Integration Test Results

Headless integration/system tests are included in the 140/140 fast-subset figure above (individually
confirmed):

```
machine_hbf1xv_m27_debug_session_integration_test ...   Passed   0.20-0.22 sec
machine_hbf1xv_m27_input_script_integration_test ....   Passed   0.02-0.03 sec
hbf1xv_m27_replay_determinism_system_test ...........   Passed   0.24-0.31 sec
hbf1xv_m27_debug_tooling_system_test ................   Passed   0.17-0.19 sec
```

SDL3-ON fast-subset (`-DSONY_MSX_ENABLE_SDL3=ON`,
`-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install`, `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`,
`ctest -LE m24_slow_full_sweep`):

```
100% tests passed, 0 tests failed out of 149
Total Test time (real) =   7.56-8.10 sec
```

(139 prior fast-subset tests + 10 new: the 7 shared headless M27 tests, re-verified in this
configuration too, plus 3 new SDL3-gated: `frontend_sdl3_app_debug_session_integration_test`,
`frontend_sdl3_input_mapper_key_names_consistency_integration_test`,
`frontend_sdl3_app_input_script_integration_test`.) Every M26 pre-existing SDL3 test remains green
unmodified (`frontend_sdl3_app_headless_integration_test`,
`frontend_sdl3_video_presenter_pixel_integration_test`,
`frontend_sdl3_audio_presenter_integration_test`, `frontend_sdl3_input_mapper_integration_test`,
`frontend_sdl3_cli_session_integration_test`, `hbf1xv_m26_sdl3_system_test`) — confirming the new
additive `Sdl3AppConfig` fields are genuine no-ops when unset (Acceptance Criterion 10).

**Manual, real two-process launch evidence (`--debug-session`, item 4, §2.5's complementary
evidence-gate step, NOT a `ctest` case):**

```
$ ./build/Debug/sony_msx_headless.exe --debug-session bios 3000 --event-log m27-manual-run1.log
debug-session: steps=3000 halted=0 final_pc=456 elapsed_cycles=22825
$ ./build/Debug/sony_msx_headless.exe --debug-session bios 3000 --event-log m27-manual-run2.log
debug-session: steps=3000 halted=0 final_pc=456 elapsed_cycles=22825
$ cmp debug/logs/m27-manual-run1.log debug/logs/m27-manual-run2.log && echo IDENTICAL
IDENTICAL
$ sha256sum debug/logs/m27-manual-run{1,2}.log
2f7b430925d1e6a8090d0362f49112c00a1a1e17f97c74b3c375b6fb764381c8  m27-manual-run1.log
2f7b430925d1e6a8090d0362f49112c00a1a1e17f97c74b3c375b6fb764381c8  m27-manual-run2.log
```

Both real, separate process invocations, 136,977 bytes each, byte-identical. (Files were scratch
evidence, not committed — cleaned up after capture.)

**Manual, real `sony_msx_sdl3.exe` launch evidence** (under `SDL_VIDEO_DRIVER=dummy`/
`SDL_AUDIO_DRIVER=dummy`):

```
$ ./sony_msx_sdl3.exe --hidden-window --bios-dir bios --max-frames 5 \
    --dump-state m27-sdl3-manual.state --trace-cpu m27-sdl3-manual.trace \
    --event-log m27-sdl3-manual.log --input-script /dev/null
sdl3: initialization failed: malformed --input-script: parse_input_script: missing/mismatched format tag
(exit 1 -- correct: a loud, non-zero-exit failure, never a silent fallback)

$ ./sony_msx_sdl3.exe --hidden-window --bios-dir bios --max-frames 5 \
    --dump-state m27-sdl3-manual.state --trace-cpu m27-sdl3-manual.trace \
    --event-log m27-sdl3-manual.log --input-script <valid-script>
sony-msx-hbf1xv SDL3 frontend: window opened, entering real-time loop ...
(exit 0; all three output files created: 1,847,357-byte event log, 15,738-byte state dump,
8,207,919-byte CPU trace)
```

---

## 6. Evidence-gate cadence discipline (`feedback_slow_test_cadence.md`) — git-diff verification at
   every gate

Per the standing directive, the default gate throughout was
`ctest --test-dir build -C Debug --output-on-failure -LE m24_slow_full_sweep`. The slow
`hbf1xv_m24_zexall_system_test` was **not run** at any point this cycle. Before every gate where it
was skipped, the required `git diff` was run and its actual output captured (not merely concluded):

**Gate 1 (after S1-S4, debug-session skeleton + SDL3-side wiring):**
```
$ git diff v1.0.26 --stat -- src/devices/ src/peripherals/ src/core/
(empty)
$ git status --porcelain -- src/devices/ src/peripherals/ src/core/
?? src/peripherals/msx_key_names.cpp
?? src/peripherals/msx_key_names.h
```

**Gate 2 (after S5-S7, audio-dump + scripted-input):** identical output to Gate 1 (re-run verbatim,
same two new files, zero modified pre-existing files).

**Gate 3 (final closure gate):**
```
$ git diff v1.0.26 --stat -- src/devices/ src/peripherals/ src/core/
(empty)
$ git status --porcelain -- src/devices/ src/peripherals/ src/core/
?? src/peripherals/msx_key_names.cpp
?? src/peripherals/msx_key_names.h
$ git diff v1.0.26 --name-only -- src/devices/cpu/ src/core/
(empty)
$ git status --porcelain -- src/devices/cpu/ src/core/
(empty)
```

All three gates show the identical, clean result: the two listed paths are genuinely NEW, additive
files this milestone adds (`src/peripherals/msx_key_names.{h,cpp}`) — the hard, zero-tolerance
requirement (no pre-existing file under `src/devices/`/`src/peripherals/` modified; `src/devices/cpu/`
and `src/core/` showing ZERO changes of any kind) holds throughout. Per R-M27-6's precise
interpretation, the fast subset was genuinely sufficient at every gate; the slow test was not needed.
A final, unfiltered `ctest` run (including the slow test) is left as a coordinator/QA judgment call
at closure time, per the dispatch's own framing.

Additional targeted checks: `git diff v1.0.26 --stat -- src/machine/hbf1xv_machine.h
src/machine/hbf1xv_machine.cpp` (empty — Acceptance Criterion 2); `git diff v1.0.26 --stat --
tools/mem-to-audio.py` (empty — confirmed untouched).

---

## 7. Evidence gates (both build configurations)

```
$ powershell tools/validate-assets.ps1
Asset validation result: True (7 BIOS files present, 2 ROM files present)

$ powershell tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
Checksum report written to docs/asset-checksums.txt (unchanged content -- no asset files touched)

$ cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF && cmake --build build --config Debug
(succeeds; sony_msx_headless.exe + sony_msx_core.lib + all test executables built)

$ ctest --test-dir build -C Debug --output-on-failure -LE m24_slow_full_sweep
100% tests passed, 0 tests failed out of 140

$ cmake -S . -B build/sdl3-on -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install
$ cmake --build build/sdl3-on --config Debug
(succeeds; sony_msx_sdl3.exe + all SDL3-gated test executables built)

$ SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build/sdl3-on -C Debug \
    --output-on-failure -LE m24_slow_full_sweep
100% tests passed, 0 tests failed out of 149
```

The committed example WAV: `debug/sounds/m27-example-tone.wav` (176,444 bytes, 1.0 s, 44,100 Hz,
stereo, 16-bit), produced by `sony_msx_headless.exe --audio-dump-demo m27-example-tone.dump`
(channel A: fixed tone period 16, fixed max volume level 15) then
`python tools/audio-dump-to-wav.py debug/sounds/m27-example-tone.dump -o
debug/sounds/m27-example-tone.wav`. Independently verified non-silent and genuinely oscillating (not
a placeholder): 44,100 stereo samples, values alternate between exactly `-32768` and `-1` (the
documented `psg_raw_sum_to_pcm16(0)`/`psg_raw_sum_to_pcm16(31)` oracle values), 13,953 toggles over
the full second (~7 kHz square wave). The underlying `.dump` file is committed alongside it (mirrors
the M26 `.frame`+`.png` pairing precedent).

---

## 8. Known Issues

- The item-4 replay-determinism proof's real-BIOS+cartridge scenario needed a supplementary
  flat-RAM driver program for the adversarial negative control specifically (§3) — the real,
  unmodified BIOS's own early boot sequence never reads the keyboard matrix within any practical
  bounded `ctest` window (a genuine architectural finding, not a defect; consistent with this
  project's pre-existing backlog C5 disclosure). Documented in full in the test file's own header
  comment.
- `PsgYm2149::write_register()` is a private implementation detail, not the public API the planner
  package's illustrative code cited (§2.3 above) — corrected in the actual implementation; no
  device-file change needed.
- The audio-dump-demo's single-channel-only tone naturally reaches only half the documented `[0,62]`
  raw-sum range (channels B/C are silenced via volume=0, so `raw_sum` alternates between 0 and 31,
  not 0 and 62) — a real, expected, disclosed consequence of the "channel A only" demo design, not
  an error in the linear PCM-scaling formula itself (which is exercised across its full documented
  range by the unit test's Case 1 boundary checks).
- No new backlog row status changes this cycle (this milestone closes zero backlog rows, per the
  planner's own §3.9 finding) — only a factual, non-status-changing cross-reference note added to
  C5 (`agent-protocol/state/deferred-backlog.md`).

## 9. QA Handoff

- Planner package: `docs/m27-planner-package.md` (accepted).
- This report: `docs/m27-implementation-report.md`.
- Headless fast-subset: 140/140. SDL3-ON fast-subset: 149/149. Both build configurations rebuilt
  from the current tree and re-confirmed green before this report was finalized.
- `git diff v1.0.26` verification captured at 3 gates (§6) — zero tracked-file modification under
  `src/devices/`/`src/peripherals/`/`src/core/`; `src/devices/cpu/`/`src/core/` show zero changes of
  any kind (new or modified). `src/machine/hbf1xv_machine.{h,cpp}` genuinely untouched (Acceptance
  Criterion 2). `tools/mem-to-audio.py` genuinely untouched.
- Real, committed, non-silent example WAV: `debug/sounds/m27-example-tone.wav` (+ `.dump`).
- Adversarial negative control (item 4) genuinely discriminates — documented design correction in
  §3 above; verify by reading `tests/system/hbf1xv_m27_replay_determinism_system_test.cpp` directly.
- Cross-consistency (item 3, R-M27-4) and cross-executable-equivalence (Acceptance Criterion 7) are
  both independently `ctest`-verified, not merely asserted — see
  `tests/integration/frontend/sdl3_input_mapper_key_names_consistency_integration_test.cpp` and the
  paired `hbf1xv_m27_input_script_integration_test.cpp`/`sdl3_app_input_script_integration_test.cpp`.
- The slow `hbf1xv_m24_zexall_system_test` was not run this cycle (per the standing cadence
  directive); a final, unfiltered `ctest` run (including it) is a QA/coordinator judgment call at
  closure, per the dispatch's own framing — the git-diff evidence above should make that call
  low-risk either way.
- Recommend QA independently: (a) reproduce both fast-subset `ctest` runs from clean rebuilds;
  (b) directly launch both real executables with `--debug-session`/SDL3 debug flags itself, not just
  trust this report's transcript; (c) listen to / spot-check `debug/sounds/m27-example-tone.wav`;
  (d) read `tests/system/hbf1xv_m27_replay_determinism_system_test.cpp` in full to independently
  judge the flat-RAM-driver-program design correction's soundness; (e) full 34-row backlog
  re-review (zero status changes expected, C5's new note only).
