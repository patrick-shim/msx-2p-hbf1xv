# M12 QA Sign-off — Z80A CPU End-to-End Parity Review & Hardening

- Milestone: M12 (REQ-M12-004), authority DEC-0003 (auto-close-authorized on 100% pass / zero
  regression M1–M12 / QA Pass).
- QA Owner: MSX QA Agent. Date: 2026-07-06.
- Inputs reviewed: `docs/m12-planner-package.md`, `docs/m12-implementation-report.md`,
  `docs/m12-parity-trace-diff.md`; source `src/devices/cpu/z80a_cpu.cpp`,
  `z80a_state.{h,cpp}`, `z80a_cpu.h`; the 6 new unit suites + `hbf1xv_cpu_parity_integration_test`;
  `references/fact-sheets/Zilog Z80A CPU.md` (§4 lines 148–162, §5, §8 lines 255–259);
  `agent-protocol/state/milestones.md` (M12), `agent-protocol/channels/decisions.md` (DEC-0003).
- All findings below are from QA-executed builds/tests and QA-read source — not developer numbers.

---

## 1. Regression Scope

The parity-hardening pass touches only `src/devices/cpu/` (state adds `wz`/`q`; WZ writes at the
§4 rule sites; SCF/CCF/RETI/LD-A,I edits). Regression surface assessed:

- **CPU device suites** (unprefixed / CB / ED / DD-FD / DDCB-FDCB / interrupt modes / state /
  trace-export) — must be unchanged-green.
- **M11 machine timing oracles** (8 QA-signed cycle assertions) — the highest regression risk,
  because a stray T-state or `increment_refresh_register()` edit would silently break them.
- **All M0–M12 unit + integration + system suites** — the DEC-0003 zero-regression bar.
- **A/B parity vs openMSX** (behavior-affecting milestone → A/B evidence mandatory, guardrails).
- **Asset/build gates** (validate-assets, checksum, Debug build, ctest).

---

## 2. Regression Matrix Status

### 2.1 QA-executed evidence gates (re-run by QA, not quoted)

| Gate | QA result |
|------|-----------|
| `cmake --build build --config Debug` | Build OK, 0 errors (pre-existing CP949 C4819 codepage warnings only). |
| `ctest --test-dir build -C Debug` (run 1) | **100% passed, 0 failed out of 45.** |
| `ctest` (run 2, determinism) | **100% passed, 0 failed out of 45** — identical. |
| Skipped/disabled tests | **None.** ctest enumerates exactly 45 real tests; no ZEX/`DISABLED` test is registered in `tests/CMakeLists.txt`. The "100% of ALL tests" bar therefore equals 45/45 with nothing hidden as skipped. |

Suite count 38 (M11) → **45** (+6 CPU unit suites #21–#26, +1 system-integration #40). QA-executed
count matches the implementation report's claimed 45/45.

### 2.2 Per-gap genuine/correct verification (QA-read source)

Each flagged gap was read in source and confirmed to be a real, wired implementation — not a
declared-but-unused stub:

| Gap | Verdict | QA evidence (source) |
|-----|---------|----------------------|
| #3/#35 WZ/MEMPTR | **GENUINE** | `wz` field present `z80a_state.h:33`; **written at ~40 sites** in `z80a_cpu.cpp` covering every §4 rule: LD A,(nn)/(BC)/(DE) `:757,761,766`; LD (nn),A store-form `(A<<8)|…` `:729-735`; JP/CALL even-if-not-taken `:694,702,709`; JR/DJNZ taken → dest `:694`; (IX/IY+d) → index+d `:1517,1614,1635,1657`; IN/OUT (C) → BC+1 `:1350,1359`; block ops `:1213,1221,1264,1310`; RRD/RLD `:1367`; EX (SP),HL/IX `:922,1692`; IRQ accept → handler `:1789,1795`. Not vestigial — consumed by BIT below. |
| #4 BIT n,(HL) X/Y | **GENUINE** | `execute_cb_prefixed:1006-1008`: `undoc_source = is_memory ? (wz>>8) : value`; register BIT still uses the tested value. Matches §4. |
| #5 BIT n,(IX/IY+d) X/Y | **GENUINE** | `execute_indexed_cb`: `wz = address` (=index+d) `:1614`, then `alu_bit(y, value, wz>>8)` `:1727`. Sourced from WZ hi, consistent with #4. |
| #20/#21 SCF/CCF Q-latch | **GENUINE** | `alu_scf:573` / `alu_ccf:585`: `xy_source = (q_prev_ ^ f) | a`, masked to bits 3/5. Q latched by the single choke point `set_f()` (`z80a_state.cpp:67`); `step()` snapshots `q_prev_ = regs().q` then clears `regs().q = 0` `:48-49`, so a step that never writes F (incl. IRQ accept, and POP AF / EX AF,AF' which write AF wholesale bypassing `set_f`) leaves Q=0. |
| #30 RETI IFF | **GENUINE** | `execute_ed_prefixed:1387-1393`: the old `y!=1` guard is gone; both RETN and RETI `set_iff1(iff2())` and set WZ. Matches openMSX `retn()`≡`reti()`. |
| #31 LD A,I / LD A,R NMOS P/V bug | **GENUINE** | `ld_a_ir:1167-1168`: after copying IFF2→P/V, clears P/V when `maskable_interrupt_pending() && iff1()` (IRQ will be accepted next boundary). Documented instruction-boundary approximation (planner R-4); unit-proven. |
| #34 HALT refresh | **DEFERRED (documented)** | `step():58-59` halted case returns 4 with no fetch/R-tick/M1 — unchanged. Planner-sanctioned decision-gated defer (R-6). See §4. |

### 2.3 Two independent hand-checks (QA-derived from the fact-sheet)

Grounded in `references/fact-sheets/Zilog Z80A CPU.md` §8 lines 258–259 and §4 lines 148–154.

**Hand-check A — SCF Q-latch distinguisher (the A-4 case).** Precondition: the previous
instruction *was* flag-modifying and left `F=0x20` (Y set), with `A=0x00`. Because F has not
changed since, `Q=F=0x20`. Fact-sheet rule: `YF = ((Q^F)|A).5`, `XF = ((Q^F)|A).3`.
Compute `(Q^F)|A = (0x20^0x20)|0x00 = 0x00` → **X=Y=0**. The implementation yields
`(q_prev_ ^ f) | a = 0 → X/Y = 0` (matches). The openMSX `(F|A)` OR-form would give
`0x20|0x00 = 0x20` → Y=1 — a *legitimate* divergence. The S6 integration test #2 encodes exactly
this vector (`CP` sets F.Y with A=0, then SCF → X/Y=0), confirming genuine-Zilog behavior, not
openMSX's. **Matches fact-sheet.**

**Hand-check B — BIT n,(HL) X/Y from WZ hi.** Precondition: `LD A,(nn)` with `nn=0x27FF` →
`WZ = nn+1 = 0x2800`, so `WZ hi = 0x28` (`0b0010_1000`, bits 5 and 3 set). Fact-sheet: X/Y = bits
11 and 13 of WZ = bits 3 and 5 of the WZ high byte. Implementation: `undoc_source = wz>>8 = 0x28`,
masked `& kXY(0x28) = 0x28` → **X and Y both set** in F. **Matches fact-sheet** (bits 11/13 of
0x2800 are both 1).

### 2.4 PRESENT-item locking (S1 regression floor)

The 26 PRESENT rubric items are locked by the S1 unit nets. QA spot-verified the highest-risk one:
the **NMOS OUTI-affects-carry** edge is a real full-flag-byte assertion in
`z80a_block_flags_unit_test.cpp:166-169` (`F = N|H|C|PV = 0x17`, carry set from `k>0xFF`), closing
the M9-S3 unasserted-flag gap. `OUT (C),0 = 0` remains at `execute_ed_prefixed:1361`.

---

## 3. Failures and Risk Ranking

**No test failures.** 45/45 pass, twice, deterministic. Residual (non-failing) risks, ranked:

1. **HALT-R fidelity gap (#34) — LOW/accepted.** On real Zilog the R register increments during
   HALT (each internal cycle is an M1/NOP); the current core idles at 4T with no R-tick and no M1.
   The MSX BIOS HALTs every frame awaiting VBLANK, so R can drift versus hardware over long HALTs.
   *Impact bounded:* no M12 evidence (A/B program HALTs terminally; ZEXALL absent) exercises it,
   and the acceptance criteria do not require it. Not a blocker (see §5); recommended as a tracked
   forward item.
2. **ZEXALL/ZEXDOC not stood up — LOW/accepted.** No legally-sourceable binary in this offline
   environment; harness honestly degraded, no fabricated pass. The strongest cross-emulator flag
   oracle is therefore not exercised (see §5).
3. **LD A,I/R NMOS bug is an instruction-boundary approximation (R-4) — LOW.** Modeled at the
   boundary, not the true mid-instruction silicon race; unit-proven, ZEXALL does not test the race.
4. **openMSX 19.1 vs 21.0 grounding (A-2) — LOW.** Documented Z80 behavior stable across these;
   the empty A/B diff corroborates. Same runtime used in M10/M11 gates.

---

## 4. M11 Timing-Oracle Regression Confirmation

The 8 QA-signed M11 timing oracles are **unchanged and green** — QA verified both the asserted
constant in the test source and its passing status:

| Oracle (test) | Signed value | QA-confirmed |
|---------------|--------------|--------------|
| `hbf1xv_cpu_step_integration` | `elapsed == 22` | asserted `:67`, green |
| `hbf1xv_ldir_program_integration` | `== 102` | asserted `:75`, green |
| `hbf1xv_indexed_program_integration` | `== 105` | asserted `:73`, green |
| `hbf1xv_interrupt_ack_integration` | IM2 `== 49`, IM0 `== 38` | asserted `:75,:118`, green |
| `hbf1xv_debug_event_log_integration` | event-log golden | green |
| `hbf1xv_m11_parity_checkpoint_integration` | checkpoint | green |
| `hbf1xv_cb_program_integration` | CB program | green |

- **No datasheet T-state constant altered:** all WZ/Q/RETI/LD-A,I edits write flags/IFF/internal-WZ
  only; RETI still returns 14, IN/OUT (C) still 12, block ops unchanged.
- **`increment_refresh_register()` untouched** (`z80a_cpu.cpp:199-207`): still the single M1/R
  choke point (preserve bit7, low-7 wrap); no call site added or removed.
- **No double-count proven:** the S6 integration IM1-ack oracle asserts `ack_cycles == 14`
  (`hbf1xv_cpu_parity_integration_test.cpp:167`) = **13 datasheet + 1 machine M1 wait** — the CPU
  returns the bare 13 and the machine adds exactly one wait. The whole-program oracle asserts
  `== 45` (40 datasheet + 5 M1 waits) twice identically `:209`. The M1 wait stays machine-owned.

Because the M11 oracles encode exact cycle values, their passing *is* the regression proof: any
weakening of a T-state or R-tick would have failed them. **Zero regression across M1–M12.**

---

## 5. A/B Adversarial Validation (QA-reproduced)

QA did not trust the developer's captured artifact — QA **re-ran the harness independently**:

- `tools/openmsx-cpu-parity.ps1` drove **openMSX 19.1** (WSL `/usr/bin/openmsx`, version banner
  captured) over `tests/parity/z80_parity_checkpoint.bin` at base 0xC000, single-stepping 48
  instructions A (emulator) vs B (openMSX). Output to QA scratch (`build/qa_m12_A.txt` /
  `qa_m12_B.txt` / `qa_m12_diff.md`) so the developer artifact was not clobbered.
- **Result reproduced: ARCHITECTURAL PARITY — EMPTY DIFF over 48/48 instructions** on every
  architectural field (PC, opcode, A, F+flags, B/C/D/E/H/L, all 16-bit pairs, shadows, IX/IY/SP,
  I, R, IFF, IM). Both traces are real and populated (48 A records, 48 B `OMTRACE` records with
  evolving state, e.g. `af=2A00` at seq 5).
- **Comparator adversarially proven honest:** QA corrupted B seq=5 `af` to `DEAD` and re-diffed →
  the tool reported **"ARCHITECTURAL DIVERGENCE — 2 field mismatch(es)"** with exit code 1,
  printing the real emulator value (`af 2A00` vs `DEAD`). The empty clean diff is therefore a true
  match, not a rigged/empty comparison.
- **Program coverage** (QA-inspected opcodes): DI, IM1 (ED56), block LDI (EDA0), indexed IX/IY
  (DD/FD 21/34/7E/86, FD 21/36/7E), CB (RLC/SRL/BIT), CALL/RET, DJNZ/JR, and immediate ALU —
  exercising WZ, prefix chaining, BIT, and block paths end-to-end.

**Exclusions are sound, not masking a defect:**
- **SCF/CCF X/Y (A-4):** a *correct* genuine-Zilog NMOS Q-latch legitimately differs from openMSX's
  `(F|A)` OR-form (fact-sheet §8 lines 255–259 vs openMSX `CPUCore.cc`). Including SCF/CCF in the
  openMSX field-diff would flag a divergence where *our* behavior is the hardware-correct one.
  Excluding it from the openMSX diff and proving it against the fact-sheet Q rule via
  `z80a_scf_ccf_q_unit_test` + the S6 CP/SCF vector is the methodologically correct choice. The
  parity program deliberately contains **no** SCF/CCF opcode (QA-confirmed: no `0x37`/`0x3F`), so
  the architectural gate stays meaningful — the exclusion is not being used to hide a real
  divergence.
- **WZ/MEMPTR (A-3):** structurally not exposed by openMSX `reg` nor exported by our trace; proven
  by unit + integration instead. Legitimate.
- **Cycle/T-states (DP-4):** openMSX inserts MSX M1 waits; proven by the machine oracles (§4).

A/B outcome: **PASS — genuine, QA-reproduced empty architectural diff; comparator verified to
detect divergence.**

---

## 6. Explicit Residual Ruling vs the DEC-0003 Auto-Close Bar

The auto-close bar (DEC-0003): 100% pass of ALL unit + system-integration tests, ZERO regression
M1–M12, QA Pass. I rule on each residual against the **M12 acceptance criteria as written**
(`milestones.md` M12), not an idealized wish-list.

- **(a) ZEXALL/ZEXDOC honest degradation — DOES NOT BLOCK.** The M12 "Integration Tests Required"
  states a ZEXDOC/ZEXALL harness *"if feasible under the headless harness (otherwise the openMSX
  A/B trace-diff serves as the cross-check)."* ZEXALL-pass is explicitly **conditional**, with the
  A/B trace-diff named as the sanctioned fallback — which was delivered and QA-reproduced (§5). No
  ZEX test is registered/skipped in the suite, so the "100% of ALL tests / no skipped test"
  condition is fully met at 45/45. The degradation is honest (no fabricated pass), the binary is
  unavailable in this offline env, and the acceptance criteria do not make ZEXALL a hard closure
  gate. **Accepted residual.** Recommendation: if a legal ZEX binary is later placed under
  `tests/parity/`, checksum it into `docs/asset-checksums.txt` and stand up the harness as
  additional hardening.
- **(b) HALT-R (#34) deferral — DOES NOT BLOCK.** The acceptance criteria do not list HALT-refresh
  as a required item; the planner explicitly scoped it as a decision-gated default-defer (R-6)
  precisely because changing it would alter the QA-signed `hbf1xv_cpu_step` oracle (22) and thus
  requires a `decisions.md` entry — changing it *inside* M12 would itself risk a self-inflicted
  regression against a signed oracle. It is documented in the implementation report §5, not hidden.
  **Accepted residual.** Recommendation: raise a `decisions.md` entry to schedule HALT-R (with the
  oracle re-derivation) in a future CPU-timing hardening pass; it is a real fidelity gap but not an
  M12 closure gate.

**Neither residual blocks a clean Pass.** The milestone's stated acceptance criteria are fully met:
documented gap analysis with citations (planner §3), every identified gap implemented and unit-
covered (QA-verified genuine, §2.2), a full system-integration test over the M11 bus (#40), 100%
of the actual 45-test suite passing with zero M1–M12 regression, all evidence gates executed, and a
genuine QA-reproduced openMSX A/B trace-diff.

---

## 7. Sign-off Decision

**PASS.**

- QA-executed ctest: **45/45 passed, 0 failed, twice, deterministic; 0 skipped.**
- All flagged gaps (#3/#35, #4, #5, #20/#21, #30, #31) verified **genuine and correct** in source;
  two independent fact-sheet hand-checks matched.
- **Zero regression M1–M12:** the 8 M11 timing oracles unchanged and green; no datasheet T-state or
  `increment_refresh_register()` site altered; S6 proves the M1 wait is not double-counted
  (IM1 ack = 14 = 13 + 1).
- A/B parity: **QA-reproduced empty architectural diff** over 48/48 vs openMSX 19.1; comparator
  adversarially proven to detect divergence; SCF/CCF and WZ exclusions are sound, not masking.
- Residuals (ZEXALL honest-degrade; HALT-R defer) are **accepted, non-blocking** under the M12
  acceptance criteria as written.

**The DEC-0003 auto-close bar is met** (100% pass, zero regression M1–M12, QA Pass). M12 is eligible
for coordinator auto-close without a further human release decision.

**Non-blocking follow-ups (do not gate M12):**
1. Record a `decisions.md` entry scheduling HALT-R (#34) fidelity + the `hbf1xv_cpu_step` oracle
   re-derivation for a future CPU-timing hardening pass.
2. If a legally-sourced ZEXDOC/ZEXALL binary becomes available offline, checksum it and stand up
   the S2 harness as additional cross-emulator flag hardening.
3. The M11 forward-obligation (flip reset `#A8=0xFF → #A8=0` once ROMs populate slot 0) remains
   owned by **M13** (memory), not M12 — untouched here, correctly.
