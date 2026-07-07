# M17 QA Sign-off — FM-PAC/MSX-MUSIC YM2413 (OPLL) Register-Accurate Device + Reusable Battery-Backed-SRAM Primitive

- Milestone: M17 (REQ-M17-002).
- QA Owner: MSX QA Agent.
- Date: 2026-07-07.
- Gate: M17 retains the NORMAL human-release-decision gate (per the planner package §7 and
  DEC-0003's auto-close grant being M12-only). A QA Pass lets the coordinator PRESENT M17 to the
  human for the release decision + tag; it does NOT auto-close the milestone.
- Verdict: **PASS**.

All results below were produced or independently reproduced by QA (fresh reconfigure, fresh
rebuild, fresh `ctest` run, direct reads of every changed/new source file, direct reads of the
cited `references/openmsx-21.0/` line ranges, a `git diff` inspection of the exact wiring lines,
and an independent re-run of the full WSL openMSX A/B harness plus an extra ad-hoc register
round-trip of my own) — developer and coordinator claims were not accepted at face value.

---

## 1. Regression Scope

Affected surface for M17 (closes backlog B3; re-grounds but does not close B4): a wholly new
`src/devices/audio/ym2413_opll.{h,cpp}` device plus a wholly new, standalone, unwired
`src/devices/memory/battery_backed_sram.{h,cpp}` primitive, plus additive-only wiring in
`src/machine/hbf1xv_machine.{h,cpp}` (two new `io_bus_.attach` calls at `#7C/#7D`, one new member,
one `reset()` call, one accessor pair) and an additive `--ym2413-parity` CLI mode in `src/main.cpp`.

New/changed source verified by QA directly:
- `src/devices/audio/ym2413_opll.{h,cpp}` (new) — 64-register file, two-port `#7C/#7D` address-
  latch/data write protocol, per-channel decode, rhythm decode, 15+3-entry ROM instrument patch
  table, `reset()`, debug-only `register_value()`.
- `src/devices/memory/battery_backed_sram.{h,cpp}` (new) — parametric-size deterministic byte
  store; confirmed **not referenced anywhere under `src/machine/`**.
- `src/machine/hbf1xv_machine.{h,cpp}` (modified, additive only) — `Ym2413Opll ym2413_` member,
  `io_bus_.attach(0x7C/0x7D, &ym2413_)`, `ym2413_.reset()` in `cold_boot()`, `ym2413()` accessors.
  The pre-existing `slot_bus_.attach(3, 3, 1, &fmmusic_rom_)` line is verified **byte-for-byte
  unchanged** (only surrounding comments changed — confirmed by QA's own `git diff HEAD` read, not
  just the developer's claim).
- `src/main.cpp` (modified, additive) — `run_ym2413_parity()` + `--ym2413-parity` CLI mode.
- `CMakeLists.txt` / `tests/CMakeLists.txt` (modified, additive) — 2 new source files, 3 new test
  targets.
- Test/tooling additions: `tests/unit/devices/audio_ym2413_opll_unit_test.cpp`,
  `tests/unit/devices/memory_battery_backed_sram_unit_test.cpp`,
  `tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp`,
  `tools/gen-m17-ym2413-probe.py`, `tools/openmsx-ym2413-parity.ps1`,
  `tests/parity/m17_ym2413_probe.bin` + `_regs.txt`, `docs/m17-parity-trace-diff.md`.

Regression-critical protected areas checked: X4 (CPU T-state math / M9-M12 timing oracles — no
`src/devices/cpu/` or `src/core/` files appear anywhere in the diff, confirmed by `git status`),
X1 (`#A8` slot-select — untouched, no PPI/mapper-IO changes this cycle), A-M17-2 (`fmmusic_rom_`
slot-3-3 attachment), M11/M13 slot tests, M13/M15/M16 boot goldens, M14 VDP (untouched), M15 device
suite (PSG/RTC/PPI/backup-RAM, untouched), M16 FDC suite (untouched).

## 2. Regression Matrix Status

| Area | Coverage | QA result |
|------|----------|-----------|
| Build (headless, SDL3 OFF) | fresh `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` | **PASS — 0 errors** (QA-executed, own tree) |
| Full deterministic suite | `ctest --test-dir build -C Debug --output-on-failure` | **PASS — 100% tests passed, 0 failed out of 75** (QA-executed, fresh) |
| YM2413 register file + two-port protocol (S1) | `devices_audio_ym2413_opll_unit_test` | PASS — read source, confirms latch-time-unmasked / use-time-masked semantics, reset, open-bus read |
| YM2413 channel/rhythm decode + ROM patch table (S2) | same test file | PASS — all 15 melody + 3 rhythm patches asserted byte-exact per field, including the carrier-KSL refinement |
| Machine wiring + `fmmusic_rom_` regression guard (S3) | `machine_hbf1xv_m17_ym2413_integration_test` | PASS — regression-guard case reads real (non-trivial, non-zero) ROM bytes before/after; confirmed meaningful (§3.4) |
| `BatteryBackedSram` standalone primitive (S4) | `devices_memory_battery_backed_sram_unit_test` | PASS — construct/zero, absent/short-file handling, save/reload round-trip, two-round determinism, `clear()` |
| Two-machine / two-run determinism | both new unit + integration suites | PASS |
| X4 timing oracles (M9/M12) | `machine_hbf1xv_cpu_step_integration_test`, `machine_hbf1xv_m11_parity_checkpoint_integration_test`, `machine_hbf1xv_m13_mem_parity_checkpoint_integration_test`, `machine_hbf1xv_m15_boot_checkpoint_integration_test`, `machine_hbf1xv_m16_boot_checkpoint_integration_test` | PASS — all green in QA's own run; zero `src/devices/cpu/`/`src/core/` diff confirms no perturbation is even possible |
| openMSX A/B — Subject 1 (CPU-visible architectural parity) | `tools/openmsx-ym2413-parity.ps1` vs genuine WSL openMSX 19.1 `Sony_HB-F1XV` | **PASS — QA independently re-ran the full harness; empty diff, 145/145 instructions** (§4) |
| openMSX A/B — Subject 2 (internal register-file parity via `debug read {MSX Music regs}`) | same harness | **PASS — QA independently re-ran; empty diff, 36/36 written addresses** (§4); QA additionally hand-verified the underlying mechanism with a second address beyond the developer's own spot-check (§4) |
| Adversarial comparator self-check (both subjects) | developer-captured, QA read the artifact | PASS — empty-side → BLOCKED, corrupted-field → DIVERGENCE, for both subjects |
| Full deferred-backlog review completeness | `docs/m17-planner-package.md` §4, `agent-protocol/state/deferred-backlog.md` | PASS — every row A.B1-B9/B.C1-C10 re-affirmed with justification; E1/E2 added; B4 correctly left OPEN/re-owned, not force-closed (§5) |

The 3 new M17 tests are additive; the 72 prior M1-M16 tests are unchanged and green (zero
regression). QA counted 75/75 in its own fresh `ctest` run, not from the developer's report.

## 3. Source-Level Verification (genuine, not stub)

### 3.1 Device-identity correctness — the central risk this milestone (R-M17-1)

QA read `src/devices/audio/ym2413_opll.h` and `.cpp` in full. The class implements exactly:
a 64-byte register array, a single-byte address latch, `write_address`/`write_data` (the two-port
protocol), `io_read` returning a hardcoded `0xFF`, `reset()`, per-channel/rhythm decode functions,
and the static ROM instrument-patch table. There is **no bank-register field, no SRAM member, no
ID-string comparison logic, and no memory-mapped register path** anywhere in either file. A
targeted grep for FM-PAC-cartridge-specific tokens (`bank_register`, `sram_handshake`,
`id.string`, `magic.byte`, `0x3FF4`, `0x3FF7`, `0x7FF7`, `0x5FFE`, `0x5FFF`, `PAC2OPLL`,
`APRLOPLL`) over both files returns **zero code hits** — the only two matches are explanatory
source comments describing what was deliberately *not* built (A-M17-1's grounding note).

QA independently read `references/openmsx-21.0/src/sound/MSXMusic.hh` and `MSXMusic.cc` (not
relying on the planner's/developer's transcription): `MSXMusicBase` holds only `Rom rom` and
`YM2413 ym2413` members, overrides only `reset`/`writeIO`/`peekMem`/`readMem`/`getReadCacheLine`,
and `writeIO` does nothing but `writePort(port & 1, value, time)` → `ym2413.writePort(...)`. There
is no bank register, no SRAM, and no memory-mapped register overlay in the real class. QA also
read `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:179-197` directly and confirms
`<MSX-MUSIC id="MSX Music">` at slot 3-3 with `<io base="0x7C" num="2" type="O"/>` and **no**
`<sramname>` tag — matching the implementation's grounding claim exactly, line for line. This is
the correct reference (`MSXMusic`), not `MSXFmPac` (the external Panasonic cartridge, confirmed
absent from every citation in the code). **No fabricated hardware behavior found.**

### 3.2 Register/latch semantics (A-M17-3)

QA read `references/openmsx-21.0/src/sound/YM2413Okazaki.cc:1368-1375` directly:

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

This confirms `port==0` (`#7C`) latches **unmasked**; `port==1` (`#7D`) applies `& 0x3f` at the
**data-write**, not at latch time. `Ym2413Opll::write_address`/`write_data` mirror this exactly
(`ym2413_opll.cpp:56-65`). QA also read `YM2413Okazaki.cc:696` (`std::ranges::fill(reg, 0)` at
construction) and `:707-720` (`reset()` zeroes all 64 registers plus `registerLatch = 0`) —
matches `Ym2413Opll::reset()` exactly. The unit test explicitly asserts the boundary case (latch
`0xFF` → register `0x3F` on data write), which QA re-ran and confirms passes.

QA additionally spot-checked the per-channel/rhythm bit-layout claims against
`YM2413Okazaki.cc:1382-1460` (`writeReg` cases `0x00`-`0x07`) and confirms the implementation's
"deliberate refinement" claim (carrier KSL decoded from register `$03` bits 7-6,
`patches[0][1].setKL((data >> 6) & 3)` at `YM2413Okazaki.cc:1439`) is accurately grounded and
correctly implemented (`decode_patch`'s `patch.carrier.ksl` assignment), and is asserted in the
unit test's byte-exact ROM-patch-table check. Not a deviation from the operational contract.

### 3.3 `BatteryBackedSram` non-wiring (DEC-0012) — independently confirmed

QA did not rely on the developer's `grep` output alone. QA read
`src/machine/hbf1xv_machine.h` and `src/machine/hbf1xv_machine.cpp` in full (not just a
diff) and confirms: no `BatteryBackedSram` type, no `#include "devices/memory/
battery_backed_sram.h"`, no member of that type, and no `.attach(...)` call referencing it,
anywhere in either file. `git diff HEAD -- src/machine/hbf1xv_machine.h src/machine/
hbf1xv_machine.cpp` shows only the `Ym2413Opll`-related additions (two attach calls, one member,
one reset line, one accessor pair) plus comments — nothing SRAM-related. The primitive is real,
unit-tested at exactly 16 KB (`0x4000`, matching `references/openmsx-21.0/src/memory/
RomHalnote.cc:44`'s `sram = std::make_unique<SRAM>(getName() + " SRAM", 0x4000, config)` — QA
read this line directly), and genuinely has zero consumers this milestone, per DEC-0012.

### 3.4 `fmmusic_rom_` regression guard — confirmed meaningful, not a no-op

QA read the integration test's Case 2 (`FmMusicRom_Slot33Page1_UnchangedByYm2413Writes`) and
confirms it navigates the real slot-switch seam (`debug_io_write(0xA8, 0x0C)` +
`debug_bus_write(0xFFFF, 0x0C)`) to page slot 3-3 into page 1, reads 64 real ROM bytes via
`debug_bus_read`, drives the full 12-register `#7C`/`#7D` write sequence directly over the bus,
then re-reads the same 64 bytes and asserts byte-for-byte equality. QA verified this is **not** a
trivial zero-vs-zero comparison: `bios/f1xvmus.rom` (loaded into `fmmusic_rom_` at machine
construction) is confirmed present and non-trivial — its first bytes are `41 42 00 00 00 50 00
00 ... 41 50 52 4C 4F 50 4C 4C` (`"AB"` header + the literal ASCII string `"APRLOPLL"` baked into
the real ROM image content itself), i.e. genuinely varied, non-zero, non-degenerate content. The
test therefore proves the OPLL write sequence has zero side effect on this real ROM window, which
is exactly what A-M17-2 requires. (Note: the `"APRLOPLL"` bytes are static ROM *content*, not
runtime ID-string-detection *logic* — no code in `ym2413_opll.{h,cpp}` or the machine wiring reads
or compares against this string; confirmed by the grep in §3.1.)

## 4. openMSX A/B Evidence — QA Independently Reproduced (not just read)

QA did not merely read `docs/m17-parity-trace-diff.md`. QA:

1. Confirmed WSL openMSX 19.1 (`/usr/bin/openmsx`) is present and the `Sony_HB-F1XV` machine XML
   is installed under `/usr/share/openmsx/machines/Sony_HB-F1XV.xml`.
2. Independently ran a **standalone verification** of the R-M17-6 mechanism (the register-
   introspection Tcl path), beyond the developer's own single-address spot-check: drove `debug
   write ioports 0x7C 0x10` / `0x7D 0x5A` then `debug read {MSX Music regs} 0x10` → returned
   `0x5A` (matches the developer's own reported verification exactly), **and** additionally drove
   a second, different address/value pair (`0x7C 0x2A` / `0x7D 0xC3` → `debug read {MSX Music
   regs} 0x2A` → returned `0xC3`) to rule out a single-address coincidence. Both round-trips
   succeeded genuinely against the real WSL openMSX process. This independently corroborates
   R-M17-6 was not fabricated.
3. Regenerated the probe fixtures from source (`python tools/gen-m17-ym2413-probe.py`) and
   re-ran the **full** `tools/openmsx-ym2413-parity.ps1` harness end-to-end against the freshly
   built `sony_msx_headless.exe` and the real WSL openMSX process. Result, reproduced independently
   by QA:
   ```
   Subject 1: trace A (emulator) instructions: 145
   Subject 1: trace B (openMSX) instructions captured: 145
   Subject 1 RESULT: ARCHITECTURAL PARITY (empty diff, 145 instructions)
   Subject 2: openMSX register dump lines captured: 64
   Subject 2 RESULT: REGISTER-FILE PARITY -- all 36 written addresses match (empty diff)
   ```
   This is a genuine PASS on both subjects, matching `docs/m17-implementation-report.md` and
   `docs/m17-parity-trace-diff.md` exactly. `git diff --stat` after the re-run shows **zero
   changes** to the regenerated probe/diff artifacts — i.e. the harness is fully deterministic and
   reproducible, not a one-off/lucky capture.
4. Confirmed the adversarial comparator self-check claims by reading the developer's captured
   description in `docs/m17-parity-trace-diff.md` §"Adversarial comparator self-check" (empty-side
   → BLOCKED, corrupted-field → DIVERGENCE, for both subjects) — consistent with the same
   `tools/trace-diff.py` comparator already proven trustworthy across M10-M16 and re-validated
   again this cycle for the new Subject-2 map-diff logic in `tools/openmsx-ym2413-parity.ps1`.

**Conclusion: both A/B subjects are genuine, reproducible PASS results, independently verified by
QA from a cold rebuild through the real WSL openMSX process — not accepted on the developer's or
coordinator's word.**

## 5. Full Deferred-Backlog Review Completeness

QA cross-checked `docs/m17-planner-package.md` §4 against the current
`agent-protocol/state/deferred-backlog.md` and confirms both re-affirm **every** row (B1-B9,
C1-C10, plus the newly proposed E1/E2), each with a one-line justification, per the human's
explicit "review deferred items and have them properly addressed" directive:

- **B3 → DONE (M17)**: correctly and specifically justified (register-accurate device delivered,
  DSP depth split to E1).
- **B4**: correctly left **OPEN**, re-grounded and re-owned to B6, per DEC-0012 — QA confirms this
  is the technically correct disposition (§3.1/§3.3 above) and that no premature closure or silent
  drop occurred.
- **B6**: correctly updated to note it is now B4's confirmed true owner.
- **B1/B2/B5/B7/B8/B9** and **C1-C10**: all present, statuses unchanged from their last accurate
  disposition (DONE where already closed at M14/M15/M16, OPEN where still pending), each with a
  named candidate owner — no row silently dropped or renumbered away.
- **E1/E2**: newly added, both correctly scoped (DSP/audio-synthesis depth; write-timing
  constraint) and both correctly grounded in the fact-sheet + `YM2413NukeYKT`/timing citations.

No backlog item was silently addressed, silently dropped, or force-closed without justification.

## 6. Failures and Risk Ranking

No failures found. No test was weakened, deleted, or rewritten to accommodate incorrect behavior.
QA read every new/modified test file in full (not just the pass/fail summary) and confirms each
assertion is genuine and non-trivial.

### Risk ranking (residual, all non-blocking, all honestly disclosed by the developer)

- **Low — ROM instrument patch table provenance (A-M17-7).** The fact-sheet's own caveat
  ("~99% but not datasheet-certain") is preserved verbatim in source comments. Since M17 delivers
  only the register/channel/rhythm *contract* (no audio synthesis), this table's byte-exact
  transcription (verified in the unit test) is what matters for this milestone — its audio-fidelity
  is out of scope until E1. Correctly deferred, not a defect.
- **Low — carrier KSL decode "refinement" beyond the fact-sheet's literal per-register text.**
  QA independently confirmed this is correctly grounded (§3.2) and does not contradict the
  planner's operational contract. Documented in source comments for future auditors.
- **None — `BatteryBackedSram` has no consumer.** By design (DEC-0012), not a defect; correctly
  disclosed as a deliberate, honest gap awaiting B6/Halnote.
- **None — no audio waveform synthesis (E1).** Explicitly and correctly out of scope per the task
  brief; tracked as backlog row E1, not hidden.
- **None — CPU-timing oracles.** Confirmed by direct `git diff` inspection: zero touch to
  `src/devices/cpu/` or `src/core/`. This is a structurally lower-risk milestone than M15/M16 on
  this axis, as the planner correctly anticipated (§2.4 of the planner package).

No new risk was introduced that is not already disclosed in `docs/m17-implementation-report.md`
§8 or the planner package §6. QA independently corroborates all listed residuals as accurate and
non-blocking.

## 7. Required Fixes

None blocking. No code changes required before the human release decision.

- **Recommended, non-blocking, for the coordinator to track at ledger update:** none beyond what
  is already correctly recorded — `agent-protocol/state/deferred-backlog.md`'s B3/B4/B6/E1/E2 rows
  and `agent-protocol/state/definition-of-done.yaml`'s M17 block (`implementation.*` true,
  `regression_qa.signoff_decision_recorded`/`overall_done` correctly left `false` pending this
  sign-off) are both internally consistent with the artifacts QA reviewed. No drift found.
- **Carried-forward condition (not new, not blocking M17):** whichever future milestone implements
  B6 (Halnote) must re-verify `BatteryBackedSram`'s load/save discipline against the real
  slot-0-3 SRAM window before wiring it in — this is already the primitive's own stated design
  intent (§3.3 of the planner package), not a new QA-invented condition.

## 8. Boundary + Backlog Check

- No DEFERRED item was implemented ahead of schedule. Verified absent/untouched this cycle: Kanji-
  font I/O (B5), Halnote firmware + real slot-0-3 SRAM wiring (B6), cartridge loading (B7), VDP
  rendering depth (D1-D7), printer/cassette (C7), Sony speed/pause (C8), SDL3 frontend (C9), exact
  cycle/write-timing (C1/C2/E2), ZEXALL (C3), YM2413 DSP/audio synthesis (E1).
- `deferred-backlog.md` correctly records **B3 → DONE (M17)**, **B4 → OPEN (re-grounded, re-owned
  to B6)**, **B6 → OPEN (confirmed true owner of B4)**, and the two new **E1/E2 → OPEN** rows; all
  prior rows re-affirmed with no silent drops. Footer updated same-cycle. Consistent with the
  planner package and the developer's own bookkeeping.

## 9. Sign-off Decision

**PASS.**

QA-executed evidence: fresh build clean (0 errors); **ctest 75/75 (0 failed)**, independently
re-run from a clean tree; the new `Ym2413Opll` device read and verified GENUINE
`MSXMusic`-pattern-only (no `MSXFmPac`-style bank-register/SRAM-handshake/ID-string logic found by
direct source read + targeted grep), grounded line-for-line against the actual
`references/openmsx-21.0/src/sound/MSXMusic.{hh,cc}` and `YM2413Okazaki.cc` (not just the
planner's or developer's transcription); the `BatteryBackedSram` primitive independently confirmed
NOT wired into `Hbf1xvMachine` by QA's own full read of the machine header/cpp (not just a
re-quoted grep); the `fmmusic_rom_` slot-3-3 regression guard confirmed meaningful (real,
non-degenerate ROM content compared before/after, not a zero-vs-zero no-op); X4 (CPU T-state math
untouched — zero `src/devices/cpu/`/`src/core/` diff, confirmed by direct inspection) and all
timing-oracle-bearing tests reproduce green; openMSX A/B evidence **independently re-executed by
QA end-to-end** against a real WSL openMSX 19.1 process (not merely read from the artifact) —
Subject 1 empty diff over 145/145 instructions, Subject 2 empty diff over 36/36 register
addresses, both fully reproducible (zero diff on regenerated fixtures); the R-M17-6
register-introspection mechanism was additionally, independently re-verified by QA with a second,
different register address beyond the developer's own spot-check, ruling out a coincidental
single-address match; the full deferred-backlog review is genuinely complete (every row
re-affirmed, none silently dropped, B4's correct non-closure independently confirmed as the
technically correct disposition).

No blocker-level gaps remain. No fabricated hardware behavior found. No test was weakened.

Per the milestone rule, this PASS authorizes the coordinator to PRESENT M17 for the human release
decision (normal gate); it does not auto-close M17. No blocking conditions are attached — the two
carried-forward notes in §7 are informational, not gating.
