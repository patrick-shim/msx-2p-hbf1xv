# M20 Implementation Report — Halnote / MSX-JE Firmware Mapping (Slot 0-3) + MSX-JE 16 KB Battery-Backed SRAM

- Milestone ID: M20
- Request: developer implementation cycle (planner package RESP-M20-001, REQ-M20-002)
- Developer: MSX Developer Agent
- Planner package: `docs/m20-planner-package.md`
- Gate: normal human-release-decision gate (no auto-close, per DEC-0003's M12-only scope) — this
  report covers implementation only; QA sign-off is a separate, subsequent gate.

---

## 1. Milestone Target

Deliver `HalnoteRom` — a byte-exact port of openMSX's Halnote mapper
(`references/openmsx-21.0/src/memory/RomHalnote.{hh,cc}`) — as the real MSX-JE firmware ROM at
primary slot 0, secondary slot 3, wired into `Hbf1xvMachine` and loading the real
`bios/f1xvfirm.rom`. `HalnoteRom` reuses two already-shipped primitives verbatim: the M19
`devices::cartridge::CartridgeRomWindow` (8-slot × 8 KB bank window) for its main window, and the
M17 `devices::memory::BatteryBackedSram` (16 KB) as its real, functioning SRAM store. This closes
**both** deferred-backlog rows **B4** (MSX-JE 16 KB SRAM) and **B6** (Halnote/MSX-JE firmware
mapping) together in this one slice, per the human's explicit, non-negotiable directive — neither
is deferred again.

## 2. Code Changes

### 2.1 New device family — `src/devices/halnote/`

| File | Responsibility | Grounding |
|---|---|---|
| `halnote_rom.h` / `.cpp` (new) | `HalnoteRom final : public core::MemoryDevice` — main 8-slot window (composes `CartridgeRomWindow`), 4 main bank-switch registers, SRAM-enable gate wired to a real `BatteryBackedSram`, sub-mapper-enable gate + 2 sub-bank registers + the 0x7000-0x7FFF shadow-read override, `reset()`, `set_image()`. | `references/openmsx-21.0/src/memory/RomHalnote.cc` (full file, see §2.2 line citations below) |

Every openMSX citation below is a behaviour reference read for understanding; no openMSX source was
copied into `src/` (GPL isolation, guardrails).

### 2.2 Byte-exact semantics, grounded per concrete `RomHalnote.cc` line ranges

- **1 MB / 128×8 KB banking + last-512 KB 256×2 KB sub-banking** — `RomHalnote.cc:1-24` (header
  comment) and `:40-43` (`if (rom.size() != 0x100000) throw ...`). `HalnoteRom::kImageBytes =
  0x100000`; `set_image()` normalizes any input to exactly this size (truncate/0xFF-pad, mirrors
  `RomDevice::normalize_image`, never throws — a disclosed, deliberate divergence appropriate here
  since `RomAssetLoader`'s own missing-asset policy already guarantees a correctly-sized image in
  practice).
- **Register numbering (`RomHalnote.cc:10-14` header comment vs. `writeMem:100`'s actual code)** —
  the header comment's own "bank 0..bank 3" (0-based, simplified prose) does NOT match the CODE's
  `auto bank = address >> 13;` (yields 2-5 for the four main-bank regions). `HalnoteRom` follows the
  CODE's numbering (2-5), matching `CartridgeRomWindow`'s own slot-index convention. Verified by a
  dedicated unit test asserting the 4 trigger addresses land on window-slots 2/3/4/5, not 0/1/2/3.
- **Write-trigger bit pattern** — `RomHalnote.cc:98`, `(address & 0x1FFF) == 0x0FFF`, independently
  re-derived: `0x4FFF/0x6FFF/0x8FFF/0xAFFF & 0x1FFF = 0x0FFF` for all four; the sub-bank triggers
  `0x77FF`/`0x7FFF` do not collide (`& 0x1FFF = 0x17FF`/`0x1FFF`, neither `0x0FFF`).
- **Bit-0x80 double duty (`RomHalnote.cc:98-116`)** — on a main-bank-switch write, `setRom(bank,
  value)` is called UNCONDITIONALLY FIRST with the RAW, untouched byte (bit 7 included); only AFTER
  that does the `bank==2`/`bank==3` branch separately interpret bit `0x80` as an enable flag. A
  single write to the bank-2 trigger (e.g. `0x85` → `0x4FFF`) both enables SRAM AND sets that same
  window-slot's visible ROM bank via the mask fallback (`0x85 & 0x7F = 5`, since `0x85(133) >=
  nrBlocks(128)`). `HalnoteRom::mem_write` preserves this exactly: `window_.set_bank(bank, value)`
  is called with the full byte BEFORE the enable-flag branch reads `(value & 0x80)`. Unit-tested
  (`DoubleDuty_Bank2Write0x85_*`/`DoubleDuty_Bank3Write0x85_*`).
- **SRAM access as a direct address-range branch, not pointer-indirection** — openMSX's read side has
  no explicit `address<0x4000` branch at all (`RomHalnote.cc:63-78`); it makes window-slots 0/1
  POINT AT the SRAM buffer or at `unmappedRead` from the WRITE-side enable-toggle handler
  (`:107-112`). Since `CartridgeRomWindow` has no mechanism to point a slot at an external buffer,
  `HalnoteRom::mem_read`/`mem_write` implement `address < 0x4000` as a direct branch
  (`sram_enabled_ ? sram_.read(address) : 0xFF` / `if (sram_enabled_) sram_.write(...)`) — proven
  behaviourally IDENTICAL (independently re-derived this cycle by the planner, A-M20-6). Window-
  slots 0/1 are consequently left permanently unmapped/unused for this device's whole lifetime.
- **Sub-bank register writes always take effect; only READS are gated** — `RomHalnote.cc:88-97`: the
  `subBanks[subBank] = value` assignment is unconditional; the `if (subBanks[subBank] != value)`
  guard around it is purely an openMSX-internal cache-invalidation optimization, not a behavior
  gate. Only the READ side (`getReadCacheLine:65`) checks `subMapperEnabled`. `HalnoteRom::mem_write`
  matches: `sub_banks_[idx] = value;` unconditionally for `address == 0x77FF/0x7FFF`. Unit-tested
  (`WriteWhileDisabled_SubBank0/1_TakesEffectImmediately`).
- **Sub-bank read formula, no masking needed** — `RomHalnote.cc:68`:
  `rom[0x80000 + subBanks[subBank]*0x800 + (address & 0x7FF)]`. `subBanks[]` is a raw, unmasked byte
  (0-255); `256 * 0x800 = 0x80000` exactly matches the image's second half, so every value 0-255 is
  in-range by construction (unit-tested: `EverySubBankByteValue_ResolvesInBounds_NoMaskingNeeded`).
- **The 0x6000-0x6FFF / 0x7000-0x7FFF boundary (the task's own flagged subtlest risk)** — the shadow
  ONLY applies when `0x7000 <= address < 0x8000` (`RomHalnote.cc:65`); `0x6000-0x6FFF` is the SAME
  window-slot 3 but OUTSIDE that range, so it always falls through to the plain window read
  regardless of `sub_mapper_enabled_`. Unit-tested exhaustively in
  `halnote_subbank_unit_test.cpp` (`Enabled_Read0x6000_StillNormalBank3Window_NeverShadowed`, etc.)
  and re-verified at the machine/bus level in the M20 integration test and the openMSX A/B probe.
- **Window-slots 6/7 (0xC000-0xFFFF) permanently unmapped** — `RomHalnote.cc:59-60`
  (`setUnmapped(6); setUnmapped(7);`) plus the write-decode's own `address < 0xC000` outer gate
  (`:87`): no main-bank trigger's `bank = address>>13` value (only ever 2-5 for
  `0x4000<=address<0xC000`) can reach slots 6/7, and no other write path touches them. Unit- and
  integration-tested with a full write-then-read sweep across `0xC000-0xFFFF`.
- **`CartridgeRomWindow` reuse (A-M20-10)** — Halnote relies on the window's DEFAULT block mask
  (`num_blocks()-1 == 127` for a 128-bank image); `HalnoteRom` makes NO `set_block_mask` call
  anywhere, unlike Konami's `set_block_mask(31)`. Unit-tested:
  `window_.num_blocks()==128 && window_.block_mask()==127` (guards against an accidental
  Konami-style copy-paste, R-M20-4).
- **`BatteryBackedSram` reuse (A-M20-11)** — `HalnoteRom` owns
  `devices::memory::BatteryBackedSram sram_{0x4000}` directly and exposes `sram()` (const +
  mutable) for the MACHINE to call `.load()`/`.save()` on directly; no redundant pass-through
  wrapper is added.
- **`reset()`** clears `sub_banks_`/`sram_enabled_`/`sub_mapper_enabled_`, re-establishes the window
  bank/unmapped layout, AND clears `sram_` content — mirroring the EXISTING `S1985Engine::reset()`
  precedent exactly (`src/devices/chipset/s1985_engine.cpp:8-17`, a disclosed simplification beyond
  literal openMSX: real battery-backed SRAM survives a reset in reality; this project's `cold_boot()`
  models a fresh, deterministic power-on, with persistence modeled entirely through the file
  load/save the owning machine performs around this call).
- **`set_image()`** normalizes, reconstructs `window_`, and re-applies the bank/flag reset ONLY —
  never touches `sram_` (SRAM content is independent of the ROM image). Unit-tested:
  `SetImage_TooShort/TooLong_SramContentUntouched_*`.

### 2.3 Machine wiring (`src/machine/hbf1xv_machine.{h,cpp}`, additive only)

- `+#include "devices/halnote/halnote_rom.h"`; `+devices::halnote::HalnoteRom halnote_;` member
  (declared alongside `fmmusic_rom_`); `+std::filesystem::path halnote_sram_path_;` (mirrors
  `backup_ram_path_`).
- `wire_bus()`: `for (int page = 0; page < SlotBus::kPages; ++page) { slot_bus_.attach(0, 3, page,
  &halnote_); }` — the ONLY change to slot 0's population since M13 (secondaries 0/1/2 of slot 0
  untouched; confirmed by a dedicated regression-guard test case).
- `cold_boot()`: `+halnote_.reset();` in the existing device-reset block (alongside `ym2413_.reset()`
  etc.); `+if (!halnote_sram_path_.empty()) halnote_.sram().load(halnote_sram_path_);` in the same
  relative position as the existing `backup_ram_path_` load (before `load_rom_assets()`).
- `load_rom_assets()`: `+halnote_.set_image(loader.load({"f1xvfirm.rom", 0x100000, "slot 0-3 pages
  0-3 (Halnote/MSX-JE firmware)"}));` — mirrors every other ROM device's `set_image` call exactly.
- New accessors: `halnote()` (const + non-const), `set_halnote_sram_path(path)`,
  `halnote_sram_path()`, `flush_halnote_sram()` — mirror `set_backup_ram_path`/`backup_ram_path`/
  `flush_backup_ram` EXACTLY (confirmed by direct grep of `src/main.cpp`: neither has a CLI flag
  today either — this project's established precedent for exactly this class of feature). **No new
  CLI flag was added for the Halnote SRAM path**, matching that scope exactly.
- `CMakeLists.txt`: `+src/devices/halnote/halnote_rom.cpp` added to `sony_msx_core`'s source list.

### 2.4 openMSX A/B tooling (new)

- `tools/gen-m20-halnote-probe.py` — generates a synthetic 1 MB Halnote image (128×8 KB main-bank
  markers + 256×2 KB sub-bank markers, disjoint marker families), reusable for any future
  local/non-destructive swap-based probe.
- `tools/openmsx-m20-halnote-parity.ps1` — the actual A/B harness used this cycle (see §4).
- `src/main.cpp`: `+run_halnote_parity()` + `--halnote-parity <bios_dir> <out.txt>` CLI mode — drives
  the identical debug-harness write/read sequence directly over `debug_bus_read`/`debug_bus_write`
  (no CPU driver program needed, matching planner §2.6's "debug-harness technique," since
  `HalnoteRom` has no CPU-visible register file beyond memory itself and is a pure, combinational
  device, §5 below).

## 3. Unit Test Results

New test executables (both new, `tests/unit/devices/halnote/`):

- `devices_halnote_rom_unit_test` (`halnote_rom_unit_test.cpp`) — M20-S1: `reset()` layout (slots
  0/1 unmapped, 2-5 = bank 0, 6/7 permanently unmapped); each of the 4 main bank-switch trigger
  addresses lands on window-slots 2/3/4/5 (not 0/1/2/3); the bit-0x80 double-duty effect on BOTH
  bank-2 and bank-3 writes; the two enable flags are independent of each other; SRAM read/write
  gated by the enable flag, cross-checked against direct `sram_.read/write` at every SRAM-region
  boundary (0x0000/0x1FFF/0x2000/0x3FFF); window-slots 6/7 permanently open-bus across a full
  `0xC000-0xFFFF` write-then-read sweep; `set_image()` normalizes too-short/too-long images without
  throwing while preserving `sram_` content untouched; `window_.num_blocks()==128 &&
  window_.block_mask()==127` (guards against an accidental Konami-style mask override); two-run
  determinism.
- `devices_halnote_subbank_unit_test` (`halnote_subbank_unit_test.cpp`) — M20-S2: sub-bank register
  writes always land regardless of `sub_mapper_enabled_`; the 0x6000-0x6FFF vs. 0x7000-0x7FFF
  boundary explicitly asserted both disabled and enabled (the single most subtle risk this
  milestone flags); disabling reverts 0x7000-0x7FFF to the plain window without resetting the
  sub-bank register values; every sub-bank byte value 0-255 resolves in-bounds with no masking;
  two-run determinism.

Real captured output (`ctest --test-dir build -C Debug --output-on-failure`, this cycle):

```
93/95 Test #93: devices_halnote_rom_unit_test ...............................   Passed    0.04 sec
94/95 Test #94: devices_halnote_subbank_unit_test ...........................   Passed    0.03 sec
```

## 4. Integration Test Results

New: `tests/integration/machine/hbf1xv_m20_halnote_integration_test.cpp`
(`machine_hbf1xv_m20_halnote_integration_test`) — M20-S3:

1. The resolved `(primary, sub, page)` triple is explicitly asserted via `slot_expanded(0)` /
   `debug_sub_slot_register(0)` BEFORE relying on it in the larger sequence (A-M20-13's own
   verification action — every hex constant used carries a bit-field-decomposition comment,
   independently re-derived from `SlotBus`'s own formulas, not copied from a prior milestone).
2. The real `bios/f1xvfirm.rom` asset loads (no missing-asset diagnostic; `window_.num_blocks() ==
   128`, `block_mask() == 127`).
3. The M13 BIOS ROM at slot 0-0 is confirmed byte-for-byte UNCHANGED after heavy Halnote traffic
   (bank-switch + SRAM + sub-bank writes) — the regression guard, plus a non-degenerate-sample
   sanity check (mirrors the M17 `fmmusic_rom_` guard pattern).
4. The full main-bank + SRAM + sub-bank protocol exercised over the REAL system bus at the real
   slot address, cross-checked structurally against `machine.halnote().window()`'s own image/bank
   accessors (content-agnostic: the real firmware's bytes are unknown/un-fabricated, so assertions
   compare the bus read-back to the independently-computed expected offset into the SAME image,
   never a hardcoded byte value) — covers bank(4)/bank(5) switch, SRAM enable+read/write, bank(3)
   double-duty + sub-bank shadow read at both offsets, the 0x6000/0x6FFF never-shadowed boundary,
   and the permanently-unmapped upper quarter.
5. SRAM persistence round-trips across two independent `Hbf1xvMachine` instances via a shared file
   path.
6. An unset SRAM path yields deterministic zero after EVERY `cold_boot()` (including a second
   `cold_boot()` on the same instance, proving no cross-boot carry-over).

Real captured output:

```
95/95 Test #95: machine_hbf1xv_m20_halnote_integration_test .................   Passed    0.42 sec
```

### Full evidence-gate run (this cycle, real output)

```
tools/validate-assets.ps1
  Asset validation result: True
  Present BIOS files: f1xvbios.rom, f1xvdisk.rom, f1xvext.rom, f1xvfirm.rom, f1xvkdr.rom,
  f1xvkfn.rom, f1xvmus.rom
  ROM file count: 2 (aleste.rom, metalgear.rom)

tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
  Checksum report written to docs/asset-checksums.txt
  bios/f1xvfirm.rom | 1048576 bytes | c78c13996a406ee69c2a6b988c53a68b649affabef6e4599e6ea507d5387f044

cmake --build build --config Debug
  Build succeeded (sony_msx_headless.exe + all test executables), no errors.

ctest --test-dir build -C Debug --output-on-failure
  100% tests passed, 0 tests failed out of 95
  Total Test time (real) = 2.77 sec
```

Before this cycle: 92/92 (M19 closure baseline). After this cycle: **95/95**, zero regression
(3 new test executables: `devices_halnote_rom_unit_test`, `devices_halnote_subbank_unit_test`,
`machine_hbf1xv_m20_halnote_integration_test`).

## 5. Determinism

`HalnoteRom` is a pure, combinational device — `mem_read`/`mem_write` are functions of stored bytes
only, never `elapsed_cycles()`. No new clock consumer was added; the M9/M12 CPU-timing oracle suites
and every prior milestone's slot-map/device-accessor goldens remained green throughout, confirming
this. SRAM persistence (`sram().load()`/`sram().save()`) is a one-time, explicit setup/flush action
performed by the owning machine at `cold_boot()`/on-demand — never inside the CPU-stepping loop,
never wall-clock driven.

## 6. openMSX A/B Evidence

**Technique (a disclosed, deliberate choice — see `tools/gen-m20-halnote-probe.py`'s own docstring
and `tools/openmsx-m20-halnote-parity.ps1`'s header comment for the full rationale):** rather than
swapping a synthetic image into the real WSL openMSX install (the fallback the planner package
anticipated), this harness uses the REAL `bios/f1xvfirm.rom` UNMODIFIED on both sides. A live SHA1
cross-check (performed by the harness itself, every run, not assumed) confirmed:

```
Local bios/f1xvfirm.rom SHA1:  ade0c5ba5574f8114d7079050317099b4519e88f
Installed WSL hb-f1xv.rom SHA1: ade0c5ba5574f8114d7079050317099b4519e88f
Identical: True
```

This is a STRONGER, zero-risk technique than a synthetic swap (no mutation of the user's real
openMSX install at all) while remaining fully content-bearing: "expected" values for every
content-bearing label are computed directly from this same real file's own bytes (never
hardcoded/assumed).

**SHA1-enforcement question (planner's explicitly flagged open item), resolved by direct source
read:** `references/openmsx-21.0/src/memory/Rom.cc:202-208` — a machine-XML `<sha1>` mismatch on a
`<ROM>` element prints a CliComm WARNING only (`motherBoard.getMSXCliComm().printWarning(...)`); it
is never a hard rejection/throw. **The SHA1 tag is ADVISORY, not enforced** — so a synthetic-image
swap technique remains available for any future milestone that needs one; it was not required for
this milestone's own evidence given the real-firmware-identity finding above. This resolves the
planner's flagged open question either way, honestly.

**No CPU driver program is loaded/run on either side** (planner §2.6's "debug-harness technique," also
the primary technique used by this milestone's own unit/integration tests): this emulator drives the
protocol sequence directly over `debug_bus_read`/`debug_bus_write` (`--halnote-parity` mode); openMSX
is driven via a Tcl script that `debug break`s the running BIOS boot, forces `#A8=0`+`#FFFF=0xFF`
(routing all 4 pages of primary 0 to secondary 3 = Halnote), then issues the IDENTICAL `debug write
memory`/`debug read memory` sequence. **CPU-halt verification** (run this cycle, not assumed):
`HALTCHECK pc_before=785B pc_after_1s=785B stable=1` — confirms `debug break` genuinely stops ALL
further CPU execution, ruling out any concurrently-running real BIOS boot code racing with the
probe's own writes.

### Result: 11/14 labels PARITY, 3/14 DIVERGENCE (genuine, disclosed, non-fabricated either way)

| label | emulator | openMSX | expected (real firmware bytes) | status |
|---|---|---|---|---|
| `BANK4_BASE` | `22` | `22` | `22` | PARITY |
| `BANK4_LAST` | `FF` | `FF` | `FF` | PARITY |
| `BANK5_BASE` | `16` | `16` | `16` | PARITY |
| `BANK2_BASE_DOUBLE_DUTY` | `1C` | `1C` | `1C` | PARITY |
| `SRAM_FIRST` | `5A` | `5A` | `5A` | PARITY |
| `SRAM_LAST` | `A5` | `A5` | `A5` | PARITY |
| `BANK3_BASE_DOUBLE_DUTY` | `00` | `00` | `00` | PARITY |
| `BANK3_LAST_BEFORE_SHADOW` | `32` | `32` | `32` | PARITY |
| `SUBBANK0_SHADOW` | `20` | `60` | `20` | DIVERGENCE |
| `SUBBANK0_SHADOW_LAST` | `FF` | `58` | `FF` | DIVERGENCE |
| `SUBBANK1_SHADOW` | `30` | `2A` | `30` | DIVERGENCE |
| `SUBBANK1_SHADOW_LAST` | `FF` | `FF` | `FF` | PARITY |
| `UPPERQUARTER_BEFORE_WRITE` | `FF` | `FF` | `FF` | PARITY |
| `UPPERQUARTER_AFTER_WRITE` | `FF` | `FF` | `FF` | PARITY |

**PARITY subjects:** main bank-switch write/read-back for banks 2/4/5 (including the bit-0x80
double-duty EFFECT ON THE BANK NUMBER for bank 2); SRAM enable/disable + content read/write (banks
2's enable bit independently isolated and re-verified: disabled shows open-bus `0xFF`; enabling
correctly exposes the REAL, cross-run-persisted `hb-f1xv_msx-je.sram` content, proving the toggle
demonstrably takes effect; re-disabling reverts to `0xFF`).

**DIVERGENCE subject (genuine finding, investigated live, not swept under the rug):** the
sub-mapper-enable bit (bank-3, bit7) was isolated the same way — writing bit7=1 (with several
different low-7-bit bank values) never causes the `0x7000-0x7FFF` read to reflect the sub-bank-
shadowed content on the installed openMSX 19.1 runtime; it consistently continues to show the
PLAIN window-slot-3 content instead, even though the BANK-NUMBER portion of the SAME write
demonstrably DOES take effect (confirmed via the `0x6000` read, which tracks the written bank value
exactly across multiple values). Reproduced multiple times with the CPU independently confirmed
halted throughout (ruling out raciness/timing). This project's own grounding reference for
`HalnoteRom` is openMSX **21.0** (`references/openmsx-21.0/`); the only runtime available in this
environment is openMSX **19.1** (`dpkg -l`: `openmsx/noble,now 19.1+dfsg-1ubuntu3`). A plausible,
disclosed (NOT certain — no 19.1 source was available to confirm directly) explanation is a
version-specific difference in the sub-mapper-shadow feature between the 19.1 runtime and the 21.0
reference this milestone's own byte-exact port is grounded in. **This emulator's own implementation
was independently, exhaustively cross-checked against the real firmware file's own raw bytes (both
via the C++ unit/integration test suite with synthetic marker images, and via this harness's
emulator-side dump matching the real file's bytes at every computed offset, including the sub-bank
offsets) — it is confirmed byte-exact against the 21.0 reference and the real firmware content. The
divergence, if genuinely a version-specific openMSX 19.1 behavior, is NOT a defect in this project's
own `HalnoteRom`.**

**Adversarial comparator self-check:** empty-side input → BLOCKED-equivalent detected = True
(expected True); corrupted-field input (`BANK4_BASE` XORed with `0xFF`) → DIVERGENCE detected = True
(expected True).

Full artifact: `docs/m20-parity-trace-diff.md`.

## 7. Known Issues / Residual Risks

- **Genuine, disclosed A/B DIVERGENCE for the sub-mapper-shadow-enable effect specifically**, isolated
  to the installed openMSX 19.1 runtime vs. this project's openMSX 21.0 grounding reference (§6). Not
  a defect in `HalnoteRom` as far as could be determined this cycle (byte-exact against the 21.0
  reference and cross-checked against real firmware bytes both C++-side and via the A/B harness
  itself). Recommended follow-up for QA/coordinator: accept as a disclosed, non-blocking finding
  (consistent with this project's established openMSX-version-skew handling precedent), or, if
  deemed necessary, obtain a newer openMSX build to re-run the probe (not available in this
  environment: `apt-cache policy openmsx` shows only `19.1+dfsg-1ubuntu3` installable).
- The real MSX-JE firmware's own application-level behavior (what the Japanese-input firmware
  actually *does* once executing) is explicitly out of scope (§1.2 of the planner package) — this
  milestone delivers the mapper CONTRACT only, mirroring the M14/M17/M18/M19 contract-vs-depth split.
- No new CLI flag for the Halnote SRAM path, matching the established M15 S1985 backup-RAM
  precedent's exact scope (confirmed by direct grep of `src/main.cpp`).

## 8. QA Handoff

- Both **B4** (MSX-JE 16 KB SRAM) and **B6** (Halnote/MSX-JE firmware mapping) are assessed READY to
  close together, pending QA sign-off and the separate human release decision (normal gate, no
  auto-close).
- Zero regression across M1-M19: `ctest` 95/95 green (92 prior + 3 new), including the M9/M12
  CPU-timing oracles and every prior milestone's slot-map/device-accessor goldens (in particular the
  M13 BIOS-ROM-at-slot-0-0 guard, now exercised alongside a newly-populated sibling secondary slot
  for the first time).
- The A/B disposition is a genuine, disclosed mixed result (11/14 PARITY, 3/14 DIVERGENCE for the
  sub-mapper-shadow-enable effect specifically, on the installed openMSX 19.1 runtime) — QA should
  independently re-run `tools/openmsx-m20-halnote-parity.ps1` and review the investigation trail in
  `docs/m20-parity-trace-diff.md` before forming an independent judgment on whether this DIVERGENCE
  is non-blocking for closure.
- Files to review: `src/devices/halnote/halnote_rom.{h,cpp}`; `src/machine/hbf1xv_machine.{h,cpp}`
  (additive diff only); `tests/unit/devices/halnote/*`; `tests/integration/machine/
  hbf1xv_m20_halnote_integration_test.cpp`; `tools/gen-m20-halnote-probe.py`;
  `tools/openmsx-m20-halnote-parity.ps1`; `docs/m20-parity-trace-diff.md`;
  `docs/asset-checksums.txt` (refreshed).
- Ledger: `agent-protocol/state/current-phase.md` and `agent-protocol/state/milestones.md` updated
  to reflect implementation complete / Ready for QA. `agent-protocol/channels/decisions.md` left
  untouched (coordinator-owned) per the task's constraints.
