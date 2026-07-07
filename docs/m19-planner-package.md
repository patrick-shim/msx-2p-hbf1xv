# M19 Planner Package — Cartridge Loading: External Primary Slots 1 & 2

- Milestone ID: M19
- Title: Cartridge ROM Loading — External Primary Slots 1 & 2 (MVP Mapper-Type Family)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M19-001 (planner-first, no production code)
- Decisions in force: DEC-0005 (backlog disposition discipline — every planner package must consult
  `agent-protocol/state/deferred-backlog.md` and re-affirm every row), DEC-0009 (indicative
  milestone order: "M19 cartridges (B7)"), DEC-0012 (precedent for how to handle a backlog-wording
  vs. XML-grounding mismatch — re-applied below in §2.8; this cycle's finding is judged
  self-resolved/non-blocking, unlike DEC-0012's own finding, for reasons given there).
- Backlog effect: **closes B7** (cartridge loading, external primary slots 1 & 2), upon successful
  developer + QA completion. All other rows re-affirmed (§4). Four new depth-deferral rows proposed:
  **G1** (KonamiSCC + embedded SCC chip), **G2** (ROM-database/heuristic mapper-type
  auto-detection), **G3** (full `CartridgeSlotManager`-style runtime hot-plug), **G4** (the long
  tail of ~80 other specialized/vendor mapper types), mirroring the M14 D1-D7 / M17 E1-E2 / M18
  F1-F2 contract-vs-depth precedent.
- Gate: **NORMAL human-release-decision gate (no auto-close)** — explicit per the task brief and
  consistent with DEC-0003's auto-close grant being M12-only. Autopilot pauses at QA sign-off for
  the separate human release decision + tag, matching the M13-M18 pattern.

> Grounding rule: every behaviour claim below cites a concrete `references/...` path (with line
> numbers where useful) or a current-repo `src/...`/`docs/...` path. openMSX sources are the
> BEHAVIOUR reference only and are GPL — **never copy openMSX code into `src/`**
> (`CLAUDE.md`, `agent-protocol/guardrails.md`).

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) A reusable 8-slot × 8 KB cartridge-window primitive** (`CartridgeRomWindow`), grounded in
  openMSX's `RomBlocks<BANK_SIZE>` bank-resolution algorithm (`references/openmsx-21.0/src/memory/
  RomBlocks.hh/.cc`), shared by every MVP mapper type below.
- **(b) Six MVP cartridge mapper-type devices**, each a `core::MemoryDevice`, byte-exact grounded in
  a real openMSX class already surveyed (§1.2 lists the deferred remainder):
  `Mirrored` (`RomPlain`/MIRRORED), `Generic8kB` (`RomGeneric8kB`), `Generic16kB`
  (`RomGeneric16kB`), `Ascii8kB` (`RomAscii8kB`), `Ascii16kB` (`RomAscii16kB`), `Konami`
  (`RomKonami`, no SCC).
- **(c) A `CartridgeSlot` wrapper device** — the ONE `core::MemoryDevice` actually attached to
  `slot_bus_` at construction (mirrors every prior milestone's "one device per slot, wired once in
  `wire_bus()`" discipline), internally holding `std::unique_ptr<CartridgeMapperDevice>` (nullptr =
  empty slot = byte-for-byte identical to the M13-M18 "reserved open-bus" default — a strong,
  built-in regression guard for anyone not using `--cart1`/`--cart2`).
- **(d) CLI/API cartridge loading**: `--cart1 <path> [--cart1-type <name>]` /
  `--cart2 <path> [--cart2-type <name>]` in `src/main.cpp`, backed by a small, pure, unit-testable
  argv parser (`src/machine/cartridge_cli.{h,cpp}`) and a new `Hbf1xvMachine::load_cartridge(...)`
  API (usable directly by tests without going through argv).
- **(e) Deterministic unit + integration tests**, zero regression M1-M18.
- **(f) Real openMSX A/B evidence** (§2.7) for the mapper write/read-back path — no parity claim
  without a genuine capture.
- **(g) Full deferred-backlog review** — every row A.B1-B9, B.C1-C10, C.D1-D7, D.E1-E2, E.F1-F2
  re-affirmed with a one-line justification (§4), per the human's explicit instruction.

### 1.2 Out of scope (named explicitly, each with justification)

| Deferred item | Why OUT of M19 | Owning milestone (proposed) |
|---|---|---|
| **KonamiSCC mapper + embedded SCC/SCC+ sound chip** (`references/openmsx-21.0/src/memory/RomKonamiSCC.hh`, which additionally owns an `SCC scc;` member — a real 5-channel wavetable synthesizer, not just a memory mapper) | Non-trivial: this is a NEW AUDIO DEVICE (mirrors the M14/M17 "contract vs. depth" reasoning — a sound chip is real, separate scope), not an incremental mapper variant. The task brief explicitly invited scoping this out "unless trivial"; it is not trivial. | Proposed new backlog row **G1** — future audio milestone, paired with C9 (SDL3 frontend) / E1 (YM2413 DSP depth), since it is the same class of work. |
| **ROM-database / SHA1 auto-mappertype-detection** (`references/openmsx-21.0/src/memory/{RomInfo,RomDatabase}.*`, `share/softwaredb.xml`) **and heuristic byte-pattern auto-detection** (`RomFactory.cc:82-169` `guessRomType`, scanning for `LD (nn),A` bank-switch instruction addresses) | Explicitly invited to be scoped out by the task brief. Both mechanisms exist in openMSX to let a user drop in an unlabeled ROM file and have the mapper type inferred; M19 instead requires an **explicit** `--cartN-type` (mirrors openMSX's own **non-auto** default path — `RomFactory.cc:176-210` — which the emulator already supports as a first-class path, not a lesser one). Building a curated software database or an instruction-scanning heuristic is real, separate scope with real false-positive risk. | Proposed new backlog row **G2** — future milestone if/when auto-identification becomes a real user need. |
| **Full `CartridgeSlotManager`-style dynamic runtime hot-plug** (`references/openmsx-21.0/src/CartridgeSlotManager.hh/.cc`: Tcl `cart`/`ext` commands, `allocateAnyPrimarySlot`, insert/eject while the machine is running) | This repo has a fixed, always-populated, construction-time device composition for every device built M11-M18 (`wire_bus()` attaches every device exactly once; nothing is ever re-attached at runtime). Cartridge loading in M19 follows the SAME discipline: a cartridge is loaded **before** (or as part of) `cold_boot()`/first run, not swapped live via a runtime command console (this emulator has no Tcl-console/command-dispatch layer at all — that is unique, dynamic-VM-control infrastructure `CartridgeSlotManager` is built for, which this project does not have and is not asked to build here). | Proposed new backlog row **G3** — future milestone only if live insert/eject during a running session becomes a real requirement (e.g., alongside a future interactive/SDL3 session, C9). |
| **The long tail of ~80 other specialized/vendor-specific mapper types** in `references/openmsx-21.0/src/memory/RomTypes.hh` (Panasonic-internal, National-internal, NEO-8/16, Majutsushi, Synthesizer, PlayBall, Zemina 25/80/90/126-in-1, Holy Qu'ran (1/2), FSA1FM (1/2), the Manbow2/MegaFlashROMSCC(+)/RBSC/HamarajaNight flash-cart family, ReproCartridge (V1/V2), KonamiUltimateCollection, MSXDOS2, R-Type, CrossBlaim, HarryFox, GameMaster2, the ASCII8/16-with-SRAM variants (ASCII8_2/8/32, ASCII16_2/8), Koei (8/32), Wizardry, MSXWrite, MultiRom, RAMFILE, ColecoMegaCart, WonderKid, Dooly, MSXtra, Yamanooto, AlAlamiah30in1, RetroHard31in1, ROMHunterMk2, ASCII16X — **excluding** `Halnote` (already backlog **B6**, the INTERNAL slot-0-3 MSX-JE firmware mapper, structurally unrelated to the external cartridge slots this milestone covers) and `DRAM` (MSXturboR-specific, not this machine)) | openMSX ships ~90 mapper-type classes total; the task brief itself frames M19's job as surveying the family and choosing a defensible MVP subset, not exhaustively porting all of them. The 6 chosen types (§2.2) cover the overwhelming majority of real, commercially significant MSX cartridges (plain ROMs, the two "generic" bank-switched forms, ASCII's two common granularities, and Konami's — the single most common real-world MegaROM scheme). | Future milestone(s), only if/when a specific real cartridge requiring one of these is actually wanted; each would be a small, additive, well-precedented slice following the exact pattern this milestone establishes. |

### 1.3 Assumptions (each with a verification action)

- **A-M19-1 (slot identity confirmed directly from the XML — re-verified, not taken on faith).**
  `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:119,121`:
  `<primary external="true" slot="1"/>` and `<primary external="true" slot="2"/>` — each a bare,
  **childless** `<primary>` element (no `<secondary>` children), unlike primary slots 0 and 3 (XML
  lines 85-117, 123-199), which each declare four explicit `<secondary slot="0..3">` children. *This
  confirms the task brief's framing exactly*: slots 1 and 2 are single, unexpanded external cartridge
  bays — **no sub-slot register / `SlotBus::set_expanded` logic is needed for them**, unlike slots
  0/3. *Verify:* `src/devices/chipset/slot_bus.cpp:51-56` (`sub_for_page`) already returns `0`
  unconditionally for any primary slot whose `expanded_[primary]` flag is `false` — i.e., attaching a
  device at `(primary, sub=0, page)` for slots 1/2 is sufficient and correct with **zero** change to
  `SlotBus` itself; `wire_bus()` simply never calls `set_expanded(1, true)` / `set_expanded(2, true)`
  (matches the existing pattern where only slots 0 and 3 call `set_expanded`,
  `hbf1xv_machine.cpp:39-40`).
- **A-M19-2 (`CartridgeSlotManager` concept read for understanding; NOT ported — see §1.2 / G3).**
  `references/openmsx-21.0/src/CartridgeSlotManager.hh` models dynamic slot allocation
  (`allocateAnyPrimarySlot`, `getSpecificSlot`, a Tcl `CartCmd`/`ExtCmd` command pair, a
  `MediaProvider` for live media-info queries) for openMSX's general "any MSX machine, any slot
  layout, runtime-configurable" design. This machine's slot layout is FIXED (2 external primary
  slots, XML-declared, never renumbered) and this project has no runtime command console anywhere.
  *Verify:* grep of `src/` for any existing "attach at runtime after construction" pattern turns up
  none — every device in `Hbf1xvMachine` is a fixed member, wired once in `wire_bus()`
  (`hbf1xv_machine.cpp:32-169`); M19's `CartridgeSlot` follows this exactly (§2.1).
- **A-M19-3 (mapper-type dispatch pattern grounded in `RomFactory`; MVP subset only).**
  `references/openmsx-21.0/src/memory/RomFactory.cc:171-383` is a `switch (type)` over ~90
  `RomType` enum values (`references/openmsx-21.0/src/memory/RomTypes.hh:8-100`), each constructing a
  concrete `Rom*` class. M19's `CartridgeMapperType` enum + a small `CartridgeSlot::load(type, ...)`
  dispatch is the SAME architectural pattern, scoped to 6 values (§2.2) instead of ~90. *Verify:*
  `src/devices/cartridge/cartridge_mapper_type.{h,cpp}` names the 6 values using openMSX's OWN
  canonical string names — `references/openmsx-21.0/src/memory/RomInfo.cc:19-20,23,26-27,92`:
  `"8kB"` (GENERIC_8KB), `"16kB"` (GENERIC_16KB), `"Konami"` (KONAMI), `"ASCII8"` (ASCII8),
  `"ASCII16"` (ASCII16), `"Mirrored"` (MIRRORED) — so a `--cart1-type` value is the exact string an
  openMSX user/config would already recognize, not an invented vocabulary.
- **A-M19-4 (CLI shape grounded in `MSXRomCLI`; explicit type, no adjacency requirement).**
  `references/openmsx-21.0/src/memory/MSXRomCLI.cc:22-31,65-84`: openMSX registers
  `-cart`/`-carta`/`-cartb`/`-cartc`/`-cartd` (slot-lettered) plus a **sequential** `-romtype <type>`
  option that "should immediately follow" the ROM option (`MSXRomCLI.cc:99-103`, enforced by
  `RomTypeOption::parseOption` throwing a `FatalError` if reached standalone — i.e., a general-option
  parser, not order-independent flags). This project's existing `src/main.cpp` CLI style
  (`--parity-trace <bin> <base> <steps> <out>`, `--vdp-parity ...`, `--ym2413-parity ...`) is
  positional-argument-per-mode, not a general option parser. M19 adopts a **simplified, explicit,
  two-slot-only** shape — `--cart1 <path> [--cart1-type <name>]` / `--cart2 <path>
  [--cart2-type <name>]`, parsed order-independently by a small dedicated function (§2.4) — rather
  than porting openMSX's general sequential-option machinery. *Verify:* `src/machine/
  cartridge_cli.{h,cpp}` is a PURE function (`argv strings in -> parsed spec out`, no file I/O, no
  `Hbf1xvMachine` dependency) so it is directly unit-testable without argv/process plumbing (mirrors
  `RomAssetLoader`'s own separation of "parse/spec" from "load," `src/machine/rom_asset_loader.h:26`).
- **A-M19-5 (mapper-type default matches openMSX's own default).**
  `references/openmsx-21.0/src/memory/RomFactory.cc:178-179`: `"if no type is mentioned, we assume
  'mirrored' which works for most plain ROMs"` (`config.getChildData("mappertype", "Mirrored")`).
  When `--cart1` is given without `--cart1-type`, M19 defaults to `Mirrored` — the SAME default
  openMSX itself uses, not an invented one. *Verify:* `cartridge_cli` unit test asserts
  `--cart1 <path>` alone yields `type == CartridgeMapperType::Mirrored`.
- **A-M19-6 (bank-resolution algorithm is byte-exact, cited).**
  `references/openmsx-21.0/src/memory/RomBlocks.cc:107-118` (`RomBlocks<BANK_SIZE>::setRom`):
  ```
  block = (block < nrBlocks) ? block : block & blockMask;
  if (block < nrBlocks) { /* real bank */ } else { /* unmapped, 0xFF */ }
  ```
  where `nrBlocks = image.size() / BANK_SIZE` (computed from the ACTUAL loaded image) and the
  DEFAULT `blockMask = nrBlocks - 1` (`RomBlocks.cc:47`, "wraps at end of ROM image, meaning the
  entire ROM is reachable"). `RomKonami`'s constructor OVERRIDES this default via
  `setBlockMask(31)` (`RomKonami.cc:24`, "Konami mapper is 256kB in size, even if ROM is smaller") —
  every other MVP type keeps the default. **Subtlety honestly captured (not glossed over):** because
  the mask is only consulted when `block >= nrBlocks`, a Konami-typed image LARGER than 256 KB (e.g.
  a 2 MB file) is NOT artificially capped to 32 reachable banks — every byte-value bank request
  `0..255` against a 2 MB (256-bank) image satisfies `block < nrBlocks(256)` directly and is used
  UNMASKED; `blockMask` only ever engages as a wrap-around fallback for genuinely out-of-range
  requests. This is the actual, literal openMSX behavior (not a plausible-sounding alternative), and
  M19 reproduces it byte-for-byte. *Verify:* `CartridgeRomWindow` unit tests assert both the
  in-range-direct case and the out-of-range-wrap-then-still-out-of-range-unmapped case, for both a
  default mask and an overridden (Konami-style) mask.
- **A-M19-7 (no silent short/long-image padding — a deliberate, disclosed, STRICTER divergence from
  openMSX).** `RomBlocks.cc:28-39` pads a short image up to the next `BANK_SIZE` multiple WITH a
  printed warning ("the dump is probably not complete/correct!") rather than rejecting it.
  `RomPlain.cc:39-43` additionally throws for any image `> 0x10000` or not a multiple of `0x2000`.
  M19 adopts the STRICTER, more honest posture project-wide (never fabricate/pad content, per
  `CLAUDE.md`/guardrails "never fabricate"): every MVP mapper type REJECTS (returns a
  `CartridgeLoadResult` error, loads nothing, slot stays in its prior state) an image whose size is
  not an exact positive multiple of its bank size — `0x2000` (8 KB) for `Mirrored`/`Generic8kB`/
  `Ascii8kB`/`Konami`, `0x4000` (16 KB) for `Generic16kB`/`Ascii16kB` — with `Mirrored` additionally
  capped at a total of `0x10000` (64 KB, matching `RomPlain.cc:39-43` exactly, since Mirrored's whole
  window IS the 64 KB external slot with no narrower `<mem>` sub-window to place inside). *Verify:*
  unit tests assert a non-multiple-sized and (for Mirrored) an oversized image are both rejected with
  a clear diagnostic, never silently padded/truncated/fabricated.
- **A-M19-8 (`RomPlain`'s `guessLocation` heuristic is not needed and is deliberately NOT ported).**
  `RomPlain.cc:116-159` (`guessLocation`) exists because openMSX's general `<mem base=.. size=..>`
  mechanism lets a ROM be placed inside an ARBITRARY sub-window of a larger machine's address space,
  so a MIRRORED/NORMAL-typed ROM's exact starting address within that window must be guessed (by
  scanning likely BASIC routine-pointer bytes) when not explicit. This machine's external cartridge
  slots have **no such sub-window** — each `CartridgeSlot` occupies the ENTIRE 64 KB unexpanded
  primary-slot address space (XML confirms no `<mem>` narrowing for external slots, since they are
  bare `<primary external="true" slot="N"/>` elements with no children at all, A-M19-1). Consequently
  `Mirrored` in M19 always places bank 0 at CPU address `0x0000` and mirrors every subsequent
  `0x2000` region at `block % nrBlocks`, with no placement ambiguity to resolve. *Verify:*
  `CartridgeMirroredRom` unit test asserts a 16 KB image mirrors 4x across the 64 KB window
  (`address >> 13` selects `block % 2`... concretely: 8 blocks of 8 KB, `nrBlocks=2`, so blocks
  `{0,2,4,6}` read bank 0 and `{1,3,5,7}` read bank 1 — i.e. the image repeats every `0x4000`).
- **A-M19-9 (reset() reinitializes bank state, never unloads the cartridge — matches real hardware
  power-cycle-with-cartridge-still-inserted semantics).** Every surveyed mapper's own `reset()`
  (`RomGeneric8kB.cc:13-22`, `RomAscii8kB.cc:24-33`, `RomAscii16kB.cc:22-28`, `RomKonami.cc:54-59`)
  reinitializes ONLY the bank-pointer array, never touches the immutable `rom` image itself — because
  a real MSX's RESET line does not physically disconnect an inserted cartridge. `CartridgeSlot::reset()`
  therefore calls the active mapper's `reset()` (if one is loaded) and is a no-op when the slot is
  empty; it never calls `unload()`. *Verify:* integration test loads a cartridge, bank-switches away
  from the reset default, calls `machine.cold_boot()` again, and asserts the bank state reverts to
  the mapper's documented reset default while `mapper_type()`/the loaded image content are unchanged.
- **A-M19-10 (`roms/aleste.rom`'s real-world identity is genuinely NOT confirmable within this
  planning cycle — disclosed honestly, not fabricated; see §2.6 for the resulting fixture strategy).**
  Direct inspection (Read tool, `roms/aleste.rom`, first ~2 lines) shows the file begins with the
  bytes `'A'`,`'B'` (the MSX cartridge-ROM header signature every surveyed mapper type's real-world
  examples share) and contains embedded ASCII text `"ALESTE20"` and `"BOOT ERROR"` further into the
  image. `docs/asset-checksums.txt:21` records it as exactly `2,097,152` bytes (2 MB), SHA256
  `4ea1e183467abe094df88901fc5dc70f0df5c1fc424de19f212692001bd5ad43`. Cross-referencing
  `references/openmsx-21.0/share/softwaredb.xml` (searched, not guessed): the title **"Aleste"**
  (Compile, 1988, MSX2, `genmsxid` 1055) has real dumps of type `ASCII16` or `Konami`
  (`softwaredb.xml:319-335`); **"Aleste 2"** (Compile, 1989, MSX2, `genmsxid` 1246) has real dumps of
  type `KonamiSCC` or `ASCII8` (`softwaredb.xml:14846-14849`); `RomKonami.cc:5` independently cites
  "Aleste 1" as a real Konami-mapper example title. **None of softwaredb's listed hashes are SHA1
  (ours is only recorded as SHA256), so a byte-identity cross-check against a specific cataloged dump
  is not possible with the artifacts on hand.** More importantly, **the file's 2 MB size is
  incompatible with treating it as a straight, real Konami-mapper OR Mirrored/Plain dump**: `Mirrored`
  is hard-capped at 64 KB (`RomPlain.cc:39-43`, A-M19-7), and `RomKonami.cc:27-31` itself warns that
  real Konami-mapper silicon does not exceed 256 KB. A 2 MB size is, however, architecturally
  consistent with `ASCII8`/`ASCII16`/`Generic8kB` (whose bank registers are a full byte, addressing up
  to 256 x 8 KB = 2 MB, or 256 x 16 KB = 4 MB) — but so is a completely unrelated, possibly homebrew
  multi-game compilation image (the embedded `"BOOT ERROR"` string is characteristic of a disk
  bootstrap, not typical cartridge-ROM content, raising a real possibility this file is a repurposed
  or hybrid image, not a straight retail cartridge dump). **Disposition (non-fabricating): this
  planning package does NOT assert `roms/aleste.rom`'s mapper-type identity.** It is used ONLY as a
  large, real, on-disk-file "mechanical pipeline" smoke fixture (§2.6), loaded under the explicitly
  disclosed, non-hardware-claiming `Generic8kB` type (openMSX's own label for this type is literally
  *"Generic ROM types that don't exist in real ROMs"* — `RomInfo.cc:17-19` — the single most honest
  choice for a file whose real identity is unconfirmed). ALL byte-exact protocol/behavior assertions
  in this milestone's unit/integration tests use SYNTHETIC, hand-authored deterministic images
  (mirrors the M16 precedent of a synthesized disk image over an ambiguous real file). *Verify:* the
  smoke-fixture test asserts only mechanical facts directly confirmed during grounding (file loads,
  SHA256 matches, `machine.debug_bus_read(0x4000) == 0x41` ('A') and `debug_bus_read(0x4001) == 0x42`
  ('B') after routing primary slot 1 into CPU page 1 — concrete, non-fabricated, and independently
  re-checkable by the developer/QA against the same file).

---

## 2. Spec Summary

### 2.1 `src/` placement (per `src/CLAUDE.md`)

**New top-level device family: `src/devices/cartridge/`** (sibling to `src/devices/{audio,chipset,
cpu,fdc,kanji,memory,rtc,video}/`) — mirrors the M18 precedent of giving a sufficiently large new
device family (6 mapper classes + a shared window primitive + a slot wrapper = 10 files) its own
top-level folder rather than overloading the existing, currently-3-file `src/devices/memory/`
(`rom_device.*`, `memory_mapper_ram.*`, `battery_backed_sram.*`). A cartridge mapper chip is
unambiguously "chip and controller implementations" per `src/CLAUDE.md`'s `src/devices/` charter,
just as `kanji`/`fdc` are.

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/cartridge/cartridge_mapper_type.{h,cpp}` | `enum class CartridgeMapperType { Mirrored, Generic8kB, Generic16kB, Ascii8kB, Ascii16kB, Konami };` + `parse_cartridge_mapper_type(string_view) -> optional<CartridgeMapperType>` + `to_string(CartridgeMapperType) -> string_view`, using openMSX's OWN canonical name strings (A-M19-3). | `RomTypes.hh`, `RomInfo.cc:19-20,23,26-27,92` |
| `src/devices/cartridge/cartridge_rom_window.{h,cpp}` | Shared 8-slot x 8 KB window primitive (`CartridgeRomWindow`): owns the raw image bytes + 8 slot descriptors (unmapped, or a resolved byte offset into the image); `set_bank(slot, requested_block)` implements the exact `RomBlocks::setRom` algorithm (A-M19-6); `set_unmapped(slot)`; `set_block_mask(mask)` (default = `nrBlocks-1`, overridable); `read(address) -> uint8_t`. All 6 MVP mapper types compose this (16 KB-granularity types drive TWO adjacent 8 KB slots per logical 16 KB bank — §2.2). | `RomBlocks.hh/.cc` (A-M19-6) |
| `src/devices/cartridge/cartridge_mapper_device.h` | Small interface `class CartridgeMapperDevice : public core::MemoryDevice { virtual void reset() = 0; virtual CartridgeMapperType mapper_type() const = 0; };` — a device-FAMILY-local interface (like the existing `RtcClockSource`/`FdcClockSource`/`CassetteClockSource` X-pattern interfaces, each defined inside their own device family, not in `src/core/`), letting `CartridgeSlot` hold one polymorphic pointer to "whichever concrete mapper is currently plugged in." Distinct from `core::device_contracts.h`'s bus-participation contracts (those are machine-bus-wide, this is family-internal). | `src/CLAUDE.md` ("Do not place device logic in `src/core/`"); existing X-pattern precedent (`hbf1xv_machine.h:279-314`) |
| `src/devices/cartridge/cartridge_mirrored_rom.{h,cpp}` | `Mirrored` type (§2.2). | `RomPlain.cc` (MIRRORED case only, A-M19-8) |
| `src/devices/cartridge/cartridge_generic8kb_rom.{h,cpp}` | `Generic8kB` type. | `RomGeneric8kB.cc` |
| `src/devices/cartridge/cartridge_generic16kb_rom.{h,cpp}` | `Generic16kB` type. | `RomGeneric16kB.cc` |
| `src/devices/cartridge/cartridge_ascii8kb_rom.{h,cpp}` | `Ascii8kB` type. | `RomAscii8kB.cc` |
| `src/devices/cartridge/cartridge_ascii16kb_rom.{h,cpp}` | `Ascii16kB` type. | `RomAscii16kB.cc` |
| `src/devices/cartridge/cartridge_konami_rom.{h,cpp}` | `Konami` type (no SCC). | `RomKonami.cc` |
| `src/devices/cartridge/cartridge_slot.{h,cpp}` | `CartridgeSlot : public core::MemoryDevice` — the ONE device actually attached to `slot_bus_`; owns `std::unique_ptr<CartridgeMapperDevice>` (nullptr = empty = open-bus, A-M19-9's regression-safe default); `CartridgeLoadResult load(CartridgeMapperType, std::vector<uint8_t> image)`; `void unload()`; `void reset()`; `bool loaded() const`; `CartridgeMapperType mapper_type() const` (precondition: loaded); `mem_read`/`mem_write` forward to the active mapper or return open-bus `0xFF`/no-op. | Mirrors the M13 `RomAssetLoader` "loader is a pure consumer, device stays filesystem-ignorant" split, and the openMSX `RomFactory::create` dispatch pattern (A-M19-3), scoped down. |

Machine wiring (in `src/machine/`, existing `hbf1xv_machine.{h,cpp}` files, additive only):

- Two new members: `devices::cartridge::CartridgeSlot cartridge_slot1_{1};` `cartridge_slot2_{2};`
  (constructed with their own primary-slot number for diagnostics only — routing itself is entirely
  `slot_bus_`'s job, per A-M19-1).
- In `wire_bus()`: `for (int page = 0; page < 4; ++page) { slot_bus_.attach(1, 0, page,
  &cartridge_slot1_); slot_bus_.attach(2, 0, page, &cartridge_slot2_); }` — **no**
  `set_expanded(1, ...)`/`set_expanded(2, ...)` call (A-M19-1: these slots are never expanded).
- In `cold_boot()`: `cartridge_slot1_.reset(); cartridge_slot2_.reset();` (A-M19-9: reinitializes
  bank state of whatever is loaded; no-op when empty; never unloads).
- New API: `CartridgeLoadResult load_cartridge(int slot_number, devices::cartridge::
  CartridgeMapperType type, std::vector<uint8_t> image);` `void unload_cartridge(int slot_number);`
  and accessors `cartridge_slot1()`/`cartridge_slot2()` (const + non-const), mirroring the existing
  `kanji()`/`printer()`/`cassette()` per-device-accessor convention (`hbf1xv_machine.h:173-183`).
- **New, small, CLI-adjacent file** (mirrors `src/machine/rom_asset_loader.h`'s placement
  precedent — filesystem/config-parsing glue lives in `src/machine/`, not `src/devices/`):
  `src/machine/cartridge_cli.{h,cpp}` — a PURE function `ParsedCartridgeCli parse_cartridge_cli(const
  std::vector<std::string>& args)` (A-M19-4): recognizes `--cart1 <path>`, `--cart1-type <name>`,
  `--cart2 <path>`, `--cart2-type <name>` anywhere in `args` (order-independent), returns per-slot
  `{optional<string> path; CartridgeMapperType type = Mirrored; bool type_was_explicit;}` plus a
  `vector<string> errors` (e.g., an unrecognized `--cart1-type` value) — never silently defaults an
  UNRECOGNIZED type string (only an OMITTED `-type` flag defaults, per A-M19-5). Does **no** file
  I/O and has **no** `Hbf1xvMachine` dependency, so it is directly unit-testable.
- `src/main.cpp` (existing file, additive edit): at startup, call `parse_cartridge_cli(args)`; for
  each slot with `path` set and no parse errors, `std::ifstream`-read the file (mirrors the EXISTING
  `run_parity_trace`/`run_vdp_parity` ifstream-loading pattern already in this file) and call
  `machine.load_cartridge(slot, type, bytes)`. On file-open failure OR a `CartridgeLoadResult` error,
  print a loud, specific diagnostic to `stderr` and return a **non-zero exit code** — this is
  wired into BOTH the default/normal run path (final `else` branch) AND the existing `--parity-trace`
  mode (§2.7, for the A/B harness), sharing the SAME parser/loading code (no duplication).

CMakeLists.txt: add the 9 new `.cpp` files under `src/devices/cartridge/` plus
`src/machine/cartridge_cli.cpp` to `add_library(sony_msx_core ...)`.

Boundary compliance: every cartridge mapper class carries zero slot-routing knowledge of its own
(each just answers reads/writes against its OWN internal bank state); `CartridgeSlot` carries zero
slot-BUS-attachment knowledge (it doesn't know it's slot 1 vs. 2 beyond a diagnostic label); the
MACHINE performs all `slot_bus_.attach(...)` composition — matching the M13-M18 precedent exactly.

### 2.2 The six MVP mapper types — byte-exact operational semantics

All six share one `CartridgeRomWindow` (8 slots of 8 KB spanning the full, unexpanded 64 KB external
primary-slot window, A-M19-1/A-M19-8); the 16 KB-granularity types (`Generic16kB`, `Ascii16kB`) drive
TWO adjacent 8 KB window-slots per logical 16 KB bank (`window-slot 2n` = low half, `2n+1` = high
half of logical bank `n`).

**`Mirrored`** (grounds `RomPlain.cc`, MIRRORED case; A-M19-7/A-M19-8):
- Load-time only: image size must be a positive multiple of `0x2000`, `<= 0x10000`, else rejected.
- Placement: bank 0 at `0x0000`; window-slot `s` (0-7) reads image bank `s mod nrBlocks` — a full,
  unconditional mirror across the whole 64 KB window (no placement ambiguity, A-M19-8).
- No bank-switch registers: `mem_write` is a no-op (read-only cartridge).

**`Generic8kB`** (grounds `RomGeneric8kB.cc:7-36`):
- `reset()`: window-slots 0,1 unmapped; slots 2,3,4,5 = banks 0,1,2,3; slots 6,7 unmapped.
- `mem_write(addr, val)`: `slot = addr >> 13` (0-7, i.e. writable at ANY address 0x0000-0xFFFF);
  `window.set_bank(slot, val)` (full byte value = requested bank index, A-M19-6 resolution).

**`Generic16kB`** (grounds `RomGeneric16kB.cc:6-24`), using 4 logical 16 KB banks (window-slot pairs
`{0,1}`,`{2,3}`,`{4,5}`,`{6,7}`):
- `reset()`: logical bank 0 unmapped; bank 1 = image bank 0; bank 2 = image bank 1; bank 3 unmapped.
- `mem_write(addr, val)`: `bank = addr >> 14` (0-3); sets window-slots `2*bank` and `2*bank+1` to
  image banks `2*val` and `2*val+1` respectively (full byte = requested 16 KB bank index).

**`Ascii8kB`** (grounds `RomAscii8kB.cc`, header comment lines 1-10 + code lines 18-52):
- `reset()`: window-slots 0,1 unmapped; slots 2,3,4,5 ALL = image bank 0; slots 6,7 unmapped.
- `mem_write(addr, val)`: only when `0x6000 <= addr < 0x8000`; `region = ((addr >> 11) & 3) + 2`
  (`0x6000-67FF`->2, `0x6800-6FFF`->3, `0x7000-77FF`->4, `0x7800-7FFF`->5); `window.set_bank(region,
  val)`.

**`Ascii16kB`** (grounds `RomAscii16kB.cc`, header comment + code lines 16-45), logical 16 KB banks:
- `reset()`: bank 0 unmapped; bank 1 = image bank 0; bank 2 = image bank 0 (**both start at the SAME
  bank, not 0/1** — grounded precisely, not assumed); bank 3 unmapped.
- `mem_write(addr, val)`: only when `0x6000 <= addr < 0x7800` AND `(addr & 0x0800) == 0` (excludes
  `0x6800-6FFF` and `0x7800-7FFF`); `region = ((addr >> 12) & 1) + 1` (`0x6xxx`->1, `0x7xxx`->2);
  sets that logical bank's TWO window-slots to `2*val`/`2*val+1`.

**`Konami`** (no SCC; grounds `RomKonami.cc`):
- Construction: `set_block_mask(31)` (overrides the default, A-M19-6) — matches
  `setBlockMask(31)`, `RomKonami.cc:24`. If the loaded image exceeds 256 KB, this is loaded
  anyway with a DISCLOSED note (not a silent claim of hardware-accuracy at that size) mirroring
  openMSX's own non-fatal warning (`RomKonami.cc:27-31`) — never a rejection, since openMSX itself
  does not reject it.
- `reset()`: `bank_switch(2, 0); bank_switch(3, 1); bank_switch(4, 2); bank_switch(5, 3);`.
- `bank_switch(page, block)`: `window.set_bank(page, block)`; if `page==2 or 3`: ALSO
  `window.set_bank(page-2, block)` (mirrors into window-slots 0/1); if `page==4 or 5`: ALSO
  `window.set_bank(page+2, block)` (mirrors into window-slots 6/7) — `RomKonami.cc:38-52`.
- `mem_write(addr, val)`: only when `0x6000 <= addr < 0xC000`; `bank_switch(addr >> 13, val)`.
  **Window-slot 2 (`0x4000-0x5FFF`) is FIXED forever at bank 0** (set once by `reset()`'s
  `bank_switch(2,0)` call; no write address ever targets slot 2 directly again, since writes only
  trigger at `addr >= 0x6000` — matches the source comment "`[0x4000..0x6000)` is fixed at segment 0",
  `RomKonami.cc:63`) — and because `bank_switch(2,...)` mirrors into slots 0/1 too, slots 0/1/2 all
  stay permanently at bank 0 for the entire session.

### 2.3 `CartridgeSlot` — the wrapper actually wired onto the bus

```
CartridgeSlot(int primary_slot_number);          // 1 or 2, diagnostic-only label
CartridgeLoadResult load(CartridgeMapperType type, std::vector<uint8_t> image);
void unload();                                    // reverts to empty/open-bus (A-M19-9 regression guard)
void reset();                                     // reinitializes bank state; no-op if empty; never unloads
[[nodiscard]] bool loaded() const;
[[nodiscard]] CartridgeMapperType mapper_type() const;  // precondition: loaded()
core::BusData mem_read(core::BusAddress address) override;   // 0xFF if empty
void mem_write(core::BusAddress address, core::BusData value) override;  // no-op if empty
```

`CartridgeLoadResult` = `{Ok, InvalidSlotNumber, ImageSizeInvalidForMapperType}` — `Hbf1xvMachine::
load_cartridge` returns this directly to the caller (test or CLI layer); it is NEVER silently
swallowed.

### 2.4 CLI / loading mechanism (concrete)

```
sony_msx_headless --cart1 roms/my_game.rom --cart1-type Konami
sony_msx_headless --cart1 roms/aleste.rom --cart1-type 8kB   # Generic8kB, per A-M19-10 disclosure
sony_msx_headless --cart2 roms/other.rom                      # defaults to Mirrored (A-M19-5)
```

- Recognized type strings (case-insensitive): `Mirrored`, `8kB`, `16kB`, `ASCII8`, `ASCII16`,
  `Konami` — openMSX's own canonical names (A-M19-3), not an invented vocabulary.
- **Missing/unreadable file policy — DELIBERATELY DIFFERENT from the M13 `RomAssetLoader` BIOS
  policy, and explicitly justified**: `RomAssetLoader` treats an absent/wrong-size BIOS/Kanji-font/
  disk-image asset as a NON-FATAL, always-expected fixed machine component — 0xFF-fill + a recorded,
  non-blocking diagnostic (`src/machine/rom_asset_loader.h:14-22`), because those assets are
  "supposed to always be there" and the emulator should still boot to SOME defined (if degraded)
  state for debugging. A user-specified `--cart1 <path>` is the OPPOSITE: an explicit, one-off,
  this-run-only request. Silently proceeding as if no cartridge were requested (falling back to an
  empty slot) would be a WORSE silent failure than the BIOS case, since the user gave an explicit
  instruction that would then be silently ignored with no visible signal. **Policy: an unreadable
  file OR any non-`Ok` `CartridgeLoadResult` prints a specific diagnostic to `stderr` and the process
  exits non-zero — never a silent fallback to "no cartridge."** This is a NEW, stricter policy
  relative to `RomAssetLoader`, deliberately scoped to user-specified, this-run-only cartridge
  arguments only (BIOS/Kanji-font/disk-image loading behavior is completely unchanged by M19).
- `Hbf1xvMachine::load_cartridge(...)` is directly callable by tests (no CLI/argv/file-I/O needed to
  exercise the device/machine layer — mirrors how existing tests call `load_memory`/`set_asset_root`
  directly rather than only through the CLI).

### 2.5 Determinism (hard requirement, cross-cutting)

- Reading a fixed on-disk cartridge file via `std::ifstream` at program startup (or, for tests,
  constructing an in-memory `std::vector<uint8_t>` directly) is a ONE-TIME, deterministic setup
  action — identical to the established BIOS/Kanji-font/disk-image asset-loading precedent
  (`RomAssetLoader`, `bios/f1xvkfn.rom` loading, M16's disk image), and categorically distinct from
  anything that runs inside the deterministic CPU-cycle simulation loop. Same file bytes in -> same
  in-memory image -> same mapper/bank-switch trajectory, every run.
- No new clock consumer: every MVP mapper type is a PURE, combinational function of its own stored
  bank state (like the M13 `RomDevice` and M17 `Ym2413Opll` classification) — `mem_read`/`mem_write`
  never consult `elapsed_cycles()`. Lowest possible regression risk on the CPU-timing-oracle axis
  (mirrors M17 §2.4's reasoning exactly).
- Two-run determinism is directly testable and REQUIRED: identical `--cart1 <path> --cart1-type
  <name>` (or identical `load_cartridge(...)` call) on two independent `Hbf1xvMachine` instances,
  followed by an identical CPU-driven bank-switch-write-then-read sequence, must produce byte-for-byte
  identical results — asserted directly in unit/integration tests (mirrors every prior milestone's
  "two-run determinism" test pattern).
- Missing-cartridge default (no `--cartN` given) is BYTE-FOR-BYTE IDENTICAL to the M13-M18 status quo
  (open-bus `0xFF` reads, ignored writes) — a strong, built-in regression guard requiring no special
  test scaffolding beyond confirming `CartridgeSlot`'s empty-state behavior matches `SlotBus`'s
  existing unattached-cell default (`slot_bus.cpp:88-92,105-109`).

### 2.6 `roms/aleste.rom` disposition (concrete fixture strategy, per A-M19-10)

Two DISTINCT fixture tracks, never conflated:

1. **Primary track — synthetic, hand-authored, byte-exact protocol fixtures** (mirrors the M16
   synthesized-disk-image precedent). A new `tools/gen-m19-cartridge-probe.py` generates, per MVP
   mapper type, a small deterministic image where bank `N`'s first byte equals `N` (or another
   distinguishable marker), used by EVERY unit test and by the A/B probe program (§2.7). These
   fixtures make EVERY byte-exact claim in §2.2 directly, unambiguously testable, with zero
   dependence on any real-world file's uncertain provenance.
2. **Secondary track — `roms/aleste.rom` as a large, real, on-disk-file MECHANICAL smoke fixture
   only** (A-M19-10). Loaded as `Generic8kB` (the explicitly non-hardware-claiming type, A-M19-10) in
   ONE dedicated integration test that asserts ONLY: (a) the 2,097,152-byte file loads without error
   (an exact multiple of `0x2000`, so `Generic8kB`'s load-time check passes); (b) its SHA256 matches
   `docs/asset-checksums.txt`'s recorded value; (c) after `cold_boot()` + routing primary slot 1 into
   CPU page 1 (`#A8 = 0x04`, mirrors the pattern in `map_flat_ram()`), `machine.debug_bus_read(0x4000)
   == 0x41` (`'A'`) and `debug_bus_read(0x4001) == 0x42` (`'B'`) — concrete, non-fabricated bytes this
   package directly observed during grounding (§1.3 A-M19-10). **No claim is made, anywhere, that this
   test proves the real Aleste game runs/plays correctly** — only that the cartridge-loading pipeline
   (file -> image -> slot -> CPU-bus-readable bytes) functions end-to-end with a real, sizeable,
   pre-existing file.

### 2.7 openMSX A/B acceptance plan

**Technique (content-bearing, unlike several prior milestones' write-only techniques): mount a
SYNTHETIC test cartridge into a real running MSX and let a genuine Z80 program page it in and read
it back — mirrors real MSX software's own slot-select convention.**

1. Extend the EXISTING `--parity-trace` mode (`src/main.cpp`, unmodified dispatch logic) to
   additionally recognize `--cart1 <path> [--cart1-type <name>]` (loaded via the SAME
   `parse_cartridge_cli`/`load_cartridge` path as the normal run mode, §2.4 — no duplicated logic).
   `run_parity_trace` already calls `machine.map_flat_ram()` (all 4 pages -> slot 3 RAM) before
   loading the driver program; the developer authors a small Z80 driver program (via
   `tools/gen-m19-cartridge-probe.py`) that itself issues `OUT (#A8),A` to repoint JUST CPU page 1
   (`0x4000-0x7FFF`) at primary slot 1 while pages 0/2/3 remain RAM (holding the program/stack) —
   `#A8 = 0b00_00_01_00 = 0x04` for "page1 -> slot 1, pages 0/2/3 -> slot 3" — then performs the
   mapper's documented bank-switch write sequence (§2.2) and reads back page-1 bytes into registers,
   before `HALT`.
2. Run the IDENTICAL synthetic cartridge file + driver program on real openMSX (WSL) with
   `-cart1 <file> -romtype <Type>` (or whichever CLI slot-letter openMSX resolves to this machine's
   primary slot 1 — **verification required, not assumed**, R-M19-6) and diff PC/registers/flags per
   instruction AND the read-back ROM byte VALUES themselves (a genuine, content-bearing comparison —
   both sides load the SAME authored file, so the expected bytes are fully known in advance, unlike
   the YM2413/printer milestones' write-only or asymmetric-default techniques).
3. **Adversarial comparator self-check** (as in every prior milestone): confirm an empty-side input
   -> BLOCKED and a corrupted field -> DIVERGENCE, so an empty diff is trustworthy.
- **Mechanics:** `tools/gen-m19-cartridge-probe.py` (new, mirrors `gen-m16-fdc-probe.py`/
  `gen-m17-ym2413-probe.py`/`gen-m18-peripheral-io-probe.py`); `tools/openmsx-m19-cartridge-parity.ps1`
  (new, mirrors the existing `openmsx-*-parity.ps1` family); output `docs/m19-parity-trace-diff.md`.
- **Hard rule (unchanged):** no parity claim without a genuine captured diff; if openMSX cannot be
  driven or the slot-lettering mapping cannot be confirmed, report BLOCKED honestly (mirrors the
  established M13/M16/M17 "honest BLOCKED" precedent) rather than fabricating a result.

### 2.8 Flagged findings — backlog-wording vs. reality verification (per the task's explicit ask)

Following the DEC-0012 precedent (verify before implementing, don't silently trust backlog shorthand
or a sample asset's apparent name):

- **B7 (cartridge loading, external primary slots 1 & 2) — NO backlog-wording discrepancy found.**
  The backlog's description ("external primary slots 1 & 2 ... M13 left them empty") matches the XML
  exactly (A-M19-1); no severity-worthy mismatch like the M17/DEC-0012 case (which risked building an
  entirely wrong device) exists here.
- **`roms/aleste.rom`'s implied identity — a genuine, disclosed finding, judged LOW severity /
  NON-BLOCKING / self-resolved (unlike DEC-0012).** The backlog/kickoff material's casual framing
  ("`roms/aleste.rom` sample" for bring-up/testing) could be read as implying the file IS a
  straightforward, identifiable "Aleste" cartridge dump ready to demonstrate real mapper behavior.
  Direct inspection (A-M19-10) shows this cannot be confirmed, and the file's size/content are
  actually INCOMPATIBLE with the most obvious guesses (Mirrored: too large; genuine 256 KB-class
  Konami dump: too large per openMSX's own warning threshold). **Why this does NOT need a
  DEC-level human decision gate (contrast DEC-0012):** DEC-0012's finding risked FABRICATING WRONG
  HARDWARE BEHAVIOR (an SRAM/bank-register overlay that does not exist on this specific chip-select)
  if followed literally — a real correctness risk either way. Here, BOTH readings (assume a specific
  identity vs. disclose uncertainty) carry ZERO fabrication risk to actual device behavior, because
  no unit/integration test's PASS/FAIL correctness claim depends on `aleste.rom`'s real identity —
  the byte-exact protocol tests use synthetic fixtures (§2.6 track 1) regardless. Choosing the
  HONEST, non-claiming path (§2.6 track 2: `Generic8kB`, disclosed, mechanical-only assertions) is
  simply the objectively safer of two options with no actual behavioral trade-off, exactly mirroring
  the M18 §2.7 printer-port-range finding's low-severity, self-resolved disposition, not the DEC-0012
  pattern. **Recommendation:** no ledger decision entry needed; this finding is recorded here for
  transparency (per the task's "surface discrepancies" instruction) and resolved by construction
  (§2.6's fixture strategy).

---

## 3. Milestones (Slice Plan M19-S1 … S6)

Every slice runs the full evidence-gate set (§6, evidence gates) and must leave `ctest` green.

### M19-S1 — `CartridgeRomWindow` + `CartridgeMapperType`
- **Goal:** the shared 8-slot x 8 KB window primitive (A-M19-6/A-M19-7) and the mapper-type
  enum/name-parsing pair (A-M19-3/A-M19-5), fully independent of any concrete mapper class.
- **Files:** `src/devices/cartridge/{cartridge_rom_window,cartridge_mapper_type}.{h,cpp}`;
  `CMakeLists.txt`.
- **Unit tests** (`tests/unit/devices/cartridge/{cartridge_rom_window,cartridge_mapper_type}
  _unit_test.cpp`): default-mask wrap-then-unmapped resolution (A-M19-6, both branches);
  overridden-mask (Konami-style) resolution including the "oversized image, mask never engages"
  subtlety (A-M19-6); `set_unmapped`/open-bus `0xFF` read; name<->enum round-trip for all 6 canonical
  strings (A-M19-3) including case-insensitivity; unrecognized name string -> `nullopt` (never a
  silent default, contrast A-M19-5's OMITTED-flag default).
- **Gates:** build + ctest green.

### M19-S2 — The six MVP mapper-type devices
- **Goal:** `CartridgeMirroredRom`, `CartridgeGeneric8kbRom`, `CartridgeGeneric16kbRom`,
  `CartridgeAscii8kbRom`, `CartridgeAscii16kbRom`, `CartridgeKonamiRom` — each a
  `CartridgeMapperDevice`, byte-exact per §2.2, composing `CartridgeRomWindow`.
- **Files:** `src/devices/cartridge/cartridge_mapper_device.h` (interface);
  `src/devices/cartridge/cartridge_{mirrored,generic8kb,generic16kb,ascii8kb,ascii16kb,konami}
  _rom.{h,cpp}`; `CMakeLists.txt`.
- **Unit tests** (one file per type, `tests/unit/devices/cartridge/cartridge_<type>_rom_unit_test
  .cpp`, using SYNTHETIC bank-marker images, A-M19-10 track 1): reset() default bank layout exactly
  per §2.2; bank-switch write lands at the documented address(es)/register(s) only, ignored
  elsewhere; read-back reflects the correct bank's marker byte; load-time size-validation rejection
  (A-M19-7) for a non-multiple-sized image; Konami's block2/0/1 permanent-fixed-mirror quirk and
  its mirror-into-adjacent-slots on every bank-switch (§2.2); Ascii16kB's "both middle banks start
  identical" reset quirk; two-run determinism.
- **Gates:** build + ctest green.

### M19-S3 — `CartridgeSlot` wrapper + load/unload/reset dispatch
- **Goal:** `CartridgeSlot` (§2.3): empty-slot open-bus default (A-M19-9 regression guard),
  `load()`/`unload()`/`reset()` dispatch across the 6 concrete types via `CartridgeMapperType`.
- **Files:** `src/devices/cartridge/cartridge_slot.{h,cpp}`; `CMakeLists.txt`.
- **Unit tests** (`tests/unit/devices/cartridge/cartridge_slot_unit_test.cpp`): empty slot reads
  `0xFF`/writes no-op (byte-for-byte M13-M18 regression guard, explicit assertion); `load()` for each
  of the 6 types switches the active mapper and its behavior is observable through `CartridgeSlot`'s
  own `mem_read`/`mem_write`; a size-invalid load returns the documented error and leaves the slot's
  PRIOR state untouched (never partially applied); `unload()` reverts to the identical empty-slot
  default; `reset()` reinitializes bank state without unloading (A-M19-9); two-run determinism.
- **Gates:** build + ctest green.

### M19-S4 — Machine wiring + system integration (both slots)
- **Goal:** attach `cartridge_slot1_`/`cartridge_slot2_` at primary slots 1/2 (all 4 pages each, no
  `set_expanded`, A-M19-1); `cold_boot()` reset wiring; `load_cartridge`/`unload_cartridge`/
  `cartridge_slot1()`/`cartridge_slot2()` machine API.
- **Files:** `src/machine/hbf1xv_machine.{h,cpp}` (edit, additive only); `CMakeLists.txt`.
- **Integration tests** (`tests/integration/machine/hbf1xv_m19_cartridge_integration_test.cpp`, using
  synthetic fixtures): a real CPU program over the M11 system bus selects primary slot 1 into a page
  via `#A8`, performs each MVP mapper's bank-switch write sequence, and reads back the expected
  marker bytes; the SAME sequence repeated for primary slot 2 independently (proving the two slots
  are independently addressable); an UNLOADED/never-loaded slot regression-guard case (byte-for-byte
  M13-M18 open-bus behavior with the M19 code present but unused); `cold_boot()`-preserves-cartridge
  case (A-M19-9); zero regression on M1-M18 suites (in particular the M9/M12 CPU-timing oracles and
  the M13-M18 slot-map goldens, untouched by construction, §2.5).
- **A separate integration test** (`tests/integration/machine/hbf1xv_m19_aleste_smoke_integration_test
  .cpp`, needs a `SONY_MSX_ROMS_DIR` compile definition mirroring the existing `SONY_MSX_BIOS_DIR`
  pattern, pointing at `${CMAKE_SOURCE_DIR}/roms`): the `roms/aleste.rom` mechanical smoke fixture
  (§2.6 track 2) — loads the real 2 MB file as `Generic8kB`, asserts the SHA256 + the two concrete
  byte assertions (A-M19-10). Explicitly documented in the test file's own comment as a MECHANICAL
  smoke check only, no gameplay/hardware-identity claim.
- **Gates:** build + ctest green (full suite).

### M19-S5 — CLI integration (`src/main.cpp`)
- **Goal:** `src/machine/cartridge_cli.{h,cpp}` (pure argv parser, A-M19-4); wire `--cart1`/
  `--cart1-type`/`--cart2`/`--cart2-type` into BOTH the default/normal run path and the existing
  `--parity-trace` mode (§2.7); the "loud, non-zero-exit, never-silent" missing/invalid-cartridge
  policy (§2.4) at the `main.cpp` layer only (library API returns a value; only the executable layer
  exits the process).
- **Files:** `src/machine/cartridge_cli.{h,cpp}` (new); `src/main.cpp` (edit, additive); `CMakeLists
  .txt`.
- **Unit tests** (`tests/unit/machine/cartridge_cli_unit_test.cpp`): `--cart1 <path>` alone defaults
  to `Mirrored` (A-M19-5); `--cart1 <path> --cart1-type Konami` (and every other of the 6 canonical
  names, case-insensitively) parses correctly regardless of flag order; an unrecognized `--cart1-type`
  value is reported as a parse error (never silently defaulted); both slots can be specified
  simultaneously and independently; absent `--cart1`/`--cart2` entirely yields no per-slot request
  (existing behavior fully preserved, zero cartridge flags = today's status quo).
- **Gates:** build + ctest green (full suite). No process-spawn/system test is added for the CLI
  layer itself (this project's test suites link `sony_msx_core` directly, never the compiled
  executable — the pure-function `cartridge_cli` unit test fully covers the parsing logic without
  needing subprocess plumbing, matching established convention).

### M19-S6 — openMSX A/B parity + backlog finalization
- **Goal:** capture the content-bearing A/B evidence (§2.7); resolve/report R-M19-6 (openMSX
  slot-lettering verification) honestly either way; finalize the full deferred-backlog review (§4)
  in the ledger; refresh checksums.
- **Files:** `tools/gen-m19-cartridge-probe.py` (new); `tools/openmsx-m19-cartridge-parity.ps1`
  (new); `tests/parity/m19_cartridge_probe.bin` + the accompanying synthetic cartridge image(s);
  `docs/m19-parity-trace-diff.md`; refreshed `docs/asset-checksums.txt`.
- **Tests:** the S4 integration test serves as the deterministic golden the A/B probe mirrors.
- **Gates:** all four evidence gates **plus** the A/B gate. No parity claim without a genuine
  capture; if the openMSX slot-lettering mapping (which CLI slot-letter lands a file into THIS
  machine's XML-declared primary slot 1 vs. 2) cannot be empirically confirmed, report BLOCKED
  honestly for that specific sub-claim while still delivering the CPU-visible architectural-parity
  and read-back-content subjects (which do not actually depend on knowing which of the two
  interchangeable external slots was used, since the probe only ever exercises one slot at a time).

---

## 4. Full Deferred-Backlog Review (mandatory, per DEC-0005 and the human's explicit instruction)

Source of truth: `agent-protocol/state/deferred-backlog.md`. **Every** row — A.B1-B9, B.C1-C10,
C.D1-D7, D.E1-E2, E.F1-F2 — re-affirmed below with a one-line justification, per the human directive
"review deferred items and have them properly addressed during development" (re-applied every cycle
since DEC-0005). Nothing is silently dropped.

### Section A (human-directive-tracked rows)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| B1 | PSG/YM2149 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, no M19 impact. |
| B2 | RTC/RP5C01 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, no M19 impact. |
| B3 | FM-PAC (OPLL YM2413) device internals | DONE (M17) — unchanged | Closed at M17; re-affirmed, no M19 impact. |
| B4 | MSX-JE 16 KB SRAM | OPEN, re-owned to B6 — unchanged | Per DEC-0012; unrelated to external cartridge loading (M19 touches slots 1/2 only, not slot 0-3). |
| B5 | Kanji-font I/O `#D8-DB` | DONE (M18) — unchanged | Closed at M18; re-affirmed, no M19 impact. |
| B6 | Halnote/MSX-JE firmware mapping (slot 0-3) | OPEN — unchanged | Distinct, INTERNAL slot (0-3), not one of this milestone's external slots (1/2); explicitly noted in §1.2 that `Halnote` mappertype is excluded from M19's deferred-mapper-type list G4 for exactly this reason (it belongs to B6, not to "external cartridges"). |
| B7 | **Cartridge loading — external primary slots 1 & 2** | **CLOSES this cycle (M19), upon successful developer + QA completion** | §2.1-§2.7 deliver the CartridgeSlot/mapper-type family, CLI/API loading, and real A/B evidence for the 6 MVP mapper types (§1.2's deferred remainder tracked as new rows G1-G4, mirroring the M14/M17/M18 contract-vs-depth pattern — this is a genuine close of the CONTRACT, not a partial/hollow one). |
| B8 | FDC drive mechanics | DONE (M16) — unchanged | Closed at M16; re-affirmed, no M19 impact. |
| B9 | VRAM/V9958 VDP | DONE (M14) — unchanged | Closed at M14; re-affirmed, no M19 impact. |

### Section B (other tracked deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| C1 | Exact cycle/T-state timing parity | OPEN — unchanged | M19 introduces no new clock consumer (§2.5); unrelated. |
| C2 | Z80 HALT-R increment | OPEN — unchanged | Per DEC-0004; unrelated to M19. |
| C3 | ZEXDOC/ZEXALL full sweep | OPEN — unchanged | Needs a legally-sourced ZEX binary; unrelated to M19. |
| C4 | S1985 backup-RAM `.sram` persistence | DONE (M15) — unchanged | Closed at M15; re-affirmed. |
| C5 | Full boot past first device read | IN-PROGRESS (carried from M16) — unchanged | M19 adds no CPU-visible boot-path interaction (external cartridge slots are not consulted by the unattended BIOS/Disk-ROM auto-boot handshake); the M16 residual (auto-disk-boot trigger investigation) is not this milestone's job to close, per M16/M17/M18's own precedent of leaving C5 untouched. |
| C6 | Keyboard matrix + joystick | DONE (M15) — unchanged | Closed at M15; re-affirmed. |
| C7 | Printer + cassette | DONE (M18) — unchanged | Closed at M18; re-affirmed, no M19 impact. |
| C8 | Sony speed-controller + hardware PAUSE + Ren-Sha Turbo | OPEN — unchanged | HB-F1XV-specific companion-chip behavior; unrelated to M19. |
| C9 | SDL3 frontend | OPEN — unchanged | Presentation layer; unrelated to M19 (a loaded cartridge's eventual VISUAL/audio output, if any, is a rendering/audio-depth concern for later milestones, not the loading contract this milestone delivers). |
| C10 | FDC flux-level/DMK fidelity | OPEN — unchanged | Unrelated to M19 (FDC, not cartridge). |
| D1 | Pixel-accurate raster rendering | OPEN — unchanged | VDP rendering depth; unrelated to M19. |
| D2 | Sprite rendering + collision | OPEN — unchanged | Unrelated to M19. |
| D3 | VDP command engine | OPEN — unchanged | Unrelated to M19. |
| D4 | Cycle-accurate VDP access-slot/command timing | OPEN — unchanged | Unrelated to M19. |
| D5 | YJK/YAE color decode + DAC | OPEN — unchanged | Unrelated to M19. |
| D6 | Horizontal scroll/interlace/blink/superimpose | OPEN — unchanged | Unrelated to M19. |
| D7 | G6/G7 VRAM planar interleave | OPEN — unchanged | Unrelated to M19. |

### Section C (M14 VDP-depth deferrals, re-affirmed)

Already re-listed above as D1-D7 (the deferred-backlog file's own Section C IS the D1-D7 table); no
separate additional rows exist in this section beyond those already covered.

### Section D (M17 YM2413 DSP/timing deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| E1 | YM2413 FM-synthesis DSP/audio depth | OPEN — unchanged | Unrelated to M19 (audio device, not a memory mapper). |
| E2 | YM2413 register-write timing constraint | OPEN — unchanged | Unrelated to M19. |

### Section E (M18 printer/cassette depth deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| F1 | Cassette tape image-format/signal fidelity | OPEN — unchanged | Unrelated to M19. |
| F2 | Printer image/ESC-sequence rendering depth | OPEN — unchanged | Unrelated to M19. |

### New rows proposed this cycle (mirroring the M14 D1-D7 / M17 E1-E2 / M18 F1-F2 precedent)

| # | Item | Status | Candidate owner | Grounding |
|---|---|---|---|---|
| **G1** | **KonamiSCC mapper + embedded SCC/SCC+ sound chip** — `RomKonamiSCC` additionally owns a real `SCC` 5-channel wavetable synth (`references/openmsx-21.0/src/memory/RomKonamiSCC.hh:9,29-30`), a genuinely new audio device, not an incremental mapper variant. M19 delivers the plain `Konami` mapper (no SCC) only. | OPEN (new) | Future audio milestone, paired with E1 (YM2413 DSP depth) / C9 (SDL3 frontend) | `references/openmsx-21.0/src/memory/RomKonamiSCC.hh/.cc`, `SCC.hh` |
| **G2** | **ROM-database/SHA1 auto-mappertype-detection + heuristic byte-pattern auto-detection** (`RomInfo`/`RomDatabase`/`share/softwaredb.xml`; `RomFactory.cc:82-169` `guessRomType`). M19 requires an explicit `--cartN-type` (defaults to `Mirrored` only when the flag is OMITTED, A-M19-5) — never infers a type from content. | OPEN (new) | Future milestone, only if auto-identification becomes a real need | `RomFactory.cc:171-210`; `RomInfo.hh/.cc`; `share/softwaredb.xml` |
| **G3** | **Full `CartridgeSlotManager`-style dynamic runtime hot-plug** (Tcl `cart`/`ext` commands, live insert/eject while running). M19's cartridge composition is fixed at construction/cold-boot time, matching every other device in this project. | OPEN (new) | Future milestone, only if live insert/eject during a running session becomes a real requirement (e.g., alongside a future interactive/SDL3 session) | `references/openmsx-21.0/src/CartridgeSlotManager.hh/.cc` |
| **G4** | **The long tail of ~80 other specialized/vendor-specific mapper types** in `RomTypes.hh` (Panasonic/National-internal, NEO-8/16, Majutsushi, Synthesizer, PlayBall, Zemina family, Holy Qu'ran (1/2), FSA1FM (1/2), the Manbow2/MegaFlashROMSCC(+)/RBSC/HamarajaNight flash-cart family, ReproCartridge (V1/V2), KonamiUltimateCollection, MSXDOS2, R-Type, CrossBlaim, HarryFox, GameMaster2, ASCII8/16-with-SRAM variants, Koei (8/32), Wizardry, MSXWrite, MultiRom, RAMFILE, ColecoMegaCart, WonderKid, Dooly, MSXtra, Yamanooto, AlAlamiah30in1, RetroHard31in1, ROMHunterMk2, ASCII16X — excluding `Halnote` (B6, internal slot 0-3, unrelated) and `DRAM` (MSXturboR-specific, not this machine). | OPEN (new) | Future milestone(s), only if/when a specific real cartridge requiring one of these is actually wanted | `RomTypes.hh:8-100`; `RomFactory.cc:221-382` |

**Backlog bookkeeping note (to be executed at ledger-update time by the coordinator, not in this
planning artifact, per the exact M17/M18 precedent):** on M19 closure, mark B7 `DONE (M19)`; add
G1-G4 as new OPEN rows under a new Section F ("M19 cartridge-mapper-depth deferrals").

---

## 5. Acceptance Criteria (M19 exit)

1. `CartridgeRomWindow` (shared 8-slot x 8 KB window, byte-exact bank-resolution per
   `RomBlocks::setRom`, A-M19-6) and `CartridgeMapperType` (6-value enum + openMSX-canonical
   name parsing, A-M19-3/A-M19-5) implemented under `src/devices/cartridge/`. *(S1)*
2. Six MVP mapper-type devices (`Mirrored`, `Generic8kB`, `Generic16kB`, `Ascii8kB`, `Ascii16kB`,
   `Konami`) implemented byte-exact per §2.2, each grounded in a concrete openMSX class citation in
   the implementation report. *(S2)*
3. `CartridgeSlot` wrapper implemented: empty-slot state is byte-for-byte identical to the M13-M18
   open-bus default (regression guard); `load`/`unload`/`reset` dispatch across all 6 types;
   size-invalid loads are rejected with a clear, non-fabricated diagnostic (A-M19-7). *(S3)*
4. Wired into the M11 `slot_bus_` at primary slots 1 and 2 (all 4 pages each, no sub-slot expansion,
   A-M19-1); `Hbf1xvMachine::load_cartridge`/`unload_cartridge`/`cartridge_slot1()`/`cartridge_slot2()`
   API added; `cold_boot()` reinitializes bank state without unloading (A-M19-9). *(S4)*
5. `roms/aleste.rom` mechanical smoke fixture (§2.6 track 2) loads successfully and its SHA256 +
   the two concrete byte assertions (A-M19-10) pass, WITHOUT any claim about the file's real-world
   game/mapper identity. *(S4)*
6. CLI integration (`--cart1`/`--cart1-type`/`--cart2`/`--cart2-type`) implemented via a pure,
   directly-unit-tested argv parser (`cartridge_cli`); missing/invalid cartridge requests are LOUD
   (stderr diagnostic + non-zero exit), never silently ignored (§2.4). *(S5)*
7. Deterministic unit + integration tests cover every new behavior in §2.2-§2.4; two identical runs
   (or two identical `load_cartridge` calls followed by identical CPU-driven bank-switch sequences)
   produce byte-identical state. *(S1-S5)*
8. Real openMSX A/B evidence captured for the mapper write/read-back path
   (`docs/m19-parity-trace-diff.md`), including genuine CONTENT-level (not just architectural)
   comparison since both sides load an identically-authored synthetic cartridge; if the openMSX
   slot-lettering mapping cannot be empirically confirmed, that specific sub-claim is honestly
   reported BLOCKED (§2.7) rather than fabricated — no parity claim without a genuine capture. *(S6)*
9. **Zero regression** across M1-M18 suites, including the CPU-timing oracles (untouched by
   construction, no new clock consumer, §2.5) and every prior milestone's slot-map/device-accessor
   goldens. *(S4, S6)*
10. Every deferred-backlog row (A.B1-B9, B.C1-C10, C.D1-D7, D.E1-E2, E.F1-F2) re-affirmed with
    justification (§4); B7 closes; G1-G4 proposed as new rows. *(§4, this package)*
11. Evidence gates executed and captured each cycle (validate-assets, checksum, Debug build, ctest).
12. QA sign-off recorded before closure (`docs/m19-qa-signoff.md`). **Normal human-release-decision
    gate — no auto-close** (per the task brief and DEC-0003's auto-close grant being M12-only);
    autopilot pauses at QA sign-off for the separate human release decision + tag.

---

## 6. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|---------------------|
| R-M19-1 | Bank-resolution algorithm subtlety (A-M19-6) implemented wrong — e.g. masking the requested value UNCONDITIONALLY instead of only as an out-of-range fallback, silently changing Konami's oversized-image behavior. | S1 unit tests explicitly assert BOTH branches (in-range-direct AND out-of-range-then-wrap-then-still-unmapped) for both a default mask and an overridden (Konami-style) mask, including the "2 MB Konami image, mask never engages" case cited verbatim from `RomBlocks.cc:107-118`. |
| R-M19-2 | Ascii16kB's "both middle logical banks default to the SAME bank (not sequential 0/1)" reset quirk gets silently "fixed" to the more intuitive sequential pattern, diverging from real `RomAscii16kB.cc:22-28`. | S2 unit test explicitly asserts both middle banks read IDENTICAL content at reset, byte-for-byte matching the cited source. |
| R-M19-3 | Konami's block-2/0/1 "permanently fixed at bank 0, mirrored into slots 0/1" quirk (a consequence of `bank_switch(2,...)` never being re-invoked by any write address) is missed, allowing slots 0/1/2 to accidentally become independently bank-switchable. | S2 unit test writes to EVERY address in `0x6000-0xBFFF` with varying values and asserts slots 0/1/2 NEVER change from their reset-time bank-0 content, matching `RomKonami.cc:63`'s "fixed at segment 0" comment. |
| R-M19-4 | `roms/aleste.rom`'s uncertain identity (A-M19-10) gets silently upgraded into an implicit "this is the real Aleste cartridge, Konami-mapper, working correctly" claim somewhere in the implementation report or tests, contradicting the disclosed finding. | §2.6/A-M19-10 explicitly fence this: the smoke test's own file-level comment states the mechanical-only scope; the implementation report must not claim gameplay/hardware-identity correctness for this file; QA specifically checks for this during sign-off. |
| R-M19-5 | The new "loud, non-zero-exit" cartridge-CLI-failure policy (§2.4) accidentally gets applied to the EXISTING BIOS/Kanji-font/disk-image `RomAssetLoader` path too, breaking the established graceful-degradation behavior relied on by every prior milestone's tests. | S5 explicitly scopes the new policy to ONLY the new `cartridge_cli`/`load_cartridge` call sites in `main.cpp`; the S4/S5 integration tests include a regression check that `RomAssetLoader`'s existing missing-BIOS diagnostic behavior (non-fatal, 0xFF-fill) is completely unchanged. |
| R-M19-6 | The openMSX CLI's `-carta`/`-cartb`/etc. slot-lettering (`MSXRomCLI.cc:27`) may not map onto THIS machine's XML-declared primary slot 1 vs. 2 the way assumed; an A/B claim about "primary slot 1 specifically" could be wrong if unverified. | Developer empirically confirms (via a live WSL openMSX Tcl probe, e.g. querying the mounted cartridge's slot via `machine_info`/the `CartridgeSlotInfo` topic cited in `CartridgeSlotManager.hh:105-110`) which CLI invocation lands a file into which of this machine's two external slots BEFORE making any slot-specific A/B claim (mirrors the R-M17-6 "verify before claiming" precedent); if unconfirmable, the A/B report notes this specific sub-claim as BLOCKED while still delivering the architectural/content-parity evidence (which does not depend on knowing which of the two interchangeable slots was used, since only one slot is exercised at a time). |
| R-M19-7 | Scope creep into KonamiSCC/auto-detection/hot-plug (all tempting, all real features) inflates M19 beyond the MVP boundary this package sets. | §1.2 and new backlog rows G1-G4 explicitly fence these out; any expansion requires a decisions-ledger entry per guardrails "Scope Control". |
| R-M19-8 | A cartridge-slot device accidentally gets marked `expanded` (A-M19-1), silently changing `#FFFF` semantics for primary slots 1/2 in a way this machine's real hardware does not exhibit. | S4 integration test explicitly asserts `machine.slot_expanded(1) == false` and `machine.slot_expanded(2) == false` after `cold_boot()` — a direct, cheap regression guard using the EXISTING `Hbf1xvMachine::slot_expanded()` debug accessor (`hbf1xv_machine.h:89`). |

---

## 7. Developer Handoff

- **Start at M19-S1** (shared window primitive + mapper-type enum) — grounded per A-M19-6/A-M19-3;
  cite `references/openmsx-21.0/src/memory/RomBlocks.{hh,cc}` line ranges in code comments (never
  copy the code itself — GPL isolation).
- **Sequence S1->S6** in order; each runs the full evidence-gate set; keep `ctest` green every cycle.
- **`src/` placement fixed by §2.1:** new top-level `src/devices/cartridge/` (10 files: window
  primitive, mapper-type enum, family-local `CartridgeMapperDevice` interface, 6 concrete mapper
  classes, `CartridgeSlot` wrapper); new `src/machine/cartridge_cli.{h,cpp}` (pure argv parser);
  machine wiring only in `src/machine/hbf1xv_machine.{h,cpp}` (additive: two new members, an
  attach loop per slot in `wire_bus()`, two `reset()` calls in `cold_boot()`, the `load_cartridge`/
  `unload_cartridge`/accessor API); `src/main.cpp` additive edit for CLI dispatch (both the default
  run path and the existing `--parity-trace` mode, sharing one parser/loader).
- **Critical architectural findings to honor:**
  - Slots 1/2 are NEVER `set_expanded` (A-M19-1) — attach at `(primary, sub=0, page)` directly;
    `SlotBus::sub_for_page` already returns 0 unconditionally for a non-expanded primary, so this
    requires ZERO change to `SlotBus` itself.
  - `CartridgeSlot::reset()` reinitializes bank state but NEVER unloads the cartridge (A-M19-9) —
    matches real hardware power-cycle-with-cartridge-inserted behavior.
  - The bank-resolution mask is a FALLBACK for out-of-range requests only, not an unconditional
    AND-mask (A-M19-6) — get this exactly right, it is the single most subtle byte-exact detail in
    this milestone.
  - `roms/aleste.rom` is a MECHANICAL smoke fixture ONLY (A-M19-10/§2.6) — never assert its
    real-world mapper/game identity; all byte-exact protocol correctness is proven via synthetic
    fixtures instead.
  - The cartridge-CLI missing/invalid-file policy (loud, non-zero exit, §2.4) is NEW and DELIBERATELY
    stricter than `RomAssetLoader`'s existing BIOS policy — do not conflate the two, and do not let
    the new policy leak into the existing BIOS/Kanji-font/disk-image loading call sites.
- **No new clock consumer (§2.5):** every mapper-type device is pure/combinational — no
  `elapsed_cycles()` adapter needed, simpler than the M15/M16/M18-cassette X-pattern devices.
- **A/B (§2.7):** build `tools/gen-m19-cartridge-probe.py` + `tools/openmsx-m19-cartridge-parity.ps1`;
  extend the EXISTING `--parity-trace` mode with `--cart1`/`--cart1-type` recognition (shared parser);
  empirically verify the openMSX slot-lettering mapping (R-M19-6) BEFORE claiming a slot-specific
  parity result; report BLOCKED honestly for any sub-claim that cannot be verified.
- **Ledger discipline (DEC-0005):** on closure, mark B7 `DONE (M19)`; add new rows G1-G4 under a new
  Section F; update `state/current-phase.md` and `state/milestones.md`.
- **Gate:** NORMAL human-release-decision gate — do not auto-close; pause at QA sign-off for the
  separate human release decision + tag (per the task brief and DEC-0003's M12-only auto-close
  scope).
- Deliverables: source per §2.1; tests per §3; `docs/m19-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`; an implementation report `docs/m19-implementation-report.md`; then hand
  to QA for `docs/m19-qa-signoff.md`.
