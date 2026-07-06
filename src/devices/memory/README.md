# `src/devices/memory/` — CPU-addressable memory devices

Device logic for the HB-F1XV CPU memory map (M13). These are `core::MemoryDevice`
implementations attached to the M11 `chipset::SlotBus` by the machine wiring
(`src/machine/hbf1xv_machine.cpp`). Composition (which device sits in which
slot/sub-slot/page, and asset loading) stays in `src/machine/` per `src/CLAUDE.md`.

## Files

- `rom_device.{h,cpp}` — `RomDevice`: a read-only window over a byte image. It is
  configured with the device `<mem base=.. size=..>` placement from the machine
  XML and maps the full CPU address to `image[address - base]` inside the window,
  returning open-bus `0xFF` outside it. Writes are ignored (mask ROM). One type
  backs BIOS/BASIC (0-0), SUB (3-1 p0), Kanji Driver/BASIC (3-1 p1-2), the DISK
  ROM presence (3-2 p1, `rom_visibility` page 1 only) and the FM-MUSIC ROM
  presence (3-3 p1).
- `memory_mapper_ram.{h,cpp}` — `MemoryMapperRam`: 64 KB RAM at slot 3-0 whose
  per-page 16 KB segment is selected by the M11 `#FC-#FF` registers. It is a pure
  CONSUMER of `chipset::MapperIo::segment(page)` — MapperIo remains the sole owner
  of the segment registers and the `100xxxxx` readback. The physical fold uses
  `segment & 3` (2-bit, 4 segments); the readback uses `0x80 | seg & 0x1F` (5-bit).
  Those two masks authentically differ.

## Grounding (read only; never copy into `src/` — GPL isolation, guardrails)

- Slot / page / device placement: `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`.
- Mapper cold-boot segment defaults (all 0) + unpopulated-segment wrap:
  `references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:47-83`.
- RAM power-on `initialContent` (alternating `00/FF`) decode + repeat-fill:
  `references/openmsx-21.0/src/memory/Ram.cc:37-78` and Sony_HB-F1XV.xml:129.
- Mapper readback (5-bit S1985): `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §4.

## Boundary rules

- No filesystem / asset-path knowledge here — that is `machine/rom_asset_loader`.
- No segment-register ownership here — that is `chipset/mapper_io`.
- No FM-PAC / FDC / Halnote / Kanji-font device internals (deferred milestones).
