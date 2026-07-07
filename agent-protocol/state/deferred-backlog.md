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
| B1 | **PSG / YM2149 device internals** (tone/noise/envelope/mixer, R0–R15, stereo A=C/B=L/C=R, joystick port A/B via R14/R15) — M11 S1985 provides only the I/O seam (`#A0-#A2`) | DONE (M15) | M11 (DEC-0002) | M15 (`src/devices/audio/psg_ym2149.*`) — numeric/register-accurate model per DEC-0009 Q4; A/B empty diff on R0/R7 vs genuine HB-F1XV | `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §2 |
| B2 | **RTC / RP5C01 device internals** (4 blocks × 13 regs, mode reg 13, BCD clock, alarm/timer, CMOS block-2 boot config) — M11 provides only the `#B4/#B5` seam gated by `#F5` bit7 | DONE (M15) | M11 (DEC-0002) | M15 (`src/devices/rtc/rp5c01.*`) — in-memory deterministic epoch per DEC-0009 Q2; `#F5` bit7 gate | S1985 fact-sheet §5 |
| B3 | **FM-PAC (OPLL YM2413) device internals** — 9-ch FM synth. M13 maps only the FM-MUSIC ROM presence (slot 3-3); the sound-generation device is unbuilt | DONE (M17) | M13 | M17 (`src/devices/audio/ym2413_opll.*`) — register-accurate `MSXMusic`-style device: 64-register file, two-port `#7C/#7D` write protocol (address-latch masked at use-time), per-channel decode (F-Num/Block/Key-on/Sustain/Instrument/Volume/Patch), rhythm decode (`$0E`,`$36-$38`), 15+3-entry ROM instrument patch table, `reset()`, debug `register_value()`; wired into `io_bus_` alongside the UNCHANGED M13 `fmmusic_rom_` (A-M17-2 regression guard held); real openMSX A/B evidence — both subjects PASS (Subject 1 architectural parity 145/145 instructions; Subject 2 register-file parity, all 36 written addresses) — `docs/m17-parity-trace-diff.md`. NOT the external `MSXFmPac` cartridge pattern the fact-sheet mostly describes (no bank register/SRAM/ID-string logic built, per DEC-0012). DSP/audio-synthesis depth split out as new row E1. | Target Machine Spec (SOUND); `references/fact-sheets/Yamaha YM2413 FM Chip.md`; `references/openmsx-21.0/src/sound/YM2413*` |
| B4 | **MSX-JE 16 KB SRAM** (battery-backed) | **DONE (M20)** | M13 | M20 (`src/devices/halnote/halnote_rom.*`) — the M17 `BatteryBackedSram` primitive (`src/devices/memory/battery_backed_sram.*`, 16KB) instantiated verbatim inside `HalnoteRom` and wired as its real SRAM store, gated at CPU `0x0000-0x3FFF` by the sram-enable bit (bit 0x80 of the bank-2 bank-switch write); deterministic load/save persistence (`Hbf1xvMachine::set_halnote_sram_path`/`halnote_sram_path`/`flush_halnote_sram`, mirroring the M15 S1985 `.sram` pattern exactly). | S1985 fact-sheet §9; `references/openmsx-21.0/src/memory/RomHalnote.cc:37-46`; Sony_HB-F1XV.xml:105-115 |
| B5 | **Kanji font ROM access via I/O `#D8-#DB`** — 256 KB JIS1+JIS2 font (`bios/f1xvkfn.rom`); M13 mapped the Kanji *driver* (slot 3-1) but not the I/O-accessed font | **DONE (M18)** | M13 | M18 (`src/devices/kanji/kanji_font_rom.*`) — direct-port `KanjiFontRom` device (NOT the switched-I/O `MSXKanji12`, confirmed via the machine XML), two independent address counters (`adr1_`/`adr2_`) for the JIS1/JIS2 halves, six-behavior `#D8-#DB` protocol grounded byte-for-byte in `MSXKanji.cc:32-88`; reads the real 256 KB `bios/f1xvkfn.rom` (SHA1-verified identical to the XML-cited revision, `218d91eb6df2823c924d3774a9f455492a10aecb`); real openMSX A/B evidence — content-level parity confirmed (empty diff, byte values manually cross-checked at 4 representative offsets spanning both halves) — `docs/m18-parity-trace-diff.md`. | `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` (kanji I/O `#D8-DB`); S1985 fact-sheet §9; `references/openmsx-21.0/src/MSXKanji.cc` |
| B6 | **Halnote / MSX-JE firmware mapping** — slot 0-3, 1 MB `bios/f1xvfirm.rom`, `mappertype=Halnote` + 16KB SRAM; M13 left it reserved open-bus | **DONE (M20)** | M13 | M20 (`src/devices/halnote/halnote_rom.*`) — `HalnoteRom` byte-exact per `RomHalnote.{hh,cc}`: main 8-slot window (reusing the M19 `CartridgeRomWindow` verbatim) with 4 bank-switch registers (window-slots 2-5), sub-mapper-enable gate + 2 sub-bank registers + the 0x7000-0x7FFF shadow-read override (never 0x6000-0x6FFF), window-slots 6/7 permanently unmapped; wired at primary slot 0, secondary slot 3, all 4 pages, reading the real `bios/f1xvfirm.rom`; real openMSX A/B evidence (11/14 PARITY, 3/14 disclosed non-blocking DIVERGENCE traced to the reference runtime, not this project's code) — `docs/m20-parity-trace-diff.md`. | Sony_HB-F1XV.xml (slot 0-3 Halnote); `references/openmsx-21.0/src/memory/RomHalnote.cc:37-46` (`sram = make_unique<SRAM>(..., 0x4000, ...)` = 16KB, grounds the exact size) |
| B7 | **Cartridge loading** — external primary slots 1 & 2 (`roms/aleste.rom` sample); M13 left them empty | **DONE (M19)** | M13 | M19 (`src/devices/cartridge/*`) — shared `CartridgeRomWindow` (8-slot x 8 KB, byte-exact `RomBlocks::setRom` bank-resolution, mask-as-fallback-only) + `CartridgeMapperType` (6-value enum, openMSX-canonical name strings) + six MVP mapper devices (`Mirrored`, `Generic8kB`, `Generic16kB`, `Ascii8kB`, `Ascii16kB`, `Konami` no-SCC) + `CartridgeSlot` wrapper (empty=open-bus regression guard, load/unload/reset dispatch, size-invalid loads rejected without partial application); wired at primary slots 1/2 (all 4 pages each, confirmed NOT expanded) via `Hbf1xvMachine::load_cartridge`/`unload_cartridge`; CLI (`--cart1`/`--cart1-type`/`--cart2`/`--cart2-type`, pure `cartridge_cli` parser, deliberately stricter loud/non-zero-exit failure policy kept separate from `RomAssetLoader`'s graceful-degradation policy); `roms/aleste.rom` used ONLY as a disclosed, non-hardware-claiming `Generic8kB` mechanical smoke fixture (SHA256 + 2-byte read-back check), never asserted as a real, identified cartridge; real openMSX 19.1 A/B evidence — empty diff (architectural + content-bearing, including the read-back marker bytes) over 8 instructions, `-carta` empirically confirmed (live WSL Tcl probe) to land in this machine's primary slot 1 — `docs/m19-parity-trace-diff.md`. KonamiSCC, ROM-database auto-detection, and runtime hot-plug split out as new rows G1-G3; the ~80-type long tail as G4 (not a partial close — mirrors the M14/M17/M18 contract-vs-depth precedent). | Sony_HB-F1XV.xml (slots 1,2); Target Machine Spec (2 cartridge slots); `references/openmsx-21.0/src/memory/{RomBlocks,RomPlain,RomGeneric8kB,RomGeneric16kB,RomAscii8kB,RomAscii16kB,RomKonami,RomInfo}.*` |
| B8 | **FDC drive mechanics** — Fujitsu MB89311 controller + 720 KB 3.5" drive behavior; M13 mapped only the DISK ROM presence (slot 3-2) | DONE (M16) | M13 | M16 (`src/devices/fdc/{wd2793,disk_drive,disk_image,sony_fdc,fdc_clock_source}.*`) — WD2793 core (Type I/II/III/IV, context-sensitive status, INTRQ/DRQ, HLD per openMSX WD2793.cc grounding), Sony connection-style decode wrapping the M13 DISK ROM, deterministic 720 KB image, ~4 s delayed motor-off, DSKCHG; unit + integration tests green; real openMSX A/B evidence (register/command sequence + functional Read-Sector completion + 512-byte content match) — `docs/m16-parity-trace-diff.md` | `references/fact-sheets/FDC for Sony HB-F1XV.md`; `references/openmsx-21.0/src/fdc/`; Target Machine Spec (BUILT-IN MEDIA) |
| B9 | **VRAM / V9958 VDP** — 128 KB VRAM owned by the VDP; the display processor (register/VRAM/status/interrupt CONTRACT; rendering DEPTH split to D1-D7) | DONE (M14) | DEC-0002 / M13 | M14 (closed, v1.0.14) | `references/fact-sheets/Yamaha V9958 VDP.md`; `references/openmsx-21.0/src/video/` |

## B. Other known deferrals (tracked from earlier milestones / decisions)

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| C1 | **Exact cycle/T-state timing parity** (cross-emulator wait-state parity beyond the M1 wait; VDP-access recovery waits) — DP-4 | **IN-PROGRESS (M23 partial)** | M9/M10/M12 | M23 (`src/devices/video/vdp_access_timing.h`) — the HALT-refetch M1-wait sub-item closes via C2; the VDP-access-recovery-wait remainder is identical to D4's remainder (below), carried forward as ONE item. A real, tested, additive, non-gating foundation ships this cycle (raster-position derivation, `VdpAccessDelta` cost-unit enum, two explicitly-separated cited latency facts — the D16 CPU-request-latency floor vs. the fact-sheet's independent "~29 Z80-cycle safe access" convention, never combined); full slot tables and actual CPU-execution gating remain OPEN. | Zilog Z80A fact-sheet §6/§8; S1985 fact-sheet §8 |
| C2 | **Z80 HALT-R increment (#34)** — R continues to increment while halted | **DONE (M23)** | DEC-0004 | M23 (`src/devices/cpu/z80a_cpu.cpp`) — `Z80aCpu::step()`'s halted branch now calls the existing `increment_refresh_register()`; the CPU core's own returned T-state count stays the bare, unchanged datasheet `4` (A-M23-1's invariant), while the already-existing, unmodified machine-level `datasheet + m1_wait` formula automatically produces the correct 5T MSX-accurate result. Grounded byte-for-byte in `references/openmsx-21.0/src/cpu/Z80.hh:19-21` (`HALT_STATES = 4 + WAIT_CYCLES`) and `CPUCore.cc:2508-2511` (`incR(advanceHalt(HALT_STATES, ...))` — the SAME `halts` value drives both the R increment and the clock advance on real hardware). Exactly one pre-existing golden test deliberately updated (`hbf1xv_cpu_step_integration_test.cpp`, t3 4→5, elapsed_cycles 22→23); the other 21 `.halted()`-using tests confirmed genuinely unmodified via an exhaustive audit, independently reproduced by the coordinator and QA. | Zilog Z80A fact-sheet §8 |
| C3 | **ZEXDOC/ZEXALL full parity sweep** — needs a legally-sourced ZEX binary (unavailable offline in M12); A/B trace-diff served as the M12 cross-check | OPEN | M10/M12 | CPU-validation milestone | Zilog Z80A fact-sheet §8 (test suites) |
| C4 | **S1985 backup-RAM `.sram` persistence** (16-byte, switched-I/O ID 0xFE) — M11 modeled volatile behavior only | DONE (M15) | M11 (A-5/R-6) | M15 (`S1985Engine::load/save_backup_ram` + machine `set_backup_ram_path`/`flush_backup_ram`); absent file → deterministic zero (M11 golden intact) | S1985 fact-sheet §6 |
| C5 | **Full boot past first device read** — BIOS boot currently verified in lockstep only up to the first VDP/PSG/RTC read | IN-PROGRESS (M16 partial) | M13 | M15 advanced the checkpoint past the M13 ~0x043C boundary (max PC 0x488). M16 advances it FURTHER with the FDC now live at slot 3-2: real-boot max PC reaches 0x7D6F over 400,000 instructions (deterministic, two-run identical), and real openMSX A/B parity is confirmed architecturally identical over a 3000-instruction real-boot window (`docs/m16-parity-trace-diff.md` Subject 1). RESIDUAL (honestly recorded, not fabricated): the automatic disk-ROM boot handshake (DSKCHG → Restore → Read Sector LBA 0) is genuinely never observed within an unattended, keyboard-less cold boot (confirmed up to a 20,000,000-instruction diagnostic run) — the FDC device itself is independently correct and A/B-confirmed (`docs/m16-parity-trace-diff.md` Subject 2, a dedicated CPU-driven register-sequence probe with the identical disk image), but the real auto-boot TRIGGER condition needs further investigation (disk-ROM/SUB-ROM disassembly) before C5 can close on "full boot to a prompt" | M13 parity boundary (`docs/m13-parity-trace-diff.md`); M16 evidence (`docs/m16-parity-trace-diff.md`, `docs/m16-implementation-report.md`) |
| C6 | **Peripherals: keyboard matrix + joystick** (PPI port B/C rows `#A9/#AA`, PSG port A/B joystick) — DP-5 | DONE (M15) | baseline | M15 (`src/devices/chipset/ppi_8255.*` full i8255 + `src/peripherals/{keyboard_matrix,joystick}.*`); `#A8` slot-select preserved byte-for-byte (X1) | S1985 fact-sheet §2/§3 |
| C7 | **Printer (Centronics) `#90-#97` (functional registers at `#90`/`#91`; `#92-#97` alias via mod-4 dispatch; `#93` PDIR/bidirectional unimplemented, matching openMSX's own scope limit) + cassette interface** | **DONE (M18)** | baseline | M18 (`src/peripherals/printer_port.*`, `src/peripherals/cassette_interface.*`) — `PrinterPort` device wired at the REAL `#90-#97` 8-port XML claim (a non-blocking backlog-wording precision correction from the planner's own literal "`#90/#91`" text, §2.7 of `docs/m18-planner-package.md`), strobe/data write protocol + falling-edge byte capture, deterministic always-ready status (a disclosed divergence from openMSX's *unplugged* default, kept out of the A/B probe by design). `CassetteInterface` peripheral (NOT a `core::IoDevice` — the XML's `<CassettePort/>` is empty, confirmed no dedicated port exists) derives motor/output read-only from the existing M15 `Ppi8255` (zero edit to `Ppi8255`) and supplies a deterministic synthetic-tape input bit into the existing M15 `JoystickPorts` (PSG R14 bit7) via a new, regression-guarded `CassetteInputSource` seam, replacing the M15 hardcoded idle-high stub. Real openMSX A/B evidence — printer write-path parity and cassette idle-state/write-path parity both PASS (empty diff, neither BLOCKED) — `docs/m18-parity-trace-diff.md`. Tape image-format/signal fidelity and printer image/ESC rendering depth explicitly split out as new rows **F1**/**F2** (not a partial close — mirrors the M14/M17 contract-vs-depth precedent). | S1985 fact-sheet §8; `references/openmsx-21.0/src/MSXPrinterPort.cc`, `Printer.cc`; `references/openmsx-21.0/src/CassettePort.hh/.cc`, `DummyCassetteDevice.cc` |
| C8 | **Sony speed-controller + hardware PAUSE (MB670836); Ren-Sha Turbo autofire** — HB-F1XV-specific | OPEN | baseline | HB-F1XV-specifics milestone | S1985 fact-sheet §9; Zilog Z80A fact-sheet §6 |
| C9 | **SDL3 frontend** (video/audio/input presentation) — in baseline scope, not yet built | OPEN | baseline | Frontend milestone | `references/sdl3/`; project-baseline Scope |
| C10 | **FDC flux-level / DMK disk fidelity** (raw flux, weak bits, copy-protection, DMK images) — M16 delivers WD2793 sector-level (2DD .dsk) fidelity; flux-level is a later stage. (Proposed as "B10" by the M16 planner) | OPEN | M16 | FDC-fidelity milestone | `references/openmsx-21.0/src/fdc/DMKDiskImage.*`; `references/fact-sheets/FDC for Sony HB-F1XV.md` |

## C. M14 VDP-depth deferrals (recorded by the M14 planner, REQ-M14-002)

M14 delivers the V9958 register/VRAM/status/interrupt CONTRACT (device-level, unit- +
A/B-verifiable) so software can drive the chip. The following rendering/timing/command
DEPTH is explicitly sequenced out of M14 per DEC-0005 (each is committed scope, not a
waiver). Grounded in `references/fact-sheets/Yamaha V9958 VDP.md` and
`references/openmsx-21.0/src/video/`.

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| D1 | **Pixel-accurate raster rendering pipeline** — per-mode VRAM→framebuffer for all Target-Spec modes (TEXT1/2, G1–G7, MULTICOLOR), border, blink, per-scanline output. M14 stores mode-selection bits + palette but emits NO pixels | **DONE (M21)** | M14 | M21 (`src/devices/video/vdp_frame_renderer.*`) — deterministic, pull-model `VdpFrameRenderer` over the existing M14 `V9958Vdp` contract; every Target-Spec mode byte-exact per `CharacterConverter.{hh,cc}`/`BitmapConverter.{hh,cc}`, incl. independently-verified dimensions (MULTICOLOR's real 256×192 canvas, not the fact-sheet's 64×48 color-cell-grid figure); border/backdrop color; blink. | fact-sheet §3/§5; `references/openmsx-21.0/src/video/` (Renderer/`VDP.cc` display path) |
| D2 | **Sprite rendering + collision / 5th-sprite detection** — Sprite Mode 1 & 2, 8/line, per-line color, EC/IC/CC, the 1-px vertical shift, S#0 5S/C and S#3–S#6 collision coords | **DONE (M22)** | M14 | M22 (`src/devices/video/sprite_engine.{h,cpp}`) — Sprite Mode 1 (4/line) and Mode 2 (8/line, per-line color/attribute table at a fixed +512-byte offset) byte-exact per `SpriteChecker.cc/.hh`; EC=bit7/CC=bit6/IC=bit5 independently confirmed (IC excludes collision ONLY, does not suppress rendering — a real, easy-to-miss distinction, dual-tested); color-0 sprite-pixel transparency conditioned on R#8 TP (a genuine fact-sheet-vs-source discrepancy resolved in favor of the source, which itself carries a "verified on real V9958" comment); the 1-pixel vertical shift (output line 0 unconditionally sprite-free); S#0 read clears only bits7/6/5, leaving the stale sprite-number field; S#6 bit1 (EO) honestly inherited as always-0, matching openMSX's own disclosed "not yet implemented" gap. Sprite pixel compositing folded directly into `VdpFrameRenderer::render_frame()`'s existing pipeline (no new output type). QA independently line-by-line compared the check/collision/5th-sprite algorithm against `SpriteChecker.cc:44-479` and confirmed a faithful, byte-exact re-derivation. | fact-sheet §6; `references/openmsx-21.0/src/video/SpriteChecker.cc/.hh` |
| D3 | **VDP command engine** — R#32–R#46, HMMC/YMMM/HMMM/HMMV/LMMC/LMCM/LMMM/LMMV/LINE/SRCH/PSET/POINT + logical ops (IMP/AND/OR/EOR/NOT + T-variants), S#2 TR/CE handshake, S#7 color, S#8/9 border, R#25 CMD-in-all-modes | **DONE (M22)** | M14 | M22 (`src/devices/video/vdp_command_engine.{h,cpp}`, `vdp_command_address.h`) — full R#32-46 register file (separate storage from `V9958Vdp::control_regs_`); all 13 commands via a HYBRID execution model: 10 execute ATOMICALLY (consistent with this project's frozen-snapshot, non-cycle-accurate philosophy, D4 deferred to M23), while LMCM/LMMC/HMMC use a genuine event-driven (never wall-clock) TR/CE state machine directly mirroring this project's own FDC DRQ/INTRQ precedent (M16) — collapsing these 3 to atomic would silently drop data, a real correctness requirement, not a timing nicety; only 5 of 13 commands consult the CMD register's low nibble for a logical op, the other 8 always perform a fixed operation; R#25 CMD-in-all-modes via `NonBitmapMode` addressing, writing into the SAME shared VRAM the M21 renderer already reads. Byte-exact per `VDPCmdEngine.cc/.hh`, independently confirmed term-for-term by both the coordinator and QA. | fact-sheet §8; `references/openmsx-21.0/src/video/VDPCmdEngine.cc/.hh` |
| D4 | **Cycle-accurate VDP access-slot / command timing** — 1368 VDP-cycles/line, slot tables (154/88/31), 16-cycle request latency, CPU-access priority, the ~29-Z80-cycle safe-access gap, too-fast dropped-request behavior, exact sub-frame raster position of the line/VBlank IRQ | **IN-PROGRESS (M23 partial)** | M14 | M23 (`src/devices/video/vdp_access_timing.h`) — ships a real, tested, additive, non-gating foundation: raster-position derivation (CPU-T-state-since-vsync -> VDP-cycle-within-line), the 15-value `VdpAccessDelta` cost-unit enum, and two explicitly-separated, cited latency facts (the V9958 `Delta::D16` CPU-request-latency floor vs. the fact-sheet's independent "~29 Z80-cycle safe access" software convention, never combined). Explicitly, precisely NOT built this cycle (carried forward as ONE tracked row, mirroring the D7/C5 precedent): (1) the five full per-mode/per-scanline slot tables + raster-position reconciliation with `run_frame()`'s whole-frame-atomic jump; (2) the CPU-access-steals-command-engine-slot interaction; (3) the command engine's real per-pixel/per-line VDP-cycle cost; (4) the exact sub-frame raster position of the line/VBlank IRQ; (5) any actual CPU-execution gating. Verified genuinely non-gating: `step_cpu_instruction()`/`run_cycles()`/`run_until_cycle()` and all of M21/M22's `VdpFrameRenderer`/`VdpCommandEngine`/`SpriteEngine` confirmed byte-for-byte UNCHANGED (`git diff` against v1.0.22, empty), independently confirmed by both the coordinator and QA. | fact-sheet §2/§7/§8; `references/openmsx-21.0/src/video/VDPAccessSlots.cc/.hh` |
| D5 | **YJK / YJK+YAE color decode + 15-bit DAC output** — SCREEN 10/11/12 Y/J/K unpack, `R=clamp(Y+J)`, `G=clamp(Y+K)`, `B=clamp((5Y−2J−K+2)/4)`, the 3-bit→5-bit palette expansion table. M14 stores R#25 YJK/YAE bits only; no color is computed/emitted | **DONE (M21)** | M14 | M21 (`src/devices/video/vdp_frame_renderer.cpp`) — byte-exact per `BitmapConverter.cc:217-285`; the YJK B-channel plain-`int`-truncating-division formula independently verified (both QA and the developer re-derived, by hand, that the floor-vs-truncation divergence is real at the pre-clamp level but never observable in the final `clamp(x,0,31)` output for this formula's value range); 3-bit→5-bit expansion table shared with every other color-output path. | fact-sheet §5 |
| D6 | **Horizontal scroll visual effect (R#25/26/27) + interlace/EO fields, blink timing, superimpose/digitize, external video** — M14 stores the register bits; the display consequences are rendering-time | **DONE (M21)** | M14 | M21 (`src/devices/video/vdp_frame_renderer.cpp`, `src/devices/video/v9958_vdp.cpp` blink-countdown) — vertical scroll (R#23), horizontal scroll for character modes (R#26, inside the per-mode converter) and bitmap modes (R#26/27, independently grounded), border mask (R#25 bit1), multi-page scroll (R#25 bit0 + R#2 bit5), blink (R#12/13, driven by the existing `on_vsync()` hook — no new clock consumer), interlace/EO bitmap-page addressing (hedged to an openMSX-parity confidence bar, since even openMSX's own source carries "TODO: verify" comments on this exact mechanism — QA independently confirmed this hedge is honest, not over-claimed), superimpose/digitize correctly scoped N/A (HB-F1XV has no digitizer/genlock hardware, fact-sheet §9). | fact-sheet §1/§3/§7/§9 |
| D7 | **G6/G7 VRAM address interleave in the display/command path** — `physical=(logical>>1)|(logical<<16)` for planar modes. M14 keeps a flat 128 KB store and models the CPU-port auto-increment addressing; the planar display/command interleave is only observable once D1/D3 exist | **DONE (M22)** | M14 | M21 closed the CPU-port piece (`V9958Vdp::effective_address()`, a 17-bit rotate-right-by-1, `(logical>>1)\|((logical&1)<<16)`, independently confirmed byte-for-byte against `VDP.cc:849-857` by both the coordinator and QA) and the display-path piece (`VdpFrameRenderer`'s `planar_row_spans`). M22 delivered the command-engine-path piece: five NEW, dedicated coordinate-based address functions (`src/devices/video/vdp_command_address.h`), confirmed BYTE-EXACT (not merely consistent) against `VDPCmdEngine.cc:175-410`'s `Graphic4Mode`-`NonBitmapMode::addressOf` functions by both the coordinator and QA independently; none of the five reference R#2 at all — commands address both pages of a bitmap mode directly through the Y-coordinate's own range, completely bypassing R#2's display-page-select bits (a genuinely new, independently-discovered nuance, dedicated-tested); `effective_address()` itself confirmed genuinely UNCHANGED (`git diff` against the M21 tag, zero occurrences, confirmed by both the coordinator and QA). **D7 closes IN FULL — no piece remains open.** | fact-sheet §2; `references/openmsx-21.0/src/video/VDP.cc:851-857` (`executeCpuVramAccess` planar path); `references/openmsx-21.0/src/video/VDPCmdEngine.cc:175-410` (`Graphic4Mode`-`NonBitmapMode::addressOf`) |

## D. M17 YM2413 DSP/timing deferrals (recorded by the M17 planner, REQ-M17-001; approved DEC-0012)

M17 delivers the YM2413 register/channel/rhythm CONTRACT (device-level, unit- + A/B-verifiable) —
mirrors the M14 VDP contract/depth split. The following synthesis/timing DEPTH is explicitly
sequenced out of M17 (committed scope, not a waiver).

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| E1 | **YM2413 FM-synthesis DSP/audio depth** — 18-slot TDM pipeline @ 49716 Hz, log-sin(256,12b)+exp(256,10b) operator, 128-level EG with global-counter rate mechanism, 2-deep feedback averaging, AM/VIB LFOs, rhythm noise generator + double-output quirk, DAC/output-level nonlinearity. M17 delivers the register/channel/rhythm contract only; zero waveform synthesis | OPEN | M17 | Future audio-rendering milestone, paired with C9 (SDL3 frontend) | YM2413 fact-sheet §4/§5/§7/§9; `references/openmsx-21.0/src/sound/YM2413NukeYKT.{hh,cc}` (recommended DSP-accuracy reference — matches the XML's `ym2413-core=NukeYKT` selection) |
| E2 | **YM2413 register-write timing constraint** — ≥12 master-cycle address-write / ≥84 master-cycle data-write minimum spacing; violating writes corrupt/drop on real hardware | OPEN | M17 | Timing-fidelity milestone (with C1/C2/D4) | YM2413 fact-sheet §8 ("openMSX currently has the too-fast-access-timing emulation disabled") |

## E. M18 printer/cassette depth deferrals (recorded by the M18 planner, REQ-M18-001)

M18 delivers the printer port register/protocol CONTRACT and the cassette motor/output/input
register-level CONTRACT (device-level, unit- + A/B-verifiable) — mirrors the M14 VDP and M17
YM2413 contract/depth splits. The following presentation/format DEPTH is explicitly sequenced out
of M18 (committed scope, not a waiver).

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|---|---|---|---|---|
| F1 | **Cassette tape image-format/signal fidelity** — real `.CAS`/`.WAV`/`.TSX` decode/encode, realistic FSK/UDF bit modulation and timing, save-to-host-file. M18 delivers the CPU-visible register contract (motor/output/input bits) + a deterministic in-memory synthetic bitstream only; zero real audio-file support. | OPEN | M18 | Future audio/tape-format milestone, paired with C9 (SDL3 frontend) | `references/openmsx-21.0/src/cassette/{CasImage,WavImage,TsxImage,TsxParser,CassettePlayer}.*`; S1985 fact-sheet §8 |
| F2 | **Printer image/ESC-sequence rendering depth** — dot-matrix font rendering, ESC command interpretation, virtual page → PNG output (openMSX's `ImagePrinter`/`ImagePrinterMSX`). M18 delivers the CPU-visible port contract (strobe/data/busy protocol) + deterministic raw-byte capture only; zero rendering. | OPEN | M18 | Future rendering-depth milestone, paired with C9 | `references/openmsx-21.0/src/Printer.cc` (`ImagePrinter`/`ImagePrinterMSX`); S1985 fact-sheet §8 |

## F. M19 cartridge-mapper-depth deferrals (recorded by the M19 planner, REQ-M19-001)

M19 delivers six MVP external-cartridge mapper types (Mirrored/Generic8kB/Generic16kB/
Ascii8kB/Ascii16kB/Konami no-SCC) as a byte-exact CONTRACT (device-level, unit- +
A/B-verifiable) — mirrors the M14/M17/M18 contract-vs-depth split. The following mapper-family
DEPTH is explicitly sequenced out of M19 (committed scope, not a waiver).

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|---|---|---|---|---|
| G1 | **KonamiSCC mapper + embedded SCC/SCC+ 5-channel wavetable sound chip** — `RomKonamiSCC` additionally owns a real `SCC scc;` member, a genuinely NEW audio device, not an incremental mapper variant. M19 delivers the plain `Konami` mapper (no SCC) only. | OPEN | M19 | Future audio milestone, paired with E1 (YM2413 DSP depth) / C9 (SDL3 frontend) | `references/openmsx-21.0/src/memory/RomKonamiSCC.hh/.cc`, `SCC.hh` |
| G2 | **ROM-database/SHA1 auto-mappertype-detection + heuristic byte-pattern auto-detection** (`RomInfo`/`RomDatabase`/`share/softwaredb.xml`; `RomFactory.cc:82-169` `guessRomType`). M19 requires an explicit `--cartN-type` (defaults to `Mirrored` only when the flag is OMITTED) — never infers a type from content. | OPEN | M19 | Future milestone, only if auto-identification becomes a real need | `references/openmsx-21.0/src/memory/RomFactory.cc:171-210`; `RomInfo.hh/.cc`; `share/softwaredb.xml` |
| G3 | **Full `CartridgeSlotManager`-style dynamic runtime hot-plug** (Tcl `cart`/`ext` commands, live insert/eject while running). M19's cartridge composition is fixed at construction/cold-boot time, matching every other device in this project. | OPEN | M19 | Future milestone, only if live insert/eject during a running session becomes a real requirement (e.g. alongside a future interactive/SDL3 session) | `references/openmsx-21.0/src/CartridgeSlotManager.hh/.cc` |
| G4 | **The long tail of ~80 other specialized/vendor-specific mapper types** in `RomTypes.hh` (Panasonic/National-internal, NEO-8/16, Majutsushi, Synthesizer, PlayBall, Zemina family, Holy Qu'ran (1/2), FSA1FM (1/2), the Manbow2/MegaFlashROMSCC(+)/RBSC/HamarajaNight flash-cart family, ReproCartridge (V1/V2), KonamiUltimateCollection, MSXDOS2, R-Type, CrossBlaim, HarryFox, GameMaster2, ASCII8/16-with-SRAM variants, Koei (8/32), Wizardry, MSXWrite, MultiRom, RAMFILE, ColecoMegaCart, WonderKid, Dooly, MSXtra, Yamanooto, AlAlamiah30in1, RetroHard31in1, ROMHunterMk2, ASCII16X — excluding `Halnote` (B6, internal slot 0-3, unrelated) and `DRAM` (MSXturboR-specific, not this machine). | OPEN | M19 | Future milestone(s), only if/when a specific real cartridge requiring one of these is actually wanted | `RomTypes.hh:8-100`; `RomFactory.cc:221-382` |

---

Last updated: 2026-07-08 (M23 CLOSED per DEC-0020/REQ-M23-004, tagged v1.0.23 — the THIRD AND FINAL
milestone of the pre-authorized M21-M23 run: **C2 → DONE (M23), closing IN FULL**; **C1 → IN-PROGRESS
(M23 partial)**; **D4 → IN-PROGRESS (M23 partial)** — a deliberately conservative, well-justified scope
decision, not a shortfall: this milestone's own planning found that the M21/M22 system-test CPU
fixtures already contain back-to-back `OUT (#98),A` writes with zero spacer instructions, so actually
gating CPU execution on VDP-access-wait timing this cycle would risk silently dropping bytes those
already-shipped, QA-signed tests depend on — a demonstrated, not hypothetical, regression risk.
`Z80aCpu::step()`'s halted branch (`src/devices/cpu/z80a_cpu.cpp`) now calls the existing
`increment_refresh_register()` before returning the bare, unchanged datasheet `4`; the ALREADY-EXISTING,
UNMODIFIED machine-level `datasheet + m1_wait` formula automatically reports 5T for a halted idle step,
matching real Z80/S1985 hardware — grounded byte-for-byte in `references/openmsx-21.0/src/cpu/Z80.hh:19-21`
(`HALT_STATES = 4 + WAIT_CYCLES`) and `CPUCore.cc:2508-2511` (`incR(advanceHalt(HALT_STATES, ...))` — the
SAME `halts` value drives both the R increment and the clock advance on real hardware, confirmed
independently by the coordinator AND QA). Exactly ONE pre-existing golden test was deliberately updated
(`tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp`, t3 4→5, elapsed_cycles 22→23,
independently hand-re-derived by both the coordinator and QA); an exhaustive `rg "\.halted\(\)" tests/`
audit (22 pre-existing files, re-run fresh by the developer, the coordinator, AND QA) confirmed every
other site uses a structurally-immune "stop stepping exactly at the halt boundary" pattern, untouched.
A new, real, tested, strictly NON-GATING VDP access-timing foundation ships for C1/D4
(`src/devices/video/vdp_access_timing.h`: 15 named `VdpAccessDelta` cost-unit offsets, raster-position
derivation, two explicitly-separated cited latency facts never combined) — independently confirmed by
BOTH the coordinator and QA, via direct `git diff` against v1.0.22, that `step_cpu_instruction()`/
`run_cycles()`/`run_until_cycle()` and all of M21/M22's `VdpFrameRenderer`/`VdpCommandEngine`/
`SpriteEngine` remain byte-for-byte UNCHANGED — a dedicated regression test re-runs the exact M21/M22
system-test CPU-program fixtures and proves the new calculator is genuinely inert. The precise remainder
(identical for C1 and D4) is carried forward as ONE tracked row each, mirroring the D7/C5 "not
force-closed, not split into a new letter" precedent: (1) the five full per-mode/per-scanline slot
tables + raster-position reconciliation with `run_frame()`'s whole-frame-atomic jump; (2) the
CPU-access-steals-command-engine-slot interaction; (3) the command engine's real per-pixel/per-line
VDP-cycle cost; (4) the exact sub-frame raster position of the line/VBlank IRQ; (5) any actual
CPU-execution gating (incl. "too-fast dropped-request" behavior). `ctest` 121/121 green (117 prior + 4
new), zero regression, independently reproduced by both the coordinator and QA from clean rebuilds; all
10 named zero-tolerance M9/M12 CPU-timing-oracle suites confirmed via independent `git diff` (by the
coordinator AND QA, separately) to show ZERO changes against v1.0.22. Real openMSX 19.1/WSL A/B evidence
for C2 (`docs/m23-parity-trace-diff.md`): the live `reg r` readback technique IS confirmed available; R
and PC match exactly for the first three steps, then a genuine, honestly-disclosed DIVERGENCE appears —
QA went further than the developer, independently re-running the harness AND designing two of its OWN
exploratory Tcl probes (an exact-reproducibility check and a boot-window-sensitivity variant), refining
the developer's causal hypothesis from "wall-clock time between Tcl round-trips" to the more precise
"emulated time accumulated during the unthrottled pre-measurement boot window, per openMSX's own bulk
`advanceHalt` ceiling-division scheduling" — confirming this is a live-session scheduling artifact, not a
defect in either engine's R-register logic, since C2's actual closure claim rests on the cited source +
this project's own deterministic tests, independent of this live-session finding. D4/C1's numeric
VDP-access-wait-cost A/B sub-claim stays honestly BLOCKED (no feasible technique exists, nothing gated on
it). A disclosed, additive, backward-compatible CLI extension (`src/main.cpp`'s `--parity-trace
halt_idle_extra_steps`, defaults to 0/no-op, mirroring the M19 `--cart1`/`--cart2` precedent) was needed
to gather the C2 A/B evidence — independently assessed by both the coordinator and QA as reasonable and
low-risk, formally ratified in DEC-0020. QA (`docs/m23-qa-signoff.md`, RESP-M23-003) = PASS. Full 34-row
backlog re-affirmed. Thirteen milestones M11-M23 now tagged (v1.0.11..v1.0.23). **This closes the
pre-authorized M21-M23 run** per the human's 2026-07-07 directive. Indicative next: M24 ZEXALL (C3) · M25
Sony speed-controller/PAUSE (C8) · M26 SDL3 frontend (C9) — none kicked off yet.

Prior status: 2026-07-07 (M22 CLOSED per DEC-0019/REQ-M22-004, tagged v1.0.22 — historical, superseded
by the closure summary above.)

Prior status: 2026-07-07 (M22 CLOSED per DEC-0019/REQ-M22-004, tagged v1.0.22: **D2 → DONE (M22)**,
**D3 → DONE (M22)**, **D7 → DONE (M22), closing IN FULL** (was IN-PROGRESS (M21 partial)) —
`SpriteEngine`/`VdpCommandEngine` (`src/devices/video/{sprite_engine,vdp_command_engine}.{h,cpp}`,
`vdp_command_address.h`) delivered as new private members owned inside `V9958Vdp` (mirrors the
`blink_countdown_` ownership style, per the planner's Resolution 1/2 architecture decisions) — sprite
pixel compositing folds additively into `VdpFrameRenderer::render_frame()` (no new output type,
R-M22-10 avoided); the command engine's execution model is the justified HYBRID (10 commands atomic,
LMCM/LMMC/HMMC event-driven mirroring the M16 FDC DRQ/INTRQ precedent, R-M22-9 avoided); D7's
command-engine-path piece closes via five NEW, independently-derived coordinate functions, confirmed
BYTE-EXACT against `VDPCmdEngine.cc:175-410` by both the coordinator and QA, with
`V9958Vdp::effective_address()` confirmed genuinely UNCHANGED (`git diff` against the M21 tag, zero
occurrences, R-M22-8 avoided). `ctest` 117/117 green (106 prior + 11 new), zero regression,
independently reproduced by both the coordinator and QA from clean rebuilds. A genuine regression was
self-caught and fixed during implementation: `SpriteEngine` needed the same `displayEnabled &&
spriteEnabled` gate (R#1 bit6 / R#8 bit1) real hardware uses, or freshly-reset all-zero VRAM produced
phantom Y=0 sprites breaking the pre-existing M14 status/IRQ tests — QA independently confirmed the
fix's grounding against `VDP.hh:299-319`/`VDP.cc:437,446`. Real openMSX A/B evidence
(`docs/m22-parity-trace-diff.md`): raw VRAM-byte comparison achieved genuine PARITY on all 7 probes
(sprite tables, command addressing incl. the G6 planar bank1 destination, transfer-command byte
sequencing) — QA independently confirmed no VRAM-byte divergence appears anywhere in the raw file.
Several STATUS-register-field divergences (5S/collision-Y/DY/NY) are honestly disclosed, not swept
under PARITY; QA went further than the developer's own narrative, independently hand-decoding the raw
numbers and correcting the causal explanation: rather than "a stale field surviving an intervening
S#0 read" (which doesn't arithmetically hold up against the observed values), the more defensible
framing is "this A/B harness's live-BIOS-driven B-side snapshot cannot be guaranteed to reflect having
processed the identical injected test scenario as the deterministic A-side" — QA independently
verified this is a harness/methodology artifact, not a code defect, by line-by-line comparing
`sprite_engine.cpp`'s algorithm against `SpriteChecker.cc:44-479` (a faithful, byte-exact
re-derivation) and confirming every dedicated deterministic unit/integration/system test for these
exact scenarios passes. Two additional non-blocking findings independently surfaced by QA (not in the
original report): ASX (S#8/S#9) is not persisted across LINE/LMMM/HMMM (broader in scope than the
report's own disclosure, though never a stated acceptance criterion); S#7/S#8/S#9 are excluded from
every A/B probe's comparison gate, with the resulting S#8 divergences visible in the raw dump but
never discussed in the original narrative — both recorded as documentation-precision follow-ups, not
gates on this milestone's closure. QA PASS (`docs/m22-qa-signoff.md`, RESP-M22-003). Full 34-row
backlog re-affirmed. Twelve milestones M11-M22 now tagged (v1.0.11..v1.0.22). Part of a
pre-authorized three-milestone run (M21-M23); M23 (backlog D4, exact cycle timing) next.

Prior status: 2026-07-07 (M22 developer implementation complete, awaiting QA sign-off — historical,
superseded by the closure summary above.)

Prior status: 2026-07-07 (M21 CLOSED per DEC-0018/REQ-M21-004, tagged v1.0.21: **D1 → DONE (M21)**,
**D5 → DONE (M21)**, **D6 → DONE (M21)**, **D7 → IN-PROGRESS (M21 partial)** — the first pixel-rendering
output path for this emulator. `VdpFrameRenderer`/`FrameBuffer` (`src/devices/video/`), a deterministic,
pull-model, frozen-register-snapshot renderer (RGB555 pixels, zero new clock consumer) built purely over
the existing M14 `V9958Vdp` port contract, needing no SDL3 frontend (still unbuilt, C9/M26) to be fully
unit/integration-testable. Every Target-Spec text/graphics mode (TEXT1/2, GRAPHIC1-7, MULTICOLOR;
TEXT1Q/MULTIQ/Unknown correctly rendered as flat blank since HB-F1XV's V9958 is never `isMSX1VDP()`)
byte-exact per `CharacterConverter.{hh,cc}`/`BitmapConverter.{hh,cc}`, with independently-re-derived
dimensions (MULTICOLOR's real canvas is 256×192, not the fact-sheet's 64×48 color-cell-grid figure).
YJK/YJK+YAE decode byte-exact per `BitmapConverter.cc:217-285`, including a rigorous, independently
re-verified finding that the YJK B-channel's floor-vs-truncation rounding risk, while real at the
pre-clamp arithmetic level, is never observable in the final clamped RGB555 output for this formula's
value range (both the developer and QA independently re-derived this by hand). GRAPHIC7's fixed
256-color byte layout confirmed **GGGRRRBB** (green in the TOP 3 bits), not the naively-expected
RRRGGGBB — independently verified against `SDLRasterizer.cc:330-336` by the coordinator, developer, AND
QA, three times over. The G6/G7 planar VRAM interleave (D7) is a 17-bit rotate-right-by-1
(`physical=(logical>>1)|((logical&1)<<16)`), independently confirmed byte-for-byte against
`VDP.cc:849-857` by all three reviewers; M21 closes its CPU-port piece (a small, surgical, single-function
edit to the already-shipped M14 `V9958Vdp::effective_address()`, confirmed NOT to touch
`advance_vram_pointer()`'s R#14-carry logic) and its display-path piece, carrying the command-engine-path
piece forward to **M22** (paired with D3) as a single tracked IN-PROGRESS row, not force-closed or split
into a new letter — mirroring the C5 precedent. Scroll/blink/interlace effects (D6) delivered, with
interlace/EO bitmap-page addressing honestly hedged to an "openMSX-parity" confidence bar (matching even
openMSX's OWN disclosed "TODO: verify" uncertainty on this exact mechanism) and superimpose/digitize
correctly scoped N/A (HB-F1XV has no digitizer hardware, fact-sheet §9). During implementation, the
developer independently caught and fixed a real bug in a first draft of the interlace/EO Field-selection
logic (a naive substitution would have silently flipped every bitmap-mode page read by default) — QA
independently assessed the fix as sound, with one non-blocking nuance flagged for forward visibility (the
final implementation is a documented SIMPLIFICATION of `getEvenOddMask()`, not a bit-for-bit port, since
the exact formula is a per-scanline mechanism this milestone's frozen-snapshot architecture cannot host —
that's D4/M23 territory). ctest 106/106 green (95 prior M1-M20 + 11 new), zero regression, independently
reproduced by both the coordinator and QA from clean rebuilds; the existing M14 VRAM-pointer/R#14-carry
unit tests confirmed genuinely unmodified (`git diff` against the M14 commit, empty diff). Real openMSX
A/B evidence (`docs/m21-parity-trace-diff.md`): a deliberately-chosen derived-value/raw-VRAM comparison
technique (not a screenshot-pixel diff, reasoned as fragile/not decision-relevant given openMSX's own
gamma/color-matrix/scaler presentation-layer confounds) — 4/4 probes achieved genuine raw
VRAM/palette/register ARCHITECTURAL PARITY, including a direct live cross-engine confirmation of the D7
planar transform's physical bank1 placement; the computed-pixel-color sub-claim honestly reported BLOCKED
(no computed-color introspection point exists in openMSX's Tcl debugger for this machine, verified via a
live `debug list` query) rather than fabricated. QA (`docs/m21-qa-signoff.md`, RESP-M21-003) = PASS:
independently re-derived the YJK rounding math by hand (three concrete cases), independently assessed the
interlace-bug fix against the actual `getEvenOddMask()`/`SDLRasterizer.cc` call site, and independently
confirmed the A/B raw dump files are genuine (read byte-for-byte, not the summarized doc). Ten milestones
M11-M21 now tagged (v1.0.11..v1.0.21). Part of a pre-authorized three-milestone run (M21-M23); M22
(sprites + command engine, D2/D3 + D7's remainder) next.

Prior status: M20 CLOSED per DEC-0017/REQ-M20-004, tagged v1.0.20: **B4 → DONE (M20)**
AND **B6 → DONE (M20)**, closed together per the human's explicit directive — `HalnoteRom`
(`src/devices/halnote/halnote_rom.*`), a byte-exact port of `references/openmsx-21.0/src/memory/
RomHalnote.{hh,cc}`: main 8-slot 8KB bank window (reusing the M19 `CartridgeRomWindow` primitive
verbatim, default block mask 127, no Konami-style override) with 4 bank-switch registers
(window-slots 2-5, write-triggered at `0x4FFF/0x6FFF/0x8FFF/0xAFFF`), the bit-0x80 double-duty
effect preserved exactly (a single write both toggles an enable flag AND updates that window-slot's
visible ROM bank via the unmasked raw byte); the REAL M17 `BatteryBackedSram` primitive
(`src/devices/memory/battery_backed_sram.*`, 16KB) instantiated verbatim as the mapper's actual SRAM
store, gated at `0x0000-0x3FFF` by the sram-enable bit; sub-mapper-enable gate + 2 sub-bank
registers (`0x77FF`/`0x7FFF`, writes always take effect regardless of enable state) + the
0x7000-0x7FFF shadow-read override scoped precisely away from 0x6000-0x6FFF (the milestone's
flagged subtlest risk); window-slots 6/7 (0xC000-0xFFFF) permanently unmapped. Wired at primary slot
0, secondary slot 3, all 4 CPU pages in `Hbf1xvMachine` (additive-only), loading the real
`bios/f1xvfirm.rom` (1,048,576 bytes); `halnote()`/`set_halnote_sram_path()`/`halnote_sram_path()`/
`flush_halnote_sram()` machine API mirroring the M15 S1985 backup-RAM precedent exactly (no new CLI
flag). Along the way, a genuine test-harness bug was found and fixed in the ALREADY-CLOSED, tagged
M17 test suite (`FmMusicRom_Slot33Page1_UnchangedByYm2413Writes`'s `#A8`/`#FFFF` slot-routing
silently resolved to RAM instead of the FM-MUSIC ROM, plus a silent missing `SONY_MSX_BIOS_DIR`
asset-root wiring gap that made the guard's ROM-content assertions vacuous under `ctest`) —
test-file-only, zero production risk, logged as DEC-0016, `ctest` reconfirmed 92/92 before M20
implementation began. ctest 95/95 green (92 prior M1-M19 + 3 new: `devices_halnote_rom_unit_test`,
`devices_halnote_subbank_unit_test`, `machine_hbf1xv_m20_halnote_integration_test`), zero
regression, independently reproduced by both the coordinator and QA from a clean rebuild. Real
openMSX A/B evidence (`docs/m20-parity-trace-diff.md`): used the REAL `bios/f1xvfirm.rom` unmodified
on both sides after a live SHA1 cross-check confirmed it byte-identical to the installed WSL system
ROM (stronger than a synthetic swap; separately resolved the planner's SHA1-enforcement question by
source read — advisory only, `Rom.cc:202-208`) — 11/14 labels PARITY (main bank-switch incl. the
bit-0x80 double-duty effect; SRAM enable/read/write), 3/14 disclosed, non-fabricated DIVERGENCE
isolated to the sub-mapper-shadow-read on the installed openMSX 19.1 runtime specifically. QA
(`docs/m20-qa-signoff.md`, RESP-M20-003) = PASS: independently re-ran the full suite, cross-checked
byte-exact semantics directly against `RomHalnote.{hh,cc}` line-by-line, confirmed genuine reuse (no
parallel reimplementation), confirmed the BIOS-ROM-at-slot-0-0 regression guard is non-vacuous,
independently recomputed the new tests' slot-routing arithmetic (confirmed free of the DEC-0016-class
bug), confirmed SRAM persistence round-trips across two independent machine instances, confirmed
determinism, confirmed the full 34-row deferred-backlog review — and on the A/B divergence, went
FURTHER than the developer's own investigation: independently re-ran the live WSL openMSX probe
itself, then read the raw firmware bytes at the "shadow not engaged, falls through to the plain
bank-3 window" hypothesis's predicted offset, finding a decisive byte-for-byte match to all three
divergent values — proving the divergence is the REFERENCE RUNTIME (openMSX 19.1) not engaging the
shadow read for this access pattern, not a defect in this project's own `HalnoteRom`. CLOSED by
human release decision (DEC-0017/REQ-M20-004); tagged git `v1.0.20`. Ten milestones M11-M20 now
tagged (v1.0.11..v1.0.20).

Prior status: M19 CLOSED per DEC-0015/REQ-M19-003, tagged v1.0.19: **B7 → DONE (M19)** — `CartridgeRomWindow`/`CartridgeMapperType`/six MVP
mapper devices/`CartridgeSlot` under `src/devices/cartridge/`, wired at primary slots 1/2
(all 4 pages each, confirmed unexpanded via `machine.slot_expanded(1/2) == false`);
`Hbf1xvMachine::load_cartridge`/`unload_cartridge` API; `cold_boot()` reinitializes bank state
without ever unloading a cartridge; pure `cartridge_cli` argv parser + `src/main.cpp` wiring
(loud/non-zero-exit failure policy kept structurally separate from the existing
`RomAssetLoader` graceful-degradation policy — verified by a dedicated regression check).
`roms/aleste.rom` used ONLY as a disclosed mechanical smoke fixture (`Generic8kB`, SHA256 +
2-byte header read-back), never asserted as a real, identified cartridge. ctest 91/91 green
(87 prior M1-M18 + 4 new M19 unit/integration test executables, ~60 additional individual
unit-test cases across 8 new test files), zero regression — in particular the M9/M12
CPU-timing oracles and the M13-M18 slot-map/device-accessor goldens all remained green,
confirming no new clock consumer was introduced. Real openMSX 19.1 (WSL) A/B evidence:
`-carta` empirically confirmed (live WSL Tcl probe, distinguishable synthetic markers +
`debug write ioports 0xA8`/`debug read memory`) to land in this machine's primary slot 1 for
this installed openMSX/machine combination; the identical `Generic8kB` synthetic
cartridge + Z80 driver program produce an EMPTY diff across all 8 instructions on both sides
(architectural PC/register/flags AND the content-bearing read-back marker bytes, since the
per-instruction trace's AF field captures each `LD A,(addr)` result) — `docs/m19-parity-trace-diff.md`.
Adversarial comparator self-check passed (empty-side → BLOCKED exit 2, corrupted-field →
DIVERGENCE exit 1). A genuine grounding finding surfaced and corrected during development:
the task brief's own "slots 0/1/2 stay permanently fixed at bank 0" Konami shorthand is an
overstatement of `RomKonami.cc:38-52,61-67` — only window-slots 0 and 2 are truly fixed
(both set only by `reset()`'s single `bank_switch(2,0)` call, since page 2 is never reachable
from any write address); slot 1 mirrors slot 3's LIVE value on every write to page 3 (and
symmetrically slot 6 mirrors slot 4, slot 7 mirrors slot 5) — corrected in both the
implementation and its own unit test, documented in `docs/m19-implementation-report.md`.
G1-G4 added as new OPEN rows (Section F) for KonamiSCC+embedded-SCC-chip,
ROM-database/heuristic auto-detection, runtime hot-plug, and the ~80-type long tail. A follow-up
round (per a human note about a newly-added second real cartridge file, `roms/metalgear.rom`)
identified it as a genuine Konami-mapper "Metal Gear" (1987) dump via an exact SHA1 match to
`references/openmsx-21.0/share/softwaredb.xml`, independently reproduced by the coordinator and
QA, plus live WSL openMSX corroboration — added as a second mechanical smoke fixture (loaded as
`Konami`, unlike `aleste.rom`'s disclosed-unidentified `Generic8kB` disposition); ctest 92/92
green after this addition. QA (`docs/m19-qa-signoff.md`, RESP-M19-004) = PASS, independently
reproducing ctest 92/92, the bank-resolution mask and Konami mirror-quirk grounding, and both
real-file fixtures' disposition discipline; one non-blocking doc-comment staleness
(`cartridge_konami_rom.h` retaining the disproven "slots 0/1/2 all fixed" claim) found by QA and
fixed by the coordinator post-QA (comment-only, ctest re-confirmed 92/92). CLOSED by human
release decision (DEC-0015/REQ-M19-003); tagged v1.0.19. Nine milestones M11-M19 now tagged
(v1.0.11..v1.0.19).

Prior status: M18 CLOSED per DEC-0014/REQ-M18-003, tagged v1.0.18: **B5 → DONE (M18)**
— `KanjiFontRom` device (two-counter `adr1_`/`adr2_` address-latch + auto-increment protocol over
`#D8-#DB`, byte-exact per `MSXKanji.cc:32-88`) reads the real 256 KB `bios/f1xvkfn.rom` (SHA1
`218d91eb6df2823c924d3774a9f455492a10aecb`, confirmed identical to the XML-cited revision).
**C7 → DONE (M18)** — `PrinterPort` device wired at the real `#90-#97` 8-port claim (a
non-blocking backlog-wording precision correction, §2.7) and `CassetteInterface` peripheral
(motor/output derived read-only from the existing `Ppi8255`, zero edit to `Ppi8255`;
synthetic-tape input bit injected into the existing `JoystickPorts` PSG R14 bit7 seam via a new,
regression-guarded `CassetteInputSource` contract, replacing the M15 hardcoded idle-high stub).
F1/F2 added as new OPEN rows (Section E) for the explicitly-deferred tape-format and
printer-rendering depth. ctest 79/79 green (75 prior M1-M17 + 4 new M18), zero regression — in
particular the M15 `JoystickPorts`/`Ppi8255` goldens and the M9/M12 CPU-timing oracles all
remained green, confirming no CPU-timing-affecting clock consumer was added and the
unattached-cassette-source regression guard holds byte-for-byte. Real openMSX A/B evidence
captured for ALL THREE subjects (none BLOCKED): Kanji content parity (SHA1-verified identical
source ROM, empty architectural diff, manually cross-checked byte content at 4 representative
offsets spanning both JIS halves — `docs/m18-parity-trace-diff.md`); printer write-path parity
(empty diff, status-bit divergence A-M18-7 deliberately kept out of the probe); cassette
idle-state + write-path parity (empty diff, both `IN A,(#A2)` reads = 0xFF on both emulators
before/after the PPI port-C write sequence). Adversarial comparator self-check passed
(empty-side → BLOCKED, corrupted-field → DIVERGENCE). QA PASS (RESP-M18-003,
`docs/m18-qa-signoff.md`) independently reproduced ctest 79/79, verified device identity against
`MSXKanji.cc`/`MSXPrinterPort.cc`/`Printer.cc`, confirmed zero edits to `ppi_8255.*` and
CPU-stepping code, and independently reproduced the Kanji content-parity claim three ways
including a live WSL openMSX 19.1 Tcl probe. Tagged git v1.0.18 (committed separately from M17
per the human's explicit "commit, and tag seperately" instruction).

Prior status: M17 CLOSED per DEC-0013/REQ-M17-003, tagged v1.0.17: **B3 → DONE (M17)** —
`Ym2413Opll` register-accurate device (64-register file, two-port `#7C/#7D` write protocol,
per-channel/rhythm decode, 15+3-entry ROM instrument patch table) wired into `io_bus_`
alongside the UNCHANGED M13 `fmmusic_rom_` (A-M17-2 regression guard held by a dedicated
integration-test case); `BatteryBackedSram` primitive (`src/devices/memory/
battery_backed_sram.*`) built standalone and unit-tested at 16 KB, confirmed NOT instantiated
in `Hbf1xvMachine` and NOT wired to any slot (B4 stays OPEN/re-owned to B6 per DEC-0012 — no
disposition change this cycle). ctest 75/75 green (72 prior + 3 new), zero regression M1-M16.
Real openMSX A/B evidence captured for BOTH subjects (not one BLOCKED): Subject 1 (CPU-visible
architectural parity across the write sequence) empty diff over 145/145 instructions; Subject 2
(internal register-file comparison via `debug read {MSX Music regs} <addr>`, mechanism verified
against real WSL openMSX before use, per R-M17-6) empty diff over all 36 written addresses —
`docs/m17-parity-trace-diff.md`. Adversarial comparator self-check passed for both subjects
(empty-side → BLOCKED, corrupted-field → DIVERGENCE). QA PASS (RESP-M17-003,
`docs/m17-qa-signoff.md`) independently reproduced ctest 75/75, verified device identity is
genuinely `MSXMusic`-pattern (no `MSXFmPac`-style code), and re-ran the full A/B harness with an
extra spot-check beyond the developer's own. Tagged git v1.0.17.

Prior status: M16 CLOSED per DEC-0011/REQ-M16-005, tagged v1.0.16: **B8 → DONE
(M16)** — WD2793 core (Type I/II/III/IV, context-sensitive status/INTRQ/DRQ, HLD fixed
against a genuine openMSX A/B divergence found this cycle), Sony connection-style decode,
deterministic 720 KB image, motor-off, DSKCHG, all under `src/devices/fdc/`; C10 (flux/DMK
fidelity, ex-"B10") re-affirmed OPEN. **C5 advanced (still IN-PROGRESS, carried forward)**:
real-boot max PC now 0x7D6F (was 0x488), real openMSX A/B parity confirmed over a
3000-instruction boot window and (separately) over a dedicated FDC register/command probe
with the identical disk image (functional match down to the transferred 512 bytes); the
automatic disk-boot handshake itself is honestly reported as NOT reached within an
unattended run (residual, not fabricated, QA-accepted as non-blocking for M16) — see
`docs/m16-implementation-report.md`, `docs/m16-parity-trace-diff.md`, `docs/m16-qa-signoff.md`.
Whichever future milestone claims C5 fully closed must specifically re-investigate the real
auto-disk-boot trigger condition. ctest 72/72 green, zero regression across M1-M15. QA PASS
(RESP-M16-004). Six milestones M11-M16 now tagged (v1.0.11..v1.0.16). M17 not yet kicked
off — awaiting human directive.) Indicative follow-on order
(re-confirmed per kickoff): M16 FDC · M17 FM-PAC/OPLL(+B4 SRAM) · M18 peripheral I/O
(B5 Kanji-font, C7 printer/cassette) · M19 cartridges (B7) · M20 Halnote (B6) · M21 VDP
rendering (D1/D5/D6/D7) · M22 sprites/command (D2/D3) · M23 exact cycle timing (C1/C2/D4)
· M24 ZEXALL (C3) · M25 Sony speed/pause (C8) · M26 SDL3 frontend (C9). Maintainer: MSX
Master Agent (coordinator) / MSX Planner Agent (D1–D7).
