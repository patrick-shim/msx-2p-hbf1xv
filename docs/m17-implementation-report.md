# M17 Implementation Report — FM-PAC/MSX-MUSIC YM2413 (OPLL) Register-Accurate Device + Reusable Battery-Backed-SRAM Primitive

- Milestone ID: M17
- Request: REQ-M17-002 (implementation only; QA sign-off is a separate, subsequent gate)
- Developer: MSX Developer Agent
- Planner package: `docs/m17-planner-package.md`
- Decisions honored: DEC-0012 (coordinator-approved scope correction — B4/MSX-JE SRAM re-grounded
  and re-owned to B6, NOT wired to slot 3-3 in M17)
- Capture date: 2026-07-07

---

## 1. Milestone Target

Deliver, per `docs/m17-planner-package.md` §3 (slices S1-S5):

1. **S1/S2** — `Ym2413Opll`, a register-accurate YM2413 (OPLL) model under `src/devices/audio/`:
   64-byte register file, two-port `#7C`/`#7D` write protocol (address-latch masking applied at
   data-write time, not latch time), per-channel decode (F-Number/Block/Key-on/Sustain/
   Instrument/Volume/Patch), rhythm-mode decode (`$0E`, `$36-$38`), the 15-melody + 3-rhythm ROM
   instrument patch table, `reset()`, and a debug-only `register_value()` accessor. **No audio
   waveform synthesis** (explicitly out of scope, backlog row E1).
2. **S3** — Machine wiring (`src/machine/hbf1xv_machine.{h,cpp}`, additive only): attach
   `Ym2413Opll` at `io_bus_` ports `#7C`/`#7D`; add to the `cold_boot()` reset sequence; expose a
   `ym2413()` accessor; **the existing `slot_bus_.attach(3, 3, 1, &fmmusic_rom_)` line must not
   change** (A-M17-2, hard regression guard).
3. **S4** — `BatteryBackedSram`, a reusable, parametric-size, deterministic battery-backed
   byte-store primitive under `src/devices/memory/`, unit-tested standalone at 16 KB. **Per
   DEC-0012: not instantiated in `Hbf1xvMachine`, not wired to any slot.**
4. **S5** — Real openMSX A/B evidence for the register-write path, with the register-introspection
   comparison mechanism (Subject 2, R-M17-6) verified against a real WSL openMSX run BEFORE any
   parity claim.
5. Full deferred-backlog review honored (already delivered by the planner package §4; this cycle
   closes B3 in the ledger and leaves B4 exactly as the planning cycle left it — re-grounded,
   re-owned to B6, still OPEN).

**Critical architectural finding honored throughout (A-M17-1/A-M17-2, DEC-0012):** the HB-F1XV's
slot-3-3 sound device is openMSX class `MSXMusic` — a fixed 16 KB ROM + YM2413 with
I/O-port-only register access at `#7C`/`#7D`, **no SRAM, no bank register, no memory-mapped
register overlay** (`references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:180-196`,
`<MSX-MUSIC id="MSX Music">` ... `<io base="0x7C" num="2" type="O"/>`, no `<sramname>` tag;
`references/openmsx-21.0/src/sound/MSXMusic.hh:11-32`, `MSXMusic.cc:9-50`: `MSXMusicBase` only
overrides `writeIO`/`peekMem` over a plain masked ROM image). This is **not** the external
Panasonic FM-PAC cartridge (`MSXFmPac.hh/.cc`: 4-bank ROM + 8 KB SRAM handshake + memory-mapped
`0x3FF4-0x3FF7` registers) — that device is **not installed** on this machine. No bank-register,
SRAM-handshake, or ID-string-detection logic was built.

---

## 2. Code Changes

### 2.1 New files

| File | Purpose |
|---|---|
| `src/devices/audio/ym2413_opll.h` / `.cpp` | `Ym2413Opll` device: register file, two-port protocol, per-channel/rhythm decode, ROM instrument patch table. |
| `src/devices/memory/battery_backed_sram.h` / `.cpp` | `BatteryBackedSram`: parametric-size deterministic byte store (standalone, not wired). |
| `tests/unit/devices/audio_ym2413_opll_unit_test.cpp` | S1/S2 unit coverage. |
| `tests/unit/devices/memory_battery_backed_sram_unit_test.cpp` | S4 unit coverage. |
| `tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp` | S3 system-integration coverage. |
| `tools/gen-m17-ym2413-probe.py` | Assembles the deterministic A/B probe program + sidecar register-value list. |
| `tools/openmsx-ym2413-parity.ps1` | S5 A/B harness (both subjects). |
| `tests/parity/m17_ym2413_probe.bin`, `tests/parity/m17_ym2413_probe_regs.txt` | Generated probe fixtures. |
| `docs/m17-parity-trace-diff.md` | S5 A/B evidence artifact. |

### 2.2 Modified files (additive only)

- `src/machine/hbf1xv_machine.h` — `+#include "devices/audio/ym2413_opll.h"`; `+Ym2413Opll ym2413_;`
  member; `+ym2413()` const/non-const accessors; comment-only clarification on `fmmusic_rom_`
  (no behavior/size change).
- `src/machine/hbf1xv_machine.cpp` — `+io_bus_.attach(0x7C, &ym2413_); io_bus_.attach(0x7D,
  &ym2413_);` in `wire_bus()`; `+ym2413_.reset();` in `cold_boot()`'s device-reset sequence;
  `+ym2413()` accessor bodies. The `slot_bus_.attach(3, 3, 1, &fmmusic_rom_)` line (A-M17-2) is
  **byte-for-byte unchanged** — verified by direct read of the diff and by the dedicated
  regression-guard test case (§4).
- `src/main.cpp` — `+run_ym2413_parity()` + `--ym2413-parity` CLI mode (mirrors the existing
  `--vdp-parity`/`--parity-trace` precedent: runs a flat-RAM probe program to HALT, then dumps
  `register_value(addr)` for `$00-$3F`).
- `CMakeLists.txt` — `+src/devices/audio/ym2413_opll.cpp`, `+src/devices/memory/
  battery_backed_sram.cpp` in the `sony_msx_core` source list.
- `tests/CMakeLists.txt` — `+3` test targets (2 unit, 1 integration).

### 2.3 Device model grounding (per-behavior citations)

- **A-M17-1 (device identity).** `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:180-196`
  (`<MSX-MUSIC id="MSX Music">`, `<io base="0x7C" num="2" type="O"/>`, no `<sramname>`);
  `references/openmsx-21.0/src/sound/MSXMusic.hh:11-32`, `MSXMusic.cc:9-50` (`MSXMusicBase`: `rom`
  + `ym2413` members only, no SRAM/bank-register member).
- **A-M17-2 (no memory-space register overlay).** `MSXMusic.cc:37-50` (`peekMem`/`getReadCacheLine`
  are a plain masked ROM read, `&rom[start & (rom.size()-1)]`) — confirms `fmmusic_rom_` needs no
  wrapping device, unlike M16's `SonyFdc` (which had to wrap `disk_rom_` because the WD2793
  registers are memory-mapped inside the ROM window).
- **A-M17-3 (two-port write protocol, address-latch masking).**
  `references/openmsx-21.0/src/sound/YM2413Okazaki.cc:1368-1374` — read directly this cycle:
  ```
  void YM2413::writePort(bool port, uint8_t value, int /*offset*/)
  {
      if (port == 0) {
          registerLatch = value;
      } else {
          writeReg(registerLatch & 0x3f, value);
      }
  }
  ```
  Confirms: port 0 (`#7C`) latches **unmasked**; port 1 (`#7D`) masks with `& 0x3f` **at the data
  write**, not at latch time. `Ym2413Opll::write_address`/`write_data` mirror this exactly.
- **A-M17-4 (reset zeroes all 64 registers).** `YM2413Okazaki.cc:696` (`std::ranges::fill(reg, 0)`
  at construction) and `:707-720` (`reset()` calls `writeReg(i, 0)` for `i` in `[0, 0x40)` plus
  `registerLatch = 0`). `Ym2413Opll::reset()` zeroes `regs_` and `latch_`.
- **A-M17-5 (open-bus read).** XML declares `#7C/#7D` write-only (`type="O"`, no `type="I"`);
  `MSXMusicBase` overrides only `writeIO` (`MSXMusic.hh:14-15`). `Ym2413Opll::io_read` always
  returns `0xFF` regardless of port or prior writes.
- **A-M17-6 (debug register readback).** `references/openmsx-21.0/src/sound/YM2413.hh:26,40-44`
  and `YM2413.cc:17-29` (`Debuggable` ctor: `SimpleDebuggable(motherBoard_, name_ + " regs",
  "MSX-MUSIC", 0x40)`, `read()` returns `core->peekRegs()[address]`). `Ym2413Opll::register_value`
  mirrors `PsgYm2149::register_value`.
- **Register bit layout (per-channel/rhythm decode, §2.2 of the planner package).** Cross-checked
  directly against `YM2413Okazaki.cc:1382-1500` (`writeReg` cases `0x00`-`0x07`) this cycle. One
  refinement beyond the fact-sheet's simplified per-register text: register `$03` packs **carrier**
  KSL in bits 7-6 (`YM2413Okazaki.cc:1441`, `patches[0][1].setKL((data >> 6) & 3)`), not just the
  DC/DM/FB fields the fact-sheet's master table names for that byte. `Ym2413Opll::decode_patch`
  decodes this (`OperatorPatch::ksl` on both modulator and carrier), documented in a source comment
  citing the exact line range — a deliberate accuracy refinement, not a deviation from the
  planner's operational contract (which never said carrier KSL must be *omitted*, only silent on
  it).
- **ROM instrument patch table (A-M17-7).** Reproduced verbatim from the fact-sheet §4 / planner
  package §2.2 table (15 melody + 3 rhythm entries, 8 bytes each). The fact-sheet's own caveat
  ("community-standard... ~99% but not datasheet-certain") is preserved as a source comment in
  `ym2413_opll.cpp`, citing `references/openmsx-21.0/src/sound/YM2413NukeYKT.hh` as the
  die-shot-derived reference a future DSP-depth milestone (E1) must re-verify against.
- **§2.4 determinism.** No `elapsed_cycles()` adapter was added — confirmed by inspection of
  `hbf1xv_machine.cpp`: `ym2413_` participates only in `wire_bus()` (I/O attach) and `cold_boot()`
  (`reset()`); it is never touched in `step_cpu_instruction()`, `run_cycles()`, or `run_frame()`.
  The M9/M12 CPU-timing oracles are therefore untouched by construction.

### 2.4 `BatteryBackedSram` (S4) — confirmed NOT wired

Per DEC-0012, `BatteryBackedSram` is a standalone primitive only. Verified this cycle:

```
$ grep -rn "BatteryBackedSram\|battery_backed_sram" src/machine/
NO MATCHES in src/machine/ (confirmed not wired)
```

No `Hbf1xvMachine` member, no `#include`, no slot/mapper attachment exists. It is built and
unit-tested at 16 KB (`0x4000`, matching `references/openmsx-21.0/src/memory/RomHalnote.cc:44`'s
`sram = std::make_unique<SRAM>(getName() + " SRAM", 0x4000, config);` exactly), ready for a future
Halnote milestone (backlog B6) to consume directly. **This is a deliberate, documented gap, not an
oversight.**

---

## 3. Unit Test Results

New unit test executables (both green, see §5 for the full-suite run):

- `devices_audio_ym2413_opll_unit_test` — two-port write protocol lands in the correct register;
  latch masking applied at data-write time (latch `0xFF` → register `0x3F`); every register in
  `$00-$3F` independently addressable; `reset()` zeroes all 64 registers + the latch; `io_read`
  always `0xFF` regardless of prior writes; per-channel F-Number/Block/Key-on/Sustain/Instrument/
  Volume decode across representative writes (looped over all 9 channels); key-on/sustain clear
  when bits are clear; user-patch (`instrument==0`) live decode reflects in-place `$00-$07` edits;
  rhythm-mode decode (`$0E` bits + `$36-$38` volume nibbles); all 15 melody + 3 rhythm ROM patches
  byte-exact against the fact-sheet table (every field of every patch asserted); the planner's own
  spot-check example (`rom_patch(3).modulator.multiple == 3`, Piano); two-run determinism
  (identical write sequence → identical register state).
- `devices_memory_battery_backed_sram_unit_test` — construct at 16 KB, zero-initialized; absent
  file → `load()` returns `false`, state stays zero; write + `save()` + fresh-instance `load()` →
  identical bytes; two independent round-trips produce identical bytes; too-short/corrupt file
  behaves like absent (store untouched, `load()` returns `false`); `clear()` resets to zero
  regardless of prior content.

Both executed via `ctest --test-dir build -C Debug --output-on-failure`:

```
Start 73: devices_audio_ym2413_opll_unit_test
73/75 Test #73: devices_audio_ym2413_opll_unit_test .........................   Passed    0.01 sec
Start 74: devices_memory_battery_backed_sram_unit_test
74/75 Test #74: devices_memory_battery_backed_sram_unit_test ................   Passed    0.02 sec
```

---

## 4. Integration Test Results

`machine_hbf1xv_m17_ym2413_integration_test` (new, S3):

1. **`WriteProbe_RegisterValue_ReflectsEveryOutWrite`** — a real CPU program (`LD A,reg; OUT
   (#7C),A; LD A,val; OUT (#7D),A` for 12 representative addresses spanning `$00-$07`/`$10-$18`/
   `$20-$28`/`$30-$38`/`$0E`/`$36-$38`) run over the full M11 `SystemBus`; `machine.ym2413().
   register_value(...)` reflects every write.
2. **`FmMusicRom_Slot33Page1_UnchangedByYm2413Writes`** (A-M17-2 regression guard) — navigates
   page 1 → primary slot 3, sub-slot 3 (`debug_io_write(0xA8, 0x0C)` + `debug_bus_write(0xFFFF,
   0x0C)`), reads all 64 bytes of `fmmusic_rom_`'s window via `debug_bus_read`, drives the full
   `#7C`/`#7D` write sequence directly over the bus, re-reads the same 64 bytes — **byte-for-byte
   identical** before and after.
3. **`IoRead_Port7C_OpenBus_OverBus` / `IoRead_Port7D_OpenBus_OverBus`** — `IN A,(#7C)`/`IN
   A,(#7D)` over the bus read `0xFF` regardless of prior writes.
4. **`TwoMachineDeterminism_IdenticalProgram_IdenticalRegisterState`** — the identical program run
   on two independent `Hbf1xvMachine` instances produces byte-identical register state across all
   64 registers.

```
Start 75: machine_hbf1xv_m17_ym2413_integration_test
75/75 Test #75: machine_hbf1xv_m17_ym2413_integration_test ..................   Passed    0.03 sec
```

**Full-suite regression check** (`ctest --test-dir build -C Debug --output-on-failure`, fresh
`cmake --build build --config Debug` beforehand):

```
100% tests passed, 0 tests failed out of 75

Total Test time (real) =   1.31 sec
```

75/75 = 72 prior (M1-M16) + 3 new (M17). **Zero regression.** The M9/M12 CPU-timing oracles
(`devices_chipset_m1_wait_unit_test`, `machine_hbf1xv_cpu_step_integration_test`,
`machine_hbf1xv_cpu_parity_integration_test`, `machine_hbf1xv_m11_parity_checkpoint_integration_test`,
`machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`,
`machine_hbf1xv_m15_boot_checkpoint_integration_test`,
`machine_hbf1xv_m16_boot_checkpoint_integration_test`) all remained green, confirming M17 added no
`elapsed_cycles()` clock consumer (§2.4).

---

## 5. Evidence Gates (exact captured output)

```
$ tools/validate-assets.ps1
Asset validation result: True
BIOS directory: C:\Users\pashim\source\sony-msx-hbf1xv\bios
Present BIOS files: f1xvbios.rom, f1xvdisk.rom, f1xvext.rom, f1xvfirm.rom, f1xvkdr.rom,
                     f1xvkfn.rom, f1xvmus.rom
ROM directory:  C:\Users\pashim\source\sony-msx-hbf1xv\roms
ROM files: aleste.rom

$ tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
Checksum report written to: C:\Users\pashim\source\sony-msx-hbf1xv\docs\asset-checksums.txt
(SHA256 for all 7 bios/ files + 1 roms/ file; see docs/asset-checksums.txt)

$ cmake --build build --config Debug
... (0 errors; only pre-existing C4819 codepage warnings, unrelated to this change)
sony_msx_headless.vcxproj -> C:\Users\pashim\source\sony-msx-hbf1xv\build\Debug\sony_msx_headless.exe

$ ctest --test-dir build -C Debug --output-on-failure
100% tests passed, 0 tests failed out of 75
Total Test time (real) =   1.31 sec
```

---

## 6. openMSX A/B Evidence (both subjects, real captures — see `docs/m17-parity-trace-diff.md`)

Because the OPLL is write-only (A-M17-5), the M11/M15-style `IN A,(port)` architectural-register
technique does not apply. Two complementary real subjects were captured (**neither BLOCKED**):

### Subject 1 — CPU-visible architectural-state parity across the write sequence

Mechanism identical to `tools/openmsx-io-parity.ps1` (reused in spirit, reimplemented inline in the
new `tools/openmsx-ym2413-parity.ps1` to avoid inheriting that script's hardcoded M11 doc-title
rewrite): the 36-register write probe (`tests/parity/m17_ym2413_probe.bin`, assembled by
`tools/gen-m17-ym2413-probe.py`, covering `$00-$07`/`$0E`/`$10-$18`/`$20-$28`/`$30-$38`) runs via
this emulator's `--parity-trace` and, on openMSX 19.1 (WSL, `Sony_HB-F1XV` machine XML), via a
chained single-step Tcl script; diffed per-instruction (PC, opcode, every register/flag) via
`tools/trace-diff.py`.

**Result: ARCHITECTURAL PARITY — EMPTY DIFF over all 145 instructions.**

### Subject 2 — Internal YM2413 register-file comparison (A-M17-6)

**R-M17-6 verification (done BEFORE building the harness, per the task brief's explicit
requirement):** confirmed the `debug read {MSX Music regs} <addr>` Tcl mechanism actually works
against a real WSL openMSX run, via a minimal standalone script:

```tcl
after time 6 {
  debug write ioports 0x7C 0x10
  debug write ioports 0x7D 0x5A
  set v [debug read {MSX Music regs} 0x10]
  puts stderr [format "REGVAL addr=0x10 value=0x%02X" $v]
}
```

Run against `openmsx -machine Sony_HB-F1XV -command 'set renderer none' -script <this>`:

```
REGVAL addr=0x10 value=0x5A
```

This confirms (a) openMSX's generic `ioports` `SimpleDebuggable` correctly drives the real
`writeIO` path (verified by reading `references/openmsx-21.0/src/cpu/MSXCPUInterface.cc:1308-1312`:
`IODebug::write` calls `interface.writeIO(...)`, the genuine CPU I/O write path, not a bypass), and
(b) `debug read {MSX Music regs} <addr>` genuinely reflects the write. **Only after this
verification** was the full harness built and a parity claim made — per R-M17-6 and the guardrails'
"honest BLOCKED" discipline (this mechanism was NOT assumed to work).

The full harness then runs the SAME 36-register write probe to `HALT` on both sides and compares
this emulator's `register_value(addr)` dump (`--ym2413-parity` CLI mode) against openMSX's
`debug read {MSX Music regs} <addr>` for every written address.

**Result: REGISTER-FILE PARITY — EMPTY DIFF over all 36 written addresses.**

### Adversarial comparator self-check (both subjects)

- **Subject 1** (`tools/trace-diff.py`, run against this cycle's own captured traces): empty B
  side → exit 2, `BLOCKED`; corrupted `af=` field at instruction 10 → exit 1,
  `ARCHITECTURAL DIVERGENCE` (2 field mismatches).
- **Subject 2** (the new register-file map-diff logic in `tools/openmsx-ym2413-parity.ps1`, run
  against this cycle's own captured register dumps): genuine A/B dumps → `PARITY`; empty B-side
  map → `BLOCKED (A=64 B=0)`; one address's value XORed with `0xFF` in a copy of the openMSX dump
  → `DIVERGENCE (1 mismatches: 0)` (the corrupted address correctly flagged).

Both comparators are proven trustworthy: neither silently passes a blocked or corrupted
comparison. Full detail: `docs/m17-parity-trace-diff.md`.

---

## 7. Ledger Discipline

- `agent-protocol/state/deferred-backlog.md`: **B3 → DONE (M17)** (updated this cycle, with full
  justification). **B4 left OPEN**, text unchanged from the planning cycle's DEC-0012 correction
  (re-grounded, re-owned to B6) — no new disposition made here, per the task brief's explicit
  instruction not to mark B4 done. E1/E2 rows unchanged (already existed from the planning cycle).
- `agent-protocol/channels/responses.md`: RESP-M17-002 filed (this implementation cycle's outcome,
  evidence, and residual risks).

---

## 8. Known Issues / Residual Risks

1. **ROM instrument patch table provenance (A-M17-7).** The fact-sheet's own caveat ("~99% but not
   datasheet-certain") is preserved verbatim as a source comment. A future DSP-depth milestone (E1)
   must re-verify against `references/openmsx-21.0/src/sound/YM2413NukeYKT.cc`'s decoded per-field
   patches before any audio-affecting claim — this milestone's register-accurate contract does not
   depend on the table's audio-fidelity, only its byte-exact transcription (asserted in the unit
   test).
2. **Carrier KSL decode is a deliberate refinement beyond the fact-sheet's literal per-register
   text** (grounded directly in `YM2413Okazaki.cc:1439-1444`, cited in code comments). This does
   not contradict the planner's operational contract; it is documented so a future auditor
   understands why `OperatorPatch::ksl` is populated for the carrier operator too.
3. **`BatteryBackedSram` has no real consumer this milestone** (by design, per DEC-0012) — this is
   stated plainly, not hidden. B6/Halnote is the intended future consumer.
4. **No audio waveform synthesis** (backlog E1) — this milestone delivers the register/channel/
   rhythm CONTRACT only, per the task brief's explicit "no audio waveform synthesis/presentation
   required this milestone."
5. No new risk to the CPU-timing oracles: confirmed no `elapsed_cycles()` adapter was added; all
   M9/M12 timing-oracle tests remain green.

---

## 9. QA Handoff

- **Build:** `cmake --build build --config Debug` — 0 errors (pre-existing C4819 codepage warnings
  only, unrelated to this change).
- **Tests:** `ctest --test-dir build -C Debug --output-on-failure` — **75/75 passed, 0 failed**
  (72 prior M1-M16 + 3 new M17). Zero regression.
- **New device files build clean:** `src/devices/audio/ym2413_opll.{h,cpp}`,
  `src/devices/memory/battery_backed_sram.{h,cpp}` — both compile without warnings beyond the
  pre-existing repo-wide C4819 codepage note (present in every file, unrelated to M17).
- **`fmmusic_rom_` regression guard: HOLDS.** `slot_bus_.attach(3, 3, 1, &fmmusic_rom_)` is
  byte-for-byte unchanged in `src/machine/hbf1xv_machine.cpp`; a dedicated integration-test case
  (`FmMusicRom_Slot33Page1_UnchangedByYm2413Writes`) proves the ROM window's 64 bytes are identical
  before and after the full YM2413 write sequence.
- **A/B outcome: REAL PARITY on both subjects, neither BLOCKED.** Subject 1 (CPU-visible
  architectural parity): empty diff, 145/145 instructions. Subject 2 (internal register-file
  parity via `debug read {MSX Music regs} <addr>`, mechanism independently verified against real
  WSL openMSX before use): empty diff, 36/36 written addresses. `docs/m17-parity-trace-diff.md`.
- **`BatteryBackedSram` is NOT wired into the machine** — confirmed via
  `grep -rn "BatteryBackedSram\|battery_backed_sram" src/machine/` returning no matches. Standalone,
  unit-tested only, per DEC-0012.
- **Assets:** `tools/validate-assets.ps1` → `True`; `tools/checksum-assets.ps1` refreshed
  `docs/asset-checksums.txt` (no asset changes this milestone — M17 needs no new BIOS/ROM asset).
- **Backlog:** B3 → DONE (M17); B4 unchanged (OPEN, re-owned to B6); E1/E2 unchanged.
- **Next gate:** QA regression assessment + sign-off recommendation (`docs/m17-qa-signoff.md`),
  then a separate human release decision (normal gate, no auto-close per DEC-0003's M12-only
  grant).
