# Requests Channel

Append-only handoff requests.

Use this format:

- Request ID:
- From Agent:
- To Agent:
- Milestone ID:
- Scope:
- Acceptance Criteria:
- Dependencies:
- Requested At:

- Request ID: REQ-M0-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M0
- Scope: Initialize M0 orchestration and produce planner-first delegation sequence aligned to project-baseline mission, in-scope boundaries, and build flow.
- Acceptance Criteria: Deliver orchestration sequencing, explicit planner handoff requirements, dependency graph, and protocol-complete transition prerequisites.
- Dependencies: agent-protocol/project-baseline.md, agent-protocol/guardrails.md, agent-protocol/templates/milestone-template.md
- Requested At: 2026-07-05T21:30:55.7257265+09:00

- Request ID: REQ-M0-002
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M0
- Scope: Initialize M0 orchestration refresh and produce planner-first delegation sequence with explicit asset and A/B evidence path requirements.
- Acceptance Criteria: Deliver orchestration sequencing, explicit planner handoff requirements, dependency graph, protocol-complete transition prerequisites, and evidence placeholders for `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, and `tools/openmsx-ab-smoke.ps1`/`docs/openmsx-ab-smoke.md` when parity applies.
- Dependencies: agent-protocol/project-baseline.md, agent-protocol/guardrails.md, agent-protocol/templates/milestone-template.md, .claude/commands/msx-kickoff.md
- Requested At: 2026-07-05T22:02:28.2508847+09:00

- Request ID: REQ-M0-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M0
- Scope: Produce the M0 planning package for deterministic Sony HB-F1XV execution, including architecture boundaries, dependency sequencing, and acceptance criteria mapped to baseline build/test flow.
- Acceptance Criteria: Planning package explicitly references `docs/`, `tools/`, `bios/`, and `roms/`; defines dependency order for implementation slices; includes protocol completeness prerequisites for transition to implementation; includes evidence gates for `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, and parity-conditional `tools/openmsx-ab-smoke.ps1` -> `docs/openmsx-ab-smoke.md`.
- Dependencies: REQ-M0-002 orchestration completion, agent-protocol/project-baseline.md, agent-protocol/guardrails.md, agent-protocol/state/current-phase.md, agent-protocol/state/milestones.md
- Requested At: 2026-07-05T22:18:00+09:00

- Request ID: REQ-M0-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M0
- Scope: Prepare implementation-readiness for first approved slice only (no feature implementation yet), including environment/tooling readiness checklist and deterministic test execution plan scaffold.
- Acceptance Criteria: Readiness output remains strictly dependent on completed planner package from REQ-M0-003; identifies first-slice prep tasks, required test harness touchpoints, and script-driven evidence collection hooks using `tools/validate-assets.ps1` and `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; references parity evidence path `docs/openmsx-ab-smoke.md` when behavior parity is in scope.
- Dependencies: REQ-M0-003 completed planner package, agent-protocol/project-baseline.md, agent-protocol/guardrails.md
- Requested At: 2026-07-05T22:18:30+09:00

- Request ID: REQ-M0-005
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M0
- Scope: Activate the M0 implementation-prep lane using accepted planner package output, produce concrete readiness checklist artifacts, and prepare first implementation slice execution handoff without crossing into feature implementation.
- Acceptance Criteria: References accepted planner package at `docs/m0-planner-package.md`; preserves deterministic test-first sequencing across `tests/unit/`, `tests/integration/`, and `tests/system/`; includes explicit evidence hooks for `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, and parity-conditional `tools/openmsx-ab-smoke.ps1` -> `docs/openmsx-ab-smoke.md`; includes concrete dependency list for implementation start.
- Dependencies: REQ-M0-003 completed planner package evidence, REQ-M0-004 dependency lock satisfied, agent-protocol/project-baseline.md, agent-protocol/guardrails.md, agent-protocol/state/current-phase.md
- Requested At: 2026-07-05T22:44:00+09:00

- Request ID: REQ-M0-006
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M0
- Scope: Execute regression-readiness and sign-off preparation for M0 based on completed planner and implementation-prep artifacts with fresh evidence gates.
- Acceptance Criteria: Validate protocol completeness for REQ-M0-003 and REQ-M0-005; verify deterministic readiness mapping across `tests/unit/`, `tests/integration/`, and `tests/system/`; confirm current-cycle evidence for `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, and `ctest --test-dir build -C Debug --output-on-failure`; provide sign-off recommendation and residual risks.
- Dependencies: REQ-M0-003 completed, REQ-M0-005 completed with fresh evidence, docs/m0-planner-package.md, docs/m0-implementation-prep.md, agent-protocol/project-baseline.md, agent-protocol/guardrails.md
- Requested At: 2026-07-05T22:25:03.8416334+09:00

- Request ID: REQ-M1-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M1
- Scope: Kick off M1 planning and first implementation slice sequencing after formal M0 closure, preserving deterministic and evidence-gated execution.
- Acceptance Criteria: Produce planner-first M1 delegation package with subsystem scope, dependency sequencing, and acceptance criteria for first implementation slice; include deterministic test obligations and evidence gate hooks (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, build/test flow); keep parity evidence conditional via `tools/openmsx-ab-smoke.ps1` and `docs/openmsx-ab-smoke.md` when behavior parity is in scope.
- Dependencies: REQ-M0-006 completed QA signoff, docs/m0-qa-signoff.md, agent-protocol/project-baseline.md, agent-protocol/guardrails.md, agent-protocol/state/milestones.md
- Requested At: 2026-07-05T22:30:58.0827941+09:00

- Request ID: REQ-M1-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M1
- Scope: Produce M1 planning package for the first behavior-affecting implementation slice with deterministic contract boundaries and dependency sequence.
- Acceptance Criteria: Define first-slice scope across `src/core/`, `src/devices/`, and `src/machine/`; include deterministic unit/integration/system test obligations; include risk/assumption verification and acceptance mapping to build/test flow; include evidence-gate obligations for `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, and `ctest --test-dir build -C Debug --output-on-failure`; include conditional parity workflow via `tools/openmsx-ab-smoke.ps1` and `docs/openmsx-ab-smoke.md` when applicable.
- Dependencies: REQ-M1-001 accepted kickoff, docs/m0-planner-package.md, docs/m0-qa-signoff.md, agent-protocol/project-baseline.md, agent-protocol/guardrails.md, agent-protocol/state/current-phase.md
- Requested At: 2026-07-05T22:32:21.4302014+09:00

- Request ID: REQ-M1-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M1
- Scope: Prepare implementation-execution readiness for first M1 slice, queued behind planner output.
- Acceptance Criteria: Do not start feature implementation before REQ-M1-002 completion; prepare build/test execution checklist, target files shortlist, and deterministic test harness touchpoints; include evidence hook paths for assets, checksums, build, tests, and conditional parity A/B.
- Dependencies: REQ-M1-002 completed planner package, agent-protocol/project-baseline.md, agent-protocol/guardrails.md
- Requested At: 2026-07-05T22:32:21.4302014+09:00

- Request ID: REQ-M1-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M1
- Scope: Execute first behavior-affecting implementation slice after planner completion and produce test-backed evidence artifacts.
- Acceptance Criteria: Implement approved first slice and update deterministic tests; execute evidence gates (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`); publish implementation report under `docs/`.
- Dependencies: REQ-M1-002 completed planner package, REQ-M1-003 completed readiness handoff, agent-protocol/project-baseline.md, agent-protocol/guardrails.md
- Requested At: 2026-07-05T22:36:09.1287078+09:00

- Request ID: REQ-M1-005
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M1
- Scope: Perform regression-readiness review and provide signoff recommendation for M1 first-slice completion.
- Acceptance Criteria: Verify protocol ordering and evidence for REQ-M1-002/003/004; verify deterministic tests and build status; validate evidence artifacts and provide closure recommendation with residual risks.
- Dependencies: REQ-M1-004 completed implementation evidence, docs/m1-planner-package.md, docs/m1-implementation-report.md, agent-protocol/project-baseline.md, agent-protocol/guardrails.md
- Requested At: 2026-07-05T22:36:09.1287078+09:00

- Request ID: REQ-M2-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M2
- Scope: Kick off M2 deterministic scheduler batching slice and completion loop through QA signoff.
- Acceptance Criteria: Planner/developer/QA sequence completed with evidence-gated validation.
- Dependencies: M1 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M2-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M2
- Scope: Produce scheduler batching slice plan.
- Acceptance Criteria: Define scope, deterministic tests, and evidence gates.
- Dependencies: REQ-M2-001 accepted.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M2-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M2
- Scope: Implement scheduler batching slice.
- Acceptance Criteria: Code/tests updated with passing evidence gates and implementation report.
- Dependencies: REQ-M2-002 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M2-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M2
- Scope: Perform M2 QA signoff.
- Acceptance Criteria: Verify deterministic coverage and evidence gates; provide signoff recommendation.
- Dependencies: REQ-M2-003 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M3-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M3
- Scope: Kick off M3 CPU bus word-access slice and completion loop through QA signoff.
- Acceptance Criteria: Planner/developer/QA sequence completed with evidence-gated validation.
- Dependencies: M2 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M3-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M3
- Scope: Produce CPU bus word-access slice plan.
- Acceptance Criteria: Define deterministic behavior and test/evidence obligations.
- Dependencies: REQ-M3-001 accepted.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M3-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M3
- Scope: Implement CPU bus word-access slice.
- Acceptance Criteria: Code/tests updated with passing evidence gates and implementation report.
- Dependencies: REQ-M3-002 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M3-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M3
- Scope: Perform M3 QA signoff.
- Acceptance Criteria: Verify deterministic coverage and evidence gates; provide signoff recommendation.
- Dependencies: REQ-M3-003 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M4-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M4
- Scope: Kick off M4 multi-frame machine stepping slice and completion loop through QA signoff.
- Acceptance Criteria: Planner/developer/QA sequence completed with evidence-gated validation.
- Dependencies: M3 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M4-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M4
- Scope: Produce multi-frame stepping slice plan.
- Acceptance Criteria: Define deterministic behavior and test/evidence obligations.
- Dependencies: REQ-M4-001 accepted.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M4-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M4
- Scope: Implement multi-frame stepping slice.
- Acceptance Criteria: Code/tests updated with passing evidence gates and implementation report.
- Dependencies: REQ-M4-002 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M4-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M4
- Scope: Perform M4 QA signoff.
- Acceptance Criteria: Verify deterministic coverage and evidence gates; provide signoff recommendation.
- Dependencies: REQ-M4-003 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M5-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M5
- Scope: Kick off M5 boot invariant slice and completion loop through QA signoff.
- Acceptance Criteria: Planner/developer/QA sequence completed with evidence-gated validation.
- Dependencies: M4 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M5-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M5
- Scope: Produce boot invariant slice plan.
- Acceptance Criteria: Define deterministic behavior and test/evidence obligations.
- Dependencies: REQ-M5-001 accepted.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M5-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M5
- Scope: Implement boot invariant slice.
- Acceptance Criteria: Code/tests updated with passing evidence gates and implementation report.
- Dependencies: REQ-M5-002 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M5-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M5
- Scope: Perform M5 QA signoff.
- Acceptance Criteria: Verify deterministic coverage and evidence gates; provide signoff recommendation.
- Dependencies: REQ-M5-003 completed.
- Requested At: 2026-07-05T22:44:26.3092259+09:00

- Request ID: REQ-M6-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M6
- Scope: Kick off M6 deterministic scheduler target-advance slice and completion loop through QA signoff.
- Acceptance Criteria: Planner/developer/QA sequence completed with evidence-gated validation.
- Dependencies: M5 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M6-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M6
- Scope: Produce scheduler target-advance slice plan.
- Acceptance Criteria: Define deterministic behavior and test/evidence obligations.
- Dependencies: REQ-M6-001 accepted.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M6-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M6
- Scope: Implement scheduler target-advance slice.
- Acceptance Criteria: Code/tests updated with passing evidence gates and implementation report.
- Dependencies: REQ-M6-002 completed.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M6-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M6
- Scope: Perform M6 QA signoff.
- Acceptance Criteria: Verify deterministic coverage and evidence gates; provide signoff recommendation.
- Dependencies: REQ-M6-003 completed.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M7-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M7
- Scope: Kick off M7 CPU bus big-endian word-access slice and completion loop through QA signoff.
- Acceptance Criteria: Planner/developer/QA sequence completed with evidence-gated validation.
- Dependencies: M6 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M7-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M7
- Scope: Produce CPU bus big-endian slice plan.
- Acceptance Criteria: Define deterministic behavior and test/evidence obligations.
- Dependencies: REQ-M7-001 accepted.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M7-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M7
- Scope: Implement CPU bus big-endian slice.
- Acceptance Criteria: Code/tests updated with passing evidence gates and implementation report.
- Dependencies: REQ-M7-002 completed.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M7-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M7
- Scope: Perform M7 QA signoff.
- Acceptance Criteria: Verify deterministic coverage and evidence gates; provide signoff recommendation.
- Dependencies: REQ-M7-003 completed.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M8-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M8
- Scope: Kick off M8 machine target-cycle slice and completion loop through QA signoff.
- Acceptance Criteria: Planner/developer/QA sequence completed with evidence-gated validation.
- Dependencies: M7 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M8-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M8
- Scope: Produce machine target-cycle slice plan.
- Acceptance Criteria: Define deterministic behavior and test/evidence obligations.
- Dependencies: REQ-M8-001 accepted.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M8-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M8
- Scope: Implement machine target-cycle slice.
- Acceptance Criteria: Code/tests updated with passing evidence gates and implementation report.
- Dependencies: REQ-M8-002 completed.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M8-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M8
- Scope: Perform M8 QA signoff.
- Acceptance Criteria: Verify deterministic coverage and evidence gates; provide signoff recommendation.
- Dependencies: REQ-M8-003 completed.
- Requested At: 2026-07-05T22:48:56.5915270+09:00

- Request ID: REQ-M9-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M9
- Scope: Kick off full Z80A CPU implementation milestone for Sony HB-F1XV with deep CPU architecture validation and deterministic-first execution.
- Acceptance Criteria: Planner/developer/QA sequence established with explicit opcode-family completion targets and interrupt architecture fidelity requirements.
- Dependencies: M8 closure and protocol baseline/guardrails.
- Requested At: 2026-07-05T23:00:00+09:00

- Request ID: REQ-M9-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M9
- Scope: Produce full Z80A implementation plan including opcode-family sequencing, interrupt architecture requirements, and deterministic test oracle strategy.
- Acceptance Criteria: Planner package must define acceptance gates for unprefixed/CB/ED/DD-FD/DDCB-FDCB completion and IM0/IM1/IM2/NMI fidelity.
- Dependencies: REQ-M9-001 accepted.
- Requested At: 2026-07-05T23:00:10+09:00

- Request ID: REQ-M9-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M9
- Scope: Execute M9 CPU implementation slices with deterministic tests and evidence gates.
- Acceptance Criteria: Deliver code, tests, and implementation report with passing evidence gates and explicit gap report when full opcode coverage is not yet complete.
- Dependencies: REQ-M9-002 completed.
- Requested At: 2026-07-05T23:00:20+09:00

- Request ID: REQ-M9-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M9
- Scope: Assess M9 completion status versus full opcode and interrupt-architecture requirements and provide signoff recommendation.
- Acceptance Criteria: Provide Completed/Partial/Blocked verdict with actionable gap list and closure recommendation.
- Dependencies: REQ-M9-003 completed.
- Requested At: 2026-07-05T23:00:30+09:00

- Request ID: REQ-M9-005
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M9
- Scope: Execute M9 slice M9-S2 per `docs/m9-planner-package.md` — complete the unprefixed instruction table and the full CB (bit/shift/rotate) family with deterministic flag and T-state semantics. Do not fold ED/DD-FD/DDCB-FDCB into this slice.
- Acceptance Criteria: Unprefixed + CB opcode coverage complete and test-backed; deterministic unit vectors assert register/flag/memory/PC/SP and T-state counts (taken/not-taken); existing suites remain green; implementation report published under `docs/` with an explicit residual-gap list (ED full set, DD/FD, DDCB/FDCB, IM0/IM2 fidelity, M9-specific parity evidence). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`; behavior-affecting so also `tools/openmsx-ab-smoke.ps1` -> `docs/openmsx-ab-smoke.md`.
- Dependencies: REQ-M9-002 completed planner package, REQ-M9-003 baseline slice, REQ-M9-004 QA gap list, docs/m9-planner-package.md, docs/m9-implementation-report.md, docs/m9-qa-signoff.md.
- Requested At: 2026-07-05T23:20:00+09:00

- Request ID: REQ-M9-006
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M9
- Scope: Re-assess M9 completion status after slice M9-S2 (unprefixed table + full CB family) landed under REQ-M9-005. Verify evidence gates and deterministic coverage; update the risk-ranked gap list; provide an updated verdict (Completed / Partial / Blocked) with closure recommendation.
- Acceptance Criteria: Confirm REQ-M9-005 developer evidence (ctest count, build result, asset/checksum gates); verify new unit/integration suites are deterministic and passing; assess remaining opcode-family gaps (ED full set, DD/FD, DDCB/FDCB, IM0/IM2 fidelity, milestone-specific parity evidence) against M9 acceptance criteria; record verdict in `docs/m9-qa-signoff.md`. M9 must not be marked Done while opcode families remain incomplete.
- Dependencies: REQ-M9-005 completed developer evidence, docs/m9-implementation-report.md, docs/asset-checksums.txt, docs/m9-planner-package.md.
- Requested At: 2026-07-05T23:40:00+09:00

- Request ID: REQ-M9-007
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M9
- Scope: Execute M9 slice M9-S3 per `docs/m9-planner-package.md` — implement the full ED-prefixed instruction family, resolving QA blocker R1. Do not fold DD/FD or DDCB/FDCB into this slice.
- Acceptance Criteria: Full ED family implemented — block ops (LDI/LDIR/LDD/LDDR/CPI/CPIR/CPD/CPDR), 16-bit ADC/SBC HL,rp with correct flags, NEG, IN r,(C)/OUT (C),r, LD A,I / LD A,R / LD I,A / LD R,A (incl. P/V=IFF2 semantics), RRD/RLD, LD (nn),rp / LD rp,(nn); existing RETN/RETI/IM0/1/2 preserved. Deterministic unit vectors assert register/flag (S,Z,Y,H,X,P/V,N,C)/memory/PC/SP and T-state counts including block-op repeat vs terminate timing. Existing suites remain green. Implementation report updated under `docs/` with an explicit residual-gap list (DD/FD, DDCB/FDCB, IM0/IM2 fidelity, parity evidence). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`; behavior-affecting so also `tools/openmsx-ab-smoke.ps1` -> `docs/openmsx-ab-smoke.md`.
- Dependencies: REQ-M9-005 completed, REQ-M9-006 QA re-assessment gap list, docs/m9-planner-package.md, docs/m9-implementation-report.md, docs/m9-qa-signoff.md.
- Requested At: 2026-07-06T00:05:00+09:00

- Request ID: REQ-M9-008
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M9
- Scope: Execute M9 slice M9-S4 per `docs/m9-planner-package.md` — implement the DD/FD indexed prefixes and the DDCB/FDCB indexed bit/rotate families, resolving QA blockers R2 and R3. Do not change IM0/IM2 vector fidelity (S5).
- Acceptance Criteria: DD/FD prefixes correctly retarget HL->IX/IY, H/L->IXH/IXL/IYH/IYL for the affected opcode subset, and implement (IX+d)/(IY+d) displacement addressing with correct T-state penalties; unrecognized/redundant DD/FD prefixes follow documented Z80 behavior (prefix chaining / fallthrough) rather than silently mis-decoding the next byte; DDCB/FDCB implement the full BIT/RES/SET/rotate/shift set on (IX+d)/(IY+d) including undocumented result-copy-to-register variants and correct timing (e.g., 20T/23T forms). Deterministic unit vectors assert register/flag/memory/PC/SP and exact T-states; at least one machine/integration vector exercises an indexed program with a hardcoded T-state oracle. Existing suites remain green. Implementation report updated under `docs/` with residual-gap list (IM0/IM2 fidelity, opcode-trace parity). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`; behavior-affecting so also `tools/openmsx-ab-smoke.ps1` -> `docs/openmsx-ab-smoke.md`.
- Dependencies: REQ-M9-007 completed, REQ-M9-006 QA gap list (R2/R3), docs/m9-planner-package.md, docs/m9-implementation-report.md, docs/m9-qa-signoff.md.
- Requested At: 2026-07-06T00:30:00+09:00

- Request ID: REQ-M9-009
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M9
- Scope: Execute M9 slice M9-S5 per `docs/m9-planner-package.md` — raise IM0/IM2 interrupt acknowledge/vector-source fidelity (QA blocker R4) and produce real opcode-trace parity evidence vs openMSX (QA blocker R5). Final M9 implementation slice.
- Acceptance Criteria: IM0 executes an acknowledge-supplied instruction (bus-provided opcode, defaulting to RST 38h only when no device vector is present) rather than hardcoding IM1's 0x0038; IM2 forms the vector table address from I:bus-vector and reads the ISR address from memory with correct T-states; NMI and RETN/RETI IFF handling preserved; deterministic unit/integration tests assert IM0/IM1/IM2/NMI acknowledge sequences, vectoring, and timing. For R5: implement or wire a real opcode-trace parity check vs openMSX on WSL (`/usr/bin/openmsx`) via `tools/openmsx-ab-smoke.ps1` (extend it if needed) and capture ACTUAL trace-diff results into `docs/openmsx-ab-smoke.md`. If a faithful trace comparison cannot be produced in this environment, do NOT fabricate parity — instead document the exact limitation, what was attempted, and the concrete verification action, and surface it as a candidate `decisions.md` waiver for coordinator/QA. Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`. Existing suites remain green. Implementation report updated under `docs/`.
- Dependencies: REQ-M9-008 completed, REQ-M9-006 QA gap list (R4/R5), docs/m9-planner-package.md, docs/m9-implementation-report.md, docs/m9-qa-signoff.md.
- Requested At: 2026-07-06T01:00:00+09:00

- Request ID: REQ-M9-010
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M9
- Scope: Final comprehensive QA sign-off assessment for milestone M9 (Full Z80A CPU Core) after all implementation slices M9-S1..M9-S5. Produce the milestone closure verdict.
- Acceptance Criteria: Independently verify current evidence gates (rebuild + re-run `ctest --test-dir build -C Debug --output-on-failure`, record actual pass/fail count); confirm opcode-family completeness (unprefixed + CB from S2, full ED from S3, DD/FD + DDCB/FDCB from S4) and IM0/IM1/IM2/NMI + RETN/RETI interrupt fidelity from S5 against M9 acceptance criteria in `agent-protocol/state/milestones.md`; assess the status of R5 (opcode-trace parity evidence), which the developer reports UNRESOLVED with a documented technical blocker and a recommended `decisions.md` waiver. Provide a final verdict of Pass / Conditional Pass / Fail with an explicit statement of exactly what (if anything) blocks full closure and whether an approved `decisions.md` waiver for R5 is required to reach Done. Record verdict in `docs/m9-qa-signoff.md` (append dated final section).
- Dependencies: REQ-M9-009 completed developer evidence, docs/m9-implementation-report.md, docs/asset-checksums.txt, docs/openmsx-ab-smoke.md, docs/m9-planner-package.md.
- Requested At: 2026-07-06T01:30:00+09:00

- Request ID: REQ-M10-001
- From Agent: MSX Master Agent
- To Agent: MSX Orchestration Agent
- Milestone ID: M10
- Scope: Kick off M10 (Debug/Trace Infrastructure & openMSX Opcode-Trace Parity) after M9 closure (REQ-M9-011), with planner-first sequencing per decision `DEC-0001`. Establish planner/developer/QA chain to deliver debug full-state dump, execution-event logging, CPU trace-export, machine slot/RAM/SRAM/VRAM wiring, the real openMSX opcode-trace parity harness, and supporting `tools/` converters; resolve deferred M9 blocker R5.
- Acceptance Criteria: Planner-first delegation issued; M10 acceptance gates require genuine (non-fabricated) trace-diff parity evidence and QA sign-off before closure; autopilot stops-and-asks on any genuine blocker that would require out-of-M10 scope (e.g., VDP/VRAM/slot subsystems warranting their own milestone).
- Dependencies: M9 closure (REQ-M9-011), DEC-0001, agent-protocol/project-baseline.md, agent-protocol/guardrails.md, agent-protocol/state/milestones.md.
- Requested At: 2026-07-06T02:31:00+09:00

- Request ID: REQ-M10-002
- From Agent: MSX Orchestration Agent
- To Agent: MSX Planner Agent
- Milestone ID: M10
- Scope: Produce the M10 planning package for Debug/Trace Infrastructure & openMSX Opcode-Trace Parity. Define scope boundaries, dependency sequencing, and a deterministic slice plan across: (a) full-state debug dump (CPU registers/state, DRAM, SRAM, VRAM) and execution-event logging under `debug/traces/` and `debug/logs/`; (b) a deterministic CPU trace-export facility; (c) the minimum machine slot/RAM/SRAM/VRAM wiring needed to run a comparable openMSX program to a defined checkpoint; (d) the openMSX opcode-trace / per-instruction parity harness (extending `tools/openmsx-ab-smoke.ps1` or a dedicated `tools/` helper) that produces a real trace-diff artifact; (e) supporting `tools/` converters (e.g., memory->png, memory->audio) as needed. Explicitly identify any prerequisite that would require scope beyond M10 (e.g., VDP/VRAM/slot subsystems) as a candidate separate milestone and flag it for coordinator decision rather than silently expanding M10.
- Acceptance Criteria: Planner package defines slice sequencing with deterministic unit/integration/system test obligations and evidence-gate mapping (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`, and real `tools/openmsx-ab-smoke.ps1` -> `docs/openmsx-ab-smoke.md` parity evidence). Package must state the R5-resolution acceptance test explicitly and must NOT permit any parity claim without a genuine trace-diff. Risk/assumption verification actions included. Durable artifact at `docs/m10-planner-package.md`. No production code.
- Dependencies: REQ-M10-001 accepted kickoff, DEC-0001, docs/m9-qa-signoff.md, docs/m9-implementation-report.md, tools/openmsx-ab-smoke.ps1, docs/openmsx-ab-smoke.md, agent-protocol/project-baseline.md, agent-protocol/guardrails.md.
- Requested At: 2026-07-06T02:32:00+09:00

- Request ID: REQ-M10-003
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M10
- Scope: Execute M10 slice M10-S1 per `docs/m10-planner-package.md` — implement a deterministic CPU trace-export facility: an off-by-default, nullable per-instruction observer hook in the Z80A core (`src/devices/cpu/`) that records PC (pre), opcode byte(s), full register file (A,F with each flag bit, B,C,D,E,H,L,IX,IY,SP,I,R), IFF1/IFF2, IM, and instruction + cumulative T-states, delivered to a sink owned by the machine layer (`src/machine/`). Do not add slot/mapper/VDP/device behavior (later slices / out of scope).
- Acceptance Criteria: Trace-export is deterministic and has zero effect on emulation state when disabled (default off); enabling it produces a stable, diffable per-instruction record. Deterministic unit tests assert the recorded fields match known instruction sequences and that enabling/disabling does not perturb CPU state or cycle counts. Existing suites remain green. Implementation report at `docs/m10-implementation-report.md` (create; dated M10-S1 section) with actual evidence-gate output and a residual-slice list (S2..S5). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`. (openMSX parity harness is S4, not this slice.)
- Dependencies: REQ-M10-002 completed planner package, docs/m10-planner-package.md, src/devices/cpu/z80a_*.{h,cpp}, src/machine/, agent-protocol/guardrails.md.
- Requested At: 2026-07-06T02:45:00+09:00

- Request ID: REQ-M10-004
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M10
- Scope: Execute M10 slice M10-S2 per `docs/m10-planner-package.md` — wire the minimum INERT, dumpable/loadable memory regions in the machine layer (`src/machine/`) with capacities matching the strict Target Machine Specification in `agent-protocol/project-baseline.md`: DRAM = 64 KB, VRAM = 128 KB. Add an FM-PAC battery SRAM inert region (standard FM-PAC SRAM size; label the exact size as an `Assumption:` with a verification action, since the spec table does not list an SRAM capacity and true FM-PAC behavior is deferred). These regions are pure storage only — NO slot/subslot/mapper decoding, NO V9958 VDP behavior, NO FM-PAC device behavior, NO I/O bus. Those are separate milestones (planner DP-1..DP-5); if any slice work appears to require them, STOP AND ASK rather than expanding scope.
- Acceptance Criteria: DRAM (64 KB) and VRAM (128 KB) inert regions plus an FM-PAC SRAM inert region exist as addressable, readable/writable, dumpable/loadable byte buffers owned by the machine, deterministically zero-initialized at reset. Existing 64K CPU-visible memory behavior and all existing tests remain unchanged/green (do not regress the M9 CPU or M10-S1 trace). Deterministic unit/integration tests assert region sizes, zero-init, read/write round-trip, and load/dump round-trip. Implementation report updated (`docs/m10-implementation-report.md`, dated M10-S2 section) with actual evidence-gate output and residual-slice list (S3, S4, S5). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`.
- Dependencies: REQ-M10-003 completed (S1), docs/m10-planner-package.md, agent-protocol/project-baseline.md (Target Machine Specification), src/machine/hbf1xv_machine.{h,cpp}.
- Requested At: 2026-07-06T03:05:00+09:00

- Request ID: REQ-M10-005
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M10
- Scope: Execute M10 slice M10-S3 per `docs/m10-planner-package.md` — full-state debug dump and execution-event logging. Serialize the complete machine state (CPU registers/state incl. shadow pairs, IX/IY, I/R, IFF1/IFF2, IM; DRAM 64 KB; SRAM; VRAM 128 KB) to files under `debug/traces/`, and write execution-event logs under `debug/logs/`. Build on the M10-S1 trace facility and M10-S2 memory regions. Do not add device behavior (DP-1..DP-5 remain separate milestones; STOP AND ASK if genuinely required).
- Acceptance Criteria: A deterministic full-state dump API writes CPU + DRAM + SRAM + VRAM content to `debug/traces/` in a stable, reproducible format (byte-identical across two identical runs); execution-event logging writes to `debug/logs/` deterministically (no wall-clock timestamps in the golden content, or timestamps excluded from the determinism assertion). Directory creation under `debug/` is handled (created if absent). The CPU per-instruction trace (S1) can be flushed to `debug/traces/`. Deterministic unit/integration tests assert dump content correctness (register values, region contents), reproducibility, and that dumping does not perturb machine state. Existing suites remain green. Implementation report updated (`docs/m10-implementation-report.md`, dated M10-S3 section) with actual evidence-gate output and residual-slice list (S4, S5). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`.
- Dependencies: REQ-M10-003 (S1) + REQ-M10-004 (S2) completed, docs/m10-planner-package.md, src/machine/, src/devices/cpu/z80a_trace.h.
- Requested At: 2026-07-06T04:40:00+09:00

- Request ID: REQ-M10-006
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M10
- Scope: Execute M10 slice M10-S4 per `docs/m10-planner-package.md` — build the openMSX opcode-trace / per-instruction parity harness that resolves QA blocker R5. Use a committed, fixed, RAM-only, I/O-free Z80 parity program (under `tests/parity/`) run instruction-by-instruction to a defined checkpoint on BOTH this emulator (via the M10-S1 trace-export) and openMSX 19.1 on WSL (`/usr/bin/openmsx`), then diff the per-instruction architectural state (PC, opcode, A,F + all flag bits, B,C,D,E,H,L,IX,IY,SP,I,R,IFF1,IFF2,IM). Extend `tools/openmsx-ab-smoke.ps1` or add a dedicated `tools/` helper. Capture the REAL trace-diff into `docs/openmsx-ab-smoke.md` and/or a dedicated `docs/m10-parity-trace-diff.md`.
- Acceptance Criteria: A genuine captured per-instruction trace-diff artifact exists and shows architectural-state parity (empty diff for PC/opcode/all registers/flags) over the parity program to the checkpoint; cycle/T-state counts compared and reported SEPARATELY with declared handling (MSX M1 wait-states may make exact cycle parity structurally impossible — architectural-state parity is the gate). HARD RULE: NO parity may be claimed, recorded, or marked resolved without a genuine captured diff — availability/capability output is NOT parity evidence. If openMSX genuinely cannot be driven to single-step the RAM-only program (drivability blocker), or faithful parity would require out-of-M10 scope (real BIOS boot / VDP / slot decoding — planner DP-1/DP-2), STOP AND ASK: report the exact blocker, what was attempted, and the verification action, and do NOT fabricate or expand scope. Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`, and (this slice) real `tools/openmsx-ab-smoke.ps1` -> `docs/openmsx-ab-smoke.md`. Implementation report updated (`docs/m10-implementation-report.md`, dated M10-S4 section) with the true R5 status.
- Dependencies: REQ-M10-003 (S1) + REQ-M10-004 (S2) + REQ-M10-005 (S3) completed, docs/m10-planner-package.md, tools/openmsx-ab-smoke.ps1, docs/openmsx-ab-smoke.md, docs/m9-qa-signoff.md (R5 blocker history).
- Requested At: 2026-07-06T05:20:00+09:00

- Request ID: REQ-M10-007
- From Agent: MSX Orchestration Agent
- To Agent: MSX Developer Agent
- Milestone ID: M10
- Scope: Execute M10 slice M10-S5 per `docs/m10-planner-package.md` — deliver `tools/` converters that transform dumped memory buffers into inspectable media: memory->PNG (e.g., a raw byte/bitplane visualization of a VRAM or DRAM dump region) and memory->audio (e.g., raw sample bytes / PSG-or-PCM-style buffer -> WAV). Operate over the M10-S3 dump artifacts (`debug/traces/` region dumps or the machine's region buffers); do NOT implement V9958 rendering or real audio synthesis (those are DP-2 / device milestones). Python and/or PowerShell in `tools/` per baseline.
- Acceptance Criteria: Deterministic converters exist in `tools/` with documented input format (a region dump) and output (PNG / WAV); given identical input they produce byte-identical output (no timestamps/nondeterminism). A deterministic test or self-check validates each converter on a small fixed input (golden output hash or dimensions/format assertions). No external network deps; standard/self-contained libraries only (if PNG/WAV need a lib, prefer stdlib-only encoders to keep determinism and avoid heavy deps — document the choice). Existing suites remain green. Implementation report updated (`docs/m10-implementation-report.md`, dated M10-S5 section) marking M10 implementation slices complete and R5 resolved (per S4). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`. If a converter genuinely needs device behavior (real VDP/PSG) to be meaningful, keep it a raw/inert visualization and note the limitation as an `Assumption:` rather than STOP — this slice is low-risk and should not require a scope decision.
- Dependencies: REQ-M10-005 (S3) completed, docs/m10-planner-package.md, src/machine/debug_dump.h, debug/ dump format.
- Requested At: 2026-07-06T06:00:00+09:00

- Request ID: REQ-M10-008
- From Agent: MSX Orchestration Agent
- To Agent: MSX QA Agent
- Milestone ID: M10
- Scope: Final comprehensive QA sign-off for milestone M10 (Debug/Trace Infrastructure & openMSX Opcode-Trace Parity) after slices M10-S1..M10-S5. Produce the milestone closure verdict. Critical focus: independently validate the R5 opcode-trace parity claim — do NOT accept the captured diff at face value.
- Acceptance Criteria: Independently verify evidence gates (rebuild + re-run `ctest --test-dir build -C Debug --output-on-failure`, record actual pass/fail count). Verify each slice against M10 acceptance criteria: S1 deterministic non-perturbing trace-export; S2 inert DRAM 64KB/VRAM 128KB/SRAM regions matching the Target Machine Specification; S3 deterministic full-state dump + `debug/` logging; S4 the R5 parity harness; S5 deterministic `tools/` converters. For R5 specifically: inspect `docs/m10-parity-trace-diff.md`, `tools/openmsx-trace-parity.ps1`, `tools/trace-diff.py`, `tests/parity/`, and the captured traces; confirm the diff is a GENUINE per-instruction architectural comparison (PC/opcode/all registers/flags) vs openMSX and not a stub or self-referential compare; if feasible re-run the harness and confirm the empty-diff result and determinism; assess whether the bounded RAM-only 48-instruction program is a sufficient R5 acceptance per `docs/m10-planner-package.md` (which defined R5 as per-instruction architectural parity over the committed parity program, cycles reported separately). Provide a final verdict of Pass / Conditional Pass / Fail with explicit statement of what is met vs outstanding, and whether M10 (and thus the DEC-0001 R5 obligation) can close. Record verdict in `docs/m10-qa-signoff.md`.
- Dependencies: REQ-M10-003..007 completed, docs/m10-implementation-report.md, docs/m10-parity-trace-diff.md, docs/m10-planner-package.md, docs/asset-checksums.txt, docs/openmsx-ab-smoke.md.
- Requested At: 2026-07-06T06:30:00+09:00

---

- Request ID: REQ-M11-001
- From: MSX Master Agent (coordinator)
- To: Protocol ledger (kickoff)
- Milestone ID: M11
- Type: Milestone kickoff
- Scope: Kick off M11 (S1985 "MSX-ENGINE" Chipset & Full System Bus) per DEC-0002. First milestone of the three-milestone autopilot run (M11 chipset+bus, M12 RAM/ROM memory, M13 V9958 VDP). Human directive of 2026-07-06 requested "M10 chipset / M11 memory / M12 VDP"; remapped +1 to M11/M12/M13 because M10 is already closed (append-only ledger). Grounding: `agent-protocol/project-baseline.md`, `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md`, openMSX 21.0 source (behavior reference only; GPL — never copied into src/).
- Acceptance Criteria: M11 milestone entry present in `state/milestones.md` (met); DEC-0002 recorded (met); planner-first sequencing enforced.
- Dependencies: M10 Done; DEC-0002.
- Requested At: 2026-07-05T18:10:00+09:00

- Request ID: REQ-M11-002
- From: MSX Master Agent (coordinator)
- To: MSX Planner Agent
- Milestone ID: M11
- Type: Planning package
- Scope: Produce the M11 planning package (durable artifact `docs/m11-planner-package.md`, no production code). Define scope boundaries, dependency sequencing, and a deterministic slice plan for: (a) the full HB-F1XV memory bus with 4-page primary-slot decode (PPI port `#A8`) and `#FFFF` secondary/sub-slot decode, replacing the trivial flat-DRAM `MachineBus` in `src/machine/hbf1xv_machine.*`; (b) the I/O bus with per-port device dispatch and the S1985 port mirrors `#98-9B`→`#9C-9F` and `#A8-AB`→`#AC-AF`; (c) the thin S1985 engine layer — 16-byte backup RAM on switched-I/O device ID `0xFE` (address/data/pattern/color regs, fact-sheet §6), mapper-readback base `0x80`/mask `0x1F` (`100xxxxx`, §4/§10), and the +1 M1 opcode-fetch wait state (§8); (d) system integration wiring CPU/scheduler onto the new bus with existing inert DRAM as the slot-3-0 backing. Decide and document the `src/` folder/file placement (authority to add/rearrange per `src/CLAUDE.md`; e.g. `src/core/` bus contracts vs `src/devices/chipset/` engine vs `src/machine/` wiring) and state the seams that PSG/RTC (out-of-scope later milestones) and M12 memory devices / M13 VDP will plug into. Explicitly scope OUT PSG/YM2149 and RTC/RP5C01 device internals. Ground every behavior claim in a concrete `references/...` path.
- Acceptance Criteria: Package defines slice sequencing with per-slice deterministic unit/integration/system test obligations and evidence-gate mapping (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`, and a real openMSX A/B trace-diff → `docs/`). States the A/B acceptance test explicitly and forbids any parity claim without a genuine trace-diff. Documents chosen `src/` placement and the cross-milestone seams. Risk/assumption items each carry a verification action. No production code.
- Dependencies: DEC-0002; `agent-protocol/project-baseline.md`; `agent-protocol/guardrails.md`; `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md`; openMSX 21.0 source; current `src/` tree (`src/core/bus.h`, `src/machine/hbf1xv_machine.*`, `src/devices/`).
- Requested At: 2026-07-05T18:10:00+09:00

- Request ID: REQ-M11-003
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M11
- Type: Implementation (full M11 slice plan S1..S6)
- Scope: Implement the M11 slice plan defined in `docs/m11-planner-package.md` end to end: S1 bus-contract extension + CPU I/O seam + M1-cycle surfacing; S2 memory slot-decode fabric (`#A8` primary + `#FFFF` secondary); S3 I/O dispatch fabric + S1985 mirrors (`#98-9B`→`#9C-9F`, `#A8-AB`→`#AC-AF`); S4 thin S1985 layer (switched-I/O `#40-4F`, backup RAM device ID 0xFE per fact-sheet §6, mapper readback `100xxxxx`, M1-wait helper); S5 system integration (compose `SystemBus`, drop `MachineBus`, DRAM as slot 3-0, apply M1 wait to scheduler); S6 real openMSX A/B trace-diff. Place files per the planner's src/ decision (bus contracts in `src/core/`, engine/decode in `src/devices/chipset/`, CPU seam in `src/devices/cpu/`, wiring in `src/machine/`). Add all new .cpp to the root `add_library(sony_msx_core ...)`. Ground behavior in concrete `references/...` paths; NEVER copy openMSX/SDL3 source into `src/`.
- Acceptance Criteria: Every M11 acceptance criterion in `state/milestones.md` met. Deterministic unit tests per slice (slot decode primary/secondary, I/O dispatch + mirrors, mapper readback pattern, backup-RAM switched-I/O, M1-wait timing oracle) + integration test (machine steps CPU over slot-decoded bus). Existing M0–M10 suites remain green (update the timing oracles affected by the +1 M1 wait per risk R-3, documenting the datasheet+M1-wait totals). Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`. S6: run the A/B parity harness; capture a REAL trace-diff to `docs/m11-parity-trace-diff.md`, or report BLOCKED honestly if openMSX cannot be driven (never claim parity without a genuine diff). Produce `docs/m11-implementation-report.md`.
- Dependencies: `docs/m11-planner-package.md`; DEC-0002; guardrails; S1985 fact-sheet; openMSX 21.0 (reference only); current `src/` tree.
- Requested At: 2026-07-05T18:25:00+09:00

- Request ID: REQ-M11-004
- From: MSX Master Agent (coordinator)
- To: MSX QA Agent
- Milestone ID: M11
- Type: Regression assessment + sign-off recommendation
- Scope: Independently assess M11 (S1985 chipset + full system bus) against its acceptance criteria in `state/milestones.md` and produce a sign-off recommendation (Pass / Conditional Pass / Fail) at `docs/m11-qa-signoff.md`. INDEPENDENTLY RE-EXECUTE the deterministic suite (`ctest --test-dir build -C Debug --output-on-failure`) and report the actual pass/fail count yourself — do not take the developer's number on trust. Verify the M11 unit tests are genuine (non-stub) in source: slot decode (primary #A8 / secondary #FFFF), I/O dispatch + mirrors (#9C-9F, #AC-AF), mapper readback 100xxxxx, backup-RAM switched-I/O ID 0xFE, and the M1-wait timing oracle. Confirm NO regression across the prior M0–M10 suites and specifically validate that the 8 recomputed timing oracles (R-3) reflect correct datasheet+M1-wait totals rather than weakened assertions. Adversarially validate the A/B evidence: open `docs/m11-parity-trace-diff.md` and confirm it is a genuine captured diff against a real openMSX machine instantiating an S1985 (not fabricated); an empty diff is only acceptable if the harness demonstrably drove both sides. Assess residual risk (R-1/A-2 reset-slot deviation carried to M12; A-5/R-6 sram persistence) and state whether each is resolved or accepted.
- Acceptance Criteria: Sign-off doc records the QA-executed ctest count, per-criterion verification (met/not-met with evidence), A/B evidence adversarial validation, regression verdict (no regression confirmed or specific failures), residual-risk disposition, and a clear Pass/Conditional Pass/Fail recommendation. No feature code written by QA.
- Dependencies: `docs/m11-implementation-report.md`; `docs/m11-parity-trace-diff.md`; `docs/m11-planner-package.md`; `state/milestones.md` (M11); guardrails.
- Requested At: 2026-07-05T19:05:00+09:00

- Request ID: REQ-M11-005
- From: Human (project owner, source of truth)
- To: MSX Master Agent (coordinator)
- Milestone ID: M11
- Type: Human release decision (milestone closure)
- Scope: Close M11 (S1985 "MSX-ENGINE" Chipset & Full System Bus) on the strength of QA PASS (REQ-M11-004, `docs/m11-qa-signoff.md`): QA-executed ctest 38/38, no regression, A/B parity independently reproduced by QA vs openMSX 19.1 Sony_HB-F1XV with a genuine <S1985>. Add the project to git and tag the closed-milestone snapshot as `v1.0.11` (version scheme: last two digits = the signed-off/closed milestone number).
- Acceptance Criteria: M11 marked Done in `state/milestones.md` and `state/definition-of-done.yaml` (overall_done: true); project under git with all project sources tracked; annotated tag `v1.0.11` created at the closure commit.
- Requested At: 2026-07-06T09:30:00+09:00

---

- Request ID: REQ-M12-001
- From: Human (project owner) via coordinator
- To: Protocol ledger (kickoff)
- Milestone ID: M12
- Type: Milestone kickoff (inserted; renumber)
- Scope: Per DEC-0003, RENUMBER planned memory milestone M12->M13 and VDP milestone M13->M14, and INSERT a new M12 = Z80A CPU End-to-End Parity Review & Hardening (HB-F1XV Zilog NMOS). Execution order M11(done) -> M12(CPU parity) -> M13(memory) -> M14(VDP). Standing human auto-close grant for M12 on 100% unit+system-integration pass, zero regression M1-M12, QA Pass.
- Acceptance Criteria: milestones.md + definition-of-done.yaml reflect renumber + new M12 (met); DEC-0003 recorded (met); planner-first sequencing enforced.
- Dependencies: M11 Done (v1.0.11); DEC-0003; references/fact-sheets/Zilog Z80A CPU.md; references/openmsx-21.0/src/cpu/.
- Requested At: 2026-07-06T10:00:00+09:00

- Request ID: REQ-M12-002
- From: MSX Master Agent (coordinator)
- To: MSX Planner Agent
- Milestone ID: M12
- Type: Planning package (CPU parity gap analysis + slice plan)
- Scope: Produce the M12 planning package (durable artifact `docs/m12-planner-package.md`, no production code). Perform a COMPREHENSIVE gap analysis of the current Z80A CPU implementation (`src/devices/cpu/z80a_cpu.cpp/.h`, `z80a_state.*`, `cpu_bus_client.*`, `z80a_trace.h`) against `references/fact-sheets/Zilog Z80A CPU.md` and openMSX (`references/openmsx-21.0/src/cpu/CPUCore.cc`, `CPURegs.cc`, `Z80.hh`). For EACH parity item in the fact-sheet rubric (§4 undocumented/illegal: SLL CB 30-37, IXH/IXL/IYH/IYL, DD/FD prefix chaining + NONI, ED-hole 2-NOP, WZ/MEMPTR incl. BIT n,(HL)/(IX+d), block-instruction flag quirks LDI/CPI/INI/OUTI incl. NMOS OUTI-affects-carry, 16-bit ADD/ADC/SBC flags, full-table DAA; §5 interrupts: IM0/1/2 + NMI, IM1=0x0038, DI/EI/RETI/RETN IFF, EI one-instruction delay, NMOS LD A,I/LD A,R P/V interrupt bug, interrupt-ack timing IM1 13T bare; §8 quirks: SCF/CCF Q-latch ((Q^F)|A bits 5/3), OUT (C),0 = 0 on NMOS, R-register low-7-bit increment with prefix/block rules and frozen bit7, undocumented X/Y F3/F5), classify current status as PRESENT / ABSENT / DIVERGENT with a concrete citation (our file:line vs references path). Then produce a deterministic SLICE PLAN (M12-S1..Sn) to close every gap (or, where already at parity, to PROVE it with targeted parity tests), each slice with goal, files touched, deterministic unit tests, the system-integration coverage, and evidence-gate mapping. Decide src/ placement for any new artifacts (keep CPU under `src/devices/cpu/`; you may add flag-table / undocumented-op / parity-test modules per `src/CLAUDE.md`). Note MSX M1 wait is owned at the machine level (M11) — verify the CPU exposes the correct datasheet T-states + M1-cycle count so machine-level timing stays correct; do NOT double-count the wait in the CPU. Define the full SYSTEM INTEGRATION test (CPU exercised through the M11 SystemBus, not isolated) and the openMSX A/B trace-diff acceptance test (real diff, no parity claim without a genuine capture; consider whether a ZEXDOC/ZEXALL-style harness is feasible headless). This is a parity-hardening pass, NOT a rewrite — call out and resist any gratuitous refactor.
- Acceptance Criteria: Package contains the per-item gap-analysis table (status + citations), the slice plan with per-slice unit + system-integration test obligations and evidence-gate mapping (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure`), the explicit system-integration + A/B acceptance tests, and the hard rule that closure requires 100% pass with zero regression M1-M12 (DEC-0003). Risks/assumptions each carry a verification action. Every behavior claim grounded in a concrete `references/...` path. No production code.
- Dependencies: DEC-0003; `references/fact-sheets/Zilog Z80A CPU.md`; `references/openmsx-21.0/src/cpu/`; current `src/devices/cpu/*`; `docs/m9-*` (prior CPU milestone); guardrails.
- Requested At: 2026-07-06T10:00:00+09:00

- Request ID: REQ-M12-003
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M12
- Type: Implementation (CPU parity slices S1..S6)
- Scope: Implement the M12 slice plan in `docs/m12-planner-package.md` end to end. S1: parity-proof unit tests for the 26 PRESENT items (test-only regression floor). S2: bring up a ZEXDOC/ZEXALL self-checking harness (headless); if the ZEX binary cannot be legally sourced in this environment, degrade HONESTLY to the A/B + unit nets and document it (no fabrication). S3: add WZ/MEMPTR register + all update rules and source BIT n,(HL)/(IX+d) X/Y from WZ hi byte (closes #3/#4/#35). S4: SCF/CCF genuine-Zilog Q-latch, X/Y = bits 5/3 of ((Q^F)|A) (closes #20/#21). S5: NMOS interrupt edges — make RETI copy IFF2->IFF1 like RETN, model the NMOS LD A,I / LD A,R P/V interrupt bug; treat HALT-R (#34) per the planner's decision-gated default (defer unless it can be done without disturbing a signed timing oracle) (closes #30/#31). S6: the `Machine_Hbf1xvCpuParity_Integration` system-integration test over the real M11 SystemBus + the `tools/openmsx-cpu-parity.ps1` A/B trace-diff -> `docs/m12-parity-trace-diff.md`; flip ZEXALL to required-pass if available. Keep CPU under `src/devices/cpu/`; add flag-table/undocumented-op/parity-test modules with best judgment per `src/CLAUDE.md`.
- Acceptance Criteria: Every DIVERGENT/ABSENT gap closed and covered by a deterministic unit test grounded in the fact sheet / openMSX. HARD CONSTRAINT (R-5/R-6): do NOT change any datasheet T-state or `increment_refresh_register()` call site — the 8 QA-signed M11 timing oracles must remain green unchanged; the CPU must not double-count the machine-level M1 wait. Full system-integration test passes. 100% pass of ALL unit + system-integration tests with ZERO regression across M1-M12 (DEC-0003 close condition). Refactor creep forbidden — every production edit cites a gap ID; S1/S2 are test-only. Evidence gates executed and captured: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug --output-on-failure` (report exact pass/fail). A/B: capture a REAL trace-diff or report BLOCKED honestly (respect the A-4 benign SCF/CCF divergence — gate that on ZEXALL/fact-sheet, not the trace-diff). Produce `docs/m12-implementation-report.md`.
- Dependencies: `docs/m12-planner-package.md`; DEC-0003; `references/fact-sheets/Zilog Z80A CPU.md`; `references/openmsx-21.0/src/cpu/`; `src/devices/cpu/*`; guardrails.
- Requested At: 2026-07-06T10:20:00+09:00

- Request ID: REQ-M12-004
- From: MSX Master Agent (coordinator)
- To: MSX QA Agent
- Milestone ID: M12
- Type: Regression assessment + sign-off recommendation (auto-close-gated)
- Scope: Independently assess M12 (Z80A CPU parity hardening) against its acceptance criteria in `state/milestones.md` and produce a sign-off recommendation (Pass / Conditional Pass / Fail) at `docs/m12-qa-signoff.md`. INDEPENDENTLY RE-EXECUTE `cmake --build build --config Debug` then `ctest --test-dir build -C Debug --output-on-failure` and report the ACTUAL pass/fail count yourself — do not trust the developer number. Verify the closed gaps are GENUINE (non-stub) in source and correct vs the fact sheet + openMSX: (#3/#35) WZ/MEMPTR register present and updated at the §4 rule sites; (#4/#5) BIT n,(HL)/(IX+d) X/Y sourced from WZ hi byte; (#20/#21) SCF/CCF X/Y = bits 5/3 of ((Q^F)|A) with a correctly-reset Q latch; (#30) RETI copies IFF2->IFF1; (#31) NMOS LD A,I/R P/V interrupt bug. Spot-check at least two by hand against the fact-sheet rule. Verify the S6 system-integration test genuinely exercises the CPU through the real M11 SystemBus (not isolated) and that its IM1-ack cycle oracle proves no M1-wait double-count. CRITICAL REGRESSION CHECK: confirm the 8 QA-signed M11 timing oracles are UNCHANGED and green and that NO datasheet T-state or increment_refresh_register() call site was altered. Adversarially validate the A/B evidence (`docs/m12-parity-trace-diff.md`) is a genuine captured diff against a real openMSX S1985 machine, and confirm the A-4 SCF/CCF benign-divergence handling is correct (a correct Zilog Q-latch SHOULD differ from openMSX's OR-form; that is not a defect).
- DECISION-CRITICAL: DEC-0003 authorizes auto-close of M12 ONLY on 100% pass of ALL unit + system-integration tests, ZERO regression M1-M12, AND a QA recommendation of Pass. You MUST state explicitly whether the ZEXALL/ZEXDOC honest-degradation (no legally-sourceable binary offline) and the deferred HALT-R (#34) prevent a clean PASS or are acceptable accepted-residuals under the milestone's acceptance criteria. If your verdict is anything short of a clean Pass (e.g. Conditional Pass), say so plainly — the coordinator will NOT auto-close on less than Pass and will escalate to the human.
- Acceptance Criteria: Sign-off records QA-executed ctest count, per-gap genuine/correct verification with the hand-checks, the M11-timing-oracle-unchanged regression confirmation, the A/B adversarial validation, an explicit ruling on the ZEXALL + HALT-R residuals vs the auto-close bar, and a clear Pass / Conditional Pass / Fail recommendation. No feature code by QA.
- Dependencies: `docs/m12-implementation-report.md`; `docs/m12-parity-trace-diff.md`; `docs/m12-planner-package.md`; `state/milestones.md` (M12); DEC-0003; guardrails.
- Requested At: 2026-07-06T11:00:00+09:00

- Request ID: REQ-M12-005
- From: MSX Master Agent (coordinator)
- To: Protocol ledger (closure)
- Milestone ID: M12
- Type: Coordinator auto-close under DEC-0003 standing grant
- Scope: Close M12 (Z80A CPU parity hardening) under the DEC-0003 standing auto-close authorization. Verified condition: QA-executed ctest 45/45 (100% pass, 0 failed, 0 skipped), ZERO regression M1-M12, QA recommendation PASS (REQ-M12-004, docs/m12-qa-signoff.md). Tag git snapshot v1.0.12 (scheme: last two digits = closed milestone number).
- Acceptance Criteria: M12 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.12 at closure commit; DEC-0004 records HALT-R deferral.
- Requested At: 2026-07-06T11:20:00+09:00

---

- Request ID: REQ-M13-001
- From: MSX Master Agent (coordinator)
- To: Protocol ledger (kickoff)
- Milestone ID: M13
- Type: Milestone kickoff
- Scope: Kick off M13 (RAM/ROM Memory Devices & Slot Population; renumbered from M12 by DEC-0003; VRAM deferred to M14) per the human directive "after M12 is closed, then move onto M13 (Memory related)". Populate the M11 slot map with the CPU-addressable memory: 64 KB main RAM as a memory-mapper device (slot 3-0, #FC-FF segment registers) + the HB-F1XV ROM set. Local assets verified present under bios/ (f1xvbios.rom 32K, f1xvext.rom 16K, f1xvdisk.rom 16K, f1xvkdr.rom 32K, f1xvkfn.rom 256K, f1xvmus.rom 16K, f1xvfirm.rom 1M) and roms/aleste.rom (cartridge). Authoritative slot layout: references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml. M13 inherits the M11 forward-obligation R-1/A-2 (flip reset slot default #A8=0xFF -> #A8=0 once slot-0 BIOS is populated). Normal human-release-decision gate (no auto-close grant).
- Acceptance Criteria: M13 milestone entry present (met); DEC-0003 recorded (met); planner-first sequencing enforced.
- Dependencies: M12 Done (v1.0.12); M11 bus/slot infrastructure; DEC-0002/DEC-0003.
- Requested At: 2026-07-06T11:30:00+09:00

- Request ID: REQ-M13-002
- From: MSX Master Agent (coordinator)
- To: MSX Planner Agent
- Milestone ID: M13
- Type: Planning package
- Scope: Produce the M13 planning package (durable artifact `docs/m13-planner-package.md`, no production code). Define scope boundaries, dependency sequencing, and a deterministic slice plan to populate the M11 slot map with the HB-F1XV CPU-addressable memory devices. FIRST read the authoritative layout `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` and the S1985 fact-sheet §4/§9 to determine the EXACT primary/secondary slot + page assignment for: (1) 64 KB main RAM as a memory-mapper device (openMSX slot 3-0, #FC-FF write-only segment registers, readback 100xxxxx already provided by the M11 MapperIo — reconcile how the RAM mapper device and the M11 mapper-readback interact), (2) the ROM set mapped to its real slots/pages — BIOS/BASIC (bios/f1xvbios.rom 32K), SUB/ext ROM (bios/f1xvext.rom 16K), DISK ROM (bios/f1xvdisk.rom 16K), Kanji driver/BASIC (bios/f1xvkdr.rom 32K), Kanji font (bios/f1xvkfn.rom 256K, accessed via I/O #D8-#DB not the CPU memory map — confirm and scope correctly), MSX-MUSIC/FM-PAC ROM (bios/f1xvmus.rom 16K — note FM-PAC device internals + SRAM are a SEPARATE later milestone; scope only the ROM presence/mapping here or defer with justification), and the firmware ROM (bios/f1xvfirm.rom 1M — MSX-JE/MSX-Write; determine its slot mapping from the XML or defer with justification). Map each local asset file to its slot with a PATH-VERIFIED reference (assets confirmed present; never fabricate provenance/checksums — use tools/checksum-assets.ps1). Reconcile the strict Target Machine Specification ROM breakdown (32K BIOS + 16K SUB + 16K KANJI + 16K DISK) with the richer real layout (256K kanji font, FM-MUSIC, firmware) and DOCUMENT the reconciliation (spec table is a simplification; XML + fact-sheet are authoritative for actual mapping). Decide src/ placement (propose `src/devices/memory/` for the RAM-mapper + ROM devices vs machine wiring in `src/machine/`; keep device logic out of core/ and machine wiring out of devices/ per src/CLAUDE.md). Handle the M11 R-1/A-2 obligation: define the slice that flips the reset slot default #A8=0xFF -> #A8=0 (slot-0 BIOS) once slot 0 is populated, with the boot checkpoint that proves BIOS is fetched/executed. Explicitly scope OUT: FM-PAC device internals + SRAM (later milestone), VRAM/VDP (M14), FDC internals (disk drive mechanics — only the DISK ROM presence is in M13 unless justified).
- Acceptance Criteria: Package defines the exact slot/page map (grounded in the XML + fact-sheet with concrete citations), the asset->slot mapping with verified paths, the slice plan with per-slice deterministic unit/integration tests and evidence-gate mapping, the reset-default-flip slice + boot checkpoint, the openMSX A/B acceptance test (real diff over a CPU-visible RAM/ROM/boot program; VRAM excluded — that is M14; no parity claim without a genuine diff), and the missing-asset deterministic-handling policy. Risks/assumptions each carry a verification action. No production code.
- Dependencies: `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`; `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §4/§9; M11 artifacts (`docs/m11-*`, `src/devices/chipset/*`, `src/machine/hbf1xv_machine.*`); local bios/roms assets; guardrails; DEC-0002/DEC-0003.
- Requested At: 2026-07-06T11:30:00+09:00

- Request ID: REQ-M13-003
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M13
- Type: Implementation (memory devices + slot population, slices S1..S5)
- Scope: Implement the M13 slice plan in `docs/m13-planner-package.md` end to end. S1: `src/devices/memory/rom_device.{h,cpp}` (read-only window) + `memory_mapper_ram.{h,cpp}` (64K, consumes segments from M11 MapperIo::segment(page); DO NOT re-own #FC-FF or the 100xxxxx readback — MapperIo stays the owner); read `references/openmsx-21.0/src/memory/MSXMemoryMapper.cc` for exact cold-boot segment defaults + unpopulated-segment wrap; isolated unit tests. S2: `src/machine/rom_asset_loader.{h,cpp}` loading the verified bios/ assets with the deterministic missing-asset policy (0xFF-fill + logged diagnostic; NO fabricated SHA/provenance; real SHAs via tools/checksum-assets.ps1). S3: slot population wiring per the XML map — expand slots 0 AND 3 (set_expanded(0,true) is the M11 correction), place BIOS 0-0, SUB 3-1 p0, Kanji driver 3-1 p1-2, DISK 3-2 p1, FM-MUSIC 3-3 p1, RAM mapper 3-0; keep #A8=0xFF in S3 to stay green; retire ram_slot_backing.*. S4: flip cold_boot reset default to authentic #A8=0 (slot-0 BIOS) + boot-checkpoint test (bus[0]==BIOS[0], single-step bounded K instrs asserting each fetched opcode==slot-resolved ROM byte, reach defined PC); reconcile M11/M12 suites — update the M11 test that pinned #A8=0xFF to #A8=0 WITH justification, add a documented map_flat_ram() helper so existing CPU-over-RAM program tests page RAM explicitly (no silent weakening; mirror the M11 R-3 discipline). S5: extend tools/openmsx-trace-parity.ps1 vs openMSX Sony_HB-F1XV for the two A/B subjects (BIOS-boot checkpoint bounded before first VDP #98/#99 access; BIOS-independent RAM/ROM probe) -> capture a REAL trace-diff to `docs/m13-parity-trace-diff.md` or report BLOCKED honestly. Keep device logic in src/devices/memory/, machine wiring in src/machine/ (src/CLAUDE.md). OUT of scope: FM-PAC device internals + MSX-JE SRAM, kanji font I/O #D8-DB, Halnote firmware, cartridge loading, VRAM/VDP, FDC drive mechanics (DISK ROM presence only).
- Acceptance Criteria: All M13 acceptance criteria in `state/milestones.md` met. Deterministic unit tests (ROM read-only + mapper segment consume/wrap, asset loader + missing-asset policy, slot-page resolution) + integration tests (boot checkpoint through slot-decoded bus; segment-switch RAM). Existing M0-M12 suites remain green — any test touched by the reset-flip/segment-folding is UPDATED to the authentic value with documented justification, never weakened. If the R-3/A-5 RAM initialContent change affects the M10 dump golden, update it with justification. Evidence gates executed and captured: tools/validate-assets.ps1, tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt, cmake --build build --config Debug, ctest --test-dir build -C Debug --output-on-failure (report EXACT pass/fail). A/B: real diff to docs/m13-parity-trace-diff.md or honest BLOCKED. Produce `docs/m13-implementation-report.md`.
- Dependencies: `docs/m13-planner-package.md`; Sony_HB-F1XV.xml; S1985 fact-sheet §4/§9; references/openmsx-21.0/src/memory/MSXMemoryMapper.cc (reference only); M11/M12 src + tests; local bios/ assets; guardrails.
- Requested At: 2026-07-06T11:45:00+09:00

- Request ID: REQ-M13-004
- From: MSX Master Agent (coordinator)
- To: MSX QA Agent
- Milestone ID: M13
- Type: Regression assessment + sign-off recommendation
- Scope: Independently assess M13 (RAM/ROM memory devices + slot population) against its acceptance criteria in `state/milestones.md` and produce a sign-off recommendation (Pass / Conditional Pass / Fail) at `docs/m13-qa-signoff.md`. INDEPENDENTLY RE-EXECUTE `cmake --build build --config Debug` then `ctest --test-dir build -C Debug --output-on-failure` and report the ACTUAL pass/fail count yourself. Verify the M13 devices are GENUINE (non-stub): RomDevice read-only enforcement; MemoryMapperRam consumes segments from M11 MapperIo (does NOT re-own #FC-FF or the 100xxxxx readback — confirm no double-ownership); slot population matches the Sony_HB-F1XV.xml map (slot 0-0 BIOS, 3-0 RAM mapper, 3-1 SUB+Kanji, 3-2 DISK, 3-3 FM-MUSIC; slots 0 AND 3 expanded). Validate the reset-flip: cold_boot #A8=0 boots slot-0 BIOS and the boot-checkpoint test genuinely fetches real BIOS opcodes from slot 0 (bus[0]==BIOS[0], opcode-by-opcode match) — not a stub/hardcoded pass. CRITICAL REGRESSION CHECK: confirm every prior M0-M12 test the developer UPDATED (reset-flip #A8, segment-folding, A-5 RAM initialContent pattern, cpu_parity OUTI #FC->#FD retarget, debug_dump DRAM golden) was changed to the AUTHENTIC value with sound justification and NOT weakened to paper over a regression — spot-check at least two of these updated tests against their rationale. Adversarially validate the A/B evidence (`docs/m13-parity-trace-diff.md`): confirm both subjects are genuine captured diffs against a real openMSX Sony_HB-F1XV, that the local BIOS SHA1s genuinely match the openMSX-referenced revisions (so both run identical images), and that the empty diff is a true match (consider the comparator's failure mode). Confirm the missing-asset policy is deterministic (0xFF-fill + logged note, no fabricated SHA). Assess residual risk and disposition each. NOTE: M13 retains the NORMAL human-release-decision gate (no auto-close grant) — your Pass recommendation enables the coordinator to present M13 for the human's release decision, not to auto-close.
- Acceptance Criteria: Sign-off records QA-executed ctest count, per-criterion verification (met/not-met with evidence), the double-ownership check, the boot-checkpoint genuineness verification, the updated-test regression audit with spot-checks, the A/B adversarial validation incl. the SHA1-match confirmation, residual-risk disposition, and a clear Pass/Conditional Pass/Fail recommendation. No feature code by QA.
- Dependencies: `docs/m13-implementation-report.md`; `docs/m13-parity-trace-diff.md`; `docs/m13-planner-package.md`; Sony_HB-F1XV.xml; `state/milestones.md` (M13); guardrails.
- Requested At: 2026-07-06T12:30:00+09:00

- Request ID: REQ-M13-005
- From: Human (project owner, source of truth)
- To: MSX Master Agent (coordinator)
- Milestone ID: M13
- Type: Human release decision (milestone closure)
- Scope: Close M13 on QA PASS (REQ-M13-004). Tag v1.0.13. Keep the deferred items recorded for planners (-> DEC-0005, agent-protocol/state/deferred-backlog.md). Proceed to M14 (VDP).
- Acceptance Criteria: M13 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.13; deferred-backlog.md established.
- Requested At: 2026-07-06T13:00:00+09:00

---

- Request ID: REQ-M14-001
- From: MSX Master Agent (coordinator)
- To: Protocol ledger (kickoff)
- Milestone ID: M14
- Type: Milestone kickoff
- Scope: Kick off M14 (Yamaha V9958 VDP incl. 128 KB VRAM ownership; renumbered from M13 by DEC-0003) per DEC-0006. Closes deferred-backlog item B9. The VDP owns the 128 KB VRAM (DEC-0002); the inert vram_ placeholder migrates out of the machine into the VDP. Normal human-release-decision gate (no auto-close grant). Grounding: references/fact-sheets/Yamaha V9958 VDP.md, references/openmsx-21.0/src/video/.
- Acceptance Criteria: M14 milestone entry present (met); DEC-0006 recorded (met); planner-first sequencing enforced.
- Dependencies: M13 Done (v1.0.13); M11 I/O bus; DEC-0002/DEC-0006; deferred-backlog.md (B9).
- Requested At: 2026-07-06T13:00:00+09:00

- Request ID: REQ-M14-002
- From: MSX Master Agent (coordinator)
- To: MSX Planner Agent
- Milestone ID: M14
- Type: Planning package
- Scope: Produce the M14 planning package (durable artifact `docs/m14-planner-package.md`, no production code) for the Yamaha V9958 VDP. This is the MOST COMPLEX chip in the system — decompose it into deterministic, independently-verifiable slices; DO NOT attempt it as one slice. FIRST read `references/fact-sheets/Yamaha V9958 VDP.md` in full and survey openMSX `references/openmsx-21.0/src/video/` (VDP.cc/.hh, VDPVRAM, VDPCmdEngine, SpriteChecker, VDPAccessSlots, V9958-specific paths — read for understanding only; GPL, never copy into src/). Define: (a) src/ placement — propose `src/devices/video/` for the VDP device + VRAM (device logic), machine wiring in src/machine/; concrete file names; the vram_ placeholder migrates from src/machine/hbf1xv_machine.* into the VDP. (b) The register/port model: ports #98 (VRAM data), #99 (status/register incl. the register-write auto-increment and the R#17 indirect path), #9A (palette), #9B (indirect register), with the M11 S1985 mirror #9C-9F already aliasing — reconcile the VDP wiring onto the M11 io_bus. (c) VRAM addressing: 128 KB, the #98 auto-increment read/write, address setup via #99. (d) Status registers S#0..S#9 (V9958), the VBlank interrupt (level-held /INT cleared on S#0 read — coordinate the seam with the CPU IM1 path already in M12), and the HBlank/line interrupt (R#19). (e) Screen/graphic modes per the Target Machine Specification (text 40x24/32x24; graphic 256x192/212, 512x212, 256x424; YJK/YAE; the palette = 512 colors 9-bit, 16 concurrent in palette modes; 19,268 is the YJK on-screen max NOT a palette dimension). (f) Slice out rendering/timing DEPTH explicitly — decide what is in M14 (at minimum: register/status/port/VRAM/palette/interrupt behavior verifiable by unit + A/B trace-diff) vs deferred to a later rendering/timing milestone (pixel-accurate raster output, sprite rendering, exact VDP-access slot timing, V9958 horizontal scroll/YJK rendering) — and RECORD any such deferral into `agent-protocol/state/deferred-backlog.md` per DEC-0005. (g) The openMSX A/B acceptance test: real trace-diff over a CPU->VDP port/VRAM/register/palette sequence vs openMSX Sony_HB-F1XV; no parity claim without a genuine capture; note VRAM state can now be compared (it was excluded in M13). Per-slice deterministic unit + integration test obligations and evidence-gate mapping. Risks/assumptions each with a verification action; ground every claim in a concrete references path.
- Acceptance Criteria: Package defines the slice plan (M14-S1..Sn) with per-slice tests + gates, the src/ placement + vram_ migration, the register/port/VRAM/palette/status/interrupt model grounded in the fact-sheet + openMSX with concrete citations, an explicit in-scope-vs-deferred rendering/timing boundary WITH backlog updates, the CPU IM1 interrupt seam coordination, and the A/B acceptance test (real diff, VRAM now comparable). Consult deferred-backlog.md (B9 active) and state what M14 closes. No production code.
- Dependencies: `references/fact-sheets/Yamaha V9958 VDP.md`; `references/openmsx-21.0/src/video/`; M11 io_bus + S1985 mirror; M12 CPU IM1 path; `src/machine/hbf1xv_machine.*` (vram_ placeholder); deferred-backlog.md; guardrails; DEC-0002/DEC-0006.
- Requested At: 2026-07-06T13:00:00+09:00
