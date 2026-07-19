# Z80 Parity Checkpoint Program (M10-S4)

Deterministic, RAM-only, I/O-free Z80 program used to produce a per-instruction
architectural-state parity trace between this emulator and openMSX 19.1.

- Binary: `z80_parity_checkpoint.bin` (95 bytes)
- SHA-256: `b58ccb3e84e94a20d771a0dce171bc1426fbe50a5f93505c6afbe54cdca732db`
- Load base: `0xC000` (MSX page 3 — main RAM on both emulators after boot)
- Data scratch: `0xC200`, `0xC220`, `0xC240` (all page 3 RAM; every byte the
  program READS is seeded by the program itself before use, so no reliance on
  power-on / BIOS-leftover RAM contents)
- Checkpoint: `HALT` at PC `0xC055`

## Why these constraints

- Everything executes and reads/writes strictly inside `0xC000-0xFFFF` (page 3).
  On this emulator that page is flat RAM; on openMSX (C-BIOS_MSX2+) that page is
  mapped RAM after boot. Nothing touches page 0-2 (which is C-BIOS ROM on
  openMSX but zero-RAM here) so the two machines cannot diverge on fetched bytes.
- No `IN`/`OUT`, no BIOS calls, no interrupts (starts with `DI`), no self-modifying
  code. The only ED block op used is the NON-repeating `LDI` (BC=1), deliberately
  avoiding block-REPEAT ops (LDIR/CPIR/...) whose per-iteration opcode re-fetch
  would make single-step alignment ambiguous across the two step engines.
- `BIT b,r` uses a register operand (undocumented X/Y flags copy bits 3/5 of the
  tested register — well defined on both), NOT `BIT b,(HL)` (whose X/Y come from
  the internal WZ/MEMPTR high byte and are a known emulator-divergence area).

## Documented identical initial CPU vector (both emulators)

Matches this emulator's `cold_boot` reset state; forced onto openMSX via `reg`:

```
AF=0000 BC=0000 DE=0000 HL=0000
AF'=0000 BC'=0000 DE'=0000 HL'=0000
IX=0000 IY=0000 SP=FFFF PC=C000
I=00 R=00 IFF1=0 IFF2=0 IM=1
```

## Listing (base 0xC000)

```
C000  F3            DI              ; interrupts off (IFF already 0)
C001  ED 56         IM 1
C003  31 F0 FF      LD SP,0FFF0h    ; stack in page-3 RAM
C006  3E 2A         LD A,2Ah
C008  06 03         LD B,03h
C00A  0E 05         LD C,05h
C00C  80            ADD A,B         ; A=2D
C00D  91            SUB C           ; A=28
C00E  0F            RRCA            ; A=14
C00F  C6 07         ADD A,07h       ; A=1B
C011  E6 3F         AND 3Fh         ; A=1B
C013  F6 40         OR 40h          ; A=5B
C015  EE FF         XOR 0FFh        ; A=A4
C017  FE A4         CP 0A4h         ; Z=1
C019  3C            INC A           ; A=A5
C01A  3D            DEC A           ; A=A4
C01B  47            LD B,A          ; B=A4
C01C  CB 40         BIT 0,B
C01E  CB 00         RLC B
C020  CB 38         SRL B
C022  21 00 C2      LD HL,0C200h
C025  11 20 C2      LD DE,0C220h
C028  36 11         LD (HL),11h     ; seed C200=11
C02A  23            INC HL
C02B  36 22         LD (HL),22h     ; seed C201=22
C02D  2B            DEC HL
C02E  01 01 00      LD BC,0001h
C031  ED A0         LDI             ; C200 -> C220 (single, non-repeating)
C033  DD 21 00 C2   LD IX,0C200h
C037  DD 7E 00      LD A,(IX+0)     ; A=(C200)=11
C03A  DD 34 01      INC (IX+1)      ; C201: 22 -> 23
C03D  DD 86 00      ADD A,(IX+0)    ; A += 11 -> A=22
C040  FD 21 40 C2   LD IY,0C240h
C044  FD 36 00 55   LD (IY+0),55h   ; seed C240=55
C048  FD 7E 00      LD A,(IY+0)     ; A=55
C04B  CD 5C C0      CALL 0C05Ch     ; exercise stack
C04E  06 03         LD B,03h
C050  0C            INC C           ; loop body
C051  10 FD         DJNZ C050h      ; loops 3x
C053  18 00         JR C055h
C055  76            HALT            ; <-- checkpoint PC
C056  00 x6                         ; padding (never executed)
C05C  47            LD B,A          ; subroutine
C05D  04            INC B
C05E  C9            RET
```

## Instruction-family coverage

- Unprefixed 8-bit loads / immediates, ALU (ADD/SUB/AND/OR/XOR/CP/INC/DEC),
  accumulator rotate (RRCA), 16-bit loads (LD rp,nn / LD SP,nn).
- CB family: `BIT`, `RLC`, `SRL` (register operands).
- ED family: `IM 1`, `LDI` (block op, non-repeating).
- DD/FD indexed family: `LD IX/IY,nn`, `LD A,(IX+d)`, `INC (IX+d)`,
  `ADD A,(IX+d)`, `LD (IY+d),n`, `LD A,(IY+d)`.
- Control flow: `CALL`/`RET` (stack), `DJNZ` loop, `JR`, `HALT`.
