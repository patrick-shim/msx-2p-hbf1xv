# M8 Planner Package (REQ-M8-002)

- Milestone ID: M8
- Source Request: REQ-M8-002
- Prepared At: 2026-07-05T22:48:56.5915270+09:00
- Status: Completed

Scope: deterministic machine target-cycle stepping API.

Planned changes:
- Add `run_until_cycle(target)` to `src/machine/hbf1xv_machine.*`.
- Extend machine integration deterministic coverage.

Evidence gates: assets/checksum/build/test baseline.
