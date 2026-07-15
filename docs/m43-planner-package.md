# M43 Planner Package — MSX Extended-Statement (`CALL`) Dispatch Fix + FM-PAC Bit-Exact SRAM

- Milestone: **M43** (DEF-M43-CALLDISPATCH + FM-PAC persistent-SRAM hardening)
- Authority: **DEC-0062** (`agent-protocol/channels/decisions.md:739-748`) — the authoritative scope.
- Release target: **v1.1.0** (minor bump — restores a whole BASIC subsystem).
- Planner role: spec + sequence + acceptance ONLY. No production code in this package.
- Standing rules applied HARD: system-wide review of memory/slot addressing BEFORE any code
  ([[feedback_system_wide_review_first]]); universal fix, never per-command/per-game
  ([[feedback_universal_fixes_only]]); license isolation — openMSX/fMSX read for understanding,
  never copied ([CLAUDE.md] Reference materials, guardrails.md:48-52).

---

## 1. Scope and Assumptions

### 1.1 Problem statement (established — do NOT re-investigate the verdict; plan the fix)

Our engine returns **"Syntax error"** for EVERY BASIC `CALL` expansion (`CALL MUSIC`, `CALL FMPAC`,
`_MUSIC`, …) on the bare HB-F1XV AND with the canonical FM-PAC inserted, while openMSX — byte-identical
ROMs, identical slot layout — accepts `CALL MUSIC` = `Ok` and launches the FM-PAC "PAC2 BACKUP DATA"
manager on `CALL FMPAC` (verdict captured in DEC-0062; A/B artifacts in `debug/fmpac-call-ab/`,
openMSX `Ok` frames `frames/omx_bare_1_music.png`, our failing frames `frames/ours_*`).

The MSX BIOS handles an unrecognized `CALL <name>` by a PASSIVE inter-slot scan: for each slot/subslot
it reads page-1 `0x4000` for the `AB` header, and — for any header with a non-zero STATEMENT vector at
`0x4004` — INTER-SLOT-CALLs that handler (via the BIOS ROM routines RDSLT/WRSLT/CALSLT/ENASLT, which
themselves drive `#A8` primary-select and the `#FFFF` expanded-subslot register). If no handler claims
the name, BASIC prints "Syntax error". The observed failure means **no extension ROM's STATEMENT handler
is ever successfully reached/claimed** on our engine.

Grounded facts (verified this cycle):
- Built-in MSX-MUSIC ROM is mapped at **slot 3-3 page 1** (`src/machine/hbf1xv_machine.cpp:153`,
  matching `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`); its BIOS is `bios/f1xvmus.rom`
  (APRLOPLL, sha1 aad42ba4), INIT=0, sharing the MSX-MUSIC CALL table (MUSIC/VOICE/TEMPO/TRANSPOSE/PITCH).
- The FM-PAC firmware defines `CALL FMPAC` (STATEMENT handler ROM 0x4082 string-compares "FMPAC").
- **Disk-BASIC boots** (slot 3-2, also inside expanded primary 3) — proving that SOME expanded-subslot
  inter-slot scan works. So the exact diverging primitive is **NOT yet isolated** — that is Slice 1's job.

### 1.2 In scope (two hard deliverables, per DEC-0062)

1. **DEF-M43-CALLDISPATCH** — UNIVERSAL slot-bus inter-slot correctness so the real BIOS's passive
   `CALL`-extension scan enumerates + inter-slot-calls the STATEMENT handler of ANY `AB`-header ROM in
   every slot/subslot: built-in MSX-MUSIC (expanded 3-3) AND external cartridge slots (FM-PAC).
2. **FM-PAC persistent SRAM hardware-bit-exact** — verify/harden the 8 KB battery SRAM (magic unlock,
   enable `0x7FF6` incl. bit4, bank `0x7FF7`, window `0x4000-0x5FFD`, host-file persistence) to
   near-bit-exact real-part + openMSX `MSXFmPac.cc` parity, working through BOTH the game register-path
   (already working) AND the newly-enabled `CALL FMPAC` manager path.

### 1.3 Out of scope (would require a new decision entry)

- No new FM/OPLL DSP work (license-sensitive-scope ban stands — [[feedback_license_sensitive_scope]]).
- No change to `#A8`/PPI slot-select behavior beyond what Slice 1 proves is required (boot/disk depend
  on it byte-for-byte).
- No expansion of the machine's static slot map (slots/subslots/pages stay exactly as
  `Sony_HB-F1XV.xml`); the fix corrects inter-slot ACCESS semantics, not the wiring.
- No per-command / per-game / per-name special-casing anywhere.
- SRAM cross-emulator FILE portability is a decision point (see §7 Open Questions), not an assumed goal.

### 1.4 Assumptions (each with a verification action)

- **Assumption A1:** The CALL/extended-statement dispatch is a fresh RDSLT/CALSLT re-scan at CALL time
  (not a boot-built table lookup), so a correct inter-slot access is sufficient to fix it.
  *Verify:* Slice 1 trace-diff will show the BIOS re-reading `0x4000/0x4004` per slot at CALL time.
- **Assumption A2:** The steady-state slot resolution in `slot_bus.cpp` is arithmetically correct vs
  openMSX (see §3); the divergence is a subtler behavior exercised only by the CALL enumeration.
  *Verify:* Slice 1 confirms the exact diverging access; if instead a gross steady-state bug surfaces,
  the fix is simpler and the hypothesis is revised (no harm — the diagnostic gates it).
- **Assumption A3:** The fix is confined to `src/devices/chipset/slot_bus.{h,cpp}` (± machine wiring),
  NOT `src/core`. *Verify:* Slice 1's file map; if a `src/core/bus.h` contract change is required, that
  ESCALATES (wider review + the mechanical ZEXALL trigger fires — §5, §6).
- **Assumption A4:** The canonical 64 KB FM-PAC (`roms/fmpac.rom`, sha1 9d789166 per DEC-0062) is
  present. *Verify:* `tools/validate-assets.ps1` + `tools/checksum-assets.ps1` at the evidence gate
  (roms/ is gitignored — confirm the concrete path before any run; never fabricate provenance).
- **Assumption A5 (Slice 3, HIGH-VALUE):** openMSX persists the FM-PAC `.sram` as a **16-byte
  `"PAC2 BACKUP DATA"` header + 8190 (`0x1FFE`) data bytes**, whereas our `CartridgeFmPacRom` persists
  **raw 8192 bytes with no header** — a real format mismatch (§3.4). *Verify:* already read from
  `references/openmsx-21.0/src/sound/MSXFmPac.cc:11` and `src/memory/SRAM.cc:83-131`; resolution is a
  Slice 3 acceptance item + Open Question §7.

---

## 2. Spec Summary — the inter-slot mechanism the fix must serve

The BIOS ROM routines (RDSLT/WRSLT/CALSLT/ENASLT) are pure Z80 code we do NOT implement — we only run
them. Our engine's ONLY obligations on the inter-slot path are to service, byte-exactly like openMSX:

| Primitive | CPU-visible surface | Our implementation | openMSX reference |
| --- | --- | --- | --- |
| Primary slot select | `OUT (#A8)`, `IN (#A8)` (+ S1985 `#AC` mirror) | `PpiSlotSelect` → `SlotBus::set_primary_select`/`primary_select` | MSXCPUInterface `setPrimarySlots` (724-748) |
| Secondary (subslot) select | `LD (#FFFF),v` when page-3 primary expanded | `SlotBus::write_ffff` (`sub_slot_register_[primary_for_page(3)] = v`) | `setSubSlot` (755-761), `writeMemSlow` (862-863) |
| Subslot readback | `LD a,(#FFFF)` when page-3 primary expanded | `SlotBus::read_ffff` (`0xFF ^ sub_slot_register_[primary_for_page(3)]`) | MSXCPUInterface.cc:209-210, 769-770, 825-826 |
| Slot/subslot/page mem access | any read/write | `SlotBus::read`/`write` → `devices_[primary][sub_for_page][page]` | `readMem`/`writeMem` visible-slot resolution |
| Expansion presence | expansion-detect probe of `#FFFF` | `expanded_[primary]` (slots 0 & 3 true) | `isExpanded` |

The passive CALL scan (BIOS) walks all of the above to enumerate `AB`-header ROMs across
{primary 0-3} × {subslot 0-3 when expanded} × page 1, reading `0x4000` (`AB`) and `0x4004` (STATEMENT),
and CALSLTs any non-zero STATEMENT with the procedure name in `PROCNM`. The fix must make that walk
enumerate + inter-slot-call the STATEMENT handler of every `AB` ROM present — a strict SUPERSET of
today's behavior (boot, Disk-BASIC in 3-2, RAM mapper 3-0, memory-mapper #FC-#FF, external carts must
all stay byte-identical).

---

## 3. Slot-Bus Architecture Review (our SlotBus vs openMSX inter-slot semantics)

Read of `src/devices/chipset/slot_bus.{h,cpp}` against
`references/openmsx-21.0/src/cpu/MSXCPUInterface.cc`:

### 3.1 What MATCHES openMSX (verified by inspection)

- **#A8 primary decode:** `primary_for_page(page) = (primary_select_ >> (2*page)) & 3` ≡ openMSX
  `primarySlotState[page]`.
- **Subslot field extraction:** `sub_for_page(primary,page) = (sub_slot_register_[primary] >> (2*page))
  & 3` ≡ openMSX `secondarySlotState[page] = (subSlotRegister[ps] >> (2*page)) & 3`
  (setPrimarySlots 726/732/738/746, setSubSlot 759-760). Page 1 → bits 2-3 on both.
- **#FFFF write** (`slot_bus.cpp:78-83`): sets `sub_slot_register_[primary_for_page(3)] = value` ≡
  `setSubSlot(primarySlotState[3], value)` (MSXCPUInterface.cc:220, 862-863).
- **#FFFF read** (`slot_bus.cpp:85-89`): `0xFF ^ sub_slot_register_[primary_for_page(3)]` ≡
  MSXCPUInterface.cc:209-210 `0xFF ^ subSlotRegister[primarySlotState[3]]`.
- **Expanded gate:** `read`/`write` return the `#FFFF` register only when `expanded_[primary_for_page(3)]`
  ≡ openMSX `isExpanded(primarySlotState[3])` (209, 219).
- **Machine wiring** (`hbf1xv_machine.cpp:111-164`): primaries 0 & 3 expanded; 3-0 RAM mapper all pages;
  3-1 sub/kanji; 3-2 FDC page 1 (Disk-BASIC — boots); **3-3 fmmusic_rom_ page 1**; external carts 1 & 2
  unexpanded, all pages sub 0. Matches `Sony_HB-F1XV.xml`.

**Conclusion:** the STEADY-STATE inter-slot arithmetic is correct. Disk-BASIC (3-2, expanded) boots,
proving RDSLT/CALSLT/ENASLT + `#FFFF` all work for at least the boot-time INIT scan of an expanded
subslot. **The bug is therefore NOT a gross arithmetic error in `slot_bus` — it is a subtler behavior
exercised specifically by the CALL-time enumeration.** (This is exactly why DEC-0062 mandates a
diagnostic-first slice rather than a blind edit.)

### 3.2 Leading root-cause hypothesis (to CONFIRM in Slice 1 — NOT assumed)

**H-A (primary): the RDSLT read-through / `#FFFF` enumeration diverges in a TRANSIENT slot state that
the steady-state path never covers.** ENASLT/RDSLT for an expanded target momentarily selects the target
primary into page 3 to program `#FFFF` for a *non-page-3* target page, then restores. If, in that
transient, our resolution of (a) which primary's `sub_slot_register_` the `#FFFF` access addresses,
(b) the open-bus fallback for a partially/newly-selected cell, or (c) an unexpanded-primary `#FFFF`
falling through to the cartridge device, differs from openMSX by even one access, the CALL scan reads a
wrong header/vector and skips the handler → "Syntax error".

**H-B (secondary): the failure is UNIVERSAL to the CALL/extended-statement dispatch** (not
slot-3-3-specific) — no extension STATEMENT handler is ever invoked. Root would be either (i) an
inter-slot primitive used by the CALL dispatch but not the boot INIT scan, or (ii) an extension INIT not
running at boot so a required RAM hook/work-area is absent (note: OPLL audio working per M31 was games
driving `#7C/#7D` directly — it does NOT prove MSX-MUSIC's BIOS INIT ran).

Both hypotheses converge on the SAME fix surface (inter-slot RDSLT/CALSLT/ENASLT + `#FFFF` servicing in
`slot_bus`), so Slice 2's contract covers both. Slice 1 **cheaply bisects H-A vs H-B before the heavy
trace-diff** by A/B-testing a *disk* CALL (e.g. `CALL SYSTEM`): if it also fails → H-B (universal
dispatch); if only 3-3/cart CALLs fail → H-A (expanded-subslot enumeration). The trace-diff then
pinpoints the exact diverging instruction/access and selects the minimal universal correction.

### 3.3 The `#A8`/PPI path (must stay byte-identical)

`PpiSlotSelect` + the S1985 `#A8→#AC` mirror underpin boot/disk. Any Slice 2 change is forbidden from
altering the `#A8` read/write surface (DEC-0062 "strict superset"; regression-guarded in §4).

### 3.4 FM-PAC SRAM review (Slice 3 grounding)

Our `CartridgeFmPacRom` (`src/devices/cartridge/cartridge_fmpac_rom.cpp`) is a faithful transcription of
`references/openmsx-21.0/src/sound/MSXFmPac.cc` at the REGISTER level — magic unlock (0x1FFE=0x4D &&
0x1FFF=0x69, AND, re-checked on every write), enable 0x3FF6 (`v & 0x11`, bit4 force-reset clears the
magic latch), bank 0x3FF7 (`v & 0x03`), SRAM window write gated `sram_enabled_ && rel < 0x1FFE`. These
MATCH openMSX `readMem`/`writeMem`/`checkSramEnable`. Two REAL discrepancies at the PERSISTENCE layer:

1. **Chip/window size:** ours `kSramBytes = 0x2000` (8192, full chip) but only `0x1FFE` (8190) is
   addressable (top 2 bytes shadowed by the magic registers). openMSX models exactly **0x1FFE = 8190**
   (`MSXFmPac.cc:11`). Our extra 2 bytes are never written/observed through the window, but they DO
   appear in the raw `.sram` file.
2. **File format:** openMSX `SRAM::save()` (`src/memory/SRAM.cc:114-131`) writes a **16-byte header
   `"PAC2 BACKUP DATA"` THEN 8190 data bytes** (= 8206-byte file), and `SRAM::load()` (83-112) validates
   the header before loading. Our persistence writes **raw 8192, no header** (`BatteryBackedSram::save`,
   `battery_backed_sram.h:74-76`). So our `.sram` file is NOT byte-interchangeable with openMSX's, and a
   naïve file-level SHA A/B would falsely diverge.

Slice 3 must resolve both (see §4 Slice 3 and Open Question §7).

---

## 4. Milestones (Slices) — interface contracts, acceptance, test obligations, blast radius

Sequenced with a HARD gate between Slice 1 and Slice 2: **no production edit until Slice 1's diagnosis
names the diverging primitive and the confirming trace excerpt is captured.**

### Slice 1 — DIAGNOSTIC (must come first; NO production code)

- **Objective:** Pinpoint the exact diverging inter-slot primitive with an A/B CPU trace of the identical
  `CALL MUSIC` keystrokes, confirming or revising the §3.2 hypothesis.
- **Method (bounded, avoids the `--trace-cpu` OOM noted at `src/main.cpp:778`):**
  1. **Cheap bisection FIRST:** A/B `CALL SYSTEM` (disk) and `CALL FMPAC` (FM-PAC) alongside `CALL MUSIC`,
     reusing the `debug/fmpac-call-ab/` scripts + `tools/gen-typed-ab.py` (emits BOTH our
     `HBF1XV-INPUT-SCRIPT` and the openMSX `keymatrixdown/up` Tcl from one keystroke sequence). Classify
     H-A vs H-B.
  2. **Compact timeline:** `sony_msx_headless --debug-session bios <steps> --input-script <call-music>
     --io-observe <csv>` to capture the `#A8` writes, and a `MemWriteObserver` (`memory_mapper_ram.h:35`,
     or a small diagnostic seam) to log `#FFFF` writes/reads across the CALL window.
  3. **Pinpoint trace:** a WINDOW-BOUNDED `--trace-cpu` capture around the CALL keystroke on our engine,
     vs an openMSX per-instruction trace over the same window (openMSX Tcl debugger: `debug`
     step/probe/watchpoint on `0xFFFF` + slot map). Diff to find the FIRST differing inter-slot access
     result (a RDSLT read returning open-bus 0xFF vs the real STATEMENT vector, a CALSLT execute-through
     landing at a different PC, or a `#FFFF` readback mismatch).
- **Deliverable:** a written diagnosis (in the implementation report / `debug/fmpac-call-ab/`) naming the
  diverging primitive, the confirming trace excerpt (both sides), and the concrete file/line the Slice 2
  fix will touch — **including whether it is `src/core` and/or `src/machine`** (governs the ZEXALL
  trigger, §6).
- **Acceptance (gate to Slice 2):**
  - A1. The H-A/H-B bisection result is stated with evidence (which CALLs fail on our side).
  - A2. The exact diverging inter-slot access is identified with a both-sides trace excerpt.
  - A3. The Slice 2 file/blast-radius map is produced and the ZEXALL-trigger determination recorded.
- **Test obligations:** none (diagnostic). Tooling only: `debug/fmpac-call-ab/*`, `tools/gen-typed-ab.py`,
  `--debug-session`/`--trace-cpu`/`--io-observe`. No `src/` edits.
- **Blast radius:** NONE (read-only + tooling). Reference reads: `MSXCPUInterface.cc` (understanding
  only, never copied).

### Slice 2 — UNIVERSAL slot-bus inter-slot fix

- **Objective:** Correct the inter-slot access semantics so the passive `CALL`-extension scan enumerates
  + inter-slot-calls the STATEMENT handler of ANY `AB`-header ROM in every slot/subslot — built-in
  MSX-MUSIC (expanded 3-3) AND external cartridge slots (FM-PAC) — as a STRICT SUPERSET of today.
- **Interface contract:**
  - The fix corrects `SlotBus` inter-slot ACCESS behavior identified by Slice 1; it does NOT change the
    static slot map, the `#A8` surface, or any public method signature unless Slice 1 proves it necessary
    (any signature/contract change is called out for QA).
  - **Universality invariant:** the correction is expressed in slot/subslot/page terms, keyed to NO
    device, name, or game (grep-checkable: no `"MUSIC"`/`"FMPAC"`/`0x4082`/game literals in the diff).
  - **Superset invariant:** existing working paths (boot to BASIC `Ok`, Disk-BASIC boot in 3-2, RAM
    mapper 3-0, memory-mapper `#FC-#FF` segments, external cart load/run, `#FFFF`/`#A8` steady state) are
    byte-identical. Proven by the unchanged oracles staying green under a from-scratch rebuild.
- **Acceptance:**
  - B1. `CALL MUSIC` → `Ok` (typed-BASIC A/B vs openMSX); the MSX-MUSIC CALL table
    (VOICE/TEMPO/TRANSPOSE/PITCH) also dispatches (no "Syntax error").
  - B2. `CALL FMPAC` → launches the "PAC2 BACKUP DATA" manager (with the FM-PAC inserted), matching
    openMSX.
  - B3. Every pre-existing memory/slot/boot/disk/mapper oracle stays green, UNCHANGED (no oracle
    weakened/rebaselined).
- **Test obligations (deterministic, `tests/`):**
  - Unit: SlotBus inter-slot access across {primary}×{subslot}×{page} incl. the expanded-3 subslot
    transient from Slice 1; `#FFFF` write→readback (`0xFF ^ reg`) per primary; unexpanded `#FFFF`
    fall-through to device; expansion-detect probe (slots 0/3 expanded, 1/2 not, incl. FM-PAC page-3
    open-bus).
  - Unit: `AB`-header enumeration across slots/subslots — a synthetic multi-slot fixture with `AB`+STATEMENT
    ROMs in an expanded subslot AND an external slot, asserting the enumeration visits both (the universal
    mechanism, device-agnostic; NO real BIOS bytes in the fixture).
  - Integration: a real-BIOS boot-to-`Ok` then a scripted `CALL MUSIC` reaching a non-"Syntax error"
    outcome (headless, deterministic).
  - Regression: an ADVERSARIAL negative control (mutate the fix → the new enumeration test fails; restore
    → green) to prove non-tautology.
- **Blast radius (EXPECTED):** `src/devices/chipset/slot_bus.{h,cpp}` (primary); `src/machine/hbf1xv_machine.cpp`
  ONLY if enumeration needs a wiring change; `src/core/*` NOT expected. **ZEXALL trigger:** slot_bus is
  `src/devices/chipset` (NOT `src/devices/cpu`/`src/core`), so the mechanical per-edit trigger does NOT
  fire — BUT DEC-0062 EXPLICITLY mandates the full sweep at QA regardless (§6). If Slice 1 forces a
  `src/core/bus.h` contract change, the mechanical trigger ALSO fires and review widens.

### Slice 3 — FM-PAC SRAM hardware-bit-exact (verify + harden)

- **Objective:** Guarantee the 8 KB battery SRAM is near-bit-exact to the real part and to openMSX
  `MSXFmPac.cc`, working through BOTH the game register-path (already working) AND — now enabled by
  Slice 2 — the `CALL FMPAC` manager path (write via manager → exit → reload → read back byte-identical).
- **Interface contract:**
  - Register behavior stays exactly as `MSXFmPac.cc` (magic 0x5FFE=0x4D/0x5FFF=0x69 readback while
    unlocked; enable 0x7FF6 incl. bit4 force-reset re-lock; bank 0x7FF7; window 0x4000-0x5FFD write gated
    by unlock; addressable size 0x1FFE). Verify — do not regress.
  - **Persistence format (RESOLVE — §3.4, §7):** align our `.sram` byte format to openMSX so the SRAM
    round-trip A/B is directly comparable. Recommended: model the addressable **8190** bytes and match
    openMSX's **16-byte `"PAC2 BACKUP DATA"` header + 8190 data** file layout (load validates the header;
    absent/short/invalid → deterministic blank, never fabricated). This makes files cross-emulator
    portable AND enables a direct file-SHA A/B. (If the human prefers our current raw-8192 self-consistent
    format, keep it and do the A/B on SRAM CONTENT only — see §7; needs a decision.)
  - `.sram` path plumbing already exists (`main.cpp --fmpac-sram`, `hbf1xv_machine set_fmpac_sram_path`/
    `flush_fmpac_sram`, SDL3 auto-derive `<cart>.rom.sram`) — reuse it; do not duplicate.
- **Acceptance:**
  - C1. Magic-unlock/enable/bank/window unit behavior verified bit-exact vs `MSXFmPac.cc` (existing tests
    pass + any hardening covered).
  - C2. SRAM CONTENT round-trip: write bytes via the register-path → `flush` → reload → read back
    byte-identical (8190 addressable bytes).
  - C3. SRAM via `CALL FMPAC` manager: drive the manager to write a backup → exit/persist → reload → read
    back byte-identical, AND byte-identical to openMSX's SRAM CONTENT after the identical manager
    keystrokes (the A/B).
  - C4. If the openMSX file format is adopted (recommended): our `.sram` file is byte-identical to
    openMSX's FM-PAC `.sram` for the same content (header + 8190), a direct file-SHA A/B.
- **Test obligations (deterministic, `tests/`):**
  - Unit: magic-unlock AND-gate; enable bit4 force-reset re-lock; bank fold; window write-gating; 8190
    addressability (0x1FFE/0x1FFF read back magic, not SRAM).
  - Unit: persistence round-trip (save→load byte-identical) in the resolved format; header validate/reject
    (absent/short/wrong-header → deterministic blank).
  - Integration: `--fmpac-sram <file>` write-then-reload round-trip through the real cartridge device.
- **Blast radius (EXPECTED):** `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}` (+ possibly
  `src/devices/memory/battery_backed_sram.{h,cpp}` OR an FM-PAC-local save/load for the header+8190
  format). `src/core`/`src/devices/cpu` NOT touched. **ZEXALL trigger:** none from Slice 3.

---

## 5. Acceptance Criteria (consolidated) + Evidence Gates

All must be captured/executed, never fabricated; each maps to a repository artifact or command output.

### 5.1 Functional (behavior)
- **AC-1** `CALL MUSIC` behaves as openMSX (`Ok`) — typed-BASIC A/B (reuse `tools/gen-typed-ab.py` +
  `debug/fmpac-call-ab/` + the M41 keymatrix-typing + frame-capture harness).
- **AC-2** `CALL FMPAC` launches the "PAC2 BACKUP DATA" manager as openMSX — same typed-BASIC A/B (FM-PAC
  inserted).
- **AC-3** The broader MSX-MUSIC CALL table (VOICE/TEMPO/TRANSPOSE/PITCH) dispatches (no "Syntax error").
- **AC-4** FM-PAC SRAM read/write/persist round-trip **bit-identical vs openMSX**, incl. via `CALL FMPAC`
  (SRAM CONTENT A/B; + file-SHA A/B if the openMSX format is adopted).

### 5.2 Determinism / regression
- **AC-5** New deterministic unit + integration tests for: slot-bus inter-slot access; `AB`-scan
  enumeration across slots/subslots; SRAM magic-unlock/enable/bank/persist. Non-tautology proven by an
  adversarial mutation on at least the enumeration test and the SRAM round-trip test.
- **AC-6** NO oracle weakened or rebaselined; every pre-existing memory/slot/boot/disk/mapper oracle
  stays green.

### 5.3 Evidence gates (run + capture at the QA gate; from-scratch clean rebuild)
- **G-1** `tools/validate-assets.ps1` — required BIOS present + ≥1 ROM (confirm canonical FM-PAC path).
- **G-2** `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — reproducible checksums.
- **G-3** `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON` + `cmake --build build --config Debug` — both
  executables build clean from scratch (single canonical `build/` per DEC-0041).
- **G-4** `ctest --test-dir build -C Debug --output-on-failure` — full deterministic suite green.
- **G-5 (MANDATORY per DEC-0062):** the **FULL ZEXALL/ZEXDOC sweep runs at QA** (this touches core
  slot-addressing; v1.1.0 is a production checkpoint) — durable log kept, 0 error markers, exit 0. This
  is required by DEC-0062 EVEN IF the mechanical per-edit trigger (src/devices/cpu or src/core) did not
  fire; if the fix DID touch src/core, it fires additionally.
- **G-6 (REQUIRED — behavior-affecting core change):** openMSX A/B evidence — the typed-BASIC CALL A/B
  (AC-1/2/3) AND the SRAM round-trip A/B (AC-4). Capture via the reused harness + `debug/fmpac-call-ab/`;
  record disposition in the QA sign-off (and `docs/openmsx-ab-*` as applicable). If the two behavior
  references disagree on any detail, record the disagreement (guardrails.md:26-27) — none expected here
  (both openMSX and the real FM-PAC firmware define these CALLs).

---

## 6. Regression Matrix

| Surface | Risk from M43 | Guard | Gate |
| --- | --- | --- | --- |
| Boot to BASIC `Ok` | HIGH (slot fabric) | boot oracle unchanged + real-BIOS integration boot | G-3/G-4 |
| Disk-BASIC boot (3-2) | HIGH (expanded subslot) | disk boot oracle + a `CALL SYSTEM` smoke | G-4/G-6 |
| RAM mapper 3-0 + `#FC-#FF` | MED | mapper/memory oracles unchanged | G-4 |
| `#A8` primary select + `#AC` mirror | HIGH | PPI/slot-select oracles unchanged; diff shows `#A8` surface untouched | G-4 |
| `#FFFF` steady-state subslot | HIGH | slot-bus unit oracles + adversarial mutation | G-4/AC-5 |
| External cartridge load/run (KonamiSCC, FM-PAC, ASCII, etc.) | MED | cartridge oracles + FM-PAC integration | G-4/AC-4 |
| Z80 CPU correctness | LOW (no cpu edit expected) | **full ZEXALL/ZEXDOC sweep** | G-5 |
| FM-PAC register/SRAM behavior | MED (Slice 3 hardening) | SRAM unit + round-trip + openMSX A/B | AC-4/G-6 |
| SRAM file compatibility | MED (format change) | round-trip + (if adopted) file-SHA A/B; migration note for existing raw-8192 `.sram` files | AC-4/§7 |

---

## 7. Risk Analysis (HIGH — the whole address fabric)

- **R1 (HIGH) — blast radius = the memory/slot fabric.** Every RAM/ROM/mapper/cart access flows through
  `SlotBus`. A subtly wrong inter-slot fix could corrupt boot, disk, or mapper detection. *Mitigation:*
  diagnostic-first (Slice 1 gate); strict-superset invariant proven by unchanged oracles; full ZEXALL +
  system-wide review before code ([[feedback_system_wide_review_first]] — this is the exact area the rule
  exists for).
- **R2 (MED) — hypothesis could be wrong.** §3 review found NO gross arithmetic bug, so the true cause may
  be subtler (transient state, or an INIT-not-run path). *Mitigation:* Slice 1 confirms with a both-sides
  trace excerpt BEFORE any edit; the H-A/H-B bisection is cheap and de-risks the direction.
- **R3 (MED) — SRAM file-format change breaks existing saves.** Adopting openMSX's header+8190 layout makes
  today's raw-8192 `.sram` files unreadable (header mismatch → deterministic blank = data loss for
  existing local saves). *Mitigation:* the Open Question below must be decided; if adopted, provide a
  one-time migration note/tool and document it; load path must fail SAFE (blank, never garbage).
- **R4 (LOW) — trace A/B tooling friction.** A full `--trace-cpu` can OOM (`main.cpp:778`); openMSX
  per-instruction trace is heavy. *Mitigation:* window-bounded traces + `--io-observe`/`MemWriteObserver`
  compact timelines first (Slice 1 method).
- **R5 (LOW) — license isolation.** The fix reads `MSXCPUInterface.cc`/`MSXFmPac.cc`/`SRAM.cc` for
  understanding. *Mitigation:* never copy; express the fix in our own terms; grep the diff for copied
  structure; no license-sensitive data tables involved.

### Open Questions (need a decision or confirmation)
- **OQ-1 (Slice 3 format):** DEC-0062 says "raw 8 KB" AND "byte-format matching openMSX" — these CONFLICT
  (openMSX = 16-byte `"PAC2 BACKUP DATA"` header + 8190 data, NOT raw 8 KB). *Recommendation:* adopt the
  openMSX format (true cross-emulator portability + direct file-SHA A/B + real-part fidelity, since the
  addressable SRAM is genuinely 8190). *Verification/decision action:* confirm with the human or record a
  decision entry; note the R3 migration impact for existing raw-8192 `.sram` files. Until resolved, the
  SRAM A/B (AC-4) runs on SRAM CONTENT (8190 addressable bytes), which is format-independent.
- **OQ-2 (Slice 1 outcome may widen scope):** if Slice 1 proves the fix needs a `src/core/bus.h` contract
  change (not expected), that is a scope escalation — record it and re-confirm the blast radius before
  Slice 2 (the mechanical ZEXALL trigger then fires too).

---

## 8. Developer Handoff (sequence)

**Gate order is HARD: 1 → 2 → 3, with a diagnosis gate between 1 and 2.**

1. **Slice 1 (DIAGNOSTIC, no code).** Run the H-A/H-B bisection (`CALL SYSTEM` vs `CALL MUSIC` vs
   `CALL FMPAC`) via `tools/gen-typed-ab.py` + `debug/fmpac-call-ab/`; capture the compact `#A8`/`#FFFF`
   timeline (`--io-observe` + `MemWriteObserver`); then a window-bounded `--trace-cpu` A/B vs openMSX to
   pinpoint the first diverging inter-slot access. **Deliverable:** written diagnosis naming the primitive
   + both-sides trace excerpt + the Slice 2 file/blast-radius map + the ZEXALL-trigger determination.
   **Do not proceed to Slice 2 until this is done and the hypothesis is confirmed or revised.**
2. **Slice 2 (UNIVERSAL fix).** Implement the minimal, device-agnostic `SlotBus` correction from Slice 1.
   Add the unit (inter-slot + `AB`-enumeration) and integration (real-BIOS `CALL MUSIC`) tests; prove
   non-tautology by adversarial mutation. Keep every existing oracle byte-identical (strict superset).
   Confirm no `#A8`-surface or static-map change (grep the diff for game/name literals — must be none).
3. **Slice 3 (SRAM bit-exact).** Verify the register behavior against `MSXFmPac.cc`; resolve OQ-1 and
   implement the persistence format; add the SRAM unit + round-trip tests; run the `CALL FMPAC` manager
   round-trip. Reuse the existing `.sram` plumbing.
4. **Evidence gates (QA).** From a from-scratch clean `build/`: G-1..G-4, the MANDATORY G-5 full
   ZEXALL/ZEXDOC sweep, and G-6 openMSX A/B (typed-BASIC CALL + SRAM round-trip). No oracle weakened.
   Then QA sign-off → coordinator release decision → owner-run tag `v1.1.0` + push (DEC-0047).

**References for the developer (read for understanding ONLY, never copy):**
`references/openmsx-21.0/src/cpu/MSXCPUInterface.cc` (secondary-slot/`#FFFF` semantics),
`references/openmsx-21.0/src/sound/MSXFmPac.cc` + `src/memory/SRAM.cc` (FM-PAC SRAM + file format),
`references/fmsx-60/source/EMULib/` (independent PPI/slot cross-reference if the two disagree).
Our surfaces: `src/devices/chipset/slot_bus.{h,cpp}`, `src/machine/hbf1xv_machine.cpp:104-164`,
`src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}`, `src/devices/memory/battery_backed_sram.{h,cpp}`,
`src/main.cpp` (`--debug-session`/`--trace-cpu`/`--io-observe`/`--fmpac-sram`).
