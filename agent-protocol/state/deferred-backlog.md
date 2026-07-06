# Deferred Scope Backlog (authoritative)

Purpose: a single durable registry of every subsystem/behavior that has been
**explicitly deferred out of a completed or in-flight milestone**, so no future
planner or coordinator loses track of committed-but-not-yet-built scope.

Rules (per DEC-0005):
- Every planner package for a new milestone MUST consult this file and state which
  backlog items (if any) the milestone closes, and re-affirm the rest as still-open.
- When a milestone CLOSES a backlog item, mark it `DONE (Mxx)` here in the same cycle;
  do not delete the row (keep the history).
- Nothing here is a waiver — each item is committed scope from
  `agent-protocol/project-baseline.md` / the Target Machine Specification, merely
  sequenced to a later milestone.
- Ground each item in a concrete `references/...` path when it is planned.

Legend: Status = OPEN | IN-PROGRESS (Mxx) | DONE (Mxx). "Origin" = milestone/decision
that deferred it. "Candidate owner" = suggested future milestone (not binding).

---

## A. Explicitly requested to keep tracked (human directive, 2026-07-06)

The project owner directed on 2026-07-06 that the following remain recorded for later
milestone reference so "no one misses a bit":

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| B1 | **PSG / YM2149 device internals** (tone/noise/envelope/mixer, R0–R15, stereo A=C/B=L/C=R, joystick port A/B via R14/R15) — M11 S1985 provides only the I/O seam (`#A0-#A2`) | OPEN | M11 (DEC-0002) | Audio/PSG milestone | `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §2 |
| B2 | **RTC / RP5C01 device internals** (4 blocks × 13 regs, mode reg 13, BCD clock, alarm/timer, CMOS block-2 boot config) — M11 provides only the `#B4/#B5` seam gated by `#F5` bit7 | OPEN | M11 (DEC-0002) | RTC milestone | S1985 fact-sheet §5 |
| B3 | **FM-PAC (OPLL YM2413) device internals** — 9-ch FM synth. M13 maps only the FM-MUSIC ROM presence (slot 3-3); the sound-generation device is unbuilt | OPEN | M13 | Audio/FM-PAC milestone | Target Machine Spec (SOUND); `references/fact-sheets/` (OPLL sheet when added) |
| B4 | **MSX-JE 16 KB SRAM** (battery-backed) — FM-PAC/MSX-JE SRAM store; M13 mapped no SRAM device | OPEN | M13 | FM-PAC/SRAM milestone | S1985 fact-sheet §9; Target Machine Spec |
| B5 | **Kanji font ROM access via I/O `#D8-#DB`** — 256 KB JIS1+JIS2 font (`bios/f1xvkfn.rom`); M13 mapped the Kanji *driver* (slot 3-1) but not the I/O-accessed font | OPEN | M13 | Kanji/peripheral milestone | `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` (kanji I/O `#D8-DB`); S1985 fact-sheet §9 |
| B6 | **Halnote / MSX-JE firmware mapping** — slot 0-3, 1 MB `bios/f1xvfirm.rom`, `mappertype=Halnote` + SRAM; M13 left it reserved open-bus | OPEN | M13 | Halnote/firmware milestone | Sony_HB-F1XV.xml (slot 0-3 Halnote) |
| B7 | **Cartridge loading** — external primary slots 1 & 2 (`roms/aleste.rom` sample); M13 left them empty | OPEN | M13 | Cartridge/slot-manager milestone | Sony_HB-F1XV.xml (slots 1,2); Target Machine Spec (2 cartridge slots) |
| B8 | **FDC drive mechanics** — Fujitsu MB89311 controller + 720 KB 3.5" drive behavior; M13 mapped only the DISK ROM presence (slot 3-2) | OPEN | M13 | FDC milestone | `references/fact-sheets/FDC for Sony HB-F1XV.md`; Target Machine Spec (BUILT-IN MEDIA) |
| B9 | **VRAM / V9958 VDP** — 128 KB VRAM owned by the VDP; the display processor (register/VRAM/status/interrupt CONTRACT; rendering DEPTH split to D1-D7) | DONE (M14) | DEC-0002 / M13 | M14 (closed, v1.0.14) | `references/fact-sheets/Yamaha V9958 VDP.md`; `references/openmsx-21.0/src/video/` |

## B. Other known deferrals (tracked from earlier milestones / decisions)

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| C1 | **Exact cycle/T-state timing parity** (cross-emulator wait-state parity beyond the M1 wait; VDP-access recovery waits) — DP-4 | OPEN | M9/M10/M12 | Timing-fidelity milestone | Zilog Z80A fact-sheet §6/§8; S1985 fact-sheet §8 |
| C2 | **Z80 HALT-R increment (#34)** — R continues to increment while halted | OPEN | DEC-0004 | Timing-fidelity milestone (with C1) | Zilog Z80A fact-sheet §8 |
| C3 | **ZEXDOC/ZEXALL full parity sweep** — needs a legally-sourced ZEX binary (unavailable offline in M12); A/B trace-diff served as the M12 cross-check | OPEN | M10/M12 | CPU-validation milestone | Zilog Z80A fact-sheet §8 (test suites) |
| C4 | **S1985 backup-RAM `.sram` persistence** (16-byte, switched-I/O ID 0xFE) — M11 modeled volatile behavior only | OPEN | M11 (A-5/R-6) | Persistence milestone | S1985 fact-sheet §6 |
| C5 | **Full boot past first device read** — BIOS boot currently verified in lockstep only up to the first VDP/PSG/RTC read | OPEN | M13 | Composed after M14 + B1/B2 | M13 parity boundary (`docs/m13-parity-trace-diff.md`) |
| C6 | **Peripherals: keyboard matrix + joystick** (PPI port B/C rows `#A9/#AA`, PSG port A/B joystick) — DP-5 | OPEN | baseline | Peripheral/input milestone | S1985 fact-sheet §2/§3 |
| C7 | **Printer (Centronics) `#90/#91` + cassette interface** | OPEN | baseline | I/O-peripheral milestone | S1985 fact-sheet §8 |
| C8 | **Sony speed-controller + hardware PAUSE (MB670836); Ren-Sha Turbo autofire** — HB-F1XV-specific | OPEN | baseline | HB-F1XV-specifics milestone | S1985 fact-sheet §9; Zilog Z80A fact-sheet §6 |
| C9 | **SDL3 frontend** (video/audio/input presentation) — in baseline scope, not yet built | OPEN | baseline | Frontend milestone | `references/sdl3/`; project-baseline Scope |

## C. M14 VDP-depth deferrals (recorded by the M14 planner, REQ-M14-002)

M14 delivers the V9958 register/VRAM/status/interrupt CONTRACT (device-level, unit- +
A/B-verifiable) so software can drive the chip. The following rendering/timing/command
DEPTH is explicitly sequenced out of M14 per DEC-0005 (each is committed scope, not a
waiver). Grounded in `references/fact-sheets/Yamaha V9958 VDP.md` and
`references/openmsx-21.0/src/video/`.

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| D1 | **Pixel-accurate raster rendering pipeline** — per-mode VRAM→framebuffer for all Target-Spec modes (TEXT1/2, G1–G7, MULTICOLOR), border, blink, per-scanline output. M14 stores mode-selection bits + palette but emits NO pixels | OPEN | M14 | Video-rendering milestone | fact-sheet §3/§5; `references/openmsx-21.0/src/video/` (Renderer/`VDP.cc` display path) |
| D2 | **Sprite rendering + collision / 5th-sprite detection** — Sprite Mode 1 & 2, 8/line, per-line color, EC/IC/CC, the 1-px vertical shift, S#0 5S/C and S#3–S#6 collision coords | OPEN | M14 | Sprite milestone (with D1) | fact-sheet §6; `references/openmsx-21.0/src/video/SpriteChecker.cc/.hh` |
| D3 | **VDP command engine** — R#32–R#46, HMMC/YMMM/HMMM/HMMV/LMMC/LMCM/LMMM/LMMV/LINE/SRCH/PSET/POINT + logical ops (IMP/AND/OR/EOR/NOT + T-variants), S#2 TR/CE handshake, S#7 color, S#8/9 border, R#25 CMD-in-all-modes | OPEN | M14 | Command-engine milestone | fact-sheet §8; `references/openmsx-21.0/src/video/VDPCmdEngine.cc/.hh` |
| D4 | **Cycle-accurate VDP access-slot / command timing** — 1368 VDP-cycles/line, slot tables (154/88/31), 16-cycle request latency, CPU-access priority, the ~29-Z80-cycle safe-access gap, too-fast dropped-request behavior, exact sub-frame raster position of the line/VBlank IRQ | OPEN | M14 | Timing-fidelity milestone (with C1) | fact-sheet §2/§7/§8; `references/openmsx-21.0/src/video/VDPAccessSlots.cc/.hh` |
| D5 | **YJK / YJK+YAE color decode + 15-bit DAC output** — SCREEN 10/11/12 Y/J/K unpack, `R=clamp(Y+J)`, `G=clamp(Y+K)`, `B=clamp((5Y−2J−K+2)/4)`, the 3-bit→5-bit palette expansion table. M14 stores R#25 YJK/YAE bits only; no color is computed/emitted | OPEN | M14 | Video-rendering milestone (with D1) | fact-sheet §5 |
| D6 | **Horizontal scroll visual effect (R#25/26/27) + interlace/EO fields, blink timing, superimpose/digitize, external video** — M14 stores the register bits; the display consequences are rendering-time | OPEN | M14 | Video-rendering milestone (with D1) | fact-sheet §1/§3/§7/§9 |
| D7 | **G6/G7 VRAM address interleave in the display/command path** — `physical=(logical>>1)|(logical<<16)` for planar modes. M14 keeps a flat 128 KB store and models the CPU-port auto-increment addressing; the planar display/command interleave is only observable once D1/D3 exist | OPEN | M14 | Video-rendering / command milestone | fact-sheet §2; `references/openmsx-21.0/src/video/VDP.cc:851-857` (`executeCpuVramAccess` planar path) |

---

Last updated: 2026-07-06 (M13 closed; M14 VDP opened; M14 planner added VDP-depth
deferrals D1–D7). Maintainer: MSX Master Agent (coordinator) / MSX Planner Agent (D1–D7).
