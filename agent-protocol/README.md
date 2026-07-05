# Agent Protocol (runtime coordination substrate)

This directory holds the **live state and history** that the Claude Code multi-agent
workflow reads and writes while delivering the Sony HB-F1XV emulator. The *rules* that
govern the agents live in the always-loaded project memory ([`../CLAUDE.md`](../CLAUDE.md));
this directory holds the *data* those rules operate on.

## How it maps to Claude Code

- **Coordinator (human-facing):** the main Claude Code session, driven by the
  [`/msx-master`](../.claude/commands/msx-master.md) and
  [`/msx-autopilot`](../.claude/commands/msx-autopilot.md) commands.
- **Specialists (subagents):** [`msx-planner`](../.claude/agents/msx-planner.md),
  [`msx-developer`](../.claude/agents/msx-developer.md),
  [`msx-qa`](../.claude/agents/msx-qa.md), coordinated with the read-only
  [`msx-orchestration`](../.claude/agents/msx-orchestration.md) protocol gate.
- **Auto-runnable loop:** the deterministic
  [`.claude/workflows/msx-milestone.js`](../.claude/workflows/msx-milestone.js) workflow
  runs planner → developer → QA per milestone and loops until sign-off.

## Canonical references

- [`project-baseline.md`](project-baseline.md) — authoritative goals, scope, and build flow.
- [`guardrails.md`](guardrails.md) — role boundaries, anti-hallucination, and evidence policy.

Both are mirrored in condensed form inside [`../CLAUDE.md`](../CLAUDE.md); the files here are
the full canonical text that handoffs cite as dependencies.

## Files

- `channels/requests.md` — append-only handoffs requested by one agent of another.
- `channels/responses.md` — append-only completed handoffs, evidence, and blockers.
- `channels/decisions.md` — approved decisions and scope changes.
- `state/current-phase.md` — single source of truth for the active phase and status.
- `state/milestones.md` — milestone checklist and progress.
- `state/definition-of-done.yaml` — machine-readable DoD gates and phase-transition rules.
- `templates/handoff-template.md` — standard handoff schema.
- `templates/milestone-template.md` — standard milestone schema.
- `templates/regression-report-template.md` — standard regression report schema.

## Workflow phases

1. Intake → 2. Planning → 3. Implementation → 4. Integration → 5. Regression QA → 6. Release Decision.

A phase advances only when entry/exit criteria are met and recorded in the `state/` files.

## Conventions

- Lowercase kebab-case for all filenames.
- Channels are append-only; timestamp every entry in ISO-8601.
- Every handoff records source, target, milestone, scope, acceptance criteria, and dependencies.
- Every completed handoff records evidence and unresolved risks.
