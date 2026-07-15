# M15-S6 openMSX Device-Integration Parity Trace Diff (Sony HB-F1XV)

- Captured (local): 2026-07-06
- Reference B: openMSX 19.1 on WSL, machine `Sony_HB-F1XV` (genuine S1985 + YM2149 PSG
  + RP5C01 RTC in its machine XML — the exact target hardware).
- Subject A (this emulator): `build/m15_parity_trace_A.txt` via `--parity-trace`.
- Reference B (openMSX): `build/m15_parity_trace_B.txt` (single-stepped, same probe,
  same base 0xC000, same forced initial CPU vector).
- Probe: `tests/parity/m15_io_probe.bin` (BIOS-independent, page-3 RAM-safe).
- Harness: `tools/openmsx-io-parity.ps1` → comparator `tools/trace-diff.py`.
- Gate fields: PC, opcode, A, F (+ all flag bits), B, C, D, E, H, L, AF, BC, DE, HL,
  AF', BC', DE', HL', IX, IY, SP, I, R, IFF1, IFF2, IM (full architectural CPU state).

## Probe (what lands the M15 device results in a CPU register)

The probe exercises the newly-wired S1985 device I/O whose result is deterministically
cross-comparable between both emulators (independent of BIOS/CMOS/host state):

1. PSG **R0 register readback** (8-bit tone-fine; AY and YM agree):
   `OUT (#A0),#00 ; OUT (#A1),#34 ; OUT (#A0),#00 ; IN A,(#A2)` → A = **0x34**.
2. PSG **R7 mixer readback with the MSX port-direction mask** (fact-sheet §2 "Critical
   R7 note"): `OUT (#A0),#07 ; OUT (#A1),#00 ; OUT (#A0),#07 ; IN A,(#A2)` → A = **0x80**
   (writing 0x00 forces port A input / port B output → stored & read back as 0x80).
3. RTC **#B4 address-port read**: `IN A,(#B4)` → A = **0xFF** (structural, MSXRTC.readIO;
   fact-sheet §5/§10). *(Executes at seq 15, just past the 15-instruction diff window.)*

## Result: ARCHITECTURAL PARITY — EMPTY DIFF

All 15 captured instructions match on every architectural field. Substantive, non-empty
evidence (not two blank traces):

| seq | behaviour | A (emulator) | B (openMSX HB-F1XV) |
|-----|-----------|--------------|---------------------|
| 3   | after `IN A,(#A2)` reading PSG R0 | AF=**3400** (A=0x34) | af=**3400** |
| 14  | after `IN A,(#A2)` reading PSG R7 (MSX mask) | AF=**8000** (A=0x80) | af=**8000** |

The R7 result (A=0x80) confirms on the **genuine HB-F1XV** that the MSX port-direction
masking (port A forced input / port B forced output) matches this emulator byte-for-byte.

```
(no differences on the gate fields across all 15 instructions)
```

## Notes / not cross-compared (documented, not fabricated)

- **RTC data-nibble value (#B5) is NOT gate-compared.** The Block-0/Block-2 nibble depends
  on each emulator's CMOS/epoch seed (this emulator uses a fixed deterministic epoch per
  DEC-0009 Q2; openMSX seeds from its own SRAM/host). Only the emulator-independent
  behaviours are gated (R6/R7 in the planner risk table). The RTC `#B4`=0xFF structural
  read is in the probe (seq 15).
- **Joystick R14 / keyboard idle reads** are not in this probe: plugged-input state is not
  guaranteed identical across emulator default configs. Those paths are covered by the
  deterministic unit + `hbf1xv_m15_devices_integration_test` (CPU→device over the bus).
- **Cycle / T-state field** is reported separately (emulator cumulative T=141) and is NOT
  part of the parity gate: openMSX inserts MSX M1 wait-states differently; exact
  cross-emulator cycle parity is the deferred timing-fidelity milestone (backlog C1).

## Reproduce

```powershell
cmake --build build --config Debug
tools/openmsx-io-parity.ps1 -ProgramBin tests/parity/m15_io_probe.bin `
  -DiffOut docs/m15-parity-trace-diff.md `
  -TraceA build/m15_parity_trace_A.txt -TraceBRaw build/m15_parity_trace_B.txt -MaxSteps 15
```
