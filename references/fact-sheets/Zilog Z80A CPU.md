# Fact-Sheet: The Z80A CPU in the Sony HB-F1XV (MSX2+) — An Emulator Developer's Reference

*Third in a series for a from-scratch cycle-accurate MSX2+ emulator in C/C++ (following the Yamaha V9958 VDP and FM-PAC/YM2413 OPLL fact-sheets).*

---

## 1. Chip Identification & Context

The Sony HB-F1XV (released **1989-10-21**, the final MSX manufactured and marketed by Sony, Japan-only) uses a **genuine Zilog Z80A**, part number **Z0840004SPC**, a discrete 40-pin DIP soldered into a socket on the mainboard. The MSX Wiki hardware page states verbatim: *"The CPU is a Z80A from Zilog (Z0840004SPC) and the MSX-Engine is the S1985. The floppy disk controller chip is the MB89311."*

Key point for accuracy: the HB-F1XV is one of the very few MSX2+ machines that uses a **real, discrete Zilog Z80** rather than a Z80 core baked into a VLSI MSX-Engine ASIC. Most other MSX2+ machines (Panasonic FS-A1WX/WSX, Sanyo) integrated a Toshiba Z80 clone inside the T9769 MSX-Engine. This matters for two reasons: (a) the HB-F1XV exhibits authentic Zilog NMOS undocumented behavior (SCF/CCF flags, OUT (C),0, LD A,I bug, etc.), and (b) second-source clone quirks do NOT apply.

**Zilog part-number decoding (`Z0840004SPC`):**
- `Z` — Zilog
- `84` — Z80 CPU family
- `00` — CPU (as opposed to `43` CTC, `44` SIO, etc.)
- `04` — 4 MHz max rated clock (i.e. the "Z80A" grade)
- `S` — package (plastic DIP-40)
- `PC` — temperature/packaging suffix

The "Z80A" informal designation historically means the 4 MHz-rated NMOS grade (Z80 = 2.5 MHz, Z80A = 4 MHz, Z80B = 6 MHz, Z80H = 8 MHz).

**Process/package/transistor count:** NMOS, single +5V supply, single-phase TTL clock, 40-pin DIP (0.6" wide). Per Federico Faggin and Masatoshi Shima's Zilog oral history (Computer History Museum), the die contained roughly **8,200 transistors** (against the 8080's 4,800) — *"a total transistor of Z80 became 8,200 while a total of transistor of 8080 was 4,800"* — a large share of which are depletion-load NMOS pull-ups. The Z80 internally uses a **4-bit ALU executed twice** per 8-bit operation — a detail that explains much of its instruction timing.

**Clock speed & divider relationships:** The Z80A in the HB-F1XV runs at **3.579545 MHz** — exactly the NTSC color subcarrier frequency (315/88 MHz). On MSX2/MSX2+ the master crystal is **21.477270 MHz**, and the Z80 clock is derived by **dividing by 6** (21.477270 / 6 = 3.579545). The VDP (V9958) uses the same 21.48 MHz crystal, so on the HB-F1XV **the CPU and VDP run perfectly in sync clock-wise** (as an MSX Resource Center reference confirms: *"in MSX2 and MSX2+ models, this 21 MHz is divided by 6 to get the 3.57 MHz clock which feeds the Z80 CPU. So on MSX2 and MSX2+, the CPU and the VDP run perfectly in sync as far as the clock is concerned."*). This gives a fixed **1:6 relationship** between CPU T-states and VDP dot-clock cycles — an important simplification for the emulator. The PSG (YM2149 inside the S1985) is likewise derived from the same clock chain. (Contrast: on MSX turbo R the Z80/R800 are fed from a separate 28.63636 MHz crystal while the VDP keeps the 21.48 MHz crystal, so they run slightly out of sync.)

**MSX-Engine:** The HB-F1XV uses the **Yamaha S1985** ("MSX-SYSTEMII") MSX-Engine, which integrates the PPI (i8255-compatible slot/keyboard/cassette logic), the YM2149 PSG, the RP5C01-compatible RTC, DRAM/ROM control, printer interface, and — critically for timing — the **M1-wait generator**. The custom **Sony MB64H444** handles memory-mapper address expansion. The floppy controller is a Fujitsu **MB89311**.

**vs. R800 (turbo R):** The HB-F1XV is a *standard* MSX2+ — there is no R800. The R800 (used in Panasonic FS-A1ST/GT turbo R machines) is a separate, faster (7.16 MHz), 16-bit-ALU, pipelined Z80-compatible CPU. It is only relevant here as future-proofing context (see §9).

---

## 2. Z80 Architecture Overview (Emulation-Relevant)

### Register set

```
Main set              Alternate set        Special / control
------------------    -----------------    -----------------------------
A   F   (AF)          A'  F'   (AF')       I   (interrupt vector, 8-bit)
B   C   (BC)          B'  C'   (BC')       R   (memory refresh, 8-bit)
D   E   (DE)          D'  E'   (DE')       IX  (index, 16-bit)
H   L   (HL)          H'  L'   (HL')       IY  (index, 16-bit)
                                           SP  (stack pointer, 16-bit)
                                           PC  (program counter, 16-bit)
                                           WZ / MEMPTR (internal, 16-bit, not directly accessible)
                                           IFF1, IFF2 (interrupt enable flip-flops)
                                           IM (interrupt mode latch: 0/1/2)
```

The alternate set is swapped in via `EX AF,AF'` (AF only) and `EXX` (BC/DE/HL). These exchanges are just pointer/bank swaps in an emulator — no flags affected by the swap itself. A common efficient C representation is a `union` giving both 16-bit pair access and 8-bit register access, with an endianness-aware layout, plus a boolean/index selecting the active bank.

### Flag register (F) bit layout

```
 bit:  7   6   5   4   3   2   1   0
       S   Z   Y   H   X  P/V  N   C
       │   │   │   │   │   │    │   └─ Carry
       │   │   │   │   │   │    └───── Add/Subtract (N): 1 after subtractions (for DAA)
       │   │   │   │   │   └────────── Parity/Overflow
       │   │   │   │   └────────────── X (undocumented) = copy of bit 3 of result
       │   │   │   └────────────────── Half-carry (bit 3→4 carry; for DAA)
       │   │   └────────────────────── Y (undocumented) = copy of bit 5 of result
       │   └────────────────────────── Zero
       └────────────────────────────── Sign (copy of bit 7 of result)
```

### The undocumented X (bit 3) and Y (bit 5) flags

These are physically real bits of F that must be emulated to pass ZEXALL and to run some copy-protected software. For most instructions they are simply **copies of bits 3 and 5 of the 8-bit result** (for 16-bit ALU ops, bits 3 and 5 of the *high byte*, i.e. bits 11 and 13 of the result). The efficient standard technique is a precomputed **256-byte table** (`sz53p_table`) giving S, Z, Y, X, and parity for any byte value; ALU routines index it and OR in the operation-specific H, N, C.

Three instruction groups deviate and require special handling (see §4): `BIT n,(HL)` and `BIT n,(IX/IY+d)` (X/Y come from WZ/MEMPTR), the block instructions (`LDI`/`CPI`/`INI`/`OUTI` families), and `SCF`/`CCF`.

---

## 3. Instruction Set

### Opcode map structure (six decoding planes)

| Prefix | Plane | Notes |
|--------|-------|-------|
| (none) | Base / "root" | 252 of 256 codes are single-byte root ops (mostly 8080-inherited); `CB`,`ED`,`DD`,`FD` are prefixes |
| `CB xx` | Rotates/shifts + `BIT`/`RES`/`SET` | fully populated (incl. undocumented `SLL`) |
| `ED xx` | Extended: block ops, 16-bit ADC/SBC, `IN/OUT (C)`, `LD I/R,A`, `IM n`, `RETI/N`, `RRD/RLD`, `NEG` | many holes = 2-byte NOPs (NONI) |
| `DD xx` | HL→IX substitution | prefix on any base op |
| `FD xx` | HL→IY substitution | prefix on any base op |
| `DD CB d xx` / `FD CB d xx` | Indexed bit/rotate ops | displacement precedes sub-opcode |

**Instruction categories:** 8-bit load; 16-bit load; exchange, block transfer & search (`EX`, `EXX`, `LDI/LDIR/LDD/LDDR`, `CPI/CPIR/CPD/CPDR`); 8-bit arithmetic & logic; general-purpose arithmetic & CPU control (`DAA`, `CPL`, `NEG`, `CCF`, `SCF`, `NOP`, `HALT`, `DI`, `EI`, `IM`); 16-bit arithmetic; rotate & shift; bit set/reset/test; jump/call/return; input/output.

### Timing (T-states) — the critical MSX M1-wait rule

On the bare Z80, the shortest instruction (`NOP`, `XOR A`, etc.) is 4 T-states; the longest single instructions are the indexed bit ops at 23. **But on the MSX this is not the whole story:** the MSX standard requires **one extra wait state added to every M1 (opcode-fetch) cycle**. As the MSX Assembly Page (grauw) states: *"SRL (IX+0) has opcode DD CB 00 3E and is documented as taking 23 T-states. On MSX, it takes 25 (two more). The instruction timing including M1 wait is listed in the Z80+M1 column."* Instructions have **+1 M1 wait**, and instructions beginning with a `CB`/`DD`/`ED`/`FD` prefix have **two M1 cycles and therefore +2**; `DD CB`/`FD CB` also net +2 (the second CB fetch after a prefix is not an M1 fetch).

**On the HB-F1XV this M1 wait is generated by the Yamaha S1985 MSX-Engine** — as the grauw/tni reference puts it, *"the MSX engine adds one additional wait state to every M1 cycle."* The S1985 has a dedicated M1-wait output that drives the Z80 `/WAIT` pin during M1, following the "Memory Speed Control" concept in the Zilog Z80 CPU User Manual (pp. 30–31). It is *not* the VDP CPUCLK circuit and *not* the V9958's separate software-enabled VDP-access waitstate generator. For cycle accuracy you MUST model this: the grauw table's **"Z80+M1" column** is the authoritative per-instruction MSX timing, and both openMSX and blueMSX emulate it (fMSX historically did not).

Representative timings (bare Z80 → MSX Z80+M1):

| Instruction | Opcode | Bare Z80 | MSX (+M1) |
|---|---|---|---|
| `NOP` / `XOR A` | `00` / `AF` | 4 | 5 |
| `LD r,r'` | | 4 | 5 |
| `LD r,(HL)` | | 7 | 8 |
| `LD A,(nn)` | `3A nn nn` | 13 | 14 |
| `ADD HL,BC` | `09` | 11 | 12 |
| `ADC HL,BC` | `ED 4A` | 15 | 17 |
| `INC (HL)` | `34` | 11 | 12 |
| `BIT b,r` | `CB 40+…` | 8 | 10 |
| `BIT b,(HL)` | `CB 46+…` | 12 | 14 |
| `ADD A,(IX+d)` | `DD 86 d` | 19 | 21 |
| `SET b,(IX+d)` | `DD CB d …` | 23 | 25 |
| `CALL nn` | `CD nn nn` | 17 | 18 |
| `CALL cc,nn` taken/not | | 17/10 | 18/11 |
| `LDIR` (per iter/last) | `ED B0` | 21/16 | 23/18 |
| `CPIR` (per iter/last) | `ED B1` | 21/16 | 23/18 |
| `EX (SP),HL` | `E3` | 19 | 20 |

Conditional instructions (`CALL cc`, `RET cc`, `JR cc`, `DJNZ`) have two timings depending on whether the branch is taken. The block-repeat instructions cost more per iteration than on the final iteration.

### DD/FD prefix M-cycle detail (indexed bit ops)

`DD CB d op` decodes **out of order**: prefix, `CB`, then the **displacement byte `d`**, then the sub-opcode. The Z80 computes `IX+d` in parallel with fetching the sub-opcode. The M/T breakdown (bare) is:

```
[1] 4  read DD (or FD) prefix        (M1)
[2] 4  read CB prefix                (M1)
[3] 3  read displacement d
[4] 5  compute (IX/IY)+d
[5] 4  read sub-opcode + do op
[6] 3  write result back  (BIT: this step is a read, and there is no writeback)
```

Note the sub-opcode fetch here does **not** increment R (it is not an M1 cycle) — a subtlety many naive cores get wrong.

---

## 4. Undocumented / Illegal Opcodes

### SLL / SL1 (`CB 30–37`)
Undocumented shift: shifts left and sets bit 0 to **1** (unlike `SLA`, which clears it). Effect: `r = (r<<1)|1`. Flags set like the other shifts (S,Z,Y,X,P,C from result; H,N reset). On the real Zilog Z80A in the HB-F1XV this behaves as described; note the R800 aliases it to plain `SLA` (a compatibility divergence — see §9).

### IXH/IXL/IYH/IYL (undocumented DD/FD register halves)
When a `DD`/`FD` prefix is applied to an instruction operating on `H` or `L` (but *not* accessing memory via `(HL)`), it instead operates on the high/low byte of the index register: `IXH`, `IXL`, `IYH`, `IYL`. E.g. `DD 24` = `INC IXH`. These are fully usable on the real Zilog part and widely relied upon. Note `LD IXL,IYH` is impossible — only one index register is referenced per instruction. The R800 makes these official.

### Prefix chaining & NONI
`DD`/`FD` act like one-shot state flags: in a run of `DD`/`FD` bytes, only the **last** one counts, the earlier ones behave as effective NOPs (each still an M1 fetch, +1 M1 wait, R+1). A `DD`/`FD` before an instruction that doesn't use HL/(HL) has no effect but still consumes 4(+1) T-states. Undocumented/hole `ED` opcodes act as two NOPs (2-byte no-ops, sometimes labelled NONI — "no interrupt"). Most undocumented `ED` `RETxx` variants behave like `RETN`.

### BIT n,(HL) and the WZ/MEMPTR register
The Z80 has an internal 16-bit register called **WZ** (a.k.a. **MEMPTR**), used to hold temporary addresses. It is invisible to software *except* through the X/Y flags of `BIT n,(HL)` and `BIT n,(IX/IY+d)`: for these, **X and Y are copied from bits 11 and 13 of WZ**, not from a result. To pass ZEXALL you must track WZ for every instruction that updates it. Key WZ update rules:
- `LD A,(nn)` / `LD (nn),A`: WZ = nn+1 (for the store form, the high byte of WZ receives A).
- `LD A,(BC/DE)`: WZ = BC/DE + 1.
- `JP nn` / `CALL nn` (even conditional, whether or not taken): WZ = nn.
- `JR`/`DJNZ` taken: WZ = destination. **Not** changed for conditional `JR` not taken, nor for `DJNZ` with B==1 (falls through).
- Any `(INDEX+d)` access: WZ = INDEX+d.
- Block instructions and `IN/OUT (C)`: see below.
- Interrupt acceptance (CALL to handler): WZ = handler address.

### Block-instruction flag quirks (must-implement for ZEXALL)
- **`LDI/LDIR/LDD/LDDR`:** Let `n = A + (last byte transferred)`. Then **Y (F5) = bit 1 of n**, **X (F3) = bit 3 of n**. H,N reset; P/V = (BC≠0 after decrement).
- **`CPI/CPIR/CPD/CPDR`:** S,Z,H set as by a hypothetical `CP (HL)`. Let `n = A − (HL) − H` (H taken *after* the compare). **Y = bit 1 of n**, **X = bit 3 of n**. P/V = (BC≠0). N=1. **C unchanged.** WZ: `CPI` → WZ+1, `CPD` → WZ−1; repeating forms adjust WZ per Boo-boo's rules.
- **`INI/INIR/IND/INDR`:** S,Z,Y,X affected as by `DEC B`. N = bit 7 of the value read. For C/H: take `(HL)` value + `((C±1) & 255)` (C+1 for INI/INIR, C−1 for IND/INDR); if the sum > 255, set **both C and H**. P/V = parity of `((sum & 7) XOR B)`. WZ = BC(before B decrement) ± 1.
- **`OUTI/OTIR/OUTD/OTDR`:** S,Z,Y,X as by `DEC B`. N = bit 7 of the value output. For C/H: take **L** (after HL inc/dec) + value output; if > 255, set C and H. P/V = parity of `((k & 7) XOR B)`. WZ = BC(after B decrement) ± 1. **Note:** these DO affect the carry flag on the MSX/real Zilog — openMSX implements this and passes ZEXALL, contradicting the (incomplete) official spec that says carry is unaffected.

### 16-bit arithmetic flags
`ADD HL,rr`: H from bit 11 carry, C from bit 15 carry; Y/X from bits 13/11 of result (high byte bits 5/3); S,Z,P/V unaffected. `ADC/SBC HL,rr`: affect all flags including S,Z,V; N=1 for SBC, 0 for ADC. `SBC HL,rr` sets N like other subtractions (a correction over some older docs).

### DAA
`DAA` must be implemented from the full table (based on prior H, N, C flags and both nibbles of A) — it works correctly after both additions and subtractions. Naive "BCD-only" implementations fail ZEXALL because the exerciser feeds `DAA` after arbitrary operations. Sean Young's document and the `z80.info` flag tables give the authoritative correction values.

---

## 5. Interrupt Handling

### Modes
- **IM 0:** executes an instruction placed on the data bus by the interrupting device (8080-style). Not used on MSX. With a floating bus reading 0xFF, that byte = `RST 38h`.
- **IM 1:** CPU jumps to a fixed address **0x0038**. **This is the mode the MSX BIOS uses.** After acceptance, the standard entry does the KEYINT/H.KEYI → VDP-check → H.TIMI processing.
- **IM 2:** vectored — forms address `(I << 8) | vector` from a 257-byte table. Usable on MSX (some software uses it for tighter line-split/MIDI response), but the MSX architecture cannot arbitrate multiple simultaneous vector-supplying devices, so it's uncommon.

### MSX / HB-F1XV interrupt wiring
On a standard MSX the **VDP is the only interrupt source** by default. The V9958 generates a **VBlank interrupt** when it begins drawing the lower border (scanline 192 or 212 depending on vertical resolution), and can optionally generate a programmable **HBlank (line) interrupt** at the scanline in VDP register R#19 (enabled/masked via bits in R#0 and R#1). The VDP asserts `/INT` (level-triggered, active low) and **holds it low until the CPU reads VDP status register S#0** (which clears the IRQ bit). NMI is **not used** on MSX — the Z80 `/NMI` pin is tied to +5V (inactive), though a handler stub exists at 0x0066.

**Emulator consequences:**
1. The VDP `/INT` line is a level, not a pulse: keep it asserted until S#0 is read. If the ISR executes `EI` before reading S#0, the interrupt re-triggers immediately (the classic MSX infinite-stack-fill crash).
2. On interrupt acknowledge the data bus on the well-designed Sony HB-F1XV floats/reads **0xFF** (no device supplies a vector). In IM1 this is irrelevant (fixed 0x0038); note that on *some* other machines (e.g. certain Gradiente models) the bus reads non-0xFF "random" values — not a concern for the HB-F1XV, but worth a configuration flag if you generalize.

### Interrupt acknowledge timing (bare Z80)
- **IM 1:** **13 T-states** = special M1 acknowledge cycle of 7 T (normal 4 + **2 automatic wait states**; during this cycle `/IORQ` + `/M1` are both low) + 3 T (push PC high) + 3 T (push PC low, load PC=0x0038). Per z80.info: *"It takes 13 clock cycles to reach #0038: 1. M1 cycle: 7 ticks acknowledge interrupt and decrement SP; 2. M2 cycle: 3 ticks write high byte of PC onto the stack…; 3. M3 cycle: 3 ticks write low byte onto the stack and to set PC to #0038."*
- **IM 2:** **19 T-states** = 7 + 3 (push PCh) + 3 (push PCl) + 3 (read vector low) + 3 (read vector high, jump).
- **NMI:** **11 T-states** = 5 (M1 opcode-read + SP dec) + 3 + 3, vector 0x0066.

The Zilog Z80 CPU User Manual corroborates the stretched acknowledge: *"a special M1 cycle is generated. During this special M1 cycle, the IORQ signal becomes active (instead of the normal MREQ)… Two wait states are automatically added to this cycle."* On the MSX, the M1 wait applies to the acknowledge M1 cycle as well, so an MSX-accurate model should add roughly +1 to the acknowledge cycle (IM1 ≈ 14 T on MSX); verify the exact application against openMSX's `CPUCore` when you get to sub-cycle audio timing. The interrupt is sampled at the **rising edge of the last T-state of an instruction**; `/INT` should therefore be held active long enough to span the longest instruction (some indexed ops last 23+ T-states).

### DI / EI / RETI / RETN, and the one-instruction EI delay
- `DI`: clears IFF1 and IFF2 immediately.
- `EI`: sets IFF1 and IFF2, but the CPU **does not accept a maskable interrupt until after the instruction following `EI`** — the "one-instruction EI delay." This lets an ISR do `EI` `RET` (or `EI` `RETI`) without being re-interrupted before returning. Emulate by deferring the interrupt-enable check by exactly one instruction (e.g. an "EI pending" flag consumed at the top of the next instruction's interrupt check).
- On accepting a maskable interrupt: both IFF1 and IFF2 are reset.
- On NMI: IFF1 → 0, IFF1's old value saved in IFF2 (so the NMI is nested-safe); `RETN` copies IFF2 back into IFF1.
- `RETI` and `RETN` are functionally identical inside the CPU (both copy IFF2→IFF1); `RETI` differs only in that its opcode is recognizable on the bus by Z80-family peripherals for daisy-chain acknowledge (irrelevant to a stock HB-F1XV). All `ED`-prefixed `RETxx` variants copy IFF2→IFF1.
- The copy IFF2→IFF1 on `RETI/RETN` actually happens in the middle of the next opcode fetch, so the earliest a maskable interrupt fires after an NMI routine is at the end of the instruction following `RETN`.

### NMOS LD A,I / LD A,R interrupt bug (present on the HB-F1XV's Zilog NMOS part)
`LD A,I` and `LD A,R` copy IFF2 into the P/V flag. On the NMOS Z80, **if an interrupt is accepted during these instructions, P/V is reset (reads 0) even though interrupts were enabled beforehand.** This is a genuine silicon bug present on the real Zilog NMOS part in the HB-F1XV; robust interrupt-state detection code double-checks. A faithful emulator of this machine should reproduce it.

---

## 6. Memory & I/O Interface Timing

### Machine cycles (bare Z80, before MSX M1 wait)
- **M1 (opcode fetch):** 4 T-states. T1: PC on address bus, `/M1` low; `/MREQ`+`/RD` low mid-T1. Data sampled at rising edge of T3. T3–T4: **refresh** — R register on A0–A6 (I on A8–A15), `/RFSH` low, `/MREQ` pulsed for the DRAM refresh row; CPU decodes/executes in parallel.
- **Memory read/write (non-M1):** 3 T-states each. `/MREQ` + (`/RD` or `/WR`).
- **I/O read/write:** 4 T-states, because the Z80 **automatically inserts one wait state (TW)** between T2 and T3 of every I/O cycle (`/IORQ` + `/RD`/`/WR`). This is intrinsic to the Z80 and independent of the MSX M1 wait.

### MSX-specific wait states
1. **M1 wait (every opcode fetch):** +1 T-state, added by the S1985 (see §3). Applies to *all* code regardless of where it runs.
2. **VDP access waits:** The V9958 needs recovery time between consecutive VRAM port (0x98) accesses. On the HB-F1XV at stock 3.58 MHz the CPU is slow enough that `OTIR`/`INIR` bulk transfers are safe in most screen modes, but tight `OUT (0x98)` pairs need a spacing instruction. Access is faster during VBlank, when the screen is disabled, or (V9938/58) when sprites are disabled. The V9958 also has an *optional* software-enabled `/WAIT` generator (R#25 bit 2, "WTE") — but on stock MSX2+ machines like the HB-F1XV this is generally left disabled; the machine relies on programmer discipline rather than hardware handshake. Model the VDP-access recovery timing per the V9958 fact-sheet (the openMSX VDP-timing tables are the reference), because it affects raster-timed effects.
3. The HB-F1XV has **no CPU turbo mode** (unlike the Panasonic FS-A1WX/WSX which can switch the Z80 to ~5.37 MHz via an I/O port). The "Speed Controller" slider is *not* a clock change — it is implemented as an autofire on the PAUSE button synced to BLANK, slowing games by pausing them intermittently.

### Interface to the memory-mapping hardware
The Z80's 16 address lines + control signals feed the **S1985** (which decodes slot selection and generates the M1 wait and DRAM control) and the **MB64H444** (which expands mapper address lines). From the CPU's perspective these are transparent: the Z80 just does normal memory reads/writes to a flat 64 KB space, and the slot/mapper hardware redirects those accesses to the physical device currently paged in. The I/O ports the CPU uses to control this are described in §7.

---

## 7. MSX-Specific Integration

### Primary slot switching — I/O port 0xA8 (PPI port A)
The 64 KB Z80 address space is divided into four 16 KB **pages** (page 0 = 0x0000–0x3FFF, … page 3 = 0xC000–0xFFFF). Port **0xA8** (the 8255/PPI port A inside the S1985) selects which of the four **primary slots** (0–3) is mapped into each page, 2 bits per page:

```
Port 0xA8 (write = select, read = current):
  bit 7-6 : primary slot for page 3 (C000-FFFF)
  bit 5-4 : primary slot for page 2 (8000-BFFF)
  bit 3-2 : primary slot for page 1 (4000-7FFF)
  bit 1-0 : primary slot for page 0 (0000-3FFF)
```

The PPI is at ports 0xA8–0xAB (A8=port A slot select, A9=port B keyboard row read, AA=port C keyboard/LED/cassette, AB=mode/control). The S1985 also **mirrors** the PPI at 0xAC–0xAF and mirrors VDP ports 0x98–0x9B at 0x9C–0x9F — a mirror that can be used to *detect* the presence of an MSX-Engine.

### Secondary slot switching — memory-mapped 0xFFFF
Each primary slot may be subdivided into four **secondary slots** via the register at physical address **0xFFFF** of that slot. Procedure: first page the target primary slot into 0xC000–0xFFFF (via 0xA8 bits 6–7), then write the secondary-slot configuration byte (same 2-bits-per-page layout) to 0xFFFF. **Reading 0xFFFF returns the value XORed with 0xFF (inverted)** — this inversion is the standard way to detect whether a slot is expanded. The HB-F1XV's internal devices (BIOS, sub-ROM, disk ROM, FM-BASIC/MSX-MUSIC ROM, RAM, Kanji ROM, MSX-JE SRAM) are distributed across expanded slots.

**Emulator note:** implement the slot state as a small matrix and recompute the four active 16 KB page pointers whenever 0xA8 or a slot's 0xFFFF register changes, so the hot memory-access path is a simple `page[addr>>14][addr&0x3FFF]` lookup.

### The R register (memory refresh) and software timing
`R` is an 8-bit counter whose **low 7 bits increment after every M1 opcode fetch** (bit 7 is preserved and only changes via `LD R,A`). Prefixed instructions increment R once per prefix (so `CB`/`ED`/`DD`/`FD` ops add 2; `DD CB`/`FD CB` add 2 as the post-prefix CB fetch is not an M1). Block-repeat instructions increment R twice per iteration. Software uses `LD A,R` for cheap pseudo-random numbers and copy-protection; you must increment R exactly correctly or such code misbehaves. `R` is *not* usable as a reliable high-precision timer on real hardware, but its value is deterministic given the instruction stream. On the MSX the refresh function itself is real (the machine uses DRAM), but it is transparent to software — you only need to model the R *register value*, not perform actual refresh.

### HB-F1XV vs. reference design / FS-A1 series
- The HB-F1XV uses a **discrete Zilog Z80 + S1985 + MB64H444**, whereas the Panasonic FS-A1 MSX2+ series integrates the Z80 clone inside a **Toshiba T9769** MSX-Engine. This is the single most emulation-relevant deviation: the HB-F1XV shows authentic Zilog NMOS quirks; T9769-based machines show Toshiba clone behavior and add their own VDP-access wait cycles (the T9769A/B add 2 CPU wait cycles per VDP I/O; the T9769C adds 1). The HB-F1XV's S1985 adds the standard M1 wait but does not add those extra Toshiba VDP waits by default.
- The HB-F1XV is nearly identical to its sibling **HB-F1XDJ** (mainly cosmetic/firmware differences); both use the same Z0840004SPC + S1985 + MB64H444 set.
- The HB-F1XV ships with only 64 KB main RAM (not internally expandable without replacing the RAM chips), 128 KB VRAM, plus 16 KB SRAM for MSX-JE.

---

## 8. Hardware Quirks, Undocumented Behavior & Emulator Pitfalls

### SCF / CCF undocumented X/Y flag behavior (Zilog NMOS — the HB-F1XV case)
The setting of X (F3) and Y (F5) after `SCF`/`CCF` depends on the **previous** instruction, abstracted via a "Q" latch: after a flag-modifying instruction, Q = new F; after a non-flag-modifying instruction (including `EX AF,AF'`, `POP AF`, and after an interrupt), Q = 0. Per redcode/Z80_XCF_Flavor: *"In 2012, Patrik Rak deciphered the behavior of YF and XF in the ccf and scf instructions, confirming that their values correspond, respectively, to bits 5 and 3 of the result of (Q ^ F) | A. This discovery applies to all Zilog Z80 models, both NMOS and CMOS."* Concretely, for genuine Zilog Z80:
```
YF = ((Q ^ F) | A).5     (bit 5)
XF = ((Q ^ F) | A).3     (bit 3)
```
i.e. if the previous instruction modified flags, X/Y are **moved** from A; otherwise they are **OR'd** from A into the previous F bits. **The HB-F1XV has a genuine Zilog NMOS part, so implement the Zilog rule.** NEC clones AND with an unknown value (unreliable); the R800 differs again — not relevant here.

### OUT (C),0 (undocumented `ED 71`)
On NMOS Zilog Z80 (the HB-F1XV), `ED 71` outputs **0**. On CMOS Z80 it outputs **0xFF**. Since the HB-F1XV is NMOS Zilog, model it as outputting 0. (This, plus the `LD A,I` bug, are the two classic ways software detects NMOS vs CMOS.)

### R register bit-7 quirk
Only bits 0–6 count; bit 7 is frozen except via `LD R,A`. So R wraps 0x7F→0x00 (within the low 7 bits) while bit 7 stays put. A common bug is letting R increment through all 8 bits.

### Contended memory — NOT applicable (contrast note)
On the ZX Spectrum, the ULA "contends" (stalls) CPU access to shared RAM/VRAM regions on specific T-states, producing complex per-address timing. **The MSX has no such CPU-RAM contention** — main RAM access is uniform. The only MSX timing complications are the (uniform) M1 wait, the intrinsic I/O wait, and VDP-port recovery timing. Do not port Spectrum contention logic here.

### Second-source timing/behavior differences (for completeness)
Because the HB-F1XV uses a real Zilog part, none of the following apply to it, but note for a general MSX emulator: NEC µPD780 clones differ on SCF/CCF X/Y and on some flag details; U880 (East German) and KR1858VM1/T34VM1 clones differ on the `OUTI` carry-clear edge case (B or HL == special values); Sharp LH0080 and Toshiba parts are generally very close to Zilog. Toshiba clones inside T9769 engines match Zilog closely for documented behavior but the *machine* adds VDP-access waits.

### Reference cores & test suites
- **ZEXDOC / ZEXALL** (Frank Cringle's Z80 Instruction Exerciser): the de-facto correctness suites. ZEXDOC tests the 6 documented flags; **ZEXALL** additionally tests the undocumented X/Y flags. Passing ZEXALL essentially requires correct WZ/MEMPTR, SCF/CCF-Q, block-instruction, and DAA emulation. Run them under a minimal CP/M or MSX-DOS harness.
- **Patrik Rak's z80test / redcode Z80_XCF_Flavor:** finer-grained modern tests, especially for the SCF/CCF-Q behavior and CPU-variant detection.
- **openMSX Z80 core** (`src/cpu/CPUCore.cc`): the gold-standard reference for MSX-accurate Z80 timing, including the M1 wait and VDP-access timing; passes ZEXALL. Consult it for exact sub-cycle behavior.
- **MAME Z80 core** (`z80.cpp`): another well-validated reference; historically had SCF/CCF X/Y issues (MAME bug #02701) later addressed.
- **floooh's cycle-stepped Z80** (`chips/z80.h`): a clean, well-documented modern C core with a tick-level breakdown of DD/FD/CB decoding and interrupt timing — excellent architectural reference for a C/C++ implementation.
- **Primary docs:** Zilog *Z80 CPU User Manual* (UM0080); Sean Young's *The Undocumented Z80 Documented* (v0.91); Boo-boo's *MEMPTR* note; the MSX Assembly Page Z80 instruction/timing tables (grauw).

### Common pitfalls checklist
1. Forgetting the **MSX M1 wait** (+1 per M1) — the single most common MSX-specific timing error; breaks PSG/sample players.
2. Treating VDP `/INT` as a pulse instead of a level (S#0-read acknowledge) — causes missed or runaway interrupts.
3. Missing the **EI one-instruction delay**.
4. Incorrect **WZ/MEMPTR** → ZEXALL `BIT n,(HL)` failures.
5. Naive **DAA** → fails after non-BCD operations.
6. Wrong **R** increment on prefixed/block instructions.
7. Not implementing **block-instruction and IN/OUT (C) carry/HF/PV quirks** (e.g. `OUTI` affecting carry on the real chip).
8. Modeling `DD`/`FD` as consuming no time when unused (they still cost 4(+1) T and R+1).

---

## 9. Comparison: Z80A (HB-F1XV) vs. R800 (turbo R)

For context if you later add turbo R support:

| Aspect | Z80A (HB-F1XV) | R800 (turbo R) |
|---|---|---|
| Clock | 3.579545 MHz | 7.159 MHz (from 28.63630 MHz crystal ÷4); ~5–8× faster in internal RAM |
| ALU | 4-bit (executed twice) | full 16-bit |
| Instruction set | Z80 + undocumented | Z80 superset: adds `MULUB`/`MULUW`; IXH/IXL/IYH/IYL made official |
| `SLL` | sets bit 0 = 1 | aliased to `SLA` (bit 0 = 0) |
| Undocumented X/Y flags | authentic Zilog behavior | **different** — fails ZEXALL |
| DRAM refresh | per-instruction (R register, transparent) | block refresh: CPU halted ~4 µs every ~30 µs |
| M1/opcode fetch | 4 T + 1 MSX M1 wait | pipelined; simple ops can be 1 cycle in internal RAM |
| CPU switching | n/a (Z80 only) | S1990 engine, I/O ports 0xE4/0xE5 reg 6 select Z80/R800-ROM/R800-DRAM |

Per Wikipedia's R800 article: *"As the R800 is not based directly on the Z80, but stems from the Z800 family… the undocumented flags represented by bits 3 and 5 of the F register do not hold the same values as in the Z80 (causing it to fail ZEXALL tests) and the undocumented opcode often called SLL is just an alias of the SLA instruction."* MSX Wiki's ASCII R800 page confirms the clock: *"Working frequency is 7.159 Mhz. (The oscillator used is a 28.63630 MHz, but an internal circuit divides this frequency by 4.)"*

**Why it matters for emulation:** an R800 core is *not* just a faster Z80 — the timing model is completely different (block refresh, pipelining, page-crossing penalties, per-region wait profiles), the undocumented-flag behavior diverges (so you cannot share the SCF/CCF-Q and BIT-WZ logic wholesale), and `SLL` semantics differ. A turbo R emulator also needs the V9958 (shared) but must decouple CPU and VDP clocks (separate crystals). For the HB-F1XV specifically, none of this applies: a correct, ZEXALL-passing Zilog NMOS Z80 core with the MSX M1 wait and VDP-access timing is exactly and only what you need.

---

### Bottom line
For the HB-F1XV, implement a **genuine Zilog NMOS Z80A** core: full documented + undocumented opcodes, WZ/MEMPTR tracking, Zilog-style SCF/CCF-Q flags, the NMOS `OUT (C),0`=0 and `LD A,I/R` interrupt-bug behaviors, correct R-register increments, the **MSX M1 wait (+1 T per opcode fetch, generated by the S1985)**, the intrinsic I/O wait state, IM1 interrupt handling driven by a level-held VDP `/INT` cleared on S#0 read, and the one-instruction EI delay. Validate against ZEXALL and cross-check timing against openMSX. That yields a behavior- and cycle-accurate CPU for this machine.