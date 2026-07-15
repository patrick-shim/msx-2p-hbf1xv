# M9 Planner Package (REQ-M9-002)

- Milestone ID: M9
- Source Request: REQ-M9-002
- Title: Full Z80A CPU Core (HB-F1XV MSX2+)

## Target CPU and Machine Facts

- Target machine: Sony HB-F1XV (MSX2+).
- CPU target: Z80A (Z0840004SPC), not R800.
- Baseline timing model: Z80 T-state accounting at MSX base clock rate.

## Scope

Implement a deterministic Z80A core with:

- Full register model (main and shadow pairs, IX/IY, I/R, IFF1/IFF2, IM mode).
- Interrupt architecture (IM0/IM1/IM2, NMI, RETN, RETI, EI delay semantics).
- Full opcode-family coverage:
  - Unprefixed table.
  - CB table.
  - ED table.
  - DD/FD indexed tables.
  - DDCB/FDCB indexed-bit tables.

## Acceptance Criteria

- Opcode-family coverage is complete and test-backed.
- Interrupt semantics are deterministic and architecture-correct.
- T-state accounting is deterministic with branch/prefix variants covered by tests.
- Evidence gates pass:
  - tools/validate-assets.ps1
  - tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
  - cmake --build build --config Debug
  - ctest --test-dir build -C Debug --output-on-failure
- Behavior parity smoke evidence refreshed:
  - tools/openmsx-ab-smoke.ps1
  - docs/openmsx-ab-smoke.md

## Execution Slices

1. M9-S1: CPU state model and deterministic instruction stepping baseline.
2. M9-S2: Complete unprefixed and CB coverage.
3. M9-S3: Complete ED coverage including block and extended operations.
4. M9-S4: Complete DD/FD and DDCB/FDCB coverage.
5. M9-S5: Interrupt-architecture fidelity hardening and parity-oriented validation.

## Deterministic Oracle Strategy

- Table-driven unit vectors for register/flag/memory/PC/SP outcomes.
- T-state assertions per path (taken/not-taken, prefix variants).
- Integration traces validating machine cycle accumulation and interrupt delivery.
- System-level deterministic bootstrap and handler-entry behavior checks.

## Risks

- Semantic drift in extended and indexed instruction families.
- Interrupt acknowledge behavior mismatch in IM0/IM2 if device-vector modeling is simplified.
- Completion effort is significant and requires multi-slice execution.
