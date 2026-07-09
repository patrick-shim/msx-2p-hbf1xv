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

- Request ID: REQ-M14-003
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M14
- Type: Implementation (V9958 VDP slices S1..S6)
- Scope: Implement the M14 slice plan in `docs/m14-planner-package.md` end to end. S1: new src/devices/video/{vdp_vram,v9958_vdp}.* — VDP as a core::IoDevice keyed on port&0x03; MIGRATE the 128KB vram_ out of src/machine/hbf1xv_machine.* into the VDP (VDP owns it; remove the machine placeholder; repoint the debug dump to vdp_.vram()); #98 VRAM data path with 17-bit (R#14<<14)|ptr addressing, auto-increment, read-ahead buffer + shared read/write latch. S2: #99 two-write register-set/address-setup protocol + R#0-R#27 register file + #9B R#17 indirect-register path with auto-increment (AII). S3: #9A 9-bit GRB palette (16 entries) + mode-selection decode (R#0 M3-M5, R#1 M1-M2) + R#25/26/27 V9958 feature-bit storage. S4: status registers S#0..S#9 (V9958; S#1 reset 0x04) + VBlank interrupt (S#0 F / R#1 IE0) + line interrupt (S#1 FH / R#0 IE1 / R#19); level-held /INT = vertical OR horizontal cleared on status read; drive the M12 CPU IM1 path via a thin IrqLine adapter (REUSE the existing CPU interrupt seam, do NOT rebuild it). S5: system integration — wire the VDP onto the M11 io_bus, confirm the S1985 #9C-9F mirror routes to the same VDP, per-frame VSYNC raises VBlank; mirror-port equivalence test. S6: extend the parity harness for a real openMSX A/B trace-diff over a CPU->VDP sequence (register/VRAM-fill/palette/indirect/status/VBlank-accept) incl. VRAM read-back -> capture a REAL diff to `docs/m14-parity-trace-diff.md` or report BLOCKED honestly. Keep device logic in src/devices/video/, machine wiring in src/machine/ (src/CLAUDE.md).
- Acceptance Criteria: All M14 acceptance criteria in state/milestones.md met. VDP owns 128KB VRAM; NO CPU-addressable VRAM region remains in the machine. Deterministic unit tests (port decode, #98 auto-increment read/write + read-ahead, #99 two-write protocol + address setup, #9B indirect AII, palette, mode decode, S#0..S#9, VBlank/line interrupt raise+clear-on-status-read) + integration tests (CPU->VDP over the M11 io_bus; #9C-9F mirror equivalence; VBlank interrupt accepted by the CPU IM1 path and cleared on S#0 read). Existing M0-M13 suites remain green — the debug-dump VRAM golden WILL change from repointing VRAM to the VDP; update it with documented justification (mirror the prior R-3/A-5 discipline), never weakened. STAY WITHIN the in-scope boundary: do NOT implement D1-D7 (pixel rendering, sprites, command engine, cycle-accurate slot timing, YJK decode, visual effects, planar interleave) — those are deferred (deferred-backlog.md). Evidence gates executed and captured: tools/validate-assets.ps1, tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt, cmake --build build --config Debug, ctest --test-dir build -C Debug --output-on-failure (report EXACT pass/fail). A/B: real diff to docs/m14-parity-trace-diff.md or honest BLOCKED. Produce `docs/m14-implementation-report.md`.
- Dependencies: `docs/m14-planner-package.md`; references/fact-sheets/Yamaha V9958 VDP.md; references/openmsx-21.0/src/video/ (reference only, GPL); M11 io_bus + S1985 mirror; M12 CPU IM1 seam; src/machine/hbf1xv_machine.* (vram_ migration); deferred-backlog.md; guardrails.
- Requested At: 2026-07-06T13:20:00+09:00

- Request ID: REQ-M14-004
- From: MSX Master Agent (coordinator)
- To: MSX QA Agent
- Milestone ID: M14
- Type: Regression assessment + sign-off recommendation
- Scope: Independently assess M14 (V9958 VDP register/VRAM/status/interrupt contract) against its acceptance criteria in `state/milestones.md` and produce a sign-off recommendation (Pass / Conditional Pass / Fail) at `docs/m14-qa-signoff.md`. INDEPENDENTLY RE-EXECUTE `cmake --build build --config Debug` then `ctest --test-dir build -C Debug --output-on-failure` and report the ACTUAL pass/fail count. Verify the devices are GENUINE (non-stub): (1) the vram_ MIGRATION — confirm the 128KB VRAM MemoryRegion + kVramBytes + vram()/vram_size() are REMOVED from Hbf1xvMachine and that NO CPU-addressable VRAM region remains (VRAM reachable only via #98/#99 + #9C/#9D mirror or machine.vdp().vram()); confirm the debug-dump repoint and that its golden is justified. (2) #98 VRAM data path: 17-bit (R#14<<14)|ptr addressing, auto-increment (+R#14 carry, legacy wrap), read-ahead buffer, shared read/write latch. (3) #99 two-write register/address protocol + direction bit + R#0-27 file; #9B indirect + AII. (4) #9A palette 9-bit GRB two-write + mode decode. (5) S#0..S#9 (S#1=0x04) + VBlank/line interrupts. Spot-check at least two behaviors by hand vs the fact-sheet. (6) CRITICAL — the interrupt seam: confirm the VDP /INT is LEVEL-held and cleared on S#0 read, wired to the M12 CPU IM1 accept path via the IrqLine adapter WITHOUT modifying the M12 accept logic; verify the integration test shows exactly ONE accept and /INT released after S#0 read. CRITICAL REGRESSION CHECK: confirm all prior M0-M13 suites remain green and that the tests updated for the VRAM migration (memory_regions unit+integration repointed machine.vram()->machine.vdp().vram()) were changed authentically, not weakened; confirm the debug_dump golden is correct. Adversarially validate the A/B evidence (`docs/m14-parity-trace-diff.md`) is a genuine captured diff vs a real openMSX Sony_HB-F1XV V9958, that VRAM read-back is now actually compared (it was excluded in M13), and that the comparator can report divergence. BOUNDARY CHECK: confirm the implementation stayed within the contract and did NOT implement D1-D7 (rendering/sprites/command-engine/cycle-accurate-timing/YJK/effects/planar) — and that everything it deferred is recorded in `agent-protocol/state/deferred-backlog.md` (rows D1-D7 present). PROCESS-INTEGRITY CHECK (per human directive): verify the milestone ledger, definition-of-done, decisions (DEC-0002/0003/0005/0006), and deferred-backlog are mutually consistent for M14 and that no follow-up/deferred item was dropped; note any drift. NOTE: M14 retains the NORMAL human-release-decision gate — a Pass lets the coordinator present M14 to the human, not auto-close.
- Acceptance Criteria: Sign-off records QA-executed ctest count, per-criterion verification, the vram-migration + no-CPU-addressable-VRAM confirmation, the interrupt-seam single-accept/level-clear verification, the two hand-checks, the updated-test regression audit, the A/B adversarial validation incl. VRAM-read-back comparison, the D1-D7 boundary + backlog-presence check, the process-integrity/ledger-consistency check, residual-risk disposition, and a clear Pass/Conditional Pass/Fail recommendation. No feature code by QA.
- Dependencies: `docs/m14-implementation-report.md` (delivered in RESP-M14-003 channel narrative — the developer noted it did not write a separate report file; use the RESP-M14-003 entry + source as the evidence base), `docs/m14-parity-trace-diff.md`, `docs/m14-planner-package.md`, `agent-protocol/state/deferred-backlog.md`, `state/milestones.md` (M14), guardrails.
- Requested At: 2026-07-06T14:00:00+09:00

- Request ID: REQ-M14-005
- From: Human (project owner, source of truth)
- To: MSX Master Agent (coordinator)
- Milestone ID: M14
- Type: Human release decision (milestone closure)
- Scope: Close M14 on QA PASS (REQ-M14-004). Tag v1.0.14.
- Acceptance Criteria: M14 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.14; backlog B9 -> DONE (M14).
- Requested At: 2026-07-06T14:40:00+09:00

---

- Request ID: REQ-M15-001
- From: MSX Master Agent (coordinator)
- To: Protocol ledger (kickoff)
- Milestone ID: M15
- Type: Milestone kickoff (planning-only this cycle)
- Scope: Open M15 per DEC-0008. North-star goal: complete device integrations with the S1985 chipset fully wired (real PSG/RTC devices replacing the M11 seams + remaining device integrations from the backlog). PLANNING ONLY this cycle — coordinator STOPS for human review before implementation.
- Acceptance Criteria: M15 milestone entry present (met); DEC-0008 recorded (met); planner-first; implementation gated on human review.
- Dependencies: M14 Done (v1.0.14); deferred-backlog.md; DEC-0008; references/fact-sheets/*; references/openmsx-21.0/src/{sound,fdc}/, MSXRTC/RP5C01/I8255/MSXPPI.
- Requested At: 2026-07-06T14:45:00+09:00

- Request ID: REQ-M15-002
- From: MSX Master Agent (coordinator)
- To: MSX Planner Agent
- Milestone ID: M15
- Type: Planning package (device-integration decomposition; NO implementation)
- Scope: Produce the M15 planning package (durable artifact `docs/m15-planner-package.md`, no production code). North-star GOAL: complete device integrations with the S1985 chipset FULLY WIRED — replace the M11 chipset seams (PSG #A0-A2, RTC #B4/B5) with real device implementations and complete the remaining device integrations. MANDATORY: consult the ENTIRE `agent-protocol/state/deferred-backlog.md` and, for EVERY row (B1-B8, C1-C9, D1-D7), recommend IN-M15 vs DEFER-to-named-follow-on with a one-line justification — nothing silently dropped (DEC-0005). Because "all pending items" is too large for one deterministic milestone, PROPOSE a decomposition: define M15 as the device-integration/chipset-completion milestone with a concrete, ordered slice plan, AND propose a recommended sequence of follow-on milestones (M16, M17, ...) for the deferred remainder (e.g. VDP rendering D1-D7, exact cycle timing C1-C4, input/peripherals if not in M15, SDL3 frontend C9). For each IN device: ground behavior in its concrete fact-sheet + openMSX reference (PSG -> S1985 fact-sheet §2 + references/openmsx-21.0/src/sound/AY8910.*/MSXPSG.*; RTC -> S1985 §5 + RP5C01.*/MSXRTC.*; FM-PAC -> references/fact-sheets/Yamaha YM2413 FM Chip.md + sound/YM2413*.*; keyboard/joystick -> S1985 §2/§3 + I8255.*/MSXPPI.*; backup-RAM persistence -> S1985 §6; FDC -> references/fact-sheets/FDC for Sony HB-F1XV.md + src/fdc/; Kanji font -> Sony_HB-F1XV.xml #D8-DB; Halnote/cartridge -> Sony_HB-F1XV.xml), define src/ placement (propose src/devices/audio/ for PSG+OPLL, src/devices/rtc/, src/peripherals/ for keyboard/joystick, src/devices/fdc/, etc. per src/CLAUDE.md), specify how each wires into the M11 io_bus / S1985 engine and reconciles with existing seams, and give per-device deterministic unit + system-integration test obligations + the openMSX A/B acceptance approach. Identify any REQUIRED add/modify/re-arrange of M1-M14 implementation (e.g. the S1985 engine must now host real PSG/RTC instead of seams; the PPI keyboard rows; the joystick path shared between PSG port A and the S1985) and record it as a planned change (DEC-0008 authorizes; ledger must capture). Call out an explicit BOOT consequence: real PSG/RTC/keyboard may let BIOS boot PAST the current first-device-read boundary (backlog C5) — assess how far and make it a checkpoint. Evidence-gate mapping per slice. Risks/assumptions each with a verification action. Ground every claim in a concrete references path. NO PRODUCTION CODE — this is a plan for human review.
- Acceptance Criteria: Package contains: the full-backlog IN/DEFER disposition table (every row), the proposed M15 slice plan + the recommended follow-on milestone sequence, per-IN-device spec+placement+wiring+tests grounded in concrete references, the list of required M1-M14 changes, the boot-boundary (C5) assessment, evidence-gate mapping, and risks with verification actions. No production code. Explicitly note this is presented for human review before implementation (DEC-0008).
- Dependencies: `agent-protocol/state/deferred-backlog.md`; DEC-0002/0005/0008; references/fact-sheets/{Yamaha S1985 MSX-ENGINE Chipset,Yamaha YM2413 FM Chip,FDC for Sony HB-F1XV}.md; references/openmsx-21.0/src/{sound,fdc}/ + RP5C01/MSXRTC/I8255/MSXPPI; M11 chipset src (src/devices/chipset/*); M13 memory/slot src; src/CLAUDE.md; guardrails.
- Requested At: 2026-07-06T14:45:00+09:00

- Request ID: REQ-M15-003
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M15
- Type: Implementation (device integration slices S1..S6)
- Scope: Implement the M15 slice plan in `docs/m15-planner-package.md` end to end, within the DEC-0009-locked scope. S1 PSG/YM2149 device on #A0-A2 (16 registers, 5-bit/32-step envelope, mixer R7 with MSX bit6=0/bit7=1 masking, stereo B=L/C=R/A=both) — NUMERIC/register-accurate sample model only, NO audio output/presentation (deferred SDL3/M26). S2 joystick peripheral + PSG port A (R14 read) / port B (R15 write) per S1985 fact-sheet §2; joystick hangs off the PSG (X5), not the S1985 engine. S3 RTC/RP5C01 on #B4/B5 gated by #F5 bit7 (ground the polarity + Block-2 boot-config in S1985 fact-sheet §5 + openMSX RP5C01/MSXRTC) — IN-MEMORY DETERMINISTIC EPOCH (fixed start each run), NO file persistence. S4 expand ppi_slot_select into a full i8255 PPI (#A8 port A slot-select PRESERVED byte-for-byte; #A9 port B keyboard-row read inverted; #AA port C keyboard/LED/cassette; #AB control bit-set/reset) + keyboard matrix (11x8, inverted, 0=pressed). S5 S1985 16-byte backup-RAM .sram persistence (switched-I/O ID 0xFE; C4). S6 system integration (wire_bus X2 attaches PSG/RTC/#F5/PPI-B-C-control), boot-checkpoint ADVANCE past the first PSG/RTC/keyboard reads to a deterministic PC/instruction checkpoint (C5), + openMSX A/B trace-diff -> docs/m15-parity-trace-diff.md. src/ placement per src/CLAUDE.md (propose src/devices/audio/ PSG, src/devices/rtc/, src/peripherals/ keyboard+joystick+PPI). CRITICAL (X4): advance PSG envelope + RTC time READ-ONLY off elapsed_cycles() — do NOT modify step_cpu_instruction T-state math or any datasheet T-state / increment_refresh_register() site (protect the M9/M12 timing oracles).
- Acceptance Criteria: All M15 IN-set devices implemented to spec grounded in fact-sheet + openMSX (concrete citations), wired into the M11 io_bus / S1985 chipset replacing the open-bus seams. Deterministic unit tests per device (PSG registers/envelope/mixer/stereo, joystick via PSG ports, RTC blocks/BCD/gate/deterministic epoch, i8255 PPI ports + keyboard matrix inversion, backup-RAM switched-I/O) + system-integration tests (CPU->device over the M11 bus; boot-checkpoint advance). X1 PPI expansion PRESERVES #A8 byte-for-byte — reuse the M11/M13 slot tests as a locked guard. ALL prior M0-M14 suites remain green — any test touched updated to the authentic value with documented justification, never weakened. Evidence gates captured: tools/validate-assets.ps1, tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt, cmake --build build --config Debug, ctest --test-dir build -C Debug --output-on-failure (report EXACT pass/fail). A/B: real diff to docs/m15-parity-trace-diff.md or honest BLOCKED. Update deferred-backlog.md rows B1/B2/C4/C6 (and C5 partial) to DONE (M15) in the same cycle. Produce docs/m15-implementation-report.md. Do NOT implement any DEFERRED backlog item (FDC, FM-PAC, Kanji-font, etc.).
- Dependencies: docs/m15-planner-package.md; DEC-0008/0009; references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md §2/§3/§5/§6; references/openmsx-21.0/src/sound/{AY8910,MSXPSG}.*, RP5C01.*/MSXRTC.*, I8255.*/MSXPPI.*; src/devices/chipset/*; src/machine/hbf1xv_machine.*; guardrails.
- Requested At: 2026-07-06T16:30:00+09:00

- Request ID: REQ-M15-004
- From: MSX Master Agent (coordinator)
- To: MSX QA Agent
- Milestone ID: M15
- Type: Regression assessment + sign-off recommendation
- Scope: Independently assess M15 (device integration + S1985 chipset full wiring) against its acceptance criteria in `state/milestones.md` (DEC-0009-locked scope) and produce a sign-off at `docs/m15-qa-signoff.md`. INDEPENDENTLY RE-EXECUTE `cmake --build build --config Debug` then `ctest --test-dir build -C Debug --output-on-failure` and report the ACTUAL pass/fail count. Verify GENUINE (non-stub) in source: PSG YM2149 (16 regs, 5-bit/32-step envelope, R7 mixer MSX masking bit6=0/bit7=1, YM2149 read-as-written, stereo A=both/B=L/C=R, NUMERIC model only — confirm NO audio-output/presentation was pulled in); joystick via PSG R14/R15 (hangs off PSG not S1985 engine, X5); RTC RP5C01 (#B4/B5 gated by #F5 bit7, 4 blocks/BCD/Block-2 reg0=0x0A, regs 14-15 unreadable, IN-MEMORY DETERMINISTIC epoch — confirm NO host clock / NO file dependency for time); full i8255 PPI (#A8 port A slot-select PRESERVED byte-for-byte, #A9 inverted keyboard row, #AA port C, #AB control) + 11x8 inverted keyboard matrix; S1985 backup-RAM .sram persistence (deterministic, absent-file -> deterministic default). HAND-CHECK at least TWO behaviors vs the S1985 fact-sheet (e.g. PSG R7 mask result; RTC Block-2 reg0 CMOS-valid value; keyboard-matrix inversion). CRITICAL X4 REGRESSION: confirm PSG envelope + RTC time advance READ-ONLY off elapsed_cycles()/scheduler and that step_cpu_instruction T-state math + every datasheet T-state / increment_refresh_register() site are UNCHANGED — spot-verify the M9/M12 timing oracles (cpu_step 22, ldir 102, indexed 105, IM2 49, IM0 38) are green with unchanged expected values. CRITICAL X1: confirm the PPI expansion PRESERVED #A8 slot-select byte-for-byte (the M11/M13 slot tests reused verbatim + green; Ppi8255 port A == PpiSlotSelect). Adversarially validate the A/B (docs/m15-parity-trace-diff.md) is a genuine captured diff vs real openMSX Sony_HB-F1XV with substantive device values (PSG R0=0x34, R7=0x80 masked) — reproduce if you can drive openMSX; confirm the comparator can report divergence. Validate the boot-checkpoint advance (C5) is genuine (boot really reaches ~PC 0x454/0x488 fetching real BIOS opcodes past the first device reads, deterministic, self-derived golden — not hardcoded). BOUNDARY: confirm NO deferred item (FDC/FM-PAC/Kanji-font/cartridge/Halnote/VDP-rendering) was implemented; confirm backlog rows B1/B2/C4/C6 -> DONE (M15), C5 -> IN-PROGRESS partial. PROCESS-INTEGRITY: verify milestones.md/definition-of-done.yaml/decisions(DEC-0008/0009)/requests/responses/deferred-backlog are mutually consistent for M15, no dropped item, correct numbering. Disposition the two documented assumptions (#F5 reset-enabled default; fixed 1988-01-01 epoch). NOTE: M15 retains the NORMAL human-release-decision gate — a Pass lets the coordinator present M15 to the human, not auto-close.
- Acceptance Criteria: Sign-off records QA-executed ctest count, per-device genuine/correct verification + the two hand-checks, the X4 timing-oracle + X1 #A8-preservation regression confirmation, the A/B adversarial validation, the boot-checkpoint genuineness check, the boundary + backlog check, the process-integrity check, assumption disposition, and a clear Pass/Conditional Pass/Fail recommendation. No feature code by QA.
- Dependencies: docs/m15-implementation-report.md; docs/m15-parity-trace-diff.md; docs/m15-planner-package.md; agent-protocol/state/deferred-backlog.md; DEC-0008/0009; state/milestones.md (M15); guardrails.
- Requested At: 2026-07-06T17:15:00+09:00

- Request ID: REQ-M15-005
- From: Human (project owner, source of truth) via /msx-master directive "start orchestration until M16"
- To: MSX Master Agent (coordinator)
- Milestone ID: M15
- Type: Human release decision (milestone closure)
- Scope: Close M15 (device integration + S1985 chipset fully wired) on QA PASS (REQ-M15-004). Tag v1.0.15. Proceed to M16 (FDC).
- Acceptance Criteria: M15 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.15; backlog B1/B2/C4/C6 DONE (M15).
- Requested At: 2026-07-06T18:00:00+09:00

---

- Request ID: REQ-M16-001
- From: MSX Master Agent (coordinator)
- To: Protocol ledger (kickoff)
- Milestone ID: M16
- Type: Milestone kickoff
- Scope: Open M16 = FDC drive mechanics (Fujitsu MB89311 + 720 KB 3.5" floppy drive), per DEC-0009 (FDC moved up to next-after-M15) and DEC-0010. Closes deferred-backlog B8; advances C5 toward full boot. Wire the FDC onto the DISK ROM already mapped at slot 3-2 (M13). Normal human-release-decision gate (no auto-close). Grounding: references/fact-sheets/FDC for Sony HB-F1XV.md, references/openmsx-21.0/src/fdc/.
- Acceptance Criteria: M16 milestone entry present (met); DEC-0009/0010 recorded (met); planner-first; backlog B8 -> IN-PROGRESS (M16).
- Dependencies: M15 Done (v1.0.15); M13 DISK ROM (slot 3-2); M11 io_bus; deferred-backlog B8.
- Requested At: 2026-07-06T18:00:00+09:00

- Request ID: REQ-M16-002
- From: MSX Master Agent (coordinator)
- To: MSX Planner Agent
- Milestone ID: M16
- Type: Planning package
- Scope: Produce the M16 planning package (durable artifact `docs/m16-planner-package.md`, no production code) for the HB-F1XV FDC. FIRST read `references/fact-sheets/FDC for Sony HB-F1XV.md` IN FULL and survey openMSX `references/openmsx-21.0/src/fdc/` (WD2793.*, WD2793BasedFDC.*, PhilipsFDC.* [Sony connection style maps here per DeviceFactory], DiskDrive.*, RealDrive.*, DriveMultiplexer.*, DSKDiskImage.* — read for understanding only; GPL, never copy into src/). Ground everything in concrete paths. The controller is a Fujitsu MB89311 == WD2793 (model as WD2793, NOT 8272/TC8566AF), connection style "Sony", memory-mapped in slot 3-2 page 1 (0x4000-0x7FFF), with the 4 core WD2793 registers at 7FF8-7FFB (status/command, track, sector, data) + Sony glue (side/motor/drive-select + INTRQ/DRQ status) in the 7FFC-7FFF window; ONE built-in 3.5" 720KB (2DD) drive. Define: (a) src/ placement — propose src/devices/fdc/ for the WD2793 core + Sony FDC decode + drive/disk-image (device logic), machine wiring in src/machine/; concrete file names; how it attaches to the slot-3-2 DISK ROM already mapped in M13 (reconcile with the ROM device — the FDC ROM+registers share the page-1 window). (b) The WD2793 model: Type I (restore/seek/step) / II (read/write sector) / III (read/write track, read address) / IV (force interrupt) commands, per-command status-bit semantics, the INTRQ/DRQ polled service loop the disk BIOS uses, the ~4s delayed motor-off, and the disk-changed / not-ready quirks DSKCHG depends on. (c) DETERMINISM: disk-image handling must be fully deterministic — a fixed in-memory / repository test disk image (NO host-disk/filesystem nondeterminism, no wall-clock); model MFM byte timing off the deterministic scheduler/elapsed_cycles like the M15 X4 pattern, never wall-clock. (d) BOOT ADVANCE (C5): with the FDC present, the Disk-ROM boot handshake should proceed past the M15 checkpoint (PC ~0x454) — assess how far and define a boot checkpoint acceptance signal. (e) A/B acceptance: real openMSX A/B trace-diff vs Sony_HB-F1XV over a CPU->FDC register/command sequence (+ a disk read) — no parity claim without a genuine capture. (f) Slice plan (M16-S1..Sn) each with goal, files, deterministic unit + system-integration tests, evidence-gate mapping. Consult `agent-protocol/state/deferred-backlog.md` — M16 closes B8 and advances C5; re-affirm the rest OPEN. Risks/assumptions each with a verification action. NO production code.
- Acceptance Criteria: Package defines the WD2793/Sony-FDC register+command model grounded in fact-sheet + openMSX (concrete citations), the src/ placement + slot-3-2 DISK-ROM reconciliation, the deterministic disk-image approach, the boot-advance (C5) assessment + checkpoint, the openMSX A/B acceptance test, the slice plan with per-slice tests + gates, and risks with verification actions. States M16 closes B8. No production code.
- Dependencies: references/fact-sheets/FDC for Sony HB-F1XV.md; references/openmsx-21.0/src/fdc/; references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml (WD2793 connectionstyle=Sony, motor_off 4000ms, drives 1, DISK ROM sha1 5a4e7dbb...); M13 DISK ROM (slot 3-2, docs/m13-*); M11 io_bus; M15 device patterns (docs/m15-*); src/CLAUDE.md; guardrails; deferred-backlog (B8).
- Requested At: 2026-07-06T18:05:00+09:00

- Request ID: REQ-M16-003
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M16
- Type: Implementation (FDC slices S1..S6)
- Scope: Implement the M16 slice plan in `docs/m16-planner-package.md` end to end. S1: src/devices/fdc/disk_image.* — deterministic 737,280-byte 2DD image (80x2x9x512, media 0xF9), synthesized-from-constants or fixture tests/parity/m16_boot.dsk (NO host-disk/wall-clock nondeterminism). S2: src/devices/fdc/wd2793.* core — 5 registers (TR=0xFF reset), Type I (restore/seek/step), context-sensitive status; INTRQ/DRQ. S3: Type II (read/write sector) + Type III (read address/track, write track) + Type IV (force interrupt) + DRQ/Lost-Data; all Busy/DRQ/step/index/motor timing off elapsed_cycles() via an FdcClockSource mirroring the M15 RtcClockSource — NEVER wall-clock, and do NOT modify step_cpu_instruction T-state math / datasheet T-states / increment_refresh_register() (protect M9/M12 oracles). S4: src/devices/fdc/{disk_drive,sony_fdc}.* — SonyFdc is a core::MemoryDevice WRAPPING the M13 disk_rom_ RomDevice and owning the page-1 decode: offset o=a&0x3FFF -> 0x7FF8-7FFB WD2793 status/cmd/track/sector/data, 0x7FFC side, 0x7FFD drive/motor + DSKCHG bit2, 0x7FFF ACTIVE-LOW INTRQ(bit6)/DTRQ(bit7), else DISK ROM. Ground the Sony glue decode on openMSX PhilipsFDC.cc (the fact-sheet's inferred table is WRONG — DSKCHG is 0x7FFD bit2, 0x7FFF active-low). S5: machine wiring — replace slot_bus_.attach(3,2,1,&disk_rom_) (hbf1xv_machine.cpp:62) with the SonyFdc wrapper; ~4s delayed motor-off (~14,318,180 cycles) off the scheduler; system-integration test. S6: boot-checkpoint advance (C5) — with the FDC live, boot proceeds past PC 0x454 through the disk-ROM handshake (DSKCHG, drive/motor/side, status/INTRQ poll, Type I Restore, Type II Read Sector LBA 0) to a self-derived checkpoint (PC > 0x454, Read Sector accepted, 512 DRQ transfers, INTRQ no-error); + openMSX A/B trace-diff over a CPU->FDC sequence incl. Read Sector LBA 0 with the IDENTICAL m16_boot.dsk presented to both -> docs/m16-parity-trace-diff.md (real diff or honest BLOCKED). Keep device logic in src/devices/fdc/, machine wiring in src/machine/ (src/CLAUDE.md).
- Acceptance Criteria: MB89311/WD2793 (Sony style) FDC implemented to spec grounded in fact-sheet + openMSX (concrete citations), wired onto the slot-3-2 DISK ROM. Deterministic unit tests (WD2793 Type I/II/III/IV commands + status bits, DRQ/Lost-Data, Sony register decode 0x7FF8-7FFF incl. DSKCHG 0x7FFD bit2 + active-low 0x7FFF, disk-image geometry/read, motor-off timing) + system-integration (CPU->FDC over the bus; boot-checkpoint advance). ALL prior M0-M15 suites remain green — M9/M12 timing oracles UNCHANGED (FDC time off elapsed_cycles only); any test touched updated to the authentic value with justification, never weakened. Evidence gates captured: tools/validate-assets.ps1, tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt, cmake --build build --config Debug, ctest --test-dir build -C Debug --output-on-failure (report EXACT pass/fail). A/B: real diff to docs/m16-parity-trace-diff.md or honest BLOCKED. Update deferred-backlog: B8 -> DONE (M16), C5 advance noted. Produce docs/m16-implementation-report.md.
- Dependencies: docs/m16-planner-package.md; references/fact-sheets/FDC for Sony HB-F1XV.md; references/openmsx-21.0/src/fdc/ (WD2793.*, PhilipsFDC.*, DiskDrive.*, RealDrive.*, DSKDiskImage.*); references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml; src/devices/memory/rom_device.* + M13 slot-3-2 wiring; src/machine/hbf1xv_machine.*; M15 RtcClockSource pattern; guardrails.
- Requested At: 2026-07-06T18:20:00+09:00

- Request ID: REQ-M16-004
- From: MSX Master Agent (coordinator)
- To: MSX QA Agent
- Milestone ID: M16
- Type: Regression assessment + sign-off recommendation
- Scope: Independently verify REQ-M16-003's implementation evidence (coordinator had already re-run the build/ctest gates directly before dispatch) and produce a Pass/Conditional Pass/Fail recommendation. Explicitly weigh two items: (1) the C5 residual (planner boot-checkpoint acceptance signal (a)-(c) not reached via the real unattended BIOS auto-boot path, though PC-advance signal (d) is satisfied); (2) two mid-cycle bug fixes (HEAD_LOADED semantics, Type-IV i2 index-pulse INTRQ scheduling) made outside the original plan while A/B-probing — verify both are genuine, correctly grounded in `references/openmsx-21.0/src/fdc/WD2793.cc`, and that no existing test was weakened to accommodate them.
- Acceptance Criteria: `docs/m16-qa-signoff.md` produced with an explicit verdict; QA-executed (not just re-read) build/ctest/A-B reproduction; RESP-M16-004 filed with the verdict and residual risks.
- Dependencies: docs/m16-planner-package.md; docs/m16-implementation-report.md; docs/m16-parity-trace-diff.md; RESP-M16-003; references/openmsx-21.0/src/fdc/WD2793.cc, PhilipsFDC.cc.
- Requested At: 2026-07-07T00:35:00+09:00

---

- Request ID: REQ-M16-005
- From: Human (project owner, source of truth) via /msx-master directive "proceed with closing M16 with tagging please"
- To: MSX Master Agent (coordinator)
- Milestone ID: M16
- Type: Human release decision (milestone closure)
- Scope: Close M16 (FDC Drive Mechanics — Fujitsu MB89311/WD2793, Sony connection style) on QA PASS (REQ-M16-004 / RESP-M16-004, docs/m16-qa-signoff.md). Tag v1.0.16.
- Acceptance Criteria: M16 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.16; backlog B8 remains DONE (M16), C5 remains honestly IN-PROGRESS (not force-closed).
- Requested At: 2026-07-07T01:10:00+09:00

---

- Request ID: REQ-M17-001
- From: Human (project owner) via /msx-master directive "go ahead and kick off M17 and M18 until QA sign-off. Ensure to review deferred items and have them properly addressed during the development."
- To: MSX Master Agent (coordinator)
- Milestone ID: M17
- Type: Milestone kickoff
- Scope: Open M17 = FM-PAC (Yamaha YM2413 OPLL, 9-channel FM synthesizer) device internals + backlog B4 (MSX-JE 16 KB battery-backed SRAM). Per the indicative follow-on order (DEC-0009) and `agent-protocol/state/deferred-backlog.md` row B3 (FM-PAC internals, OPEN since M13) and B4 (MSX-JE SRAM, OPEN since M13). M13 already mapped the FM-MUSIC ROM presence at slot 3-3; the sound-generation device itself and its battery-backed SRAM store are unbuilt. Grounding: `references/fact-sheets/Yamaha YM2413 FM Chip.md` (exists — B3's original grounding note "OPLL sheet when added" is now satisfied) and openMSX `references/openmsx-21.0/src/sound/YM2413*.{cc,hh}` + `references/openmsx-21.0/src/sound/YM2413Core.hh`. Normal human-release-decision gate (no auto-close) — per DEC-0003, the M12 auto-close grant does not extend to M17.
- Acceptance Criteria: M17 milestone entry present in milestones.md; planner-first; backlog B3 -> IN-PROGRESS (M17), B4 -> IN-PROGRESS (M17); explicit review of ALL deferred-backlog rows per the human's "review deferred items" directive, not just B3/B4, re-affirming the rest OPEN/IN-PROGRESS with justification.
- Dependencies: M16 Done (v1.0.16, tag confirmed); M13 FM-MUSIC ROM slot 3-3 mapping; S1985 fact-sheet §9 (MSX-JE SRAM reference); deferred-backlog B3/B4.
- Requested At: 2026-07-07T02:00:00+09:00

---

- Request ID: REQ-M17-002
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M17
- Type: Implementation (slices S1..S5)
- Scope: Implement `docs/m17-planner-package.md` end to end, per DEC-0012's approved scope correction. S1: `src/devices/audio/ym2413_opll.{h,cpp}` — 64-register file, two-port `#7C`(address-latch)/`#7D`(data, masked `&0x3F`) write protocol, `reset()` zeroing all registers, debug-only `register_value(addr)` readback, `io_read` returns open-bus `0xFF`. S2: per-channel decode (F-Number/Block/Key-on/Sustain/Instrument/Volume/patch for channels 0-8), rhythm decode (`$0E`, `$36-$38` volumes), the 15-melody + 3-rhythm ROM instrument patch table (verbatim from the fact-sheet) with a `rom_patch(n)` accessor, live user-patch decode from `$00-$07` when `instrument==0`. S3: machine wiring in `src/machine/hbf1xv_machine.{h,cpp}` — attach the device at `io_bus_` ports `#7C`/`#7D` (mirroring the PSG attachment style), add to the `cold_boot()` reset sequence, add a `ym2413()` accessor; the existing `slot_bus_.attach(3,3,1,&fmmusic_rom_)` at slot 3-3 MUST remain completely unchanged (regression guard, A-M17-2) — system-integration test required. S4: `src/devices/memory/battery_backed_sram.{h,cpp}` — a reusable, parametric-size, deterministic battery-backed byte-store generalizing `S1985Engine::load_backup_ram`/`save_backup_ram`; unit-tested standalone at 16KB matching `RomHalnote.cc`'s SRAM size; per DEC-0012, this is explicitly NOT instantiated in `Hbf1xvMachine` and NOT wired to any slot this milestone (no real consumer yet — that's the future Halnote/B6 milestone's job). S5: openMSX A/B evidence — CPU-visible architectural parity across a register-write instruction sequence (extend the existing `tools/openmsx-io-parity.ps1`-style comparator), plus a new register-introspection comparison (`register_value()` vs openMSX's `"MSX Music regs"` SimpleDebuggable via WSL Tcl `debug read`) that MUST be verified against a real WSL openMSX run before being claimed — if it doesn't work, report BLOCKED honestly for that subject and still deliver the first subject. Finalize the full deferred-backlog review at closure (mark B3 DONE, update B4's row text per DEC-0012 — do NOT mark B4 done, add E1/E2 if not already reflected).
- Acceptance Criteria: All 9 acceptance criteria in `docs/m17-planner-package.md` §5. In particular: zero regression across M1-M16 (including the `fmmusic_rom_` slot-3-3 read-path guard and untouched CPU-timing oracles); do NOT build any bank-register/SRAM-handshake/ID-string logic for the sound device (that's the external `MSXFmPac` cartridge pattern, not what this machine has); do NOT wire `BatteryBackedSram` into any slot; evidence gates captured (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1`, `cmake --build`, `ctest`, exact pass/fail); real A/B or honest BLOCKED in `docs/m17-parity-trace-diff.md`; `docs/m17-implementation-report.md` produced.
- Dependencies: `docs/m17-planner-package.md`; DEC-0012; `references/fact-sheets/Yamaha YM2413 FM Chip.md`; `references/openmsx-21.0/src/sound/YM2413Okazaki.cc` (register/latch semantics), `YM2413.hh` (debuggable pattern), `MSXMusic.hh/cc` (device shape); `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:179-197`; M13 `fmmusic_rom_` slot 3-3 wiring; M15 `S1985Engine::load_backup_ram`/`save_backup_ram` + `psg_ym2149.h` precedents; `src/CLAUDE.md`.
- Requested At: 2026-07-07T02:25:00+09:00

---

- Request ID: REQ-M17-003
- From: Human (project owner, source of truth) via /msx-master directive "close M17 and M18. commit, and tag seperately."
- To: MSX Master Agent (coordinator)
- Milestone ID: M17
- Type: Human release decision (milestone closure)
- Scope: Close M17 (FM-PAC — YM2413 OPLL register-accurate device) on QA PASS (REQ-M17-002 / RESP-M17-003, docs/m17-qa-signoff.md). Tag v1.0.17. Commit separately from M18.
- Acceptance Criteria: M17 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.17; backlog B3 remains DONE (M17), B4 remains OPEN/re-owned to B6 (not force-closed).
- Requested At: 2026-07-07T12:15:00+09:00

---

- Request ID: REQ-M18-001
- From: Human (project owner) via /msx-master directive "go ahead and kick off M17 and M18 until QA sign-off. Ensure to review deferred items and have them properly addressed during the development."
- To: MSX Master Agent (coordinator)
- Milestone ID: M18
- Type: Milestone kickoff
- Scope: Open M18 = peripheral I/O — Kanji-font ROM access via I/O `#D8-#DB` (backlog B5: 256KB JIS1+JIS2 font, `bios/f1xvkfn.rom`, present and confirmed; M13 mapped the Kanji driver at slot 3-1 but not the I/O-accessed font) + printer `#90/#91` (Centronics) and cassette interface (backlog C7, baseline scope, never built). Per the indicative follow-on order (DEC-0009, re-confirmed) and the human's bundled M17+M18 directive. Grounding: `references/openmsx-21.0/src/MSXKanji.{hh,cc}` + `MSXKanji12.{hh,cc}` (Kanji I/O device reference); `references/openmsx-21.0/src/{Printer,PrinterPortSimpl,MSXPrinterPort,PrinterPortDevice}.*` (printer reference); `references/openmsx-21.0/src/cassette/{CassetteDevice,CassettePort,CassettePlayer}.*` (cassette reference); `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §8/§9; `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` for exact port/device wiring on this machine. Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope).
- Acceptance Criteria: M18 milestone entry present in milestones.md; planner-first; backlog B5 -> IN-PROGRESS (M18), C7 -> IN-PROGRESS (M18); explicit review of the FULL deferred-backlog (not just B5/C7) per the human's "review deferred items" directive, re-affirming the rest with justification — same discipline as M17's package.
- Dependencies: M17 QA PASS (met, RESP-M17-003); M13 Kanji driver at slot 3-1; M11 io_bus; `bios/f1xvkfn.rom` asset (confirmed present); deferred-backlog B5/C7.
- Requested At: 2026-07-07T03:15:00+09:00

---

- Request ID: REQ-M18-002
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M18
- Type: Implementation (slices S1..S5)
- Scope: Implement `docs/m18-planner-package.md` end to end. S1: `src/devices/kanji/kanji_font_rom.{h,cpp}` — `KanjiFontRom` as a `core::IoDevice`: two address counters `adr1_`/`adr2_`, the exact six-behavior `#D8-#DB` protocol (write `#D8`/`#D9` build `adr1_`, write `#DA`/`#DB` build `adr2_` preserving bit 17, read `#D9`/`#DB` return the stored 256KB image with 32-byte-wrap auto-increment on the low 5 bits, `#D8`/`#DA` reads are open-bus `0xFF`), `reset()` restoring `adr1_=0x00000`/`adr2_=0x20000`. Ground every behavior in `references/openmsx-21.0/src/MSXKanji.cc` (NOT `MSXKanji12.cc` — this machine uses the direct-port variant, confirmed via the XML, not the switched-I/O `0xF7` variant). S2: `src/peripherals/printer_port.{h,cpp}` — `PrinterPort` as a `core::IoDevice` at the real `#90-#97` claim (8 ports, mod-4 dispatch): strobe/data write protocol, falling-edge byte capture into `captured_bytes()`, deterministic always-ready status (`0x00`), `#93`/`#97` PDIR unimplemented (matches openMSX's own scope limit). Ground in `references/openmsx-21.0/src/MSXPrinterPort.cc` and `Printer.cc`. S3: `src/peripherals/cassette_interface.{h,cpp}` — `CassetteInterface` (NOT a `core::IoDevice` — no CPU-visible port): `motor_on()`/`output_level()` derived read-only, on-demand, from the existing `Ppi8255` (`cassette_motor_off()`, `port_c_latch() & 0x20`) with zero edits to `ppi_8255.{h,cpp}`; a deterministic synthetic-tape input model (`load_synthetic_tape(bits, cycles_per_bit)`, cycle-driven via an injected `CassetteClockSource` mirroring `RtcClockSource`/`FdcClockSource`, no-tape default always idle-high). Additive edit to `src/peripherals/joystick.{h,cpp}`: new `CassetteInputSource` interface + `attach_cassette_input_source()`, replacing the hardcoded `kCassetteInputBit` idle-high stub at `joystick.h:41-42` with a nullptr-safe injected source (unattached path MUST remain byte-for-byte identical to current M15 behavior — hard regression guard). S4: machine wiring in `src/machine/hbf1xv_machine.{h,cpp}` — attach all three devices, add a `CassetteClock` nested class + member (X4 pattern), wire `cassette_.attach_clock_source(&cassette_clock_)` and `joystick_.attach_cassette_input_source(&cassette_)`, add to `cold_boot()` reset sequence, load `bios/f1xvkfn.rom` via the existing `RomAssetLoader`, add `kanji()`/`printer()`/`cassette()` accessors. NO edits to `step_cpu_instruction()`/`run_cycles()`/`run_frame()` or `ppi_8255.{h,cpp}` — cassette clock consultation is pull-only. System-integration test required (real CPU program drives all three devices over the M11 bus; regression guard re-runs M15 boot-checkpoint-class assertions). S5: openMSX A/B evidence — three subjects per planner §2.6 (Kanji content parity conditioned on a SHA1 identity check against the XML-cited `218d91eb...`, reporting the actual disposition honestly either way; printer write-path-only parity avoiding the known status-bit default divergence; cassette idle-state + write-path parity). If any mechanism cannot be verified against real WSL openMSX, report BLOCKED honestly for that subject.
- Acceptance Criteria: All 10 acceptance criteria in `docs/m18-planner-package.md` §5. In particular: zero regression across M1-M17 (including the M15 `JoystickPorts`/`Ppi8255` goldens and untouched CPU-timing oracles); do NOT build a switched-I/O `MSXKanji12`-style dispatch; implement the printer at the accurate `#90-#97` claim, not the backlog's narrower `#90/#91` wording; evidence gates captured (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1`, `cmake --build`, `ctest`, exact pass/fail); real A/B or honest BLOCKED in `docs/m18-parity-trace-diff.md`; `docs/m18-implementation-report.md` produced.
- Dependencies: `docs/m18-planner-package.md`; `references/openmsx-21.0/src/MSXKanji.{hh,cc}`; `references/openmsx-21.0/src/{MSXPrinterPort,Printer,PrinterPortDevice}.*`; `references/openmsx-21.0/src/cassette/{CassettePort,CassetteDevice,DummyCassetteDevice}.*`; `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`; M13 `RomAssetLoader`; M15 `Ppi8255`/`JoystickPorts`/`RtcClockSource` precedents; M17 `Ym2413Opll` no-clock-consumer precedent; `src/CLAUDE.md`.
- Requested At: 2026-07-07T04:05:00+09:00

---

- Request ID: REQ-M18-003
- From: Human (project owner, source of truth) via /msx-master directive "close M17 and M18. commit, and tag seperately."
- To: MSX Master Agent (coordinator)
- Milestone ID: M18
- Type: Human release decision (milestone closure)
- Scope: Close M18 (Peripheral I/O — Kanji-font ROM, printer, cassette) on QA PASS (REQ-M18-002 / RESP-M18-003, docs/m18-qa-signoff.md). Tag v1.0.18. Commit separately from M17.
- Acceptance Criteria: M18 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.18; backlog B5/C7 remain DONE (M18); F1/F2 remain OPEN.
- Requested At: 2026-07-07T12:15:00+09:00

---

- Request ID: REQ-M19-001
- From: Human (project owner) via /msx-master directive "move on into M19."
- To: MSX Master Agent (coordinator)
- Milestone ID: M19
- Type: Milestone kickoff
- Scope: Open M19 = cartridge loading (deferred-backlog B7) — external primary slots 1 & 2, confirmed via `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:119,121` (`<primary external="true" slot="1"/>`, `<primary external="true" slot="2"/>`, each a single, unexpanded slot, not a multi-secondary bay). M13 left both reserved open-bus. Per the indicative follow-on order (DEC-0009, re-confirmed at every kickoff since). Grounding: `references/openmsx-21.0/src/CartridgeSlotManager.{hh,cc}` (slot management), `references/openmsx-21.0/src/memory/{RomFactory,MSXRomCLI,RomTypes,RomInfo}.{hh,cc}` (mapper-type dispatch + CLI ROM loading), `references/openmsx-21.0/src/memory/Rom{Plain,Generic8kB,Generic16kB,Ascii8kB,Ascii16kB,KonamiSCC,Konami}.{hh,cc}` (the common mapper-type family — survey and decide which are in-scope MVP vs deferred depth). Local sample asset `roms/aleste.rom` (2MB) already present for bring-up/testing. Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope).
- Acceptance Criteria: M19 milestone entry present in milestones.md; planner-first; backlog B7 -> IN-PROGRESS (M19); explicit review of the FULL deferred-backlog (not just B7) re-affirming the rest with justification, matching the M17/M18 discipline; if any backlog-wording vs. XML-grounding discrepancy is found (the M17/M18 precedent), surface it explicitly before implementation.
- Dependencies: M18 Done (v1.0.18, tag confirmed); M11 io_bus/slot_bus; `roms/aleste.rom` sample asset; deferred-backlog B7.
- Requested At: 2026-07-07T13:00:00+09:00

---

- Request ID: REQ-M19-002
- From: MSX Master Agent (coordinator)
- To: MSX Developer Agent
- Milestone ID: M19
- Type: Implementation (slices S1..S6)
- Scope: Implement `docs/m19-planner-package.md` end to end. S1: `src/devices/cartridge/{cartridge_rom_window,cartridge_mapper_type}.{h,cpp}` — the shared 8-slot x 8KB window primitive implementing `RomBlocks::setRom`'s exact bank-resolution algorithm (mask is a FALLBACK for out-of-range requests only, never an unconditional AND-mask — the single most subtle detail in this milestone), and the 6-value `CartridgeMapperType` enum + openMSX-canonical name parsing (case-insensitive; unrecognized name -> `nullopt`, never a silent default). S2: the six MVP mapper devices (`CartridgeMirroredRom`, `CartridgeGeneric8kbRom`, `CartridgeGeneric16kbRom`, `CartridgeAscii8kbRom`, `CartridgeAscii16kbRom`, `CartridgeKonamiRom`) under `src/devices/cartridge/`, each implementing `CartridgeMapperDevice`, byte-exact per planner package §2.2 — pay special attention to Ascii16kB's "both middle logical banks default to the SAME bank at reset" quirk and Konami's "block 2/0/1 permanently fixed at bank 0, mirrored, never re-writable" quirk. S3: `src/devices/cartridge/cartridge_slot.{h,cpp}` — the wrapper actually attached to the bus; empty-slot state must be byte-for-byte identical to the M13-M18 open-bus default (`0xFF` reads, no-op writes); `load()`/`unload()`/`reset()` dispatch across the 6 types; a size-invalid load returns a documented error and leaves the slot's PRIOR state completely untouched. S4: machine wiring in `src/machine/hbf1xv_machine.{h,cpp}` — attach `cartridge_slot1_`/`cartridge_slot2_` at primary slots 1 and 2 (all 4 pages each) WITHOUT calling `set_expanded(1,...)`/`set_expanded(2,...)`; add `cartridge_slot1_.reset()`/`cartridge_slot2_.reset()` to `cold_boot()` (reinitializes bank state, never unloads); add `load_cartridge`/`unload_cartridge`/`cartridge_slot1()`/`cartridge_slot2()` machine API. System-integration test required (real CPU program selects each slot via `#A8`, performs each mapper's bank-switch sequence, reads back marker bytes; an unloaded-slot regression-guard case; a `cold_boot()`-preserves-cartridge case; assert `machine.slot_expanded(1) == false` and `machine.slot_expanded(2) == false`). ALSO in S4: a SEPARATE integration test loading the real `roms/aleste.rom` (2,097,152 bytes) as `Generic8kB`, asserting ONLY: it loads without error, its SHA256 matches `docs/asset-checksums.txt`, and after routing primary slot 1 into CPU page 1, `debug_bus_read(0x4000) == 0x41` ('A') and `debug_bus_read(0x4001) == 0x42` ('B') — explicitly documented in the test file's own comment as a MECHANICAL smoke check only, with NO claim about the file's real-world game/mapper identity. S5: `src/machine/cartridge_cli.{h,cpp}` (pure argv parser, no file I/O, no `Hbf1xvMachine` dependency) + additive `src/main.cpp` edit wiring `--cart1 <path> [--cart1-type <name>]` / `--cart2 <path> [--cart2-type <name>]` into BOTH the default/normal run path and the existing `--parity-trace` mode, sharing the SAME parser/loader code. The new missing/invalid-cartridge policy (loud stderr diagnostic + non-zero exit) is DELIBERATELY stricter than and MUST NOT leak into the existing `RomAssetLoader` BIOS/Kanji-font/disk-image graceful-degradation path — verify this with a regression check. S6: openMSX A/B evidence — build `tools/gen-m19-cartridge-probe.py` + `tools/openmsx-m19-cartridge-parity.ps1`; mount an identically-authored SYNTHETIC test cartridge on both this emulator and real WSL openMSX, diff PC/registers/flags per instruction AND the read-back ROM byte VALUES (content-bearing, not just architectural). BEFORE claiming any slot-specific result, empirically verify which openMSX CLI slot-letter (`-carta`/`-cartb`/etc.) maps onto THIS machine's XML-declared primary slot 1 vs. slot 2 — if this cannot be confirmed, report that specific sub-claim as BLOCKED honestly while still delivering the architectural/content-parity evidence (which doesn't depend on knowing which of the two interchangeable slots was used).
- Acceptance Criteria: All 12 acceptance criteria in `docs/m19-planner-package.md` §5. In particular: zero regression across M1-M18 (including the CPU-timing oracles, untouched by construction since no mapper device consults `elapsed_cycles()`); `roms/aleste.rom` is NEVER asserted as a real, identified cartridge anywhere in the implementation report or tests; the bank-resolution mask subtlety (R-M19-1) is implemented and tested exactly per `RomBlocks.cc:107-118`; evidence gates captured (`tools/validate-assets.ps1`, `tools/checksum-assets.ps1`, `cmake --build`, `ctest`, exact pass/fail); real A/B or honest BLOCKED (for the slot-lettering sub-claim only) in `docs/m19-parity-trace-diff.md`; `docs/m19-implementation-report.md` produced.
- Dependencies: `docs/m19-planner-package.md`; `references/openmsx-21.0/src/memory/{RomBlocks,RomPlain,RomGeneric8kB,RomGeneric16kB,RomAscii8kB,RomAscii16kB,RomKonami,RomFactory,MSXRomCLI,RomTypes,RomInfo}.{hh,cc}`; `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:119,121`; `roms/aleste.rom`; M13 `RomAssetLoader` precedent (contrast, don't reuse its graceful-degradation policy); `src/CLAUDE.md`.
- Requested At: 2026-07-07T13:40:00+09:00

---

- Request ID: REQ-M19-003
- From: Human (project owner, source of truth) via /msx-master directive "M19 and tag v1.0.19"
- To: MSX Master Agent (coordinator)
- Milestone ID: M19
- Type: Human release decision (milestone closure)
- Scope: Close M19 (Cartridge Loading — external primary slots 1 & 2, six MVP mapper types) on QA PASS (REQ-M19-002 / RESP-M19-004, docs/m19-qa-signoff.md). Tag v1.0.19.
- Acceptance Criteria: M19 Done in milestones.md + definition-of-done.yaml (overall_done: true); annotated tag v1.0.19; backlog B7 -> DONE (M19); new rows G1-G4 remain OPEN.
- Requested At: 2026-07-07T14:20:00+09:00

---

- Request ID: REQ-M20-001
- From: Human (project owner) via /msx-master directive "go ahead with M20. ensure that all the backlogs related M20 and up to M20 are all addressed and tested."
- To: MSX Master Agent (coordinator)
- Milestone ID: M20
- Type: Milestone kickoff
- Scope: Open M20 = Halnote/MSX-JE firmware mapping, slot 0-3 (deferred-backlog **B6**), TOGETHER WITH backlog **B4** (MSX-JE 16 KB battery-backed SRAM) — confirmed via `agent-protocol/state/deferred-backlog.md` that B4 was explicitly re-grounded and re-owned to B6 during M17 (DEC-0012): the reusable, standalone `BatteryBackedSram` primitive built in M17 (`src/devices/memory/battery_backed_sram.*`, 16KB, unwired) is meant for B6 to consume directly — M20 is where that actually happens. Per the human's explicit instruction to ensure all backlog items related to and up through M20 are addressed, this milestone MUST close both B4 and B6 together, not just B6 while leaving B4 dangling again. Confirmed present: `bios/f1xvfirm.rom` (1,048,576 bytes = 1MB). Confirmed via the machine XML (`references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:105-115`): slot 0, secondary 3 — `<ROM id="HB-F1XV MSX-JE"><rom><filename>hb-f1xv.rom</filename>...</rom><mem base="0x0000" size="0x10000"/><sramname>hb-f1xv_msx-je.sram</sramname><mappertype>Halnote</mappertype></ROM>` (note: openMSX's own filename reference is `hb-f1xv.rom`, NOT `f1xvfirm.rom` — this project's established local-asset-naming convention differs from upstream's XML filenames for every prior ROM too, e.g. `f1xvbios.rom`/`f1xvext.rom`/etc.; planner to confirm this is consistent, not a mismatch). Grounding: `references/openmsx-21.0/src/memory/RomHalnote.{hh,cc}` — a genuinely complex mapper: 1MB ROM as 128×8KB banks (last 512KB additionally addressable as 256×2KB sub-banks), 4 main bank-switch registers (0x4FFF/0x6FFF/0x8FFF/0xAFFF for regions 0x4000-0x5FFF/0x6000-0x7FFF/0x8000-0x9FFF/0xA000-0xBFFF), 2 sub-bank-switch registers (0x77FF/0x7FFF) that can shadow part of main bank at 0x7000-0x7FFF when a sub-mapper-enable bit is set, a separate SRAM-enable bit (both bits are the upper bit, 0x80, of two of the main bank-switch write values) gating 16KB SRAM at 0x0000-0x3FFF. Normal human-release-decision gate (no auto-close per DEC-0003's M12-only scope).
- Acceptance Criteria: M20 milestone entry present in milestones.md; planner-first; backlog B4 -> IN-PROGRESS (M20), B6 -> IN-PROGRESS (M20); explicit review of the FULL deferred-backlog (not just B4/B6) re-affirming the rest with justification, matching the M17-M19 discipline; if any backlog-wording vs. XML/asset-grounding discrepancy is found (the M17 DEC-0012 / M18/M19 self-resolved precedents), surface it explicitly before implementation — in particular the `hb-f1xv.rom` vs. `f1xvfirm.rom` filename question above.
- Dependencies: M19 Done (v1.0.19, tag confirmed); M17 `BatteryBackedSram` primitive (built, unwired); `bios/f1xvfirm.rom` asset (confirmed present, 1MB); deferred-backlog B4/B6.
- Requested At: 2026-07-07T14:40:00+09:00

---

- Request ID: REQ-M20-002
- From: MSX Master Agent (coordinator), approving `docs/m20-planner-package.md` (RESP-M20-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M20
- Type: Implementation (slices S1..S4)
- Scope: Implement `docs/m20-planner-package.md` end to end. New top-level device family `src/devices/halnote/halnote_rom.{h,cpp}` — `HalnoteRom final : public core::MemoryDevice`, reusing `devices::cartridge::CartridgeRomWindow` (M19, A-M20-10) for the main 8-slot 8KB window and `devices::memory::BatteryBackedSram sram_{0x4000}` (M17, A-M20-11) as the real SRAM store — both REUSED verbatim, neither reimplemented. S1: main window + 4 bank-switch registers (window-slots 2-5, write-trigger `(address & 0x1FFF) == 0x0FFF`, `bank = address >> 13`) + SRAM-enable gate wired to the real SRAM (0x0000-0x3FFF, A-M20-6 direct address-range branch, not pointer indirection) + `reset()`. Critical: the bit-0x80 double-duty effect (A-M20-5) — `window_.set_bank(bank, value)` runs UNCONDITIONALLY on the raw byte INCLUDING bit 7, and only afterward does bank==2 separately interpret bit 0x80 as the SRAM-enable flag (bank==3 as the sub-mapper-enable flag) — do not mask bit 7 out before calling `set_bank()`. S2: sub-mapper-enable gate + 2 sub-bank registers (`0x77FF`/`0x7FFF`, A-M20-7: writes always take effect regardless of enable state, only READS are gated) + the 0x7000-0x7FFF read override (A-M20-8: `window_.image()[0x80000 + sub_banks_[idx]*0x800 + (address & 0x7FF)]`, no masking needed). Critical: the shadow ONLY covers 0x7000-0x7FFF, never 0x6000-0x6FFF (R-M20-2, the task's flagged subtlest risk) — get this boundary exactly right. S3 (**closes backlog B4 AND B6 together**): wire into `src/machine/hbf1xv_machine.{h,cpp}` at primary slot 0, secondary slot 3, ALL 4 pages (`Sony_HB-F1XV.xml:111`'s `<mem base="0x0000" size="0x10000">` spans the whole 64KB sub-slot, A-M20-9 — window-slots 6/7 must remain permanently unmapped/0xFF, no write path may ever reach them); load real `bios/f1xvfirm.rom` (1,048,576 bytes) via the established `RomAssetLoader` pattern; add `set_halnote_sram_path`/`halnote_sram_path`/`flush_halnote_sram` machine API mirroring `set_backup_ram_path`/`backup_ram_path`/`flush_backup_ram` exactly (A-M20-12, NO new CLI flag — mirrors the confirmed-by-grep S1985 backup-RAM precedent); `halnote_.reset()` + SRAM path load in `cold_boot()`'s existing reset-block position (before `load_rom_assets()`); confirm secondaries 0/1/2 of primary slot 0 (BIOS/SUB/Kanji/Disk ROMs) are byte-for-byte unchanged. S4: openMSX A/B evidence — `tools/gen-m20-halnote-probe.py` + `tools/openmsx-m20-halnote-parity.ps1`, covering main-bank-switch (incl. the double-duty effect), SRAM enable/content, and the sub-bank shadow boundary specifically; verify whether openMSX's XML `<sha1>` tag is enforced or advisory BEFORE any synthetic-image-swap claim, reporting BLOCKED honestly if unconfirmable; finalize the full 34-row deferred-backlog review in the ledger (no new rows required this cycle per §4's closing note).
- Mandatory test discipline (A-M20-13, R-M20-6): every `#A8`/`#FFFF` hex constant used in M20's own tests/probe must be independently re-derived from `SlotBus`'s own formulas (`primary_for_page`, `sub_for_page`, `write_ffff`'s `sub_slot_register_[primary_for_page(3)]` target selection) with an explicit bit-field-decomposition comment — never copied from a prior milestone's worked example. At least one dedicated test must assert the RESOLVED `(primary, sub, page)` routing via `slot_expanded`/`debug_sub_slot_register` before relying on it in a larger sequence. (Note: the M17 test bug this finding was grounded in — `FmMusicRom_Slot33Page1_UnchangedByYm2413Writes`'s routing AND a separate silent asset-root gap — was independently found and fixed by the coordinator before this dispatch; see DEC-0016. `ctest` reconfirmed 92/92 passing.)
- Acceptance Criteria: All 12 acceptance criteria in `docs/m20-planner-package.md` §5, including: byte-exact Halnote semantics per §2.2 with the A-M20-5 double-duty finding explicitly demonstrated; the REAL `BatteryBackedSram` primitive (not reimplemented) gated correctly; the sub-bank shadow's 0x7000-0x7FFF/0x6000-0x6FFF boundary exactly preserved; wired at the real slot reading the real firmware, BIOS-ROM-at-slot-0-0 regression guard intact; deterministic SRAM persistence (absent path -> zero every cold_boot; configured path round-trips across independent machine instances); zero regression M1-M19 including the CPU-timing oracles; both B4 and B6 close together; real A/B evidence or honest BLOCKED for the SHA1-enforcement sub-claim; evidence gates captured; `docs/m20-implementation-report.md` produced.
- Dependencies: `docs/m20-planner-package.md`; `references/openmsx-21.0/src/memory/{RomHalnote,RomBlocks}.{hh,cc}`; `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:105-115`; `bios/f1xvfirm.rom`; `src/devices/cartridge/cartridge_rom_window.{h,cpp}` (M19); `src/devices/memory/battery_backed_sram.{h,cpp}` (M17); `src/devices/chipset/s1985_engine.cpp` (persistence-lifecycle precedent); `src/CLAUDE.md`.
- Requested At: 2026-07-07T15:15:00+09:00

---

- Request ID: REQ-M20-003
- From: MSX Master Agent (coordinator), after independently verifying the developer's evidence (RESP-M20-002: rebuilt + re-ran ctest 95/95 myself, read the implementation against the reference source, confirmed the A/B dump files genuine)
- To: MSX QA Agent
- Milestone ID: M20
- Type: Regression assessment + sign-off recommendation
- Scope: Independently verify the M20 implementation against `docs/m20-planner-package.md`'s 12 acceptance criteria: rebuild + re-run the full suite; cross-check byte-exact semantics directly against `references/openmsx-21.0/src/memory/RomHalnote.{hh,cc}` (not just the developer's citations); confirm genuine reuse of `CartridgeRomWindow`/`BatteryBackedSram` (no parallel reimplementation); confirm the BIOS-ROM-at-slot-0-0 regression guard is non-vacuous; independently recompute the new tests' `#A8`/`#FFFF` slot-routing arithmetic to confirm it is free of the DEC-0016-class bug; confirm SRAM persistence genuinely round-trips across two independent machine instances; confirm determinism (no clock coupling); confirm the full 34-row deferred-backlog review. Form an independent judgment (not a rubber stamp) on the disclosed A/B divergence (3/14 labels, sub-mapper-shadow read, isolated to the installed openMSX 19.1 runtime) — is it a blocking defect or a reasonable non-blocking residual.
- Acceptance Criteria: `docs/m20-qa-signoff.md` produced with a Pass/Conditional Pass/Fail recommendation, real captured command output, and explicit reasoning (not deference to the developer's framing) on the A/B divergence disposition.
- Dependencies: `docs/m20-planner-package.md`, `docs/m20-implementation-report.md`, `docs/m20-parity-trace-diff.md`, `src/devices/halnote/halnote_rom.{h,cpp}`, the M20 test files, `agent-protocol/channels/decisions.md` (DEC-0016).
- Requested At: 2026-07-07T16:05:00+09:00

---

- Request ID: REQ-M20-004
- From: Human (project owner, source of truth) via /msx-master directive "let's close it with a proper tag."
- To: MSX Master Agent (coordinator)
- Milestone ID: M20
- Type: Human release decision (milestone closure)
- Scope: Close M20 (Halnote/MSX-JE firmware mapping, slot 0-3, + real `BatteryBackedSram` SRAM consumer) on QA PASS (REQ-M20-003/RESP-M20-003, `docs/m20-qa-signoff.md`). Tag v1.0.20.
- Acceptance Criteria: M20 Done in `milestones.md` + `definition-of-done.yaml` (`overall_done: true`); annotated tag v1.0.20; backlog B4 AND B6 -> DONE (M20) in `deferred-backlog.md`; no new backlog rows required this cycle.
- Requested At: 2026-07-07T17:20:00+09:00

---

- Request ID: REQ-M21-001
- From: Human (project owner) via /msx-master directive "let's do M21 - M23 (separate tags each, with qa sign off on each, and of course full system integration test each) like we run autopilot until m23. we need deliberate cross check across the entire system as we go."
- To: MSX Master Agent (coordinator)
- Milestone ID: M21
- Type: Milestone kickoff
- Scope: Open M21 = VDP rendering depth — deferred-backlog rows **D1** (pixel-accurate raster rendering pipeline: per-mode VRAM->framebuffer for all Target-Spec modes TEXT1/2, G1-G7, MULTICOLOR, border, blink, per-scanline output), **D5** (YJK/YJK+YAE color decode + 15-bit DAC output for SCREEN 10/11/12), **D6** (horizontal scroll visual effect R#25/26/27 + interlace/EO fields, blink timing, superimpose/digitize, external video), **D7** (G6/G7 VRAM address interleave in the DISPLAY path — the planner must explicitly scope whether the COMMAND-engine-path interleave, which the backlog notes is "only observable once D1/D3 exist," is separable from the display-path interleave this milestone covers, since D3/command-engine is M22 not M21). M14 (closed, v1.0.14) delivered the VDP register/VRAM/status/interrupt CONTRACT with zero pixel output; M21 is the first milestone to add an actual rendering/pixel-output path. Per the human's explicit directive, this is the first of a THREE-milestone run (M21, M22, M23), each requiring: a separate planner package, developer implementation with full evidence gates, a dedicated system integration test, independent QA sign-off, and its OWN separate tag (mirroring the M17/M18 "commit and tag separately" precedent, not a bundled release) — proceeding through the human release-decision/tag step for each without an additional per-milestone pause request, since the human has pre-authorized this specific three-milestone run in this message. Any milestone that does NOT reach a clean QA PASS is a real blocker: STOP and consult the human rather than proceeding to the next milestone. The human also explicitly requires "deliberate cross-check across the entire system" at each step — i.e., regression verification is not limited to the new device; independently confirm no earlier milestone's device/test/golden is disturbed at every phase.
- Acceptance Criteria: M21 milestone entry present in `milestones.md`; planner-first; a rendering/pixel-output architecture that is deterministic and unit/integration-testable without requiring the SDL3 frontend (C9, not yet built, M26); real openMSX A/B evidence for the new rendering behavior where feasible (honest BLOCKED if a genuine visual-only comparison has no headless equivalent); full deferred-backlog re-affirmation; zero regression across the ENTIRE M1-M20 suite (not just VDP-adjacent tests); QA sign-off; separate human release decision + tag v1.0.21.
- Dependencies: M20 Done (v1.0.20, tag confirmed); M14 VDP register/VRAM/status/interrupt contract (`src/devices/video/v9958_vdp.*`); `references/fact-sheets/Yamaha V9958 VDP.md`; `references/openmsx-21.0/src/video/` (Renderer/VDP.cc display path, read-only grounding); `agent-protocol/state/deferred-backlog.md` rows D1/D5/D6/D7.
- Requested At: 2026-07-07T18:00:00+09:00

---

- Request ID: REQ-M21-002
- From: MSX Master Agent (coordinator), approving `docs/m21-planner-package.md` (RESP-M21-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M21
- Type: Implementation (slices S1..S7)
- Scope: Implement `docs/m21-planner-package.md` end to end, in the existing `src/devices/video/` family (no new device family). New files: `vdp_palette.h` (3-bit->5-bit expansion `(c3<<2)|(c3>>1)`, RGB555 pack/unpack), `frame_buffer.h` (`FrameBuffer`/`Field` types), `vdp_frame_renderer.{h,cpp}` (`VdpFrameRenderer`, pure pull-model renderer over `const V9958Vdp&`, zero new clock consumer). S1: infra + TEXT1 (240x192, 40 chars x 6px) + GRAPHIC1 (256x192) as pipeline proof. S2: TEXT2 (+blink, countdown driven by the EXISTING `on_vsync()` hook, re-armed `(blink_state_ ? R13>>4 : R13&0xF)*10`), GRAPHIC2/3, MULTICOLOR (256-wide canvas with 4x4 color-cell grouping — NOT a literal 64x48 image, the fact-sheet's table is a cell-grid not a pixel canvas), TEXT1Q/MULTIQ/Unknown -> flat blank fill (palette entry 15, since HB-F1XV's V9958 is never `isMSX1VDP()`). S3: GRAPHIC4/GRAPHIC5 non-planar bitmap modes. S4: `vdp_base_is_planar(base)` helper + the D7 CPU-port edit to the EXISTING M14 `V9958Vdp::effective_address()` (planar transform `addr = (addr>>1) | ((addr&1)<<16)`, a 17-bit rotate-right-by-1 -- confirmed exact via direct source read of `VDP.cc:849-857`; `advance_vram_pointer()`'s R#14-carry logic at `VDP.cc:883-887` operates on the ORIGINAL vramPointer and must remain UNCHANGED -- re-run the existing M14 pointer-carry tests unmodified to confirm) + the renderer's `planar_row_spans` bank-split helper (D7 display-path piece) + GRAPHIC6 + GRAPHIC7 (fixed 256-color decode is byte layout **GGGRRRBB** -- green in the TOP 3 bits, bits 7-5; red bits 4-2; blue bits 1-0 via a 2-bit->3-bit map `{0,2,4,7}` -- confirmed exact via direct source read of `SDLRasterizer.cc:330-336`, NOT the naively-expected RRRGGGBB). Include a genuine end-to-end cross-path test: write via the CPU port at chosen logical addresses in G6/G7 mode, then render, confirming non-garbled expected content (this is the strong correctness signal for D7, since a CPU-port-only round-trip test would pass even with the transform omitted on both sides). S5: YJK (SCREEN12) and YJK+YAE (SCREEN10/11) decode per `k=(p[0]&7)+((p[1]&3)<<3)-((p[1]&4)<<3)`, `j` symmetric from p[2]/p[3], `y=p[n]>>3`, `r=clamp(y+j,0,31)`, `g=clamp(y+k,0,31)`, `b=clamp((5*y-2*j-k+2)/4,0,31)` using PLAIN C++ truncating `int` division (never `std::floor()` -- both are C++ so plain `/` is automatically byte-exact to the reference; a "helpful" floor() call would silently diverge for negative numerators). S6: vertical scroll (R#23), horizontal scroll character-mode (R#26, applied inside the per-mode converter) and bitmap-mode (R#26/R#27, independently ground the rasterizer-level windowing formula at implementation time, cite the specific source line range used, do not assume it's the same mechanism as character modes), border mask (R#25 bit1), multi-page scroll (R#25 bit0 + R#2 bit5), interlace/EO bitmap-page addressing (hedged to an "openMSX-parity" bar since even openMSX's own source carries "TODO: verify" comments on this exact mechanism -- do not claim independent ground-truth confidence beyond that), superimpose/digitize explicitly documented as N/A (HB-F1XV has no digitizer hardware, fact-sheet §9) -- cite this, do not silently skip it. S7: `Hbf1xvMachine::render_frame(Field)` accessor (additive only); a dedicated SYSTEM integration test (per `tests/CLAUDE.md`'s three-tier convention) driving a real CPU program over the M11 bus that writes VRAM/registers via `OUT` and renders, asserting a hand-computed golden per mode family; A/B evidence via a derived-value (not screenshot-pixel) comparison technique over the Tcl debugger, covering 16-color palette expansion, GRAPHIC7 fixed-color decode, YJK/YAE decode (incl. a negative-numerator rounding-boundary case), and the D7 CPU-port planar transform -- report BLOCKED honestly for any introspection point that turns out infeasible; full deferred-backlog review finalization (D1/D5/D6 -> DONE, D7 -> IN-PROGRESS M21 partial, no new letter-row for the remainder).
- Mandatory regression discipline (human's explicit standing instruction for this M21-M23 run): every slice's evidence gate must include a FRESH, FULL `ctest` run covering the ENTIRE M1-M20 suite (95 tests) plus new M21 tests -- not an assumption that untouched files can't have regressed. In particular confirm the M9/M12 CPU-timing oracles remain untouched and the existing M14 VRAM-pointer/R#14-carry unit tests remain green UNMODIFIED after the D7 edit.
- Acceptance Criteria: All 12 acceptance criteria in `docs/m21-planner-package.md` §5 -- deterministic RGB555 FrameBuffer output requiring no SDL3/PNG; every Target-Spec mode byte-exact with independently-verified dimensions; YJK/YAE byte-exact including the rounding risk and GGGRRRBB boundary; D7's CPU-port + display-path pieces closed with the command-engine piece explicitly NOT claimed; D1/D5/D6 close in full, D7 advances to IN-PROGRESS (M21 partial); zero regression across the FULL M1-M20 suite independently confirmed; real A/B evidence or honest BLOCKED sub-claims; full backlog re-affirmation; QA sign-off before closure; `docs/m21-implementation-report.md` produced.
- Dependencies: `docs/m21-planner-package.md`; `references/openmsx-21.0/src/video/{CharacterConverter,BitmapConverter,DisplayMode,VDP,VDPVRAM,SDLRasterizer}.{hh,cc}` (read-only grounding, never copy into src/); `references/fact-sheets/Yamaha V9958 VDP.md`; `src/devices/video/v9958_vdp.{h,cpp}` (M14, existing contract); `src/CLAUDE.md`.
- Requested At: 2026-07-07T18:30:00+09:00

---

- Request ID: REQ-M21-003
- From: MSX Master Agent (coordinator), after independently verifying the developer's evidence (RESP-M21-002: rebuilt + re-ran ctest 106/106 myself, confirmed the D7 edit's confinement, verified the interlace-bug and YJK-rounding findings, confirmed the A/B dump files genuine)
- To: MSX QA Agent
- Milestone ID: M21
- Type: Regression assessment + sign-off recommendation
- Scope: Independently verify the M21 implementation against `docs/m21-planner-package.md`'s 12 acceptance criteria: rebuild + re-run the FULL M1-M20+M21 suite (106 tests) yourself, not a subset; cross-check byte-exact semantics directly against `references/openmsx-21.0/src/video/{CharacterConverter,BitmapConverter,VDP,VDPVRAM,SDLRasterizer}.*` (not just the developer's citations) for at least the GRAPHIC7 GGGRRRBB layout and the D7 planar rotate-right-by-1 transform; confirm the D7 edit to `v9958_vdp.cpp`'s `effective_address()` is surgically confined and `advance_vram_pointer()`/R#14-carry logic is genuinely unmodified; independently assess the developer's two self-disclosed findings (the interlace/EO Field-substitution bug catch, and the YJK floor-vs-truncation "not observable after clamp" mathematical claim) rather than rubber-stamping them; confirm the full 34-row deferred-backlog review; confirm D1/D5/D6 are genuinely ready to close DONE and D7 to advance to IN-PROGRESS (M21 partial) with the command-engine piece honestly not built; scrutinize the A/B evidence (`docs/m21-parity-trace-diff.md`) including the honest BLOCKED disposition for the computed-pixel-color sub-claim — form your own independent judgment on whether the derived-value/raw-VRAM-parity technique is a reasonable substitute for a screenshot-pixel diff, not deference to the developer's framing.
- Acceptance Criteria: `docs/m21-qa-signoff.md` produced with a Pass/Conditional Pass/Fail recommendation, real captured command output (full 106-test run), and explicit reasoning on the two self-disclosed findings and the A/B technique choice.
- Dependencies: `docs/m21-planner-package.md`, `docs/m21-implementation-report.md`, `docs/m21-parity-trace-diff.md`, `src/devices/video/{vdp_frame_renderer,vdp_palette,frame_buffer,v9958_vdp,vdp_mode}.*`, the M21 test files.
- Requested At: 2026-07-07T20:10:00+09:00

---

- Request ID: REQ-M21-004
- From: MSX Master Agent (coordinator), acting on QA PASS (REQ-M21-003/RESP-M21-003) and the human's standing pre-authorization for this M21-M23 run ("let's do M21 - M23 ... like we run autopilot until m23")
- To: MSX Master Agent (coordinator) — self-directed release decision per the pre-authorization
- Milestone ID: M21
- Type: Release decision (milestone closure)
- Scope: Close M21 (VDP Rendering Depth) on QA PASS (`docs/m21-qa-signoff.md`). Tag v1.0.21.
- Acceptance Criteria: M21 Done in `milestones.md` + `definition-of-done.yaml` (`overall_done: true`); annotated tag v1.0.21; backlog D1/D5/D6 -> DONE (M21), D7 -> IN-PROGRESS (M21 partial) in `deferred-backlog.md`; no new backlog rows required this cycle; proceed directly to M22 kickoff per the pre-authorized run.
- Requested At: 2026-07-07T21:10:00+09:00

---

- Request ID: REQ-M22-001
- From: Human (project owner) via /msx-master directive "let's do M21 - M23 (separate tags each, with qa sign off on each, and of course full system integration test each) like we run autopilot until m23. we need deliberate cross check across the entire system as we go." (second milestone of the pre-authorized three-milestone run)
- To: MSX Master Agent (coordinator)
- Milestone ID: M22
- Type: Milestone kickoff
- Scope: Open M22 = sprites + VDP command engine — deferred-backlog rows **D2** (sprite rendering + collision/5th-sprite detection: Sprite Mode 1 & 2, 8/line, per-line color, EC/IC/CC, the 1-px vertical shift, S#0 5S/C and S#3-S#6 collision coords) and **D3** (VDP command engine: R#32-46, HMMC/YMMM/HMMM/HMMV/LMMC/LMCM/LMMM/LMMV/LINE/SRCH/PSET/POINT + logical ops IMP/AND/OR/EOR/NOT + T-variants, S#2 TR/CE handshake, S#7 color, S#8/9 border, R#25 CMD-in-all-modes). Per DEC-0018, this milestone must ALSO close **D7's remaining command-engine-path piece** (the VDP command engine's own coordinate-to-VRAM-address resolution for G6/G7 planar modes, e.g. HMMM/LMMM/LINE/PSET operating in planar coordinate space) — D7 has been IN-PROGRESS since M21, and closing D3 without also closing D7's last piece would leave a tracked row dangling with no future owner. M22 builds atop M21's `VdpFrameRenderer`/`FrameBuffer` architecture (closed, v1.0.21): sprites must composite onto the existing background-plane output; the command engine's VRAM writes must interoperate with the SAME G6/G7 planar-interleave transform D7 already established (`V9958Vdp::effective_address()`'s CPU-port piece, and the renderer's `planar_row_spans` display-path piece) — the planner must determine whether the command engine needs its OWN address-resolution helper (since it operates on X/Y coordinates, not a raw CPU-port address) or can reuse/extend the existing D7 pieces, grounding this explicitly rather than assuming. Per the human's explicit directive, this is the second of the pre-authorized three-milestone run: separate planner package, developer implementation with full evidence gates, a dedicated system integration test, independent QA sign-off, and its own separate tag — proceeding through the release-decision/tag step without an additional pause on a clean QA PASS (pre-authorized). Any milestone that does not reach a clean QA PASS is a real blocker: STOP and consult the human. "Deliberate cross-check across the entire system" required: regression verification must cover the FULL M1-M21 suite (106 tests), not just VDP-adjacent tests.
- Acceptance Criteria: M22 milestone entry present in `milestones.md`; planner-first; sprite rendering + collision detection implemented byte-exact per `references/openmsx-21.0/src/video/SpriteChecker.cc/.hh`, compositing onto M21's `FrameBuffer` output; VDP command engine implemented byte-exact per `references/openmsx-21.0/src/video/VDPCmdEngine.cc/.hh` for at least the commands named in D3's own wording (logical ops + HMMC/YMMM/HMMM/HMMV/LMMC/LMCM/LMMM/LMMV/LINE/SRCH/PSET/POINT); D7's command-engine-path piece closed, D7 -> DONE in full (no longer partial); deterministic unit + integration + a dedicated system integration test; real openMSX A/B trace-diff where feasible (honest BLOCKED otherwise); zero regression across the FULL M1-M21 suite; full deferred-backlog review; QA sign-off; separate human/coordinator release decision + tag v1.0.22.
- Dependencies: M21 Done (v1.0.21, tag confirmed); M21's `VdpFrameRenderer`/`FrameBuffer` architecture (`src/devices/video/vdp_frame_renderer.*`); M14 VDP register/VRAM/status/interrupt contract; `references/fact-sheets/Yamaha V9958 VDP.md` §6/§8; `references/openmsx-21.0/src/video/{SpriteChecker,VDPCmdEngine}.{hh,cc}`; `agent-protocol/state/deferred-backlog.md` rows D2/D3/D7 (partial); DEC-0018 (D7 carry-forward directive).
- Requested At: 2026-07-07T21:20:00+09:00

---

- Request ID: REQ-M22-002
- From: MSX Master Agent (coordinator), approving `docs/m22-planner-package.md` (RESP-M22-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M22
- Type: Implementation (slices S1..S8)
- Scope: Implement `docs/m22-planner-package.md` end to end, in the existing `src/devices/video/` family (no new device family, no new machine-level wiring — `vdp()`/`render_frame()` already suffice). Two new classes, both owned INSIDE `V9958Vdp` as private members (mirrors the existing `blink_countdown_`/`blink_state_` ownership style, NOT new machine-level siblings): `SpriteEngine` (`sprite_engine.{h,cpp}`) and `VdpCommandEngine` (`vdp_command_engine.{h,cpp}`), plus a header-only `vdp_command_address.h` with 5 pure coordinate-to-address functions.

**S1-S2 (sprites, closes D2)**: Sprite Mode 1 (base 0x00/0x02/0x04, max 4/line) and Mode 2 (base 0x08/0x0C/0x10/0x14/0x1C incl. YJK, max 8/line, per-line color/attribute table at a FIXED +512-byte offset within the same sprite-attribute-table window). Critical grounded details (all independently confirmed by the coordinator directly against the reference source): EC=bit7(0x80, x-32 shift, applied at check time), CC=bit6(0x40, color-cascade merge in mode 2), IC=bit5(0x20, EXCLUDES FROM COLLISION ONLY — does NOT suppress rendering, a real and easy-to-miss distinction, confirmed via direct read of `SpriteConverter.hh`'s drawing loop never testing bit 0x20). Color-0 sprite-pixel transparency is CONDITIONED ON R#8 TP (confirmed via `SpriteConverter.hh:110-114/170-171`, which carries the source's own comment "Verified on real V9958: TP bit also has effect in sprite mode 1") — this CONTRADICTS the fact-sheet's blanket "always transparent regardless of TP" prose; implement the CODE behavior, not the fact-sheet's stronger claim. The 1-pixel vertical sprite shift: sprite-visibility test for OUTPUT line L uses `sprite_line = ((L-1)+R23-Y)&0xFF`, and output line 0 is UNCONDITIONALLY sprite-free (there is no "check line -1"). S#0 read clears ONLY bits7/6/5 (F/5S/C), leaving the 5th/9th-sprite-number field (bits4-0) stale until the next frame's recompute (`SpriteChecker.hh:104-110`'s `& 0x1F` mask). S#6 bit1 (EO copy) is honestly UNIMPLEMENTED even in openMSX itself (`SpriteChecker.hh:378-383`'s own "TODO: not yet implemented" comment) — reproduce this exact gap (always 0), do not fabricate a "fixed" version. Sprite pixel compositing is folded DIRECTLY into `VdpFrameRenderer::render_frame()`'s existing pipeline as an overdraw pass onto the SAME `FrameBuffer` (a new `composite_sprites()` step called after the background content is dispatched, before the border-mask step) — NEVER a separate, consumer-visible sprite-plane output type. `SpriteEngine::recompute_frame()` runs once per the EXISTING `on_vsync()` hook (no new clock consumer), computing ALL output lines' visible-sprite lists + collision/5th-sprite status in one deterministic pass; `VdpFrameRenderer` only QUERIES this already-computed state (read-only), never re-derives the sprite-selection algorithm itself.

**S3 (closes D7 in full)**: `VdpCommandEngine`'s own R#32-46 register file (SEPARATE storage from `V9958Vdp::control_regs_`, which stays at its existing 32 entries) with bit widths SX=9-bit/SY,DY,NY=10-bit/DX=9-bit/NX=10-bit (`VDPCmdEngine.cc:1808-1874`). `change_register()`'s existing `if (reg >= kNumControlRegs) return;` early-return becomes a dispatch to the command engine for `reg` in [32,47). scrMode determination: 0=G4,1=G5,2=G6,3=G7(+YJK),4=NonBitmap(any other mode when R#25 CMD bit set),-1=no commands (silent no-op). FIVE NEW, DEDICATED coordinate-based address-resolution functions in `vdp_command_address.h` — confirmed via direct source read that the command engine does NOT and CANNOT call the existing `V9958Vdp::effective_address()` (that function has no X/Y parameters at all; these are genuinely different functions operating on a different input domain). The five formulas, confirmed byte-for-byte by the coordinator directly against `VDPCmdEngine.cc:175-390`: `graphic4_command_address(x,y) = ((y&1023)<<7)|((x&255)>>1)`; `graphic5_command_address(x,y) = ((y&1023)<<7)|((x&511)>>2)`; `graphic6_command_address(x,y) = ((x&2)<<15)|((y&511)<<7)|((x&511)>>2)`; `graphic7_command_address(x,y) = ((x&1)<<16)|((y&511)<<7)|((x&255)>>1)`; `non_bitmap_command_address(x,y) = ((y&511)<<8)|(x&255)`. CRITICAL: none of these formulas reference R#2 at all — commands address BOTH pages of a bitmap mode DIRECTLY through the Y-coordinate's own range (`y&511` spans BOTH G6/G7 pages, `y&1023` spans all 4 G4/G5 pages), completely BYPASSING R#2's display-page-select bits (which only matter for the DISPLAY path, M21's `resolve_bitmap_page()`). Do not copy the display path's page-resolution logic into the command engine's addressing — that would be a genuine defect. Extended VRAM (MXS/MXD/R#45 bit6) is stored but always treated as false (HB-F1XV has no expansion socket, matching the established 128KB-fixed scope boundary).

**S4-S5 (atomic commands)**: 10 commands execute ATOMICALLY within the single call triggered by writing R#46 (CMD): ABRT/STOP (aliased, 4 reserved code-points), POINT, SRCH, HMMV, HMMM, YMMM (no logical-op support — always perform their fixed/implicit operation regardless of the CMD register's low nibble), and PSET/LINE/LMMV/LMMM (WITH logical-op support: IMP/AND/OR/XOR/NOT + T-variants, low nibble selects the op, undefined nibbles are a no-op). CMD>>4 dispatch table (confirmed via `VDPCmdEngine.cc:1963-2028`): 0-3=ABRT,4=POINT,5=PSET,6=SRCH,7=LINE,8=LMMV,9=LMMM,0xA=LMCM,0xB=LMMC,0xC=HMMV,0xD=HMMM,0xE=YMMM,0xF=HMMC. CE (S#2 bit0) is observably 0 immediately after the triggering call returns for these 10 commands. BD (S#2 bit4, SRCH's border-detected flag) is REAL persistent state, cleared only on an explicit S#9 read (`resetBD()`) — do not atomically clear it.

**S6 (the 3 transfer commands, event-driven stateful — a genuine correctness requirement, not a timing nicety)**: LMCM, LMMC, HMMC require an EXPLICIT, multi-step, EVENT-DRIVEN (never wall-clock) state machine, directly mirroring this project's own FDC DRQ/INTRQ precedent (M16, `src/devices/fdc/wd2793.*`) — CE stays 1 across the whole transfer; TR toggles to signal "ready for the next byte/pixel"; the command completes (CE→0) only after NX*NY individual CPU-port interactions (writes to R#44/COL for CPU→VRAM commands LMMC/HMMC, reads of S#7 for VRAM→CPU command LMCM) have each been serviced. This is NOT optional or a nice-to-have: real software's LMCM/LMMC/HMMC control flow physically depends on this multi-step protocol (the pixel/byte data for LMMC/HMMC arrives via SEPARATE, later port writes the CPU hasn't made yet at the moment R#46 is written; for LMCM the CPU must physically read each transferred pixel via S#7 before the next one is produced) — collapsing this to the atomic model would silently DROP every pixel/byte after the first, an outright correctness defect.

**S7 (R#25 CMD-in-all-modes)**: commands execute via `NonBitmapMode` addressing when R#25's CMD bit is set and the display mode is anything other than G4-G7 — writes land in the SAME shared `VdpVram` the M21 renderer already reads, so a command run in e.g. GRAPHIC1 mode must be confirmed to write bytes the EXISTING `render_graphic1` path subsequently displays correctly with ZERO renderer changes (a genuine cross-path test, mirroring M21's own D7 cross-path-test precedent).

**S8 (system integration + A/B + backlog finalization)**: a dedicated SYSTEM integration test (per `tests/CLAUDE.md`'s three-tier convention) driving a real CPU program over the M11 bus that sets up sprites and reads back collision status through a real scenario, AND issues a representative command sequence (at least one atomic command and one transfer command) confirmed via `render_frame()`. A/B evidence via the established raw-byte/register Tcl-debugger technique (stronger/more direct than M21's own color-decode content, since sprite collision flags and command-engine VRAM writes are exactly the kind of raw-byte-comparable state this technique handles well) — cover sprite collision/5th-sprite status (incl. an IC=1 non-collision case and a TP-disabled color-0 collision case), command-engine VRAM writes (incl. a G6/G7-planar destination cross-validating D7's closure), and the transfer-command TR/CE sequence; verify feasibility via a live `debug list` query before claiming any introspection point works, report BLOCKED honestly for any infeasible sub-claim. Full deferred-backlog finalization: D2/D3 -> DONE (M22), D7 -> DONE (M22, no longer partial).
- Mandatory regression discipline (human's explicit standing instruction for this M21-M23 run): every slice's evidence gate must include a FRESH, FULL `ctest` run covering the ENTIRE M1-M21 suite (106 tests) plus new M22 tests. Confirm `V9958Vdp::effective_address()` remains UNCHANGED (`git diff` against the M21 commit, empty diff) — the command engine uses genuinely separate address functions, never a copy-paste-forced reuse of that function.
- Acceptance Criteria: All 13 acceptance criteria in `docs/m22-planner-package.md` §5 — sprite engine + compositing folded into the existing `FrameBuffer` pipeline; the hybrid atomic/event-driven command execution model exactly as specified; D7 closed in full via the 5 new address functions with the R#2-bypass nuance explicitly tested; zero regression across the FULL M1-M21 suite; real A/B evidence or honest BLOCKED sub-claims; full backlog re-affirmation with D2/D3/D7 correctly dispositioned; QA sign-off before closure; `docs/m22-implementation-report.md` produced.
- Dependencies: `docs/m22-planner-package.md`; `references/openmsx-21.0/src/video/{SpriteChecker,SpriteConverter,VDPCmdEngine,DisplayMode,VDP}.{hh,cc}` (read-only grounding, never copy into src/); `references/fact-sheets/Yamaha V9958 VDP.md` §6/§8; `src/devices/video/{v9958_vdp,vdp_frame_renderer,vdp_mode}.*` (M14/M21, existing); `src/devices/fdc/wd2793.*` (M16, the DRQ/INTRQ event-driven precedent); `src/CLAUDE.md`.
- Requested At: 2026-07-07T22:15:00+09:00

---

- Request ID: REQ-M22-003
- From: MSX Master Agent (coordinator), after independently verifying the developer's evidence (RESP-M22-002: rebuilt + re-ran ctest 117/117 myself, confirmed `effective_address()` untouched, confirmed the sprite-enable-gate fix's grounding, confirmed the address functions byte-exact) and finding a framing discrepancy in the A/B evidence summary requiring explicit QA scrutiny
- To: MSX QA Agent
- Milestone ID: M22
- Type: Regression assessment + sign-off recommendation
- Scope: Independently verify the M22 implementation against `docs/m22-planner-package.md`'s 13 acceptance criteria: rebuild + re-run the FULL M1-M21+M22 suite (117 tests) yourself; cross-check byte-exact semantics directly against `references/openmsx-21.0/src/video/{SpriteChecker,SpriteConverter,VDPCmdEngine}.*` for at least the EC/CC/IC bit positions, the color-0/TP transparency conditioning, and the five command-address functions; confirm `V9958Vdp::effective_address()` is genuinely unchanged from the M21 commit; independently assess the self-caught sprite-enable-gate regression fix (`references/openmsx-21.0/src/video/VDP.hh:307-319`'s `spritesEnabledFast()`) for correctness; confirm the full 34-row deferred-backlog review and that D2/D3/D7 are genuinely ready to close (D7 in FULL, no longer partial). **Critical scrutiny item, do NOT rubber-stamp**: `docs/m22-parity-trace-diff.md` labels EVERY ONE of its 7 probes `Result: DIVERGENCE` — not PARITY — even though the implementation report's §6 narrows this to "raw VRAM-byte comparison: genuine PARITY on all 7 probes" with two disclosed residual divergence classes (status-field sampling-moment differences attributed to openMSX's live BIOS continuously polling/clearing S#0 vs. this engine's one-shot deterministic snapshot; a disclosed DY/NY register-mutation-in-place simplification). Independently read the raw diff file yourself and form your OWN judgment: (a) is the "raw VRAM bytes are clean, only STATUS/REG fields diverge" characterization actually accurate on inspection of every probe's Diff section: (b) is the S#0/S#5 status divergence genuinely explainable by BIOS-polling timing (cross-check against the already-confirmed A-M22-14 "S#0 read clears bits7/6/5" mechanic — does a live, continuously-polling BIOS reading S#0 first plausibly explain the SPECIFIC observed differences, e.g. `sprite_mode1_fifth`'s A=0xC4/B=0x85 where B's 5S bit is already 0), or could it indicate a genuine logic defect coincidentally rationalized away; (c) is the DY/NY-not-mutated-in-place omission consistent with the planner package's actual acceptance criteria (re-read §2.3 and §5) — was in-place register mutation ever a stated requirement, or is treating it as an out-of-scope depth-limit reasonable given this milestone's explicitly atomic/event-driven (not cycle-accurate) execution model.
- Acceptance Criteria: `docs/m22-qa-signoff.md` produced with a Pass/Conditional Pass/Fail recommendation, real captured command output (full 117-test run), and explicit, independently-reasoned findings on the three A/B scrutiny items above — not deference to the implementation report's own framing.
- Dependencies: `docs/m22-planner-package.md`, `docs/m22-implementation-report.md`, `docs/m22-parity-trace-diff.md`, `src/devices/video/{sprite_engine,vdp_command_engine,vdp_command_address,vdp_frame_renderer,v9958_vdp}.*`, the M22 test files.
- Requested At: 2026-07-07T23:10:00+09:00

---

- Request ID: REQ-M22-004
- From: MSX Master Agent (coordinator), acting on QA PASS (REQ-M22-003/RESP-M22-003) and the human's standing pre-authorization for this M21-M23 run
- To: MSX Master Agent (coordinator) — self-directed release decision per the pre-authorization
- Milestone ID: M22
- Type: Release decision (milestone closure)
- Scope: Close M22 (Sprites + VDP Command Engine, closes D2/D3, finishes D7) on QA PASS (`docs/m22-qa-signoff.md`). Tag v1.0.22.
- Acceptance Criteria: M22 Done in `milestones.md` + `definition-of-done.yaml` (`overall_done: true`); annotated tag v1.0.22; backlog D2/D3 -> DONE (M22), D7 -> DONE (M22) in full in `deferred-backlog.md`; no new backlog rows required this cycle; proceed directly to M23 kickoff per the pre-authorized run.
- Requested At: 2026-07-07T23:50:00+09:00

---

- Request ID: REQ-M23-001
- From: Human (project owner) via /msx-master directive "let's do M21 - M23 (separate tags each, with qa sign off on each, and of course full system integration test each) like we run autopilot until m23. we need deliberate cross check across the entire system as we go." (third and FINAL milestone of the pre-authorized three-milestone run)
- To: MSX Master Agent (coordinator)
- Milestone ID: M23
- Type: Milestone kickoff
- Scope: Open M23 = exact cycle timing — deferred-backlog rows **C1** (exact cycle/T-state timing parity: cross-emulator wait-state parity beyond the M1 wait, VDP-access recovery waits), **C2** (Z80 HALT-R increment: R continues to increment while halted, per DEC-0004), and **D4** (cycle-accurate VDP access-slot/command timing: 1368 VDP-cycles/line, slot tables 154/88/31, 16-cycle request latency, CPU-access priority, the ~29-Z80-cycle safe-access gap, too-fast dropped-request behavior, exact sub-frame raster position of the line/VBlank IRQ). This is the single biggest architectural departure of the three-milestone run: M21 and M22 BOTH explicitly built pull-model/frozen-snapshot (`VdpFrameRenderer`) and atomic/event-driven-but-not-cycle-scheduled (`VdpCommandEngine`) devices SPECIFICALLY BECAUSE cycle-accurate VDP timing was deferred to this milestone (see `docs/m21-planner-package.md` §1.2, `docs/m22-planner-package.md` §1.2 — both explicitly name D4 as the owner of VDP access-slot/cycle-cost modeling). M23 must introduce genuine VDP access-slot/cycle-cost timing without breaking: (a) the M9/M12 CPU-timing oracles (the project's foundational, most-guarded regression suite), (b) any of the 117 existing deterministic tests built on the M21/M22 pull-model/atomic assumptions (a naive retrofit could force those devices to become cycle-scheduled, invalidating their own test suites), and (c) the FDC's own existing DRQ/INTRQ timing model (M16) or the command engine's event-driven transfer model (M22), both of which the planner must explicitly reconcile with whatever new cycle-cost mechanism this milestone introduces. This is the LAST milestone of the pre-authorized run — the same rules apply (separate tag, own QA sign-off, dedicated system integration test, full-system cross-check across the ENTIRE M1-M22 suite (117 tests), proceed through the release-decision/tag step without an extra pause on a clean QA PASS) — but given the scale of architectural risk, the planner must be EXPLICIT and conservative about what "cycle-accurate" means for THIS milestone's scope (a genuinely full, real-time VDP access-slot scheduler affecting every existing device's timing assumptions, vs. a narrower, additive timing-cost/wait-state model that doesn't retrofit the already-shipped pull-model architecture) — do not assume the broader interpretation without explicitly justifying it against the real risk of destabilizing 117 passing tests.
- Acceptance Criteria: M23 milestone entry present in `milestones.md`; planner-first; explicit scope resolution of what "cycle-accurate" means for this specific milestone, justified against the risk of destabilizing the existing pull-model/atomic architecture; C1/C2/D4 (or an explicitly-scoped, honestly-justified subset, with any remainder re-scoped as a new backlog row rather than force-closed) implemented and tested; deterministic unit + integration + a dedicated system integration test; real openMSX A/B trace-diff where feasible; zero regression across the FULL M1-M22 suite (117 tests); full deferred-backlog review; QA sign-off; separate human/coordinator release decision + tag v1.0.23 (final milestone of this run).
- Dependencies: M22 Done (v1.0.22, tag confirmed); M21's `VdpFrameRenderer` (pull-model architecture) and M22's `VdpCommandEngine` (atomic/event-driven architecture), both explicitly built to defer this exact scope; the M9/M12 CPU-timing oracle suites (`references/fact-sheets/Zilog Z80A CPU.md` or equivalent, `references/openmsx-21.0/src/video/VDPAccessSlots.{hh,cc}`); `references/fact-sheets/Yamaha V9958 VDP.md` §2/§7/§8; DEC-0004 (HALT-R deferral); `agent-protocol/state/deferred-backlog.md` rows C1/C2/D4.
- Requested At: 2026-07-08T00:20:00+09:00

---

- Request ID: REQ-M23-002
- From: MSX Master Agent (coordinator), approving `docs/m23-planner-package.md` (RESP-M23-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M23
- Type: Implementation (slices S1..S3)
- Scope: Implement `docs/m23-planner-package.md` end to end. This is a DELIBERATELY conservative, narrow-scope milestone — read the planner package's §2 (Critical Scope-Resolution Section) in full before writing any code, since it explains WHY the scope is narrower than the full C1/D4 backlog wording and what hard constraints must not be crossed.

**S1 (closes C2 in full)**: `Z80aCpu::step()`'s existing halted branch (`src/devices/cpu/z80a_cpu.cpp`, currently `else if (state_.halted()) { tstates = 4; }`) additionally calls the EXISTING `increment_refresh_register()` helper before returning — the `tstates` literal itself stays `4` (bare, datasheet-correct; the machine-level M1-wait arithmetic in `hbf1xv_machine.cpp` already adds +1 automatically via the existing, unmodified formula, producing the correct 5T MSX-accurate result with zero changes to that file). This is grounded in `references/openmsx-21.0/src/cpu/Z80.hh:21` (`HALT_STATES = 4 + WAIT_CYCLES` = 5) and `CPUCore.cc:2510` (`incR(advanceHalt(HALT_STATES, ...))`) — confirmed directly by the coordinator: the SAME physical mechanism drives both the R-register increment and the clock advance; they cannot be separated on real hardware. **Exactly ONE existing test needs a deliberate update**: `tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp` (confirmed by the coordinator directly, lines 62-73) currently asserts `t3==4`/`machine.elapsed_cycles()==22` with a comment stating the now-incorrect assumption "the halted idle step performs no opcode fetch (0 M1) so it stays at 4T" — update to `t3==5`/`elapsed_cycles()==23` (recomputed: `8+5+5+5=23`, was `8+5+5+4=22`), with an updated comment citing the HALT_STATES grounding and DEC-0004 by ID. Before touching ANYTHING else, run `rg "\.halted\(\)" tests/` fresh and diff against the planner package's own enumerated 22-file audit (§1.3 A-M23-3) — every other file uses a "stop stepping exactly at the halt boundary, never step again while already halted" pattern and must NOT be touched; if a new site has appeared since planning, classify it explicitly (safe pattern vs. genuine new risk) and report it, don't silently patch it.

**S2 (VDP access-timing foundation — non-gating, additive only)**: new `src/devices/video/vdp_access_timing.h` (header-only, pure functions/constants): the `VdpAccessDelta` enum (15 named VDP-cycle offsets: D0=0,D1=1,D16=16,D24=24,D28=28,D32=32,D40=40,D48=48,D64=64,D72=72,D88=88,D104=104,D120=120,D128=128,D136=136); `kVdpCyclesPerLine=1368`; `kCpuTstatesPerVdpCycleRatio=6`; `kCpuTstatesPerLine = kVdpCyclesPerLine/6` (=228, cross-check this against the EXISTING `kFrameCycles = 228*262` constant already in `hbf1xv_machine.cpp:14` via a dedicated test — `kCpuTstatesPerLine*262==kFrameCycles`, sharing ONE constant, not two independently-hardcoded literals that could drift apart); `vdp_cycle_within_line(cpu_tstates_since_frame_start)`; `minimum_request_latency_tstates(VdpAccessDelta)` (a lower-bound-only `ceil(delta/6)` conversion of the D16 CPU-request-latency floor, `references/openmsx-21.0/src/video/VDP.cc:842-844`); `kDocumentedSafeAccessGapTStates=29` (the V9958 fact-sheet §2's separate, coarser "safe access" software-discipline convention). **CRITICAL**: these two latency facts (the D16-derived floor and the 29T fact-sheet convention) are DIFFERENT, non-interchangeable hardware facts — keep them as two separately-named, separately-cited constants, NEVER algebraically combined or presented as if one derives the other; write a dedicated test asserting they are unequal and never summed/divided together anywhere in the implementation. Additive-only edit to `src/machine/hbf1xv_machine.{h,cpp}`: new private field `last_vsync_cycle_` updated by one new line inside the EXISTING `run_frame()` (alongside the existing `vdp_.on_vsync()` call); new const accessors `cycles_since_last_vsync()`/`vdp_cycle_position()`. **HARD CONSTRAINT, do not deviate**: `step_cpu_instruction()`, `run_cycles()`, and `run_until_cycle()` must remain byte-for-byte UNCHANGED (verify via `git diff` against the v1.0.22 tag) — this new calculator must NOT be wired into any CPU-execution-gating path this cycle; it is purely queryable/informational. Do NOT touch `src/devices/video/{vdp_frame_renderer,vdp_command_engine,sprite_engine}.*` at all (verify via `git diff`, zero changes) — M21/M22's already-shipped, QA-signed devices need no changes for this foundation. Write a dedicated regression test that re-runs the EXACT M21 (`tests/system/hbf1xv_m21_vdp_render_system_test.cpp:56-62`, three consecutive `LD A,n`/`OUT (#98),A` pairs with zero spacer instructions) and M22 (`tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp:58-78`, five consecutive `LD A,n`/`OUT (#98),A` pairs) CPU-program fixture byte sequences through `step_cpu_instruction()` and asserts the cumulative T-state total is BYTE-IDENTICAL to their pre-M23 (v1.0.22) values — this is the concrete, checkable proof that the new code is inert for every existing call path. Do NOT reproduce openMSX's slot-table arrays (`VDPAccessSlots.cc:14-141`, ~340 numeric entries) under `src/` this cycle — a license-isolation risk explicitly out of scope.

**S3 (backlog/documentation/evidence)**: full 34-row deferred-backlog review; C2 A/B evidence via a genuine live-WSL-Tcl-debugger R-register comparison (verify feasibility first via a live check, per the established M11-M22 methodology — if the specific R-register readback turns out unavailable, report that probe honestly BLOCKED); the D4/C1 numeric wait-cost sub-claim stays explicitly, honestly marked BLOCKED/not-attempted (no feasible technique exists in this project's established methodology, and nothing is gated on the value anyway).

**Regression discipline (zero-tolerance, the human's explicit standing instruction for this M21-M23 run)**: every slice's evidence gate must include a FRESH, FULL `ctest` run covering the ENTIRE M1-M22 suite (117 tests) plus new M23 tests. The following suites are a NAMED, ZERO-TOLERANCE gate — `git diff` against the v1.0.22 tag must show ZERO changes to every one of them: `z80a_unprefixed_unit_test.cpp`, `z80a_trace_export_unit_test.cpp`, `hbf1xv_cpu_parity_integration_test.cpp`, `hbf1xv_m11_parity_checkpoint_integration_test.cpp`, `hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp`, `hbf1xv_parity_checkpoint_integration_test.cpp`, `hbf1xv_indexed_program_integration_test.cpp`, `hbf1xv_cb_program_integration_test.cpp`, `hbf1xv_ldir_program_integration_test.cpp`, `hbf1xv_interrupt_ack_integration_test.cpp`.
- Acceptance Criteria: All 11 acceptance criteria in `docs/m23-planner-package.md` §6 — C2 closes in full with exactly one deliberate golden update and zero collateral changes; the VDP access-timing foundation is provably non-gating (dedicated regression test + `git diff` confirmation); the two latency facts stay separated; full backlog review with C1/D4 correctly disposed as `IN-PROGRESS (M23 partial)` (NOT force-closed) and the precise 5-item remainder named; zero regression across the FULL M1-M22 suite with the named oracle suites showing zero diff; real A/B evidence or honest BLOCKED disposition; no scope beyond §1.1/§1.2 without a new decisions.md entry; `docs/m23-implementation-report.md` produced.
- Dependencies: `docs/m23-planner-package.md`; `references/openmsx-21.0/src/cpu/{Z80,CPUClock,CPUCore}.{hh,cc}`; `references/openmsx-21.0/src/video/{VDP,VDPAccessSlots}.{hh,cc}` (read-only grounding for the foundation only, never copy slot-table data into src/); `src/devices/cpu/z80a_cpu.*`; `src/machine/hbf1xv_machine.*` (M9-M22, existing); `src/CLAUDE.md`.
- Requested At: 2026-07-08T00:45:00+09:00

---

- Request ID: REQ-M23-003
- From: MSX Master Agent (coordinator), after independently verifying the developer's evidence (RESP-M23-002: rebuilt + re-ran ctest 121/121, confirmed all 10 zero-tolerance oracle suites AND the three M21/M22 device files show zero diff, confirmed the HALT-R fix confinement and golden-update arithmetic, read the A/B divergence disclosure)
- To: MSX QA Agent
- Milestone ID: M23
- Type: Regression assessment + sign-off recommendation
- Scope: Independently verify the M23 implementation against `docs/m23-planner-package.md`'s 11 acceptance criteria. This is the highest-CPU-timing-risk milestone in the project — apply the same zero-tolerance scrutiny the planner package itself demands. Rebuild + re-run the FULL M1-M22+M23 suite (121 tests) yourself. Independently confirm via your OWN `git diff v1.0.22` that all 10 named zero-tolerance oracle suites (`z80a_unprefixed_unit_test.cpp`, `z80a_trace_export_unit_test.cpp`, `hbf1xv_cpu_parity_integration_test.cpp`, `hbf1xv_m11_parity_checkpoint_integration_test.cpp`, `hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp`, `hbf1xv_parity_checkpoint_integration_test.cpp`, `hbf1xv_indexed_program_integration_test.cpp`, `hbf1xv_cb_program_integration_test.cpp`, `hbf1xv_ldir_program_integration_test.cpp`, `hbf1xv_interrupt_ack_integration_test.cpp`) AND all three M21/M22 device files (`vdp_frame_renderer.*`, `vdp_command_engine.*`, `sprite_engine.*`) show ZERO changes. Independently re-verify the HALT-R grounding directly against `references/openmsx-21.0/src/cpu/{Z80.hh:19-21,CPUClock.hh:53-59,CPUCore.cc:2508-2511}` and re-derive the golden-test arithmetic by hand (`8+5+5+5=23`) before accepting it. Re-run `rg "\.halted\(\)" tests/` yourself and confirm no site beyond the one deliberately-updated test was touched. Confirm the VDP access-timing foundation is genuinely non-gating (`step_cpu_instruction()`/`run_cycles()`/`run_until_cycle()` byte-for-byte unchanged; the two latency facts never combined). **Two specific items requiring independent judgment, not a rubber stamp**: (1) assess the honestly-reported A/B DIVERGENCE for C2 (`docs/m23-parity-trace-diff.md`) — is the developer's hypothesis (openMSX's live Tcl `step()` advancing by wall-clock-elapsed time while halted, not a fixed quantum) a reasonable, sufficiently-hedged explanation, or does it risk masking a genuine defect; form your own view rather than deferring to the report's framing; (2) assess whether the disclosed, additive `src/main.cpp` CLI extension (`halt_idle_extra_steps`, defaults to 0/no-op) is an acceptable scope note within the already-authorized milestone (mirroring the M19 `--cart1/--cart2` precedent) or should have required a `decisions.md` entry before being added. Confirm the full 34-row deferred-backlog review and that C1/D4 are correctly disposed as `IN-PROGRESS (M23 partial)` (not force-closed) with the precise 5-item remainder named, while C2 is genuinely ready to close DONE in full.
- Acceptance Criteria: `docs/m23-qa-signoff.md` produced with a Pass/Conditional Pass/Fail recommendation, real captured command output (full 121-test run, explicit zero-diff confirmation for all 13 named files), and independently-reasoned findings on the two scrutiny items above.
- Dependencies: `docs/m23-planner-package.md`, `docs/m23-implementation-report.md`, `docs/m23-parity-trace-diff.md`, `src/devices/cpu/z80a_cpu.cpp`, `src/devices/video/vdp_access_timing.h`, `src/machine/hbf1xv_machine.*`, `src/main.cpp`, the M23 test files.
- Requested At: 2026-07-08T01:20:00+09:00

---

- Request ID: REQ-M23-004
- From: MSX Master Agent (coordinator), acting on QA PASS (REQ-M23-003/RESP-M23-003) and the human's standing pre-authorization for this M21-M23 run
- To: MSX Master Agent (coordinator) — self-directed release decision per the pre-authorization
- Milestone ID: M23
- Type: Release decision (milestone closure) — FINAL milestone of the pre-authorized M21-M23 run
- Scope: Close M23 (Exact Cycle Timing — C2 in full, C1/D4 non-gating foundation) on QA PASS (`docs/m23-qa-signoff.md`). Tag v1.0.23. Ratify the disclosed `src/main.cpp` CLI extension per QA's recommendation.
- Acceptance Criteria: M23 Done in `milestones.md` + `definition-of-done.yaml` (`overall_done: true`); annotated tag v1.0.23; backlog C2 -> DONE (M23) in full, C1/D4 -> IN-PROGRESS (M23 partial) with the precise 5-item remainder named in `deferred-backlog.md`; `main.cpp` CLI extension formally ratified in `decisions.md`; no new backlog rows required beyond the already-named remainder; this closes the full pre-authorized M21-M23 run.
- Requested At: 2026-07-08T01:50:00+09:00

---

- Request ID: REQ-M24-001
- From: Human (project owner) via /msx-master directive "sounds good. I confirm that there should be zero license-sensitive future work. Let's advance to all M25, milestone by milestone, fully developed, and fully qa. I would also want to have test articafts generated in debug/ folders (such as generated frames in PNGs, etc) before we wire SDL3 in M26 (I will ask after we complete M25)." — a new, similarly-scoped continuation of the M21-M23 cadence: planner -> developer -> QA -> release decision/tag for each milestone in turn, proceeding without an additional pause on a clean QA PASS (mirroring the immediately-prior run), STOP and consult the human if QA does not reach a clean PASS for either milestone.
- To: MSX Master Agent (coordinator)
- Milestone ID: M24
- Type: Milestone kickoff
- Scope: Open M24 = ZEXDOC/ZEXALL full parity sweep — deferred-backlog row **C3** (needs a legally-sourced ZEX binary, unavailable offline in M12; M12's own A/B trace-diff served as a partial cross-check instead). `references/zexall/` is ALREADY present in the repository (found during M19 verification, flagged but left uncommitted across M19-M23) — a legally-sourced ZEXALL/ZEXDOC Z80 exerciser suite from the YAZE-AG project (GPL-licensed, read-only behavior/validation reference, same isolation discipline as `references/openmsx-21.0/`), containing real, runnable CP/M-style `.com` binaries (`zexall.com`, `zexdoc.com`) plus their `.z80`/`.mac` sources and a `LICENSE`/`README.md`. This milestone must: (a) formally commit `references/zexall/` under version control (it has sat uncommitted for 5 milestones); (b) design and build a genuine harness capable of loading and running these CP/M-targeted `.com` binaries against this project's already-built, QA-verified Z80A CPU core in this project's headless, non-CP/M environment (ZEXALL/ZEXDOC binaries expect a minimal CP/M BDOS environment — at minimum, console-character-output via `CALL 5`/RST-based BDOS function 2/9 — the planner must determine the minimal, honestly-scoped BDOS shim needed, grounded in how the reference suite's own `.z80`/`.mac` source actually invokes it, not guessed); (c) run the FULL ZEXALL and ZEXDOC suites to completion and report genuine, unfabricated pass/fail results for every documented test case (both suites test overlapping but distinct opcode-behavior claims — read the suite's own README/source to understand the distinction, do not assume); (d) capture real openMSX A/B evidence where feasible (running the identical binary under WSL openMSX and comparing final CPU-state/pass-count) or honestly BLOCKED where not. Per the user's explicit, standing "zero license-sensitive future work" requirement (recorded in project memory): this milestone's own scope is comparatively low license risk (RUNNING a third-party GPL test suite as a black-box validation tool is a fundamentally different, much lower-risk act than transcribing a GPL project's own internal data tables into this project's source — but the planner must still explicitly confirm nothing from `references/zexall/`'s own Z80 machine code or assembly source is copied into `src/`, only that the pre-built `.com` binary is loaded as DATA/a test fixture, exactly as `bios/`/`roms/` assets already are). Per the human's directive, this is the first of an M24-M25 continuation of the established milestone cadence — own planner package, developer implementation, dedicated system integration test, QA sign-off, separate tag; proceeding through the release-decision/tag step without an additional pause on a clean QA PASS.
- Acceptance Criteria: M24 milestone entry present in `milestones.md`; planner-first; `references/zexall/` committed; a genuine, minimal, honestly-scoped BDOS/CP/M shim (or equivalent) built and grounded in the suite's own actual BDOS-call sites; the FULL ZEXALL and ZEXDOC suites run to completion with real, captured, unfabricated results (100% pass expected against an already-QA-verified Z80A core, but report honestly whatever is actually observed — including any genuine failures found, which would be a significant, high-value finding requiring careful triage, not silently patched); deterministic unit/integration/system tests; real openMSX A/B evidence where feasible; zero regression across the FULL M1-M23 suite (121 tests); full deferred-backlog review; QA sign-off before closure.
- Dependencies: M23 Done (v1.0.23, tag confirmed); `references/zexall/` (present, uncommitted); the existing, QA-verified Z80A CPU core (`src/devices/cpu/z80a_cpu.*`, M9/M10/M12); `agent-protocol/state/deferred-backlog.md` row C3; the user's "zero license-sensitive future work" standing directive (project memory).
- Requested At: 2026-07-08T03:00:00+09:00

---

- Request ID: REQ-M24-002
- From: MSX Master Agent (coordinator), approving `docs/m24-planner-package.md` (RESP-M24-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M24
- Type: Implementation (slices S0..S4)
- Scope: Implement `docs/m24-planner-package.md` end to end. Read the full package before writing any code — it is the authoritative spec for exact BDOS-shim semantics, the message-format grammar, and (critically) the license-isolation boundary between generic `src/machine/` mechanism code and zexall-specific `tools/` parsing.

**S0**: `git add references/zexall/` (README, LICENSE, `.gitattributes`, both `.z80`/`.mac`/`.com` pairs, `zexlax.pl`) — no code changes.

**S1 (the generic mechanism, `CpmBdosHarness`)**: new `src/machine/cpm_bdos_harness.{h,cpp}`. `load_com(Hbf1xvMachine&, image, top_of_memory=0xFF00)`: `map_flat_ram()`, validate `0x0100 + image.size() < top_of_memory` (defensive, generic size guard — refuse to load rather than silently corrupt memory), `load_memory(0x0100, ...)`, write `top_of_memory` little-endian at address `0x0006` (the standard CP/M "top of TPA" convention, confirmed by the coordinator directly against `references/zexall/zexall.z80:80-81`: `ld hl,(6) / ld sp,hl` — the program reads this word as its own initial stack pointer), set `regs().pc = 0x0100` (confirmed against `zexall.z80:20-23`: `org 100h` / `jp start`). `run(Hbf1xvMachine&, max_instructions)`: loop `step_cpu_instruction()`, but BEFORE each step check `regs().pc`: if `0x0000`, set `finished=true` and stop (CP/M warm-boot = program-finished signal); if `0x0005`, dispatch the BDOS trap WITHOUT calling `step_cpu_instruction()` for that iteration (the CPU must never actually decode whatever raw byte sits at address 5 or 0) — read `regs().c()`: `c==9` reads bytes from `DE` via `read_memory()` until a `'$'` (0x24) terminator, appending each non-`$` byte to a captured-output buffer; `c==2` appends the byte in `regs().e()` directly; any other `c` value is recorded as a non-fatal "unexpected BDOS function" diagnostic. Then synthesize the RET effect manually: pop the return address off the stack (`sp = regs().sp; ret = read_memory(sp) | (read_memory(sp+1)<<8); regs().sp = sp+2; regs().pc = ret;`). Return `{finished, instructions_executed, captured_output, unexpected_bdos_calls}`.

**CRITICAL license-isolation constraint**: `cpm_bdos_harness.*` must contain ZERO knowledge of zexall/zexdoc specifically — no message strings, no CRC constants, no group names or counts, no byte constants transcribed from `zexall.z80`/`zexdoc.z80`'s own source. It implements ONLY the generic, third-party (Digital Research CP/M) loading convention. Verify this yourself before considering S1 done: the file should be usable, in principle, to run ANY small CP/M-style `.com` binary that only needs BDOS functions 2/9, not just this specific exerciser.

New unit tests (`tests/unit/machine/cpm_bdos_harness_unit_test.cpp`) against a TINY, HAND-WRITTEN-BY-YOU synthetic fixture — a handful of literal Z80 bytes you assemble by hand and document with a comment in the test file itself (e.g. `LD DE,<msg>` / `LD C,9` / `CALL 5` / `LD E,'!'` / `LD C,2` / `CALL 5` / `JP 0`, with a short `$`-terminated message) — deliberately NOT derived from `zexall.z80`/`zexdoc.z80`, proving the shim's mechanics work independently before the real GPL binaries are ever loaded. Confirm: the defensive size-guard rejects an oversized image; the captured-output buffer accumulates both a C=9 string and a C=2 character in the correct order; the CP/M warm-boot (`JP 0`) trap sets `finished=true` and stops; the RET-synthesis correctly resumes execution after a BDOS call (test with a fixture that makes multiple BDOS calls); an out-of-budget run (a fixture that never reaches `JP 0`) reports `finished=false` honestly.

**S2 (CLI mode + device-isolation invariant)**: new `--cpm-run <program.com> <max_instructions> <out_log_path>` mode in `src/main.cpp` (mirrors the existing `--parity-trace`/`--bios-boot-trace` shape at `src/main.cpp:739-755`): reads the `.com` file, `cold_boot()`s a fresh machine, calls `CpmBdosHarness::load_com`+`run`, writes captured output verbatim to `out_log_path`, prints a one-line summary to stderr, exits 0 if `finished` else a distinct non-zero code (an honest "ran out of budget" signal, never silently reported as success). New integration test (`tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp`) using the SAME synthetic fixture through the full `Hbf1xvMachine`+`CpmBdosHarness` API, additionally asserting the device-isolation invariant: PSG/VDP/PPI/RTC/FDC state snapshots taken immediately after `cold_boot()`+`map_flat_ram()` and again after the harness run are byte-for-byte identical (this exerciser never touches real I/O ports — confirm this claim yourself by checking that the synthetic fixture and, later, the real ZEXALL run genuinely never diverge these device states).

**S3 (the real system test — first point any GPL binary is loaded)**: `tools/zexall-report.py` (new, Python, non-shipped) parses a captured-output log by scanning for the OBSERVED runtime output markers `"  OK"` / `"  ERROR **** crc expected:"` / `" found:"` in order of appearance, emitting a structured per-group PASS/FAIL table — this is OBSERVED BLACK-BOX OUTPUT, not transcribed source (document this reasoning as an explicit code comment in the file itself). Give it its own small Python unit test against a synthetic captured-output fixture before trusting it on the real binaries — specifically test the edge case where a group's own printed name might contain a decoy substring like "OK" or "ERROR" before the real marker (anchor on structural position, not a bare substring search).

New dedicated system test (`tests/system/hbf1xv_m24_zexall_system_test.cpp`): loads `references/zexall/zexall.com` and separately `zexdoc.com` via a `SONY_MSX_ZEXALL_DIR`-style compile definition (mirror the EXACT established `SONY_MSX_BIOS_DIR` pattern in `tests/CMakeLists.txt` — DEC-0016's standing watch-item explicitly applies here: this test MUST wire its own asset-root path correctly via a `target_compile_definitions` entry, or it will silently degrade/fail under `ctest`'s different working directory). Runs each to completion via `CpmBdosHarness` directly (in-process, no subprocess spawn), asserts `finished==true` for both (not a budget timeout), asserts exactly 67 PASS-or-FAIL markers appear in each captured output, and reports the REAL per-group PASS/FAIL disposition.

**Before attempting the real full sweep, measure runtime empirically** — do not assume it's fast enough for `ctest`. Run a small subset first (the smallest few named groups by combinatorial count), measure actual wall-clock time per `step_cpu_instruction()` call, extrapolate to the full ~1.7M-plus-overhead total, then actually run the full sweep and report the REAL measured time honestly in the implementation report. If the full sweep genuinely proves prohibitive for the standard `ctest` cadence, do NOT silently truncate/skip groups or fabricate a "ran to completion" claim — escalate this specific finding back explicitly rather than resolving it unilaterally.

**Mandatory adversarial self-check**: deliberately, temporarily force ONE group to fail (e.g. temporarily truncate the max-instruction budget in a controlled test-only scenario, or corrupt one byte of a copy of the loaded image in a unit-test-only fixture — NEVER touch the real committed `references/zexall/zexall.com` file itself) and confirm the harness correctly reports FAIL/INCOMPLETE rather than silently passing, then revert. This proves a "100% PASS" result (if that's what's actually observed) is a genuine, discriminating finding.

**THE non-negotiable disposition rule (Acceptance Criterion 4)**: if every one of the 134 group verdicts (67 groups × 2 suites) is PASS, report "100% PASS, real run, real CRC comparisons" — the expected but NOT pre-assumed outcome. If ANY group reports FAIL, report it as a genuine, significant finding (the specific group name(s) as observed output, the expected/actual CRC values as printed, an explicit triage note on whether this looks like a genuine CPU-core defect or a harness/fixture artifact) — WITHOUT silently patching the CPU core (do not touch `src/devices/cpu/*` this cycle under any circumstance) and WITHOUT forcing a false "100% PASS" narrative.

**S4 (A/B evidence + backlog/documentation closure)**: `tools/openmsx-m24-zexall-parity.ps1` — mirror the EXISTING, already-working Tcl mechanics from `tools/openmsx-trace-parity.ps1` (read it first): a RAM-only probe machine (like M10's `C-BIOS_MSX2+` profile, NOT the real `Sony_HB-F1XV` profile, since page 0 there is real BIOS ROM not writable RAM), poke the identical `.com` bytes at 0x0100, seed the same registers/memory, single-step via the same `after break emit`/`step` pattern implementing the identical PC==5/PC==0 trap logic on the Tcl side, BOUNDED to a small prefix (enough to complete the first 1-2 named test groups — verify this bound live at implementation time, don't assume). Diff the captured BDOS-output text between engines for that bounded prefix — genuine PARITY/DIVERGENCE. Explicitly, honestly mark BLOCKED (not attempted): a full-suite live-Tcl single-step A/B (infeasible per M23's own real-time-scheduling finding) and booting a real MSX-DOS disk (depends on an unconfirmed MSX-DOS asset and the still-open C5 backlog item — check whether `bios/`/`roms/` actually contains any MSX-DOS system-disk asset before finalizing this as BLOCKED, don't assume absence).

Full 34-row deferred-backlog review; `docs/m24-implementation-report.md` with the full per-suite PASS/FAIL table, measured runtime, adversarial self-check result, A/B disposition.

**Regression discipline**: run the FULL M1-M23 suite (121 tests) before and after each slice. `git diff` against `v1.0.23` must show changes ONLY in `src/machine/cpm_bdos_harness.*` (new) and an additive edit to `src/main.cpp` — confirm zero changes to any file under `src/devices/`, `src/peripherals/`, or `src/core/`.
- Acceptance Criteria: All 12 acceptance criteria in `docs/m24-planner-package.md` §5 — `references/zexall/` committed; `CpmBdosHarness` proven generic via a synthetic-only unit-test suite; both real suites run to genuine completion with real, captured, unfabricated 134-group results; the non-negotiable disposition rule honored exactly; the adversarial self-check performed; the device-isolation invariant holds; zero regression across the full M1-M23 suite; measured runtime reported honestly; real A/B evidence or honest BLOCKED sub-claims; zero GPL source copied into `src/`; full backlog re-affirmation; `docs/m24-implementation-report.md` produced.
- Dependencies: `docs/m24-planner-package.md`; `references/zexall/{zexall,zexdoc}.z80` (read-only grounding, never copy into src/); `src/machine/hbf1xv_machine.*` (existing public API); `tools/openmsx-trace-parity.ps1` (existing Tcl mechanics to mirror); `src/CLAUDE.md`.
- Requested At: 2026-07-08T03:45:00+09:00

---

- Request ID: REQ-M24-003
- From: MSX Master Agent (coordinator), acting on RESP-M24-002's independent verification (developer implementation reproduced end-to-end: clean rebuild, fast `ctest` subset 123/123, the slow `hbf1xv_m24_zexall_system_test` independently re-run to an EXACT figure-for-figure match — `ZEXALL`/`ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` — `git diff v1.0.23` confirmed empty for `src/devices/`/`src/peripherals/`/`src/core/`, `cpm_bdos_harness.{h,cpp}` read directly and confirmed generic, the SHA-256 self-check digest independently recomputed and matched, and the combinatorial-total discrepancy independently resolved).
- To: MSX QA Agent
- Milestone ID: M24
- Type: QA sign-off
- Scope: Independently reproduce and assess the M24 ZEXDOC/ZEXALL full parity sweep (backlog C3) for release readiness. Do not rubber-stamp the developer's or coordinator's own claims — re-derive them. At minimum: (1) rebuild from a clean reconfigure and re-run the FULL suite (`ctest -LE m24_slow_full_sweep` for the fast 123, then the dedicated `ctest -R hbf1xv_m24_zexall_system_test` for the slow 124th — budget ~20-25 minutes wall-clock for the latter); (2) independently confirm `git diff v1.0.23` is empty for `src/devices/`, `src/peripherals/`, `src/core/`, and that `src/main.cpp`'s `--cpm-run` addition is purely additive; (3) read `src/machine/cpm_bdos_harness.{h,cpp}` directly and independently assess the license-isolation claim (zero zexall-specific knowledge); (4) independently review the adversarial self-check's reasoning in `docs/m24-implementation-report.md` §4.4 and confirm the real `references/zexall/zexall.com` is genuinely untouched (recompute the SHA-256 yourself); (5) independently re-derive at least a sample of the per-group combinatorial cycle counts from `references/zexall/zexall.z80` and reconcile against the reported 1,727,144 total (the coordinator's own from-scratch recount already matches the developer's figure exactly — confirm or refute independently); (6) weigh in on the `m24_slow_full_sweep` CTest-label disclosure as a permanent evidence-gate-cadence question; (7) make the explicit, disclosed judgment call flagged by both the developer and coordinator regarding DEC-0021 (the `disks/` MSX-DOS assets added mid-cycle): should M24's own disk-boot-A/B BLOCKED sub-claim be redone against the new assets before closure, or left BLOCKED-as-recorded with `disks/` reserved for a future dedicated C5 investigation — this is QA's call to make, not force-resolved by either upstream agent; (8) independently assess the coordinator-flagged nuance that `hbf1xv_m24_zexall_system_test`'s ctest-level gate does not itself hard-fail on a genuine per-group `error_markers > 0` (only on marker-count mismatch or unexpected BDOS calls) — judge whether this observe-and-report design is an acceptable regression-safety posture or should be tightened in a future cycle; (9) full 34-row deferred-backlog review; (10) produce `docs/m24-qa-signoff.md` with a PASS / Conditional PASS / FAIL recommendation.
- Acceptance Criteria: Full regression reproduced independently (124/124, 0 failed); the 134/134 group-verdict claim independently reproduced (not merely re-read); license-isolation claim independently assessed; adversarial self-check independently reviewed; DEC-0021 judgment call made explicitly; `docs/m24-qa-signoff.md` delivered with a clear recommendation. Per the standing M24-M25 continuation directive: on a clean QA PASS, the coordinator proceeds through the release-decision/tag step (v1.0.24) without an additional human pause; on anything short of a clean PASS, STOP and consult the human.
- Dependencies: `docs/m24-implementation-report.md`; `docs/m24-parity-trace-diff.md`; RESP-M24-002 (coordinator's independent verification); `agent-protocol/state/deferred-backlog.md` row C3; DEC-0021.
- Requested At: 2026-07-08T07:15:00+09:00

---

- Request ID: REQ-M24-004
- From: Human (project owner) via /msx-master directive, choosing "Fix, re-confirm, then tag" in response to the coordinator's stop-and-consult on M24's Conditional Pass (RESP-M24-003, `docs/m24-qa-signoff.md`).
- To: MSX Developer Agent
- Milestone ID: M24
- Type: Implementation (QA-mandated hardening fix, Required Fix #1 of `docs/m24-qa-signoff.md` §4)
- Scope: In `tests/system/hbf1xv_m24_zexall_system_test.cpp`, harden the pass/fail gate to hard-assert `zexall.error_markers == 0` and `zexdoc.error_markers == 0` (new `expect_true` calls, e.g. `Zexall_ZeroErrorMarkers`/`Zexdoc_ZeroErrorMarkers`), in addition to the existing `ok_markers + error_markers == 67` and `unexpected_bdos_calls == 0` checks — this converts the test from "observe-and-report" to a proper hard-gate consistent with every other zero-tolerance test in this codebase, per QA's Risk 1 finding. This is a small, mechanical, purely additive change to test code only — no production code (`src/`) is touched. Do NOT touch `src/machine/cpm_bdos_harness.*`, `src/main.cpp`, or any other already-QA-verified file. After the change: rebuild, run the fast subset (`ctest -LE m24_slow_full_sweep`) AND the slow full sweep (`ctest -R hbf1xv_m24_zexall_system_test`, ~20-25 min) to confirm both are green with the new assertions active.
- Acceptance Criteria: `tests/system/hbf1xv_m24_zexall_system_test.cpp` hard-asserts `error_markers == 0` for both suites; full regression (124/124) reproduced green after the change, including the slow system test with the new assertions genuinely exercised (not skipped); zero changes to any file outside `tests/system/hbf1xv_m24_zexall_system_test.cpp`; brief written confirmation of the diff and the fresh test run output.
- Dependencies: `docs/m24-qa-signoff.md` §4 Required Fix #1; the existing, already-independently-verified `tests/system/hbf1xv_m24_zexall_system_test.cpp` (developer/coordinator/QA all independently confirmed `error_markers=0` for both suites three times over this cycle already — this fix asserts something already proven true, not a speculative change).
- Requested At: 2026-07-08T08:00:00+09:00

---

- Request ID: REQ-M24-005
- From: MSX Master Agent (coordinator), acting on the human's explicit choice ("Fix, re-confirm, then tag") and the independently-reconfirmed hardening fix (RESP-M24-004) satisfying QA's Required Fix #1
- To: MSX Master Agent (coordinator) — self-directed release decision
- Milestone ID: M24
- Type: Release decision (milestone closure)
- Scope: Close M24 (ZEXDOC/ZEXALL Full Parity Sweep, backlog C3) on the now-clean PASS (QA's Conditional Pass, RESP-M24-003, plus the independently-reconfirmed Required Fix #1, RESP-M24-004). Tag v1.0.24. Commit `references/zexall/` (staged since this cycle, uncommitted since M19) plus all new M24 source/test/tool/doc files as part of the closure commit.
- Acceptance Criteria: M24 Done in `milestones.md` + `definition-of-done.yaml`; annotated tag v1.0.24; backlog C3 -> DONE (M24) in `deferred-backlog.md`; DEC-0022 logged; no new backlog rows required beyond what DEC-0021 already named for C5.
- Requested At: 2026-07-08T09:05:00+09:00

---

- Request ID: REQ-M25-001
- From: Human (project owner) via /msx-master directive "let's advance to all M25, milestone by milestone, fully developed, and fully qa" (2026-07-08) — M25 is the second and final milestone of this continuation.
- To: MSX Master Agent (coordinator)
- Milestone ID: M25
- Type: Milestone kickoff
- Scope: Open M25 = Sony speed-controller + hardware PAUSE + Ren-Sha Turbo autofire — deferred-backlog row **C8** (OPEN, HB-F1XV-specific, never previously scoped). Grounded directly against `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §9 and `references/fact-sheets/Zilog Z80A CPU.md` §6 (read this cycle, not assumed): a second Sony custom LSI (**MB670836**) sits beside the S1985 and handles DRAM address multiplexing plus the speed-controller (slow-motion) and hardware-PAUSE circuitry (S1985 fact-sheet §9). Critically, the HB-F1XV has **no CPU turbo mode** — the "Speed Controller" slider is explicitly **NOT a clock-speed change**; it is implemented as an autofire on the PAUSE button synced to BLANK (VBlank), slowing games by pausing them intermittently (Z80A fact-sheet §6, item 3). The hardware PAUSE **physically halts the CPU and cannot be bypassed in software** (S1985 fact-sheet §9) — this is a genuinely different mechanism from the Z80's own `HALT` instruction (already modeled, M9/M23/C2) and needs its own design: likely an external, machine-level halt/resume gate distinct from the CPU-internal halted state, triggered by a PAUSE key/button input and (for the speed-controller) an autofire timer synced to VBlank. Ren-Sha Turbo is a separate, simpler autofire feature (rapid-fire button-press synthesis, typically for joystick/keyboard fire buttons) — the planner must determine its real HB-F1XV-specific trigger/control mechanism from the fact sheets and, if available, `references/openmsx-21.0/` (behavior reference only, never copied into `src/`) rather than guessing. This is the SECOND and FINAL milestone of the M24-M25 continuation (2026-07-08 human directive) — own planner package, developer implementation, dedicated system integration test, QA sign-off, separate tag; proceeding through the release-decision/tag step without an extra pause on a clean QA PASS, UNLESS QA does not reach a clean PASS, in which case STOP and consult the human (this exact condition already fired once this run, for M24 — see DEC-0022).
- Acceptance Criteria: M25 milestone entry present in `milestones.md`; planner-first; a genuine, fact-sheet-grounded design for the PAUSE/speed-controller/Ren-Sha Turbo mechanisms (not guessed); implementation with deterministic unit/integration/system tests; real openMSX A/B evidence where feasible (openMSX's own HB-F1XV or comparable MSX2+ machine definition may or may not model this Sony-specific hardware — check, don't assume, and honestly report BLOCKED if it doesn't); zero regression across the FULL M1-M24 suite (124 tests, noting `hbf1xv_m24_zexall_system_test` takes ~24-27 minutes — budget accordingly for full-suite runs); full deferred-backlog review; QA sign-off before closure.
- Dependencies: M24 Done (v1.0.24, tag confirmed); `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §9; `references/fact-sheets/Zilog Z80A CPU.md` §6; the existing Z80A CPU core (`src/devices/cpu/z80a_cpu.*`) and PPI/keyboard-matrix peripherals (M15, for whatever the PAUSE button's own input path turns out to be); `agent-protocol/state/deferred-backlog.md` row C8.
- Requested At: 2026-07-08T09:15:00+09:00

---

- Request ID: REQ-M25-002
- From: MSX Master Agent (coordinator), approving `docs/m25-planner-package.md` (RESP-M25-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M25
- Type: Implementation (slices S1-S5)
- Scope: Implement `docs/m25-planner-package.md` end to end, exactly as specced. Read the full package before writing any code — §2.3/§2.4/§2.5 are the authoritative architecture (`Mb670836PauseController` at `src/devices/chipset/mb670836_pause.{h,cpp}`, gating `step_cpu_instruction()` via an early-return BEFORE any opcode decode; `RenshaTurbo` at `src/peripherals/rensha_turbo.{h,cpp}`, wired into `KeyboardMatrix`/`JoystickPorts` via additive OR-mask attach points), §3 is the exact 5-slice build order (S1 PauseController isolated → S2 RenshaTurbo isolated → S3 peripheral wiring+regression guards → S4 machine wiring, the highest-risk slice → S5 dedicated system test + A/B evidence + backlog/documentation closure), and §6 is the developer handoff with the exact file list and hard constraints.
- Hard constraints (§6, do not deviate without a `decisions.md` entry): do NOT touch `src/devices/cpu/*` (hardware PAUSE is machine-level only); do NOT model the PAUSE key as a `KeyboardMatrix` row/column cell (grounded reasoning in §1.2/§2.3: "cannot be bypassed in software" is incompatible with a software-maskable matrix key); do NOT add any CLI flag or SDL3/frontend binding this cycle (deferred to C9); do NOT mix `run_frame()`'s atomic scheduler tick with `step_cpu_instruction()`'s per-instruction tick inside the same test (R-M25-5 — call `pause_controller().on_vsync()` directly instead, mirroring the established A-M24-8 precedent); do NOT fabricate an A/B PASS for PAUSE/Speed-Controller — Acceptance Criterion 9 requires an honest BLOCKED disposition with the exact §2.2 citations reproduced in `docs/m25-parity-trace-diff.md`. Run the FULL M1-M24 suite (124 tests, including the ~24-27-minute `hbf1xv_m24_zexall_system_test`) at least once before requesting QA — run it directly/synchronously, do not nest a background nested-wait pattern for it (a prior M24 dispatch stalled doing exactly that; just run the command and wait for it to return within your own turn).
- After S4 (machine wiring, the highest-risk slice touching `step_cpu_instruction()`/`run_frame()`/`cold_boot()`): immediately run and report the `git diff v1.0.24` confirmation that every named M9/M12/M23 zero-tolerance CPU-timing-oracle test file is byte-for-byte unchanged, before proceeding to S5.
- Acceptance Criteria: all 12 acceptance criteria in `docs/m25-planner-package.md` §4 — genuine fact-sheet-grounded design; hardware PAUSE genuinely freezes PC/registers/R/memory (proven, not asserted); PAUSE cannot be bypassed via any CPU-visible API; Speed Controller's duty cycle is deterministic and hand-computable; Ren-Sha Turbo never forces a spurious press (R-M25-6, exhaustively tested); zero regression (124/124, independently reproduced); named CPU-timing-oracle files confirmed unchanged; default/unattached behavior byte-for-byte unchanged from pre-M25; the precise BLOCKED-for-PAUSE/PARITY-attempt-for-RenSha A/B disposition rule; dedicated system integration test combining all three features; full 34-row backlog review; all evidence gates executed and captured.
- Dependencies: `docs/m25-planner-package.md`; `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §9; `references/fact-sheets/Zilog Z80A CPU.md` §6; `src/machine/hbf1xv_machine.{h,cpp}` (existing `run_frame()`/`step_cpu_instruction()`/vsync architecture); `references/openmsx-21.0/src/{RenShaTurbo,Autofire,MSXPPI,MSXTurboRPause,SG1000Pause}.{hh,cc}` and `references/openmsx-21.0/src/sound/MSXPSG.cc` (behavior reference only, never copied into `src/`); `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` (RenShaTurbo calibration).
- Requested At: 2026-07-08T09:45:00+09:00

---

- Request ID: REQ-M25-003
- From: MSX Master Agent (coordinator), acting on RESP-M25-002's independent verification (developer implementation reproduced end-to-end: rebuild, fast `ctest` subset 129/129 exactly matching, `git diff v1.0.24` confirmed empty for all nine non-M25 device categories AND all 12 named zero-tolerance CPU-timing-oracle files, both new source files read directly and confirmed to exactly match the planner's design, the `hbf1xv_machine.cpp` diff read in full and confirmed purely additive with the PAUSE gate correctly positioned).
- To: MSX QA Agent
- Milestone ID: M25
- Type: QA sign-off
- Scope: Independently reproduce and assess M25 (Sony speed-controller + hardware PAUSE + Ren-Sha Turbo, backlog C8) for release readiness. Do not rubber-stamp the developer's or coordinator's own claims — re-derive them. At minimum: (1) rebuild from a clean reconfigure and re-run the FULL suite (130 tests total, budget ~24-27 minutes for the slow `hbf1xv_m24_zexall_system_test`); (2) independently confirm `git diff v1.0.24` is empty for `src/devices/{cpu,video,audio,rtc,fdc,cartridge,memory,halnote,kanji}/` and `src/core/`, AND independently re-run the exact 12-file zero-tolerance CPU-timing-oracle diff command (listed in `docs/m25-implementation-report.md` §4.1) — this is the single highest-sensitivity confirmation this milestone requires, given it touches `step_cpu_instruction()`/`run_frame()`; (3) read `src/devices/chipset/mb670836_pause.{h,cpp}` and `src/peripherals/rensha_turbo.{h,cpp}` directly and independently assess the design against the planner package's §2.4/§2.5 spec; (4) independently verify hardware PAUSE genuinely freezes PC/every register/R/memory across multiple paused `step_cpu_instruction()` calls AND cannot be bypassed via any CPU-visible API — read `hbf1xv_m25_pause_integration_test.cpp` and, if you judge it warranted, write and run your own additional probe; (5) independently verify Ren-Sha Turbo's R-M25-6 invariant (never forces a spurious press) is exhaustively tested, not just asserted; (6) **independently re-run the live openMSX A/B script yourself** (`tools/openmsx-m25-rensha-parity.ps1` against the real WSL `Sony_HB-F1XV` machine) — this was NOT independently re-run by the coordinator this cycle (left explicitly for QA, per RESP-M25-002), so this is the one piece of evidence in this milestone that has only been produced once so far; (7) independently assess whether the PAUSE/Speed-Controller BLOCKED A/B disposition is genuinely justified (re-run the same openMSX source searches the planner/developer performed, don't just trust the citations); (8) full 34-row deferred-backlog review; (9) produce `docs/m25-qa-signoff.md` with a PASS / Conditional Pass / FAIL recommendation. Note: this milestone's own continuation directive already had its Conditional-Pass stop condition exercised once this run (M24, DEC-0022) — the same standing rule applies here: on a clean QA PASS the coordinator proceeds through the release-decision/tag step without an additional human pause; on anything short of a clean PASS, STOP and consult the human.
- Acceptance Criteria: Full regression reproduced independently (130/130, 0 failed); the 12-file CPU-timing-oracle zero-diff claim independently reproduced; hardware PAUSE's freeze/non-bypassability claims independently assessed; Ren-Sha Turbo's R-M25-6 invariant independently assessed; the live openMSX A/B evidence independently reproduced (not merely re-read, since the coordinator did not do this); the PAUSE/Speed-Controller BLOCKED disposition independently judged sound; `docs/m25-qa-signoff.md` delivered with a clear recommendation.
- Dependencies: `docs/m25-implementation-report.md`; `docs/m25-parity-trace-diff.md`; RESP-M25-002 (coordinator's independent verification); `agent-protocol/state/deferred-backlog.md` row C8.
- Requested At: 2026-07-08T10:10:00+09:00

---

- Request ID: REQ-M25-004
- From: MSX Master Agent (coordinator), acting on QA PASS (REQ-M25-003/RESP-M25-003) and the human's standing pre-authorization for this M24-M25 run
- To: MSX Master Agent (coordinator) — self-directed release decision per the pre-authorization
- Milestone ID: M25
- Type: Release decision (milestone closure) — SECOND AND FINAL milestone of the M24-M25 continuation
- Scope: Close M25 (Sony Speed-Controller + Hardware PAUSE + Ren-Sha Turbo, backlog C8) on QA PASS (`docs/m25-qa-signoff.md`). Apply QA's recommended Finding-1 documentation fix. Tag v1.0.25.
- Acceptance Criteria: M25 Done in `milestones.md` + `definition-of-done.yaml` (`overall_done: true`); annotated tag v1.0.25; backlog C8 -> DONE (M25) in `deferred-backlog.md`; DEC-0023 logged; Finding 1 fixed; this closes the full M24-M25 continuation.
- Requested At: 2026-07-08T10:50:00+09:00

---

- Request ID: REQ-M26-001
- From: Human (project owner) via /msx-master directive "let's do SDL3 in M26 first. You can plan debug features as you go or define M27 as PRODUCTION and do thorough testing and its needed tools and features there. your call." (2026-07-08) — following up on an earlier, explicitly-deferred request (2026-07-08, after the M21-M23 run): "I would also want to have test artifacts generated in debug/ folders (such as generated frames in PNGs, etc) before we wire SDL3 in M26."
- To: MSX Master Agent (coordinator)
- Milestone ID: M26
- Type: Milestone kickoff
- Scope: Open M26 = SDL3 frontend — deferred-backlog row **C9** ("SDL3 frontend (video/audio/input presentation) — in baseline scope, not yet built"). Build the real-time SDL3 application layer: a window, a main loop driving the existing `Hbf1xvMachine::run_frame()`/`step_cpu_instruction()` core at real (or throttled) wall-clock pacing, video presentation of the existing M21 `VdpFrameRenderer`/`FrameBuffer` RGB555 pixel output, audio output from the existing PSG/YM2413 devices, and keyboard/joystick input mapped into the existing M15 `KeyboardMatrix`/`JoystickPorts` APIs (including, per M25, the new PAUSE-button/Speed-Controller-slider/Ren-Sha-Turbo-slider API surfaces that were built with exactly this future binding in mind). Ground the design in `references/sdl3/` (API reference only, never copied into `src/`) — read the actual SDL3 headers/init/event/audio APIs, don't assume.
  **Coordinator's scope-boundary decision on the debug/-tooling request** (the human explicitly delegated this call): M26 MUST include a minimal, new frame-capture capability — dumping the *decoded* `FrameBuffer` (RGB555 pixels, from M21, NOT raw VRAM bytes) to a real color PNG under `debug/frames/`. This is a genuine testability necessity, not scope creep: neither an agent nor a human can verify "does SDL3 render the correct picture" from ctest output alone, and the existing `tools/mem-to-png.py` (M10-S5) only visualizes raw memory bytes as grayscale noise, not decoded pixels — it cannot answer this question. Without SOME real color-image capture point, this milestone's QA cycle would have no way to produce genuine visual evidence, only "it compiled and didn't crash." This is the ONLY debug/testing capability in scope for M26 — do not exceed it.
  **Everything else from the human's original debug/-tooling request is explicitly OUT of M26, deferred to a new M27 ("Production Hardening + Debug/Test Tooling") milestone**: audio capture to file (`tools/mem-to-audio.py` integration or equivalent), full CPU/memory/VRAM state-dump CLI wiring (the existing `write_state_dump()`/`write_cpu_trace()`/`write_event_log()` API from M10-S3 has never been wired to any CLI flag — it is only exercised by two tests today, both redirected to a temp directory), keystroke-sequencing/scripted-input automation (does not exist anywhere in this project today — `KeyboardMatrix::set_key()` is a raw, single-call API with no scripting/macro layer), and broader production-readiness testing (performance/timing validation under real wall-clock constraints, stress testing, cross-platform build validation, replay-determinism validation via the debug event log).
  A genuine, novel testing-architecture question for the planner to resolve (not force-solved here): SDL3 needs a real window/audio device to initialize, which is unlikely to be available in this project's automated ctest/agent execution environment. The planner must determine how M26's own deterministic tests avoid requiring a real display/audio device — SDL3 supports a "dummy"/headless video driver (`SDL_VIDEODRIVER=dummy`) and a "disk" or null audio driver for exactly this kind of automated testing; read the actual `references/sdl3/` source to confirm the real API/env-var names and mechanics, don't assume from general SDL knowledge. Real, human-observed interactive verification (actually launching the built exe with a real window) is a separate, disclosed, non-ctest-gated check — the planner should name it explicitly as a distinct evidence category from the deterministic automated suite.
- Acceptance Criteria: M26 milestone entry present in `milestones.md`; planner-first; a genuine SDL3 frontend that actually builds and runs (both `-DSONY_MSX_ENABLE_SDL3=ON` build flow, per `CLAUDE.md`); deterministic unit/integration/system tests that run headlessly in `ctest` (no real display/audio device required); the new decoded-FrameBuffer-to-PNG capture capability, with at least one real captured PNG committed as evidence under `debug/frames/`; a full deferred-backlog review; QA sign-off before closure. M27's scope is explicitly named as a forward-looking note in this milestone's planner package, not designed in detail here.
- Dependencies: M25 Done (v1.0.25, tag confirmed); `references/sdl3/` (API reference, read directly, never copied into `src/`); the existing `VdpFrameRenderer`/`FrameBuffer` (M21), PSG/YM2413 audio devices, `KeyboardMatrix`/`JoystickPorts` (M15) + the M25 PAUSE/Speed-Controller/Ren-Sha-Turbo API surfaces (built specifically to be ready for this binding); `src/machine/debug_dump.{h,cpp}` (M10-S3, for grounding the new PNG-capture capability's output-path conventions); `agent-protocol/state/deferred-backlog.md` row C9.
- Requested At: 2026-07-08T11:30:00+09:00

---

- Request ID: REQ-M26-002
- From: MSX Master Agent (coordinator), approving `docs/m26-planner-package.md` (RESP-M26-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M26
- Type: Implementation (slices S1-S8)
- Scope: Implement `docs/m26-planner-package.md` end to end, exactly as specced — this is the largest, most architecturally novel milestone to date (first frontend code), so read the FULL package before writing any code, not just the slice list. §2.3 is the authoritative real-time-loop architecture (the new `Hbf1xvMachine::on_vsync_boundary()` seam — a PURE, MECHANICAL extraction of `run_frame()`'s existing body, the ONLY change to `src/machine/`; the `Sdl3App::run_one_frame()` deterministic-core vs. `run_interactive()` real-time-wrapper split). §2.4 is the headless-testability mechanism (SDL3's real "dummy" video/audio drivers, env vars `SDL_VIDEO_DRIVER`/`SDL_AUDIO_DRIVER` — note the underscore, SDL3 renamed these from SDL2's `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER`; `SDL_PushEvent` for real synthetic input injection). §2.5 is the ONE authorized new debug capability (decoded-`FrameBuffer`→PNG, NOT raw VRAM bytes — `tools/mem-to-png.py` is explicitly insufficient for this, build a NEW `tools/frame-to-png.py`). §2.6 is the audio design (PSG-only real synthesis wired from `src/frontend/` only, never core; YM2413 honestly disclosed as silent — this is a hard, non-negotiable constraint, not a suggestion). §2.7 is input mapping (SDL3 physical scancodes → `KeyboardMatrix`/`JoystickPorts`/PAUSE/Speed-Controller/Ren-Sha-Turbo, all first-principles PC-keybinding choices, disclosed as such). §2.8 is CLI/asset wiring, including the coordinator-approved `--disk` flag (A-M26-6, R-M26-7).
- Follow the exact 8-slice build order from §3/§6: S1 (`on_vsync_boundary()` — do this FIRST, run the FULL M9/M12/M23 CPU-timing-oracle `git diff` check immediately after, before proceeding, mirroring the M25-S4 precedent for the highest-blast-radius slice) → S2 (app skeleton + first SDL3-ON `ctest` proof — do this EARLY so any real environment blocker, e.g. SDL3 not being discoverable via `find_package`, is found before the rest of the plan is built on top of it) → S3 (video) → S4 (PNG capture, headless-buildable, may proceed in parallel with S3) → S5 (audio) → S6 (input) → S7 (CLI/disk) → S8 (system test + evidence + backlog closure).
- Hard constraints (§6, do not deviate without a `decisions.md` entry): do NOT touch `src/devices/cpu/*`; do NOT call `run_frame()` and `step_cpu_instruction()`/`on_vsync_boundary()` in the same session or test — the double-count hazard already burned this project once at the test level (M25's R-M25-5); do NOT add ANY waveform/DSP synthesis for YM2413 — disclose silence honestly instead; do NOT use `SDL_Delay`/real-time pacing anywhere inside `ctest`'s own execution path — every SDL3-gated test drives `run_one_frame()` directly, a bounded number of times; do NOT transcribe SDL3 API signatures from memory/SDL2 experience — read the actual vendored `references/sdl3/include/SDL3/*.h` header for every new call site; do NOT add a keybinding/remapping configuration system, multi-window support, or any other named §1.2 out-of-scope item; do NOT fabricate an A/B PASS for anything this milestone cannot actually cross-check.
- Run the FULL M1-M25 headless suite (130 tests, including the ~24-27-minute `hbf1xv_m24_zexall_system_test` at least once before requesting QA) AND the full SDL3-ON suite — run these directly/synchronously, do NOT nest a background wait inside your own turn (a prior M24 dispatch stalled doing exactly that).
- If SDL3 is genuinely not discoverable/installable in this execution environment (a real possibility per A-M26-1/R-M26-1 — no prior milestone has actually attempted an SDL3-ON build), report this honestly as a real blocker at S2, do NOT attempt to fabricate or route around it. This is exactly the kind of finding the standing "STOP and consult the human" discipline exists for.
- Acceptance Criteria: all 14 acceptance criteria in `docs/m26-planner-package.md` §4 — genuine working SDL3 app; video presentation proven pixel-exact, not merely "compiles"; audio genuinely real for PSG, honestly silent for YM2413; input mapping exhaustively verified via real `SDL_PushEvent` injection; the PNG capture capability real and evidenced with a genuine committed example under `debug/frames/`; zero regression on the headless configuration (130/130); the SDL3-ON `ctest` suite itself fully headless (dummy drivers, no real hardware); the `ctest`-gated vs. human-observed-only evidence split explicitly honored; all 12 named CPU-timing-oracle files confirmed unchanged; honest BLOCKED/N-A A/B disposition; dedicated system test; full 34-row backlog review; all evidence gates captured for BOTH build configurations; M27 named only as a forward-looking note, not designed.
- Dependencies: `docs/m26-planner-package.md`; RESP-M26-001 (coordinator's independent verification); `references/sdl3/`; the existing M21/M15/M25 APIs named in §2.2's seam table.
- Requested At: 2026-07-08T12:05:00+09:00

---

- Request ID: REQ-M26-003
- From: MSX Master Agent (coordinator), acting on RESP-M26-002's independent verification (developer implementation reproduced end-to-end: BOTH build configurations rebuilt from scratch, fast subsets 133/133 + 139/139, the slow ZEXALL/ZEXDOC sweep independently re-run to completion with byte-identical figures, `git diff` confirmed empty for devices/peripherals/core AND the 12-file CPU-timing-oracle list AND the YM2413 files specifically, a real manual `sony_msx_sdl3.exe` session run with a real cartridge loaded, the committed PNG evidence independently viewed and confirmed genuine).
- To: MSX QA Agent
- Milestone ID: M26
- Type: QA sign-off
- Scope: Independently reproduce and assess M26 (SDL3 Frontend, backlog C9) for release readiness — this is the largest, most architecturally novel milestone to date (first frontend/presentation-layer code, first real-time loop, first real audio wiring), so budget accordingly. Do not rubber-stamp the developer's or coordinator's own claims — re-derive them. Read `docs/m26-planner-package.md`, `docs/m26-implementation-report.md`, and `docs/m26-frontend-evidence.md` in full first. At minimum:
  1. **Reproduce the real environment finding yourself**: starting from a state where SDL3 is NOT assumed pre-installed, confirm `references/sdl3/` genuinely builds and installs locally (the documented `cmake -S references/sdl3 -B <dir> -DCMAKE_INSTALL_PREFIX=<install-dir> ...` sequence in the implementation report §2), and that `-DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH=<install-dir>` then configures/builds `sony_msx_sdl3.exe` cleanly. (If SDL3 is already built/installed in this environment from the coordinator's own verification pass, confirm the SAME install works, and independently re-verify the documented build sequence is what actually produced it, don't just trust it's there.)
  2. **Rebuild and test BOTH configurations from clean**: headless (`-DSONY_MSX_ENABLE_SDL3=OFF`, the full 134-test suite including the ~30-35-minute `hbf1xv_m24_zexall_system_test` — budget generously) AND SDL3-ON (140 tests, entirely under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`, verify this yourself by setting these env vars externally, not just trusting the test's own internal `setenv` calls).
  3. Independently confirm `git diff v1.0.25` is empty for `src/devices/`, `src/peripherals/`, `src/core/`, the 12 named zero-tolerance CPU-timing-oracle files, AND specifically `src/devices/audio/ym2413_opll.{h,cpp}` (R-M26-5's hard constraint — also `grep -rn "ym2413\|opll" src/frontend/` and confirm any hit is disclosure-comment-only, never synthesis logic).
  4. Read `src/machine/hbf1xv_machine.cpp`'s `on_vsync_boundary()`/`run_frame()` pair directly and independently confirm the extraction is genuinely, purely mechanical (same operations, same order) — this is the single highest-blast-radius edit in the milestone.
  5. Independently read and assess `src/frontend/sdl3_video_presenter.{h,cpp}` (the zero-conversion blit claim) and its pixel-readback test; `src/frontend/sdl3_audio_presenter.{h,cpp}` + `src/frontend/psg_audio_pump.{h,cpp}` (the real PSG wiring, honest YM2413 silence) and its numeric-oracle test; `src/frontend/sdl3_input_mapper.{h,cpp}` (the 71-entry scancode table) and its exhaustive `SDL_PushEvent`-injection test.
  6. **Run the real, built `sony_msx_sdl3.exe` yourself** under the dummy drivers (e.g. `--hidden-window --max-frames N --bios-dir bios`, optionally with a real cartridge via `--cart1 roms/metalgear.rom --cart1-type Konami` or `--cart1 roms/aleste.rom`) — do not just read the developer's/coordinator's own transcript of having done this; produce your own real invocation and real output.
  7. Independently view the committed `debug/frames/m26-example-boot.png` directly and confirm it is a genuine, correctly-decoded, multi-color image, not a placeholder; run `python tools/frame-to-png.py --self-check` yourself.
  8. Independently assess whether `docs/m26-frontend-evidence.md`'s honest, mostly-BLOCKED/N-A A/B disposition and its `ctest`-gated vs. human-observed-only evidence split are reasoned soundly — do NOT attempt to fabricate evidence for the human-observed-only category (a real window/audio/keyboard on real hardware) yourself; confirm the report correctly declines to claim it.
  9. Full 34-row deferred-backlog review.
  10. Produce `docs/m26-qa-signoff.md` with a PASS / Conditional Pass / FAIL recommendation. Per the standing continuation directive: on a clean QA PASS, the coordinator proceeds through the release-decision/tag step without an additional human pause; on anything short of a clean PASS (this milestone's own planner package explicitly named this as the bar, given the novel architectural territory), STOP and consult the human.
- Acceptance Criteria: Full regression reproduced independently for BOTH configurations; the real SDL3-not-preinstalled/build-from-vendored-source finding independently reproduced; the 12-file CPU-timing-oracle diff and the YM2413-silence constraint independently reproduced; the `on_vsync_boundary()` extraction independently read and confirmed mechanical; the real `sony_msx_sdl3.exe` independently run by QA itself; the committed PNG independently viewed; the honest A/B/human-observed disposition independently judged sound; `docs/m26-qa-signoff.md` delivered with a clear recommendation.
- Dependencies: `docs/m26-implementation-report.md`; `docs/m26-frontend-evidence.md`; RESP-M26-002 (coordinator's independent verification); `agent-protocol/state/deferred-backlog.md` row C9.
- Requested At: 2026-07-08T15:25:00+09:00

---

- Request ID: REQ-M26-004
- From: MSX Master Agent (coordinator), acting on QA PASS (REQ-M26-003/RESP-M26-003) — zero findings of any severity
- To: MSX Master Agent (coordinator) — self-directed release decision
- Milestone ID: M26
- Type: Release decision (milestone closure)
- Scope: Close M26 (SDL3 Frontend, backlog C9) on QA PASS (`docs/m26-qa-signoff.md`). Tag v1.0.26. Commit all new `src/frontend/`, `src/machine/frame_dump.*`, `tools/frame-to-png.py`, the committed PNG evidence, and all new test files.
- Acceptance Criteria: M26 Done in `milestones.md` + `definition-of-done.yaml` (`overall_done: true`); annotated tag v1.0.26; backlog C9 -> DONE (M26) in `deferred-backlog.md`; DEC-0024 logged; M27 named as the next indicative milestone.
- Requested At: 2026-07-08T16:15:00+09:00

---

- Request ID: REQ-M27-001
- From: Human (project owner) via /msx-master directive "Let's go into M27 'Production Hardening + Tooling'. Let's not do slow test until it's really necessary." (2026-07-08).
- To: MSX Master Agent (coordinator)
- Milestone ID: M27
- Type: Milestone kickoff
- Scope: Open M27 = "Production Hardening + Debug/Test Tooling" — everything explicitly deferred out of M26's scope-boundary decision (`agent-protocol/state/current-phase.md`, DEC-0024), grounded directly in `CLAUDE.md`'s own baseline text: *"Debug capabilities (full state dump - cpu states, dram, sram, vram content, etc) and logs of the execution events in `debug/traces/` and `debug/logs`... Generate all needed tools in python and powershell in `tools/` to develop, test and debug (including, without limitation, memory to content to png or audio convert, etc)."* This is not new baseline scope — it is already-committed scope that has simply never been wired up. Four named work items, each independently groundable:
  1. **Full CPU/memory/VRAM state-dump CLI wiring.** `Hbf1xvMachine::write_state_dump()`/`write_cpu_trace()`/`write_event_log()` (M10-S3) are real, already-implemented, already-tested APIs with zero CLI flag anywhere in either executable today. Wire them into both `sony_msx_headless` and `sony_msx_sdl3` (e.g. `--dump-state <name>`, `--trace-cpu <name> <max_steps>`, `--event-log <name>`), reusing the existing `debug_root_`-relative convention M26's `frame_dump` already established as the pattern to follow.
  2. **Real, decoded audio capture to file.** Checked this cycle, not assumed: `tools/mem-to-audio.py` (M10-S5) is the SAME class of tool as the pre-M26 `tools/mem-to-png.py` — it treats raw memory bytes as PCM samples verbatim and explicitly, by its own doc comment, "does NOT synthesize PSG (AY-3-8910) or YM2413/OPLL audio and models no sound chip at all." M26 wired REAL PSG sample generation (`PsgAudioPump`/`PsgYm2149::advance_cycles()`/`sample()`) for the first time in this project's history, but only for live SDL3 audio-stream playback — nothing captures those real, decoded samples to a file yet. This milestone should mirror M26's exact `frame_dump`→`frame-to-png.py` precedent: a new, deterministic C++ sample-dump capability (headless-buildable, no SDL3 dependency, reusing `PsgAudioPump`) + a new Python WAV-writer tool (NOT reusing `mem-to-audio.py`, which is genuinely insufficient for this — mirrors the `mem-to-png.py`-is-insufficient finding from M26 exactly), with at least one real, committed example WAV under `debug/sounds/`.
  3. **Keystroke-sequencing/scripted-input automation.** Does not exist anywhere in this project today — `KeyboardMatrix::set_key()` is a raw, single-call API with no macro/scripting layer over it, confirmed absent in both the headless and SDL3 CLIs. Design and build a scripted-input mechanism (e.g. a simple timed key-event script format + a CLI flag to drive it against either executable) — this is directly useful for the still-open backlog **C5** (full-boot/auto-disk-boot-trigger investigation) and for any future automated interaction testing, though this milestone does not need to attempt the C5 investigation itself.
  4. **Debug event-log CLI wiring + a genuine replay-determinism proof.** Wire `set_event_logging_enabled()`/`write_event_log()` to a CLI flag; then use it for a concrete, testable "production readiness" claim — two independent, identical runs (same asset root, same cartridge/disk, same instruction count) produce byte-for-byte identical event logs, proving this project's own core determinism claim end-to-end via the debug tooling itself, not just asserted in prose.
  Explicitly, deliberately OUT of scope this cycle unless the planner finds a small, well-grounded, low-risk item worth folding in: vague/open-ended "stress testing" or "cross-platform build validation" with no concrete, testable claim attached — if the planner identifies something concrete and worth doing here, name it explicitly with a real acceptance criterion; do not add speculative scope with no test attached.
  **Standing evidence-gate guidance for this milestone (the human's explicit instruction this cycle, now also recorded as a persistent project memory, `feedback_slow_test_cadence.md`)**: do NOT run the slow `hbf1xv_m24_zexall_system_test` (~25-35 min) at every gate by default. Use the fast subset (`ctest -LE m24_slow_full_sweep`) as the default evidence gate throughout this milestone. Only run the slow test if a slice's own `git diff` touches `src/devices/cpu/`, `src/core/`, or otherwise risks CPU-timing/scheduling behavior — none of the four items above are expected to (state-dump/audio-capture/keystroke-scripting/event-log wiring are all machine-introspection or frontend/peripheral-adjacent, not CPU-core changes), but this must be CONFIRMED via `git diff v1.0.26 --name-only -- src/devices/ src/peripherals/ src/core/` at each gate, not assumed. If that diff is genuinely empty, the fast subset is sufficient and the slow test does not need to run this cycle at all.
- Acceptance Criteria: M27 milestone entry present in `milestones.md`; planner-first; a genuine, working implementation of items 1-4 above (or an honestly-scoped subset with any deferral explicitly justified); deterministic unit/integration/system tests; zero regression across the full M1-M26 suite (using the fast-subset-by-default evidence-gate discipline above, confirmed via `git diff` that the slow test is genuinely not needed each time it's skipped); full deferred-backlog review; QA sign-off before closure.
- Dependencies: M26 Done (v1.0.26, tag confirmed); `src/machine/debug_dump.{h,cpp}`, `debug_event_log.{h,cpp}` (M10-S3, existing dump/log API); `src/machine/frame_dump.{h,cpp}` + `tools/frame-to-png.py` (M26, the established dump-then-convert pattern to mirror); `src/frontend/psg_audio_pump.{h,cpp}` (M26, the real PSG sample-generation wiring to reuse for audio capture); `tools/mem-to-audio.py` (confirmed insufficient, do not extend it — build fresh, mirroring the `mem-to-png.py` precedent); `agent-protocol/state/deferred-backlog.md`.
- Requested At: 2026-07-08T17:00:00+09:00

---

- Request ID: REQ-M27-002
- From: MSX Master Agent (coordinator), approving `docs/m27-planner-package.md` (RESP-M27-001) for implementation dispatch
- To: MSX Developer Agent
- Milestone ID: M27
- Type: Implementation (slices S1-S8)
- Scope: Implement `docs/m27-planner-package.md` end to end, exactly as specced. Read the full package before writing any code. §2.2 is the new headless `--debug-session` mode (state-dump/CPU-trace/event-log/scripted-input CLI wiring, plus a new `--disk` flag) — a wholly additive new `main.cpp` branch, mirroring the existing `--bios-boot-trace`/`--frame-dump-demo` pattern exactly, never touching the pre-existing default run path. §2.3 is the headless PSG audio-dump capability (`src/frontend/psg_audio_dump.{h,cpp}`, genuinely reusing M26's `PsgAudioPump`) + a NEW `tools/audio-dump-to-wav.py` (do not extend `tools/mem-to-audio.py`'s own responsibilities). §2.4 is the scripted-input mechanism (`src/peripherals/msx_key_names.{h,cpp}` + `src/machine/input_script.{h,cpp}`) — critically, every table entry in `msx_key_names` MUST be derived by directly reading `sdl3_input_mapper.cpp`'s actual array literal, never independently re-derived, since a hard cross-consistency test (S7) will catch any divergence. §2.5 is the event-log CLI wiring (shares §2.2) plus the adversarially-validated replay-determinism system test — two independent machine instances must produce byte-identical event logs, and a deliberately-diverged third must produce a DIFFERENT one (the negative control proving the check genuinely discriminates, mirroring the M24 ZEXALL corruption self-check precedent).
- Follow the exact 8-slice build order from §3/§6: S1 (`--debug-session` skeleton) → S2 (state-dump/CPU-trace) → S3 (event-log + the replay-determinism proof — the item-4 headline claim) → S4 (SDL3-side wiring for items 1/4 — run the FULL M26 SDL3-ON regression suite immediately after, before proceeding) → S5 (audio-dump, may run in parallel with S2-S4) → S6 (key-name table + script parser/player) → S7 (scripted-input CLI wiring + the two cross-consistency/cross-executable-equivalence tests) → S8 (dedicated system test + evidence + backlog closure).
- **Standing evidence-gate cadence for this milestone (the human's explicit directive, `feedback_slow_test_cadence.md`) — read §6 of the planner package in full for the precise interpretation.** Default gate throughout: `ctest --test-dir build -C Debug --output-on-failure -LE m24_slow_full_sweep`. Before every gate where the slow test is skipped, run and record `git diff v1.0.26 --name-only -- src/devices/ src/peripherals/ src/core/` — per R-M27-6's precise interpretation, a listed path is fine if it is a genuinely NEW, additive file this milestone adds (expected — `msx_key_names.*`, `psg_audio_dump.*`, `mb670836_pause.cpp`-adjacent new chipset files are NOT expected here, but new peripherals/frontend files ARE); the hard, zero-tolerance check is that NO pre-existing file under those paths shows as modified, and `src/devices/cpu/`/`src/core/` show ZERO changes of any kind (new or modified). Document the actual `git diff` output at each such gate, not just a pass/fail conclusion. A full, unfiltered `ctest` run (including the slow test) at the final closure gate is a coordinator/QA judgment call at that time, not a hard per-slice requirement, given the git-diff confirmation holds throughout.
- Hard constraints (§6, do not deviate without a `decisions.md` entry): do NOT touch `src/devices/cpu/*` or `src/core/*`; do NOT edit any PRE-EXISTING file under `src/devices/` or `src/peripherals/` (every touch there must be a wholly new, additive file); do NOT add any waveform/DSP synthesis for YM2413 anywhere; do NOT attempt the C5 auto-boot-trigger investigation this cycle; do NOT use `SDL_Delay`/real-time pacing anywhere inside `ctest`'s own execution path; do NOT extend `tools/mem-to-audio.py`'s own responsibilities (a pure, zero-behavior-change `encode_wav()`-extraction refactor into a shared module is the only acceptable touch, and only if you judge it worth the coupling).
- Acceptance Criteria: all 15 acceptance criteria in `docs/m27-planner-package.md` §4 — items 1-4 genuinely, working, wired on both executables where required (item 2 is headless-only per the dispatch's own scoping); zero new machine-level method needed for item 1; the audio-dump capability genuinely reuses `PsgAudioPump`, with a real, committed, non-silent example WAV; the scripted-input mechanism's cross-consistency and cross-executable-equivalence claims independently tested, not merely asserted; the replay-determinism proof adversarially validated; zero regression on the headless configuration (134 prior + all new M27 tests) with the precise `git diff` verification named above; the SDL3-ON suite fully headless; every pre-existing M26 SDL3 test remains green unmodified; the dedicated system test; full 34-row backlog review (zero rows change status, C5 gets a factual note only); all evidence gates captured for both build configurations.
- Dependencies: `docs/m27-planner-package.md`; RESP-M27-001 (coordinator's independent verification); the existing M10-S3/M26 APIs named in §2.1's seam table.
- Requested At: 2026-07-08T17:45:00+09:00

---

- Request ID: REQ-M27-003
- From: MSX Master Agent (coordinator), acting on RESP-M27-002's independent verification (developer implementation reproduced end-to-end: both build configurations rebuilt, fast subsets 140/140 + 149/149 exactly matching, the cadence-guidance `git diff` re-run independently, the `write_register()` private-visibility correction confirmed, the committed WAV independently decoded and its exact toggle count reproduced, the key-name cross-consistency spot-checked, the negative control read directly in the test file, and the two-process replay-determinism launch independently reproduced from scratch).
- To: MSX QA Agent
- Milestone ID: M27
- Type: QA sign-off
- Scope: Independently reproduce and assess M27 ("Production Hardening + Debug/Test Tooling") for release readiness. Do not rubber-stamp the developer's or coordinator's own claims — re-derive them. **Standing evidence-gate cadence for this milestone applies to you too** (the human's `feedback_slow_test_cadence.md` directive): default to the fast subset (`ctest -LE m24_slow_full_sweep`) for both build configurations; before concluding the slow test is unnecessary, independently run `git diff v1.0.26 --name-only -- src/devices/ src/peripherals/ src/core/` and `git diff v1.0.26 --name-only -- src/devices/cpu/ src/core/` yourself and confirm the same result the developer/coordinator found (only the new `src/peripherals/msx_key_names.{h,cpp}` files, zero pre-existing file modified, zero cpu/core touch of any kind) — if you independently confirm this, you do NOT need to run the slow test either; only run it if your own diff shows something different from what's claimed. At minimum: (1) rebuild both configurations from clean and reproduce the fast-subset counts (140/140 headless, 149/149 SDL3-ON); (2) independently confirm the `write_register()` private-visibility finding by reading `psg_ym2149.h` directly; (3) read `tests/system/hbf1xv_m27_replay_determinism_system_test.cpp` in full and independently judge whether the flat-RAM-driver-program design correction (documented in `docs/m27-implementation-report.md` §3) is sound — this is the milestone's headline claim; (4) independently launch the real `sony_msx_headless.exe --debug-session ...` yourself, at least twice, and confirm byte-identical event logs from your own two process invocations — do not just trust the report's transcript; (5) independently decode `debug/sounds/m27-example-tone.wav` yourself (not just check its file type) and confirm it is genuinely non-silent/oscillating, not a placeholder; (6) independently verify the key-name cross-consistency test's completeness (all 71 entries, not a sample) by reading `sdl3_input_mapper_key_names_consistency_integration_test.cpp` directly; (7) independently confirm zero YM2413 file touched and zero waveform/DSP-shaped code added anywhere; (8) full 34-row deferred-backlog review (zero rows should change status; C5 gets a factual note only); (9) produce `docs/m27-qa-signoff.md` with a PASS / Conditional Pass / FAIL recommendation.
- Acceptance Criteria: Full regression reproduced independently for both configurations; the cadence-guidance git-diff independently re-run and confirmed (or the slow test genuinely run if it isn't); the replay-determinism proof's design correction independently judged sound by direct test-file read; the real executables independently launched by QA itself; the committed WAV independently decoded; the cross-consistency test's completeness independently confirmed; `docs/m27-qa-signoff.md` delivered with a clear recommendation.
- Dependencies: `docs/m27-implementation-report.md`; RESP-M27-002 (coordinator's independent verification); `agent-protocol/state/deferred-backlog.md`.
- Requested At: 2026-07-08T19:20:00+09:00

---

- Request ID: REQ-M27-004
- From: MSX Master Agent (coordinator), acting on RESP-M27-003's clean, unconditional QA PASS (`docs/m27-qa-signoff.md`) and the standing continuation cadence (no human pause required on a clean PASS).
- To: (self-directed — coordinator release decision, no further subagent dispatch)
- Milestone ID: M27
- Type: Release decision
- Scope: Close M27 ("Production Hardening + Debug/Test Tooling") and tag `v1.0.27`. Per DEC-0025: update `agent-protocol/state/milestones.md` (M27 status Done, overall_done true, QA summary appended), `agent-protocol/state/current-phase.md` (M27 closed, awaiting next human directive), `agent-protocol/state/definition-of-done.yaml` (M27 regression_qa.blocker_risks_resolved_or_accepted / signoff_decision_recorded / overall_done all true), stage all new/modified M27 files, create the closure commit, and create annotated tag `v1.0.27`.
- Acceptance Criteria: All ledger/state files reflect M27 Done with QA's clean PASS summarized; working tree staged and committed; annotated tag `v1.0.27` created on the closure commit.
- Dependencies: `docs/m27-qa-signoff.md`; RESP-M27-003; DEC-0025.
- Requested At: 2026-07-08T20:05:00+09:00

---

- Request ID: REQ-M28-001
- From: MSX Master Agent (coordinator), acting on the human's directive (2026-07-08): "it's time to deliver all the backlogged items to complete the project. Let's roll it into M28 and call it as a release-candidate. We can skip the slow-sweep unless it's really needed. We can review the source code and see 1) all the components are in good shape, integration without a flow, features are in, and all the backlogs are clearly implemented in."
- To: MSX Planner Agent
- Milestone ID: M28
- Type: Planning package
- Scope: Two combined objectives for "Release Candidate — Backlog Closure Sweep + Full-Project Health Audit":

  (A) BACKLOG CLOSURE. Consult `agent-protocol/state/deferred-backlog.md` in full (34 rows). The following 11 rows are currently OPEN or IN-PROGRESS (everything else is already DONE):
  - C1 (IN-PROGRESS, M23 partial) / D4 (IN-PROGRESS, M23 partial) — twin references to the SAME remaining work: cycle-accurate VDP access-slot/command timing (full per-mode/per-scanline slot tables, raster-position reconciliation, CPU-access-steals-command-engine-slot interaction, command engine's real per-pixel/per-line VDP-cycle cost, exact sub-frame IRQ raster position, actual CPU-execution gating).
  - C5 (IN-PROGRESS, M16 partial) — full-boot / automatic disk-ROM boot-handshake trigger investigation. Now equipped with real `disks/msxdos22.dsk`/`msxdos23.dsk`/`msxdos24/` assets (DEC-0021) and M27's `--disk` flag + deterministic scripted-input mechanism (`src/machine/input_script.*`, `src/peripherals/msx_key_names.*`).
  - C10 (OPEN) — FDC flux-level/DMK disk fidelity (raw flux, weak bits, copy-protection).
  - E1 (OPEN) — YM2413 FM-synthesis DSP/audio depth (18-slot TDM pipeline, log-sin/exp operators, EG, LFOs, rhythm noise, DAC nonlinearity). M17 delivered the register/channel/rhythm contract only.
  - E2 (OPEN) — YM2413 register-write timing constraint (minimum address/data write spacing).
  - F1 (OPEN) — cassette tape image-format/signal fidelity (real .CAS/.WAV/.TSX decode/encode, FSK/UDF modulation). M18 delivered the CPU-visible register contract + a synthetic bitstream only.
  - F2 (OPEN) — printer image/ESC-sequence rendering depth (dot-matrix font rendering, ESC interpretation, page→PNG). M18 delivered the CPU-visible port contract + raw-byte capture only.
  - G1 (OPEN) — KonamiSCC mapper + embedded SCC/SCC+ 5-channel wavetable sound chip (a genuinely new audio device, not an incremental mapper).
  - G2 (OPEN) — ROM-database/SHA1 auto-mappertype-detection + heuristic byte-pattern detection.
  - G3 (OPEN) — full `CartridgeSlotManager`-style dynamic runtime hot-plug (live insert/eject while running).
  - G4 (OPEN) — long tail of ~80 other specialized/vendor-specific cartridge mapper types, explicitly scoped in its own row as "only if a specific real cartridge requiring one of these is actually wanted."

  **Hard constraint — standing license-sensitive-scope directive (`feedback_license_sensitive_scope.md`, user-confirmed 2026-07-08: "there should be zero license-sensitive future work"):** never reproduce openMSX's own large numeric tables/arrays under `src/`, even framed as "independently re-derived." This directly and explicitly gates C1/D4's remaining depth — the ~340-entry V9958 access-slot timing tables have no known independent, non-GPL source (Yamaha datasheet, real hardware measurement, or a `references/fact-sheets/` entry) as of M23's own planning. If the planner cannot identify a genuine independent source for this exact numeric data, C1/D4's remaining depth MUST be ruled honestly UNBUILDABLE-WITHOUT-FABRICATION and left OPEN/carried-forward with that reasoning recorded — NOT attempted via openMSX table transcription no matter how the citation is worded, and NOT force-closed. Apply the same scrutiny to any other row that would require reading exact numeric/timing data out of `references/openmsx-21.0/` rather than an independent source (e.g., check E1/E2's `YM2413NukeYKT.{hh,cc}` citation, C10's `DMKDiskImage.*` citation, and F1's cassette-format citations for the same risk before committing to IN).

  Per the M15 precedent (`docs/m15-planner-package.md`, "the planner MUST consult the ENTIRE backlog, recommend each item IN/DEFERRED with justification, and PROPOSE a deterministic decomposition — 'all pending items' is too large for one deterministic milestone"): the planner MUST NOT assume all 11 rows fit in M28. Recommend each row IN (for M28) or DEFERRED (to a named follow-on milestone or left OPEN indefinitely, with justification) — several rows are individually milestone-sized on their own merits regardless of the license question (E1 is a real-time DSP audio engine; G1 is a new wavetable sound chip; F1 is a real audio-codec integration; C5 is an open-ended disassembly investigation). Propose a deterministic M28 scope (and any necessary named follow-on milestones, e.g. M29/M30) rather than forcing everything into one cycle.

  (B) RELEASE-CANDIDATE HEALTH AUDIT. Independent of which backlog rows make the IN cut, define a concrete audit deliverable covering: (1) component-by-component source-code health across every `src/` subsystem (core/devices/peripherals/machine/frontend) — no dangling stubs, no dead/orphaned code, no unwired seams; (2) integration-flow coherence — machine composition, bus/slot wiring, CLI (headless + SDL3), asset loading, confirmed end-to-end, not just unit-level; (3) feature-completeness cross-check against the Target Machine Specification in `CLAUDE.md` (every named component: CPU, RAM, VRAM, ROM set, built-in media, text/graphic modes, colors, sound, I/O ports); (4) a full, current backlog-accuracy audit (every one of the 34 rows' status/citations still accurate). Deliverable: a durable artifact under `docs/` (planner to name it, e.g. `docs/m28-release-candidate-audit.md`) plus whatever concrete fixes/closures fall out of it.

  Standing evidence-gate cadence continues (`feedback_slow_test_cadence.md`): default to the fast subset (`ctest -LE m24_slow_full_sweep`); only run the slow `hbf1xv_m24_zexall_system_test` if `git diff <baseline-tag> --name-only -- src/devices/ src/peripherals/ src/core/` shows a genuine touch to an EXISTING file, or any touch at all to `src/devices/cpu/`/`src/core/`.

- Acceptance Criteria: Full 34-row backlog consultation with an explicit IN/DEFERRED recommendation and justification for every currently-open row; C1/D4's license-sensitive-scope risk explicitly assessed and ruled on (not silently skipped); a deterministic M28 scope proposed (with named follow-on milestones for anything DEFERRED, if applicable); a concrete release-candidate health-audit plan with named deliverable(s); dependency sequencing and acceptance criteria for every IN item; risks and assumptions explicitly captured.
- Dependencies: `agent-protocol/state/deferred-backlog.md` (full 34-row table); `CLAUDE.md` (Target Machine Specification, non-negotiables); `feedback_license_sensitive_scope.md`; `feedback_slow_test_cadence.md`; `docs/m15-planner-package.md` (decomposition precedent); `docs/m23-planner-package.md` §2.1 (why C1/D4 was scoped down originally).
- Requested At: 2026-07-08T20:25:00+09:00

---

- Request ID: REQ-M28-002
- From: MSX Master Agent (coordinator), acting on RESP-M28-001 (planner package APPROVED, `docs/m28-planner-package.md` — coordinator independently re-verified the highest-stakes citations, corrected one self-report bookkeeping error, no changes to the IN/DEFERRED decomposition).
- To: MSX Developer Agent
- Milestone ID: M28
- Type: Implementation (slices S1-S4)
- Scope: Implement `docs/m28-planner-package.md` §3.1 end to end, exactly as scoped. Read the full package before writing any code. Three work items ONLY:
  - **M28-S1 (E2 — YM2413 register-write timing constraint, §2.1b):** extend the existing `src/devices/audio/ym2413_opll.{h,cpp}` (shipped M17) with a last-write-cycle tracker for the `#7C` address-latch and `#7D` data ports, consulted read-only against `elapsed_cycles()` (mirrors the M15 X4 pattern — do NOT touch the CPU-timing formula in `step_cpu_instruction`). Ground the 12/84-master-cycle constants in `references/fact-sheets/Yamaha YM2413 FM Chip.md` §8. **Mandatory pre-check (R-M28-1): before enabling any default-on gate, `rg` the existing YM2413 test files for back-to-back `OUT (#7C),A`/`OUT (#7D),A` sequences with insufficient spacing; if found, either add disclosed spacer instructions to those specific tests (test-only edit) or default the gate OFF with an explicit, unit-tested toggle (matching openMSX's own disabled-by-default stance).** A/B disposition is honestly N/A for the drop-behavior comparison (openMSX disables this itself) — the gate MECHANISM is unit-tested against the cited constants instead.
  - **M28-S2 (C5 — full-boot/auto-disk-boot-trigger investigation, §2.1a):** use the M10 CPU trace-export facility plus M27's `--debug-session --dump-state --trace-cpu --disk --input-script` tooling and the real `disks/msxdos22.dsk`/`msxdos23.dsk`/`msxdos24/` assets to drive long, deterministic boots (with and without scripted keypresses) and compare against a real openMSX boot to the same disk image via the established live-Tcl PC/register readback technique, to find the FIRST point of divergence. **Dual-outcome acceptance (R-M28-2): outcome (a) full close — a real auto-boot sequence is observed and A/B-verified, C5 → DONE (M28); outcome (b) — a concrete, evidenced finding is produced and recorded, C5 stays IN-PROGRESS (M28 partial). Either is acceptable. Do NOT fabricate a "reached DOS prompt" claim without a genuine, reproduced trace.** Any production change only if a genuine defect is found (pure observation requires no production edit).
  - **M28-S3 (release-candidate health audit, §2.4):** produce `docs/m28-release-candidate-audit.md` covering all four parts — (1) component-by-component `src/` source-code health (every file exercised by a test, every device/peripheral actually wired in `hbf1xv_machine.cpp`, no untracked TODO/STUB markers; resolve the seeded finding A-M28-5, `src/devices/device_placeholder.h`/`src/peripherals/peripheral_placeholder.h`, per R-M28-7's removal pre-check); (2) integration-flow coherence confirmed END-TO-END via actual executable launches (not just `ctest`) — both `sony_msx_headless` and `sony_msx_sdl3` (if SDL3 is configured), covering `--bios-dir`/`--disk`/`--cart1`/`--cart2`/`--debug-session`/`--input-script`; (3) a Target Machine Specification feature-completeness cross-check table (per `CLAUDE.md`); (4) a full 34-row backlog-accuracy audit — run LAST, after S1/S2/S4 land, so it reflects the milestone's final state. Fix-scope rule (§2.4): only small, safe, mechanically-verifiable fixes without a separate decision entry; anything behavior-changing on an already-QA-signed device is a new backlog/decision candidate, not an ad hoc fix.
  - **M28-S4 (backlog/ledger closure):** update `agent-protocol/state/deferred-backlog.md` — E2 → DONE (M28); C5 → per S2's honest outcome; C1/D4's UNBUILDABLE-WITHOUT-FABRICATION reasoning recorded verbatim (§2.2); G1/E1/C10/F1/F2 → DEFERRED with their named M29-M33 owners and each row's specific scoping caveat from §2.3 recorded verbatim (not summarized); G2/G3/G4 re-affirmed indefinite, unchanged.
- **Hard constraints (do not deviate without a `decisions.md` entry):** do NOT implement any part of C1/D4's remaining VDP-timing depth (no slot-table transcription in any form, "independently re-derived" framing included); do NOT begin E1/C10/F1/F2/G1 production code this cycle (each has an unmet precondition per §2.3); perform the E2 regression pre-check BEFORE enabling any default-on gate; C5's acceptance is dual-outcome — do not force a "closed" narrative; run the FULL, unfiltered `ctest` (not `-LE m24_slow_full_sweep`) at least once this cycle per A-M28-3/criterion 8 — mechanically required since E2/C5 touch existing `src/devices/` files, regardless of the kickoff's "skip unless needed" framing.
- Acceptance Criteria: all 10 acceptance criteria in `docs/m28-planner-package.md` §4 — E2 implemented with the mandatory pre-check performed first; C5 investigated with either honest outcome accepted; the health audit produced with cited command output (not restated planning text); zero regression across the full M1-M27 suite; the full unfiltered `ctest` run at least once with both ZEXALL/ZEXDOC `error_markers == 0` gates still passing; every touched backlog row status-updated in the same cycle; all evidence gates captured.
- Dependencies: `docs/m28-planner-package.md`; RESP-M28-001 (coordinator's independent verification); the existing M10/M15/M17/M27 APIs named in §2.1a/§2.1b's seam descriptions.
- Requested At: 2026-07-08T21:15:00+09:00

---

- Request ID: REQ-M28-003
- From: MSX Master Agent (coordinator), acting on independently-verified developer implementation (REQ-M28-002/§ above) AND on DEC-0026 (a separately-landed, coordinator-authored bug fix that landed in this same working tree between M28's developer-implementation-complete state and this QA dispatch).
- To: MSX QA Agent
- Milestone ID: M28
- Type: QA sign-off (combined: M28's approved scope + DEC-0026's bug fix)
- Scope: Independently reproduce and assess for release readiness TWO things landing together in this cycle's closure commit:

  **(A) M28 itself** ("Release Candidate — Backlog Closure Sweep + Full-Project Health Audit"), per `docs/m28-planner-package.md`: E2 (YM2413 register-write timing constraint, `src/devices/audio/ym2413_opll.{h,cpp}`), C5 (full-boot/auto-disk-boot-trigger investigation, honest dual-outcome — reached outcome (b), `docs/m28-parity-trace-diff.md`), and the release-candidate health audit (`docs/m28-release-candidate-audit.md`, four parts). Full details in `docs/m28-implementation-report.md`.

  **(B) DEC-0026** — a genuine bug fix discovered via direct human interactive use (SDL3 boot showed a permanent black screen), NOT part of M28's own approved scope, landed in the SAME working tree/commit: `V9958Vdp`'s status register S#2 bits VR/HR were hardcoded to 0 (a disclosed M23 simplification), causing the real BIOS's boot-time VDP-init wait-loop to hang forever. Fixed via a new, optional `VdpClockSource` (X-pattern of the project's existing clock adapters), computing VR/HR live from real elapsed raster time, grounded in `references/fact-sheets/Yamaha V9958 VDP.md` §7 and reusing M23's own `vdp_access_timing::vdp_cycle_within_line()` foundation — zero license-sensitive data involved, C1/D4's own actual remainder (the ~340-entry slot tables) is completely untouched. Full details, including a second, deeper finding intentionally left OPEN (an exact, precisely-located memory-mapper segment-content divergence found via a real openMSX A/B instruction trace, NOT fixed this cycle per explicit human direction), in `docs/vdp-vr-hr-boot-hang-fix-report.md`. Full decision record in `agent-protocol/channels/decisions.md` DEC-0026.

  Do not rubber-stamp either the developer's or the coordinator's own claims — re-derive them independently. **Standing evidence-gate cadence applies** (`feedback_slow_test_cadence.md`): the full, unfiltered `ctest` (including the slow ZEXALL/ZEXDOC sweep) is MANDATORY this cycle regardless of the usual fast-subset default — it was already run to completion twice this cycle (once for M28's own E2/C5 changes, once again after DEC-0026's fix), both times 100% green with `error_markers=0` for both ZEXALL/ZEXDOC suites; QA should independently reproduce this at least once from a clean rebuild.

  At minimum: (1) rebuild from clean and reproduce the full, unfiltered `ctest` run (146/146 expected, including the slow sweep with both ZEXALL/ZEXDOC `error_markers==0`); (2) independently verify E2's regression pre-check conclusion (re-run the `rg` sweep against the existing M17 YM2413 test files, confirm they remain byte-for-byte unmodified via `git diff v1.0.27`); (3) independently re-run `hbf1xv_m28_c5_disk_boot_investigation_system_test` and `tools/openmsx-m28-c5-boot-parity.ps1`, confirm the same honest outcome-(b) finding; (4) independently re-run the health audit's own concrete command checks (re-launch both executables, re-run the `rg`/path-existence sweeps); (5) for DEC-0026: independently reproduce the black-screen-to-blue-screen fix (build headless or SDL3-ON, run a real BIOS boot for ~300 frames via a debug session or equivalent, confirm the VDP configures non-zero registers where it previously stayed all-zero forever) — a real, direct reproduction, not merely trusting the coordinator's own report; (6) independently run the two new VR/HR tests AND independently verify the M24 CP/M-isolation test's masked-bits fix is justified (read the test diff, confirm the masking is narrowly scoped to bits 0x60 on S#2 only, confirm every other byte in that snapshot remains an exact, unmasked comparison); (7) confirm zero touch to `src/devices/cpu/`/`src/core/` across BOTH (A) and (B) via `git diff v1.0.27`; (8) full 34-row deferred-backlog review (E2→DONE, C5→IN-PROGRESS with new finding, C1/D4's ruling unchanged but both rows carry DEC-0026's factual cross-reference note, G1/E1/C10/F1/F2→DEFERRED with named owners, G2/G3/G4 unchanged); (9) produce `docs/m28-qa-signoff.md` with a PASS / Conditional Pass / FAIL recommendation covering BOTH (A) and (B) together, since they land in the same closure commit.
- Acceptance Criteria: Full regression independently reproduced from a clean rebuild, including the mandatory slow sweep; E2/C5/health-audit claims independently re-verified per M28's own original acceptance criteria (`docs/m28-planner-package.md` §4); DEC-0026's fix independently reproduced via a real boot (not merely trusted); the M24 test-fix's scope independently judged sound; zero unauthorized touch to CPU/core files across both changesets; full backlog review; `docs/m28-qa-signoff.md` delivered with a clear recommendation covering both (A) and (B).
- Dependencies: `docs/m28-implementation-report.md`; `docs/m28-release-candidate-audit.md`; `docs/m28-parity-trace-diff.md`; `docs/vdp-vr-hr-boot-hang-fix-report.md`; DEC-0026; `agent-protocol/state/deferred-backlog.md`.
- Requested At: 2026-07-08T16:35:00+09:00

---

- Request ID: REQ-M28-004
- From: MSX Master Agent (coordinator), acting on the human's "Fix, re-confirm, then tag" directive (in response to M28's Conditional Pass, `docs/m28-qa-signoff.md`/RESP-M28-003) and QA's own explicit recommendation to fold DEC-0026 into the same closure.
- To: (self-directed — coordinator release decision, no further subagent dispatch)
- Milestone ID: M28
- Type: Release decision
- Scope: Apply QA's two required fixes (§9 of `docs/m28-qa-signoff.md`: refresh/caveat the stale C5 trace figures in `docs/m28-parity-trace-diff.md` and `deferred-backlog.md`'s C5 row; reference DEC-0026 and the updated 146/146 count in `current-phase.md`/`milestones.md`/`definition-of-done.yaml`/the backlog trailer). Close M28 ("Release Candidate — Backlog Closure Sweep + Full-Project Health Audit") and tag `v1.0.28`, folding DEC-0026 (the VDP S#2 VR/HR boot-hang fix) into the same closure commit/tag per DEC-0027.
- Acceptance Criteria: Both QA-required fixes applied (documentation/ledger-only, no code change); all ledger/state files reflect M28 Done + DEC-0026 folded in; working tree staged and committed; annotated tag `v1.0.28` created on the closure commit.
- Dependencies: `docs/m28-qa-signoff.md`; RESP-M28-003; DEC-0027; DEC-0026.
- Requested At: 2026-07-08T17:20:00+09:00

---

- Request ID: REQ-M29-001 / RESP-M29-001 (combined kickoff + planner outcome record, autonomous run per DEC-0035)
- From/To: MSX Master Agent (coordinator) ⇄ MSX Planner Agent
- Milestone ID: M29 (backlog G1 — KonamiSCC mapper + SCC wavetable chip; tag target v1.0.30)
- Outcome: Planner delivered BOTH deliverables. (1) `references/fact-sheets/Konami SCC.md` (346 lines) — grounded entirely in independent community measurements (De Schrijder 2003 amplitude/mixing formula incl. the AmpOut=640+Σ / AND-#7FF0-div-16 truncation and [+40..+1235] range; Pazos 2003 deformation-register semantics + the 3.58MHz/(freq+1) rotation fact; NYYRIKKI 2005 volume-latch/freq-restart/period<9-stop; enen power-on state), mapper facts from RomKonamiSCC (banks 5000/7000/9000/B000 in 0x800 windows, ==0x3F enable, SCC window 9800-9FFF), SCC-I facts recorded for the named remainder. SEVEN openMSX-vs-fMSX disagreements recorded and arbitrated per DEC-0030 (headline: fMSX divides by period with no +1 and stops only at period 0 — both arbitrated to openMSX on independent corroboration), five double-grounded agreements noted, license hygiene confirmed (no numeric table exists to transcribe — SCC waveforms are runtime-written). (2) `docs/m29-planner-package.md` (410 lines) — six slices S1-S6; SCC-I/SCC+ scoped OUT as proposed new backlog row G5 (a different cartridge device; purely-additive later); audio mixing via a new SDL3-independent `frontend::MachineAudioMixer` with `AudioPacer`/`PsgAudioPump` byte-for-byte untouched and a hard zero-SCC byte-identity regression oracle; full 34-row backlog re-affirmation (E1's M30→M31 renumber correctly deferred to M31 kickoff per DEC-0035's own instruction); A/B plan with the mapper-forcing-syntax verification precondition and audio-sample A/B honestly declared N/A up front.
- Coordinator verification: independently re-read the highest-stakes citations — `SCC.cc:1-32` (De Schrijder formula verbatim), `SCC.cc:34-70` (Pazos 3.58MHz/(freq+1)), `RomKonamiSCC.cc` (banks 5000/7000/9000/B000; `0x9800 <= address < 0xA000` window confirming the mirrored-window arbitration), and `fmsx-60/source/EMULib/SCC.c:113` (`SCC_BASE/J` no-+1 + zero-only stop — both recorded disagreements are real and correctly arbitrated). Package structure sound; the MachineAudioMixer design correctly isolates DEC-0033's pacer. APPROVED; proceeding to developer dispatch (REQ-M29-002) without human pause per DEC-0035. Coordinator note passed to the run: a real SCC game ROM (Nemesis 2 / Space Manbow class) would upgrade M29's smoke evidence — relayed to the human as optional/non-gating. Also of note for S6: `roms/aleste.rom` is now IDENTIFIED (coordinator, via exact softwaredb SHA1 match during the M30 pre-probe) as an Aleste 2 KonamiSCC-labeled multi-mapper repro dump that ALREADY BOOTS under ASCII16 — once the KonamiSCC mapper exists it should boot under it as well, a free cross-validation fixture.
- Requested At: 2026-07-09
