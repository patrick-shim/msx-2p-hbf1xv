# M10 Implementation Report

Milestone M10 — Debug/Trace Infrastructure & openMSX Opcode-Trace Parity.
Developer-authored implementation evidence, one dated section per delivered slice.

---

## M10-S1 — Deterministic CPU trace-export facility (REQ-M10-003)

- Date: 2026-07-06 (Asia/Seoul, +09:00)
- Slice: M10-S1 (per `docs/m10-planner-package.md`, Section 3)
- Author: MSX Developer Agent
- Status: Completed (S1 scope only; green evidence gates)

### What was implemented

An off-by-default, nullable per-instruction trace-export facility layered on the
existing M9 Z80A core. No M9 CPU semantics, timing, or state were changed; the
facility is purely additive and observational.

- CPU-side hook (`src/devices/cpu/`):
  - New header `src/devices/cpu/z80a_trace.h` defines the plain-data
    `Z80aTraceRecord` and the abstract observer interface `Z80aTraceObserver`.
  - `Z80aCpu` (`z80a_cpu.h` / `z80a_cpu.cpp`) gained a nullable, non-owning
    `Z80aTraceObserver*` (default `nullptr`), `set_trace_observer(...)` /
    `trace_observer()`, and three private helpers:
    `trace_begin_instruction()` snapshots the pre-execution register file at the
    instruction boundary; `trace_capture_opcode_byte(...)` (invoked from
    `fetch_opcode()`) records the M1 opcode-fetch bytes; `trace_end_instruction(...)`
    stamps the instruction/cumulative T-states and emits the record.
  - Non-perturbation guard: every trace helper returns immediately when no
    observer is attached, so `step()` performs no extra work and has zero
    behavioral/state/cycle effect when tracing is off. The observer is invoked
    exactly once per completed `step()`.
- Machine-side sink (`src/machine/`):
  - New `src/machine/cpu_trace_sink.{h,cpp}` defines `CpuTraceSink`, which
    implements `Z80aTraceObserver`, retains records in an in-memory
    `std::vector<Z80aTraceRecord>`, and provides deterministic, diffable text
    serialization (`format_record(...)` for one line; `serialize()` for the
    whole buffer). Formatting is hand-rolled ASCII (no locale/stream state):
    fixed field order, uppercase hex of fixed width, `\n`-terminated lines.
  - `Hbf1xvMachine` owns a `CpuTraceSink` and exposes
    `set_cpu_trace_enabled(bool)` (attach/detach the sink as the CPU observer),
    `cpu_trace_enabled()`, and `cpu_trace()` accessors. Trace is off by default.
- Build: `src/machine/cpu_trace_sink.cpp` added to the `sony_msx_core` target in
  the root `CMakeLists.txt`.

Folder guidance honored (planner DP-6): the CPU hook lives in
`src/devices/cpu/` and the sink lives in `src/machine/`. No new top-level
`src/debug/` folder was introduced.

### Trace record structure / fields (`Z80aTraceRecord`)

Capture conventions (fixed, documented in-header for byte-stable diffs):

- `sequence` — 0-based instruction index since the observer attached.
- `pc` — PRE-execution program counter (address of the instruction).
- `opcode_bytes[4]` + `opcode_length` — the M1 opcode-fetch bytes consumed by
  the instruction (unprefixed opcode plus any CB/ED/DD/FD prefix bytes fetched as
  opcode). Operand/immediate/displacement bytes are excluded. Non-fetching steps
  (HALT idle, interrupt acknowledge) record `opcode_length == 0`.
- Primary 8-bit register file (pre-execution): `a, f, b, c, d, e, h, l`.
- 16-bit register file (pre-execution): `af, bc, de, hl`, shadow
  `af_shadow, bc_shadow, de_shadow, hl_shadow`, `ix, iy, sp`.
- `i, r` — interrupt vector / refresh registers (pre-execution; `r` is the value
  before this instruction's M1 refresh tick).
- `iff1, iff2` (bool), `im` (`InterruptMode`).
- `instr_tstates` — T-state cost of THIS instruction.
- `cumulative_tstates` — running total AFTER this instruction retires.
- Flag accessors derived from `F`: `flag_s/z/y/h/x/pv/n/c()` (S,Z,Y,H,X,P/V,N,C).

Serialized line format (one instruction per line), e.g. the first record of the
S1 test program (`LD A,0x2A`):

```
SEQ=0000 PC=0000 OP=3E A=00 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=0000 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=00 IFF1=0 IFF2=0 IM=1 T=7 TC=7
```

### New tests

- Unit `tests/unit/devices/z80a_trace_export_unit_test.cpp`
  (suite `Devices_Z80ATraceExport_Unit`). Cases:
  - `SetObserver_Attached_ObserverReadable`
  - `Observer_FourInstructions_FourRecords`
  - `Record0_LdAImm_PreStateAndTimingExact`
  - `Record1_LdBImm_PreStateReflectsPriorInstruction`
  - `Record2_AddAB_PreStateExact`
  - `Record3_Halt_PreStateHasAddResultAndDecomposedFlags`
  - `ObserverOff_SameProgram_IdenticalEndStateAndCycles` (non-perturbation)
  - `TwoOnRuns_SameProgram_RecordsBitIdentical` (reproducibility)
  - `DetachObserver_Step_NoNewRecords`
- Integration `tests/integration/machine/hbf1xv_trace_program_integration_test.cpp`
  (suite `Machine_Hbf1xvTraceProgram_Integration`). Cases:
  - `DefaultMachine_TraceDisabled_NoRecords`
  - `TraceDisabled_Step_StillNoRecords`
  - `TraceEnabled_FourInstructions_FourRecords`
  - `TraceEnabled_FinalCumulativeTstates_MatchesMachineClock` (TC == 22 == clock)
  - `FormatRecord_FirstInstruction_MatchesGoldenLine` (golden serialization)
  - `Serialize_FourRecords_FourNewlineTerminatedLines`
  - `TwoTracedRuns_SameProgram_ByteForByteIdentical` (deterministic oracle)
- Both registered in `tests/CMakeLists.txt`.

The unit test asserts each recorded field exactly against a hand-derived
expectation for the fixed program `LD A,0x2A / LD B,0x03 / ADD A,B / HALT`
(including decomposed flags for `ADD A,B` = `F=0x28`, i.e. Y and X set). The
non-perturbation test runs the same program with the observer OFF and ON and
asserts identical final CPU register state, IFF/IM/halt state, and
`total_tstates()`. Byte-for-byte reproducibility is asserted at the record level
(unit) and at the serialized-text level (integration).

### Actual evidence-gate output (captured this cycle)

- `tools/validate-assets.ps1` -> `Asset validation result: True` (7 BIOS files,
  ROM file count: 1). Exit 0.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` -> "Checksum
  report written to: .../docs/asset-checksums.txt". Exit 0.
- `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` -> configure/generate done,
  exit 0.
- `cmake --build build --config Debug` -> BUILD_EXIT=0. New targets linked:
  `devices_z80a_trace_export_unit_test.exe`,
  `machine_hbf1xv_trace_program_integration_test.exe`, plus `sony_msx_core` and
  `sony_msx_headless`.
- `ctest --test-dir build -C Debug --output-on-failure` ->
  `100% tests passed, 0 tests failed out of 23`. CTEST_EXIT=0.
  (Baseline was 21 tests; +2 new S1 suites. New tests #13
  `devices_z80a_trace_export_unit_test` and #16
  `machine_hbf1xv_trace_program_integration_test` both Passed.)
- Determinism: a second `ctest` run reproduced `100% tests passed, 0 tests
  failed out of 23`.

Exact ctest pass/fail count: 23 passed / 0 failed (23 total).

### Residual slices (not in S1 scope — remain open under M10)

- M10-S2 — minimum machine DRAM/SRAM/VRAM region wiring (INERT, dumpable regions
  only; no slot/mapper/VDP/FMPAC behavior).
- M10-S3 — full-state debug dump (CPU + DRAM + SRAM + VRAM) + execution-event
  logging under `debug/traces/` and `debug/logs/` (reuses the S1 record/format).
- M10-S4 — openMSX opcode-trace parity harness producing a genuine captured
  trace-diff (resolves QA blocker R5). Depends on S1 (this facility) + S2.
- M10-S5 — `tools/` converters (memory->png, memory->audio) over dumped buffers.

No S2..S5 behavior (slot/mapper/VDP/SRAM/VRAM/device) was implemented in this
slice, and no openMSX parity was claimed (that is S4).

### Notes / boundaries observed

- No out-of-M10 (DP-1..DP-5) work was required or performed; no STOP-AND-ASK
  blocker was hit for S1.
- "Opcode byte(s)" is defined as the M1 opcode-fetch bytes (prefixes included,
  operands excluded). This is deterministic and reusable; S4 may extend the
  record with operand bytes if the parity harness needs them, without disturbing
  the existing fields.

---

## M10-S2 — Minimum inert DRAM/SRAM/VRAM region wiring (REQ-M10-004)

- Date: 2026-07-06 (Asia/Seoul, +09:00)
- Slice: M10-S2 (per `docs/m10-planner-package.md`, Section 3)
- Author: MSX Developer Agent
- Status: Completed (S2 scope only; green evidence gates)

### What was implemented

The minimum INERT, dumpable/loadable memory regions on the machine layer, sized
EXACTLY to the strict Target Machine Specification
(`agent-protocol/project-baseline.md`). These are PURE STORAGE byte buffers:
addressable, readable, writable, loadable, dumpable, and deterministically
zero-initialized at reset. NO slot/subslot/mapper decoding, NO V9958 VDP
behavior, NO FM-PAC device behavior, and NO I/O bus were implemented — those are
separate milestones (planner DP-1/DP-2/DP-3). The M9 CPU and the M10-S1 trace
facility are unchanged.

- New storage primitive (`src/machine/`):
  - `src/machine/memory_region.h` / `memory_region.cpp` define `MemoryRegion`, a
    fixed-size, zero-initialized `std::vector<std::uint8_t>` byte buffer with
    `size()`, `clear()` (deterministic zero-init), bounds-checked `read(offset)` /
    `write(offset,value)`, bulk `load(offset,bytes,count)` (clamped, no overflow),
    `dump()` (full snapshot copy), and `data()`. Out-of-range reads yield `0x00`
    and out-of-range writes are ignored, so no access can perturb state outside
    the region.
- Machine wiring (`src/machine/hbf1xv_machine.{h,cpp}`):
  - The former flat `std::array<std::uint8_t,65536> memory_` was replaced by a
    `MemoryRegion dram_{kDramBytes}` (64 KB). The CPU-visible bus
    (`MachineBus`) now reads/writes `dram_`, and `load_memory` / `read_memory`
    route to `dram_` — so existing CPU-visible 64K memory behavior is preserved
    exactly (the DRAM region IS the store the CPU sees).
  - Added `MemoryRegion vram_{kVramBytes}` (128 KB) and
    `MemoryRegion sram_{kSramBytes}` (8 KB) as inert regions.
  - `cold_boot()` now `clear()`s all three regions (deterministic zero-init).
  - Public accessors: `dram_size()/vram_size()/sram_size()` and const/non-const
    `dram()/vram()/sram()` returning the `MemoryRegion` (giving uniform
    read/write/load/dump per region).
- Build: `src/machine/memory_region.cpp` added to the `sony_msx_core` target in
  the root `CMakeLists.txt`.

Region sizes (compile-time constants in `Hbf1xvMachine`):

| Region | Constant     | Size    | Source / rationale |
|--------|--------------|---------|--------------------|
| DRAM   | `kDramBytes` | 64 KB   | Spec table: **RAM 64 KB** (strict). |
| VRAM   | `kVramBytes` | 128 KB  | Spec table: **VRAM 128 KB** (strict); V9958 storage only, no VDP behavior. |
| SRAM   | `kSramBytes` | 8 KB    | FM-PAC battery SRAM (see Assumption). |

### SRAM-size Assumption + verification action

- Assumption: the strict spec table lists **no** SRAM capacity (it names the
  sound hardware as "FM-PAC (OPLL YM-2413)" but gives no SRAM size). The standard
  Panasonic FM-PAC carries **8 KB** of battery-backed SRAM, so `kSramBytes` is set
  to 8 KB (`8 * 1024`). This region is INERT storage only; true FM-PAC device
  behavior (mapper, battery persistence semantics) is a separate deferred
  milestone (planner **DP-3**).
- Verification action: confirm the FM-PAC SRAM capacity against the real FM-PAC
  device datasheet / an FM-PAC ROM+SRAM dump when DP-3 (FM-PAC device) is
  implemented; adjust `kSramBytes` if the authoritative capacity differs.

### New tests

- Unit `tests/unit/machine/hbf1xv_memory_regions_unit_test.cpp`
  (suite `Machine_Hbf1xvMemoryRegions_Unit`). Cases include:
  `DramSize_Strict_Is64KiB`, `VramSize_Strict_Is128KiB`,
  `SramSize_FmPacAssumption_Is8KiB` (+ region-accessor size checks);
  `ColdBoot_{Dram,Vram,Sram}_ZeroInitialized`;
  boundary+interior round-trips `Vram_Write{First,Interior,Last}_ReadsBack`,
  `Sram_Write{First,Last}_ReadsBack`, `Dram_Write{First,Last}_ReadsBack`;
  `Sram_OutOfRange_ReadsZeroWriteIgnored`;
  region independence `RegionIndependence_*_First`;
  load/dump round-trip `Vram_LoadThenDump_MatchesPattern`,
  `Vram_DumpReload_ByteIdentical`; `Load_PastEnd_ClampsDeterministically`,
  `Load_PastEnd_SizeUnchanged`; and cold-boot re-zero
  `ColdBootAgain_{Dram,Vram,Sram}_ReZeroed`.
- Integration `tests/integration/machine/hbf1xv_memory_regions_integration_test.cpp`
  (suite `Machine_Hbf1xvMemoryRegions_Integration`). Cases:
  `LoadMemory_DramRegionAndReadMemory_Coherent` (CPU-visible DRAM alias
  coherence), `RunProgram_ReachesHalt`,
  `RunProgram_{Sram,Vram}Region_Unperturbed` (CPU execution does not touch inert
  regions), `Vram_LoadDumpThroughMachine_Matches`, `Vram_ReloadDump_ByteIdentical`
  (write pattern -> dump -> reload -> identical), and determinism
  `TwoMachines_SameSramLoad_ByteIdenticalDump`,
  `TwoMachines_ColdBootDram_ByteIdenticalDump`.
- Both registered in `tests/CMakeLists.txt`.

### Actual evidence-gate output (captured this cycle)

- `tools/validate-assets.ps1` -> `Asset validation result: True` (7 BIOS files:
  f1xvbios/f1xvdisk/f1xvext/f1xvfirm/f1xvkdr/f1xvkfn/f1xvmus.rom; ROM file
  count: 1 = aleste.rom). VALIDATE_EXIT=0.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` -> "Checksum
  report written to: .../docs/asset-checksums.txt". CHECKSUM_EXIT=0.
- `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` -> configure/generate done,
  CONFIGURE_EXIT=0.
- `cmake --build build --config Debug` -> BUILD_EXIT=0. New targets linked:
  `machine_hbf1xv_memory_regions_unit_test.exe`,
  `machine_hbf1xv_memory_regions_integration_test.exe`, plus `sony_msx_core` and
  `sony_msx_headless`. (Only benign MSVC C4819 codepage-949 warnings on ASCII
  headers; no errors.)
- `ctest --test-dir build -C Debug --output-on-failure` ->
  `100% tests passed, 0 tests failed out of 25`. CTEST_EXIT=0.
  (Baseline was 23 tests after S1; +2 new S2 suites: #14
  `machine_hbf1xv_memory_regions_unit_test` and #16
  `machine_hbf1xv_memory_regions_integration_test`, both Passed. All prior M9
  CPU + M10-S1 trace suites remain green and unchanged.)
- Determinism: a second `ctest` run reproduced `100% tests passed, 0 tests
  failed out of 25`.

Exact ctest pass/fail count: 25 passed / 0 failed (25 total).

### Residual slices (not in S2 scope — remain open under M10)

- M10-S3 — full-state debug dump (CPU + DRAM + SRAM + VRAM) + execution-event
  logging under `debug/traces/` and `debug/logs/` (reuses the S1 record/format
  and these S2 regions).
- M10-S4 — openMSX opcode-trace parity harness producing a genuine captured
  trace-diff (resolves QA blocker R5). Depends on S1 + S2.
- M10-S5 — `tools/` converters (memory->png, memory->audio) over dumped buffers.

### Notes / boundaries observed

- No out-of-M10 (DP-1..DP-5) work was required or performed; no STOP-AND-ASK
  blocker was hit for S2. The regions are inert storage only, exactly as the
  planner package scopes S2.
- CPU-visible 64K memory behavior, the M9 CPU, and the M10-S1 trace facility are
  unchanged (all prior suites green).

---

## M10-S3 — Full-state debug dump + execution-event logging (REQ-M10-005)

- Date: 2026-07-06 (Asia/Seoul, +09:00)
- Slice: M10-S3 (per `docs/m10-planner-package.md`, Section 3)
- Author: MSX Developer Agent
- Status: Completed (S3 scope only; green evidence gates)

### What was implemented

A deterministic full-state debug dump (CPU + DRAM + SRAM + VRAM) and a
deterministic execution-event log, both written under `debug/` via C++17
`<filesystem>`. Reuses the M10-S1 trace record/sink and the M10-S2 memory
regions; no M9 CPU semantics/timing/state and no S1/S2 behavior were changed
(all prior suites remain green). No device behavior (slot/mapper/VDP/FM-PAC,
DP-1..DP-5) was added.

- Shared formatter (`src/machine/debug_format.h`, new): inline, locale-independent
  ASCII primitives `to_hex` (fixed-width uppercase, big-endian), `to_dec`, and
  `flag_string(f)` (decomposed S,Z,Y,H,X,P/V,N,C). `cpu_trace_sink.cpp` (S1) was
  refactored to consume these shared helpers instead of its private copies —
  genuine reuse; the S1 serialized output is byte-unchanged (verified: the S1
  golden test `FormatRecord_FirstInstruction_MatchesGoldenLine` still passes).
- Full-state dump serializer (`src/machine/debug_dump.{h,cpp}`, new):
  `debug_dump::serialize_cpu(state)` emits a fixed multi-line `[CPU]` block over
  the complete Z80aState (PC, SP, AF/BC/DE/HL, shadow AF'/BC'/DE'/HL', IX, IY,
  A/F+decomposed flags/B/C/D/E/H/L, I, R, IFF1, IFF2, IM, HALT, cumulative
  T-states). `debug_dump::serialize_region(name, data, size)` emits a canonical
  folded hex dump (16 bytes/line, 8-hex-digit offset; runs of identical interior
  lines fold to a single `*`; the first and last line of every region are always
  emitted verbatim), so 64 KB DRAM / 8 KB SRAM / 128 KB VRAM stay compact while
  remaining complete and unambiguous.
- Execution-event log (`src/machine/debug_event_log.{h,cpp}`, new):
  `DebugEventLog` collects `DebugEvent{sequence, tstate, type, detail}` where
  `type` is `Reset | InstructionRetire | Halt | Dump`. `serialize()` emits one
  `\n`-terminated line per event: `EVT=<hex4 seq> T=<dec tstate> TYPE=<name>
  [detail]`. `tstate` is the deterministic scheduler clock (`elapsed_cycles()`),
  never wall-clock.
- Machine wiring (`src/machine/hbf1xv_machine.{h,cpp}`): the machine owns a
  `DebugEventLog`, an `event_logging_enabled_` flag (off by default), and a
  configurable `debug_root_` (`std::filesystem::path`, default `"debug"`). New
  API: `serialize_state_dump()` (pure/const; CPU + DRAM + SRAM + VRAM), 
  `write_state_dump(name)`, `write_cpu_trace(name)` (flushes the S1 trace),
  `write_event_log(name)`, `set_/event_logging_enabled`, `debug_event_log()`,
  `set_/debug_root`. `cold_boot()` clears the event log and records a `Reset`
  event when logging is enabled; `step_cpu_instruction()` records one
  `InstructionRetire` (with pre-PC, opcode byte, instruction T-states) and, on
  the step that first enters HALT, a `Halt` event; `write_state_dump()` records
  a `Dump` event. The private `write_text_file()` creates directories with
  `create_directories` and writes in binary mode so `\n` is byte-exact on disk.

Folder guidance honored (planner DP-6): all new code lives in `src/machine/`
(CPU state is read via the existing `Z80aCpu::state()` accessor); no new
top-level `src/debug/` folder was introduced.

### Dump/log format and file layout under `debug/`

Output-file layout (created on demand under `debug_root_`, default `debug/`):

| Path | Producer | Content |
|------|----------|---------|
| `debug/traces/<name>` | `write_state_dump` | Full-state dump (CPU + DRAM + SRAM + VRAM). |
| `debug/traces/<name>` | `write_cpu_trace` | Flushed S1 per-instruction CPU trace. |
| `debug/logs/<name>`   | `write_event_log` | Execution-event log. |

Full-state dump layout (for the fixed program `LD A,0x2A / LD B,0x03 / ADD A,B /
HALT` after 4 steps):

```
HBF1XV-STATE-DUMP v1
[CPU]
PC=0006 SP=FFFF
AF=2D28 BC=0300 DE=0000 HL=0000
AF'=0000 BC'=0000 DE'=0000 HL'=0000
IX=0000 IY=0000
A=2D F=28[..Y.X...] B=03 C=00 D=00 E=00 H=00 L=00
I=00 R=04 IFF1=0 IFF2=0 IM=1 HALT=1
TSTATES=22
[DRAM] size=65536
00000000 3E 2A 06 03 80 76 00 00 00 00 00 00 00 00 00 00
00000010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
*
0000FFF0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[SRAM] size=8192
00000000 00 ... (16 bytes) ...
*
00001FF0 00 ...
[VRAM] size=131072
00000000 00 ...
*
0001FFF0 00 ...
[END]
```

Event-log layout (same program, logging enabled before `cold_boot`):

```
EVT=0000 T=0 TYPE=RESET cold_boot
EVT=0001 T=7 TYPE=INSTR PC=0000 OP=3E T=7
EVT=0002 T=14 TYPE=INSTR PC=0002 OP=06 T=7
EVT=0003 T=18 TYPE=INSTR PC=0004 OP=80 T=4
EVT=0004 T=22 TYPE=INSTR PC=0005 OP=76 T=4
EVT=0005 T=22 TYPE=HALT PC=0005
```

### How determinism is guaranteed

- All serialization is hand-rolled ASCII: fixed field order, fixed-width
  uppercase hex, base-10 decimals, `\n`-only line endings, no locale/stream
  state, no floating point.
- No wall-clock anywhere: event `T=` stamps come from the deterministic
  scheduler clock (`elapsed_cycles()`); `sequence` is a monotonic counter. The
  `Date.now()`-style nondeterminism forbidden by `tests/CLAUDE.md` is absent by
  construction.
- Files are written in binary mode (no CRLF translation), so on-disk bytes equal
  the in-memory serialization exactly and are byte-stable across runs.
- The region hex-fold algorithm is deterministic (first/last line always
  verbatim; identical interior lines fold to one `*`).
- Tests assert byte-identical output across two identical runs at both the
  in-memory-serialization level and the on-disk-file level.

### New tests

- Unit `tests/unit/machine/hbf1xv_debug_dump_unit_test.cpp`
  (suite `Machine_Hbf1xvDebugDump_Unit`). Cases:
  - `Dump_FirstLine_IsVersionTag`
  - `Dump_CpuSection_ExactGolden` (full CPU block: PC/SP/AF/BC/DE/HL/shadows/
    IX/IY/A/F+flags/B..L/I/R/IFF/IM/HALT/TSTATES exact)
  - `Dump_DramRegion_FoldedHexExact`
  - `Dump_SramRegion_FoldedHexExact`
  - `Dump_VramRegion_FoldedHexExactThenEnd`
  - `TwoRuns_SameProgram_ByteIdenticalDump` (reproducibility oracle)
  - `SerializeDump_Twice_NonPerturbingAndStable` (dumping does not change PC, A,
    R, HALT, or `elapsed_cycles`; two dumps identical)
  - `WriteStateDump_FileMatchesSerializationAndRerunnable` (creates `traces/`,
    on-disk == serialization, re-run byte-identical; hermetic temp root)
- Integration `tests/integration/machine/hbf1xv_debug_event_log_integration_test.cpp`
  (suite `Machine_Hbf1xvDebugEventLog_Integration`). Cases:
  - `LoggingOff_Step_NoEvents`
  - `EnabledLogging_KnownProgram_SixEvents`
  - `EventLog_KnownProgram_MatchesGolden` (committed golden oracle)
  - `TwoRuns_SameProgram_ByteIdenticalLog` (reproducibility oracle)
  - `WriteStateDump_LogsDumpEvent_AndFlushesLogToDisk` (Dump event appended;
    on-disk log == serialization; hermetic temp root)
  - `ColdBoot_ReArmsFreshEventStream`
- Both registered in `tests/CMakeLists.txt`.

Non-perturbation is asserted directly: after running the program, two calls to
`serialize_state_dump()` leave PC/A/R/HALT and `elapsed_cycles()` unchanged and
produce identical output; the file-writing path only appends to the event log
(not machine state). Tests are hermetic — they use fixed names under a
temp-directory root, overwrite on re-run, and clean up with `remove_all`.

### Actual evidence-gate output (captured this cycle)

- `tools/validate-assets.ps1` -> `Asset validation result: True` (7 BIOS files:
  f1xvbios/f1xvdisk/f1xvext/f1xvfirm/f1xvkdr/f1xvkfn/f1xvmus.rom; ROM file
  count: 1 = aleste.rom). VALIDATE_EXIT=0.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` -> "Checksum
  report written to: .../docs/asset-checksums.txt". CHECKSUM_EXIT=0.
- `cmake --build build --config Debug` -> BUILD_EXIT=0. New targets linked:
  `machine_hbf1xv_debug_dump_unit_test.exe`,
  `machine_hbf1xv_debug_event_log_integration_test.exe`, plus `sony_msx_core`
  and `sony_msx_headless`. (Only benign MSVC C4819 codepage-949 warnings on
  ASCII headers; no errors.)
- `ctest --test-dir build -C Debug --output-on-failure` ->
  `100% tests passed, 0 tests failed out of 27`. CTEST_EXIT=0.
  (Baseline was 25 tests after S2; +2 new S3 suites: #15
  `machine_hbf1xv_debug_dump_unit_test` and #17
  `machine_hbf1xv_debug_event_log_integration_test`, both Passed. All prior M9
  CPU + M10-S1 trace + M10-S2 region suites remain green — the S1 refactor to the
  shared formatter is output-identical.)
- Determinism: a second `ctest` run reproduced `100% tests passed, 0 tests
  failed out of 27`.

Exact ctest pass/fail count: 27 passed / 0 failed (27 total).

### Residual slices (not in S3 scope — remain open under M10)

- M10-S4 — openMSX opcode-trace parity harness producing a genuine captured
  trace-diff (resolves QA blocker R5). Depends on S1 (trace-export) + S2
  (comparable run). This slice writes the emulator's own trace/dump/log; it does
  NOT perform or claim any openMSX parity.
- M10-S5 — `tools/` converters (memory->png, memory->audio) over dumped buffers.

### Notes / boundaries observed

- No out-of-M10 (DP-1..DP-5) work was required or performed; no STOP-AND-ASK
  blocker was hit for S3. The dump reads the INERT S2 regions as-is (VRAM does
  not reflect real V9958 rendering; SRAM does not reflect real FM-PAC behavior).
- The `[CPU]` full-state block is a distinct multi-line format from the S1
  single-line per-instruction trace record; both now share the same underlying
  hex/flag formatters (`debug_format.h`) for genuine reuse.
- Dump/log files are written on demand only; the repository `debug/traces/` and
  `debug/logs/` directories remain the durable output location (auto-created if
  absent). Tests use hermetic temp roots to avoid polluting the repo.

---

## M10-S4 — openMSX opcode-trace parity harness (resolves R5) — 2026-07-06

- Source Request: REQ-M10-006
- Scope: the REAL per-instruction architectural-state parity harness between this
  emulator and openMSX 19.1 (WSL `/usr/bin/openmsx`) over a committed RAM-only,
  I/O-free Z80 program. Resolves QA blocker R5. No out-of-M10 (DP-1..DP-5) work.

### Honest R5 status: RESOLVED with a real captured architectural-parity diff

- Result: **ARCHITECTURAL PARITY — EMPTY DIFF** over all **48** instructions from
  base `0xC000` to the `HALT` checkpoint at PC `0xC055`. Every architectural field
  matches on every instruction: PC, opcode byte(s), A, F (+ all decomposed flag
  bits S,Z,Y,H,X,P/V,N,C), B, C, D, E, H, L, AF, BC, DE, HL, AF', BC', DE', HL',
  IX, IY, SP, I, R, IFF1, IFF2, IM.
- Authoritative artifact: `docs/m10-parity-trace-diff.md` (generated by the harness,
  not hand-written). Raw captured traces: `build/parity_trace_A.txt` (emulator),
  `build/parity_trace_B.txt` (openMSX). This is a genuine captured diff — NOT an
  availability/capability probe.
- The openMSX-side trace is deterministic: two consecutive full harness runs
  produced byte-identical `build/parity_trace_B.txt` (verified via `diff`).

### How the documented R5 blocker was overcome (not routed around dishonestly)

The M9 QA blocker had two concrete causes, both genuinely resolved:

1. "openMSX `step` does not advance PC in the headless `-script` startup context."
   Cause: `step` is asynchronous and only advances when the emulation resumes from
   a break; the old probe called it in the un-broken startup context. Fix: the
   harness boots the machine (`after time 6`, emulated seconds), then drives
   single-stepping through chained break-callbacks — `debug break`, then each
   `after break` handler records the pre-execution register snapshot and issues the
   next `step`. PC then advances correctly (empirically verified: C000 -> C001 ->
   C003 -> C005 ... -> C055 HALT).
2. "Flat write to 0xC000 reads back 0xFF (unmapped at reset)." Cause: that probe ran
   before the BIOS configured the slots. Fix: the parity program is based at
   `0xC000` (MSX page 3 = main RAM after boot on C-BIOS_MSX2+). After the boot wait,
   the harness writes the program bytes into page-3 RAM and confirms the readback
   (`OMSETUP ram_base_readback=F3`, the program's first opcode) before stepping.

### Identical initial state (both emulators)

openMSX's full CPU vector is forced via `reg` to match this emulator's `cold_boot`
reset state exactly: AF=BC=DE=HL=AF'=BC'=DE'=HL'=IX=IY=0000, SP=FFFF, PC=C000,
I=00, R=00, IM=1, IFF1=IFF2=0. All 16 register names (incl. shadows af2/bc2/de2/hl2,
and `im`/`iff`) are settable in openMSX 19.1 — verified. Because R starts identical
and this emulator increments R per M1 fetch (bit-7 preserved, +2 for prefixed
opcodes) exactly as openMSX does, R matches on every instruction in the captured
diff (e.g. seq 1 ED56 `IM 1`: R 01->03 on both).

### Parity program (committed)

- `tests/parity/z80_parity_checkpoint.bin` (95 bytes, SHA-256
  b58ccb3e84e94a20d771a0dce171bc1426fbe50a5f93505c6afbe54cdca732db) +
  `tests/parity/z80_parity_checkpoint.md` (documented listing, base, initial
  vector, checkpoint, family coverage).
- RAM-only, I/O-free, no BIOS calls, no interrupts (`DI` first), no self-modifying
  code, everything executes/reads/writes strictly inside page 3 (0xC000-0xFFFF) so
  the two machines cannot diverge on fetched bytes (page 0-2 is C-BIOS ROM on
  openMSX vs zero-RAM here and is never touched). Every RAM byte the program READS
  is seeded by the program itself, so power-on/BIOS-leftover RAM is irrelevant.
- Family coverage: unprefixed 8-bit load/ALU (ADD/SUB/AND/OR/XOR/CP/INC/DEC),
  RRCA, 16-bit loads; CB (BIT/RLC/SRL, register operands); ED (`IM 1`, `LDI`);
  DD/FD indexed (LD IX/IY,nn; LD A,(IX+d); INC (IX+d); ADD A,(IX+d); LD (IY+d),n;
  LD A,(IY+d)); control flow CALL/RET (stack), DJNZ loop, JR, HALT.
- Deliberate design choices to keep single-step alignment unambiguous and avoid
  known emulator-divergence traps: the only ED block op is the NON-repeating `LDI`
  (BC=1) — block-REPEAT ops (LDIR/CPIR/...) re-fetch their opcode per iteration,
  which would make cross-engine step alignment ambiguous; `BIT b,r` uses a register
  operand (X/Y flags copy bits 3/5 of the tested register — well defined on both)
  rather than `BIT b,(HL)` (whose X/Y come from the internal WZ/MEMPTR high byte,
  a documented divergence area). DAA is not used.

### Cycle / T-state divergence (reported separately — NOT an R5 gate)

Per planner Section 4 / DP-4, exact cross-emulator T-state parity is structurally
out of M10 scope: openMSX inserts MSX M1 wait-states while this emulator's M9 core
uses canonical Z80 T-states. The harness therefore compares only architectural
state for the R5 gate and reports the cycle field separately. The emulator-side
canonical Z80 T-states are recorded in trace A (T=/TC= columns; final cumulative
T-states = 435). No openMSX per-instruction T-state counter is read; this is a
declared, documented handling — not a hidden failure.

### Harness & regression lock (files)

- `tools/openmsx-trace-parity.ps1` — orchestrator: runs the emulator
  `--parity-trace` mode for trace A, drives openMSX for trace B, invokes the diff.
- `tools/trace-diff.py` — deterministic normalizer/diff; exit 0 = empty
  architectural diff, 1 = divergence, 2 = not comparable (blocked). Writes the diff
  artifact.
- `src/main.cpp` — added a `--parity-trace <bin> <base_hex> <max_steps> <out>` mode
  (default headless scaffold behavior unchanged) that loads the flat program from
  the cold_boot reset vector, sets PC=base, enables the S1 trace-export, steps to
  HALT, and writes the exact CpuTraceSink text.
- `tests/integration/machine/hbf1xv_parity_checkpoint_integration_test.cpp`
  (suite `Machine_Hbf1xvParityCheckpoint_Integration`) — regression-locks the
  emulator half: embeds the 95 program bytes, runs to HALT, asserts 48
  instructions, HALT at 0xC055 (opcode 0x76), cumulative T=435, and a byte-exact
  committed golden serialization, plus two-run determinism. Environment-independent
  (does not require WSL/openMSX), so CI stays deterministic.

### Actual evidence-gate output (this cycle)

- `tools/validate-assets.ps1` = Asset validation result **True** (7 BIOS files,
  ROM count 1), exit 0.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` = report written,
  exit 0.
- `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` = configure/generate done, exit 0.
- `cmake --build build --config Debug` = BUILD_EXIT 0 (new parity test executable +
  updated `sony_msx_headless` linked; only benign MSVC C4819 codepage-949 warnings,
  no errors).
- `ctest --test-dir build -C Debug --output-on-failure` = **100% tests passed,
  0 failed out of 28** (up from 27 baseline; new #25
  `machine_hbf1xv_parity_checkpoint_integration_test` Passed; all prior M9 CPU +
  M10-S1/S2/S3 suites remain green). Exact count: **28 passed / 0 failed**.
- `tools/openmsx-trace-parity.ps1` = `R5 RESULT: ARCHITECTURAL PARITY (empty diff)`;
  48 instructions captured on both sides; trace B reproducible across two runs.
- Independent cross-check (separate parsing logic from `trace-diff.py`): 0
  mismatches across all 48 instructions incl. the final HALT state.

### Boundaries observed

- No STOP-AND-ASK blocker was hit: openMSX WAS drivable to single-step the RAM-only
  program to the checkpoint (real, documented mechanism above), and faithful
  architectural parity did NOT require real BIOS boot, V9958 VDP, or slot/subslot
  decoding (DP-1/DP-2) — the program is confined to page-3 RAM present on both
  machines. Only the cycle field is DP-4-bounded and is reported separately, never
  used to fail R5.
- M10-S5 (`tools/` memory->png / memory->audio converters) remains the only open
  M10 slice after S4.

---

## M10-S5 — `tools/` memory converters (memory->png / memory->audio) — 2026-07-06

- Source Request: REQ-M10-007
- Slice: M10-S5 (per `docs/m10-planner-package.md`, Section 3) — the FINAL M10
  implementation slice.
- Author: MSX Developer Agent
- Status: Completed (S5 scope only; green evidence gates). Tooling-only slice: no
  C++/`src/` change, no device behavior. No out-of-M10 (DP-1..DP-5) work; no
  STOP-AND-ASK blocker hit.

### What was implemented

Two deterministic, stdlib-only Python converters in `tools/` that turn a dumped MSX
memory buffer into an inspectable artifact. Both are INERT raw-buffer
visualizers/serializers — they treat source bytes directly and model NO device:

- `tools/mem-to-png.py` — memory buffer -> 8-bit grayscale PNG. One source byte
  becomes one grayscale pixel (pixel value == byte value). `--width` px/row
  (default 256); height = `ceil(len/width)`; the final scanline is padded with
  `--pad` (default `0x00`). This is a raw byte picture only; it does NOT perform
  real V9958 rendering, tile/sprite decoding, or palette lookup (planner DP-2).
- `tools/mem-to-audio.py` — memory buffer -> canonical PCM WAV. Source bytes are
  interpreted directly as PCM samples: mono, 8-bit unsigned, 8000 Hz by default;
  `--bits 16` reads little-endian signed 16-bit samples (odd trailing byte padded),
  `--rate`/`--channels` configurable. No gain/filter/resample; it does NOT
  synthesize PSG (AY-3-8910) or YM2413/OPLL audio (separate audio-device milestone).

Both are self-contained standalone scripts (matching the existing `tools/`
convention, e.g. `trace-diff.py`), each with a hermetic `--self-check` mode.

### Input/output contracts

Input (either converter accepts one of two forms):

| Form | Selector | Source | Handling |
|------|----------|--------|----------|
| Raw binary buffer | default (positional `<input>`) | a file of raw bytes — a region extracted from a dump, or any committed memory image (e.g. `tests/parity/z80_parity_checkpoint.bin`) | bytes used verbatim |
| M10-S3 region dump | `--region NAME` | the full-state dump text from `src/machine/debug_dump.cpp` (`write_state_dump`, tag `HBF1XV-STATE-DUMP v1`) | locates `[NAME] size=<n>`, parses the folded canonical hex, and losslessly EXPANDS the `*` fold marker (repeat previous printed 16-byte line until the next printed offset) back to the full `size` bytes |

The S3 de-fold exactly reverses `serialize_region()` (16 bytes/line, 8-hex-digit
offset, `*` = identical-interior-line fold, first+last line always verbatim), so a
128 KB VRAM region dumped compactly re-expands to the full 131072 bytes.

Output:

- PNG: PNG signature + `IHDR` (width, height, bit depth 8, color type 0 grayscale)
  + single `IDAT` + `IEND`. No ancillary chunks.
- WAV: 44-byte canonical RIFF/WAVE header (`RIFF`/`fmt `(16, PCM=1)/`data`) + raw
  sample bytes. No ancillary chunks.

### Determinism approach

Identical input -> byte-identical output on ANY platform (not merely within one
machine):

- PNG `IDAT` is emitted as DEFLATE **stored (uncompressed) blocks**, hand-assembled
  here rather than via `zlib.compress()`. Stored blocks are byte-identical across
  every zlib build/version, removing the usual compression-level/version
  nondeterminism from committed golden hashes. `zlib` is used ONLY for the
  fixed-algorithm CRC-32 (chunk checksums) and Adler-32 (zlib trailer).
- NO metadata chunks that could embed environment/time: no `tIME`, `tEXt`, `pHYs`,
  or `gAMA` in the PNG; no `LIST`/`INFO`/`fact`/`cue`/software tag in the WAV. WAV
  natively carries no creation timestamp.
- All headers are hand-assembled with `struct` in fixed field order (PNG big-endian,
  RIFF little-endian per spec); ASCII-only; no locale/float dependence.
- Verified: two consecutive encodes of the same buffer are byte-identical (the
  raw-path sample below produced the same SHA-256 on both runs), and each
  `--self-check` asserts a fixed golden SHA-256.

### Dependency choices

- **Python standard library only**: `struct`, `zlib` (CRC/Adler only), `hashlib`,
  `argparse`, `tempfile`. No Pillow/NumPy/wave or any third-party or nondeterministic
  dependency. This keeps the converters deterministic and dependency-free, matching
  the existing `tools/` Python style (`trace-diff.py`). Python 3.14.4 was used this
  cycle; the code targets 3.x stdlib.
- Rationale for stored-DEFLATE over `zlib.compress`: guarantees a version-stable
  golden hash so the self-check is a durable regression lock, not an
  environment-coupled one.

### Tests / self-checks (deterministic, hermetic, re-runnable)

Each converter carries an embedded `--self-check` that:

- encodes a FIXED 256-byte ramp (`bytes(range(256))`) and asserts a hardcoded golden
  SHA-256 of the produced artifact;
- asserts format invariants (PNG: signature, IHDR width/height/depth/colortype,
  trailing `IEND`; WAV: `RIFF`/`WAVE`/`fmt `/`data` tags, PCM format, channels, rate,
  bits, block-align, byte-rate, data size, total byte-length, first/last sample);
- asserts two encodes are byte-identical (reproducibility);
- round-trips the S3 folded-hex region parser against a synthetic fixture that
  mirrors `debug_dump.cpp serialize_region()` INCLUDING a `*` fold, and asserts the
  reconstructed bytes exactly.

These are `tools/` self-checks (per planner S5: converters are `tools/` scripts;
C++ ctest coverage is not required, determinism is demonstrated). They write to
tempfiles / stdout only and commit nothing.

Actual `--self-check` output (captured this cycle):

- `python tools/mem-to-png.py --self-check` -> all checks `PASS`; `SELF-CHECK: OK`;
  exit 0. Golden PNG SHA-256
  `0b51d6574f1b17aefbdba223f42505fa865f6fbfd29e1c639974616f393e0968`, 340 bytes.
- `python tools/mem-to-audio.py --self-check` -> all checks `PASS`;
  `SELF-CHECK: OK`; exit 0. Golden WAV SHA-256
  `360911aa5e193cf2adcc660b619ee6faa0d72cae0f36a41dc539fb88be63bef9`, 300 bytes.

### ACTUAL sample-run output (produced this cycle — not fabricated)

Raw-path on the genuine committed memory buffer
`tests/parity/z80_parity_checkpoint.bin` (95 bytes):

| Converter | Input bytes | Output | Dimensions / duration | Output bytes | SHA-256 |
|-----------|-------------|--------|-----------------------|--------------|---------|
| `mem-to-png.py --width 16` | 95 | PNG grayscale 8-bit | 16 x 6 px | 170 | `c36b503f96b2a15c9b0ceb6e2078f6f906dd98d6e3874070f19400af98c2d2c6` |
| `mem-to-audio.py` (defaults) | 95 | WAV PCM 8-bit mono 8000 Hz | 95 frames / 0.011875 s | 139 | `5b4913ecef271787329bbd00732f56561ff7c2a929b7f355f84001b2cbd820f3` |

- Determinism confirmed: the converter was run twice on the same input; `cmp`
  reported the two PNGs identical and the two WAVs identical (same SHA-256 on both
  runs).
- PNG validity independently confirmed with stdlib: signature OK, IHDR = 16 x 6,
  depth 8, color type 0; the concatenated `IDAT` inflated to 102 bytes ==
  `(width+1) * height` = `(16+1) * 6` (filter byte + 16 pixels per scanline).

S3 dump-format region-parse path on a full-state-dump fixture (documented S3 format,
128 KB VRAM region stored folded with `*`):

- `mem-to-png.py <dump> --region VRAM -o vram.png` -> de-folded to 131072 bytes ->
  PNG 256 x 512, 131662 bytes, SHA-256
  `71ceb892bd2aabd940d6433d37811a11144f3829fb2495e39eb1f4ca605f00c0`. De-fold
  verified: VRAM length 131072, first 16 bytes `0102...10`, interior byte at 0x100 =
  `0x01` (correct `*` repeat of the previous line), last byte `0xFF`.
- `mem-to-audio.py <dump> --region DRAM -o dram.wav` -> 64-byte region -> WAV 108
  bytes, 64 frames / 0.008 s.

### Evidence-gate output (captured this cycle)

- `tools/validate-assets.ps1` -> `Asset validation result: True` (7 BIOS files:
  f1xvbios/f1xvdisk/f1xvext/f1xvfirm/f1xvkdr/f1xvkfn/f1xvmus.rom; ROM file count: 1
  = aleste.rom). VALIDATE_EXIT=0.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` -> "Checksum report
  written to: .../docs/asset-checksums.txt". CHECKSUM_EXIT=0.
- `cmake --build build --config Debug` -> BUILD_EXIT=0 (tooling-only slice; no new
  C++ targets; all existing targets relink cleanly, only benign MSVC C4819
  codepage-949 warnings, no errors).
- `ctest --test-dir build -C Debug --output-on-failure` -> `100% tests passed, 0
  tests failed out of 28`. CTEST_EXIT=0. All prior M9 CPU + M10-S1/S2/S3/S4 suites
  remain green (unchanged from the S4 baseline of 28 — S5 adds no C++ test target,
  per planner S5). Exact ctest count: **28 passed / 0 failed (28 total)**.

### M10 completion status

M10 implementation slices are complete: **S1** (deterministic CPU trace-export),
**S2** (minimum inert DRAM/SRAM/VRAM regions), **S3** (full-state debug dump +
execution-event log), **S4** (openMSX opcode-trace parity harness — **R5 RESOLVED**
with a genuine captured architectural-parity EMPTY DIFF over all 48 instructions to
the HALT checkpoint, per `docs/m10-parity-trace-diff.md` and the S4 section above),
and now **S5** (these `tools/` memory->png / memory->audio converters). All M10
implementation slices S1..S5 are complete with R5 resolved per S4's captured parity
diff. QA sign-off for M10 milestone closure is owned by QA (not this slice).

### Boundaries observed

- No `src/` or C++ change; converters are `tools/` scripts operating on dumped byte
  buffers only. They read the INERT S2 regions/S3 dump as-is — a VRAM PNG is a raw
  byte picture, NOT real V9958 output; a memory WAV is raw bytes as PCM, NOT
  synthesized PSG/OPLL audio (DP-2 and the audio-device milestone remain separate).
- Determinism is guaranteed by construction (stored-DEFLATE PNG, metadata-free WAV,
  stdlib-only), demonstrated via golden-hash self-checks and a two-run byte-identical
  sample. Windows/Python 3.14.4 this cycle; encoders are platform-independent by
  construction (stored blocks + fixed-algorithm CRC/Adler).
