# sony-msx-hbf1xv

Production-oriented Sony HB-F1XV MSX2+ emulator written in C++ with a cycle-aware core and an optional SDL3 desktop frontend.

## Current status

This repository now includes a working initial scaffold with:

- CMake-based build system.
- Core machine/scheduler skeleton.
- Initial CPU bus contract skeleton and deterministic tests.
- Deterministic unit, integration, and system test executables.
- Claude Code multi-agent orchestration: subagents and commands under `.claude/`, with runtime coordination state under `agent-protocol/`.
- Asset directories for BIOS and ROM files.
- A `tools` directory for Python/PowerShell helper scripts used by agents.
- A `docs` directory for project documentation.

## Project goals

- Emulate Sony HB-F1XV behavior with strong timing correctness and predictable results.
- Keep strict subsystem boundaries (`core`, `devices`, `peripherals`, `machine`, `frontend`, `tests`).
- Maintain deterministic test-first development and regression discipline.

## Scope

### In scope

- Sony HB-F1XV focused emulation (MSX2+ generation).
- Cycle-aware execution model for timing-sensitive behavior.
- SDL3 desktop frontend (optional build target).
- Unit, integration, and system tests.
- Local BIOS/ROM assets for development workflows.
- Agent automation helper scripts under `tools`.

### Out of scope (for now)

- Broad multi-vendor MSX model coverage.
- Netplay, rewind, TAS workflows, advanced debugger UX.
- Mobile/web targets.

## Project scaffold

```text
.
|-- CMakeLists.txt
|-- README.md
|-- docs
|-- bios
|-- roms
|-- tools
|   `-- README.md
|-- src
|   |-- main.cpp
|   |-- core
|   |   |-- bus.h
|   |   |-- scheduler.h
|   |   `-- scheduler.cpp
|   |-- devices
|   |   `-- cpu
|   |       |-- cpu_bus_client.h
|   |       `-- cpu_bus_client.cpp
|   |-- machine
|   |   |-- hbf1xv_machine.h
|   |   `-- hbf1xv_machine.cpp
|   `-- frontend
|       `-- sdl3_main.cpp
|-- tests
|   |-- CMakeLists.txt
|   |-- unit
|   |   `-- core
|   |       |-- scheduler_unit_test.cpp
|   |       `-- bus_contract_unit_test.cpp
|   |   `-- devices
|   |       `-- cpu_bus_client_unit_test.cpp
|   |-- integration
|   |   |-- devices
|   |   |   `-- cpu_bus_contract_integration_test.cpp
|   |   `-- machine
|   |       `-- hbf1xv_machine_integration_test.cpp
|   `-- system
|       `-- boot_system_test.cpp
|-- CLAUDE.md
|-- .claude
|   |-- agents            (msx-orchestration, msx-planner, msx-developer, msx-qa)
|   |-- commands          (msx-master, msx-autopilot, msx-kickoff, msx-test-matrix)
|   |-- workflows         (msx-milestone.js)
|   `-- settings.json
`-- agent-protocol
    |-- channels          (requests, responses, decisions)
    |-- state             (current-phase, milestones, definition-of-done)
    `-- templates
```

## Build and test

### Headless/core scaffold

```powershell
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

### SDL3 frontend scaffold

Prerequisite: install SDL3 with `SDL3Config.cmake` available to CMake.

```powershell
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

Notes:

- On Windows Visual Studio generators (multi-config), use `-C Debug` (or `-C Release`) with `ctest`.
- On single-config generators, configure with `-DCMAKE_BUILD_TYPE=Debug` and run `ctest --test-dir build --output-on-failure`.

## BIOS, ROM, and disk directories

- `bios/`: local BIOS blobs used for development and validation.
- `roms/`: local software/game/test ROM images used for emulator bring-up.
- `disks/`: local MSX-DOS floppy disk images used for FDC/boot testing (e.g. `msxdos22.dsk`,
  `msxdos23.dsk`, `msxdos24/`).

Repository policy:

- Keep BIOS/ROM/disk files local and legally sourced.
- Do not assume redistribution rights for proprietary files.
- Agents must reference exact file paths and must not invent missing assets.

Quick validation:

```powershell
./tools/validate-assets.ps1
./tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
```

## Tools directory

- `tools/` is reserved for Python and PowerShell helper scripts used by agents and developers.
- See `tools/README.md` for script expectations and conventions.
- Prefer invoking repository scripts from `tools/` when an equivalent script exists.
- Current asset check script: `tools/validate-assets.ps1`.
- Current checksum evidence script: `tools/checksum-assets.ps1`.
- Current openMSX A/B smoke script: `tools/openmsx-ab-smoke.ps1`.

## Docs directory

- `docs/` contains design notes, milestones, and implementation references.
- Planner outputs should be mirrored or summarized in `docs/` for long-lived decisions.
- A/B smoke evidence with openMSX is recorded in `docs/openmsx-ab-smoke.md`.

## A/B testing with openMSX on WSL

openMSX is available on this machine through WSL and can be used as a reference emulator for A/B validation.

Verified binary path:

- `/usr/bin/openmsx` (inside WSL)

Run smoke evidence generation:

```powershell
./tools/openmsx-ab-smoke.ps1
```

This produces:

- `docs/openmsx-ab-smoke.md`

A/B policy:

- A: `sony-msx-hbf1xv` local behavior.
- B: openMSX behavior on WSL for the same BIOS/ROM.
- For behavior-affecting milestones, capture and reference A/B evidence in QA handoffs.

## Deterministic test policy

- `tests/unit/*_unit_test.cpp`: isolated deterministic behavior.
- `tests/integration/*_integration_test.cpp`: cross-component deterministic behavior.
- `tests/system/*_system_test.cpp`: machine-level deterministic behavior.

All integration/system tests must include a deterministic oracle and avoid wall-clock dependence.

## Agent workflow (Claude Code multi-agent)

The repository runs a Claude Code native, role-based orchestration.
See [`CLAUDE.md`](CLAUDE.md) for the full rules.

Hierarchy — **Human -> Coordinator (main session) -> specialist subagents**:

- The **main Claude Code session** is the human-facing coordinator.
- It consults the read-only `msx-orchestration` subagent to validate protocol gates.
- It delegates execution, planner-first, to the specialist subagents:
  - `msx-planner` (specs, milestones, acceptance criteria)
  - `msx-developer` (implementation + deterministic tests + evidence gates)
  - `msx-qa` (regression assessment + sign-off)

### How to run it

Interactive coordination:

```text
/msx-master <objective, scope, constraints, completion criteria>
```

Continuous autopilot loop (wrap with `/loop` for hands-off, interval or self-paced runs):

```text
/msx-autopilot target=M10 completion="M10 done with QA signoff"
```

Initialize a fresh run, or generate a deterministic test matrix:

```text
/msx-kickoff <run name, objective, constraints>
/msx-test-matrix <devices/subsystems, acceptance goals>
```

Deterministic auto-runnable orchestration (planner -> developer -> QA, loop until sign-off) is
defined in [`.claude/workflows/msx-milestone.js`](.claude/workflows/msx-milestone.js) and runs
via the Workflow tool.

All handoffs and state are tracked under `agent-protocol/`:

- `agent-protocol/channels/requests.md`
- `agent-protocol/channels/responses.md`
- `agent-protocol/state/current-phase.md`
- `agent-protocol/state/milestones.md`

## Next build-out steps

1. Add first CPU execution micro-slice on top of the existing bus contract skeleton.
2. Replace placeholder device/peripheral headers with first real interfaces.
3. Expand system tests to cover deterministic boot and ROM flow checkpoints using `bios/` and `roms/` assets.
