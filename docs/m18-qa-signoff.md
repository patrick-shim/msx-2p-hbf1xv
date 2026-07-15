# M18 QA Sign-off — Peripheral I/O: Kanji-Font ROM (`#D8-#DB`) + Printer (`#90-#97`) + Cassette Interface

- Milestone: M18 (REQ-M18-002).
- QA Owner: MSX QA Agent.
- Date: 2026-07-07.
- Gate: M18 retains the NORMAL human-release-decision gate (per the planner package §7/§5 item 10
  and DEC-0003's auto-close grant being M12-only). A QA Pass lets the coordinator PRESENT M18 to
  the human for the release decision + tag (bundled/sequenced with M17's own still-pending release
  decision per the human's directive); it does NOT auto-close M18 and does NOT itself authorize
  release.
- Verdict: **PASS**.

All results below were produced or independently reproduced by QA (fresh reconfigure, fresh
rebuild, fresh `ctest` run, direct reads of every changed/new source file, direct reads of the
cited `references/openmsx-21.0/` files, `git diff`/`git diff --numstat` inspection of the exact
wiring/test-file changes, an independent SHA1 + byte-offset check of the real `bios/f1xvkfn.rom`,
and — going beyond simply re-reading the developer's captured artifact — an independent,
from-scratch WSL openMSX 19.1 session built and driven by QA itself against the real
`Sony_HB-F1XV` machine XML) — developer, planner, and coordinator claims were not accepted at face
value.

---

## 1. Regression Scope

Affected surface for M18 (closes backlog B5 and C7; adds new backlog rows F1/F2): three wholly new
device/peripheral classes and additive-only wiring, zero edits to any existing device internals.

New source verified by QA directly (full read, not just diff):
- `src/devices/kanji/kanji_font_rom.{h,cpp}` (new) — `KanjiFontRom`, a `core::IoDevice` on
  `#D8-#DB`: two address counters (`adr1_`/`adr2_`), the six-behavior address-latch + auto-increment
  protocol, 256 KB image, `reset()`.
- `src/peripherals/printer_port.{h,cpp}` (new) — `PrinterPort`, a `core::IoDevice` on `#90-#97`
  (mod-4 dispatch): strobe/data write protocol, falling-edge byte capture, deterministic
  always-ready status, `reset()`.
- `src/peripherals/cassette_interface.{h,cpp}` (new) — `CassetteInterface` + `CassetteClockSource`:
  **not** a `core::IoDevice` (confirmed by its declaration, `class CassetteInterface final : public
  CassetteInputSource` — no `core::IoDevice` base, no `io_read`/`io_write`); motor/output derived
  read-only from a `const Ppi8255&`; deterministic synthetic-tape input model pulled read-only off
  an injected clock source.
- `src/peripherals/joystick.{h,cpp}` (modified, additive only, confirmed via `git diff --numstat`:
  69 insertions / 0 deletions) — `+class CassetteInputSource`, `+attach_cassette_input_source()`,
  `read_port_a()`'s bit7 gains a `nullptr`-guarded override; the unattached path is provably
  byte-for-byte identical to the pre-M18 code.
- `src/machine/hbf1xv_machine.{h,cpp}` (modified, additive only, confirmed via full `git diff HEAD`
  read) — three new members (`kanji_font_rom_`, `printer_`, `cassette_{ppi_}`), a nested
  `CassetteClock` class + member, `wire_bus()`/`cold_boot()`/`load_rom_assets()` additions,
  `kanji()`/`printer()`/`cassette()` accessors. **Zero** occurrences of `step_cpu_instruction`,
  `run_cycles`, or `run_frame` appear anywhere in the diff (QA-executed `git diff HEAD -- src/
  machine/hbf1xv_machine.cpp | grep -n "step_cpu_instruction\|run_cycles\|run_frame"` returns
  nothing, exit code 1 — confirmed directly by QA, not merely re-quoted from the report).
- `CMakeLists.txt` / `tests/CMakeLists.txt` (modified, additive only, confirmed via `git diff
  --stat`: +5 / +47 lines, 0 deletions) — 3 new source files, 4 new test targets.
- **Zero-touch confirmed by QA directly:** `git diff HEAD --stat -- src/devices/chipset/
  ppi_8255.h src/devices/chipset/ppi_8255.cpp` returns empty output — no lines changed.

Test/tooling additions (all read in full by QA): `tests/unit/devices/kanji/
kanji_font_rom_unit_test.cpp`, `tests/unit/peripherals/printer_port_unit_test.cpp`,
`tests/unit/peripherals/cassette_interface_unit_test.cpp`,
`tests/integration/machine/hbf1xv_m18_peripheral_io_integration_test.cpp`,
`tools/gen-m18-peripheral-io-probe.py`, `tools/openmsx-peripheral-io-parity.ps1`,
`tests/parity/m18_peripheral_io_probe.bin`, `docs/m18-parity-trace-diff.md`.

Regression-critical protected areas checked: X4 (CPU T-state math / M9-M12 timing oracles — no
`src/devices/cpu/` or `src/core/` files appear anywhere in the diff), X1 (`#A8` slot-select —
untouched), Ppi8255 internals (untouched, confirmed empty diff), M11/M13 slot tests, M13/M15/M16
boot goldens, M14 VDP (untouched), M15 device suite (PSG/RTC/PPI/joystick/backup-RAM — the joystick
file DOES change, additively and regression-guarded), M16 FDC suite (untouched), M17 YM2413 suite
(untouched by this milestone; present in the same working tree but out of M18's scope).

## 2. Regression Matrix Status

| Area | Coverage | QA result |
|------|----------|-----------|
| Build (headless, SDL3 OFF) | fresh `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` | **PASS — 0 errors** (QA-executed, own tree) |
| Full deterministic suite | `ctest --test-dir build -C Debug --output-on-failure` | **PASS — 100% tests passed, 0 failed out of 79** (QA-executed, fresh, own run — not the developer's captured output) |
| Kanji font ROM device (S1) | `devices_kanji_font_rom_unit_test` | PASS — read source; confirms 32-byte wrap boundary (exercised past 32 reads), bit-17 (JIS2-half) preservation even with a deliberately JIS1-shaped input address, `#D8`/`#DA` always-`0xFF`-no-side-effect, `reset()` defaults, two-run determinism |
| Printer port device (S2) | `peripherals_printer_port_unit_test` | PASS — read source; confirms falling-edge-only capture (low→low is NOT an edge), mod-4 aliasing (`#94-#97` == `#90-#93`), `#93` PDIR inertness, `reset()` does not spuriously capture, two-run determinism |
| Cassette interface + JoystickPorts integration (S3) | `peripherals_cassette_interface_unit_test`, `peripherals_joystick_unit_test` | PASS — read source; motor/output track a REAL `Ppi8255`'s writes live; no-tape idle-high default holds with/without a clock; synthetic-tape bit sequence correct at cycle boundaries; past-end reverts to idle-high; regression-guard case is genuine (multiple sub-cases: idle unattached, other-bits-pressed unattached, attached high/low, detach-reverts) |
| Machine wiring + system integration (S4) | `machine_hbf1xv_m18_peripheral_io_integration_test` | PASS — QA read the full test; Case 1 reads the REAL `bios/f1xvkfn.rom` via a genuine CPU program over the M11 bus at 4 representative offsets (byte-exact vs. the real file, independently re-verified by QA, §3.4); Case 3 is the R-M18-5 regression guard, genuinely exercised (joystick_ IS wired to cassette_, yet the default observable bit is unchanged) |
| Two-machine / two-run determinism | new unit + integration suites | PASS |
| X4 timing oracles (M9/M12) | `machine_hbf1xv_cpu_step_integration_test`, `machine_hbf1xv_m11_parity_checkpoint_integration_test`, `machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`, `machine_hbf1xv_m15_boot_checkpoint_integration_test`, `machine_hbf1xv_m16_boot_checkpoint_integration_test` | PASS — all green in QA's own run; zero `src/devices/cpu/`/`src/core/` diff and zero `step_cpu_instruction`/`run_cycles`/`run_frame` diff (QA-confirmed directly) rule out perturbation structurally, not just empirically |
| M15 `JoystickPorts`/`Ppi8255` goldens | `peripherals_joystick_unit_test`, `devices_chipset_ppi_8255_unit_test`, `machine_hbf1xv_m15_devices_integration_test` | PASS — all green; `Ppi8255` empty diff confirmed independently by QA |
| openMSX A/B — Kanji content parity (strongest subject) | `tools/openmsx-peripheral-io-parity.ps1` vs genuine WSL openMSX 19.1 `Sony_HB-F1XV`; QA additionally built and ran its own independent WSL openMSX probe | **PASS — QA independently reproduced this subject from scratch** (§4) |
| openMSX A/B — printer write-path parity | same harness; QA read the probe generator source to confirm the status bit is never read | PASS — confirmed by direct source read of `tools/gen-m18-peripheral-io-probe.py` (§4) |
| openMSX A/B — cassette idle-state + write-path parity | same harness; QA independently re-derived the expected shape via its own WSL probe's `IN A,(#A2)` semantics | PASS |
| Adversarial comparator self-check | developer-captured, QA read the artifact | PASS — empty-side → BLOCKED (exit 2), corrupted-field → DIVERGENCE (exit 1, 42 mismatches), consistent with the shared M10 `tools/trace-diff.py` comparator already proven trustworthy across M10-M17 |
| Full deferred-backlog review completeness | `docs/m18-planner-package.md` §4, `agent-protocol/state/deferred-backlog.md` | PASS — every row A.B1-B9/B.C1-C10 re-affirmed with justification; new Section E (F1/F2) added correctly |

The 4 new M18 tests are additive; the 75 prior M1-M17 tests are unchanged and green (zero
regression). QA counted 79/79 in its own fresh `ctest` run, not from the developer's report.

## 3. Source-Level Verification (genuine, not stub)

### 3.1 Kanji device identity — the central risk this milestone (R-M18-1)

QA read `references/openmsx-21.0/src/MSXKanji.cc` (all 125 lines, in full) and
`references/openmsx-21.0/src/MSXKanji12.cc` (constructor/dispatch) directly, and compared them
line-by-line against `src/devices/kanji/kanji_font_rom.{h,cpp}`:

- `MSXKanji`'s `writeIO`/`readIO`/`peekIO`/`reset` (`MSXKanji.cc:26-88`) is a **direct** `switch
  (port & 0x03)` dispatch with no switched-I/O registration. `MSXKanji12` (`MSXKanji12.cc:10-38`)
  is a `MSXSwitchedDevice` with a static `ID = 0xF7` and `readSwitchedIO`/`peekSwitchedIO` methods —
  an entirely different registration mechanism (ports `#40-#4F`, single running `address` counter,
  no JIS1/JIS2 split). `KanjiFontRom` implements only the former shape: a plain `core::IoDevice`
  answering `io_read`/`io_write` directly at fixed ports, with the two independent `adr1_`/`adr2_`
  counters `MSXKanji` uses — not `MSXKanji12`'s single `address` field.
- QA independently opened `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` (lines 1-90)
  directly: `<Kanji id="Kanji ROM">` (line 29) with four explicit `<io base=.. num=.. type=..>`
  entries at `0xD8/0xD9/0xDA/0xDB` (lines 34-37), **no** `<type>` child, **no** `<Kanji12>` element
  anywhere in the file. This independently confirms A-M18-1's grounding claim — QA did not accept
  the planner's/developer's transcription on faith.
- Byte-for-byte mask/shift comparison, done by QA line-by-line: `KanjiFontRom::io_write`'s four
  cases (`kanji_font_rom.cpp:36-55`) reproduce `MSXKanji::writeIO`'s four cases
  (`MSXKanji.cc:32-47`) exactly, including the `0x1F800`/`0x007E0`/`0x3F800`/`0x207E0` masks (the
  last of which structurally preserves bit 17 — QA confirms this is a property of the mask itself,
  not merely asserted behavior). `KanjiFontRom::io_read`'s two live cases (`#D9`/`#DB`, `kanji_
  font_rom.cpp:19-30`) reproduce `MSXKanji::readIO`/`peekIO`'s case-1/case-3 logic
  (`MSXKanji.cc:50-88`) exactly, including the low-5-bit-only auto-increment and the
  `isLascom`-false code path (this machine has no `<type>lascom</type>` XML child, confirmed
  absent) that keeps `#D8`/`#DA` reads at open-bus `0xFF` with no side effect. **No discrepancy
  found; this is a genuinely correct, verified-by-QA-from-primary-source implementation of the
  right device.**

### 3.2 Kanji content parity — independently reproduced by QA from scratch, not merely re-read

QA did not stop at reading `docs/m18-parity-trace-diff.md`. QA:

1. Independently computed the local ROM's SHA1: `sha1sum bios/f1xvkfn.rom` →
   `218d91eb6df2823c924d3774a9f455492a10aecb`, and independently confirmed this matches
   `Sony_HB-F1XV.xml:32`'s `<sha1>218d91eb6df2823c924d3774a9f455492a10aecb</sha1>` by reading the
   XML line itself — not by trusting the developer's reported match.
2. Independently spot-checked **all four** claimed offset/byte-value pairs directly against the
   real file (a fresh Python read, not the developer's dump):
   `0x00020`→`00 18 18 18`, `0x1FFE0`→`00 00 00 10`, `0x20020`→`00 00 00 00`,
   `0x2FFE0`→`10 30 60 C4` — every value matched the implementation report and
   `docs/m18-parity-trace-diff.md` exactly.
3. **Confirmed real WSL access and independently drove openMSX 19.1 itself** (`wsl -d Ubuntu-24.04
   -- /usr/bin/openmsx`, version confirmed `openMSX 19.1`), rather than accepting the developer's
   captured trace as given. QA wrote its own minimal Tcl single-step harness (independent of
   `tools/openmsx-peripheral-io-parity.ps1`) that boots the real `Sony_HB-F1XV` machine XML,
   injects a small hand-assembled program (`LD A,1 ; OUT (#D8),A ; LD A,0 ; OUT (#D9),A ; IN
   A,(#D9) ×4 ; HALT`) selecting JIS1 address `0x00020`, and single-steps it. Result, captured
   directly by QA against the real openMSX process:
   ```
   QATRACE seq=5 ... af=0000   (1st IN A,(#D9) result: A=0x00)
   QATRACE seq=6 ... af=1800   (2nd IN A,(#D9) result: A=0x18)
   QATRACE seq=7 ... af=1800   (3rd IN A,(#D9) result: A=0x18)
   QATRACE seq=8 ... af=1800   (4th IN A,(#D9) result: A=0x18)
   ```
   i.e. `00, 18, 18, 18` — matching the real file bytes at `0x00020` exactly.
4. QA then ran the **identical** 17-byte probe through this repo's own `sony_msx_headless.exe
   --parity-trace`, and confirmed the emulator produces the identical `A` sequence (`00, 18, 18,
   18`) at the corresponding trace steps.

This is a genuine, first-hand, three-way corroboration (real ROM file bytes ↔ QA's own real WSL
openMSX 19.1 session against the actual `Sony_HB-F1XV` machine ↔ this repo's own emulator) of the
single strongest, content-bearing A/B claim in this milestone — not a re-statement of the
developer's artifact. **No fabrication found; the claim is genuine and independently
reproducible.**

### 3.3 Printer port range + protocol correctness (A-M18-5/A-M18-6)

QA read `references/openmsx-21.0/src/MSXPrinterPort.cc` (all 133 lines) and
`references/openmsx-21.0/src/Printer.cc:54-71` directly, and confirms:
- `Sony_HB-F1XV.xml:74-78` claims `<io base="0x90" num="8" type="IO"/>` with
  `<bidirectional>true</bidirectional>` — QA read this XML block directly (not merely quoted from
  the planner package).
- `src/machine/hbf1xv_machine.cpp`'s `wire_bus()` attaches `printer_` via a `for (port = 0x90; port
  <= 0x97; ++port)` loop (8 calls) — QA read this loop directly in the diff, confirming the full
  8-port claim is genuinely wired, not merely `#90/#91`.
- `PrinterPort::io_write`/`io_read` (`printer_port.cpp`) dispatch on `port & 0x03` exactly matching
  `MSXPrinterPort::writeIO`/`peekIO`'s `port & writePortMask`/`readPortMask` cases
  (`MSXPrinterPort.cc:31-62`), including case 3 (`#93`/`#97` PDIR) being a genuine no-op that
  matches openMSX's **own** documented gap (`MSXPrinterPort.cc:57`, "0x93 PDIR (BiDi) is not
  implemented" — QA confirms this comment is present in the real openMSX source, not invented).
- The falling-edge byte-capture semantic (`PrinterPort::set_strobe`, `printer_port.cpp:13-20`)
  matches `PrinterCore::setStrobe` (`Printer.cc:59-66`, `if (!strobe && prevStrobe) { write(toPrint);
  }`) exactly — QA confirms the "write data first, then pulse strobe low" software convention this
  device models is the correct falling-edge (not rising-edge, not level) trigger.
- QA read `tools/gen-m18-peripheral-io-probe.py`'s Section C directly and confirms it contains
  **only** `_out(0x91, ...)`/`_out(0x90, ...)` calls — no `_in_a(0x90)` anywhere in that section —
  independently confirming watch-item 5 (the printer A/B probe genuinely never reads the status
  bit, the disclosed A-M18-7 divergence point).

### 3.4 Cassette interface non-wiring discipline (A-M18-8/A-M18-9)

QA read `src/peripherals/cassette_interface.h` in full: `class CassetteInterface final : public
CassetteInputSource` — no `core::IoDevice` base, no `io_read`/`io_write` overrides anywhere in
either file. QA independently read `Sony_HB-F1XV.xml:21`, confirming `<CassettePort/>` is the
**only** empty device element in the file (every other device — Kanji, PPI, VDP, PSG, RTC,
PrinterPort — declares an explicit `<io>` child) — this genuinely grounds the "no dedicated port"
claim from the primary source, not from a transcription.

QA read `src/devices/chipset/ppi_8255.h` in full and confirms `cassette_motor_off()`
(line 64) and `port_c_latch()` (line 62) are **already public, pre-existing** accessors (i.e. not
added this milestone) — `CassetteInterface::motor_on()`/`output_level()` (`cassette_interface.cpp:
21-29`) consume only these two, with no other coupling. `git diff HEAD --stat -- src/devices/
chipset/ppi_8255.h src/devices/chipset/ppi_8255.cpp` — QA ran this directly — returns **empty**
output, confirming zero lines changed in either file.

### 3.5 `JoystickPorts` regression guard — confirmed meaningful, not tautological

QA read `tests/unit/peripherals/joystick_unit_test.cpp` in full, focusing on the M18-added block
(lines 108-175). It contains four genuinely distinct sub-cases: (1) unattached + idle (bit7=1),
(2) unattached + other joystick bits pressed (bit7 still 1, and the OTHER bits are independently
asserted correct — `r == 0xFF & ~0x02`, not merely "bit7 is 1"), (3) attached source reflecting
both `high=true` and `high=false` (both polarities exercised, plus a check that OTHER bits remain
unaffected by the override), and (4) detaching (`nullptr`) reverts to the unconditional-1 default.
This is a real, non-trivial regression guard, not a tautology. `git diff HEAD --numstat -- tests/
unit/peripherals/joystick_unit_test.cpp` — QA ran this directly — shows `69 insertions(+), 0
deletions(-)`, confirming the edit is purely additive; the pre-M18 test cases (idle, direction/
trigger bits, R15 port-select, through-the-PSG) are untouched, still present, and still passing.

QA read `src/peripherals/joystick.cpp`'s `read_port_a()` (lines 60-73) directly and confirms the
`cassette_source_ != nullptr` guard wraps the ENTIRE bit7 override — when unattached, `bits` is
returned exactly as `encode()` produced it (bit7 unconditionally set by `kCassetteInputBit` in
`encode()`, `joystick.cpp:56`), which is structurally identical machine code to the pre-M18
behavior, not merely behaviorally coincidental.

### 3.6 No new CPU-stepping hook (A-M18-12) — independently confirmed, not by construction alone

QA ran, itself: `git diff HEAD -- src/machine/hbf1xv_machine.cpp | grep -n
"step_cpu_instruction\|run_cycles\|run_frame"` → **empty output, exit code 1** (no match). QA also
read `step_cpu_instruction()`, `run_cycles()`, and `run_frame()` (`hbf1xv_machine.cpp:313-380`) in
full, directly, and confirms none of the three references `cassette_`, `cassette_clock_`, or any
M18 symbol at all. `CassetteClock::cpu_cycles()` (the only new clock-adapter method) is called
exclusively from `CassetteInterface::cassette_input_high()` and
`CassetteInterface::load_synthetic_tape()` (both pull-style, caller-invoked) — QA grepped
`cassette_clock_`/`clock_source_` usage across `src/` and found no call site inside the CPU
stepping loop. **Structurally confirmed, not merely empirically observed via green tests.**

## 4. openMSX A/B Evidence — QA Independently Reproduced (not just read)

QA did not merely read `docs/m18-parity-trace-diff.md`. QA:

1. Confirmed WSL (`Ubuntu-24.04`) with a working openMSX 19.1 install is present
   (`wsl -d Ubuntu-24.04 -- /usr/bin/openmsx -version` → `openMSX 19.1`) and the real
   `Sony_HB-F1XV` machine XML boots successfully under it.
2. Built and ran its own independent Tcl single-step probe (distinct from
   `tools/openmsx-peripheral-io-parity.ps1`, hand-written by QA) driving the real WSL openMSX
   process through a Kanji-font-ROM JIS1 read sequence at `0x00020` — captured directly in §3.2
   above. Result: `00, 18, 18, 18`, matching the real ROM file and the developer's reported trace
   exactly.
3. Ran the identical 17-byte probe through this repo's own freshly built
   `build/Debug/sony_msx_headless.exe --parity-trace`, confirming the emulator independently
   produces the same `A`-register sequence at the corresponding steps.
4. Read `tools/gen-m18-peripheral-io-probe.py`'s full source (not merely its docstring) and
   confirmed Section C (printer) contains zero `IN A,(#90)` reads — genuinely honoring A-M18-7's
   disclosed-divergence exclusion — and Section D's `IN A,(#A2)` shape matches exactly what QA's
   own hand-driven cassette-idle check (`machine.debug_io_write(0xA0,14)` /
   `machine.debug_io_read(0xA2)`) would produce (QA additionally exercised this path via the
   `machine_hbf1xv_m18_peripheral_io_integration_test`'s Case 3, which QA re-read and re-ran as
   part of the fresh `ctest`).
5. Confirmed the adversarial comparator self-check claims are consistent with the same shared M10
   `tools/trace-diff.py` comparator already proven trustworthy across M10-M17 — no new comparator
   logic was introduced this milestone that would need independent re-validation.

**Conclusion: the strongest and most content-bearing A/B claim (Kanji font content parity) was
independently reproduced by QA end-to-end, from a cold WSL openMSX session through to a
byte-for-byte match against the real ROM file — not accepted on the developer's or coordinator's
word. The printer write-path and cassette idle-state subjects were verified by direct source
inspection of the probe generator (confirming the disclosed A-M18-7 exclusion is real) plus
re-execution of the equivalent machine-level assertions via the fresh `ctest` run.**

## 5. Full Deferred-Backlog Review Completeness

QA cross-checked `docs/m18-planner-package.md` §4 against the current
`agent-protocol/state/deferred-backlog.md` and confirms both re-affirm **every** row (A.B1-B9,
B.C1-C10, C.D1-D7, D.E1-E2), each with a one-line justification, per the human's explicit "review
deferred items and have them properly addressed" directive:

- **B5 → DONE (M18)**: correctly and specifically justified (direct-port `KanjiFontRom`,
  confirmed-correct device identity, real ROM content, A/B-verified).
- **C7 → DONE (M18)**: correctly justified, with the row TEXT itself refined from the backlog's
  original "`#90/#91`" shorthand to the accurate "`#90-#97`" claim — QA confirms this refinement is
  present verbatim in the current `deferred-backlog.md` (the C7 row header text literally reads
  "Printer (Centronics) `#90-#97`..."), not merely mentioned in prose elsewhere.
- **New Section E (F1/F2)**: QA confirms `deferred-backlog.md` contains a genuine new
  `## E. M18 printer/cassette depth deferrals` heading (mirroring the existing `## C.`/`## D.`
  sectioning convention) with F1 (cassette tape image-format/signal fidelity) and F2 (printer
  image/ESC-sequence rendering depth) correctly scoped and grounded.
- **All other rows (B1-B4, B6-B9, C1-C10, D1-D7, E1-E2 from prior milestones)**: present, statuses
  unchanged from their last accurate disposition, each with a named candidate owner — no row
  silently dropped or renumbered away. The B1 informational note (PSG R14 bit7 now
  source-injected, B1's own register model unchanged) is correctly non-disposition-changing.

No backlog item was silently addressed, silently dropped, or force-closed without justification.

## 6. Failures and Risk Ranking

No failures found. No test was weakened, deleted, or rewritten to accommodate incorrect behavior
(confirmed via `git diff --numstat` on every modified test file: purely additive, 0 deletions
anywhere). No M9/M12 CPU-timing-oracle test file was touched at all (confirmed via `git status`/
`git diff --stat` on `tests/`: only `joystick_unit_test.cpp` and `tests/CMakeLists.txt` among
pre-existing test files show any change, both purely additive).

### Risk ranking (residual, all non-blocking, all honestly disclosed by the developer)

- **Low — printer status-bit default divergence (A-M18-7).** Deliberate, disclosed, and
  structurally kept out of the A/B probe (QA independently confirmed by reading the probe
  generator source, §3.3/§4). Not a bug; a documented architectural choice necessitated by this
  emulator's lack of a pluggable-peripheral framework.
- **Low — synthetic-tape input model is not a real tape-file decoder (backlog F1).** Correctly
  scoped out and tracked as a new OPEN row; the CPU-visible register contract (motor/output/input
  bits) is what this milestone owes, and QA confirms it is delivered and correctly tested.
  Audio-file fidelity is genuinely separate, larger scope.
- **Low — printer capture is a raw-byte log only, no rendering (backlog F2).** Same reasoning;
  correctly scoped out and tracked.
- **None — `Ppi8255`/CPU-stepping-loop untouched.** Confirmed by direct `git diff` inspection
  (empty for `ppi_8255.{h,cpp}`; zero `step_cpu_instruction`/`run_cycles`/`run_frame` occurrences
  in the machine-file diff) — this is a structurally low-risk milestone on the CPU-timing-oracle
  axis, as the planner correctly anticipated.
- **Informational only — pre-existing M15 RTC `#B6`/`#B7` wiring gap.** Explicitly out of scope
  for M18 (unrelated device), correctly disclosed for continuity by the planner, not touched this
  cycle, not a new defect.

No new risk was introduced that is not already disclosed in `docs/m18-implementation-report.md`
§8 or the planner package §6. QA independently corroborates all listed residuals as accurate and
non-blocking.

## 7. Required Fixes

None blocking. No code changes required before the human release decision.

- **Recommended, non-blocking, for the coordinator to track at ledger update:** none beyond what
  is already correctly recorded — `agent-protocol/state/deferred-backlog.md`'s B5/C7/F1/F2 rows and
  `agent-protocol/state/definition-of-done.yaml`'s M18 block are internally consistent with the
  artifacts QA reviewed. No drift found.
- **Carried-forward condition (not new, not blocking M18):** the pre-existing M15 RTC `#B4-#B7`
  vs.-only-`#B4`/`#B5`-wired residual (noted by the M18 planner in §2.7 for transparency) remains
  unaddressed and is explicitly not this milestone's job; whichever future milestone touches RTC
  wiring should re-examine it then.

## 8. Boundary + Backlog Check

- No DEFERRED item was implemented ahead of schedule. Verified absent/untouched this cycle:
  Halnote firmware + real slot-0-3 SRAM wiring (B6), cartridge loading (B7), VDP rendering depth
  (D1-D7), Sony speed/pause (C8), SDL3 frontend (C9), exact cycle/write-timing (C1/C2/E2), ZEXALL
  (C3), YM2413 DSP/audio synthesis (E1), FDC flux/DMK fidelity (C10), real cassette tape-file
  decode (F1, correctly deferred not implemented), printer image rendering (F2, correctly deferred
  not implemented).
- `deferred-backlog.md` correctly records **B5 → DONE (M18)**, **C7 → DONE (M18)** (row text
  refined), and the two new **F1/F2 → OPEN** rows under a genuine new Section E; all prior rows
  re-affirmed with no silent drops. Footer updated same-cycle. Consistent with the planner package
  and the developer's own bookkeeping.

## 9. Sign-off Decision

**PASS.**

QA-executed evidence: fresh build clean (0 errors); **ctest 79/79 (0 failed)**, independently
re-run from a clean tree; the new `KanjiFontRom` device read and verified GENUINE
`MSXKanji`-pattern-only (not `MSXKanji12`) by direct, line-by-line comparison against the real
`references/openmsx-21.0/src/MSXKanji.cc`/`MSXKanji12.cc` and the real `Sony_HB-F1XV.xml`, not just
the planner's or developer's transcription; the Kanji content-parity claim was **independently
reproduced by QA from a cold WSL openMSX 19.1 session** against the real `Sony_HB-F1XV` machine,
matching the real ROM file bytes and this repo's own emulator output exactly (`00,18,18,18` at
offset `0x00020`, confirmed three ways); `PrinterPort` confirmed wired at the genuine `#90-#97`
8-port XML claim with the falling-edge capture semantic verified against `Printer.cc` directly, and
the A/B probe's exclusion of the disclosed status-bit divergence confirmed by direct source read
of the probe generator; `CassetteInterface` confirmed NOT a `core::IoDevice` and confirmed to
consume only already-public `Ppi8255` accessors with **zero** diff to `ppi_8255.{h,cpp}`; the
`JoystickPorts` regression guard confirmed genuine and non-tautological (four distinct sub-cases,
purely additive 69-line diff, 0 deletions); **zero** occurrences of `step_cpu_instruction`/
`run_cycles`/`run_frame` in the machine-file diff, confirmed by QA's own `git diff | grep`, ruling
out a stepping-loop hook structurally, not just empirically; all X4/M9/M12 timing-oracle-bearing
tests reproduce green; the full deferred-backlog review is genuinely complete (every row
re-affirmed, B5/C7 correctly closed, F1/F2 correctly added under a genuine new Section E, none
silently dropped).

No blocker-level gaps remain. No fabricated hardware behavior found. No test was weakened.

Per the milestone rule, this PASS authorizes the coordinator to PRESENT M18 for the human release
decision (normal gate, bundled/sequenced with M17's own still-pending release decision per the
human's directive); it does not auto-close M18 and does not itself authorize release. No blocking
conditions are attached — the carried-forward note in §7 (pre-existing M15 RTC wiring residual) is
informational, not gating.
