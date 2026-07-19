# `src/devices/memory/` — CPU-addressable memory devices

Device logic for the HB-F1XV CPU memory map. These are `core::MemoryDevice`
implementations attached to the `chipset::SlotBus` by the machine wiring
(`src/machine/hbf1xv_machine.cpp`). Composition — which device sits in which
slot/sub-slot/page, and asset loading — stays in `src/machine/`.

## Files

- `rom_device.{h,cpp}` — `RomDevice`: a read-only window over a byte image. It is
  configured with the device `<mem base=.. size=..>` placement from the machine
  XML and maps the full CPU address to `image[address - base]` inside the window,
  returning open-bus `0xFF` outside it. Writes are ignored (mask ROM). One type
  backs BIOS/BASIC (0-0), SUB (3-1 p0), Kanji Driver/BASIC (3-1 p1-2), the DISK
  ROM presence (3-2 p1, `rom_visibility` page 1 only) and the FM-MUSIC ROM
  presence (3-3 p1).
- `memory_mapper_ram.{h,cpp}` — `MemoryMapperRam`: 64 KB RAM at slot 3-0 whose
  per-page 16 KB segment is selected by the `#FC-#FF` mapper registers. It is a pure
  CONSUMER of `chipset::MapperIo::segment(page)` — MapperIo remains the sole owner
  of the segment registers and the `100xxxxx` readback. The physical fold uses
  `segment & 3` (2-bit, 4 segments); the readback uses `0x80 | seg & 0x1F` (5-bit).
  Those two masks authentically differ.

## Grounding (read only; never copy into `src/` — GPL isolation, guardrails)

- Slot / page / device placement: openMSX 21.0: `share/machines/Sony_HB-F1XV.xml`.
- Mapper cold-boot segment defaults (all 0) + unpopulated-segment wrap:
  openMSX 21.0: `src/memory/MSXMemoryMapperBase.cc`.
- RAM power-on `initialContent` (alternating `00/FF`) decode + repeat-fill:
  openMSX 21.0: `src/memory/Ram.cc` and `Sony_HB-F1XV.xml`.
- Mapper readback (5-bit S1985): Yamaha S1985 MSX-ENGINE Chipset fact sheet §4.

## Boundary rules

- No filesystem / asset-path knowledge here — that is `machine/rom_asset_loader`.
- No segment-register ownership here — that is `chipset/mapper_io`.
- No FM-PAC / FDC / Halnote / Kanji-font device internals — those are implemented, but own
  folders elsewhere: `devices/cartridge/` (FM-PAC peripheral cartridge),
  `devices/fdc/`, `devices/halnote/`, `devices/kanji/`.
