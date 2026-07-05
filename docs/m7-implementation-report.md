# M7 Implementation Report (REQ-M7-003)

- Milestone ID: M7
- Source Request: REQ-M7-003
- Prepared At: 2026-07-05T22:48:56.5915270+09:00
- Status: Completed

Implemented:
- Added `read_word_be` and `write_word_be` in `src/devices/cpu/cpu_bus_client.h` and `src/devices/cpu/cpu_bus_client.cpp`.
- Added deterministic unit oracles for big-endian word semantics and normalized addresses in `tests/unit/devices/cpu_bus_client_unit_test.cpp`.

Evidence:
- validate-assets pass
- checksum report refreshed
- Debug build success
- ctest `6/6` pass
