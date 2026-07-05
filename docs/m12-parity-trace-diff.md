# M10-S4 openMSX Per-Instruction Parity Trace Diff

- Subject A (emulator): `build/m12_cpu_parity_trace_A.txt`
- Reference B (openMSX 19.1 / WSL): `build/m12_cpu_parity_trace_B.txt`
- Emulator instructions (A): 48
- openMSX instructions (B): 48

## Result: ARCHITECTURAL PARITY -- EMPTY DIFF

All 48 instructions match on every architectural field (PC, opcode, A, F + all flag bits, B, C, D, E, H, L, AF, BC, DE, HL, AF', BC', DE', HL', IX, IY, SP, I, R, IFF1, IFF2, IM).

## Cycle / T-state field (reported separately -- NOT part of R5 gate)

Per planner Section 4 / DP-4: openMSX inserts MSX M1 wait-states while this emulator's M9 core uses canonical Z80 T-states, so exact cross-emulator cycle parity is structurally out of M10 scope. This harness does not read openMSX's per-instruction T-state counter; the emulator-side canonical Z80 T-states are recorded in trace A (T=/TC= columns). Final emulator cumulative T-states: 435.


---

## M12-S6 CPU-parity boundaries (excluded from the architectural field gate)

This diff was produced by `tools/openmsx-cpu-parity.ps1` (extends
`tools/openmsx-trace-parity.ps1`) over `tests/parity/z80_parity_checkpoint.bin` at base 0xC000.

The following boundaries are INTENTIONALLY out of the field-diff and are proven
elsewhere (planner A-2/A-3/A-4, R-2):

- **WZ / MEMPTR (A-3):** not exposed by openMSX `reg` and not exported by this
  emulator's trace; structurally absent. Proven by `z80a_wz_memptr_unit_test`
  and the `BIT n,(HL)` X/Y path in `hbf1xv_cpu_parity_integration_test`.
- **SCF/CCF undocumented X/Y (A-4):** genuine-Zilog NMOS Q-latch
  `X/Y = ((Q^F)|A)` legitimately diverges from openMSX's `(F|A)` OR-form.
  Gated on the fact-sheet Q rule + `z80a_scf_ccf_q_unit_test`, NOT this diff.
  The parity program avoids SCF/CCF so the gate stays meaningful.
- **Cycle / T-states (DP-4):** openMSX inserts MSX M1 waits; this core reports
  canonical Z80 T-states. Timing parity is proven by the machine oracles
  (M11 ?4 and `hbf1xv_cpu_parity_integration_test` cycle assertions).

If openMSX could not be driven, the section above records BLOCKED (empty B side)
and no parity is asserted ??an honest outcome, never a fabricated pass.
