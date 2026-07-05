# M10 QA Sign-off (REQ-M10-008)

- Milestone: M10 — Debug/Trace Infrastructure & openMSX Opcode-Trace Parity
- QA Owner: MSX QA Agent
- Date: 2026-07-06 (Asia/Seoul, +09:00)
- Source Request: REQ-M10-008 (final comprehensive milestone-closure sign-off)
- Verdict: **PASS** — M10 is closeable; the DEC-0001 R5 obligation is discharged.

This sign-off is grounded in QA-executed evidence (build + full ctest re-run) and an
independent, adversarial re-run of the openMSX parity harness against openMSX 19.1 on WSL.
No result below is restated from the developer report without QA confirmation unless
explicitly labeled `Assumption:`.

---

## 1. Regression Scope

Affected components across slices M10-S1..M10-S5 (all additive over the completed M9 CPU core):

- CPU trace-export hook (`src/devices/cpu/z80a_trace.h`, `z80a_cpu.{h,cpp}`) — must be
  off-by-default and non-perturbing to M9 CPU semantics/timing.
- Machine memory regions (`src/machine/memory_region.{h,cpp}`, `hbf1xv_machine.{h,cpp}`) —
  inert DRAM/SRAM/VRAM storage; must not regress CPU-visible 64K behavior.
- Debug dump + event log (`src/machine/debug_dump.{h,cpp}`, `debug_event_log.{h,cpp}`,
  `debug_format.h`, `cpu_trace_sink.{h,cpp}`) — determinism under `debug/`.
- openMSX parity harness (`tools/openmsx-trace-parity.ps1`, `tools/trace-diff.py`,
  `src/main.cpp --parity-trace`, `tests/parity/`) — genuine captured trace-diff (R5).
- `tools/` converters (`tools/mem-to-png.py`, `tools/mem-to-audio.py`) — deterministic.
- Regression baseline: all M9 CPU + interrupt suites must remain green.

## 2. Regression Matrix Status

| Area | Evidence | QA-verified | Status |
|------|----------|-------------|--------|
| Full build (Debug) | `cmake --build build --config Debug` re-run by QA — exit 0, all targets linked | Yes (re-run) | PASS |
| Full test suite | `ctest --test-dir build -C Debug --output-on-failure` re-run by QA — **28/28 passed, 0 failed** | Yes (re-run) | PASS |
| Trace-export non-perturbation (S1) | `z80a_trace_export_unit_test.cpp` asserts observer-OFF vs observer-ON identical AF/BC/DE/HL/IX/IY/SP/PC/I/R/IFF/HALT/total_tstates (lines 144-156); reproducibility record-equal (line 171) | Yes (source read) | PASS |
| Region sizes vs strict spec (S2) | `hbf1xv_machine.h`: `kDramBytes=64*1024`, `kVramBytes=128*1024` match baseline RAM 64KB / VRAM 128KB; `kSramBytes=8*1024` documented FM-PAC assumption | Yes (source read) | PASS (SRAM = Assumption, see Risks) |
| Debug dump/event-log determinism (S3) | `hbf1xv_debug_dump_unit_test.cpp` + `..._event_log_integration_test.cpp` byte-identical golden + on-disk oracles; both in the green 28/28 | Yes (ctest) | PASS |
| Converter determinism (S5) | `mem-to-png.py --self-check` and `mem-to-audio.py --self-check` re-run by QA — golden SHA-256 match + S3 fold round-trip = `SELF-CHECK: OK` | Yes (re-run) | PASS |
| Parity program integrity (S4) | `tests/parity/z80_parity_checkpoint.bin` SHA-256 re-computed by QA = `b58ccb3e84e9...cdca732db` (matches documented) | Yes (re-run) | PASS |
| **R5 openMSX parity (S4)** | See Section on R5 below — QA re-ran the harness against real openMSX 19.1 | Yes (re-run, not just audited) | PASS |

## 3. R5 Adversarial Validation (the critical duty)

QA treated the captured empty diff as a claim to be broken, not accepted. Findings:

1. **openMSX genuinely executed the program (not a stub).** WSL exposes
   `/usr/bin/openmsx` = openMSX 19.1. QA re-ran `tools/openmsx-trace-parity.ps1` end-to-end
   producing a FRESH openMSX trace (`build/qa_trace_B.txt`). It contains
   `OMSETUP ram_base_readback=F3` (the program's first opcode read back from page-3 RAM)
   followed by 48 `OMTRACE` lines with PC advancing C000 -> C001 -> C003 -> ... -> C055
   HALT. The R register increments per M1 fetch (e.g. seq1 `IM 1` R 01->03, +2 for the ED
   prefix) exactly as a real Z80 fetch would — this is live execution, not a copied file.
2. **The compared fields are real architectural state.** `tools/trace-diff.py` compares
   PC, opcode bytes, A, F (+ decomposed S,Z,Y,H,X,P/V,N,C), B,C,D,E,H,L, AF,BC,DE,HL,
   AF',BC',DE',HL', IX, IY, SP, I, R, IFF1, IFF2, IM. Trace A (emulator) and trace B
   (openMSX) use **different formats parsed by different functions** (`parse_trace_a`
   `SEQ=/PC=/OP=` vs `parse_trace_b` `OMTRACE seq=/pc=/op=`), so this is not a
   file-compared-to-itself artifact.
3. **The empty diff means agreement, not absence of data.** QA adversarially fed the
   comparator an EMPTY B side: it returned exit **2** = `BLOCKED -- a trace side is empty
   (not comparable)`, NOT pass. QA also corrupted a single field (AF `5500`->`5501` on the
   final instruction): the comparator returned exit **1** = `ARCHITECTURAL DIVERGENCE`
   pinpointing seq 47. The comparator therefore genuinely detects divergence and refuses
   to pass on missing data. The 48-instruction empty diff is real agreement.
4. **Determinism.** QA's freshly captured `build/qa_trace_B.txt` is byte-identical to the
   committed `build/parity_trace_B.txt` (verified via `diff` on the OMTRACE lines). The
   developer's two-run determinism claim is independently confirmed.
5. **Emulator half is regression-locked** independent of WSL by
   `machine_hbf1xv_parity_checkpoint_integration_test` (in the green 28/28), so CI stays
   deterministic even where openMSX is unavailable.

**R5 finding: the parity diff is GENUINE. QA RE-RAN it (openMSX 19.1 on WSL was available),
not merely audited it.** R5 is resolved by a real captured architectural-state empty diff
over 48 instructions to the HALT checkpoint at PC 0xC055.

## 4. Failures and Risk Ranking

No blocker-level or high-severity failures. Residual risks:

- **R-1 (Medium, coverage): the parity program is a bounded 48-instruction RAM-only
  sample, not a full zexall/zexdoc sweep.** Coverage is representative (unprefixed
  ALU/load/rotate/16-bit, CB BIT/RLC/SRL, ED IM1/LDI, DD/FD indexed, CALL/RET/DJNZ/JR/HALT)
  but does NOT exercise every opcode/flag edge case (e.g. DAA, 16-bit ADC/SBC flag
  synthesis, block-repeat ops, `BIT b,(HL)` MEMPTR/WZ X/Y behavior, IN/OUT). This is
  WITHIN the R5 acceptance definition of record (`docs/m10-planner-package.md` Section 4
  defines R5 as exactly this bounded committed program), so it is a residual risk, not an
  acceptance gap. Recommendation: schedule a broader signature-level zexdoc/zexall parity
  pass as a future milestone; several excluded areas (DAA, MEMPTR, 16-bit ADC/SBC flags)
  are classic Z80 divergence traps worth locking down before display/audio milestones.
- **R-2 (Medium, structural, declared): cross-emulator T-state/cycle parity is NOT gated.**
  openMSX inserts MSX M1 wait-states; the M9 core uses canonical Z80 T-states (planner
  DP-4). The harness reports the emulator-side cycle field separately (final TC=435) and
  never fails R5 on it. This is a documented, accepted structural bound, not a hidden
  failure. Recommendation: keep DP-4 as its own milestone if exact cycle parity is ever
  required.
- **R-3 (Low, Assumption): SRAM region size = 8 KB** is an FM-PAC assumption
  (`kSramBytes=8*1024`); the strict spec table lists no SRAM capacity. Region is INERT
  storage only. Verification action: confirm against the real FM-PAC device when DP-3 is
  implemented and adjust if authoritative capacity differs.
- **R-4 (Low): asset gates (`validate-assets.ps1`, `checksum-assets.ps1`) were not
  re-run by QA this cycle.** They are unaffected by M10's additive code and were reported
  green by the developer each slice. `Assumption:` asset gates remain green; verification
  action: re-run both before release tagging if assets changed (they did not this cycle).

## 5. Required Fixes

None required for M10 closure. All items in Section 4 are tracked residual risks/future
milestones, not blocker-level gaps. No fix is a precondition for discharging R5 or closing
M10.

## 6. Sign-off Decision

**PASS.**

- All six M10 acceptance criteria in `agent-protocol/state/milestones.md` are met:
  deterministic full-state dump + event log under `debug/` (S3); deterministic
  per-instruction CPU trace-export (S1); minimum inert DRAM/SRAM/VRAM wiring sufficient to
  run the parity program to its HALT checkpoint (S2); a real captured openMSX opcode-trace
  parity diff artifact `docs/m10-parity-trace-diff.md` with no fabricated claim (S4);
  evidence gates green with QA-executed build + **ctest 28/28**; and M9 blocker R5 resolved.
- The DEC-0001 R5 obligation is **discharged** by a genuine, QA-reproduced captured
  architectural-state empty diff (48 instructions, openMSX 19.1). M9's deferral is honored,
  not waived.
- M10 is **closeable** pending the human release decision. Residual risks R-1..R-4 are
  recommended follow-ups (notably a future broader zexdoc/zexall parity pass), none of
  which blocks closure.
