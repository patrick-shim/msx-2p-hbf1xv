# M29 QA Sign-off — KonamiSCC Mapper + SCC Wavetable Audio Chip (backlog G1)

- Milestone: M29 (`docs/m29-planner-package.md`, DEC-0035 autonomous run; tag target v1.0.30)
- QA Owner: MSX QA Agent · Date: 2026-07-09
- Inputs reviewed: `docs/m29-planner-package.md`, `references/fact-sheets/Konami SCC.md`,
  `docs/m29-implementation-report.md`, `docs/m29-parity-trace-diff.md`, all new/modified source
  and test files (working tree vs `v1.0.29`), grounding sources under `references/`.
- Verification posture: every developer headline was independently re-derived (clean rebuilds
  in fresh build dirs, A/B harness re-executed against the QA-built binary, reference files
  read directly). Nothing below is repeated from the implementation report without re-checking.

## 1. Regression Scope

Behavior-affecting milestone. Affected surfaces:

- NEW: `src/devices/audio/scc_wavetable.{h,cpp}`, `src/devices/cartridge/
  cartridge_konami_scc_rom.{h,cpp}`, `src/frontend/machine_audio_mixer.{h,cpp}`.
- MODIFIED (additive): `cartridge_mapper_type.{h,cpp}`, `cartridge_slot.cpp`,
  `hbf1xv_machine.{h,cpp}` (scc_chip accessor), `sdl3_audio_presenter.{h,cpp}`,
  `sdl3_app.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`.
- Regression-critical invariants: DEC-0033 audio pacing untouched; zero-SCC audio output
  byte-identical to v1.0.29; M19 cartridge subsystem untouched; no CPU/core edits (therefore
  no ZEXALL sweep — DEC-0035 defers it to M31's QA gate; correctly NOT run, verified).

## 2. Regression Matrix Status

| Check | Method (QA-independent) | Result |
|---|---|---|
| Headless clean rebuild + fast subset | Fresh dir `build-qa-m29`, configure+build from scratch, `ctest -C Debug -LE m24_slow_full_sweep` | **PASS — 159/159** (matches claim; was 154 pre-M29, +5 new) |
| SDL3-ON clean rebuild + fast subset | Fresh dir `build-qa-m29-sdl` (+`CMAKE_PREFIX_PATH` to local SDL3 install), dummy video/audio drivers | **PASS — 168/168** (all pre-existing M26/M27 frontend tests green against the reworked presenter) |
| Zero-touch diff (AC 10) | `git diff v1.0.29` over `src/devices/cpu/`, `src/core/`, `audio_pacer.*`, `psg_audio_pump.*`, `psg_ym2149.*`, `ym2413_opll.*`, all six M19 mapper device files | **EMPTY — PASS**; under `tests/` only `tests/CMakeLists.txt` + the one disclosed mapper-type test edit are modified (CPU-timing-oracle test files untouched) |
| `src/main.cpp` probe reversion | `git diff HEAD -- src/main.cpp` | **EMPTY — PASS** (byte-identical to HEAD) |
| Zero-SCC byte-identity oracle | Read `tests/unit/frontend/machine_audio_mixer_unit_test.cpp` case 1; verified the reference side is the LITERAL pre-M29 loop (`pump_samples` + hard-coded `* 400` clamp, independent PSG twin), not shared with the mixer's code path; `PsgAudioPump`/`PsgYm2149` byte-frozen at v1.0.29, and `pump_samples` is a loop over `pump_one_sample` (the mixer's call) in that frozen file — so "both sides share a bug" collapses to "pre-M29 behavior," which is exactly what the oracle must pin | **SOUND — PASS** |
| Presenter pacing contract (R-M29-5) | Line-read of the `sdl3_audio_presenter.cpp` diff: `AudioPacer::plan()` call and inputs unchanged; full-batch pump preserved; push-side trim = first `samples_to_push` pairs, byte-equal to pre-M29 | **PASS** |
| SCC device semantics vs references | Read `references/openmsx-21.0/src/sound/SCC.cc` (De Schrijder block 1–32, `adjust()` 347–353, `setFreqVol` 374–415, `incr=(per<=8)?0:32` at 393, deform 417–464, read-as-write-0xFF 196–207) and `references/fmsx-60/source/EMULib/SCC.c` + `fMSX/MSX.c` (no `+1` at SCC.c:113, `V==0x3F` exact at MSX.c:1531, `(A&0xDF00)==0x9800` window, deform unmodeled) — all four §9 arbitration rows confirmed real; our code follows the arbitrated model on each | **PASS** |
| Mapper semantics vs `RomKonamiSCC.cc` | Line-compare: mirror directions (44–58, opposite of our M19 `cartridge_konami_rom.cpp:16-24` — contrast verified in both files), masked enable (110), both-effects fall-through (108–123), `(addr&0x1800)==0x1000` 0x800-wide decode (hand-checked: 0x5400→switch, 0x5800→ignore), `addr&0xFF` window mirroring, ROM mirror pages never expose SCC (keyed on raw range, 70–96) | **PASS** |
| Unit/integration/system tests assert each claim | Read all five new test files; AC 2/3/4/5/6 items individually present (incl. both mirror directions, 0x3F/0xBF/0x3E, both-effects, 0x9900 mirror, deform-read side effect via mapper `mem_read`, literal `amp_out()==640`) | **PASS** |
| openMSX A/B re-run | Regenerated fixtures (hash-identical — deterministic), re-ran `tools/openmsx-m29-scc-parity.ps1` against the QA-built emulator vs real WSL openMSX 19.1 | **EMPTY DIFF over 140 instructions — PASS**; AF columns additionally diffed identical by hand; self-checks: empty-side → exit 2 (BLOCKED), corrupted-AF → exit 1 (DIVERGENCE), both as required; the deform read fires via a REAL in-trace `LD A,(0x9FE0)` on both sides |
| Audio-sample A/B | N/A by design, recorded in advance per planner §2.5 (resampler-policy comparison, not chip semantics); chip semantics locked by the measured-formula unit oracles | **ACCEPTED (precedented)** |
| Aleste-2 bonus smoke | Viewed all three PNGs myself: f240 = MSX logo "Main RAM:64Kbytes"; f500 = loader banner "Aleste 2 ROM version v8 / Konami8 mapper / Press any key to start."; f899 = intro running ("AFTER 20YEARS / WE BATTLED AGAINST DIA51…") | **MATCHES CLAIMS** (non-gating, disclosed repro-dump fixture) |
| Evidence gates | `validate-assets.ps1` → True (re-run); `docs/asset-checksums.txt` diff vs v1.0.29 = timestamp line only (verified) | **PASS** |
| Disclosed test edit audit | The M19 `parse("KonamiSCC")==nullopt` case was annotated "G1, deliberately not an MVP value" — an explicit scope-boundary marker; M29 closes G1, so converting it to positive cases (with new `KonamiSCC+`/`SCC` negative guards preserving the nullopt contract) is the legitimate boundary-closure it claims to be; every other pre-existing assertion intact | **LEGITIMATE — PASS** |
| Game-keyed logic sweep | `grep -riE "aleste|nemesis|manbow|snatcher|metalgear"` over `src/` → comments only (mapper host-cart documentation); the only `Mode` enumerator is `Real` (R-M29-9) | **CLEAN — PASS** |
| Ledger | G1 → DONE (M29) + new G5 row in `deferred-backlog.md`; RESP-M29-002; `current-phase.md` reflects Ready-for-QA | **PASS** |

### Adversarial discrimination audit (two substantive tests re-derived)

1. **Frequency semantics** (`Devices_SccWavetable_Unit`): the pair
   `FreqRewrite_ResetsIntraByteCounter_NoStepAt99` / `FreqRewrite_StepLandsExactlyAtPeriodPlus1`
   (period 99: 99 cycles → no step, +1 cycle → step) EXACTLY separates the arbitrated
   `(period+1)` divisor from fMSX's no-`+1` (which steps at 99). `PeriodEight_CounterStopped`
   fails under fMSX zero-only-stop arithmetic (1,000,000 cycles at step-10 would leave
   position 111111 % 32 = 7, not 0), and `PeriodNine_Runs` guards the boundary from the other
   side. Genuine discriminators — **PASS**.
2. **Mixing law** (`Frontend_MachineAudioMixer_Unit` + De Schrijder cases): a naive
   sum-then-truncate mixer yields amp_out 644 (75>>4=4), not 640 — the literal oracle catches
   it; the 635 case catches truncation-toward-zero vs arithmetic-shift floor ((-15)/16=0 vs
   (-15)>>4=-1); a clamp-less additive mixer wraps 39,080 to −26,456 as int16 — the saturation
   case catches it, with the 31,940 one-SCC no-clamp control proving the clamp is not hiding
   scale errors. Genuine discriminators — **PASS**.

## 3. Failures and Risk Ranking

No test failures. Findings:

- **F1 (Low — AC 7 letter gap).** AC 7 requires "additive cases in the existing CLI unit
  tests"; `tests/unit/machine/cartridge_cli_unit_test.cpp` still enumerates only the six M19
  values (no `KonamiSCC` row in its `--cart1-type` table). Mitigations already in evidence:
  the shared `parse_cartridge_mapper_type` (the single function both executables' CLI paths
  delegate to — verified in `cartridge_cli.cpp:41-58` and `sdl3_cli.cpp:80`) IS unit-tested for
  KonamiSCC (exact/lower/upper), the `sdl3_cli` delegation test remains valid, and the headless
  `--cart1-type KonamiSCC` path was exercised END-TO-END live in the QA A/B re-run
  ("--cart1 loaded (tests/parity/m29_scc.rom, KonamiSCC)"). Functional risk ≈ zero; the gap is
  test-coverage letter only.
- **F2 (Info — out-of-milestone working-tree edit).** `.claude/settings.json` carries a
  one-character model-name typo fix ("sonnect"→"sonnet") not part of M29's declared scope. No
  permissions change; benign. Coordinator should commit it knowingly (or split it) at tag time.
- **F3 (Info — disclosed reference divergence).** Rotation-shift rate uses the Pazos-measured
  `3.58MHz/(period+1)` per shift; openMSX's `deformTimer` ticks at /32 granularity. Properly
  disclosed (device header + report §9.1), fact-sheet-grounded (§10.1 "do not copy it"), and
  the A/B probe was deliberately made rotation-shift-independent. Accepted.
- **F4 (Info — brief-vs-artifact nuance).** The bus-level deform-read (`LD A,(0x9FE0)`) lives
  in the A/B trace probe (both emulators, real CPU read) and at the mapper seam in the unit
  test — not in the M29 integration test. The developer's report attributes it correctly; no
  misclaim.
- **F5 (Info — reproducibility note).** A from-scratch SDL3-ON configure needs
  `-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install` and `SDL3.dll` on PATH at test time
  (0xc0000135 otherwise). Matches the documented "needs SDL3Config.cmake on the prefix path"
  build-flow caveat; environment-only.

## 4. Required Fixes

1. **F1**: add additive `KonamiSCC`/`konamiscc` acceptance rows to
   `tests/unit/machine/cartridge_cli_unit_test.cpp`'s `--cart1-type` table (two-line, additive,
   no production code), re-run the headless fast subset, and record the green count — OR record
   an explicit AC-7-letter waiver in `agent-protocol/channels/decisions.md`. Per DEC-0035 this
   proceeds fix-re-confirm-then-tag WITHOUT a human stop.

## 5. Sign-off Decision

**CONDITIONAL PASS** — pass in substance on every gating criterion (all independently
re-derived: 159/159 + 168/168 clean rebuilds, sound byte-identity oracle, reference-grounded
device/mapper semantics on both behavior references, EMPTY-DIFF A/B re-run with working
adversarial self-checks, clean scope diff, legitimate disclosed test edit, correct slow-sweep
omission), conditioned ONLY on the F1 two-line CLI-test addition (or recorded waiver) before
the coordinator tags **v1.0.30**. No blocker-level gaps. G1 → DONE (M29) and the G5 remainder
row are correctly recorded.

---
*QA evidence generated this cycle: fresh builds `build-qa-m29/` + `build-qa-m29-sdl/`; A/B
artifacts `build-qa-m29/m29_scc_trace_{A,B}.txt`, `m29_scc_trace_diff.md`, self-check files
(QA-regenerated diff doc content matches `docs/m29-parity-trace-diff.md` modulo path
separators). No production code was written or modified by QA.*
