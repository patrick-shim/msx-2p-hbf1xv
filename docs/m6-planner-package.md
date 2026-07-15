# M6 Planner Package (REQ-M6-002)

- Milestone ID: M6
- Source Request: REQ-M6-002
- Prepared At: 2026-07-05T22:48:56.5915270+09:00
- Status: Completed

Scope: deterministic scheduler target-advance API.

Planned changes:
- Add `advance_to(target_cycles)` in `src/core/scheduler.*`.
- Add deterministic unit coverage in `tests/unit/core/scheduler_unit_test.cpp`.

Evidence gates: assets/checksum/build/test baseline.
