# M13 QA Sign-off — RAM/ROM Memory Devices & Slot Population

- Milestone: M13 (RAM/ROM Memory Devices & Slot Population; VRAM excluded — owned by M14).
- Request: REQ-M13-004. QA Owner: MSX QA Agent. Date: 2026-07-06.
- Inputs reviewed: `docs/m13-planner-package.md`, `docs/m13-implementation-report.md`,
  `docs/m13-parity-trace-diff.md`, source under `src/devices/memory/` + `src/machine/`, the new/updated
  tests, `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`, `agent-protocol/state/milestones.md`,
  `agent-protocol/guardrails.md`.
- Gate note: M13 retains the NORMAL human-release-decision gate. This QA Pass authorizes the coordinator
  to PRESENT M13 to the human; it does not auto-close.

All findings below are from QA-executed commands and QA-read source, not developer-reported numbers.

---

## 1. Regression Scope

Affected surfaces re-assessed:
- New memory devices: `RomDevice` (read-only window), `MemoryMapperRam` (64 KB consumer of M11 `MapperIo`).
- Machine wiring: slot population (0-0 BIOS, 3-0 RAM, 3-1 SUB+Kanji, 3-2 DISK, 3-3 FM-MUSIC), both primary
  slots 0 & 3 expanded, `#A8=0xFF`→`#A8=0` reset flip, A-5 RAM power-on pattern, ROM asset loading + missing-asset policy.
- Prior M0–M12 suites reconciled by the reset flip and the now-real mapper (13 flat-RAM tests + slot-map,
  memory-regions, debug-dump, cpu-parity, system-bus).
- A/B parity vs openMSX (RAM/ROM probe + BIOS-boot checkpoint), VRAM excluded.

## 2. QA-Executed Evidence Gates

- Build dir already configured `SONY_MSX_ENABLE_SDL3:BOOL=OFF` (verified in `build/CMakeCache.txt`).
- `cmake --build build --config Debug` → exit 0 (only pre-existing C4819 code-page warnings; zero errors).
- `ctest --test-dir build -C Debug --output-on-failure` → **100% tests passed, 0 failed out of 50**
  (QA-executed; matches the developer-reported 50/50).
- Local BIOS SHA1s independently computed via `sha1sum` (see §7).
- A/B boot subject independently reproduced by QA against real openMSX on WSL (see §7).

## 3. Per-Acceptance-Criterion Assessment

| # (planner §8 / ledger) | Criterion | Verdict | Evidence |
|---|---|---|---|
| 1 | 64 KB `MemoryMapperRam` in slot 3-0, `#FC-#FF` segments consumed from `MapperIo`; readback independence | MET | `memory_mapper_ram.{h,cpp}`: `physical_address` folds `seg & 3`; holds `const MapperIo&`, calls only `segment()`. Single owner (§4). Unit + slot-map tests green. |
| 2 | ROM devices at exact XML cells; both slots 0 & 3 expanded; DISK page-1-only | MET | `wire_bus()` matches XML lines 87-196 (§5). `set_expanded(0,true)`+`set_expanded(3,true)`. DISK window = page 1 → 0xFF elsewhere by construction. |
| 3 | Assets loaded from verified `bios/`; missing-asset deterministic (0xFF-fill + note), no fabrication | MET | `rom_asset_loader.cpp`: absent/unreadable/size-mismatch → 0xFF-fill of exact expected size + diagnostic; never a fabricated SHA (§6). |
| 4 | Reset flipped `#A8=0xFF`→`0`; boot checkpoint proves real BIOS opcode fetch/execute; suites reconciled | MET | `cold_boot()` sets `#A8=0`+`write_ffff(0)`. Boot test golden self-derived from image; PC advances 0000→043C (§5). Suites reconciled (§6). |
| 5 | System integration: CPU fetch/execute from ROM + read/write mapper RAM through M11 bus; boot reaches checkpoint | MET | Boot + m13 parity-checkpoint integration tests green; QA boot trace advances through real BIOS. |
| 6 | Per-slice evidence gates green | MET | QA build exit 0; ctest 50/50 (§2). |
| 7 | Real A/B trace-diff vs openMSX, VRAM excluded | MET | QA independently reproduced empty boot diff vs real openMSX Sony_HB-F1XV (§7). |
| 8 | QA sign-off recorded before closure | MET | This document. |

## 4. Double-Ownership Check (single owner of the mapper registers)

CONFIRMED SINGLE OWNER. In `wire_bus()` the ONLY device attached to ports `#FC-#FF` is `mapper_io_`
(`hbf1xv_machine.cpp:76-78`). `MapperIo` (`mapper_io.h`) holds the four segment registers and the S1985
`0x80 | (seg & 0x1F)` (`100xxxxx`) 5-bit readback; it is the sole writer/owner. `MemoryMapperRam` holds a
`const chipset::MapperIo&` and only *reads* `segment(page)` — it declares/owns no register and no readback.
The two paths authentically apply different masks: 5-bit readback (chipset) vs 2-bit physical fold `seg & 3`
(device), grounded in `references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:72-83` and XML line 25. No
double-declaration of `#FC-#FF`. The retired `src/machine/ram_slot_backing.*` is confirmed DELETED (no dead
flat-RAM path).

## 5. Boot-Checkpoint Genuineness

VERIFIED GENUINE (not a hardcoded constant that would pass with boot broken):
- `hbf1xv_bios_boot_integration_test.cpp` reads the real `f1xvbios.rom` from disk and, for every fetched
  opcode while the page routes to slot 0, asserts `fetched == bios[pc]` — a **self-derived** image golden.
  Open bus (0xFF) or RAM as the fetch source would fail this invariant.
- It asserts `bus[0x0000] == bios[0]`, `bios[0] != 0xFF` (open bus = RST 38h would be wrong), and reset
  `PC == 0x0000`. QA independently confirmed `bios/f1xvbios.rom` bytes 0-3 = `F3 C3 16 04` (DI; JP 0x0416).
- It asserts `trace.back().pc != 0x0000` — PC actually ADVANCED (no trivial stall/loop at the reset vector).
  QA's own run reached `final_pc=0x043C` after 24 steps, fetching F3→C3→(jump 0x0416)→AF… i.e. real BIOS flow.
- Deterministic across two cold_boot+run cycles.
The absolute PC is cross-validated against openMSX in §7 (not memorized).

## 6. Updated-Test Regression Audit (two required spot-checks)

The developer updated prior tests for two authentic hardware changes: the reset flip (`#A8` 0xFF→0) and the
A-5 RAM pattern, plus `map_flat_ram()` on 13 CPU-over-RAM tests (explicit paging, not a hidden default).
QA scrutinized the two highest-risk edits:

(a) **cpu_parity OUTI `#FC`→`#FD` retarget — LEGITIMATE, not a masked mapper bug.** Under the now-REAL
mapper the program runs from page 0 (segment register `#FC`). `OUT (#FC),0xFF` would set page-0 segment to
`0xFF & 3 = 3` → remap the executing code page to physical `0xC000`, corrupting the very fetch stream. `#FD`
targets the page-1 segment (never accessed by the program), so the OUTI behavior under test is unchanged:
B decrement, HL increment, NMOS carry from `data(0xFF)+newL(0x01)=0x100`→carry set, I/O dispatch on
`port & 0xFF`, and readback `0x80|(0xFF&0x1F)=0x9F` all identical. The M12 whole-program timing oracle is
unchanged at **45** (40 datasheet + 5 M1 wait). This is a self-modification avoidance, not a weakening.

(b) **debug_dump DRAM golden A-5 change — AUTHENTIC and STRENGTHENED; M12 CPU behavior unchanged.** QA
adversarially decoded the actual XML gz-base64 `initialContent` (`Sony_HB-F1XV.xml:129`) with zlib and
confirmed it equals `(00,FF)*128 + (FF,00)*128` (512 bytes) — matching `initialize_dram_pattern()` and the
test's `a5_byte()` exactly (verified from the binary, not merely the XML comment). The new golden checks the
WHOLE 64 KB via the production `serialize_region` over an independently-built buffer (stronger than the old
partial all-zero fold). The CPU golden (`A=2D F=28 PC=0006 R=04 TSTATES=22` datasheet) is UNCHANGED — the M12
CPU-parity behavior is untouched; the M1 wait affects only the machine scheduler's `elapsed_cycles`, not the
CPU-section datasheet T-states. SRAM/VRAM goldens unchanged.

No prior test was disabled, deleted, or had an assertion relaxed. Both spot-checks reflect authentic hardware
values with sound justification.

## 7. A/B Adversarial Validation (incl. SHA1-match confirmation)

**SHA1 match — INDEPENDENTLY CONFIRMED (load-bearing).** QA computed local BIOS SHA1s; all five equal the
XML "confirmed by Meits" revisions, so both emulators run identical images (an empty boot diff is meaningful):

| Asset | Local SHA1 (QA `sha1sum`) | XML line |
|---|---|---|
| f1xvbios.rom | 174c9254f09d99361ff7607630248ff9d7d8d4d6 | 95 |
| f1xvext.rom | fe0254cbfc11405b79e7c86c7769bd6322b04995 | 142 |
| f1xvkdr.rom | dcc3a67732aa01c4f2ee8d1ad886444a4dbafe06 | 154 |
| f1xvdisk.rom | 5a4e7dbbfb759109c7d2a3b38bda9c60bf6ffef5 | 172 |
| f1xvmus.rom | aad42ba4289b33d8eed225d42cea930b7fc5c228 | 188 |

**QA reproduced the BIOS-boot subject** against real openMSX on WSL (`/usr/bin/openmsx`,
`/usr/share/openmsx/machines/Sony_HB-F1XV.xml` present; openMSX reports `reset_vector=F3` matching our BIOS):
- Emulator trace A (`build/qa_boot_A.txt`) = 24 instructions; openMSX trace B (`build/qa_boot_B.txt`) = 24
  instructions, both opening F3 → C3 → (0416) AF.
- **Genuine A vs B: `tools/trace-diff.py` exit 0 — ARCHITECTURAL PARITY, EMPTY DIFF** over all 24 instructions.

**Comparator is not rigged (adversarial self-check reproduced by QA):**
- Corrupt one B register (`sp FFFF→1234`): exit **1 — DIVERGENCE**, concrete row `| 2 | 0416 | sp | FFFF | 1234 |`.
- Empty B side: exit **2 — BLOCKED** ("a trace side is empty"). An empty side can never yield a pass.

Source review of `trace-diff.py` confirms: `if not a or not b:` → exit 2; any architectural-field mismatch →
exit 1; only a fully-matching equal-length pair → exit 0. The RAM/ROM probe subject (Subject 1) was reported
empty-diff by the developer; QA did not re-drive it separately but validated the identical comparator + machine
+ SHA1 basis, so the claim rests on the same verified pipeline. Boundary honesty is intact: the boot window is
bounded before the BIOS first READS an unimplemented device (VDP/PSG/RTC, M14+), which is a device-scope
boundary, not an M13 memory defect.

## 8. Missing-Asset Policy

DETERMINISTIC and NON-FABRICATED (`rom_asset_loader.cpp`): a required file that is absent / unreadable /
wrong-size yields a `0xFF`-filled image of the exact expected window size plus a recorded diagnostic in
`rom_diagnostics()`; never a silent zero-fill, never a fabricated SHA or provenance. A green run has
`rom_diagnostics()` empty (asserted by the boot + slot-map tests). Diagnostics deliberately stay out of the
M10 event-log stream to keep it byte-deterministic — a sound, documented choice.

## 9. Residual-Risk Disposition

| Risk | Disposition |
|---|---|
| R-1 / A-2 reset flip regression | RESOLVED — flip in `cold_boot`; boot checkpoint + suite reconciliation green (M11 forward-obligation discharged). |
| R-2 segment mapping vs flat-RAM | RESOLVED — `map_flat_ram()` explicit; `load_memory`/`read_memory` linear debug accessors preserved. |
| R-3 / A-5 RAM init pattern | RESOLVED — QA decoded XML binary; pattern authentic; goldens updated (strengthened), not weakened. |
| R-4 openMSX driveability / BIOS presence | RESOLVED — QA drove openMSX Sony_HB-F1XV directly; SHA1 identity confirmed. |
| R-5 / A-4 unpopulated-segment wrap + reset defaults | RESOLVED — grounded in `MSXMemoryMapperBase.cc:72-83`; `seg & 3`, cold-boot segments 0. |
| R-6 DISK/FM ROM presence without device I/O | ACCEPTED & TRACKED — boot window bounded before device reads; FDC/FM internals owned by later milestones. |
| D-3 Halnote/MSX-JE (0-3) | ACCEPTED & DEFERRED — reserved open-bus; future Halnote milestone. |
| D-4 Kanji font `#D8-#DB` | ACCEPTED & DEFERRED — I/O-accessed, orthogonal; future Kanji/peripheral milestone. |
| D-5 cartridge slots 1-2 | ACCEPTED & DEFERRED — future cartridge-loading milestone. |

No blocker-level gap remains. All deferrals are named, justified, and owned by future milestones — consistent
with the M13 scope fence (VRAM/VDP=M14; Halnote/FM-PAC/FDC/Kanji-font internals out).

## 10. Failures and Risk Ranking

No failures. No high/medium regressions found. Low-severity, non-blocking observations:
- The raw A/B artifacts named in the report (`build/m13_*.txt`) are transient harness outputs and were not
  present in the fresh build dir; QA regenerated equivalent independent evidence (`build/qa_boot_A.txt`,
  `build/qa_boot_B.txt`, `build/qa_boot_diff.md`) confirming the empty-diff claim. Non-blocking.
- Cross-emulator cycle/T-state parity remains out of scope (M1 wait-states) — reported separately, never gated,
  consistent with prior milestones.

## 11. Required Fixes

None required for sign-off.

## 12. Sign-off Decision

**PASS.**

Rationale: QA independently rebuilt (exit 0) and re-ran the suite (**50/50**, zero failures); the memory devices
are genuine and non-stub with a single confirmed owner of the mapper registers; slot population matches the
authoritative `Sony_HB-F1XV.xml`; the reset flip + boot checkpoint are real and self-grounded with PC advancing
through actual BIOS; the two highest-risk prior-test updates were verified authentic and non-weakening; the
missing-asset policy is deterministic and non-fabricated; and QA independently reproduced the openMSX A/B
empty boot diff on SHA1-identical images with an adversarially-validated comparator (divergence and empty-side
both correctly reported). No blocker-level gap remains; all deferrals are named, justified, and tracked.

Per the milestone gate, this Pass authorizes the coordinator to present M13 to the human for the normal release
decision; it does not auto-close.
