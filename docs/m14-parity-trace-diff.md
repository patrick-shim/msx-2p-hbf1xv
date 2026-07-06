# M14-S6 openMSX V9958 VDP Parity Trace Diff (Sony HB-F1XV)

- Captured (UTC): 2026-07-06T02:21:41Z
- Reference: openMSX on WSL, machine `Sony_HB-F1XV` (genuine V9958).
- Program: `tests/parity/m14_vdp_probe.bin` at base 0xC000; VRAM read-back = 256 bytes.
- A = this emulator (`--vdp-parity`); B = openMSX (`physical VRAM` / `VDP regs` / `VRAM pointer` debuggables).
- Gate fields: VRAM[0..255] + VRAM pointer + explicitly-written R#0,R#1,R#7,R#14,R#17.
- Excluded (not cross-comparable / deferred): BIOS-preset registers; frame-timing S#0 F / S#2 EO (D4).

## Result: ARCHITECTURAL PARITY (empty diff)

- openMSX VRAM lines captured (B): 16
- A VRAMPTR=0002  B VRAMPTR=0002
- Adversarial comparator check: PASS (corrupted byte at 0000: 00 -> 01 diverges from B)

## Diff (empty = pass)
```
(no differences on the gate fields)
```

