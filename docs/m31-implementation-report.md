# M31 Implementation Report — YM2413 FM-Synthesis DSP Depth (backlog E1) — RELEASE CANDIDATE

- Milestone: M31 (final milestone of the DEC-0035 autonomous run; tag target v1.0.32)
- Spec: `docs/m31-planner-package.md` (S1-S6, acceptance criteria §4)
- Developer: MSX Developer Agent
- Date: 2026-07-09
- Baseline: M30 closed (DEC-0037, tag v1.0.31); headless fast 163/163, SDL3-ON fast 172/172;
  ZEXALL last clean at v1.0.28
- Status: IMPLEMENTATION COMPLETE — Ready for QA. All changes UNCOMMITTED.

---

## 1. THE NON-OPENING ATTESTATION (R-M31-1 — read this first)

**I attest that `references/openmsx-21.0/src/sound/YM2413NukeYktTables.ii` was NEVER opened,
read, grepped, previewed, or consulted in ANY form during this milestone** (the M30 SHA1.c
precedent). No tool invocation in this implementation cycle targeted that file. Beyond that
file, `references/openmsx-21.0/src/sound/YM2413NukeYKT.cc` was NOT opened this cycle either —
the planner package's §1.2/§2.1 line citations (its constexpr-lambda table construction at
:56-71 and its `#include "YM2413NukeYktTables.ii"` at :106-114) were relied on as
planner-verified facts rather than re-read next to the temptation. The only reference-source
reads performed by the developer this cycle were: (a)
`references/fmsx-60/source/EMULib/YM2413.c` grep for `KSR` (result: zero hits — fMSX has no
operator-level KSR handling; corroborates the planner §2.8 finding) and (b)
`references/openmsx-21.0/src/sound/YM2413Okazaki.cc:425-436` (`updateTLL`/`updateRKS`) — a
SEMANTICS-ONLY read for the A-M31-3 KSR cross-check (`freq >> (KSR ? 8 : 10)`), zero table
values involved.

**Every numeric constant and table in the new code comes from**: (a) the fact-sheet's own
formulas/printed tables (§5 select tables {0,1,0,1,0,1,0,1}/{0,1,0,1,1,1,0,1}/{0,1,1,1,0,1,1,1}/
{0,1,1,1,1,1,1,1}; §5 closed form s = {127,102,85,73}; §3 MUL table; §7 measured-peaks series
— test oracle only; §8 12/84 already shipped in M28), (b) closed-form math derived and
documented in-code (logsin/exp table build formulas; the 9:8 decimation arithmetic;
KSL/VIB/AM derived laws, each flagged as a disclosed approximation), or (c) already-shipped
M17 project code (the 15+3 ROM patch table, reused via `Ym2413Opll::rom_patch()`/
`rhythm_*_patch()` — not re-derived, not re-litigated). The QA source-form review should find
exactly these literal arrays in the new code and NO others: the §5 select tables, the §3 MUL
table (×2 form), the §7 measured-peaks series (in the S2 unit test only), the fact-sheet §4
BD patch row (in the S4 unit test only, for the ×2-law twin), and documented-choice scalar
constants (k=2 attack shift, 512/1024 LFO dividers, LFSR taps 23/18, kFmAmplitudeScale=5,
>>3 channel scale).

## 2. Milestone Target

Give the QA-signed M17 YM2413 register contract a real, deterministic, formula-grounded FM
synthesis engine (backlog E1's formulaically-derivable subset, ratified by DEC-0035 from the
M28 §2.3(a) finding); mix it as the THIRD audio source through M29's `MachineAudioMixer`;
run the FULL Release-Candidate gate (the run's only ZEXALL/ZEXDOC sweep, both configs' fast
suites, the 6-item smoke matrix, health re-check); close E1 with the named-remainder split-out
(new row E3) and the G1 cross-note.

## 3. Code Changes

| File | Change |
|---|---|
| `src/devices/audio/ym2413_synth.h` | NEW — the synthesis engine. Carries the MANDATORY DISCLOSURE BLOCK (9 numbered disclosed approximations, §2.4/§2.1: attack policy naming `YM2413NukeYktTables.ii` as the inaccessible source; rate-52-59 anomaly; VIB PM table; AM counter rate; SD/HH/T-CY; noise LFSR; KSL base curve; key-on phase reset A-M31-5; digital summing/width/ZOH). |
| `src/devices/audio/ym2413_synth.cpp` | NEW — closed-form table construction (`round(-log2(sin((i+0.5)π/512))·256)`, `round(2^(-j/256)·2048)`); §8 phase generation; §5 EG mechanism (one global counter, 18 operators); §2.4 attack approximation (`att -= (att>>2)+1` on granted steps; 0-3 infinite, 60-63 instant); feedback 2-deep average with the FB exponential ladder; KSR/Rks (A-M31-3); SUS→RR=5; EG-TYP; §6 rhythm (BD 2-op, TOM 1-op, SD/HH/T-CY approximations, exact ×2); AM/VIB LFOs; 23-bit LFSR; D-M31-1/D-M31-3 recorded in-code. |
| `src/devices/audio/ym2413_opll.{h,cpp}` | ADDITIVE ONLY: owned `Ym2413Synth synth_`; `advance_cycles(delta)` (one native tick per 72 cycles = exact 49716 Hz, ZOH); `fm_sample()`; debug `synth()` accessor; `reset()` also resets the synth; accepted data writes call `synth_.refresh_keys(*this)` (§2.6); D-M31-2 comment at `write_address`. The M17 two-port contract, latch/mask semantics, open-bus reads, and the M28 E2 gate (still default OFF) are byte-for-byte unchanged in behaviour. |
| `src/frontend/machine_audio_mixer.{h,cpp}` | ADDITIVE overload `mix_interleaved_stereo(psg, sccs, Ym2413Opll* fm, count)`; `kFmAmplitudeScale = 5` (documented presentation policy, §7 "blend ratio varies by machine"); existing 3-arg overload delegates with `fm=nullptr`, byte-behaviour identical (M29 unit test untouched, green). |
| `src/frontend/sdl3_audio_presenter.{h,cpp}` | ADDITIVE paced overload taking `Ym2413Opll* fm`; narrower overloads forward with nullptr; stale M26-era "YM2413 deliberately silent / E1 still open" doc block replaced with an honest history note (R-M31-7). |
| `src/frontend/sdl3_app.cpp` | Passes `&machine_.ym2413()` into the paced mixer path. |
| `src/frontend/sdl3_main.cpp` | Startup banner corrected: "PSG + SCC + YM2413 FM audio live — backlog E1 closed by M31" (the old text claimed E1 still open — would have been a false claim after this milestone). |
| `CMakeLists.txt` / `tests/CMakeLists.txt` | `ym2413_synth.cpp` added to `sony_msx_core`; 8 new test executables registered. |

Untouched (verified `git diff v1.0.31` EMPTY): `src/devices/cpu/`, `src/core/`,
`src/frontend/audio_pacer.*`, `src/frontend/psg_audio_pump.*`, `src/frontend/psg_audio_dump.*`,
`src/devices/audio/psg_ym2149.*`, `src/devices/audio/scc_wavetable.*`, all of `src/machine/`,
the four existing YM2413 test files, the M29 mixer unit test, and all CPU-timing-oracle test
files. Temporary evidence probe in `src/main.cpp` (M29 precedent) was reverted via
`git checkout -- src/main.cpp` and the tree rebuilt clean afterwards.

## 4. New Tests (8 executables, all headless-buildable, all green)

| Test | Slice | Key oracles |
|---|---|---|
| `devices_audio_ym2413_synth_tables_unit_test` | S1 | Both tables vs independent in-test double-precision recomputation; 12-bit range; monotonicity; endpoints; exp∘logsin round-trip within quantization bounds; phase-increment MUL table properties. |
| `devices_audio_ym2413_synth_operator_unit_test` | S2 | Pitch: 5 (F-Num, BLOCK, MUL) triples incl. MUL=0, 32 measured periods vs 2^19/dP within ±1.5 samples; volume peaks vs the §7 measured series (±2 LSB) + √2 ratio law + monotonicity; FB exact doubling + end-to-end FB0≠FB7 + byte-identical reruns; half-sine negative-half-exact-zero with positive half identical; never-keyed device exactly 0 forever. |
| `devices_audio_ym2413_synth_envelope_unit_test` | S3 | Release durations EXACT for rates 24/25/26/27/40/55 vs the independently-recomputed §5 mechanism AND tied to §5's closed form `(1<<(14-(rate/4)))·s[rate&3]` at event-period resolution; 60+ → 2 levels/sample (64 ticks); SUS forces rate 20 (not rate 4); first-segment-shorter (counter-phase 0 vs 100, both exactly matching the §5 sim, deterministic); attack: AR15 instant, AR0 never+silent, monotonic, terminating, higher-AR-never-slower (1..14); EG-TYP sustain-hold at SL boundary (16..17 for SL=2) vs percussive decay-to-idle. |
| `devices_audio_ym2413_synth_rhythm_lfo_unit_test` | S4 | BD == EXACTLY 2× the identical user-patch melody note on ch6, sample-for-sample over 3000 ticks; each of 5 drums keyed alone → non-silent + unrelated slots idle + rerun byte-identical (Yamaha §6 recommended setup); melody ch7 suppressed to exact silence when rhythm enables while its $27 key bit REMAINS set in the register file (D-M31-3); AM depth: loudest window peak 256, softest 145 (≈4.875 dB), ratio in [1.70,1.85]; VIB ±(F-Num>>7) zero-mean triangle law + phase returns to exactly 0 after a full LFO cycle. |
| `frontend_machine_audio_mixer_fm_unit_test` | S5 | **THE HARD ORACLE**: fm=nullptr AND real-but-never-keyed device both byte-identical to the literal v1.0.31 arithmetic over 2000 varying samples (and the 3-arg overload too); lock-step-twin mixing-law proof; constructed saturation (9 aligned carriers + max PSG = 36,320 → clamped 32767); exact 9:8 decimation counter pattern {1,2,3,4,5,6,7,9}(+9 repeating); three-source two-run determinism. |
| `machine_hbf1xv_m31_fm_title_probe_integration_test` | S6 | A-M31-1: `bios/f1xvmus.rom` offset 0x18 == "APRLOPLL" asserted on the local asset; the Aleste-2 FM-activity probe (900 scripted frames) — NON-GATING outcome, printed; probe determinism across two runs. Skip-gated per DEC-0016. |
| `machine_hbf1xv_m31_metalgear2_scc_integration_test` | S6/RC 7(d) | MG2 auto-ID via DB SHA1 (title/type/method asserted); boots under the real frame loop; **real SCC samples through the 3-source mixer (first activity frame 1244, enable bits 0x1E)**; A-M31-6 negative control: ZERO FM key-on edges; two independent fixed-length sessions byte-identical PCM. Skip-gated. |
| `hbf1xv_m31_fm_synth_system_test` | S6 | Real CPU `OUT (#7C)/(#7D)` programs over the M11 bus (three in-RAM stages, each to HALT): key-on → machine-level pitch vs the formula oracle in the output domain (12 periods vs 128·72/81 ±3); key-off → decay to EXACT digital silence within the §5 release bound (RR=8 → 8128 native ticks; final 4096 samples all zero); rhythm burst non-silent; ENTIRE session byte-identical across two independent runs. |

## 5. RC Gate Evidence (criterion 6-8; all outputs genuinely captured)

### 5.1 Full unfiltered headless suite INCLUDING the ZEXALL/ZEXDOC sweep

```
100% tests passed, 0 tests failed out of 172
hbf1xv_m24_zexall_system_test  Passed  1746.82 sec
ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
```

The invariant 67/67 groups with **0 error markers on BOTH suites** — first sweep since
v1.0.28, per DEC-0035 the run's only sweep. **Durable log: `docs/m31-rc-zexall-log.txt`**
(2,664 lines, copied from `build/Testing/Temporary/LastTest.log` immediately after the run).
The `feedback_slow_test_cadence.md` fast-cadence rule explicitly does not apply at this gate.

### 5.2 Fast suites, both configs (final post-revert, post-banner-fix runs)

- Headless (`build/`, `-LE m24_slow_full_sweep`): **171/171** (baseline 163 + 8 new), 32.09 s.
- SDL3-ON (`build-m31-sdl/`, dummy drivers, `-LE m24_slow_full_sweep`): **180/180**
  (baseline 172 + 8 new), 39.02 s.
- Both configs build with zero errors (headless full rebuild + SDL3 full build captured).

### 5.3 The 6-item smoke matrix (real frame loop; every PNG/WAV was generated AND read)

Artifacts produced via a TEMPORARY `--m31-rc-evidence` probe mode in `src/main.cpp`
(the M29 bonus-smoke precedent), reverted afterwards; conversions via the existing
`tools/frame-to-png.py` / `tools/audio-dump-to-wav.py`.

| # | Item | Result | Evidence (all READ/verified) |
|---|---|---|---|
| (a) | BIOS cold boot, logo phase → BASIC | PASS with note | `debug/frames/m31-rc-bios-f150/f210.png`: the MSX2+ boot logo with "Main RAM:64Kbytes" (DEC-0031 logo phase INTACT); `m31-rc-bios-f400/f520.png`: GRAPHIC1 BASIC screen phase (skyblue border 0x277F) per the green `machine_hbf1xv_boot_logo_integration_test` contract. The full "Ok" banner on a DISKLESS boot does not appear within 3000 frames (drive-wait; pre-existing, see Known Issues) — the REAL MSX-BASIC prompt banner is captured on this RC build via item (e). |
| (b) | Metal Gear boots/plays | PASS | Auto-ID: "Metal Gear" (Konami) via DB SHA1 919fa773…; `m31-rc-metalgear-f1100.png`: REAL GAMEPLAY — Snake sprite among trucks with the LIFE/CLASS HUD (scripted SPACE); `f1400.png` continues in-game. |
| (c) | Aleste 2 auto-ID + boots | PASS | Auto-ID: "Aleste 2" (KonamiSCC) via DB SHA1 e93d0840…; `m31-rc-aleste-f500.png`: loader "Aleste 2 ROM version v8 / Konami8 mapper / Press any key to start."; `f900.png`: intro text running ("AFTER 20YEARS / WE BATTLED AGAINST DIA51…"). |
| (d) | MG2 auto-ID + title + REAL SCC through the mixer | PASS — closes A-M29-4 | Auto-ID: "Metal Gear 2 - Solid Snake" (KonamiSCC) via DB SHA1 78560a5c…; integration test asserts non-zero SCC samples through `MachineAudioMixer` from frame 1244 with byte-identical two-session PCM; `m31-rc-mg2-f1260.png`: "KONAMI PRESENTS 1990"; `f1700/f2200.png`: the title/intro animation running; `debug/sounds/m31-mg2-scc-title.wav` (4.17 s of the mixed title audio, frames 1151-1400). |
| (e) | MSX-DOS disk → Disk BASIC (DEC-0034 shape) | PASS | Unattended cold boot with `disks/msxdos22.dsk`; `m31-rc-dos-f1000.png`: **"MSX BASIC version 3.0 / Copyright 1988 by Microsoft / 23431 Bytes free / Disk BASIC version 1.0 / Ok"** with the function-key row — the genuine MSX-BASIC prompt, re-confirmed on the RC build. |
| (f) | FM audible in a real title | **PASS — REAL-TITLE PATH (no fallback needed)** | A-M31-1: "APRLOPLL" verified at `bios/f1xvmus.rom` offset 0x18 (asserted in-test AND via a byte read: `4150524c4f504c4c`). A-M31-2 CONFIRMED: the probe observed Aleste-2 YM2413 activity — 48 register-change observations, **key-ons on 2 channels at frame ~698** (game start after the second scripted SPACE). Two IDENTICAL deterministic sessions, one mixing WITH the OPLL and one with fm=nullptr, first differ at interleaved index 1,026,062 == frame 698 — the FM contribution is demonstrably non-silent exactly at the real title's key-on. `debug/sounds/m31-fm-aleste-fmON.wav` vs `m31-fm-aleste-fmOFF.wav` (3.37 s window from that frame; differing SHA-256, playable). |

### 5.4 Health re-check (criterion 8)

- TODO/FIXME grep: 1 hit in `src/` — the pre-existing doc-comment in
  `vdp_frame_renderer.cpp:187` (identical count at v1.0.31 via `git grep v1.0.31`). **Zero new.**
- Executables: `sony_msx_headless --help` exit 0; default run prints the scaffold banner
  (exit 0); `sony_msx_sdl3 --help` exit 0; real-asset bounded run
  (`--bios-dir bios --hidden-window --max-frames 300`, dummy drivers) exit 0 with the FM-wired
  loop active (R-M31-6 sanity: no starvation/hang).
- `tools/validate-assets.ps1`: True — 7 BIOS files, 3 ROMs incl. `metalgear2_scc.rom`.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`: refreshed; now covers
  `metalgear2_scc.rom`.
- Both configs rebuilt clean AFTER the main.cpp probe revert.

## 6. Assumption Verifications (planner §1.3)

- **A-M31-1 VERIFIED**: `bios/f1xvmus.rom` bytes at 0x18-0x1F are literally `APRLOPLL`
  (PowerShell/python byte read + the in-test assertion).
- **A-M31-2 VERIFIED (CONFIRMED)**: Aleste 2 DOES write YM2413 key-ons when internal MSX-MUSIC
  is detected — 2 channel key-ons at frame ~698 of the scripted run. The RC 7(f) synthetic
  fallback was NOT needed (the synthetic register-driven demo still exists as the S6 system
  test's stages).
- **A-M31-3 ADOPTED + CROSS-CHECKED**: Rks construction bit-identical to Okazaki's
  `freq >> (KSR ? 8 : 10)` semantics; fMSX has no KSR handling (no disagreement possible).
- **A-M31-4 ADOPTED**: 19-bit accumulator; pinned empirically by the S2/S6 pitch oracles;
  fMSX's /2^19 independently corroborates (D-M31-1).
- **A-M31-5 ADOPTED**: key-on phase-reset + attack-from-current-attenuation, documented in the
  disclosure block (item 8) and deterministic; E3 carries the damp-phase truth.
- **A-M31-6 VERIFIED**: MG2 is SCC-only — ZERO FM key-on edges across the full 1345-frame
  session (asserted in the integration test; the free negative control).

## 7. Approximation-Disclosure Checklist (criterion 3 — QA verifies all three places)

1. `src/devices/audio/ym2413_synth.h` — the mandatory header disclosure block (9 items,
   attack policy names `YM2413NukeYktTables.ii` as the inaccessible source).
2. THIS REPORT — §1 (attestation) + this checklist; the attack curve, rate-52-59 handling,
   VIB PM values, AM counter rate, SD/HH/T-CY synthesis, LFSR taps, KSL breakpoints, key-on
   phase reset, and digital-summing/ZOH are approximations and are NOT claimed cycle-exact
   anywhere.
3. Backlog row E3 (`agent-protocol/state/deferred-backlog.md`, Section D) — the verbatim-
   equivalent remainder list per the package §6.3 draft.

## 8. Ledger / Protocol Updates (criterion 10, same-cycle)

- `agent-protocol/state/deferred-backlog.md`: **E1 → DONE (M31)** with the full closure note;
  **NEW row E3** (OPEN, no owner, C1/D4 standard); **G1 cross-note** (A-M29-4 resolved by RC
  7(d) evidence, no status change); new M31 trailer block with the §6.1 full-ledger
  re-affirmation.
- `agent-protocol/state/current-phase.md`: Active Phase → M31 IMPLEMENTATION COMPLETE, Ready
  for QA; M30 demoted to Prior phase (CLOSED, DEC-0037, v1.0.31).
- `agent-protocol/state/milestones.md`: M30 status → CLOSED (DEC-0037); NEW M31 block with
  scope/regression/evidence/status.
- `agent-protocol/channels/responses.md`: RESP-M31-002 appended (ISO-8601; honestly notes that
  no REQ-M31 entry existed in requests.md for this dispatch).
- `docs/m31-parity-trace-diff.md`: the §2.7 A/B dispositions (audio-sample N/A BY DESIGN;
  CPU-visible surface unchanged with the git-diff proof; conditional A/B not triggered).

## 9. Known Issues / Residuals (honest, none gating per the package)

1. **E3 remainder (license/sourcing-blocked)** — attack-curve exactness, rate-52-59 tables,
   exact PM/LFSR/KSL data, damp phase, TDM DAC, optional band-limited resampling. OPEN, no
   owner, C1/D4 standard.
2. **Diskless-boot BASIC banner**: without a mounted disk image the boot sits on the
   drive-detection wait (GRAPHIC1 BASIC screen phase, blank blue, ≥3000 frames observed) and
   the "Ok" banner does not appear; with `disks/msxdos22.dsk` the genuine prompt appears
   (item 5.3(e)). PRE-EXISTING: M31's diff touches no FDC/boot/CPU path (`git diff v1.0.31`
   empty there); consistent with the C5/DEC-0034 history (the drive/boot handshake territory).
   Not an M31 regression; recorded for a future FDC-idle/timeout investigation.
3. **Probe granularity**: the FM-activity probes poll the register file at frame boundaries —
   sub-frame key pulses shorter than one frame could in principle be missed by the PROBE
   (synthesis itself processes every accepted write). Sufficient for the evidence purpose;
   disclosed.
4. **ZOH imaging** (§2.5 disclosed simplification) — accepted; E3 item 8 records the optional
   quality upgrade.
5. `build-m31-sdl/` is a new untracked build directory (SDL3-ON config for this cycle), and
   the background full-suite console log lives in the session scratchpad; the durable ZEXALL
   evidence is the committed `docs/m31-rc-zexall-log.txt`.

## 10. QA Handoff

Recommended QA verification path:
1. Source-form review per acceptance criterion 2 (the §1 attestation + the literal-array
   inventory listed there).
2. Fresh-build both configs; headless FULL unfiltered ctest (expect 172/172 with the ~29-min
   sweep; compare `docs/m31-rc-zexall-log.txt`); SDL3-ON fast suite (expect 180/180 under
   dummy drivers).
3. Verify the three disclosure places (§7), the four untouched YM2413 test files + M29 mixer
   test (`git diff v1.0.31 -- tests/...` empty), and the §3 no-touch git-diff proof.
4. Re-run the two asset-gated M31 integration tests and the S6 system test; optionally
   regenerate any smoke artifact (the probe code is in this report's history; artifacts are
   deterministic).
5. Listen to `debug/sounds/m31-fm-aleste-fmON.wav` vs `fmOFF.wav` (the audible FM difference)
   and `m31-mg2-scc-title.wav` (real SCC title music through the mixer).
