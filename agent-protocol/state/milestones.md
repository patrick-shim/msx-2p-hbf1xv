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
- Details: CLOSED by human release decision on 2026-07-06 (REQ-M11-005); tagged git `v1.0.11`. Planner package `docs/m11-planner-package.md` (REQ-M11-002). Implementation REQ-M11-003 delivered all 6 slices (S1 bus-contract+CPU I/O seam+M1-cycle; S2 memory slot-decode #A8/#FFFF; S3 I/O dispatch + S1985 mirrors #9C-9F/#AC-AF; S4 thin S1985 backup-RAM ID 0xFE + mapper readback 100xxxxx + M1-wait; S5 system integration replacing MachineBus; S6 A/B parity) — `docs/m11-implementation-report.md`. QA sign-off REQ-M11-004 (`docs/m11-qa-signoff.md`) = PASS: QA-executed ctest 38/38 (+10 new), no regression (8 R-3 timing oracles recomputed to datasheet+M1-wait, hand-verified not weakened), and A/B parity independently REPRODUCED by QA — genuine EMPTY trace-diff over 15/15 real records vs openMSX 19.1 Sony_HB-F1XV with real <S1985> (`docs/m11-parity-trace-diff.md`, `build/qa_m11_A.txt`/`qa_m11_B.txt`). Forward obligation to M12: flip reset slot default #A8=0xFF -> #A8=0 (slot-0 BIOS) once ROMs populate slot 0 (R-1/A-2). Backup-RAM .sram persistence accepted out-of-scope (A-5/R-6). Per user instruction, autopilot STOPS here awaiting human release decision + go-ahead before M12.

## M12 (Kickoff 2026-07-06)

- Milestone ID: M12
- Title: RAM / ROM Memory Devices & Slot Population (VRAM deferred to M13)
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Scope: Implement the CPU-addressable memory devices and populate the M11 slot map per the Target Machine Specification and the openMSX HB-F1XV layout (`references/openmsx-21.0/src/memory/`, S1985 fact-sheet §4/§9): 64 KB main RAM as a memory-mapper device (openMSX slot 3-0, `#FC-#FF` segment registers, S1985 512 KB drive limit / HB-F1XV 64 KB population), and the ROM set — 32 KB BIOS/BASIC, 16 KB SUB-ROM (MSX-BASIC V3.0), 16 KB KANJI (KANJI BASIC + KANJI ROM), 16 KB DISK ROM. Per DEC-0002 the 128 KB VRAM is OWNED BY THE VDP and is OUT of M12 scope (delivered in M13). Wire these devices into the M11 slots/bus; asset loading uses local `bios/`/`roms/` (verified paths only — no fabricated provenance).
- Acceptance Criteria:
	- Memory-mapper RAM device (64 KB) with `#FC-#FF` segment registers, wired into slot 3-0, replaces the inert DRAM region as the CPU RAM backing.
	- ROM devices for BIOS/BASIC, SUB, KANJI, DISK implemented and mapped to their correct slots/pages per the HB-F1XV layout; missing local assets handled deterministically (documented, no fabrication).
	- System integration: CPU can fetch/execute from ROM and read/write mapper RAM through the M11 bus; boot reaches a defined checkpoint.
	- Evidence gates executed (validate-assets, checksum, Debug build, ctest all green).
	- A/B trace-diff vs openMSX over a CPU-visible RAM/ROM program (real diff; VRAM parity excluded, it is M13).
- Unit Tests Required: Deterministic mapper segment-register decode/readback, ROM read-only enforcement, slot-page mapping, and asset-absence handling tests.
- Integration Tests Required: Deterministic boot/run through ROM+mapper-RAM over the M11 bus to a checkpoint.
- Regression Scope: All prior suites remain passing; QA sign-off required before closure.
- Status: Planned

## M13 (Kickoff 2026-07-06)

- Milestone ID: M13
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
- Status: Planned
