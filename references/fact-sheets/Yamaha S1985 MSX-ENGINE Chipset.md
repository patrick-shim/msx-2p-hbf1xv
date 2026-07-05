# Yamaha S1985 "MSX-ENGINE" (MSX-SYSTEMII) — Emulation Fact-Sheet (Sony HB-F1XV)

*Companion to the V9958 VDP, YM2413/FM-PAC, and Z80A CPU fact-sheets in this series. Focus: behaviour/cycle-accurate reimplementation in C/C++.*

## 1. Chip identification & context

- **Part number:** Yamaha **S1985**. Yamaha's own product name is **MSX-SYSTEMII** (successor to the S3527 "MSX-SYSTEM"/"MPC"). In Europe these Yamaha chips are commonly (if technically incorrectly) called "MSX-ENGINE," a term that originally denoted Toshiba's T-series.
- **Manufacturer:** Yamaha (Nippon Gakki), designed for ASCII Corporation.
- **Package:** 100-pin flat plastic package (QFP), CMOS.
- **Role:** central glue-logic ASIC that integrates into one package what would otherwise be several discrete chips: the YM2149 PSG, an i8255-compatible PPI, an RP5C01A-compatible RTC with battery-backed CMOS, a 512 KB memory-mapper controller, a small backup RAM, DRAM/ROM control, printer/cassette interfaces, slot-select decoding, VDP read/write strobe generation, and the Z80 M1 wait-state generator. It deliberately does **not** contain the Z80 CPU, the VDP, or the RAM/ROM (all external).
- **What "MSX-ENGINE" means as a category:** a custom LSI that condenses the standard MSX support chips to cut board area, power and cost. Engines differ in what they integrate.

**Engine comparison:**

| Chip | Vendor | Gen | Integrates CPU? | Mapper | RTC | PSG output | Notes |
|---|---|---|---|---|---|---|---|
| T7775 | Toshiba | MSX1 | No | No | No | — | Early CMOS glue, external VDP/PSG |
| T7937(A) | Toshiba | MSX1 | Yes (Z80 clone) | No | No | 3-ch separate | Near-complete MSX1, 144-pin |
| S3527 | Yamaha | MSX1/MSX2 | No | **No** | **No** | **mono** | MSX-SYSTEM; RTC/mapper external |
| **S1985** | **Yamaha** | **MSX2/2+** | **No** | **512 KB** | **Yes (RP5C01A)** | **stereo (A=C, B=L, C=R)** | MSX-SYSTEMII, 100-pin |
| T9769(A/B/C) | Toshiba | MSX2/2+ | Yes (Z80) | Yes | — | 3-ch separate | Adds VDP-I/O waits (A/B: +2, C: +1) |
| S1990 | NEC | turbo R | No (bus ctrl) | — | — | — | Not an engine; glues R800+T9769C |

The S1985 sits between the S3527 (which it resembles but adds the mapper, RTC and backup RAM to) and Toshiba's CPU-integrating T9769. Note per MSX Wiki: "The S1985 is a chipset called MSX-SYSTEMII made by Yamaha for ASCII. It is similar to the S3527 with some optional features in extra." The PSG contrast is explicit (MSX Wiki, Category:PSG): "The Yamaha S3527 MSX-SYSTEM integrated PSG is down-mixed to a single mono channel. The Yamaha S1985 MSX-SYSTEMII mixes the channels into a left and right channel."

- **Clock inputs:** the shared MSX system clock is 3.579545 MHz (the NTSC colour-burst frequency; the master crystal is 21.477 MHz ≈ 6×). The Z80 runs at 3.579545 MHz, the PSG core runs at half that (1.7897725 MHz), and the RTC uses its own 32.768 kHz crystal on XIN/XOUT (pins 89/88). The S1985 takes the CPU clock on **φIN (pin 30)**; community testing indicates the S1985 uses this clock chiefly for DRAM timing and the PSG, and to time the /WAIT, /CSR and /CSW strobes.
- **Cross-reference (Z80A fact-sheet):** the S1985 is the source of the MSX "M1 wait" (see §2, §8).

## 2. YM2149 PSG subsystem

**I/O ports:**

| Port | Direction | Function |
|---|---|---|
| `#A0` | write | Register-address latch (select register 0–15) |
| `#A1` | write | Data write to selected register |
| `#A2` | read | Data read from selected register |

**Register map (16 registers, 0–15):**

| Reg | Function |
|---|---|
| R0/R1 | Channel A tone period (12-bit: R0 fine, R1 coarse 4-bit) |
| R2/R3 | Channel B tone period (12-bit) |
| R4/R5 | Channel C tone period (12-bit) |
| R6 | Noise period (5-bit) |
| R7 | Mixer / I/O enable (bits 0–2 tone A/B/C enable [0=on], 3–5 noise enable, 6 = port A dir, 7 = port B dir) |
| R8/R9/R10 | Amplitude A/B/C (bits 0–3 level, bit 4 = use envelope) |
| R11/R12 | Envelope period (16-bit) |
| R13 | Envelope shape (4-bit) |
| R14 | I/O port A (read on MSX) |
| R15 | I/O port B (write on MSX) |

**Critical R7 note:** on MSX, port A is input and port B is output, so **bit 6 must be 0 and bit 7 must be 1**. Writing wrong values can damage joystick-port drivers on some machines; emulators should mask/preserve these bits.

**YM2149 vs AY-3-8910 differences (all relevant to emulation):**
- **Envelope resolution:** the YM2149 uses an internal **5-bit (32-step)** envelope counter vs the AY's **4-bit (16-step)**. Per maidavale.org ("AY vs YM sound IC differences"): "On the YM2149F it has 5 bits, resulting in twice the steps, and counts up twice as fast … even though the register for setting its direct value remains 4 bits wide." Internally the envelope clock is divided by 8 on the YM2149 vs 16 on the AY.
- **Register readback:** per maidavale.org, "On the AY, unused bits in registers always read back as 0 even if you had earlier written 1 to them. On the YM2149 … the register values can be read back as written."
- **DC offset / DAC curve:** per maidavale.org, "The YM2149 has a 2V DC offset on all outputs, whereas the AY-3-8910 has a 0.2V offset … The output from the AY is louder"; Sergei Bulba notes "YM is closer to logarithmic, than AY."
- **/SEL pin:** tying /SEL low halves the clock divider.

**Stereo on the S1985:** unlike the S3527 (mono downmix) and the discrete/Toshiba PSGs (three separate outputs), the S1985 mixes to **left (SSGSNDL, pin 99) and right (SSGSNDR, pin 100)** with the panning **A = Center, B = Left, C = Right** (MSX Wiki: "with stereo output (A: Center, B: Left, C: Right)"). Emulators should route channel B to left, C to right, and A to both.

**Joystick ports:** two general-purpose ports are wired directly to the S1985 (pins 57–70: FWD/BACK/LEFT/RIGHT/TRGA/TRGB per port + STB strobe), with 22 kΩ pull-ups on inputs and open-collector triggers. They are read through **PSG port A (R14, read-only)** and controlled through **PSG port B (R15)**:

*PSG R14 (read):* bit0 up, bit1 down, bit2 left, bit3 right, bit4 trigger A, bit5 trigger B (0=pressed), bit6 keyboard layout (1=JIS/0=ANSI), bit7 cassette input.
*PSG R15 (write):* bits0–3 pin-6/7 output enables for ports 1&2, bits4–5 pin-8 (STB) outputs, **bit6 selects which joystick port feeds R14 (1=port 2)**, bit7 KANA LED (1=off).

**Timing:** tone frequency = fc/(16·TP) with fc = 1.7897725 MHz; lowest note ~30.6 Hz. Cycle-accurate PSG players depend on the PSG advancing relative to the CPU; because of the M1 wait (§8), `OUT (n),A` to the PSG effectively takes **12 T-states** on MSX (MSX Resource Center, citing Grauw's z80instr: "MSX Z80 use more wait state … OUT (n),A is 12cc"), and correct replay timing requires modelling the M1 wait.

## 3. PPI (i8255-compatible) subsystem

**I/O ports:**

| Port | Direction | Function |
|---|---|---|
| `#A8` | read/write | PPI port A — primary slot select |
| `#A9` | read | PPI port B — keyboard matrix row input (inverted) |
| `#AA` | read/write | PPI port C — keyboard/cassette/LED/click |
| `#AB` | write | PPI control register (mode / bit-set-reset) |

**Port A (`#A8`) — primary slot select** (2 bits per 16 KB page):
```
bits 0-1: primary slot for page 0 (#0000-#3FFF)
bits 2-3: primary slot for page 1 (#4000-#7FFF)
bits 4-5: primary slot for page 2 (#8000-#BFFF)
bits 6-7: primary slot for page 3 (#C000-#FFFF)
```
Readable and writable. Secondary (sub-)slot selection is via memory-mapped `#FFFF` (see Z80/slot fact-sheet), decoded by the S1985 via the **RSEL pin (28)**, which detects accesses to the top 256 bytes of any page (the S1985 has A15, A14 and A7–A0 inputs but not A13–A8, so an external NAND signals when A13–A8 are all 1, i.e. the `#xxFF` region including `#FFFF`).

**Port B (`#A9`) — keyboard row read:** returns the 8 column bits of the currently selected matrix row; **inverted** (0 = pressed). The MSX keyboard is an 11×8 matrix.

**Port C (`#AA`):**
```
bits 0-3: keyboard matrix row select (0-10)
bit 4  : cassette motor control (1 = off)   -> REM pin
bit 5  : cassette write data (1 = high)      -> CMO pin
bit 6  : CAPS LED (1 = off)                   -> CAPS pin (open-collector)
bit 7  : 1-bit key-click sound output         -> PPISND pin
```

**Control register (`#AB`):** i8255 bit-set/reset mode — bit0 = value, bits1–3 = which port-C bit, bit7 = 0. This is the recommended fast path to toggle port C bits 4–7 (motor, cassette, CAPS, click).

**Mirroring `#AC-#AF`:** the S1985 mirrors all four PPI ports (`#A8-#AB`) to `#AC-#AF`; this mirror also exists on the S3527. Detection routine relying on it: §7 and §10.

## 4. Memory Mapper subsystem

- The S1985 implements the standard MSX **external memory mapper** on write-only ports `#FC/#FD/#FE/#FF` (one 8-bit segment register per page 0/1/2/3). Each segment is 16 KB; the standard allows up to 256 segments (4 MB), but the S1985 drives address lines **MA14–MA18** (pins 4–7 and 3) for **up to 512 KB (32 segments)**.
- **Shared-pin limitation:** pin 3 is **MA18/KBDIR** — shared between the top mapper address line and the keyboard-bus direction signal. Whether the maximum is 256 KB or 512 KB therefore depends on the keyboard wiring of the particular machine (MSX Wiki, Memory mapper: "Due to a shared pin, it depends on the type of keyboard whether the maximum supported size is 256 or 512 kB").
- **Integration with slots:** the mapper lives inside a (sub)slot; the Z80 first selects the slot via PPI port A / `#FFFF`, then the `#FC-#FF` registers pick the 16 KB segment for each page. Slot select and segment select are independent mechanisms.
- **Readback:** reading `#FC-#FF` is explicitly discouraged by the MSX standard. On the S1985 the readback is **not** a true register — it returns `0x80 | (segment & 0x1F)` (bit 7 forced 1, bits 5–6 forced 0, bits 0–4 = segment; measured as the pattern `100xxxxx`). This matches openMSX's `MapperReadBackBaseValue = 0x80` with mask `0x1F`. (The MAP.COM utility patches DOS2 code — converting `ld a,(#F2Cx)` into `in a,(#FCh…#FFh)` — to read these ports for legacy-game compatibility.) Emulators should reproduce the `100xxxxx` pattern.
- **Sony HB-F1XV configuration:** ships with **64 KB main RAM** (a single 64 KB mapper) plus **128 KB VRAM**. openMSX places the main RAM as a 64 KB MemoryMapper in slot **3-0**. The board was not designed for internal RAM expansion; expansion requires replacing the DRAM chips with a small PCB. A second Sony custom chip (**MB670836**) handles DRAM address multiplexing and the speed/pause circuitry alongside the S1985.

## 5. RTC (RP5C01A-compatible) subsystem

**I/O ports:**

| Port | Function |
|---|---|
| `#B4` | Register/address select (register 0–15; bits 4–7 unused) |
| `#B5` | Data read/write (4-bit; bits 4–7 unused) |

- The RTC is gated by system-control port `#F5` **bit 7 (CLOCK-IC enable)**.
- **Structure:** four register **blocks (banks)**, each with 13 4-bit registers (0–12), plus three shared control registers (13–15). Mode **register 13** selects the active block (bits 0–1), alarm-output enable (bit 2), timer enable (bit 3). Register 14 = test; register 15 = 1 Hz/16 Hz/timer-reset/alarm-reset. **Registers 14–15 are not readable.**
  - **Block 0:** current time/date in **BCD** — seconds units/tens, minutes, hours, weekday, day, month, year.
  - **Block 1:** alarm/timer + 12/24-hour mode + leap-year counter. Bits marked 0 are physically unwritable.
  - **Block 2:** system init parameters — screen mode/interlace, WIDTH, text/background/border colours, key-click, printer type, transfer speed, BEEP timbre/volume, startup-logo colours, and region/area code. **Register 0 must read `0Ah`** to signal valid CMOS; otherwise BIOS defaults load and Block 3 is cleared.
  - **Block 3:** title/password/prompt characters (ASCII) plus key-cartridge fields.
- **The "26 × 4 bits RAM":** the two general-purpose blocks (Block 2 and Block 3), 13 registers each = **26 registers × 4 bits** of battery-backed CMOS usable by system/software. Unused free bits total 22 (Block 1), 4 (Block 2), 48 (Block 3).
- **Battery backup:** BVSS (pin 86) is the clock back-up supply; the 32.768 kHz crystal is on XIN/XOUT (89/88); ALM alarm output is pin 87 (never used on any MSX). BCD: each nibble is a decimal digit; time updates continuously while battery-powered.
- **BIOS access:** Sub-ROM routines REDCLK (`#01F5`) and WRTCLK (`#01F9`); entry C = block<<4 | register, data in low nibble of A.

## 6. Backup RAM subsystem (16 × 8-bit)

- The S1985 contains a small **16-byte battery-backed RAM** distinct from the RTC's CMOS. openMSX models it as `SRAM "S1985 Backup RAM"` of size `0x10` (16 bytes).
- **Access mechanism (verbatim from openMSX `MSXS1985.cc`):** the device is exposed via the MSX **expanded/switched I/O** mechanism (ports `#40-#4F`) under **device ID `0xFE` (254)** — `static constexpr byte ID = 0xFE;`. Internally it maintains:
  - an **address register**: on write, `address = value & 0x0F` (a 4-bit index into the 16 bytes);
  - a **data register**: read returns `(*sram)[address]`, write does `sram->write(address, value)`;
  - a rotating **`pattern`** register: `pattern = byte((pattern << 1) | (pattern >> 7))`;
  - two **colour registers** `color1`, `color2`, where a read returns `(pattern & 0x80) ? color2 : color1`.
  - `reset()` clears `color1 = color2 = pattern = address = 0`.
  These are accessed via `readSwitchedIO`/`writeSwitchedIO`/`peekSwitchedIO`, dispatched by `switch (port & 0x0F)`.
- **Purpose:** persistent low-level configuration storage. (On MSX2/2+ the *primary* boot-configuration store — screen mode, key-click, colours, region — is the RTC Block 2 CMOS of §5; the 16-byte S1985 backup RAM is an auxiliary store, saved by openMSX as a `.sram` persistency file.)
- **Battery relationship:** battery-backed like the RTC CMOS; the MSX Wiki describes the RTC's 26×4 CMOS as backed by a *separate* battery.

## 7. VDP port mirroring (`#98-#9B` → `#9C-#9F`)

- The S1985 (and S3527) **mirror the VDP ports `#98-#9B` onto `#9C-#9F`**. On the HB-F1XV the VDP is the V9958 (see V9958 fact-sheet): `#98` VRAM data, `#99` status/register, `#9A` palette, `#9B` indirect register.
- **Why:** incomplete address decoding — the S1985 decodes the VDP region but does not distinguish the `#9C-#9F` alias, so those addresses fall through to the same VDP strobes (VDPCR pin 9 / VDPCW pin 8).
- **Software reliance:** essentially none; well-behaved software reads the VDP base port from BIOS `#0006/#0007` and uses `#98-#9B`. The mirror is mainly an artifact/detection aid.
- **Emulator implementation:** on machines with an S1985/S3527, register the VDP on `#9C-#9F` as well as `#98-#9B`. openMSX does this per-machine in the machine XML rather than hard-coding it in the VDP core. It is safe to model as a straight alias.

## 8. DRAM/ROM control & other glue logic

**DRAM control:** RAS (pin 39), CAS (42), WE (43) and MPX (41, row/column multiplex) drive the DRAM; refresh is timed off the RFSH input (pin 33) from the Z80. The S1985 multiplexes row/column addresses and generates refresh cycles.

**ROM/chip-select decoding:** the S1985 produces slot and ROM chip-selects:
- CS0/SLT02 (49) = `#0000-#3FFF`, CS1 (44) = `#4000-#7FFF`, CS2 (45) = `#8000-#BFFF`, CS12 (46) = `#4000-#BFFF`, CS01/SLT03 (50) = `#0000-#7FFF`.
- Slot selects SLT0..SLT3 and sub-slot selects SLT00..SLT33 (pins 47–56), driven from PPI port A and the `#FFFF` sub-slot register.

**M1 wait & VDP strobes (cross-ref Z80A fact-sheet):**
- Z80 handshake pins: M1 (32), RFSH (33), MREQ (34), IORQ (35), RD (36), WR (37), and **WAIT (38, open-collector output with pull-up)**.
- The S1985 **inserts exactly one wait state into every M1 (opcode-fetch) cycle**. Consequently (per MSX Assembly Page / Grauw): "XOR A has opcode AF and is documented as taking 4 T-states. On MSX, it takes 5 (one more)," and "BIT 0,A has opcode CB 47 and is documented as taking 12 T-states. On MSX, it takes 14 (two more)." Each ED/CB/DD/FD prefix adds another M1 wait. All published Z80 timings for MSX must add these M1 waits. Emulators add +1 T per M1 fetch (+1 per prefix byte).
- **VDP strobes:** VDPCR (9)/VDPCW (8) are generated for VDP I/O; the falling edge of /VDPCW is synced to the CPU clock. Known hardware quirk: the Yamaha engines (S3527 and S1985) can glitch /CSR & /CSW if the VDP VRAM data port is accessed *too fast* — relevant for accuracy at higher clocks.
- **Optional VDP-I/O wait:** if the **X7** keyboard-return input (pin 80) is held low at reset, the S1985 adds one extra wait state to VDP reads/writes. Standard 3.58 MHz machines don't need it; it matters on turbo mods.

**Printer (Centronics) interface:**
- Pins PDIR (91), PRD (92), PWR (93), PSTB strobe (94), PBUSY (95).
- I/O ports: **`#90`** — bit 0 = STROBE output (write), bit 1 = BUSY status (read); **`#91`** — 8-bit data (write). A 14-pin unidirectional Centronics port; only BUSY (bit 1 of `#90`) is readable. BIOS LPTOUT pulses STROBE low after writing data. (A few later Panasonic models add a bidirectional mode on `#93`; not present on the HB-F1XV.) Note: on Yamaha engines the printer ports also appear on their mirrors (`#92-#97`), returning the written I/O address with only BUSY variable.

**Cassette:** CMI read (96), CMO write (97), REM motor (98); controlled via PPI port C bits 4–5 and read via PSG R14 bit 7.

## 9. HB-F1XV-specific integration

- The **Sony HB-F1XV** (released 1989-10-21) is, per MSX Wiki, "the final MSX manufactured and marketed by Sony," Japan-only, MSX2+. CPU: Zilog Z80A **Z0840004SPC**; engine: Yamaha **S1985**; VDP: V9958; FDC: **MB89311**; MSX-Music (YM2413/OPLL) built in; MSX-JE with 16 KB SRAM; 256 KB JIS1+JIS2 Kanji ROM; 64 KB RAM; 128 KB VRAM; hardware PAUSE, speed-controller, Ren-Sha Turbo autofire.
- **Sony-specific companion chip:** a second Sony custom LSI (**MB670836**) sits beside the S1985 and handles DRAM address multiplexing plus the speed-controller (slow-motion) and hardware-PAUSE circuitry; the earlier HB-F1II used a similar chip. The hardware PAUSE physically halts the CPU and cannot be bypassed in software.
- **Sony F4/system-flag quirk:** on Sony MSX2+ machines the `#F4` boot/system-flag register is **not inverted** (unlike Panasonic/Sanyo MSX2+, whose `#F4` is inverted, initial `0FFh`). Emulators must set the correct polarity per machine; BIOS RDRES/WRRES abstracts it.
- **Slot layout:** in the openMSX HB-F1XV machine definition, main RAM is a 64 KB mapper in slot 3-0, MSX2 Sub-ROM in 3-1, Kanji driver/BASIC also in slot 3, with two external cartridge slots (top + rear).
- Sony wired the S1985 to a standard Yamaha reference layout; the notable deviations are the MB670836 companion, the non-inverted F4, and Sony's speed/pause hardware.

## 10. Known quirks, undocumented behaviour & emulation pitfalls

- **MSX-ENGINE detection routine:** because the S1985/S3527 mirror PPI port A (`#A8`) onto `#AC`, software detects an engine by comparing the two. Read `#A8` and `#AC` (from assembly, interrupts disabled); if `INP(#AC) == INP(#A8)` an MSX-ENGINE is present. To distinguish **S1985 from S3527**, probe the expanded/switched I/O (the S1985's backup RAM responds to device ID `0xFE` on `#40`), which the S3527 lacks.
- **Port `#B4` read instability:** reading the RTC address port (and unconnected engine ports) returns machine-state-dependent / floating-bus values. On a Sony HB-F1XDmk2, `?HEX$(INP(&HB4))` returned `8B` one way and `2`/`8B` another, depending on how BASIC evaluated the expression — the value reflects the last byte on the bus. openMSX developers concluded this is not worth precisely reproducing, but be aware reads are unreliable. (The RTC also mirrors on `#B6-#B7`.)
- **Mapper readback `100xxxxx`:** as in §4 — bit7=1, bits5–6=0, bits0–4=segment. Don't model these as full readable registers. Related: on the T9769 engine the unused mapper data bits are driven low-ohm to +5 V, which can overheat when a larger external mapper pulls them down; on the S1985 they are safe (weak).
- **PPI keyboard-scan pitfalls:** the matrix is **inverted** (0=pressed); many keyboards lack a numeric keypad; and **key ghosting** occurs with 3+ keys sharing a row/column (e.g. SHIFT+S+X ghosts F1; C+D+SPACE ghosts HOME). Model the matrix as pure row/column with ghosting only if you need hardware-accurate keyboard behaviour; most emulators inject the NEWKEY buffer directly.
- **RTC bank switching:** the block-select is in mode register 13 (bits 0–1); forgetting to restore it, or reading registers 14/15 (unreadable), are common bugs. openMSX had leap-year/days-in-month RTC bugs that were later fixed.
- **VDP-access-too-fast glitches** and the **M1 wait** (§8) are the two timing behaviours most often gotten wrong; both originate in the S1985.
- **How reference emulators model it:** openMSX, blueMSX and MAME do **not** emulate the S1985 as one monolithic chip. They instantiate the constituent functions as separate logical devices — `MSXPPI` (i8255), an AY8910/PSG class, an RP5C01 RTC device, `MSXMapperIO` for the shared mapper registers — while the dedicated **`MSXS1985`** class (registered via `if (type == "S1985") result = make_unique<MSXS1985>(conf);` in `DeviceFactory`) handles only residual engine-specific behaviour: the 16-byte backup RAM (switched-I/O ID `0xFE`), the mapper-readback base value, and the pattern/colour registers. VDP and PPI port mirrors are added per-machine in XML. openMSX even *removed* a formerly-emulated S1985 from at least one machine (Victor HC-95A) when found not to be present — underscoring that the engine is a per-machine composition, not a fixed block. **Practical takeaway for a from-scratch C/C++ emulator:** implement the PPI, PSG, RTC and mapper as independent modules with clean interfaces, then add a thin "S1985 layer" that (a) mirrors `#98-#9B`→`#9C-#9F` and `#A8-#AB`→`#AC-#AF`, (b) forces the `100xxxxx` mapper readback, (c) exposes the 16-byte backup RAM on switched-I/O ID `0xFE`, and (d) asserts the +1 M1 wait and the VDP `/CSR`/`/CSW` strobe timing.

### Key pin reference (100-pin QFP), selected

| Pin | Name | Function |
|---|---|---|
| 2 | PPISND | PPI key-click output |
| 3 | MA18/KBDIR | mapper A18 / keyboard direction (shared) |
| 4–7 | MA17–MA14 | mapper address |
| 8/9 | VDPCW/VDPCR | VDP write/read strobe |
| 10–19 | AB15,AB14,AB7–AB0 | CPU address inputs |
| 20–27 | DB7–DB0 | CPU data bus |
| 28 | RSEL | `#FFFF` sub-slot decode enable |
| 29 | KANJI | Kanji-ROM select |
| 30 | φIN | CPU clock in |
| 32–37 | M1,RFSH,MREQ,IORQ,RD,WR | Z80 bus inputs |
| 38 | WAIT | wait output (open-collector) |
| 39/41/42/43 | RAS/MPX/CAS/WE | DRAM control |
| 44–46/49/50 | CS1/CS2/CS12/CS0/CS01 | ROM chip-selects |
| 47–56 | SLT* | slot / sub-slot selects |
| 57–70 | FWD/BACK/LEFT/RIGHT/TRGA/TRGB/STB ×2 | joystick ports 1 & 2 |
| 71/72 | CAPS/KANA | LED outputs (open-collector) |
| 73–80 | X0–X7 | keyboard return (X7 = VDP-wait config at reset) |
| 81–84 | YA–YD | keyboard drive |
| 85 | RESET | reset input |
| 86/88/89/87 | BVSS/XOUT/XIN/ALM | RTC battery, crystal, alarm |
| 91–95 | PDIR/PRD/PWR/PSTB/PBUSY | printer |
| 96–98 | CMI/CMO/REM | cassette |
| 99/100 | SSGSNDL/SSGSNDR | PSG stereo out (L/R) |

Pin-type legend: ▼ = 22 kΩ pull-up input; △ = open-collector (pull-down) output.

---

### Primary sources
Yamaha S1985 datasheet / "S1985 MSX-SYSTEMII Application Manual" (scanned, HansO, via map.grauw.nl); MSX Wiki (Yamaha S1985, Yamaha S3527, Ricoh RP-5C01, Real Time Clock Programming, MSX-SYSTEM, Memory mapper, Printer port/programming, Sony HB-F1XV, Category:PSG); MSX Assembly Page (Grauw) — MSX I/O ports overview, Keyboard matrices, Z80 instruction set; Ricoh RP/RF5C01A datasheet; AY-3-8910/YM2149 references (Wikipedia; maidavale.org; aym-js/Olivier Poncet; nesdev.org); openMSX source (`MSXS1985.cc`/`MSXS1985.hh` via doxygen, `DeviceFactory`, release history) and GitHub issue #1355 (engine-detection mirror); MSX Resource Center forum threads (S1985 /RSEL, VDP rd/wr timing & M1 wait, mapper I/O readback, HB-F1XV RAM); MSX2 Technical Handbook / MSX Datapack (I/O map, PSG, printer); Sony HB-F1XV service documentation and openMSX HB-F1XV machine definition. Where community measurements conflict (e.g. `#B4` read values), this is flagged in the text rather than resolved to a single value.