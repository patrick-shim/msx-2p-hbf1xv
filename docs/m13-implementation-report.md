# M13 Implementation Report — RAM/ROM Memory Devices & Slot Population

- Milestone: M13 (REQ-M13-003), executed per `docs/m13-planner-package.md` in slice order S1→S5.
- Developer: MSX Developer Agent. Date: 2026-07-06.
- Build: `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` (Debug, MSVC multi-config).
- Result summary: all 5 slices implemented; **50/50 ctest pass** on a clean build; genuine openMSX
  A/B parity captured for both memory subjects. Not a QA sign-off (separate agent).

---

## 1. Slot map as built (from `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`)

Primary slots **0 and 3 both expanded** (A-6; XML lines 85-116 and 123-199). Page = 16 KB.

| Cell | Device | Window `<mem>` | Pages | Asset |
|---|---|---|---|---|
| 0-0 | BIOS + BASIC ROM | base 0x0000 size 0x8000 (l.97) | 0-1 | `bios/f1xvbios.rom` |
| 0-1/0-2 | empty | — | — | open bus |
| 0-3 | MSX-JE Halnote | — | — | **DEFERRED (D-3)** reserved open bus |
| 3-0 | Main RAM MemoryMapper 64K | base 0x0000 size 0x10000 (l.128) | 0-3 | RAM (A-5 init) |
| 3-1 | MSX Sub ROM | base 0x0000 size 0x4000 (l.144) | 0 | `bios/f1xvext.rom` |
| 3-1 | MSX Kanji Driver/BASIC | base 0x4000 size 0x8000 (l.156) | 1-2 | `bios/f1xvkdr.rom` |
| 3-2 | WD2793 DISK ROM presence | base 0x4000 size 0x4000, `rom_visibility` page 1 (l.174-175) | 1 | `bios/f1xvdisk.rom` |
| 3-3 | MSX-MUSIC ROM presence | base 0x4000 size 0x4000 (l.195) | 1 | `bios/f1xvmus.rom` |

Reset default flipped to authentic **`#A8 = 0`** (page 0 → slot 0-0 BIOS, PC = 0x0000); sub-slot
registers 0. RAM power-on content = XML `initialContent` alternating 00/FF pattern (A-5).

Out of scope (unchanged): Halnote banking + MSX-JE SRAM (0-3), Kanji-font `#D8-DB` I/O, FDC
mechanics, FM-PAC OPLL/`#7C-7D`, VRAM/V9958 (M14), cartridge slots 1-2.

## 2. Asset → slot mapping with REAL checksums

Local SHA1s were captured and **equal the XML "confirmed by Meits" revisions** (so both this
emulator and openMSX execute the same images). SHA256 from `docs/asset-checksums.txt`
(refreshed this run). Loader asserts each file's **size** matches the XML window; never asserts a
SHA (A-1).

| Asset | Size | Slot cell | SHA1 (== XML) | SHA256 |
|---|---|---|---|---|
| `bios/f1xvbios.rom` | 32768 | 0-0 p0-1 | `174c9254f09d99361ff7607630248ff9d7d8d4d6` | `75adcdb462137c61fa570a2773a8d27ce3eeaf18873b3ac500492b594a1a1b93` |
| `bios/f1xvext.rom` | 16384 | 3-1 p0 | `fe0254cbfc11405b79e7c86c7769bd6322b04995` | `36000685128f95ff515a128973f8d439116c1a1a8e38c1777293a428894434a2` |
| `bios/f1xvkdr.rom` | 32768 | 3-1 p1-2 | `dcc3a67732aa01c4f2ee8d1ad886444a4dbafe06` | `9fb7ef5003fcb953f46ffe5bab399832d8175f53cccb763d8c454a91bc2429fd` |
| `bios/f1xvdisk.rom` | 16384 | 3-2 p1 | `5a4e7dbbfb759109c7d2a3b38bda9c60bf6ffef5` | `b9e0f8db66c06069eae881096245666b798dcdc0461afb415a45a95476be8f9b` |
| `bios/f1xvmus.rom` | 16384 | 3-3 p1 | `aad42ba4289b33d8eed225d42cea930b7fc5c228` | `29325437196ec7fc7b39b25a87f6981805bd8cb302acf4708f504fb1fb89108b` |

Missing-asset policy (A-7): absent/unreadable/wrong-size → 0xFF-fill of the exact expected size +
a recorded diagnostic in `Hbf1xvMachine::rom_diagnostics()`; never silent, never fabricated. A
green run has `rom_diagnostics()` empty (verified by the slot-map + boot tests).

## 3. Files added / changed

**New source** (device logic in `src/devices/memory/`, wiring in `src/machine/`):
- `src/devices/memory/rom_device.{h,cpp}` — read-only ROM window (writes ignored; 0xFF outside).
- `src/devices/memory/memory_mapper_ram.{h,cpp}` — 64 KB RAM, consumes `MapperIo::segment()`
  (MapperIo stays the sole owner of `#FC-FF` + `100xxxxx` readback). Physical fold `seg & 3`.
- `src/devices/memory/README.md`.
- `src/machine/rom_asset_loader.{h,cpp}` — bios/ loader + missing-asset policy.

**Changed source:**
- `src/machine/hbf1xv_machine.{h,cpp}` — slot population per §1; `set_expanded(0,true)` +
  `set_expanded(3,true)`; `cold_boot` flips `#A8=0` + A-5 RAM pattern + `load_rom_assets()`;
  new `map_flat_ram()`, `set_asset_root()`, `rom_diagnostics()`, and non-perturbing debug seams
  `debug_bus_read/write`, `debug_io_read/write`, `slot_expanded()`.
- `src/main.cpp` — `--parity-trace` now calls `map_flat_ram()`; new `--bios-boot-trace` mode.
- `CMakeLists.txt` — add `rom_device.cpp`, `memory_mapper_ram.cpp`, `rom_asset_loader.cpp`;
  remove `ram_slot_backing.cpp`.
- **Deleted** `src/machine/ram_slot_backing.{h,cpp}` (retired, superseded by `MemoryMapperRam`).

**New tests** (5 executables): `devices_memory_rom_device_unit_test`,
`devices_memory_mapper_ram_unit_test`, `machine_rom_asset_loader_unit_test`,
`machine_hbf1xv_bios_boot_integration_test`, `machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`.

**New tooling / artifacts:** `tools/openmsx-mem-parity.ps1`, `tests/parity/m13_mem_probe.bin`,
`docs/m13-parity-trace-diff.md`, refreshed `docs/asset-checksums.txt`.

## 4. Grounding citations (behavior, never copied — GPL isolation)

- Cold-boot segment defaults (all 0) + unpopulated-segment wrap `segment & (numSegments-1)`:
  `references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:47-83`. For 64 KB (numSegments=4, a
  power of two) this equals `seg & 3`, as implemented in `MemoryMapperRam::physical_address`.
- RAM `initialContent` decode + repeat-fill: `references/openmsx-21.0/src/memory/Ram.cc:37-78`;
  pattern decoded from `Sony_HB-F1XV.xml:129` = `(00,FF)*128 + (FF,00)*128` (512 B) repeated.
- 5-bit S1985 readback vs 2-bit physical fold (A-3): `Sony_HB-F1XV.xml:25`,
  `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §4.

## 5. Per-slice test evidence

- **S1** `devices_memory_rom_device_unit_test`, `devices_memory_mapper_ram_unit_test`: ROM window
  read/write-ignore/open-bus + DISK page-1-only; mapper segment consume, linear {0,1,2,3}, wrap
  seg4→0 / seg0x25→1, readback independence (0x25→0x85 vs phys seg1). PASS.
- **S2** `machine_rom_asset_loader_unit_test`: real f1xvbios loads 32K, determinism, absent→0xFF
  fill + one diagnostic, small-window fill size. PASS.
- **S3** `machine_hbf1xv_slot_map_unit_test` (rewritten): each cell resolves to the loaded image
  (BIOS 0-0 p0-1, SUB 3-1 p0, Kanji 3-1 p1-2, DISK 3-2 p1 + p0 open-bus, FM-MUSIC 3-3 p1); both
  slots 0&3 expanded; `#FFFF` readback in both; A-5 DRAM pattern; segment-switch reroute. PASS.
- **S4** `machine_hbf1xv_bios_boot_integration_test`: bus[0x0000]==BIOS[0] (0xF3, not open bus);
  32-step image-grounded fetch invariant; deterministic across two boots; reached PC=0x0448. PASS.
- **S5** `machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`: RAM r/w (0xEE), segment
  switch readback (0x85), BIOS byte via slot switch (0xF3), determinism. PASS. Plus the openMSX
  A/B (§7).

## 6. Evidence gates (actual captured output, clean build)

```
tools/validate-assets.ps1                                  -> Asset validation result: True (exit 0)
tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt -> report written (exit 0)
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF              -> configure exit 0
cmake --build build --config Debug                         -> build exit 0, errors=0
ctest --test-dir build -C Debug --output-on-failure        -> 100% tests passed, 0 failed out of 50
```
(Compiler emits pre-existing C4819 code-page warnings for UTF-8 comment characters; no errors.)

## 7. A/B parity (REAL captures) — `docs/m13-parity-trace-diff.md`

openMSX 19.1 on WSL, machine `Sony_HB-F1XV` (real ROMs installed; SHA1s match §2). Comparator
`tools/trace-diff.py` over all architectural Z80 fields; VRAM excluded (M14).

- **Subject 1 (RAM/ROM probe, BIOS-independent):** ARCHITECTURAL PARITY — empty diff, 13/13
  instructions. RAM r/w = 0xEE, mapper `100xxxxx` = 0x85, BIOS byte = 0xF3, all identical.
- **Subject 2 (BIOS-boot checkpoint, authentic reset):** ARCHITECTURAL PARITY — empty diff at the
  committed K = 24, and still empty at K = 200 (BIOS boot stays in lockstep; no unimplemented
  device *read* reached in the window). Bounded before any VDP/PSG/RTC read (M14+ boundary).

Both B-side traces are non-empty genuine captures; the comparator self-check (empty→BLOCKED,
mismatch→DIVERGENCE) is the same one validated in M10–M12.

## 8. Prior-suite tests updated (justified, non-weakening)

The reset flip `#A8=0xFF`→`#A8=0` reroutes page 0 from RAM to BIOS, and A-5 replaces zero-init
DRAM with the alternating pattern; both are authentic. Every touched test was updated to the
authentic value with documented justification (mirrors the M11 R-3 discipline), never weakened:

- **`map_flat_ram()` added after `cold_boot()`** in the CPU-over-RAM tests so they page the flat
  64 KB RAM in EXPLICITLY (segments {0,1,2,3}) instead of relying on the old hidden default:
  `hbf1xv_cpu_step`, `hbf1xv_ldir_program`, `hbf1xv_cb_program`, `hbf1xv_indexed_program`,
  `hbf1xv_trace_program`, `hbf1xv_interrupt_ack`, `hbf1xv_cpu_parity`, `hbf1xv_memory_regions`
  (integration), `hbf1xv_debug_event_log`, `hbf1xv_parity_checkpoint`,
  `hbf1xv_m11_parity_checkpoint`, `hbf1xv_debug_dump` (unit), `cpu_bootstrap_im1_system`.
- **`hbf1xv_slot_map_unit_test`** — rewritten: reset assertion `#A8==0xFF`→`#A8==0` (A-2), plus
  the full M13 slot-map cross-checks against the loaded images.
- **`hbf1xv_memory_regions_unit_test`** — `ColdBoot_Dram_ZeroInitialized` → `..._A5AlternatingPattern`
  (and the re-boot case), asserting the exact XML pattern (R-3).
- **`hbf1xv_debug_dump_unit_test`** — the DRAM `[DRAM]` golden now asserts the WHOLE 64 KB region
  byte-for-byte via the production serializer over an independently-built A-5 pattern buffer
  (stronger than the old partial all-zero fold), plus an exact first-line anchor (R-3). CPU/SRAM/
  VRAM goldens unchanged.
- **`hbf1xv_cpu_parity_integration_test`** — the three OUTI probes retarget mapper port `#FC`→`#FD`
  (page-1 segment): under the now-REAL mapper, writing `#FC` would remap the executing page 0.
  Identical OUTI carry/counter, I/O dispatch, and `100xxxxx` readback behaviour is preserved.
- **`hbf1xv_system_bus_integration_test`** — rewritten to capture the I/O readbacks in CPU
  registers (B/C/D/E/H) instead of flat-RAM stores: with the real mapper + populated slots, a
  store to a page whose segment/sub-slot the program itself changes is no longer valid. Same five
  behaviours verified; the mapper-readback probe now targets `#FD` (non-executing page).

No prior test was disabled, deleted, or had an assertion relaxed.

## 9. Deviations / assumptions (with verification actions)

- **Diagnostics not mirrored to the event log.** The planner optionally mentioned appending
  missing-asset notes to the debug event log. Doing so would make the deterministic M10 event-log
  golden depend on whether the working directory resolves `bios/`. Decision: diagnostics go to
  `rom_diagnostics()` ONLY (the authoritative list), keeping the event stream byte-deterministic.
  *Verify:* `rom_diagnostics()` asserted empty in the slot-map + boot tests when assets are present.
- **Added non-perturbing debug seams** (`debug_bus_read/write`, `debug_io_read/write`,
  `slot_expanded`) to the machine — analogous to the existing `read_memory`/`load_memory` debug
  accessors and to CLAUDE.md's debug-capability mandate. They route through the real fabric (no
  cheating) and never advance the CPU/clock. *Verify:* used by the slot-map unit test; the boot
  and parity paths also confirm the same fabric via the CPU.
- **Boot checkpoint reaches PC=0x0448** self-derived from the image; the absolute value is
  cross-validated by the S5 openMSX A/B (empty diff to K=200), not hardcoded from memory.
- **A-4 wrap grounding:** for 64 KB the openMSX `segment < numSegments ? segment : segment &
  (numSegments-1)` reduces to `seg & 3`; cited in code at `memory_mapper_ram.{h,cpp}`.

## 10. QA handoff

Ready for QA regression assessment. Entry points: this report; `docs/m13-parity-trace-diff.md`;
gates in §6 (reproducible). No open blockers. Scope fences (D-3/D-4/D-5, FM-PAC/FDC/VDP) intact.
