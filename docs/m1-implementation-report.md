# M1 Implementation Report (REQ-M1-004)

- Milestone ID: M1
- Source Request: REQ-M1-004
- Prepared By: MSX Developer Agent
- Prepared At: 2026-07-05T22:36:09.1287078+09:00
- Status: Completed

## Implemented Slice

Deterministic machine stepping and frame accounting.

Code changes:

1. `src/machine/hbf1xv_machine.h`
   - Added `run_cycles(std::uint64_t cycles)`.
   - Added `frame_count() const`.
   - Added internal `frame_count_` state.
2. `src/machine/hbf1xv_machine.cpp`
   - `cold_boot()` now resets both scheduler and frame counter.
   - `run_frame()` now increments frame counter after ticking frame cycles.
   - Added `run_cycles()` implementation for deterministic custom cycle stepping.
   - Added `frame_count()` accessor.
3. `tests/integration/machine/hbf1xv_machine_integration_test.cpp`
   - Added deterministic custom-cycle accumulation oracle.
   - Added frame-counter tracking oracle.
   - Added cold-boot reset oracles for both cycles and frames.

## Deterministic Oracles

- `RunCycles_CustomStep_ReproducibleCycleCount`
- `FrameCount_AfterTwoFrames_TracksRunFrameOnly`
- `ColdBoot_AfterProgress_ResetsCycleCount`
- `ColdBoot_AfterProgress_ResetsFrameCount`

## Evidence Gates Executed

Executed at 2026-07-05T22:36:09.1287078+09:00 cycle:

1. `tools/validate-assets.ps1`
   - Result: `True`.
2. `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
   - Result: checksum artifact refreshed at `docs/asset-checksums.txt`.
3. `cmake --build build --config Debug`
   - Result: successful build including updated core, integration test, and headless executable.
4. `ctest --test-dir build -C Debug --output-on-failure`
   - Result: `100% tests passed, 0 tests failed out of 6`.

## Notes

- This slice changes deterministic machine behavior API and validation only.
- openMSX parity rerun remains conditional for behavior-parity scopes and is not mandatory for this internal stepping contract slice.
