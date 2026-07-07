# Milestones

Use one section per milestone.

## Milestone Template

- Milestone ID:
- Title:
- Spec Owner:
- Developer Owner:
- QA Owner:
- Scope:
- Acceptance Criteria:
- Unit Tests Required:
- Integration Tests Required:
- Regression Scope:
- Status: Planned | In Progress | Ready for QA | Done | Blocked

## M0

- Milestone ID: M0
- Title: Foundation and Protocol Alignment
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Define baseline architecture decomposition for core/devices/peripherals/machine/frontend/tests, lock deterministic test strategy skeleton, and align protocol artifacts to authoritative goals and build flow.
- Acceptance Criteria:
	- Planner specification for M0 includes subsystem boundaries and dependency map.
	- M0 plan references authoritative build flow for both headless and SDL3 configurations.
	- Risks and assumptions are explicitly captured and traceable.
	- First implementation slice candidates and test obligations are identified for M1 handoff.
- Unit Tests Required: Deterministic test naming and matrix plan defined for first implementation slices.
- Integration Tests Required: Initial cross-subsystem integration test plan drafted and prioritized.
- Regression Scope: Baseline regression risk catalog created for scheduler, bus contracts, and device integration points.
- Status: Planned

## M0 (Kickoff Refresh 2026-07-05)

- Milestone ID: M0
- Title: Foundation and Protocol Alignment
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Re-establish baseline architecture and deterministic planning package with explicit runtime directory references (`docs/`, `tools/`, `bios/`, `roms/`) and required evidence script paths.
- Acceptance Criteria:
	- Planner package defines subsystem boundaries and dependency map for current scaffold state.
	- Baseline build and test flow remains authoritative for headless and optional SDL3 paths.
	- Asset evidence requirements are listed: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`.
	- A/B evidence requirement is listed when behavior parity is in scope: `tools/openmsx-ab-smoke.ps1` and `docs/openmsx-ab-smoke.md`.
	- Risks and assumptions are captured with verification actions.
- Unit Tests Required: Existing deterministic unit suite is mapped to milestone plan and gap list.
- Integration Tests Required: Existing deterministic integration suite is mapped to milestone plan and gap list.
- Regression Scope: QA regression plan includes asset-dependent and openMSX A/B evidence handling where applicable.
- Status: Done
- Details: Planner package completed via `REQ-M0-003` with durable artifact `docs/m0-planner-package.md`; implementation-prep completed via `REQ-M0-005` at `docs/m0-implementation-prep.md` with fresh cycle evidence for `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, and `ctest --test-dir build -C Debug --output-on-failure` (`6/6` pass); QA signoff completed under `REQ-M0-006` with recommendation to close M0 documented in `docs/m0-qa-signoff.md`.

## M1 (Kickoff 2026-07-05)

- Milestone ID: M1
- Title: First Implementation Slice
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Define and execute first behavior-affecting implementation slice with deterministic-first contracts and evidence-gated validation.
- Acceptance Criteria:
	- Planner package defines first-slice boundaries and dependency sequencing across core/device/machine integration paths.
	- Developer implementation includes deterministic unit/integration coverage for the selected slice.
	- Evidence gates are executed and recorded: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, and `ctest --test-dir build -C Debug --output-on-failure`.
	- Conditional behavior parity workflow is defined using `tools/openmsx-ab-smoke.ps1` and `docs/openmsx-ab-smoke.md` when applicable.
- Unit Tests Required: New/updated deterministic unit tests for first-slice contracts.
- Integration Tests Required: New/updated deterministic integration tests covering first-slice cross-subsystem behavior.
- Regression Scope: QA regression-readiness and signoff required before M1 closure.
- Status: Done
- Details: Planner package completed under `REQ-M1-002` (`docs/m1-planner-package.md`); readiness and implementation completed under `REQ-M1-003`/`REQ-M1-004` with deterministic machine stepping slice (`docs/m1-implementation-report.md`); QA signoff completed under `REQ-M1-005` with closure recommendation (`docs/m1-qa-signoff.md`); evidence gates executed successfully including asset validation, checksum refresh, Debug build, and `ctest` `6/6` pass.

## M2 (Kickoff 2026-07-05)

- Milestone ID: M2
- Title: Scheduler Batch Stepping
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Add deterministic scheduler batch tick behavior and unit verification.
- Acceptance Criteria:
	- `Scheduler::tick_many` added and verified by deterministic tests.
	- Required evidence gates executed successfully.
- Unit Tests Required: Scheduler deterministic batch-step unit cases.
- Integration Tests Required: Existing integration suite must remain passing.
- Regression Scope: QA signoff required before closure.
- Status: Done
- Details: Completed through `REQ-M2-002`/`REQ-M2-003`/`REQ-M2-004` with artifacts `docs/m2-planner-package.md`, `docs/m2-implementation-report.md`, and `docs/m2-qa-signoff.md`.

## M3 (Kickoff 2026-07-05)

- Milestone ID: M3
- Title: CPU Bus Word Access
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Add deterministic 16-bit little-endian CPU bus client helpers and unit verification.
- Acceptance Criteria:
	- `read_word_le` and `write_word_le` implemented.
	- Unit tests validate normalized-address word semantics.
	- Required evidence gates executed successfully.
- Unit Tests Required: CPU bus client word access deterministic unit cases.
- Integration Tests Required: Existing integration suite must remain passing.
- Regression Scope: QA signoff required before closure.
- Status: Done
- Details: Completed through `REQ-M3-002`/`REQ-M3-003`/`REQ-M3-004` with artifacts `docs/m3-planner-package.md`, `docs/m3-implementation-report.md`, and `docs/m3-qa-signoff.md`.

## M4 (Kickoff 2026-07-05)

- Milestone ID: M4
- Title: Machine Multi-Frame Stepping
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Add deterministic multi-frame machine stepping API and integration verification.
- Acceptance Criteria:
	- `run_frames` and `frame_cycles_per_frame` implemented.
	- Integration tests verify frame/cycle deterministic behavior.
	- Required evidence gates executed successfully.
- Unit Tests Required: Existing unit suite must remain passing.
- Integration Tests Required: Machine deterministic multi-frame integration cases.
- Regression Scope: QA signoff required before closure.
- Status: Done
- Details: Completed through `REQ-M4-002`/`REQ-M4-003`/`REQ-M4-004` with artifacts `docs/m4-planner-package.md`, `docs/m4-implementation-report.md`, and `docs/m4-qa-signoff.md`.

## M5 (Kickoff 2026-07-05)

- Milestone ID: M5
- Title: Boot Invariant Hardening
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Strengthen deterministic boot-state invariants and expose frame metadata in headless output.
- Acceptance Criteria:
	- System boot invariants extended for cycle/frame reset behavior.
	- Headless output includes deterministic frame metadata.
	- Required evidence gates executed successfully.
- Unit Tests Required: Existing unit suite must remain passing.
- Integration Tests Required: Existing integration suite must remain passing.
- Regression Scope: QA signoff required before closure.
- Status: Done
- Details: Completed through `REQ-M5-002`/`REQ-M5-003`/`REQ-M5-004` with artifacts `docs/m5-planner-package.md`, `docs/m5-implementation-report.md`, and `docs/m5-qa-signoff.md`.

## M6 (Kickoff 2026-07-05)

- Milestone ID: M6
- Title: Scheduler Target Advance
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Add deterministic scheduler target-advance behavior with unit coverage.
- Acceptance Criteria:
	- `Scheduler::advance_to` implemented and deterministic.
	- Required evidence gates executed successfully.
- Unit Tests Required: Scheduler advance-to deterministic unit cases.
- Integration Tests Required: Existing integration suite must remain passing.
- Regression Scope: QA signoff required before closure.
- Status: Done
- Details: Completed through `REQ-M6-002`/`REQ-M6-003`/`REQ-M6-004` with artifacts `docs/m6-planner-package.md`, `docs/m6-implementation-report.md`, and `docs/m6-qa-signoff.md`.

## M7 (Kickoff 2026-07-05)

- Milestone ID: M7
- Title: CPU Bus Big-Endian Word Access
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Add deterministic big-endian word read/write helpers in CPU bus client with unit coverage.
- Acceptance Criteria:
	- `read_word_be` and `write_word_be` implemented and deterministic.
	- Required evidence gates executed successfully.
- Unit Tests Required: CPU bus client big-endian deterministic unit cases.
- Integration Tests Required: Existing integration suite must remain passing.
- Regression Scope: QA signoff required before closure.
- Status: Done
- Details: Completed through `REQ-M7-002`/`REQ-M7-003`/`REQ-M7-004` with artifacts `docs/m7-planner-package.md`, `docs/m7-implementation-report.md`, and `docs/m7-qa-signoff.md`.

## M8 (Kickoff 2026-07-05)

- Milestone ID: M8
- Title: Machine Target-Cycle Stepping
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Add deterministic machine run-until-cycle behavior with integration coverage.
- Acceptance Criteria:
	- `run_until_cycle` implemented with deterministic semantics.
	- Required evidence gates executed successfully.
- Unit Tests Required: Existing unit suite must remain passing.
- Integration Tests Required: Machine deterministic target-cycle integration cases.
- Regression Scope: QA signoff required before closure.
- Status: Done
- Details: Completed through `REQ-M8-002`/`REQ-M8-003`/`REQ-M8-004` with artifacts `docs/m8-planner-package.md`, `docs/m8-implementation-report.md`, and `docs/m8-qa-signoff.md`.

## M9 (Kickoff 2026-07-05)

- Milestone ID: M9
- Title: Full Z80A CPU Core (HB-F1XV MSX2+)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement a deterministic Z80A CPU core for Sony HB-F1XV with full interrupt architecture and complete opcode-family coverage for unprefixed/CB/ED/DD-FD/DDCB-FDCB paths.
- Acceptance Criteria:
	- CPU model includes complete register state (main/shadow pairs, IX/IY, I/R, IFF1/IFF2, IM0/1/2).
	- Interrupt architecture implemented with deterministic IM0/IM1/IM2/NMI semantics and RETN/RETI behavior.
	- Opcode-family coverage completed for unprefixed, CB, ED, DD/FD, and DDCB/FDCB paths.
	- Deterministic unit/integration/system tests validate instruction semantics and timing oracles.
	- Required evidence gates executed successfully (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`).
	- Behavior parity smoke evidence refreshed using `tools/openmsx-ab-smoke.ps1` and `docs/openmsx-ab-smoke.md`.
- Unit Tests Required: Deterministic CPU register, opcode, interrupt, and timing test suites.
- Integration Tests Required: Deterministic machine+CPU stepping and interrupt delivery suites.
- Regression Scope: Existing suites must remain passing; QA signoff required before closure.
- Status: Done
- Details: Planner package in `docs/m9-planner-package.md`. Implementation completed across five slices with green gates each cycle: baseline core + interrupt scaffold (REQ-M9-003), full unprefixed + CB families (M9-S2/REQ-M9-005), full ED family (M9-S3/REQ-M9-007), DD/FD + DDCB/FDCB indexed families with documented prefix-chaining (M9-S4/REQ-M9-008), and IM0/IM1/IM2/NMI + RETN/RETI interrupt fidelity (M9-S5/REQ-M9-009). Final QA sign-off REQ-M9-010 (`docs/m9-qa-signoff.md`) = CONDITIONAL PASS: QA-executed `ctest` `21/21` green; all opcode-family and interrupt-architecture acceptance criteria verified as genuine (non-stub) in source. Sole outstanding criterion R5 (openMSX opcode-trace parity) cannot be produced within M9 CPU-core scope (needs machine slot/RAM/SRAM/VRAM wiring + CPU trace-export). Per human decision `DEC-0001` (2026-07-06), R5 is DEFERRED to milestone M10 as an accepted, tracked risk. CLOSED by human release decision on 2026-07-06 (REQ-M9-011) on the strength of that deferral; R5 is now owned by M10. Correctness note: deferral is not a waiver — opcode-family and interrupt behavior are complete and QA-verified green.

## M10 (Kickoff 2026-07-06)

- Milestone ID: M10
- Title: Debug/Trace Infrastructure & openMSX Opcode-Trace Parity
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Deliver the baseline-scoped debug/trace/parity capability (`agent-protocol/project-baseline.md`, In-Scope) and resolve deferred M9 blocker R5 per decision `DEC-0001`. Includes: (a) full-state debug dump (CPU registers/state, DRAM, SRAM, VRAM) with execution-event logs under `debug/traces/` and `debug/logs/` (subfolders under `debug/` as needed for frames/events/audio); (b) a deterministic CPU trace-export facility; (c) machine slot/RAM/SRAM/VRAM wiring sufficient to run a comparable openMSX program to a known state; (d) an openMSX opcode-trace / per-instruction parity harness (extending `tools/openmsx-ab-smoke.ps1` or a dedicated `tools/` helper) that produces real, non-fabricated trace-diff evidence; (e) supporting `tools/` converters (e.g., memory->png, memory->audio) as needed.
- Acceptance Criteria:
	- Debug full-state dump and execution-event logging implemented and deterministic, writing under `debug/traces/` and `debug/logs/`.
	- CPU trace-export facility emits a deterministic, comparable per-instruction trace.
	- Machine slot/RAM/SRAM/VRAM wiring sufficient to boot/run the parity test program to a defined checkpoint.
	- openMSX opcode-trace parity harness produces an actual captured trace-diff artifact in `docs/openmsx-ab-smoke.md` (or a dedicated parity report under `docs/`); no parity result is claimed without a real diff.
	- Required evidence gates executed successfully (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`).
	- Resolves M9 R5; QA sign-off required before closure.
- Unit Tests Required: Deterministic tests for trace-export formatting/determinism and debug-dump content correctness.
- Integration Tests Required: Deterministic machine+trace integration covering the parity test program to its checkpoint.
- Regression Scope: Existing suites (including all M9 CPU suites) must remain passing; QA signoff required before closure.
- Status: Done
- Details: CLOSED by human release decision on 2026-07-06 (REQ-M10-009) on QA PASS (REQ-M10-008). Created by decision `DEC-0001` (2026-07-06) to own the debug/trace/parity scope and absorb deferred M9 blocker R5. Planner package REQ-M10-002 (`docs/m10-planner-package.md`). Slices delivered with green gates each cycle: M10-S1 CPU trace-export (REQ-M10-003), M10-S2 inert DRAM 64KB/VRAM 128KB/SRAM 8KB region wiring per Target Machine Specification (REQ-M10-004), M10-S3 full-state debug dump + execution logging under `debug/` (REQ-M10-005), M10-S4 openMSX opcode-trace parity harness (REQ-M10-006), M10-S5 `tools/` memory->png/audio converters (REQ-M10-007). Final QA sign-off REQ-M10-008 (`docs/m10-qa-signoff.md`) = PASS: QA-executed `ctest` 28/28, and QA independently RE-RAN `tools/openmsx-trace-parity.ps1` reproducing an empty per-instruction architectural-state diff (PC/opcode/all registers/flags) over the 48-instruction RAM-only parity program vs openMSX 19.1, adversarially validating the comparator (empty-side -> BLOCKED, corrupted field -> DIVERGENCE). R5 DISCHARGED with genuine, reproduced evidence (`docs/m10-parity-trace-diff.md`); DEC-0001 obligation satisfied. Non-blocking residuals: R5 bounded to a 48-instr RAM-only program (not a full zexall sweep) per the acceptance definition of record; cycle/T-state parity ungated (MSX M1 wait-states, DP-4); SRAM 8KB is an FM-PAC assumption (DP-3). Out-of-M10 prerequisites remain candidate separate milestones (DP-1 full slot/BIOS boot, DP-2 V9958 VDP, DP-3 FMPAC/SRAM device, DP-4 exact cycle timing, DP-5 full I/O bus). Closeable on human release decision.

## M11 (Kickoff 2026-07-06)

- Milestone ID: M11
- Title: S1985 "MSX-ENGINE" Chipset & Full System Bus
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Deliver the real HB-F1XV system bus and the Yamaha S1985 (MSX-SYSTEMII) engine glue, replacing the trivial flat-DRAM `MachineBus` in `src/machine/hbf1xv_machine.*`. Per DEC-0002 and the S1985 fact-sheet (`references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md`) grounded in openMSX (`references/openmsx-21.0/src/MSXS1985.*`, `DeviceFactory.cc`, `MSXCPUInterface.*`, `MSXMapperIO.*`, `MSXPPI.*`). In scope: (a) full memory bus with 4-page primary-slot decode (PPI port `#A8`) and `#FFFF` secondary/sub-slot decode; (b) I/O bus with device dispatch and the S1985 port mirrors `#98-9B`→`#9C-9F` and `#A8-AB`→`#AC-AF`; (c) the thin S1985 layer — 16-byte backup RAM on switched-I/O device ID `0xFE`, mapper-readback base `0x80`/mask `0x1F` (`100xxxxx`), and the +1 M1 opcode-fetch wait state; (d) system integration wiring the CPU/scheduler onto the new bus. PSG/YM2149 and RTC/RP5C01 device internals are OUT of scope (separate later milestones) — M11 provides only the bus/engine seams. Source placement under `src/` is the planner's call per `src/CLAUDE.md`.
- Acceptance Criteria:
	- Full memory bus with primary-slot (`#A8`) + secondary-slot (`#FFFF`) decode replaces the flat DRAM bus; CPU-visible reads/writes route through slot selection deterministically.
	- I/O bus dispatches by port with S1985 mirrors (`#9C-9F`, `#AC-AF`) modeled as aliases; unmapped I/O reads return the documented floating value.
	- S1985 layer: backup RAM (switched-I/O ID `0xFE`, 16 bytes, address/data/pattern/color regs per fact-sheet §6), mapper readback `100xxxxx`, and +1 M1 wait implemented and unit-verified.
	- System integration: `Hbf1xvMachine` boots and steps the CPU over the new bus with existing inert DRAM as the slot-3-0 backing; all prior suites remain green.
	- Evidence gates executed (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`).
	- A/B trace-diff evidence captured vs openMSX (real diff, no fabricated parity) via `tools/openmsx-ab-smoke.ps1`/parity harness → `docs/`.
- Unit Tests Required: Deterministic tests for slot decode (primary/secondary), I/O dispatch + mirrors, mapper readback pattern, backup-RAM switched-I/O, and M1-wait timing oracle.
- Integration Tests Required: Deterministic machine+bus stepping over slot-decoded memory; CPU program run to a checkpoint through the real bus.
- Regression Scope: All M0–M10 suites must remain passing; QA sign-off required before closure.
- Status: Done
- Details: CLOSED by human release decision on 2026-07-06 (REQ-M11-005); tagged git `v1.0.11`. Planner package `docs/m11-planner-package.md` (REQ-M11-002). Implementation REQ-M11-003 delivered all 6 slices (S1 bus-contract+CPU I/O seam+M1-cycle; S2 memory slot-decode #A8/#FFFF; S3 I/O dispatch + S1985 mirrors #9C-9F/#AC-AF; S4 thin S1985 backup-RAM ID 0xFE + mapper readback 100xxxxx + M1-wait; S5 system integration replacing MachineBus; S6 A/B parity) — `docs/m11-implementation-report.md`. QA sign-off REQ-M11-004 (`docs/m11-qa-signoff.md`) = PASS: QA-executed ctest 38/38 (+10 new), no regression (8 R-3 timing oracles recomputed to datasheet+M1-wait, hand-verified not weakened), and A/B parity independently REPRODUCED by QA — genuine EMPTY trace-diff over 15/15 real records vs openMSX 19.1 Sony_HB-F1XV with real <S1985> (`docs/m11-parity-trace-diff.md`, `build/qa_m11_A.txt`/`qa_m11_B.txt`). Forward obligation now owned by the MEMORY milestone (renumbered to M13 by DEC-0003): flip reset slot default #A8=0xFF -> #A8=0 (slot-0 BIOS) once ROMs populate slot 0 (R-1/A-2). Backup-RAM .sram persistence accepted out-of-scope (A-5/R-6). NOTE: after M11 closure the human inserted a new M12 (Z80A CPU parity review, DEC-0003) and renumbered memory->M13, VDP->M14; execution order is M11(done) -> M12(CPU parity) -> M13(memory) -> M14(VDP).

## M12 (Kickoff 2026-07-06)

- Milestone ID: M12
- Title: Z80A CPU End-to-End Parity Review & Hardening (HB-F1XV Zilog NMOS)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Comprehensive review of the current Z80A CPU implementation (`src/devices/cpu/*`, built in M9) against `references/fact-sheets/Zilog Z80A CPU.md` and openMSX (`references/openmsx-21.0/src/cpu/CPUCore.cc`, `CPURegs.cc`, `Z80.hh`), establishing end-to-end behavioral + timing parity for a genuine Zilog NMOS Z80A. Per DEC-0003: gap analysis -> plan -> implement any deltas -> full system integration test. Parity rubric (fact-sheet §4/§5/§8): undocumented X/Y (F3/F5) flags incl. 16-bit high-byte source; WZ/MEMPTR tracking for `BIT n,(HL)`/`(IX+d)` and every updating op; block-instruction flag quirks (`LDI/CPI/INI/OUTI` families incl. the NMOS `OUTI`-affects-carry edge); SCF/CCF Q-latch (`((Q^F)|A)` bits 5/3 rule); `SLL` (`CB 30-37`); `IXH/IXL/IYH/IYL`; DD/FD prefix chaining + NONI; ED holes as 2-byte NOPs; full-table `DAA`; R register low-7-bit increment (prefix/block rules, bit7 frozen); NMOS `OUT (C),0` = 0; NMOS `LD A,I`/`LD A,R` interrupt P/V bug; EI one-instruction delay; DI/EI/RETI/RETN IFF semantics; IM0/IM1/IM2 + NMI; MSX M1 wait (+1/opcode fetch, +1/prefix — owned at machine level per M11) and the intrinsic I/O wait; interrupt-acknowledge timing (IM1 13T bare). src placement stays under `src/devices/cpu/`; the planner may reorganize/add files (flag tables, undocumented-op modules) with best judgment per `src/CLAUDE.md`. This is a parity-hardening pass, NOT a rewrite (scope control: no gratuitous refactor).
- Acceptance Criteria:
	- Documented gap analysis mapping EACH fact-sheet parity item to current-impl status (present / absent / divergent) with concrete citations (our `file:line` vs `references/...` path).
	- Every identified gap implemented and covered by a deterministic unit test grounded in the fact sheet / openMSX behavior.
	- Full SYSTEM INTEGRATION test: the CPU exercised end-to-end through the M11 system bus (not isolated), covering the parity-critical behaviors.
	- 100% pass of ALL unit + system-integration tests with ZERO regression across the M1–M12 suites (standing human close-authorization condition, DEC-0003).
	- Evidence gates executed (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`).
	- A/B trace-diff vs openMSX (real captured diff) exercising the parity-critical opcode/flag/timing behaviors; NO parity claim without a genuine diff.
- Unit Tests Required: Deterministic tests for undocumented X/Y flags, WZ/MEMPTR (`BIT n,(HL)`/`(IX+d)`), block-instruction flag quirks (incl. `OUTI` carry), SCF/CCF Q-latch, `SLL`, `IXH/IXL/IYH/IYL`, `DAA` table, R-register increments, `OUT (C),0`=0, `LD A,I/R` NMOS bug, EI-delay, RETI/RETN IFF, and IM/NMI timing oracles.
- Integration Tests Required: CPU-over-M11-bus system integration exercising the above end-to-end; a ZEXDOC/ZEXALL-style parity harness if feasible under the headless harness (otherwise the openMSX A/B trace-diff serves as the cross-check).
- Regression Scope: ALL M1–M12 unit + system-integration suites must pass 100% with zero regression. QA sign-off required. Per DEC-0003, closure is AUTHORIZED on that exact condition (100% pass, zero regression M1–M12, QA Pass) without a further human release decision; otherwise escalate.
- Status: Done
- Details: CLOSED by coordinator under the DEC-0003 standing auto-close grant on 2026-07-06 (REQ-M12-005); tagged git `v1.0.12`. Planner package `docs/m12-planner-package.md` (REQ-M12-002): 36-item gap analysis (26 PRESENT, 4 DIVERGENT, 5 ABSENT, 1 UNVERIFIED). Implementation REQ-M12-003 (`docs/m12-implementation-report.md`) closed all target gaps as a parity-hardening pass confined to `src/devices/cpu/`: #3/#35 WZ/MEMPTR register + all §4 update sites; #4/#5 BIT n,(HL)/(IX+d) X/Y from WZ hi byte; #20/#21 genuine-Zilog SCF/CCF Q-latch X/Y=((Q^F)|A) bits 5/3; #30 RETI copies IFF2->IFF1; #31 NMOS LD A,I/R P/V interrupt bug. #34 HALT-R DEFERRED (planner default, protects the signed cpu_step oracle; tracked as DEC-0004). QA sign-off REQ-M12-004 (`docs/m12-qa-signoff.md`) = PASS: QA-executed ctest 45/45 (+7 suites), ZERO regression (8 M11 timing oracles unchanged/green; no datasheet T-state or increment_refresh_register() site altered; S6 integration proves IM1 ack=14 with no M1-wait double-count), gaps hand-verified genuine/correct, A/B parity QA-REPRODUCED (empty diff 48/48 vs openMSX 19.1 + adversarial corruption test confirming a true match) — `docs/m12-parity-trace-diff.md`. Residuals ruled NON-BLOCKING: ZEXALL/ZEXDOC honestly degraded (no legally-sourceable binary offline; acceptance made ZEX "if feasible", A/B fallback delivered) and HALT-R #34 defer (not a required acceptance item). DEC-0003 auto-close condition met.

## M13 (Renumbered 2026-07-06 by DEC-0003; was M12)

- Milestone ID: M13
- Title: RAM / ROM Memory Devices & Slot Population (VRAM deferred to M14)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement the CPU-addressable memory devices and populate the M11 slot map per the Target Machine Specification and the openMSX HB-F1XV layout (`references/openmsx-21.0/src/memory/`, S1985 fact-sheet §4/§9): 64 KB main RAM as a memory-mapper device (openMSX slot 3-0, `#FC-#FF` segment registers, S1985 512 KB drive limit / HB-F1XV 64 KB population), and the ROM set — 32 KB BIOS/BASIC, 16 KB SUB-ROM (MSX-BASIC V3.0), 16 KB KANJI (KANJI BASIC + KANJI ROM), 16 KB DISK ROM. Per DEC-0002 the 128 KB VRAM is OWNED BY THE VDP and is OUT of this milestone's scope (delivered in M14). Wire these devices into the M11 slots/bus; asset loading uses local `bios/`/`roms/` (verified paths only — no fabricated provenance). Carries the M11 forward-obligation (R-1/A-2): flip the bring-up reset slot default `#A8=0xFF` -> `#A8=0` (slot-0 BIOS) once ROMs populate slot 0.
- Acceptance Criteria:
	- Memory-mapper RAM device (64 KB) with `#FC-#FF` segment registers, wired into slot 3-0, replaces the inert DRAM region as the CPU RAM backing.
	- ROM devices for BIOS/BASIC, SUB, KANJI, DISK implemented and mapped to their correct slots/pages per the HB-F1XV layout; missing local assets handled deterministically (documented, no fabrication).
	- System integration: CPU can fetch/execute from ROM and read/write mapper RAM through the M11 bus; boot reaches a defined checkpoint.
	- Evidence gates executed (validate-assets, checksum, Debug build, ctest all green).
	- A/B trace-diff vs openMSX over a CPU-visible RAM/ROM program (real diff; VRAM parity excluded, it is M14).
- Unit Tests Required: Deterministic mapper segment-register decode/readback, ROM read-only enforcement, slot-page mapping, and asset-absence handling tests.
- Integration Tests Required: Deterministic boot/run through ROM+mapper-RAM over the M11 bus to a checkpoint.
- Regression Scope: All prior suites remain passing; QA sign-off required before closure.
- Status: Done
- Details: CLOSED by human release decision on 2026-07-06 (DEC-0006 / REQ-M13-005); tagged git `v1.0.13`. Planner package `docs/m13-planner-package.md` (REQ-M13-002) derived the exact slot map from `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`. Implementation REQ-M13-003 (`docs/m13-implementation-report.md`) delivered slices S1..S5: new `src/devices/memory/{rom_device,memory_mapper_ram}.*` + `src/machine/rom_asset_loader.*`; slot population (BIOS 0-0, RAM mapper 3-0, SUB+Kanji 3-1, DISK 3-2, FM-MUSIC 3-3; slots 0 & 3 expanded); reset-flip to authentic #A8=0 so the machine BOOTS real BIOS; retired `ram_slot_backing.*` (M11 MapperIo stays sole owner of #FC-FF + 100xxxxx readback). QA sign-off REQ-M13-004 (`docs/m13-qa-signoff.md`) = PASS: QA-executed ctest 50/50 (+5), zero regression (updated tests authentic not weakened — OUTI #FC->#FD retarget + debug_dump A-5 golden spot-checked genuine; M12 CPU golden unchanged), boot checkpoint genuine (self-derived golden, PC advances 0x0000->0x043C through real BIOS), all 5 BIOS SHA1s match openMSX 'confirmed by Meits' revisions, both A/B subjects empty diff QA-reproduced with comparator failure-mode checks (`docs/m13-parity-trace-diff.md`). Deferred out-of-scope, tracked: FM-PAC internals+SRAM, Kanji-font I/O #D8-DB, Halnote/MSX-JE firmware, cartridge loading, FDC drive mechanics, VRAM/VDP (M14). Per user directive, M13 retains the NORMAL human-release-decision gate — autopilot PAUSES here awaiting the human release decision (+ tag v1.0.13) before M14.

## M14 (Renumbered 2026-07-06 by DEC-0003; was M13)

- Milestone ID: M14
- Title: Yamaha V9958 VDP (incl. 128 KB VRAM ownership)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement the Yamaha V9958 Video Display Processor for the HB-F1XV, grounded in `references/fact-sheets/Yamaha V9958 VDP.md` and openMSX (`references/openmsx-21.0/src/video/`). Per DEC-0002 the VDP OWNS the 128 KB VRAM (the inert `vram_` placeholder migrates out of the machine into the VDP). In scope: VDP register file, status registers, VRAM read/write via ports `#98/#99` (with the S1985 `#9C-9F` mirror from M11), palette (`#9A`) and indirect-register (`#9B`) ports, the documented screen/graphic modes from the Target Machine Specification, and system integration onto the M11 I/O bus. Rendering/timing depth to be sliced by the planner. The V9958 is the most complex chip in the system — the planner MUST decompose into deterministic slices.
- Acceptance Criteria:
	- V9958 device owns 128 KB VRAM; CPU VRAM access flows only through ports `#98/#99` (+`#9C/#9D` mirror); no CPU-addressable VRAM region remains in the machine.
	- Register/status/palette/indirect-register port behavior implemented and unit-verified against the fact-sheet.
	- Documented screen/graphic modes represented per the Target Machine Specification (palette = 512 colors, 16 concurrent in palette modes; YJK/YAE max on-screen 19,268 — NOT a palette dimension).
	- System integration onto the M11 I/O bus; prior suites remain green.
	- Evidence gates executed (validate-assets, checksum, Debug build, ctest all green).
	- A/B trace-diff vs openMSX covering VDP port/VRAM/register behavior (real diff, no fabricated parity).
- Unit Tests Required: Deterministic VDP register/status decode, VRAM address auto-increment via `#98/#99`, palette/indirect-register writes, and mode-selection tests.
- Integration Tests Required: Deterministic CPU→VDP port sequence over the M11 bus producing verifiable VRAM/register state; mirror-port equivalence.
- Regression Scope: All prior suites remain passing; QA sign-off required before closure.
- Status: Done
- Details: CLOSED by human release decision on 2026-07-06 (DEC-0007 / REQ-M14-005); tagged git `v1.0.14`. Closes deferred-backlog item B9. Planner package `docs/m14-planner-package.md` (REQ-M14-002) set the contract-level scope and deferred rendering/sprite/command/timing/YJK/effects/planar DEPTH to deferred-backlog rows D1-D7. Human reviewed the plan and approved continuation (2026-07-06). Implementation REQ-M14-003 (`docs/m14-implementation-report.md`) delivered slices S1..S6: new `src/devices/video/{v9958_vdp,vdp_vram,vdp_mode,irq_line}` — the 128 KB VRAM MIGRATED out of the machine into the VDP (no CPU-addressable VRAM region remains; reachable only via #98/#99 + #9C/#9D mirror); #98 data path (17-bit addressing, auto-increment, read-ahead, shared latch), #99 two-write register/address protocol, #9B indirect+AII, #9A palette, mode decode, S#0..S#9, VBlank/line interrupts wired to the M12 IM1 accept path via a thin IrqLine adapter (M12 accept logic unchanged). QA sign-off REQ-M14-004 (`docs/m14-qa-signoff.md`) = PASS: QA-executed ctest 56/56 (+6), zero regression (memory_regions repointed to machine.vdp().vram() authentically; debug-dump golden unchanged), interrupt seam verified (exactly one accept, /INT released on S#0 read, no re-trigger loop), openMSX A/B empty diff QA-reproduced with VRAM read-back now genuinely compared (`docs/m14-parity-trace-diff.md`), D1-D7 boundary held. Closes deferred-backlog item B9 (VRAM/VDP). Per the normal gate, autopilot PAUSES here awaiting the human release decision (+ tag v1.0.14) before any further milestone.

## M15 (Kickoff 2026-07-06)

- Milestone ID: M15
- Title: Device Integration Completion & S1985 Chipset Full Wiring
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- North-star Goal (per DEC-0008): complete the device integrations so the machine is a fully-wired device set, with the Yamaha S1985 chipset's own constituent devices REALLY implemented (not just seams). M11 delivered the S1985 as a thin engine layer exposing seams only (PSG #A0-A2, RTC #B4/B5); M15 replaces those seams with real device implementations and completes the remaining device integrations drawn from `agent-protocol/state/deferred-backlog.md`.
- Scope (to be finalized by the planner + human review): candidate IN — B1 PSG/YM2149 internals + stereo + joystick ports, B2 RTC/RP5C01 internals (gated by #F5 bit7), C4 S1985 backup-RAM .sram persistence, C6 keyboard-matrix + joystick input (PPI port B/C + PSG port A/B), and — planner to recommend IN vs sequence-later — B3 FM-PAC/OPLL YM2413, B4 MSX-JE SRAM, B5 Kanji-font I/O #D8-DB, B6 Halnote firmware, B7 cartridge loading, B8 FDC drive mechanics, C7 printer/cassette. Candidate DEFERRED to follow-on milestones (planner to confirm): D1-D7 VDP rendering/sprite/command/timing depth, C1-C2 exact cycle timing, C3 ZEXALL sweep, C8 Sony speed/pause + Ren-Sha, C9 SDL3 frontend. The planner MUST consult the ENTIRE backlog, recommend each item IN/DEFERRED with justification, and PROPOSE a deterministic decomposition (M15 + follow-on milestone sequence) — "all pending items" is too large for one deterministic milestone.
- Acceptance Criteria (planner to detail): each IN device implemented to production spec grounded in its fact-sheet + openMSX reference, wired into the M11 system bus / S1985 chipset, with deterministic unit + system-integration tests and a real openMSX A/B trace-diff where behavior-affecting; zero regression across M1-M14; the S1985 chipset presents its real devices (no remaining seam-only stubs for the IN set); every backlog item touched status-updated same-cycle.
- Unit/Integration Tests Required (planner to detail per device).
- Regression Scope: all M1-M14 suites remain green; QA sign-off required before closure.
- Cross-milestone changes: DEC-0008 authorizes the planner to propose add/modify/re-arrange of M1-M14 implementation where required for clean integration; any such change goes through the ledger (decisions/requests/responses/milestones/backlog).
- Status: Done
- Scope (LOCKED per DEC-0009): IN — B1 PSG/YM2149 (numeric/register-accurate sample model, no audio presentation), B2 RTC/RP5C01 (in-memory deterministic epoch, no file persistence), C6 full i8255 PPI + keyboard matrix + joystick, C4 S1985 16-byte backup-RAM `.sram` persistence, C5 boot-checkpoint ADVANCE (deterministic PC/instruction checkpoint past first-device reads, A/B-matched to openMSX — NOT a BASIC prompt). Slices S1 PSG · S2 joystick+PSG ports · S3 RTC · S4 PPI+keyboard · S5 backup-RAM persistence · S6 integration+boot-checkpoint+A/B. Required M1-M14 changes X1-X5 (DEC-0008; X4 device-time advanced READ-ONLY off elapsed_cycles(), CPU T-state math untouched to protect M9/M12 oracles). Follow-on: FDC prioritized to M16 (next). All other backlog rows DEFERRED per DEC-0009 sequence.
- Details: Planner package `docs/m15-planner-package.md` (REQ-M15-002); human reviewed 2026-07-06 and answered the 6 scope questions -> DEC-0009 (scope locked). Human gave go-ahead (RESP-M15-002b). Implementation REQ-M15-003 (`docs/m15-implementation-report.md`) delivered S1-S6: new src/devices/audio/psg_ym2149.* (numeric YM2149), src/peripherals/{joystick,keyboard_matrix}.*, src/devices/rtc/rp5c01.* + src/devices/chipset/system_control.* (#F5 gate), src/devices/chipset/ppi_8255.* (full i8255 composing PpiSlotSelect verbatim); s1985_engine .sram persistence; machine wiring X2-X5. QA sign-off REQ-M15-004 (`docs/m15-qa-signoff.md`) = PASS: QA-executed ctest 64/64 (+8), zero regression (X4 CPU timing untouched — M9/M12 oracles green unchanged; X1 #A8 preserved byte-for-byte, M11/M13 slot guards reused), openMSX A/B empty diff 15/15 QA-reproduced (substantive PSG R0=0x34/R7=0x80), boot-checkpoint advanced ~0x043C -> PC 0x454/max 0x488 over 4096 instr (deterministic, self-derived golden). Backlog updated: B1/B2/C4/C6 -> DONE (M15), C5 -> IN-PROGRESS (partial; full boot needs FDC/M16). Assumptions (deterministic, tested): #F5 gate reset-enabled 0x80 for openMSX parity; RTC fixed 1988-01-01 epoch (DEC-0009). CLOSED by human release decision on 2026-07-06 (DEC-0010 / REQ-M15-005); tagged git `v1.0.15`. Closes deferred-backlog rows B1/B2/C4/C6; C5 advanced (partial, full boot needs FDC/M16). Next: M16 (FDC).

## M16 (Kickoff 2026-07-06)

- Milestone ID: M16
- Title: FDC Drive Mechanics (Fujitsu MB89311 + 720 KB 3.5" Floppy)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement the HB-F1XV floppy disk controller — Fujitsu MB89311 FDC + the built-in 720 KB 3.5" drive — and wire it onto the DISK ROM already mapped at slot 3-2 (M13), so the Disk-ROM boot handshake can proceed. Per DEC-0009/DEC-0010; closes deferred-backlog B8; advances C5 (full boot). Grounded in `references/fact-sheets/FDC for Sony HB-F1XV.md` and openMSX `references/openmsx-21.0/src/fdc/`. Planner to scope the FDC register/command model + drive/disk-image handling into deterministic slices, decide src/ placement (candidate `src/devices/fdc/`), define the disk-image handling (deterministic; no host-disk nondeterminism), and set the boot-checkpoint advance target. Normal human-release-decision gate (no auto-close).
- Acceptance Criteria (planner to detail): MB89311 FDC device implemented to spec + wired into the M11 io_bus / slot-3-2 DISK ROM path; disk-image read/seek/status behavior deterministic; boot advances past the M15 checkpoint through the Disk-ROM handshake to a defined checkpoint; deterministic unit + system-integration tests; real openMSX A/B trace-diff (no fabricated parity); zero regression M1-M15; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M15 suites remain green; QA sign-off required before closure.
- Status: Done. Planner `docs/m16-planner-package.md` -> developer `docs/m16-implementation-report.md` (REQ-M16-003, ctest 72/72, real openMSX A/B in `docs/m16-parity-trace-diff.md`, two genuine WD2793 bugs found+fixed via A/B probing: HEAD_LOADED semantics, Type-IV i2 index-pulse INTRQ scheduling) -> QA `docs/m16-qa-signoff.md` (RESP-M16-004) = PASS, independently reproduced by QA (ctest 72/72 fresh, bug-fix citations verified against `references/openmsx-21.0/src/fdc/WD2793.cc`, no test weakened, raw A/B trace files independently diffed). CLOSED by human release decision on 2026-07-07 (DEC-0011 / REQ-M16-005); tagged git `v1.0.16`. Closes deferred-backlog row B8 (FDC drive mechanics, DONE). C5 honestly left IN-PROGRESS, NOT force-closed: PC advances far past the M15 checkpoint (max PC 0x7D6F vs 0x488) but the real, unattended BIOS auto-boot path never pages the FDC into page 1 (confirmed to 20,000,000 instructions) — QA judged this non-blocking for M16 closure since the planner package pre-authorized this exact fallback and the FDC device itself is independently A/B-verified correct; carried forward as a condition for whichever future milestone claims C5 fully closed. Next: M17 (indicative: FM-PAC/OPLL + MSX-JE SRAM per DEC-0009), not yet kicked off.

## M17 (Kickoff 2026-07-07)

- Milestone ID: M17
- Title: FM-PAC (Yamaha YM2413 OPLL, `MSXMusic`-style) Register-Accurate Device + Reusable Battery-Backed-SRAM Primitive
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement the FM-PAC sound-generation device (Yamaha YM2413/OPLL, 9-channel FM synthesizer, per Target Machine Spec SOUND), register-accurate, wired to the real I/O ports `#7C/#7D` alongside the M13 FM-MUSIC ROM at slot 3-3 (deferred-backlog B3). Per the human directive "go ahead and kick off M17 and M18 until QA sign-off. Ensure to review deferred items and have them properly addressed during the development." Grounded in `references/fact-sheets/Yamaha YM2413 FM Chip.md` and openMSX `references/openmsx-21.0/src/sound/YM2413*.{cc,hh}`. Numeric/register-accurate model per the M15 PSG precedent (no audio presentation — deferred to a future audio-rendering milestone paired with SDL3/C9, tracked as new backlog row E1). **Scope correction (DEC-0012, coordinator-approved after independent re-verification):** the "MSX-JE 16 KB SRAM" (B4) does NOT belong to this device — the real HB-F1XV's slot-3-3 sound device (`MSXMusic`) has no SRAM at all; the 16KB SRAM belongs to the Halnote-mapped MSX-JE firmware ROM at slot 0-3 (backlog B6). M17 instead builds a reusable, standalone, deterministic `BatteryBackedSram` primitive (generalizing the M15 S1985 `.sram` pattern) for B6 to consume later — NOT wired to any slot this milestone. Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope).
- Acceptance Criteria: YM2413 device implemented to spec (64-register file, two-port `#7C/#7D` protocol, per-channel + rhythm decode, 15+3-entry ROM instrument patch table, debug register readback) wired into the M11 io_bus_ alongside the unmodified M13 `fmmusic_rom_`; reusable `BatteryBackedSram` primitive built + unit-tested standalone at 16KB (not wired to any slot); deterministic unit + integration tests; real openMSX A/B trace-diff (or honest BLOCKED for the register-introspection subject if unverifiable); zero regression M1-M16; explicit review of the full deferred-backlog (all ~19+ rows, not just B3/B4) with each re-affirmed with justification; QA sign-off before closure.
- Unit/Integration Tests Required: per `docs/m17-planner-package.md` §3 (S1-S5 slice plan).
- Regression Scope: all M1-M16 suites remain green, including the `fmmusic_rom_` slot-3-3 read-path regression guard; QA sign-off required before closure.
- Status: Done. Planner `docs/m17-planner-package.md` -> developer `docs/m17-implementation-report.md` (REQ-M17-002, ctest 75/75, real openMSX A/B in `docs/m17-parity-trace-diff.md` — both subjects genuine empty-diff, none BLOCKED) -> QA `docs/m17-qa-signoff.md` (RESP-M17-003) = PASS, independently reproduced (ctest 75/75 fresh, device-identity correctness verified against `MSXMusic.hh/cc` with zero `MSXFmPac`-style code found, `fmmusic_rom_` regression guard confirmed byte-for-byte unchanged, `BatteryBackedSram` confirmed NOT wired per DEC-0012, full openMSX A/B harness independently re-run end-to-end against real WSL openMSX 19.1 with an extra spot-check beyond the developer's own). CLOSED by human release decision on 2026-07-07 (DEC-0013 / REQ-M17-003); tagged git `v1.0.17`. Closes deferred-backlog row B3 (FM-PAC/YM2413 device contract, DONE). B4 (MSX-JE SRAM) correctly left OPEN, re-owned to B6 per DEC-0012 — NOT force-closed; reusable `BatteryBackedSram` primitive built standalone for B6 to consume later. New rows E1 (DSP/audio-synthesis depth) and E2 (write-timing constraint) added. Residual risks all Low, pre-disclosed, none blocking. Next: M18 (peripheral I/O), closed separately (see below).
