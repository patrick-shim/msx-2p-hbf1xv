---
description: Generate deterministic device and subsystem test matrices for Sony HB-F1XV work — use when defining unit/integration/system test plans before or during implementation.
argument-hint: target devices/subsystems, acceptance goals, constraints, current coverage
allowed-tools: Read, Grep, Glob, Task
---

Generate a deterministic test matrix for Sony HB-F1XV development. Prefer delegating this to the
`msx-planner` subagent so the output lands as a durable planning artifact.

Target scope: $ARGUMENTS

Treat the arguments as authoritative for scope, acceptance goals, constraints, and current
coverage. If any are missing, infer reasonable defaults and list the assumptions.

## Requirements

Produce two matrices:

1. **Device matrix** — CPU/Z80A, VDP (V9958), PSG, YM2413, RTC, FDC, I/O, mapper, slot logic.
2. **Subsystem matrix** — core scheduler, buses, peripherals, machine composition, frontend.

For each row include: Test ID · Behavior under test · Preconditions/fixtures · Stimulus ·
Deterministic oracle (what must be identical across runs) · Priority (P0/P1/P2) ·
Recommended level (unit/integration/system) · Regression tags.

Also:

- Include edge cases and failure modes.
- Include asset-dependent cases with explicit `bios/`, `roms/`, and `disks/` paths when applicable.
- Include script-assisted execution notes with explicit `tools/` script paths.
- Include openMSX-on-WSL A/B rows when parity is relevant, referencing `tools/openmsx-ab-smoke.ps1`.
- Add a **Top Gaps** section (highest-risk missing tests).
- Add an **Execution Order** section (the next 10 tests to implement).

Follow the naming rules in `tests/CLAUDE.md`. Use markdown tables; keep entries concrete and
implementation-ready.
