# M20 Planner Package — Halnote / MSX-JE Firmware Mapping (Slot 0-3) + MSX-JE 16 KB Battery-Backed SRAM

- Milestone ID: M20
- Title: Halnote-Mapped MSX-JE Firmware ROM (Slot 0-3) + Real `BatteryBackedSram` Consumer
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M20-001 (planner-first, no production code)
- Decisions in force: DEC-0005 (backlog disposition discipline), DEC-0009 (indicative order: "M20
  Halnote (B6)"), DEC-0012 (the M17 decision that re-grounded B4 to B6 and built the reusable
  `BatteryBackedSram` primitive specifically for this milestone to consume — read in full below).
- Backlog effect: **closes B4 AND B6 together in the same slice (M20-S3)** — see §3 and §4. No other
  row closes this cycle; all rows re-affirmed (§4, full registry, all 34 rows).
- Gate: **NORMAL human-release-decision gate (no auto-close)** — explicit per the task brief and
  consistent with DEC-0003's auto-close grant being M12-only. Autopilot pauses at QA sign-off for the
  separate human release decision + tag, matching the M13/M14/M15/M16/M17/M18/M19 pattern.

> Grounding rule: every behaviour claim below cites a concrete `references/...` path (with line
> numbers) or a current-repo `src/...`/`docs/...` path. openMSX sources are the BEHAVIOUR reference
> only and are GPL — **never copy openMSX code into `src/`** (`CLAUDE.md`,
> `agent-protocol/guardrails.md`). Every hex/bit-arithmetic claim below was independently
> re-computed by the planner (not just transcribed), given a real transcription-risk finding
> surfaced during this cycle's own grounding pass (§1.3 A-M20-13).

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) `HalnoteRom` — a byte-exact port of `references/openmsx-21.0/src/memory/RomHalnote.{hh,cc}`**:
  a 1 MB ROM organized as 128×8 KB main banks (window-slots 2-5 switchable; slots 0/1 SRAM-gated;
  slots 6/7 permanently unmapped), with the last 512 KB additionally addressable as 256×2 KB
  sub-banks that can shadow part of window-slot 3 (CPU 0x7000-0x7FFF) when a sub-mapper-enable bit
  is set.
- **(b) The REAL 16 KB `BatteryBackedSram` consumer** — the M17-built primitive
  (`src/devices/memory/battery_backed_sram.{h,cpp}`), reused verbatim (not reimplemented), wired as
  `HalnoteRom`'s own SRAM store, gated into CPU addresses 0x0000-0x3FFF by the sram-enable bit.
- **(c) Machine wiring**: attach at primary slot 0, secondary slot 3, all 4 CPU pages (the XML's
  `<mem base="0x0000" size="0x10000">` — confirmed below, §1.3 A-M20-9, to span the FULL 64 KB CPU
  address space of this sub-slot); a real `bios/f1xvfirm.rom` load via the established
  `RomAssetLoader` pattern; deterministic SRAM file persistence mirroring the M15 S1985 backup-RAM
  precedent exactly.
- **(d) Deterministic unit + integration tests**, zero regression M1-M19.
- **(e) Real openMSX A/B evidence** (§2.6) covering the main-bank-switch protocol, the SRAM-enable
  gate, the SRAM content read/write path, and the sub-bank shadowing quirk specifically — no parity
  claim without a genuine capture.
- **(f) Full deferred-backlog review** — every row across all six registry sections (B1-B9, C1-C10,
  D1-D7, E1-E2, F1-F2, G1-G4 — 34 rows total) re-affirmed with a one-line justification (§4).
- **(g) Both B4 and B6 close together this cycle** — per the human's explicit, non-negotiable
  directive. §4 shows this is achieved with no need to defer B4 again.

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M20 | Owning milestone (candidate) |
|---|---|---|
| **Halnote's own runtime firmware/software behavior** (what the real MSX-JE Japanese-input firmware actually *does* once executing) | M20 delivers the mapper CONTRACT (bank-switch/SRAM/sub-bank protocol), mirroring the M14/M17/M18/M19 contract-vs-depth split — not an emulation of the firmware's own application logic, which is outside any single device's scope and not named by any backlog row. | N/A — not a tracked backlog item; the firmware ROM's bytes are simply data the mapper serves correctly. |
| **A CLI flag for the Halnote SRAM path** (e.g. `--halnote-sram <path>`) | The established precedent for exactly this class of feature (M15 S1985 backup-RAM persistence, `set_backup_ram_path`/`flush_backup_ram`) has **no CLI flag** either — confirmed by direct grep of `src/main.cpp` (§1.3 A-M20-12). M20 mirrors that exact scope level: a directly-testable machine API, not a new CLI surface the human has not asked for. | Future CLI-completeness milestone, if ever wanted (small, additive, low risk). |
| **Real-time/asynchronous SRAM auto-save** (e.g. flush-on-every-write, a background timer) | Every existing persistence primitive in this project (S1985 backup RAM, M17 `BatteryBackedSram`) is load-at-cold-boot / explicit-flush-on-demand only — no host-nondeterminism, no implicit timers. M20 matches this discipline exactly (§2.3). | N/A — not a tracked backlog item; would be a determinism regression if built naively. |

### 1.3 Assumptions (each grounded, each with a verification action)

- **A-M20-1 (asset naming: `f1xvfirm.rom` vs `hb-f1xv.rom` — resolved, not a mismatch).** Confirmed by
  direct reads: `docs/asset-checksums.txt:12` records `bios/f1xvfirm.rom | 1048576 bytes |
  c78c13996a4...` (exactly `0x100000` bytes, matching the required size, §1.3 A-M20-2).
  `src/machine/rom_asset_loader.h:26-32` documents the established, project-wide convention: local
  BIOS-class assets are renamed from the openMSX XML's own filenames (`Sony_HB-F1XV.rom` →
  `f1vxbios.rom`... concretely, `hb-f1xv_basic-bios2p.rom`→n/a, `hb-f1xv_msx2sub.rom`→`f1xvext.rom`,
  `hb-f1xv_kanjibasic.rom`→`f1xvkdr.rom`, `hb-f1xv_disk.rom`→`f1xvdisk.rom`,
  `hb-f1xv_fmbasic.rom`→`f1xvmus.rom`, `hb-f1xv_kanjifont.rom`→`f1xvkfn.rom` — every single ROM role
  in `src/machine/hbf1xv_machine.cpp:291-296`'s `load_rom_assets()` already renames the XML's own
  filename to a shorter local one). `hb-f1xv.rom` (XML, `Sony_HB-F1XV.xml:108`) → `f1xvfirm.rom` (this
  project's local name) is the SAME, already-established pattern — not a new or different
  convention, and not a real mismatch. *Verify:* `tools/validate-assets.ps1`/`tools/checksum-assets.ps1`
  re-run at implementation time must show `f1xvfirm.rom` present at exactly `1,048,576` bytes (already
  true today, per the checksum file read this cycle).
- **A-M20-2 (1 MB / 128×8 KB banking + last-512 KB 256×2 KB sub-banking).** Grounded directly:
  `references/openmsx-21.0/src/memory/RomHalnote.cc:1-24` (header comment: "1024kB mapper... divided
  in 128 pages of 8kB. The last 512kB can also be mapped as 256 pages of 2kB. There is 16kB SRAM.")
  and `:40-43` (`if (rom.size() != 0x100000) throw ...`). `0x100000 / 0x2000 (8 KB) = 128` banks;
  `0x100000 / 2 = 0x80000` (last 512 KB) `/ 0x800 (2 KB) = 256` sub-banks — arithmetic independently
  re-verified this cycle, not merely transcribed.
- **A-M20-3 (register-numbering discrepancy between the source file's own comment and its code —
  resolved explicitly, per the task's instruction to verify rather than trust the summary).**
  `RomHalnote.cc:10-14`'s own header comment labels the four main bank-switch registers "bank 0"
  (region 0x4000-0x5FFF) through "bank 3" (region 0xA000-0xBFFF) — a **simplified, zero-based,
  comment-only numbering scheme**. The ACTUAL CODE variable (`writeMem:100`, `auto bank =
  address >> 13;`) computes `bank ∈ {2, 3, 4, 5}` for these same four regions (`0x4000>>13=2`,
  `0x6000>>13=3`, `0x8000>>13=4`, `0xA000>>13=5` — independently re-verified this cycle), matching
  `Rom8kBBlocks`'s own 8-slot addressing convention (region *i* starts at `i*0x2000`,
  `RomBlocks.hh:53-54`). **Both describe the identical four registers; only the ordinal label
  differs.** M20's own implementation follows the CODE's numbering (2-5), matching
  `CartridgeRomWindow`'s own slot-index convention exactly (§2.1 — direct reuse), and this
  distinction is documented in source comments so no future reader is confused by the header
  comment's own simplified prose. *Verify:* unit test asserts the write-trigger addresses
  `0x4FFF/0x6FFF/0x8FFF/0xAFFF` land on window-slots `2/3/4/5` respectively (not `0/1/2/3`).
- **A-M20-4 (write-trigger bit pattern, independently re-derived).** `RomHalnote.cc:98`:
  `(address & 0x1FFF) == 0x0FFF`. Re-computed this cycle: `0x4FFF & 0x1FFF = 0x0FFF` ✓,
  `0x6FFF & 0x1FFF = 0x0FFF` ✓, `0x8FFF & 0x1FFF = 0x0FFF` ✓, `0xAFFF & 0x1FFF = 0x0FFF` ✓ — exactly
  one trigger address per applicable 8 KB region, and (separately re-checked) the two sub-bank
  trigger addresses `0x77FF`/`0x7FFF` do **not** collide with this pattern (`0x77FF & 0x1FFF = 0x17FF`,
  `0x7FFF & 0x1FFF = 0x1FFF` — neither equals `0x0FFF`), so the source's `if/else if` ordering
  (sub-bank check first, `writeMem:88`; main-bank check second, `:98`) is a style choice, not a
  disambiguation requirement — ranges are genuinely disjoint either way.
- **A-M20-5 (a genuine refinement found this cycle, NOT stated in the task brief's own summary: bit
  0x80 does DOUBLE DUTY as both an enable flag AND part of the numeric ROM-bank request for the SAME
  window slot).** `RomHalnote.cc:98-116`: on any write matching the main-bank-switch pattern,
  `setRom(bank, value)` is called **unconditionally first**, using the **raw, untouched byte**
  (including bit 7) as the requested ROM block index — **only after that** does the `bank==2`/`bank==3`
  branch separately interpret bit `0x80` as an enable flag. Concretely: writing `0x85` to the bank-2
  trigger address (`0x4FFF`) **both** (a) enables SRAM (`0x85 & 0x80 != 0`) **and** (b) sets
  window-slot 2's (CPU 0x4000-0x5FFF) visible ROM content to bank `0x85 & 0x7F = 5` (independently
  re-derived: `133` is `≥ nrBlocks(128)`, so the mask fallback engages: `133 & 127 = 5`). This is
  correct, intentional hardware behavior (not a contradiction) because window-slot 2 (0x4000-0x5FFF)
  is **never** covered by the SRAM window (SRAM only ever occupies 0x0000-0x3FFF, window-slots 0/1) —
  the two effects are simultaneous and independent. **This must be preserved exactly**: a naive port
  that "cleans up" bit 0x80 by masking it out of the value passed to the bank-resolution logic would
  silently diverge from real hardware whenever firmware writes an enable-bit-plus-bank-index byte in
  one instruction (a plausible, even likely, real firmware pattern). *Verify:* a dedicated unit test
  (§3, M20-S1) asserts BOTH effects from a single write.
- **A-M20-6 (SRAM region access re-implemented as a direct address-range branch, proven behaviourally
  identical to openMSX's pointer-indirection mechanism — a disclosed, deliberate simplification, not
  a deviation).** openMSX's read side has NO explicit `address<0x4000` branch in `readMem`/
  `getReadCacheLine` at all (`RomHalnote.cc:63-78`) — reads fall through uniformly to
  `Rom8kBBlocks::getReadCacheLine`, i.e. `bankPtr[address/0x2000][address & 0x1FFF]`; window-slots 0/1
  are made to POINT AT the SRAM buffer (`RomHalnote.cc:107-109`,
  `setBank(0,&(*sram)[0x0000],value); setBank(1,&(*sram)[0x2000],value);`) or at `unmappedRead`
  (`:111-112`) by the WRITE-side enable-toggle handler. Re-derived this cycle: for `address < 0x2000`,
  `bankPtr[0][address] == (*sram)[address]`; for `0x2000 <= address < 0x4000`,
  `bankPtr[1][address & 0x1FFF] == (*sram)[0x2000 + (address - 0x2000)] == (*sram)[address]` — i.e.
  the net effect for ANY `address < 0x4000` is simply `sram[address]` when enabled. `CartridgeRomWindow`
  (§2.1) has no mechanism to point one of its slots at an EXTERNAL byte buffer (its `image_` is an
  owned `std::vector`, sized to the ROM only) — so M20's `HalnoteRom::mem_read`/`mem_write` implement
  `address < 0x4000` as a direct, explicit branch (`sram_enabled_ ? sram_.read(address) : kOpenBus` for
  reads; `if (sram_enabled_) sram_.write(address, value);` for writes, write ignored when disabled) —
  proven above to be the IDENTICAL resulting behavior, just without the pointer-indirection detour.
  `window_`'s own slots 0/1 are consequently **never touched** by `HalnoteRom` at all (left in their
  default-constructed unmapped state permanently, harmlessly unused real estate in the reused
  primitive). *Verify:* unit test cross-checks `mem_read`/`mem_write` at addresses 0x0000, 0x1FFF,
  0x2000, 0x3FFF against direct `sram_.read/write` calls, both enabled and disabled.
- **A-M20-7 (sub-bank register writes always take effect; only READS are gated by the enable bit).**
  `RomHalnote.cc:88-97`: the `if (address == one_of(0x77FF, 0x7FFF))` branch unconditionally sets
  `subBanks[subBank] = value` (the `if (subBanks[subBank] != value)` guard around it is purely an
  openMSX internal cache-invalidation optimization — `invalidateDeviceRCache` — not a behavior gate;
  the assignment happens regardless of `subMapperEnabled`). Only the READ side
  (`getReadCacheLine:65`, `if (subMapperEnabled && ...)`) is gated. *Verify:* unit test writes to
  `0x77FF`/`0x7FFF` while `subMapperEnabled==false`, then enables the sub-mapper, and confirms the
  PREVIOUSLY-written sub-bank values are already in effect (not reset to 0 by the later enable).
- **A-M20-8 (sub-bank read formula needs no masking — independently re-verified, not assumed).**
  `RomHalnote.cc:68`: `rom[0x80000 + subBanks[subBank] * 0x800 + (address & 0x7FF)]`. `subBanks[]` is
  a raw, unmasked byte (0-255, no `setBlockMask`-style fallback for sub-banks anywhere in the file).
  Re-derived this cycle: `256 sub-banks × 0x800 (2 KB) = 0x80000 (512 KB)` exactly matches the size of
  the image's *second half* (`0x100000 - 0x80000 = 0x80000`), so **every** possible byte value
  0-255 is in-range by construction — the maximum computed offset,
  `0x80000 + 255*0x800 + 0x7FF = 0xFFFFF`, is exactly the last valid byte of a 1 MB image. No
  wraparound/mask logic is needed or present, unlike the main-bank registers.
- **A-M20-9 (device-attachment span vs. internal content mapping — the XML's `<mem
  base="0x0000" size="0x10000">` spans ALL 4 CPU pages, but window-slots 6/7 never show real content
  — both facts held simultaneously, not contradictory).** `Sony_HB-F1XV.xml:111`: `<mem base="0x0000"
  size="0x10000"/>` = `65536` bytes = exactly 4×16 KB CPU pages (the WHOLE unexpanded 64 KB address
  space of secondary slot 3, contrast the narrower `<mem base="0x4000" size="0x4000">` windows used by
  e.g. the FM-MUSIC ROM at slot 3-3, `Sony_HB-F1XV.xml:195`). This means `HalnoteRom` must be
  **attached at all 4 `SlotBus` pages** (0-3) of (primary 0, secondary 3) — nothing else can occupy
  that sub-slot's remaining pages. Independently, `RomHalnote.cc:59-60`'s `reset()`
  (`setUnmapped(6); setUnmapped(7);`) and the write-decode's own `address<0xC000` outer gate
  (`:87`) together guarantee window-slots 6/7 (CPU 0xC000-0xFFFF) are **permanently unmapped**
  (0xFF reads) for the device's entire lifetime — no main-bank-switch trigger address ever targets
  them (the only possible `bank` values from `address>>13` for `0x4000<=address<0xC000` are
  `{2,3,4,5}`), and no other write path touches them either. Both facts are simultaneously true and
  not in tension: the device *claims* the whole 64 KB window (attachment), but *never maps real
  content* into its top quarter (behavior) — exactly matching real Halnote-cartridge/firmware
  hardware, independently confirmed via `RomBlocks.hh:17-19` (`NUM_BANKS = 0x10000/BANK_SIZE = 8`,
  i.e. the class's own domain IS the full 64 KB, by design). *Verify:* integration test confirms
  0xC000-0xFFFF reads `0xFF` regardless of any bank-switch write sequence exercised elsewhere.
- **A-M20-10 (`CartridgeRomWindow` reuse — a disclosed, justified cross-family dependency, not a
  rename/relocation of M19's already-shipped code).** `src/devices/cartridge/cartridge_rom_window.{h,cpp}`
  (M19) is itself a byte-exact, QA-signed-off port of the SAME upstream base class Halnote's own
  `Rom8kBBlocks` inherits from (`references/openmsx-21.0/src/memory/RomBlocks.hh/.cc`,
  `RomBlocks<0x2000>` — read again this cycle, §"Read first" item 5/6 grounding): 8 slots of 8 KB,
  `set_bank`/`set_unmapped` implementing the exact `setRom`/`setUnmapped` mask-fallback algorithm
  (`RomBlocks.cc:107-118`, re-read this cycle), default `block_mask_ = num_blocks_-1`
  (`cartridge_rom_window.cpp:5-9`, confirmed by direct read) — **exactly** the default Halnote itself
  relies on (no `setBlockMask` override call anywhere in `RomHalnote.cc`, unlike `RomKonami.cc:24`'s
  `setBlockMask(31)`). Reusing this ALREADY-BUILT, ALREADY-TESTED (92/92 ctest, QA-verified M19)
  primitive for Halnote's main window is DRY, low-risk, and faithfully mirrors openMSX's OWN
  architecture (where `RomHalnote` and `RomKonami` are BOTH just `RomBlocks<0x2000>` subclasses — the
  primitive is not conceptually "cartridge-specific" in the upstream project either, it just happened
  to get its first consumer, and its local name, from the M19 cartridge milestone). **Recommendation
  (not a blocking decision-gate item — no hardware behavior is fabricated either way): reuse
  `devices::cartridge::CartridgeRomWindow` directly from the new `src/devices/halnote/` family,
  rather than renaming/relocating the M19 primitive** (which would touch already-shipped,
  QA-signed-off M19 files for no behavior gain — violates guardrails "Scope Control: smallest
  meaningful step"). This is a new precedent (first cross-device-family dependency within
  `src/devices/`) worth naming explicitly for the record. *Verify:* confirm `CartridgeRomWindow` is
  move-assignable (no user-declared destructor/copy operations that would suppress the implicit move
  members — confirmed by inspection this cycle, `cartridge_rom_window.h:40-91`) since `HalnoteRom::
  set_image()` needs to reconstruct it wholesale on each asset (re)load (§2.1).
- **A-M20-11 (`BatteryBackedSram` reuse — the EXACT M17 primitive, its existing API used directly, not
  reimplemented).** `src/devices/memory/battery_backed_sram.h` (read in full this cycle) already
  exposes exactly what's needed: `explicit BatteryBackedSram(std::size_t byte_count)`,
  `read(offset)`/`write(offset,value)`, `clear()`, `load(path)->bool`/`save(path)const->bool` — all
  deterministic, absent-file-safe (per its own doc comment, `battery_backed_sram.h:50-59`), already
  unit-tested standalone at exactly `0x4000` (16 KB) in M17 (`battery_backed_sram.cpp` unit test),
  matching `RomHalnote.cc:44`'s `sram = make_unique<SRAM>(..., 0x4000, ...)` exactly. `HalnoteRom`
  will own a `devices::memory::BatteryBackedSram sram_{0x4000};` member directly and expose a
  `sram()` accessor (const + mutable) for the MACHINE to call `.load()`/`.save()` on directly — no
  redundant pass-through wrapper method is added on `HalnoteRom` itself, since the primitive's own
  public API is already exactly the right shape (a deliberate difference from the M11-era
  `S1985Engine::load_backup_ram`/`save_backup_ram` hand-rolled wrappers, which pre-date this reusable
  primitive and duplicate ifstream logic that no longer needs duplicating).
- **A-M20-12 (SRAM persistence lifecycle mirrors the M15 S1985 backup-RAM precedent EXACTLY, including
  its documented reset-zeroes-then-file-reloads discipline and its NO-CLI-flag scope).**
  `src/devices/chipset/s1985_engine.cpp:8-17` (`S1985Engine::reset()`, read again this cycle):
  `"openMSX MSXS1985::reset clears color1/color2/pattern/address (not the SRAM contents, which are
  battery-backed). M11 backup RAM is volatile (A-5); we additionally zero it here so cold_boot is
  deterministic."` followed by `sram_.fill(0)`. This is an EXPLICIT, already-precedented, disclosed
  simplification in THIS codebase (not a new invention for M20): real battery-backed SRAM survives a
  reset/power-cycle in reality, but this emulator's `cold_boot()` represents a fresh, deterministic
  power-on for testing purposes, and persistence is modeled entirely through the FILE (loaded AFTER
  the zeroing reset, in `Hbf1xvMachine::cold_boot()`'s existing sequence:
  `s1985_engine_.reset()` early in the reset block, `s1985_engine_.load_backup_ram(backup_ram_path_)`
  later, right before `load_rom_assets()`). M20's `HalnoteRom::reset()` mirrors this exactly:
  `reset_bank_state()` PLUS `sram_.clear()`; the machine calls `halnote_.reset()` in the same
  reset-block position as `ym2413_.reset()` etc., then (mirroring `backup_ram_path_`'s position)
  `if (!halnote_sram_path_.empty()) halnote_.sram().load(halnote_sram_path_);` BEFORE
  `load_rom_assets()`. **Separately confirmed by direct grep of `src/main.cpp` this cycle**: neither
  `set_backup_ram_path` nor `flush_backup_ram` has ANY corresponding CLI flag today — this is a
  library-API-only, directly-testable feature in this project's own established precedent. M20
  matches this exact scope level: `set_halnote_sram_path`/`halnote_sram_path`/`flush_halnote_sram`
  machine API, no new CLI flag (§1.2). *Verify:* integration test: set a path, `cold_boot()`, write
  distinguishing SRAM bytes, `flush_halnote_sram()`, construct a FRESH `Hbf1xvMachine`, set the SAME
  path, `cold_boot()`, confirm the bytes round-trip; separately confirm an EMPTY/unset path leaves
  SRAM at deterministic zero after `cold_boot()` (mirrors the M15 "absent file → deterministic zero,
  M11 golden preserved" pattern exactly, just for a different device).
- **A-M20-13 (a demonstrated, real risk category found DURING this cycle's own grounding — NOT a
  claim requiring a fix under M20, disclosed for the record per the guardrails' anti-hallucination
  discipline).** While independently re-deriving the exact `SlotBus` primary/sub-slot arithmetic
  needed to design M20's own test/probe recipes (`src/devices/chipset/slot_bus.cpp`, read in full
  this cycle), the planner found that the EXISTING, already-shipped, QA-signed M17 integration test
  case `FmMusicRom_Slot33Page1_UnchangedByYm2413Writes`
  (`tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp:116-117`,
  `machine.debug_io_write(0xA8, 0x0C); machine.debug_bus_write(0xFFFF, 0x0C);`, commented "slot-3
  sub-slot reg: page1 field = 11 (sub 3)") appears, by direct bit-arithmetic
  (`primary_for_page(3) = (0x0C>>6)&3 = 0` when `#A8==0x0C`, so `SlotBus::write_ffff` — which targets
  `sub_slot_register_[primary_for_page(3)]`, `slot_bus.cpp:65-70` — writes into
  `sub_slot_register_[0]`, NOT `sub_slot_register_[3]`), to land the `0xFFFF` write in PRIMARY SLOT
  0's sub-register rather than slot 3's, meaning the subsequent 64-byte read may resolve to
  `(3, sub_for_page(3,1), 1)` where `sub_for_page(3,1)` is whatever `sub_slot_register_[3]`'s
  page-1 field already was (default `0` since it is otherwise never touched, per `SlotBus::
  reset()`'s `sub_slot_register_.fill(0)`) — i.e. `(primary 3, sub 0, page 1)` = `ram_mapper_`
  (RAM), not `(primary 3, sub 3, page 1)` = `fmmusic_rom_`. The test's own PASS/FAIL outcome is
  **not actually affected** either way (it only asserts "before == after" across a write sequence
  that never touches RAM, so it passes whichever device answers) — this is a **disclosed,
  non-blocking observation about a possibly-mislabeled comment in an ALREADY-CLOSED, tagged
  (v1.0.17) milestone's test, explicitly OUT of M20's scope to fix** (M20 has no mandate to modify
  M17 code, and the task brief's constraints forbid production-code changes in a planning package
  regardless). It is recorded here **only** because it directly informed how the planner derived
  M20's OWN test/probe slot-routing recipes (§2.5) with independent, from-first-principles
  arithmetic rather than by copying a prior milestone's worked example verbatim — exactly the
  discipline the task brief itself demanded ("read the code directly to confirm the exact bit
  pattern, don't just trust this summary"). *Verify (developer, M20-S3):* every hex constant used in
  M20's own tests must be accompanied by an explicit comment showing the bit-field decomposition (not
  just the resulting hex value), and at least one dedicated test case must assert the RESOLVED
  `(primary, sub, page)` triple via existing debug accessors (`slot_expanded`,
  `debug_sub_slot_register`) before relying on it in a larger read/write sequence.

---

## 2. Spec Summary

### 2.1 `src/` placement (per `src/CLAUDE.md`)

**New top-level device family: `src/devices/halnote/`** — mirrors the M18 (`src/devices/kanji/`) and
M19 (`src/devices/cartridge/`) precedent of giving a sufficiently complex new device its own family
folder, distinguishing "the one fixed, internal, Halnote-mapped firmware ROM" from the general
external-cartridge-mapper family, even though it reuses that family's window primitive (A-M20-10).
Given there is exactly ONE mapper instance here (not a family of interchangeable types like M19's six
cartridge mappers), a single class is sufficient — no `CartridgeMapperType`-style enum, no
`CartridgeSlot`-style swappable wrapper (Halnote is a fixed, always-present device, like every other
memory device in this machine, not a user-loadable cartridge).

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/halnote/halnote_rom.h` / `.cpp` (**new**) | `HalnoteRom final : public core::MemoryDevice` — the complete mapper: main 8-slot window (composes `devices::cartridge::CartridgeRomWindow`, A-M20-10), 4 main bank-switch registers (window-slots 2-5), SRAM-enable gate wired to an owned `devices::memory::BatteryBackedSram sram_{0x4000}` (A-M20-11), sub-mapper-enable gate + 2 sub-bank registers + the 0x7000-0x7FFF shadow-read override, `reset()`, `set_image()`. | `RomHalnote.{hh,cc}` (full grounding §1.3 A-M20-2..A-M20-9) |

Machine wiring (in `src/machine/`, existing files, additive only):

- `src/machine/hbf1xv_machine.h` — `+#include "devices/halnote/halnote_rom.h"`; `+devices::halnote::
  HalnoteRom halnote_;` member (placed alongside the other slot-0/slot-3 memory devices, e.g. near
  `bios_rom_`/`fmmusic_rom_`); `+std::filesystem::path halnote_sram_path_;` member (mirrors
  `backup_ram_path_`); accessors `halnote()` (const + non-const), `set_halnote_sram_path(path)`,
  `halnote_sram_path()`, `flush_halnote_sram()` (mirrors `set_backup_ram_path`/`backup_ram_path`/
  `flush_backup_ram` exactly, A-M20-12).
- `src/machine/hbf1xv_machine.cpp`:
  - `wire_bus()`: `for (int page = 0; page < devices::chipset::SlotBus::kPages; ++page) {
    slot_bus_.attach(0, 3, page, &halnote_); }` (A-M20-9: all 4 pages, primary 0 secondary 3 — the ONLY
    change to slot 0's population since M13; secondaries 0/1/2 of slot 0 are untouched).
  - `cold_boot()`: `+halnote_.reset();` in the main device-reset block (alongside `ym2413_.reset()`
    etc.); `+if (!halnote_sram_path_.empty()) { halnote_.sram().load(halnote_sram_path_); }` placed in
    the SAME relative position as the existing `backup_ram_path_` load (right before
    `load_rom_assets()`, A-M20-12); `load_rom_assets()` gains
    `halnote_.set_image(loader.load({"f1xvfirm.rom", 0x100000, "slot 0-3 pages 0-3 (Halnote/MSX-JE
    firmware)"}));` (mirrors every other ROM device's `set_image` call exactly, A-M20-1).
  - `flush_halnote_sram()`: `if (halnote_sram_path_.empty()) return false; return
    halnote_.sram().save(halnote_sram_path_);` (mirrors `flush_backup_ram()` exactly).
- `CMakeLists.txt`: add `src/devices/halnote/halnote_rom.cpp` to `sony_msx_core`'s source list.
- No new `src/core/` types required — `core::MemoryDevice` already covers this device's bus
  participation (mirrors every ROM/RAM/mapper device in this project).

Boundary compliance: `HalnoteRom` carries zero slot-routing knowledge of its own (it just answers
reads/writes against its own internal bank/SRAM state, given the raw 16-bit CPU address); the MACHINE
performs all `slot_bus_.attach(...)` composition — matching the M13-M19 precedent exactly.

### 2.2 Halnote byte-exact operational semantics (fully grounded — see §1.3 for line-cited derivations)

**Storage.** `CartridgeRomWindow window_` (1 MB image, `num_blocks()==128`, default
`block_mask()==127` — no override, A-M20-10) + `BatteryBackedSram sram_{0x4000}` (A-M20-11) +
`std::array<std::uint8_t,2> sub_banks_{}` + `bool sram_enabled_ = false;` + `bool
sub_mapper_enabled_ = false;`.

**`reset()`** (`RomHalnote.cc:48-61`, byte-exact): `sub_banks_ = {0,0}`; `sram_enabled_ = false`;
`sub_mapper_enabled_ = false`; `sram_.clear()` (A-M20-12, a disclosed simplification beyond the
literal openMSX reset, which never clears SRAM — see A-M20-12); `window_.set_unmapped(0)`;
`window_.set_unmapped(1)`; `window_.set_bank(2,0); window_.set_bank(3,0); window_.set_bank(4,0);
window_.set_bank(5,0);`; `window_.set_unmapped(6); window_.set_unmapped(7);`.

**`mem_read(address)`**:
```
if address < 0x4000:
    return sram_enabled_ ? sram_.read(address) : 0xFF        // A-M20-6
if sub_mapper_enabled_ and 0x7000 <= address < 0x8000:
    idx = (address < 0x7800) ? 0 : 1
    return window_.image()[0x80000 + sub_banks_[idx]*0x800 + (address & 0x7FF)]   // A-M20-8, no mask needed
return window_.read(address)   // banks 2-5 (0x4000-0xBFFF); slots 6/7 (0xC000-0xFFFF) permanently 0xFF (A-M20-9)
```

**`mem_write(address, value)`**:
```
if address < 0x4000:
    if sram_enabled_: sram_.write(address, value)            // A-M20-6; ignored when disabled
    return
if address >= 0xC000:
    return                                                     // A-M20-9, never a trigger target
if address == 0x77FF or address == 0x7FFF:
    sub_banks_[address == 0x77FF ? 0 : 1] = value             // A-M20-7, always takes effect
    return
if (address & 0x1FFF) == 0x0FFF:                               // A-M20-4
    bank = address >> 13                                       // 2..5, A-M20-3
    window_.set_bank(bank, value)                              // A-M20-5: full byte, incl. bit 7
    if bank == 2:
        sram_enabled_ = (value & 0x80) != 0                    // A-M20-5: simultaneous, independent effect
    elif bank == 3:
        sub_mapper_enabled_ = (value & 0x80) != 0
```

**`set_image(image)`**: `image.resize(0x100000, 0xFF)` (truncate/pad, mirrors
`RomDevice::normalize_image`, `rom_device.cpp:19-24` — a disclosed, deliberate divergence from
openMSX's hard-throw-on-wrong-size constructor, §1.3 A-M20-1/A-M20-2, appropriate because
`RomAssetLoader`'s own A-7 missing-asset policy already guarantees a correctly-sized image reaches
this device in practice); `window_ = CartridgeRomWindow(std::move(image));` (A-M20-10, movable);
re-applies the bank/flag portion of `reset()` (NOT `sram_.clear()` — SRAM is independent of the ROM
image, A-M20-6/A-M20-12) via a shared private helper.

### 2.3 SRAM wiring — concrete persistence lifecycle (A-M20-11/A-M20-12)

1. Construction: `HalnoteRom`'s default constructor builds an all-`0xFF` 1 MB placeholder `window_`
   (mirrors `bios_rom_{0x0000,0x8000}`'s own placeholder-then-later-`set_image` pattern) and a
   zero-initialized `sram_` (via `BatteryBackedSram`'s own constructor default). Already in the
   documented `reset()` bank/flag layout.
2. `cold_boot()`:
   a. `halnote_.reset()` — bank/flag state cleared, `sram_` zeroed (A-M20-12).
   b. `if (!halnote_sram_path_.empty()) halnote_.sram().load(halnote_sram_path_);` — overwrites the
      zeroed `sram_` from the configured file, if any (absent/short file → the primitive's own
      documented "leave untouched" behavior, i.e. it STAYS at the zero state from step (a) — matches
      the M15 "absent file → deterministic zero" golden pattern).
   c. `load_rom_assets()` → `halnote_.set_image(loader.load({"f1xvfirm.rom", 0x100000, ...}));` —
      replaces the ROM image and re-applies the bank/flag reset ONLY (never touches `sram_`'s just-
      loaded content, A-M20-6).
3. On-demand: `machine.flush_halnote_sram()` (mirrors `flush_backup_ram()`) writes the CURRENT
   `sram_` bytes to the configured path — callable by tests/tools at any point, never automatic/
   implicit (no host-nondeterminism, §1.2).
4. No file path configured (the default): `sram_` is deterministically zero after every
   `cold_boot()`, forever, in every run — a strict superset of the M15 "M11 golden preserved"
   discipline, applied fresh to this device.

### 2.4 Asset naming resolution (concrete answer to the flagged discrepancy)

**Resolved: this is the project's established local-asset-renaming convention, not a real mismatch.**
`hb-f1xv.rom` is openMSX's OWN filename for this ROM (`Sony_HB-F1XV.xml:108`); `bios/f1xvfirm.rom` is
this project's local name for the SAME content, following the IDENTICAL renaming pattern already
applied to all six other ROM roles (BIOS/SUB/Kanji-driver/Disk/FM-MUSIC/Kanji-font — every single one
renamed from its own distinct openMSX XML filename to a short local one, `rom_asset_loader.h`/
`hbf1xv_machine.cpp:291-296`). `docs/asset-checksums.txt:12` confirms `f1xvfirm.rom` is present today
at exactly `1,048,576` bytes (`0x100000`, matching A-M20-2's required size exactly) with a recorded
SHA256 (`c78c13996a4...`). No SHA1 cross-check against the XML's own `ade0c5ba55...` SHA1 is possible
(only SHA256 is recorded locally, mirroring the exact same limitation already disclosed for
`roms/aleste.rom` in M19) — this is a disclosed, non-blocking limitation, not a fabricated provenance
claim (guardrails "Asset and Script Safety").

### 2.5 Determinism (hard requirement)

- `HalnoteRom` is a **pure, combinational** device (like `RomDevice`/`Ym2413Opll`/every M19 cartridge
  mapper) — `mem_read`/`mem_write` are functions of stored bytes only, never `elapsed_cycles()`.
  **No new clock consumer** — lowest possible regression risk on the CPU-timing-oracle axis (mirrors
  M17 §2.4/M19 §2.5's reasoning exactly). *Verify:* the M9/M12 CPU-timing oracle suites remain green
  unmodified.
- SRAM persistence is a ONE-TIME, deterministic setup/flush action (file load at `cold_boot()`,
  explicit `flush_halnote_sram()` on demand) — never inside the CPU-stepping loop, never wall-clock
  driven (A-M20-12).
- Two-run determinism: identical write sequences on two independent `Hbf1xvMachine` instances (same
  ROM image, same or absent SRAM path) produce byte-identical bank/SRAM/sub-bank state.

### 2.6 openMSX A/B acceptance plan (real capture required; independently-derived routing math, A-M20-13)

Halnote is internally attached at **primary slot 0, secondary slot 3** — a DIFFERENT primary than
M17's slot-3-3 example, so the routing recipe is derived fresh here, not copied:

- **Debug-harness technique (unit/integration tests, no running CPU program needed)**: since primary
  slot 0 is ALREADY the `#A8` reset default for every page (`cold_boot()`'s own `ppi_.io_write(0xA8,
  0x00)`), `primary_for_page(3) == 0` already holds with NO `#A8` write at all. A single
  `debug_bus_write(0xFFFF, 0xFF)` therefore lands in `sub_slot_register_[0]` (not `[3]`, confirmed via
  A-M20-13's derivation) and sets EVERY page's field to `3` (secondary slot 3) at once, making the
  ENTIRE Halnote 64 KB address space directly reachable via plain `debug_bus_read`/`debug_bus_write`
  calls with no further slot juggling. This is the primary technique for M20-S1..S3's own tests.
- **CPU-driven technique (A/B parity probe only, mirrors the M19 §2.7 "genuine running program"
  discipline)**: because Halnote's SRAM (page 0), main banks 2/3 + sub-bank shadow (page 1), and
  banks 4/5 (page 2) each live in a DIFFERENT CPU page, and the driver program's own code/stack must
  keep running throughout, the probe:
  1. **Before** `map_flat_ram()` changes `#A8`, issue `debug_bus_write(0xFFFF, 0xFF)` (as above) — this
     is a harness-level, pre-CPU-execution setup step (mirrors `map_flat_ram()` itself being a
     harness-level convenience, not something the emulated CPU does to itself), so it carries zero
     risk of disturbing a running program.
  2. `map_flat_ram()` (all 4 pages → primary 3 RAM; independently sets `sub_slot_register_[3]`, a
     DIFFERENT array slot, harmlessly).
  3. Load the driver program + stack into **CPU page 3** (0xC000-0xFFFF) ONLY, and never remap page 3
     away from primary-3/RAM for the program's entire run (keeps its own code/stack always resolvable).
  4. The driver successively issues `OUT (#A8),A` to repoint ONE of pages {0, 1, 2} to primary 0 at a
     time (each value independently computed and comment-annotated with its bit-field decomposition,
     per A-M20-13's verification action — e.g. page 1→primary 0 with pages {0,2,3}→primary 3 is
     `(3<<6)|(3<<4)|(0<<2)|(3<<0) = 0xF3`, re-derivable, not copied from any prior milestone's
     worked example), exercises the relevant register(s) via memory reads/writes in that page, then
     restores `#A8` to all-primary-3 before moving to the next page.
  5. Distinguishing marker bytes are planted in a **synthetic** 1 MB Halnote image (via a new
     `tools/gen-m20-halnote-probe.py`, mirroring the M16/M17/M18/M19 synthetic-fixture precedent) at
     each of banks 0-5's first bytes, plus 2KB-granular markers across the sub-bank region, so every
     read-back is independently verifiable — never relying on the real firmware's actual (unknown,
     un-fabricated) content for correctness assertions.
- **Real openMSX side**: swap the synthetic image in place of the real `hb-f1xv.rom` for the WSL run.
  **Verification required before any claim** (mirrors R-M17-6/R-M19-6's "verify before claiming"
  precedent): confirm whether openMSX's `<sha1>` tag on the `<ROM>` element
  (`Sony_HB-F1XV.xml:109`) is enforced (hard rejection of a non-matching file) or advisory/
  informational only. If enforced, report this specific sub-claim honestly as **BLOCKED** and fall
  back to an architecture-only technique using the REAL firmware image (no planted markers, verifying
  only that the write/read instruction stream executes identically, no crash/hang/wait-state
  divergence) — never fabricate a result either way.
- **Subjects to cover** (unlike M17's write-only YM2413, Halnote's SRAM is fully CPU-**readable**, so
  no separate register-introspection subject is needed — simpler than M17):
  1. Main bank-switch write/read-back for banks 2-5 (including A-M20-5's double-duty effect:
     confirm a SINGLE write both flips the enable bit AND updates that slot's visible content).
  2. SRAM enable/disable + content read/write (page 0).
  3. **The sub-bank shadowing quirk specifically** (mirrors the M19 Konami-mirror-quirk rigor,
     per the task's explicit instruction): with sub-mapper DISABLED, confirm 0x7000-0x7FFF shows the
     normal bank-3 window content; enable the sub-mapper, write distinguishing sub-bank register
     values, confirm 0x7000-0x77FF and 0x7800-0x7FFF now show the SHADOWED 2 KB content while
     0x6000-0x6FFF (still governed by the SAME window-slot 3, but OUTSIDE the shadow's narrower
     0x7000-0x7FFF range, A-M20-8's read-formula boundary) is UNCHANGED — the exact boundary the task
     brief calls out as the milestone's subtlest risk.
- **Adversarial comparator self-check** (as in every prior milestone): confirm an empty-side input →
  BLOCKED and a corrupted field → DIVERGENCE.
- **Mechanics**: `tools/gen-m20-halnote-probe.py` (new); `tools/openmsx-m20-halnote-parity.ps1` (new,
  mirrors the `openmsx-m19-cartridge-parity.ps1` structure); output `docs/m20-parity-trace-diff.md`.
- **Hard rule (unchanged)**: no parity claim without a genuine captured diff; any unconfirmable
  sub-claim (e.g. the SHA1-enforcement question) is reported BLOCKED honestly, not fabricated.

---

## 3. Milestones (Slice Plan M20-S1 … S4)

Every slice runs the full evidence-gate set (§5, item 8) and must leave `ctest` green.

### M20-S1 — `HalnoteRom`: main window + bank-switch registers + real SRAM wiring
- **Goal:** `HalnoteRom` implementing the main 8-slot window (reusing `CartridgeRomWindow`, A-M20-10),
  the 4 main bank-switch registers (window-slots 2-5, byte-exact per §2.2), the SRAM-enable gate WITH
  the real `BatteryBackedSram` store wired in (0x0000-0x3FFF, A-M20-6/A-M20-11), and `reset()`. SRAM
  and the main bank-switch logic are deliberately delivered in the SAME slice (not split further) since
  they share one write-decode function and the bit-0x80 double-duty behavior (A-M20-5) — splitting
  them would create an artificial, half-built interim state.
- **Files:** `src/devices/halnote/halnote_rom.{h,cpp}` (new, partial — sub-bank mechanism deferred to
  S2); `CMakeLists.txt`.
- **Unit tests** (`tests/unit/devices/halnote/halnote_rom_unit_test.cpp`): `reset()` establishes the
  documented layout (slots 0/1 unmapped, 2-5 = bank 0, 6/7 permanently unmapped, A-M20-9); each of the
  4 main bank-switch trigger addresses lands on its correct window-slot (A-M20-3/A-M20-4); the bit-0x80
  double-duty effect on bank-2 AND bank-3 writes (A-M20-5, e.g. writing `0x85` to `0x4FFF` both enables
  SRAM and sets window-slot 2 to bank 5); SRAM read/write only when enabled, else open-bus/no-op
  (A-M20-6), cross-checked against direct `sram_.read/write` calls; `sram_enabled_`/window-slot state
  survive independently of each other; `set_image()` normalizes a wrong-sized image (truncate/pad,
  never throws) and re-establishes the bank/flag layout WITHOUT touching `sram_` content; two-run
  determinism.
- **Gates:** build + ctest green.

### M20-S2 — Sub-bank shadow mechanism
- **Goal:** the sub-mapper-enable gate (bank-3 write, bit 0x80, A-M20-3), the 2 sub-bank registers
  (`0x77FF`/`0x7FFF`, A-M20-7), and the 0x7000-0x7FFF read override (A-M20-8), layered onto S1.
- **Files:** `src/devices/halnote/halnote_rom.{h,cpp}` (extend).
- **Unit tests** (same file or a companion `halnote_subbank_unit_test.cpp`): sub-bank register writes
  always land (A-M20-7) regardless of enable state; enabling the sub-mapper makes 0x7000-0x77FF/
  0x7800-0x7FFF show the shadow content while 0x6000-0x6FFF stays governed by the normal bank-3 window
  (the exact boundary the task brief flags as this milestone's subtlest risk); disabling reverts
  0x7000-0x7FFF to the normal bank-3 window; every sub-bank byte value 0-255 resolves in-bounds with no
  masking (A-M20-8); two-run determinism.
- **Gates:** build + ctest green.

### M20-S3 — Machine wiring + SRAM persistence + system integration — **closes B4 AND B6 together**
- **Goal:** attach `HalnoteRom` at primary slot 0, secondary slot 3, all 4 pages (A-M20-9); load the
  real `bios/f1xvfirm.rom` via `RomAssetLoader` (A-M20-1); wire `set_halnote_sram_path`/
  `halnote_sram_path`/`flush_halnote_sram` (A-M20-12); confirm secondaries 0/1/2 of primary slot 0 are
  untouched (BIOS ROM regression guard).
- **Files:** `src/machine/hbf1xv_machine.{h,cpp}` (edit, additive only); `CMakeLists.txt`.
- **Integration tests** (`tests/integration/machine/hbf1xv_m20_halnote_integration_test.cpp`): a
  debug-harness-driven test (§2.6 technique 1) exercises the full main-bank + SRAM + sub-bank protocol
  over the REAL M11 system bus at the REAL slot address; the real `f1xvfirm.rom` loads (size/asset
  gate); the M13 BIOS ROM at slot 0-0 is confirmed byte-for-byte unchanged (regression guard, mirrors
  the M17 `fmmusic_rom_` guard — this time independently verified with correctly-derived routing math,
  A-M20-13); SRAM persistence round-trips across two independent `Hbf1xvMachine` instances via a
  shared file path (A-M20-12); an unset SRAM path yields deterministic zero after every `cold_boot()`;
  zero regression on M1-M19 suites (in particular the M9/M12 CPU-timing oracles and every prior
  milestone's slot-map/device-accessor goldens, untouched by construction).
- **Gates:** build + ctest green (full suite).
- **Backlog effect: this is the slice that closes BOTH B6 (Halnote mapped at the real slot, reading the
  real firmware) AND B4 (the real `BatteryBackedSram` primitive wired as its real, functioning SRAM
  consumer, with real persistence) — together, in this one slice, per the human's explicit directive.**

### M20-S4 — openMSX A/B parity + backlog finalization
- **Goal:** capture the A/B evidence (§2.6) covering main-bank-switch, SRAM, and the sub-bank shadow
  quirk specifically; verify (or honestly report BLOCKED for) the SHA1-enforcement question before
  any synthetic-image-swap claim; finalize the full deferred-backlog review (§4) in the ledger;
  refresh checksums.
- **Files:** `tools/gen-m20-halnote-probe.py` (new); `tools/openmsx-m20-halnote-parity.ps1` (new);
  `tests/parity/m20_halnote_probe.bin` + the synthetic 1 MB Halnote image; `docs/m20-parity-trace-diff.md`;
  refreshed `docs/asset-checksums.txt`.
- **Tests:** the S3 integration test serves as the deterministic golden the A/B probe mirrors.
- **Gates:** all four evidence gates **plus** the A/B gate. No parity claim without a genuine capture;
  any unconfirmable sub-claim (SHA1 enforcement) reported BLOCKED honestly.

---

## 4. Full Deferred-Backlog Review (mandatory — every row across all six registry sections, per DEC-0005 and the task's explicit instruction)

Source of truth: `agent-protocol/state/deferred-backlog.md`, read in FULL this cycle (not just B4/B6).
**Every** row — A.B1-B9, B.C1-C10, C.D1-D7, D.E1-E2, E.F1-F2, F.G1-G4 (34 rows total) — re-affirmed
below with a one-line justification.

### Section A (human-directive-tracked rows)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| B1 | PSG/YM2149 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, no M20 impact. |
| B2 | RTC/RP5C01 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, no M20 impact. |
| B3 | FM-PAC (OPLL YM2413) device internals | DONE (M17) — unchanged | Closed at M17; re-affirmed, no M20 impact (different device, slot 3-3, unrelated to slot 0-3). |
| B4 | MSX-JE 16 KB SRAM | **CLOSES this cycle (M20-S3), together with B6** | The M17-built `BatteryBackedSram` primitive is instantiated inside `HalnoteRom` and wired as its real SRAM-enable-gated store (§2.3), with deterministic load/save persistence — the real consumer DEC-0012 anticipated. Not deferred again. |
| B5 | Kanji-font I/O `#D8-DB` | DONE (M18) — unchanged | Closed at M18; re-affirmed, no M20 impact. |
| B6 | Halnote/MSX-JE firmware mapping (slot 0-3) | **CLOSES this cycle (M20-S3), together with B4** | `HalnoteRom` implemented byte-exact (§2.2) and wired at the real slot, reading the real `bios/f1xvfirm.rom` (§2.4). |
| B7 | Cartridge loading | DONE (M19) — unchanged | Closed at M19; re-affirmed, no M20 impact (external slots 1/2, unrelated to the internal slot 0-3 this milestone covers). |
| B8 | FDC drive mechanics | DONE (M16) — unchanged | Closed at M16; re-affirmed. |
| B9 | VRAM/V9958 VDP | DONE (M14) — unchanged | Closed at M14; re-affirmed. |

### Section B (other tracked deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| C1 | Exact cycle/T-state timing parity | OPEN — unchanged | M20 introduces no new clock consumer (§2.5); unrelated. |
| C2 | Z80 HALT-R increment | OPEN — unchanged | Per DEC-0004; unrelated to M20. |
| C3 | ZEXDOC/ZEXALL full sweep | OPEN — unchanged | A legally-sourced suite was found present in `references/zexall/` during M19 but remains unactioned; unrelated to M20's own scope. |
| C4 | S1985 backup-RAM `.sram` persistence | DONE (M15) — unchanged | Closed at M15; **its exact load/reset/save discipline is the direct precedent M20's SRAM lifecycle mirrors (A-M20-12)**. |
| C5 | Full boot past first device read | IN-PROGRESS (carried from M16) — unchanged | M20 adds no CPU-visible boot-path interaction the unattended BIOS auto-boot handshake would traverse (Halnote is a separate, non-boot-critical slot); the M16 residual (auto-disk-boot trigger investigation) remains untouched, per the M17/M18/M19 precedent of leaving C5 as-is unless directly worked. |
| C6 | Keyboard matrix + joystick | DONE (M15) — unchanged | Closed at M15; re-affirmed. |
| C7 | Printer + cassette | DONE (M18) — unchanged | Closed at M18; re-affirmed. |
| C8 | Sony speed-controller + hardware PAUSE + Ren-Sha Turbo | OPEN — unchanged | HB-F1XV-specific companion-chip behavior; unrelated to M20. |
| C9 | SDL3 frontend | OPEN — unchanged | Presentation layer; unrelated to M20. |
| C10 | FDC flux-level/DMK fidelity | OPEN — unchanged | Unrelated to M20 (FDC, not the Halnote mapper). |
| D1 | Pixel-accurate raster rendering | OPEN — unchanged | VDP rendering depth; unrelated to M20. |
| D2 | Sprite rendering + collision | OPEN — unchanged | Unrelated to M20. |
| D3 | VDP command engine | OPEN — unchanged | Unrelated to M20. |
| D4 | Cycle-accurate VDP access-slot/command timing | OPEN — unchanged | Unrelated to M20. |
| D5 | YJK/YAE color decode + DAC | OPEN — unchanged | Unrelated to M20. |
| D6 | Horizontal scroll/interlace/blink/superimpose | OPEN — unchanged | Unrelated to M20. |
| D7 | G6/G7 VRAM planar interleave | OPEN — unchanged | Unrelated to M20. |

### Section C (M14 VDP-depth deferrals) — already covered as D1-D7 above; no separate additional rows.

### Section D (M17 YM2413 DSP/timing deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| E1 | YM2413 FM-synthesis DSP/audio depth | OPEN — unchanged | Unrelated to M20 (audio device, not a memory mapper). |
| E2 | YM2413 register-write timing constraint | OPEN — unchanged | Unrelated to M20. |

### Section E (M18 printer/cassette depth deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| F1 | Cassette tape image-format/signal fidelity | OPEN — unchanged | Unrelated to M20. |
| F2 | Printer image/ESC-sequence rendering depth | OPEN — unchanged | Unrelated to M20. |

### Section F (M19 cartridge-mapper-depth deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| G1 | KonamiSCC mapper + embedded SCC/SCC+ sound chip | OPEN — unchanged | Unrelated to M20 (external cartridge depth, not the internal Halnote slot). |
| G2 | ROM-database/heuristic mapper-type auto-detection | OPEN — unchanged | Unrelated to M20 (Halnote's mapper type is fixed/known, never auto-detected — it is not a user-loadable cartridge). |
| G3 | Full `CartridgeSlotManager`-style runtime hot-plug | OPEN — unchanged | Unrelated to M20 (Halnote is a fixed, always-present device, never hot-plugged, like every device in this project). |
| G4 | The long tail of ~80 other specialized/vendor mapper types | OPEN — unchanged | Unrelated to M20; explicitly excludes `Halnote` already (it is this milestone, not part of that deferred tail). |

**Backlog bookkeeping note (to be executed at ledger-update time by the coordinator, not in this
planning artifact, per the exact M17/M18/M19 precedent):** on M20 closure, mark **both** B4 and B6
`DONE (M20)` in the same cycle (per the human's explicit non-negotiable requirement that they close
together); no new backlog rows are proposed this cycle (unlike M14/M17/M18/M19, which each split off a
contract-vs-depth remainder) — M20's scope (byte-exact mapper protocol + real SRAM wiring) IS the full
committed scope named by both B4 and B6; there is no disclosed, named "depth" beyond it analogous to
E1/F1/G1 (Halnote's own firmware application-level behavior is not a tracked backlog item, §1.2).

---

## 5. Acceptance Criteria (M20 exit)

1. `HalnoteRom` implemented byte-exact per §2.2, grounded in `references/openmsx-21.0/src/memory/
   RomHalnote.{hh,cc}` with concrete, independently-verified citations (including the A-M20-5
   double-duty finding) in the implementation report. *(S1, S2)*
2. The REAL M17 `BatteryBackedSram` primitive instantiated inside `HalnoteRom` as its actual 16 KB
   SRAM store, gated by the sram-enable bit into 0x0000-0x3FFF, reused via its existing public API
   (not reimplemented). *(S1)*
3. Sub-bank shadow mechanism (sub-mapper-enable + 2 sub-bank registers + the 0x7000-0x7FFF read
   override, with the 0x6000-0x6FFF boundary exactly preserved) implemented byte-exact. *(S2)*
4. Wired into the M11 `slot_bus_` at primary slot 0, secondary slot 3, all 4 pages; the real
   `bios/f1xvfirm.rom` (confirmed present, 1,048,576 bytes) loaded via `RomAssetLoader`; the M13 BIOS
   ROM at slot 0-0 confirmed byte-for-byte unchanged (regression guard). *(S3)*
5. Deterministic SRAM file persistence (`set_halnote_sram_path`/`flush_halnote_sram`) mirroring the
   M15 S1985 backup-RAM discipline exactly: absent path → deterministic zero every `cold_boot()`;
   configured path → round-trips byte-identical across independent machine instances. *(S3)*
6. **Both backlog rows B4 and B6 close together in M20-S3** — no deferral of either. *(§4, S3)*
7. Deterministic unit + integration tests cover every new behavior in §2.2-§2.3; two identical runs
   produce byte-identical bank/SRAM/sub-bank state. *(S1-S3)*
8. Real openMSX A/B evidence captured for the main-bank-switch protocol, SRAM enable/content, and the
   sub-bank shadow quirk specifically (`docs/m20-parity-trace-diff.md`); any unconfirmable sub-claim
   (e.g. SHA1-enforcement of a synthetic-image swap) is reported BLOCKED honestly rather than
   fabricated — no parity claim without a genuine capture. *(S4)*
9. **Zero regression** across M1-M19 suites, including the CPU-timing oracles (untouched by
   construction, no new clock consumer, §2.5) and every prior milestone's slot-map/device-accessor
   goldens (in particular the M13 BIOS-ROM-at-slot-0-0 guard, now exercised alongside a NEWLY populated
   sibling secondary slot for the first time). *(S3, S4)*
10. Every deferred-backlog row (all 34, across all six registry sections) re-affirmed with
    justification (§4); B4 and B6 close together; no new backlog row is required (§4's closing note).
11. Evidence gates executed and captured each cycle (validate-assets, checksum, Debug build, ctest).
12. QA sign-off recorded before closure (`docs/m20-qa-signoff.md`). **Normal human-release-decision
    gate — no auto-close** (per the task brief and DEC-0003's auto-close grant being M12-only);
    autopilot pauses at QA sign-off for the separate human release decision + tag.

---

## 6. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|---------------------|
| R-M20-1 | The bit-0x80 double-duty effect (A-M20-5) is "cleaned up" by masking it out of the value passed to `window_.set_bank()`, silently diverging from real hardware whenever firmware writes an enable-bit-plus-bank-index byte in one instruction. | S1 unit test explicitly asserts BOTH simultaneous effects from a single write to the bank-2 (and bank-3) trigger address, per §2.2/A-M20-5's exact worked example. |
| R-M20-2 | The sub-bank shadow's read-range boundary is over-extended to the whole bank-3 window (0x6000-0x7FFF) instead of being narrowly scoped to 0x7000-0x7FFF only — the task's own flagged "single most subtle detail." | S2 unit test explicitly asserts 0x6000-0x6FFF is NEVER shadowed (always shows the normal bank-3 window) while ONLY 0x7000-0x7FFF is shadowed when enabled; the A/B probe (S4) exercises this boundary specifically with distinguishing markers on both sides. |
| R-M20-3 | Sub-bank register writes are incorrectly gated on `sub_mapper_enabled_` (i.e., writes to 0x77FF/0x7FFF are silently dropped when the sub-mapper is disabled), diverging from A-M20-7's "writes always take effect, only reads are gated" finding. | S2 unit test writes sub-bank values while disabled, then enables and confirms the previously-written values are already in effect (not reset). |
| R-M20-4 | `CartridgeRomWindow`'s default block-mask (127, matching Halnote's own 128-bank default) is accidentally overridden with Konami's `31` via copy-paste from the M19 family. | S1 unit test asserts `window_.num_blocks() == 128` and `window_.block_mask() == 127` after construction with the real/synthetic 1 MB image, with NO `set_block_mask` call anywhere in `HalnoteRom`. |
| R-M20-5 | `HalnoteRom::reset()` or `set_image()` accidentally clears/corrupts `sram_` content when it shouldn't (SRAM is independent of both bank/flag state and the ROM image, A-M20-6/A-M20-12). | S1/S3 tests explicitly assert `sram_` content survives a `set_image()` call untouched, and that `reset()`'s clearing of `sram_` is the ONLY place it happens (never inside `mem_read`/`mem_write`/`set_image`). |
| R-M20-6 | The real, demonstrated risk category of slot-routing bit-arithmetic transcription slips (A-M20-13) recurs in M20's OWN tests/probe, producing a test that "passes" without actually exercising the intended device/address. | Every hex constant used in M20's tests is accompanied by an explicit bit-field-decomposition comment (not just the resulting value); at least one dedicated test asserts the RESOLVED `(primary, sub, page)` routing via existing debug accessors before relying on it in a larger sequence (§2.6, A-M20-13's verification action). |
| R-M20-7 | The cross-family reuse of `devices::cartridge::CartridgeRomWindow` from `src/devices/halnote/` is judged, on review, an inappropriate coupling (naming/ownership concern) even though no hardware behavior is fabricated by it. | A-M20-10 documents the justification in full (both classes ground the identical upstream `RomBlocks<0x2000>`); flagged explicitly for coordinator/human visibility at review — NOT treated as silently decided, per the DEC-0012 precedent of surfacing non-obvious architectural calls even when not strictly decision-gated. |
| R-M20-8 | The 1 MB asset-size validation path (`set_image`'s truncate/0xFF-pad, A-M20-1/A-M20-2) is never actually exercised/tested because the real `f1xvfirm.rom` is always correctly sized in this environment, leaving a latent, unverified code path. | S1 unit test deliberately constructs `HalnoteRom` with a wrong-sized (too short AND too long) synthetic image and asserts the graceful truncate/pad behavior, independent of whatever the real asset's current state is. |
| R-M20-9 | Window-slots 6/7 (CPU 0xC000-0xFFFF) become reachable/switchable through some overlooked write-address path, fabricating hardware behavior this machine's real Halnote silicon does not have (A-M20-9). | S1/S3 tests write to EVERY address in the full 0x4000-0xFFFF range with varying values and assert slots 6/7 NEVER leave their permanently-unmapped (0xFF) state — a direct, cheap regression guard mirroring the M19 Konami-fixed-slot risk category. |
| R-M20-10 | Scope creep into modeling the real MSX-JE firmware's own Japanese-input application behavior (tempting given the rich Kanji-related context already built in M18), inflating M20 beyond the mapper-contract boundary this package sets. | §1.2 explicitly fences this out — it is not a tracked backlog item and has no acceptance criterion; any expansion requires a decisions-ledger entry per guardrails "Scope Control". |

---

## 7. Developer Handoff

- **Start at M20-S1** (main window + bank-switch registers + real SRAM wiring) — grounded per
  A-M20-2..A-M20-6; cite `references/openmsx-21.0/src/memory/RomHalnote.cc` line ranges in code
  comments (never copy the code itself — GPL isolation).
- **Sequence S1→S4** in order; each runs the full evidence-gate set; keep `ctest` green every cycle.
- **`src/` placement fixed by §2.1:** new top-level `src/devices/halnote/` (`halnote_rom.{h,cpp}`,
  reusing `devices::cartridge::CartridgeRomWindow` directly and `devices::memory::BatteryBackedSram`
  directly — both REUSED, neither reimplemented, per the human's explicit instruction); machine wiring
  only in `src/machine/hbf1xv_machine.{h,cpp}` (additive: one new member, a 4-page attach loop in
  `wire_bus()`, one `reset()` call + one SRAM-path load line + one `set_image()` call in `cold_boot()`,
  four new accessor methods).
- **Critical architectural findings to honor:**
  - The bit-0x80 double-duty effect (A-M20-5) — a SINGLE write to the bank-2/bank-3 trigger address
    BOTH toggles an enable flag AND updates that same window-slot's visible ROM bank. Do not mask bit
    7 out before passing the value to `window_.set_bank()`.
  - The sub-bank shadow ONLY covers 0x7000-0x7FFF, never 0x6000-0x6FFF (R-M20-2, the task's own
    flagged subtlest risk) — get this boundary exactly right.
  - SRAM access is a direct address-range branch (A-M20-6), NOT a pointer-indirection into
    `window_`'s own slots 0/1 (which stay permanently unused) — this is a disclosed, proven-equivalent
    simplification, not a shortcut that changes behavior.
  - `reset()` clears `sram_` content (A-M20-12, mirrors the EXISTING `S1985Engine::reset()` precedent
    exactly, `s1985_engine.cpp:8-17`) — `set_image()` must NOT.
  - Window-slots 6/7 (0xC000-0xFFFF) are permanently unmapped forever — no write path may ever reach
    them (A-M20-9/R-M20-9).
- **No new clock consumer (§2.5):** `HalnoteRom` needs no `elapsed_cycles()` adapter — simpler than
  the M15/M16/M18-cassette X-pattern devices, matching M13/M17/M19's memory-device classification.
- **No new CLI flag** for the SRAM path (§1.2/A-M20-12) — mirrors the S1985 backup-RAM precedent's
  exact, confirmed (grep-verified) scope: a directly-testable machine API only.
- **Slot-routing arithmetic (A-M20-13):** derive every `#A8`/`#FFFF` test constant fresh, from
  `SlotBus`'s own documented formulas (`primary_for_page`, `sub_for_page`, `write_ffff` target
  selection) — do NOT copy a prior milestone's worked hex example without independently re-deriving
  it; a real, demonstrated transcription-risk category was found in this exact codebase during this
  cycle's grounding (disclosed, non-blocking, not to be fixed under M20).
- **A/B (§2.6):** build `tools/gen-m20-halnote-probe.py` + `tools/openmsx-m20-halnote-parity.ps1`;
  verify whether openMSX's `<sha1>` ROM-identity check is enforced or advisory BEFORE claiming any
  synthetic-image-swap-based parity result; report BLOCKED honestly for any unconfirmable sub-claim.
- **Ledger discipline (DEC-0005):** on closure, mark BOTH B4 and B6 `DONE (M20)` in the same cycle; no
  new backlog rows are required this cycle (§4); update `state/current-phase.md` and
  `state/milestones.md`.
- **Gate:** NORMAL human-release-decision gate — do not auto-close; pause at QA sign-off for the
  separate human release decision + tag (per the task brief and DEC-0003's M12-only auto-close scope).
- Deliverables: source per §2.1; tests per §3; `docs/m20-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`; an implementation report `docs/m20-implementation-report.md`; then hand
  to QA for `docs/m20-qa-signoff.md`.
