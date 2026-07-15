# M42 openMSX A/B — mapper RAM detection (`--ram 512` internal topology)

DEC-0061 / M42. QA-run, 2026-07-12. Grounds the "software actually SEES the extra
RAM" end-to-end claim: a mapper-aware segment-detection probe run on BOTH engines
after a real Sony HB-F1XV BIOS boot.

## Environment

- openMSX **19.1** (debian), `/usr/bin/openmsx`, real Sony HB-F1XV system ROMs
  present (`~/.openMSX/share/systemroms/hb-f1xv*.rom`).
- Machine XMLs: stock `Sony_HB-F1XV.xml` (`<MemoryMapper id="Main RAM"><size>64</size>`)
  and a local override `HBF1XV512.xml` = the SAME XML with ONLY the Main RAM
  `<size>` changed `64 -> 512` (internal-512 topology — the same internal
  `<MemoryMapper>` our `--ram 512` implements, NOT an external `ram512kmapper` ext).
- Our engine: `build/Debug` (Debug, SDL3=ON), commit `3f69786`.

## Probe (identical intent on both engines)

Route CPU page 2 (`0x8000`) through the Main RAM segment register (openMSX
`{Main RAM regs}` index 2 == MSX I/O port `0xFE`), write the segment index into
each of 32 candidate 16 KB segments, then read them all back. The count of
DISTINCT read-back values == the number of independently addressable
(software-visible) 16 KB segments.

- openMSX driver: `/tmp/mapper_probe.tcl` (pauses emulation after 8 s of boot,
  then `debug write {Main RAM regs} 2 <seg>` + `debug write/read memory 0x8000`).
- Our driver: `tests/integration/machine/hbf1xv_ram_detect_boot_integration_test.cpp`
  (real bios/ cold-boot + 180 NTSC frames of live Z80, then the same segment walk
  through the CPU-visible SlotBus/IoBus decode — the identical `MapperIo::io_write`
  + `MemoryMapperRam` fold the fetch-executed `OUT`/`LD` use). Both sides drive the
  mapper via their engine's debug interface after a real BIOS boot — a symmetric,
  apples-to-apples comparison.

## Results — BYTE-FOR-BYTE MATCH

### Internal 512 KB

| Engine  | Main RAM bytes | READBACK        | DISTINCT |
| ------- | -------------- | --------------- | -------- |
| openMSX | 524288         | `0 1 2 … 31`    | **32**   |
| ours    | 524288         | `0 1 2 … 31`    | **32**   |

### Stock 64 KB (default)

| Engine  | Main RAM bytes | READBACK                     | DISTINCT |
| ------- | -------------- | ---------------------------- | -------- |
| openMSX | 65536          | `28 29 30 31` repeating (×8) | **4**    |
| ours    | 65536          | `28 29 30 31` repeating (×8) | **4**    |

Both the distinct counts AND the exact fold-aliasing sequence match. At 512 KB all
32 segments are independently addressable (software sees the full 512 KB); at stock
64 KB exactly 4 segments are real and the mapper folds `seg & 3` (high segments
mirror low — the wrap a BIOS/DOS RAM search relies on to size RAM).

## Raw openMSX output

```
########## HBF1XV512 (internal 512 KB override) ##########
MAINRAM_BYTES=524288
REGS_BYTES=4
PAGE2_RAMCHECK=170
READBACK=0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
DISTINCT_COUNT=32
########## Sony_HB-F1XV (stock 64 KB) ##########
MAINRAM_BYTES=65536
REGS_BYTES=4
PAGE2_RAMCHECK=170
READBACK=28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31
DISTINCT_COUNT=4
```

## Raw our-engine output

```
[M42] internal-512 READBACK=0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31  DISTINCT_COUNT=32
[M42] stock-64 READBACK=28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31 28 29 30 31  DISTINCT_COUNT=4
```
