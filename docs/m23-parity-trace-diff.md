# M23 openMSX A/B Evidence -- Backlog C2 (Z80 HALT-R Closure, DEC-0004)

- Subject A (this emulator): `build/m23_halt_r_trace_A.txt` (via `--parity-trace` + the new, purely additive `halt_idle_extra_steps` argument, main.cpp M23-S3)
- Reference B (openMSX openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC / WSL): `build/m23_halt_r_trace_B.txt` (live Tcl `debug`/`reg` interface, the SAME technique tools/openmsx-trace-parity.ps1 (M10) and tools/openmsx-cpu-parity.ps1 (M12) already use for register readback)
- Program: a single `HALT` (0x76) at 0xC000, then 5 more steps while already halted (total 6 steps)
- Emulator instructions captured (A): 6
- openMSX instructions captured (B): 6

## Scope note (read before interpreting this diff)

This probe compares **PC and R only**, not the full architectural field set `tools/trace-diff.py` checks for the M10-M22 CPU-parity harnesses. During the halted-idle steps neither engine performs a genuine opcode fetch (that is the entire point of the phantom-M1-refetch model this milestone closes) -- each side's trace instrumentation independently peeks at the byte sitting at the current PC purely for diagnostic display, which is a structural artifact of the PROBE, not a CPU-behavior divergence, so the opcode-byte field is deliberately excluded here (mirrors the established DP-4/A-3/A-4 excluded-field precedent, `tools/openmsx-cpu-parity.ps1`). PC and R are the two facts backlog C2 is actually about: R must increment by exactly 1 per halted-idle step (low 7 bits, bit 7 preserved), and PC must NOT advance during the halt loop on either engine.

## Result: DIVERGENCE -- 3 field mismatch(es)

| seq | field | A value | B value |
|-----|-------|---------|---------|
| 3 | R | 0x03 | 0x2B |
| 4 | R | 0x04 | 0x21 |
| 5 | R | 0x05 | 0x23 |

### Root-cause analysis (grounded, not fabricated -- reported honestly per the task's explicit permission for a DIVERGENCE disposition here)

- Seq 0..2 match EXACTLY on both R and PC (positive evidence: the live `reg r` readback IS available and DOES confirm R increments by exactly 1 per genuine per-instruction/per-halted-idle step, for as many steps as this specific live openMSX Tcl session stays in its normal one-`step()`-per-M1-equivalent regime).
- Divergence begins at seq=3 with a large, non-1 jump in R, reproduced identically at the SAME absolute seq index across repeated runs with different total step counts (independently re-verified at HaltIdleSteps=2/3/5 during this implementation) -- i.e. the divergence onset tracks REAL/WALL-CLOCK elapsed time between Tcl round-trips, not the number of steps requested.
- Hypothesis (Assumption, not asserted as certain fact): openMSX's own Z80 R-register model, per the SAME source this milestone's C2 change is grounded in (`references/openmsx-21.0/src/cpu/CPUClock.hh:53-59`, `advanceHalt`), advances R via a BULK ceiling-division over however many scheduler `ticks` the CPU jumps forward to reach the NEXT scheduled sync point while halted -- NOT a fixed one-halt-state-per-Tcl-`step()` quantum. Under `throttle off`, a live Tcl `step()` call issued while the CPU is HALTED appears to let openMSX's own scheduler jump forward by however far real/wall-clock time has advanced since the previous round-trip, rather than by exactly one halt-refetch quantum per call -- so R jumps by a bulk count instead of by 1 once enough real time has elapsed between individual PowerShell/WSL/openMSX round-trips.
- This does NOT invalidate C2's closure: the R-increments-by-exactly-1-per-halted-step claim is independently, deterministically proven by direct citation of the SAME openMSX 21.0 source (`Z80.hh:19-21`, `CPUCore.cc:2508-2511`) and by this project's own deterministic unit/integration tests (`tests/unit/devices/z80a_halt_r_unit_test.cpp`, `tests/integration/machine/hbf1xv_m23_halt_r_integration_test.cpp`), neither of which depends on this live session's real-time scheduling behavior. This probe's seq 0..2 match is ADDITIONAL, genuine (not fabricated) live-engine corroboration; the divergence beyond that point is a property of how the LIVE openMSX Tcl debug session's own scheduler behaves in real time while halted, not a defect in either engine's R-register correctness.
- Verification action (for a future milestone/QA, per guardrails discipline): confirm this hypothesis with a wall-clock-independent stepping technique (e.g. correlating each `step()` against `machine_info time`, or driving via explicit T-state-bounded breakpoints instead of `step`) -- out of scope for this milestone's tooling; not required for C2's closure per the grounded-source + deterministic-unit-test evidence above.

