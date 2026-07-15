---
description: Initialize a new Sony HB-F1XV protocol run — populate current phase, the next milestone, and the first cross-agent handoff entries with goals, scope, build flow, and guardrails.
argument-hint: run name, primary objective, constraints, target milestone date
allowed-tools: Read, Edit, Write, Glob
---

Initialize a new project run in the protocol artifacts.

Run parameters: $ARGUMENTS

## Read first

- `CLAUDE.md`
- `agent-protocol/project-baseline.md`
- `agent-protocol/guardrails.md`
- `agent-protocol/README.md`

## Steps

1. Read the references above.
2. Update `agent-protocol/state/current-phase.md` with:
   - Objective aligned to the baseline goals and user input.
   - Active Phase = Planning; entry/exit criteria for Planning.
   - Open blockers initialized from user constraints.
   - Updated-at timestamp in ISO-8601.
3. Append the next milestone to `agent-protocol/state/milestones.md` using
   `agent-protocol/templates/milestone-template.md`, with concrete acceptance criteria tied to
   the build/test flow and deterministic behavior, and Status = Planned.
4. Append the first handoff to `agent-protocol/channels/requests.md`:
   - From: MSX Master Agent (coordinator) → To: MSX Planner Agent
   - Milestone ID, scope, acceptance criteria, dependencies, ISO-8601 requested-at.
5. Append a pending tracking entry to `agent-protocol/channels/responses.md` for that request
   (Status: Partial; Summary: awaiting planner response).
6. If assets are in scope, include evidence-gate artifacts in acceptance criteria:
   `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`.
7. If behavior parity is in scope, include `tools/openmsx-ab-smoke.ps1` and
   `docs/openmsx-ab-smoke.md` as required A/B evidence. openMSX A/B is the parity harness, but
   hardware behavior/timing decisions follow the STRICT reference-precedence rule in `CLAUDE.md` —
   real hardware spec (fact-sheets or web-researched primary hardware docs) > openMSX > fMSX, the
   spec winning on a confirmed conflict.
8. If any information is missing, make minimal assumptions and list them.

## Output format

1. Initialized Files
2. Assumptions
3. Request ID Created
4. Next Delegation
