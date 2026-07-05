# M11 Planner Package — S1985 "MSX-ENGINE" Chipset & Full System Bus

- Milestone ID: M11
- Title: S1985 "MSX-ENGINE" Chipset & Full System Bus
- Spec Owner: MSX Planner Agent
- Request: REQ-M11-002
- Authority: DEC-0002 (`agent-protocol/channels/decisions.md`); ledger entry M11
  (`agent-protocol/state/milestones.md`); `agent-protocol/state/current-phase.md`.
- Grounding: `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` (authoritative spec);
  openMSX 21.0 behavior reference (read-only, GPL — never copied into `src/`):
  `references/openmsx-21.0/src/MSXS1985.cc` / `.hh`,
  `references/openmsx-21.0/src/cpu/MSXCPUInterface.cc` / `.hh`,
  `references/openmsx-21.0/src/memory/MSXMapperIO.cc` / `.hh`,
  `references/openmsx-21.0/src/MSXPPI.cc` / `.hh`,
  `references/openmsx-21.0/src/MSXSwitchedDevice.cc` / `.hh`,
  `references/openmsx-21.0/src/DeviceFactory.cc`.

This package is a planning artifact only. It contains NO production code. All C++ signatures below
are **proposed contracts** for the developer slice; they are illustrative, not implementations.

---

## 1. Scope and Assumptions

### 1.1 In scope (DEC-0002 / ledger M11)

- **(a) Full memory bus** — replace the trivial flat-DRAM `Hbf1xvMachine::MachineBus`
  (`src/machine/hbf1xv_machine.cpp:17-26`) with a real slot-decoded fabric:
  - 4-page **primary-slot** decode driven by PPI port `#A8` (2 bits per 16 KB page), per S1985
    fact-sheet §3 and openMSX `MSXCPUInterface.cc:724-747`.
  - **secondary/sub-slot** decode via memory-mapped `#FFFF` (RSEL), decoded only when the page-3
    primary slot is expanded, per fact-sheet §3 and openMSX `MSXCPUInterface.cc:209-220,769-770`.
- **(b) I/O bus** — per-port device dispatch over the 256-port MSX I/O space, plus the S1985
  straight-alias mirrors `#98-#9B`→`#9C-#9F` (fact-sheet §7) and `#A8-#AB`→`#AC-#AF`
  (fact-sheet §3, §10). Documented floating-bus value for unmapped I/O reads.
- **(c) Thin S1985 layer** (fact-sheet §10 "practical takeaway" + §4, §6, §8):
  - 16-byte battery-backed **backup RAM** on the switched-I/O device **ID `0xFE`**, with the
    address / data / rotating-`pattern` / `color1` / `color2` register semantics transcribed in
    fact-sheet §6 and confirmed in `MSXS1985.cc:44-91`.
  - **mapper readback** base `0x80`, mask `0x1F` → the `100xxxxx` pattern (fact-sheet §4;
    `MSXS1985.cc:31-34`, `MSXMapperIO`).
  - **+1 M1 opcode-fetch wait state** (fact-sheet §8; each ED/CB/DD/FD prefix adds one more).
- **(d) System integration** — wire the CPU/scheduler onto the new bus; the existing inert 64 KB
  `dram_` region (`src/machine/hbf1xv_machine.h:136`) becomes the **slot 3-0** backing (fact-sheet
  §9; openMSX HB-F1XV layout).

### 1.2 Out of scope — SEAMS only (named explicitly)

M11 defines the interface each future milestone plugs into; it does **not** implement these:

| Deferred subsystem | M11 provides (the seam) | Owning milestone |
|---|---|---|
| PSG / YM2149 internals (ports `#A0-#A2`) | `IoDevice` port-registration hook on the I/O bus; ports unmapped → open-bus | later (baseline lists PSG separately) |
| RTC / RP5C01 internals (ports `#B4/#B5`) | `IoDevice` registration hook; unmapped → open-bus | later (baseline lists RTC separately) |
| Mapper **RAM** device + segment→bank mapping | `MapperIo` holds the 4 segment registers + `100xxxxx` readback; a `MemoryDevice` slot backing consumes the segment (not yet wired to real banks) | **M12** |
| ROM devices (BIOS/SUB/KANJI/DISK) | `MemoryDevice` interface + slot/sub-slot population API on `SlotBus` | **M12** |
| **V9958 VDP + 128 KB VRAM** | I/O-bus port-registration + the `#98-#9B`/`#9C-#9F` mirror alias wired and test-doubled | **M13** (DEC-0002: VRAM is owned by the VDP; it is NOT in the CPU address map) |
| PPI keyboard/cassette (`#A9/#AA/#AB` behavior) | only PPI **port A** (`#A8`) slot-select is modeled; `#A9-#AB` + their mirror are registered-but-inert seams | later peripherals milestone |
| Exact unified T-state / cycle model | M1-wait count surfaced + applied to the scheduler; full VDP-access-too-fast / strobe timing not modeled | DP-4 (deferred residual from M10) |

**Non-goal guardrails:** do not implement PSG/RTC register behavior, do not create the mapper RAM
banks or ROM images, do not implement any VDP register/VRAM logic. If a prerequisite appears to
need any of these, STOP and raise a coordinator decision rather than absorbing scope
(guardrails "Scope Control").

### 1.3 Assumptions (each with a verification action)

- **A-1 (floating-bus value = `0xFF`).** Unmapped I/O reads return `0xFF` (open bus / all-ones).
  Fact-sheet §10 documents true floating-bus reads as machine-state-dependent "last byte on the
  bus," and that openMSX "concluded this is not worth precisely reproducing." The current CPU stub
  already returns `0xFF` for `IN A,(n)` / `IN r,(C)` (`src/devices/cpu/z80a_cpu.cpp:835-837,1239`).
  *Verify:* keep `0xFF` so no M9 I/O test regresses; record the choice in code comment citing §10.
- **A-2 (M11 reset slot default exposes DRAM).** On real hardware `#A8` resets to `0` → page
  routes to primary slot 0 = BIOS ROM, which does not exist until M12. To keep the machine bootable
  and all 28 prior suites green, `cold_boot` initializes the slot registers so all four pages
  resolve to **slot 3-0 (DRAM)** (i.e. `#A8=0xFF`, slot-3 sub-slot register = `0x00`). This is an
  explicit **M11 bring-up default**, superseded in M12 by the authentic `#A8=0` / slot-0-BIOS reset
  once ROMs populate slot 0. *Verify:* unit test asserts the reset slot state; a comment + this
  package document the deviation; M12 planner package must flip the default. **This is a tracked
  risk (R-1), not a silent behavior.**
- **A-3 (`#FFFF` readback is ones-complement).** Reading `#FFFF` in an expanded page-3 slot returns
  `0xFF ^ subSlotRegister`. Grounded in openMSX `MSXCPUInterface.cc:209-210,769-770`. *Verify:*
  unit test in M11-S2; A/B diff in M11-S6.
- **A-4 (M1 wait belongs to the S1985 layer, applied by the machine).** The Z80 core keeps
  publishing datasheet T-states (M9 timing oracles must stay green); the +1-per-M1-cycle MSX wait is
  computed by the S1985 layer and added to the scheduler by the machine step. *Verify:* the M1-wait
  unit test (S4) + updated machine timing oracle (S5); see R-3.
- **A-5 (backup-RAM `.sram` persistence is out of M11).** openMSX persists the 16 bytes as a
  `.sram` file (`MSXS1985.cc:19-29`). M11 models the *volatile* 16-byte store + register semantics
  only; disk persistence is deferred (no owning milestone yet — flag to coordinator if required).
  *Verify:* unit test covers in-session read/write + reset clearing; persistence explicitly noted
  out of scope.

---

## 2. Spec Summary (behavior to implement, grounded)

### 2.1 Memory bus — slot decode

- **Primary select (`#A8`, PPI port A).** 8-bit register, 2 bits per page:
  bits0-1 page0 (`#0000-#3FFF`), bits2-3 page1 (`#4000-#7FFF`), bits4-5 page2 (`#8000-#BFFF`),
  bits6-7 page3 (`#C000-#FFFF`). Read and write. (Fact-sheet §3; `MSXCPUInterface.cc:724-747`.)
- **Secondary select (`#FFFF`).** Present only in an **expanded** primary slot. On HB-F1XV only
  primary slot 3 is expanded (fact-sheet §9). Write to `#FFFF` sets the sub-slot register of the
  page-3 primary slot (2 bits per page); read returns `0xFF ^ subSlotRegister`. When the page-3
  primary slot is NOT expanded, `#FFFF` is ordinary memory in the mapped device.
  (Fact-sheet §3; `MSXCPUInterface.cc:209-220,755-770`.)
- **Resolution order:** effective device for an access at address `A` =
  `slotLayout[ primary(page(A)) ][ secondary(page(A)) ][ page(A) ]`, then offset within the device.
  Unmapped (page(A), primary, secondary) triple → read `0xFF`, write ignored.
- **HB-F1XV population in M11:** slot **3-0** = 64 KB `dram_`; every other (primary, sub-slot)
  is empty (open bus) until M12. Slot 3 is the only expanded primary slot.

### 2.2 I/O bus — dispatch + mirrors

- 256-port dispatch keyed on `port & 0xFF`; a device registers one or more ports.
- **Straight-alias mirrors** (fact-sheet §7, §3, §10 — incomplete address decoding, model as
  aliases): `#9C-#9F` alias `#98-#9B`; `#AC-#AF` alias `#A8-#AB`. Implemented as an alias map so
  that whatever device is registered on the base port is automatically reachable on the mirror
  (this is how the M13 VDP on `#98-#9B` will "just work" on `#9C-#9F`).
- **Unmapped read → `0xFF`** (A-1); unmapped write → ignored.
- **Ports live in M11:** `#A8` (PPI port-A slot select, R/W) and its mirror `#AC`;
  `#FC-#FF` (mapper I/O) — mirrors are not documented for `#FC-#FF`; switched-I/O `#40-#4F`.
  `#98-#9B`/`#9C-#9F` are wired as an **alias pair with no backing device yet** (VDP is M13); the
  alias mechanism is verified in M11 with a test-double device.

### 2.3 Thin S1985 layer

- **Switched-I/O + backup RAM (ID `0xFE`).** The expanded/switched-I/O mechanism uses ports
  `#40-#4F`: a controller register (written via `#40`) selects a device ID; while ID `0xFE` is
  selected, `#40-#4F` route to the S1985 backup-RAM handler, dispatched on `port & 0x0F`
  (`MSXS1985.cc:49-91`, `MSXSwitchedDevice.cc`). Register semantics (verbatim from §6 /
  `MSXS1985.cc`):
  - read index 0 → `~ID` (= `0x01`); read index 2 → `sram[address]`;
    read index 7 → `(pattern & 0x80) ? color2 : color1` **and then** rotate
    `pattern = (pattern<<1)|(pattern>>7)`; other reads → `0xFF`.
  - write index 1 → `address = value & 0x0F`; write index 2 → `sram[address]=value`;
    write index 6 → `color2=color1; color1=value`; write index 7 → `pattern=value`.
  - `reset()` → `color1=color2=pattern=address=0`.
- **Mapper readback (`#FC-#FF`).** Writes store a per-page 8-bit segment register. Reads return
  `0x80 | (segment & 0x1F)` = `100xxxxx` (fact-sheet §4; `MSXMapperIO`, `MSXS1985.cc:31-34`). In M11
  the segment value does not yet select a RAM bank (M12); only storage + readback pattern.
- **M1 wait.** +1 T-state per M1 opcode-fetch cycle; each ED/CB/DD/FD prefix byte is its own M1
  cycle and adds another +1 (fact-sheet §8: `XOR A` 4→5, `BIT 0,A` 12→14, `OUT (n),A` 11→12).

---

## 3. src/ Placement Decision

Authority to add/rearrange under `src/` is delegated to the planner (DEC-0002) within the
`src/CLAUDE.md` boundary rules: `core/` = contracts/primitives (no device logic); `devices/` = chip
implementations (no machine wiring); `machine/` = HB-F1XV composition. The whole tree compiles into
one static lib `sony_msx_core` via the root `CMakeLists.txt:13-25` (no per-dir CMake), so each new
`.cpp` is added to that `add_library(...)` list.

### 3.1 `src/core/` — abstract contracts only

| File | Purpose |
|---|---|
| `src/core/bus.h` (**edit**) | Extend `core::Bus` with I/O access: add `virtual BusData io_read(BusAddress port)` (default `0xFF`) and `virtual void io_write(BusAddress port, BusData)` (default no-op). Keep `read`/`write` as the memory path. Defaults keep existing fake-bus test doubles compiling unchanged. |
| `src/core/device_contracts.h` (**new**) | Pure-virtual participation interfaces, no logic: `MemoryDevice { BusData mem_read(BusAddress); void mem_write(BusAddress, BusData); }`, `IoDevice { BusData io_read(BusAddress port); void io_write(BusAddress port, BusData); }`, `SwitchedDevice { std::uint8_t id(); BusData switched_read(BusAddress port); void switched_write(BusAddress port, BusData); }`. These are contracts (allowed in `core/`), not device behavior. |

Rationale: slot/port routing *contracts* are core "device contracts" per `src/CLAUDE.md`; the
*decode behavior* is device logic and stays out of `core/`.

### 3.2 `src/devices/chipset/` — S1985 engine + bus-decode fabric (implements the empty README)

| File | Purpose | Grounding |
|---|---|---|
| `src/devices/chipset/README.md` (**edit**) | Document the chipset module: thin-S1985-over-independent-modules architecture, the file map below, and the "no GPL copy" rule. | fact-sheet §10 |
| `src/devices/chipset/slot_bus.h/.cpp` | `SlotBus` memory-decode fabric: holds `MemoryDevice*` at `[primary][sub][page]`, the `#A8` primary byte, the per-primary-slot sub-slot registers + `isExpanded[]`; resolves memory `read/write`; handles `#FFFF` secondary read (`0xFF ^ reg`) / write. Population API for machine/M12. | §3, §9; `MSXCPUInterface.cc:209-220,724-770` |
| `src/devices/chipset/io_bus.h/.cpp` | `IoBus` I/O-dispatch fabric: 256-entry `IoDevice*` table + alias map (`register_mirror(base,mirror)`); `io_read/io_write` with open-bus default. | §3, §7, §10 |
| `src/devices/chipset/ppi_slot_select.h/.cpp` | `PpiSlotSelect` — the PPI **port-A** slot-select register as an `IoDevice` on `#A8`; drives `SlotBus` primary selection; R/W with readback. (Minimal PPI; `#A9-#AB` deferred.) | §3; `MSXPPI.cc` |
| `src/devices/chipset/mapper_io.h/.cpp` | `MapperIo` — `IoDevice` on `#FC-#FF`: 4 segment registers, readback `0x80\|(seg&0x1F)`. | §4; `MSXMapperIO` |
| `src/devices/chipset/switched_io.h/.cpp` | `SwitchedIoController` — `IoDevice` on `#40-#4F`: ID-select register + dispatch to registered `SwitchedDevice`s. | §6, §10; `MSXSwitchedDevice.cc` |
| `src/devices/chipset/s1985_engine.h/.cpp` | `S1985Engine` — the thin layer: owns the 16-byte backup RAM (`SwitchedDevice`, ID `0xFE`, address/data/pattern/color1/color2 per §6), configures `MapperIo` readback base/mask, and exposes `m1_wait_tstates(std::uint32_t m1_cycles)` = `m1_cycles * kM1WaitTStates` (`kM1WaitTStates = 1`). | §4, §6, §8; `MSXS1985.cc` |
| `src/devices/chipset/system_bus.h/.cpp` | `SystemBus : public core::Bus` — the `MachineBus` replacement the CPU talks to. Composes `SlotBus` (memory `read/write`) + `IoBus` (`io_read/io_write`). | — |

### 3.3 `src/devices/cpu/` — I/O seam + M1 surfacing (minimal, keep M9 green)

| File | Purpose |
|---|---|
| `cpu_bus_client.h/.cpp` (**edit**) | Add `BusData io_read(std::uint32_t port)` / `void io_write(std::uint32_t port, BusData)` forwarding to `core::Bus::io_read/io_write`. |
| `z80a_cpu.h/.cpp` (**edit**) | (1) Route `OUT (n),A` / `IN A,(n)` (`z80a_cpu.cpp:832-838`) and ED `IN r,(C)` / `OUT (C),r` + block I/O (`:1238-1318`) through `bus_.io_read/io_write` instead of the `0xFF` stub. Port for `IN A,(n)`/`OUT (n),A` = `(A<<8)\|n`; for `(C)` variants = `BC`. (2) Add a per-`step()` **M1-cycle counter** reset at `step()` entry (`:37`), incremented in `fetch_opcode()` (`:152`) and interrupt-ack M1 cycles; expose `std::uint32_t m1_cycles_last_step() const`. Datasheet T-state returns are UNCHANGED. |

Boundary compliance: the M1 counter *exposes a CPU signal*; it is not machine wiring. The wait is
*computed* by `S1985Engine` and *applied* by the machine — device logic stays in `devices/`,
composition in `machine/`.

### 3.4 `src/machine/` — composition changes

| File | Purpose |
|---|---|
| `hbf1xv_machine.h/.cpp` (**edit**) | Remove the nested `MachineBus` (`hbf1xv_machine.h:124-133`, `.cpp:17-26`). Instantiate `SlotBus`, `IoBus`, `PpiSlotSelect`, `MapperIo`, `SwitchedIoController`, `S1985Engine`, `SystemBus`. Register `#A8`(+`#AC` mirror), `#FC-#FF`, `#40-#4F`, and the `#98-#9B`↔`#9C-#9F` alias pair. Populate slot **3-0** with `dram_` via a `MemoryDevice` adapter. `cold_boot` sets the M11 default slot state (A-2). In `step_cpu_instruction` (`.cpp:73-94`) add `s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step())` to the `scheduler_.tick(...)`. Keep `load_memory`/`read_memory` as DRAM-direct debug accessors (documented). |
| `src/machine/ram_slot_backing.h/.cpp` (**new**) | Thin adapter presenting `MemoryRegion& dram_` as a `core::MemoryDevice` occupying slot 3-0 pages 0-3. (Machine-side composition glue; keeps device fabric ignorant of HB-F1XV specifics.) |

---

## 4. Interface Contracts (proposed; illustrative signatures)

**Memory slot-decode API** (`chipset::SlotBus`):
```
BusData  read(BusAddress a);                 // resolve device, offset, or 0xFF
void     write(BusAddress a, BusData v);      // resolve device, or ignore
void     set_primary_select(std::uint8_t a8);        // PPI #A8 byte (2 bits/page)
std::uint8_t primary_select() const;                 // #A8 readback
void     set_expanded(int primary, bool expanded);   // HB-F1XV: only slot 3
void     write_ffff(BusData v);   // set sub-slot reg of page-3 primary (if expanded)
BusData  read_ffff() const;       // 0xFF ^ subSlotReg  (else normal memory)
void     attach(int primary, int sub, int page, core::MemoryDevice* dev);
```

**I/O-bus dispatch API** (`chipset::IoBus`):
```
BusData io_read(BusAddress port);            // (port & 0xFF), open-bus 0xFF default
void    io_write(BusAddress port, BusData v);
void    attach(std::uint8_t port, core::IoDevice* dev);
void    register_mirror(std::uint8_t base, std::uint8_t mirror);  // straight alias
```

**Switched-I/O device interface** (`core::SwitchedDevice`, S1985 backup RAM implements it):
```
std::uint8_t id() const;                      // 0xFE for the S1985 backup RAM
BusData switched_read(BusAddress port);       // dispatch on (port & 0x0F)
void    switched_write(BusAddress port, BusData v);
// controller: #40 selects ID; #40-#4F route to the device whose id() matches.
```

**M1-wait surfacing to scheduler/CPU** (grounding note): openMSX conceptually accounts MSX wait
states inside the CPU-interface/clock layer rather than in the generic Z80 (fact-sheet §8; the
S1985 asserts `/WAIT` on every M1). We mirror the *concept* without copying code: the Z80 core
counts M1 cycles per instruction and exposes the count; `S1985Engine::m1_wait_tstates(count)` maps
it to `+count` T-states; `Hbf1xvMachine::step_cpu_instruction` ticks the scheduler by
`datasheet_tstates + wait`. This isolates the MSX-specific wait to the chipset+machine and leaves
the reusable Z80 datasheet timings (and every M9 timing oracle) untouched.

---

## 5. Slice Plan (deterministic; M11-S1 … M11-S6)

Every slice runs the **full evidence-gate set** (see §6) and must leave `ctest` green. Slice order
is dependency-driven: contracts/seams → memory fabric → I/O fabric → S1985 → integration → A/B.

### M11-S1 — Bus-contract extension + CPU I/O seam + M1-cycle surfacing
- **Goal:** add the I/O path and M1-cycle count without changing any observable result yet.
- **Files:** `src/core/bus.h`, `src/core/device_contracts.h` (new), `src/devices/cpu/cpu_bus_client.{h,cpp}`, `src/devices/cpu/z80a_cpu.{h,cpp}`; root `CMakeLists.txt` (add `device_contracts.h` is header-only — no new .cpp yet).
- **Unit tests:** `tests/unit/devices/cpu_io_seam_unit_test.cpp` — `OUT (n),A` then `IN A,(n)` round-trips through a capturing fake `IoDevice` bus; `IN r,(C)`/`OUT (C),r` use `BC` as port; unmapped read → `0xFF`; M1-cycle count oracle: `NOP`=1, `LD A,n`=1, CB op=2, ED op=2, DD op=2, DDCB op=2.
- **Integration tests:** existing suite must stay green (unmapped I/O still `0xFF`; datasheet T-states unchanged).
- **Evidence gates:** validate-assets, checksum, Debug build, ctest.

### M11-S2 — Memory slot-decode fabric (SlotBus + PPI #A8 + #FFFF)
- **Goal:** deterministic primary/secondary slot decode.
- **Files:** `src/devices/chipset/slot_bus.{h,cpp}` (new), `src/devices/chipset/ppi_slot_select.{h,cpp}` (new); `CMakeLists.txt`.
- **Unit tests:** `tests/unit/devices/chipset_slot_bus_unit_test.cpp` — each page routes to the primary slot chosen by `#A8`; `#A8` readback equals last write; attach devices to slots 0/3, verify address→device→offset; expanded slot-3 `#FFFF` write sets sub-slot and read returns `0xFF ^ reg`; non-expanded page-3 `#FFFF` is normal memory; unmapped triple → `0xFF`/ignored. `tests/unit/devices/chipset_ppi_slot_select_unit_test.cpp` — `#A8` R/W drives `SlotBus.set_primary_select`.
- **Integration tests:** none new; existing green.
- **Evidence gates:** all four.

### M11-S3 — I/O dispatch fabric (IoBus + mirror aliasing)
- **Goal:** port dispatch with `#98-9B`→`#9C-9F` and `#A8-AB`→`#AC-AF` straight aliases + open bus.
- **Files:** `src/devices/chipset/io_bus.{h,cpp}` (new); `CMakeLists.txt`.
- **Unit tests:** `tests/unit/devices/chipset_io_bus_unit_test.cpp` — register a test-double `IoDevice` on a base port, assert reads/writes on the mirror hit the same device (engine-detection oracle `read(#AC)==read(#A8)`); VDP-mirror pair `#98`↔`#9C` via a stub proves M13 seam; unmapped ports → `0xFF`/ignored; decode uses `port & 0xFF`.
- **Integration tests:** none new; existing green.
- **Evidence gates:** all four.

### M11-S4 — S1985 thin layer (switched-I/O + backup RAM + MapperIo + M1-wait)
- **Goal:** the residual engine behavior over the fabrics from S2/S3.
- **Files:** `src/devices/chipset/switched_io.{h,cpp}`, `src/devices/chipset/mapper_io.{h,cpp}`, `src/devices/chipset/s1985_engine.{h,cpp}` (all new); `CMakeLists.txt`.
- **Unit tests:**
  - `tests/unit/devices/chipset_backup_ram_unit_test.cpp` — via `#40` ID-select `0xFE`: write index1 sets `address&0x0F`; write/read index2 hits `sram[address]`; index7 read returns `color2/color1` per `pattern&0x80` and rotates `pattern`; index6 shifts `color2=color1;color1=v`; index0 read = `~ID` = `0x01`; other reads = `0xFF`; `reset()` clears all (`MSXS1985.cc:44-91`).
  - `tests/unit/devices/chipset_mapper_io_unit_test.cpp` — write seg to `#FC-#FF`, read returns `0x80|(seg&0x1F)` (`100xxxxx`) for representative segs (0→0x80, 0x1F→0x9F, 0x25→0x85).
  - `tests/unit/devices/chipset_m1_wait_unit_test.cpp` — `S1985Engine::m1_wait_tstates(n)==n`; oracle table maps M1-cycle counts (from S1) to the §8 examples: `XOR A` 4→5, `BIT 0,A` 12→14, `OUT (n),A` 11→12.
- **Integration tests:** none new; existing green.
- **Evidence gates:** all four.

### M11-S5 — System integration (SystemBus + machine wiring, replace MachineBus)
- **Goal:** the machine boots/steps the CPU over the real bus; DRAM in slot 3-0; M1 wait applied.
- **Files:** `src/devices/chipset/system_bus.{h,cpp}` (new), `src/machine/ram_slot_backing.{h,cpp}` (new), `src/machine/hbf1xv_machine.{h,cpp}` (edit); `CMakeLists.txt`.
- **Unit tests:** `tests/unit/machine/hbf1xv_slot_map_unit_test.cpp` — `cold_boot` default slot state (A-2) makes all 4 pages resolve to slot 3-0 DRAM; `read_memory`/`load_memory` remain DRAM-direct.
- **Integration tests:** `tests/integration/machine/hbf1xv_system_bus_integration_test.cpp` — load a small program (incl. an `OUT #A8` / `IN #A8` / `IN #AC` sequence and a mapper `IN #FC`), run to a checkpoint, assert CPU-visible reads and the mirror/readback oracles; **update timing oracles** in `hbf1xv_cpu_step_integration_test.cpp` and any `elapsed_cycles` assertions to the new datasheet+M1-wait totals (R-3).
- **Evidence gates:** all four; full 28+ prior suite must pass (with the intentional timing-oracle updates).

### M11-S6 — A/B trace-diff evidence + regression close
- **Goal:** capture a REAL openMSX trace-diff for slot-decode / I/O behavior; finalize package.
- **Files:** `tools/openmsx-io-parity.ps1` (new, or extend `tools/openmsx-trace-parity.ps1`), `tests/parity/m11_bus_probe.bin` (new probe program), `docs/m11-parity-trace-diff.md` (generated artifact), `docs/asset-checksums.txt` (refresh).
- **Tests:** `tests/integration/machine/hbf1xv_m11_parity_checkpoint_integration_test.cpp` — runs the same probe program through this emulator to the same checkpoint as the harness feeds openMSX.
- **Evidence gates:** all four **plus** the A/B gate (§6.2). No parity claim without a captured diff.

---

## 6. Evidence-Gate Mapping

### 6.1 Per-slice deterministic gates (S1–S6, every cycle)

| Gate | Command | Pass condition |
|---|---|---|
| Assets present | `tools/validate-assets.ps1` | required BIOS present + ≥1 ROM (run/capture; do not fabricate) |
| Checksums | `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | reproducible checksum file written |
| Build | `cmake --build build --config Debug` | build succeeds |
| Tests | `ctest --test-dir build -C Debug --output-on-failure` | all deterministic tests pass |

(Configure once: `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF`, per baseline build flow.)

### 6.2 A/B acceptance gate (M11-S6, behavior-affecting)

**Definition.** Extend the existing per-instruction parity harness
(`tools/openmsx-trace-parity.ps1`, which already drives openMSX on WSL, single-steps, and diffs
architectural register state via `tools/trace-diff.py`) with a **new probe program**
`tests/parity/m11_bus_probe.bin` that exercises exactly the M11 behaviors whose result lands in a
CPU register (so the existing register-diff comparator captures them):

- `OUT (#A8),A` with a known slot byte, then `IN A,(#A8)` → `A` == written byte (primary readback).
- `IN A,(#AC)` → equals the `#A8` read (engine-detection mirror oracle, fact-sheet §10).
- mapper `IN A,(#FC)` after `OUT (#FC),A` → `A` == `0x80|(seg&0x1F)` (`100xxxxx`, §4).
- switched-I/O: `OUT (#40),#FE` then `IN A,(#40)` → `A` == `0x01` (`~ID`), and a
  backup-RAM write/read round-trip via index1/index2.
- `#FFFF` sub-slot write/readback in the expanded slot (A-3).

The harness (a) runs this emulator via `--parity-trace` to produce trace A, (b) drives openMSX
(machine with an S1985, e.g. a C-BIOS MSX2+ profile) over the SAME program/base/initial-vector to
produce trace B, (c) runs `tools/trace-diff.py` to emit the REAL diff to
`docs/m11-parity-trace-diff.md`. **Hard rule (guardrails; DEC-0001/DEC-0002 precedent):** NO parity
claim is made without a genuine captured diff. If openMSX cannot be driven, the tool reports
**BLOCKED** (empty B side) and QA treats parity as outstanding — never as a pass. The adversarial
self-check from M10 (empty-side → BLOCKED, corrupted field → DIVERGENCE) is reused to validate the
comparator.

*Assumption to verify in S6:* the chosen openMSX machine profile actually instantiates an S1985 and
exposes `#A8`/`#AC`/`#FC`/`#40` as this fact-sheet describes; if the reference machine lacks the
mirror, select a machine XML that includes it (fact-sheet §10 notes the mirror is per-machine XML,
`references/openmsx-21.0/...` machine configs). Record the exact machine used in the diff artifact.

---

## 7. Acceptance Criteria (M11 exit)

1. Slot-decoded memory bus (primary `#A8` + secondary `#FFFF`, `0xFF^reg` readback) replaces
   `MachineBus`; CPU-visible reads/writes route through slot selection deterministically. *(S2, S5)*
2. I/O bus dispatches by port; `#9C-#9F`↔`#98-#9B` and `#AC-#AF`↔`#A8-#AB` modeled as straight
   aliases; unmapped I/O reads return the documented floating value `0xFF` (A-1). *(S3)*
3. S1985 layer: 16-byte backup RAM on switched-I/O ID `0xFE` (address/data/pattern/color1/color2 per
   §6), mapper readback `100xxxxx`, and +1 M1 opcode-fetch wait — all unit-verified. *(S4)*
4. System integration: `Hbf1xvMachine` boots and steps the CPU over the new bus with inert DRAM as
   slot 3-0; all M0–M10 suites remain green (timing oracles updated for the M1 wait, R-3). *(S5)*
5. Per-slice evidence gates all executed and green (§6.1). *(S1–S6)*
6. A/B trace-diff vs openMSX captured as a REAL artifact `docs/m11-parity-trace-diff.md`; no
   fabricated parity (§6.2). *(S6)*
7. QA sign-off recorded before closure (`docs/m11-qa-signoff.md`).

---

## 8. Dependency Map (across src/)

```
core/bus.h, core/device_contracts.h        (contracts)
        │
        ├── devices/cpu/  (cpu_bus_client io seam, z80a M1 counter)   [S1]
        │
        └── devices/chipset/
              slot_bus  ── ppi_slot_select                            [S2]
              io_bus                                                  [S3]
              switched_io ── s1985_engine ── mapper_io                [S4]
              system_bus (composes slot_bus + io_bus)                 [S5]
                    │
                    └── machine/hbf1xv_machine + ram_slot_backing     [S5]
                            │
                            └── tools/openmsx-io-parity + docs/       [S6]
```
Cross-milestone seams: **M12** attaches `MemoryDevice` ROM/mapper-RAM instances via
`SlotBus::attach` and consumes `MapperIo` segments; **M13** attaches the VDP `IoDevice` on
`#98-#9B` and inherits the `#9C-#9F` alias from `IoBus`.

---

## 9. Risks and Assumptions (with verification actions)

- **R-1 (reset slot default deviates from hardware).** M11 forces `#A8` so DRAM is visible (A-2);
  real reset is `#A8=0`→slot-0 BIOS. *Impact:* boot flow is not yet authentic. *Verify/close:* unit
  test pins the M11 default and comments cite this package; M12 planner MUST flip the reset default
  to `#A8=0` once BIOS ROM populates slot 0. Tracked as an explicit, non-silent deviation.
- **R-2 (existing tests assume flat DRAM everywhere).** 28 prior tests load/run programs at
  arbitrary addresses. *Verify:* the M11 default slot config maps all 4 pages to slot 3-0 DRAM, so
  flat behavior is preserved; run full `ctest` in S2/S5 to confirm; if any test hard-codes a slot
  assumption, update it with an explicit note (no silent behavior change).
- **R-3 (M1 wait changes machine cycle totals).** Applying +1/M1 to the scheduler will change
  `elapsed_cycles` after stepping, breaking timing oracles that assumed datasheet-only totals
  (e.g. `hbf1xv_cpu_step_integration_test.cpp`). *Verify:* identify every affected oracle in S5 and
  update it to the MSX-accurate value (datasheet + M1 waits), grounded in §8 example timings; QA
  re-derives independently. This is a correctness improvement, not a regression.
- **R-4 (openMSX driveability for A/B).** The harness may be BLOCKED if openMSX cannot single-step
  the probe in the scripted context (the historical M9/M10 blocker). *Verify:* reuse the M10
  chained break-callback stepping technique that already works in `tools/openmsx-trace-parity.ps1`;
  if still blocked, report BLOCKED honestly (never claim parity) and escalate to coordinator.
- **R-5 (openMSX reference machine may not include the mirror / S1985).** Mirrors are per-machine
  XML (§10). *Verify in S6:* pick a machine profile whose XML instantiates an S1985 and the
  `#9C-#9F`/`#AC-#AF` mirrors; record the exact profile in `docs/m11-parity-trace-diff.md`.
- **R-6 (backup-RAM persistence out of scope).** openMSX persists 16 bytes to `.sram`
  (`MSXS1985.cc:19-29`); M11 models volatile behavior only (A-5). *Verify:* if a later milestone
  needs persistence, raise a coordinator decision; do not silently add file I/O in M11.
- **R-7 (switched-I/O `#40` controller semantics under-specified in the fact-sheet).** §6/§10 give
  the device-side (`port & 0x0F`) behavior and ID-select, but the exact `#40` controller
  read/write contract is drawn from openMSX `MSXSwitchedDevice.cc`. *Verify in S4:* re-read
  `references/openmsx-21.0/src/MSXSwitchedDevice.cc` before implementing the controller (do NOT copy
  code); unit-test the ID-select + dispatch against the §6 register table.
- **Assumptions A-1…A-5** are listed in §1.3 each with a verification action.

---

## 10. Developer Handoff

- **Start at M11-S1** (bus-contract + CPU I/O seam + M1 counter); it must leave every existing test
  green with zero observable change (unmapped I/O still `0xFF`, datasheet T-states unchanged).
- Implement slices **in order** (S1→S6); each slice runs the full §6.1 gate set and keeps `ctest`
  green. A/B (§6.2) is the S6 gate.
- **src/ placement is fixed by §3:** contracts in `src/core/`, all decode/engine logic in
  `src/devices/chipset/`, composition in `src/machine/`. Do not put decode logic in `core/` or
  machine wiring in `devices/` (`src/CLAUDE.md`). Add each new `.cpp` to `add_library(sony_msx_core`
  in the root `CMakeLists.txt`.
- **Never copy** `references/openmsx-21.0/` code into `src/` (GPL isolation, guardrails). Read it to
  understand behavior; ground each behavior claim in a concrete `references/...` path in code
  comments and the implementation report.
- **Hard scope fence:** no PSG/RTC register behavior, no mapper RAM banks, no ROM images, no VDP.
  If blocked on a prerequisite, raise a coordinator decision (guardrails "Scope Control").
- **No fabricated parity:** the A/B artifact must contain a genuine captured diff or an honest
  BLOCKED result (guardrails; DEC-0001/DEC-0002 precedent).
- Deliverables: source per §3; tests per §5; `docs/m11-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`; an implementation report `docs/m11-implementation-report.md`; then
  hand to QA for `docs/m11-qa-signoff.md`.
