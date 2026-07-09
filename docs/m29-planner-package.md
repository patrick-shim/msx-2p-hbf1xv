# M29 Planner Package — KonamiSCC Mapper + SCC Wavetable Audio Chip (backlog G1)

- Milestone ID: M29
- Title: KonamiSCC mapper + Konami SCC 5-channel wavetable sound chip (closes backlog G1)
- Authorization: DEC-0035 (`agent-protocol/channels/decisions.md`, 2026-07-09) — three-milestone
  roadmap M29 (SCC) → M30 (Aleste-2/mapper) → M31 (YM2413 FM synthesis, RC), autonomous
  continuation; tag target **v1.0.30** on close.
- Spec Owner: MSX Planner Agent · Developer Owner: MSX Developer Agent · QA Owner: MSX QA Agent
- Grounding precondition (M28 planner's own, DEC-0035-ratified): a dedicated SCC fact-sheet
  produced FIRST — **DELIVERED this cycle**: `references/fact-sheets/Konami SCC.md` (cited below
  as "SCC fact-sheet §N"). All behavioural facts there are grounded in independent community
  measurements (De Schrijder 2003 / Pazos 2003 / NYYRIKKI 2005 / enen), with openMSX 21.0 as
  primary behaviour reference and fMSX 6.0 as independent cross-check; seven explicit
  openMSX-vs-fMSX disagreements recorded and arbitrated (SCC fact-sheet §9, DEC-0030 discipline).

## 1. Scope and Assumptions

### 1.1 Objective

Deliver the second half of backlog G1: a `KonamiSCC` external-cartridge mapper (the SCC-bearing
sibling of M19's plain `Konami` mapper) plus the SCC wavetable sound generator itself, wired
into the existing cartridge subsystem, the CLI, and the M26/M27 audio presentation chain — with
deterministic unit/integration/system evidence and an honest openMSX A/B disposition.

### 1.2 In scope (the IN set)

1. **`SccWavetable` sound device** — `src/devices/audio/scc_wavetable.{h,cpp}` (placement per
   `src/CLAUDE.md`: chip implementations live in `src/devices/`; sibling of
   `psg_ym2149.*`/`ym2413_opll.*`). Plain SCC ("Real") mode as shipped in Konami MegaROM carts:
   5 channels × 32-byte signed waveforms (ch5 mirrors ch4's waveform), 12-bit frequency
   dividers, 4-bit volumes, channel-enable register, deformation/test register, the measured
   amplitude/mixing formula. Spec in §2.2.
2. **`CartridgeKonamiScc` mapper** — `src/devices/cartridge/cartridge_konami_scc_rom.{h,cpp}`,
   extending the M19 Konami-family pattern (`CartridgeMapperDevice` + `CartridgeRomWindow`),
   owning an `SccWavetable` member (the `RomKonamiSCC` "owns a real `SCC scc;`" shape). Spec in
   §2.1.
3. **Additive plumbing**: `CartridgeMapperType::KonamiSCC` (canonical display string
   `"KonamiSCC"` per `references/openmsx-21.0/src/memory/RomInfo.cc:24`, same
   openMSX-vocabulary rule as M19's six), `CartridgeSlot::load()` factory case, machine-level
   SCC accessor(s) (§2.3).
4. **Audio mixing into the existing frontend chain** (§2.4): a second sound source joins the
   PSG in the pump/pacer pipeline. Hard constraint: DEC-0033's `AudioPacer` exact-accounting is
   untouched (zero edits to `src/frontend/audio_pacer.*`).
5. **CLI**: `--cart1-type KonamiSCC` / `--cart2-type KonamiSCC` via the existing
   `src/machine/cartridge_cli.*` + `src/frontend/sdl3_cli.*` parse path (no new flag — a new
   accepted VALUE for existing flags).
6. **Deterministic tests** per `tests/CLAUDE.md` (§4 test plans) and the openMSX A/B evidence
   plan (§2.5).
7. **Backlog bookkeeping**: full 34-row re-affirmation (§2.6); G1 → DONE (M29) at closure;
   SCC-I named-remainder row proposed (§2.7).

### 1.3 Explicitly out of scope (named, not silent)

- **SCC-I / SCC+ ("Plus"/"Compatible" modes, the RAM-based Konami Sound Cartridge)** — named
  remainder, recommended new backlog row G5 (§2.7). Rationale: it is a genuinely different
  cartridge DEVICE (RAM mapper, mode register at 0xBFFE/0xBFFF, 0xB800–0xBFFF window,
  independent ch5 wave RAM — SCC fact-sheet §8), not an increment on the KonamiSCC mapper; no
  known SCC-I software asset exists in `roms/`; and G1's real-world payoff (Konami SCC game
  carts) needs only the plain SCC. The `SccWavetable` register dispatch must be written
  mode-aware-ready (SCC fact-sheet §10.3) so G5 is additive later.
- Deform-register data-bus noise anomaly (Pazos's bits6+7 corruption note) — documented
  hardware anomaly, explicitly NOT emulated (SCC fact-sheet §6).
- ROM-database/auto-detection of KonamiSCC images (backlog G2 — M30's Aleste-2 probe may touch
  it, per DEC-0035; not here). `--cartN-type` stays explicit.
- Any change to `src/devices/cpu/`, `src/core/`, `src/frontend/audio_pacer.*`,
  `src/devices/audio/psg_ym2149.*`, `src/devices/audio/ym2413_opll.*`, and the six existing
  M19 mapper device files (`cartridge_{mirrored,generic8kb,generic16kb,ascii8kb,ascii16kb,
  konami}_rom.*`) — all byte-for-byte unchanged (verification: `git diff v1.0.29` at every
  gate).
- ZEXALL/ZEXDOC slow sweep — **not run this milestone** (§3 evidence gates; DEC-0035: the sweep
  runs only at M31's QA gate; additionally, the `feedback_slow_test_cadence.md` mechanical gate
  cannot fire because M29 touches neither CPU nor core files).

### 1.4 Assumptions (each with a verification action)

- **A-M29-1**: The SCC register window mirrors across the full 0x9800–0x9FFF range
  (`addr & 0xFF`). openMSX models this; fMSX decodes only 0x9800–0x98FF (SCC fact-sheet §9.5,
  arbitrated to openMSX with a disclosed hardware-decode inference). *Verify*: include a
  0x9900-vs-0x9800 readback probe in the live-Tcl A/B (§2.5).
- **A-M29-2**: SCC enable is the masked compare `(value & 0x3F) == 0x3F` (0xBF also enables);
  fMSX requires exact 0x3F (SCC fact-sheet §9.6). *Verify*: dedicated unit test + a 0xBF case
  in the A/B probe.
- **A-M29-3**: openMSX's CLI/Tcl supports forcing a ROM's mapper type (needed to load a
  non-databased image as KonamiSCC on the reference side). *Verify BEFORE running the A/B*: read
  `references/openmsx-21.0/src/CommandLineParser.cc` / `CartridgeSlotManager.cc` / the `carta`
  command implementation for the exact `-romtype` (or equivalent) syntax; if no forcing
  mechanism exists, fall back to any locally-present, legally-sourced real SCC-game ROM, or
  record the register-level A/B honestly BLOCKED with the checked citation.
- **A-M29-4**: No real SCC-game ROM is currently known-present (`roms/` = `aleste.rom`,
  `metalgear.rom` — Metal Gear is plain-Konami territory). All tests therefore use SYNTHETIC
  in-test images (M19 precedent), never claiming real-cartridge identity. *Verify*: developer
  re-lists `roms/` at implementation time; if the human has added an SCC title, it may be used
  as an additional non-gating smoke fixture (disclosed as such).
- **A-M29-5**: Collapsing openMSX's powerUp-vs-reset split into this project's single
  deterministic `reset()` (SCC fact-sheet §10.5) is a disclosed simplification consistent with
  `CartridgeSlot::load()`'s reset-once contract. *Verify*: document in the device header; QA
  checks the disclosure exists.
- **A-M29-6**: The SCC's internal time base for deform-rotation readback is advanced by
  `advance_cycles()` only (no machine-clock attach this cycle) — CPU reads between pump batches
  see rotation frozen at the last advance. A disclosed simplification affecting only the exotic
  rotation TEST modes, not normal playback. *Verify*: documented in the header; the rotation
  unit tests drive `advance_cycles()` explicitly. (If the developer instead prefers the E2
  `Ym2413ClockSource` X-pattern, that is an allowed, documented alternative — but the default
  recommendation is the simpler internal accumulator.)

## 2. Spec Summary

### 2.1 `CartridgeKonamiScc` mapper spec (SCC fact-sheet §2; RomKonamiSCC.cc grounding)

Extends the M19 pattern (`cartridge_konami_rom.{h,cpp}` is the template — read it first):

- `is_valid_image_size(size)`: `size > 0 && size % kBankSize == 0` (8 kB banks). Images larger
  than 512 kB are ACCEPTED (openMSX warns non-fatally, `RomKonamiSCC.cc:28-34`); document the
  real-chip 512 kB/64-bank ceiling in the header.
- **No `set_block_mask(31)` override** (that is plain-Konami's 256 kB rule). Keep
  `CartridgeRomWindow`'s image-derived default mask (mask-as-fallback-only, A-M19-6).
- `reset()`: `bank_switch(2,0); (3,1); (4,2); (5,3)`; `scc_enabled_ = false`; `scc_.reset()`.
  Constructor does NOT self-reset (M19 convention; `CartridgeSlot::load()` resets).
- `bank_switch(page, block)`: `window_.set_bank(page, block)`; **mirrors are the OPPOSITE of
  plain Konami** (`RomKonamiSCC.cc:44-58`, "the mirror behavior is different from RomKonami!"):
  pages 2/3 (0x4000–0x7FFF) ALSO set window-slots 6/7 (0xC000–0xFFFF); pages 4/5
  (0x8000–0xBFFF) ALSO set window-slots 0/1 (0x0000–0x3FFF).
- `mem_write(addr, value)`:
  1. Ignore `addr < 0x5000 || addr >= 0xC000`.
  2. If SCC enabled and `0x9800 <= addr < 0xA000`: `scc_.write(addr & 0xFF, value)`; return.
  3. If `(addr & 0xF800) == 0x9000`: `scc_enabled_ = ((value & 0x3F) == 0x3F)` — **and fall
     through** (no return) to step 4: the same write also bank-switches (both-effects rule,
     `RomKonamiSCC.cc:108-123`).
  4. If `(addr & 0x1800) == 0x1000`: `bank_switch((addr >> 13) & 0x07, value)`.
- `mem_read(addr)`: if SCC enabled and `0x9800 <= addr < 0xA000` → `scc_.read(addr & 0xFF)`
  (note: `read`, not `peek` — the deform-range read side effect must fire); else
  `window_.read(addr)`. The ROM mirror pages (e.g. 0x1800–0x1FFF) never expose the SCC window.
- `mapper_type()`: `CartridgeMapperType::KonamiSCC`.
- Debug/test seam: `const SccWavetable& scc() const` + non-const accessor, plus
  `scc_enabled()` — mirrors the M19 `window()` seam.

### 2.2 `SccWavetable` device spec (SCC fact-sheet §3–§7, §10)

Not a `core::IoDevice`/`core::MemoryDevice` — it is reached only through the mapper's memory
window (openMSX shape: `RomKonamiSCC` owns `SCC scc;`). Public surface:

- `void reset()` — the enen-measured power-on state (fact-sheet §7): wave RAM all 0xFF, volumes
  15, enable 0, deform 0, periods 0, positions 0, held outputs refreshed. (A-M29-5 disclosure.)
- `std::uint8_t read(std::uint8_t offset)` / `void write(std::uint8_t offset, std::uint8_t v)`
  / `std::uint8_t peek(std::uint8_t offset) const` (peek = no side effects, for dumps/tests) —
  the 256-byte plain-SCC map of fact-sheet §3: wave 0x00–0x7F R/W (ch4 write also lands in
  ch5's playback copy; rotation-shifted reads in rotation modes; read-only under
  deform bits 6/7); freq/vol/enable 0x80–0x8F with the ×2 mirror at 0x90–0x9F (write-only;
  reads 0xFF); 0xA0–0xDF dead (0xFF); 0xE0–0xFF deform — **read acts as write 0xFF and returns
  0xFF** (Pazos).
- Register semantics: 12-bit period assembly with high-nibble masking; period write resets the
  channel's intra-byte counter and refreshes the held output (NYYRIKKI restart); deform bit5
  additionally resets position to 0 on period writes; volume writes take effect at the next
  position step (NYYRIKKI latency); enable register bits 0–4.
- `void advance_cycles(std::uint64_t delta)` — cycle-exact generator (fact-sheet §10.1): per
  channel, position steps once per `(period+1)` master cycles via modular accumulation
  (integer math only); frozen when `period <= 8`; held output = `(int8 wave[pos] * vol) >> 4`
  refreshed at each step; disabled channels contribute 0 but keep phase running. Deform
  bits 0/1 mask the effective period (4-bit/8-bit modes, bit1 precedence); deform rotation
  (bits 6/7) advances off the same internal accumulator (A-M29-6), with the plain-SCC ch4-at-
  ch5's-period quirk in all-rotate mode.
- `std::int32_t sample() const` — the AC mix sum (range ≈ ±600); plus
  `std::int32_t amp_out() const` = `640 + sum` for direct De Schrijder oracles (fact-sheet §5).
- Introspection for tests: `wave(ch, i)`, `period(ch)`, `volume(ch)`, `enable_bits()`,
  `deform()`, `position(ch)`.
- Mode-awareness: keep the dispatch structured so SCC-I Compatible/Plus maps can be added as a
  mode enum later (G5), but ONLY `Real` ships and no Plus/Compatible code path is claimed or
  tested this cycle.

### 2.3 Machine wiring (`src/machine/hbf1xv_machine.{h,cpp}`)

- `CartridgeSlot::load()` gains the `KonamiSCC` factory case (size-validate → construct →
  reset → install; failure leaves prior state untouched — existing contract).
- New machine accessor for the frontend/test layers, mirroring `psg()`'s shape:
  `devices::audio::SccWavetable* scc_chip(int slot_number)` returning the slot's SCC when that
  slot holds a `KonamiSCC` cartridge, else `nullptr` (both-slot support: the frontend queries
  slot 1 and slot 2 and mixes every non-null result — real hardware mixes both slots' sound-in
  lines). Zero behavior change when no SCC cart is loaded (nullptr, regression-null — the M25
  default-nullptr attach-point precedent).
- NO new clock consumer, NO `wire_bus()` change (the cartridge slots are already wired at all
  4 pages of primary slots 1/2 since M19).

### 2.4 Audio-mixing design (the DEC-0033 constraint) — decision

**Decision: mix at the frontend pump layer; `AudioPacer` and `PsgAudioPump` stay byte-for-byte
untouched.**

- New SDL3-independent `src/frontend/machine_audio_mixer.{h,cpp}` (headless-buildable, added to
  the same unguarded core source list as `psg_audio_pump.cpp` — the M26-S5 precedent):
  - Owns the per-sample loop: for each planned sample, advance the PSG by `kCyclesPerSample`
    (via the existing `PsgAudioPump`, genuine reuse) AND advance each attached
    `SccWavetable*` (0..2 of them) by the SAME `kCyclesPerSample`, then produce one interleaved
    S16 stereo pair:
    `pcm = clamp_int16(psg_raw * kAmplitudeScale + Σ scc_ac * kSccAmplitudeScale)`.
  - `kAmplitudeScale = 400` (existing, unchanged); recommended `kSccAmplitudeScale = 12`
    (SCC mono AC ≈ ±600 ⇒ ±7,200; worst case 62·400 + 7,200·n_scc: one SCC = 32,000 < 32,767;
    two SCCs can exceed int16 ⇒ the clamp is REQUIRED, documented, and unit-tested — a
    disclosed presentation-policy constant, same class as `kAmplitudeScale` itself). SCC is
    mono: the same contribution is added to both channels.
  - **Regression oracle (hard)**: with zero SCC sources attached, the mixer's PCM output is
    byte-identical to the pre-M29 presenter arithmetic (`psg_raw * 400` per channel) for any
    input sequence — dedicated unit test.
- `Sdl3AudioPresenter::pump_and_push_paced()` keeps its exact `AudioPacer::plan()` call
  (samples_to_pump remains a pure function of `total_elapsed_cycles`; SCC generator time, like
  PSG generator time, always tracks machine time) and delegates per-sample production to the
  mixer; it gains the SCC pointers from `Sdl3App` (which queries `machine_.scc_chip(1/2)` once
  per frame — cheap, and correct across cart load state).
- Pitch note: the SCC inherits the same disclosed integer `kCyclesPerSample = 81` (−3.6 cent)
  simplification as the PSG (documented in `sdl3_audio_presenter.h:44-61`); do NOT "fix" it
  here (DEC-0033 accounting untouched).
- Optional (developer discretion, non-gating): extend the M27 audio-dump path with an
  SCC-inclusive variant alongside `serialize_psg_audio_dump()` (never modifying the existing
  function's output format) — if done, it must reuse the mixer, not duplicate synthesis.

### 2.5 openMSX A/B evidence plan (honest scope)

- **Genuinely comparable — register/window/mapper behavior via the established live-Tcl
  technique** (`tools/openmsx-m25-rensha-parity.ps1` / `tools/openmsx-m28-c5-boot-parity.ps1`
  are the script patterns): load the SAME image (synthetic fixture or `roms/aleste.rom` as a
  disclosed, non-identity-claiming mechanical fixture — M19 precedent) as `KonamiSCC` on both
  sides (A-M29-3 syntax verification FIRST), then drive identical write sequences and compare
  readbacks: bank switches at 0x5000/0x7000/0x9000/0xB000 (+ mid-window addresses, e.g.
  0x5400, to exercise the 0x800-wide decode); SCC enable via 0x3F AND 0xBF, disable via 0x3E;
  wave write/readback at 0x9800–0x985F; the 0x9900-mirror probe (A-M29-1); mirror-page reads at
  0x0000/0xC000 regions (the KonamiSCC-specific mirroring); deform-read side effect if cleanly
  probeable via `debug read`. Memory-content comparisons follow the **DEC-0028 power-cycle
  rule** (`set power off/on`, never bare `reset`).
- **Audio-sample comparison — honestly N/A.** openMSX exposes no Tcl introspection of its
  resampled SCC sample stream, and its output chain (ResampledSoundDevice at 3579545/32 Hz
  input rate + host resampler) is architecturally different from this project's 81-cycle pump;
  a waveform diff would compare resampler policies, not chip semantics. The chip semantics are
  instead locked by the De Schrijder/NYYRIKKI-derived unit oracles (§4). This disposition is
  recorded here in advance per the E2/M28 "N/A by design, mechanism proven via cited
  constants" precedent.
- Evidence artifact: `docs/m29-parity-trace-diff.md` + `tools/openmsx-m29-scc-parity.ps1`.

### 2.6 Full 34-row backlog re-affirmation (DEC-0005 duty)

Rows re-read this cycle from `agent-protocol/state/deferred-backlog.md`:

| Rows | Disposition at M29 |
|---|---|
| B1–B9 | DONE (M15/M15/M17/M20/M18/M20/M19/M16/M14) — re-affirmed closed, untouched |
| C1, D4 | OPEN / IN-PROGRESS (M23 partial); UNBUILDABLE-WITHOUT-FABRICATION ruling stands (no independent source for the ~340-entry VDP slot tables); no milestone number; untouched by M29 |
| C2/C3/C4/C6/C7/C8/C9 | DONE (M23/M24/M15/M15/M18/M25/M26) — re-affirmed |
| C5 | DONE (DEC-0034) — re-affirmed; MSXDOS.SYS→`A>` handoff residual remains a named M31-era candidate |
| C10 | DEFERRED — row text says "→ M31"; per DEC-0035 the numeric owner shifts back (M32-era) and is to be re-recorded at ITS OWN kickoff, not edited here |
| D1/D2/D3/D5/D6/D7 | DONE (M21/M22/M22/M21/M21/M22) — re-affirmed |
| **E1** | DEFERRED; row text says "→ M30"; **renumbered M30→M31 by DEC-0035** ("backlog row to be updated at M31 kickoff" — deliberately NOT edited this cycle, per the decision's own instruction). M29 adds ZERO YM2413 DSP code (hard constraint carried from M26/R-M26-5) |
| E2 | DONE (M28) — re-affirmed; the default-OFF write-timing gate is untouched |
| F1/F2 | DEFERRED (rows say M32/M33; shift to M33/M34-era per DEC-0035, re-recorded at their own kickoffs) — untouched |
| **G1** | **THE M29 TARGET.** Mark **DONE (M29)** at closure (coordinator/developer applies the row edit in the closure cycle, with the SCC-I remainder split out as G5 — §2.7) |
| G2 | OPEN — untouched here; NOTE: DEC-0035's M30 (Aleste-2) may touch/partially close it |
| G3 | OPEN — untouched (no hot-plug work; cartridge composition remains cold-boot-time) |
| G4 | OPEN — untouched; KonamiSCC's closure removes it from the "not built" long tail implicitly (the row's own list never included KonamiSCC) |

No other backlog row is touched, opened, or narrowed by M29.

### 2.7 Named remainder → proposed new row G5 (recorded at closure)

**G5 — SCC-I ("SCC+") Konami Sound Cartridge**: RAM-based mapper (64/128 kB), mode register at
0xBFFE/0xBFFF (RAM-mode bits, SCC+ mode select), SCC+ window 0xB800–0xBFFF, independent ch5
waveform, Compatible-mode ch5 wave readback at +0xA0–0xBF, deform-bit7 inertness. Grounding
already in place: SCC fact-sheet §8 + `references/openmsx-21.0/src/sound/MSXSCCPlusCart.cc`.
Status OPEN, no owner (needs a real SCC-I software asset to be worth scheduling). This is a
named remainder split-out (the M14/M17/M18 contract-vs-depth precedent), NOT a partial close of
G1 — G1's own deliverable (KonamiSCC mapper + SCC chip as shipped in Konami carts) closes in
full.

## 3. Milestones (single milestone; slice plan with build order)

| Slice | Content | Depends on |
|---|---|---|
| M29-S1 | `CartridgeMapperType::KonamiSCC` + parse/to_string + CLI acceptance (additive edits to `cartridge_mapper_type.{h,cpp}`; grep all `to_string`/switch call sites) + unit-test cases | — |
| M29-S2 | `SccWavetable` device + full unit suite (register map, generator, deform, De Schrijder oracles, determinism) | — (parallel with S1) |
| M29-S3 | `CartridgeKonamiScc` mapper + `CartridgeSlot::load()` case + unit suite (bank/mirror/enable/window semantics) | S1, S2 |
| M29-S4 | Machine accessor `scc_chip()` + bus-level integration test (real CPU writes over the slot bus) | S3 |
| M29-S5 | `MachineAudioMixer` + `Sdl3AudioPresenter`/`Sdl3App` wiring + mixer unit tests (incl. the psg-only byte-identity regression oracle) + SDL3-ON regression run | S2 (device), S4 (accessor) |
| M29-S6 | System test (cold-boot + in-cart Z80 driver program + deterministic PCM oracle), A/B harness + `docs/m29-parity-trace-diff.md`, `docs/m29-implementation-report.md`, ledger updates | S1–S5 |

Dependency map (folders): `src/devices/audio` (new device) ← `src/devices/cartridge` (new
mapper owns it) ← `src/machine` (factory case + accessor) ← `src/frontend` (mixer/presenter)
← `tests/*`. Zero edges into `src/core` or `src/devices/cpu`.

## 4. Acceptance Criteria

1. **Fact-sheet precondition met**: `references/fact-sheets/Konami SCC.md` exists (delivered
   with this package) and every M29 device/mapper behavior claim in code comments cites it or
   a `references/...` path.
2. **`SccWavetable` semantics — dedicated unit tests, each hand-computable**
   (`tests/unit/devices/audio_scc_wavetable_unit_test.cpp`, suite `Devices_SccWavetable_Unit`):
   wave write/readback; ch4-write-mirrors-into-ch5 playback (fact-sheet §3, double-grounded);
   freq/vol block ×2 mirror at +0x90; 12-bit period assembly + high-nibble masking; reads of
   0x80–0x9F/0xA0–0xDF return 0xFF; period ≤ 8 freezes stepping (NYYRIKKI); period-write
   restart (counter reset + immediate output refresh); volume-latch-at-next-step (NYYRIKKI);
   enable bits gate contribution to 0 while phase keeps advancing; **the De Schrijder oracle
   reproduced literally**: all five channels at SampleValue=+1, Vol=15, enabled ⇒
   `amp_out() == 640`; deform bits 0/1 (incl. bit1-wins precedence), bit5 restart-on-freq-write,
   bits 6/7 rotation + read-only + the ch4-at-ch5's-period quirk; deform-range READ acts as
   write 0xFF; reset() state = fact-sheet §7; two-instance determinism (identical sequences ⇒
   identical outputs).
3. **`CartridgeKonamiScc` semantics — dedicated unit tests**
   (`tests/unit/devices/cartridge/cartridge_konami_scc_rom_unit_test.cpp`): reset bank layout
   0,1,2,3; bank switching accepted across the full 0x800-wide register windows (e.g. 0x5400)
   and at the canonical addresses; **KonamiSCC-specific mirroring** (0x4000-region content
   mirrored at 0xC000+, 0x8000-region content at 0x0000+ — asserted BOTH directions and
   contrasted against plain-Konami expectations); enable via 0x3F AND 0xBF, disable via 0x3E
   (A-M29-2); the 0x9000 write's both-effects rule (enable state AND bank 4 both change);
   SCC window read/write routing at 0x9800–0x9FFF incl. the 0x9900 mirror (A-M29-1); ROM
   mirror pages never expose the SCC; writes <0x5000 and ≥0xC000 ignored; disabled-SCC reads
   return ROM; `is_valid_image_size`; >512 kB image accepted (documented).
4. **Bus-level integration**
   (`tests/integration/machine/hbf1xv_m29_konami_scc_integration_test.cpp`): a real
   `Hbf1xvMachine` with a synthetic KonamiSCC image loaded via `load_cartridge()` into slot 1
   (and a slot-2 case), driven by REAL Z80 instructions over the M11 bus (LD (nn),A shapes):
   bank switch → readback; SCC enable → waveform/freq/vol/enable writes → wave readback over
   the bus; `scc_chip()` accessor non-null exactly when a KonamiSCC cart is loaded;
   empty-slot/other-mapper machines return nullptr and behave byte-identically to v1.0.29
   (regression guard).
5. **Mixing** (`tests/unit/frontend/` per existing frontend-test placement): with zero SCC
   sources the mixer's PCM equals the pre-M29 presenter arithmetic byte-for-byte (hard
   regression oracle); with an SCC source, output = documented clamp formula (hand-computed
   cases incl. a saturation case); determinism across two identical runs; `AudioPacer` files
   byte-for-byte untouched (`git diff v1.0.29 -- src/frontend/audio_pacer.*` empty) and
   `plan()` semantics unexercised by any new code path beyond the existing call.
6. **System test** (`tests/system/hbf1xv_m29_scc_system_test.cpp`): cold boot with a synthetic
   KonamiSCC cart whose in-image Z80 program banks, enables the SCC, writes a simple waveform/
   period/volume/enable set; the frame loop runs via `step_cpu_instruction()` +
   `on_vsync_boundary()` (the DEC-0034-proven loop shape); the mixer then pumps N samples; the
   PCM sequence matches a hand-computed oracle and is byte-identical across two runs.
7. **CLI**: `--cart1-type KonamiSCC` parses (case-insensitive) end-to-end on BOTH executables'
   parse paths; unrecognized values still error loudly (M19 policy unchanged); additive cases
   in the existing CLI unit tests.
8. **A/B evidence** (§2.5): register/window/mapper live-Tcl probe executed with the DEC-0028
   power-cycle methodology and recorded in `docs/m29-parity-trace-diff.md` (PARITY per probed
   fact, or honestly BLOCKED with the A-M29-3 citation); the audio-sample comparison recorded
   as N/A with the §2.5 reasoning verbatim. A BLOCKED/N-A disposition does not gate closure if
   the reasoning is concrete and checked (M25/M26/E2 precedent).
9. **Evidence gates** (run and capture; never fabricate): `tools/validate-assets.ps1`;
   `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; headless build + fast subset
   `ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure` (all green,
   count recorded); SDL3-ON build + fast subset in the dedicated SDL3 build dir under dummy
   drivers (all green, count recorded). **The ZEXALL/ZEXDOC slow sweep is explicitly NOT run**
   (DEC-0035: M31 QA gate only; and the `feedback_slow_test_cadence.md` trigger cannot fire —
   AC 10's diff-gate proves no CPU/core touch).
10. **Zero-touch verification**: `git diff v1.0.29 --stat` shows NO change under
    `src/devices/cpu/`, `src/core/`, `src/frontend/audio_pacer.*`,
    `src/devices/audio/psg_ym2149.*`, `src/devices/audio/ym2413_opll.*`, the six M19 mapper
    device files, and all 12 named zero-tolerance CPU-timing-oracle test files — checked by
    developer AND QA independently.
11. **Ledger/docs**: `docs/m29-implementation-report.md` + `docs/m29-parity-trace-diff.md`
    written; at closure G1 → DONE (M29) with the G5 named-remainder row added (§2.7);
    `state/current-phase.md`/`state/milestones.md` refreshed; QA sign-off
    (`docs/m29-qa-signoff.md`) precedes the coordinator's tag **v1.0.30** (Conditional Pass →
    fix-re-confirm-then-proceed WITHOUT a human stop, per DEC-0035).

## 5. Risks (each with a verification action)

- **R-M29-1 (mirroring inversion).** The KonamiSCC mirror direction is the exact opposite of
  the already-shipped plain Konami; a copy-paste of M19 code would silently invert it.
  *Verify*: AC 3's both-directions mirror test; QA line-compares against
  `RomKonamiSCC.cc:44-58`.
- **R-M29-2 (enable-compare semantics).** fMSX disagrees (exact 0x3F); shipping the wrong
  compare would still pass naive tests. *Verify*: the 0xBF-enables unit case (A-M29-2) + A/B
  probe case.
- **R-M29-3 (frequency `+1` divisor).** fMSX omits it; an off-by-one here shifts every pitch.
  *Verify*: hand-computed step-count oracles in AC 2 lock `(period+1)`; fact-sheet §9.1
  arbitration cited in code.
- **R-M29-4 (audio clipping / two-SCC sum).** 62·400 + 2·7,200 exceeds int16. *Verify*: clamp
  is mandatory, unit-tested at a constructed saturation input (AC 5); constants documented as
  presentation policy.
- **R-M29-5 (DEC-0033 regression).** Touching the presenter risks the just-fixed latency/pacing
  behavior. *Verify*: `audio_pacer.*` byte-diff empty (AC 5/10); `plan()`'s inputs unchanged
  (still exactly `total_elapsed_cycles` + queue depth); existing M26/M27 frontend tests all
  green in the SDL3-ON run.
- **R-M29-6 (shared-file edits).** `cartridge_mapper_type.*`, `cartridge_slot.cpp`, CLI files
  are pre-existing M19 files; edits must be strictly additive. *Verify*: all pre-existing M19
  unit tests pass UNMODIFIED; `git diff` on the six mapper device files empty (AC 10).
- **R-M29-7 (A/B forcing syntax unknown).** A-M29-3. *Verify in source before running*; honest
  BLOCKED fallback path defined in AC 8.
- **R-M29-8 (deform/rotation time base).** A-M29-6's internal-accumulator choice makes
  CPU-visible rotation reads advance only at pump boundaries — acceptable (test modes only) but
  must be disclosed, and rotation unit tests must drive `advance_cycles()` explicitly so the
  oracle is well-defined. *Verify*: header disclosure + test structure review by QA.
- **R-M29-9 (scope creep toward SCC-I).** "Mode-aware-ready" must not become "Plus mode built".
  *Verify*: grep gate — no `Plus`/`Compatible` behavioral branch with test coverage claims; G5
  row records the remainder instead.

## 6. Developer Handoff

Read first: this package; `references/fact-sheets/Konami SCC.md` (both delivered this cycle);
`src/devices/cartridge/cartridge_konami_rom.{h,cpp}` (the pattern being extended);
`references/openmsx-21.0/src/memory/RomKonamiSCC.cc` + `references/openmsx-21.0/src/sound/
SCC.cc` (behavior reference — never copy); `references/fmsx-60/source/EMULib/SCC.c` (cross-check
only); `src/frontend/{psg_audio_pump,sdl3_audio_presenter,audio_pacer}.{h,cpp}` (the chain being
joined, DEC-0033 constraints in the presenter header); `tests/CLAUDE.md` naming rules.

Build order: S1 → S2 → S3 → S4 → S5 → S6 (S1/S2 parallelizable). Hard constraints: zero touch
to `src/devices/cpu/` and `src/core/`; `AudioPacer` untouched; universal-fixes-only discipline
(no game-keyed behavior anywhere); no ZEXALL sweep this milestone (DEC-0035 — state this in the
implementation report's evidence section); label every uncertainty `Assumption:` with a
verification action; A/B memory-content probes use `set power off/on` (DEC-0028 rule). Deliver
`docs/m29-implementation-report.md` + `docs/m29-parity-trace-diff.md` +
`tools/openmsx-m29-scc-parity.ps1`, then hand to QA for `docs/m29-qa-signoff.md`; coordinator
closes with G1 → DONE (M29), the G5 row addition, ledger refresh, and tag **v1.0.30**.

---
*Authored 2026-07-09 by the MSX Planner Agent (REQ: M29 planning package per DEC-0035
autonomous continuation). Planning artifact only — no production code was written or modified
this cycle.*
