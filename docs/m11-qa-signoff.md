# M11 QA Sign-off — S1985 "MSX-ENGINE" Chipset & Full System Bus

- Milestone ID: M11
- Request: REQ-M11-004
- QA Owner: MSX QA Agent
- Date: 2026-07-06
- Authority: `agent-protocol/state/milestones.md` (M11), `docs/m11-planner-package.md`,
  `docs/m11-implementation-report.md`, `docs/m11-parity-trace-diff.md`,
  `agent-protocol/guardrails.md`.
- Grounding: `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` (§3/§4/§6/§8/§9/§10),
  openMSX behavior reference `references/openmsx-21.0/src/MSXS1985.*`, `MSXCPUInterface.*`;
  WSL A/B runtime `/usr/share/openmsx/machines/Sony_HB-F1XV.xml`.

All evidence below was independently executed or re-read by QA. Developer numbers were NOT
trusted; every figure is QA-observed.

---

## 1. Regression Scope

- Chipset fabric: `src/devices/chipset/{slot_bus,ppi_slot_select,io_bus,mapper_io,switched_io,
  s1985_engine,system_bus}.*`.
- Core contracts / bus seam: `src/core/bus.h`, `src/core/device_contracts.h`.
- CPU I/O seam + M1 surfacing: `src/devices/cpu/{cpu_bus_client,z80a_cpu}.*`.
- Machine composition: `src/machine/{hbf1xv_machine,ram_slot_backing}.*`.
- Cross-cutting regression risk (R-3): the 8 machine-level timing oracles whose totals change
  because the S1985 +1-per-M1 wait is now added by the scheduler.
- Asset-dependent flow: A/B parity harness `tools/openmsx-io-parity.ps1` over
  `tests/parity/m11_bus_probe.bin` against the genuine openMSX Sony HB-F1XV machine.

## 2. Regression Matrix Status

QA build: `cmake --build build --config Debug` → exit 0 (only pre-existing C4819 non-ASCII code
page warnings; no errors, no new warnings).

QA test run: `ctest --test-dir build -C Debug --output-on-failure` →
**100% tests passed, 0 failed out of 38** (QA-executed, not developer-reported).

M11-specific tests confirmed registered and green (QA filtered run
`ctest -R "chipset|io_seam|slot_map"` → 8/8, plus the two integration tests #27/#34):

| # | Test | Result | Acceptance mapping |
|---|---|---|---|
| 4 | devices_cpu_io_seam_unit_test | Pass | AC3 (M1 count), CPU I/O seam |
| 5 | devices_chipset_slot_bus_unit_test | Pass | AC1 |
| 6 | devices_chipset_ppi_slot_select_unit_test | Pass | AC1 |
| 7 | devices_chipset_io_bus_unit_test | Pass | AC2 |
| 8 | devices_chipset_backup_ram_unit_test | Pass | AC3 |
| 9 | devices_chipset_mapper_io_unit_test | Pass | AC3 |
| 10 | devices_chipset_m1_wait_unit_test | Pass | AC3 (M1 wait) |
| 22 | machine_hbf1xv_slot_map_unit_test | Pass | AC4, R-1 |
| 27 | machine_hbf1xv_system_bus_integration_test | Pass | AC1/AC2/AC3/AC4 |
| 34 | machine_hbf1xv_m11_parity_checkpoint_integration_test | Pass | AC6 |

### Per-acceptance-criterion verdict (with concrete evidence)

**AC1 — slot-decoded memory bus (primary `#A8` + secondary `#FFFF`, `0xFF^reg`) replaces flat
bus; MET.** `chipset_slot_bus_unit_test.cpp` verifies page→primary routing, `#A8` readback,
attach/resolve with address offset, expanded-slot-3 `#FFFF` write→`0xFF^reg` readback, and the
non-expanded case where `#FFFF` is ordinary memory (lines 51–155). End-to-end through the CPU:
`hbf1xv_system_bus_integration_test.cpp` asserts `#FFFF` readback `0xFF^0xC0 = 0x3F` (line 86).
The flat `MachineBus` is gone (report §3; machine now composes `SystemBus`).

**AC2 — I/O dispatch by port with `#9C-9F`/`#AC-AF` mirrors as aliases; unmapped → `0xFF`; MET.**
`chipset_io_bus_unit_test.cpp` verifies dispatch on `port&0xFF`, `#AC`→`#A8` and `#9C`→`#98`
straight-alias mirror (read and write both land on the base device), and open-bus `0xFF` on
unmapped ports (lines 47–107). Machine-level: `#AC == #A8` mirror oracle (integration test
res[1]==res[0], line 77).

**AC3 — S1985 layer (backup RAM ID `0xFE`, mapper `100xxxxx`, +1 M1 wait); MET.**
`chipset_backup_ram_unit_test.cpp` checks ID-select via `#40`, `~ID=0x01`, `address=value&0x0F`
(0x1A→index 0x0A), sram round-trip, `color2<-color1;color1<-v`, index-7 rotating pattern
(bit7 → color2 else color1, then left-rotate), other indices `0xFF`, and reset clears
(lines 41–105). `chipset_mapper_io_unit_test.cpp` checks `0x80|(seg&0x1F)` for 0x00→0x80,
0x1F→0x9F, 0x25→0x85, 0xFF→0x9F (lines 34–49). `chipset_m1_wait_unit_test.cpp` cross-checks the
wait against the REAL CPU M1 count for the fact-sheet §8 examples (XOR A 4→5, BIT 0,(HL) 12→14,
OUT (n),A 11→12).

**AC4 — system integration: machine boots/steps over the new bus, DRAM as slot 3-0, prior suites
green; MET.** `hbf1xv_slot_map_unit_test.cpp` shows all 4 pages resolve to slot 3-0 DRAM, CPU
read+write route to DRAM, `#FFFF` returns `0xFF` not DRAM. All 38 tests green (§2).

**AC5 — evidence gates executed and green; MET.** QA re-ran build (exit 0) and ctest (38/38).
Asset gates recorded in report §6; `docs/asset-checksums.txt` present.

**AC6 — real A/B trace-diff, no fabricated parity; MET.** See §5 — QA independently reproduced
the diff. Genuine capture, both sides driven.

**AC7 — QA sign-off recorded; MET by this document.**

## 3. Genuine-Test Verification (non-stub)

QA read every M11 test's source and confirmed each assertion checks the behavior it claims —
none are trivially-true or self-referential:

- **Slot decode** (`chipset_slot_bus_unit_test.cpp`): `TagDevice` returns `tag | (addr&0x0F)` so
  BOTH the resolved device and the offset it received are observable; primary `#A8=0b01000100`
  routes each page to the correct slot; `#FFFF` readback asserts `0xFF^reg` (0x00 for reg 0xFF,
  0xFE for reg 0x01) and the expanded/non-expanded split is distinguished. Genuine.
- **I/O dispatch + mirrors** (`chipset_io_bus_unit_test.cpp`): mirror writes reach the base device
  and base reads see mirror writes; `last_write_port` captured to prove the alias, not a copy.
  Genuine.
- **Mapper readback** (`chipset_mapper_io_unit_test.cpp`): asserts the exact `100xxxxx` pattern
  including upper-bit masking (0x25→0x85), plus per-page independence. Genuine.
- **Backup-RAM switched device** (`chipset_backup_ram_unit_test.cpp`): device ID 0xFE select,
  `~ID=0x01`, address masked to low nibble, rotating pattern with color1/color2 selection, reset
  clears the volatile store. Genuine.
- **M1-wait oracle** (`chipset_m1_wait_unit_test.cpp` + `cpu_io_seam_unit_test.cpp`): the wait is
  cross-checked against the CPU's actual `m1_cycles_last_step()` for real opcodes; datasheet
  T-states asserted unchanged (OUT/IN stay 11). Genuine.

Implementation cross-check: `z80a_cpu.cpp:192-205` — the M1 counter increments exactly once per
opcode/prefix fetch (`fetch_opcode()` line 159) and once for the maskable-interrupt acknowledge
M1, tied to the R-refresh increment. `hbf1xv_machine.cpp:133-134` applies
`datasheet_tstates + s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step())`. The wait is
isolated to the machine; CPU datasheet T-states are unchanged (A-4 confirmed).

## 4. R-3 Timing-Oracle Hand-Check (independent re-derivation)

QA re-derived the recomputed oracles from each instruction's M1 fetch count; all are **strict
`==` assertions** (not loosened bounds) and reflect datasheet + one-M1-wait-per-opcode-fetch:

- **`hbf1xv_cpu_step_integration` 19→22 (VERIFIED).** LD A,n(7)+INC A(4)+HALT(4)+halted-idle(4)
  = 19 datasheet. M1 waits: LD A,n 1, INC A 1, HALT 1, idle 0 (no fetch) = 3 → 22. Matches
  `elapsed_cycles()==22` (line 67).
- **`hbf1xv_ldir_program_integration` 92→102 (VERIFIED).** LD HL/DE/BC nn (10×3) + LDIR
  (21+21+16) + HALT(4) = 92 datasheet. The core models the Z80 LDIR repeat as genuine ED+B0
  re-fetches (hardware re-fetches on each iteration by decrementing PC — this is why block ops
  are interruptible), so 3 iterations × 2 M1 = 6, plus 3 loads + HALT = 4 → 10 waits → 102.
  Matches `elapsed_cycles()==102` (line 75). The 2-M1-per-iteration model is hardware-correct.
- **`hbf1xv_interrupt_ack_integration` IM2 44→49 (VERIFIED).** IM2 ack(19)+LD A,n(7)+RETI ED-4D(14)
  +HALT(4) = 44 datasheet. M1: ack 1 + LD A,n 1 + RETI 2 (ED+4D) + HALT 1 = 5 → 49. Matches
  `cycles==49` (line 75). (IM0 companion 34→38 also re-derived and correct, line 118.)

Conclusion: the R-3 updates are a correctness improvement (MSX-accurate totals), **not** weakened
assertions. No timing regression.

## 5. A/B Adversarial Validation (AC6)

QA did not trust the committed diff. QA verified the runtime capability and **independently
re-ran** `tools/openmsx-io-parity.ps1` (writing to a scratch diff, TraceA=`build/qa_m11_A.txt`,
TraceB=`build/qa_m11_B.txt`):

- **openMSX present and real:** `/usr/bin/openmsx`, `openMSX 19.1` (WSL). Confirmed directly.
- **Reference machine genuinely instantiates an S1985:** `/usr/share/openmsx/machines/
  Sony_HB-F1XV.xml` contains `<S1985 id="S1985">` with the "includes 5 bits mapper-read-back"
  comment and the `#A8`/`#98` S-1985 mirror `<io>` declarations. This is the exact target
  hardware model, not a generic profile.
- **Both sides demonstrably driven:** QA's re-run reported `Trace A (emulator) instructions: 15`
  and `Trace B (openMSX HB-F1XV) instructions captured: 15`. The B side is a real per-instruction
  register capture (15 `OMTRACE` lines, seq 0–14) preceded by `OMSETUP ram_base_readback=3E`
  (proving the probe was actually written into and read back from openMSX RAM at 0xC000).
- **The B side shows S1985-specific results, not open bus:** QA re-captured B trace shows
  `seq=3 af=8500` (mapper `100xxxxx` readback of seg 0x25 → 0x85), `seq=7 af=0100`
  (switched-I/O `~ID` = 0x01), `seq=13 af=3C00` (16-byte backup-RAM round-trip → 0x3C). If
  openMSX lacked the S1985, these three ports would return open-bus `0xFF`, not the exact
  S1985 values. They match this emulator field-for-field.
- **Result:** `M11 A/B RESULT: ARCHITECTURAL PARITY (empty diff)`. The empty diff is valid
  because both traces contain 15 real records (not a 0-vs-0 false empty). The shared M10
  comparator was adversarially validated at M10 (empty-side→BLOCKED, corrupted field→DIVERGENCE)
  and re-exercised here with 15/15 real rows.

**A/B validation outcome: parity PROVEN (genuine, reproduced by QA).** Not fabricated, not
blocked.

Honest scope note (accepted, consistent with the artifact): the probe is deliberately slot-safe
and BIOS-independent; the `#A8`/`#AC` mirror absolute values are BIOS-dependent so only the
mirror invariant is exercised A/B (mirror equality is covered by unit + machine integration
tests). Cycle/T-state parity is not gated A/B (DP-4; openMSX inserts M1 waits, emulator reports
canonical Z80 T-states in trace A). These are pre-existing, documented boundaries — not M11
defects.

## 6. Failures and Risk Ranking

No failures. No regressions detected across M0–M10 (all 38 tests green; prior CPU/scheduler/
machine oracles either unchanged or correctly recomputed for the M1 wait).

Residual risks (none blocker-level):

| Risk | Severity | Disposition |
|---|---|---|
| R-1 / A-2: `cold_boot` sets `#A8=0xFF` (all pages → slot 3-0 DRAM), deviating from the real `#A8=0`→slot-0 BIOS reset | Low | **Accepted and tracked.** Pinned by `hbf1xv_slot_map_unit_test.cpp` (`ResetPrimarySelect_A8_IsFF_M11BringUpDefault`), commented in source, documented planner §1.3/§9 and report §8. This is a deliberate bring-up default (no BIOS ROM until M12). **Carried to M12: its planner MUST flip the reset default to `#A8=0` once slot 0 is populated.** Not resolvable in M11 (no ROMs in scope). |
| A-5 / R-6: backup-RAM `.sram` persistence not implemented (volatile store, cleared on reset) | Low | **Accepted, out of M11 scope.** openMSX persists 16 bytes to `.sram`; M11 models volatile semantics only, verified by `Reset_ClearsBackupRam`. No owning milestone yet — flag to coordinator if a later milestone needs persistence. |
| Minor: A/B runtime is openMSX 19.1 while the grounding source tree is 21.0 | Info | **Accepted.** Same 19.1 runtime used for the M10 parity gate; S1985 behavior is stable across these versions and the reference machine genuinely instantiates the S1985. Not a defect. |

## 7. Required Fixes

None for M11. One forward obligation to record for the next milestone:

- **M12 must flip the reset slot default** from the M11 bring-up `#A8=0xFF` to the authentic
  `#A8=0` (slot-0 BIOS) once ROM devices populate slot 0 (tracks R-1). This is a milestone
  hand-off, not an M11 fix.

## 8. Sign-off Decision

**PASS.**

- QA-executed `ctest`: **38/38 passed, 0 failed** (own run).
- All six M11 acceptance criteria met with concrete, genuine (non-stub) evidence.
- R-3 timing oracles independently hand-re-derived (4 spot-checks: cpu_step 22, ldir 102,
  IM2 49, IM0 38) — all strict-equality and MSX-accurate; no timing regression.
- A/B parity independently reproduced by QA: both sides driven (15 vs 15), empty diff, with the
  genuine openMSX Sony HB-F1XV S1985 returning the exact S1985 values (0x85 / 0x01 / 0x3C).
- No regression across M0–M10.
- Residual risks R-1 (M12-tracked) and R-6 (accepted, out of scope) are non-blocking and
  properly recorded.

Sign-off conditions: none blocking. Forward obligation only — M12 planner MUST flip the reset
slot default (R-1). Closeable on human release decision.
