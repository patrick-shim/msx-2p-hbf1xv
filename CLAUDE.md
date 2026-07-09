# CLAUDE.md — Sony HB-F1XV Emulator

Project memory and operating rules for Claude Code. This file is loaded automatically every
session. It is the authoritative, condensed version of the mission, guardrails, and workflow;
the full canonical text lives in [`agent-protocol/project-baseline.md`](agent-protocol/project-baseline.md)
and [`agent-protocol/guardrails.md`](agent-protocol/guardrails.md).

## Mission

Deliver a production-oriented **Sony HB-F1XV MSX2+** emulator in C++ with a cycle-aware core
and an optional SDL3 frontend.

Non-negotiables:
- **Development Accuracy** - deliver precisely spec-matching code for all the components of Sony MSX2+ HBF1XV. This includes the core, devices, and peripherals. The emulator must be accurate to production spec for all of these components (such as cpu, ram, sram, vram, rtc, vdp, psg, fmpac, fdc, io) which their variants are actually used in production models.  
- **Determinism** — identical inputs produce identical state/output across runs.
- **Timing fidelity** — preserve cycle-aware behavior for timing-sensitive components.
- **Clear boundaries** — `core`, `devices`, `peripherals`, `machine`, `frontend`, `tests`.
- **Incremental verification** — every milestone carries executable test evidence.
- **Evidence-grounded automation** — rely on repository artifacts; never invent files, tests,
  outputs, or asset provenance.
- **Reference alignment** — behavior-affecting changes include an A/B check vs openMSX on WSL
  (`/usr/bin/openmsx`) when applicable.

## Target machine specification (strict, non-negotiable)

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

## Scope

**In scope:** 
- Sony HB-F1XV (MSX2+); core scheduling and bus contracts; devices (CPU/Z80A,
V9958 VDP, PSG, YM2413, RTC, FDC, I/O, slot logic, mapper); peripherals (keyboard matrix,
joystick); machine composition; SDL3 frontend; unit/integration/system tests; local
`bios/` + `roms/` assets; docs and decisions under `docs/`; automation under `tools/`.
- Fully compiled executable with appropriate CLI options needed to load cartridges, load disks, in headless or SDL3 (windows).
- Debug capabilities (full state dump - cpu states, dram, sram, vram content, etc) and logs of the execution events in `debug/traces/` and `debug/logs`.  You may add subfolders uner this `debug/` neccesary to capture frames, events, and audio elements if not-exist as needed.
- Generate all needed tools in python and powershell in `tools/` to develop, test and debug (including, without limitation, memory to content to png or audio convert, etc).


**Out of scope (unless a decision approves it):** multi-vendor models; netplay/rewind/TAS;
mobile/web targets; redistribution of proprietary BIOS/ROM/disk assets.

## Reference materials (grounding sources)

The `references/` folder holds **read-only upstream sources and hardware fact sheets** used to
ground accuracy and behavior. Treat them as authoritative grounding evidence — prefer them over
memory or assumption when specifying or implementing device behavior. This folder grows over
time; check it before inventing behavior.

- `references/openmsx-21.0/` — openMSX 21.0 source tree. The **primary behavior reference** for
  device/timing semantics and the basis for A/B parity. Read it to understand correct behavior;
  **never copy its code into `src/`** (license isolation — openMSX is GPL). Cite it as
  `references/openmsx-21.0/<path>` when grounding a claim.
- `references/fmsx-60/` — fMSX 6.0 (Marat Fayzullin) source tree + executables/ROMs. A **second,
  independent behavior cross-reference** (added 2026-07-08, human-provided): an implementation
  lineage independent of openMSX, valuable for triangulating device behavior when the two
  references agree/disagree (V9938 command engine in `source/fMSX/V9938.c`, AY8910 PSG, YM2413,
  SCC, WD1793-family FDC, i8255 PPI in `source/EMULib/`, Z80 core in `source/Z80/`). License:
  freeware, **non-commercial distribution only, not open-source** — the same hard rule as openMSX
  applies: read for understanding, **never copy its code into `src/`**, and its large data tables
  fall under the standing zero-license-sensitive-work directive exactly like openMSX's. Cite as
  `references/fmsx-60/<path>`. Its bundled generic MSX ROMs (`executables/*.ROM`) are
  cross-reference assets only — this project's machine uses the Sony-specific ROMs in `bios/`.
- `references/sdl3/` — SDL3 source tree. The **API reference** for frontend integration; consult
  for exact SDL3 signatures/semantics rather than guessing. Do not vendor its code into `src/`.
- `references/fact-sheets/` — curated hardware/device fact sheets (e.g.
  `references/fact-sheets/Yamaha V9958 VDP.md`). Authoritative spec grounding for the target
  machine's components. New sheets are added here as needed.
- `references/music_in_basic.md` — human-provided (2026-07-09) MSX-MUSIC BASIC sample programs
  (`CALL MUSIC` / `PLAY #n` MML, preset instruments `@n`, rhythm channel `#10`). Purpose: typed
  BASIC programs as agile, game-asset-free FM test drivers — they exercise the extended-statement
  dispatch into `bios/f1xvmus.rom`, the FM-BIOS sequencer, and the OPLL built-in instrument
  presets, a path games (which drive the chip directly) do not cover. Caveat: MSX-MUSIC BASIC
  dialects vary — validate any statement against the actual `f1xvmus.rom` behavior and an openMSX
  A/B of the identical typed program before treating a sample as a fixture.
- `references/zexall/` — legally-sourced ZEXALL/ZEXDOC Z80 exerciser binaries (GPL v2, YAZE-AG),
  used as black-box test fixtures by the M24 CPU-regression suite.

When two behavior references disagree, do not silently pick one: check the fact-sheets/primary
documentation, note the disagreement in the relevant investigation/implementation report, and
prefer the interpretation corroborated by real-hardware documentation or A/B evidence.

Rules: when a behavior-affecting decision cites hardware behavior, cite the concrete
`references/...` path. Reference material grounds understanding and A/B comparison; it does not
relax the [target machine specification](#target-machine-specification-strict-non-negotiable) or
license terms. Do not assert a reference says something without reading the concrete file.

## Build & test flow (authoritative)

**Single-build policy (M33, DEC-0041): there is exactly ONE build tree — `build/`.** Never
create per-agent or per-purpose trees (`build-qa-*`, `build-<milestone>-*`, ...); QA re-verifies
from the same `build/` after a clean rebuild, not from a parallel tree. `.gitignore` swallows
any `build*` path, but creating one is itself a policy violation.

The standard configuration is SDL3=ON — it is the superset (both executables + all tests):

```powershell
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Headless-only fallback (only for an environment without `SDL3Config.cmake` on the prefix
path) reconfigures the SAME tree: `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF`.

- Multi-config generators (VS): use `-C Debug`/`-C Release` at test time.
- Single-config generators: set `-DCMAKE_BUILD_TYPE=Debug` at configure time.
- Executables land in `build/Debug/` (`sony_msx_headless.exe`, `sony_msx_sdl3.exe`).

## Evidence gates (run and capture; never fabricate)

- `tools/validate-assets.ps1` — required BIOS present + ≥1 ROM.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — reproducible checksums.
- `cmake --build build --config Debug` — build succeeds.
- `ctest --test-dir build -C Debug --output-on-failure` — deterministic suite passes.
- Behavior-affecting only: `tools/openmsx-ab-smoke.ps1` → `docs/openmsx-ab-smoke.md`.

## Multi-agent orchestration (Claude Code native)

Hierarchy — **Human → Coordinator (main session) → specialist subagents**:

- **Coordinator** = this main Claude Code session, driven by the
  [`/msx-master`](.claude/commands/msx-master.md) and
  [`/msx-autopilot`](.claude/commands/msx-autopilot.md) commands. It owns human interaction,
  keeps the user as source of truth, and delegates execution.
- **[`msx-orchestration`](.claude/agents/msx-orchestration.md)** (subagent, read-only) —
  validates protocol gates and returns the next allowed handoff. It advises; it does not
  itself implement.
- **[`msx-planner`](.claude/agents/msx-planner.md)** — specs, milestones, dependencies,
  acceptance criteria. No code.
- **[`msx-developer`](.claude/agents/msx-developer.md)** — implements the current slice; adds
  unit + integration tests; runs evidence gates.
- **[`msx-qa`](.claude/agents/msx-qa.md)** — regression assessment and sign-off recommendation.

The coordinator spawns specialists via the Agent/Task tool. Subagents do **not** spawn other
subagents — sequencing is owned by the coordinator (or the workflow below).

### Ways to run it

1. **Interactive:** `/msx-master <objective, scope, constraints, completion criteria>`.
2. **Autopilot loop:** `/msx-autopilot target=M10 completion="M10 done with QA signoff"` —
   runs planner → developer → QA cycles until the target closes or a real blocker appears.
   Wrap with the `/loop` skill for interval/self-paced re-entry.
3. **Deterministic workflow:** the auto-runnable
   [`.claude/workflows/msx-milestone.js`](.claude/workflows/msx-milestone.js) fans out
   planner → developer → QA per milestone with evidence gates and a loop-until-signoff barrier
   (opt-in; run via the Workflow tool).
4. **Kickoff:** `/msx-kickoff` initializes phase/milestone/handoff state for a new run.
5. **Planning aid:** `/msx-test-matrix` generates deterministic device/subsystem test matrices.

### Protocol rules (hard)

- Delegate execution only through the specialist subagents; keep the user as source of truth.
- **Planner-first** sequencing → developer evidence → QA sign-off → release decision. Never
  skip a gate; never declare completion without QA sign-off.
- Keep `agent-protocol/` current every cycle: channels are append-only, ISO-8601 timestamps,
  and `state/current-phase.md` + `state/milestones.md` reflect reality.
- Never claim a file, test, build, or command output that was not read or executed. Label
  uncertainty with `Assumption:` and a verification action.
- Work only the approved milestone; scope changes require an entry in
  `agent-protocol/channels/decisions.md`.

## Repository layout

- `src/` — emulator source. Folder responsibilities: [`src/CLAUDE.md`](src/CLAUDE.md).
- `tests/` — deterministic unit/integration/system tests: [`tests/CLAUDE.md`](tests/CLAUDE.md).
- `agent-protocol/` — runtime coordination state, history, and templates.
- `docs/` — design notes, milestone packages, implementation reports, QA sign-offs.
- `tools/` — PowerShell/Python helper scripts (prefer these over ad-hoc command chains).
- `bios/`, `roms/`, `disks/` — local development assets (legally sourced; not redistributable).
  `disks/` holds MSX-DOS floppy disk images (e.g. `msxdos22.dsk`, `msxdos23.dsk`, `msxdos24/`)
  used for FDC/boot testing, plus `disks/games/` — game floppy images (e.g. the two-disk
  YS II set) for live/regression playtesting of the disk-boot and multi-disk paths.
- `references/` — read-only grounding sources: openMSX 21.0 source (primary behavior reference),
  fMSX 6.0 source (independent second behavior cross-reference, non-commercial freeware), SDL3
  source (API reference), `fact-sheets/` (hardware specs), and `zexall/` (CPU test fixtures).
  Reference only; never copied into `src/`. See
  [Reference materials](#reference-materials-grounding-sources).
- `.claude/` — agents, commands, workflow, and settings for the orchestration.
- `debug/` - debug traces, video frames, audio captures, cpu and memory dumps, and etc.
