# Floppy Disk Controller Fact-Sheet — Sony HB-F1XV (MSX2+)
### Fact-sheet #5 in the HB-F1XV emulation series (companions: V9958 VDP, YM2413/FM-PAC, Z80A CPU, S1985 MSX-ENGINE)

## TL;DR
- The Sony HB-F1XV uses a **Fujitsu MB89311** CMOS floppy disk controller/formatter — a WD279x-family part that is instruction- and register-compatible with the **Western Digital WD2793**; openMSX therefore models the HB-F1XV as a `WD2793` device with `connectionstyle=Sony`, and any WD2793/FD1793-accurate core will drive it correctly. Emulate it as a WD2793, **not** as an Intel 8272/Toshiba TC8566AF.
- The FDC is a **memory-mapped** interface living in a secondary slot (openMSX places it at slot **3-2**, ROM+registers in page 1, 0x4000–0x7FFF), with the four core WD2793 registers at **7FF8h–7FFBh** (status/command, track, sector, data) plus Sony glue-logic registers for side/motor/drive-select and an INTRQ/DRQ status read in the 7FFC–7FFFh window. There is **one** built-in 3.5" 720KB (2DD) drive.
- For behavior-accuracy you must model: WD2793 Type I/II/III/IV command semantics and per-command status-bit meanings, MFM double-density byte timing (~32 µs/byte at 250 kbit/s), the polled INTRQ/DRQ service loop the MSX disk BIOS uses, the delayed motor-off (~4 s) behavior, and the WD2793 "disk changed" / not-ready quirks that the DSKCHG BIOS routine depends on.

---

## 1. Chip identification & context

**Part / manufacturer.** The HB-F1XV floppy controller chip is the **Fujitsu MB89311**, described in Fujitsu's catalog as a "CMOS Floppy Disk Controller / Formatter." This is stated directly on the MSX Wiki HB-F1XV page — verbatim: *"The CPU is a Z80A from Zilog (Z0840004SPC) and the MSX-Engine is the S1985. The floppy disk controller chip is the MB89311."* — and repeated on the near-twin HB-F1XDJ page (*"The floppy disk controller chip is the MB89311."*). The MB89311 belongs to the WD179x/WD279x-compatible lineage (the same family Fujitsu served with its earlier MB8876A/MB8877A, which are documented clones of the WD1791/WD1793). Functionally the MB89311 behaves as a CMOS WD2793: same five programmer-visible registers, same Type I/II/III/IV command set, same status semantics, MFM (IBM System 34, double density) and FM (IBM 3740, single density) support.

**Why "WD2793" is the right emulation model.** openMSX's machine file `share/machines/Sony_HB-F1XV.xml` describes the controller as:
```xml
<WD2793 id="Memory Mapped FDC">
  <connectionstyle>Sony</connectionstyle>
  <motor_off_timeout_ms>4000</motor_off_timeout_ms>
  <drives>1</drives>
  <rom>
    <filename>hb-f1xv_disk.rom</filename>
    <sha1>5a4e7dbbfb759109c7d2a3b38bda9c60bf6ffef5</sha1>
  </rom>
  <mem base="0x4000" size="0x4000"/>
</WD2793>
```
In openMSX's `src/DeviceFactory.cc` the mapping is: `WD2793 → PhilipsFDC`, `Microsol → MicrosolFDC`, `MB8877A → NationalFDC`, `TC8566AF → TurboRFDC`. All of the first group share one WD2793 core; they differ only in *how the WD2793 registers and the glue-logic control/status registers are decoded into the MSX memory map* (the "connection style"). Sony is one such style. This matches the classification on the MSX Assembly Page and MSX Resource Center: the WD2793(-02) is "used in most MSX2's," the MB8877A in Sanyo/Victor/National machines, and the TC8566AF (an 8272 clone) in Panasonic and turboR machines.

**Contrast with sibling machines.** The earlier MSX2 Sony **HB-F1XD / HB-F1XDmk2** use a discrete WD2793 — the MSX Wiki HB-F1XD page states verbatim: *"The Sony HB-F1XD is a MSX2 with a Z80A from Sharp (LH-0080A) and an MSX-Engine S1985 from Yamaha... The floppy disk controller chip is the 2793."* (the HB-F1XDmk2 page is identical). The MSX2+ **HB-F1XDJ** and **HB-F1XV** integrate the equivalent function into the Fujitsu MB89311. From the programmer's/emulator's viewpoint they are the same controller. (Note the naming trap: HB-F1XD is an MSX2; HB-F1XDJ and HB-F1XV are MSX2+.)

**Package / clock.** The WD2793/FD1793 family is a 40-pin DIL device (the MB89311 is the CMOS equivalent). The family is driven from a **1 MHz** clock for 5.25"/3.5" (250 kbit/s MFM double-density) operation; step-rate and settling timings in the datasheet are quoted for a 1 MHz clock and change for a 2 MHz clock. Key drive-interface pins used by MSX designs: `INTRQ` (command-complete interrupt request), `DRQ` (data request), `TR00`/`!TR00` (track-0 sense), `IP` (index pulse), `WPRT` (write protect), `READY`, `STEP`, `DIRC` (direction), `WD`/`RD` (write/read data), and `HLD`/`HLT` (head-load / head-load-timing).

**Board block diagram (HB-F1XV disk interface, conceptual).**
```
        Z80A (via S1985 MSX-ENGINE, slot/subslot decode)
             │  A0..A15, D0..D7, /RD /WR /SLTSL(3-2)
             ▼
   ┌───────────────────────────────────────────────┐
   │  Disk interface (secondary slot 3-2, page 1)   │
   │                                                │
   │  Disk-ROM (16KB) ── 0x4000..0x7FF7             │
   │  Address decode (A0..A2 + CS) ── 0x7FF8..0x7FFF│
   │        │                                       │
   │        ├─► MB89311 (WD2793-class FDC)          │
   │        │      Status/Cmd 7FF8, Track 7FF9,     │
   │        │      Sector 7FFA, Data 7FFB           │
   │        ├─► Glue latch: side/motor/drive-sel    │
   │        └─◄ Glue status: INTRQ(b7)/DRQ(b6)      │
   │        │                                       │
   │   Data separator / PLL (clock recovery, MFM)   │
   │   Motor-off delay logic (~4 s)                 │
   └───────────────┬───────────────────────────────┘
                   │ 34-pin Shugart-style FDD bus
                   ▼
        Built-in 3.5" 2DD drive (720KB, 300 rpm)
```
The disk ROM occupies page 1 (0x4000–0x7FFF); the top eight bytes (0x7FF8–0x7FFF) are stolen for memory-mapped register access, decoded from `A0..A2` plus the interface's chip-select. Cross-reference the **S1985 fact-sheet**: the S1985 MSX-ENGINE provides the PPI (8255-compatible) primary-slot select at I/O port 0xA8 and the per-primary-slot secondary-slot select register at memory address 0xFFFF; the disk interface answers on `/SLTSL` for its assigned (sub)slot, so slot decoding for the FDC is handled by the standard S1985 slot machinery, not by the FDC itself.

---

## 2. MSX disk interface architecture

**Memory-mapped, not I/O-mapped.** Unlike the Microsol style (which uses Z80 I/O ports 0xD0–0xD7), the standard "Philips/Sony" MSX disk interface — including the HB-F1XV — is **memory-mapped** into page 1. A disassembly of the Sony HB-720 interface documentation states verbatim: *"Individual registers of the FDC (U4) have been allocated to addresses 7FF8H through 7FFBH, and are selected by address signals A0 through A2 and signal CS."* This is the canonical Sony layout and applies to the HB-F1XV.

**Register/address map (Sony connection style, page 1).**

| Address | Access | Function |
|---|---|---|
| 0x4000–0x7FF7 | R | Disk-ROM (Disk BASIC + MSX-DOS 1 kernel + driver) |
| 0x7FF8 | R = Status / W = Command | WD2793 status/command register |
| 0x7FF9 | R/W | WD2793 Track register |
| 0x7FFA | R/W | WD2793 Sector register |
| 0x7FFB | R/W | WD2793 Data register |
| 0x7FFC | W (Sony) | Side-select register (bit 0 = side) |
| 0x7FFD | W (Sony) | Drive-select / motor-on control latch |
| 0x7FFF | R (Sony) | Interface status: bit 7 = INTRQ, bit 6 = DRQ |

The four **core** WD2793 registers at 7FF8–7FFB are firmly confirmed (Sony HB-720 doc; openMSX PhilipsFDC decodes `address & 0x3FFF` → offsets 0x3FF8–0x3FFB). The exact placement of the side/motor/drive-select latch and the INTRQ/DRQ status read within the 7FFC–7FFF window is the part that distinguishes the *Sony* connection style from the *Philips* style; the convention shown above (side at 7FFC, drive/motor at 7FFD, INTRQ/DRQ status at 7FFF with INTRQ in bit 7 and DRQ in bit 6) matches how the Disk BIOS polls the controller. **Verify the precise bit assignments against `src/fdc/PhilipsFDC.cc`** before shipping, because these glue bits vary by manufacturer even when the FDC core is identical (see §8 and Recommendation 4).

**Disk BIOS entry points (jump table at 0x4010 of the disk interface ROM).** The MSX disk driver exposes a fixed jump table; a table at 0xFB21 (per-interface: byte 0 = number of drives, byte 1 = slot id) lets the kernel enumerate up to 4 disk interfaces:

| Address | Routine | Purpose | Maps to FDC activity |
|---|---|---|---|
| 0x4010 | DSKIO | Read/write sector(s) | Seek (Type I) + Read/Write Sector (Type II), often multi-sector |
| 0x4013 | DSKCHG | Disk-change status | reads write-protect / not-ready / disk-changed sense; may re-read boot sector |
| 0x4016 | GETDPB | Build Drive Parameter Block | reads boot sector, parses BPB |
| 0x4019 | CHOICE | Return format-choice string | (no FDC access; returns menu text) |
| 0x401C | DSKFMT | Physically + logically format | Restore/Seek + Write Track (Type III) per track, then write FS structures |
| 0x401F | MTOFF/DSKSTP | Stop motor (present if byte ≠0) | drops motor-on latch / starts motor-off timer |

DSKIO register interface: `A`=drive (0=A:), `F`=carry set→write / clear→read, `B`=sector count, `C`=media descriptor (0xF9 for 720KB 2DD), `DE`=logical sector number (LBA, starts at 0), `HL`=transfer address. Error codes returned in `A`: 0=write-protected, 2=not ready, 4=CRC/data error, 6=seek error, 8=record not found, 10=write fault, 12=other. DSKCHG returns in `B`: 0x01=unchanged, 0x00=unknown, 0xFF=changed (the DOS1 kernel expects the DPB updated when the status is unknown/changed).

**A crucial page-0/page-1 subtlety for emulator authors.** The disk ROM sits in page 1 (0x4000–0x7FFF) while the FDC data-transfer routine must be able to move sector data to/from page 0 and page 1 addresses that would otherwise be shadowed by the ROM/BIOS. Real MSX disk drivers copy a small, position-independent DSKIO transfer routine into a RAM sector buffer (`$SECBUF`) and execute it there, disabling interrupts around the tight DRQ loop. An accurate emulator does not need to replicate the copy, but it must ensure the FDC data register is reachable while page 1 holds the ROM — which is automatic given the memory-mapped 0x7FFB decode.

**Slot / subslot configuration on the HB-F1XV.** openMSX places the disk ROM+FDC in **secondary slot 3-2** (slot 3 is the internal, expanded slot containing RAM, sub-ROM, Kanji BASIC, and the disk interface). The Kanji BASIC ROM is in another 3-x subslot; main RAM (64KB) is elsewhere in slot 3. The S1985 handles the 0xFFFF secondary-slot register for slot 3. When MSX-DOS boots, the kernel finds the disk ROM slot id in system variable 0xF348 and the per-interface slot table at 0xFB21/0xFB22.

---

## 3. FDC registers (WD2793 core)

**Processor interface / register selection** (A1,A0 within the FDC's 4-register block; on MSX these are the low 2 bits of 7FF8–7FFB):

| A1 A0 | Read | Write |
|---|---|---|
| 0 0 | Status Register | Command Register |
| 0 1 | Track Register | Track Register |
| 1 0 | Sector Register | Sector Register |
| 1 1 | Data Register | Data Register |

- **Data Shift Register** (internal, not addressable): serializes/deserializes disk data.
- **Data Register (DR)** — holding register for read/write bytes; also holds the destination track for a Seek.
- **Track Register (TR)** — current head track; incremented on step-in, decremented on step-out; compared against the ID-field track number during read/write/verify. Master-reset sets TR = 0xFF; when `!TR00` goes low, TR is forced to 0x00. Do not load while Busy.
- **Sector Register (SR)** — target sector; compared against the ID-field sector number. Do not load while Busy.
- **Command Register (CR)** — write-only; do not load while Busy except for Force Interrupt.
- **Status Register (STR)** — read-only; **bit meanings depend on the last command type** (see tables below).

### Command set and encodings

| Type | Command | b7 b6 b5 b4 b3 b2 b1 b0 |
|---|---|---|
| I | Restore (seek track 0) | 0 0 0 0 h V r1 r0 |
| I | Seek | 0 0 0 1 h V r1 r0 |
| I | Step | 0 0 1 T h V r1 r0 |
| I | Step-In | 0 1 0 T h V r1 r0 |
| I | Step-Out | 0 1 1 T h V r1 r0 |
| II | Read Sector | 1 0 0 m S E C 0 |
| II | Write Sector | 1 0 1 m S E C a0 |
| III | Read Address | 1 1 0 0 0 E 0 0 |
| III | Read Track | 1 1 1 0 0 E 0 0 |
| III | Write Track (format) | 1 1 1 1 0 E 0 0 |
| IV | Force Interrupt | 1 1 0 1 i3 i2 i1 i0 |

**Flag bits:** `r1 r0` = step rate; `V` = verify destination track (Type I); `h` = head-load-at-start; `T` = update Track Register on step; `a0` = data address mark (0→0xFB normal, 1→0xF8 deleted); `C` = side-compare enable (Type II); `S` = side-compare value (0/1); `E` = 15/30 ms head-settle delay; `m` = multiple-record flag; `i3..i0` = interrupt condition (i0=not-ready→ready, i1=ready→not-ready, i2=index pulse, i3=immediate interrupt/requires reset; i3..i0=0 → terminate with no INTRQ).

### Status register bit meanings

**Type I (Restore/Seek/Step/Step-In/Step-Out) and after Force Interrupt with no command running:**

| Bit | Name | Meaning |
|---|---|---|
| 7 | Not Ready | drive not ready (inverted READY) |
| 6 | Write Protect | disk write-protected |
| 5 | Head Loaded | head loaded and engaged |
| 4 | Seek Error | target track not verified within limit |
| 3 | CRC Error | ID-field CRC error during verify |
| 2 | Track 00 | head positioned at track 0 (`!TR00` active) |
| 1 | Index | index pulse currently under head |
| 0 | Busy | command in progress |

**Type II / III (Read/Write Sector, Read Address/Track, Write Track):**

| Bit | Name | Meaning |
|---|---|---|
| 7 | Not Ready | drive not ready |
| 6 | Write Protect (write cmds) / 0 | write protect on writes |
| 5 | Record Type / Write Fault | on read: DAM type (deleted vs normal); on write: write fault |
| 4 | Record Not Found | matching ID field not found within 5 revolutions |
| 3 | CRC Error | CRC error in ID or data field |
| 2 | Lost Data | CPU did not service DRQ in time |
| 1 | DRQ | data request pending (mirrors DRQ line) |
| 0 | Busy | command in progress |

### Command behavior notes (for accurate implementation)
- **Restore:** if `!TR00` already active, TR←0 and INTRQ immediately; else issue step pulses at rate `r1r0` until `!TR00`, up to 255 pulses, then set Seek Error if not reached (and V set).
- **Seek:** step from TR toward DR value; INTRQ at completion; with multiple drives, TR must be reloaded for the selected drive before seeking.
- **Read Sector:** find ID with matching track/sector (and side if C=1) and valid CRC; stream data field to DR generating a DRQ per byte; set Lost Data if a DRQ is missed; record DAM type in status bit 5; if `m=1`, auto-increment SR and continue until SR exceeds sectors on track or a Force Interrupt arrives.
- **Write Sector:** after ID match, count off 22 bytes (double density), assert Write Gate if DRQ serviced, write 12 bytes of 0x00, the DAM (per a0), the data, 2 CRC bytes, then one 0xFF; missed DRQ mid-write → Lost Data and a 0x00 byte substituted (command continues).
- **Read Address:** returns the 6 ID bytes (Track, Side, Sector, Length code, CRC1, CRC2) via DRQ; track address is copied into SR.
- **Write Track (format):** see §5 — the CPU streams the entire track image including gaps; bytes 0xF5–0xFE are interpreted specially (0xF5→A1+preset CRC, 0xF6→C2, 0xF7→write 2 CRC bytes, 0xF8–0xFE→address marks with missing clocks).
- **Force Interrupt (Type IV):** loadable any time; terminates a running command, resets Busy; i-flags select the interrupt condition; after a Force Interrupt with no running command, the status register reflects Type I semantics.

---

## 4. Timing

**Step-rate field (`r1 r0`), 1 MHz clock (3.5"/5.25" double density):**

| r1 r0 | Step rate |
|---|---|
| 0 0 | 6 ms |
| 0 1 | 12 ms |
| 1 0 | 20 ms |
| 1 1 | 30 ms |

MSX 3.5" drives typically use the fastest (6 ms) setting; the step pulse itself is a 4 µs (MFM) / 8 µs (FM) pulse. The direction (`DIRC`) line is valid 24 µs before the first step pulse. After the last directional step, an additional head-settling time occurs if the Type I verify flag `V=1`. The Western Digital WD2797 datasheet, as quoted on the MSX Resource Center "WD2793 HLD/HLT timing?" thread, states verbatim: *"after the last directional step, an additional 15 milliseconds of head settling time takes place if the verify flag is set in Type I commands note that this time doubles to 30 ms for a 1MHz clock."* A comparable settle delay applies if the `E` flag is set in a Type II/III command. Verification searches for a matching ID with valid CRC within 5 disk revolutions before declaring a Seek Error.

**Data rate / byte timing (MFM double density, 250 kbit/s, 300 rpm):**
- Cell rate 250 kbit/s → **1 raw byte ≈ 32 µs**. This is the hard real-time budget for servicing DRQ during Read/Write Sector: the CPU must move each byte within ~32 µs or Lost Data is set. At 3.58 MHz the Z80 has on the order of ~110–115 T-states between bytes, which is why the transfer loop is tight, runs with interrupts disabled, and is copied to RAM.
- One track ≈ **6250 bytes** unformatted (250 000 bits/s ÷ 5 rev/s ÷ 8); 80 tracks × 2 sides × 6250 ≈ 1 000 000 bytes ("1 MB unformatted", 720KB formatted). This derivation is given explicitly in the MSX Assembly Page "Low-level disk storage" article.
- **Index pulse** period ≈ **200 ms** (300 rpm). Read Track / Write Track are framed index-pulse to index-pulse.

**Head-load / motor timing.** In the WD2793 family, `HLD` (head-load) is asserted when the controller wants heads engaged, and external logic must return `HLT` (head-load-timing) when settled. On 3.5"/5.25" MSX drives there is no physical 8" head-load solenoid, so MSX designs commonly tie `HLD`→`HLT` (or use a fixed delay), meaning "head loaded" is effectively immediate/derived from drive-ready. On the HB-F1XV the *motor* is not controlled by the WD2793 spindle logic but by the interface's drive-select/motor latch plus a **delayed motor-off** timer. openMSX models this via `motor_off_timeout_ms = 4000` (motor spins down ~4 s after the last access); the openMSX 0.9.0 release notes describe this verbatim as *"added support for delayed motor off for disk drives, as in real machines implemented by the CXD1032 chip."* An accurate emulator should keep the motor "on" for 4 s after the last command and reflect drive-ready accordingly.

**Command execution / Busy duration.** Busy (status bit 0) is set on command receipt and cleared at completion with INTRQ. Practical durations: Type I ≈ seek distance × step rate + settle; Read/Write Sector ≈ rotational latency to the target ID (0–200 ms) + sector transfer time (512 bytes × 32 µs ≈ 16.4 ms) + gaps; Write Track ≈ one full revolution (~200 ms). For behavior-accuracy, model Busy as spanning the realistic time; many games and copy-protection checks poll Busy and time-sensitive status transitions.

---

## 5. Disk format specifics (3.5" 720KB 2DD)

**Geometry:** 80 cylinders (tracks 0–79) × 2 sides × 9 sectors/track × 512 bytes = **737,280 bytes (720KB)**, MFM double density, 300 rpm, 250 kbit/s. Media descriptor byte **0xF9**; sectors numbered 1–9 per track; first track = track 0. (Single-sided 360KB variants use media 0xF8 and 1 head.) A .DSK image of a 720KB disk is exactly 1440 × 512 = 737,280 bytes; emulators infer geometry from file size (720KB→double-sided, 360KB→single-sided).

**FAT12 logical layout** (standard MSX 720KB): boot sector (LBA 0) with BPB, 2 FAT copies of 3 sectors each, root directory 112 entries, 2 sectors/cluster.

**Track layout (IBM System 34 MFM), per the WD2793 Write-Track byte stream.** For a 512-byte-sector MSX track the structure is (values are the *logical* bytes fed to the FDC; the FDC converts 0xF5/0xF6/0xF7 into A1/C2/CRC as noted):

```
Track header (once per track, after index):
  Gap4a : 80 × 0x4E            (post-index gap)
  Sync  : 12 × 0x00
  IAM   :  3 × 0xC2 (via 0xF6) + 1 × 0xFC   (index address mark)
  Gap1  : 50 × 0x4E

Per sector (×9):
  Sync  : 12 × 0x00
  IDAM  :  3 × 0xA1 (via 0xF5) + 1 × 0xFE   (ID address mark)
  ID    :  Track, Side, Sector, Length(=0x02 for 512), + 0xF7 (writes 2 CRC bytes)
  Gap2  : 22 × 0x4E
  Sync  : 12 × 0x00
  DAM   :  3 × 0xA1 (via 0xF5) + 1 × 0xFB   (data address mark; 0xF8 = deleted)
  Data  : 512 data bytes + 0xF7 (writes 2 CRC bytes)
  Gap3  : (gap between sectors; ~84 bytes 0x4E for 9×512 MSX layout)

Gap4b : 0x4E repeated to the next index pulse (absorbs speed variation)
```

- **Sector ID (CHRN) field:** Cylinder (track #), Head (side 0/1), Record (sector 1–9), N (size code; 0x02 = 512 bytes), followed by 2 CRC-16-CCITT bytes over the ID.
- **GAP3** length is the formatting parameter that determines sectors/track; ~84 bytes for the standard 9-sector MSX 720KB track (matching common tooling that formats MSX 2DD as `secs=9 bps=512 gap3=84 rate=250`).
- **GAP4 (4a+4b)** is effectively one gap split by the index; per the grauw "Low-level disk storage" article it absorbs the fact that a track is not exactly 6250 bytes (rotation/data-rate tolerance), so the physical track length varies without corrupting the last sector.

**How the WD2793 constructs this (Type III Write Track).** The MSX CPU streams the *entire* track image above, byte-by-byte, DRQ-paced, from the first index pulse to the next. The FDC substitutes special encodings on the fly: writing 0xF5 emits the A1 sync mark and presets the CRC generator; 0xF6 emits C2; 0xF7 emits the two computed CRC bytes; 0x00–0xF4 and 0xF8–0xFF are written literally. Therefore the WD2793 does *not* auto-generate gaps or headers — the disk ROM's format routine holds the full template. (This differs fundamentally from the TC8566AF, whose Format command takes only sector-size/count/gap parameters and generates the track itself — a reason Panasonic images and Sony/Philips images behave differently at the flux level and why openMSX distinguishes the two cores.)

---

## 6. Interrupt / DRQ handling

**Signals.** The WD2793 exposes two handshake outputs: `INTRQ` (pulses/holds at command completion or on Force-Interrupt condition) and `DRQ` (asserted when a byte is ready to read, or needed to write). On the Sony interface these are **not** wired to the Z80 `/INT` line; instead they are read back by the CPU through the interface status register (INTRQ in bit 7, DRQ in bit 6 of the 7FFF read in the Sony layout). INTRQ is cleared by reading the status register or loading a new command; DRQ is cleared by reading/writing the Data Register.

**Polled, not interrupt-driven.** The standard MSX disk BIOS DSKIO loop is a **tight polling loop**: it reads the interface status, tests DRQ, and does an `INI`/`OUTI`-style transfer, repeating until the sector count is exhausted, then tests INTRQ/Busy for completion and reads the WD2793 status for errors. Interrupts are disabled (`DI`) around the transfer because a single missed 32 µs window sets Lost Data and fails the sector. (Some interfaces optionally route INTRQ to the Z80 NMI to break out of a hung loop, but the reference MSX disk driver relies on polling, and the HB-F1XV disk BIOS is polled.)

**Timing constraint an emulator must honor.** Because DRQ pacing is ~1 byte / 32 µs, a cycle-accurate emulator should assert DRQ on that cadence during Read/Write Sector, set Lost Data (status bit 2) if the guest fails to service in time, and clear DRQ on DR access. Copy-protection routines and fast loaders depend on realistic DRQ/INTRQ timing and on Busy remaining set for the physically-plausible duration; a "return the whole sector instantly" shortcut will pass MSX-DOS but fail protected titles.

---

## 7. HB-F1XV-specific integration

- **Number of drives: one.** The HB-F1XV has a single built-in 3.5" 720KB (2DD) drive; openMSX configures `<drives>1</drives>`. This contrasts with the two-drive Sanyo WAVY PHC-70FD2. (The disk BIOS still implements the standard "phantom drive" feature: a single physical drive is presented as two drive letters A:/B: with a "insert disk for drive B and press a key" prompt, unless a second physical drive exists.) The HB-F1XV's belt-driven mechanism is a known failure point — the MSX Wiki HB-F1XV page notes *"The floppy disk drive belt is the main failure cause… Diameter: 59mm (length: 186mm) Width: 2.8mm Thickness: 0.4mm"* — but that is a hardware-maintenance detail, not an emulation concern.
- **Controller is the MB89311, wired in the Sony connection style** (memory-mapped, page 1, registers at 7FF8–7FFF). Sony's glue-logic placement of side/motor/drive-select differs in detail from Panasonic (TC8566AF, I/O-region mapped with a bank register at 0x7FF0) and from Microsol (I/O ports 0xD0–0xD7). An emulator selects the WD2793 core + Sony decode.
- **Delayed motor-off (~4 s).** Modeled by `motor_off_timeout_ms=4000`. openMSX notes real machines implement this with dedicated glue (the CXD1032 chip in Sony/related designs handles delayed motor-off); emulate the timer so software that assumes the motor is still spinning between quick accesses behaves correctly.
- **Interaction with the S1985 mapper when loading MSX-DOS.** The disk ROM lives in expanded slot 3-2; RAM is elsewhere in slot 3. On boot the BIOS scans slots, finds the disk kernel, and MSX-DOS's DSKIO must transfer sectors into the 64KB RAM. Because the disk ROM is paged into page 1 while data often targets page 0/page 1 RAM, the driver copies its transfer stub into a RAM buffer and switches slots via the S1985 secondary-slot register (0xFFFF) and PPI port 0xA8. The FDC data register remains reachable at 0x7FFB regardless because it is memory-mapped in page 1's top bytes — so slot switching affects only where sector *data* lands, not FDC register access. Cross-reference the **S1985 fact-sheet** for the 0xA8/0xFFFF slot-select mechanics and the note that the S1985 leaves unused data-bus bits pulled high (relevant when probing for the disk interface).

---

## 8. Known hardware quirks, undocumented behavior, and emulation pitfalls

**WD2793-family quirks to model:**
- **Power-on/reset step rate.** After master reset the 17xx/27xx-family defaults to the *slowest* step timing while restoring to track 0 (the characteristic "rattle"); TR is set to 0xFF at reset and cleared to 0 only when `!TR00` is seen. Model TR=0xFF at reset.
- **Seek with no verify (V=0) and the `T` update flag.** For Step/Step-In/Step-Out, if the track-update flag is 0 the Track Register is *not* updated even though a step pulse is issued — used when accessing uninitialized disks. Getting this wrong desynchronizes TR from head position.
- **Multi-sector read/write boundary.** With `m=1`, SR auto-increments and the operation continues until SR exceeds the track's sector count or a Force Interrupt is issued; a CRC error in the data field terminates even a multi-record command. Emulators must stop at the right boundary and set the right status.
- **Record-Not-Found timing.** The FDC searches 5 revolutions before setting Record Not Found; the DAM must be found within 30 bytes (FM) / 43 bytes (MFM) of the ID CRC or the ID search restarts. Instant success/failure breaks timing-sensitive code.
- **Lost Data semantics differ read vs write.** On read, a missed DRQ sets Lost Data but the byte is simply lost; on write, a missed DRQ writes a 0x00 byte and continues (write not terminated), whereas a missed DRQ *before* the first data byte (or before index in Write Track) terminates the command. Model both paths.
- **Write-protect detection** is sampled from `WPRT`; write commands to a protected disk return status bit 6 set and error code 0. **Side select** is a glue-logic bit on the MSX side (the WD2793's own side-compare `C`/`S` flags additionally validate the ID-field side byte when enabled) — emulators must apply the interface side latch to choose the physical head *and* honor the FDC side-compare when C=1.

**Disk-change / DSKCHG quirk (most important behavioral subtlety).** The MSX has no reliable hardware "disk changed" line on all drives; DSKCHG frequently returns **"unknown" (0x00)**, and the DOS1 kernel then re-reads the boot sector / FAT ID to decide whether the medium changed and rebuilds the DPB. The "disk-changed" latch behavior is exactly the kind of bit that "almost all the emulator programs set and clear inappropriately" (a caution voiced in FDC documentation about the analogous PC controller's disk-change bit). For MSX behavior-accuracy: implement DSKCHG to report unchanged/unknown/changed consistently with whether the emulator's mounted image was swapped, and always update the DPB when returning unknown/changed, because DOS1 depends on it.

**"Seek to track 0" edge cases.** Restore issues up to 255 step pulses; if `!TR00` never asserts, Seek Error is set (only reported if V=1). Emulate the pulse cap and the TR00 sense so that a mispositioned/empty drive yields Seek Error rather than hanging.

**How reference emulators implement it:**
- **openMSX** models a genuine WD2793 core (`src/fdc/WD2793.cc`) shared by all Philips/Sony/National-style interfaces, decoded per connection style (`PhilipsFDC` handles both Philips and Sony). It supports **DMK** images for flux-level/copy-protection accuracy, models disk rotation, delayed motor-off (CXD1032), head-load, and per-command timing. Release notes document ongoing WD2793 accuracy work — the 0.9.0 notes state verbatim: *"much improved accuracy for Floppy Drive Controllers (mostly WD2793 and alike) - added support for delayed motor off for disk drives, as in real machines implemented by the CXD1032 chip - disk drive rotation is now correct"*; later notes add *"improved accuracy of WD2793 FDC and disk drive emulation"* and *"WD2793: fixed very rare corner case when writing CRC bytes."* This is the reference to match.
- **blueMSX** hardcodes machine/FDC details in its databases and emulates the WD2793 at the register/command level with standard-image handling; it is generally accurate for MSX-DOS-level access but historically less flux-accurate than openMSX for exotic copy protection.
- **MAME** implements the WD/FD179x family (`wd_fdc`) as a shared device with realistic Busy/DRQ/INTRQ timing and disk-image geometry; MSX drivers attach it with the appropriate connection glue.

**Common emulator mistakes to avoid:**
1. Treating the interface as I/O-port-mapped (Microsol/Panasonic style) instead of memory-mapped at 7FF8–7FFF for Sony.
2. Returning sector data instantly with Busy never realistically set — breaks copy protection and Busy-polling loops.
3. Ignoring DRQ pacing / Lost Data — passes DOS but fails fast loaders and protection.
4. Mishandling the disk-change bit and failing to update the DPB on unknown/changed — causes DOS1 to read stale directory/FAT.
5. Not modeling TR=0xFF-at-reset and the Restore step-cap → wrong Seek Error behavior.
6. Emulating format as "zero the image" instead of parsing the Write-Track byte stream — loses non-standard/copy-protected track layouts and mis-handles gap/CRC special bytes 0xF5–0xF7.
7. Applying WD2793 (Sony) format semantics to a Panasonic image or vice-versa — the Write-Track (WD) vs Format (TC8566AF) models are not interchangeable.

---

## Recommendations (staged implementation plan)

1. **Stage 1 — DOS-level correctness.** Implement a WD2793 core (5 registers, Type I/II/III/IV commands, status semantics) with the Sony memory-mapped decode at 7FF8–7FFB + control/status glue in 7FFC–7FFF. Back it with a 737,280-byte flat .DSK image, CHS↔LBA via the 720KB geometry (9/2/80), media 0xF9. Poll-loop DSKIO/DSKCHG/GETDPB should now boot MSX-DOS 1 and Disk BASIC. *Benchmark:* HB-F1XV boots its Creative Tool disks and MSX-DOS; FILES/COPY/FORMAT work.
2. **Stage 2 — Timing/behavior accuracy.** Add realistic Busy durations, DRQ pacing at ~32 µs/byte, Lost Data, INTRQ semantics, step-rate/settle timing, index-pulse (200 ms) framing, and the ~4 s delayed motor-off. *Benchmark:* timing-sensitive loaders and Busy-polling titles run; motor-off delay observable.
3. **Stage 3 — Flux/format fidelity.** Implement Write-Track byte-stream parsing (0xF5/0xF6/0xF7 handling, full gap/ID/DAM construction) and Read Address/Read Track; adopt a DMK-style image for non-standard tracks. *Benchmark:* copy-protected disks that openMSX runs also run in your emulator; formatting produces a correct 9-sector track image.
4. **Verify the Sony glue bits against source.** Before release, confirm the exact 7FFC/7FFD/7FFF bit assignments (side, motor, drive-select, INTRQ/DRQ) against `openMSX/src/fdc/PhilipsFDC.cc` and a real HB-F1XV disk-ROM disassembly; do not rely on the "typical" convention if you can read the source.

**Thresholds that change the plan:** if you only ever target MSX-DOS/Disk-BASIC software, Stages 1–2 suffice and you can skip DMK/flux work; if you target the copy-protected commercial catalog, Stage 3 is mandatory. If you later add a Panasonic/turboR machine, you must add a *separate* TC8566AF core (do not extend the WD2793 model).

---

## Caveats
- **Chip identity vs emulation model.** The physical part is the Fujitsu **MB89311**; it is programmed and timed as a **WD2793**. I found no public MB89311 datasheet text beyond catalog listings ("CMOS Floppy Disk Controller / Formatter"), so all register/command/timing specifics here are taken from the WD1793/WD2793/FD179x datasheets that the MB89311 is compatible with. If a genuine MB89311 datasheet surfaces, verify step-settle figures and any CMOS-specific status nuances against it.
- **Sony glue-register bit map is partially inferred.** The four core WD2793 registers at 7FF8–7FFB are firmly documented (Sony HB-720 interface doc + openMSX decode). The precise addresses/bits of the side-select, motor/drive-select latch, and INTRQ/DRQ status read in the 7FFC–7FFF window follow the standard Sony convention and the way the disk BIOS polls, but I could not quote them verbatim from `PhilipsFDC.cc`; treat the exact bit positions as "verify against source" (Recommendation 4).
- **Slot assignment.** The 3-2 placement and single-drive count come from the openMSX HB-F1XV machine file; a real-hardware service manual could refine the exact subslot number, but the emulation-relevant fact (disk ROM in expanded internal slot 3, single 2DD drive, page-1 memory-mapped FDC) is solid.
- **Timing figures** (15/30 ms settle, 6/12/20/30 ms step, 32 µs/byte, 200 ms index, 6250 bytes/track, ~4 s motor-off) are datasheet/derived nominal values; real settle time and the exact motor-off timeout can vary slightly, and the datasheet itself is ambiguous about whether settle is 15 or 30 ms at a 1 MHz clock.
- **Force Interrupt / status-after-FI** behavior is summarized from the WD1793 datasheet; corner cases (e.g., writing CRC bytes, status during the FI window) are exactly where emulators historically diverged, so validate against openMSX's test behavior.