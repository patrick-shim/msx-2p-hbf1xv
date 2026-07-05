# M0 Implementation Prep (REQ-M0-005)

- Milestone ID: M0
- Source Request: REQ-M0-005
- Prepared By: MSX Developer Agent
- Prepared At: 2026-07-05T23:12:00+09:00
- Status: Completed

## Scope

Implementation-prep only. No feature implementation is performed in this artifact.

Referenced runtime and protocol paths:

- `docs/`
- `bios/`
- `roms/`
- `tools/`
- `src/`
- `tests/`
- `agent-protocol/`

## First-Slice Prep Checklist

1. Planner dependency lock
   - [x] Confirm accepted planner package: `docs/m0-planner-package.md`.
2. Asset readiness gate
   - [x] Execute `tools/validate-assets.ps1` in current cycle.
   - [x] Verify required asset directories exist: `bios/`, `roms/`.
3. Checksum baseline gate
   - [x] Execute `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` in current cycle.
   - [x] Existing checksum artifact present: `docs/asset-checksums.txt`.
4. Deterministic test lane prep
   - [x] Unit lane mapped from `tests/unit/`.
   - [x] Integration lane mapped from `tests/integration/`.
   - [x] System lane mapped from `tests/system/`.
5. Build/test smoke gate
   - [x] Execute `cmake --build build` in current cycle.
   - [x] Execute `ctest --test-dir build -C Debug --output-on-failure` in current cycle.
6. Parity evidence gate (conditional)
   - [x] Existing parity evidence path confirmed: `docs/openmsx-ab-smoke.md`.
   - [ ] Re-run `tools/openmsx-ab-smoke.ps1` only when behavior-affecting implementation starts.

## Deterministic Test-Plan Mapping

Naming policy for deterministic lanes:

- Unit lane: `*_unit_test`
- Integration lane: `*_integration_test`
- System lane: `*_system_test`

Lane mapping from current repository evidence:

1. Unit (`tests/unit/`)
   - `tests/unit/core/scheduler_unit_test.cpp` -> `core_scheduler_unit_test`
   - `tests/unit/core/bus_contract_unit_test.cpp` -> `core_bus_contract_unit_test`
   - `tests/unit/devices/cpu_bus_client_unit_test.cpp` -> `devices_cpu_bus_client_unit_test`
2. Integration (`tests/integration/`)
   - `tests/integration/machine/hbf1xv_machine_integration_test.cpp` -> `machine_hbf1xv_machine_integration_test`
   - `tests/integration/devices/cpu_bus_contract_integration_test.cpp` -> `devices_cpu_bus_contract_integration_test`
3. System (`tests/system/`)
   - `tests/system/boot_system_test.cpp` -> `boot_system_test`

Execution order policy for first slice:

1. Unit lane pass required before integration lane run.
2. Integration lane pass required before system lane run.
3. System lane and asset gates must be current-cycle valid before QA routing.

## Dependency List For Implementation Start

1. Protocol dependencies
   - `REQ-M0-003` completed planner package at `docs/m0-planner-package.md`.
   - `REQ-M0-004` actionable lock satisfied.
2. Asset/script dependencies
   - `tools/validate-assets.ps1`
   - `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
   - `tools/openmsx-ab-smoke.ps1` (conditional for behavior parity)
3. Build/test dependencies
   - Existing configured build directory at `build/`
   - Test targets defined in `tests/CMakeLists.txt`
4. Folder dependency verification
   - `docs/`, `bios/`, `roms/`, `tools/`, `src/`, `tests/`, `agent-protocol/`

## Evidence Hooks And Executed Evidence

Required command hooks for this lane:

1. `tools/validate-assets.ps1`
2. `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
3. `cmake --build build`
4. `ctest --test-dir build -C Debug --output-on-failure`
5. `tools/openmsx-ab-smoke.ps1` (conditional)

Executed evidence available in repository (historical, not re-executed in this session):

1. `docs/asset-checksums.txt`
   - Generated at: `2026-07-05T21:57:32.8544447+09:00`
   - Scope includes all required `bios/*.rom` and one `roms/*.rom` entry.
2. `build/Testing/Temporary/LastTest.log`
   - Records a `ctest` run at Jul 05 21:48 with 6 executed tests and per-test pass records.
3. `build/Testing/Temporary/LastTestsFailed.log`
   - Contains failing test names from a different run snapshot; indicates stale/inconsistent regression state if not re-run.
4. `docs/openmsx-ab-smoke.md`
   - Contains a prior A/B smoke report with openMSX version and input paths.

Current-cycle execution status:

- Completed at `2026-07-05T22:25:03.8416334+09:00` with fresh execution evidence:
  1. `tools/validate-assets.ps1`
     - Result: `True`
     - BIOS files found: `f1xvbios.rom`, `f1xvdisk.rom`, `f1xvext.rom`, `f1xvfirm.rom`, `f1xvkdr.rom`, `f1xvkfn.rom`, `f1xvmus.rom`
     - ROM file count: `1` (`aleste.rom`)
  2. `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
     - Result: report refreshed at `docs/asset-checksums.txt`
  3. `cmake --build build --config Debug`
     - Result: Debug targets built successfully, including `sony_msx_headless` and test executables
  4. `ctest --test-dir build -C Debug --output-on-failure`
     - Result: `100% tests passed, 0 tests failed out of 6`

## Blockers

None for implementation-prep closure in this cycle.

## Completion Decision

REQ-M0-005 implementation-prep artifact production is complete with current-cycle command evidence and is ready for QA routing.