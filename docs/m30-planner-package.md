# M30 Planner Package — Cartridge Auto-Identification (backlog G2, the Aleste-2 usability fix)

- Milestone ID: M30
- Title: Universal cartridge mapper auto-identification — SHA1 software-database lookup +
  bank-write heuristic fallback (closes backlog G2)
- Authorization: DEC-0035 (`agent-protocol/channels/decisions.md`, 2026-07-09) — three-milestone
  roadmap M29 (SCC, CLOSED v1.0.30 per DEC-0036) → **M30 (this package)** → M31 (YM2413 FM
  synthesis, RC); autonomous continuation; tag target **v1.0.31** on close.
- Spec Owner: MSX Planner Agent · Developer Owner: MSX Developer Agent · QA Owner: MSX QA Agent
- Root cause already established (coordinator pre-probe, recorded in REQ-M29-001's
  coordinator-verification paragraph and DEC-0035/DEC-0036): `roms/aleste.rom` is **Aleste 2**
  (Compile 1989), identified by EXACT SHA1 match `e93d0840c59c6eba273df546d22148d486a150a6`
  against `references/openmsx-21.0/share/softwaredb.xml:14839-14850`, which labels this dump
  **KonamiSCC**. It is a multi-mapper "v8" repro build that boots under ASCII16 AND (post-M29)
  under KonamiSCC (M29 evidence: loader banner "Konami8 mapper",
  `debug/frames/m29-aleste-f{240,500,899}.png`). The human's garbage screen came from the M19
  CLI's silent `Mirrored` default when `--cart1-type` is omitted. **There is NO emulation
  defect.** The universal, system-wide fix (human's explicit requirement — never a game-keyed
  tweak) is automatic mapper identification: `--cart1 <rom>` with no type flag must just work.

## 1. Scope and Assumptions

### 1.1 Objective

`--cartN <rom>` without `--cartN-type`, on BOTH executables, resolves the mapper type
automatically via a strict, loud, deterministic precedence chain:

**explicit `--cartN-type` flag → softwaredb SHA1 match → bank-write heuristic → (small-image
plain-ROM rule inside the heuristic)** — with a clear stderr report of WHAT was identified and
HOW, and a hard, non-zero-exit failure when the DB identifies a mapper type this emulator does
not implement (never a silent garbage boot).

### 1.2 In scope (the IN set)

1. **Clean-room SHA-1** — `src/machine/sha1.{h,cpp}`: an independently-implemented FIPS 180-4 /
   RFC 3174 SHA-1 (streaming update + lowercase-hex digest string). Spec §2.1.
2. **Software-database lookup** — `src/machine/software_db.{h,cpp}`: a minimal, tolerant,
   runtime parser for the openMSX `softwaredb.xml` schema SUBSET this project needs
   (`<software>/<title>/<dump>/<rom>|<megarom>/<type>/<hash>`), keyed SHA1 → (title, type
   string). Parses the user's locally-present file at runtime; degrades gracefully when absent.
   Spec §2.2 (license analysis in §2.2.4).
3. **Heuristic fallback** — `src/machine/cartridge_identifier.{h,cpp}`: a re-derivation of the
   BEHAVIOR of openMSX's `RomFactory::guessRomType`
   (`references/openmsx-21.0/src/memory/RomFactory.cc:82-169`), cross-checked against fMSX's
   `GuessROM` (`references/fmsx-60/source/fMSX/MSX.c:2784-2878`) with the disagreements recorded
   and arbitrated per DEC-0030. Spec §2.3. Also owns precedence resolution + report formatting
   (§2.4).
4. **CLI wiring, both executables** — additive `--softwaredb <path>` flag + the `auto` type
   value, wired through the ONE shared resolution function; identification runs ONLY when
   `type_was_explicit == false` (the field already exists — `src/machine/cartridge_cli.h:18`).
   Spec §2.4.
5. **Deterministic tests** per §4 (synthetic fixtures only — never copies of real softwaredb
   entries) + the asset-gated real-ROM integration test + dual-executable coverage.
6. **A/B evidence** (§2.5): identification is frontend/composition policy — openMSX A/B is N/A
   by design EXCEPT one meaningful live check: openMSX launched with `roms/aleste.rom` and NO
   `-romtype` forcing must agree with our DB identification (both sides boot it as KonamiSCC).
7. **Backlog bookkeeping** (§2.6/§2.7): G2 → DONE (M30) at closure; the DEC-0035 E1 renumber
   (M30→M31) applied to the ledger THIS cycle (decision + reasoning in §2.7).

### 1.3 Explicitly out of scope (named, not silent)

- **fMSX `CARTS.SHA` / `CARTS.CRC` database support — SKIPPED, with reasoning** (§2.2.5). Not a
  partial close of G2: G2's own row text names "ROM-database/SHA1 auto-mappertype-detection +
  heuristic byte-pattern auto-detection"; both are delivered. CARTS.SHA is an inferior,
  redundant second DB format, not a G2 requirement.
- **New mapper devices.** Identification NEVER adds mapper types. A DB match naming a type
  outside our implemented seven (`Mirrored/8kB/16kB/ASCII8/ASCII16/Konami/KonamiSCC`) is a loud,
  non-zero-exit "identified but unsupported" outcome (§2.4.3) — implementing the ~80-type long
  tail stays G4; SCC-I stays G5.
- **openMSX's `PAGE2`/`RomPageNN` placement refinement for ≤16 KB "AB" ROMs**
  (`RomFactory.cc:97-108`): disclosed simplification. Our M19 `Mirrored` device mirrors the
  image across the whole 64 KB window (`src/devices/cartridge/cartridge_mirrored_rom.h:22-28`,
  ratified A-M19-8 — this machine's external slots have no narrower sub-window), so a PAGE2-class
  ROM's content is present at 0x8000 anyway; the BIOS calls the header's own INIT address. No
  `PageNN` enum value is added. Recorded in the heuristic spec (§2.3 step 2).
- **`--disk` / disk-image identification** — carts only (G2's scope). Disk fidelity is C10/M32-era.
- **Runtime hot-plug** (G3) — untouched.
- **ZEXALL/ZEXDOC slow sweep — NOT run this milestone** (DEC-0035: M31 QA gate only; this
  milestone touches zero CPU/core files, so `feedback_slow_test_cadence.md`'s mechanical gate
  does not fire either).
- Zero touch to `src/devices/cpu/`, `src/core/`, `src/frontend/audio_pacer.*`, and ALL existing
  cartridge mapper device files (`src/devices/cartridge/*` — identification is policy ABOVE the
  devices, never inside them). Verification: `git diff v1.0.30` at every gate.
- `Hbf1xvMachine::load_cartridge(slot, type, image)` signature and semantics unchanged
  (`src/machine/hbf1xv_machine.h:266-267`) — identification produces the `type` argument, one
  layer up.

### 1.4 Assumptions (each with a verification action)

- **A-M30-1**: `roms/aleste.rom` is present and is the `e93d0840c59c6eba273df546d22148d486a150a6`
  dump (coordinator-established via exact SHA1 match; file presence re-verified by this planner
  via directory listing this cycle). Developer verification: compute SHA1 at implementation time
  (`Get-FileHash -Algorithm SHA1 roms/aleste.rom` AND our own new `sha1.cpp` must agree — a free
  cross-validation of S1). The integration test skip-gates on presence AND on this exact hash
  (user-supplied assets may legitimately differ; a different dump → SKIP with a note, never FAIL).
- **A-M30-2**: `references/openmsx-21.0/share/softwaredb.xml` is present in-repo (verified this
  cycle: read lines 1-45 for schema; the Aleste 2 entry verified verbatim at lines 14839-14850;
  ~3,049 `<software>` entries counted). The real-DB integration test additionally skip-gates on
  its presence (references/ is committed, but the gate keeps the suite honest on partial checkouts).
- **A-M30-3**: every in-repo harness/test that passes `--cartN` also passes `--cartN-type`
  explicitly, so the new auto-identify path changes NO existing invocation's behavior. Planner
  verification this cycle: repo-wide grep for `--cart1 ` found explicit types in every
  tool/test invocation (`tools/openmsx-m19-cartridge-parity.ps1:76`,
  `tools/openmsx-m29-scc-parity.ps1:87`, all docs/QA transcripts); the only type-less usages are
  (a) `tests/unit/machine/cartridge_cli_unit_test.cpp:46` — a PARSER-level default-Mirrored
  assertion, unaffected because the parser is unchanged (§2.4.1), and (b)
  `tests/integration/frontend/sdl3_cli_session_integration_test.cpp:142` — a bad-path negative
  case that fails at file-open, before type resolution. Developer MUST re-run this grep before
  landing and record it in the implementation report.
- **A-M30-4**: WSL openMSX (`/usr/bin/openmsx`) is available for the single live A/B check
  (established repeatedly, most recently M29's A/B run). If genuinely unavailable at evidence
  time, the check is recorded BLOCKED with the concrete failure output — never fabricated.
- **A-M30-5**: no SHA-1 implementation exists anywhere in `src/` (verified this cycle: grep for
  `sha1|SHA1|Sha1` across `src/` matches only comments in `src/main.cpp:601` and
  `src/machine/hbf1xv_machine.cpp`). S1 is genuinely new code, written from the public FIPS
  180-4/RFC 3174 specification — explicitly NOT from `references/fmsx-60/source/EMULib/SHA1.c`,
  which must not be opened during S1's implementation (clean-room discipline, §6 R-M30-2).

## 2. Spec Summary

### 2.1 S1 — Clean-room SHA-1 (`src/machine/sha1.{h,cpp}`)

- SHA-1 per FIPS 180-4 (equivalently RFC 3174): 512-bit blocks, 80-round compression, standard
  padding, 160-bit digest. API: `class Sha1 { void update(std::span<const std::uint8_t>);
  [[nodiscard]] std::string hex_digest(); }` plus a one-shot convenience
  `sha1_hex(std::span<const std::uint8_t>) -> std::string` (lowercase hex, 40 chars).
- SHA-1 is a published standard algorithm; implementing it from the standard's own text is
  trivially clean-room. The unit test gates correctness on the PUBLISHED standard vectors
  (FIPS 180-1 App. A/B, RFC 3174 §7.3):
  - `""` → `da39a3ee5e6b4b0d3255bfef95601890afd80709`
  - `"abc"` → `a9993e364706816aba3e25717850c26c9cd0d89d`
  - `"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"` →
    `84983e441c3bd26ebaae4aa1f95129e5e54670f1`
  - 1,000,000 × `'a'` → `34aa973cd4c4daa4f61eeb2bdbad27316534016f`
  - plus multi-chunk streaming == one-shot equality on a synthetic multi-MB buffer (covers the
    block-boundary/padding edge cases at realistic ROM sizes).
- Placement note: `src/machine/` (not `src/core/` — zero-touch constraint; not `tools/` — the
  identifier needs it in-process). Namespace `sony_msx::machine`.

### 2.2 S2 — Software database (`src/machine/software_db.{h,cpp}`)

#### 2.2.1 Schema subset (grounded in the real file + openMSX's own parser)

Grounding read this cycle: `references/openmsx-21.0/share/softwaredb.xml:1-45` (prolog,
DOCTYPE `softwaredb1.dtd`, CDATA credits block, entry shape) and
`references/openmsx-21.0/src/memory/RomDatabase.cc` (the DBParser state machine). Facts our
parser must honor:

- Root `<softwaredb>`; entries are `<software>` elements containing `<title>` and 1..n `<dump>`
  elements; each `<dump>` contains either `<rom>` or `<megarom>`, which contains optional
  `<type>` and a 40-hex-char `<hash>` (SHA1), optionally `<remark>`, `<start>`, and an
  `<original value="...">` element.
- `<rom>` WITHOUT `<type>` defaults to type **"Mirrored"** (`RomDatabase.cc:201-208`);
  `<megarom>` has NO default — `<type>` is expected (`RomDatabase.cc:193-199`).
- A `<dump>` with no `<hash>` is dropped (`RomDatabase.cc:490-494`).
- A `<start>` value composes subtype names like `Mirrored0000` (`RomDatabase.cc:500-522`) — our
  parser records the COMPOSED name as the raw type string; all composed names fall outside our
  seven and therefore route to the loud "identified but unsupported" outcome. No silent
  truncation to plain `Mirrored`.
- Duplicate hashes: first occurrence wins (deterministic; matches openMSX's own keep-the-old
  rule, `RomDatabase.cc:437-447`).
- Robustness: XML comments, the CDATA credits block, DOCTYPE, attributes, unknown elements
  (`<genmsxid>`, `<company>`, `<year>`, `<country>`, `<remark>`, `<original>`, `<system>`, …)
  are all tolerated/skipped. This is NOT a general XML parser — it is a tolerant scanner for
  exactly this schema; malformed regions skip to the next `<software>` with a collected
  diagnostic, never a crash or abort.
- Scale/performance: ~3,049 `<software>` entries today (planner-counted). Requirement: load +
  index in well under a second on the CI box; the DB is loaded LAZILY, once per process, only
  when at least one slot actually needs auto-identification.

#### 2.2.2 API

```cpp
namespace sony_msx::machine {
struct SoftwareDbEntry { std::string title; std::string type_name; };  // type_name: raw DB string
class SoftwareDb {
 public:
  // std::nullopt when the file is absent/unreadable (graceful degradation, §2.4.2).
  // `diagnostics` collects per-entry skip notes (never fatal).
  static std::optional<SoftwareDb> load_from_file(const std::string& path,
                                                  std::vector<std::string>& diagnostics);
  [[nodiscard]] const SoftwareDbEntry* lookup(std::string_view sha1_hex_lowercase) const;
  [[nodiscard]] std::size_t entry_count() const;
};
}
```

#### 2.2.3 DB path discovery (precise)

- New flag `--softwaredb <path>`, recognized by the SHARED cartridge CLI parser
  (`ParsedCartridgeCli` gains `std::optional<std::string> softwaredb_path` — additive field), so
  ALL consumers (headless default run, `--parity-trace`, `--debug-session`, SDL3) get it with
  zero per-mode wiring, exactly like the M19 cart flags themselves.
- Default when the flag is absent: the single candidate
  `references/openmsx-21.0/share/softwaredb.xml`, resolved relative to the current working
  directory (the same working-directory convention as the existing default `bios/` asset root
  and every `tools/` script). No search list, no environment variable — one flag, one default,
  documented in both executables' usage text.
- Absent/unreadable DB (default or explicit): identification degrades to the heuristic with ONE
  informational stderr line (§2.4.4 message E). With an EXPLICIT `--softwaredb` path that cannot
  be read, the same line is a WARNING naming the failed path (the user asked for something that
  could not be honored — louder, but still non-fatal by design: the heuristic remains available).

#### 2.2.4 License analysis (state explicitly, verbatim requirement of the dispatch)

`softwaredb.xml` is GPL-project DATA that REMAINS in `references/` untouched. Our code PARSES a
locally-present file at runtime — the same posture as reading the user's own `bios/` ROMs.
Nothing from the file is copied into `src/` or `tests/`: no entry, no hash, no title ships in
our code or fixtures (test fixtures are fully synthetic/fictional, §4.1). The ONE exception is
the Aleste-2 SHA1 constant `e93d0840...` in the asset-gated integration test — that is the hash
of the USER'S OWN local ROM file (independently computable from `roms/aleste.rom` without the
DB existing at all), used as a skip-gate; it is a fact about the local asset, not database
content. If the DB file is absent, identification degrades gracefully (§2.2.3) — the emulator
never depends on the GPL data file to function.

#### 2.2.5 fMSX CARTS.SHA — evaluated and SKIPPED (decision + reasoning)

Read this cycle: `references/fmsx-60/executables/CARTS.SHA:1-15` (format: `<40-hex-sha1>
<numeric-code>` per line) and the consuming code `MSX.c:2812-2830`; codes map to fMSX's coarse
mapper indices (`MSX.h:80-88`: 0=GEN8, 1=GEN16, 2=KONAMI5/SCC, 3=KONAMI4, 4=ASCII8, 5=ASCII16).
Skip rationale: (1) Aleste 2's hash is NOT in it (planner-verified grep, zero matches); (2) its
coverage is a strict subset in usefulness of softwaredb (~3,049 titled entries with rich type
vocabulary vs. an untitled coarse-code list); (3) supporting a second DB format doubles the
parser/test/precedence surface for near-zero user benefit; (4) fMSX's `CARTS.CRC` sibling is a
plain byte-sum (`MSX.c:2800-2806`) — collision-prone, not worth touching. What we DO take from
fMSX here: its DB-before-heuristic precedence (`MSX.c:2790-2837` — CRC, then SHA, then scan)
independently corroborates the precedence design of §2.4. Recorded as a named non-goal on G2's
row at closure, not a remainder.

### 2.3 S3 — Heuristic fallback (re-derived `guessRomType` BEHAVIOR)

Grounded line-by-line in `references/openmsx-21.0/src/memory/RomFactory.cc:82-169` (read this
cycle; behavior re-derived, code never copied). Given a non-empty image (an empty `--cartN`
file keeps today's loud load-failure path — identification is skipped):

1. **Format signatures** (`RomFactory.cc:90-95`): if size ≥ 24, the 8 bytes at offset 16 are
   compared to `"ASCII16X"`, `"ROM_NEO8"`, `"ROM_NE16"` (file-format magic values — facts of
   those third-party formats, not license-sensitive data). A match → the loud
   identified-but-unsupported outcome (§2.4.3) naming the format; these are G4-tail types this
   project does not implement, and misbooting them as Generic8kB is exactly the garbage-screen
   class M30 exists to kill.
2. **size < 0x10000** (`RomFactory.cc:96-111`): → `Mirrored`. openMSX's `PAGE2` refinement for
   ≤16 KB "AB" images targeted at 0x8000 is a disclosed simplification here (§1.3): our
   `Mirrored` full-window mirror already exposes the image at page 2, and the BIOS jumps to the
   header's own INIT address.
3. **size == 0x10000 without an "AB" header** (`RomFactory.cc:112-116`): → `Mirrored`.
4. **Otherwise** (64 KB with "AB", or > 64 KB) — the bank-select-write scan
   (`RomFactory.cc:117-167`): for every `i` in `[0, size-3)`, if `data[i] == 0x32` (the Z80
   `LD (nn),A` opcode) then `addr = data[i+1] | (data[i+2] << 8)` scores:
   | addr | scores |
   |---|---|
   | `0x5000`, `0x9000`, `0xB000` | KonamiSCC +1 |
   | `0x4000`, `0x8000`, `0xA000` | Konami +1 |
   | `0x6800`, `0x7800` | Ascii8kB +1 |
   | `0x6000` | Konami +1, Ascii8kB +1, Ascii16kB +1 |
   | `0x7000` | KonamiSCC +1, Ascii8kB +1, Ascii16kB +1 |
   | `0x77FF` | Ascii16kB +1 |
   Then the deliberate ASCII8 handicap: if `ascii8 > 0`, `ascii8 -= 1` (guard exactly as the
   reference guards its unsigned underflow, `RomFactory.cc:160`). Winner: start from
   `Generic8kB` at score 0 and evaluate candidates in the FIXED order **Ascii8kB, Ascii16kB,
   Konami, KonamiSCC**, replacing the current winner when a candidate's nonzero score is `>=`
   the winner's. This documented order reproduces openMSX's enum-iteration tie-break exactly
   (`RomFactory.cc:161-166` iterates `RomType` in declaration order; the relevant subset order
   in `RomTypes.hh:8-38` is ASCII8 < ASCII16 < GENERIC_8KB < KONAMI < KONAMI_SCC, and
   GENERIC_8KB's zero score can never re-win via the `tg &&` guard) — ties therefore resolve
   KonamiSCC > Konami > Ascii16kB > Ascii8kB. All scores zero → `Generic8kB`.
   Every scored count is captured in the report's detail string (§2.4.4 message C).

**Decision: the heuristic is IN for M30** (not DB-only). Justification: (a) the DB covers only
well-dumped, cataloged software — homebrew/repro/trimmed dumps (exactly the Aleste-2-repro
class) need the fallback for "just works" to be universal; (b) EVERY type the heuristic can
output (Generic8kB default, Konami, KonamiSCC, Ascii8kB, Ascii16kB, Mirrored) is already
implemented and QA-signed — zero new device risk; (c) both behavior references implement the
same core algorithm independently, so its semantics are unusually well corroborated; (d) misfire
risk is mitigated structurally: the heuristic is LAST (below explicit flag and DB), its use is
loudly disclosed with scores, and the explicit flag always overrides.

**openMSX-vs-fMSX disagreements, recorded per DEC-0030** (fMSX `GuessROM`,
`MSX.c:2784-2878`, read this cycle). These are software POLICY divergences — no real-hardware
truth exists to arbitrate toward, so per the primary-reference rule all four arbitrate to
openMSX; none is silently dropped:
1. **Eligibility threshold**: openMSX scans only at ≥64 KB (`RomFactory.cc:96-117`); fMSX scans
   anything >32 KB (`MSX.c:3246`, `ROMMask[Slot]+1>4`). A 48 KB image: openMSX → Mirrored,
   fMSX → scan. Adopted: openMSX.
2. **Baseline biasing**: openMSX zero-initializes and applies only the conditional ASCII8
   decrement (`RomFactory.cc:125,160`); fMSX initializes ALL counters to 1, then GEN8 +1 and
   ASCII8 −1 (`MSX.c:2840-2844`) — so e.g. a single `0x77FF` hit yields ASCII16 in openMSX but
   GEN8 in fMSX (2-vs-2 tie, first-max wins at index 0). Adopted: openMSX.
3. **Tie-break direction**: openMSX `>=` later-declaration-wins (`RomFactory.cc:162-166`);
   fMSX strict `>` first-index-wins (`MSX.c:2872-2874`). Adopted: openMSX (the §2.3 step-4
   fixed candidate order).
4. **DB formats/precedence**: both agree DB-before-heuristic (`RomFactory.cc:180-201`,
   `MSX.c:2790-2837`) — an AGREEMENT, recorded as corroboration; formats differ (§2.2.5).

### 2.4 S4 — Precedence, UX, and CLI wiring

#### 2.4.1 Precedence (exact, and what does NOT change)

For each slot with a `--cartN <path>`:
1. `type_was_explicit == true` (a `--cartN-type` VALUE was given) → use it. No DB read, no
   heuristic, no new stderr output — the explicit path is byte-for-byte today's behavior (zero
   churn for every existing invocation, A-M30-3).
2. Else: compute SHA1 of the image; if a softwaredb is loadable and `lookup(sha1)` hits:
   - type name parses into our seven (`parse_cartridge_mapper_type`, which already accepts the
     openMSX-canonical strings, `src/devices/cartridge/cartridge_mapper_type.h:30-47`) → use it
     (message A);
   - type name does NOT parse → **identified-but-unsupported**: loud message B, exit non-zero.
3. Else (no DB, or no match): heuristic (§2.3) → use its result (message C or D).
4. The `Mirrored` PARSER default (`cartridge_cli.h:14-17`) is untouched — but the load layer now
   consults `type_was_explicit` before trusting it. The parser also gains ONE additive value:
   `--cartN-type auto` sets `type_was_explicit = false` explicitly (openMSX's own vocabulary,
   `RomFactory.cc:180`); `auto` is recognized ONLY at the CLI layer — the device-level enum
   parser stays pure (no `CartridgeMapperType::Auto`).
5. Machine API callers (`load_cartridge(slot, type, image)`) are unaffected: tests and tools
   that pass a type programmatically keep exact current behavior.

Placement: ALL of steps 2-3 live in ONE shared function in `cartridge_identifier.h` —
`resolve_cartridge_type(const ParsedCartridgeSlotCli&, std::span<const std::uint8_t> image,
const SoftwareDb*) -> CartridgeIdentification` — consumed by BOTH `src/main.cpp`
(`load_cartridges_from_args`) and `src/frontend/sdl3_app.cpp` (`load_configured_assets`); the
executables keep only I/O + printing glue. Identification is machine/CLI-layer POLICY, never
device behavior: zero edits under `src/devices/`.

```cpp
enum class CartridgeIdentificationMethod { ExplicitFlag, SoftwareDbSha1, HeuristicBankScan,
                                           SmallImagePlainRule };
struct CartridgeIdentification {
  devices::cartridge::CartridgeMapperType type;   // valid unless unsupported
  CartridgeIdentificationMethod method;
  std::string sha1_hex;          // always computed on the auto path
  std::string title;             // DB title when matched, else empty
  std::string db_type_name;      // raw DB/signature type string (also for unsupported case)
  bool unsupported = false;      // DB/signature named a type outside our seven
  std::string detail;            // heuristic score breakdown, e.g. "KonamiSCC=12 Konami=2 ..."
};
```

#### 2.4.2 Determinism

Identification is a pure function of (image bytes, DB file bytes, flags) — no wall clock, no
host state, no randomness. Same inputs → same `CartridgeIdentification`, unit-proven by running
the resolver twice over identical inputs and comparing every field.

#### 2.4.3 Failure policy (M19 "loud failure, never silent fallback" applies in full)

Identified-but-unsupported (DB hit outside our seven, `<start>`-composed subtype, or a §2.3
step-1 format signature) → message B on stderr and **non-zero exit** (headless: return 2 from
`load_cartridges_from_args`, the existing error-path convention, `src/main.cpp:41`; SDL3:
`last_error_` + startup abort, the existing `load_configured_assets` convention,
`sdl3_app.cpp:29-49`). Booting a known-ASCII8SRAM2 dump as a guessed Generic8kB would be
exactly the silent-garbage failure mode this milestone eliminates.

#### 2.4.4 Report messages (exact formats — tests assert these verbatim)

All on stderr, prefix `cartridge:`, produced by one shared formatter in
`cartridge_identifier.{h,cpp}`:
- **A (DB hit)**: `cartridge: --cart1: identified as "Aleste 2" (KonamiSCC) via softwaredb SHA1
  match [sha1=e93d0840c59c6eba273df546d22148d486a150a6, db=references/openmsx-21.0/share/softwaredb.xml]`
- **B (unsupported)**: `cartridge: --cart1: identified as "<title>" (<TypeName>) via <source>,
  but mapper type "<TypeName>" is not implemented (implemented: Mirrored, 8kB, 16kB, ASCII8,
  ASCII16, Konami, KonamiSCC); pass --cart1-type to force one. Aborting.`
- **C (heuristic)**: `cartridge: --cart1: no softwaredb match [sha1=<...>]; guessed KonamiSCC
  via bank-write heuristic (scores: KonamiSCC=12 Konami=2 ASCII8=0 ASCII16=1); pass
  --cart1-type to override`
- **D (small-image rule)**: `cartridge: --cart1: no softwaredb match [sha1=<...>]; image < 64 KB
  -> Mirrored (plain-ROM rule); pass --cart1-type to override`
- **E (DB absent, once per process)**: `cartridge: softwaredb not found at
  "references/openmsx-21.0/share/softwaredb.xml" -- SHA1 identification disabled (heuristic
  only); pass --softwaredb <path> to enable` (WARNING-worded variant when an explicit
  `--softwaredb` path failed).

#### 2.4.5 Usage text

Both executables' usage lines gain `[--softwaredb <path>]` and the note that `--cartN-type` is
optional (auto-identified when omitted; `auto` accepted explicitly).

### 2.5 A/B plan (single meaningful check; the rest N/A by design)

Identification is frontend/composition policy with no hardware analogue — per-register A/B
would be fabricated relevance. The ONE meaningful check: **both emulators, given the same
`roms/aleste.rom` with NO type forcing, must land on the same identification** (openMSX resolves
`auto` via its own softwaredb, `RomFactory.cc:176-201`).
- Side A (ours): `sony_msx_headless` (and/or the SDL3 exe under dummy drivers) with
  `--cart1 roms/aleste.rom` and NO `--cart1-type` → assert exit 0 + stderr message A naming
  KonamiSCC; frame-dump evidence of the loader banner (M29 precedent: "Konami8 mapper" is the
  v8 loader's own banner under a KonamiSCC boot).
- Side B (openMSX on WSL): launch with `-carta <rom>` and NO `-romtype`; capture the resolved
  type (Tcl-queryable per `RomFactory.cc:212-219`'s written-back `mappertype`; the developer
  verifies the exact Tcl query in source FIRST, per the A-M29-3 syntax-verification precedent —
  fallback evidence: a screenshot showing the same loader banner).
- Script `tools/openmsx-m30-identification-ab.ps1` → `docs/m30-identification-ab.md`. If WSL
  openMSX is unavailable (contra A-M30-4): record BLOCKED with concrete output, never fabricate.

### 2.6 Backlog re-affirmation (35 rows, per DEC-0005)

Consulted `agent-protocol/state/deferred-backlog.md` this cycle. M30 closes **G2** and touches
no other row's status:
- **DONE (unchanged)**: B1-B9; C2-C9 (C5 DONE per DEC-0034); D1-D3, D5-D7; E2; G1 (M29).
- **IN-PROGRESS/OPEN (unchanged)**: C1/D4 (UNBUILDABLE-WITHOUT-FABRICATION ruling stands
  untouched — no VDP-timing work here); C10 (deferred; numeric owner shifts per DEC-0035, see
  §2.7); E1 (renumber applied this cycle, §2.7); F1, F2 (numeric owners shift per DEC-0035,
  factual note only); G3, G4, G5 (untouched).
- **G2 → DONE (M30)** at closure, with named non-goals recorded on the row: CARTS.SHA/CARTS.CRC
  skipped (§2.2.5 reasoning); PAGE2 placement refinement subsumed by A-M19-8's Mirrored
  full-window mirror (disclosed simplification); unsupported-type identifications fail loud by
  design (implementing those mappers is G4/G5, not G2).

### 2.7 E1 renumber decision (dispatch item 7 — planner's call, made here)

**Apply the E1 M30→M31 renumber in THIS package's backlog pass, not at M31 kickoff.**
Reasoning: DEC-0035 already formally renumbered E1 ("E1's named owner formally renumbered
M30→M31") and deferred only the LEDGER EDIT to "M31 kickoff"; but M30 is now underway as the
G2/identification milestone, so the E1 row's "DEFERRED → M30" text is factually WRONG today in
a way it was not when DEC-0035 wrote that instruction (two different milestones would both
claim M30 in the ledger during an active autonomous run). The edit is mechanical bookkeeping of
an already-human-ratified decision — not a scope change, so no new decision entry is required;
the closure entry cites DEC-0035 for it. Also applied as factual one-line notes (no status
change): C10/F1/F2's numeric owners shift back one (M32/M33/M34 era) per DEC-0035's own text,
with final numbers still recorded at their own kickoffs.

## 3. Evidence gates (run and capture; never fabricate)

1. `tools/validate-assets.ps1` — required BIOS present + ≥1 ROM.
2. `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — reproducible checksums.
3. `cmake --build build --config Debug` (headless) AND the SDL3-ON build directory — both green.
4. `ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure` — headless fast
   subset: 159 baseline + new tests, all green. SDL3-ON fast subset: 168 baseline + new, all
   green (dummy drivers). **ZEXALL/ZEXDOC slow sweep NOT run** (DEC-0035: M31 QA gate only;
   zero CPU/core touch this milestone — developer confirms via `git diff v1.0.30 --stat
   src/devices/cpu src/core src/frontend/audio_pacer.h src/frontend/audio_pacer.cpp` EMPTY).
5. A/B identification agreement per §2.5 (`docs/m30-identification-ab.md`), or an honest
   BLOCKED record.
6. `git diff v1.0.30` audit: zero changes under `src/devices/` (all of it — identification
   never touches devices), `src/core/`, `src/peripherals/`, `src/frontend/audio_pacer.*`.

## 4. Milestones (slices S1-S6) and test plans

### S1 — SHA-1 (`src/machine/sha1.{h,cpp}`)
Unit: `tests/unit/machine/sha1_unit_test.cpp` — the four published vectors + streaming==oneshot
(§2.1). Clean-room note recorded in the header comment (implemented from FIPS 180-4/RFC 3174;
`references/fmsx-60/source/EMULib/SHA1.c` deliberately never opened).

### S2 — SoftwareDb (`src/machine/software_db.{h,cpp}`)
Unit: `tests/unit/machine/software_db_unit_test.cpp` against SYNTHETIC fixtures written to a
temp dir at test runtime (fictional titles/types; hashes computed by OUR sha1 over tiny
synthetic images — co-validates S1+S2 without touching real DB content; never a copy of any
real softwaredb entry): rom-defaults-Mirrored; megarom-with-type; hashless-dump dropped;
`<start>`-composed name preserved raw; CDATA/comments/DOCTYPE/unknown-tags tolerated; duplicate
hash → first wins; malformed entry skipped with diagnostic; absent file → `std::nullopt`;
`entry_count()` sanity.

### S3 — Identifier (`src/machine/cartridge_identifier.{h,cpp}`)
Unit: `tests/unit/machine/cartridge_identifier_unit_test.cpp` — constructed images with planted
`0x32 lo hi` sequences hitting every §2.3 score-table row (each expected winner hand-computed
in comments); the ASCII8 handicap (one 0x6800 hit alone → the decrement leaves ASCII8=0 →
Generic8kB); tie cases proving KonamiSCC > Konami > Ascii16kB > Ascii8kB resolution; size gates
(<64 KB → SmallImagePlainRule/Mirrored; ==64 KB no-AB → Mirrored; ==64 KB with AB → scan);
signature rows ("ASCII16X"/"ROM_NEO8"/"ROM_NE16" → unsupported); precedence (explicit flag
bypasses DB+heuristic — a poisoned DB entry for the same hash proves the bypass; DB beats
heuristic — an image whose scan says Konami but whose synthetic DB entry says ASCII16 resolves
ASCII16); unsupported DB type → `unsupported=true` + message B text verbatim; absent DB → C/D
messages verbatim; two-run determinism (§2.4.2).

### S4 — CLI wiring (both executables)
- `cartridge_cli.{h,cpp}`: additive `--softwaredb <path>` + `--cartN-type auto`; additive rows
  in `tests/unit/machine/cartridge_cli_unit_test.cpp` (existing assertions unmodified — the
  parser's Mirrored default and `type_was_explicit` semantics are unchanged).
- `src/main.cpp` `load_cartridges_from_args` + `src/frontend/sdl3_app.cpp`
  `load_configured_assets`: read image → call the ONE shared resolver → print report → load or
  abort. Additive `frontend_sdl3_cli_unit_test` row proving `--softwaredb` flows through
  `parse_sdl3_cli`'s delegated cartridge parse.

### S5 — Integration/system evidence
- `tests/integration/machine/hbf1xv_m30_cartridge_identification_integration_test.cpp`
  (compile-defs `SONY_MSX_ROMS_DIR` + a new `SONY_MSX_SOFTWAREDB_PATH=
  "${CMAKE_SOURCE_DIR}/references/openmsx-21.0/share/softwaredb.xml"`, per DEC-0016's
  established absolute-path pattern, `tests/CMakeLists.txt:587-594` precedent): skip-gates on
  `roms/aleste.rom` present AND sha1==`e93d0840...` AND the DB file present; then asserts
  lookup → ("Aleste 2", "KonamiSCC"), resolver → KonamiSCC via SoftwareDbSha1, message A text,
  `load_cartridge` → Ok, and a bounded real-frame-loop boot smoke (DEC-0034 shape,
  `step_cpu_instruction()` + `on_vsync_boundary()`, ~60 frames, machine alive — deep-boot
  evidence already exists from M29's committed PNGs; keep this test fast).
- SDL3-gated additive case in `tests/integration/frontend/sdl3_cli_session_integration_test.cpp`:
  `--cart1 <aleste> ` with NO type under dummy drivers → session starts (covers the Sdl3App
  path end-to-end); same skip-gating.
- Headless CLI-level coverage rides the §2.5 A/B script (real `sony_msx_headless.exe`
  invocation, exit 0 + message A captured in `docs/m30-identification-ab.md`).

### S6 — Evidence, ledger, docs
A/B run (§2.5); `docs/m30-implementation-report.md`; ledger updates (G2 → DONE (M30), E1
renumber + C10/F1/F2 notes per §2.7, `state/current-phase.md`/`state/milestones.md` refresh);
usage-text updates; evidence gates §3 captured. Tag v1.0.31 is the coordinator's step after QA.

## 5. Acceptance Criteria

1. SHA-1 unit test passes all four published FIPS/RFC vectors + streaming equality (§2.1).
2. SoftwareDb parses the REAL local `references/openmsx-21.0/share/softwaredb.xml` (when
   present) and `lookup("e93d0840c59c6eba273df546d22148d486a150a6")` returns title "Aleste 2",
   type "KonamiSCC" — asset-gated integration evidence.
3. All §4-S2 synthetic-fixture parser semantics unit-proven; zero real-DB content in any
   committed fixture (QA greps for it).
4. Heuristic matches the re-derived §2.3 semantics on hand-computed constructed images,
   including score table, ASCII8 handicap, tie order, size gates, and signature outcomes; the
   four DEC-0030 disagreement records are present in the code/docs with citations.
5. Precedence unit-proven: explicit flag > DB > heuristic; explicit flag consults neither DB
   nor heuristic (poisoned-DB proof); `--cartN-type auto` behaves as omitted; omitted-type
   parser default (`Mirrored`, `type_was_explicit=false`) unchanged at the parser layer.
6. Loud reporting: messages A-E produced verbatim by the shared formatter (unit-asserted);
   identified-but-unsupported → non-zero exit on the headless path and startup abort on the
   SDL3 path, with NO cartridge left partially loaded.
7. Both executables resolve through the ONE shared `resolve_cartridge_type` (code-review
   criterion + the S4/S5 tests); `--softwaredb` recognized by both; absent-DB graceful path
   proven (message E + heuristic still functional).
8. Real-ROM end-to-end: `--cart1 roms/aleste.rom` with NO type → identified KonamiSCC via
   softwaredb SHA1 match, loads Ok, bounded boot smoke passes (asset-gated skip discipline per
   DEC-0016 precedent).
9. Regression: headless fast subset (159 baseline + new) and SDL3-ON fast subset (168 baseline
   + new) fully green; every pre-existing test unmodified except named additive rows;
   `git diff v1.0.30` shows zero changes under `src/devices/`, `src/core/`, `src/peripherals/`,
   `src/frontend/audio_pacer.*`. A-M30-3's grep re-run recorded in the implementation report.
10. A/B: openMSX (no `-romtype`) and this emulator (no `--cart1-type`) agree on KonamiSCC for
    `roms/aleste.rom` (`docs/m30-identification-ab.md`), or an honest BLOCKED record with
    concrete output.
11. ZEXALL/ZEXDOC explicitly NOT run, with the zero-CPU/core-touch git-diff proof recorded
    (DEC-0035 discipline).
12. Ledger closure: G2 → DONE (M30) with the §2.6 named non-goals; E1 renumbered M30→M31 (per
    §2.7, citing DEC-0035); determinism proof (identical inputs → identical identification,
    twice) recorded.

## 6. Risks

- **R-M30-1 (Medium): minimal-XML-parser scope creep or fragility.** The real file has CDATA,
  comments, DOCTYPE, attributes, and 3,049 entries. Mitigation: §2.2.1 pins the exact tolerated
  subset and the skip-to-next-`<software>` recovery rule; the real-DB integration test is the
  canary; diagnostics are collected, never fatal.
- **R-M30-2 (Low, discipline): SHA-1 clean-room.** `references/fmsx-60/source/EMULib/SHA1.c`
  must not be opened during S1. Mitigation: implement solely from FIPS 180-4/RFC 3174; the
  published vectors are the correctness gate; developer states the discipline in the
  implementation report.
- **R-M30-3 (Low): behavior change for type-less `--cartN` invocations.** By design — that IS
  the milestone. A-M30-3's verified grep shows no in-repo harness relies on the old silent
  Mirrored default; the explicit-flag path is byte-for-byte unchanged. Residual risk: a HUMAN's
  muscle-memory invocation that wanted Mirrored now gets a DB/heuristic answer — mitigated by
  the loud report line and the always-available explicit override.
- **R-M30-4 (Low): heuristic misidentification.** Inherent to any guesser (openMSX ships the
  same). Structurally mitigated: DB outranks it, its use + scores are loudly disclosed, the
  explicit flag always wins, and unsupported identifications abort rather than misboot.
- **R-M30-5 (Low): default DB path is CWD-relative.** Running the exe from outside the repo
  root silently degrades to heuristic-only. Accepted for M30 (same CWD convention as the
  default `bios/` root); message E makes the degradation visible; `--softwaredb` overrides.
  Coordinator attention: if a future "installed binary" story matters, path discovery becomes a
  real design item (candidate for the packaging-era backlog, not now).
- **R-M30-6 (Low): openMSX-side auto-type Tcl query mechanism unverified.** The A/B side-B
  query (§2.5) needs its exact Tcl syntax verified in openMSX source first (A-M29-3 precedent).
  Fallback (banner screenshot) specified; worst case the check records BLOCKED honestly.
- **R-M30-7 (Info): GPL-data posture.** §2.2.4's analysis is explicit and conservative (runtime
  parse of a local file; nothing copied into src/tests; graceful absence). QA should
  independently re-affirm it during sign-off so the record carries two readings.

## 7. Developer Handoff

Implement S1→S6 in order (S1/S2/S3 are dependency-ordered; S4 depends on S3; S5 on S4). Hard
constraints: zero edits under `src/devices/`, `src/core/`, `src/peripherals/`,
`src/frontend/audio_pacer.*`; `load_cartridge` API unchanged; parser defaults unchanged; no
ZEXALL; fast subsets are the gates; universal-fixes-only (nothing keyed to Aleste 2 anywhere in
production code — the title string appears only in test expectations and evidence docs).
Verification actions carried from §1.4: re-run the A-M30-3 grep; SHA1 `roms/aleste.rom` both
via PowerShell and the new S1 code; verify the §2.5 side-B Tcl mechanism in openMSX source
before scripting it. Deliverables: the S1-S3 sources + unit tests, S4 wiring + additive CLI
tests, S5 integration/SDL3 tests, `tools/openmsx-m30-identification-ab.ps1`,
`docs/m30-identification-ab.md`, `docs/m30-implementation-report.md`, ledger updates per §2.6/
§2.7. Leave all changes uncommitted for coordinator/QA per the established cadence.
