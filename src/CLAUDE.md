# src/ — folder conventions

Applies when editing anything under `src/`. See root [`../CLAUDE.md`](../CLAUDE.md) for mission,
build flow, and orchestration rules.

## Strict folder responsibilities

- `src/core/` — scheduler, clock domains, buses, device contracts, orchestration primitives.
- `src/devices/` — chip and controller implementations (CPU/Z80A, VDP, PSG, YM2413, RTC, FDC,
  I/O, mapper, slot logic).
- `src/peripherals/` — keyboard matrix, joystick, and slot-side peripheral adapters.
- `src/machine/` — Sony HB-F1XV composition, memory/slot map, model-specific wiring.
- `src/frontend/` — SDL3 application layer, input mapping, audio/video presentation.

## Boundary rules

- Do **not** place device logic in `src/core/`.
- Do **not** place machine wiring in `src/devices/`.
- Keep frontend concerns out of `src/core/`, `src/devices/`, and `src/machine/`.
- Behavior-affecting device changes should carry an openMSX A/B check when applicable.

Every new behavior needs a deterministic test — see [`../tests/CLAUDE.md`](../tests/CLAUDE.md).
