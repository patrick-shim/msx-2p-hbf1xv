# M9 QA Signoff (REQ-M9-004)

- Milestone ID: M9
- Source Request: REQ-M9-004
- Status: Partial

## Verdict

M9 is not ready to close yet.

The delivered CPU slice is stable and deterministic for covered behaviors, but the milestone target was full Z80A opcode-family completion and full interrupt-architecture fidelity, which is not yet met.

## Verified Positives

- New CPU core and state model compile and integrate cleanly.
- Deterministic tests for current slice are passing.
- Evidence gates passed in current cycle.
- System bootstrap defaulting to IM1 is covered.

## Blocking Gaps for Closure

1. Opcode-family completeness gap:
- Missing complete CB family.
- Missing complete ED family.
- Missing DD/FD and DDCB/FDCB families.
- Broad unprefixed instruction coverage is incomplete.

2. Interrupt architecture fidelity gap:
- IM0 and IM2 acknowledge/vector source behavior remains simplified relative to full target.

3. Parity evidence depth gap:
- openMSX artifact is currently smoke-level metadata, not milestone-specific parity assertion evidence.

## Recommended Closure Slices

1. Complete all remaining unprefixed opcodes with deterministic flag/timing tests.
2. Complete CB family with bit/shift/rotate behavior and timing tests.
3. Complete ED family including block and extended operations.
4. Complete DD/FD and DDCB/FDCB indexed families.
5. Expand IM0/IM2 interrupt fidelity and add stronger parity-oriented evidence.

## Signoff Recommendation

- Milestone state: In Progress.
- Closure recommendation: Defer closure until opcode-family and interrupt-fidelity gaps are resolved.

---

# M9 QA Re-Assessment After Slice M9-S2 (REQ-M9-006) — 2026-07-06

- Source Request: REQ-M9-006
- Scope Under Review: M9-S2 addendum (full unprefixed table + full CB family)
- Verdict: **Partial** (M9 remains open; S2 accepted as a quality slice)

## 1. Regression Scope

- Affected component: `src/devices/cpu/z80a_cpu.cpp` (decode-driven unprefixed table,
  CB dispatch, ALU/rotate/shift/bit helpers), `src/devices/cpu/z80a_state.h`
  (added `kFlagY` / `kFlagX` undocumented-flag constants), `src/devices/cpu/z80a_cpu.h`.
- New tests: `tests/unit/devices/z80a_unprefixed_ops_unit_test.cpp` (45 vectors),
  `tests/unit/devices/z80a_cb_prefixed_unit_test.cpp` (18 vectors),
  `tests/integration/machine/hbf1xv_cb_program_integration_test.cpp`.
- Regression surface: full deterministic suite (14 registered targets), all prior CPU/
  scheduler/bus/machine/boot suites must remain green.
- Asset-dependent flows: none newly introduced; asset gates re-verified for continuity.

## 2. Regression Matrix Status (independently re-run, not restated)

- Debug build (`cmake --build build --config Debug`): PASS (all 14 test targets + core +
  headless linked; verified in this cycle).
- `ctest --test-dir build -C Debug --output-on-failure`: **14/14 passed, 0 failed**
  (verified by QA in this cycle, matches developer's claim).
- Determinism: re-ran ctest a second time -> identical `14/14` result; no flakiness observed.
- Test quality review: the two new unit suites assert full flag bytes (S,Z,Y,H,X,P/V,N,C),
  exact T-state counts including taken vs not-taken conditional variants, memory effects,
  and PC/SP outcomes. They are table-driven with per-case setup/check lambdas and reset the
  CPU per case; they are NOT trivial placeholders. The CB integration test uses a bounded
  guard loop (no wall-clock) and a hardcoded 89-T-state oracle. All conform to
  `tests/CLAUDE.md` determinism and naming conventions.
- Undocumented-flag coverage is genuinely exercised (CP X/Y from operand, BIT X/Y from
  tested value, SLL bit-0 set, DAA edge, ADD HL X/Y from result high byte).

## 3. Failures and Risk Ranking

No test failures. Residual M9 acceptance gaps, risk-ranked:

- R1 (Blocker, High): ED full family absent. `execute_ed_prefixed` handles only
  RETN/RETI/IM0/IM1/IM2; every other ED opcode returns a silent 8T no-op. Missing: block
  ops (LDIR/LDDR/CPIR/CPDR/INIR/OTIR...), 16-bit `ADC/SBC HL,rp`, `NEG`, `IN/OUT (C)`,
  `LD A,I` / `LD A,R` / `LD I,A` / `LD R,A`, `RRD/RLD`, `LD (nn),rp` / `LD rp,(nn)`.
- R2 (Blocker, High): DD/FD indexed families absent. `execute_unprefixed` x=3/z=5 default
  path returns a 4T no-op for the DD/FD prefix, so IX/IY instructions execute the FOLLOWING
  byte as an unprefixed opcode. This mis-executes silently rather than trapping — a
  correctness hazard for any IX/IY-using code (including BIOS paths).
- R3 (Blocker, High): DDCB/FDCB indexed bit/rotate family absent (depends on R2).
- R4 (Major, Medium): IM0/IM2 interrupt-acknowledge fidelity remains simplified. IM0 shares
  the IM1 fixed vector (0x0038); IM2 reads `(I<<8 | 0xFF)` with no device-supplied vector
  bus. Deterministic but not architecture-faithful for device-driven acknowledge.
- R5 (Major, Medium): Milestone-specific opcode-trace parity vs openMSX not produced.
  `docs/openmsx-ab-smoke.md` confirms openMSX 19.1 availability/version + A/B inputs only,
  and the report itself states behavioral parity requires trace comparison artifacts. No
  parity waiver exists in `agent-protocol/channels/decisions.md` (channel is template-only).
- R6 (Minor, Low): `IN/OUT` port data is a deterministic open-bus stub (0xFF read, discarded
  write); timing/PC correct, data backing deferred to a later milestone.

## 4. Required Fixes (for M9 closure)

1. Implement the full ED family with block-op timing and flag oracles (S3).
2. Implement DD/FD (and DDCB/FDCB) indexed families with displacement + timing coverage;
   until then IX/IY silent no-op behavior is a latent correctness risk (S4).
3. Raise IM0/IM2 acknowledge/vector fidelity toward architecture-correct behavior (S5).
4. Produce milestone-specific opcode-trace parity evidence vs openMSX, or record an explicit
   waiver in `agent-protocol/channels/decisions.md` (S5).

## 5. Sign-off Decision

- **Partial.** M9-S2 is accepted as a correct, deterministic, well-tested slice and the
  regression suite is fully green (14/14, verified twice). However, M9 acceptance requires
  complete opcode-family coverage (ED + DD/FD + DDCB/FDCB), full interrupt-architecture
  fidelity (IM0/IM2), and milestone parity evidence — none of which are met. M9 stays
  **In Progress**. Recommended next slice: **M9-S3 (full ED family)**, then S4 (indexed
  families) and S5 (interrupt fidelity + parity evidence).

---

# M9 FINAL QA Sign-off (REQ-M9-010) — 2026-07-06

- Source Request: REQ-M9-010
- Scope Under Review: FINAL milestone closure verdict for M9 after slices M9-S1..M9-S5
  (REQ-M9-003/005/007/008/009).
- Verdict: **Conditional Pass** — closeable ONLY upon an approved `decisions.md`
  waiver/deferral for the R5 opcode-trace parity criterion. Absent that waiver, M9 stays
  In Progress.

## 1. Regression Scope

Full Z80A CPU core across all opcode families and interrupt architecture:
`src/devices/cpu/z80a_state.{h,cpp}`, `src/devices/cpu/z80a_cpu.{h,cpp}`
(1588 lines), and the machine integration in `src/machine/hbf1xv_machine.{h,cpp}`.
Regression surface = the full deterministic suite (21 registered targets incl. 9 z80a
unit suites and 6 machine integration suites), which must all remain green. Asset-dependent
flows unchanged since S2; asset gates re-verified for continuity.

## 2. Regression Matrix Status (independently executed by QA this cycle)

- `cmake --build build --config Debug`: PASS. All 21 test executables plus core and
  `sony_msx_headless` linked (observed build output, not restated).
- `ctest --test-dir build -C Debug --output-on-failure`: **21/21 passed, 0 failed**
  (QA-executed and observed this cycle — NOT restated from the developer's 21/21 claim).
- Determinism: re-ran ctest a second time -> identical **21/21**, no flakiness.
- Source spot-check (verified, not assumed):
  - Opcode families are genuinely dispatched, not stubbed: `execute_cb_prefixed` (line 836),
    `execute_ed_prefixed` (line 1158, full x/y/z/p/q decode incl. block ops, ADC/SBC HL,rp,
    NEG, IN/OUT (C), LD A,I/R, RRD/RLD, LD (nn),rp), `execute_indexed` +
    `execute_indexed_opcode` (lines 1340/1360, DD/FD with prefix chaining and (IX+d)
    displacement), and `execute_indexed_cb` (line 1505, DDCB/FDCB with register-copy variants).
  - Interrupt fidelity (`service_maskable_interrupt`, line 1548) genuinely implements:
    IM0 executes the device-driven bus opcode in place, floating-bus 0xFF -> RST 38h, 13T;
    IM1 fixed 0x0038, 13T; IM2 vector-table address `(I<<8)|bus_vector`, 16-bit LE ISR lookup,
    19T. `service_nmi` (line 1539) vectors 0x0066, clears IFF1, retains IFF2. RETN
    (line 1197) restores IFF1<-IFF2; RETI (y==1) does not. IM decode via `kImTable`.
  - Corresponding test suites exist and pass: `z80a_unprefixed_ops_unit_test`,
    `z80a_cb_prefixed_unit_test`, `z80a_ed_prefixed_unit_test`, `z80a_indexed_unit_test`,
    `z80a_indexed_cb_unit_test`, `z80a_interrupt_modes_unit_test`, plus machine integration
    for CB/LDIR/indexed/interrupt-ack programs.

## 3. Acceptance-Criteria Disposition (milestones.md lines 206-212)

- Complete register state (main/shadow, IX/IY, I/R, IFF1/IFF2, IM0/1/2): **MET**.
- Interrupt architecture IM0/IM1/IM2/NMI + RETN/RETI, deterministic: **MET** (R4 resolved,
  verified in source + tests).
- Opcode-family coverage unprefixed/CB/ED/DD-FD/DDCB-FDCB: **MET** (R1/R2/R3 resolved,
  verified in source + tests).
- Deterministic unit/integration/system tests validate semantics + timing: **MET** (21/21).
- Required evidence gates executed successfully: **MET** (build + ctest QA-verified;
  validate-assets/checksum executed per developer report + prior-cycle QA continuity).
- Behavior parity smoke evidence refreshed (`tools/openmsx-ab-smoke.ps1` /
  `docs/openmsx-ab-smoke.md`): **PARTIALLY MET / OUTSTANDING**. The report was refreshed
  and now includes a real (non-fabricated) openMSX 19.1 capability probe, but it is
  availability/capability-level only — it is NOT an opcode-trace parity assertion. This is
  the R5 gap.

## 4. Failures and Risk Ranking

No test failures. Prior blockers R1 (ED), R2 (DD/FD), R3 (DDCB/FDCB), R4 (IM0/IM2 fidelity)
are all RESOLVED and independently verified. Remaining:

- **R5 (Blocker for closure, Major): opcode-trace parity vs openMSX — UNRESOLVED.**
  `docs/openmsx-ab-smoke.md` is honest availability + capability-probe evidence, not a
  state-comparable instruction trace diff. The developer's documented blocker is credible
  and reproducible: (1) openMSX executes only within a full MSX slot/subslot/mapper memory
  map (page 0xC000 reads back 0xFF/unmapped at reset), which is exactly the machine wiring
  M9 excludes; (2) the headless `-script` debugger `step` does not advance PC
  (`step_advanced=0`); (3) this emulator has no per-instruction trace-export facility. No
  parity diff is asserted (anti-fabrication — correct handling). **No approved R5 waiver
  exists in `agent-protocol/channels/decisions.md` (confirmed template-only/empty).**
- R6 (Minor, Low): Port-I/O and block-I/O data are deterministic open-bus stubs
  (timing/PC/counter/memory correct); explicitly deferred to a later I/O-bus milestone.
  Non-blocking for M9.
- R7 (Minor, Low): block-I/O and indexed-BIT undocumented-flag precision not yet verified
  against zexall/zexdoc (depends on the same trace/self-check harness as R5). Non-blocking.
- Note (informational): RETI does not copy IFF2->IFF1; some references restore it. Modeled
  deliberately, documented, and test-asserted; MSX uses IM1/IM2 so this is not a closure
  risk.

## 5. Sign-off Decision

- **Conditional Pass.** Every M9 acceptance criterion is met and independently verified
  EXCEPT milestone-specific opcode-trace parity (R5), which has a credible, reproducible,
  honestly-documented technical blocker rather than a hidden defect. The regression suite
  is fully green and deterministic (21/21, verified twice this cycle).
- **What is required to mark M9 Done:** an explicit approved entry in
  `agent-protocol/channels/decisions.md` that either (a) grants an R5 opcode-trace-parity
  waiver scoping true trace parity to a later machine-wiring milestone, or (b) defers R5 to
  a dedicated trace-harness slice. Upon that recorded decision, M9 is closeable; the
  opcode-availability A/B smoke evidence already on file satisfies the guardrail's
  "review openMSX A/B evidence OR explicit waiver" condition once the waiver exists.
- Absent that decision, M9 remains **In Progress** — R5 is the sole outstanding item.
