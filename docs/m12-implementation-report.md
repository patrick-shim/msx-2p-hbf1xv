# M12 Implementation Report — Z80A CPU End-to-End Parity Review & Hardening

- Milestone: M12 (REQ-M12-003), executed per `docs/m12-planner-package.md` (S1→S6).
- Authority: DEC-0003. Scope discipline: parity-HARDENING pass, not a rewrite.
- Date: 2026-07-06.
- Result headline: **all 6 planner gap-fixes landed; 45/45 ctest pass (twice, deterministic);
  ZERO regression across M1–M12; the 8 M11 timing oracles unchanged and green; a REAL openMSX
  19.1 A/B trace-diff = ARCHITECTURAL PARITY (empty).** ZEXALL/ZEXDOC honestly DEGRADED
  (no legally-sourceable binary in-environment). QA sign-off is a separate agent.

---

## 1. Gap-ID → fix mapping (production edits, all under `src/devices/cpu/`)

Every production edit maps to a concrete §3 gap ID from the planner package. No opcode-dispatch
restructuring; no datasheet T-state or `increment_refresh_register()` call-site changed.

| Gap | Item | Fix | File:site |
|-----|------|-----|-----------|
| #3, #35 | WZ/MEMPTR register + update rules | Added `std::uint16_t wz` to `Z80aRegisters`; wrote it at every fact-sheet §4 rule site (see §1.1) | `z80a_state.h`; `z80a_cpu.cpp` (many sites) |
| #4 | `BIT n,(HL)` X/Y from WZ hi | `execute_cb_prefixed`: `(HL)` case sources undoc X/Y from `wz>>8` (register BIT still uses value) | `z80a_cpu.cpp` `execute_cb_prefixed` |
| #5 | `BIT n,(IX/IY+d)` X/Y via WZ | DDCB/FDCB sets `wz = index+d` and sources X/Y from `wz>>8` (behavior-preserving) | `z80a_cpu.cpp` `execute_indexed_cb` |
| #20, #21 | SCF/CCF genuine-Zilog Q-latch | Added `q` to `Z80aRegisters` (set by `set_f`), `q_prev_` snapshot in CPU; rewrote `alu_scf`/`alu_ccf` to `X/Y = ((Q^F)|A)` bits 3/5 | `z80a_state.{h,cpp}`; `z80a_cpu.{h,cpp}` |
| #30 | RETI copies IFF2→IFF1 | `execute_ed_prefixed` RETxx: removed the `y!=1` guard so RETI also restores IFF1 (and sets WZ) | `z80a_cpu.cpp` |
| #31 | NMOS `LD A,I`/`LD A,R` P/V bug | `ld_a_ir`: clears P/V when an IRQ will be accepted at the next boundary (pending && IFF1) | `z80a_cpu.cpp` `ld_a_ir` |
| #34 | HALT refresh | **DECISION-GATED DEFER** (default per R-6): current behavior documented, unchanged, to avoid regressing the QA-signed `hbf1xv_cpu_step` oracle (22). See §5. |

### 1.1 WZ update sites implemented (grounded in openMSX `setMemPtr`, cited inline)

`LD A,(nn)`→nn+1; `LD (nn),A`→(A<<8)|((nn+1)&FF); `LD A,(BC/DE)`→rp+1; `LD (BC/DE),A`→(A<<8)|((rp+1)&FF);
`LD (nn),HL`/`LD HL,(nn)`→nn+1; `LD (nn),rp`/`LD rp,(nn)` (ED)→nn+1; `ADD HL,rr`/`ADD IX/IY,rr`→base+1;
`ADC/SBC HL,rr`→HL+1; `JP nn`/`JP cc` (even not taken)→nn; `CALL nn`/`CALL cc` (even not taken)→nn;
`JR`/`JR cc` taken/`DJNZ` taken→dest; `RET`/`RET cc` taken/`RETI`/`RETN`→popped PC; `RST`→target;
`EX (SP),HL`/`EX (SP),IX/IY`→exchanged value; every `(IX/IY+d)` access→index+d; `IN A,(n)`→((A<<8)|n)+1;
`OUT (n),A`→(A<<8)|((n+1)&FF); `IN r,(C)`/`OUT (C),r`→BC+1; `RRD`/`RLD`→HL+1; `CPI/CPD`→WZ±1
(and repeat→PC+1); `INI/IND`→BC±1 (before B dec); `OUTI/OUTD`→BC±1 (after B dec);
`LDIR/CPIR` mid-iteration→PC+1; IM1 accept→0x0038; IM2 accept→ISR vector.

### 1.2 Q-latch mechanism

`Z80aRegisters::set_f()` re-latches `q = new F` on every flag write (the single choke point).
`Z80aCpu::step()` snapshots `q_prev_ = regs().q` and clears `regs().q = 0` at the instruction
boundary, so any step that never writes F — including interrupt acceptance, `EX AF,AF'`, and
`POP AF` (which write AF wholesale, bypassing `set_f`) — leaves Q = 0 for the next instruction.
`alu_scf`/`alu_ccf` read `q_prev_`. This is the genuine-Zilog rule (fact-sheet §8, Patrik-Rak);
it deliberately diverges from openMSX's `(F|A)` OR-form (planner A-4/R-2).

### 1.3 Timing-invariant confirmation (planner §4 / R-5)

No datasheet `return` T-state was changed and no `increment_refresh_register()` call site was
added/removed. All WZ/Q/RETI/LD-A,I edits touch flags/IFF/internal-WZ only. The M1 wait stays
machine-owned; the CPU does not double-count it. Verified by the unchanged M11 oracles (§4).

---

## 2. Files added / changed

Production (`src/devices/cpu/`): `z80a_state.h`, `z80a_state.cpp`, `z80a_cpu.h`, `z80a_cpu.cpp`.

Tests added (`tests/`):
- `unit/devices/z80a_parity_undocumented_unit_test.cpp` (S1)
- `unit/devices/z80a_block_flags_unit_test.cpp` (S1)
- `unit/devices/z80a_16bit_daa_unit_test.cpp` (S1)
- `unit/devices/z80a_wz_memptr_unit_test.cpp` (S3)
- `unit/devices/z80a_scf_ccf_q_unit_test.cpp` (S4)
- `unit/devices/z80a_nmos_interrupt_unit_test.cpp` (S5)
- `integration/machine/hbf1xv_cpu_parity_integration_test.cpp` (S6)

Test edit: `unit/devices/z80a_cb_prefixed_unit_test.cpp` — the pre-existing
`Bit3MemHl` case encoded the OLD (incorrect) BIT-(HL) X/Y-from-value simplification
that gap #4 corrects; updated to assert X/Y sourced from the WZ high byte.

Tooling/docs: `tools/openmsx-cpu-parity.ps1` (new, extends `tools/openmsx-trace-parity.ps1`);
`docs/m12-parity-trace-diff.md` (captured); `docs/asset-checksums.txt` (regenerated);
CMake registration in `tests/CMakeLists.txt`.

---

## 3. Per-slice test evidence

- **S1 (regression floor, test-only):** `z80a_parity_undocumented` (SLL reg+(HL), IXH/IXL/IYH/IYL,
  DD/FD chaining last-wins + DD-NOP, ED-hole 2-NOP R+2, OUT (C),0=0, LD R,A bit7, R low-7 wrap
  bit7-frozen, DDCB no-R-tick), `z80a_block_flags` (full flag byte for LDI/LDD/CPI/CPD/INI/IND/
  OUTI/OUTD incl. the NMOS OUTI-affects-carry edge and the OUTD carry-clears case — closes the
  M9-S3 unasserted-flag gap), `z80a_16bit_daa` (ADD/ADC/SBC HL full flags incl. SBC N=1 and X/Y
  from the high byte; DAA after add AND subtract incl. borrow). All pass.
- **S2 (ZEX):** DEGRADED — see §6. No fabricated pass.
- **S3 (WZ):** `z80a_wz_memptr` — one vector per WZ update rule (raw `wz` assertion) plus the
  observable `BIT n,(HL)` and `BIT n,(IX+d)` X/Y-from-WZ-hi consequences, incl. a real
  `LD A,(nn)`→`BIT` predecessor chain. All pass.
- **S4 (SCF/CCF-Q):** `z80a_scf_ccf_q` — move-from-A after a flag op (incl. the F-has-Y/A-lacks-Y
  distinguisher that separates the Q-latch from the OR-form), OR-from-F after POP AF / EX AF,AF' /
  interrupt acceptance (Q=0), for both SCF and CCF (CCF H=old-carry checked). All pass.
- **S5 (NMOS edges):** `z80a_nmos_interrupt` — RETI copies IFF2→IFF1 (proves the #30 fix) and the
  IFF2-clear case; LD A,I and LD A,R P/V cleared by the NMOS bug (EI;LD A,I pattern) and the
  no-interrupt control (P/V=IFF2); EI;RET and EI;EI one-instruction-delay edges; DI/EI/RETN IFF
  matrix; bare IM1/IM2/NMI ack = 13/19/11 unchanged. All pass.
- **S6 (system-integration + A/B):** `hbf1xv_cpu_parity_integration` over the real M11 `SystemBus`
  — WZ→BIT via real reads; SCF-Q move-from-A (the A-4 divergence) end-to-end; OUTI to the real
  mapper port 0xFC with carry + I/O dispatch readback (0x9F); IM1 accept oracle **ack == 14**
  (datasheet 13 + one M1 wait — proves no double count), vector 0x0038, IFF1/IFF2 cleared, push
  order; RETI IFF1 restore + EI one-instruction delay across a real ISR; whole-program timing
  oracle **elapsed == 45** (40 datasheet + 5 M1 waits), run twice identically. All pass.

---

## 4. Evidence gates (actual captured output)

Run in order; all captured, none fabricated.

1. `tools/validate-assets.ps1` → `Asset validation result: True` (7 BIOS files incl.
   f1xvbios/ext/disk/kdr/kfn/mus/firm; ROM count 1: aleste.rom). Exit 0.
2. `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` → report written, SHA256
   (e.g. `f1xvbios.rom … 75adcdb4…`). Exit 0.
3. `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` → configure exit 0.
4. `cmake --build build --config Debug` → build exit 0 (only pre-existing C4819 codepage
   warnings on the CP949 console; zero errors).
5. `ctest --test-dir build -C Debug --output-on-failure` → **100% tests passed, 0 failed out of
   45**. Re-run for determinism → **100% passed, 0 failed out of 45**.

Suite count: 38 pre-M12 → **45** (six new CPU unit suites + one new system-integration suite).
Tests #21–#26 (unit) and #40 (`machine_hbf1xv_cpu_parity_integration_test`) are the new M12 suites.

### 4.1 openMSX A/B trace-diff (REAL, captured)

`tools/openmsx-cpu-parity.ps1` drove openMSX **19.1** (WSL, `/usr/bin/openmsx`; A-2 recorded) over
the RAM-only `tests/parity/z80_parity_checkpoint.bin` at base 0xC000, single-stepped 48
instructions A vs B. Result in `docs/m12-parity-trace-diff.md`:
**ARCHITECTURAL PARITY — EMPTY DIFF** across all 48 instructions on every architectural field
(PC, opcode, A, F + all flag bits, B/C/D/E/H/L, AF/BC/DE/HL, shadows, IX/IY/SP, I, R, IFF1/IFF2,
IM). Documented benign boundaries excluded from the field gate and proven elsewhere:
- **WZ (A-3):** not exposed by openMSX `reg` nor exported by our trace → structurally absent;
  proven by `z80a_wz_memptr` + the integration BIT-from-WZ path.
- **SCF/CCF X/Y (A-4):** genuine-Zilog Q-latch legitimately diverges from openMSX's `(F|A)`;
  gated on fact-sheet + `z80a_scf_ccf_q`, NOT the diff (the parity program avoids SCF/CCF). The
  Q-latch was NOT "fixed" to match openMSX.
- **Cycle/T-states (DP-4):** proven by the machine oracles, not the diff.

---

## 5. M11 timing oracles — unchanged and green (confirmed)

The 8 QA-signed M11 timing oracles were re-run and hold at their exact signed values with no code
change to any T-state or R-tick site:

- `hbf1xv_cpu_step_integration` → `elapsed_cycles() == 22`.
- `hbf1xv_ldir_program_integration` → `== 102`.
- `hbf1xv_indexed_program_integration` → `== 105`.
- `hbf1xv_interrupt_ack_integration` → IM2 `== 49`, IM0 `== 38`.
- `hbf1xv_debug_event_log_integration` (event-log golden), `hbf1xv_m11_parity_checkpoint_integration`,
  `hbf1xv_cb_program_integration` — all green, unchanged.

**HALT-refresh (#34) decision:** default DEFER (R-6). The current HALT idle-step (returns 4, no
R tick, 0 M1) is preserved so the signed `hbf1xv_cpu_step` oracle (22) stays valid. No ZEXALL/
openMSX evidence in this milestone demanded the change (ZEXALL degraded; the A/B program does not
idle in HALT-refresh territory). Recorded here; escalate to `decisions.md` only if future evidence
proves a real defect.

---

## 6. ZEXDOC/ZEXALL — honest DEGRADE (A-1 / R-3)

Assessed IN as the primary opcode/flag oracle, but **no legally-sourceable `zexdoc.com`/
`zexall.com` binary exists in this environment** (none under `tests/parity/` or anywhere in-tree;
no network to source one). Per the planner fallback (§5.3, A-1), the ZEX harness therefore
**degrades to "assessed, deferred"** and is NOT stood up; **no ZEXALL/ZEXDOC pass is claimed or
fabricated.** The parity evidence of record for M12 is instead:
- the S1 regression-floor + S3/S4/S5 targeted unit nets (deterministic, exact flag bytes),
- the S6 system-integration test over the real M11 bus, and
- the REAL openMSX 19.1 A/B trace-diff (empty architectural diff, §4.1).

If a ZEX binary is later legally placed under `tests/parity/`, record its SHA-256 in
`docs/asset-checksums.txt` and stand up the S2 harness to close this residual.

---

## 7. Deviations / assumptions (with verification actions)

- **A-1 (ZEX):** binary absent → degraded honestly (§6). *Verify later:* place a legal copy +
  checksum + stand up `z80_zex_system_test`.
- **A-2 (openMSX 19.1 vs 21.0 grounding):** recorded at run; documented Z80 behavior stable;
  empty A/B diff corroborates.
- **A-3 (WZ not A/B-comparable):** WZ excluded from the field-diff; proven via unit + integration.
- **A-4 (SCF/CCF-Q diverges from openMSX):** intentional; gated on fact-sheet + unit tests; the
  Q-latch was not altered to match openMSX.
- **R-4 (LD A,I/R bug is a mid-instruction race):** modeled at the instruction boundary
  (pending && IFF1 → P/V=0). In this instruction-atomic core the bug is observable only via the
  `EI; LD A,I` pattern (a pending+enabled IRQ without an EI-delay is serviced *before* the
  instruction). Documented in code and unit-proven; ZEXALL does not exercise the IRQ race.
- **#36 (interrupted-block mid-iteration X/Y):** final-state correct (proven); the mid-iteration
  WZ=PC+1 rule is implemented for LDIR/CPIR repeat, but the interrupted-mid case is not separately
  asserted (low priority; would be backstopped by ZEXALL when available).

---

## 8. QA handoff

- Milestone target: M12 REQ-M12-003 — Z80A parity hardening. Scope respected (fixes confined to
  `src/devices/cpu/`; every edit maps to a §3 gap ID; S1/S2 test-only).
- Gaps closed: #3, #4, #5, #20, #21, #30, #31, #35 (+ #6–#19/#22–#33 PRESENT items locked by the
  S1 net and the empty A/B diff). #34 decision-gated DEFER (documented).
- Tests: 7 new suites (6 unit + 1 system-integration); **45/45 ctest pass, twice, deterministic;
  zero M1–M12 regression.**
- M11 timing oracles: **unchanged and green** (22 / 102 / 105 / 49 / 38 + event-log golden + CB +
  m11-checkpoint).
- A/B: **REAL** captured openMSX 19.1 trace-diff = empty architectural diff (`docs/m12-parity-trace-diff.md`).
- ZEXALL/ZEXDOC: honestly **BLOCKED/degraded** (no legal binary) — NOT a fabricated pass.
- Open items for QA judgment: the ZEX residual (§6), the #34 defer decision (§5), and the R-4
  approximation (§7). No known defects.
