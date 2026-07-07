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
