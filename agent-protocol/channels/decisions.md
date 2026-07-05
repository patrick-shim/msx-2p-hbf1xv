# Decisions Channel

Record approved decisions and scope changes.

Use this format:

- Decision ID:
- Requested By:
- Approved By:
- Decision:
- Impacted Milestones:
- Risk Notes:
- Effective Date:

- Decision ID: DEC-0001
- Requested By: Human (project owner, source of truth)
- Approved By: Human (project owner, source of truth)
- Decision: The in-scope debug/trace/parity capability in `agent-protocol/project-baseline.md` (full state dump of CPU/DRAM/SRAM/VRAM, execution-event logs under `debug/traces/` and `debug/logs/`, subfolders under `debug/` for frames/events/audio, plus the `tools/` converters such as memory->png/audio) is confirmed as committed scope and is assigned to a new milestone **M10 — Debug/Trace Infrastructure & openMSX Opcode-Trace Parity**. M10 owns: (a) the CPU trace-export facility, (b) full machine slot/RAM/SRAM/VRAM wiring sufficient to run a comparable openMSX program, and (c) the openMSX opcode-trace / per-instruction parity harness. Milestone **M9** QA blocker **R5** (openMSX opcode-trace parity evidence), which QA rated a CONDITIONAL PASS in `docs/m9-qa-signoff.md` (REQ-M9-010), is formally DEFERRED to M10. R5 is therefore an accepted (deferred), non-fabricated risk for M9 rather than an open coding gap: M9's Z80A opcode-family coverage and interrupt architecture are complete and test-backed (`ctest` 21/21). M9 remains open until the human explicitly requests closure; upon that request M9 may be closed on the strength of this deferral, with R5 tracked to M10. M9 A/B parity evidence therefore remains outstanding-but-accepted and must be delivered under M10 before M10 itself can close.
- Impacted Milestones: M9 (R5 deferred/accepted), M10 (new milestone owning debug/trace/parity implementation).
- Risk Notes: Deferral must not be read as a waiver of correctness — opcode-family and interrupt behavior remain green and verified. R5 parity remains a real, required deliverable under M10; no parity result is to be claimed until a genuine trace-diff harness produces one. Blocker documented in `docs/m9-qa-signoff.md`: openMSX requires full slot/RAM wiring (unmapped 0xC000 reads back 0xFF at reset; headless debugger `step` does not advance PC) and this emulator lacks a trace-export facility — both are M10 scope.
- Effective Date: 2026-07-06

- Decision ID: DEC-0002
- Requested By: Human (project owner) via `/msx-autopilot target=M12` directive on 2026-07-06 requesting three next subsystem milestones labeled "M10 chipset / M11 memory / M12 VDP".
- Approved By: MSX Master Agent (coordinator), interpreting the human directive against the append-only authoritative ledger.
- Decision: The label **M10 is already consumed** by the closed "Debug/Trace Infrastructure & openMSX Opcode-Trace Parity" milestone (`agent-protocol/state/milestones.md`, closed REQ-M10-009). The append-only ledger cannot be rewritten. The human's three requested work items are therefore allocated to the next free IDs, preserving the requested substance and ordering exactly (only the numeric label shifts +1): **M11 = S1985 "MSX-ENGINE" chipset + full system bus + system integration**; **M12 = RAM/VRAM/ROM memory devices + system integration**; **M13 = V9958 VDP + system integration**. Each milestone carries: system integration (wiring into bus/CPU), system-wide unit tests, an openMSX A/B trace-diff, and a required QA sign-off before closure. Sequencing is strict: M11 → M12 → M13 (bus/slot infrastructure precedes the memory devices that populate slots, which precede the VDP that plugs into the I/O bus).
- VRAM placement ruling (human explicitly delegated this judgment): the **128 KB VRAM is owned by the V9958 VDP device**, NOT modeled as an independently CPU-addressable memory region. On real HB-F1XV hardware and in openMSX (`references/openmsx-21.0/src/video/`) the CPU reaches VRAM only through VDP ports `#98/#99`; VRAM has no place in the CPU slot/address map. Consequently the inert `vram_` placeholder currently in `src/machine/hbf1xv_machine.*` (added under M10-S2 as pure storage) migrates into the VDP device in **M13**. M12's memory scope is the CPU-addressable store: 64 KB main RAM (mapper, openMSX slot 3-0) + the ROM set (32 KB BIOS/BASIC, 16 KB SUB, 16 KB KANJI, 16 KB DISK per the Target Machine Specification). Final source placement (folders/files under `src/`) is delegated to the planner with authority to add/rearrange per `src/CLAUDE.md` boundary rules.
- S1985 architecture ruling: per the S1985 fact-sheet §10 (`references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md`) and openMSX (`references/openmsx-21.0/src/MSXS1985.*`, `DeviceFactory.cc`), the engine is NOT a monolith. M11 implements the residual engine-specific behavior as a thin S1985 layer — 16-byte backup RAM on switched-I/O device ID `0xFE`, mapper-readback base `0x80` mask `0x1F` (`100xxxxx`), port mirrors `#98-9B`→`#9C-9F` and `#A8-AB`→`#AC-AF`, the +1 M1 (opcode-fetch) wait state, and VDP `/CSR`/`/CSW` strobe timing — over independent modules (PPI/i8255 slot select, MapperIO) as the planner scopes them. Full PSG/YM2149 and RTC/RP5C01 device internals are their own later milestones (baseline lists psg/rtc separately); M11 provides only the bus/engine seams they will plug into unless the planner justifies including a module. No `references/openmsx-21.0/` or `references/sdl3/` code may be copied into `src/` (GPL isolation, guardrails).
- Impacted Milestones: M11 (new — chipset + system bus), M12 (new — RAM/VRAM/ROM memory), M13 (new — V9958 VDP). No change to closed M0–M10.
- Risk Notes: The human's spoken labels (M10/M11/M12) are remapped to (M11/M12/M13); all downstream artifacts use the new IDs (`docs/m11-*`, `docs/m12-*`, `docs/m13-*`). VDP (M13) is one of the most complex chips in the system; the planner must decompose it into deterministic slices and MUST NOT claim A/B parity without a genuine captured trace-diff (guardrails, DEC-0001 precedent). VRAM-under-VDP means M12 A/B parity is scoped to CPU-visible RAM/ROM only; VRAM parity is M13 scope.
- Effective Date: 2026-07-06
