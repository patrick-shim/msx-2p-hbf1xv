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
## M18 (Kickoff 2026-07-07)

- Milestone ID: M18
- Title: Peripheral I/O — Kanji-Font ROM Access (`#D8-#DB`) + Printer (Centronics `#90/#91`) + Cassette Interface
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement I/O-port access to the 256KB Kanji font ROM (`bios/f1xvkfn.rom`, confirmed present) via `#D8-#DB` (deferred-backlog B5 — M13 mapped only the Kanji *driver* at slot 3-1, not the I/O-accessed font itself), plus the printer (Centronics `#90/#91`) and cassette interfaces (backlog C7, baseline scope, never built). Per the human directive "go ahead and kick off M17 and M18 until QA sign-off. Ensure to review deferred items and have them properly addressed during the development." Grounded in `references/openmsx-21.0/src/MSXKanji.{hh,cc}`, `MSXKanji12.{hh,cc}`, `references/openmsx-21.0/src/{Printer,PrinterPortSimpl,MSXPrinterPort,PrinterPortDevice}.*`, `references/openmsx-21.0/src/cassette/{CassetteDevice,CassettePort,CassettePlayer}.*`, and `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` for this machine's exact wiring. Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope).
- Acceptance Criteria (planner to detail): Kanji font I/O device implemented + wired at `#D8-#DB`, reading `bios/f1xvkfn.rom` deterministically; printer port device implemented + wired at `#90/#91` (deterministic output capture, no host-printer/OS dependency); cassette interface implemented + wired (deterministic, no host-audio dependency); deterministic unit + integration tests; real openMSX A/B trace-diff; zero regression M1-M17; explicit review of the full deferred-backlog with each row re-affirmed with justification; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M17 suites remain green; QA sign-off required before closure.
- Status: Done. Planner `docs/m18-planner-package.md` -> developer `docs/m18-implementation-report.md` (REQ-M18-002, ctest 79/79, real openMSX A/B in `docs/m18-parity-trace-diff.md` — all three subjects genuine PASS, none BLOCKED) -> QA `docs/m18-qa-signoff.md` (RESP-M18-003) = PASS, independently reproduced (ctest 79/79 fresh, Kanji device identity verified against `MSXKanji.cc` with zero `MSXKanji12`-style code found, `ppi_8255.*` and CPU-stepping code confirmed byte-for-byte untouched, `JoystickPorts` regression guard confirmed genuine and additive, Kanji content-parity claim independently reproduced THREE ways including a live WSL openMSX 19.1 Tcl probe). CLOSED by human release decision on 2026-07-07 (DEC-0014 / REQ-M18-003); tagged git `v1.0.18` (committed separately from M17 per the human's explicit instruction). Closes deferred-backlog rows B5 (Kanji-font I/O) and C7 (printer + cassette). New rows F1 (cassette tape-format depth) and F2 (printer rendering depth) added under a new Section E. Residual risks all Low, pre-disclosed, none blocking.

## M19 (Kickoff 2026-07-07)

- Milestone ID: M19
- Title: Cartridge Loading — External Primary Slots 1 & 2
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement cartridge ROM loading into the HB-F1XV's two external primary slots (deferred-backlog B7). Confirmed via `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:119,121`: `<primary external="true" slot="1"/>` and `<primary external="true" slot="2"/>` — each a single, unexpanded external slot. M13 left both reserved open-bus; the sample asset `roms/aleste.rom` (2MB) is present for bring-up/testing. Per the human directive "move on into M19." Grounded in `references/openmsx-21.0/src/CartridgeSlotManager.{hh,cc}`, `references/openmsx-21.0/src/memory/{RomFactory,MSXRomCLI,RomTypes,RomInfo}.{hh,cc}`, and the common mapper-type family (`RomPlain`, `RomGeneric8kB`/`16kB`, `RomAscii8kB`/`16kB`, `RomKonami`, `RomKonamiSCC`, etc.) — planner to survey and decide MVP mapper-type scope vs. deferred depth. Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope).
- Acceptance Criteria (planner to detail): cartridge ROM loading implemented for external slots 1 & 2 with CLI/config specification of the ROM file (+ mapper type where not auto-detectable); at least the most common MSX mapper types supported (planner to scope which); deterministic unit + integration tests; real openMSX A/B trace-diff where applicable; zero regression M1-M18; explicit review of the full deferred-backlog with each row re-affirmed with justification; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M18 suites remain green; QA sign-off required before closure.
- Status: Done. Planner `docs/m19-planner-package.md` -> developer `docs/m19-implementation-report.md` (REQ-M19-002, ctest 91/91, real openMSX A/B in `docs/m19-parity-trace-diff.md`, content-bearing empty diff with empirically-verified slot-lettering) -> follow-up round (RESP-M19-003, per human note about the newly-added `roms/metalgear.rom`): identified as a genuine Konami-mapper "Metal Gear" (1987) dump via exact SHA1 match to `references/openmsx-21.0/share/softwaredb.xml`, plus live WSL openMSX corroboration — added as a second mechanical smoke fixture; ctest 92/92 -> QA `docs/m19-qa-signoff.md` (RESP-M19-004) = PASS, independently reproduced (ctest 92/92 fresh, bank-resolution mask fallback-only semantics and the Konami mirror-quirk both verified byte-exact against `RomBlocks.cc`/`RomKonami.cc` directly, both real-file fixtures' disposition discipline confirmed, `slot_expanded(1/2)==false` regression guard confirmed, CLI failure-policy isolation from `RomAssetLoader` confirmed). One non-blocking doc-comment staleness found by QA (`cartridge_konami_rom.h` still had the disproven "slots 0/1/2 all fixed" claim) — fixed by the coordinator post-QA (comment-only, ctest re-confirmed 92/92 after the fix). CLOSED by human release decision on 2026-07-07 (DEC-0015 / REQ-M19-003); tagged git `v1.0.19`. Closes deferred-backlog row B7 (cartridge loading, DONE). New rows G1 (KonamiSCC + SCC chip), G2 (auto-detection), G3 (runtime hot-plug), G4 (long-tail mapper types) added under Section F, all OPEN.

## M20 (Kickoff 2026-07-07)

- Milestone ID: M20
- Title: Halnote / MSX-JE Firmware Mapping (Slot 0-3) + MSX-JE 16 KB Battery-Backed SRAM
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement the Halnote-mapped MSX-JE firmware ROM at slot 0-3 (deferred-backlog **B6**) TOGETHER WITH wiring the M17-built `BatteryBackedSram` primitive as its real 16 KB SRAM consumer (deferred-backlog **B4**, re-owned to B6 per DEC-0012) — per the human's explicit directive to close both together, not defer B4 again. Confirmed present: `bios/f1xvfirm.rom` (1 MB). Confirmed via `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:105-115`: slot 0, secondary 3, `<mappertype>Halnote</mappertype>`, `<sramname>hb-f1xv_msx-je.sram</sramname>`. Grounded in `references/openmsx-21.0/src/memory/RomHalnote.{hh,cc}` — a genuinely complex mapper (1MB ROM as 128×8KB banks, last 512KB additionally addressable as 256×2KB sub-banks; 4 main bank-switch registers with SRAM-enable and sub-mapper-enable bits; 2 sub-bank-switch registers that shadow part of main bank 1 when enabled). Per the human directive "go ahead with M20. ensure that all the backlogs related M20 and up to M20 are all addressed and tested." Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope).
- Acceptance Criteria (planner to detail): Halnote mapper implemented byte-exact per `RomHalnote.cc`, wired at slot 0-3 (all 4 pages), reading the real `bios/f1xvfirm.rom`; the M17 `BatteryBackedSram` primitive instantiated and wired as the mapper's real 16KB SRAM (sram-enable bit gating `0x0000-0x3FFF`), with deterministic load/save persistence mirroring the M15 S1985 `.sram` pattern; deterministic unit + integration tests; real openMSX A/B trace-diff; zero regression M1-M19; explicit review of the full deferred-backlog with each row re-affirmed with justification; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M19 suites remain green; QA sign-off required before closure.
- Status: Done. CLOSED by human release decision on 2026-07-07 (DEC-0017 / REQ-M20-004); tagged git `v1.0.20`. Closes deferred-backlog rows B4 AND B6 (both DONE, M20).
- Details: Planner package `docs/m20-planner-package.md` (RESP-M20-001/REQ-M20-002) -> developer implementation `docs/m20-implementation-report.md`: new `src/devices/halnote/halnote_rom.{h,cpp}` (`HalnoteRom final : public core::MemoryDevice`, byte-exact per `RomHalnote.{hh,cc}`, composing the M19 `CartridgeRomWindow` verbatim for the main 8-slot window + the M17 `BatteryBackedSram` verbatim for the real 16 KB SRAM store, the bit-0x80 double-duty enable/bank-number effect on bank-2/bank-3 preserved exactly, the 0x7000-0x7FFF sub-bank shadow scoped precisely away from 0x6000-0x6FFF); wired at primary slot 0, secondary slot 3, all 4 pages in `Hbf1xvMachine` (additive-only edits to `hbf1xv_machine.{h,cpp}`), loading the real `bios/f1xvfirm.rom`; `halnote()`/`set_halnote_sram_path()`/`halnote_sram_path()`/`flush_halnote_sram()` machine API mirroring the M15 S1985 backup-RAM precedent exactly (no new CLI flag, matching that established scope). ctest 95/95 green (92 prior M1-M19 + 3 new: `devices_halnote_rom_unit_test`, `devices_halnote_subbank_unit_test`, `machine_hbf1xv_m20_halnote_integration_test`), zero regression, independently reproduced by both the coordinator and QA. Real openMSX A/B evidence (`docs/m20-parity-trace-diff.md`): 11/14 probed labels PARITY (main bank-switch banks 2/4/5 incl. the bit-0x80 double-duty effect; SRAM enable/disable/read/write), 3/14 DIVERGENCE isolated to the sub-mapper-shadow-enable effect on the installed openMSX 19.1 runtime — reported honestly, not swept under PARITY. QA (`docs/m20-qa-signoff.md`, RESP-M20-003) = **PASS**: independently reproduced ctest 95/95 fresh; cross-checked byte-exact semantics directly against `RomHalnote.{hh,cc}` line-by-line; confirmed genuine reuse (no parallel reimplementation); confirmed the BIOS-ROM-at-slot-0-0 regression guard is non-vacuous; independently recomputed the new tests' slot-routing arithmetic, confirming it is free of the DEC-0016-class bug; confirmed SRAM persistence round-trips across two independent machine instances; confirmed determinism; confirmed the full 34-row deferred-backlog review. On the A/B divergence, QA independently re-ran the live WSL openMSX probe itself and went further than the developer by reading the raw firmware bytes at the predicted fallback offset — a decisive, byte-for-byte match proving the divergence is the REFERENCE RUNTIME (openMSX 19.1) not engaging the shadow read for this access pattern, not a defect in this project's own `HalnoteRom`. QA's independent judgment: non-blocking, does not gate sign-off. Both B4 and B6 confirmed READY to close together. Awaiting the separate human release decision (normal gate, no auto-close per DEC-0003's M12-only scope).

## M21 (Kickoff 2026-07-07)

- Milestone ID: M21
- Title: VDP Rendering Depth — Pixel Pipeline, YJK/YAE Color Decode, Scroll/Interlace/Blink, G6/G7 Planar Interleave
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement deferred-backlog rows **D1** (pixel-accurate raster rendering pipeline: per-mode VRAM->framebuffer for all Target-Spec modes TEXT1/2, G1-G7, MULTICOLOR, border, blink, per-scanline output), **D5** (YJK/YJK+YAE color decode + 15-bit DAC output for SCREEN 10/11/12), **D6** (horizontal scroll visual effect R#25/26/27 + interlace/EO fields, blink timing, superimpose/digitize, external video), **D7** (G6/G7 VRAM address interleave in the display path — scope boundary with the command-engine-path interleave, M22/D3, to be resolved explicitly by the planner). M14 (closed, v1.0.14) delivered the VDP register/VRAM/status/interrupt CONTRACT with zero pixel output; this is the first milestone to add an actual rendering/pixel-output path, which must be deterministic and testable without the not-yet-built SDL3 frontend (C9, M26). Per the human directive (2026-07-07) "let's do M21 - M23 ... like we run autopilot until m23 ... deliberate cross check across the entire system as we go" — first of a three-milestone run, each with its own planner package, developer implementation, dedicated system integration test, QA sign-off, and separate tag. Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope), but pre-authorized to proceed through the release-decision/tag step for this specific three-milestone run without an additional pause, UNLESS QA does not reach a clean PASS (real blocker -> stop and consult the human).
- Acceptance Criteria (planner to detail): deterministic pixel/framebuffer output for every Target-Spec graphics/text mode, byte-exact per `references/openmsx-21.0/src/video/` grounding; YJK/YAE color decode matching the documented `R=clamp(Y+J)`/`G=clamp(Y+K)`/`B=clamp((5Y-2J-K+2)/4)` formulas; scroll/interlace/blink register-driven visual effects; G6/G7 planar interleave in the display path; deterministic unit + integration tests; real openMSX A/B trace-diff where a headless comparison is feasible (honest BLOCKED otherwise); zero regression across the FULL M1-M20 suite; full deferred-backlog review; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M20 suites remain green (deliberate full-system cross-check, not just VDP-adjacent tests, per the human's explicit instruction); QA sign-off required before closure.
- Status: Done. CLOSED by human release decision on 2026-07-07 (DEC-0018 / REQ-M21-004); tagged git `v1.0.21`. Closes deferred-backlog rows D1, D5, D6 (DONE); D7 advances to IN-PROGRESS (M21 partial), command-engine piece carried to M22.
- Details: Planner package `docs/m21-planner-package.md` (RESP-M21-001/REQ-M21-002) resolved D7's scope boundary explicitly (§1.4): the CPU-port piece (`V9958Vdp::effective_address()`) and the display-path piece (`VdpFrameRenderer::planar_row_spans`) both close this cycle; the command-engine-path piece is explicitly carried to M22/D3. Developer implementation (`docs/m21-implementation-report.md`) delivered new `src/devices/video/{vdp_palette.h,frame_buffer.h,vdp_frame_renderer.{h,cpp}}` (a deterministic, pull-model, frozen-register-snapshot renderer — no new clock consumer), additive edits to `vdp_mode.h` (`vdp_base_is_planar`), `v9958_vdp.{h,cpp}` (the one D7 CPU-port edit to `effective_address()` + blink-countdown state driven by the existing `on_vsync()` hook), and `hbf1xv_machine.{h,cpp}` (additive `render_frame()` accessor). Covers every Target-Spec mode (TEXT1/2, GRAPHIC1-7, MULTICOLOR, YJK/YJK+YAE, TEXT1Q/MULTIQ/Unknown flat-blank fallback per A-M21-6), the independently-derived GRAPHIC7 GGGRRRBB byte order (A-M21-4), the YJK truncating-division B-channel (A-M21-5), the G6/G7 17-bit-rotate planar interleave with a genuine end-to-end CPU-port-write -> renderer-read cross-path test (A-M21-10/A-M21-11), and scroll/blink/border-mask/multi-page-scroll/hedged-Field-EO effects (D6). `ctest` 106/106 (95 prior + 11 new: `devices_vdp_palette_unit_test`, 7 more `devices_vdp_frame_renderer_*`/`devices_vdp_planar_interleave_unit_test` unit suites, `machine_hbf1xv_m21_vdp_render_integration_test`, `hbf1xv_m21_vdp_render_system_test`), zero regression (M9/M12 CPU-timing oracles untouched; the existing M14 `video_v9958_ports_unit_test` pointer-carry suite re-run UNMODIFIED and green after the D7 edit). Real openMSX A/B evidence (`docs/m21-parity-trace-diff.md`, `tools/gen-m21-vdp-render-probe.py` + `tools/openmsx-m21-vdp-render-parity.ps1`): all 4 probes (palette/planar/graphic7/yjk) achieved genuine raw VRAM/palette/register-byte ARCHITECTURAL PARITY against openMSX 19.1 on WSL, including the D7 CPU-port planar-transform placement into physical bank1 (0x10000) — a direct, live cross-engine confirmation. The COMPUTED-pixel-color cross-engine sub-claim is honestly reported BLOCKED (openMSX's Tcl debugger exposes no computed-pixel/framebuffer SimpleDebuggable — confirmed via a live `debug list` query this cycle), NOT fabricated or silently upgraded to PASS; this engine's own computed RGB555 values are recorded for the record, cross-checked against the already-independently-unit-tested formulas. QA (`docs/m21-qa-signoff.md`, RESP-M21-003) = **PASS**: independently rebuilt and reproduced ctest 106/106; re-read the reference source for both critical grounding claims (GRAPHIC7 GGGRRRBB, D7 planar rotate) directly, confirming both exact; confirmed the M14 pointer-carry guard is genuinely unmodified via `git diff` against the M14 commit; independently re-derived the YJK rounding math by hand (three concrete negative-numerator cases) and cross-checked it against the live A/B probe by hand-recomputing the claimed pixel output; independently assessed the interlace/EO Field-substitution bug fix as sound (one non-blocking nuance flagged: the final implementation is a documented simplification of `getEvenOddMask()`, not a bit-for-bit port — forward note for M22/M23); confirmed the A/B raw dump files are genuine by reading them byte-for-byte; confirmed the full 34-row deferred-backlog review and that D7's command-engine piece is genuinely not pre-built (repo-wide grep, none found). Deferred-backlog: D1/D5/D6 -> DONE (M21); D7 -> IN-PROGRESS (M21 partial), command-engine piece carried to M22.

## M22 (Kickoff 2026-07-07)

- Milestone ID: M22
- Title: Sprites + VDP Command Engine (closes D2/D3, finishes D7)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement deferred-backlog rows **D2** (sprite rendering + collision/5th-sprite detection: Sprite Mode 1 & 2, 8/line, per-line color, EC/IC/CC, the 1-px vertical shift, S#0 5S/C and S#3-S#6 collision coords) and **D3** (VDP command engine: R#32-46, HMMC/YMMM/HMMM/HMMV/LMMC/LMCM/LMMM/LMMV/LINE/SRCH/PSET/POINT + logical ops, S#2 TR/CE handshake, S#7 color, S#8/9 border, R#25 CMD-in-all-modes). Per DEC-0018, MUST ALSO finish **D7's remaining command-engine-path piece** (the command engine's own coordinate-to-VRAM-address resolution for G6/G7 planar modes) — D7 has been IN-PROGRESS since M21 and this milestone closes it in full. Builds atop M21's `VdpFrameRenderer`/`FrameBuffer` architecture (closed, v1.0.21): sprites composite onto the existing background-plane output; the command engine's VRAM writes must interoperate with the same G6/G7 planar-interleave transform D7 already established. Second of the pre-authorized three-milestone run (2026-07-07 human directive) — separate tag, own QA sign-off, dedicated system integration test; proceeding through the release-decision/tag step without an extra pause on a clean QA PASS.
- Acceptance Criteria (planner to detail): sprite rendering + collision detection byte-exact per `SpriteChecker.cc/.hh`, compositing onto M21's `FrameBuffer`; VDP command engine byte-exact per `VDPCmdEngine.cc/.hh` for the commands named in D3; D7 closed in full (no longer partial); deterministic unit + integration + system tests; real openMSX A/B trace-diff where feasible; zero regression across the FULL M1-M21 suite (106 tests); full deferred-backlog review; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M21 suites remain green (deliberate full-system cross-check, per the human's explicit instruction); QA sign-off required before closure.
- Status: Done. CLOSED by coordinator release decision on 2026-07-07 (DEC-0019 / REQ-M22-004); tagged git `v1.0.22`. Closes deferred-backlog rows D2, D3 (DONE); D7 closes IN FULL (no longer partial).
- Details: Planner package `docs/m22-planner-package.md` (RESP-M22-001/REQ-M22-002) resolved both required architectural questions explicitly (§1.4): sprite compositing folds into `VdpFrameRenderer::render_frame()`'s existing pipeline (no new output type), and the command engine uses a HYBRID execution model (10 commands atomic; LMCM/LMMC/HMMC event-driven, mirroring the M16 FDC DRQ/INTRQ precedent). Developer implementation (`docs/m22-implementation-report.md`) delivered new `src/devices/video/{sprite_engine.{h,cpp},vdp_command_engine.{h,cpp},vdp_command_address.h}` (both new classes owned INSIDE `V9958Vdp`, mirroring the `blink_countdown_` ownership style), additive edits to `vdp_mode.h`, `v9958_vdp.{h,cpp}` (command-register dispatch extension, `on_vsync()`/`recompute_mode()` hooks, live S#0/S#2/S#3-S#9 status wiring), and `vdp_frame_renderer.{h,cpp}` (`composite_sprites()` step). `V9958Vdp::effective_address()` confirmed genuinely UNCHANGED by both the coordinator and QA independently. `ctest` 117/117 (106 prior + 11 new), zero regression, independently reproduced by both the coordinator and QA from clean rebuilds. A genuine regression was self-caught and fixed during implementation: `SpriteEngine` needed the same `displayEnabled && spriteEnabled` gate real hardware uses, or freshly-reset all-zero VRAM produced phantom Y=0 sprites breaking pre-existing M14 tests — QA independently confirmed the fix's grounding. QA (`docs/m22-qa-signoff.md`, RESP-M22-003) = **PASS**, with genuinely independent, adversarial scrutiny of the A/B evidence: QA did NOT accept the developer's "PARITY on all 7 probes" framing at face value — it hand-decoded the raw status-register divergence numbers itself, found the developer's specific causal narrative ("a stale field survives an intervening status read") does not arithmetically hold up, and independently reached a more defensible conclusion ("this A/B harness's live-BIOS-driven reference snapshot cannot be guaranteed to reflect the identical injected test scenario as the deterministic engine side") — while separately, independently confirming via a full line-by-line algorithm comparison against the reference source (plus the complete deterministic test suite) that this is a harness/methodology artifact, not a code defect. QA also independently surfaced two additional non-blocking findings the implementation report itself had not disclosed (ASX/S#8-S#9 persistence scope narrower than documented; S#7/S#8/S#9 excluded from every A/B probe's gate) — both recorded as documentation-precision follow-ups, not blockers. Backlog: D2/D3 -> DONE (M22); D7 -> DONE (M22), closing IN FULL (no longer partial).

## M23 (Kickoff 2026-07-08)

- Milestone ID: M23
- Title: Exact Cycle Timing (closes C1/C2/D4, or an explicitly-scoped subset)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement deferred-backlog rows **C1** (exact cycle/T-state timing parity), **C2** (Z80 HALT-R increment, DEC-0004), and **D4** (cycle-accurate VDP access-slot/command timing: 1368 VDP-cycles/line, slot tables 154/88/31, 16-cycle request latency, CPU-access priority, the ~29-Z80-cycle safe-access gap, exact sub-frame raster IRQ position). THE single biggest architectural departure of the M21-M23 run: M21 (`VdpFrameRenderer`, pull-model/frozen-snapshot) and M22 (`VdpCommandEngine`, atomic/event-driven) were BOTH explicitly built to defer cycle-accurate VDP timing to THIS milestone. M23 must introduce genuine access-slot/cycle-cost modeling without breaking the M9/M12 CPU-timing oracles, any of the 117 existing deterministic tests built on the pull-model/atomic assumptions, or the FDC's (M16) and command engine's (M22) own existing event-driven timing models — the planner must explicitly reconcile all of these. Third and FINAL milestone of the pre-authorized three-milestone run (2026-07-07 human directive) — separate tag, own QA sign-off, dedicated system integration test; proceeding through the release-decision/tag step without an extra pause on a clean QA PASS.
- Acceptance Criteria (planner to detail): explicit scope resolution of what "cycle-accurate" means for this milestone, justified against the risk of destabilizing the pull-model/atomic architecture; C1/C2/D4 (or an explicitly-scoped, honestly-justified subset, with any remainder re-scoped as a new backlog row rather than force-closed) implemented and tested; deterministic unit + integration + system tests; real openMSX A/B trace-diff where feasible; zero regression across the FULL M1-M22 suite (117 tests); full deferred-backlog review; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M22 suites remain green (deliberate full-system cross-check, per the human's explicit instruction, and the project's MOST safety-critical regression surface — the M9/M12 CPU-timing oracles); QA sign-off required before closure.
- Status: Done. CLOSED by coordinator release decision on 2026-07-08 (DEC-0020 / REQ-M23-004); tagged git `v1.0.23`. Closes deferred-backlog row C2 (DONE, in full); C1 and D4 both advance to IN-PROGRESS (M23 partial). This is the THIRD AND FINAL milestone of the pre-authorized M21-M23 run. Planner package accepted (`docs/m23-planner-package.md`, RESP-M23-001/REQ-M23-002) resolved scope deliberately narrow (§2): C2 closes in full; C1/D4 advance to a real, tested, non-gating foundation with a precisely-named 5-item remainder carried forward as ONE tracked row each (mirrors the D7/C5 precedent), NOT the full slot-table/gating depth — a conservative, honestly-justified call given the demonstrated risk that naive real CPU-execution gating would break the M21/M22 back-to-back-`OUT (#98),A` system-test fixtures.
- Details: Developer implementation (`docs/m23-implementation-report.md`) delivered: (S1) the ONE-line surgical fix to `Z80aCpu::step()`'s halted branch (`src/devices/cpu/z80a_cpu.cpp`) — calls the EXISTING `increment_refresh_register()`, keeping the CPU core's own returned datasheet T-state count the bare, unchanged `4` (A-M23-1's invariant); the machine-level `datasheet + m1_wait` formula (UNCHANGED) now reports 5T for a halted idle step. The ONE deliberately-affected golden (`tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp`, t3 4→5, `elapsed_cycles()` 22→23) updated with cited grounding (`Z80.hh:19-21`, `CPUCore.cc:2508-2511`); a fresh `rg "\.halted\(\)" tests/` re-run confirmed the identical 22-file list the planner's own audit found, zero new sites, so no other existing test needed changing. New tests: `tests/unit/devices/z80a_halt_r_unit_test.cpp`, `tests/integration/machine/hbf1xv_m23_halt_r_integration_test.cpp` (the latter deliberately a NEW file rather than an edit to `hbf1xv_interrupt_ack_integration_test.cpp`, which is one of the planner's own NAMED zero-tolerance oracle files). (S2) new, additive, strictly non-gating `src/devices/video/vdp_access_timing.h` (15 named `VdpAccessDelta` offsets; raster-position derivation; the two deliberately-separate, cited latency facts, confirmed via a dedicated test to never be combined) plus additive-only `Hbf1xvMachine::cycles_since_last_vsync()`/`vdp_cycle_position()` accessors fed by exactly one new bookkeeping line inside the existing `run_frame()`. `git diff v1.0.22` confirms: `step_cpu_instruction()`/`run_cycles()`/`run_until_cycle()` byte-for-byte unchanged; `vdp_frame_renderer.*`/`vdp_command_engine.*`/`sprite_engine.*` untouched; the 10 named zero-tolerance M9/M12 CPU-timing-oracle suites show ZERO diff. A dedicated regression test (`tests/integration/machine/hbf1xv_m23_vdp_access_timing_non_gating_integration_test.cpp`) re-runs the EXACT M21/M22 system-test CPU-program fixtures (back-to-back `OUT (#98),A` writes, zero spacers) and proves their cumulative T-state totals are byte-identical to their pre-M23 values, captured as literal golden constants. Also a small, additive, backward-compatible extension to `src/main.cpp`'s existing `--parity-trace` CLI mode (a new, optional `halt_idle_extra_steps` trailing argument, default 0 = byte-identical pre-M23 behavior for every prior invocation — mirrors the M19 `--cart1`/`--cart2` precedent of extending this SAME mode for a new milestone's specific A/B need) to enable the C2 live-openMSX A/B probe. `ctest` 121/121 green (117 prior + 4 new), zero regression. Real openMSX 19.1/WSL A/B evidence (`tools/openmsx-m23-halt-r-parity.ps1` -> `docs/m23-parity-trace-diff.md`): the live `reg r` Tcl readback IS confirmed available; R and PC match exactly for the HALT opcode's own M1 fetch and the first two genuine halted-idle steps, then a large, reproducible, REAL-TIME-tracked (not step-count-tracked) divergence appears in the live openMSX session — traced, with a grounded hypothesis directly citing the SAME `CPUClock.hh:53-59` `advanceHalt` bulk-scheduling source this milestone's own C2 change is grounded in, to how openMSX's live Tcl `step()` command behaves while the CPU is halted (not a defect in either engine's R-register correctness); honestly reported as DIVERGENCE with root-cause analysis, not fabricated as a clean PASS. D4/C1's numeric VDP-access-wait-cost sub-claim stays explicitly BLOCKED/not-attempted, per the plan (no feasible technique exists; nothing gated on it this cycle). QA (`docs/m23-qa-signoff.md`, RESP-M23-003) = **PASS**, with an exceptional level of independent scrutiny: QA independently re-ran the FULL suite (121/121), independently confirmed via its OWN `git diff v1.0.22` that all 10 named zero-tolerance oracle suites AND all 6 M21/M22 device files show zero changes, independently re-derived the golden arithmetic and all 6 new non-gating fixture totals BY HAND, and — most notably — independently re-ran the C2 A/B harness against live WSL openMSX itself AND designed two of its OWN exploratory Tcl probes (an exact-reproducibility check and a boot-window-timing-sensitivity variant) to test the developer's divergence hypothesis, refining the causal explanation from "wall-clock time between Tcl round-trips" to the more precise "emulated time accumulated during the unthrottled pre-measurement boot window, per openMSX's own bulk `advanceHalt` scheduling" — confirming the divergence is a live-session scheduling artifact, not a code defect. QA also independently assessed the disclosed `src/main.cpp` CLI extension and judged it reasonable, recommending only a `decisions.md` ratification (folded into DEC-0020) as a non-blocking, procedural follow-up. Backlog: C2 -> DONE (M23), closing in full; C1/D4 -> IN-PROGRESS (M23 partial) with the precise 5-item remainder named (`agent-protocol/state/deferred-backlog.md`).

## M24 (Kickoff 2026-07-08)

- Milestone ID: M24
- Title: ZEXDOC/ZEXALL Full Parity Sweep (closes C3)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement deferred-backlog row **C3** (ZEXDOC/ZEXALL full parity sweep) using `references/zexall/` (present, uncommitted since M19) — a legally-sourced ZEXALL/ZEXDOC Z80 exerciser suite (YAZE-AG project, GPL-licensed, read-only validation reference) containing real, runnable CP/M-style `.com` binaries. Commit `references/zexall/`; design a genuine, minimal, honestly-scoped BDOS/CP/M shim grounded in the suite's own actual BDOS-call sites (not guessed); run the FULL ZEXALL and ZEXDOC suites to completion against this project's already-QA-verified Z80A CPU core, reporting real, unfabricated pass/fail results; capture real openMSX A/B evidence where feasible. Per the human's explicit, standing "zero license-sensitive future work" directive: running a third-party GPL test suite as a black-box validation tool (loading its pre-built `.com` binary as DATA, exactly as `bios/`/`roms/` assets already are) is fundamentally different from, and much lower-risk than, transcribing a GPL project's own data tables into `src/` — but nothing from the suite's own Z80 assembly source may be copied into `src/`. Second of an M24-M25 continuation of the established milestone cadence (2026-07-08 human directive) — own planner package, developer implementation, dedicated system integration test, QA sign-off, separate tag; proceeding through the release-decision/tag step without an extra pause on a clean QA PASS.
- Acceptance Criteria (planner to detail): `references/zexall/` committed; a genuine BDOS/CP/M shim built and grounded in the suite's own actual call sites; FULL ZEXALL and ZEXDOC suites run to completion with real, captured results (100% pass expected against an already-QA-verified core, but report honestly whatever is actually observed); deterministic unit/integration/system tests; real openMSX A/B evidence where feasible; zero regression across the FULL M1-M23 suite (121 tests); full deferred-backlog review; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M23 suites remain green; QA sign-off required before closure.
- Status: Done. CLOSED by coordinator release decision on 2026-07-08 (DEC-0022 / REQ-M24-005); tagged git `v1.0.24`. Closes deferred-backlog row C3 in full.
- Details: Planner package `docs/m24-planner-package.md`. Developer implementation
  `docs/m24-implementation-report.md`: new, generic `CpmBdosHarness` (`src/machine/
  cpm_bdos_harness.{h,cpp}`, zero zexall-specific knowledge -- implements only the standard,
  third-party CP/M `.com` loading convention: `org 0x0100`, the top-of-memory word at `0x0006`,
  `CALL 5` BDOS dispatch trap, `JP 0` warm-boot trap), proven first against a hand-written
  synthetic fixture (13 unit cases, `tests/unit/machine/cpm_bdos_harness_unit_test.cpp`) and the
  device-isolation invariant (`tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp`)
  BEFORE the real GPL binaries were ever loaded. New `--cpm-run` CLI mode in `src/main.cpp`
  (additive only, 67 lines, confirmed via `git diff v1.0.23`). The real, committed
  `references/zexall/zexall.com` and `zexdoc.com` were then run to genuine CP/M warm-boot
  completion via a dedicated system test (`tests/system/hbf1xv_m24_zexall_system_test.cpp`,
  in-process, no subprocess): **134/134 group verdicts PASS** (67 named groups x 2 suites, real
  captured "  OK" markers parsed via the new `tools/zexall-report.py` + its own self-check),
  5,764,169,474 real instructions executed per suite (identical count, both suites), ~23m53s
  combined measured wall-clock (12m28s zexall + 11m25s zexdoc) -- see the implementation report
  for the full small-subset-measure -> extrapolate -> actually-run-full-sweep protocol. Mandatory
  adversarial self-check PASSED: a deliberately corrupted IN-MEMORY-ONLY copy of the real
  `zexall.com` (real file confirmed byte-identical before/after via SHA-256) correctly produced a
  genuine FAIL verdict via the same, unmodified harness + report tool. Real openMSX A/B evidence
  (`tools/openmsx-m24-zexall-parity.ps1` -> `docs/m24-parity-trace-diff.md`): a bounded
  11-real-Z80-instruction prefix (live-verified at implementation time; the "complete 1-2 named
  groups" bound the planner package originally suggested was found NOT live-Tcl-feasible, since
  even the smallest group needs 10^5-10^8 real instructions) achieved genuine PARITY for BOTH
  suites; the full-suite live-Tcl A/B and the MSX-DOS-disk-boot A/B are both explicitly, honestly
  marked BLOCKED (the latter after actually checking `bios/`/`roms/` for an MSX-DOS asset --
  confirmed absent, not assumed). `ctest` 124/124 (121 prior + 3 new), zero regression; `git diff
  v1.0.23` confirms zero changes to `src/devices/`, `src/peripherals/`, `src/core/` -- only
  `src/main.cpp` (additive) and the two new `src/machine/cpm_bdos_harness.*` files. The new system
  test genuinely takes ~24 minutes (both real suites, sequentially, in one process) -- registered
  with CTest LABEL `m24_slow_full_sweep` so the routine evidence-gate cadence can exclude it
  explicitly (123/123 in ~3.4s via `ctest -LE m24_slow_full_sweep`), disclosed explicitly for
  QA/coordinator review rather than silently resolved. `references/zexall/` staged (`git add`),
  committed as part of this milestone's closure commit, ending 5 milestones (M19-M23) of it
  sitting staged-but-uncommitted.

  QA (`docs/m24-qa-signoff.md`, RESP-M24-003) independently reproduced the full 134/134 headline
  claim from a genuinely clean rebuild (a THIRD from-scratch reproduction, matching the developer's
  and coordinator's own independent reproductions byte-for-byte) plus every other substantive
  claim (license isolation, device isolation, adversarial self-check, combinatorial-total
  arithmetic, openMSX A/B PARITY re-run end-to-end), finding no CPU-core defect. QA returned a
  **Conditional Pass**, not a clean PASS: `tests/system/hbf1xv_m24_zexall_system_test.cpp`'s
  ctest-level gate checked marker COUNT (`ok_markers + error_markers == 67`) but never hard-
  asserted `error_markers == 0` for either suite — a future genuine CPU-core regression that
  flipped all 67 groups to FAIL could still leave this specific ctest reporting `Passed`. Per the
  milestone's own standing directive ("STOP and consult the human" on anything short of a clean
  PASS), the coordinator stopped and presented the human with QA's own two named paths; the human
  chose **"Fix, re-confirm, then tag."** The developer added two hard assertions
  (`Zexall_ZeroErrorMarkers`/`Zexdoc_ZeroErrorMarkers`) — purely additive, test-code-only — and
  re-ran the full slow sweep to completion a FOURTH independent time this cycle (1638.22s),
  landing on the identical 5,764,169,474-instruction/67-0-marker result yet again, with the new
  hard gates genuinely exercised and passing. The coordinator independently re-verified the fix by
  direct file read plus its own fast-subset re-run (123/123). QA also explicitly ruled (per
  DEC-0021) that the disk-boot-A/B sub-claim stays BLOCKED-as-recorded — redoing it would require
  solving backlog C5's own still-unsolved auto-boot-trigger problem, out of this milestone's
  planned scope; `disks/` (added by the human mid-cycle) is reserved for a future, dedicated C5
  investigation instead. Ledger status transition: backlog C3 -> DONE (M24).

## M25 (Kickoff 2026-07-08)

- Milestone ID: M25
- Title: Sony Speed-Controller + Hardware PAUSE + Ren-Sha Turbo (closes C8)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement deferred-backlog row **C8** (Sony speed-controller + hardware PAUSE (MB670836); Ren-Sha Turbo autofire — HB-F1XV-specific, never previously scoped). Grounded directly against `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §9 and `references/fact-sheets/Zilog Z80A CPU.md` §6: a second Sony custom LSI (MB670836) handles DRAM address multiplexing plus the speed-controller (slow-motion) and hardware-PAUSE circuitry. The HB-F1XV has NO CPU turbo mode — the Speed Controller slider is NOT a clock-speed change, it is an autofire on the PAUSE button synced to VBlank, slowing games by pausing them intermittently. Hardware PAUSE physically halts the CPU and cannot be bypassed in software — a distinct mechanism from the Z80's own `HALT` instruction (already modeled, M9/M23/C2). Ren-Sha Turbo is a separate autofire feature whose real trigger/control mechanism the planner must determine from the fact sheets/references, not guess. Second and FINAL milestone of the M24-M25 continuation (2026-07-08 human directive) — own planner package, developer implementation, dedicated system integration test, QA sign-off, separate tag; proceeding through the release-decision/tag step without an extra pause on a clean QA PASS, UNLESS QA does not reach a clean PASS (this condition already fired once this run, for M24 — see DEC-0022), in which case STOP and consult the human.
- Acceptance Criteria (planner to detail): a genuine, fact-sheet-grounded design for PAUSE/speed-controller/Ren-Sha Turbo (not guessed); deterministic unit/integration/system tests; real openMSX A/B evidence where feasible or honest BLOCKED; zero regression across the FULL M1-M24 suite (124 tests, `hbf1xv_m24_zexall_system_test` alone taking ~24-27 minutes); full deferred-backlog review; QA sign-off before closure.
- Unit/Integration Tests Required: `tests/unit/devices/chipset/mb670836_pause_unit_test.cpp`,
  `tests/unit/peripherals/rensha_turbo_unit_test.cpp`,
  `tests/integration/peripherals/rensha_turbo_integration_test.cpp`,
  `tests/integration/machine/hbf1xv_m25_pause_integration_test.cpp`,
  `tests/integration/machine/hbf1xv_m25_speed_controller_integration_test.cpp`,
  `tests/system/hbf1xv_m25_speed_pause_rensha_system_test.cpp`.
- Regression Scope: all M1-M24 suites remain green; QA sign-off required before closure.
- Status: Done. CLOSED by coordinator release decision on 2026-07-08 (DEC-0023/REQ-M25-004);
  tagged git `v1.0.25`. Closes deferred-backlog row C8 in full. This is the SECOND AND FINAL
  milestone of the M24-M25 continuation, which is now fully complete.
- Details: Planner package `docs/m25-planner-package.md` (REQ-M25-001/RESP-M25-001). Developer
  implementation `docs/m25-implementation-report.md`: new `Mb670836PauseController`
  (`src/devices/chipset/mb670836_pause.{h,cpp}`) — a machine-level CPU-execution gate consulted at
  the very top of `step_cpu_instruction()`, BEFORE any opcode decode, so PC/R/every register stay
  completely frozen while engaged (architecturally distinct from the Z80's own CPU-internal `HALT`,
  which keeps incrementing R — see the planner package §2.3 comparison table). The manual PAUSE
  button (toggle semantics, A-M25-1) and the Speed Controller's own VBlank-synced duty cycle
  (`kPeriodFrames=8`, driven by a single additive line in `run_frame()` alongside the existing
  `vdp_.on_vsync()` call) OR into ONE combined `cpu_should_pause()` gate (A-M25-4). New `RenshaTurbo`
  (`src/peripherals/rensha_turbo.{h,cpp}`) — a simpler, peripheral-level autofire signal generator
  grounded in real openMSX behavior (`RenShaTurbo.{hh,cc}`/`Autofire.{hh,cc}`, the `MSXPPI.cc:90-93`/
  `sound/MSXPSG.cc:90-93` OR-combine wiring, independently confirmed by direct source read) and the
  real per-machine `Sony_HB-F1XV.xml:16-19` calibration (`min_ints=47`/`max_ints=221`), wired into
  `KeyboardMatrix`/`JoystickPorts` via additive, default-nullptr OR-mask attach points (byte-for-byte
  pre-M25 regression guard when unattached, M25-S3). Built in the planner's exact 5-slice order
  (S1 PauseController isolated → S2 RenshaTurbo isolated → S3 peripheral wiring+regression guards →
  S4 machine wiring → S5 dedicated system test + A/B evidence + backlog/documentation closure). 6 new
  dedicated unit/integration/system tests prove: hardware PAUSE genuinely freezes PC/every
  register/R/memory across multiple paused `step_cpu_instruction()` calls with `elapsed_cycles()`
  still advancing exactly 1 T-state/call, and resumes correctly on release; PAUSE cannot be bypassed
  via any CPU-visible API (a dedicated I/O-port+arithmetic probe program run 10 steps while paused
  never advances PC); the Speed Controller's duty cycle is deterministic and hand-computable (a
  counter-loop program driven through 16 simulated VBlank windows matches an independently
  hand-computed growth total exactly); Ren-Sha Turbo never forces a spurious press (R-M25-6, an
  exhaustive negative-control sweep across a full toggle period, both in isolation and through a real
  `Hbf1xvMachine`). Real openMSX A/B evidence (`tools/openmsx-m25-rensha-parity.ps1` →
  `docs/m25-parity-trace-diff.md`): Ren-Sha Turbo achieved genuine live **PARITY** against the real
  `Sony_HB-F1XV` openMSX machine — driven via openMSX's own live Tcl `set renshaturbo <speed>`
  setting + `debug write/read ioports` sampling (scheduled via `after time`, native continuous
  emulation between samples, NOT per-instruction single-stepping, specifically because M23/M24 both
  independently found live per-instruction Tcl stepping becomes slow/inconsistent past a small step
  count): the not-held invariant (R-M25-6) confirmed ZERO observable effect at every sampled point
  for BOTH the keyboard-row-8 and PSG-R14 paths, and the held case confirmed genuine bit0 alternation
  live via openMSX's own `keymatrixdown 8 1` Tcl primitive for the keyboard path. The joystick-
  trigger-A held-alternation sub-case is honestly disclosed as NOT attempted (no live "hold a
  joystick button" Tcl primitive exists in openMSX 21.0's scripting layer). Hardware PAUSE / Speed
  Controller is honestly reported **BLOCKED** for A/B — an exhaustive, cited search (independently
  re-confirmed this cycle, not merely trusted from the planner package) established that openMSX
  21.0 has ZERO Sony-specific PAUSE/speed-controller modeling anywhere: `SG1000Pause.hh` is a
  different machine family/mechanism (NMI, not a WAIT-gate); `MSXTurboRPause.{hh,cc}` is a different
  chipset (S1990, not S1985/MB670836) using an architecturally-incompatible whole-session
  `getMotherBoard().pause()`; four of the five real Sony MSX machine XML definitions explicitly say
  "speed controller (not emulated)" in their own `<description>` text, and the fifth (this project's
  actual target, `Sony_HB-F1XV.xml`) does not even mention it. Per Acceptance Criterion 9, this
  BLOCKED disposition does NOT gate C8's closure (mirrors the M21 computed-pixel-color and C3/M24
  disk-boot-A/B precedents). `git diff v1.0.24` confirms zero changes to `src/devices/cpu/`,
  `src/devices/video/`, `src/devices/audio/`, `src/devices/rtc/`, `src/devices/fdc/`,
  `src/devices/cartridge/`, `src/devices/memory/`, `src/devices/halnote/`, `src/devices/kanji/`,
  `src/core/`, and all 12 named zero-tolerance CPU-timing-oracle test files (byte-for-byte
  unchanged, independently re-confirmed). Full regression: 130/130 (124 prior + 6 new), zero
  regression, including the slow `hbf1xv_m24_zexall_system_test` re-run to completion at least once
  before requesting QA. Full 34-row deferred-backlog review completed
  (`agent-protocol/state/deferred-backlog.md`).

  QA (`docs/m25-qa-signoff.md`, RESP-M25-003) independently reproduced everything from a genuinely
  clean rebuild — full regression (130/130, including its own fresh 22m31s ZEXALL/ZEXDOC re-run),
  the 12-file CPU-timing-oracle diff (empty), hardware PAUSE's non-bypassability (confirmed both by
  test AND by static `grep` inspection that no I/O port dispatch touches the gate), and — critically
  — the ONE piece of evidence the coordinator had explicitly left unreproduced: the live openMSX A/B
  script itself, re-run end-to-end by QA against the real WSL `Sony_HB-F1XV` machine, reproducing
  genuine Ren-Sha Turbo PARITY from scratch. QA returned a **clean, unconditional Pass** — the
  standing "STOP if not a clean PASS" condition, which fired once for M24, did NOT fire here. QA's
  sole finding (Low, non-blocking): its own fresh `find` sweep for every `Sony_HB-F1*.xml` file
  turned up a SIXTH Sony machine (`Sony_HB-F1XDmk2.xml`, missed by every prior search this cycle) —
  independently confirmed to also wire no Pause/SpeedController device, reinforcing rather than
  undermining the BLOCKED disposition. Per QA's own explicit recommendation, the coordinator applied
  this "five"→"six" documentation fix directly across all four affected artifacts (verifying the
  PowerShell edit's backtick-escaping via a standalone parse-and-render check first, learning from
  that same script's own earlier self-caught escaping bug) rather than routing back to the developer.
  Ledger status transition: backlog C8 -> DONE (M25). **This closes the full M24-M25 continuation**
  the human's 2026-07-08 directive requested.

## M26 (Kickoff 2026-07-08)

- Milestone ID: M26
- Title: SDL3 Frontend (closes C9)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement deferred-backlog row **C9** (SDL3 frontend — video/audio/input presentation, in baseline scope, not yet built). A real-time SDL3 application layer: window + main loop driving the existing `run_frame()`/`step_cpu_instruction()` core, video presentation of the M21 `VdpFrameRenderer`/`FrameBuffer` RGB555 output, audio from the existing PSG/YM2413 devices, and keyboard/joystick input mapped into the M15 `KeyboardMatrix`/`JoystickPorts` APIs (including the M25 PAUSE/Speed-Controller/Ren-Sha-Turbo surfaces built for exactly this). Coordinator's explicit scope-boundary decision (human delegated this call): M26 includes ONLY a minimal, new decoded-`FrameBuffer`-to-real-color-PNG capture capability (under `debug/frames/`) as a testability necessity — NOT the broader debug/-tooling request (audio capture, full CPU/memory/VRAM dump CLI wiring, keystroke-sequencing automation, production/stress testing), which is deferred whole-cloth to a new **M27 "Production Hardening + Debug/Test Tooling"** milestone. Planner must resolve how M26's own tests run headlessly in `ctest` without a real display/audio device (SDL3's dummy/null driver support, grounded in `references/sdl3/`, not assumed).
- Acceptance Criteria (planner to detail): genuine SDL3 frontend that builds and runs (`-DSONY_MSX_ENABLE_SDL3=ON`); deterministic unit/integration/system tests running headlessly in `ctest`; the new PNG frame-capture capability with at least one real committed PNG under `debug/frames/`; full deferred-backlog review; M27's scope named as a forward-looking note only, not designed here; QA sign-off before closure.
- Unit/Integration Tests Required (planner to detail).
- Regression Scope: all M1-M25 suites remain green; QA sign-off required before closure.
- Status: Done. CLOSED by coordinator release decision on 2026-07-08 (DEC-0024/REQ-M26-004);
  tagged git `v1.0.26`. Closes deferred-backlog row C9 in full. Largest, most architecturally
  novel milestone to date; QA returned zero findings of any severity.
- Details: `docs/m26-planner-package.md` accepted (RESP-M26-001). Implementation delivered per
  `docs/m26-implementation-report.md` + `docs/m26-frontend-evidence.md`.

  **`Hbf1xvMachine::on_vsync_boundary()`** — a pure, mechanical extraction of `run_frame()`'s
  pre-M26 body (frame counter, `vdp_.on_vsync()`, `pause_controller_.on_vsync()`,
  `last_vsync_cycle_` bookkeeping), EXCLUDING the `scheduler_.tick(kFrameCycles)` call; `run_frame()`
  itself becomes `scheduler_.tick(kFrameCycles); on_vsync_boundary();` — textually different,
  behaviorally IDENTICAL for every pre-M26 caller (confirmed both by direct inspection and by a new
  regression-guard test). This is the ONLY behavior-affecting change to `src/machine/` this
  milestone makes (`git diff --stat src/devices src/peripherals src/core` confirmed EMPTY,
  immediately after this edit, before proceeding to anything else, per the dispatch's explicit
  instruction). It exists so a real-time SDL3 loop can drive the CPU purely via
  `step_cpu_instruction()` in a sub-loop to the frame boundary, then call `on_vsync_boundary()`
  directly — never `run_frame()` in the same session (A-M26-5, the exact double-count hazard M25's
  own R-M25-5 already named at the test level).

  **New `src/frontend/`** (SDL3-gated, compiled only under `-DSONY_MSX_ENABLE_SDL3=ON`): `Sdl3App`
  (window/renderer/audio-stream lifecycle, machine ownership + asset loading, the deterministic
  `run_one_frame()` core step vs. the real-time `run_interactive()` wrapper — `SDL_Delay`-paced,
  NEVER exercised by `ctest`); `Sdl3VideoPresenter` (owns an `SDL_PIXELFORMAT_XRGB1555` texture —
  bit-for-bit identical to `FrameBuffer`'s own documented RGB555 layout, A-M26-3 — uploaded via
  `SDL_UpdateTexture` with ZERO per-pixel conversion in this project's own code, independently
  PROVEN via a real `SDL_RenderReadPixels` pixel-readback test, not merely assumed);
  `Sdl3AudioPresenter` (real `SDL_OpenAudioDeviceStream`/`SDL_PutAudioStreamData` wiring around a
  new, SDL3-INDEPENDENT `PsgAudioPump` — the actual PSG/YM2149 generator-advance WIRING, headlessly
  `ctest`-provable against a hand-computed square-wave oracle, discharging R-M26-4's
  "untested-in-anger" risk); `Sdl3InputMapper` (a new, disclosed, first-principles 71-entry
  `SDL_Scancode`→11×8-keyboard-matrix table for rows 0-8, independently cross-checked against this
  project's own already-established ground truth — `RenshaTurbo`'s M25 doc comment citing "keyboard
  row 8 bit 0 (SPACE)" matches the table's own row=8/column=0/`SDL_SCANCODE_SPACE` entry exactly —
  plus joystick digital button/axis mapping and PAUSE/Speed-Controller/Ren-Sha-Turbo PC-keybindings,
  A-M26-7); `sdl3_cli.h` (`--bios-dir`/`--disk`(A-M26-6)/`--cart1`/`--cart1-type`/`--cart2`/
  `--cart2-type`/`--max-frames`, reusing the existing, unmodified M19 `cartridge_cli` parser
  verbatim, never reimplemented).

  **YM2413/FM-PAC is honestly, deliberately left SILENT** in the audio mix — a hard, non-negotiable
  constraint (R-M26-5). It has real register-file/channel/rhythm-decode fidelity (M17, backlog B3)
  but ZERO waveform-synthesis capability (backlog **E1**, still OPEN); inventing a DSP pipeline
  (log-sin/exp operator tables, the 128-level EG rate mechanism, etc.) would be exactly the kind of
  unfounded shortcut this project's culture explicitly disallows. Independently re-verified this
  cycle: `git diff --stat -- src/devices/audio/ym2413_opll.{h,cpp}` is EMPTY, and `find src -iname
  "*ym2413*"` shows only the two pre-existing M17 files — no new parallel YM2413-audio file exists
  anywhere. `Sdl3AudioPresenter`'s own doc comment and the SDL3 app's startup message both disclose
  this explicitly to a human user.

  **The ONE new debug/testing capability the coordinator authorized** — decoded-`FrameBuffer`→real
  color PNG capture (NOT raw VRAM bytes, unlike the pre-existing `tools/mem-to-png.py`) — new
  `src/machine/frame_dump.h`/`.cpp` (headless-buildable, reusing the existing, already-proven
  `debug_dump::serialize_region()` folded-hex routine — genuine reuse, not a parallel
  reimplementation) + new `tools/frame-to-png.py` (mirroring `tools/mem-to-png.py`'s exact
  determinism discipline: DEFLATE stored blocks only, a hermetic `--self-check` with a golden
  SHA-256). Ships with a real, committed example: `debug/frames/m26-example-boot.png` (256×192,
  24-bit truecolor, 16 vertical color bars spanning all 16 palette entries of a GRAPHIC4/SCREEN5
  test scene) + its source `debug/frames/m26-example-boot.frame` dump, produced via a new
  `sony_msx_headless --frame-dump-demo` evidence-generation CLI mode that drives the VDP directly
  through the real `#98`/`#99`/`#9A` port protocol via the existing, non-perturbing
  `debug_io_write()` seam (M13) — zero CPU driver program/Z80 assembly needed. Both files are
  exempted from the pre-existing `/debug/**/*.png` blanket-ignore rule via two new, explicit `!`
  `.gitignore` exceptions.

  **A real, empirically-confirmed environment finding (A-M26-1/R-M26-1), reported honestly as
  instructed rather than assumed either way**: SDL3 was NOT pre-installed in this execution
  environment — a bare `cmake -S . -B build_probe -DSONY_MSX_ENABLE_SDL3=ON` genuinely failed with
  `Could not find a package configuration file provided by "SDL3"`. However, the project's own
  vendored `references/sdl3/` tree is a complete, buildable SDL3 3.5.0 source distribution
  (zlib-licensed — permissive, categorically different from the openMSX GPL-isolation concern,
  since building/linking against it as a binary dependency is the normal, intended use of a
  zlib-licensed library and does not copy any of its source into `src/`). Built and installed once,
  locally, to `build/_sdl3_install/` (gitignored, not committed, fully reproducible via a documented
  `cmake`/`cmake --build`/`cmake --install` sequence against `references/sdl3/`, recorded in
  `docs/m26-implementation-report.md` §2). With `-DCMAKE_PREFIX_PATH=<repo>/build/_sdl3_install`,
  `-DSONY_MSX_ENABLE_SDL3=ON` now configures and builds `sony_msx_sdl3.exe` cleanly. The "dummy"
  video/audio driver mechanism (A-M26-2, grounded by directly reading
  `references/sdl3/src/video/dummy/SDL_nullvideo.c` and `.../audio/dummy/SDL_dummyaudio.c`) was then
  independently, empirically confirmed to work end to end — NOT merely assumed from the source
  read — via a real `sony_msx_sdl3.exe --hidden-window --max-frames 5 --bios-dir bios` invocation
  under `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`: real `SDL_Init`/window/renderer/
  audio-stream creation, 5 real emulated frames, clean exit code 0.

  Headless suite (`-DSONY_MSX_ENABLE_SDL3=OFF`, the default): **134/134** (130 prior M1-M25 + 4 new
  headless-buildable M26 tests: `machine_hbf1xv_m26_vsync_boundary_integration_test`,
  `machine_frame_dump_unit_test`, `frontend_psg_audio_pump_unit_test`, `frontend_sdl3_cli_unit_test`),
  including the slow `hbf1xv_m24_zexall_system_test` re-run to completion, run directly/
  synchronously (not abandoned behind a background wait, per the dispatch's explicit instruction —
  the actual `ctest` invocation genuinely exceeds this environment's single-foreground-call time
  budget, a real platform constraint; the developer stayed on the task, independently monitored the
  background process's live CPU usage as a sanity check, and captured its real, completed output).
  SDL3-ON suite (a SECOND, dedicated build directory): **140/140** (134 shared, re-verified in this
  configuration too + 6 new SDL3-gated: `frontend_sdl3_app_headless_integration_test`,
  `frontend_sdl3_video_presenter_pixel_integration_test`,
  `frontend_sdl3_audio_presenter_integration_test`,
  `frontend_sdl3_input_mapper_integration_test` (71/71 mapped keys exhaustively `SDL_PushEvent`-
  injection tested, plus PAUSE/Speed-Controller/Ren-Sha-Turbo bindings and an unmapped-scancode
  regression guard), `frontend_sdl3_cli_session_integration_test` (real `roms/aleste.rom` +
  `disks/msxdos22.dsk` assets), `hbf1xv_m26_sdl3_system_test`), all entirely under
  `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`, zero `SDL_Delay` calls anywhere in any `ctest`
  execution path (grep-confirmed).

  Real openMSX A/B evidence (`docs/m26-frontend-evidence.md`): honestly, mostly **BLOCKED (N/A)** —
  neither engine's SDL/audio-hardware presentation output has a comparable introspection point
  (mirrors the established M21-computed-pixel-color and M25-PAUSE-Speed-Controller BLOCKED-
  sub-claim precedent). The one genuinely comparable sub-claim — input-mapping TABLE row/column
  correctness — is fully discharged WITHOUT a new A/B technique by the existing, already-verified
  M15 `KeyboardMatrix`/`JoystickPorts` contract tests plus M26's own exhaustive `SDL_PushEvent`-
  injection tests. Per Acceptance Criterion 10, this disposition does NOT gate closure. An explicit,
  honest, non-fabricated human-observed-only verification checklist (§4 of the evidence doc) is
  named for whichever human next launches the real, non-dummy-driver `sony_msx_sdl3.exe`.

  Full 34-row deferred-backlog review completed (`agent-protocol/state/deferred-backlog.md`) — C9 →
  IN-PROGRESS (M26 implementation complete, pending QA), target closes IN FULL at coordinator
  closure; E1 re-affirmed OPEN with an honest cross-reference note (confirms E1's own "paired with
  C9" candidate-owner note was prescient); all other 32 rows re-affirmed unchanged. Ledger status
  transition to DONE (M26) and any `v1.0.26` tag are reserved for the coordinator at closure.

  **QA (`docs/m26-qa-signoff.md`, RESP-M26-003) independently reproduced everything from TWO
  genuinely clean, from-scratch rebuilds, plus a THIRD, fully independent rebuild of the vendored
  SDL3 source to a brand-new install directory neither the developer nor coordinator had touched**
  — confirming the environment finding is genuinely reproducible, not a one-off fluke. Headless
  134/134 (including a fresh 30.8-minute ZEXALL/ZEXDOC re-run, byte-identical to every prior
  recorded run); SDL3-ON 139/139, dummy drivers set externally at the shell level. Cross-checked
  every one of the 20 new files' line counts against the implementation report's own tables (exact
  match on all), every cited SDL3 API/doc-comment against the actual vendored headers, and
  independently re-traced the developer's self-discovered `SDL_PollEvent` poll-batching sentinel
  finding to `SDL_events.c` directly. Launched the real `sony_msx_sdl3.exe` itself three times
  (cartridge, disk, `--help`) — all exit 0. Regenerated the committed PNG from the committed dump
  and confirmed a byte-identical SHA-256 match. **QA found zero findings of any severity** — the
  cleanest QA cycle of this entire session (every prior milestone's QA cycle found at least one
  Low-severity documentation-precision nit; this one, despite deliberately searching for exactly
  that class of issue, found none beyond one trivial, non-gating doc-comment imprecision
  (`run_frame_dump_demo()` says "256×212," the actual committed evidence is 256×192 — harmless,
  not tracked as actionable). **Sign-off: Pass** (clean, unconditional). Per the milestone's own
  standing directive, the coordinator proceeded through the release-decision/tag step without an
  additional human pause.
