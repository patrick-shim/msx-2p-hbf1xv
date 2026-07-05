# tests/ — deterministic test conventions

Applies when editing anything under `tests/`. See root [`../CLAUDE.md`](../CLAUDE.md) for the
build/test flow and evidence gates.

## Test levels

- `tests/unit/` — small deterministic tests for one class or one behavior.
- `tests/integration/` — cross-device and cross-subsystem deterministic tests.
- `tests/system/` — machine-level boot, ROM workflow, and regression scenarios.

## Required file naming

- Unit: `tests/unit/<area>/<subject>_unit_test.cpp`
- Integration: `tests/integration/<area>/<subject>_integration_test.cpp`
- System: `tests/system/<subject>_system_test.cpp`

## Suite / case naming

- Suite: `<Domain>_<Component>_<Level>` — e.g. `Devices_VDP_Unit`, `Core_Scheduler_Integration`.
- Case: `<Behavior>_<Condition>_<ExpectedResult>` — e.g. `RegisterWrite_IllegalValue_Clamped`,
  `MultiDeviceTick_SameInputs_ReproducibleState`.

## Determinism requirements

- Every new integration or system test defines a **deterministic oracle** (what must be
  identical across runs).
- No wall-clock timing or external non-deterministic inputs.
- If randomness is required, seed it explicitly and assert reproducible outcomes.
- Prefer one behavior per unit test file when feasible.
