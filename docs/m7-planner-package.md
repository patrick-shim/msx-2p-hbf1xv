# M7 Planner Package (REQ-M7-002)

- Milestone ID: M7
- Source Request: REQ-M7-002
- Prepared At: 2026-07-05T22:48:56.5915270+09:00
- Status: Completed

Scope: deterministic CPU bus big-endian word helpers.

Planned changes:
- Add `read_word_be` and `write_word_be` to `src/devices/cpu/cpu_bus_client.*`.
- Extend unit tests in `tests/unit/devices/cpu_bus_client_unit_test.cpp`.

Evidence gates: assets/checksum/build/test baseline.
