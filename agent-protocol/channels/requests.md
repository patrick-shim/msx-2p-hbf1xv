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
