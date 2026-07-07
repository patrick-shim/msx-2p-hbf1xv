# M18 Planner Package — Peripheral I/O: Kanji-Font ROM (`#D8-#DB`) + Printer (`#90-#97`) + Cassette Interface

- Milestone ID: M18
- Title: Peripheral I/O — Kanji-Font ROM Access (`#D8-#DB`) + Printer (Centronics `#90/#91`) + Cassette Interface
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M18-001 (planner-first, no production code)
- Decisions in force: DEC-0005 (backlog disposition discipline — every planner package must consult
  `agent-protocol/state/deferred-backlog.md` and re-affirm every row), DEC-0009 (indicative milestone
  order: "M18 peripheral I/O (B5 Kanji-font, C7 printer/cassette)"), DEC-0012 (precedent for how to
  handle a backlog-wording vs. XML-grounding mismatch — re-applied below in §2.7).
- Backlog effect: **closes B5** (Kanji-font ROM I/O `#D8-#DB`) and **closes C7** (printer
  `#90/#91` + cassette interface). All other rows re-affirmed (§4). Two new depth-deferral rows
  proposed: **F1** (cassette tape image-format/signal fidelity) and **F2** (printer
  image/ESC-sequence rendering depth), mirroring the M14 D1-D7 and M17 E1-E2 contract/depth splits.
- Gate: **NORMAL human-release-decision gate (no auto-close)** — explicit per the task brief and
  consistent with DEC-0003's auto-close grant being M12-only. Autopilot pauses at QA sign-off for the
  separate human release decision + tag, matching the M13-M17 pattern. M17's own release
  decision/tag remains a separate, still-pending gate (unaffected by this package).

> Grounding rule: every behaviour claim below cites a concrete `references/...` path (with line
> numbers where useful) or a current-repo `src/...`/`docs/...` path. openMSX sources are the
> BEHAVIOUR reference only and are GPL — **never copy openMSX code into `src/`**
> (`CLAUDE.md`, `agent-protocol/guardrails.md`).

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) Kanji font ROM I/O device** (backlog B5) — a new `core::IoDevice` answering ports
  `#D8-#DB`, reading the 256 KB JIS1+JIS2 font image already present at `bios/f1xvkfn.rom`
  (confirmed 262,144 bytes = `0x40000` via `docs/asset-checksums.txt` line 14: `f1xvkfn.rom | 262144
  bytes | 3c9f60fda2...`), implementing the exact address-latch + auto-increment protocol of openMSX's
  `MSXKanji` class (**not** `MSXKanji12` — see §2.2/§2.7 device-identity verification).
- **(b) Printer port device** (part of backlog C7) — a new `core::IoDevice` answering the real
  Centronics-style ports the HB-F1XV XML claims (`#90-#97`, see §2.7 finding), implementing the
  strobe/data write protocol and a deterministic, host-independent byte-capture sink (no PNG/ESC
  rendering — that depth is split out as new backlog row **F2**, §1.2/§4).
- **(c) Cassette interface peripheral** (part of backlog C7) — a new peripheral (NOT a
  `core::IoDevice` — it owns no CPU-visible port of its own; confirmed via the XML's empty
  `<CassettePort/>` tag, §2.7) that derives motor/output state from the already-existing M15
  `Ppi8255` port-C latch and supplies the cassette-input bit consumed by the already-existing M15
  `JoystickPorts` (PSG R14 bit 7), replacing the M15 hardcoded idle-high stub
  (`src/peripherals/joystick.h:41-42`, `kCassetteInputBit` documented "inert in M15 — tape transport
  is deferred, backlog C7"). A deterministic, in-memory, cycle-driven synthetic tape model is defined
  (§2.4) — real `.CAS`/`.WAV`/`.TSX` file-format decode/encode and signal-level (FSK) modulation
  fidelity are explicitly out of scope (new backlog row **F1**, §1.2/§4).
- **(d) Deterministic unit + integration tests**, zero regression M1-M17.
- **(e) Real openMSX A/B evidence** (§2.6) — no parity claim without a genuine capture; the Kanji
  device gives the strongest, content-bearing parity opportunity in this milestone.
- **(f) Full deferred-backlog review** — every row A.B1-B9, B.C1-C10, C.D1-D7, D.E1-E2 re-affirmed
  with a one-line justification (§4), per the human's explicit instruction.

### 1.2 Out of scope (named explicitly, each with justification)

| Deferred item | Why OUT of M18 | Owning milestone (proposed) |
|---|---|---|
| **Cassette tape image-format/signal fidelity** — real `.CAS`/`.WAV`/`.TSX` decode/encode (`references/openmsx-21.0/src/cassette/{CasImage,WavImage,TsxImage,TsxParser}.*`), realistic FSK/UDF bit modulation, save-to-host-file. | M18 delivers the CPU-visible register CONTRACT (motor/output/input bits, deterministic in-memory synthetic bitstream) only — mirrors the M14 VDP contract/rendering-depth split (D1-D7) and the M17 YM2413 contract/DSP-depth split (E1). Building a real audio-file decoder is genuine, large, separate scope nobody has asked for yet. | Proposed new backlog row **F1** — future audio/tape-format milestone, paired with C9 (SDL3 frontend) since real tape files are a presentation/asset concern. |
| **Printer image/ESC-sequence rendering depth** — dot-matrix font rendering, ESC command interpretation, PNG page output (`references/openmsx-21.0/src/Printer.cc`'s `ImagePrinter`/`ImagePrinterMSX`). | M18 delivers the CPU-visible port CONTRACT (strobe/data/busy protocol) + a deterministic raw-byte capture sink only. Rendering a virtual printed page is presentation-layer depth, not the register contract. | Proposed new backlog row **F2** — future rendering-depth milestone, paired with C9. |
| **Halnote/MSX-JE firmware (slot 0-3), 16 KB SRAM wiring** | Backlog B6, unchanged; unrelated to peripheral I/O. | Halnote/MSX-JE milestone (indicative M20) |
| **Cartridge loading (external slots 1/2)** | Backlog B7, unchanged; unrelated. | Cartridge-loading milestone |
| **FDC flux/DMK depth, VDP rendering depth, exact-cycle timing, ZEXALL, Sony speed/pause + Ren-Sha, SDL3 frontend, YM2413 DSP/write-timing** | Unchanged from prior milestones' deferrals (§4). | Per §4 candidate owners |

### 1.3 Assumptions (each with a verification action)

- **A-M18-1 (device identity: `MSXKanji`, not `MSXKanji12`).** The HB-F1XV's Kanji font device is
  openMSX class `Kanji` (`MSXKanji`, `references/openmsx-21.0/src/MSXKanji.hh/.cc`), a **directly
  port-mapped** device on `#D8-#DB` (`switch (port & 0x03)` dispatch, `MSXKanji.cc:34,53,72`) — **not**
  `MSXKanji12` (`references/openmsx-21.0/src/MSXKanji12.hh/.cc`), which is a **switched/expanded I/O**
  device (`MSXSwitchedDevice`, device ID `0xF7`, ports `#40-#4F`, `MSXKanji12.cc:10,14`) used by other
  machines (e.g. Hangul/Lascom variants), not the HB-F1XV. *Verify:* the XML confirms directly —
  `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:29-38`: `<Kanji id="Kanji ROM">` with four
  explicit `<io base=.. num=.. type=..>` entries at `0xD8/0xD9/0xDA/0xDB` — the `Kanji` class name and
  direct port bases match `MSXKanji` exactly; there is no `<Kanji12>` element anywhere in this
  machine's XML. **No discrepancy found** (contrast the M17/DEC-0012 precedent, where a mismatch WAS
  found) — see §2.7 for the full verification writeup.
- **A-M18-2 (Kanji ROM size selects the non-Lascom, non-Hangul, 256 KB code path).**
  `MSXKanji`'s constructor (`MSXKanji.cc:9-24`) requires `rom.size()` to be `0x20000` (128 KB) or
  `0x40000` (256 KB); `isLascom` is `true` only when a `<type>lascom</type>` XML child is present
  (absent here — `Sony_HB-F1XV.xml:29-38` has no `<type>` child), and `highAddressMask` is `0x7F` only
  for `<type>hangul</type>` (also absent), else `0x3F`. The local asset
  (`docs/asset-checksums.txt:14`, 262,144 bytes = `0x40000`) selects the **256 KB, non-Lascom,
  non-Hangul** code path — the richest variant (`readIO`/`peekIO` case 3 requires `rom.size() ==
  0x40000` exactly, `MSXKanji.cc:82`). *Verify:* the KanjiFontRom device is constructed with
  `highAddressMask = 0x3F`, `isLascom = false`, and the full 256 KB path is exercised (both `adr1`'s
  128 KB JIS1 half and `adr2`'s 128 KB JIS2 half at offset `0x20000`).
- **A-M18-3 (exact address-latch protocol, cited byte-for-byte).** Grounded in `MSXKanji.cc:32-88`:
  - `reset()`: `adr1 = 0x00000`, `adr2 = 0x20000` (`MSXKanji.cc:28-29`).
  - `#D8` (write, `port&3==0`): `adr1 = (adr1 & 0x1F800) | ((value & 0x3F) << 5)` — low-address byte.
  - `#D9` (write, `port&3==1`): `adr1 = (adr1 & 0x007E0) | ((value & 0x3F) << 11)` — high-address byte.
  - `#DA` (write, `port&3==2`): `adr2 = (adr2 & 0x3F800) | ((value & 0x3F) << 5)`.
  - `#DB` (write, `port&3==3`): `adr2 = (adr2 & 0x207E0) | ((value & 0x3F) << 11)` — note the mask
    `0x207E0` **preserves bit 17 (`0x20000`)**, so `adr2` never leaves the JIS2 half via a normal
    write; this is exactly how a 256 KB ROM's two 128 KB halves stay addressed independently by two
    separate counters that share one physical image.
  - `#D9` (read, `port&3==1`): returns `rom[adr1 & (rom.size()-1)]`, THEN auto-increments the low 5
    bits only: `adr1 = (adr1 & ~0x1F) | ((adr1+1) & 0x1F)` (wraps within one 32-byte glyph block,
    `MSXKanji.cc:59-60,79`).
  - `#DB` (read, `port&3==3`): returns `rom[adr2]` **only when `rom.size()==0x40000`** (our case,
    `MSXKanji.cc:82-84`, "temp workaround" comment preserved), then auto-increments `adr2`'s low 5
    bits identically (`MSXKanji.cc:62-63`).
  - `#D8`/`#DA` reads: return open-bus `0xFF`, no increment (the `isLascom` fallthrough branch that
    would make `#D8` also readable does not apply here, `MSXKanji.cc:54-58`).
  *Verify (unit tests, §3):* assert every one of the six behaviors above against a synthetic,
  byte-distinguishable in-memory 256 KB image (no dependency on real glyph content for pure-protocol
  tests); a separate machine-level check reads real content from `bios/f1xvkfn.rom` (§3, M18-S4).
- **A-M18-4 (no clock dependency for Kanji — pure combinational device).** Like the M13 `RomDevice`
  and the M17 `Ym2413Opll` register model, `KanjiFontRom` has no time-dependent state; reads/writes
  are pure functions of the stored `adr1`/`adr2` counters and the static image. No
  `elapsed_cycles()`-driven clock adapter is needed (mirrors M17's §2.4 "no new clock consumer"
  reasoning) — this keeps the CPU-timing-oracle regression surface (M9/M12) untouched by construction.
- **A-M18-5 (printer port claim is `#90-#97`, not just `#90/#91` — see §2.7 flagged finding).**
  `Sony_HB-F1XV.xml:74-78`: `<PrinterPort id="Printer Port"><io base="0x90" num="8" type="IO"/>
  <bidirectional>true</bidirectional><unused_bits>0x00</unused_bits></PrinterPort>` — the device
  claims **eight** ports (`0x90-0x97`) directly (not a chipset-level `IoBus` mirror pointing at a
  smaller base claim, unlike the VDP `#98-9B`→`#9C-9F` or PPI `#A8-AB`→`#AC-AF` aliases, which ARE
  separate `register_mirror()` entries — S1985 fact-sheet §7/§10). Internally the device dispatches
  on `port & writePortMask`; because `<bidirectional>true</bidirectional>` is present,
  `writePortMask = 0x03` (`MSXPrinterPort.cc:18`, `getChildDataAsBool("bidirectional", false) ? 0x03 :
  0x01`) and `readPortMask = writePortMask` (no `<status_readable_on_all_ports>` child,
  `MSXPrinterPort.cc:19`). *Verify:* the KanjiFontRom analogy applies — attach the single `PrinterPort`
  device instance at all eight ports `0x90..0x97` in `wire_bus()`, with internal dispatch on
  `port & 0x03`.
- **A-M18-6 (printer write protocol, byte-for-byte, and the falling-edge capture semantic).**
  `MSXPrinterPort.cc:46-62`: write `port&3==0` → `setStrobe(value & 1)` (bit0 = strobe); write
  `port&3==1` → `writeData(value)`; write `port&3==2` → no-op; write `port&3==3` → PDIR/bidirectional
  register, explicitly **not implemented** even by openMSX itself (`MSXPrinterPort.cc:57`, "0x93 PDIR
  (BiDi) is not implemented" — our device matches this exact scope limit, not a gap on our part).
  Byte capture happens on the strobe **falling edge** (`Printer.cc:59-66`,
  `PrinterCore::setStrobe`: `if (!strobe && prevStrobe) { write(toPrint); }` — new-low while
  previous-was-high), i.e. software writes data first, then pulses strobe low
  (`references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §8: "BIOS LPTOUT pulses STROBE low
  after writing data"). **Architectural simplification (disclosed):** openMSX splits this across a
  `Connector`/`Pluggable` pair (`MSXPrinterPort` the bus device + a separately-pluggable
  `PrinterCore`/`ImagePrinterMSX`); this single-machine emulator has no pluggable-peripheral framework
  (nothing else in `src/` uses one), so `PrinterPort` folds both the port protocol AND the
  falling-edge byte-capture into one class — the same simplification precedent M15's `Ppi8255`
  already used by folding `PpiSlotSelect` behavior directly rather than modeling a separate slot-select
  connector. *Verify:* unit test writes `data` then toggles strobe high→low→high and asserts exactly
  one byte lands in `captured_bytes()`; writing data with strobe already low, or toggling low→low
  (no edge), captures nothing.
- **A-M18-7 (printer status/busy default — a deliberate, disclosed divergence from openMSX's
  *unplugged* default; see §2.6 for how this is kept A/B-clean).** `PrinterPortDevice.hh:12-17`:
  status `false`=ready, `true`=not-ready/busy; `DummyPrinterPortDevice::getStatus`
  (`DummyPrinterPortDevice.cc:5-8`) returns `true` (busy) always — openMSX's *default unplugged*
  state. Our `PrinterPort` **is** the built-in port (no separate pluggable/unplugged state exists in
  this design), so it reports **ready (busy=0)** by default — matching `PrinterCore::getStatus`
  (`Printer.cc:54-57`, returns `false`) i.e. the behavior of an openMSX run **with a printer actually
  plugged in**, not the bare default. With `unused_bits=0x00` (`Sony_HB-F1XV.xml:77`), `peekIO` for
  `port&3==0` (`#90`,`#94`) returns `0x00` (ready) or `0x02` (busy, bit1 set) exactly per
  `MSXPrinterPort.cc:36-44`. *Verify:* documented explicitly in code comments and in the A/B plan
  (§2.6) so a status-bit divergence, if the probe ever touches it, is never mistaken for a bug; the
  A/B probe is designed to avoid depending on this bit's value (write-path-only parity).
- **A-M18-8 (cassette has no dedicated CPU-visible port — confirmed from the XML).**
  `Sony_HB-F1XV.xml:21`: `<CassettePort/>` is an **empty** element with no `<io>` child, unlike every
  other device in this XML (Kanji, PPI, VDP, PSG, RTC, PrinterPort all declare explicit `<io>`
  entries). This matches the fact-sheet's description exactly (§8: "Cassette: CMI read (96), CMO write
  (97), REM motor (98); controlled via **PPI port C bits 4-5** and read via **PSG R14 bit 7**" — no
  cassette-specific I/O port exists; the signal lines are wired through the PPI and PSG, which already
  own their ports). *Verify:* `CassetteInterface` does **not** implement `core::IoDevice` and is
  **not** attached to `io_bus_` at any port.
- **A-M18-9 (cassette motor/output derive read-only from the existing M15 `Ppi8255`; no PPI edit
  needed).** `Ppi8255` (`src/devices/chipset/ppi_8255.h:64-66`) already exposes
  `cassette_motor_off()` (port C bit4, "1 = off") as a named accessor, and `port_c_latch()`
  (`ppi_8255.h:62`) exposes the full port-C byte, from which bit5 (CMO/cassette-write-data, S1985
  fact-sheet §3 Port C table: "bit 5: cassette write data (1 = high) → CMO pin") is directly readable
  as `port_c_latch() & 0x20` with **zero edit to `Ppi8255`**. Mirroring the M13 "`MemoryMapperRam` is a
  pure CONSUMER" pattern (`docs/m13-planner-package.md` §4.3), `CassetteInterface` holds a
  `const Ppi8255&` reference and computes `motor_on()`/`output_level()` **on demand**, live, with no
  duplicated state and no new push/notification mechanism. *Verify:* unit test constructs
  `CassetteInterface` over a real `Ppi8255`, writes `#AA` with various bit4/bit5 patterns, and asserts
  `motor_on()`/`output_level()` track immediately.
- **A-M18-10 (cassette input bit is source-injected into the EXISTING `JoystickPorts`, replacing the
  M15 hardcoded stub, with a nullptr-safe default that exactly preserves the M15 golden).**
  `src/peripherals/joystick.h:41-42` documents `kCassetteInputBit` as "idle high, inert in M15 — tape
  transport is deferred, backlog C7" — this is the literal seam M18 closes. A new small interface
  `CassetteInputSource` (defined in `joystick.h`, the **consumer's** header — mirroring how
  `PsgPortSource` is defined in `psg_ym2149.h`, the PSG's own header, per the existing precedent) is
  added; `JoystickPorts` gains `attach_cassette_input_source(CassetteInputSource*)` and a
  `CassetteInputSource* cassette_source_ = nullptr;` member. `read_port_a()`'s bit7 becomes: unattached
  (`nullptr`) → unconditionally `1` (byte-for-byte identical to the current M15 code, the regression
  guard); attached → `cassette_source_->cassette_input_high() ? 1 : 0`. *Verify:* a dedicated unit test
  asserts the unattached path is bit-for-bit unchanged from the pre-M18 behavior (regression guard),
  and the attached path reflects the injected source's live value.
- **A-M18-11 (deterministic synthetic tape — cycle-driven, not wall-clock; explicitly not a real
  audio-file decoder).** `CassetteInterface::cassette_input_high()` with no tape loaded returns `true`
  (idle-high), matching `DummyCassetteDevice::readSample()` returning `32767`
  (`DummyCassetteDevice.cc:10-13`) which the real comparator (`CassettePort.cc:103`,
  `return sample >= 0;`) converts to `true` — i.e. our no-tape default is the SAME externally-observed
  idle state as vanilla openMSX's default. With a tape loaded (`load_synthetic_tape(bits,
  cycles_per_bit)`), the currently-read bit is `bits[(clock_source_->cpu_cycles() -
  start_cycle_) / cycles_per_bit]` (clamped to `true`/idle past the end) — driven **read-only** off an
  injected `CassetteClockSource` (X4 pattern, mirrors `RtcClockSource`/`FdcClockSource` in
  `src/machine/hbf1xv_machine.h:264-284`), never the host wall clock, never perturbing CPU T-state
  accounting. Real `.CAS`/`.WAV`/`.TSX` decode/encode is explicitly out of scope (backlog **F1**,
  §1.2).
- **A-M18-12 (no production hook added to the CPU stepping loop — pull-only design, lowest regression
  risk).** Cassette input is consulted **pull-style** (only when `JoystickPorts::read_port_a()` is
  actually invoked by a CPU `IN A,(#A2)` with R14 selected) and cassette output/motor are pure,
  on-demand derivations from `Ppi8255::port_c_latch()` (A-M18-9) — **no new call site is added to
  `Hbf1xvMachine::step_cpu_instruction()`/`run_cycles()`/`run_frame()`**. This mirrors M17's explicit
  low-risk stance ("No new clock consumer... Do not add one speculatively") and keeps the
  CPU-timing-sensitive stepping code paths (guarded since M9/M12) completely untouched by this
  milestone. *Consequence:* there is no always-on, per-cycle "recorded transition log" for the
  cassette write path in production; deterministic output-transition capture (for tests/tools) is
  built as an explicit, caller-driven sampling utility (§2.4), not a background hook.
- **A-M18-13 (asset presence).** `bios/f1xvkfn.rom` is confirmed present and exactly 262,144 bytes
  (`docs/asset-checksums.txt:14`). No new BIOS asset requirement beyond loading this existing file via
  the M13 `RomAssetLoader` (already-established mechanism, `src/machine/rom_asset_loader.h`).
  *Verify:* `tools/validate-assets.ps1`.
- **A-M18-14 (Kanji ROM SHA identity — a disclosed, open verification item, not a blocker).** The
  XML cites `hb-f1xv_kanjifont.rom` with SHA1 `218d91eb6df2823c924d3774a9f455492a10aecb`
  (`Sony_HB-F1XV.xml:32`); our local file's checksum is only recorded as **SHA256**
  (`docs/asset-checksums.txt:14`, `3c9f60fda2...`), so byte-identity with the exact XML-cited revision
  is **not yet confirmed** (per the established M13 A-1 discipline: "asset→slot identity is by role/
  size, not by SHA"). *Verify (developer, before claiming Kanji-content A/B parity, §2.6):* compute the
  local file's SHA1 and compare; if it differs (as several other assets in this project's XML comments
  already note, e.g. "different than EPROM dump, but confirmed on Meits's machine"), treat any
  byte-level content divergence at specific glyph addresses as an **expected, disclosed** difference —
  never silently hidden, never claimed as a bug, and never allowed to invalidate the *protocol*-level
  parity (address-latch math), only the specific glyph *content* bytes.

---

## 2. Spec Summary

### 2.1 `src/` placement (per `src/CLAUDE.md`)

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/kanji/kanji_font_rom.h` / `.cpp` (**new folder**, alongside `src/devices/memory/`, `src/devices/audio/`) | The Kanji font I/O device: `adr1`/`adr2` counters, `#D8-#DB` two-counter address-latch + auto-increment protocol, read-only 256 KB image. Implements `core::IoDevice`. Placed under `src/devices/` (not `src/peripherals/`) because it is a ROM-adjacent **chip** reachable via I/O (analogous to the M13 `RomDevice` and M17 `Ym2413Opll` placement precedent), not a human-facing peripheral. | `MSXKanji.hh/.cc` (A-M18-1..4); `src/CLAUDE.md` ("`src/devices/` — chip and controller implementations... I/O") |
| `src/peripherals/printer_port.h` / `.cpp` (**new**, alongside `keyboard_matrix.*`, `joystick.*`) | The Centronics printer port: strobe/data write protocol, deterministic ready/busy status, falling-edge byte capture. Implements `core::IoDevice` (peripherals implementing device-side contracts is an established precedent — `JoystickPorts` already implements `devices::audio::PsgPortSource`). Placed under `src/peripherals/` because it is a physical external-connector peripheral (matches `src/CLAUDE.md`'s "slot-side peripheral adapters"), not an internal chip. | `MSXPrinterPort.hh/.cc`, `PrinterPortDevice.hh`, `Printer.cc` (A-M18-5..7) |
| `src/peripherals/cassette_interface.h` / `.cpp` (**new**) | The cassette port: motor/output derivation from `Ppi8255` (read-only consumer, A-M18-9), the deterministic synthetic-tape input model (A-M18-11), implements the new `CassetteInputSource` contract consumed by `JoystickPorts`. **Not** a `core::IoDevice` (A-M18-8). | `CassettePort.hh/.cc`, `CassetteDevice.hh`, `DummyCassetteDevice.cc` (A-M18-8..12); S1985 fact-sheet §2/§3/§8 |

Machine wiring (in `src/machine/`, existing files, additive only):
- `src/machine/hbf1xv_machine.{h,cpp}` — add members `devices::kanji::KanjiFontRom kanji_font_rom_;`,
  `peripherals::PrinterPort printer_;`, `peripherals::CassetteInterface cassette_{ppi_};` (declared
  after `ppi_`, mirroring the existing `Ppi8255 ppi_{slot_bus_, keyboard_};` ordering rule), and a
  nested `CassetteClock` class (X4 pattern, mirrors `RtcClock`/`FdcClock` at
  `hbf1xv_machine.h:262-284`) plus a `CassetteClock cassette_clock_{scheduler_};` member. In
  `wire_bus()`: attach `kanji_font_rom_` at `0xD8..0xDB` (4 calls) and `printer_` at `0x90..0x97` (8
  calls, loop); call `cassette_.attach_clock_source(&cassette_clock_)` and
  `joystick_.attach_cassette_input_source(&cassette_)` near the existing
  `psg_.attach_port_source(&joystick_);` line. In `cold_boot()`: add `kanji_font_rom_.reset();
  printer_.reset(); cassette_.reset();` alongside the other device resets. In `load_rom_assets()`: add
  the Kanji font ROM load (`RomAssetLoader::Spec{"f1xvkfn.rom", 0x40000, "Kanji font ROM (I/O
  #D8-#DB)"}`) and `kanji_font_rom_.set_image(...)`. Add accessors `kanji()`, `printer()`, `cassette()`
  (const/non-const), mirroring the `ym2413()`/`rtc()` pattern. **No edit** to `step_cpu_instruction()`,
  `run_cycles()`, or any CPU-timing code path (A-M18-12).
- `src/peripherals/joystick.{h,cpp}` — additive edit (A-M18-10): new `CassetteInputSource` interface,
  `attach_cassette_input_source()`, nullptr-safe bit7 computation. Regression-guarded by an explicit
  "unattached path unchanged" unit test.
- **No edit** to `src/devices/chipset/ppi_8255.{h,cpp}` — `CassetteInterface` consumes the already-
  public `cassette_motor_off()`/`port_c_latch()` accessors read-only (A-M18-9); this is a deliberate,
  lower-risk choice (zero touch to M15-verified PPI code).

CMakeLists.txt: add `src/devices/kanji/kanji_font_rom.cpp`, `src/peripherals/printer_port.cpp`,
`src/peripherals/cassette_interface.cpp` to `add_library(sony_msx_core ...)`.

Boundary compliance: all three new classes carry zero slot/bus-composition knowledge of their own
(the Kanji device just answers `#D8-#DB`; the printer just answers `#90-#97`; the cassette interface
reads an injected `Ppi8255&` reference and a small clock-source contract) — the machine performs all
composition, matching the M15-M17 precedent.

### 2.2 Kanji font I/O model — what "`#D8-#DB`" means operationally

**Storage.** A `std::vector<std::uint8_t>` 256 KB image (loaded via `RomAssetLoader`, A-M18-13), plus
two address counters `adr1_` (18-bit-ish, addresses the JIS1 half `[0x00000, 0x1FFFF]`) and `adr2_`
(addresses the JIS2 half `[0x20000, 0x3FFFF]`, bit 17 pinned by construction — A-M18-3).

**Protocol** (byte-exact per A-M18-3): four ports, dispatch on `port & 0x03`:

| Port | Direction | Effect |
|---|---|---|
| `#D8` | write | `adr1_low5..10 = value & 0x3F` (low address byte) |
| `#D9` | write | `adr1_bits11..16 = value & 0x3F` (high address byte) |
| `#D9` | read | returns `rom[adr1_]`, then increments `adr1_`'s low 5 bits (wraps in one 32-byte glyph block) |
| `#DA` | write | `adr2_low5..10 = value & 0x3F` |
| `#DB` | write | `adr2_bits11..16 = value & 0x3F` (bit 17 preserved — stays in JIS2 half) |
| `#DB` | read | returns `rom[adr2_]` (256 KB path), then increments `adr2_`'s low 5 bits |
| `#D8`, `#DA` | read | open-bus `0xFF`, no side effect |

This exact XML↔protocol correspondence is unusually clean (§2.7): the XML's `type="O"`-only entries
for `#D8`/`#DA` and the separate explicit `type="I"` entries for `#D9`/`#DB` map one-to-one onto
`MSXKanji.cc`'s `readIO`/`peekIO` switch cases that actually produce a result versus fall through to
`0xFF` — there is no ambiguity to resolve.

**Operational test contract (no clock, no CPU-fetch dependency):**
1. Write `#D8=lo, #D9=hi` for a chosen `adr1_`, read `#D9` N times, assert each returned byte equals
   the synthetic image at the expected (auto-incrementing, 32-byte-wrapping) address.
2. Same for `#DA`/`#DB`/`adr2_`, additionally asserting the JIS2 half offset (`0x20000`) is never lost
   across writes.
3. Assert `#D8`/`#DA` reads are always `0xFF` regardless of prior writes.
4. `reset()` restores `adr1_=0x00000`, `adr2_=0x20000`.
5. (Machine-level, §3 M18-S4) load the **real** `bios/f1xvkfn.rom` and spot-check a handful of known
   offsets return the real file's bytes (via the checksum-recorded file, not fabricated content).

### 2.3 Printer port model — Centronics `#90-#97`

**Storage.** `strobe_` (bool, idle=`true`=high), `data_` (`std::uint8_t`, last-written byte),
`captured_bytes_` (`std::vector<std::uint8_t>`, deterministic append-only capture log).

**Protocol** (A-M18-5/A-M18-6), dispatch on `port & 0x03` across the claimed 8-port range:

| Port (mod 4) | Direction | Effect |
|---|---|---|
| `0` (`#90`,`#94`) | write | `set_strobe(value & 1)` — falling edge (high→low) appends `data_` to `captured_bytes_` |
| `0` (`#90`,`#94`) | read | status byte: `unused_bits(0x00) | (busy ? 0b10 : 0)` — busy is always `false` in M18 (A-M18-7) |
| `1` (`#91`,`#95`) | write | `data_ = value` |
| `1` (`#91`,`#95`) | read | open-bus `0xFF` (no separate `type="I"` entry for `#91` in the XML) |
| `2` (`#92`,`#96`) | write/read | no-op / `0xFF` |
| `3` (`#93`,`#97`) | write/read | PDIR/bidirectional — **not implemented**, matching openMSX's own scope limit (A-M18-6) |

**Determinism.** Purely combinational (no clock dependency, mirrors A-M18-4's reasoning for the Kanji
device). `reset()` sets `strobe_=true`, `data_=0`, and does **not** trigger a capture (no edge is
observed at reset, since the values are set directly rather than through the edge-detecting setter).

**Deterministic output capture (debug convention).** `captured_bytes()` exposes the raw vector for
tests. A machine-level helper `Hbf1xvMachine::write_printer_capture(filename)` writes the captured
bytes as a deterministic, fixed-width, uppercase-hex ASCII listing (one row of N bytes, `'\n'`
terminated — matching the existing "every serializer is hand-rolled ASCII... byte-for-byte identical"
convention documented at `hbf1xv_machine.h:190-196`) to `<debug_root>/printer/<filename>`, reusing the
existing `write_text_file` static helper and the `debug/` subfolder convention
(`agent-protocol/project-baseline.md`: "Subfolders may be added under `debug/` as necessary to capture
frames, events, and audio elements" — a printer-capture subfolder is a natural, small, justified
extension of that same rule). This is a debug/introspection aid, not a rendered "printed page" (that
depth is backlog **F2**).

### 2.4 Cassette interface model — deterministic operational definition

**What "deterministic" means here (per the task's explicit ask to define this, not assume it):**

1. **Write path (motor/output) — pure, on-demand derivation, no owned state.**
   `motor_on() = !ppi_.cassette_motor_off()`; `output_level() = (ppi_.port_c_latch() & 0x20) != 0`
   (CMO/MIC pin, S1985 fact-sheet §3). Both are computed live from the referenced `Ppi8255`
   (A-M18-9) — there is nothing to "advance" or desynchronize; two identical instruction streams
   produce identical values at every read, trivially.
2. **Read path (cassette input, CMI, feeding PSG R14 bit 7) — cycle-driven, not wall-clock.**
   Default (**no tape loaded**): `cassette_input_high() == true` always — this is the exact
   externally-observed idle state of vanilla openMSX's own default (`DummyCassetteDevice`,
   A-M18-11), so this milestone changes **nothing observable** for anyone not loading a synthetic
   tape (regression-safe default).
   With a tape loaded via `load_synthetic_tape(std::vector<bool> bits, std::uint64_t
   cycles_per_bit)`: the interface records `start_cycle_ = clock_source_->cpu_cycles()` at load time;
   every subsequent read computes `index = (clock_source_->cpu_cycles() - start_cycle_) /
   cycles_per_bit` and returns `index < bits.size() ? bits[index] : true` (past-end reverts to idle
   high, deterministically). `clock_source_` is the injected `CassetteClockSource` (X4 pattern,
   read-only `elapsed_cycles()`, A-M18-11/A-M18-12) — never the host wall clock.
   **Why a synthetic bitstream, not a real tape-file format:** this gives a fully deterministic,
   host-independent, directly-testable "prerecorded tape" (two runs at the same cycle produce the same
   bit, trivially provable in a unit test) without requiring real WAV/CAS/TSX decoding — the same
   scope-boundary logic M16 used when it shipped a synthetic/deterministic 720 KB disk image rather
   than needing real flux emulation (backlog C10, still deferred). Real tape-file fidelity is
   captured as new backlog row **F1** (§1.2/§4), explicitly not fabricated here.
3. **Output/transition capture for tests — explicit, caller-driven sampling, not a production hook.**
   Per A-M18-12, there is no always-on per-cycle recorder in `Hbf1xvMachine`'s stepping loop. Tests
   that want a deterministic transition trace call `CassetteInterface::sample_output(std::uint64_t
   at_cycle)` themselves after each simulated write (a small, explicit, opt-in utility method that
   appends `(at_cycle, motor_on(), output_level())` to an in-memory log) — this keeps
   `step_cpu_instruction()`/`run_cycles()` byte-for-byte unmodified while still giving tests a genuine,
   deterministic way to observe the write-path over time when they choose to.
4. **`reset()`** ejects any loaded tape (reverts to the idle-high default) and clears the sample log —
   deterministic, no host-file dependency.

### 2.5 Determinism (hard requirement, cross-cutting)

- Kanji font device and printer port: **no clock dependency at all** (A-M18-4, §2.3) — pure
  combinational functions of stored state, identical to the M13 `RomDevice`/M17 `Ym2413Opll`
  no-clock-consumer classification. Lowest possible regression risk on the CPU-timing-oracle axis.
- Cassette interface: the **only** new clock consumer in this milestone, and it is READ-ONLY off
  `elapsed_cycles()` via the `CassetteClockSource` X4 pattern (mirrors `RtcClockSource`/
  `FdcClockSource` exactly) — never wall-clock, never perturbing CPU T-state accounting
  (`step_cpu_instruction()` is untouched, A-M18-12).
- No new production per-instruction hook is added anywhere (A-M18-12) — a deliberate, disclosed,
  lower-risk architectural choice for this milestone, explicitly called out (mirrors M17's explicit
  "this is a lower-risk milestone on the CPU-timing-oracle axis" framing).
- Missing-asset policy for the Kanji ROM reuses the exact M13 A-7 discipline (0xFF-fill + logged
  diagnostic note on absence/wrong-size, never silent, never fabricated) via the existing
  `RomAssetLoader` — no new asset-loading code path is invented.

### 2.6 openMSX A/B acceptance plan

Three genuine, bounded subjects, each grounded in what is actually CPU-readable:

1. **Kanji font content parity (primary, strongest subject).** `#D9`/`#DB` are genuinely
   CPU-readable and content-bearing (real glyph bytes, not just architectural bookkeeping). Probe:
   write `#D8/#D9` (and `#DA/#DB`) to select a handful of representative addresses spanning both the
   JIS1 and JIS2 halves, `IN A,(#D9)` / `IN A,(#DB)` repeatedly (exercising the auto-increment), and
   diff PC/registers/flags per instruction against openMSX exactly as `tools/openmsx-io-parity.ps1`
   already does (same harness class as M11-M17). Because **both** sides read the same underlying file
   role (`bios/f1xvkfn.rom` vs. the XML's `hb-f1xv_kanjifont.rom`), a genuine content-level check is
   possible — **conditioned on A-M18-14's SHA1 verification**: if the local file's SHA1 matches the
   XML-cited `218d91eb...`, expect a full empty diff including the read byte VALUES; if it differs (a
   different but role-equivalent dump revision, as several other assets in this project already are),
   report the specific-byte content divergence honestly as an **expected, disclosed** difference (per
   A-M18-14), not a bug, while still confirming the *addressing protocol* itself is architecturally
   identical (same PC/register trace shape).
2. **Printer write-path parity (write-only technique, mirrors M17's YM2413 approach).** Because
   reading printer status (A-M18-7) is a **deliberate, known divergence point** (our default = ready,
   vanilla openMSX's default = not-ready/unplugged), the probe exercises **only** the write path:
   `OUT (#91),data` / `OUT (#90),strobe-pattern` covering several data bytes and strobe transitions,
   diffing PC/registers/flags per instruction (never reading `#90`'s status bit). This is a genuine,
   clean, comparable subject with no expected divergence. *(Optional stretch, developer's call, not
   required for acceptance: if the WSL openMSX install can be configured with an actual plugged-in
   printer via CLI, e.g. `-printer`, verify whether that also reports ready/busy=false, which would
   let a status-bit read be included too — attempt only if practical; report BLOCKED/skip honestly
   rather than fabricating a CLI flag that doesn't exist.)*
3. **Cassette idle-state + write-path parity.** With no synthetic tape loaded (the default,
   A-M18-11), read PSG R14 via `#A2` and confirm bit7=1 on both sides (genuine, since **both**
   defaults are idle-high — unlike the printer's asymmetric default, this is directly comparable).
   Separately, write representative bit4/bit5 patterns to PPI port C (`#AA`) and diff the
   architectural write-sequence trace (mirrors technique 2). Real tape-content-driven parity has no
   comparable openMSX-side artifact without also constructing a matching WAV/CAS file for openMSX —
   out of scope (folds into backlog **F1**).

**Mechanics.** Extend the existing `tools/openmsx-io-parity.ps1`-class harness (per M11-M17 precedent)
with new probe binaries — recommend one combined `tests/parity/m18_peripheral_io_probe.bin`
(Kanji-content subject first, then printer write-path, then cassette write+idle-read), matching the
M15 style of one combined multi-device A/B trace, and one artifact `docs/m18-parity-trace-diff.md`.
Reuse the adversarial comparator self-check (empty-side → BLOCKED, corrupted-field → DIVERGENCE) as in
every prior milestone. **Hard rule (unchanged):** no parity claim without a genuine captured diff; if
openMSX cannot be driven, report BLOCKED honestly.

### 2.7 Flagged findings — backlog-wording vs. XML verification (per the task's explicit ask)

Following the DEC-0012 precedent (verify before implementing, don't silently trust backlog shorthand):

- **B5 (Kanji-font I/O `#D8-#DB`) — NO discrepancy found.** The backlog's port range and mechanism
  description match the XML and `MSXKanji.cc` exactly (A-M18-1/A-M18-3); the alternative
  `MSXKanji12`/switched-I/O-ID-`0xF7` device (used by *other* MSX machines, not this one) was
  explicitly checked and ruled out. **No coordinator/human confirmation needed for B5** — implementable
  as backlog-described.
- **C7 (printer `#90/#91` + cassette interface) — one minor, LOW-severity wording gap found (flagged,
  non-blocking).** The backlog text says "`#90/#91`", but the actual XML claim is **`#90-#97`** (eight
  ports, `io base="0x90" num="8"`, A-M18-5) — the device answers all eight directly (not via a
  separate chipset-level mirror). This is materially different in **severity** from the M17/DEC-0012
  finding: that case risked *building the wrong device entirely* (a fabricated SRAM/bank-register
  overlay on hardware that doesn't have one); this case is purely a **documentation-precision gap** —
  implementing the backlog's narrower "`#90/#91`" literally would under-claim the real decode footprint
  (leaving `#92-#97` unmapped/open-bus when real hardware maps them to the same two functional
  registers via mod-4 aliasing), which is a **strictly worse, less-accurate** outcome than following
  the XML. **Recommendation (not gated on human confirmation, unlike DEC-0012):** implement the
  accurate `#90-#97` claim per the XML/openMSX grounding (A-M18-5/A-M18-6); recommend the coordinator
  update the backlog C7 row text from "`#90/#91`" to "`#90-#97` (functional registers at `#90`/`#91`;
  `#92-#97` alias via mod-4 dispatch; `#93` PDIR/bidirectional unimplemented, matching openMSX's own
  scope limit)" at ledger-update time. This does **not** require pausing implementation for a decision
  entry, because — unlike DEC-0012 — no fabrication risk exists either way; it is a strict accuracy
  improvement in the same direction the backlog already intended.
- **Cassette interface wording — NO discrepancy.** The backlog names no specific ports for the
  cassette interface (generic "cassette interface"), consistent with the XML's port-less
  `<CassettePort/>` (A-M18-8) — nothing to reconcile.
- **Incidental, OUT-OF-SCOPE observation (not an M18 defect, mentioned for completeness only).** The
  RTC's XML claim is also `io base="0xB4" num="4"` (i.e., `#B4-#B7`), but the current M15
  implementation only attaches `rtc_` at `#B4`/`#B5` in `wire_bus()`
  (`src/machine/hbf1xv_machine.cpp:109-110`) — `#B6`/`#B7` are currently unmapped. This is a
  **pre-existing M15 residual**, unrelated to the Kanji/printer/cassette devices this milestone
  builds, and is **not** touched by M18 (touching M15's RTC wiring would need its own justification/
  decision if ever prioritized). Recorded here only for transparency per the task's "surface
  discrepancies" instruction; not a blocker, not part of B5/C7, and explicitly **not** an M18 action
  item.

---

## 3. Milestones (Slice Plan M18-S1 … M18-S5)

Every slice runs the full evidence-gate set (§5, evidence gates table) and must leave `ctest` green.

### M18-S1 — Kanji font ROM I/O device (`#D8-#DB`)
- **Goal:** `KanjiFontRom` as a `core::IoDevice`: `adr1_`/`adr2_` counters, the exact six-behavior
  protocol (§2.2/A-M18-3), `reset()` (A-M18-3), debug accessors `address1()`/`address2()` for tests.
- **Files:** `src/devices/kanji/kanji_font_rom.{h,cpp}`; `CMakeLists.txt`.
- **Unit tests** (`tests/unit/devices/kanji/kanji_font_rom_unit_test.cpp`, synthetic
  byte-distinguishable 256 KB image, no file dependency): address-latch writes land correctly for both
  `adr1_`/`adr2_`; auto-increment wraps within the 32-byte block; `adr2_` bit 17 (JIS2 half) never lost
  across writes; `#D8`/`#DA` reads are always `0xFF`; `reset()` restores the documented defaults;
  two-run determinism (identical write/read sequence → identical results).
- **Gates:** build + ctest green.

### M18-S2 — Printer port device (`#90-#97`)
- **Goal:** `PrinterPort` as a `core::IoDevice`: strobe/data write protocol (A-M18-5/A-M18-6),
  falling-edge byte capture, deterministic ready/busy status (A-M18-7), `reset()`.
- **Files:** `src/peripherals/printer_port.{h,cpp}`; `CMakeLists.txt`.
- **Unit tests** (`tests/unit/peripherals/printer_port_unit_test.cpp`): data-then-strobe-pulse
  captures exactly one byte; strobe held low (no edge) captures nothing; multiple pulses capture
  multiple bytes in order; status reads `0x00` (ready) by default with `unused_bits=0x00`; ports
  `#92/#93/#96/#97` (mod-4 cases 2/3) are inert/open-bus per A-M18-6; `#94-#97` alias the same
  behavior as `#90-#93` (mod-4 dispatch); `reset()` does not spuriously capture; two-run determinism.
- **Gates:** build + ctest green.

### M18-S3 — Cassette interface peripheral + `JoystickPorts` integration
- **Goal:** `CassetteInterface` (motor/output derivation from `Ppi8255`, synthetic-tape input model,
  `CassetteInputSource` implementation) and the additive `JoystickPorts` edit (A-M18-10) wiring the
  cassette-input bit into PSG R14 bit7, replacing the M15 hardcoded stub.
- **Files:** `src/peripherals/cassette_interface.{h,cpp}` (new); `src/peripherals/joystick.{h,cpp}`
  (edit, additive); `CMakeLists.txt`.
- **Unit tests:**
  - `tests/unit/peripherals/cassette_interface_unit_test.cpp` — motor/output track a real `Ppi8255`'s
    port-C writes live (A-M18-9); no-tape default is always idle-high; loading a synthetic tape and
    advancing an injected fake `CassetteClockSource` returns the expected bit sequence at the expected
    cycle boundaries; past-end-of-tape reverts to idle-high; `reset()` ejects the tape; two-run
    determinism (same cycle sequence → same bit sequence).
  - Extend `tests/unit/peripherals/joystick_unit_test.cpp` with: unattached `cassette_source_` (the
    default) reproduces the exact pre-M18 R14 bit7 behavior byte-for-byte (**regression guard**);
    attached source's live value is reflected in `read_port_a()`'s bit7.
- **Gates:** build + ctest green.

### M18-S4 — Machine wiring + system integration (all three devices)
- **Goal:** attach `kanji_font_rom_` at `#D8-#DB`, `printer_` at `#90-#97`; wire
  `cassette_.attach_clock_source(&cassette_clock_)` and
  `joystick_.attach_cassette_input_source(&cassette_)`; add all three to the `cold_boot()` reset
  sequence; load the real `bios/f1xvkfn.rom` via `RomAssetLoader` into `kanji_font_rom_`; expose
  `kanji()`/`printer()`/`cassette()` accessors.
- **Files:** `src/machine/hbf1xv_machine.{h,cpp}` (edit, additive only); `CMakeLists.txt`.
- **Integration tests** (`tests/integration/machine/hbf1xv_m18_peripheral_io_integration_test.cpp`,
  needs `SONY_MSX_BIOS_DIR` for the real Kanji ROM): a real CPU program drives `OUT (#D8),A` /
  `OUT (#D9),A` / `IN A,(#D9)` over the M11 system bus and reads back **real** `bios/f1xvkfn.rom`
  content at a handful of known offsets (byte-exact, sourced from the checksummed file, never
  fabricated); a CPU program drives the printer write protocol over the bus and
  `machine.printer().captured_bytes()` reflects it; a CPU program writes PPI port C bit4/bit5 patterns
  and reads PSG R14 via the bus, confirming `machine.cassette()`'s live derivation and the idle-high
  default; **zero regression** on M1-M17 suites (in particular the M15 `JoystickPorts`/`Ppi8255`
  goldens and the M9/M12 CPU-timing oracles, untouched by construction per A-M18-4/A-M18-12).
- **Gates:** build + ctest green (full suite).

### M18-S5 — openMSX A/B parity + backlog finalization
- **Goal:** capture the three-subject A/B evidence (§2.6); verify the Kanji ROM's SHA1 identity
  (A-M18-14) and report the finding honestly either way; finalize the full deferred-backlog review
  (§4) in the ledger; refresh checksums.
- **Files:** `tools/openmsx-peripheral-io-parity.ps1` (new, mirrors `tools/openmsx-io-parity.ps1`
  structure) or an extension of the existing script; `tests/parity/m18_peripheral_io_probe.bin`;
  `docs/m18-parity-trace-diff.md`; refreshed `docs/asset-checksums.txt`.
- **Tests:** the S4 integration test serves as the deterministic golden the A/B probe mirrors.
- **Gates:** all four **plus** the A/B gate. No parity claim without a genuine capture; if the Kanji
  ROM's SHA1 differs from the XML-cited revision, report the specific content divergence honestly
  (A-M18-14) rather than fabricating a match or hiding the difference.

---

## 4. Full Deferred-Backlog Review (mandatory, per DEC-0005 and the human's explicit instruction)

Source of truth: `agent-protocol/state/deferred-backlog.md`. **Every** row — A.B1-B9, B.C1-C10,
C.D1-D7, D.E1-E2 — re-affirmed below with a one-line justification, per the human directive "review
deferred items and have them properly addressed during development." Nothing is silently dropped.

### Section A (human-directive-tracked rows)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| B1 | PSG/YM2149 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, no M18 impact (though M18 DOES change who feeds PSG R14 bit7 — see B1 note below). |
| B2 | RTC/RP5C01 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, no M18 impact. |
| B3 | FM-PAC (OPLL YM2413) device internals | DONE (M17) — unchanged | Closed at M17; re-affirmed. |
| B4 | MSX-JE 16 KB SRAM | OPEN — re-owned to B6 (DEC-0012), unchanged | No M18 impact; unrelated to peripheral I/O. |
| B5 | Kanji-font I/O `#D8-#DB` | **CLOSES this cycle (M18)** | `KanjiFontRom` device (§2.2) wired to the real `#D8-#DB` I/O ports, grounded in `MSXKanji.hh/.cc` (confirmed correct device identity vs. `MSXKanji12`, §2.7); reads the real 256 KB `bios/f1xvkfn.rom`. No discrepancy found between backlog wording and XML (§2.7). |
| B6 | Halnote/MSX-JE firmware mapping (slot 0-3) | OPEN — unchanged | Unrelated to peripheral I/O; still the confirmed true owner of B4's 16 KB SRAM (DEC-0012). |
| B7 | Cartridge loading | OPEN — unchanged | External slots 1/2; unrelated to M18. |
| B8 | FDC drive mechanics | DONE (M16) — unchanged | Closed at M16; re-affirmed. |
| B9 | VRAM/V9958 VDP | DONE (M14) — unchanged | Closed at M14; re-affirmed. |

**B1 note (informational, not a disposition change):** M18 changes bit7 of the register B1 already
owns (PSG R14) from a hardcoded idle-high constant to a source-injected value (A-M18-10) — this is a
**consumer-side** change (the cassette interface now supplies the bit), not a change to B1's own
register/tone/noise/envelope/mixer model, which is untouched. B1 stays DONE.

### Section B (other tracked deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| C1 | Exact cycle/T-state timing parity | OPEN — unchanged | M18 introduces no new CPU-timing-affecting clock consumer (§2.5); companion rows E2 (YM2413 write-timing) and C1 itself remain grouped for a future timing-fidelity milestone. |
| C2 | Z80 HALT-R increment | OPEN — unchanged | Per DEC-0004; unrelated to M18. |
| C3 | ZEXDOC/ZEXALL full sweep | OPEN — unchanged | Needs a legally-sourced ZEX binary; unrelated to M18. |
| C4 | S1985 backup-RAM `.sram` persistence | DONE (M15) — unchanged | Closed at M15; re-affirmed. |
| C5 | Full boot past first device read | IN-PROGRESS (carried from M16) — unchanged | M18 adds no CPU-visible boot-path interaction; the documented M16 residual (auto-disk-boot trigger investigation) is not this milestone's job, per M16's closure terms (DEC-0011). |
| C6 | Keyboard matrix + joystick | DONE (M15) — unchanged | Closed at M15; re-affirmed (the joystick file gets an ADDITIVE M18 edit for cassette-input injection, A-M18-10, but the joystick/keyboard behavior itself is unchanged and regression-guarded). |
| C7 | Printer `#90/91` + cassette interface | **CLOSES this cycle (M18)** | `PrinterPort` device (§2.3) wired at the real `#90-#97` claim (a minor, non-blocking backlog-wording precision gap found and resolved in favor of accuracy, §2.7) and `CassetteInterface` peripheral (§2.4) wired through the existing `Ppi8255`/`JoystickPorts` seams. DSP/rendering/tape-format depth explicitly split out as new rows **F1**/**F2** (not a partial close — mirrors the M14/M17 contract-vs-depth precedent). |
| C8 | Sony speed-controller + hardware PAUSE + Ren-Sha Turbo | OPEN — unchanged | HB-F1XV-specific companion-chip behavior; unrelated to M18. |
| C9 | SDL3 frontend | OPEN — unchanged | Presentation layer; will eventually consume the printer's captured output / cassette's tape I/O once F1/F2 exist — not this milestone. |
| C10 | FDC flux-level/DMK fidelity | OPEN — unchanged | Unrelated to M18 (FDC, not peripheral I/O). |
| D1 | Pixel-accurate raster rendering | OPEN — unchanged | VDP rendering depth; unrelated to M18. |
| D2 | Sprite rendering + collision | OPEN — unchanged | Unrelated to M18. |
| D3 | VDP command engine | OPEN — unchanged | Unrelated to M18. |
| D4 | Cycle-accurate VDP access-slot/command timing | OPEN — unchanged | Unrelated to M18. |
| D5 | YJK/YAE color decode + DAC | OPEN — unchanged | Unrelated to M18. |
| D6 | Horizontal scroll/interlace/blink/superimpose | OPEN — unchanged | Unrelated to M18. |
| D7 | G6/G7 VRAM planar interleave | OPEN — unchanged | Unrelated to M18. |
| E1 | YM2413 FM-synthesis DSP/audio depth | OPEN — unchanged | Unrelated to M18 (audio device, not peripheral I/O). |
| E2 | YM2413 register-write timing constraint | OPEN — unchanged | Unrelated to M18. |

### New rows proposed this cycle (Section E, mirroring the M14 D1-D7 and M17 E1-E2 contract/depth splits)

| # | Item | Status | Candidate owner | Grounding |
|---|---|---|---|---|
| **F1** | **Cassette tape image-format/signal fidelity** — real `.CAS`/`.WAV`/`.TSX` decode/encode, realistic FSK/UDF bit modulation and timing, save-to-host-file. M18 delivers the CPU-visible register contract (motor/output/input bits) + a deterministic in-memory synthetic bitstream only (§2.4); zero real audio-file support. | OPEN (new) | Future audio/tape-format milestone, paired with C9 (SDL3 frontend) | `references/openmsx-21.0/src/cassette/{CasImage,WavImage,TsxImage,TsxParser,CassettePlayer}.*`; S1985 fact-sheet §8 |
| **F2** | **Printer image/ESC-sequence rendering depth** — dot-matrix font rendering, ESC command interpretation, virtual page → PNG output (openMSX's `ImagePrinter`/`ImagePrinterMSX`). M18 delivers the CPU-visible port contract (strobe/data/busy protocol) + deterministic raw-byte capture only (§2.3); zero rendering. | OPEN (new) | Future rendering-depth milestone, paired with C9 | `references/openmsx-21.0/src/Printer.cc` (`ImagePrinter`/`ImagePrinterMSX`); S1985 fact-sheet §8 |

**Backlog bookkeeping note (to be executed at ledger-update time, not in this planning artifact):** on
M18 closure, mark B5 `DONE (M18)` and C7 `DONE (M18)`; add F1/F2 as new OPEN rows under a new
"Section E" heading (continuing the D1-D7/E1-E2 lettering convention); optionally refine C7's row text
per §2.7's non-blocking wording-precision recommendation (`#90/#91` → `#90-#97`).

---

## 5. Acceptance Criteria (M18 exit)

1. `KanjiFontRom` device (address-latch + auto-increment protocol over `#D8-#DB`, 256 KB image,
   `reset()`) implemented under `src/devices/kanji/`, grounded in `MSXKanji.hh/.cc` with confirmed
   device-identity verification (not `MSXKanji12`, §2.7) in the implementation report. *(S1)*
2. `PrinterPort` device (strobe/data write protocol, falling-edge byte capture, deterministic
   ready/busy status) implemented under `src/peripherals/`, wired at the real `#90-#97` claim
   (§2.7 finding). *(S2)*
3. `CassetteInterface` peripheral (motor/output derivation from `Ppi8255`, deterministic synthetic-tape
   input model, `CassetteInputSource` contract) implemented under `src/peripherals/`, with the
   additive, regression-guarded `JoystickPorts` edit (A-M18-10) replacing the M15 hardcoded cassette-
   input stub. *(S3)*
4. Machine wiring: all three devices attached/reset in `Hbf1xvMachine`; the real `bios/f1xvkfn.rom`
   loaded via the existing M13 `RomAssetLoader`; **no edit** to `step_cpu_instruction()`/
   `run_cycles()`/`ppi_8255.{h,cpp}` (A-M18-9/A-M18-12, deliberately low-risk). *(S4)*
5. Deterministic unit + system-integration tests cover every new behavior; two identical runs produce
   byte-identical device/capture state. *(S1-S4)*
6. Real openMSX A/B evidence captured for all three subjects (`docs/m18-parity-trace-diff.md`),
   including an honest disposition of the Kanji-ROM SHA1 identity check (A-M18-14) — no parity claim
   without a genuine capture; any expected/disclosed divergence (printer status default, A-M18-7;
   possible Kanji glyph-content revision mismatch, A-M18-14) is documented, not hidden. *(S5)*
7. **Zero regression** across M1-M17 suites, including the M15 `JoystickPorts`/`Ppi8255` goldens
   (unattached-cassette-source regression guard, A-M18-10) and the M9/M12 CPU-timing oracles
   (untouched by construction, §2.5). *(S4, S5)*
8. Every deferred-backlog row (A.B1-B9, B.C1-C10, C.D1-D7, D.E1-E2) re-affirmed with justification
   (§4); B5 and C7 close; F1/F2 proposed as new rows; the C7 wording-precision finding recorded
   (§2.7, non-blocking). *(§4, this package)*
9. Evidence gates executed and captured each cycle (validate-assets, checksum, Debug build, ctest).
10. QA sign-off recorded before closure (`docs/m18-qa-signoff.md`). **Normal human-release-decision
    gate — no auto-close** (per the task brief and DEC-0003's auto-close grant being M12-only);
    autopilot pauses at QA sign-off for the separate human release decision + tag (bundled/sequenced
    with M17's own still-pending release decision per the human's directive, but each is its own gate).

---

## 6. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|---------------------|
| R-M18-1 | Misidentifying the Kanji device as `MSXKanji12` (switched I/O, ID `0xF7`) instead of `MSXKanji` (direct `#D8-#DB`), producing a device that answers the wrong port mechanism entirely. | A-M18-1/§2.7 ground the correct class directly from the machine XML (no `<Kanji12>` element, no `<type>` child); implementation report must cite `Sony_HB-F1XV.xml:29-38` and `MSXKanji.cc` (not `MSXKanji12.cc`) as the behavioral reference. |
| R-M18-2 | Kanji address-latch masking/increment errors (e.g. incrementing the full counter instead of only the low 5 bits, or losing `adr2_`'s bit 17 across a write) — easy to get subtly wrong given the multi-mask bit-packing. | A-M18-3 cites the exact openMSX source lines for all six port behaviors; S1 unit tests explicitly assert the 32-byte wrap boundary and the JIS2-half bit-17 preservation across writes. |
| R-M18-3 | Printer port range under-claimed (only `#90/#91`, matching the backlog's literal wording) instead of the real `#90-#97` XML claim, silently leaving `#92-#97` as inert open-bus when real software might probe the mirror pattern. | §2.7 explicitly flags and resolves this in favor of the XML/openMSX-grounded `#90-#97` claim; S2 unit tests assert `#94-#97` alias `#90-#93`'s behavior via the mod-4 dispatch. |
| R-M18-4 | Printer status-bit default divergence (our "always ready" vs. openMSX's default "always busy/unplugged") gets accidentally exercised by the A/B probe, producing a confusing false-looking DIVERGENCE that is actually an expected, disclosed difference. | A-M18-7/§2.6 explicitly design the printer A/B probe to avoid reading status; the divergence is documented in code comments and the parity report proactively, not discovered as a surprise. |
| R-M18-5 | Cassette-input injection (A-M18-10) accidentally changes the unattached-source default (e.g. a sign/polarity slip), silently breaking the M15 boot-checkpoint or A/B goldens that depend on R14 bit7's idle-high value. | A dedicated regression-guard unit test (S3) asserts the unattached path is byte-for-byte identical to the pre-M18 `read_port_a()` output; the M18-S4 integration test re-runs the M15 boot-checkpoint-class assertions to confirm no drift. |
| R-M18-6 | Scope creep into real tape-file decoding or printer image rendering (tempting given how rich the openMSX reference classes are — `CasImage`/`WavImage`/`TsxImage`/`ImagePrinterMSX`). | §1.2 and new backlog rows F1/F2 explicitly fence this out; any expansion requires a decisions-ledger entry per guardrails "Scope Control". |
| R-M18-7 | Kanji ROM content-level A/B parity is claimed without actually checking whether the local `bios/f1xvkfn.rom` is byte-identical to the XML-cited revision (SHA1 `218d91eb...`), risking either a false PASS claim or a confusing unexplained DIVERGENCE. | A-M18-14 requires the developer to compute and compare the SHA1 before claiming Kanji-content parity in S5; any mismatch is reported as an honest, disclosed content difference, not hidden or fabricated as a match. |
| R-M18-8 | Adding a new clock consumer (`CassetteClockSource`) is mistakenly wired into the CPU stepping loop (a push/polling hook) rather than being purely pull-based, inflating the CPU-timing-oracle regression surface unnecessarily. | A-M18-12 explicitly mandates the pull-only design; the implementation report must show zero edits to `step_cpu_instruction()`/`run_cycles()`/`run_frame()`, and the M9/M12 CPU-timing oracle tests must remain unchanged/green as direct evidence. |
| R-M18-9 | `PrinterPort`'s single-class simplification (folding the port-protocol device and the falling-edge byte-capture logic together, versus openMSX's separate `MSXPrinterPort`/`PrinterCore` pair) is mistaken for an incomplete/non-conformant implementation during review. | A-M18-6 explicitly documents this as a disclosed architectural simplification consistent with the existing `Ppi8255`/`PpiSlotSelect`-folding precedent, with citations to both openMSX source files it merges. |

---

## 7. Developer Handoff

- **Start at M18-S1** (Kanji font ROM device) — grounded per A-M18-1/A-M18-2/A-M18-3; cite
  `references/openmsx-21.0/src/MSXKanji.cc` line ranges in code comments (never copy the code itself —
  GPL isolation).
- **Sequence S1→S5** in order; each runs the full four-gate evidence set; keep `ctest` green every
  cycle.
- **`src/` placement fixed by §2.1:** `src/devices/kanji/kanji_font_rom.{h,cpp}` (ROM-adjacent chip),
  `src/peripherals/printer_port.{h,cpp}` and `src/peripherals/cassette_interface.{h,cpp}` (external
  connector peripherals); machine wiring only in `src/machine/hbf1xv_machine.{h,cpp}` (additive: three
  new members + a nested `CassetteClock` class + `wire_bus()`/`cold_boot()`/`load_rom_assets()`
  additions — no change to existing device wiring). One small, explicitly regression-guarded additive
  edit to `src/peripherals/joystick.{h,cpp}` (A-M18-10). **No edit** to
  `src/devices/chipset/ppi_8255.{h,cpp}` (A-M18-9) or any CPU-timing code path (A-M18-12).
- **Critical device-identity finding to honor (A-M18-1/§2.7):** this machine's Kanji device is
  `MSXKanji` (direct `#D8-#DB` ports) — **not** `MSXKanji12` (switched I/O, ID `0xF7`). Do not build a
  switched-I/O dispatch mechanism for this device; it doesn't apply here.
- **C7 wording-precision finding (§2.7):** implement the printer at the real `#90-#97` XML claim, not
  the backlog's narrower "`#90/#91`" shorthand — this is a non-blocking accuracy improvement, not a
  scope change requiring a decision entry.
- **No new production stepping-loop hook (A-M18-12):** the cassette clock source is consulted
  pull-style only; do not add a per-instruction or per-cycle polling call anywhere in
  `Hbf1xvMachine::step_cpu_instruction()`/`run_cycles()`/`run_frame()`.
- **A/B (§2.6):** build/extend a peripheral-I/O parity script mirroring `tools/openmsx-io-parity.ps1`;
  verify the Kanji ROM's SHA1 (A-M18-14) before claiming content-level parity; design the printer probe
  to avoid the known status-bit default divergence (A-M18-7); if any mechanism cannot be verified
  against real WSL openMSX, report BLOCKED honestly and still deliver the subjects that do work.
- **Ledger discipline (DEC-0005):** on closure, mark B5 and C7 `DONE (M18)`; add new rows F1/F2 under
  a new "Section E" heading; optionally refine C7's row text per §2.7; update
  `state/current-phase.md` and `state/milestones.md`.
- **Gate:** NORMAL human-release-decision gate — do not auto-close; pause at QA sign-off for the
  separate human release decision + tag (per the task brief and DEC-0003's M12-only auto-close scope).
  M17's own release decision/tag remains separately pending — do not conflate the two.
- Deliverables: source per §2.1; tests per §3; `docs/m18-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`; an implementation report `docs/m18-implementation-report.md`; then hand
  to QA for `docs/m18-qa-signoff.md`.
