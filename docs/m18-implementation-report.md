# M18 Implementation Report — Peripheral I/O: Kanji-Font ROM (`#D8-#DB`) + Printer (`#90-#97`) + Cassette Interface

- Milestone ID: M18
- Request: REQ-M18-002 (implementation only; QA sign-off is a separate, subsequent gate)
- Developer: MSX Developer Agent
- Planner package: `docs/m18-planner-package.md`
- Backlog effect: closes **B5** (Kanji-font ROM I/O `#D8-#DB`) and **C7** (printer + cassette
  interface); adds new backlog rows **F1** (cassette tape image-format/signal fidelity) and **F2**
  (printer image/ESC-sequence rendering depth).
- Capture date: 2026-07-07

---

## 1. Milestone Target

Deliver, per `docs/m18-planner-package.md` §3 (slices S1-S5):

1. **S1** — `KanjiFontRom`, a `core::IoDevice` under `src/devices/kanji/` answering the direct
   `#D8-#DB` ports: two independent address counters (`adr1_` for the JIS1 half
   `[0x00000,0x1FFFF]`, `adr2_` for the JIS2 half `[0x20000,0x3FFFF]`), the exact six-behavior
   address-latch + auto-increment protocol, `reset()`.
2. **S2** — `PrinterPort`, a `core::IoDevice` under `src/peripherals/` answering the real 8-port
   claim `#90-#97` (mod-4 dispatch): strobe/data write protocol, falling-edge byte capture,
   deterministic always-ready status.
3. **S3** — `CassetteInterface` (NOT a `core::IoDevice` — no dedicated CPU-visible port exists) +
   an additive, regression-guarded edit to `JoystickPorts` (`src/peripherals/joystick.{h,cpp}`):
   motor/output derived read-only from the existing `Ppi8255`; a deterministic synthetic-tape input
   model feeding PSG R14 bit7 via a new `CassetteInputSource` contract.
4. **S4** — Machine wiring (`src/machine/hbf1xv_machine.{h,cpp}`, additive only): attach all three
   devices, reset them at `cold_boot()`, load the real `bios/f1xvkfn.rom` via the existing
   `RomAssetLoader`, expose `kanji()`/`printer()`/`cassette()` accessors. **Zero edits** to
   `ppi_8255.{h,cpp}` or any CPU-timing code path.
5. **S5** — Real openMSX A/B evidence for all three subjects (Kanji content parity, printer
   write-path parity, cassette idle-state + write-path parity), with an honest SHA1-identity
   disposition for the Kanji ROM before any content-parity claim.

**Critical architectural findings honored throughout (§2.7 of the planner package):**

- **A-M18-1 (device identity).** The HB-F1XV's Kanji font device is openMSX class `Kanji`
  (`MSXKanji`, `references/openmsx-21.0/src/MSXKanji.hh/.cc`) — a **directly port-mapped** device
  — **not** `MSXKanji12` (`references/openmsx-21.0/src/MSXKanji12.hh/.cc`), a switched/expanded-I/O
  device (`MSXSwitchedDevice`, ID `0xF7`, ports `#40-#4F`) used by *other* MSX machines. Confirmed
  directly from `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:29-38`: `<Kanji id="Kanji
  ROM">` with four explicit direct `<io base=.. num=.. type=..>` entries at `#D8/#D9/#DA/#DB`, no
  `<Kanji12>` element, no `<type>` child anywhere. **No switched-I/O dispatch mechanism was built.**
- **A-M18-5/§2.7 (printer port range).** The backlog text says "`#90/#91`", but the XML claims the
  fuller 8-port window directly (`Sony_HB-F1XV.xml:74-78`, `io base="0x90" num="8" type="IO"/>`).
  This is a non-blocking, strictly-more-accurate correction (unlike the M17/DEC-0012 case, no
  fabrication risk existed either way) — implemented per the XML, not the backlog's narrower
  shorthand.
- **A-M18-8 (cassette has no dedicated port).** `Sony_HB-F1XV.xml:21`: `<CassettePort/>` is an
  **empty** element — the only device entry in this XML with no `<io>` child. `CassetteInterface`
  is therefore **not** a `core::IoDevice` and is **not** attached to `io_bus_` at any port.

---

## 2. Code Changes

### 2.1 New files

| File | Purpose |
|---|---|
| `src/devices/kanji/kanji_font_rom.h` / `.cpp` | `KanjiFontRom` device: `adr1_`/`adr2_` counters, six-behavior `#D8-#DB` protocol, 256 KB image, `reset()`. |
| `src/peripherals/printer_port.h` / `.cpp` | `PrinterPort` device: strobe/data protocol, falling-edge byte capture, deterministic status, `reset()`. |
| `src/peripherals/cassette_interface.h` / `.cpp` | `CassetteClockSource` (X4 clock-source contract) + `CassetteInterface` (motor/output derivation, synthetic-tape input model, `CassetteInputSource` implementation). |
| `tests/unit/devices/kanji/kanji_font_rom_unit_test.cpp` | S1 unit coverage. |
| `tests/unit/peripherals/printer_port_unit_test.cpp` | S2 unit coverage. |
| `tests/unit/peripherals/cassette_interface_unit_test.cpp` | S3 unit coverage. |
| `tests/integration/machine/hbf1xv_m18_peripheral_io_integration_test.cpp` | S4 system-integration coverage. |
| `tools/gen-m18-peripheral-io-probe.py` | Assembles the combined M18-S5 A/B probe program. |
| `tools/openmsx-peripheral-io-parity.ps1` | S5 A/B harness (all three subjects, one combined trace). |
| `tests/parity/m18_peripheral_io_probe.bin` | Generated probe fixture (125 bytes). |
| `docs/m18-parity-trace-diff.md` | S5 A/B evidence artifact. |

### 2.2 Modified files (additive only)

- `src/peripherals/joystick.h` — `+class CassetteInputSource` (defined here, the **consumer's**
  header, mirroring how `PsgPortSource` is defined in `psg_ym2149.h`); `+JoystickPorts::
  attach_cassette_input_source(CassetteInputSource*)`; `+CassetteInputSource* cassette_source_ =
  nullptr;` member. Existing `kCassetteInputBit`/`encode()` documentation comments updated to
  describe the new injectable seam; `encode()`'s bit7 computation is **byte-for-byte unchanged**
  (still unconditionally sets the bit).
- `src/peripherals/joystick.cpp` — `+attach_cassette_input_source()`; `read_port_a()` now computes
  `bits = encode(...)` (unchanged) then, **only if `cassette_source_ != nullptr`**, overrides bit7
  from `cassette_source_->cassette_input_high()`. The unattached path is provably identical to the
  pre-M18 code (regression-guard unit test, §3).
- `tests/unit/peripherals/joystick_unit_test.cpp` — `+` cases: unattached regression guard (idle
  and with-other-bits-pressed); attached source reflects live value; detach reverts to the default.
- `src/machine/hbf1xv_machine.h` — `+#include` for the three new headers; `+devices::kanji::
  KanjiFontRom kanji_font_rom_;`, `+peripherals::PrinterPort printer_;`, `+CassetteClock` nested
  class (X4 pattern, mirrors `RtcClock`/`FdcClock`) + `+CassetteClock cassette_clock_{scheduler_};`
  + `+peripherals::CassetteInterface cassette_{ppi_};` (declared **after** `ppi_`, per the ordering
  rule); `+kanji()`/`printer()`/`cassette()` accessors (const/non-const).
- `src/machine/hbf1xv_machine.cpp` — `wire_bus()`: `+cassette_.attach_clock_source(&cassette_clock_);
  +joystick_.attach_cassette_input_source(&cassette_);` (placed with the PSG/joystick wiring);
  `+io_bus_.attach(0xD8..0xDB, &kanji_font_rom_)` (4 calls); `+for (port = 0x90; port <= 0x97;
  ++port) io_bus_.attach(port, &printer_);` (8-port loop). `cold_boot()`: `+kanji_font_rom_.reset();
  +printer_.reset(); +cassette_.reset();`. `load_rom_assets()`: `+kanji_font_rom_.set_image(loader.
  load({"f1xvkfn.rom", 0x40000, "Kanji font ROM (I/O #D8-#DB)"}));`. `+CassetteClock::cpu_cycles()`
  implementation + `+kanji()`/`printer()`/`cassette()` accessor bodies.
- `CMakeLists.txt` — `+src/devices/kanji/kanji_font_rom.cpp`, `+src/peripherals/printer_port.cpp`,
  `+src/peripherals/cassette_interface.cpp` in the `sony_msx_core` source list.
- `tests/CMakeLists.txt` — `+4` test targets (3 unit, 1 integration).
- `docs/asset-checksums.txt` — refreshed (no content change; `bios/f1xvkfn.rom` was already present
  from a prior milestone).

**Verified additive-only, zero-touch guarantees (direct diff inspection, not by construction alone):**

```
$ git diff --stat -- src/devices/chipset/ppi_8255.h src/devices/chipset/ppi_8255.cpp
(empty -- zero lines changed)

$ git diff -- src/machine/hbf1xv_machine.cpp | grep -n "step_cpu_instruction\|run_cycles\|run_frame"
(empty -- zero matches; those functions were not touched)
```

### 2.3 Device model grounding (per-behavior citations)

**Kanji font ROM (S1).** Every port behavior is grounded in `references/openmsx-21.0/src/
MSXKanji.cc:32-88` (re-expressed in code comments, never copied — GPL isolation):

- `reset()`: `adr1_ = 0x00000`, `adr2_ = 0x20000` (`MSXKanji.cc:28-29`).
- `#D8` write: `adr1_ = (adr1_ & 0x1F800) | ((value & 0x3F) << 5)`.
- `#D9` write: `adr1_ = (adr1_ & 0x007E0) | ((value & 0x3F) << 11)`.
- `#DA` write: `adr2_ = (adr2_ & 0x3F800) | ((value & 0x3F) << 5)`.
- `#DB` write: `adr2_ = (adr2_ & 0x207E0) | ((value & 0x3F) << 11)` — the mask preserves bit 17
  (`0x20000`), so `adr2_` never leaves the JIS2 half via a normal write.
- `#D9` read: returns `image_[adr1_]`, then `adr1_ = (adr1_ & ~0x1F) | ((adr1_+1) & 0x1F)` (wraps
  within a 32-byte glyph block).
- `#DB` read: returns `image_[adr2_]` (this machine always uses the 256 KB path,
  `MSXKanji.cc:82`'s "temp workaround"), then increments `adr2_`'s low 5 bits identically.
- `#D8`/`#DA` reads: always `0xFF`, no side effect (`isLascom` is `false` for this machine —
  `MSXKanji.cc:9-24`, no `<type>` XML child — so the `isLascom` fallthrough that would make `#D8`
  readable does not apply).

A non-obvious but important detail confirmed while implementing: **both** `#D8` and `#D9` writes
clear `adr1_`'s low 5 bits (the `0x1F800`/`0x007E0` masks each exclude bits 0-4), so a full
`OUT (#D8),lo ; OUT (#D9),hi` sequence — in **either order** — always lands the counter exactly at
`addr & ~0x1F` (block-aligned), matching the BASIC usage example preserved as a comment in
`MSXKanji.cc` itself (`OUT &HD8, I MOD 64: OUT &HD9, I\64`). This is asserted directly by the S1
unit test (`Adr1Write_LandsBlockAligned`) and exploited by the S4/S5 probes (no extra "clear the low
bits" step is ever needed).

**Printer port (S2).** Grounded in `references/openmsx-21.0/src/MSXPrinterPort.cc:46-62` and
`Printer.cc:54-66`:

- mod-4 case 0 write: `set_strobe(value & 1)`; a **falling** edge (previous HIGH, new LOW) appends
  `data_` to `captured_bytes_` (`Printer.cc:59-66`, `if (!strobe && prevStrobe) { write(toPrint); }`).
- mod-4 case 0 read: status byte, **always `0x00`** (ready) in this milestone — see the disclosed
  A-M18-7 divergence below.
- mod-4 case 1 write: `data_ = value`; read: open-bus `0xFF` (no `type="I"` XML entry for `#91`).
- mod-4 case 2: no-op / `0xFF`.
- mod-4 case 3: PDIR/bidirectional — **not implemented**, matching openMSX's **own** documented
  scope limit (`MSXPrinterPort.cc:57`, "0x93 PDIR (BiDi) is not implemented").

**Disclosed divergence (A-M18-7).** openMSX's *default unplugged* printer port
(`DummyPrinterPortDevice::getStatus`, `DummyPrinterPortDevice.cc:5-8`) returns `true` (busy) —
"nothing plugged in". This emulator has no pluggable-peripheral framework (nothing else in `src/`
uses one), so `PrinterPort` **is** the built-in port and reports **ready** by default, matching
`PrinterCore::getStatus` (`Printer.cc:54-57`, returns `false`) — i.e. the behavior of an openMSX run
**with a printer plugged in**, not the bare default. This is documented in code comments and the
A/B probe was deliberately designed to never read this bit (§2.6 of the planner package).

**Cassette interface (S3).** Grounded in the S1985 fact-sheet §3/§8 and openMSX's
`CassettePort.hh/.cc`/`DummyCassetteDevice.cc` (behavior reference only):

- `motor_on() = !ppi_.cassette_motor_off()`; `output_level() = (ppi_.port_c_latch() & 0x20) != 0` —
  both pure, on-demand reads of the **already-public** `Ppi8255` accessors
  (`src/devices/chipset/ppi_8255.h:64,62`), **zero edit** to `Ppi8255`.
- `cassette_input_high()` with no tape loaded **always** returns `true` (idle-high) — the exact
  externally-observed default of vanilla openMSX's own `DummyCassetteDevice` (`readSample()` returns
  `32767`, converted to `true` by `CassettePort.cc:103`'s `sample >= 0`), so this milestone changes
  **nothing observable** for anyone not loading a synthetic tape.
- With a tape loaded (`load_synthetic_tape(bits, cycles_per_bit)`): `index = (clock_source_->
  cpu_cycles() - start_cycle_) / cycles_per_bit`; returns `bits[index]` (clamped to `true` past the
  end).
- The `CassetteClockSource` contract mirrors `RtcClockSource`/`FdcClockSource` exactly (X4 pattern:
  read-only `cpu_cycles()`, never wall-clock) and is consulted **pull-style only** — never wired
  into `step_cpu_instruction()`/`run_cycles()`/`run_frame()` (verified above by direct diff, not
  just by design intent).
- `CassetteInputSource` (the contract `JoystickPorts` consumes) is defined in `joystick.h` — the
  **consumer's** header — mirroring how `PsgPortSource` is defined in `psg_ym2149.h` (the PSG's own
  header) for the same injected-source relationship. `JoystickPorts::read_port_a()`'s bit7:
  unattached (`nullptr`, the default) → unconditionally `1` (byte-for-byte identical to the
  pre-M18 code — regression-guard unit test); attached → `cassette_source_->cassette_input_high()
  ? 1 : 0`.

### 2.4 Determinism (§2.5 of the planner package)

- Kanji font device and printer port: **no clock dependency at all** — pure combinational functions
  of stored state, identical to the M13 `RomDevice`/M17 `Ym2413Opll` no-clock-consumer
  classification.
- Cassette interface: the **only** new clock consumer this milestone, and it is **read-only** off
  `elapsed_cycles()` via the `CassetteClockSource` X4 pattern — never wall-clock, never perturbing
  CPU T-state accounting.
- No new production per-instruction hook was added anywhere — confirmed by direct `git diff`
  inspection of `step_cpu_instruction()`/`run_cycles()`/`run_frame()` (empty), not just by design
  intent.

---

## 3. Unit Test Results

New unit test executables (all green, see §5 for the full-suite run):

- `devices_kanji_font_rom_unit_test` — `reset()` defaults (`adr1_=0`, `adr2_=0x20000`); `#D8`/`#D9`
  writes land `adr1_` block-aligned; `#D9` sequential reads match a synthetic byte-distinguishable
  256 KB image and the auto-increment wraps within the 32-byte block (exercised past 32 reads);
  `#DB` sequential reads mirror the same protocol for the JIS2 half; `adr2_` bit 17 is **never**
  lost across `#DA`/`#DB` writes, even when the requested address is deliberately JIS1-shaped
  (`0x00456`) — the write path structurally cannot clear it; a genuine JIS2 address lands exactly;
  `#D8`/`#DA` reads are always `0xFF` with **no side effect** on either counter; `reset()` restores
  the documented defaults after writes/reads; two-run determinism (identical write/read sequence →
  identical results, both address counters and every returned byte).
- `peripherals_printer_port_unit_test` — data-then-strobe-pulse captures exactly one byte; strobe
  held low (no edge) causes no repeat/additional capture; multiple pulses capture multiple bytes in
  order; status read is always `0x00` (ready) including after a capture; `#91` read is open-bus;
  `#92`/`#93` (mod-4 cases 2/3) are inert/open-bus; `#94-#97` alias `#90-#93` exactly (mod-4
  dispatch); `reset()` clears captures without a spurious capture during reset itself; two-run
  determinism.
- `peripherals_cassette_interface_unit_test` — motor/output track a **real** `Ppi8255`'s port-C
  writes live; no-tape default is idle-high with or without an attached clock; loading a synthetic
  tape and advancing an injected fake clock returns the expected bit sequence at the expected cycle
  boundaries; past-end-of-tape reverts to idle-high; `reset()`/`eject_tape()` eject the tape;
  `sample_output()` is an explicit caller-driven utility (not a production hook); two-run
  determinism (identical cycle sequence → identical bit sequence).
- `peripherals_joystick_unit_test` (extended) — **regression guard**: the unattached path
  (`cassette_source_ == nullptr`, the default) reproduces the exact pre-M18 `read_port_a()` output
  byte-for-byte, both idle and with other joystick bits pressed; attached source's live value is
  reflected in bit7 without disturbing other bits; detaching (`nullptr`) reverts to the
  unconditional-1 default.

```
Start 76: devices_kanji_font_rom_unit_test
76/79 Test #76: devices_kanji_font_rom_unit_test ............................   Passed    0.01 sec
Start 77: peripherals_printer_port_unit_test
77/79 Test #77: peripherals_printer_port_unit_test ..........................   Passed    0.01 sec
Start 78: peripherals_cassette_interface_unit_test
78/79 Test #78: peripherals_cassette_interface_unit_test ....................   Passed    0.02 sec
```

(`peripherals_joystick_unit_test`, extended in place, is test #58 in the full suite — see §5.)

---

## 4. Integration Test Results

`machine_hbf1xv_m18_peripheral_io_integration_test` (new, S4):

1. **Kanji font ROM real-content readback** — a real CPU program drives `OUT (#D8),A`/`OUT
   (#D9),A`/`IN A,(#D9)` (JIS1) and `OUT (#DA),A`/`OUT (#DB),A`/`IN A,(#DB)` (JIS2) over the full
   M11 system bus for four representative addresses spanning both halves (`0x00020`, `0x1FFE0`
   JIS1; `0x20020`, `0x2FFE0` JIS2), reading 8 bytes each into RAM. Every one of the 32 read bytes
   is asserted **byte-exact** against the real `bios/f1xvkfn.rom` file, read directly (never
   fabricated). A direct device-level accessor cross-check (`machine.kanji().image()`) confirms the
   same.
2. **Printer write-path over the bus** — a real CPU program drives `OUT (#91),data`/`OUT
   (#90),strobe-pattern` for three bytes (`'H'`,`'I'`,`'!'`); `machine.printer().captured_bytes()`
   reflects the exact sequence.
3. **PPI port-C → cassette derivation + idle-state regression guard** — `debug_io_write(0xAA,
   ...)` bit4/bit5 patterns drive `machine.cassette()`'s live `motor_on()`/`output_level()`; **the
   cassette-input default (no synthetic tape loaded) still gives PSG R14 bit7 = 1 exactly as
   pre-M18**, confirmed both BEFORE and AFTER the PPI writes, even though `joystick_` IS wired to
   `cassette_` at the machine level (`attach_cassette_input_source` is no longer `nullptr` — this is
   the R-M18-5 regression risk, and the test directly proves the DEFAULT observable behavior is
   unchanged).
4. **Fresh-machine sanity re-check** — PSG R14 idle read and keyboard idle-row read both still
   `0xFF` on a fresh machine after all M18 wiring (mirrors the M15/M16/M17 inline-golden-recheck
   pattern; the full `ctest` run is the authoritative regression gate, §5).
5. **Two-machine determinism** — the identical driving sequence (Kanji select+read, printer
   write+capture, PPI port-C write) on two independent machine instances produces identical
   Kanji/printer/cassette observable state.

```
Start 79: machine_hbf1xv_m18_peripheral_io_integration_test
79/79 Test #79: machine_hbf1xv_m18_peripheral_io_integration_test ...........   Passed    0.08 sec
```

**Full-suite regression check** (`ctest --test-dir build -C Debug --output-on-failure`, fresh
`cmake --build build --config Debug` beforehand):

```
100% tests passed, 0 tests failed out of 79
Total Test time (real) =   1.67 sec
```

79/79 = 75 prior (M1-M17) + 4 new (M18). **Zero regression.** The M9/M12 CPU-timing oracles
(`devices_chipset_m1_wait_unit_test`, `machine_hbf1xv_cpu_step_integration_test`,
`machine_hbf1xv_cpu_parity_integration_test`, `machine_hbf1xv_m11_parity_checkpoint_integration_test`,
`machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`,
`machine_hbf1xv_m15_boot_checkpoint_integration_test`,
`machine_hbf1xv_m16_boot_checkpoint_integration_test`) and the M15
`peripherals_joystick_unit_test`/`devices_chipset_ppi_8255_unit_test`/
`machine_hbf1xv_m15_devices_integration_test` goldens all remained green, confirming M18 added no
`elapsed_cycles()`-driven CPU-timing-affecting clock consumer and the cassette-input injection is a
byte-for-byte-preserving regression guard.

---

## 5. Evidence Gates (exact captured output)

```
$ powershell.exe -NoProfile -Command "./tools/validate-assets.ps1"
Asset validation result: True
BIOS directory: C:\Users\pashim\source\sony-msx-hbf1xv\bios
ROM directory:  C:\Users\pashim\source\sony-msx-hbf1xv\roms
Present BIOS files:
  - f1xvbios.rom
  - f1xvdisk.rom
  - f1xvext.rom
  - f1xvfirm.rom
  - f1xvkdr.rom
  - f1xvkfn.rom
  - f1xvmus.rom
ROM file count: 1
ROM files:
  - aleste.rom

$ powershell.exe -NoProfile -Command "./tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt"
Checksum report written to: C:\Users\pashim\source\sony-msx-hbf1xv\docs\asset-checksums.txt
(SHA256 for all 7 bios/ files + 1 roms/ file, unchanged content; see docs/asset-checksums.txt)

$ cmake --build build --config Debug
... (0 errors; only pre-existing C4819 codepage warnings, unrelated to this change)
sony_msx_headless.vcxproj -> C:\Users\pashim\source\sony-msx-hbf1xv\build\Debug\sony_msx_headless.exe

$ ctest --test-dir build -C Debug --output-on-failure
100% tests passed, 0 tests failed out of 79
Total Test time (real) =   1.67 sec
```

---

## 6. openMSX A/B Evidence (all three subjects, real captures — see `docs/m18-parity-trace-diff.md`)

**Kanji-ROM SHA1 identity verification (A-M18-14), done BEFORE claiming content parity:**

```
$ sha1sum bios/f1xvkfn.rom
218d91eb6df2823c924d3774a9f455492a10aecb  bios/f1xvkfn.rom
```

This **matches** the XML-cited revision exactly (`Sony_HB-F1XV.xml:32`,
`<sha1>218d91eb6df2823c924d3774a9f455492a10aecb</sha1>`, "same as XDJ") — the strongest possible
disposition for this check, per A-M18-14.

**Mechanism.** ONE combined 63-instruction architectural trace (`tests/parity/
m18_peripheral_io_probe.bin`, generated by `tools/gen-m18-peripheral-io-probe.py`) exercises all
three subjects back-to-back, run via this emulator's existing `--parity-trace` mode and, on openMSX
19.1 (WSL, `Sony_HB-F1XV` machine XML), via a chained single-step Tcl script — **identical
mechanism to `tools/openmsx-io-parity.ps1`/`tools/openmsx-ym2413-parity.ps1` Subject 1, no new CLI
mode needed** (every value this probe reads — Kanji font bytes via `#D9`/`#DB`, PSG R14 via `#A2`
— lands directly in the CPU's `A` register, already captured by the architectural trace).

**Result: ARCHITECTURAL PARITY — EMPTY DIFF over all 63 instructions.**

- **Subject 1 (Kanji content parity, the strongest subject).** Manually decoded the exact
  instruction-index-to-`seq` mapping and cross-checked **all 16** `IN A,(#D9)`/`IN A,(#DB)` read
  results directly against the real `bios/f1xvkfn.rom` bytes at four representative addresses
  spanning both halves (`0x00020`→`00,18,18,18`; `0x1FFE0`→`00,00,00,10`; `0x20020`→`00,00,00,00`;
  `0x2FFE0`→`10,30,60,C4`) — every one matches, both against the real file and against openMSX's
  own trace. This is a genuinely content-bearing match, not a degenerate all-zero coincidence.
- **Subject 2 (printer write-path parity).** Write-only technique (`OUT (#91),data`/`OUT
  (#90),strobe-pattern` for 3 bytes) — **never** reads `#90`'s status bit (the A-M18-7 disclosed
  divergence, deliberately kept out of this probe). Empty diff confirms the write-path trace shape
  is architecturally identical.
- **Subject 3 (cassette idle-state + write-path parity).** `IN A,(#A2)` with PSG R14 selected
  returns `A=0xFF` (bit7=1, idle-high) on **both** emulators, both BEFORE and AFTER a representative
  PPI port-C (`#AA`) bit4/bit5 write sequence — genuinely comparable (both default to idle-high).

### Adversarial comparator self-check

Run against this cycle's own captured traces (`tools/trace-diff.py`, the shared M10 comparator,
reused unmodified):

- **Empty-side check**: diffed the emulator trace against an empty B file → **exit 2, BLOCKED**.
- **Corrupted-field check**: every `af=0000` occurrence in the captured openMSX trace XORed to
  `af=FFFF` in a copy → **exit 1, ARCHITECTURAL DIVERGENCE**, 42 field mismatches correctly flagged.

Both checks behave correctly — the comparator does not silently pass a blocked or corrupted
comparison, so the empty-diff PARITY result is trustworthy. Full detail, including the per-address
byte-content table: `docs/m18-parity-trace-diff.md`.

---

## 7. Ledger Discipline

- `agent-protocol/state/deferred-backlog.md`: **B5 → DONE (M18)**, **C7 → DONE (M18)** (row text
  refined from "`#90/#91`" to the accurate "`#90-#97`" claim, per the planner's non-blocking §2.7
  recommendation), both updated this cycle with full justification. New rows **F1** (cassette
  tape image-format/signal fidelity) and **F2** (printer image/ESC-sequence rendering depth) added
  under a new "Section E" heading, mirroring the existing D1-D7/E1-E2 sectioning.
- `agent-protocol/channels/responses.md`: RESP-M18-002 filed (this implementation cycle's outcome,
  evidence, and residual risks).
- `agent-protocol/state/current-phase.md`: updated to reflect M18 implementation delivered, QA next.

---

## 8. Known Issues / Residual Risks

1. **Synthetic-tape input model is deliberately NOT a real tape-file decoder** (backlog F1) — real
   `.CAS`/`.WAV`/`.TSX` decode/encode, FSK/UDF modulation, and save-to-host-file are explicitly out
   of scope. The deterministic in-memory bitstream contract is sufficient for the CPU-visible
   register behavior this milestone delivers.
2. **Printer capture is a deterministic raw-byte log only** (backlog F2) — no ESC-sequence
   interpretation, dot-matrix rendering, or PNG page output. `captured_bytes()` is a debug/
   introspection aid, not a rendered "printed page."
3. **Printer status-bit default (always-ready) is a deliberate, disclosed divergence** from
   openMSX's *unplugged* default (A-M18-7) — documented in code comments and kept out of the A/B
   probe by design; not a bug.
4. **Pre-existing M15 residual, explicitly NOT touched this milestone** (per the planner's §2.7
   transparency note): the RTC's XML claim is `#B4-#B7` but only `#B4`/`#B5` are currently wired in
   `wire_bus()` — unrelated to the Kanji/printer/cassette devices this milestone builds; recorded
   here only for continuity, not an M18 action item.
5. No new risk to the CPU-timing oracles: confirmed by direct `git diff` inspection that no
   `step_cpu_instruction()`/`run_cycles()`/`run_frame()` edit was made and no `elapsed_cycles()`
   adapter beyond the pull-only `CassetteClock` was added; all M9/M12 timing-oracle tests remain
   green.

---

## 9. QA Handoff

- **Build:** `cmake --build build --config Debug` — 0 errors (pre-existing C4819 codepage warnings
  only, unrelated to this change).
- **Tests:** `ctest --test-dir build -C Debug --output-on-failure` — **79/79 passed, 0 failed**
  (75 prior M1-M17 + 4 new M18). Zero regression.
- **New device files build clean:** `src/devices/kanji/kanji_font_rom.{h,cpp}`,
  `src/peripherals/printer_port.{h,cpp}`, `src/peripherals/cassette_interface.{h,cpp}` — all compile
  without warnings beyond the pre-existing repo-wide C4819 codepage note.
- **`JoystickPorts`/`Ppi8255` regression guards: HOLD.** A dedicated unit test proves the unattached
  cassette-input path is byte-for-byte identical to the pre-M18 `read_port_a()` output; the M18
  integration test independently re-checks the PSG R14/keyboard idle-row goldens on a fresh machine.
  `git diff --stat` confirms **zero** lines changed in `src/devices/chipset/ppi_8255.{h,cpp}`.
- **No CPU-timing/stepping-loop code touched:** `git diff` of `src/machine/hbf1xv_machine.cpp`
  contains zero occurrences of `step_cpu_instruction`, `run_cycles`, or `run_frame` — confirmed
  directly, not assumed.
- **A/B outcome: REAL PARITY on all three subjects, none BLOCKED.** One combined 63-instruction
  trace, empty diff. Kanji content parity independently confirmed byte-exact at 4 representative
  offsets (both against the real ROM file and against openMSX's own trace) after a genuine SHA1
  identity match (A-M18-14). Printer write-path parity and cassette idle-state + write-path parity
  both empty-diff, with the printer's known status-bit divergence deliberately never exercised.
  `docs/m18-parity-trace-diff.md`.
- **Assets:** `tools/validate-assets.ps1` → `True`; `tools/checksum-assets.ps1` refreshed
  `docs/asset-checksums.txt` (no asset content changed this milestone — `bios/f1xvkfn.rom` was
  already present from a prior milestone).
- **Backlog:** B5 → DONE (M18); C7 → DONE (M18), row text refined; F1/F2 added as new OPEN rows
  (Section E).
- **Next gate:** QA regression assessment + sign-off recommendation (`docs/m18-qa-signoff.md`),
  then a separate human release decision (normal gate, no auto-close per DEC-0003's M12-only
  grant) — bundled with M17's own still-pending release decision per the human's directive, but
  each is its own gate.
