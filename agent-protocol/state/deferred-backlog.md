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
| B4 | **MSX-JE 16 KB SRAM** (battery-backed) | OPEN — **re-grounded + re-owned to B6 (DEC-0012)** | M13 | B6 (Halnote milestone, indicative M20) — NOT M17/B3. Concrete grounding (`Sony_HB-F1XV.xml:105-115`, `RomHalnote.cc:37-46`) shows this SRAM belongs to the Halnote-mapped MSX-JE firmware ROM at slot 0-3, not the YM2413/MSX-MUSIC device at slot 3-3 (which the XML shows has no `<sramname>` at all). M17 builds a reusable, standalone, deterministic `BatteryBackedSram` primitive (`src/devices/memory/battery_backed_sram.*`, 16KB, generalizing the M15 S1985 `.sram` pattern) ready for B6 to wire directly — but does NOT instantiate/wire it to any slot this milestone (no real consumer yet; wiring it into slot 3-3 would fabricate hardware this machine does not have). | S1985 fact-sheet §9; `references/openmsx-21.0/src/memory/RomHalnote.cc:37-46`; Sony_HB-F1XV.xml:105-115 |
| B5 | **Kanji font ROM access via I/O `#D8-#DB`** — 256 KB JIS1+JIS2 font (`bios/f1xvkfn.rom`); M13 mapped the Kanji *driver* (slot 3-1) but not the I/O-accessed font | OPEN | M13 | Kanji/peripheral milestone | `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` (kanji I/O `#D8-DB`); S1985 fact-sheet §9 |
| B6 | **Halnote / MSX-JE firmware mapping** — slot 0-3, 1 MB `bios/f1xvfirm.rom`, `mappertype=Halnote` + 16KB SRAM; M13 left it reserved open-bus | OPEN — **confirmed true owner of B4's 16KB SRAM (DEC-0012)** | M13 | Halnote/firmware milestone (indicative M20) | Sony_HB-F1XV.xml (slot 0-3 Halnote); `references/openmsx-21.0/src/memory/RomHalnote.cc:37-46` (`sram = make_unique<SRAM>(..., 0x4000, ...)` = 16KB, grounds the exact size) |
| B7 | **Cartridge loading** — external primary slots 1 & 2 (`roms/aleste.rom` sample); M13 left them empty | OPEN | M13 | Cartridge/slot-manager milestone | Sony_HB-F1XV.xml (slots 1,2); Target Machine Spec (2 cartridge slots) |
| B8 | **FDC drive mechanics** — Fujitsu MB89311 controller + 720 KB 3.5" drive behavior; M13 mapped only the DISK ROM presence (slot 3-2) | DONE (M16) | M13 | M16 (`src/devices/fdc/{wd2793,disk_drive,disk_image,sony_fdc,fdc_clock_source}.*`) — WD2793 core (Type I/II/III/IV, context-sensitive status, INTRQ/DRQ, HLD per openMSX WD2793.cc grounding), Sony connection-style decode wrapping the M13 DISK ROM, deterministic 720 KB image, ~4 s delayed motor-off, DSKCHG; unit + integration tests green; real openMSX A/B evidence (register/command sequence + functional Read-Sector completion + 512-byte content match) — `docs/m16-parity-trace-diff.md` | `references/fact-sheets/FDC for Sony HB-F1XV.md`; `references/openmsx-21.0/src/fdc/`; Target Machine Spec (BUILT-IN MEDIA) |
| B9 | **VRAM / V9958 VDP** — 128 KB VRAM owned by the VDP; the display processor (register/VRAM/status/interrupt CONTRACT; rendering DEPTH split to D1-D7) | DONE (M14) | DEC-0002 / M13 | M14 (closed, v1.0.14) | `references/fact-sheets/Yamaha V9958 VDP.md`; `references/openmsx-21.0/src/video/` |

## B. Other known deferrals (tracked from earlier milestones / decisions)

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| C1 | **Exact cycle/T-state timing parity** (cross-emulator wait-state parity beyond the M1 wait; VDP-access recovery waits) — DP-4 | OPEN | M9/M10/M12 | Timing-fidelity milestone | Zilog Z80A fact-sheet §6/§8; S1985 fact-sheet §8 |
| C2 | **Z80 HALT-R increment (#34)** — R continues to increment while halted | OPEN | DEC-0004 | Timing-fidelity milestone (with C1) | Zilog Z80A fact-sheet §8 |
| C3 | **ZEXDOC/ZEXALL full parity sweep** — needs a legally-sourced ZEX binary (unavailable offline in M12); A/B trace-diff served as the M12 cross-check | OPEN | M10/M12 | CPU-validation milestone | Zilog Z80A fact-sheet §8 (test suites) |
| C4 | **S1985 backup-RAM `.sram` persistence** (16-byte, switched-I/O ID 0xFE) — M11 modeled volatile behavior only | DONE (M15) | M11 (A-5/R-6) | M15 (`S1985Engine::load/save_backup_ram` + machine `set_backup_ram_path`/`flush_backup_ram`); absent file → deterministic zero (M11 golden intact) | S1985 fact-sheet §6 |
| C5 | **Full boot past first device read** — BIOS boot currently verified in lockstep only up to the first VDP/PSG/RTC read | IN-PROGRESS (M16 partial) | M13 | M15 advanced the checkpoint past the M13 ~0x043C boundary (max PC 0x488). M16 advances it FURTHER with the FDC now live at slot 3-2: real-boot max PC reaches 0x7D6F over 400,000 instructions (deterministic, two-run identical), and real openMSX A/B parity is confirmed architecturally identical over a 3000-instruction real-boot window (`docs/m16-parity-trace-diff.md` Subject 1). RESIDUAL (honestly recorded, not fabricated): the automatic disk-ROM boot handshake (DSKCHG → Restore → Read Sector LBA 0) is genuinely never observed within an unattended, keyboard-less cold boot (confirmed up to a 20,000,000-instruction diagnostic run) — the FDC device itself is independently correct and A/B-confirmed (`docs/m16-parity-trace-diff.md` Subject 2, a dedicated CPU-driven register-sequence probe with the identical disk image), but the real auto-boot TRIGGER condition needs further investigation (disk-ROM/SUB-ROM disassembly) before C5 can close on "full boot to a prompt" | M13 parity boundary (`docs/m13-parity-trace-diff.md`); M16 evidence (`docs/m16-parity-trace-diff.md`, `docs/m16-implementation-report.md`) |
| C6 | **Peripherals: keyboard matrix + joystick** (PPI port B/C rows `#A9/#AA`, PSG port A/B joystick) — DP-5 | DONE (M15) | baseline | M15 (`src/devices/chipset/ppi_8255.*` full i8255 + `src/peripherals/{keyboard_matrix,joystick}.*`); `#A8` slot-select preserved byte-for-byte (X1) | S1985 fact-sheet §2/§3 |
| C7 | **Printer (Centronics) `#90/#91` + cassette interface** | OPEN | baseline | I/O-peripheral milestone | S1985 fact-sheet §8 |
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
| D1 | **Pixel-accurate raster rendering pipeline** — per-mode VRAM→framebuffer for all Target-Spec modes (TEXT1/2, G1–G7, MULTICOLOR), border, blink, per-scanline output. M14 stores mode-selection bits + palette but emits NO pixels | OPEN | M14 | Video-rendering milestone | fact-sheet §3/§5; `references/openmsx-21.0/src/video/` (Renderer/`VDP.cc` display path) |
| D2 | **Sprite rendering + collision / 5th-sprite detection** — Sprite Mode 1 & 2, 8/line, per-line color, EC/IC/CC, the 1-px vertical shift, S#0 5S/C and S#3–S#6 collision coords | OPEN | M14 | Sprite milestone (with D1) | fact-sheet §6; `references/openmsx-21.0/src/video/SpriteChecker.cc/.hh` |
| D3 | **VDP command engine** — R#32–R#46, HMMC/YMMM/HMMM/HMMV/LMMC/LMCM/LMMM/LMMV/LINE/SRCH/PSET/POINT + logical ops (IMP/AND/OR/EOR/NOT + T-variants), S#2 TR/CE handshake, S#7 color, S#8/9 border, R#25 CMD-in-all-modes | OPEN | M14 | Command-engine milestone | fact-sheet §8; `references/openmsx-21.0/src/video/VDPCmdEngine.cc/.hh` |
| D4 | **Cycle-accurate VDP access-slot / command timing** — 1368 VDP-cycles/line, slot tables (154/88/31), 16-cycle request latency, CPU-access priority, the ~29-Z80-cycle safe-access gap, too-fast dropped-request behavior, exact sub-frame raster position of the line/VBlank IRQ | OPEN | M14 | Timing-fidelity milestone (with C1) | fact-sheet §2/§7/§8; `references/openmsx-21.0/src/video/VDPAccessSlots.cc/.hh` |
| D5 | **YJK / YJK+YAE color decode + 15-bit DAC output** — SCREEN 10/11/12 Y/J/K unpack, `R=clamp(Y+J)`, `G=clamp(Y+K)`, `B=clamp((5Y−2J−K+2)/4)`, the 3-bit→5-bit palette expansion table. M14 stores R#25 YJK/YAE bits only; no color is computed/emitted | OPEN | M14 | Video-rendering milestone (with D1) | fact-sheet §5 |
| D6 | **Horizontal scroll visual effect (R#25/26/27) + interlace/EO fields, blink timing, superimpose/digitize, external video** — M14 stores the register bits; the display consequences are rendering-time | OPEN | M14 | Video-rendering milestone (with D1) | fact-sheet §1/§3/§7/§9 |
| D7 | **G6/G7 VRAM address interleave in the display/command path** — `physical=(logical>>1)|(logical<<16)` for planar modes. M14 keeps a flat 128 KB store and models the CPU-port auto-increment addressing; the planar display/command interleave is only observable once D1/D3 exist | OPEN | M14 | Video-rendering / command milestone | fact-sheet §2; `references/openmsx-21.0/src/video/VDP.cc:851-857` (`executeCpuVramAccess` planar path) |

## D. M17 YM2413 DSP/timing deferrals (recorded by the M17 planner, REQ-M17-001; approved DEC-0012)

M17 delivers the YM2413 register/channel/rhythm CONTRACT (device-level, unit- + A/B-verifiable) —
mirrors the M14 VDP contract/depth split. The following synthesis/timing DEPTH is explicitly
sequenced out of M17 (committed scope, not a waiver).

| # | Item | Status | Origin | Candidate owner | Grounding |
|---|------|--------|--------|-----------------|-----------|
| E1 | **YM2413 FM-synthesis DSP/audio depth** — 18-slot TDM pipeline @ 49716 Hz, log-sin(256,12b)+exp(256,10b) operator, 128-level EG with global-counter rate mechanism, 2-deep feedback averaging, AM/VIB LFOs, rhythm noise generator + double-output quirk, DAC/output-level nonlinearity. M17 delivers the register/channel/rhythm contract only; zero waveform synthesis | OPEN | M17 | Future audio-rendering milestone, paired with C9 (SDL3 frontend) | YM2413 fact-sheet §4/§5/§7/§9; `references/openmsx-21.0/src/sound/YM2413NukeYKT.{hh,cc}` (recommended DSP-accuracy reference — matches the XML's `ym2413-core=NukeYKT` selection) |
| E2 | **YM2413 register-write timing constraint** — ≥12 master-cycle address-write / ≥84 master-cycle data-write minimum spacing; violating writes corrupt/drop on real hardware | OPEN | M17 | Timing-fidelity milestone (with C1/C2/D4) | YM2413 fact-sheet §8 ("openMSX currently has the too-fast-access-timing emulation disabled") |

---

Last updated: 2026-07-07 (M17 CLOSED per DEC-0013/REQ-M17-003: **B3 → DONE (M17)** —
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
extra spot-check beyond the developer's own. Tagged git v1.0.17. Next: M18 (peripheral I/O),
closed separately.

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
