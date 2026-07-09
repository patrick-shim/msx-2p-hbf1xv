# M31 QA Sign-off — YM2413 FM-Synthesis DSP Depth (backlog E1) — RELEASE-CANDIDATE GATE

- Milestone ID: M31 (final milestone of the DEC-0035 autonomous run; tag target v1.0.32)
- QA Owner: MSX QA Agent
- Date: 2026-07-09
- Inputs reviewed: `docs/m31-planner-package.md`,
  `references/fact-sheets/Yamaha YM2413 FM Chip.md` (read in full),
  `docs/m31-implementation-report.md`, `docs/m31-parity-trace-diff.md`,
  `docs/m31-rc-zexall-log.txt`, full source of `src/devices/audio/ym2413_synth.{h,cpp}`,
  all 8 new test files (read in full), every M31 diff hunk vs v1.0.31, all ledger/state files.
- Regression Scope: new FM synthesis engine (18 slots, EG, phase, LFOs, rhythm, LFSR);
  additive `Ym2413Opll` surface; third `MachineAudioMixer` source; SDL3 presenter/app wiring;
  the entire pre-M31 audio pipeline (byte-identity contract); CPU/core/machine no-touch
  contract; RC full-suite + smoke matrix + health; ledger closure (E1→DONE, new E3, G1 note).
- Test Matrix Reference: planner package §3 (S1-S6) + §4 RC acceptance criteria 1-10.

---

## 1. THE HEADLINE CHECK — source-form review (R-M31-1 / acceptance criterion 2): **PASS**

Method: `references/openmsx-21.0/src/sound/YM2413NukeYktTables.ii` was **not opened by QA
either** (one-sided, source-form-only check, per the review protocol). Both new source files
were read in full and EVERY numeric literal audited against the three permitted classes.

Literal-array inventory actually found in `src/devices/audio/ym2413_synth.cpp`:

| Literal | Class | Grounding verified |
|---|---|---|
| `kMulX2 = {1,2,4,6,8,10,12,14,16,18,20,20,24,24,30,30}` | fact-sheet-printed | §3 MUL table (0→½ … F→15), ×2 form; matches the sheet's printed row exactly |
| `kEgSelect[4][8]` ({0,1,0,1,0,1,0,1} …) | fact-sheet-printed | §5's four printed 8-entry select tables, verbatim in the sheet |
| `kUnitsPerOctave = {0,128,64,256}` | fact-sheet-derived | direct unit conversion of §3's printed KSL 0/1.5/3.0/6.0 dB/oct (1 unit = 3/128 dB); mapping order checked against §3's 00/10/01/11 assignment — correct |
| logsin/exp tables | closed-form COMPUTED at construction | formulas only, no value literals (see recomputation below) |
| Scalars: attack k=2, AM 512/26, VIB 1024/8, LFSR taps (x^23+x^18, `>>5`/`<<22`), `>>3` channel scale, 0x7FFFF mask, 72/81 cycle constants | documented-choice / fixed-point plumbing | each carries its disclosure or in-code rationale |

Test-only literals: §7 measured-peaks series {255,180,…,1,1} (S2 test ONLY — grep confirms it
appears nowhere in `src/`); §4 BD patch row (S4 test ONLY, identical to shipped M17
`kRhythmBd`); §5 select tables + s={127,102,85,73} as in-test oracles; {1,2,3,4,5,6,7,9}
(pure floor(9k/8) arithmetic, recomputed by QA).

**No attack/EG-step-shaped table, no 8×8 PM values, no golden operator tables, no LFSR set
claimed hardware-exact — anywhere in the M31 tree.** The report §1 literal-array inventory
matches the files (one benign omission, finding F3 below).

Independent closed-form recomputation by QA (python, double precision):
- `logsin[0]=2137` (12-bit max, fits), `logsin[255]=0`, `exp2neg[0]=2048`, `exp2neg[255]=1027`
  — all match the formulas the code embeds and the S1 test asserts.
- exp∘logsin round-trip at i=64: 789 vs sin·2048 = 789.54 — within quantization.
- §5 closed form `cycles=(1<<(14-(rate/4)))·s[rate&3]`, s={127,102,85,73} (the s-constants are
  **printed in the fact-sheet §5 itself** — verified by direct read): rate 24→32512 samples =
  654.0 ms, 25→525.2, 26→437.7, 27→375.9, 40→40.9, **52→254 samples = 5.11 ms**, 55→2.94 —
  each reproduces the sheet's own printed Table III-7 value. The select-table fractions
  (4/8..7/8) are arithmetically consistent with s={127,102,85,73} (e.g. 127/(4/8)=254=2·127).
- Pitch hand case fnum=256/block=4/mul=1: dP=4096, native period exactly 128,
  f = 388.41 Hz = 256·2⁴·1·49716/2¹⁹ — matches the S2/S6 oracles.

Disclosure verification (criterion 3): the mandatory 9-item block IS present in
`ym2413_synth.h` (attack policy explicitly names `YM2413NukeYktTables.ii` as the
inaccessible source; rate-52-59; VIB PM; AM rate; SD/HH/T-CY; LFSR; KSL; A-M31-5 phase
reset; digital summing/width/ZOH) — plus report §7 and backlog row E3. All three places sighted.

## 2. Regression Matrix Status

| Gate | Method | Result |
|---|---|---|
| ZEXALL/ZEXDOC RC sweep | Durable log SIGHTED (`docs/m31-rc-zexall-log.txt`): 2,664 lines, genuine CTest LastTest.log structure (per-test blocks, ko-KR locale timestamps Jul 09 11:38→12:08, sweep elapsed 00:29:06 / 1746.82 s), lines 1924-25: ZEXALL **67/67 ok, 0 error markers, 5,764,169,474 instructions**; ZEXDOC byte-identical figures; 172× "Test Passed.", zero "failed". **Disposition: NOT re-run by QA** — the durable committed log + the standing `feedback_slow_test_cadence.md` rule make sighting sufficient at this gate; stated explicitly. | PASS (sighted) |
| Headless fast suite | FRESH dir `build-qa-m31/` (clean configure+build, zero compile errors) | **171/171** (33.84 s) |
| SDL3-ON fast suite | FRESH dir `build-qa-m31-sdl/` (SDL3 from `build/_sdl3_install`; dummy drivers; SDL3.dll placed beside test exes) | **180/180** (35.11 s) |
| Zero-YM2413 byte-identity oracle | Test source audited: reference side is the LITERAL v1.0.31 arithmetic (`pump_one_sample·400`, clamp — independent of the mixer), asserted for fm=nullptr AND real-but-silent-attached AND the 3-arg overload | PASS (and mutation-proven, below) |
| Synthesis correctness spot-checks | QA hand-verified: pitch case (above); rate-52 closed form 254 = 5.11 ms; release oracles tied to Table III-7 values recomputed by QA; BD==2× twin logic (same $36 register serves melody-vol and BD-vol readings, same counter phase → exact sample-for-sample ×2); ZOH pattern {1,2,3,4,5,6,7,9} = floor(9k/8) recomputed | PASS |
| Adversarial mutation A | `eg_granted_steps` divisor rate/4 → rate/3, rebuilt: envelope suite fails **9 cases** (all release-duration + counter-phase oracles) | discriminates |
| Adversarial mutation B | idle DC offset (`held_sample_ = acc + 1`), rebuilt: `SilentAttachedFm_ByteIdenticalToPreM31Arithmetic_HardOracle` fails | discriminates |
| Mutation cleanup | source restored **byte-identical** to pre-mutation backup (cmp), both suites re-run green | clean |
| Smoke matrix artifacts | ALL PNGs viewed by QA: bios-f210 (MSX2+ logo + "Main RAM:64Kbytes", DEC-0031 intact), metalgear-f1100 (real gameplay, Snake + trucks + LIFE/CLASS HUD), aleste-f500/f900 (loader + intro text), mg2-f1260 ("KONAMI PRESENTS 1990"), dos-f1000 (**"MSX BASIC version 3.0 / 23431 Bytes free / Disk BASIC version 1.0 / Ok"**) | PASS |
| WAV forensics | python-decoded: fmON 44.1 kHz stereo 3.37 s, **105,076 differing samples counted by QA** (== the claim; fmOFF is digital silence in the window, peak delta 900); mg2-scc-title 4.17 s, 61% non-zero, peak 14,212 | PASS |
| M31 asset-gated tests re-run | Probe: 48 reg-changes / **2 key-on edges** / 900 frames (A-M31-2 CONFIRMED); MG2: first SCC activity frame **1244**, enable bits **0x1E**, **0 FM key-ons** (A-M31-6); S6 system test all green | PASS (figures match report exactly) |
| A-M31-1 | QA python read: `bios/f1xvmus.rom` offset 0x18 = `4150524c4f504c4c` = "APRLOPLL" | VERIFIED |
| Scope no-touch | `git diff v1.0.31` EMPTY for `src/devices/cpu/`, `src/core/`, `src/machine/`, `audio_pacer.*`, `psg_audio_pump/dump.*`, `psg_ym2149.*`, `scc_wavetable.*`; the four YM2413 test files + M29 mixer test byte-unmodified (existence confirmed — no vacuous diff); E2 gate default still OFF (`write_timing_enforced_ = false`); no game-keyed logic anywhere in the diff | PASS |
| openMSX A/B | Disposition artifact `docs/m31-parity-trace-diff.md` reviewed: audio-sample A/B **N/A BY DESIGN** (planner §2.7, ratified in advance — a diff against the GPL table data this project refuses to read proves nothing); CPU-visible surface UNCHANGED (git-diff proof re-verified by QA; M17 A/B remains standing evidence; conditional re-run correctly not triggered) | PASS (recorded disposition) |
| Ledger | E1 → **DONE (M31)**; NEW row E3 verbatim-equivalent to planner §6.3 (attack data, rate-52-59 tables, PM/LFO widths, LFSR/SD-HH-TCY overrides, KSL breakpoints, damp phase, TDM DAC, optional ZOH upgrade; **OPEN, no owner, C1/D4 sourcing-blocker standard**); G1 cross-note (A-M29-4 RESOLVED by RC 7(d), no status change); current-phase/milestones refreshed; RESP-M31-002 appended (ISO-8601, honest no-REQ note) | PASS |
| RC health | `validate-assets.ps1` → True (7 BIOS, 3 ROMs incl. metalgear2_scc.rom); `docs/asset-checksums.txt` covers metalgear2_scc.rom; zero new TODO/FIXME (only the pre-existing vdp_frame_renderer hit); headless exe `--help` + 120-frame real-BIOS run exit 0; SDL3 exe `--help` + 300-frame hidden-window real-BIOS run exit 0 (fresh QA builds) | PASS |

## 3. Failures and Risk Ranking

No gate failures. Findings, ranked:

- **F1 (Low, process)**: `.claude/settings.json` modified vs v1.0.31 — a one-character typo
  fix (`"sonnect"` → `"sonnet"` in the model field). Not a permission/config-security change,
  but it is outside M31's declared scope. Coordinator should consciously include or exclude
  it at commit time.
- **F2 (Low, cosmetic)**: `tests/system/hbf1xv_m31_fm_synth_system_test.cpp` Stage-B comment
  says "RR=8 → rate 32 → 8128 native ticks"; with block 4 / F-Num[8]=1 / KSR=0, Rks=2 gives
  effective rate 34 → 5440 ticks. The assertion is a generous BOUND (silence within 16384
  samples) and remains valid; only the comment is imprecise.
- **F3 (Info)**: the report §1 literal inventory omits `kUnitsPerOctave = {0,128,64,256}`
  (KSL §3 unit conversion — same permitted-literal class; inventoried above by QA).
- **F4 (Info)**: the exp table is stored at 2048 scale rather than §4's "10-bit" structural
  wording — a documented fixed-point width choice (disclosure item 9 / R-M31-2), pinned by
  the volume/pitch oracles; not a fidelity claim.
- **F5 (Info, environment)**: SDL3-ON tests require SDL3.dll discoverability; QA used
  copy-beside-exe in the fresh dir. Not a product defect.
- **Pre-existing, not M31**: diskless-boot "Ok" banner sits on the drive-wait (report Known
  Issue 2) — QA confirms the M31 diff touches no FDC/boot/CPU path; stays on the future
  FDC-idle investigation list.

Blocking Issues: **none**.

## 4. Required Fixes

None gating. Recommended (non-blocking, may ride the commit or a later cycle): fix the F2
comment; add `kUnitsPerOctave` to the report inventory (F3) or accept this sign-off's
inventory as the corrected record.

## 5. Sign-off Decision

**PASS.**

**RC verdict: fit for the v1.0.32 production-candidate tag.** All ten RC acceptance criteria
hold on re-derived evidence: the source-form review (the run's highest governance risk) is
clean with the non-opening attestation corroborated by the absence of any transcribable-shaped
data; the ZEXALL/ZEXDOC RC checkpoint is satisfied by the sighted durable log (67/67, 0 errors,
both suites, the historical instruction count); both fast suites reproduce exactly in fresh
build directories (171/171, 180/180); the pre-M31 audio pipeline is provably unperturbed
(hard oracle + mutation evidence); every smoke/health artifact was independently opened,
decoded, or re-executed by QA. Sign-off Conditions: none beyond the coordinator's F1
include/exclude decision at commit time.
