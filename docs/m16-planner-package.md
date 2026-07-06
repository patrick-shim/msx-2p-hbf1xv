# M16 Planner Package — FDC Drive Mechanics (Fujitsu MB89311 + 720 KB 3.5" Floppy)

- Milestone ID: M16
- Title: FDC Drive Mechanics (Fujitsu MB89311 == WD2793, Sony connection style + built-in 720 KB 2DD drive)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M16-002 (planner-first, no production code)
- Decisions in force: DEC-0009 (FDC moved up to next-after-M15), DEC-0010 (M15 close / M16 open)
- Backlog effect: **closes B8** (FDC drive mechanics); **advances C5** (full boot past first device read). All other backlog rows re-affirmed OPEN (see §9).
- Gate: Normal human-release-decision gate (no auto-close). Autopilot PAUSES at QA for the human release decision + tag.

> Grounding rule: every behaviour claim below cites a concrete `references/...` path or a
> current-repo `src/...`/`docs/...` path. openMSX sources are the BEHAVIOUR reference only and
> are GPL — **never copy openMSX code into `src/`** (CLAUDE.md, `agent-protocol/guardrails.md`).

---

## 1. Scope and Assumptions

### 1.1 In scope
Implement the HB-F1XV floppy disk controller as a **WD2793** core with the **Sony** memory-mapped
connection style, plus the built-in **single** 3.5" 720 KB (2DD) drive and a fully deterministic
disk image, and wire it onto the DISK ROM already mapped at **slot 3-2 page 1** in M13
(`src/machine/hbf1xv_machine.cpp:62` attaches `disk_rom_` at `(3, 2, 1)`;
`src/machine/hbf1xv_machine.h:276`).

The physical chip is the **Fujitsu MB89311**, a CMOS WD279x-family part that is register- and
instruction-compatible with the **Western Digital WD2793**; it is modelled as a WD2793 and **NOT**
as an Intel 8272 / Toshiba TC8566AF (`references/fact-sheets/FDC for Sony HB-F1XV.md` §1 TL;DR +
§1 "Why WD2793 is the right emulation model"; the Write-Track vs Format distinction that forbids
reusing the TC8566AF model is in §5 and §8 pitfall 7). openMSX confirms the machine wiring:
`references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:161-176` declares
`<WD2793 id="Memory Mapped FDC">` with `<connectionstyle>Sony</connectionstyle>`,
`<motor_off_timeout_ms>4000</motor_off_timeout_ms>`, `<drives>1</drives>`,
`<mem base="0x4000" size="0x4000"/>` and `<rom_visibility base="0x4000" size="0x4000"/>`, disk ROM
`sha1 5a4e7dbbfb759109c7d2a3b38bda9c60bf6ffef5`. Per `references/openmsx-21.0/src/fdc/` (DeviceFactory
maps `WD2793 -> PhilipsFDC`), the Sony connection style decodes through **PhilipsFDC**
(`references/openmsx-21.0/src/fdc/PhilipsFDC.cc`).

Behavioural depth targeted for M16 = fact-sheet **Stage 1 (DOS-level correctness) + the Stage 2
timing behaviours needed for a Busy/DRQ-polling boot** (`FDC for Sony HB-F1XV.md` §"Recommendations"
Stages 1–2). Concretely: five WD2793 registers, Type I/II/III/IV commands, per-command status-bit
semantics, polled INTRQ/DRQ, realistic (deterministic) Busy / DRQ pacing / motor-off timing, and
the DSKCHG disk-changed / not-ready quirks.

### 1.2 Non-goals (deferred, tracked)
- **Flux/DMK-level copy-protection fidelity** (fact-sheet Stage 3): non-standard track layouts,
  RawTrack/DMK images, exact 5-revolution Record-Not-Found search timing. M16 uses a flat 720 KB
  sector image; Write Track is modelled well enough to re-format a standard 9-sector MSX track but
  not to preserve arbitrary flux. → **new deferred-backlog row B10** (see §9).
- **Second/phantom physical drive**: `<drives>1</drives>`; the A:/B: phantom-drive prompt is a
  Disk-BIOS behaviour, not a second mechanism (fact-sheet §7). Single drive only.
- **Real-`.dsk` file mounting from an arbitrary host path at runtime** as a *behaviour-affecting*
  emulation feature — M16 mounts a deterministic, repository-controlled image only (see §5). A
  general "insert disk from host FS" front-end feature is a later frontend/CLI concern (C9).
- **SDL3 / CLI disk-insert UX** (C9, OPEN).
- **Interrupt-driven (NMI) disk transfer** — the HB-F1XV disk BIOS is **polled**; INTRQ/DRQ are read
  back through the interface status register, not wired to Z80 `/INT` (`FDC for Sony HB-F1XV.md` §6;
  `PhilipsFDC.cc:98-113` — "Drive control IRQ and DRQ lines are not connected to Z80 interrupt
  request"). So M16 does **not** touch the M12/M14 interrupt-accept path.

### 1.3 Assumptions (each with a verification action in §8)
- **A-M16-1**: The exact Sony glue-register decode (side/motor/drive-select + INTRQ/DRQ/DSKCHG
  positions in `0x7FFC–0x7FFF`) follows `PhilipsFDC.cc` — the authoritative source, per fact-sheet
  Recommendation 4 which explicitly says the fact-sheet's "typical convention" is inferred and must
  be verified against `PhilipsFDC.cc`. Details in §3.2.
- **A-M16-2**: Deterministic command/DRQ/motor timing is driven off the emulated cycle clock
  (`Hbf1xvMachine::elapsed_cycles()` / `core::Scheduler` total cycles) exactly like the M15 X4 RTC
  pattern, never wall-clock.
- **A-M16-3**: The DISK ROM asset (`bios/f1xvdisk.rom`, `sha1 5a4e7dbb...`) is present locally
  (M13 already loads it, `src/machine/hbf1xv_machine.cpp:217`). If absent, the deterministic
  0xFF-fill policy applies and the boot-advance criterion degrades gracefully (documented, not
  fabricated).

---

## 2. src/ Placement (device logic in `src/devices/fdc/`, wiring in `src/machine/`)

Per `src/CLAUDE.md` ("device logic in `src/devices/`; machine wiring in `src/machine/`; nothing
device-specific in `src/core/`"). New folder `src/devices/fdc/`:

| File | Responsibility | openMSX behaviour reference (read-only) |
|------|----------------|------------------------------------------|
| `src/devices/fdc/wd2793.{h,cpp}` | The **WD2793 core**: five registers (status/command, track, sector, data), Type I/II/III/IV command FSM, status-bit semantics, INTRQ/DRQ state, Busy/DRQ/motor timing computed in emulated cycles. Talks to a `DiskDrive` abstraction; owns no memory decode and no slot knowledge. | `references/openmsx-21.0/src/fdc/WD2793.{cc,hh}` |
| `src/devices/fdc/disk_drive.{h,cpp}` | **Drive abstraction** (`DiskDrive`-equivalent): head position (track), side, motor on/off + delayed motor-off timer, ready/write-protect/track-00/index-pulse sense, disk-changed latch, and byte-addressed sector access to the mounted image. Single built-in drive (`drives=1`); no multiplexer needed (one drive), but keep a thin `select(drive)` so drive-select `11`/`01` return NONE/not-ready like real HW. | `references/openmsx-21.0/src/fdc/DiskDrive.{cc,hh}`, `RealDrive.{cc,hh}` (esp. `RealDrive.cc:263-321` `setMotor` delayed-off), `DriveMultiplexer.{cc,hh}` |
| `src/devices/fdc/disk_image.{h,cpp}` | **Deterministic 720 KB sector image**: 737,280-byte store (80×2×9×512), CHS↔LBA geometry (media 0xF9), read/write sector, write-protect flag, disk-present + disk-changed flags. In-memory, synthesized or loaded from a repository fixture — **no host-FS/wall-clock nondeterminism** (see §5). | `references/openmsx-21.0/src/fdc/DSKDiskImage.{cc,hh}`, `Disk.{cc,hh}`, `SectorAccessibleDisk.{cc,hh}` |
| `src/devices/fdc/sony_fdc.{h,cpp}` | **Sony connection-style memory decode** — a `core::MemoryDevice` that wraps the DISK-ROM window and decodes `0x7FF8–0x7FFF` to the WD2793 core + Sony glue latches (side/motor/drive-select write, INTRQ/DRQ + DSKCHG read). This is the M13 DISK-ROM reconciliation owner (see §3.1). | `references/openmsx-21.0/src/fdc/PhilipsFDC.{cc,hh}`, `WD2793BasedFDC.{cc,hh}` |

Machine wiring (in `src/machine/`, existing files):
- `src/machine/hbf1xv_machine.{h,cpp}` — own a `SonyFdc` composed over the existing `disk_rom_`
  RomDevice + a `DiskDrive` + the deterministic `DiskImage`; **replace** the raw `disk_rom_`
  attachment at slot `(3, 2, 1)` (`hbf1xv_machine.cpp:62`) with the `SonyFdc` device, so page-1
  reads route through the FDC decode (see §3.1). Supply the WD2793 core a **cycle clock adapter**
  (mirror of the existing `RtcClock`, `hbf1xv_machine.h:234-241`) returning `scheduler_` total
  cycles read-only.
- No new `src/core/` types are required — `core::MemoryDevice` (`src/core/device_contracts.h:35-41`)
  already gives the FDC its slot participation contract. If the WD2793 needs a clock-source
  interface, add it under `src/devices/fdc/` (mirroring `RtcClockSource` in
  `src/devices/rtc/rp5c01.h:14-18`), **not** in core.

Boundary compliance: the WD2793 FSM, drive, and image carry zero slot/PPI/CPU knowledge; the machine
does all composition. Matches how M15 kept `Rp5c01`/`PsgYm2149` slot-agnostic.

---

## 3. Slot-3-2 DISK-ROM ↔ Register-Window Reconciliation

### 3.1 Who owns the decode
**The `SonyFdc` memory device owns the decode**, wrapping the DISK-ROM `RomDevice`. The machine
attaches `SonyFdc` (not the bare `disk_rom_`) at slot cell `(3, 2, 1)`. This is exactly openMSX's
model: `PhilipsFDC` is an `MSXFDC`/`MSXDevice` mapped over `mem base=0x4000 size=0x4000` and it
dispatches register accesses vs ROM accesses internally (`PhilipsFDC.cc:24-59` read,
`:130-174` write, `:9-15` ctor `parseRomVisibility(config, 0x4000, 0x4000)`). Keeping the decode in
the device (rather than special-casing addresses in `SlotBus`) preserves the clean SlotBus contract
(`src/devices/chipset/slot_bus.h:38-39` "Attach a MemoryDevice to a (primary, sub, page) cell") — the
fabric still just routes the page-1 cell to one `MemoryDevice`.

### 3.2 Exact decode (authoritative: PhilipsFDC.cc, not the fact-sheet's inferred table)
openMSX decodes on `address & 0x3FFF` (because the ROM is mapped at base 0x4000). When the device is
attached at page 1, the CPU addresses `0x7FF8–0x7FFF` map to offsets `0x3FF8–0x3FFF`. `SonyFdc`
mirrors this — for a CPU address `a` in page 1, compute `o = a & 0x3FFF`:

| CPU addr | offset `a&0x3FFF` | Read | Write | Source |
|----------|-------------------|------|-------|--------|
| `0x4000–0x7FF7` | `0x0000–0x3FF7` | DISK ROM image | ignored (ROM) | `PhilipsFDC.cc:56-59,114-116` default → `MSXFDC::peekMem` (ROM) |
| `0x7FF8` | `0x3FF8` | WD2793 **status** | WD2793 **command** | `PhilipsFDC.cc:27-28,133-134` |
| `0x7FF9` | `0x3FF9` | WD2793 **track** | WD2793 **track** | `:29-30,136-137` |
| `0x7FFA` | `0x3FFA` | WD2793 **sector** | WD2793 **sector** | `:31-32,139-140` |
| `0x7FFB` | `0x3FFB` | WD2793 **data** | WD2793 **data** | `:33-34,142-143` |
| `0x7FFC` | `0x3FFC` | `sideReg` (peek) | **side select**, bit0 → head side | `:77-80,145-149` |
| `0x7FFD` | `0x3FFD` | `driveReg & ~4`, **bit2 = 0 iff disk changed** (DSKCHG) | **drive-select** (bits1..0) + **motor on** (bit7) | `:35-41,81-94,151-172` |
| `0x7FFE` | `0x3FFE` | `0xFF` (unused) | ignored | `:95-97` |
| `0x7FFF` | `0x3FFF` | interface status: **bit6 = !INTRQ, bit7 = !DTRQ**, all others pulled to 1 | ignored | `:42-55,98-113` |

Critical corrections vs the fact-sheet's *inferred* convention (fact-sheet §2 table + §2 caveat +
Recommendation 4 explicitly flag these as "verify against PhilipsFDC.cc"):
- **DSKCHG is at `0x7FFD` bit 2 (active-low: 0 = changed), NOT at `0x7FFF`.** The fact-sheet's guess
  put disk-change elsewhere; `PhilipsFDC.cc:35-41,89-94` is authoritative.
- **INTRQ/DRQ at `0x7FFF` are ACTIVE-LOW and inverted**: value starts `0xFF`, `IRQ` clears bit 6,
  `DTRQ` clears bit 7 (`PhilipsFDC.cc:51-54`). The fact-sheet's "bit7=INTRQ, bit6=DRQ (active-high)"
  is wrong; model exactly the openMSX inversion.
- **Drive-select encoding** (`0x7FFD` bits 1..0): `00`/`10` → drive A, `01` → drive B, `11` → NONE
  (`PhilipsFDC.cc:158-169`). With one physical drive, B/NONE present as not-ready.
- **Reset writes** `0x3FFC=0` and `0x3FFD=0` (`PhilipsFDC.cc:17-22`) — side 0, drive/motor cleared.

`SonyFdc::mem_read`/`mem_write` implement the table above; the default branch delegates to the
wrapped `RomDevice` (`src/devices/memory/rom_device.h:56-57`). Note the DISK ROM window is exactly
page 1 with **no mirroring** (`Sony_HB-F1XV.xml:174` "verified: no mirroring"), so `SonyFdc` decodes
only the page-1 instance; it does not need the 0x3FF8/0xBFF8/0xFFF8 mirrors openMSX lists for other
machines (`PhilipsFDC.cc:61-68` peek comment).

### 3.3 Determinism note on caching
`PhilipsFDC::getReadCacheLine` returns `nullptr` for the register cache line so the ROM cache never
shadows the live registers (`PhilipsFDC.cc:119-128`). Our fabric has no read cache (SlotBus calls
`mem_read` per access), so no equivalent is needed — but the `SonyFdc` must always evaluate the
`0x7FF8–0x7FFF` decode on every access (no memoization of those eight bytes).

---

## 4. WD2793 Command / Status Model (headline)

Grounded in `references/fact-sheets/FDC for Sony HB-F1XV.md` §3 (register/command tables), §4
(timing), §6 (INTRQ/DRQ), §8 (quirks) and `references/openmsx-21.0/src/fdc/WD2793.cc`.

### 4.1 Registers (WD2793.cc state: `WD2793.hh:144-158`)
Status (STR), Command (CR, write-only, don't load while Busy except Force Interrupt), Track (TR),
Sector (SR), Data (DR). **TR = 0xFF at master reset**, forced to 0x00 when `!TR00` seen
(fact-sheet §3 + §8 quirk "TR=0xFF-at-reset"; model in `reset()`).

### 4.2 Command set (encodings: fact-sheet §3 table; dispatch: `WD2793.cc` setCommandReg)
- **Type I** — Restore (seek track 0), Seek, Step, Step-In, Step-Out. Flags: `r1r0` step rate,
  `V` verify, `h` head-load, `T` update-TR. Restore issues ≤255 step pulses; Seek Error if `!TR00`
  never asserts and `V=1` (fact-sheet §3 command notes, §8 "Seek to track 0"). openMSX
  `startType1Cmd`/`seek`/`step`/`endType1Cmd` (`WD2793.hh:70-75`).
- **Type II** — Read Sector, Write Sector. Flags: `m` multi-record, `C` side-compare, `S` side value,
  `E` settle delay, `a0` DAM type. Streams data via DRQ per byte; sets Lost Data on missed DRQ
  (`WD2793.cc:522` `startType2Cmd`, `:262`/`:679`/`:756` LOST_DATA, `:620` RECORD_NOT_FOUND;
  `WD2793.hh:77-88`).
- **Type III** — Read Address, Read Track, Write Track (format). Write Track parses the streamed
  track image, converting `0xF5→A1`, `0xF6→C2`, `0xF7→2 CRC bytes` (fact-sheet §5 "How the WD2793
  constructs this", §3; `WD2793.cc:822` startType3, `WD2793.hh:89-95`). M16 supports Read Address +
  a standard-layout Write Track sufficient to re-format the flat image; arbitrary flux → B10.
- **Type IV** — Force Interrupt: loadable any time, terminates running command, resets Busy; `i3..i0`
  select interrupt condition; after FI with no running command STR shows Type I semantics
  (fact-sheet §3; `WD2793.hh:97`, `WD2793.cc:1059` `statusReg &= ~BUSY`).

### 4.3 Status-bit semantics (context-sensitive — fact-sheet §3 two status tables)
- Type I / after-FI: b7 Not Ready, b6 Write Protect, b5 Head Loaded, b4 Seek Error, b3 CRC Error,
  b2 Track 00, b1 Index, b0 Busy.
- Type II / III: b7 Not Ready, b6 Write Protect (writes), b5 Record Type / Write Fault, b4 Record Not
  Found, b3 CRC Error, b2 Lost Data, b1 DRQ, b0 Busy.
- Constants match openMSX (`WD2793.cc:15-22`: BUSY=0x01, LOST_DATA=0x04, RECORD_NOT_FOUND=0x10).

### 4.4 INTRQ / DRQ polled service loop (fact-sheet §6; `PhilipsFDC.cc:42-55`)
INTRQ set at command completion / on FI condition, cleared by reading STR (`0x7FF8`) or loading a new
command. DRQ set per ready byte, cleared by reading/writing DR (`0x7FFB`). Both are **read back via
`0x7FFF`** (active-low) — the disk BIOS polls in a tight `DI`-guarded loop; a byte must be serviced
each DRQ window or Lost Data is set (fact-sheet §6 "Timing constraint"). We reproduce DRQ pacing and
Lost Data deterministically (§5.3), so a Busy-polling boot behaves correctly.

### 4.5 Motor + delayed motor-off (~4 s) (fact-sheet §4 "Head-load/motor timing"; XML
`motor_off_timeout_ms=4000`; `RealDrive.cc:263-321`)
Writing `0x7FFD` bit7 sets motor-on immediately; clearing it (or after last access) starts a **4 s**
motor-off timer. Ready/head-loaded derive from motor-on for this design (HLD→HLT tied, no head-load
delay — `WD2793.hh:129-135`). Modelled deterministically off elapsed cycles (§5.2): 4 s ×
3,579,545 Hz ≈ **14,318,180 cycles** motor-off timeout.

### 4.6 Disk-changed / not-ready quirks for DSKCHG (fact-sheet §8 "Disk-change / DSKCHG quirk")
`0x7FFD` bit2 reports disk-changed (0 = changed). Because MSX has no reliable disk-changed line,
DSKCHG frequently returns "unknown" and DOS1 re-reads the boot sector; our latch reports changed only
when the mounted image is (deterministically) swapped, else unchanged. Not-ready is reported when no
disk is present or drive B/NONE is selected. Model `RealDrive` not-ready-with-motor-off nuance
(`RealDrive.cc:156-157,256-259`) at least to the extent the boot path needs it.

---

## 5. Determinism (hard requirement)

### 5.1 Deterministic disk image
The mounted medium is a **fixed 737,280-byte** (80 tracks × 2 sides × 9 sectors × 512 bytes) 2DD
image, media descriptor **0xF9**, CHS↔LBA per fact-sheet §5. **No host-filesystem or wall-clock
input feeds emulation state.** Two sources, both deterministic:
1. **Synthesized in-memory image** built by a pure function (e.g. all-0x00 data with a valid FAT12
   boot sector + BPB, or a fixed byte pattern) for unit tests and the default boot medium. This is
   generated in code from constants — byte-for-byte reproducible.
2. **Repository fixture** `tests/parity/m16_boot.dsk` (a committed 737,280-byte image) used for the
   A/B test so the *identical* medium is presented to both emulators (§6). This lives beside the
   existing `tests/parity/m15_io_probe.bin`. A `tools/` generator (PowerShell or Python) produces it
   deterministically from the same constants as (1) so the fixture is reproducible and auditable
   (evidence, not fabricated). A real third-party `.dsk` is **not** required and must not be assumed.

Where a real `.dsk` asset would live if later introduced: under a test/fixture dir (e.g.
`tests/parity/` or a new `roms/disks/`), never loaded from an arbitrary host path during a
deterministic test. Runtime host-FS disk mounting is a frontend/CLI concern (C9), out of M16.

### 5.2 Time base — emulated cycles only (M15 X4 pattern)
All FDC timing (Busy duration, DRQ cadence, step/settle, index pulse, 4 s motor-off) derives from the
emulated cycle clock, mirroring the M15 RTC design: `Rp5c01` advances only from `RtcClockSource`
returning `scheduler` total cycles read-only (`src/devices/rtc/rp5c01.h:14-18,71`;
`src/machine/hbf1xv_machine.h:234-241` `RtcClock`). The FDC gets an analogous `FdcClockSource`
(returning `Hbf1xvMachine::elapsed_cycles()`), and computes command-completion / DRQ-window
deadlines as absolute cycle counts. On each register access the core `sync(now_cycles)` advances the
FSM to `now`. **CPU T-state accounting is untouched** (protects the M9/M12 CPU oracles, same
guarantee X4 gave in M15).

Cycle conversions (fact-sheet §4; system clock 3,579,545 Hz):
- 1 MFM byte ≈ 32 µs → ≈ **114 cycles/byte** (DRQ cadence). openMSX uses a `DynamicClock drqTime` at
  the track byte-rate (`WD2793.hh:112-113`, `WD2793.cc:89` `setDrqRate`) — we compute the same
  cadence in integer cycles.
- Step rate 6/12/20/30 ms; settle 15/30 ms (`V`/`E`); index pulse ≈ 200 ms; sector transfer 512×32 µs
  ≈ 16.4 ms; Write Track ≈ 1 revolution ≈ 200 ms.
- Motor-off ≈ 4 s ≈ 14,318,180 cycles.

### 5.3 Deterministic DRQ / Lost Data
DRQ asserts on the computed cadence; if the guest reads/writes DR before the next window, the byte
transfers; if it misses, Lost Data (b2) is set (read: byte lost; write: 0x00 substituted, command
continues) per fact-sheet §8 "Lost Data semantics differ read vs write" and `WD2793.cc:262,679,756`.
Because the cadence is a pure function of elapsed cycles, the pass/fail of every DRQ window is
reproducible run-to-run.

---

## 6. Boot Advance (C5) Assessment + Checkpoint

### 6.1 Current state
M15 advanced the deterministic boot checkpoint to final **PC 0x454 / max PC 0x488 over 4096
instructions** with real PSG/RTC/keyboard/VDP but **no FDC** (`docs/m15-implementation-report.md`;
backlog C5 IN-PROGRESS: "full boot still pending FDC/M16",
`agent-protocol/state/deferred-backlog.md:47`). Without an FDC answering slot 3-2, the BIOS's disk
scan/handshake cannot progress; the machine idles near the early BIOS boot loop.

### 6.2 Expected advance with the FDC present
With `SonyFdc` answering page 1 and a disk mounted, the BIOS disk-ROM handshake can proceed: the
disk-ROM header at `0x4000` ('AB' + init/entry) is now real ROM (already mapped in M13), and the
disk driver's device touches become live. The DISK BIOS (fact-sheet §2 jump table) will perform, in
roughly this order, reads/writes the FDC must answer:
- **DSKCHG** (`0x7FFD` bit2) and **drive/motor** latch writes (`0x7FFD`), **side** (`0x7FFC`).
- **WD2793 status poll** (`0x7FF8`) and **INTRQ/DRQ poll** (`0x7FFF`).
- **Type I Restore/Seek** to track 0, then **Type II Read Sector** of the **boot sector (LBA 0)**
  into RAM for **GETDPB / boot** (fact-sheet §2 DSKIO/GETDPB).

How far it goes (boot sector read, DPB build, and possibly the boot-sector chain to a DiskBASIC/DOS
prompt) is **not asserted a priori** — we do not have the disk-ROM disassembly and must not invent a
PC. The checkpoint is **self-derived and A/B-confirmed** (same method M13/M15 used:
`docs/m13-parity-trace-diff.md`, `docs/m15-parity-trace-diff.md`).

### 6.3 Boot-checkpoint acceptance signal (deterministic)
Define the M16 checkpoint as a deterministic tuple captured by running the machine with the FDC +
`tests/parity/m16_boot.dsk` for a fixed instruction budget:
- **Primary signal**: the machine reaches (and the CPU trace records) a **successful boot-sector
  read** — i.e. the FDC services a Type II Read Sector of LBA 0 and the 512 bytes land in RAM — with
  the CPU PC advancing **past the M15 final 0x454** into the disk-driver code path. Concretely:
  assert (a) a Read Sector command was accepted at `0x7FF8`, (b) 512 DRQ byte-transfers occurred
  through `0x7FFB`, (c) command completed with INTRQ set and no error bits, (d) final/max PC strictly
  greater than the M15 checkpoint, over a fixed instruction budget.
- **Determinism**: self-derived golden (record the exact PC/instruction-count tuple once green, pin
  it in the integration test), reproduced byte-for-byte across runs. Exact PC value is **filled in by
  the developer from the real run**, not guessed here.
- **A/B**: the same PC/behaviour must match openMSX to the same checkpoint (§7). If, empirically, the
  Sony disk ROM stops earlier (e.g. waits on a specific status bit we must refine), the checkpoint is
  set at the furthest deterministic, A/B-matched point — still strictly past 0x454 — and any residual
  is documented (not fabricated).

---

## 7. openMSX A/B Acceptance

Real captured trace-diff vs openMSX `Sony_HB-F1XV`, using the existing parity harness
(`tools/openmsx-ab-smoke.ps1` / the opcode-trace parity tool used for M11–M15,
`docs/m1x-parity-trace-diff.md` precedent). **No parity claim without a genuine capture** (CLAUDE.md
evidence gates; guardrails).

- **Subject sequence**: a CPU program (or the real boot path) that drives the FDC register/command
  sequence: write side/drive/motor (`0x7FFC`/`0x7FFD`), Restore (Type I) to track 0, Seek, then a
  **Read Sector (Type II) of LBA 0** from the mounted image, polling status (`0x7FF8`) and
  INTRQ/DRQ (`0x7FFF`) and reading the 512 data bytes through `0x7FFB`; include a DSKCHG read
  (`0x7FFD` bit2). Capture per-step architectural state (PC/opcode/registers/flags) + the FDC-visible
  reads.
- **Identical medium (hard)**: the **same** `tests/parity/m16_boot.dsk` (§5.1) is presented to both
  emulators — mounted in our machine and passed to openMSX (`-diska tests/parity/m16_boot.dsk` or the
  harness's disk-insert). A diff is meaningless unless both read the same sectors; state this in the
  harness invocation.
- **Adversarial comparator check** (as QA did for M10–M15): confirm an empty-side input → BLOCKED and
  a corrupted field → DIVERGENCE, so an empty diff is trustworthy.
- **Output**: `docs/m16-parity-trace-diff.md` with the real captured diff; referenced from the
  implementation report + QA sign-off.

---

## 8. Slice Plan (M16-S1 … S6)

Every slice: deterministic unit tests + (from S5) system-integration tests; each cycle runs the
evidence gates — `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile
docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug
--output-on-failure`. Test dirs follow the repo layout (`tests/unit/devices/fdc/`,
`tests/integration/machine/`, `tests/parity/`).

### M16-S1 — Deterministic disk image + geometry
- **Goal**: 737,280-byte 2DD image, CHS↔LBA (9/2/80, media 0xF9), read/write sector, write-protect +
  disk-present + disk-changed flags; synthesized-image generator (pure, reproducible).
- **Files**: `src/devices/fdc/disk_image.{h,cpp}`; `tools/` deterministic `.dsk` generator; fixture
  `tests/parity/m16_boot.dsk`.
- **Unit tests** (`tests/unit/devices/fdc/disk_image_unit_test.cpp`): size == 737280; LBA↔CHS
  round-trip for boundary sectors (LBA 0, 8, 9, 17, 1439); read-after-write determinism; two
  synthesized images byte-identical; write-protect blocks writes.
- **Gates**: build + ctest green.

### M16-S2 — WD2793 core: registers + Type I + status semantics
- **Goal**: five registers, reset (TR=0xFF), Type I Restore/Seek/Step(-In/-Out), Type-I status bits
  (Busy/Track00/Seek Error/Index), INTRQ set/clear, step-rate/settle timing in cycles.
- **Files**: `src/devices/fdc/wd2793.{h,cpp}`, `src/devices/fdc/disk_drive.{h,cpp}`,
  `FdcClockSource` (mirror of `RtcClockSource`).
- **Unit tests** (`tests/unit/devices/fdc/wd2793_type1_unit_test.cpp`): reset TR=0xFF; Restore sets
  TR=0 and Track00; Seek toward DR updates TR; Seek Error when no TR00 and V=1 (≤255 step cap);
  Busy set→clear with INTRQ after computed cycles; status shows Type-I layout after FI.
- **Gates**: build + ctest green.

### M16-S3 — WD2793 core: Type II/III/IV + DRQ / Lost Data
- **Goal**: Read Sector / Write Sector (single + multi-record), Read Address, standard-layout Write
  Track, Force Interrupt; DRQ cadence (~114 cyc/byte), Lost Data (read vs write), Record Not Found,
  CRC-16-CCITT over ID/data, Type-II/III status bits.
- **Files**: `src/devices/fdc/wd2793.{h,cpp}` (+ image/drive glue).
- **Unit tests** (`tests/unit/devices/fdc/wd2793_type2_unit_test.cpp`,
  `..._type3_unit_test.cpp`): Read Sector LBA 0 → 512 DRQ bytes match image + CRC ok + INTRQ, no
  error; missed DRQ → Lost Data (read: byte lost; write: 0x00 written, continues); Record Not Found
  for a non-existent sector; multi-record stops at track boundary; Force Interrupt clears Busy and
  reverts STR to Type-I; Read Address returns CHRN + copies track to SR; Write Track re-formats a
  standard 9-sector track and Read Sector reads it back.
- **Gates**: build + ctest green.

### M16-S4 — Sony connection-style memory decode (SonyFdc)
- **Goal**: `core::MemoryDevice` wrapping the DISK-ROM RomDevice; decode `0x7FF8–0x7FFF` per §3.2
  (status/cmd/track/sector/data + side/drive/motor + DSKCHG + INTRQ/DRQ active-low at 0x7FFF);
  reset writes 0x3FFC=0/0x3FFD=0; ROM elsewhere in page 1.
- **Files**: `src/devices/fdc/sony_fdc.{h,cpp}`.
- **Unit tests** (`tests/unit/devices/fdc/sony_fdc_unit_test.cpp`): read `0x4000` → DISK-ROM byte,
  read `0x7FF7` → ROM, read `0x7FF8` → status; write `0x7FF8` → command reaches core; side/drive/
  motor latch writes observable; DSKCHG bit2 at `0x7FFD` reflects disk-changed; `0x7FFF` bit6/bit7
  are active-low INTRQ/DTRQ; `0x7FFE` reads 0xFF; ROM writes ignored; **no mirroring** outside page 1.
- **Gates**: build + ctest green.

### M16-S5 — Machine wiring + motor-off + system integration
- **Goal**: compose `SonyFdc` over `disk_rom_` + drive + image in `Hbf1xvMachine`; attach at slot
  `(3, 2, 1)` replacing the raw ROM; feed the WD2793 the cycle clock; wire ~4 s delayed motor-off;
  mount the deterministic default image; accessors for tests.
- **Files**: `src/machine/hbf1xv_machine.{h,cpp}` (+ CMake for the new sources / test dirs).
- **Integration tests** (`tests/integration/machine/hbf1xv_m16_fdc_integration_test.cpp`): CPU program
  over the M11 bus drives Restore + Read Sector of LBA 0 and reads 512 bytes matching the image;
  motor-on then motor-off after the 4 s cycle budget; DSKCHG through the bus; **zero regression** on
  M1–M15 suites.
- **Gates**: build + ctest green (full suite).

### M16-S6 — Boot-checkpoint advance + openMSX A/B parity
- **Goal**: run the machine with FDC + `m16_boot.dsk`, capture the deterministic boot checkpoint
  (§6.3, strictly past PC 0x454), pin it as a golden; produce the real A/B trace-diff (§7).
- **Files**: `tests/integration/machine/hbf1xv_m16_boot_checkpoint_integration_test.cpp`;
  `docs/m16-parity-trace-diff.md`; harness/tooling updates for disk-mounted A/B.
- **Integration tests**: boot-checkpoint golden reproduced byte-for-byte across two runs; checkpoint
  PC strictly > M15 0x454; boot-sector read observed (Type II + 512 DRQ + INTRQ, no error).
- **A/B**: `docs/m16-parity-trace-diff.md` real captured diff vs openMSX `Sony_HB-F1XV` with the same
  disk; adversarial comparator check documented.
- **Gates**: full evidence-gate set incl. asset validation + checksum refresh; A/B captured.

---

## 9. Deferred-Backlog Consultation (per DEC-0005)

Consulted `agent-protocol/state/deferred-backlog.md` in full.
- **B8 (FDC drive mechanics)** — M16 **closes** this. Mark `DONE (M16)` on QA sign-off (keep the row).
- **C5 (full boot past first device read)** — M16 **advances** it (boot-sector-read checkpoint past
  0x454). C5 remains **IN-PROGRESS** unless/until a full prompt is reached and A/B-confirmed; the
  exact residual is documented at closure, not fabricated.
- **New row proposed — B10 (FDC flux/DMK copy-protection fidelity)**: DMK/RawTrack images, arbitrary
  non-standard track layouts, exact 5-revolution RNF search, flux-level protection (fact-sheet Stage
  3). Deferred out of M16 (Stage 1–2 scope). Grounding: `FDC for Sony HB-F1XV.md` §5/§8;
  `references/openmsx-21.0/src/fdc/` (RawTrack/DMK). Record on the ledger at implementation time.
- **All other rows re-affirmed OPEN** (no change): B3 FM-PAC/OPLL, B4 MSX-JE SRAM, B5 Kanji-font I/O,
  B6 Halnote firmware, B7 cartridge loading, C1/C2 exact cycle timing, C3 ZEXALL sweep, C7
  printer/cassette, C8 Sony speed/pause+Ren-Sha, C9 SDL3 frontend, D1–D7 VDP depth. B1/B2/B9/C4/C6
  remain DONE. Indicative follow-on order unchanged (M17 FM-PAC/OPLL next).

---

## 10. Acceptance Criteria (milestone-level)

1. WD2793 core (5 registers; Type I/II/III/IV; context-sensitive status bits; INTRQ/DRQ; deterministic
   Busy/DRQ/step/settle/index/motor timing) implemented under `src/devices/fdc/`, grounded in
   `FDC for Sony HB-F1XV.md` §3–§8 and `references/openmsx-21.0/src/fdc/WD2793.cc` (concrete citations
   in the implementation report).
2. Sony connection-style decode (`SonyFdc`) wraps the M13 DISK-ROM RomDevice and answers
   `0x7FF8–0x7FFF` per §3.2 (authoritative `PhilipsFDC.cc`), ROM elsewhere in page 1, attached at slot
   `(3, 2, 1)` replacing the bare ROM.
3. Fully deterministic 720 KB image (737,280 bytes, media 0xF9); all timing off emulated cycles
   (elapsed_cycles), no host-FS/wall-clock; CPU T-state accounting untouched (M9/M12 oracles green).
4. DSKCHG / not-ready / disk-changed quirks modelled (`0x7FFD` bit2; drive-B/NONE not-ready).
5. Boot advances past the M15 checkpoint (PC 0x454) to a deterministic, self-derived, A/B-matched
   boot-sector-read checkpoint (§6.3).
6. Real openMSX A/B trace-diff vs `Sony_HB-F1XV` over the FDC register/command + sector-read sequence
   with the **same** disk image (§7) → `docs/m16-parity-trace-diff.md`; comparator adversarially
   checked; no parity claim without a genuine capture.
7. Deterministic unit + system-integration tests (§8); **zero regression** across M1–M15.
8. Evidence gates executed and captured each cycle (validate-assets, checksum, Debug build, ctest).
9. Backlog updated same-cycle: B8 → DONE (M16), C5 advanced, B10 recorded; rest re-affirmed.
10. QA sign-off before closure; normal human-release-decision gate (no auto-close).

---

## 11. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|---------------------|
| R-M16-1 | Sony glue-bit decode wrong (fact-sheet convention is inferred and partly wrong — DSKCHG at 0x7FFD not 0x7FFF; 0x7FFF active-low). | Implement strictly from `references/openmsx-21.0/src/fdc/PhilipsFDC.cc:24-172`; S4 unit tests assert each bit; S6 A/B empty diff over the register reads confirms against openMSX. (fact-sheet Recommendation 4.) |
| R-M16-2 | DRQ/Busy timing so aggressive it breaks the M9/M12 CPU oracles or so loose the polled DSKIO loop mis-syncs and boot stalls. | Timing derived read-only from `elapsed_cycles()` (X4 pattern), CPU T-state math untouched; verify M9/M12 oracle suites unchanged/green; tune DRQ cadence (~114 cyc/byte) against the boot poll loop; S6 A/B confirms Busy/INTRQ transitions match openMSX. |
| R-M16-3 | Boot doesn't actually advance past 0x454 (disk ROM waits on a status nuance we mis-model), so C5 gain is nil. | S6 runs the real disk ROM against the mounted image and records the furthest deterministic A/B-matched PC; if it stalls, trace-diff pinpoints the diverging FDC read and the model is corrected; residual documented honestly (no fabricated PC). |
| R-M16-4 | Disk image nondeterminism (accidental host-FS/time dependency) breaks reproducibility. | Image synthesized from constants + committed fixture; S1 test asserts two builds byte-identical; no `std::filesystem`/clock reads in the emulation path (only the fixture load at setup, which is a fixed committed file). |
| R-M16-5 | openMSX A/B not truly comparable because the two emulators see different media. | Present the identical `tests/parity/m16_boot.dsk` to both; document the exact openMSX `-diska` invocation; adversarial comparator check (empty-side→BLOCKED, corrupted→DIVERGENCE) as in M10–M15. |
| R-M16-6 | DISK ROM asset absent locally → boot can't run. | M13 already loads `bios/f1xvdisk.rom` (`hbf1xv_machine.cpp:217`); `tools/validate-assets.ps1` gate confirms presence; if absent, 0xFF-fill degrades deterministically and the boot-advance criterion is reported as blocked (documented), not faked. |
| R-M16-7 | Scope creep into flux/DMK (Stage 3) inflates M16. | Hold M16 at Stage 1–2 (flat image, standard Write Track); record B10 for flux fidelity; any expansion requires a decisions-ledger entry. |

---

## 12. Developer Handoff

- **Build/first**: add `src/devices/fdc/` sources + `tests/unit/devices/fdc/` and the new integration
  tests to CMake (`CMakeLists.txt`, `tests/CMakeLists.txt`); keep headless + SDL3 configs building.
- **Sequence**: S1 (image) → S2 (WD2793 Type I) → S3 (Type II/III/IV + DRQ) → S4 (SonyFdc decode) →
  S5 (machine wiring + motor-off + integration) → S6 (boot checkpoint + A/B). Run all four evidence
  gates every slice; keep M1–M15 green each cycle.
- **Authoritative decode**: use `references/openmsx-21.0/src/fdc/PhilipsFDC.cc` for the `0x7FF8–0x7FFF`
  bit map (NOT the fact-sheet's inferred table) — §3.2. Cite `WD2793.cc` line ranges in the
  implementation report for each command/status behaviour. **Never copy openMSX code** (GPL isolation).
- **Determinism**: FDC clock via an `FdcClockSource` mirroring `src/devices/rtc/rp5c01.h:14-18` +
  `src/machine/hbf1xv_machine.h:234-241` `RtcClock`; all deadlines in absolute emulated cycles; do
  not touch CPU T-state accounting (protects M9/M12 oracles).
- **Reconciliation**: replace `slot_bus_.attach(3, 2, 1, &disk_rom_)`
  (`src/machine/hbf1xv_machine.cpp:62`) with the `SonyFdc` (which wraps `disk_rom_`).
- **Boot checkpoint**: derive the exact PC/instruction tuple from the real run (do not guess); pin as
  a golden; must be strictly past 0x454 and A/B-matched.
- **A/B**: same `tests/parity/m16_boot.dsk` to both emulators → `docs/m16-parity-trace-diff.md`; run
  the adversarial comparator checks.
- **Ledger**: on closure, update `agent-protocol/state/deferred-backlog.md` (B8 → DONE (M16), C5
  advanced, add B10), `state/milestones.md`, and the channels; refresh `docs/asset-checksums.txt`.
- **Durable artifacts to produce**: `docs/m16-implementation-report.md`, `docs/m16-parity-trace-diff.md`,
  `docs/m16-qa-signoff.md`.
