# M1 Planner Package (REQ-M1-002)

- Milestone ID: M1
- Source Request: REQ-M1-002
- Prepared By: MSX Planner Agent
- Prepared At: 2026-07-05T22:36:09.1287078+09:00
- Status: Completed

## First-Slice Scope

First behavior-affecting slice: deterministic machine execution controls.

Target changes:

1. `src/machine/hbf1xv_machine.h`
2. `src/machine/hbf1xv_machine.cpp`
3. `tests/integration/machine/hbf1xv_machine_integration_test.cpp`

Behavior goals:

- Add deterministic custom cycle stepping API.
- Track frame progression separately from raw cycle ticks.
- Keep cold-boot reset deterministic across both cycle and frame counters.

## Boundaries And Dependencies

- `src/core/`: scheduler remains authoritative cycle accumulator.
- `src/machine/`: owns frame-level behavior and public machine stepping API.
- `tests/integration/`: validates cross-call deterministic machine behavior.

Dependency sequence:

1. Planner package completion (`REQ-M1-002`).
2. Developer readiness/implementation execution (`REQ-M1-003` + `REQ-M1-004`).
3. QA regression-readiness and signoff (`REQ-M1-005`).

## Deterministic Test Obligations

- Integration suite `Machine_Hbf1xv_Integration` must verify:
  1. frame stepping deterministic cycle totals,
  2. custom cycle stepping deterministic totals,
  3. cold-boot reset determinism for cycle and frame counters.

## Evidence Gates

Required execution and evidence for this milestone path:

1. `tools/validate-assets.ps1`
2. `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
3. `cmake --build build --config Debug`
4. `ctest --test-dir build -C Debug --output-on-failure`
5. Conditional parity workflow (when applicable):
   - `tools/openmsx-ab-smoke.ps1`
   - `docs/openmsx-ab-smoke.md`

## Risks And Verification

1. Risk: API expansion introduces inconsistent state updates.
   - Verification: integration assertions for frame and cycle state transitions.
2. Risk: Reset semantics regress existing behavior.
   - Verification: cold-boot deterministic oracle added in integration test.
3. Risk: Behavior-parity overreach for non-parity scope.
   - Verification: parity remains conditional and not mandatory for this internal stepping slice.
