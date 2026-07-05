# M2 Planner Package (REQ-M2-002)

- Milestone ID: M2
- Source Request: REQ-M2-002
- Prepared At: 2026-07-05T22:44:26.3092259+09:00
- Status: Completed

Scope: deterministic scheduler batching slice.

Planned changes:
- Add `tick_many(steps, cycles_per_step)` to `src/core/scheduler.*`.
- Add deterministic scheduler unit coverage in `tests/unit/core/scheduler_unit_test.cpp`.

Evidence gates:
- `tools/validate-assets.ps1`
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`
- `cmake --build build --config Debug`
- `ctest --test-dir build -C Debug --output-on-failure`
