# M27 QA Sign-off — Production Hardening + Debug/Test Tooling

- Milestone ID: M27
- QA Owner: MSX QA Agent
- Planner package: `docs/m27-planner-package.md`
- Implementation report: `docs/m27-implementation-report.md`
- Dispatch: REQ-M27-003 (`agent-protocol/channels/requests.md:1387-1395`)
- Coordinator's own independent verification: RESP-M27-002 (`agent-protocol/channels/responses.md:1890`)

All findings below were independently reproduced this cycle from the current working tree —
nothing is a re-statement of the developer's or coordinator's transcripts without direct
re-verification.

---

## 1. Regression Scope

M27 is infrastructure/tooling scope — it closes zero backlog rows. Four work items, all
introspection/frontend/peripheral-adjacent, none touching `src/devices/cpu/` or `src/core/`:

1. Full CPU/memory/VRAM state-dump/CPU-trace/event-log CLI wiring via a new headless
   `--debug-session` mode (`src/main.cpp`) + additive `Sdl3AppConfig`/`sdl3_cli` fields.
2. Real, decoded PSG audio capture to file (`src/frontend/psg_audio_dump.{h,cpp}` +
   `tools/audio-dump-to-wav.py`), genuinely reusing M26's `PsgAudioPump`/`PsgYm2149`.
3. Keystroke-sequencing/scripted-input automation (`src/peripherals/msx_key_names.{h,cpp}` +
   `src/machine/input_script.{h,cpp}`), wired via `--input-script` on both executables.
4. Debug event-log CLI wiring (shared with item 1) + an adversarially-validated,
   byte-for-byte replay-determinism system test.

Regression surface reviewed: the full M1-M26 suite (both build configurations), the new M27
unit/integration/system tests, the CMake build graph (headless vs. SDL3-gated placement), the
committed audio evidence, and the full 34-row deferred-backlog registry. Per the milestone's
standing cadence directive (`feedback_slow_test_cadence.md`), the slow
`hbf1xv_m24_zexall_system_test` was evaluated for necessity via independent `git diff`, not run
by default (see §2).

## 2. Regression Matrix Status

| Check | Method | Result |
|---|---|---|
| Slow-test-skip cadence check | `git diff v1.0.26 --name-only -- src/devices/ src/peripherals/ src/core/` (independently run) | Empty (untracked new files invisible to tag-diff, as expected) |
| Slow-test-skip cadence check | `git status --porcelain -- src/devices/ src/peripherals/ src/core/` (independently run) | Only `?? src/peripherals/msx_key_names.cpp` / `.h` — zero modified pre-existing file |
| CPU/core zero-touch | `git diff v1.0.26 --name-only -- src/devices/cpu/ src/core/` (independently run) | Empty | 
| **Conclusion** | Per R-M27-6's precise interpretation, the slow test is genuinely unnecessary this cycle — **QA did not run it**, consistent with the standing directive and the developer's/coordinator's own finding, independently reproduced a third time. |
| Headless clean rebuild | `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF && cmake --build build --config Debug` | Succeeds |
| Headless fast subset | `ctest --test-dir build -C Debug --output-on-failure -LE m24_slow_full_sweep` | **140/140 passed**, 3.81s — independently reproduced, exact match to report |
| SDL3-ON clean rebuild | `cmake -S . -B build/sdl3-on -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=.../_sdl3_install && cmake --build build/sdl3-on --config Debug` | Succeeds |
| SDL3-ON fast subset | `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ctest --test-dir build/sdl3-on -C Debug --output-on-failure -LE m24_slow_full_sweep` | **149/149 passed**, 7.23s — independently reproduced, exact match to report |
| Every pre-existing M26 SDL3 test still green | Read output of the SDL3-ON run above | `frontend_sdl3_app_headless_integration_test`, `frontend_sdl3_video_presenter_pixel_integration_test`, `frontend_sdl3_audio_presenter_integration_test`, `frontend_sdl3_input_mapper_integration_test`, `frontend_sdl3_cli_session_integration_test`, `hbf1xv_m26_sdl3_system_test` all Passed, unmodified |
| `write_register()` private-visibility finding | Direct read of `src/devices/audio/psg_ym2149.h` | Confirmed: `private:` at line 103 precedes `write_register()` at line 141. Finding (a) is accurate. |
| `--audio-dump-demo` uses public API | Direct read of `src/main.cpp:870-901` | Confirmed: uses `psg.write_address()`/`psg.write_data()` only, never the private `write_register()`. |
| PPI keyboard-port grounding for the driver program | Direct read of `src/devices/chipset/ppi_8255.h`/`.cpp` | Confirmed: `#A9` = port B keyboard-row read, `#AA` = port C (bits0-3 row-select), exactly as the test's driver program uses. |
| "A" key = (row 2, col 6) | `grep '"A"' src/peripherals/msx_key_names.cpp` | Confirmed: `{"A", 2, 6}` |
| Replay-determinism test read in full | `tests/system/hbf1xv_m27_replay_determinism_system_test.cpp` | Read end to end (see §3 for judgment) |
| Event-log format/oracle math | Direct read of `src/machine/hbf1xv_machine.cpp:458-483`, `src/machine/debug_event_log.cpp` | Hand-verified: 16-instruction not-pressed path → 18 lines (1 Reset+16 Instr+1 Halt); 11-instruction pressed path → 13 lines. Both match the test's asserted oracles exactly. |
| Real two-process `--debug-session` launch | QA launched `sony_msx_headless.exe --debug-session bios 3000 --event-log <name>.log` **twice**, independently, as two separate process invocations | Both printed `steps=3000 halted=0 final_pc=456 elapsed_cycles=22825`; `cmp` reported identical; SHA-256 of both files: `2f7b430925d1e6a8090d0362f49112c00a1a1e17f97c74b3c375b6fb764381c8` (136,977 bytes each) — **exact match** to the developer's and coordinator's independently-claimed hash |
| Committed WAV decoded | QA decoded `debug/sounds/m27-example-tone.wav` directly via Python's `wave`/`struct` modules (raw PCM, not just file-type check) | 44,100 stereo frames (88,200 int16 samples), values alternate strictly between `-32768` and `-1`, 13,953 toggles, non-silent — exact match to claimed figures |
| PCM-scaling formula sanity | Hand-computed `psg_raw_sum_to_pcm16(0)` and `(31)` from the documented formula in `psg_audio_dump.cpp` | `0 → -32768`, `31 → -1` (integer-division floor) — matches the two observed WAV sample values exactly, confirming the formula is genuinely driving the committed evidence, not a placeholder |
| Cross-consistency completeness (71 entries) | `grep -c` on `sdl3_input_mapper_key_names_consistency_integration_test.cpp`, `sdl3_input_mapper.cpp`, `msx_key_names.cpp`; also read the test's own runtime completeness check (`every_scancode_map_entry_covered`) | All three sources show exactly 71 entries; the test's own runtime loop additionally cross-checks against the real `scancode_map().size()`, not a hardcoded count |
| Zero YM2413/DSP touch | `git diff v1.0.26 --stat -- src/devices/audio/ym2413_opll.h src/devices/audio/ym2413_opll.cpp`; `grep -n "sample(" src/devices/audio/ym2413_opll.h` | Both empty — confirmed |
| `hbf1xv_machine.{h,cpp}` untouched (AC2) | `git diff v1.0.26 --stat -- src/machine/hbf1xv_machine.h src/machine/hbf1xv_machine.cpp` | Empty — confirmed zero new machine-level method was needed |
| `tools/mem-to-audio.py` untouched | `git diff v1.0.26 --stat -- tools/mem-to-audio.py` | Empty — confirmed |
| `audio-dump-to-wav.py --self-check` | QA ran directly | `SELF-CHECK: OK`, all 21 sub-checks PASS, golden SHA-256 match |
| R-M27-2 ordering (headless) | Direct read of `src/main.cpp:787-794` (`run_debug_session`) | `set_event_logging_enabled(true)` (line 791) precedes `cold_boot()` (line 794) |
| R-M27-2 ordering (SDL3) | Direct read of `src/frontend/sdl3_app.cpp:102-113` | `set_event_logging_enabled(true)` (line 109) precedes `cold_boot()` (line 113) |
| Default headless run path unchanged | `git diff v1.0.26 -- src/main.cpp` filtered for removed (`-`) lines | Zero removed/modified lines outside the diff header — purely additive |
| CMake placement (headless-buildable) | `grep -n` in `CMakeLists.txt`/`tests/CMakeLists.txt` | `psg_audio_dump.cpp`, `input_script.cpp`, `msx_key_names.cpp` all listed before the `if(SONY_MSX_ENABLE_SDL3)` guard; new headless tests registered before the guard, SDL3-gated tests after |
| Regression-guard test (unset fields = no-op) | Direct read of `tests/integration/frontend/sdl3_app_debug_session_integration_test.cpp:110-142` | Confirmed: asserts no debug-output directory is ever created and event logging stays disabled when all new fields are left at their `std::nullopt` default |
| 34-row backlog review | Direct read of `agent-protocol/state/deferred-backlog.md` in full | 34 rows present (B1-B9, C1-C10, D1-D7, E1-E2, F1-F2, G1-G4); zero status changes; only C5 gains a factual M27 cross-reference note (`grep -n "M27"` shows exactly one hit, on C5's row) — matches Acceptance Criterion 12's precise wording exactly (only C5, not E1) |

## 3. Failures and Risk Ranking

**No blocker- or high-severity findings.**

One judgment call independently assessed, per the dispatch's explicit instruction to evaluate
whether the flat-RAM-driver design correction is sound rather than a workaround:

- The replay-determinism proof's negative control could not use the real BIOS alone, because the
  real BIOS never reads the keyboard matrix within any bounded `step_cpu_instruction()`-only
  window (independently corroborated against this project's own pre-existing backlog **C5**
  disclosure, `agent-protocol/state/deferred-backlog.md` row C5, which independently states the
  same fact from an unrelated M16-era investigation). The chosen fix — a small, 21-byte,
  hand-disassembled Z80 driver program that reads the real `#AA`/`#A9` PPI ports and branches on
  the result — is grounded in real, already-tested hardware wiring (`Ppi8255`, confirmed by direct
  source read) and in an already-established project precedent (`run_parity_trace()`'s own
  `map_flat_ram()` technique, pre-existing since M10/M19). The test's hand-computed instruction/
  line-count oracles (18 lines vs. 13 lines, a 5-instruction delta matching the 5 skipped NOPs
  exactly) were independently re-derived by QA from the raw driver-program bytes and matched
  exactly. This is a genuine, principled fix that makes the negative control actually discriminate
  — not a weakening of the proof. **Judged sound.**

No other residual risk was found above Low severity. Two items are worth naming for completeness,
neither gating:

- **Low, non-blocking.** The planner package's own §3.9 narrative text suggested E1 might also
  receive a factual cross-reference note (alongside C5); the actual `deferred-backlog.md` update
  only touches C5's row. This is NOT a discrepancy against the actual acceptance criterion — §4
  Acceptance Criterion 12 explicitly says "C5's row gains a factual, non-status-changing
  cross-reference note only" (singular, C5 only) — the delivered backlog file matches the hard
  acceptance criterion exactly. Recorded only so a future reader isn't confused by the planner
  narrative's looser phrasing elsewhere in the package.
- **Low, non-blocking, already disclosed.** The `--audio-dump-demo`'s single-channel-only tone
  naturally exercises only the `{0, 31}` subset of the documented `[0,62]` raw-sum range (channels
  B/C silenced) — independently confirmed via the WAV decode (values are exactly `-32768`/`-1`,
  the `psg_raw_sum_to_pcm16(0)`/`(31)` outputs, never approaching `62`'s `+32767` endpoint). This
  is honestly disclosed in the implementation report's Known Issues and does not affect the unit
  test's own full-range boundary coverage (verified present in `psg_audio_dump_unit_test.cpp` via
  the passing `frontend_psg_audio_dump_unit_test` case).

## 4. Required Fixes

None. No blocker-level or high-severity gap was found; both Low-severity notes above are
informational only and require no code or documentation change before closure.

## 5. Sign-off Decision (Pass | Conditional Pass | Fail)

**Pass** (clean, unconditional).

Every substantive claim in `docs/m27-implementation-report.md` and RESP-M27-002 was independently
reproduced from the current working tree this cycle, not taken on trust: both build configurations
rebuilt and both fast-subset counts reproduced exactly (140/140, 149/149); the slow-test-skip
cadence git-diff independently re-run with the identical result; the `write_register()`
private-visibility finding confirmed by direct header read; the real headless executable launched
twice by QA itself as two separate process invocations, producing byte-identical event logs whose
SHA-256 matches the developer's and coordinator's independently-claimed hash exactly; the committed
WAV independently decoded at the raw-PCM level (not just file-type-checked) and confirmed genuinely
non-silent and oscillating with the exact claimed sample/toggle counts; the key-name
cross-consistency table's completeness independently confirmed at 71/71 entries across all three
source locations; zero YM2413/DSP-shaped code confirmed; the replay-determinism proof's flat-RAM
driver-program design correction read in full and judged sound on hardware-grounding and
oracle-math re-derivation, not merely accepted on the developer's word; the full 34-row backlog
review confirmed with zero status changes. No blocker, high, or medium-severity finding of any kind
was identified.

Recommend the coordinator proceed to close M27 and tag v1.0.27 without further QA iteration.
