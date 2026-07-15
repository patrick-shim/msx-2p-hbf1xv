# M31 openMSX A/B Disposition — YM2413 FM-Synthesis DSP Depth (backlog E1)

- Milestone: M31 (RELEASE CANDIDATE, tag target v1.0.32), DEC-0035 run
- Spec: `docs/m31-planner-package.md` §2.7 (disposition decided IN ADVANCE by the planner,
  the M29 §2.5 / E2-M28 precedent)
- Date: 2026-07-09

## 1. Audio-sample A/B: N/A BY DESIGN

Recorded verbatim per the planner package §2.7:

> **Audio-sample A/B: N/A BY DESIGN.** openMSX runs the cycle-exact GPL NukeYKT core with the
> very attack/rate tables this project may not possess; a live sample diff would show
> DIVERGENCE BY DESIGN, not a defect.

This project's M31 synthesis engine is grounded exclusively in
`references/fact-sheets/Yamaha YM2413 FM Chip.md` formulas (§4 logsin/exp closed forms, §5
EG decay/release mechanism + closed form, §8 phase generation, §3 MUL/FB/KSL/SUS semantics,
§6 rhythm structure, §7 native rate + measured per-volume peaks) plus the disclosed
approximations named in `src/devices/audio/ym2413_synth.h`'s mandatory disclosure block
(attack curve, rate-52-59 anomaly, VIB PM table, AM counter rate, SD/HH/T-CY synthesis,
noise LFSR taps, KSL base curve, key-on phase reset, digital summing/ZOH). The attack-curve
data in particular exists in this repository ONLY inside the GPL pre-generated
`references/openmsx-21.0/src/sound/YM2413NukeYktTables.ii`, which was **never opened in any
form** (see the implementation report's non-opening attestation). A sample-level diff against
openMSX would therefore compare *this project's disclosed approximations* against *the GPL
table data it deliberately refuses to read* — divergence by design, not evidence of a defect.
Correctness is instead proven by the fact-sheet-formula oracles (the mechanism-vs-livediff
pattern, exactly as E2/M28 proved its gate against cited constants with A/B honestly N/A):

- S1: logsin/exp tables vs independent in-test double-precision recomputation
  (`tests/unit/devices/audio_ym2413_synth_tables_unit_test.cpp`).
- S2: pitch vs `f = F-Num · 2^BLOCK · MUL_eff · 49716 / 2^19`; volume vs the §7
  andete-measured peak series; FB exact-doubling property; half-sine rectification
  (`audio_ym2413_synth_operator_unit_test.cpp`).
- S3: decay/release EXACT native-sample durations from §5's own closed form
  `cycles = (rate<60) ? (1<<(14-(rate/4)))·s[rate&3] : 63`, s = {127,102,85,73}
  (`audio_ym2413_synth_envelope_unit_test.cpp`).
- S4: the ×2 rhythm double-output law asserted as an EXACT sample-for-sample factor
  (`audio_ym2413_synth_rhythm_lfo_unit_test.cpp`).
- S5: the zero-YM2413 byte-identity hard oracle + exact 9:8 decimation
  (`machine_audio_mixer_fm_unit_test.cpp`).
- S6: machine-level pitch/release/rhythm via real CPU `OUT (#7C)/(#7D)` over the M11 bus
  (`hbf1xv_m31_fm_synth_system_test.cpp`).

This disposition does not gate closure (planner §2.7).

## 2. CPU-visible surface: NO new A/B required — UNCHANGED, with git-diff proof

The YM2413 is write-only with no readback and no status register (fact-sheet §8;
`io_read` = open-bus 0xFF, A-M17-5). M31's synthesis engine adds **zero CPU-visible state**:
the register file, two-port #7C/#7D protocol, latch/mask semantics, reset behaviour, and
open-bus reads are byte-for-byte the M17 contract (the M17 A/B — register-file + architectural
parity, both PASS, `docs/m17-parity-trace-diff.md` — remains the standing evidence), and the
E2 write-timing gate (M28) is untouched and still defaults OFF.

Git-diff proof captured on the implementation tree (2026-07-09):

```
$ git diff --stat v1.0.31 -- src/devices/cpu src/core \
    src/frontend/audio_pacer.cpp src/frontend/audio_pacer.h \
    src/frontend/psg_audio_pump.cpp src/frontend/psg_audio_pump.h \
    src/frontend/psg_audio_dump.cpp src/frontend/psg_audio_dump.h \
    src/devices/audio/psg_ym2149.cpp src/devices/audio/psg_ym2149.h \
    src/devices/audio/scc_wavetable.cpp src/devices/audio/scc_wavetable.h
(empty — zero lines of output)
```

The full M31 change set touches only:
`src/devices/audio/ym2413_synth.{h,cpp}` (NEW), `src/devices/audio/ym2413_opll.{h,cpp}`
(additive: owned synth, `advance_cycles`/`fm_sample`/`synth()`, key-edge notify on accepted
data writes — no change to any CPU-facing method's observable behaviour),
`src/frontend/machine_audio_mixer.{h,cpp}` (additive overload),
`src/frontend/sdl3_audio_presenter.{h,cpp}` (additive overload),
`src/frontend/sdl3_app.cpp` (passes the machine's OPLL to the mixer), `CMakeLists.txt`,
`tests/CMakeLists.txt`, and NEW test files. The four existing YM2413 test files and the M29
mixer unit test are byte-unmodified; both remained green in the RC suites. `src/machine/` is
untouched (the `ym2413()` accessor already existed).

Since no CPU-visible path was touched, the planner's conditional bounded architectural A/B
re-run is NOT triggered.

## 3. Cross-reference recordings (DEC-0030 obligations, planner §2.8)

Recorded in-code in `src/devices/audio/ym2413_synth.{h,cpp}` and in the S2 unit test:

- **D-M31-1 (frequency constant)**: fMSX (`references/fmsx-60/source/EMULib/YM2413.c:193-195,
  212-214`) computes `Freq = (3125·Frq·(1<<Oct))>>15` ≡ F-Num·2^BLOCK·**50000**/2^19 — a
  rounding simplification (~10 cents sharp). Arbitrated to the fact-sheet/openMSX
  3.579545 MHz / 72 = **49716 Hz**. Bonus: fMSX independently corroborates the /2^19
  accumulator scaling (A-M31-4).
- **D-M31-2 (address-latch masking)**: fMSX masks at latch time (`Latch = V&0x3F`); this
  project (M17, A-M17-3) latches unmasked and masks at use — observationally equivalent,
  settled in M17, no change.
- **D-M31-3 (rhythm-off key-bit clearing)**: fMSX zeroes $0E bits 0-4 whenever bit5=0
  (YM2413.c:170) — a register-file mutation neither the fact-sheet nor openMSX supports.
  Arbitrated to fact-sheet/openMSX: registers store what is written; drum key bits simply
  have no effect while rhythm mode is off (unit-asserted:
  `MelodyCh7_RegisterFileKeepsKeyBit_DM31_3`).
- **A-M31-3 KSR semantics cross-check** (READ for semantics only, no table transcription):
  openMSX `YM2413Okazaki.cc` `updateRKS` computes `rks = freq >> patch.KR` with
  `KR = KSR ? 8 : 10` over a `block<<9 | fnum` packing — bit-identical to this project's
  `Rks = (BLOCK·2 + F-Num[8])` (KSR=1) / `>> 2` (KSR=0) construction. fMSX contains no
  operator-level KSR handling at all (its OPLL DSP is `#if 0`-disabled MIDI-style mapping,
  planner §2.8) — no disagreement possible.
