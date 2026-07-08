# Agent Guardrails

All agents must follow these rules on every task.

## Role Boundaries (Hard Rules)

- Master: human interaction and delegation to Orchestration only.
- Orchestration: phase control, protocol validation, and delegation to specialist subagents.
- Planner: specs, milestones, dependencies, acceptance criteria only.
- Developer: implementation plus unit/integration execution only.
- QA: regression analysis and sign-off recommendations only.

No agent may perform another role's primary responsibilities without an explicit approved decision in channels/decisions.md.
Master must not directly invoke Planner, Developer, or QA.

## Anti-Hallucination Controls

- Never claim a file, test, build result, or command output that was not read or executed.
- Never infer API/device behavior as fact without repository evidence or an explicit assumption label.
- Never claim BIOS/ROM assets exist without verifying concrete paths under `bios/` or `roms/`.
- Never claim A/B parity with openMSX without concrete evidence artifacts.
- Label uncertain statements with `Assumption:` and include a verification action.
- Prefer quoting exact artifact paths when referencing decisions and evidence.
- Prefer `references/` (openMSX 21.0 source, fMSX 6.0 source, SDL3 source, `fact-sheets/`) over
  memory when grounding device/timing/API behavior; cite the concrete `references/...` file path.
- When the two behavior references (openMSX, fMSX) disagree, record the disagreement explicitly
  and prefer the fact-sheet/real-hardware-corroborated interpretation — never silently pick one.
- Never assert what a reference source says without having read that concrete file.

## Evidence Requirements

- Every non-trivial claim must map to one of:
  - protocol artifact entry,
  - repository file content,
  - command/test output.
- Developer and QA outputs must include concrete test evidence (pass/fail and scope).
- Planner outputs must include dependency and risk evidence.
- Script-assisted steps must include the script path used (for example under `tools/`).

## Asset and Script Safety

- Treat `bios/` and `roms/` as local development assets; do not assert redistribution rights.
- Do not fabricate BIOS/ROM file names, checksums, or provenance.
- Prefer existing helper scripts in `tools/` when available; avoid duplicating script logic inline.
- Treat `references/` as read-only grounding material. Never copy `references/openmsx-21.0/`,
  `references/fmsx-60/`, or `references/sdl3/` source code into `src/` — they are GPL /
  non-commercial-freeware / third-party respectively and must stay isolated; reference their
  behavior/API only. fMSX's large data tables fall under the same zero-license-sensitive-work
  discipline as openMSX's (no transcription, however framed).
- For checksum claims, prefer `tools/checksum-assets.ps1` and store output in `docs/asset-checksums.txt`.
- For openMSX A/B smoke checks, prefer `tools/openmsx-ab-smoke.ps1` and store output in `docs/openmsx-ab-smoke.md`.

## Scope Control

- Work only the currently approved milestone unless a scope-change decision exists.
- Reject broad rewrites that do not map to milestone acceptance criteria.
- Keep updates incremental: smallest meaningful step first.

## Phase Transition Rules

- A phase transition is valid only when required artifacts are complete.
- Orchestration must explicitly confirm entry/exit criteria before transition.
- Master reports unresolved blockers to human before requesting transition.

## Failure and Blocker Handling

- When blocked, write blocker details and next required input in channels/responses.md.
- Do not fabricate workaround results.
- Escalate unresolved uncertainty to Master for user clarification.
