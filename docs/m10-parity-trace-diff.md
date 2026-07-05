# M10-S4 openMSX Per-Instruction Parity Trace Diff

- Subject A (emulator): `build/parity_trace_A.txt`
- Reference B (openMSX 19.1 / WSL): `build/parity_trace_B.txt`
- Emulator instructions (A): 48
- openMSX instructions (B): 48

## Result: ARCHITECTURAL PARITY -- EMPTY DIFF

All 48 instructions match on every architectural field (PC, opcode, A, F + all flag bits, B, C, D, E, H, L, AF, BC, DE, HL, AF', BC', DE', HL', IX, IY, SP, I, R, IFF1, IFF2, IM).

## Cycle / T-state field (reported separately -- NOT part of R5 gate)

Per planner Section 4 / DP-4: openMSX inserts MSX M1 wait-states while this emulator's M9 core uses canonical Z80 T-states, so exact cross-emulator cycle parity is structurally out of M10 scope. This harness does not read openMSX's per-instruction T-state counter; the emulator-side canonical Z80 T-states are recorded in trace A (T=/TC= columns). Final emulator cumulative T-states: 435.

