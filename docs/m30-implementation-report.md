# M30 Implementation Report — Universal Cartridge Auto-Identification (backlog G2)

- Milestone: M30 (DEC-0035 autonomous run, milestone 2 of 3; tag target v1.0.31 after QA)
- Spec: `docs/m30-planner-package.md` (all six slices S1-S6 implemented as specified)
- Developer: MSX Developer Agent, 2026-07-09
- Baseline: tag `v1.0.30` (M29 closed, DEC-0036). All M30 changes left UNCOMMITTED for
  coordinator/QA per the established cadence.

## 1. Milestone target

`--cartN <rom>` with NO `--cartN-type`, on BOTH executables, resolves the mapper type
automatically via the strict, loud, deterministic precedence chain **explicit flag → softwaredb
SHA1 match → bank-write heuristic (small-image plain-ROM rule inside it)** — with a verbatim
stderr report of WHAT was identified and HOW, and a hard non-zero-exit failure when the
identification names a mapper type this emulator does not implement. Universal-fixes-only:
"Aleste 2" appears ONLY in test expectations and evidence documents, never in production logic
(verifiable: `grep -rn "Aleste" src/` returns zero matches).

## 2. Code changes (per slice)

### S1 — Clean-room SHA-1 (`src/machine/sha1.{h,cpp}`, new)

- FIPS 180-4 / RFC 3174 SHA-1: streaming `update(std::span<const std::uint8_t>)`, idempotent
  finalizing `hex_digest()` (40-char lowercase), one-shot `sha1_hex()`.
- **Clean-room discipline statement (R-M30-2)**: this implementation was written solely from
  the published standard (initial hash values, 80-round f/K schedule, W-expansion, 0x80/length
  padding). `references/fmsx-60/source/EMULib/SHA1.c` was **never opened at any point during
  this milestone** — not for S1, not for anything else. The fMSX files that WERE read this
  cycle are exactly `references/fmsx-60/source/fMSX/MSX.c` (GuessROM, lines 2784-2878, for the
  DEC-0030 disagreement records) — nothing under `EMULib/`.
- Correctness gate: the four published FIPS 180-1/RFC 3174 §7.3 vectors ("" / "abc" / the
  448-bit two-block message / 1,000,000 × 'a') plus streaming==one-shot over a 2 MB+12,345-byte
  deterministic LCG-patterned buffer, plus odd-chunk streaming of the million-'a' vector.

### S2 — Software database (`src/machine/software_db.{h,cpp}`, new)

- Tolerant subset scanner for `<softwaredb>/<software>/<title>/<dump>/<rom>|<megarom>/<type>/
  <hash>/<start>`; comments, CDATA, DOCTYPE, PIs, attributes and unknown elements tolerated;
  malformed regions skip to the next `<software>` with a collected (never fatal) diagnostic.
- Semantics per the planner's RomDatabase.cc grounding (re-derived, never copied):
  `<rom>`-without-`<type>` → "Mirrored" (RomDatabase.cc:201-208); `<megarom>` no default —
  raw "" preserved (RomDatabase.cc:193-199); hashless dump dropped (RomDatabase.cc:490-494);
  `<start>` composes `Mirrored4000`-style RAW names only onto Mirrored/Normal
  (RomDatabase.cc:460-464,500-522) — composed names later route to the loud unsupported
  outcome, never silently truncated; duplicate hash → first wins (RomDatabase.cc:437-449).
- **License posture (planner §2.2.4, restated)**: `softwaredb.xml` is GPL-project DATA that
  remains in `references/` untouched. This code PARSES the user's locally-present file at
  runtime — same posture as reading the user's own `bios/` ROMs. Nothing from the file is
  copied into `src/` or `tests/`: all parser test fixtures are fully synthetic (fictional
  titles "Zorblax & the Space Weasels"/"Moon Ferret 2"/"Quantum Badger"/"Vapor Title", hashes
  computed by our own S1 SHA-1 over byte-fill patterns). The ONE exception is the Aleste-2
  SHA1 skip-gate constant in the asset-gated tests — a fact about the user's own local ROM
  file (independently computed from `roms/aleste.rom` via BOTH `Get-FileHash` and our own S1
  code, see §5), not database content. Absent DB → graceful degradation (message E), the
  emulator never depends on the GPL data file to function.
- Real-file scale check: the in-repo `references/openmsx-21.0/share/softwaredb.xml` loads in
  well under a second inside the integration test (entry_count > 1000 asserted as a sane lower
  bound; planner counted ~3,049 `<software>` entries).

### S3 — Identifier (`src/machine/cartridge_identifier.{h,cpp}`, new)

- `resolve_cartridge_type(spec, image, db)` — THE one shared resolution function (pure;
  determinism unit- and integration-proven by running it twice and comparing every field).
- Heuristic: an independent re-derivation of the BEHAVIOR of `RomFactory::guessRomType`,
  re-verified line-by-line against `references/openmsx-21.0/src/memory/RomFactory.cc:82-169`
  during implementation (not taken on faith from the package):
  - format signatures "ASCII16X"/"ROM_NEO8"/"ROM_NE16" at offset 16 (size ≥ 24), checked
    BEFORE the size gates → loud identified-but-unsupported;
  - size < 0x10000 → Mirrored (SmallImagePlainRule; the PAGE2 refinement is the package's
    disclosed simplification — our A-M19-8 full-window Mirrored already exposes page 2);
  - size == 0x10000 without "AB" → Mirrored;
  - else the 0x32 `LD (nn),A` scan with the exact score table, the conditional ASCII8
    decrement, and the fixed candidate order **ASCII8, ASCII16, Konami, KonamiSCC** with
    `nonzero && score >= winner` replacement — reproducing openMSX's enum-order `>=` tie-break
    (RomTypes.hh declares ASCII8 < ASCII16 < GENERIC_8KB < KONAMI < KONAMI_SCC — verified in
    the file this cycle; GENERIC_8KB's zero score can never re-win through the `tg &&` guard),
    so ties resolve KonamiSCC > Konami > ASCII16 > ASCII8.
- The four **DEC-0030 openMSX-vs-fMSX disagreement records** are written as comments at the
  exact code locations they affect (eligibility threshold; baseline biasing; tie-break
  direction; DB-before-heuristic agreement-as-corroboration), each with both citations, all
  arbitrated to openMSX per the planner §2.3 ruling (software policy, no hardware truth).
- Shared verbatim message formatter (messages A-D) + `format_softwaredb_unavailable_message`
  (message E, normal + WARNING wordings) — unit-asserted byte-for-byte.
- `CartridgeIdentificationSession`: the lazy at-most-once DB load (only when a slot actually
  needs auto-identification), message-E-once-per-process behavior, explicit-spec pass-through
  (zero messages, zero DB access), empty-image pass-through (the existing loud load-failure
  path owns that case), unsupported → `ok=false` with the message-B line last.

### S4 — CLI wiring (both executables)

- `src/machine/cartridge_cli.{h,cpp}`: additive `--softwaredb <path>`
  (`ParsedCartridgeCli::softwaredb_path`) + `--cartN-type auto` (case-insensitive; sets
  `type_was_explicit=false`, type back to the Mirrored struct default; last-occurrence-wins,
  matching the parser's existing repeated-flag rule). The parser's Mirrored default and every
  pre-existing assertion are UNCHANGED (the pre-M30 unit-test block is untouched above the
  marked M30 additive section).
- `src/main.cpp` (`load_cartridges_from_args`): builds ONE `CartridgeIdentificationSession`
  per run, resolves each slot through it, prints its messages to stderr, returns 2 on
  unsupported (the existing error-path convention), loads with the RESOLVED type. Because all
  headless modes (default run, `--parity-trace`, `--debug-session`) already delegate to this
  one function, they all gained identification with zero per-mode wiring. The `--debug-session`
  usage text documents `--softwaredb` and the optional/`auto` type.
- `src/frontend/sdl3_app.{h,cpp}` + `src/frontend/sdl3_main.cpp`: `Sdl3AppConfig` gains
  `cart1_type_explicit`/`cart2_type_explicit` (**default true** — every pre-existing
  PROGRAMMATIC config construction keeps byte-for-byte behavior, mirroring the planner §2.4.1
  step-5 machine-API rule) + `softwaredb_path`; `load_configured_assets()` resolves through
  the SAME shared session; unsupported → messages printed, `last_error_` = the message-B line,
  startup abort (the existing never-partially-initialize convention); `sdl3_main.cpp` carries
  the parsed `type_was_explicit` + `softwaredb_path` into the config and its usage text
  documents the new behavior.
- `Hbf1xvMachine::load_cartridge(slot, type, image)` — signature and semantics UNTOUCHED
  (identification produces the `type` argument one layer up, exactly as specified).

### S5 — Integration/system evidence (new/additive tests)

- `tests/integration/machine/hbf1xv_m30_cartridge_identification_integration_test.cpp`
  (compile-defs `SONY_MSX_BIOS_DIR` + `SONY_MSX_ROMS_DIR` + new `SONY_MSX_SOFTWAREDB_PATH`,
  DEC-0016 absolute-path pattern): skip-gates on ROM presence AND exact SHA1
  `e93d0840c59c6eba273df546d22148d486a150a6` AND DB presence (a differing user asset SKIPs,
  never FAILs); then asserts real-DB load, lookup → ("Aleste 2", "KonamiSCC"), resolver →
  KonamiSCC via SoftwareDbSha1, message A verbatim, two-run field-identical determinism,
  `load_cartridge` → Ok, and a 60-frame DEC-0034-shape boot smoke
  (`step_cpu_instruction()` + `on_vsync_boundary()`, never `run_frame()`).
- Additive Case 3 in `tests/integration/frontend/sdl3_cli_session_integration_test.cpp`
  (SDL3-gated, dummy drivers, same skip discipline): type-less `--cart1 <aleste>` +
  `--softwaredb <abs path>` → parse → config (explicit=false) → `Sdl3App::init()` succeeds →
  cartridge loaded via the shared resolver. Cases 1/2 assertions unmodified (Case 1 is
  explicit-typed; Case 2 fails at file-open, before type resolution); the shared
  `config_from_args` helper was extended to mirror `sdl3_main.cpp` exactly (its documented
  contract).
- New unit tests: `machine_sha1_unit_test`, `machine_software_db_unit_test`,
  `machine_cartridge_identifier_unit_test`; additive rows in `machine_cartridge_cli_unit_test`
  and `frontend_sdl3_cli_unit_test`.
- Build registration: `CMakeLists.txt` (+3 sources in `sony_msx_core`), `tests/CMakeLists.txt`
  (M30 section + the SONY_MSX_SOFTWAREDB_PATH compile-def on the SDL3 session test).

### S6 — Evidence, ledger, docs

- `tools/openmsx-m30-identification-ab.ps1` → `docs/m30-identification-ab.md` (see §6).
- Ledger: `agent-protocol/state/deferred-backlog.md` — **G2 → DONE (M30)** with the named
  non-goals on the row (CARTS.SHA/CARTS.CRC skipped with the §2.2.5 reasoning; PAGE2 subsumed
  by A-M19-8; unsupported identifications fail loud by design — long tail stays G4, SCC-I
  stays G5); **E1 renumbered M30→M31 THIS cycle** (per package §2.7, citing DEC-0035 —
  mechanical bookkeeping of the already-ratified decision); **C10/F1/F2** factual
  numeric-owner shift notes (M32/M33/M34-era per DEC-0035's own text, final numbers at their
  own kickoffs). `state/current-phase.md` + `state/milestones.md` refreshed (M30 Ready for QA;
  M29's stale "Ready for QA" status line corrected to CLOSED/DEC-0036/v1.0.30).

## 3. Unit test results (real captured output)

Both configurations, fast subsets only (**ZEXALL/ZEXDOC deliberately NOT run** — see §7):

| Gate | Result |
|---|---|
| `cmake --build build --config Debug` (headless) | SUCCESS (no errors; only the pre-existing C4819 codepage warnings) |
| `cmake --build build/sdl3-on --config Debug` (SDL3 ON) | SUCCESS (no errors) |
| `ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure` | **163/163 passed** (159 baseline + 4 new), 17.11 s |
| `ctest --test-dir build/sdl3-on -C Debug -LE m24_slow_full_sweep` under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy` | **172/172 passed** (168 baseline + 4 new), 22.55 s |
| `tools/validate-assets.ps1` | PASS (all required BIOS files present; ROM count 2) |
| `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | Reproduced — diff vs v1.0.30 is the generation timestamp ONLY (all checksums identical) |

New tests, run individually (all exit 0):

```
All Machine_Sha1_Unit cases passed
All Machine_SoftwareDb_Unit cases passed
All Machine_CartridgeIdentifier_Unit cases passed
All Machine_Hbf1xvM30CartridgeIdentification_Integration cases passed
```

The SDL3 session test's Case 3 genuinely exercised the auto path (not skipped) — its direct
run prints the message-A line with the absolute DB path before "All
Frontend_Sdl3CliSession_Integration cases passed".

## 4. Integration test results — the end-to-end behavior (real captured output)

From the repo root (the default CWD-relative DB path):

```
$ ./build/Debug/sony_msx_headless.exe --cart1 roms/aleste.rom
cartridge: --cart1: identified as "Aleste 2" (KonamiSCC) via softwaredb SHA1 match [sha1=e93d0840c59c6eba273df546d22148d486a150a6, db=references/openmsx-21.0/share/softwaredb.xml]
cartridge: --cart1 loaded (roms/aleste.rom, KonamiSCC)
... exit 0
```

Explicit path — byte-for-byte pre-M30 output (no identification line):

```
$ ./build/Debug/sony_msx_headless.exe --cart1 roms/aleste.rom --cart1-type KonamiSCC
cartridge: --cart1 loaded (roms/aleste.rom, KonamiSCC)
```

`--cart1-type auto` behaves exactly as omitted (same message A + load). A second real ROM
confirms universality: `--cart1 roms/metalgear.rom` (no type) → `identified as "Metal Gear"
(Konami) via softwaredb SHA1 match` → loads as Konami. Degradation path (explicit bad DB):

```
$ ./build/Debug/sony_msx_headless.exe --cart1 roms/aleste.rom --softwaredb no/such/file.xml
cartridge: WARNING: --softwaredb "no/such/file.xml" could not be read -- SHA1 identification disabled (heuristic only)
cartridge: --cart1: no softwaredb match [sha1=e93d0840...]; guessed KonamiSCC via bank-write heuristic (scores: KonamiSCC=6 Konami=6 ASCII8=3 ASCII16=4); pass --cart1-type to override
cartridge: --cart1 loaded (roms/aleste.rom, KonamiSCC)
```

Note the free corroboration: for this exact file the HEURISTIC ALone also lands KonamiSCC
(6-6 KonamiSCC/Konami tie resolved by the re-derived `>=` tie-break) — DB and heuristic agree.

## 5. Assumption verification actions (carried from planner §1.4)

- **A-M30-1**: `Get-FileHash -Algorithm SHA1 roms/aleste.rom` →
  `E93D0840C59C6EBA273DF546D22148D486A150A6`; our own S1 `sha1.cpp` computes the identical
  digest (embedded in the captured message-A line above and asserted in the integration test)
  — the free S1 cross-validation passed.
- **A-M30-2**: `references/openmsx-21.0/share/softwaredb.xml` present; the Aleste 2 entry
  re-read verbatim at lines 14839-14850 this cycle (KonamiSCC for this hash).
- **A-M30-3 grep RE-RUN (required)**: repo-wide search for `--cart1 ` (references/ excluded)
  found explicit `--cart1-type` in every live tool/test invocation —
  `tools/openmsx-m19-cartridge-parity.ps1:76` (`--cart1-type 8kB`),
  `tools/openmsx-m29-scc-parity.ps1:87` (`--cart1-type KonamiSCC`), plus docs/QA transcripts.
  The only type-less usages are (a) `tests/unit/machine/cartridge_cli_unit_test.cpp:48` — the
  PARSER-level default-Mirrored assertion, unaffected (parser unchanged, verified green), and
  (b) `tests/integration/frontend/sdl3_cli_session_integration_test.cpp` Case 2 — the bad-path
  negative case, which fails at file-open BEFORE type resolution (verified green). No existing
  invocation changes behavior.
- **A-M30-4**: WSL openMSX available (`/usr/bin/openmsx`, openMSX 19.1) — A/B ran live, §6.
- **A-M30-5**: S1 is genuinely new code; clean-room statement in §2-S1.
- **R-M30-6**: the Side-B Tcl mechanism was verified in source BEFORE scripting:
  `MSXMotherBoard.cc:1227-1252` (`machine_info device <name>` → `getDeviceInfo`),
  `MSXRom.cc:40-63` (`getExtraDeviceInfo` → `getInfo` → dict keys `mappertype` — "'auto' is
  already changed to actual type", MSXRom.cc:26-27 — and `actualSHA1`, our cartridge-device
  selector), `HardwareConfig.cc:115-122` (no `-romtype` → `<mappertype>auto`),
  `RomFactory.cc:176-219` (auto resolution + resolved-type write-back). Then live-verified.

## 6. A/B evidence (planner §2.5) — AGREEMENT

`tools/openmsx-m30-identification-ab.ps1` → `docs/m30-identification-ab.md`:

- **Side A** (this emulator, NO `--cart1-type`): exit 0, message A verbatim, type
  **KonamiSCC** via softwaredb SHA1 match.
- **Side B** (openMSX 19.1 on WSL, `-carta` with NO `-romtype`, machine `Sony_HB-F1XV`): live
  Tcl query over `machine_info device`; the device with
  `actualSHA1 == e93d0840c59c6eba273df546d22148d486a150a6` (openMSX names it "Aleste 2" from
  its own DB) reports `mappertype=KonamiSCC`.
- **RESULT: AGREEMENT (A='KonamiSCC', B='KonamiSCC')**, script exit 0. Per the package, all
  other A/B is N/A by design (identification is composition policy with no hardware analogue).

## 7. Regression + constraint compliance

- `git diff v1.0.30 --stat -- src/devices src/core src/peripherals src/frontend/audio_pacer.h
  src/frontend/audio_pacer.cpp` → **EMPTY** (zero touch to all constrained trees, including
  ALL cartridge mapper device files — identification is policy above the devices).
- **ZEXALL/ZEXDOC NOT run** (Acceptance Criterion 11): DEC-0035 reserves the slow sweep for
  M31's QA gate, and `feedback_slow_test_cadence.md`'s mechanical gate does not fire (the
  empty diff above IS the zero-CPU/core-touch proof).
- Parser defaults, `type_was_explicit` semantics, and `load_cartridge` API unchanged;
  pre-existing tests unmodified except the named additive rows/cases.
- Temporary probes: none left behind (the one ad-hoc Tcl probe file used to live-verify the
  Side-B query before scripting was deleted; `git status` shows only the intended M30 files).
- Determinism (Acceptance Criterion 12): unit
  (`Determinism_TwoRuns_IdenticalIdentification_EveryField`) + integration
  (`Resolver_TwoRuns_IdenticalEveryField`) both prove identical inputs → identical
  identification on every struct field.

## 8. Known issues / residuals (honest disclosures)

1. **R-M30-5 (accepted by the package)**: the default DB path is CWD-relative — running the
   exe from outside the repo root degrades to heuristic-only, loudly (message E), with
   `--softwaredb` as the override. Candidate for a future packaging-era path-discovery story.
2. The WSL reference binary is openMSX **19.1** while the grounding source tree is 21.0 (the
   long-standing situation of every prior A/B); the `machine_info device`/mappertype mechanism
   verified in the 21.0 source works identically on the live 19.1 binary (raw transcript in
   the A/B artifact).
3. Message C's score detail prints POST-handicap ASCII8 (the number the decision actually
   used) — documented in `CartridgeIdentification::detail`'s comment and asserted verbatim in
   the unit tests.
4. For format-signature unsupported outcomes (message B), `title` is set to the signature
   string itself (e.g. `identified as "ASCII16X" (ASCII16X) via ROM format signature`) — there
   is no DB title to report; asserted verbatim in the unit tests.
5. `docs/asset-checksums.txt` diff vs v1.0.30 is the generation timestamp line only.

## 9. QA handoff

- Artifacts: this report; `docs/m30-identification-ab.md`;
  `tools/openmsx-m30-identification-ab.ps1`; the four new test binaries; the ledger/state
  updates named in §2-S6. All changes UNCOMMITTED on `main` (v1.0.30 + working tree).
- Suggested QA focus (per the package's own risk list): re-affirm the §2.2.4 GPL-data posture
  independently (R-M30-7 asks for a second reading); grep `tests/` fixtures for any real
  softwaredb content (there is none — fixtures are fictional and self-hashed); spot-check the
  §2.3 heuristic re-derivation against `RomFactory.cc:82-169`; re-run the A/B script; confirm
  the explicit-type path's byte-for-byte stability (§4's second transcript).
