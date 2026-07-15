---
description: Run the Sony HB-F1XV protocol in continuous autopilot loops (planner → developer → QA) until the target milestone closes with QA sign-off or a real blocker needs human input.
argument-hint: target=M<N> completion="<stop condition>" [constraints]
allowed-tools: Read, Grep, Glob, Task, TodoWrite, Edit
---

Run protocol autopilot from the current state as the coordinator, cycling
planner → developer → QA through the specialist subagents.

Directive: $ARGUMENTS

## Read before the first delegation

- `CLAUDE.md`
- `agent-protocol/project-baseline.md`
- `agent-protocol/guardrails.md`
- `agent-protocol/state/current-phase.md` and `state/milestones.md`
- Latest `agent-protocol/channels/requests.md` and `channels/responses.md`

## Autopilot loop

1. Inspect the latest phase, open blockers, and newest pending request.
2. Validate the gate via the `msx-orchestration` subagent, then delegate only through
   `msx-planner`, `msx-developer`, `msx-qa`.
3. Enforce ordering: planner-first → developer evidence → QA sign-off → release decision.
4. Keep channels append-only; timestamp entries in ISO-8601; refresh `state/` each cycle.
5. Run evidence gates when applicable and never fabricate output:
   - `tools/validate-assets.ps1`
   - `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
   - `cmake --build build --config Debug`
   - `ctest --test-dir build -C Debug --output-on-failure`
   - Conditional parity: `tools/openmsx-ab-smoke.ps1` → `docs/openmsx-ab-smoke.md`
6. Continue looping without asking for confirmation unless a real blocker requires user input.

Reference precedence: openMSX A/B is the parity harness, but for any hardware behavior/timing
dispute apply the STRICT order in `CLAUDE.md` — real hardware spec (fact-sheets or web-researched
primary hardware docs) > openMSX > fMSX, the spec winning on a confirmed conflict (divergence
documented, not silently followed).

## Stop conditions (default)

- Target milestone status becomes `Done`, or
- A protocol blocker needs a human decision, or
- A required tool/action cannot be performed in this environment.

To run hands-off on an interval or self-paced, wrap this command with the `/loop` skill
(e.g. `/loop /msx-autopilot target=M10 completion="M10 done with QA signoff"`).

## Output per cycle

1. Current Objective
2. Active Phase
3. Latest Subagent Outcomes
4. Risks and Blockers
5. Next Delegations
6. Completion Status
