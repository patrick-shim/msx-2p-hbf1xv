# M0 Planner Package (REQ-M0-003)

- Milestone ID: M0
- Source Request: REQ-M0-003
- Prepared By: MSX Planner Agent
- Accepted By: MSX Orchestration Agent
- Prepared At: 2026-07-05T22:42:00+09:00

## Scope Baseline

This package defines the M0 planning baseline for deterministic Sony HB-F1XV execution and implementation-lane opening.

Referenced runtime and protocol paths:

- `docs/`
- `bios/`
- `roms/`
- `tools/`
- `src/`
- `tests/`
- `agent-protocol/`
- `references/`
- `debug/`

## Subsystem Boundaries

1. `src/core/`: Scheduling, bus contracts, deterministic execution primitives.
2. `src/devices/`: Device-level components (CPU and future chips) behind core contracts.
3. `src/peripherals/`: Input and peripheral abstractions and adapters.
4. `src/machine/`: HB-F1XV machine wiring and model-specific composition.
5. `src/frontend/`: Optional SDL3 host integration; no core behavior ownership.
6. `tests/unit/`: Deterministic isolated contract tests.
7. `tests/integration/`: Deterministic cross-subsystem interactions.
8. `tests/system/`: Deterministic machine-level boot and scenario checks.

Boundary policy:

- Core contracts are the source of truth for timing-sensitive interactions.
- Device/peripheral code may depend on core contracts; core must not depend on frontend.
- Machine composition may depend on core/devices/peripherals, but not vice versa.
- Frontend integration remains optional and must not alter deterministic core outcomes.

## Dependency Sequencing

1. Asset readiness gate before behavior work:
   - Verify required assets by path under `bios/` and `roms/`.
   - Run `tools/validate-assets.ps1`.
2. Evidence baseline gate:
   - Run `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`.
3. Deterministic core/device lane:
   - Maintain/extend core bus and scheduler deterministic contracts.
   - Add implementation slices in `src/devices/` only after contract alignment.
4. Machine composition lane:
   - Integrate approved slices into `src/machine/` and validate deterministic integration tests.
5. Optional frontend lane:
   - Enable SDL3 path only after headless deterministic evidence is stable.
6. Parity lane (conditional):
   - When behavior parity is in scope, run `tools/openmsx-ab-smoke.ps1` and update `docs/openmsx-ab-smoke.md`.

## Risks, Assumptions, and Verification

1. Risk: Missing or mismatched local assets could invalidate downstream test outcomes.
   - Verification: Run `tools/validate-assets.ps1` before implementation and before QA regression cycles.
2. Risk: Undocumented asset changes can break reproducibility.
   - Verification: Refresh `docs/asset-checksums.txt` via `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` at lane start.
3. Assumption: Current deterministic test scaffold remains the implementation baseline.
   - Verification: Execute baseline build/test flow and compare against expected deterministic suite naming in `tests/`.
4. Assumption: openMSX parity evidence is required only for behavior-affecting slices.
   - Verification: For behavior-affecting changes, capture run output using `tools/openmsx-ab-smoke.ps1` in `docs/openmsx-ab-smoke.md`.

## Acceptance Criteria Mapped to Build/Test Flow

1. Planner acceptance:
   - Subsystem boundaries and dependency sequencing are documented in this file.
2. Implementation lane acceptance (entry gate):
   - Asset and checksum evidence gates are defined with concrete script paths.
3. Build/test mapping:
   - Headless flow (authoritative):
     - `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF`
     - `cmake --build build`
     - `ctest --test-dir build -C Debug --output-on-failure`
   - SDL3 flow (optional):
     - `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON`
     - `cmake --build build`
     - `ctest --test-dir build -C Debug --output-on-failure`
4. Evidence gate mapping:
   - Asset validation: `tools/validate-assets.ps1`
   - Checksums: `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
   - Parity conditional: `tools/openmsx-ab-smoke.ps1` with output in `docs/openmsx-ab-smoke.md`

## Folder Structure Verification Snapshot

Verified present at planning cycle execution time:

1. `docs/`
2. `bios/`
3. `roms/`
4. `tools/`
5. `src/`
6. `tests/`
7. `agent-protocol/`
8. `references/`
9. `debug/`

No folder bootstrap action required for this cycle.

## Evidence Notes

- This planner package defines required evidence gates and protocol readiness.
- No build/test/script execution results are claimed in this planning artifact.