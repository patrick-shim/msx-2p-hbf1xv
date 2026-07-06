# Project Baseline: Sony HB-F1XV Emulator

This document is the canonical baseline for agent planning and execution.

## Mission

Deliver a production-oriented Sony HB-F1XV MSX2+ emulator in C++ with a cycle-aware core and optional SDL3 frontend.

## Target Machine Specification (Strict, Non-Negotiable)

This is the authoritative hardware target. The emulator MUST match this specification exactly;
any deviation is a defect. No substitution of components, capacities, or variants is permitted.

| Field              | Value                                                                                                                                                            |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **NAME**           | HitBit F1XV                                                                                                                                                      |
| **MANUFACTURER**   | Sony                                                                                                                                                             |
| **ORIGIN**         | Japan                                                                                                                                                            |
| **YEAR**           | 1988                                                                                                                                                             |
| **KEYBOARD**       | Full-stroke keyboard with 5 function keys, numeric keypad and arrow keys                                                                                         |
| **CPU**            | Zilog Z80A                                                                                                                                                       |
| **SPEED**          | 3.58 MHz                                                                                                                                                         |
| **CO-PROCESSOR**   | Yamaha V9958 Video Display Processor                                                                                                                             |
| **RAM**            | 64 KB                                                                                                                                                            |
| **VRAM**           | 128 KB                                                                                                                                                           |
| **ROM**            | 32 KB (Basic & BIOS) + 16 KB (SUB ROM > MSX-BASIC V3.0) + 16 KB (KANJI BASIC ROM + KANJI ROM) + 16 KB (DISK ROM)                                                 |
| **BUILT-IN MEDIA** | 720 KB 3.5" floppy drive                                                                                                                                         |
| **TEXT MODES**     | 40 × 24 / 32 × 24                                                                                                                                                |
| **GRAPHIC MODES**  | 64 × 48, 256 × 192, 256 × 212, 512 × 212, 256 × 212, 256 × 424. Additional KANJI screen modes (Japanese text screen modes): 40 × 24, 32 × 24, 256 × 192, 64 × 48 |
| **COLORS**         | 19,268 simultaneous colors — this is the V9958 YJK maximum on-screen count (YJK/YAE modes), NOT the palette size. The V9958 palette is 512 colors (9-bit: 3 bits each R/G/B), of which 16 are selectable at once in palette modes. Implementers must not treat 19,268 as a palette dimension.                                                                                                                                                           |
| **SOUND**          | FM-PAC (OPLL YM-2413) – 9-channel FM synthesizer                                                                                                                 |
| **I/O PORTS**      | 2 cartridge slots, RGB/Scart video output, 2 joystick ports, printer port, NTSC video output, mono audio output, RF video output, cassette interface             |

## Non-Negotiable Goals

- Determinism: identical inputs produce identical emulator state and outputs across runs.
- Timing fidelity: preserve cycle-aware behavior for timing-sensitive components.
- Clear architecture boundaries: core, devices, peripherals, machine, frontend, tests.
- Deliver precisely spec-matching code for all the components of Sony MSX2+ HBF1XV. This includes the core, devices, and peripherals. The emulator must be accurate to production spec for all of these components (such as cpu, ram, sram, vram, rtc, vdp, psg, fmpac, fdc, io) which their variants are actually used in production models.
- Incremental verification: every milestone must carry executable test evidence.
- Evidence-grounded automation: agents must rely on repository artifacts, including tools scripts and local assets, without inventing missing files.
- Reference alignment: behavior-affecting changes should include A/B checks against openMSX on WSL when applicable.
- Debugging data: `debug/` directory is a sole source of debug data for all the agents (read from and written to) and `tools/`.

## Scope

### In Scope

- Sony HB-F1XV model focus.
- Core scheduling and bus contracts.
- Device workstreams: CPU, V9958 VDP, PSG, YM2413, RTC, FDC, I/O, slot logic, mapper.
- Peripherals: keyboard matrix and joystick input paths.
- Machine composition and model-specific wiring.
- SDL3 frontend integration.
- Unit, integration, and regression tests.
- Local development assets under `bios/` and `roms/`.
- Project documentation and decision records under `docs/`.
- Fully compiled executable with appropriate CLI options needed to load cartridges, load disks, in headless or SDL3 (windows).
- Debug capabilities: full state dump (CPU registers/state, DRAM, SRAM, VRAM content, etc.) and execution-event logs under `debug/traces/` and `debug/logs/`. Subfolders may be added under `debug/` as necessary to capture frames, events, and audio elements. These capabilities underpin opcode-trace, per-instruction parity verification against openMSX, which additionally requires full machine slot/RAM/SRAM/VRAM wiring plus a CPU trace-export facility. This debug/trace/parity capability is owned by milestone M10 (see `agent-protocol/state/milestones.md` and decision `DEC-0001`).
- Generate all needed tools in python or powershell in `tools/` to develop, test and debug (including, without limitation, memory to content to png or audio convert, etc).
- Automation scripts under `tools/` (Python/PowerShell).

### Out of Scope (unless explicitly approved)

- Multi-vendor model expansion.
- Netplay/rewind/TAS workflows.
- Mobile or web targets.
- Redistribution of proprietary BIOS/ROM assets.

## Build Flow (Authoritative)

### Headless/Core

```powershell
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

### SDL3 Frontend

Prerequisite: SDL3 package providing SDL3Config.cmake.

```powershell
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

### Build Notes

- Multi-config generators: use `-C Debug` or `-C Release` at test time.
- Single-config generators: set `-DCMAKE_BUILD_TYPE=Debug` at configure time.

## Milestone Quality Gates

- Planner gate: spec boundaries, dependencies, acceptance criteria, and risks documented.
- Developer gate: implementation complete with unit and integration evidence.
- QA gate: regression risk assessed and sign-off decision recorded.
- Orchestration gate: protocol artifacts complete for each phase transition.
- A/B gate (when applicable): openMSX-on-WSL comparison evidence captured in docs and referenced in QA output.

## Reference Materials (Grounding Sources)

The `references/` folder holds read-only upstream sources and hardware fact sheets used to ground
accuracy and behavior. Agents must prefer these over memory or assumption when specifying or
implementing device behavior, and must cite concrete `references/...` paths when grounding a
behavior claim. The folder grows over time; check it before inventing behavior.

- `references/openmsx-21.0/`: openMSX 21.0 source tree — the behavior reference for device/timing
  semantics and the basis for A/B parity. Read for understanding only; its code is GPL and MUST
  NOT be copied into `src/` (license isolation).
- `references/sdl3/`: SDL3 source tree — the API reference for frontend integration. Consult for
  exact signatures/semantics; do not vendor its code into `src/`.
- `references/fact-sheets/`: curated hardware/device fact sheets (e.g.
  `references/fact-sheets/Yamaha V9958 VDP.md`) — authoritative spec grounding for the target
  machine's components. New sheets are added as needed.

Reference material grounds understanding and A/B comparison; it does not relax the Target Machine
Specification or license terms. Do not assert a reference says something without reading the file.

## Repository Directories (Operational)

- `docs/`: design notes, architecture records, milestone references.
- `bios/`: local BIOS assets for development/testing.
- `roms/`: local ROM assets for development/testing.
- `references/`: read-only grounding sources (openMSX 21.0 source, SDL3 source, `fact-sheets/`).
- `tools/`: helper automation scripts used by agents and developers.
- `debug/`: logs, traces, frame captures, audio captures, cpu and memory dumps, and etc.

Directory usage rules:

- Agents should prefer existing scripts in `tools/` over ad-hoc one-off command chains.
- Any assumption about BIOS/ROM presence must be verified by checking file paths.
- Planning decisions that affect execution should be captured in `docs/` or protocol channels.
- Behavior/spec claims should be grounded in `references/` and cite the concrete file path; never
  copy `references/openmsx-21.0/` or `references/sdl3/` code into `src/`.
- openMSX A/B smoke evidence should be captured via `tools/openmsx-ab-smoke.ps1` and stored in `docs/openmsx-ab-smoke.md`.
- While QAing and debugging, all data must be read or written from/to this `debug/` directory (NOTE: organize sub-directories if needed).
