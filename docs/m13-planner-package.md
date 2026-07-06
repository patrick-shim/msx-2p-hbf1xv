# M13 Planner Package — RAM / ROM Memory Devices & Slot Population

- Milestone ID: M13 (renumbered from M12 by DEC-0003; VRAM is OUT — owned by the VDP in M14)
- Title: RAM / ROM Memory Devices & Slot Population
- Spec Owner: MSX Planner Agent
- Request: REQ-M13-002
- Authority: `agent-protocol/state/milestones.md` (M13 ledger, Status Planned); DEC-0002 (VRAM owned
  by VDP), DEC-0003 (renumber + M12 insertion). Carries the M11 forward-obligation R-1/A-2 (flip the
  bring-up reset slot default `#A8=0xFF` → `#A8=0`).
- Grounding (read; concrete paths cited inline):
  - `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` — THE authoritative slot/subslot/page
    layout for this exact machine (lines cited in §2).
  - `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §4 (mapper `#FC-#FF`, `100xxxxx`
    readback), §9 (HB-F1XV: 64 KB RAM mapper in slot 3-0; Kanji ROM; MSX-JE 16 KB SRAM).
  - `references/fact-sheets/Zilog Z80A CPU.md` §7 (`0xA8` primary + `0xFFFF` secondary slot switching;
    lines 223-241).
  - M11 artifacts: `docs/m11-planner-package.md`, `docs/m11-implementation-report.md`,
    `src/devices/chipset/*` (`SlotBus`, `MapperIo`, `IoBus`, `PpiSlotSelect`, `S1985Engine`,
    `SystemBus`), `src/machine/hbf1xv_machine.{h,cpp}`, `src/machine/ram_slot_backing.{h,cpp}`.
  - `src/CLAUDE.md` (boundary rules), `agent-protocol/guardrails.md`.

This is a planning artifact only. It contains NO production code. All C++ signatures below are
**proposed contracts** for the developer slice — illustrative, not implementations. Never copy
`references/openmsx-21.0/` code into `src/` (GPL isolation, guardrails).

---

## 1. Scope and Assumptions

### 1.1 In scope (M13 ledger)

- **(a) CPU-addressable RAM** — a 64 KB **memory-mapper RAM device** occupying slot **3-0**, pages
  0-3, whose 16 KB segment for each page is selected by the M11 `#FC-#FF` registers. Replaces the
  inert `RamSlotBacking` flat adapter as the CPU RAM backing.
- **(b) CPU-addressable ROM devices** — read-only ROM devices attached to the exact
  slot/sub-slot/page cells the openMSX HB-F1XV XML specifies (§2): BIOS+BASIC (0-0), SUB (3-1 p0),
  Kanji Driver/BASIC (3-1 p1-2), DISK ROM **presence** (3-2 p1), FM-MUSIC ROM **presence** (3-3 p1).
- **(c) Slot population + expansion wiring** — attach (a)/(b) into the M11 `SlotBus`; mark **both**
  primary slots **0 and 3** expanded (see A-6 — an M11 correction).
- **(d) Asset loading** — load local `bios/` images into the ROM devices via verified paths only;
  deterministic documented policy for a missing asset (no fabrication, no silent zero-fill).
- **(e) Reset-default flip (R-1/A-2)** — flip the M11 bring-up default `#A8=0xFF` to the authentic
  `#A8=0` (all pages → primary slot 0; page 0 → slot 0-0 BIOS), with a **boot checkpoint** proving
  the CPU fetches/executes real BIOS opcodes from slot 0, and reconcile the M11/M12 suites that
  pinned `#A8=0xFF`.
- **(f) A/B trace-diff** vs openMSX over a CPU-visible RAM/ROM/boot program (VRAM excluded).

### 1.2 Out of scope (named explicitly, each with justification)

| Deferred item | Why OUT of M13 | Owning milestone |
|---|---|---|
| **128 KB VRAM / V9958 VDP** | DEC-0002: VRAM is owned by the VDP; not CPU-addressable. | M14 |
| **MSX-JE Halnote mapper (slot 0-3) + 16 KB MSX-JE SRAM** | XML line 113 `mappertype=Halnote` — a banked 1 MB ROM + battery SRAM, a complex mapper; not needed for boot (BIOS is in 0-0). Task brief puts the 16 KB SRAM explicitly OUT. M13 **reserves** slot 0-3 as documented open-bus (see D-3). | later (Halnote/MSX-JE milestone) |
| **FM-PAC / YM2413 OPLL internals + `#7C/#7D` I/O + FM-PAC SRAM** | Only the **FM-MUSIC ROM presence/mapping** (3-3 p1) is in M13. OPLL synthesis, the `#7C/#7D` register I/O (XML line 194), and any FM SRAM are device internals. | later (FM-PAC milestone) |
| **FDC drive mechanics (WD2793 / MB89311), disk I/O, `motor`/`drives`** | Only the **DISK ROM presence** (3-2 p1, `rom_visibility` page 1) is in M13. XML lines 161-176 device behavior is deferred. | later (FDC milestone) |
| **Kanji FONT ROM I/O access path `#D8-#DB` (256 KB)** | The 256 KB font (`f1xvkfn.rom`) is accessed via **I/O ports** `#D8-#DB` (XML lines 34-37), NOT the CPU memory map; it is orthogonal to slot population and its screen use depends on the VDP (M14). M13 maps only the CPU-addressable Kanji **Driver/BASIC** ROM (3-1 p1-2). See D-4. | later (Kanji/peripheral milestone) |
| PSG/RTC/PPI-keyboard device internals | Unchanged from M11 (seams only). | later |

**Non-goal guardrails:** do not implement Halnote banking, OPLL synthesis, FDC mechanics, or the
Kanji-font I/O device. If a prerequisite appears to need any of these, STOP and raise a coordinator
decision (guardrails "Scope Control").

### 1.3 Assumptions (each with a verification action)

- **A-1 (asset→slot identity).** Each local `bios/*.rom` maps to the XML device whose role and size
  match (§3 table). The mapping is by role/size, not by SHA (local dumps may differ from the XML's
  cited revision SHAs — XML notes several "different than EPROM dump" revisions, e.g. lines 95, 154,
  172). *Verify:* `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` records the real
  local SHAs; the loader asserts each file's **size** matches the XML `size`/`window` (32K/16K/etc.)
  and logs the actual SHA. Do NOT assert a local file equals an XML SHA without a checksum capture.
- **A-2 (authentic reset `#A8=0`).** Real hardware resets `#A8` to `0`, and the sub-slot registers
  to `0`, so page 0 → slot 0-0 = BIOS and the CPU fetches the reset vector at `0x0000` from BIOS ROM
  (Z80A §7 lines 225-241; S1985 §3). This supersedes the M11 bring-up default (`#A8=0xFF`, R-1).
  *Verify:* boot-checkpoint test (M13-S4) asserts the opcode fetched at `0x0000` equals the loaded
  BIOS image byte 0; M11 reset-state unit test updated to `#A8=0`.
- **A-3 (mapper physical addressing vs readback are independent).** The `#FC-#FF` **readback**
  stays `0x80 | (seg & 0x1F)` (`100xxxxx`) as M11 implemented and the XML confirms 5-bit readback
  ("includes 5 bits mapper-read-back, confirmed by Meits", line 25). The RAM device's **physical**
  address uses only the populated segment count: `phys = (seg & (kSegments-1))*0x4000 +
  (addr & 0x3FFF)` with `kSegments = 4` for 64 KB. The two masks (5-bit readback vs 2-bit physical)
  differ authentically. *Verify:* unit test asserts readback `0x25→0x85` while the same write maps
  physical `seg 0x25 & 3 = 1`.
- **A-4 (unpopulated-segment wrap + reset segment values).** Exactly how a 64 KB mapper folds
  segments ≥4 and what the segment registers reset to are parity-critical. *Verify:* read
  `references/openmsx-21.0/src/memory/MSXMemoryMapper.cc` / `MSXMapperIO.cc` /
  `references/openmsx-21.0/src/memory/RamBlock*`/`CheckedRam*` (behavior only, never copy) in M13-S1
  before fixing the wrap and the cold-boot segment defaults; ground the choice in a cited path.
- **A-5 (RAM power-on content).** XML line 129 `initialContent` (gz-base64) is documented in the
  same line's comment as `(chr(0)+chr(255))*128 + (chr(255)+chr(0))*128`, i.e. an alternating
  `00/FF` pattern, not all-zero. This is parity-relevant for any RAM-content diff. *Verify:* decode
  the exact base64 in M13-S3; cold_boot initializes the mapper RAM to that pattern; update the M10
  full-state-dump golden if it asserted all-zero DRAM (justified, like M11 R-3). If decoding is
  uncertain, default to matching openMSX and record the decoded bytes in the implementation report.
- **A-6 (BOTH slot 0 and slot 3 are expanded — M11 correction).** The XML gives primary slot 0
  four `<secondary slot=...>` children (lines 85-116) and primary slot 3 four (lines 123-199);
  therefore **both** are expanded sub-slotted primaries. M11 wired only slot 3 expanded. *Verify:*
  M13-S3 calls `slot_bus_.set_expanded(0, true)` and `set_expanded(3, true)`; a unit test asserts
  `#FFFF` sub-slot decode works in both, and that the `0xFF ^ reg` readback is active for page-3
  primary 0 as well as 3.
- **A-7 (missing-asset determinism).** An absent required BIOS file yields a ROM device filled with
  a documented constant (open-bus `0xFF`) AND a recorded diagnostic note; never a silent zero-fill,
  never fabricated provenance (guardrails "Asset and Script Safety"). *Verify:* unit test with a
  deliberately-absent path asserts fill value + that a note was recorded; `tools/validate-assets.ps1`
  remains the gate for "all required BIOS present".

---

## 2. Exact Slot / Page Map (derived from `Sony_HB-F1XV.xml`)

All citations are `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` line numbers. A page =
16 KB: page0 `#0000-#3FFF`, page1 `#4000-#7FFF`, page2 `#8000-#BFFF`, page3 `#C000-#FFFF`. `<mem
base=.. size=..>` is the device's placement **inside its (sub)slot's 64 KB view**.

### 2.1 Primary slot 0 — EXPANDED (lines 85-117)

| Sub | Device (XML id) | `<mem>` | Pages | Local asset | Notes |
|---|---|---|---|---|---|
| **0-0** | MSX BIOS with BASIC ROM (l.87) | base `0x0000` size `0x8000` (l.97) | 0-1 (`#0000-#7FFF`) | `bios/f1xvbios.rom` (32K) | XML file `hb-f1xv_basic-bios2p.rom` (l.94). Reset fetch origin. |
| 0-1 | *(empty)* (l.101) | — | — | — | open bus |
| 0-2 | *(empty)* (l.103) | — | — | — | open bus |
| **0-3** | HB-F1XV MSX-JE (l.106) | base `0x0000` size `0x10000` (l.111) | 0-3 | `bios/f1xvfirm.rom` (1M) | `mappertype=Halnote` (l.113) + `hb-f1xv_msx-je.sram` (l.112). **DEFERRED** — reserved open-bus in M13 (D-3). |

### 2.2 Primary slots 1 & 2 — external cartridges (lines 119, 121)

`<primary external="true" slot="1"/>` and `slot="2"` — the two physical cartridge slots. `roms/aleste.rom`
(2M) is a **cartridge** image, not a built-in device. Cartridge insertion/mapper handling is not an
M13 built-in-memory task; M13 leaves slots 1-2 empty (open bus) and defers cartridge loading (see D-5).

### 2.3 Primary slot 3 — EXPANDED (lines 123-199)

| Sub | Device (XML id) | `<mem>` | Pages | Local asset | Notes |
|---|---|---|---|---|---|
| **3-0** | Main RAM `MemoryMapper` size 64 (l.126) | base `0x0000` size `0x10000` (l.128) | 0-3 | *(RAM, no file)* | 64 KB = 4×16 KB segments via `#FC-#FF`. `initialContent` alt 00/FF (l.129, A-5). |
| **3-1** | MSX Sub ROM (l.134) | base `0x0000` size `0x4000` (l.144) | 0 (`#0000-#3FFF`) | `bios/f1xvext.rom` (16K) | XML `hb-f1xv_msx2sub.rom` (l.141). |
| **3-1** | MSX Kanji Driver w/ BASIC (l.146) | base `0x4000` size `0x8000` (l.156) | 1-2 (`#4000-#BFFF`) | `bios/f1xvkdr.rom` (32K) | XML `hb-f1xv_kanjibasic.rom` (l.153). Two devices coexist in sub-slot 3-1 at disjoint pages. |
| **3-2** | WD2793 Memory-Mapped FDC (l.161) | ROM base `0x4000` size `0x4000` (l.174) | 1 (`#4000-#7FFF`) | `bios/f1xvdisk.rom` (16K) | `rom_visibility base=0x4000 size=0x4000` (l.175) → ROM visible in **page 1 only**, no mirroring (l.174). M13 = ROM presence only; FDC mechanics OUT. |
| **3-3** | MSX-MUSIC (l.180) | ROM base `0x4000` size `0x4000` (l.195) | 1 (`#4000-#7FFF`) | `bios/f1xvmus.rom` (16K) | XML `hb-f1xv_fmbasic.rom` (l.187); `io base=0x7C num=2` (l.194) OPLL — OUT. M13 = ROM presence only. |

### 2.4 Not in the CPU memory map — I/O-accessed (lines 29-38)

| Device | Access | Local asset | M13 status |
|---|---|---|---|
| Kanji ROM (font) (l.29) | I/O `#D8-#DB` (l.34-37) | `bios/f1xvkfn.rom` (256K) | **DEFERRED** (D-4). Present on disk, not wired in M13. |

### 2.5 Spec-table reconciliation (Target Machine Specification vs XML/fact-sheet)

The strict spec ROM breakdown — "32 KB (BIOS/BASIC) + 16 KB (SUB) + 16 KB (KANJI BASIC + KANJI ROM)
+ 16 KB (DISK)" — is a **simplification**. The authoritative XML + S1985 fact-sheet §9 give a richer
real layout; the reconciliation:

- **32 KB BIOS/BASIC** ↔ slot 0-0, 32 KB (`f1xvbios.rom`). ✅ direct.
- **16 KB SUB** ↔ slot 3-1 p0, 16 KB (`f1xvext.rom`). ✅ direct.
- **16 KB "KANJI BASIC ROM + KANJI ROM"** ↔ splits into TWO real parts: (i) the **32 KB** Kanji
  Driver/BASIC ROM in the CPU map (3-1 p1-2, `f1xvkdr.rom`); (ii) the **256 KB** Kanji **font** ROM
  reached via I/O `#D8-#DB` (`f1xvkfn.rom`), which is NOT in the memory map. The spec's single "16 KB
  KANJI" line conflates the memory-mapped driver and understates both parts. Authoritative sizes are
  32 KB (driver, mapped) + 256 KB (font, I/O).
- **16 KB DISK** ↔ slot 3-2 p1, 16 KB (`f1xvdisk.rom`). ✅ direct.
- **NOT in the spec ROM table but present in hardware/XML:** the **16 KB FM-MUSIC** ROM (3-3 p1,
  `f1xvmus.rom`) and the **1 MB MSX-JE firmware** (0-3, Halnote, `f1xvfirm.rom`) + its 16 KB SRAM.

**Ruling (per CLAUDE.md "Reference materials" — XML + fact-sheet are authoritative grounding, and
the strict spec table is non-negotiable but explicitly flagged a simplification for the COLORS/ROM
lines):** M13 follows the XML layout. The spec table is not violated — its ROM figures are a subset
of the real map — and this document records the reconciliation so the divergence is non-silent.

---

## 3. Asset → Slot Mapping Table

Verified present via `Glob {bios,roms}/*` (this session) — all eight files exist. Provenance is NOT
asserted here beyond file presence; real SHAs are captured by `tools/checksum-assets.ps1`.

| Local path | Size (role) | Target cell | Role in M13 | Policy if absent |
|---|---|---|---|---|
| `bios/f1xvbios.rom` | 32K BIOS+BASIC | slot 0-0, pages 0-1 | Boot ROM; reset fetch origin | **required** → 0xFF-fill + logged note (A-7); `validate-assets` fails the gate |
| `bios/f1xvext.rom` | 16K SUB (BASIC V3.0) | slot 3-1, page 0 | Sub-ROM mapped | required → 0xFF-fill + note |
| `bios/f1xvkdr.rom` | 32K Kanji Driver/BASIC | slot 3-1, pages 1-2 | Kanji BASIC driver mapped | required → 0xFF-fill + note |
| `bios/f1xvdisk.rom` | 16K DISK | slot 3-2, page 1 | DISK ROM **presence** (FDC OUT) | required → 0xFF-fill + note |
| `bios/f1xvmus.rom` | 16K FM-MUSIC | slot 3-3, page 1 | FM-MUSIC ROM **presence** (OPLL OUT) | required → 0xFF-fill + note |
| `bios/f1xvkfn.rom` | 256K Kanji font | *(I/O `#D8-#DB`)* | **DEFERRED** (D-4) — not wired | n/a in M13 |
| `bios/f1xvfirm.rom` | 1M MSX-JE firmware | slot 0-3 (Halnote) | **DEFERRED** (D-3) — reserved open-bus | n/a in M13 |
| `roms/aleste.rom` | 2M cartridge | *(external slot)* | **DEFERRED** (D-5) — not a built-in | n/a in M13 |

**Missing-asset policy (deterministic, A-7):** a required-but-absent file → the ROM device is
constructed over an image filled with `0xFF` (open-bus-like) AND a note
`"ROM asset <path> ABSENT; slot <p-s> page <n> filled 0xFF"` is appended to a machine diagnostics
list (and to the debug event log when logging is enabled). No silent zero-fill; no fabricated SHA.
The overall gate `tools/validate-assets.ps1` still fails if any required BIOS is missing, so a green
M13 run always has real assets — the fill path exists for determinism/robustness and is unit-tested
with a deliberately-bad path, not exercised in the passing evidence run.

---

## 4. src/ Placement Decision

Authority to add under `src/` is the planner's per `src/CLAUDE.md` (device logic → `src/devices/`;
machine wiring → `src/machine/`; contracts already in `src/core/`). New device sources go under a new
`src/devices/memory/` folder. Each new `.cpp` is added to `add_library(sony_msx_core …)` in the root
`CMakeLists.txt` (single static lib, no per-dir CMake — confirmed by M11 report §3).

### 4.1 `src/devices/memory/` — new device logic

| File | Purpose | Grounding |
|---|---|---|
| `src/devices/memory/README.md` (**new**) | Document the module: ROM device + RAM mapper, the "MapperIo owns segment state, RAM device consumes it" split, and the "no GPL copy" rule. | S1985 §4 |
| `src/devices/memory/rom_device.{h,cpp}` (**new**) | `RomDevice : core::MemoryDevice` — a read-only window over a byte image. Configured with the device-window `base`/`size` (from `<mem>`) and the source image; `mem_read` returns `image[addr - base]` inside `[base,base+size)` and open-bus `0xFF` outside; `mem_write` is ignored (documented — ROM). Handles the `rom_visibility` page-1-only case for the DISK ROM (l.175) by construction (window = page 1). Reusable for BIOS/SUB/Kanji-driver/DISK/FM-MUSIC. | XML `<mem>`/`<rom_visibility>`; `references/openmsx-21.0/src/memory/Rom*.cc` (behavior only) |
| `src/devices/memory/memory_mapper_ram.{h,cpp}` (**new**) | `MemoryMapperRam : core::MemoryDevice` — 64 KB RAM whose page→physical mapping consumes the M11 `MapperIo` segment registers. Holds/references a 64 KB backing (the machine's `dram_` `MemoryRegion`, A-5 init); `mem_read/write(addr)` compute `phys = (mapper.segment(page(addr)) & (kSegments-1)) * 0x4000 + (addr & 0x3FFF)`, `kSegments=4`. Does NOT own segment registers (single source of truth = `MapperIo`). | S1985 §4; A-3/A-4; `references/openmsx-21.0/src/memory/MSXMemoryMapper.cc` (behavior only) |

### 4.2 `src/machine/` — composition + asset loading (machine wiring)

| File | Purpose |
|---|---|
| `src/machine/rom_asset_loader.{h,cpp}` (**new**) | Resolve `bios/` paths, read files into `std::vector<std::uint8_t>`, validate size against the expected role size, and apply the A-7 missing-asset policy (0xFF-fill + note). Returns images + a diagnostics list. Machine-side (knows HB-F1XV asset names), keeps `RomDevice` ignorant of the filesystem. |
| `src/machine/hbf1xv_machine.{h,cpp}` (**edit**) | (1) Instantiate `RomDevice`s (BIOS, SUB, Kanji-driver, DISK, FM-MUSIC) + `MemoryMapperRam` over `dram_` bound to `mapper_io_`; **remove** `RamSlotBacking` usage (or keep the header, drop the member). (2) `wire_bus()`: attach devices to the §2 cells; `set_expanded(0,true)` and `set_expanded(3,true)` (A-6); reserve slot 0-3 open-bus (D-3). (3) `cold_boot()`: authentic `#A8=0`, sub-slot regs `0`, RAM init pattern (A-5). (4) add a documented debug/test helper `map_flat_ram()` (see §6/R-2). (5) keep `load_memory`/`read_memory` as **RAM-backing-direct** debug accessors (linear into `dram_`, segment-independent — documented) so M10 dump + existing load semantics survive. |
| `src/machine/ram_slot_backing.{h,cpp}` (**retire/repurpose**) | Superseded by `MemoryMapperRam`. Either delete (and drop from `CMakeLists.txt`) or leave unused; planner recommends deletion with a note in the implementation report to avoid a dead flat-RAM path. |

### 4.3 Interaction with the M11 `mapper_io` `100xxxxx` readback (double-owning avoided)

- **`MapperIo` (M11, `src/devices/chipset/mapper_io.{h,cpp}`) remains the SOLE owner** of the four
  `#FC-#FF` segment registers and of the `0x80 | (seg & 0x1F)` readback. It is already the I/O-bus
  device on `#FC-#FF`; M13 does not add a second writer/owner.
- **`MemoryMapperRam` is a pure CONSUMER**: it reads `mapper_io_.segment(page)` (existing accessor,
  `mapper_io.h:33`) to resolve physical addresses. No segment state is duplicated. This realizes the
  M11 seam note ("a `MemoryDevice` slot backing consumes the segment", `docs/m11-planner-package.md`
  §1.2 row "Mapper RAM device").
- Consequence (A-3): the readback path (chipset) and the physical-address path (memory device) share
  one register file but apply different masks (5-bit vs 2-bit) — authentic and unit-tested.
- *Verify in S1:* the `MapperIo::segment()` accessor is sufficient; if `MemoryMapperRam` needs a
  live reference rather than a value snapshot, pass `MapperIo&` (composition in the machine), not a
  copy — segment writes must be observed immediately on the next CPU access.

---

## 5. Slice Plan (M13-S1 … M13-S5)

Every slice runs the full evidence-gate set (§7) and must leave `ctest` green. Order is
dependency-driven: device layer → asset loading → slot wiring → reset-flip+boot → A/B.

### M13-S1 — Memory device layer (ROM device + RAM mapper), isolated
- **Goal:** deterministic `RomDevice` and `MemoryMapperRam` with no machine wiring yet.
- **Files:** `src/devices/memory/rom_device.{h,cpp}`, `src/devices/memory/memory_mapper_ram.{h,cpp}`,
  `src/devices/memory/README.md`; `CMakeLists.txt`.
- **Pre-work (A-4):** read `references/openmsx-21.0/src/memory/MSXMemoryMapper.cc` / `MSXMapperIO.cc`
  for the unpopulated-segment wrap and cold-boot segment defaults; cite the path in code comments.
- **Unit tests:**
  - `tests/unit/devices/memory_rom_device_unit_test.cpp` — read returns image bytes inside the
    window; writes ignored (still reads original); addresses outside `[base,base+size)` → `0xFF`;
    DISK-style page-1-only window returns `0xFF` in page 0/2/3.
  - `tests/unit/devices/memory_mapper_ram_unit_test.cpp` — with a fake/real `MapperIo`: segment
    write changes the physical cell read/written; 64 KB linear config `{0,1,2,3}` gives a flat 64 KB
    view; segment wrap `seg 4→0`, `seg 0x25→1` (A-3); readback independence (`MapperIo` still
    `0x25→0x85`).
- **Integration tests:** none new; existing suite green.
- **Evidence gates:** validate-assets, checksum, Debug build, ctest.

### M13-S2 — Asset loading + missing-asset policy (machine side)
- **Goal:** deterministic ROM-image provisioning from `bios/`.
- **Files:** `src/machine/rom_asset_loader.{h,cpp}`; `CMakeLists.txt`.
- **Unit tests:** `tests/unit/machine/rom_asset_loader_unit_test.cpp` — loads a real `bios/f1xvbios.rom`
  and asserts size == 32K; a deliberately-absent path → 0xFF-filled image of the expected size + a
  recorded diagnostic note (A-7); two loads of the same present file are byte-identical (determinism).
- **Integration tests:** none new; existing green.
- **Evidence gates:** all four.

### M13-S3 — Slot population wiring (attach devices; expand slots 0 & 3; RAM init)
- **Goal:** the §2 map is live on the M11 `SlotBus`; RAM mapper replaces `RamSlotBacking`.
- **Files:** `src/machine/hbf1xv_machine.{h,cpp}` (edit), retire `ram_slot_backing.*`; `CMakeLists.txt`.
- **Unit tests:** `tests/unit/machine/hbf1xv_slot_map_unit_test.cpp` (extend) — for a chosen `#A8`/
  sub-slot config, assert each cell resolves to the expected device: BIOS at 0-0 p0-1 returns the
  loaded image bytes; SUB at 3-1 p0; Kanji-driver at 3-1 p1-2; DISK at 3-2 p1 (and `0xFF` in p0/2/3);
  FM-MUSIC at 3-3 p1; RAM at 3-0 read/writes hit `dram_` via the current segments; slots 0 AND 3
  report `is_expanded()==true` (A-6); `#FFFF` sub-slot decode + `0xFF^reg` readback active in both;
  RAM cold-boot content matches the A-5 pattern.
- **Integration tests:** existing suite green (still under the M11 `#A8=0xFF` default until S4 — see
  ordering note below).
- **Evidence gates:** all four.
- **Ordering note:** S3 keeps `cold_boot`'s `#A8=0xFF` default so the existing program tests stay
  green while the map is populated; S4 performs the reset flip and the test reconciliation together
  (so no intermediate slice is left red).

### M13-S4 — Reset-default flip (`#A8=0xFF`→`0`) + boot checkpoint + suite reconciliation **(dedicated)**
- **Goal:** authentic boot from slot-0 BIOS, proven by a boot checkpoint, with M11/M12 suites updated.
- **Files:** `src/machine/hbf1xv_machine.{h,cpp}` (cold_boot flip to `#A8=0`, sub-slot regs `0`; add
  `map_flat_ram()` helper); update the M11/M12 tests enumerated in R-2/R-3.
- **The reset flip:** `cold_boot` sets `#A8=0` and all sub-slot registers `0` → page0→0-0 BIOS,
  page1→0-0 (BIOS spans 0x0000-0x7FFF), page2→0-0 (empty→open bus), page3→0-0 (empty; `#FFFF` is the
  sub-slot register, reads `0xFF^0`). CPU reset PC=`0x0000` fetches BIOS byte 0 (Z80A §7).
- **Boot-checkpoint test:** `tests/integration/machine/hbf1xv_bios_boot_integration_test.cpp` —
  `cold_boot` with the real BIOS; then a deterministic, self-grounded checkpoint:
  1. Assert bus-visible byte at `0x0000` (through `SystemBus`) == loaded BIOS image byte 0 (proves
     slot-0 ROM is the fetch source, not open bus / RAM).
  2. Single-step a bounded K instructions (K fixed, e.g. 32); for each fetched instruction assert the
     opcode byte fetched by the CPU equals the slot-resolved BIOS image byte at that PC (proves the
     CPU executes *real ROM opcodes*, golden derived from the image itself — no guessed disassembly).
  3. Assert PC reaches a defined checkpoint (e.g. the first `JP`/`CALL` target resolved from the ROM
     bytes) and that the run is deterministic across two `cold_boot`+run cycles.
  The absolute PC/trace values are cross-validated against openMSX in S5 (not hardcoded from memory).
- **Suite reconciliation (R-2/R-3):**
  - Update `machine_hbf1xv_slot_map_unit_test` reset assertion `#A8==0xFF` → `#A8==0` with a comment
    citing A-2 and this package (mirrors the M11 R-3 justified-update pattern).
  - CPU/program integration tests that `load_memory(...)` a program and run it from RAM now need RAM
    paged in (authentic reset boots BIOS). Add `map_flat_ram()` (sets `#A8`→slot 3-0 all pages +
    mapper segments `{0,1,2,3}` linear) and call it after `cold_boot` in those tests. This preserves
    the exact M11 flat-64 KB behavior for CPU-behavior tests **by explicit configuration**, not by a
    hidden default — the tests still exercise the CPU over RAM, now the way BIOS pages it in. Justify
    each edit in the implementation report (no silent weakening).
- **Integration tests:** the new boot checkpoint + all updated suites.
- **Evidence gates:** all four; full prior suite (M0–M12) green with the justified updates.

### M13-S5 — A/B trace-diff vs openMSX + regression close
- **Goal:** capture a REAL openMSX trace-diff for RAM/ROM/boot behavior (VRAM excluded); finalize.
- **Files:** extend `tools/openmsx-trace-parity.ps1` (or new `tools/openmsx-mem-parity.ps1`);
  `tests/parity/m13_mem_probe.bin` and/or a BIOS-boot checkpoint script; `docs/m13-parity-trace-diff.md`;
  refresh `docs/asset-checksums.txt`.
- **Tests:** `tests/integration/machine/hbf1xv_m13_parity_checkpoint_integration_test.cpp` — runs the
  same probe/boot to the same checkpoint the harness feeds openMSX.
- **Evidence gates:** all four **plus** the A/B gate (§6). No parity claim without a captured diff.

---

## 6. A/B Acceptance Test (definition)

**Harness.** Extend the existing per-instruction parity harness (`tools/openmsx-trace-parity.ps1`,
which the M11/M12 reports confirm drives openMSX on WSL against the genuine
`Sony_HB-F1XV` machine and diffs architectural state via `tools/trace-diff.py`). Reference machine =
openMSX `Sony_HB-F1XV` (its XML is exactly `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`),
so slot/ROM semantics match by construction. **VRAM/VDP state is excluded from the diff** (M14).

**Two complementary A/B subjects:**

1. **BIOS-boot checkpoint (primary).** From authentic reset (`#A8=0`), single-step a bounded K
   instructions on both emulators and diff PC / opcode / all Z80 architectural registers / flags at
   each step. This directly exercises real ROM fetch/execute from slot 0 and early BIOS slot/mapper
   setup. *Bounded* to a fixed K to stay deterministic and avoid VDP-dependent divergence (BIOS soon
   touches the VDP; the checkpoint must stop at or before the first VDP-dependent branch — determine
   K in S5 by finding the first `#98/#99` access in the trace and cutting before it, recorded in the
   artifact).
2. **CPU-visible RAM/ROM probe (secondary).** A small `tests/parity/m13_mem_probe.bin` that, from a
   RAM-mapped state: reads known BIOS bytes into registers (`LD A,(0x0000)` etc.), writes+reads
   mapper RAM, and switches a mapper segment via `OUT (#FC),A` then reads back the `100xxxxx` value —
   all landing in CPU registers so the existing register-diff comparator captures them.

**Hard rule (guardrails; DEC-0001/DEC-0002 precedent):** NO parity claim without a genuine captured
diff. If openMSX cannot be driven, the tool reports **BLOCKED** (empty B side) and QA treats parity
as outstanding — never a pass. Reuse the M10/M11/M12 adversarial self-check (empty-side→BLOCKED,
corrupted-field→DIVERGENCE) to validate the comparator. Record the exact openMSX build/machine and
the chosen K in `docs/m13-parity-trace-diff.md`.

*Assumption to verify in S5 (R-4):* openMSX must have the real HB-F1XV system ROMs installed to boot
this machine; M11/M12 reports state the WSL install did (`/usr/share/openmsx/machines/Sony_HB-F1XV.xml`
with real ROMs). If ROMs are absent there, the boot subject is BLOCKED and only the RAM-mapped probe
(which needs no BIOS) proceeds; report honestly.

---

## 7. Evidence-Gate Mapping (per slice)

Configure once: `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` (baseline build flow).

| Gate | Command | Pass condition | Slices |
|---|---|---|---|
| Assets present | `tools/validate-assets.ps1` | required BIOS present + ≥1 ROM (run/capture; never fabricate) | S1-S5 |
| Checksums | `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | reproducible checksum file written (captures real local SHAs, A-1) | S1-S5 |
| Build | `cmake --build build --config Debug` | build succeeds | S1-S5 |
| Tests | `ctest --test-dir build -C Debug --output-on-failure` | all deterministic tests pass | S1-S5 |
| A/B (behavior) | `tools/openmsx-trace-parity.ps1`/`openmsx-mem-parity.ps1` → `docs/m13-parity-trace-diff.md` | genuine captured diff or honest BLOCKED (never fabricated) | S5 |

---

## 8. Acceptance Criteria (M13 exit)

1. `MemoryMapperRam` (64 KB, `#FC-#FF` segments consumed from `MapperIo`) wired into slot 3-0 replaces
   `RamSlotBacking` as the CPU RAM backing; segment decode + `100xxxxx` readback independence
   unit-verified. *(S1, S3)*
2. `RomDevice`s for BIOS/BASIC (0-0), SUB (3-1 p0), Kanji-driver (3-1 p1-2), DISK presence (3-2 p1),
   FM-MUSIC presence (3-3 p1) attached to the exact §2 cells; both primary slots 0 & 3 expanded (A-6);
   DISK `rom_visibility` page-1-only honored. *(S1, S3)*
3. Assets loaded from verified `bios/` paths; missing-asset policy deterministic (0xFF-fill + logged
   note, A-7); no fabricated provenance. *(S2)*
4. Reset default flipped `#A8=0xFF`→`#A8=0`; boot checkpoint proves the CPU fetches/executes real
   BIOS opcodes from slot 0; M11/M12 suites reconciled (justified, non-silent). *(S4)*
5. System integration: CPU fetches/executes from ROM and reads/writes mapper RAM through the M11 bus;
   boot reaches the defined checkpoint. *(S4)*
6. Per-slice evidence gates all executed and green (§7). *(S1-S5)*
7. A/B trace-diff vs openMSX captured as a REAL artifact `docs/m13-parity-trace-diff.md` (VRAM
   excluded); no fabricated parity. *(S5)*
8. QA sign-off recorded before closure (`docs/m13-qa-signoff.md`).

---

## 9. Dependency Map (across src/)

```
core/device_contracts.h  (MemoryDevice — M11)          core/bus.h
        │                                                   │
devices/chipset/ (M11): SlotBus, MapperIo, IoBus, SystemBus, S1985Engine   [reused]
        │  segment source            │ slot attach/expand
        ▼                            ▼
devices/memory/ (NEW):  memory_mapper_ram ── consumes ─▶ chipset/MapperIo   [S1]
                        rom_device                                          [S1]
        │
        ▼
machine/ (edit): rom_asset_loader  ─▶ hbf1xv_machine.wire_bus()/cold_boot() [S2,S3,S4]
                 (retire ram_slot_backing)
        │
        ▼
tools/openmsx-*-parity.ps1 + tests/parity + docs/m13-parity-trace-diff.md   [S5]
```
Cross-milestone: **M14** attaches the V9958 `IoDevice` on `#98-#9B` (inheriting the `#9C-#9F` alias)
and takes ownership of the `vram_` region out of the machine; the **MSX-JE/Halnote** and **FM-PAC**
and **FDC** and **Kanji-font** milestones plug into the cells/ports M13 reserves (D-3/D-4/D-5).

---

## 10. Risks and Deferrals (with verification/close actions)

- **R-1 / A-2 (reset-flip regression surface).** Flipping to `#A8=0` reroutes page 0 from RAM to
  BIOS; any test that ran a program from RAM at reset breaks. *Close:* S4 adds `map_flat_ram()` and
  updates those tests explicitly; full `ctest` proves green (mirrors the M11 R-3 justified-update
  discipline). This is the M11 forward-obligation being discharged.
- **R-2 (segment mapping breaks flat-RAM assumptions).** M11's `RamSlotBacking` was flat 64 KB; the
  M13 mapper folds pages onto the current segments (all-0 at reset → 16 KB mirrored 4×). *Close:*
  `map_flat_ram()` sets segments `{0,1,2,3}`; `load_memory`/`read_memory` write the backing linearly
  (segment-independent debug accessors, documented) so M10 dump + load semantics survive. *Verify:*
  M10 `hbf1xv_debug_dump`/`serialize_state_dump` golden re-checked in S3/S4.
- **R-3 (RAM init pattern changes debug-dump golden).** A-5 replaces zero-init DRAM with the XML
  alternating 00/FF pattern. *Verify/close:* decode the exact base64 (S3), update the M10 full-state
  golden with justification; if the decode is uncertain, default to matching openMSX and record the
  bytes. Alternatively, if A/B and dump goldens conflict, escalate a decision rather than guess.
- **R-4 (openMSX driveability / BIOS presence for A/B).** The boot subject needs real HB-F1XV ROMs in
  the openMSX install. *Verify:* confirm as in M11/M12 (`/usr/share/openmsx/machines/Sony_HB-F1XV.xml`
  + ROMs); if absent, boot subject → BLOCKED, run only the BIOS-independent RAM/ROM probe, report
  honestly (never claim parity).
- **R-5 (unpopulated-segment wrap + reset segment defaults).** A-4 — parity-critical, not asserted
  from memory. *Verify:* read the concrete openMSX memory-mapper sources in S1 and ground the wrap +
  cold-boot segment values in a cited path.
- **R-6 (DISK/FM-MUSIC ROM presence without device I/O may confuse a probe).** Mapping these ROMs
  but not their I/O (`#7C/#7D`, FDC regs) means CPU reads of the ROM work, but a BIOS boot may branch
  into disk/FM init and hit unimplemented I/O (open-bus `0xFF`). *Verify:* keep the boot checkpoint K
  bounded before disk/FM init (found from the S5 trace); the RAM/ROM probe avoids those paths.
- **D-3 (MSX-JE Halnote, slot 0-3) DEFERRED.** Reserved open-bus in M13; not needed for boot.
  *Forward:* a Halnote/MSX-JE milestone attaches the 1 MB banked ROM + 16 KB SRAM at 0-3.
- **D-4 (Kanji font `#D8-#DB`) DEFERRED.** I/O-accessed, orthogonal to the memory map; screen use
  needs the VDP (M14). *Forward:* a Kanji/peripheral milestone adds the I/O device over `f1xvkfn.rom`.
- **D-5 (cartridge slots 1-2) DEFERRED.** `roms/aleste.rom` is a cartridge, not a built-in; left open
  bus. *Forward:* a cartridge-loading milestone.

---

## 11. Developer Handoff

- **Start at M13-S1** (device layer) — `RomDevice` + `MemoryMapperRam` unit-tested in isolation with
  a fake/real `MapperIo`; read the concrete openMSX memory-mapper sources first (A-4/R-5) and cite
  paths in comments; never copy GPL code.
- Implement slices **in order** (S1→S5); each runs the full §7 gate set and keeps `ctest` green. The
  **reset flip + boot checkpoint + suite reconciliation are one dedicated slice (S4)** so no
  intermediate slice is left red; A/B (§6) is the S5 gate.
- **src/ placement is fixed by §4:** new device logic in `src/devices/memory/`, asset loading +
  composition in `src/machine/`; `MapperIo` stays the SOLE owner of the `#FC-#FF` segment state
  (RAM device only consumes it). Add each new `.cpp` to `add_library(sony_msx_core …)` in the root
  `CMakeLists.txt`.
- **Assets:** map each `bios/*.rom` per §3 by role/size; capture real SHAs with
  `tools/checksum-assets.ps1` (do not assert XML SHAs of local files). Missing-asset path = 0xFF-fill
  + logged note, unit-tested with a bad path; never silent, never fabricated.
- **No fabricated parity:** `docs/m13-parity-trace-diff.md` must contain a genuine captured diff or an
  honest BLOCKED result; VRAM is excluded (M14).
- **Hard scope fence:** no Halnote banking, no OPLL, no FDC mechanics, no Kanji-font I/O device, no
  VDP/VRAM. If blocked on a prerequisite, raise a coordinator decision (guardrails "Scope Control").
- Deliverables: source per §4; tests per §5; `docs/m13-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`; an implementation report `docs/m13-implementation-report.md`; then hand
  to QA for `docs/m13-qa-signoff.md`.
