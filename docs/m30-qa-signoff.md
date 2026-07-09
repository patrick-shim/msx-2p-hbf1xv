# M30 QA Sign-off — Universal Cartridge Auto-Identification (backlog G2)

- Milestone: M30 (DEC-0035 autonomous run, milestone 2 of 3; tag target v1.0.31)
- QA Owner: MSX QA Agent, 2026-07-09
- Baseline: tag `v1.0.30` (commit `e0746e2`); all M30 changes UNCOMMITTED on `main`.
- Inputs reviewed: `docs/m30-planner-package.md` (spec), `docs/m30-implementation-report.md`
  (developer evidence), `docs/m30-identification-ab.md` (A/B artifact), full source/test diffs
  vs `v1.0.30`, and the grounding sources under `references/` cited below.
- Method: every developer headline was independently re-derived — clean-directory rebuilds of
  BOTH configurations, full fast-subset ctest re-runs, an independent Python-hashlib SHA-1
  cross-check, a line-by-line heuristic re-check against openMSX AND fMSX sources, a live A/B
  re-run, and a byte-for-byte explicit-path comparison against a freshly built REAL v1.0.30
  binary (git worktree at the tag). Nothing below is taken from the report on faith.

## 1. Regression Scope

Identification is machine/CLI-layer policy layered above the cartridge devices. Affected
surfaces: `src/machine/` (3 new file pairs: `sha1.*`, `software_db.*`,
`cartridge_identifier.*`; additive edits to `cartridge_cli.*`), both executable shells
(`src/main.cpp`, `src/frontend/sdl3_app.*`, `src/frontend/sdl3_main.cpp`), build registration,
tests. Constrained-to-zero surfaces: `src/devices/` (all), `src/core/`, `src/peripherals/`,
`src/frontend/audio_pacer.*`, the `load_cartridge` API, parser defaults, every pre-existing
test assertion. ZEXALL/ZEXDOC out of scope by decision (DEC-0035: M31 QA gate only).

## 2. Regression Matrix Status

| # | Check | Method (QA-independent) | Result |
|---|---|---|---|
| 1 | Headless clean rebuild | fresh `build-qa-m30/` (SDL3 OFF), configure+build Debug | PASS (`build-qa-m30-build.log`) |
| 2 | SDL3-ON clean rebuild | fresh `build-qa-m30-sdl/` (prefix `build/_sdl3_install`) | PASS (`build-qa-m30-sdl-build.log`) |
| 3 | Headless fast subset | `ctest --test-dir build-qa-m30 -C Debug -LE m24_slow_full_sweep` | **163/163 passed** (18.05 s) — reproduces the report |
| 4 | SDL3-ON fast subset | same on `build-qa-m30-sdl`, `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`, SDL3.dll on PATH | **172/172 passed** (18.84 s) — reproduces the report |
| 5 | Four new M30 test binaries, run individually | direct invocation | all 4 exit 0, all-cases-passed banners; the M30 integration test ran UN-skipped (real DB + real ROM gates satisfied) |
| 6 | SDL3 session Case 3 genuinely exercised | direct run of `frontend_sdl3_cli_session_integration_test.exe` | PASS — message-A line printed with the absolute DB path (no SKIP) |
| 7 | SHA-1 independent cross-check | Python `hashlib` on the same inputs | PASS — `""`→`da39a3ee...0709`, `"abc"`→`a9993e36...d89d`, 2-block→`84983e44...70f1`, 1M×'a'→`34aa973c...016f`, `roms/aleste.rom`→`e93d0840c59c6eba273df546d22148d486a150a6` (2,097,152 bytes); identical to what our `sha1.cpp` prints in message A |
| 8 | SHA-1 clean-room structure | read `src/machine/sha1.{h,cpp}` in full | PASS — standard FIPS 180-4 textbook form (5-word H init §5.3.1, W[80] expansion with rotl-1, 80 rounds in four Ch/Parity/Maj/Parity stages with the four published K constants, 0x80+64-bit-BE-length padding), FIPS section citations inline; NOT an emulator-shaped variant. fMSX's `EMULib/SHA1.c` was NOT opened by QA either (structure judged against the public standard only) |
| 9 | Heuristic vs openMSX ground truth | re-read `references/openmsx-21.0/src/memory/RomFactory.cc:82-169` + `RomTypes.hh:8-38` | PASS — signature gate (offset 16, size≥24, 3 magics), size gates (<64K→Mirrored; ==64K-no-AB→Mirrored), full score table (0x5000/0x9000/0xB000→SCC; 0x4000/0x8000/0xA000→Konami; 0x6800/0x7800→A8; 0x6000→K+A8+A16; 0x7000→SCC+A8+A16; 0x77FF→A16), guarded ASCII8 decrement (RomFactory.cc:160), and the `tg && tg >=` enum-order tie-break; RomTypes.hh confirms ASCII8 < ASCII16 < GENERIC_8KB < KONAMI < KONAMI_SCC, so the fixed candidate order + `>=` reproduces KonamiSCC > Konami > ASCII16 > ASCII8 exactly |
| 10 | fMSX disagreement records | re-read `references/fmsx-60/source/fMSX/MSX.c:2784-2878` and `:3246` | PASS — all four records real: (1) `ROMMask[Slot]+1>4` = >32 KB scan gate vs openMSX ≥64 KB; (2) all-counters=1 + GEN8+1 + ASCII8−1 baseline (MSX.c:2840-2844); (3) strict `>` first-index-wins (MSX.c:2872-2874); (4) CRC→SHA→scan DB-before-heuristic order (MSX.c:2790-2837) — agreement recorded as corroboration |
| 11 | Adversarial: tests genuinely discriminate | hand-simulation | PASS — `Scan_TieKonamiSccBeatsKonami` ({0x4000,0x5000}=1-1 tie) FAILS under fMSX's strict-`>` rule; `Scan_77FF_Ascii16_OpenMsxArbitration` FAILS under fMSX's baseline biasing; `Precedence_ExplicitFlag_BypassesDbAndHeuristic` FAILS if precedence were DB-over-explicit (poisoned DB maps that image's real SHA-1 to ASCII16; test demands Konami + empty sha1_hex); `Scan_Single6800_...` FAILS without the ASCII8 handicap |
| 12 | Poisoned-DB precedence proofs | read `cartridge_identifier_unit_test.cpp:239-288` + green run | PASS — the DB entry is keyed to the CONSTRUCTED image's actual SHA-1 with a type that contradicts both the explicit flag (Konami) and the heuristic answer (scan=Konami, DB=ASCII16); explicit>DB and DB>heuristic each proven on real conflicts |
| 13 | End-to-end aleste, NO type | QA-built binary from repo root | PASS — exit 0, message A byte-exact: `identified as "Aleste 2" (KonamiSCC) via softwaredb SHA1 match [sha1=e93d0840..., db=references/openmsx-21.0/share/softwaredb.xml]` |
| 14 | End-to-end metalgear, NO type | QA-built binary | PASS — exit 0, `identified as "Metal Gear" (Konami)`; QA verified hash `919fa773...` sits under `<title>Metal Gear</title>` (Konami) at softwaredb.xml:7793-7821 |
| 15 | `--cart1-type auto` == omitted | QA-built binary | PASS — identical message A + load |
| 16 | Explicit path byte-for-byte pre-M30 | REAL v1.0.30 binary built from a git worktree at the tag; `diff` of stdout AND stderr vs the M30 binary, two ROM/type combos (aleste/KonamiSCC, metalgear/Konami) | PASS — both streams byte-identical, zero identification lines (strongest possible A-M30-3/AC9 proof) |
| 17 | Degradation path (bad `--softwaredb`) | QA-built binary | PASS — WARNING variant of message E, then heuristic C line (`KonamiSCC=6 Konami=6 ASCII8=3 ASCII16=4`, the 6-6 tie resolving KonamiSCC via `>=`), loads, exit 0 — DB and heuristic independently agree for this ROM |
| 18 | Unsupported → loud non-zero exit | QA-crafted synthetic 32 KB ROM with `ASCII16X` at offset 16, no type flag | PASS — verbatim message B, **exit 2**, no load |
| 19 | CLI-level determinism | two identical type-less runs, `diff` both streams | PASS — byte-identical |
| 20 | A/B re-run (QA, independent) | `tools/openmsx-m30-identification-ab.ps1` against the QA-built binary, artifact to scratch | **PASS — AGREEMENT (A='KonamiSCC', B='KonamiSCC')**, exit 0; Side B live-queried openMSX 19.1 on WSL, device selected by `actualSHA1==e93d0840...`, device name `Aleste 2`. Query mechanism verified in source by QA: `MSXRom.cc:24-31` ("'auto' is already changed to actual type") + `getInfo`/`getExtraDeviceInfo` at `MSXRom.cc:40-63` |
| 21 | S2 parser semantics vs RomDatabase.cc | re-read `references/openmsx-21.0/src/memory/RomDatabase.cc:185-215, 430-524` | PASS — rom→"Mirrored" default (201-208), megarom no default (193-199), hashless drop (490-494), `<start>` composes only 6-char `0x....` onto Mirrored/Normal (460-464, 500-515), duplicate keep-old (437-449); our `compose_type_name`/commit logic matches each |
| 22 | Scope: constrained trees | `git diff v1.0.30 --stat -- src/devices src/core src/peripherals src/frontend/audio_pacer.h src/frontend/audio_pacer.cpp` | **EMPTY** |
| 23 | Universal-fixes-only | `grep -rn "Aleste" src/` | zero matches; "Aleste 2" appears only in the two asset-gated tests (as expectations) and evidence docs |
| 24 | A-M30-3 grep re-run | QA grep for type-less `--cart1` across tools/tests/src | PASS — only the M30 A/B script (deliberately type-less), the parser-default unit assertion (parser unchanged), and the SDL3 session Cases 2/3 (bad-path pre-resolution / the new M30 case); M19/M29 parity scripts pass explicit types |
| 25 | Asset gates | `tools/validate-assets.ps1`; `tools/checksum-assets.ps1` re-run to scratch and diffed | PASS — all 7 BIOS files + 2 ROMs; checksums identical to `docs/asset-checksums.txt` except the generation timestamp |
| 26 | Ledger | read `deferred-backlog.md`, `current-phase.md`, `milestones.md` diffs | PASS — G2 → DONE (M30) with named non-goals; E1 renumbered M30→M31 citing DEC-0035/§2.7; C10/F1/F2 factual owner-shift notes; M29 stale status corrected to CLOSED |
| 27 | ZEXALL/ZEXDOC | not run, per DEC-0035 | COMPLIANT — the empty diff in row 22 is the zero-CPU/core-touch proof; `feedback_slow_test_cadence.md` gate does not fire |

## 3. Failures and Risk Ranking

No blocker-, high-, or medium-severity findings. Residuals/observations:

1. **[Low, out-of-milestone config edit] `.claude/settings.json`** carries a one-character
   model-name typo fix (`"sonnect"`→`"sonnet"`) in the uncommitted tree, not mentioned in the
   implementation report and not part of M30's scope. No emulator/test impact. Coordinator
   should attribute or split this hunk at commit time (agent-config changes are not something
   an agent run can self-authorize).
2. **[Low, disclosed by design] DB types outside the seven abort loudly.** QA counted the real
   DB's type distribution: `page23`(596) + `page2`(282) + `Normal`(94) ≈ 972 dumps whose exact
   match will identify-then-abort with message B, requiring an explicit `--cartN-type`. This is
   the planner's explicit §2.4.3 design (loud failure over silent garbage boot) and most such
   images would work under a forced `Mirrored` on this machine's A-M19-8 full-window mirror —
   candidate usability note for G4-era work, not a defect.
3. **[Info, pre-existing] WSL reference binary is openMSX 19.1** vs the 21.0 grounding tree —
   the long-standing situation of every prior A/B; the queried mechanism verified in 21.0
   source works identically live on 19.1 (QA re-ran it).
4. **[Info, accepted R-M30-5] CWD-relative default DB path** — running outside the repo root
   degrades loudly (message E) to heuristic-only; `--softwaredb` overrides.
5. **[Info, housekeeping] Untracked residue** in the working tree: `build-qa*` trees and
   `build-qa-m29-*.log`/`build-qa-m30-*.log` files (QA-cycle artifacts, including this cycle's)
   and two M29-era `debug/frames/*.frame` dumps. None are M30 deliverables; exclude from the
   closure commit.

## 4. Required Fixes

None blocking. Item 1 of §3 is routed to the coordinator for commit-time handling (attribution
or hunk split); items 2-5 are recorded residuals requiring no M30 action.

## 5. License-Posture Verdict (R-M30-7 — QA's independent second reading)

**AFFIRMED.** Independently verified, not restated from the report:

- `references/openmsx-21.0/share/softwaredb.xml` is untouched GPL-project DATA (git diff vs
  v1.0.30 shows zero changes under `references/`); our code PARSES a locally-present copy at
  runtime and degrades gracefully (message E, heuristic still functional — live-verified) when
  it is absent. The emulator never depends on the GPL data file to function.
- QA grepped every 40-hex literal in the M30 test files: the only constants are the four
  PUBLISHED FIPS 180-1/RFC 3174 vector digests (public standard data) and the Aleste-2
  skip-gate hash `e93d0840...` — which QA independently recomputed from the user's own local
  `roms/aleste.rom` with Python hashlib, i.e. a fact about the local asset, obtainable with no
  DB present. No other real-DB hash, entry, or title ships in `src/` or M30 fixtures.
- The S2/S3 fixtures were read in full: all titles are fictional ("Zorblax & the Space
  Weasels", "Moon Ferret 2", "Quantum Badger", "Vapor Title", "First/Second Claimant",
  "Survivor", ...) and every fixture hash is self-computed by our own SHA-1 over byte-fill
  patterns at test runtime. The "Metal Gear"/real-title grep hits in `tests/` are all
  pre-existing M19/VDP-era files unchanged since before M30. The "Aleste 2" title string
  appears only as an asserted EXPECTATION of the runtime lookup in the two asset-gated tests,
  exactly as the package's handoff text sanctions.
- S1 clean-room: QA did NOT open `references/fmsx-60/source/EMULib/SHA1.c` (nor did the
  developer, per the recorded discipline statement); QA instead verified `sha1.cpp` is the
  standard's own 80-round/5-word/4-stage reference form with FIPS section citations, and that
  its digests match an independent implementation (Python hashlib) on the published vectors
  and on a real 2 MB input. A tree cannot prove a negative, but structure + provenance
  statements + independent-vector agreement leave no license-sensitive residue to flag.
- The heuristic and parser are re-derivations with different structure from their references
  (behavior tables and semantics only; no code or large data tables transcribed — the score
  table and the three 8-byte format magics are facts of third-party file formats/hardware
  usage, not copyrightable openMSX data tables), consistent with the
  `feedback_license_sensitive_scope.md` discipline.

## 6. Sign-off Decision

**PASS.**

All 12 acceptance criteria of `docs/m30-planner-package.md` §5 verified against independently
re-derived evidence: published-vector SHA-1 (AC1), real-DB lookup → ("Aleste 2", "KonamiSCC")
(AC2), synthetic-fixture parser semantics with zero real-DB content (AC3), heuristic fidelity
incl. score table/handicap/tie order/size gates/signatures + the four DEC-0030 records (AC4),
precedence with poisoned-DB proofs and `auto` (AC5), verbatim A-E messages + exit-2/abort loud
failure (AC6), ONE shared resolver on both executables + graceful absent-DB path (AC7),
real-ROM end-to-end (AC8), 163/163 + 172/172 clean-tree regression with byte-identical
explicit path vs the real v1.0.30 binary and empty constrained-tree diffs (AC9), live A/B
AGREEMENT re-run by QA (AC10), ZEXALL correctly withheld with the diff proof (AC11), and the
ledger closure/renumber (AC12). Backlog G2 → DONE (M30) is affirmed. Ready for the
coordinator's closure step (commit + tag v1.0.31), with §3 item 1 handled at commit time.

Evidence artifacts: `build-qa-m30-configure.log`, `build-qa-m30-build.log`,
`build-qa-m30-sdl-configure.log`, `build-qa-m30-sdl-build.log` (clean-tree builds);
`docs/m30-identification-ab.md` (developer A/B, contents cross-checked) plus QA's independent
scratch re-run (AGREEMENT, exit 0, transcript in the QA session record);
`docs/m30-implementation-report.md` §3-§7 (all headline numbers reproduced by QA).
