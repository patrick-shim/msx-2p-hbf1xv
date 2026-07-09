# M29 Implementation Report — KonamiSCC Mapper + SCC Wavetable Audio Chip (backlog G1)

- Milestone: M29 (`docs/m29-planner-package.md`, DEC-0035 autonomous run; tag target v1.0.30)
- Developer: MSX Developer Agent · Date: 2026-07-09
- Status: **IMPLEMENTATION COMPLETE — Ready for QA** (all changes UNCOMMITTED per the dispatch)
- Grounding: `references/fact-sheets/Konami SCC.md` ("SCC fact-sheet") — the package's own
  delivered precondition; openMSX 21.0 `SCC.cc`/`RomKonamiSCC.cc` as primary behaviour reference
  and fMSX 6.0 `EMULib/SCC.c` as cross-check (read only, never copied — license isolation).

## 1. Milestone Target

Backlog G1's second half: the `KonamiSCC` external-cartridge mapper (the SCC-bearing sibling of
M19's plain `Konami`) plus the SCC 5-channel wavetable sound generator itself, wired into the
cartridge subsystem, the CLI, and the M26/M27 audio chain — with the DEC-0033 `AudioPacer`
exact-accounting byte-for-byte untouched, and the SCC-I/"SCC+" remainder split out as new
backlog row G5 (named remainder, planner §2.7 — NOT a partial close of G1).

## 2. Code Changes (slice by slice)

### S1 — `CartridgeMapperType::KonamiSCC` + CLI value

- `src/devices/cartridge/cartridge_mapper_type.h/.cpp` (additive): new enumerator + parse/
  to_string cases for the canonical openMSX display string `"KonamiSCC"`
  (`references/openmsx-21.0/src/memory/RomInfo.cc:24`). Both executables' CLI parse paths accept
  `--cartN-type KonamiSCC` automatically (both delegate to `parse_cartridge_mapper_type`;
  unrecognized values still error loudly — M19 policy unchanged).
- **Disclosed test edit (the ONLY pre-existing assertion changed this milestone):**
  `tests/unit/devices/cartridge/cartridge_mapper_type_unit_test.cpp`'s M19 case
  `Parse_KonamiScc_Nullopt_OutOfScopeMapper` (annotated "G1, deliberately not an MVP value") was
  an explicit SCOPE-BOUNDARY marker, not a behavioural contract; M29 is precisely the milestone
  that closes G1, so it is converted into positive parse cases (exact/lower/upper), with new
  negative guards (`KonamiSCC+`, bare `SCC` alias) keeping the "unrecognized → nullopt" contract
  sharp. Every other pre-existing assertion in that file — and every other pre-existing test
  file in the repository — is byte-for-byte unchanged (R-M29-6).

### S2 — `SccWavetable` (`src/devices/audio/scc_wavetable.{h,cpp}`, new)

Plain-SCC ("Real") mode only, reached solely through the owning mapper's memory window (openMSX
`RomKonamiSCC` owns `SCC scc;` shape). Implemented per SCC fact-sheet §3–§7/§10:

- 256-byte register map: waveforms 0x00–0x7F R/W (ch4 write also lands in ch5's playback copy —
  no independent ch5 RAM); freq/vol/enable block 0x80–0x8F with the ×2 mirror at 0x90–0x9F
  (write-only, reads 0xFF); 0xA0–0xDF dead (0xFF); deformation register 0xE0–0xFF with the
  Pazos **read-acts-as-write-0xFF** side effect (`read()` non-const; `peek()` side-effect-free).
- Generator: position steps once per **(period+1) master cycles** (integer modular
  accumulation; the `+1` is the fact-sheet §9.1 arbitration — fMSX omits it); **period ≤ 8
  stops the counter** (NYYRIKKI, §9.2 arbitration); held output `(int8 wave[pos]*vol4)>>4`
  refreshed only at position steps and at period writes (NYYRIKKI restart + immediate refresh);
  volume/wave writes therefore latch at the next step; disabled channels contribute 0 but keep
  phase running.
- Mixing: the De Schrijder law — `amp_out() = 640 + Σ ((SampleValue*Vol) AND #7FF0)/16` with the
  low-4-bit drop BEFORE summation (arithmetic shift, C++20-guaranteed); `sample()` exposes the
  AC sum (~±600) for the mixer.
- Deformation: bits 0/1 period masking applied at frequency-write time (bit1 wins; matches
  openMSX `SCC.cc:385-392`, documented in-header — a deform change alone does not retroactively
  re-mask a latched period); bit5 restart-on-freq-write + rotation-timer resync; bits 6/7
  rotation with read-only wave RAM, incl. the plain-SCC ch4-at-ch5's-period quirk in all-rotate
  mode and the bits6+7 "only ch1–3 rotate, ALL read-only" configuration. Pazos's bits6+7
  data-bus-noise anomaly is documented, NOT emulated (planner §1.3).
- **Rotation time base (A-M29-6, disclosed in-header):** an internal master-cycle accumulator
  advanced by `advance_cycles()` only. Rotation shift rate is the fact-sheet's measured
  `3.58MHz/(period+1)` per shift — a deliberate, disclosed divergence from openMSX's /32 tick
  granularity, which the fact-sheet itself rules a resampler artifact ("do not copy it", §10.1).
- **reset() (A-M29-5, disclosed in-header):** the enen power-on state (wave RAM 0xFF, volumes
  15, enable 0, deform 0, periods 0, positions 0, held outputs refreshed); openMSX's
  powerUp-vs-soft-reset split is collapsed into this single deterministic state, consistent
  with `CartridgeSlot::load()`'s reset-once contract.
- **Mode-awareness (R-M29-9):** dispatch is structured over an `enum class Mode { Real }` with a
  single enumerator — SCC-I Compatible/Plus (row G5) is additive later; no Plus/Compatible
  behaviour exists or is claimed anywhere.

### S3 — `CartridgeKonamiScc` (`src/devices/cartridge/cartridge_konami_scc_rom.{h,cpp}`, new)

Extends the M19 pattern (`CartridgeMapperDevice` + `CartridgeRomWindow`), grounded in
`RomKonamiSCC.cc` + SCC fact-sheet §2:

- `is_valid_image_size`: positive multiple of 8 kB; >512 kB ACCEPTED (real-chip 64-bank ceiling
  documented in-header; openMSX warns non-fatally).
- **No `set_block_mask(31)`** (plain-Konami's 256 kB rule does not apply): image-derived default
  fallback mask (A-M19-6 semantics preserved).
- `reset()`: banks 0,1,2,3; `scc_enabled_=false`; `scc_.reset()`. Constructor does not
  self-reset (M19 convention; `CartridgeSlot::load()` resets).
- `bank_switch`: **mirrors are the OPPOSITE of plain Konami** (R-M29-1): pages 2/3 also set
  window-slots 6/7; pages 4/5 also set window-slots 0/1 (`RomKonamiSCC.cc:44-58`).
- `mem_write` decode order: ignore outside [0x5000,0xC000); SCC window (when enabled)
  0x9800–0x9FFF → `scc_.write(addr&0xFF,v)`; `(addr&0xF800)==0x9000` → `scc_enabled_ =
  ((v&0x3F)==0x3F)` **with fall-through** to the bank check (both-effects rule);
  `(addr&0x1800)==0x1000` → bank switch (0x800-wide windows).
- `mem_read`: SCC window (when enabled) uses `scc_.read()` — NOT peek — so the deform-range read
  side effect fires; ROM mirror pages never expose the SCC window (keyed on the raw CPU range).
- Factory case added in `cartridge_slot.cpp` (size-validate → construct → reset → install;
  failure leaves prior state untouched — existing contract).

### S4 — Machine accessor (`src/machine/hbf1xv_machine.{h,cpp}`)

`devices::audio::SccWavetable* scc_chip(int slot_number)` (+ const overload): non-null exactly
when the named bay holds a `KonamiSCC` cartridge; nullptr for empty bays, other mapper types,
and invalid slot numbers — the v1.0.29 regression null (M25 default-nullptr precedent). NO new
clock consumer, NO `wire_bus()` change.

### S5 — `MachineAudioMixer` (`src/frontend/machine_audio_mixer.{h,cpp}`, new) + presenter/app wiring

- SDL3-independent (headless-buildable, unguarded core source list — M26-S5 precedent). Per
  planned sample: PSG advanced via the genuine, unmodified `PsgAudioPump` + each attached
  `SccWavetable*` (0..2) advanced by the SAME `kCyclesPerSample = 81` (the disclosed −3.6-cent
  integer simplification inherited, NOT "fixed" — DEC-0033 accounting untouched); then
  `pcm = clamp_int16(psg_raw*400 + scc_sum*12)` (SCC mono → both channels). `kSccAmplitudeScale
  = 12` is documented presentation policy: one SCC peaks at 24,800+7,140 = 31,940 (inside
  int16); two SCCs + max PSG reach 39,080 → **the clamp is required and unit-tested** (R-M29-4).
- `Sdl3AudioPresenter`: `pump_and_push_paced()` keeps its exact `AudioPacer::plan()` call and
  inputs (samples_to_pump remains a pure function of `total_elapsed_cycles`; the full batch is
  always pumped, push-side trimming unchanged); per-sample production delegated to the mixer. A
  new 3-arg overload takes the SCC sources; **the pre-M29 2-arg signature is preserved
  verbatim** as a zero-SCC forwarding overload, so every pre-existing SDL3 test compiles and
  passes UNMODIFIED. `pump()` accessor preserved (now returns the mixer's pump); a
  `static_assert` pins `kAmplitudeScale == kPsgAmplitudeScale`.
- `Sdl3App::run_one_frame()`: queries `machine_.scc_chip(1/2)` each frame and passes them to the
  presenter (nullptr ⇒ byte-identical pre-M29 output).
- `src/frontend/audio_pacer.{h,cpp}` and `src/frontend/psg_audio_pump.{h,cpp}`: **byte-for-byte
  untouched** (verified, §5).
- The optional M27 audio-dump extension (planner §2.4 last bullet) was NOT taken (explicitly
  developer-discretion/non-gating; `serialize_psg_audio_dump()` untouched).

### S6 — System test, A/B harness, ledger

- `tests/system/hbf1xv_m29_scc_system_test.cpp`, `tools/gen-m29-scc-probe.py`,
  `tools/openmsx-m29-scc-parity.ps1`, `docs/m29-parity-trace-diff.md`, ledger updates (§7).

### Build registration

`CMakeLists.txt`: +`scc_wavetable.cpp`, +`cartridge_konami_scc_rom.cpp`,
+`machine_audio_mixer.cpp` (all in the unguarded core list). `tests/CMakeLists.txt`: five new
test targets (M29 block).

## 3. Unit Test Results

New suites (all green; counts within the gate runs of §5):

| Test | Coverage highlights |
|---|---|
| `devices_audio_scc_wavetable_unit_test` | enen power-on state; wave R/W; ch4→ch5 playback copy; ×2 freq/vol mirror; 12-bit assembly + high-nibble mask; write-only/dead reads 0xFF; period≤8 stop; period-write restart (counter reset at exactly (period+1)) + immediate refresh; wave-write latency; volume-latch-at-next-step; enable gating with phase running; **the literal De Schrijder oracle: five channels at +1/vol15 ⇒ amp_out()==640** (+715/+635 companions incl. signed-floor semantics); deform bits 0/1 (bit1 wins; masking-at-freq-write-time semantics asserted explicitly); bit5 restart; bits 6/7 rotation (measured-rate shift, ch4-at-ch5's-period quirk, read-only, same-value-rewrite no-resync, change-resync); deform-read-acts-as-write-0xFF (peek side-effect-free); two-instance determinism |
| `devices_cartridge_konami_scc_rom_unit_test` | size validation (>512 kB accepted); no self-reset; reset layout + **both mirror directions asserted (opposite of plain Konami)**; canonical + mid-window (0x5400/0xB400) + window-top decode, out-of-window (0x5800) ignored; <0x5000 and ≥0xC000 ignored; image-derived block mask (no 31 override); enable 0x3F/0xBF vs 0x3E incl. mid-window decode; both-effects 0x9000 write; SCC window routing incl. the 0x9900 mirror, disabled-SCC ROM readback, deform-read side effect via the mapper, ROM-mirror-pages-never-expose-SCC; reset clears latch+chip; determinism |
| `frontend_machine_audio_mixer_unit_test` | **THE HARD ORACLE: zero SCC sources ⇒ byte-identical to the literal pre-M29 presenter arithmetic over a 2000-sample varying PSG sequence**; hand-computed one-SCC case (720 everywhere, mono on both channels, generator advance verified); **constructed saturation: max PSG (62) + two max SCCs (595 each) = 39,080 ⇒ clamped 32,767**, with the one-SCC 31,940 no-clamp control; slot-2-only source; two-run determinism |
| `devices_cartridge_mapper_type_unit_test` (extended) | KonamiSCC parse (exact/lower/upper), 7-type round trip, exact label, `KonamiSCC+`/`SCC` rejected (disclosed M19 marker conversion, §2-S1) |

## 4. Integration / System Test Results

- `machine_hbf1xv_m29_konami_scc_integration_test` (M29-S4): real Z80 `OUT (#A8)`/`LD (nn),A`
  traffic over the M11 bus against BOTH bays (#A8=0xD7/0xEB routing pages 1+2): bus-driven bank
  switches (canonical + mid-window) with mirror-direction assertions; SCC enable → wave/freq/
  vol/enable writes → wave readback over the bus incl. the 0x9900 mirror and dead-region 0xFF;
  `scc_chip()` nullability matrix (empty/invalid/plain-Konami/unload ⇒ nullptr; per-bay
  independence); empty-slot open-bus byte-identity guard. **PASS.**
- `hbf1xv_m29_scc_system_test` (M29-S6): cold boot + synthetic KonamiSCC cart whose **in-image
  Z80 driver** (fetched from the cartridge over the bus) banks page 3, enables the SCC, writes
  waveform/period/volume/enable, HALTs; three frames of the DEC-0034 loop shape
  (`step_cpu_instruction()` to the boundary + `on_vsync_boundary()`); the real
  `MachineAudioMixer` then pumps 512 samples ⇒ hand-computed oracle (720 in every PCM slot),
  byte-identical across two fully independent runs. **PASS.**

## 5. Evidence Gates (all run, real captured output)

| Gate | Result |
|---|---|
| `tools/validate-assets.ps1` | `Asset validation result: True` (7 BIOS files, 2 ROMs) |
| `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | regenerated; **content identical to v1.0.29** (timestamp-only diff) |
| `cmake --build build --config Debug` (headless) | clean (no errors; pre-existing C4819 codepage warnings only) |
| `ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure` | **159/159 passed** (was 154 pre-M29; +5 new) |
| `cmake --build build/sdl3-on --config Debug` (SDL3-ON) | clean |
| `ctest --test-dir build/sdl3-on -C Debug -LE m24_slow_full_sweep` under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy` | **168/168 passed** (every pre-existing M26/M27 frontend test green UNMODIFIED against the reworked presenter) |
| ZEXALL/ZEXDOC slow sweep | **explicitly NOT run** (DEC-0035: M31 QA gate only; additionally the `feedback_slow_test_cadence.md` mechanical trigger cannot fire — the zero-touch diff below proves no CPU/core file changed) |
| Zero-touch (AC 10) | `git diff v1.0.29 --stat` over `src/devices/cpu/`, `src/core/`, `src/frontend/audio_pacer.*`, `src/devices/audio/psg_ym2149.*`, `src/devices/audio/ym2413_opll.*`, and all six M19 mapper device files: **EMPTY** |

## 6. openMSX A/B Evidence (planner §2.5 / AC 8)

- **A-M29-3 verified in source BEFORE running** (the package's hard precondition):
  `references/openmsx-21.0/src/CartridgeSlotManager.cc:455` (cart-command help,
  `-romtype <romtype> : specify the ROM mapper type`),
  `references/openmsx-21.0/src/config/HardwareConfig.cc:60` (`valueArg("-romtype", mapper)`),
  `references/openmsx-21.0/src/memory/RomInfo.cc:24` (canonical name), plus the live M19
  `-carta <file> -romtype 8kB` precedent. Forcing syntax: `-carta <file> -romtype KonamiSCC`.
- **Result: ARCHITECTURAL + CONTENT PARITY — EMPTY DIFF over 140 instructions** against real WSL
  openMSX 19.1 on `Sony_HB-F1XV` (`tools/openmsx-m29-scc-parity.ps1` →
  `docs/m29-parity-trace-diff.md`). Probed facts (each AF-visible): both KonamiSCC-specific
  mirror directions; 0x800-wide decode (0x5400 works, 0x5800 ignored); both-effects 0x9000
  write; masked enable (0x3F, 0xBF enable; 0x3E disables); wave write/readback; **the 0x9900
  window mirror (A-M29-1 — fMSX would serve ROM there)**; write-only/dead regions;
  **deform-range read-as-write-0xFF fired via a REAL CPU read on both sides** (openMSX `debug
  read` is peek-semantics and could not probe it), then its read-only side effect confirmed via
  a blocked wave write against a deliberately uniform (rotation-shift-independent, timing-safe)
  waveform. The driver self-normalizes all bank state first, so nothing depends on reset
  defaults surviving the reference side's real BIOS boot.
- Adversarial comparator self-check: empty-side → exit 2 (BLOCKED) ✓; corrupted-AF → exit 1
  (DIVERGENCE) ✓.
- **Audio-sample comparison: N/A by design**, recorded in advance per the package's §2.5
  disposition verbatim (openMSX exposes no Tcl introspection of its resampled SCC stream; a
  waveform diff would compare resampler policies, not chip semantics — which are instead locked
  by the De Schrijder/NYYRIKKI/Pazos unit oracles). No bare-`reset` memory-content comparison
  was performed anywhere (DEC-0028 discipline noted in the harness).

## 7. Bonus real-ROM smoke: Aleste 2 under KonamiSCC (coordinator-directed)

Via a TEMPORARY probe mode in `src/main.cpp` (reverted afterward with
`git checkout -- src/main.cpp`; headless target rebuilt clean post-revert): real BIOS cold boot
with `roms/aleste.rom` loaded as `KonamiSCC` into slot 1, 900 frames of the DEC-0034 loop shape,
SPACE (`keyboard().set_key(8,0,...)`) held at frames 300–314 and 600–614, frame dumps converted
via `tools/frame-to-png.py` and READ (evidence in `debug/frames/`):

- `m29-aleste-f240.png` — the MSX boot logo ("Main RAM:64Kbytes").
- `m29-aleste-f500.png` — **the loader BOOTED under KonamiSCC**: "Aleste 2 ROM version v8 /
  **Konami8 mapper** / Please do not sell. / Press any key to start." (contrast the coordinator's
  pre-probe finding that under ASCII16 it prints "Ascii16 mapper").
- `m29-aleste-f899.png` — **the game STARTED** after the scripted keypress: the intro text
  ("AFTER 20YEARS / WE BATTLED AGAINST DIA51…") is running.

Disclosed honestly: this is a multi-mapper repro dump (softwaredb-identified by the coordinator,
KonamiSCC-labeled); this smoke shows its KonamiSCC loader path boots and starts under this
project's new mapper — a mechanical, non-gating cross-validation, not a gameplay-depth claim.
No game-keyed behaviour was added anywhere (universal-fixes-only discipline).

## 8. Ledger Updates

- `agent-protocol/state/deferred-backlog.md`: **G1 → DONE (M29)** with a full closure note;
  **NEW row G5** (SCC-I/"SCC+" Sound Cartridge, OPEN, no owner, fact-sheet §8 +
  `MSXSCCPlusCart.cc` grounding — the §2.7 named-remainder split-out); new trailer block with
  the §2.6 34-row re-affirmation (E1/C10/F1/F2 renumbers deliberately NOT edited here, per
  DEC-0035's own instruction).
- `agent-protocol/state/current-phase.md` + `state/milestones.md`: M29 recorded as
  implementation-complete / Ready for QA.
- `agent-protocol/channels/responses.md`: RESP-M29-002 appended.

## 9. Known Issues / Assumptions / Residuals

1. **Assumption (disclosed divergence, in-header + §2-S2):** rotation-shift rate implemented at
   the fact-sheet's measured `3.58MHz/(period+1)` per shift; openMSX's implementation ticks this
   at 1/32 that granularity (its resampler artifact per fact-sheet §10.1). Verification action:
   if a future real-hardware capture of the rotation TEST modes appears, re-check; normal
   playback and every A/B-probed behaviour are unaffected (the A/B probe was deliberately made
   shift-independent).
2. **A-M29-6 (disclosed):** rotation readback advances only via `advance_cycles()` (frontend
   pump); CPU reads between pump batches see it frozen. Test modes only.
3. **A-M29-5 (disclosed):** single deterministic `reset()` = power-on state (openMSX's
   powerUp/soft-reset split collapsed).
4. Deform bits 0/1 masking applies at frequency-write time only (openMSX-matching; asserted by a
   dedicated unit case) — a deform write alone does not re-mask an already-latched period.
5. Pazos's bits6+7 data-bus noise anomaly: documented, deliberately NOT emulated (planner §1.3).
6. No real SCC-game ROM is present (`roms/` re-listed at implementation time: `aleste.rom`,
   `metalgear.rom` — A-M29-4 confirmed); all gating tests use synthetic images. The Aleste-2
   smoke (§7) is additional, non-gating, disclosed as a repro-dump fixture. A Nemesis 2 /
   Space Manbow class ROM remains the coordinator's relayed optional upgrade path.
7. SCC-I/"SCC+" — backlog row G5 (OPEN). No Plus/Compatible code path exists (R-M29-9 grep-clean
   by construction: the only `Mode` enumerator is `Real`).

## 10. QA Handoff

Recommended QA focus: (1) independently rebuild both configs and re-run both fast subsets;
(2) re-run `tools/openmsx-m29-scc-parity.ps1` (regenerate fixtures via
`python tools/gen-m29-scc-probe.py` first) and confirm the empty diff; (3) line-compare
`cartridge_konami_scc_rom.cpp`'s mirror directions against `RomKonamiSCC.cc:44-58` (R-M29-1)
and the enable compare against `RomKonamiSCC.cc:110` (R-M29-2); (4) verify the zero-touch diff
set independently (`git diff v1.0.29`), including `audio_pacer.*` byte-identity (R-M29-5);
(5) audit the one disclosed pre-existing-test edit (§2-S1); (6) confirm the slow sweep was
correctly NOT run (DEC-0035); (7) view the three `debug/frames/m29-aleste-f*.png` evidence
images. Sign-off artifact: `docs/m29-qa-signoff.md`; coordinator closes with tag v1.0.30.
