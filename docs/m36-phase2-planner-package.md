# M36 Phase 2 Planner Package (DEC-0050 Reframe Addendum)

- Milestone ID: **M36** (Phase 2)
- Title: Live-Playtest Bug Fixes + FM-PAC Peripheral Cartridge + Disk-Save Persistence
- Date: 2026-07-10
- Planner: MSX Planner (opus)
- Status: PLANNING — no production code. Supersedes the machine-level-SRAM framing of
  `docs/m36-planner-package.md` (§2.4, §3 S4/S5, §8 Phase 2A).

---

## 0. DEC-0050 Reconciliation Statement (authoritative)

**This addendum SUPERSEDES `docs/m36-planner-package.md`'s machine-level SRAM framing.** Per
**DEC-0050** (`agent-protocol/channels/decisions.md:567-608`), the following are the governing
facts; where the pre-DEC-0050 package conflicts, THIS document controls:

1. **The HB-F1XV built-in FM is MSX-MUSIC (OPLL YM2413 + APRLOPLL BIOS, NO SRAM), NOT a Panasonic
   FM-PAC.** Grounding: `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` declares
   `<MSX-MUSIC>` with **no** `<sramname>`, while every SRAM-bearing device in that same file
   (S1985, RTC/cmos, MSX-JE) carries an explicit `<sramname>`. The real `bios/f1xvmus.rom` is the
   "APRLOPLL" MSX-MUSIC BIOS (verified present: `bios/f1xvmus.rom`, 16 KB, per M17 DEC-0012/DEC-0013).
2. **"NO S-RAM AVAILABLE" is CORRECT hardware behavior on a bare HB-F1XV.** The YS II symptom the
   human saw is NOT an internal-FM defect — the pre-DEC-0050 package's "Bug A" was mis-framed as a
   machine defect. It is a *missing peripheral*: the game wants an FM-PAC cartridge inserted.
3. **SRAM = PERIPHERAL.** The 8 KB battery SRAM belongs to the external, insertable Panasonic
   FM-PAC CARTRIDGE, not the machine.
4. **The speculative internal `sram_` is an inaccuracy to reconcile.** `src/machine/hbf1xv_machine.h:176-189`
   documents `kSramBytes = 8*1024` as an explicit *assumption* ("the strict spec table lists no
   SRAM capacity ... Verification action: confirm ... when the FM-PAC device milestone is
   implemented"). DEC-0050 discharges that verification action: the bare machine has no FM-PAC
   SRAM, so `sram_` must be removed from / relocated out of the bare machine.
5. **Both save-clause paths are now achievable this cycle** (see §0.1 asset update). The completion
   criterion's save clause is AND/OR: disk-save persistence OR FM-PAC SRAM-save satisfies it.

### 0.1 Asset update (coordinator, 2026-07-10) — FM-PAC ROM now present, real-software validation UNBLOCKED

The human provided the FM-PAC BIOS ROM at **`roms/fmpac.rom`** (verified present:
`Glob roms/**` → `roms/fmpac.rom`). Coordinator-reported facts (to be re-verified by the developer
as an evidence gate, not asserted here as read-verified): 16,384 bytes (correct FM-PAC BIOS size);
MSX cart header `"AB"` (0x4142) at offset 0 → maps to 0x4000; INIT=0x0000, STATEMENT/CALL-handler
entry=0x4082; FM-PAC detect signature `"PAC2OPLL"` at offset 0x18 (= 0x4018 mapped); SHA1
`2dc4517ebd5a061f9b5aa6b449cc4d4a2073540c`. Loaded via `--cart roms/fmpac.rom` (like plugging the
cartridge into real hardware).

**Provenance (honest, per guardrails — NO hash-match criterion):** the coordinator checked
`references/openmsx-21.0/share/extensions/fmpac.xml`, which pins two canonical Panasoft SW-M004
FM-PAC dumps (SHA1 `fec451b9256570a0e4869355a40329c57f40705f` and
`9d789166e3caf28e4742fe933d962e99618c633d`). **Our `roms/fmpac.rom` (`2dc4517e…`) matches
NEITHER.** Disposition: it is a GENUINE FM-PAC BIOS by SIGNATURE (valid "AB" header at 0x4000,
"PAC2OPLL" detect marker at 0x4018, 16 KB) but a DIFFERENT DUMP VARIANT than openMSX's two known
dumps — and `fmpac.xml`'s own comment notes FM-PAC dumps vary precisely in the memory-mapped-register
region, so variance in exactly this ROM is EXPECTED, not a defect. **Therefore S-d's evidence gate
is FUNCTIONAL, not hash-matching:** record the actual SHA1 (`2dc4517e…`) and disclose it as a
"non-canonical-but-signature-valid FM-PAC variant" — never claim it matches an openMSX dump.

**Consequence for this package:** the FM-PAC peripheral's real-software YS II SRAM-save validation
is **NO LONGER BLOCKED**. Slice S-d is now a *full completion path*, not a partial/deferred one.
Its acceptance criteria and test obligations include the real `roms/fmpac.rom` load plus a YS II
SRAM-detect + save-persist SYSTEM test — not synthetic unit tests alone. (The previous brief's
"blocked on human-provided SW-M004" disposition is retired.)

### 0.2 Completion criterion (human-set, DEC-0049/DEC-0050)

> "YS II fully playable like real hardware — building interiors load, FM-PAC S-RAM saves work
> (peripheral cart) AND/OR disk saves persist — verified by msx-playtest"

Decomposed:
- **(C-build)** Building interiors load (NOT a black screen). **Mandatory, non-negotiable.** → S-b.
- **(C-save)** Save persistence via **disk saves (bare machine)** OR **FM-PAC SRAM (cart inserted)**.
  AND/OR → EITHER slice S-c OR S-d satisfies it; this package delivers BOTH. → S-c + S-d.
- **(C-verify)** Verified by msx-playtest. Requires the agent to be spawnable → S-a enabler.

---

## 1. Scope and Assumptions

### 1.1 In scope (device-level + wiring + additive CLI only)

- **S-a** — msx-playtest agent registration fix (add YAML frontmatter). Orchestration-layer only.
- **S-b** — Bug B ("black screen on building entry"): repro-first → root-cause → universal fix.
- **S-c** — Disk-save PERSISTENCE: WD2793 write path → `DiskImage` host-file flush → `--disk-writable`
  opt-in → committed formatted-blank-720KB-disk asset/tool.
- **S-d** — FM-PAC peripheral cartridge: new `CartridgeMapperType::FmPac`, ROM-bank window + 8 KB
  battery SRAM + magic unlock + bank/enable registers, loadable via `--cart roms/fmpac.rom`; `.sram`
  load/save persistence reusing the M17 `BatteryBackedSram` primitive.
- **S-e** — Reconcile / remove the internal machine `sram_` (system-wide, per §2.4).
- **S-f** — R-M35-1 ride-along: strengthen the multi_disk integration test to assert disk-index
  advancement.

### 1.2 Out of scope (unchanged from the base package unless noted)

- FM-PAC / YM2413 FM audio SYNTHESIS depth beyond what M31 (backlog E1) already delivers — the
  FM-PAC's OPLL is the same YM2413 already implemented; S-d wires the *memory-mapped register
  overlay + SRAM*, not new DSP.
- Any `src/devices/cpu/` or `src/core/` edit (ZEXALL stays WITHDRAWN per `feedback_slow_test_cadence`;
  none expected — see §6.4).
- Cartridge types beyond the existing six MVP + KonamiSCC + the new FmPac (backlog G3/G4 deferred).
- Cassette/printer/flux-format depth (F1/F2/C10), VDP rendering remainders (D8/D9/D10).
- Fixing Bug B before its root cause is established (repro-first, `feedback_universal_fixes_only`).

### 1.3 Assumptions (each with a verification action)

| # | Assumption | Verification action |
|---|---|---|
| A1 | YS II is at `disks/games/ys2/disk1.dsk` + `disk2.dsk` (path CORRECTED from the base package's `disks/games/ys2_disk1.dsk`). | VERIFIED present via `Glob disks/**/*.dsk` → `disks\games\ys2\disk1.dsk`, `disks\games\ys2\disk2.dsk`. |
| A2 | `roms/fmpac.rom` is a GENUINE but NON-CANONICAL FM-PAC BIOS variant (16 KB, "PAC2OPLL"@0x4018, SHA1 `2dc4517e…`, matching NEITHER openMSX canonical dump). | VERIFIED present via `Glob roms/**`. Developer re-computes SHA1 (`tools/checksum-assets.ps1`) and discloses it as a signature-valid non-canonical variant vs the two `fmpac.xml` dumps — do NOT set a hash-match gate; validation is FUNCTIONAL (§0.1, AC-d7). |
| A3 | On a bare HB-F1XV, YS II falls back to DISK saves (no internal SRAM); WITH the FM-PAC cart inserted, its SRAM-save path activates. | S-b/S-c/S-d msx-playtest runs observe which path YS II actually exercises in each configuration; verify empirically, do not presume. |
| A4 | The disk WRITE path stops at in-memory `DiskImage::data_` (no host-file persistence). | VERIFIED by read: `src/devices/fdc/disk_image.{h,cpp}` has NO path member / no `ofstream` (§2.2). |
| A5 | Bug B is either FDC-pipeline (interior-load sector) or VDP-blank, not CPU. | S-b full-pipeline diagnostic (§2.3) discriminates; honest "inconclusive" allowed, no speculation. |
| A6 | openMSX (WSL) is available for A/B on the FM-PAC memory behavior + Bug B pipeline. | If unavailable, downgrade to "developer-verified, A/B pending" and document as a blocker — never fabricate a diff. |

---

## 2. System-Wide Review FIRST (per `feedback_system_wide_review_first`)

Whole-system architectural review of each work item BEFORE any slicing/coding. All claims below
are grounded in files read this cycle; concrete citations inline.

### 2.1 FM-PAC peripheral cartridge — memory map + slot coexistence

**Authoritative memory map** — from `references/openmsx-21.0/src/sound/MSXFmPac.cc` (read this
cycle). The device is a `MSXMusicBase` subclass occupying **page 1 (CPU 0x4000-0x7FFF)** of its
slot; every access is masked `address &= 0x3FFF` (relative to 0x4000). Translating openMSX's
relative addresses to the CPU-visible ABSOLUTE addresses:

| CPU address | openMSX rel. | Function | Grounding |
|---|---|---|---|
| 0x4000-0x5FFD | 0x0000-0x1FFD | 8 KB battery SRAM window **when unlocked**; else ROM bank read | `MSXFmPac.cc:43-55` |
| 0x5FFE | 0x1FFE | `r1ffe` magic register — write `0x4D` (gated by enable bit-4); read returns 0x4D when unlocked | `MSXFmPac.cc:44-48,84-89` |
| 0x5FFF | 0x1FFF | `r1fff` magic register — write `0x69` (gated by enable bit-4); read returns 0x69 when unlocked | `MSXFmPac.cc:44-49,90-95` |
| 0x7FF4 / 0x7FF5 | 0x3FF4 / 0x3FF5 | YM2413 memory-mapped address / data port | `MSXFmPac.cc:96-99` |
| 0x7FF6 | 0x3FF6 | Enable register: bit-0 = I/O-port enable; bit-4 = force-reset latches (`enable = value & 0x11`) | `MSXFmPac.cc:38-39,100-106` |
| 0x7FF7 | 0x3FF7 | Bank register: `bank = value & 0x03` (selects 16 KB ROM bank) | `MSXFmPac.cc:40-41,107-113` |
| 0x4000-0x7FFF (locked) | 0x0000-0x3FFF | ROM read: `rom[bank * 0x4000 + (addr & 0x3FFF)]` | `MSXFmPac.cc:53-54` |

**Unlock state machine** (`MSXFmPac.cc:137-144`): `sramEnabled = (r1ffe == 0x4D) && (r1fff == 0x69)`,
re-evaluated on EVERY write to 0x5FFE/0x5FFF and on the enable-bit-4 force-reset. Both bytes
required (AND, not either-or).

**CORRECTION of the base package (evidence-grounded):** `docs/m36-planner-package.md` §2.4 / Appendix B
listed the registers at `0x3FFE/0x3FFF` and `0x3FF4-0x3FF7` and an impossible "0x4000-0x3FFD"
range — those are the openMSX *relative* addresses mislabeled as CPU-visible ones. The CPU-visible
addresses are **0x5FFE/0x5FFF (magic) and 0x7FF4-0x7FF7 (ports/enable/bank)**, exactly as DEC-0050
states. S-d MUST use the absolute addresses in this table.

**SRAM size nuance:** openMSX declares the SRAM as `0x1FFE` = **8190 bytes** (`MSXFmPac.cc:11`), not
8192 — the physical 8 KB chip's top two bytes are shadowed by the r1ffe/r1fff magic registers. The
`.sram` persistence file in openMSX carries the header `"PAC2 BACKUP DATA"` (`MSXFmPac.cc:7`).
S-d grounds the SRAM window size in this reference (0x1FFE addressable); matching the PAC2 header
for cross-tool `.sram` compatibility is OPTIONAL (note it, developer's judgment).

**ROM-bank vs 16 KB image:** `roms/fmpac.rom` is exactly one 16 KB bank. `rom[bank*0x4000 + …]` for
`bank>0` would run past a 16 KB image; the developer masks the bank against the actual ROM size
(wrap within available banks), grounded in how openMSX's `Rom` object sizes the image — verify, do
not fabricate.

**Slot coexistence (the whole-system decision):**
- Cartridges attach at **all 4 pages of primary slots 1 & 2, sub 0** —
  `hbf1xv_machine.cpp:126-128` (`slot_bus_.attach(1,0,page,&cartridge_slot1_)` for page 0..3). The
  `CartridgeSlot` device sees the full address space; the *mapper* decides which pages respond. The
  FmPac mapper must respond ONLY on page 1 (0x4000-0x7FFF) and present open-bus on pages 0/2/3
  (matching a bare FM-PAC cart, which is a page-1 device).
- `load_cartridge(slot, type, image)` → `cartridge_slotN_.load(type, image)`
  (`hbf1xv_machine.cpp:885-896`). S-d adds an `FmPac` arm to that `load()` dispatch — no new slot
  wiring, reusing the M19/M29 cartridge-device pattern (`cartridge_konami_scc_rom.*` is the closest
  precedent: a mapper device with an embedded sound chip).
- **Must NOT regress the built-in MSX-MUSIC** (slot 3-3, `fmmusic_rom_`, `hbf1xv_machine.cpp:112,402`),
  which stays SRAM-less. The internal MSX-MUSIC is I/O-port-only (#7C/#7D); the FM-PAC's OPLL is
  gated by its own enable-bit-0 and its I/O ports are the cartridge's own — coexistence must be
  proven by a regression test (S-d cross-subsystem tests).
- **Must NOT regress the memory-mapper** (slot 3-0, #FC-#FF): page-1 mapper RAM must still be
  reachable when NO cartridge occupies page 1, and cartridge-page-1 priority over slot 3-0 must
  follow the existing `SlotBus` #A8 primary-slot decode — no new priority logic, reuse `SlotBus`.

### 2.2 Disk-write PERSISTENCE — the full pipeline (grounded, read this cycle)

The write path EXISTS in-memory and STOPS before the host file:

```
WD2793 WRITE SECTOR / WRITE TRACK          src/devices/fdc/wd2793.cpp
  begin_write_sector (433) → DRQ transfers → finish_write_sector (463)
  finish_write_sector: drive_->write_sector(write_sector_num_, buffer_.data())   (464)
  parse_write_track_byte → drive_->write_sector(wt_sector_num_, wt_data_.data())  (603)
        ↓
DiskDrive::write_sector                     src/devices/fdc/disk_drive.cpp:97-102
  → image_->write_chs(physical_track_, side_, sector, in)
        ↓
DiskImage::write_chs → write_lba            src/devices/fdc/disk_image.cpp:117-142
  → writes into in-memory std::vector<uint8_t> data_        [STOPS HERE — no host flush]
```

**Finding (VERIFIED by read):** `DiskImage` (`disk_image.h:34-86`) has **no path member, no
`ofstream`/`fwrite`, no flush**. It is constructed either from `synthesize()` (default) or from a
`std::vector<uint8_t> bytes` (in-memory). So **disk-save-PERSIST is an IMPLEMENT task, not a
verify.** The write commands, DRQ handling, and in-memory sector write all work already; what is
missing is (a) associating a host `.dsk` path with the mounted image, and (b) flushing written
sectors back to that host file.

**Where the host path lives today (two load sites, both discard the path):**
- Headless `--disk` (`src/main.cpp:872-879`): reads host bytes → `DiskImage(std::move(bytes))` →
  `attach_image`. Path NOT retained.
- SDL3 (`src/frontend/sdl3_app.cpp:104-124`): reads ALL `config_.disk_paths` into `disk_images_`
  (in-memory byte vectors) AND retains `config_.disk_paths`; constructs
  `DiskImage(disk_images_[0])` → `attach_image`. **On F11 swap (`sdl3_app.cpp:322-325`): the current
  `DiskImage` is DISCARDED and replaced from `disk_images_[i]` — any writes are LOST**, and the
  in-memory `disk_images_[i]` copy is never updated either.

**System-wide design (the whole-system decision, per memory):**
1. **Host-file backing on `DiskImage`** — add an optional `std::filesystem::path host_path_` +
   `bool dirty_` + a `flush()` that writes `data_` back to `host_path_`. `flush()` is write-only
   OUTPUT: it MUST NOT re-read the host file into emulation state (determinism — emulation reads
   only `data_`, loaded once at attach).
2. **Safety default = NON-DESTRUCTIVE.** Writes stay in-memory (today's behavior) unless the user
   opts in with a new additive **`--disk-writable`** flag. Never clobber a real `.dsk` by default.
   Game disks (disk1/disk2) default write-protected (`DiskImage::write_protected_`, already exists,
   `disk_image.h:76-77`); the dedicated SAVE disk is the writable target.
3. **Flush trigger** — flush on clean shutdown and/or after a completed sector write. Simplest
   deterministic policy: mark `dirty_` on `write_lba`, flush at shutdown (and before a swap
   discards a dirty writable image). Per-write write-through is also acceptable if deterministic.
4. **Multi-disk interaction** — before a swap discards a dirty writable image, flush it AND update
   `disk_images_[current_disk_index_]` so a swap-back sees the writes. This is the exact seam S-f
   also touches; sequence S-c before/with S-f.
5. **Determinism** — given identical inputs, the same sectors are written → the host file receives
   byte-identical content. A write-back is deterministic because it is a pure function of the final
   `data_`. Add a determinism test: two identical scripted write runs produce byte-identical output
   `.dsk` files.

**Formatted-blank-disk asset:** `DiskImage::synthesize()` already produces a valid FAT12 720 KB
blank (media 0xF9, `disk_image.cpp:38-85`). Provide a **`tools/` script** (PowerShell or Python)
and/or a headless `--format-disk <path>` additive mode that writes exactly that deterministic
737,280-byte image to a host `.dsk`, so YS II's DISK save has a real formatted target. Prefer a
tool over committing a binary under `disks/` (per DEC-0047 `disks/` content is untracked; a
committed blank would need `git add -f` and adds a binary — a generator tool is cleaner and matches
CLAUDE.md's "generate all needed tools in `tools/`"). Repo hygiene per
`feedback_single_build_repo_hygiene`: one tool, concise README entry, no stray artifacts.

**openMSX A/B honesty:** disk write-back has **no direct openMSX CPU-trace A/B** — comparing host
`.dsk` bytes is not an instruction/register trace. The CPU-visible FDC write *protocol* (DRQ/INTRQ,
status) is already M16-A/B-proven. S-c states this honestly: the write-BACK persistence is verified
by round-trip (write → flush → reload → read-back identical), not by an openMSX diff.

### 2.3 Bug B — black-screen-on-building-entry (REPRO-FIRST, full-pipeline)

Per `feedback_universal_fixes_only` and `feedback_system_wide_review_first`, **no fix is in scope
until the root cause is localized to exactly one pipeline stage.** The base package's §2.5 / §6
full-pipeline diagnostic plan remains VALID and is adopted here by reference; the Phase-2 refinements:

- **Repro is now first-class** via the msx-playtest agent (spawnable after S-a) driving the
  deterministic `--input-script` + `--dump-frame` harness (M27 tooling, `src/machine/input_script.*`,
  `tools/frame-to-png.py`) to reach "just before building entry", then across the transition.
- **Candidate root causes to discriminate** (deterministic evidence in parentheses):
  1. **FDC interior-load read fails** — a READ SECTOR for the interior data never completes: INTRQ
     never raised / DRQ stalls / wrong track after a seek (evidence: FDC status/command/track/sector
     register file + INTRQ/DRQ timeline at the black frame; CPU stuck in a wait loop, PC frozen).
     Ground WD2793 behavior in `references/openmsx-21.0/src/fdc/WD2793.cc` and cross-ref
     `references/fmsx-60/source/EMULib/WD1793.c`.
  2. **VDP display-blank (R#1 BL / mode)** — the game cleared display-enable or switched mode and
     the render gate blanks (evidence: VDP R#1 bit-6, mode registers, VRAM sample non-zero where
     tiles/sprites belong). Note the M34/DEC-0045 precedent: an R#1 BL render gate was the Metal
     Gear room-transition root cause — the same gate could plausibly recur here; check it explicitly.
  3. **Mapper/slot mis-route** — the interior loader bank-switches (#FC-#FF or a cartridge bank) and
     reads/executes the wrong page (evidence: #A8 + mapper segment registers + slot decode at the
     black frame vs openMSX).
- **Discriminator:** run the identical script on openMSX (WSL) and diff FIRST-divergence — whichever
  of FDC-state / CPU-PC / VDP-R#1 / VRAM diverges first names the stage. If openMSX also blacks out,
  it is game behavior, not a bug (record honestly; both behavior references consulted per guardrails).
- **Then, and only then, a UNIVERSAL fix** modeling the real hardware behavior generically — never
  keyed to YS II or to "the building interior". Fix scope is confined to the one diverging stage
  (FDC state machine, or the VDP render gate, or the slot/mapper route).
- **msx-playtest verification obligation:** after the fix, the same deterministic script must show
  the building interior RENDERING (not black), re-played byte-for-byte, and the end-to-end YS II
  "building interiors load" signal captured as a frame PNG.

### 2.4 Internal `sram_` reconciliation — every reference (system-wide, so removal breaks nothing silently)

The inert machine `sram_` (`hbf1xv_machine.h:559`, `MemoryRegion sram_{kSramBytes}`,
`kSramBytes = 8*1024`) is the speculative FM-PAC region DEC-0050 orders removed/relocated. EXHAUSTIVE
consumer list (grep'd this cycle — these are the ONLY references to the machine-level `sram_`, and
are distinct from `halnote_.sram()` (16 KB MSX-JE) and `s1985_engine`'s 16-byte `sram_`, which MUST
NOT be touched):

| Location | Use | Impact of removal |
|---|---|---|
| `hbf1xv_machine.h:189` | `kSramBytes = 8*1024` constant | Remove/relocate |
| `hbf1xv_machine.h:192` | `sram_size()` decl | Remove or repoint |
| `hbf1xv_machine.h:559` | `MemoryRegion sram_{kSramBytes}` member | Remove from bare machine |
| `hbf1xv_machine.cpp:262` | `sram_.clear()` in cold_boot | Remove |
| `hbf1xv_machine.cpp:783-784` | `sram_size()` impl | Remove or repoint |
| `hbf1xv_machine.cpp:1078,1082` | `sram()` const + non-const accessors | Remove or repoint to FM-PAC cart |
| `hbf1xv_machine.cpp:1115` | **state dump `serialize_region("SRAM", sram_.data(), …)`** | **Format-affecting — see below** |
| `tests/unit/machine/hbf1xv_memory_regions_unit_test.cpp:100-164` | asserts `sram_size()==8KB`, zero-init, R/W, out-of-range, region-independence, cold-boot re-zero | **Test file must be updated** |
| `tests/integration/machine/hbf1xv_memory_regions_integration_test.cpp:67-102` | `sram().write/dump/load`, "RunProgram_SramRegion_Unperturbed", A/B determinism | **Test file must be updated** |

**State-dump decision (the silent-breakage risk):** `serialize_state_dump()`
(`hbf1xv_machine.cpp:1109-1122`) emits a `"SRAM"` section between DRAM and VRAM. The M10/M13 dump
golden expectations reference this format. Two safe options — the developer picks one and records it:
- **(a) Relocate:** the `"SRAM"` section reflects the inserted FM-PAC cartridge's SRAM when a
  FmPac cart is present, and is EMPTY/ABSENT on a bare machine. Preserves accuracy (bare machine has
  no SRAM) but changes the dump for the bare case.
- **(b) Keep an empty region** for format stability: emit a zero-length or explicitly-empty `"SRAM"`
  section on the bare machine so no golden/tool that parses the section header breaks.

Either way, the two memory-region test files MUST be updated in the SAME slice, and any golden
state-dump fixture re-checked. **Sequence S-e AFTER S-d** so the relocation target (the FM-PAC
cartridge SRAM) exists before `sram_` is removed.

---

## 3. Milestones (Slice Decomposition + Ordering)

**Ordering rationale.** The completion criterion makes "building interiors load" (C-build)
**mandatory** and the save clause **AND/OR** (C-save satisfied by EITHER disk-save OR FM-PAC). So
the non-blocked, completion-critical path leads; the FM-PAC path — now unblocked by `roms/fmpac.rom`
— is a full second save path, not a deferred one. Sequence:

```
S-a  msx-playtest frontmatter enabler (trivial; enables spawnable repro agent)
  └─> S-b  Bug B repro → root-cause → universal fix   [C-build: MANDATORY, highest priority]
        └─> S-c  Disk-save persistence + formatted-blank tool  [C-save path #1: bare machine, no cart asset]
              └─> S-d  FM-PAC peripheral cartridge + real roms/fmpac.rom + YS II SRAM system test
                        [C-save path #2: peripheral; now a FULL path, unblocked]
                    └─> S-e  Reconcile/remove internal sram_ (relocation target now exists)
                          └─> S-f  R-M35-1 multi_disk test strengthening (rides the S-c swap seam)
```

S-b and S-c/S-d are independent in principle (Bug B is FDC-or-VDP; saves are FDC-persist / cart);
they may proceed in parallel if capacity allows, but C-build (S-b) is the priority gate.

### S-a — msx-playtest agent registration enabler

- **Problem (VERIFIED):** `.claude/agents/msx-playtest.md` starts with `# MSX Playtest Agent` and
  has NO YAML frontmatter, unlike `.claude/agents/msx-qa.md:1-6` (`---\nname: msx-qa\ndescription:
  …\ntools: …\nmodel: opus\n---`). So the agent does not register / is not spawnable via the Agent
  tool. The `/msx-playtest` command works independently.
- **Fix:** prepend a frontmatter block (`name: msx-playtest`, `description:`, `tools: Read, Grep,
  Glob, Write, Edit, Bash, TodoWrite`, `model: opus`) matching the msx-qa pattern. Content-only,
  orchestration layer; takes effect next session.
- **No `src/` edit. No test target.** Evidence: file parses as valid agent frontmatter; a
  side-by-side structural match to `msx-qa.md`.

### S-b — Bug B: black-screen-on-building-entry (repro → root-cause → universal fix) [MANDATORY]

- Per §2.3. Deliverables: deterministic repro script `debug/playtest_ys2_building_entry_*.script`;
  full-pipeline state dumps (FDC/CPU/VDP) + frame PNG sequence across the transition;
  `docs/m36-bug-b-root-cause.md` naming the FIRST diverging stage with cited evidence; the universal
  fix confined to that stage; a regression/integration test exercising the building-load path.
- **Fix location conditional on root cause:** FDC (`src/devices/fdc/wd2793.*`), VDP render gate
  (`src/devices/video/*` / the machine render-hook), or slot/mapper (`src/devices/chipset/*`). All
  device-level — expect zero cpu/core edits.

### S-c — Disk-save persistence + formatted-blank-disk tool [C-save path #1]

- Per §2.2. Deliverables: host-file backing on `DiskImage` (`host_path_` + `flush()`); additive
  `--disk-writable` flag on both executables (headless `src/main.cpp`, SDL3 `sdl3_cli`); flush on
  shutdown (+ before swap-discard of a dirty writable image); `tools/format-blank-disk.ps1` (or a
  `--format-disk` mode) producing the deterministic 737,280-byte FAT12 blank; `tools/README.md`
  entry. Default (no `--disk-writable`) = byte-for-byte unchanged, in-memory-only (safety).

### S-d — FM-PAC peripheral cartridge mapper [C-save path #2, now a FULL path]

- Per §2.1. Deliverables: `CartridgeMapperType::FmPac` (+ canonical name string, following the
  `cartridge_mapper_type.h` verbatim-openMSX-name discipline; openMSX's RomInfo name for this type
  is "FMPAC" — verify in `references/openmsx-21.0/src/memory/RomInfo.cc` before adding);
  `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}` implementing the §2.1 map (page-1 window, 4-bank
  ROM, 8 KB SRAM, 0x5FFE/0x5FFF magic unlock, 0x7FF6 enable, 0x7FF7 bank) grounded verbatim-behavior
  (not verbatim-code) in `references/openmsx-21.0/src/sound/MSXFmPac.cc`; SRAM via the M17
  `src/devices/memory/battery_backed_sram.*` primitive; `.sram` load/save mirroring the M20 Halnote
  path (`set_halnote_sram_path`/`flush_halnote_sram` pattern, `hbf1xv_machine.cpp:1062-1074`); an
  `FmPac` arm in `load_cartridge`; `--cart roms/fmpac.rom` loads it.
- **Real-software validation (now unblocked):** load the real `roms/fmpac.rom` + YS II; observe (via
  msx-playtest) YS II detect the FM-PAC ("PAC2OPLL"@0x4018 read) and SRAM-detect succeed; drive a
  save; confirm the `.sram` persists across a reload.

### S-e — Reconcile / remove internal machine `sram_`

- Per §2.4. Remove `sram_`/`kSramBytes`/`sram_size()`/`sram()` from the bare machine (or repoint to
  the FM-PAC cartridge SRAM); decide + record the state-dump `"SRAM"` section handling; update
  `hbf1xv_memory_regions_unit_test.cpp` and `hbf1xv_memory_regions_integration_test.cpp`; re-check
  any state-dump golden. Bare-machine invocation stays accurate (no phantom internal SRAM).

### S-f — R-M35-1 multi_disk test strengthening (ride-along)

- Per DEC-0049 R-M35-1. The current `tests/integration/frontend/sdl3_app_multi_disk_integration_test.cpp`
  (IT-2, lines 144-167) does NOT press F11 and does NOT assert `current_disk_index_` advances — it
  only checks `disk_drive().image() != nullptr`. Strengthen: expose a callable swap seam (e.g. a
  public `Sdl3App::swap_to_next_disk()` and/or a `current_disk_index()` accessor) so the test can
  assert the index advances 0→1 after a swap (without SDL event injection), and add an adversarial
  mutation (hard-code index 0) that MUST fail the test. Sequence with/after S-c so the swap seam and
  the write-back/flush-before-discard interaction are settled together.

---

## 4. Acceptance Criteria (mapped to the completion criterion; honest disposition)

Legend: **[ACHIEVABLE]** this cycle · **[A/B]** behavior-affecting, needs openMSX diff (or honest
"blocked/unavailable") · **[PLAYTEST]** verified by the msx-playtest agent.

### S-a
- AC-a1 `.claude/agents/msx-playtest.md` opens with valid YAML frontmatter (`name`, `description`,
  `tools`, `model: opus`) structurally matching `.claude/agents/msx-qa.md`. **[ACHIEVABLE]**
- AC-a2 The agent is spawnable via the Agent/Task tool next session (structural verification only).

### S-b — C-build (MANDATORY)
- AC-b1 Deterministic repro: `debug/playtest_ys2_building_entry_*.script` re-plays byte-for-byte
  identically twice (prefix invariant). **[ACHIEVABLE][PLAYTEST]**
- AC-b2 Full-pipeline evidence captured at the black frame: FDC register file + INTRQ/DRQ timeline,
  CPU PC + halted flag, VDP R#1 + mode + VRAM sample, frame PNG sequence (pre → black). **[ACHIEVABLE]**
- AC-b3 `docs/m36-bug-b-root-cause.md` names the FIRST diverging pipeline stage with cited evidence;
  if inconclusive, says so honestly (no speculation, do NOT proceed to a fix). **[A/B]**
- AC-b4 Universal fix confined to the diverging stage; never YS II-keyed
  (`feedback_universal_fixes_only`). **[ACHIEVABLE]**
- AC-b5 Post-fix: the same script shows the **building interior RENDERING (not black)**; captured as
  a frame PNG; re-play deterministic. **[ACHIEVABLE][PLAYTEST]** — this is the C-build completion signal.
- AC-b6 Behavior-affecting fix carries an openMSX A/B (the repro now continues into the building on
  both), or an honest "openMSX unavailable" note. **[A/B]**

### S-c — C-save path #1 (disk saves persist, bare machine)
- AC-c1 `DiskImage` gains host-file backing + `flush()`; `flush()` is write-only (does not re-read
  into emulation state — determinism preserved). **[ACHIEVABLE]**
- AC-c2 `--disk-writable` opt-in; DEFAULT (absent) leaves single-`--disk`/no-cart invocations
  byte-for-byte unchanged and in-memory-only (safety: never clobber a real `.dsk`). **[ACHIEVABLE]**
- AC-c3 Round-trip persistence: scripted sector write → flush → reload the `.dsk` → read-back
  byte-identical to what was written. **[ACHIEVABLE]**
- AC-c4 Determinism: two identical scripted write runs produce byte-identical output `.dsk` files. **[ACHIEVABLE]**
- AC-c5 `tools/format-blank-disk.ps1` (or `--format-disk`) emits the deterministic 737,280-byte
  FAT12 720 KB blank (media 0xF9); documented in `tools/README.md`. **[ACHIEVABLE]**
- AC-c6 **End-to-end:** YS II drives a DISK save to the formatted-blank disk and the save PERSISTS
  across a fresh reload — verified by msx-playtest. **[ACHIEVABLE][PLAYTEST]** — satisfies C-save alone.
- AC-c7 openMSX A/B disposition recorded HONESTLY: write-back host-file persistence has no direct
  CPU-trace A/B; the CPU-visible FDC write protocol was M16-A/B-proven; verification is round-trip. **[A/B note]**

### S-d — C-save path #2 (FM-PAC SRAM saves, peripheral cart) — NOW UNBLOCKED
- AC-d1 `CartridgeMapperType::FmPac` + canonical openMSX name string added (name verified against
  `RomInfo.cc`). **[ACHIEVABLE]**
- AC-d2 `cartridge_fmpac_rom.{h,cpp}` implements the §2.1 CPU-absolute map exactly per
  `MSXFmPac.cc`: page-1 window; 4-bank ROM read `rom[bank*0x4000 + (addr&0x3FFF)]`; 8 KB SRAM;
  0x5FFE/0x5FFF magic unlock (both bytes, re-checked every write); 0x7FF6 enable (bit-0 I/O, bit-4
  force-reset); 0x7FF7 bank. **[ACHIEVABLE]**
- AC-d3 Unit test (100% path coverage): lock→unlock→SRAM R/W→relock→ROM read; bank register affects
  ROM only (SRAM window unchanged when unlocked); enable-bit-4 force-reset clears latches; enable-bit-0
  gates I/O only. **[ACHIEVABLE]**
- AC-d4 Cross-subsystem regression: FM-PAC page-1 priority vs slot 3-0 mapper; internal MSX-MUSIC
  (#7C/#7D) unaffected by FM-PAC presence; Konami/KonamiSCC/ASCII*/Generic* carts still load
  unchanged. **[ACHIEVABLE]**
- AC-d5 `.sram` persistence via `BatteryBackedSram`: save/load round-trip survives across two machine
  instances (mirroring the M20 Halnote `.sram` test). **[ACHIEVABLE]**
- AC-d6 **Real-software SYSTEM test:** `--cart roms/fmpac.rom` + YS II → YS II detects the FM-PAC
  ("PAC2OPLL"@0x4018) and SRAM-detect SUCCEEDS ("NO S-RAM AVAILABLE" gone); a save writes the
  `.sram`; the save PERSISTS across reload — verified by msx-playtest. **[ACHIEVABLE][PLAYTEST]** —
  satisfies C-save via the peripheral path.
- AC-d7 Provenance evidence gate — **FUNCTIONAL, not hash-matching:** developer records the actual
  `roms/fmpac.rom` SHA1 (`2dc4517e…`, via `tools/checksum-assets.ps1`) and DISCLOSES it as a
  signature-valid but NON-CANONICAL FM-PAC variant (matches neither `fmpac.xml` dump `fec451b9…` /
  `9d789166…`; FM-PAC dumps vary in the memory-mapped-register region per openMSX's own comment).
  **Do NOT require a hash match.** Validation is: the mapper reads "PAC2OPLL"@0x4018, the FM-PAC
  INIT/CALL-statement handler works, and the magic unlock + bank register behave per `MSXFmPac.cc`
  — demonstrated with the real ROM (AC-d6). No fabricated checksum/provenance/dump-identity claim. **[ACHIEVABLE]**
- AC-d8 **[A/B]** openMSX A/B on the FM-PAC MEMORY behavior: load the same `roms/fmpac.rom` in
  openMSX's `FM-PAC` device and diff the magic-unlock + SRAM-window read/write behavior against this
  mapper; empty diff or honest documented divergence. **[A/B]**

### S-e
- AC-e1 Bare-machine `sram_`/`kSramBytes`/`sram_size()`/`sram()` removed or repointed; no phantom
  internal 8 KB SRAM claimed on a bare HB-F1XV. **[ACHIEVABLE]**
- AC-e2 State-dump `"SRAM"` section handling decided + recorded (relocate-to-cart OR empty-for-format);
  any golden re-checked; both memory-region test files updated and passing. **[ACHIEVABLE]**
- AC-e3 `halnote_.sram()` (16 KB MSX-JE) and `s1985_engine` `sram_` (16 B) provably untouched. **[ACHIEVABLE]**

### S-f
- AC-f1 New multi_disk test case asserts `current_disk_index_` advances 0→1 after a swap. **[ACHIEVABLE]**
- AC-f2 Adversarial mutation (hard-code index 0) KILLS the test (`feedback_single_build_repo_hygiene`
  non-destructive mutation discipline per DEC-0049). **[ACHIEVABLE]**
- AC-f3 Zero regression: existing multi_disk cases pass; write-back-before-swap-discard (S-c) covered. **[ACHIEVABLE]**

### Global evidence gates (every slice touching build/tests)
- `tools/validate-assets.ps1` green (BIOS present + ≥1 ROM; `roms/fmpac.rom` counts). 
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` refreshed (includes `roms/fmpac.rom`).
- `cmake --build build --config Debug` succeeds (single canonical `build/`, DEC-0041).
- `ctest --test-dir build -C Debug --output-on-failure` — full suite green, zero regression M1-M35.
- ZEXALL/ZEXDOC: **WITHHELD** — no `src/devices/cpu/` or `src/core/` edit expected (§6.4); run ONLY
  if a genuine core edit appears (`feedback_slow_test_cadence`).

### Honest disposition summary
- **C-build** (building interiors load): fully **ACHIEVABLE** this cycle (S-b), MANDATORY.
- **C-save** (save persistence): **BOTH paths ACHIEVABLE** this cycle — disk-save (S-c) needs NO
  FM-PAC asset and alone satisfies the AND/OR clause; FM-PAC SRAM-save (S-d) is now a full second
  path, UNBLOCKED by `roms/fmpac.rom` (the earlier "blocked on SW-M004" disposition is retired).
- **C-verify** (msx-playtest): enabled by S-a; the playtest agent verifies C-build (AC-b5), the
  disk-save path (AC-c6), and the FM-PAC path (AC-d6). Whichever path YS II actually exercises on the
  bare machine vs with the cart is observed empirically (A3).

---

## 5. Risks

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **FM-PAC slot/window routing coexistence** — page-1 (0x4000-0x7FFF) FM-PAC must not shadow the slot 3-0 mapper on non-cart pages, nor collide with internal MSX-MUSIC #7C/#7D. | Med | S-d regression | Route ONLY page 1 to the FmPac mapper; reuse `SlotBus` #A8 decode (no new priority logic); cross-subsystem regression tests AC-d4; A/B AC-d8. |
| R2 | **Disk write-back corrupts a real `.dsk`** | Med | Data loss | Default in-memory-only; host flush ONLY under `--disk-writable`; game disks default write-protected; flush is write-only (no read-back); the SAVE target is the throwaway formatted-blank disk. |
| R3 | **Disk write-back non-determinism** | Low | Reproducibility | Flush is a pure function of final `data_`; never re-read host into emulation; determinism test AC-c4. |
| R4 | **Bug B root-cause ambiguity (FDC vs VDP vs mapper)** | Med | S-b stalls | Full-pipeline first-divergence diff vs openMSX; honest "inconclusive" allowed (do not fix blind); consult BOTH openMSX + fMSX references; M34 R#1-BL precedent flagged as a concrete VDP candidate. |
| R5 | **Removing `sram_` silently breaks the state dump / memory-region tests** | Med | Regression | §2.4 exhaustive consumer list; update both test files in-slice; decide + record the `"SRAM"` section handling; re-check golden; sequence S-e AFTER S-d so the relocation target exists. |
| R6 | **`roms/fmpac.rom` is a non-canonical dump variant** (SHA1 `2dc4517e…` matches neither openMSX `fmpac.xml` dump; FM-PAC dumps vary in the mapped-register region) | Known | S-d validation must not assume hash identity | FUNCTIONAL gate only (AC-d7): validate by "PAC2OPLL"@0x4018 read + magic-unlock/bank behavior + YS II SRAM-detect/persist; disclose the actual SHA1 as signature-valid-but-non-canonical; NEVER a hash-match or dump-identity claim. |
| R7 | **msx-playtest agent stalls reaching the building / save menu** | Med | Verification | Bounded iteration + human-hint fallback (base package §2.3); a hint is a manual screenshot, not a code change. |
| R8 | **openMSX unavailable (WSL) for A/B** | Low | Evidence downgrade | Document as "developer-verified, A/B pending" — never fabricate a diff (guardrails). |
| R9 | **Multi-disk swap discards a dirty writable image (S-c/S-f interaction)** | Med | Lost saves | Flush + update `disk_images_[i]` before swap-discard; S-f covers the seam. |

**Tag target:** **v1.0.37** on completion of the mandatory C-build (S-b) + at least one C-save path
(S-c and/or S-d) with QA sign-off, remaining slices (S-e/S-f) folded in. If Bug B (S-b) proves a
larger architectural item than one slice, S-b may split (root-cause tagged, fix to a follow-on) —
but C-build is the release-gating criterion, so the coordinator escalates rather than tagging
without it. Alternative: split v1.0.37 (harness enabler + saves) / v1.0.38 (Bug B fix) only if S-b
root-cause is genuinely inconclusive this cycle.

---

## 6. Developer Handoff

### 6.1 Recon verification (independently confirmed this cycle)
1. **msx-playtest not spawnable — CONFIRMED.** `.claude/agents/msx-playtest.md:1` = `# MSX Playtest
   Agent` (no frontmatter) vs `.claude/agents/msx-qa.md:1-6` (`---`/`name`/`tools`/`model: opus`). → S-a.
2. **FM-PAC asset — UPDATED (no longer absent).** `roms/fmpac.rom` is now present (`Glob roms/**`);
   `bios/` still has only `f1xvmus.rom` (the internal APRLOPLL BIOS), confirming the FM-PAC BIOS is a
   separate cartridge asset. Real-software validation UNBLOCKED. → S-d.
3. **Disk WRITE in-memory-only — CONFIRMED.** `DiskImage` (`disk_image.{h,cpp}`) has no path
   member/no `ofstream`; write path `wd2793.cpp:464/603 → disk_drive.cpp:97-102 → disk_image.cpp:117-142`
   ends at in-memory `data_`. Host-file persistence is an IMPLEMENT task. Load sites:
   `main.cpp:872-879`, `sdl3_app.cpp:104-124`, swap-discard at `sdl3_app.cpp:322-325`. → S-c.
4. **Machine `sram_` — CONFIRMED inert + speculative.** `hbf1xv_machine.h:176-189,559`; exhaustive
   consumer list in §2.4 incl. the state-dump at `hbf1xv_machine.cpp:1115` and two memory-region
   test files. → S-e.

### 6.2 Grounding files the developer must read before coding
- FM-PAC behavior: `references/openmsx-21.0/src/sound/MSXFmPac.cc` (§2.1 table) + `MSXFmPac.hh`;
  name string: `references/openmsx-21.0/src/memory/RomInfo.cc`; ROM sizing: openMSX `Rom` class.
- FDC write behavior: `references/openmsx-21.0/src/fdc/WD2793.cc` (+ cross-ref
  `references/fmsx-60/source/EMULib/WD1793.c`); disk image write-back: openMSX `DSKDiskImage.cc` /
  `SectorAccessibleDisk.cc` (behavior only — never copy).
- Reuse precedents: `src/devices/memory/battery_backed_sram.*` (M17), the M20 Halnote `.sram` path
  (`hbf1xv_machine.cpp:1062-1074`), `src/devices/cartridge/cartridge_konami_scc_rom.*` (M29,
  mapper-with-embedded-sound-chip pattern).

### 6.3 Suggested test targets (deterministic)
- **Unit:** `cartridge_fmpac_rom_unit_test.cpp` (S-d, 100% unlock/bank/enable path coverage);
  `disk_image_writeback_unit_test.cpp` (S-c, flush round-trip + write-protect gate + determinism).
- **Integration:** `hbf1xv_m36_fmpac_cartridge_integration_test.cpp` (S-d, real `roms/fmpac.rom`
  load + SRAM detect/persist); `hbf1xv_m36_disk_save_persist_integration_test.cpp` (S-c, scripted
  write → flush → reload → read-back); update `sdl3_app_multi_disk_integration_test.cpp` (S-f, index
  advance + swap-discard flush); update `hbf1xv_memory_regions_{unit,integration}_test.cpp` (S-e).
- **System / playtest:** YS II building-interior render (S-b, AC-b5); YS II disk-save persist (S-c,
  AC-c6); YS II FM-PAC SRAM-save persist (S-d, AC-d6) — all via the msx-playtest deterministic
  `--input-script`/`--dump-frame` harness.
- **A/B:** FM-PAC memory behavior vs openMSX `FM-PAC` (AC-d8); Bug B first-divergence pipeline diff
  (AC-b3/b6). Disk write-back: NO direct A/B (AC-c7, honest).

### 6.4 Scope guardrails (hard)
- **Device-level only:** `src/devices/` (fdc, cartridge, memory), `src/machine/` wiring,
  `src/frontend`/`src/main.cpp` ADDITIVE flags (`--disk-writable`, `--cart` FmPac, optional
  `--format-disk`). **Expect ZERO `src/devices/cpu/` and `src/core/` edits → ZEXALL stays WITHHELD**
  (`feedback_slow_test_cadence`; if a genuine core edit appears, one sweep is mandatory).
- **Additive / default-off:** absent `--disk-writable` and absent an FmPac cart, single-`--disk` /
  no-cart invocations stay **byte-for-byte unchanged**. The FM-PAC must NOT regress the built-in
  SRAM-less MSX-MUSIC or any existing cartridge (AC-d4).
- **Universal fixes only** (`feedback_universal_fixes_only`): Bug B's fix models real hardware
  generically — never keyed to YS II or "the building".
- **System-wide review first** (`feedback_system_wide_review_first`): §2 is the required pre-code
  review; do not localize a patch that can break a neighbor (state dump, mapper, MSX-MUSIC, swap).
- **Single build tree** `build/` only (DEC-0041); no per-agent trees; clean rebuild for QA.
- **No scope expansion** without a `agent-protocol/channels/decisions.md` entry (DEC-0050 already
  authorizes the FM-PAC-peripheral + disk-write scope).
- **No copying** `references/` code into `src/` (GPL / non-commercial isolation); behavior/API only.

### 6.5 Durable artifacts to produce (developer)
- `.claude/agents/msx-playtest.md` frontmatter (S-a).
- `docs/m36-bug-b-root-cause.md` (S-b).
- `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}` + enum arm (S-d).
- `DiskImage` host-file backing + `tools/format-blank-disk.ps1` (S-c).
- `docs/m36-implementation-report.md` handoff; refreshed `docs/asset-checksums.txt`;
  `docs/openmsx-ab-smoke.md` for the A/B obligations (or honest unavailable note).
