# devices/chipset — S1985 "MSX-ENGINE" + full system bus (M11)

The Yamaha **S1985** (MSX-SYSTEMII) is the HB-F1XV glue-logic ASIC. Per the
fact-sheet `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` sec.10 and
`agent-protocol/channels/decisions.md` DEC-0002, reference emulators do **not**
model it as a monolith: the PPI, PSG, RTC and mapper registers are independent
modules, and a thin S1985 layer adds only the residual engine-specific
behaviour. This folder implements that architecture as a **thin S1985 layer over
independent decode fabrics**.

## File map

| File | Role |
|---|---|
| `slot_bus.{h,cpp}` | Memory slot-decode fabric: primary select (PPI `#A8`, 2 bits/page) + secondary/sub-slot select (`#FFFF`, `0xFF^reg` readback on expanded slot 3). Attaches `core::MemoryDevice`s per `[primary][sub][page]`. |
| `ppi_slot_select.{h,cpp}` | Legacy M11 minimal i8255 PPI **port A** (`#A8`) slot-select `IoDevice`. Superseded in machine wiring by `ppi_8255` (M15); survives with its own unit test as the minimal reference implementation. |
| `ppi_8255.{h,cpp}` | Full i8255 PPI on `#A8-#AB` (M15): port A slot select, port B keyboard-row read, port C keyboard column/caps/click, mode/control register — the PPI the machine actually wires. |
| `mb670836_pause.{h,cpp}` | MB670836 pause-gate (M25): the HB-F1XV PAUSE hardware latch gating CPU advance. |
| `reset_status_register.{h,cpp}` | Port `#F4` reset-status register (DEC-0031): non-inverted per `Sony_HB-F1XV.xml`; the MSX2+ boot logo depends on it. |
| `system_control.{h,cpp}` | System-control/turbo-adjacent register surface (see header for the exact port contract). |
| `io_bus.{h,cpp}` | 256-port I/O dispatch fabric (`port & 0xFF`) + straight-alias mirrors (`#98-#9B`->`#9C-#9F`, `#A8-#AB`->`#AC-#AF`) + open-bus default. |
| `mapper_io.{h,cpp}` | Mapper registers `#FC-#FF`; read-back `0x80 \| (seg & 0x1F)` = `100xxxxx` (sec.4). Storage only in M11; bank selection is M12. |
| `switched_io.{h,cpp}` | Expanded/switched-I/O controller `#40-#4F`: `#40` selects a device ID, the selected `core::SwitchedDevice` answers `#40-#4F` (openMSX `MSXDeviceSwitch`). |
| `s1985_engine.{h,cpp}` | The **thin S1985 layer**: the 16-byte backup RAM (`SwitchedDevice` ID `0xFE`, address/data/rotating-pattern/color1/color2, sec.6), the mapper read-back pattern config (sec.4), and the `+1` M1 opcode-fetch wait helper (sec.8, applied by the machine). |
| `system_bus.{h,cpp}` | `core::Bus` the CPU talks to: composes `SlotBus` (memory) + `IoBus` (I/O). Replaces the flat-DRAM `Hbf1xvMachine::MachineBus`. |

## Boundaries

- Decode/engine logic lives here (`devices/`); the abstract participation
  contracts (`MemoryDevice`/`IoDevice`/`SwitchedDevice`) live in
  `core/device_contracts.h`; HB-F1XV composition (which device sits in which
  slot/port, the M1-wait application, the cold-boot slot default) lives in
  `machine/`.
- **License isolation (guardrails):** behaviour is grounded in openMSX
  (`references/openmsx-21.0/src/MSXS1985.*`, `MSXCPUInterface.*`,
  `MSXMapperIO.*`, `MSXDeviceSwitch.*`) and cited by path in comments;
  **no GPL source is copied** into `src/`.

## A/B parity

`tools/openmsx/io-parity.ps1` drives the genuine **Sony HB-F1XV** machine
(real `<S1985>`) in openMSX on WSL over `tests/parity/m11_bus_probe.bin` and
diffs architectural state vs this emulator; the captured result is
`docs/m11-parity-trace-diff.md` (M11 achieved an EMPTY diff).
