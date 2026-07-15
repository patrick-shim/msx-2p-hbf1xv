# M12 Planner Package — Z80A CPU End-to-End Parity Review & Hardening

- Milestone ID: M12
- Title: Z80A CPU End-to-End Parity Review & Hardening (HB-F1XV genuine Zilog NMOS Z80A)
- Source Request: REQ-M12-002
- Spec Owner: MSX Planner Agent
- Authority: DEC-0003 (`agent-protocol/channels/decisions.md`), `agent-protocol/state/milestones.md` (M12)
- Grounding read this cycle (concrete files):
  - `references/fact-sheets/Zilog Z80A CPU.md` (parity rubric §3/§4/§5/§8)
  - `references/openmsx-21.0/src/cpu/CPUCore.cc` (behavior reference; read `scf`/`ccf` 4257-4274/4187-4206, `retn` 3911-3915, `daa` 4220-4238, `setMemPtr` sites 364/789/2711/3247/3303..)
  - Current implementation: `src/devices/cpu/z80a_cpu.cpp` (1686 lines), `z80a_cpu.h`, `z80a_state.h`, `cpu_bus_client.h`
  - `docs/m9-implementation-report.md`, `docs/m11-qa-signoff.md` (§4 the 8 recomputed timing oracles), `tools/openmsx-trace-parity.ps1`, `tools/openmsx-io-parity.ps1`
- Scope discipline (DEC-0003 / DEC-0003 Risk Notes): this is a **parity-hardening pass, NOT a rewrite**. The M9 core already passed QA as non-stub with complete opcode families + interrupt architecture. No gratuitous refactor. Every code change must be justified by a concrete parity gap below.

---

## 1. Scope and Assumptions

### 1.1 In scope
- Close the residual **undocumented / NMOS-exact** behavior gaps in the existing Z80A core and **prove** parity for items already implemented.
- All CPU work stays under `src/devices/cpu/` (DEC-0003). New files permitted: a WZ/Q hardening in the existing state struct, optional flag-table helper, and new test modules. No new production module unless a gap requires it.
- A full **system-integration** test exercising the CPU end-to-end through the **M11 `SystemBus`** (not isolated).
- A real, captured **openMSX A/B trace-diff** and a **ZEXDOC/ZEXALL** self-checking harness assessment (see §5).

### 1.2 Non-goals / out of scope
- No rewrite of the opcode dispatch, no restructuring of the M9 x/y/z/p/q decoder.
- No VDP `/INT` level-hold semantics — that is a VDP (M14) / machine concern; M12 only verifies the CPU's acceptance/EI-delay/timing at the CPU boundary.
- No R800 behavior (SLL-as-SLA, different X/Y) — the HB-F1XV is genuine Zilog NMOS only (fact sheet §9).
- The M11 forward-obligation to flip reset `#A8=0xFF -> #A8=0` belongs to **M13** (memory), NOT this milestone (DEC-0003 Risk Notes) — do not touch it here.
- The MSX **M1 wait** stays owned at the machine level (M11); M12 must not move or double-count it (see §4).

### 1.3 Assumptions (each with a verification action)
- **A-1:** ZEXDOC/ZEXALL binaries (`zexdoc.com`, `zexall.com`, Frank Cringle's Z80 Instruction Exerciser) are freely redistributable test artifacts and may live under `tests/parity/`. *Verify:* confirm a legally-sourced copy is placed by the developer; record its SHA-256 in `docs/asset-checksums.txt`. If a copy cannot be sourced, the ZEX harness degrades to "assessed, deferred" and the openMSX A/B trace-diff becomes the sole cross-check (see §5.3). **No fabricated pass/fail.**
- **A-2:** openMSX on WSL is 19.1 (per M10/M11 evidence) while the grounding tree is 21.0; S1985 + Z80 documented behavior is stable across these. *Verify:* `openmsx --version` at harness run; record in the diff artifact.
- **A-3:** openMSX's `reg` debug interface does **not** expose WZ/MEMPTR, so WZ cannot be compared field-for-field A/B. *Verify:* WZ correctness is proven indirectly via `BIT n,(HL)` X/Y flag output and via ZEXALL's `bit` CRC, not via trace-diff.
- **A-4:** openMSX 21.0 `scf`/`ccf` (`CPUCore.cc:4257-4274`) use the **(F | A)** OR-form for X/Y and do **not** implement the Patrik-Rak Q-latch; a correct genuine-NMOS Q-latch will therefore **diverge from openMSX** in the "previous instruction modified flags" case. *Verify:* gate SCF/CCF X/Y on the fact-sheet Q rule + ZEXALL (whose CRCs come from real Zilog silicon), not on the openMSX trace-diff; document this as a known benign A/B boundary.

---

## 2. Spec Summary

The target is a **genuine Zilog NMOS Z80A** (fact sheet "Bottom line", §9): full documented + undocumented opcodes, WZ/MEMPTR tracking, Zilog SCF/CCF-Q X/Y, NMOS `OUT (C),0`=0, NMOS `LD A,I/R` interrupt P/V bug, correct R increments, MSX M1 wait (+1/opcode fetch, machine-owned), intrinsic I/O wait, IM1 acceptance, one-instruction EI delay. The audit below (§3) shows the M9 core is **largely at parity for documented + most undocumented behavior**, with a small set of genuine, well-localized gaps concentrated in **WZ/MEMPTR**, **SCF/CCF-Q**, and **two NMOS interrupt-edge behaviors**. The bulk of M12 is therefore **proving** parity (test additions + ZEX + A/B), with targeted fixes for the identified deltas.

### Boundaries / dependency map (`src/core|devices|peripherals|machine|frontend`)
- `src/devices/cpu/` — all M12 production edits (state adds `wz`/`q`; a few update-site writes; SCF/CCF/RETI/LD-A,I edits). Owner of the parity behavior.
- `src/machine/` — no behavior edits expected; it composes CPU + `SystemBus` and applies the M1 wait (`hbf1xv_machine.cpp:133-134`, per M11 QA §3). M12 adds a system-integration **test** here, and any reset-state coordination for the new WZ/Q fields (must default deterministically).
- `src/core/` — unchanged (bus contracts stable). `src/peripherals/`, `src/frontend/` — untouched.
- `tests/` — new unit + integration + system (ZEX) suites.
- `tools/` — new `openmsx-cpu-parity.ps1` (extends `openmsx-trace-parity.ps1`) and optional `run-zextest.ps1` / headless CLI mode.

---

## 3. Per-item Gap-Analysis Table

Status legend: **PRESENT** (implemented + correct on read), **DIVERGENT** (implemented but wrong/incomplete vs rubric), **ABSENT** (not implemented), **UNVERIFIED** (cannot confirm correctness from static read — a slice must prove it).

Evidence column cites our `file:line` (in `src/devices/cpu/`) vs `references/...`.

| # | Item | FS § | Expected behavior | Status | Evidence (ours vs reference) |
|---|------|------|-------------------|--------|------------------------------|
| 1 | Undocumented X/Y (F3/F5) on 8-bit results | §2, §4 | X=bit3, Y=bit5 of result | **PRESENT** | `z80a_cpu.cpp:356,376,452,631` OR `result & kXY`; matches FS §2 table `sz53p` model |
| 2 | X/Y from 16-bit high byte (ADD/ADC/SBC HL) | §4 | X/Y from bits 11/13 (= high byte bits 3/5) | **PRESENT** | `alu_add16` `:470`, `alu_adc16` `:983`, `alu_sbc16` `:1010` use `(result>>8)&kXY` |
| 3 | WZ/MEMPTR register exists & tracked | §4 | Internal 16-bit WZ updated by many ops | **ABSENT** | No `wz` field in `z80a_state.h:13-46` or `:99-109`; no `setMemPtr` analog anywhere in `z80a_cpu.cpp`. Ref: `CPUCore.cc` setMemPtr at 364/789/2711/3247/3303/3320/3339... |
| 4 | `BIT n,(HL)` X/Y from WZ hi byte | §4 | X/Y = bits 11/13 of WZ, NOT the tested value | **DIVERGENT** | `execute_cb_prefixed` `:944` passes `value` as undoc source (`alu_bit(y,value,value)`); M9-S2 addendum admits the simplification (`docs/m9-implementation-report.md:138-140`). Ref `CPUCore.cc` bit ops use memptr hi |
| 5 | `BIT n,(IX/IY+d)` X/Y from WZ hi byte | §4 | X/Y = (IX+d) high byte (= WZ hi) | **PRESENT** | `execute_indexed_cb` `:1615` uses `address>>8`; equals WZ hi for indexed since WZ=IX+d. Keep, and route via the new WZ for consistency |
| 6 | `LDI/LDD/LDIR/LDDR` flag quirks | §4 | Y=bit1, X=bit3 of `n=A+value`; H=N=0; P/V=(BC!=0) | **PRESENT** | `block_transfer` `:1120-1131` (n=value+A, X=bit3, Y=bit1). Final-state correct |
| 7 | `CPI/CPD/CPIR/CPDR` flag quirks | §4 | S/Z/H per CP; n=A-(HL)-H; Y=bit1,X=bit3 of n; P/V=(BC!=0); N=1; C unchanged | **PRESENT** | `block_compare` `:1149-1170` (n=result-H, C preserved via `f & kC`) |
| 8 | `INI/IND/INIR/INDR` flag quirks | §4 | S/Z/Y/X per DEC B; N=bit7 data; C/H from data+((C±1)&255); P/V=parity((sum&7)^B) | **PRESENT** | `block_input` `:1188-1207` (c_adj=(C+delta)&0xFF; k>0xFF sets H|C; parity of (k&7)^b). Verify flag byte (M9-S3 left it unasserted, report `:253-258`) |
| 9 | `OUTI/OUTD/OTIR/OTDR` incl. **NMOS OUTI-affects-carry** | §4 | S/Z/Y/X per DEC B; N=bit7; C/H from L+value; P/V=parity((k&7)^B); **carry IS affected** | **PRESENT** | `block_output` `:1225-1243` uses L+data, sets `kH|kC` when k>0xFF (matches FS §4 "these DO affect carry"). Flag byte needs assertion (report `:253-258`) |
| 10 | 16-bit `ADD HL,rr` flags | §4 | H bit11, C bit15, X/Y from bits13/11; S/Z/PV preserved | **PRESENT** | `alu_add16` `:463-471` preserves `S|Z|PV`, H from 0x0FFF carry, C from >0xFFFF |
| 11 | 16-bit `ADC/SBC HL,rr` flags (SBC N=1) | §4 | full S/Z/H/V/C; N=1 for SBC, 0 for ADC | **PRESENT** | `alu_adc16` `:962-985` (N=0), `alu_sbc16` `:988-1013` (N=1) |
| 12 | Full-table `DAA` | §4 | Correct after add AND subtract; not naive BCD | **PRESENT** | `alu_daa` `:509-548`; algorithmically matches `CPUCore.cc:4220-4238` (adjust 6/0x60, H=(a^res)&0x10, C=oldC\|a>0x99). Prove via ZEXDOC |
| 13 | `SLL`/`SL1` (`CB 30-37`) | §4 | `r=(r<<1)\|1`; shift flags | **PRESENT** | `alu_shift_rotate` case 6 `:608-611` sets bit0 |
| 14 | `IXH/IXL/IYH/IYL` halves | §4 | DD/FD on H/L (non-memory) → index halves | **PRESENT** | `read/write_reg_indexed_half` `:1387-1408`, `indexed_inc_dec` `:1427-1435` |
| 15 | DD/FD prefix chaining (last wins) + NONI | §4 | run of DD/FD: only last counts, each 4T+R+1 | **PRESENT** | `execute_indexed` `:1446-1451` recurses (+4T each), R ticked via `fetch_opcode` |
| 16 | ED-hole opcodes = 2-byte NOP | §4 | undefined ED acts as 2 NOPs (8T), R+2 | **PRESENT** | `execute_ed_prefixed` default `:1350-1351` returns 8; both bytes fetched via `fetch_opcode` (R+2) |
| 17 | R low-7-bit increment, bit7 frozen | §7, §8 | +1 per M1; bit7 only via `LD R,A`; block=+2/iter; DDCB post-CB not M1 | **PRESENT** | `increment_refresh_register` `:192-201` (preserve `&0x80`, low7); `execute_indexed_cb` `:1603-1606` uses `read_imm8` (no R) for disp+subop |
| 18 | `LD R,A` sets full 8 bits | §7 | writes bit7 too | **PRESENT** | `execute_ed_prefixed` `:1315` `r = a()` |
| 19 | NMOS `OUT (C),0` = 0 | §8 | `ED 71` outputs 0x00 (NMOS) | **PRESENT** | `execute_ed_prefixed` `:1272` `(y==6)?0x00:...` |
| 20 | SCF/CCF undocumented X/Y **Q-latch** | §8 | X/Y from `((Q^F)\|A)` bits 3/5; Q=prev-F if prev modified flags else 0 | **DIVERGENT** | `alu_scf` `:562` / `alu_ccf` `:574` use `A & kXY` (pure move-from-A, no Q, no OR). openMSX uses `(F\|A)` (`CPUCore.cc:4266,4269,4201`); true Zilog needs Q. Current = neither |
| 21 | Q latch tracked across instructions | §8 | Q updated every instruction (new F, or 0) | **ABSENT** | No `q_` state in `z80a_state.h`; prerequisite for #20 |
| 22 | Interrupt modes IM0/IM1/IM2 | §5 | IM1→0x0038; IM2 vectored; IM0 bus opcode | **PRESENT** | `service_maskable_interrupt` `:1657-1683`, IM table `:1302-1306` |
| 23 | IM1 acceptance = 13T bare (+M1 at machine) | §5 | 13T datasheet; MSX adds M1 wait (~14T) | **PRESENT** | returns 13 `:1682`; ack M1 ticked `:1655`; machine adds wait (M11 QA §3, IM2 44→49 verified) |
| 24 | IM0 ack = 13T for bus RST (7+3+3) | §5 | 2 auto waits + RST | **PRESENT** | `:1665-1666` `2 + execute_unprefixed` (11+2=13) |
| 25 | IM2 ack = 19T | §5 | 7+3+3+3+3 | **PRESENT** | `:1677` returns 19 |
| 26 | NMI = 11T, vector 0x0066, IFF1→0, IFF2 kept | §5 | | **PRESENT** | `service_nmi` `:1637-1643` |
| 27 | DI clears IFF1+IFF2 | §5 | immediate | **PRESENT** | `:876-877` |
| 28 | EI one-instruction delay | §5 | interrupt not accepted until after instr following EI | **PRESENT** | `ei_delay_active` set `:882`, consumed `:44,49,58-60`. Verify EI;EI and EI;RET edges |
| 29 | `RETN` copies IFF2→IFF1 | §5 | | **PRESENT** | `:1298-1299` (`y!=1`) |
| 30 | `RETI` copies IFF2→IFF1 (same as RETN) | §5 | RETI and RETN functionally identical inside CPU | **DIVERGENT** | `:1297-1299` explicitly **skips** the copy for `y==1` (RETI). openMSX `retn()` **is also reti()** → `setIFF1(getIFF2())` (`CPUCore.cc:3911-3915`). FS §5: "all ED-prefixed RETxx copy IFF2→IFF1" |
| 31 | NMOS `LD A,I` / `LD A,R` P/V interrupt bug | §5 | if interrupt accepted during the instr, P/V reads 0 despite IFF2=1 | **ABSENT** | `ld_a_ir` `:1081-1095` copies IFF2→P/V unconditionally; no interrupt-edge suppression. Genuine NMOS silicon bug (FS §5) |
| 32 | Intrinsic I/O wait state (4T I/O cycle) | §6 | baked into datasheet totals; independent of M1 wait | **PRESENT** | IN/OUT return 11 (`:850,857`), IN(C)/OUT(C) return 12 (`:1268,1274`) — datasheet already includes the auto TW. Not double-counted with M1 |
| 33 | Per-step M1-cycle count (for machine M1 wait) | §3 | count opcode/prefix M1s + ack M1; datasheet T unchanged | **PRESENT** | `m1_cycle_count_` via `increment_refresh_register` `:196`; M11 QA §3 confirms machine applies wait, CPU T-states unchanged |
| 34 | HALT refresh (R increments during HALT) | §6 | HALT executes internal NOP M1s; R advances; each is an M1 (wait applies) | **DIVERGENT** | halted step `:50-51` returns 4 with **no** `fetch`/R-increment/M1 count. M11 oracle treated idle as 0 M1 (`docs/m11-qa-signoff.md:124`). Real Z80 refreshes during HALT. Flag for decision (see §5 / Risk R-6) |
| 35 | WZ update rules (LD A,(nn), JP/CALL, JR taken, (IX+d), block, IN/OUT(C), IRQ-accept) | §4 | per FS §4 bullet list | **ABSENT** | consequence of #3; no update sites exist. Ref: `CPUCore.cc` setMemPtr sites enumerated above |
| 36 | Block-repeat mid-iteration X/Y (interrupted LDIR) | §4 | intermediate iterations source X/Y from PC hi | **UNVERIFIED** | current re-executes each iter as a fresh step using `A+value` model; final state correct (#6), interrupted-mid flags not proven. Low priority; ZEXALL covers final state |

### 3.1 Headline findings
- **36 rubric items classified: 26 PRESENT, 3 DIVERGENT (#4 BIT-(HL) X/Y, #20 SCF/CCF-Q, #30 RETI IFF, #34 HALT-R = actually 4 DIVERGENT), 3 ABSENT (#3 WZ register, #21 Q latch, #31 LD A,I/R NMOS bug, #35 WZ rules = 4 ABSENT-cluster), 1 UNVERIFIED (#36).**
  Corrected tally: **PRESENT 26, DIVERGENT 4 (#4, #20, #30, #34), ABSENT 5 (#3, #21, #31, #35, and #21 is Q-latch prerequisite), UNVERIFIED 1 (#36).**
- The M9 core is genuinely **strong**: all opcode families, block-instruction flag quirks (incl. the NMOS OUTI-carry edge), SLL, index halves, prefix chaining, DAA, R-register rules, OUT(C),0=0, and the interrupt-mode architecture are **already implemented and read as correct**.
- The real work concentrates in **three coherent gap clusters**: (a) **WZ/MEMPTR** (#3/#35/#4), (b) **SCF/CCF Q-latch** (#20/#21), (c) **NMOS interrupt edges + IFF** (#30/#31), plus one **timing-fidelity decision** (#34 HALT refresh). Everything else is **prove-it** work (tests + ZEX + A/B).

---

## 4. Interface / Timing Note (M1 wait ownership — do not double-count)

- The MSX **M1 wait is owned by the machine**: `hbf1xv_machine.cpp:133-134` applies `datasheet_tstates + s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step())` (M11 QA §3). The CPU returns **canonical Z80 datasheet T-states** and exposes `m1_cycles_last_step()`; it must **not** add the +1/opcode-fetch itself.
- **Invariant M12 must preserve:** every parity fix keeps (a) the datasheet T-state return values unchanged, and (b) the per-step M1 count = (opcode + prefix fetches) + (interrupt-ack M1). The single choke-point is `increment_refresh_register()` (`z80a_cpu.cpp:192-201`), called once per `fetch_opcode()` and once on ack — do not add or remove call sites for a flag fix.
- **The 8 M11 timing oracles must stay green** (M11 QA §4 re-derived: `hbf1xv_cpu_step` 22, `hbf1xv_ldir_program` 102, `hbf1xv_interrupt_ack` IM2 49 / IM0 38, plus the four others in the M11 regression set). None of the WZ/Q/RETI/LD-A,I fixes change T-state returns, so these oracles are unaffected **except** the HALT-refresh decision (#34): if M12 changes HALT to refresh R (and thus incur an M1 wait per idle 4T), the `hbf1xv_cpu_step` oracle (which counts the halted-idle step as 0 M1 → 22) **would change**. Therefore #34 is gated behind an explicit decision (Risk R-6); the default recommendation is to **document the current behavior and defer the change** unless ZEXALL/openMSX evidence demands it, to avoid a self-inflicted regression against a QA-signed oracle.
- The intrinsic **I/O wait** (item #32) is already inside the datasheet totals and is orthogonal to the M1 wait — no interaction, no double-count.

---

## 5. Slice Plan (regressions surface early: build the net, then fix)

Ordering rationale: stand up the **broad parity test net and the ZEX self-checker FIRST** (S1-S2) so any latent defect or later-introduced regression fails immediately; then land the three targeted fix clusters (S3-S5); then the **system-integration + A/B + closure** (S6).

### M12-S1 — Parity proof net for PRESENT items (test-only; resist refactor)
- **Goal:** Lock the 26 PRESENT items with deterministic vectors so subsequent fixes cannot silently regress them. Prove, do not rebuild.
- **Files touched (production):** none expected (pure tests). If a vector exposes a real defect, that defect is re-classified and moved into S3-S5.
- **New tests:**
  - `tests/unit/devices/z80a_parity_undocumented_unit_test.cpp` — SLL (`CB 30-37`) all regs incl. (HL); IXH/IXL/IYH/IYL INC/DEC/LD/ALU; DD/FD chaining (DD DD, DD FD, DD ED) and NONI; ED-hole 2-NOP (8T, R+2); OUT (C),0 = 0; `LD R,A` bit7; R low-7 wrap `0x7F→0x00` with bit7 frozen; DDCB post-CB no-R-tick.
  - `tests/unit/devices/z80a_block_flags_unit_test.cpp` — full flag-byte assertions for LDI/LDD/CPI/CPD/INI/IND/**OUTI/OUTD** incl. the **NMOS OUTI-affects-carry** edge (k>0xFF) and the parity `((k&7)^B)` rule (closes the M9-S3 unasserted-flag gap, report `:253-258`).
  - `tests/unit/devices/z80a_16bit_daa_unit_test.cpp` — ADD/ADC/SBC HL,rr full flags (SBC N=1, X/Y from hi byte), full-table DAA sweep after add and subtract.
- **Determinism:** fixed opcode buffers + a fake `CpuBusClient` backing store; exact flag byte and T-state `==` assertions.
- **System-integration contribution:** none (unit only); establishes the regression floor S6 will re-run over the real bus.
- **Evidence gates:** build + ctest green (see §7).

### M12-S2 — ZEXDOC/ZEXALL self-checking harness (gold-standard exerciser)
- **Goal:** Bring the de-facto Z80 correctness suite online **early** to enumerate every remaining undocumented gap at once (WZ, SCF/CCF, block, DAA) via emulator-independent CRCs.
- **Files touched (production):** a headless CP/M-style test harness — preferred location `tests/system/z80_zex_system_test.cpp` plus a tiny BDOS shim (intercept `CALL 5`: function 2 = conout, function 9 = print `$`-terminated string; program loaded at `0x0100`, `WBOOT` at `0x0000`) built against the existing CPU + a flat 64K test bus (NOT the slot bus — ZEX needs flat RAM). This shim lives under `tests/` (harness), not `src/`. Assets: `tests/parity/zexdoc.com`, `tests/parity/zexall.com` (A-1).
- **Deterministic tests:**
  - `Z80_Zexdoc_System` — runs ZEXDOC to completion, asserts **all** subtests print `OK` (documented flags). **Hard gate.**
  - `Z80_Zexall_System` — runs ZEXALL; asserts pass. Initially expected to **fail** on the WZ/SCF-CCF subtests until S3-S4 land — so this test is added **disabled/expected-fail-annotated** in S2 and **flipped to required-pass in S6**. (Never commit a green claim over a red run.)
- **System-integration contribution:** exercises tens of thousands of instruction/flag combinations end-to-end through the CPU step loop — the strongest single parity oracle.
- **Evidence gates:** build + ctest; checksum the ZEX binaries into `docs/asset-checksums.txt`.
- **Feasibility note & fallback:** if A-1 cannot be satisfied (no legally-sourced ZEX binary), this slice degrades to a documented "assessed, deferred" outcome and the openMSX A/B trace-diff (S6) plus the S1/S3-S5 unit nets become the parity evidence of record. No fabrication.

### M12-S3 — WZ/MEMPTR register + BIT n,(HL) X/Y (closes #3/#4/#35)
- **Goal:** Add the internal WZ register and update it at every rule site; source `BIT n,(HL)` X/Y from WZ hi byte.
- **Files touched:** `src/devices/cpu/z80a_state.h`/`.cpp` (add `std::uint16_t wz` to `Z80aRegisters`, reset to 0 deterministically; add to trace snapshot only if desired — WZ is not A/B-comparable, A-3); `src/devices/cpu/z80a_cpu.cpp` (write WZ at: `LD A,(nn)`/`LD (nn),A` → nn+1; `LD A,(BC/DE)` → rp+1; `JP nn`/`CALL nn` (cond incl. not-taken) → nn; `JR`/`DJNZ` taken → dest; every `(IX/IY+d)` → index+d; block ops per FS §4; `IN/OUT (C)` → BC±1; IRQ accept → handler addr). `execute_cb_prefixed` `:944` changed to pass `wz>>8` for the `(HL)` case; `execute_indexed_cb` re-routed through WZ for consistency (#5, behavior-preserving).
- **Deterministic tests:** `tests/unit/devices/z80a_wz_memptr_unit_test.cpp` — `BIT n,(HL)` X/Y equal WZ bits 11/13 after a WZ-setting predecessor (e.g. `LD A,(nn)` then `BIT`); one vector per WZ update rule verifying the observable X/Y consequence; `BIT n,(IX+d)` unchanged.
- **System-integration contribution:** feeds the ZEXALL `bit` subtest (S2/S6) to green.
- **Evidence gates:** build + ctest; ZEXALL `bit` subtest progresses.

### M12-S4 — SCF/CCF genuine-Zilog Q-latch (closes #20/#21)
- **Goal:** Implement the Q latch and the `((Q^F)|A)` bits-5/3 rule for SCF/CCF on the genuine NMOS part.
- **Files touched:** `src/devices/cpu/z80a_state.h`/`.cpp` (add `std::uint8_t q_` and accessor; reset deterministically); `src/devices/cpu/z80a_cpu.cpp` — maintain Q at instruction retirement (Q = new F after a flag-modifying op; Q = 0 after a non-flag op incl. `EX AF,AF'`, `POP AF`, and interrupt accept), and rewrite `alu_scf`/`alu_ccf` `:559-576` to `X/Y = ((q ^ F) | A) & kXY`. Keep the CCF H = old-carry logic already present.
- **Deterministic tests:** `tests/unit/devices/z80a_scf_ccf_q_unit_test.cpp` — Patrik-Rak vectors: SCF/CCF after a flag-modifying op (move-from-A) vs after a non-flag op (OR-from-A), including `EX AF,AF'`/`POP AF`/post-interrupt Q=0 cases.
- **A/B caveat (A-4):** these vectors are grounded in the **fact-sheet Q rule + ZEXALL**, NOT the openMSX trace-diff (openMSX uses the (F|A) OR-form and will diverge in the move-from-A case). Document this explicitly in the S6 diff artifact.
- **Evidence gates:** build + ctest; ZEXALL `scf/ccf` subtests green.

### M12-S5 — NMOS interrupt edges + IFF semantics (closes #30/#31, verifies #28/#34)
- **Goal:** Fix RETI IFF copy; implement the NMOS `LD A,I/R` P/V interrupt bug; verify EI-delay edges; make the HALT-refresh (#34) decision.
- **Files touched:** `src/devices/cpu/z80a_cpu.cpp` — `execute_ed_prefixed` `:1295-1300`: RETI (`y==1`) must **also** `set_iff1(iff2())` (match openMSX `retn()`/`reti()` `CPUCore.cc:3911-3915`); `ld_a_ir` `:1081-1095`: if a maskable interrupt is pending and will be accepted at this instruction boundary (IFF1 set, not EI-delayed), clear P/V (models the silicon race under the instruction-atomic step — document the approximation). Optionally a small `z80a_state` flag to expose "irq will be accepted next".
- **Deterministic tests:** `tests/unit/devices/z80a_nmos_interrupt_unit_test.cpp` — RETI restores IFF1 from IFF2 (currently would fail → proves the fix); `LD A,I` with interrupt pending clears P/V while IFF2=1; EI;RET and EI;EI one-instruction-delay edges; DI/EI/RETN IFF matrix; IM/NMI acceptance T-states (13/19/11 bare) unchanged.
- **HALT decision (#34):** default = **document current behavior + defer** (avoid regressing the QA-signed `hbf1xv_cpu_step` oracle). Escalate to `decisions.md` **only if** ZEXALL/openMSX evidence shows a real defect. Record the decision either way.
- **Evidence gates:** build + ctest; interrupt suites green.

### M12-S6 — System integration over M11 bus + openMSX A/B + closure
- **Goal:** Prove parity **end-to-end through the real M11 `SystemBus`**, capture a genuine openMSX A/B trace-diff, flip ZEXALL to required-pass, and satisfy the DEC-0003 close bar.
- **Files touched:** `tests/integration/machine/hbf1xv_cpu_parity_integration_test.cpp` (new, §6); `tests/parity/z80_parity_checkpoint.bin` refreshed to exercise parity-critical opcodes; `tools/openmsx-cpu-parity.ps1` (extends `tools/openmsx-trace-parity.ps1`, output `docs/m12-parity-trace-diff.md`); flip `Z80_Zexall_System` to required-pass.
- **System-integration contribution:** the definitional M12 system test (see §6).
- **A/B:** see §7.4. No parity claim without a genuine captured diff.
- **Evidence gates:** all four gates + the A/B diff + ZEXDOC/ZEXALL green.

---

## 6. System-Integration Test Definition (CPU over the real M11 SystemBus)

- **Name:** `Machine_Hbf1xvCpuParity_Integration` (`tests/integration/machine/hbf1xv_cpu_parity_integration_test.cpp`).
- **Under test:** `Hbf1xvMachine` composing the real `SystemBus` (slot decode + I/O bus + S1985), the CPU, and the machine-applied M1 wait — **not** a fake bus. Program loaded into slot-3-0 DRAM at a slot-safe base (as M11 integration tests do).
- **Coverage (parity-critical, end-to-end):**
  1. **WZ path:** `LD A,(nn)` then `BIT n,(HL)` — assert X/Y reflect WZ hi byte through the real memory read path.
  2. **SCF/CCF-Q:** a flag-modifying op → SCF (move-from-A) vs a non-flag op → SCF (OR-from-A); assert F X/Y.
  3. **Block + NMOS carry:** an `OUTI`/`OTIR` to a real I/O port; assert B/HL/flags incl. carry, and that the I/O bus dispatched on `port&0xFF`.
  4. **Interrupt path:** request a maskable IM1 interrupt through the machine; assert acceptance vector 0x0038, IFF1/IFF2 cleared, push order, **and the cycle total = datasheet + M1 wait** (ties to M11's IM oracle; verifies no double-count).
  5. **RETI IFF restore** and **EI one-instruction delay** across a real ISR (`EI`/`RETI`).
  6. **Timing oracle:** an exact `elapsed_cycles() ==` assertion for the whole program (datasheet + machine M1 wait), consistent with the M11 §4 methodology; bounded guard loop, no wall-clock.
- **Determinism:** fixed program bytes, fixed injected interrupt timing, exact register/memory/flag/cycle assertions.

---

## 7. A/B Acceptance Test, Evidence Gates, and Closure Rule

### 7.1 openMSX trace-diff (extend the existing harness)
- **Tool:** `tools/openmsx-cpu-parity.ps1`, extending `tools/openmsx-trace-parity.ps1` (per-instruction PC/opcode/AF/BC/DE/HL/AF'/BC'/DE'/HL'/IX/IY/SP/I/R/IM/IFF capture, single-stepped over the same RAM-only program in openMSX 19.1). Output: `docs/m12-parity-trace-diff.md` via `tools/trace-diff.py`.
- **Program:** refreshed `tests/parity/z80_parity_checkpoint.bin` exercising: undocumented-X/Y ALU, SLL, IXH/IXL, block ops (incl. OUTI carry), 16-bit ADC/SBC, DAA sweep, prefix chaining, `BIT n,(HL)`/`(IX+d)`, IM1/RETI/EI sequences.
- **Documented benign boundaries (must be stated in the artifact, not silently excluded):**
  - **WZ (A-3):** not exposed by openMSX `reg`; excluded from field-diff, proven via ZEXALL `bit` + S3/S6 unit/integration.
  - **SCF/CCF X/Y (A-4):** openMSX uses (F|A) OR-form; a correct genuine-NMOS Q-latch diverges in the move-from-A case. Exclude SCF/CCF X/Y from the openMSX field-diff; gate on the fact-sheet Q rule + ZEXALL.
  - **Cycle/T-state parity ungated A/B** (DP-4 precedent, M11 QA §5): the emulator trace reports canonical Z80 T-states; openMSX inserts M1 waits. Timing parity is proven via the machine oracles (§6) and M11 §4, not the trace-diff.
- **Hard rule:** no parity claim without a genuine captured diff. If openMSX cannot be driven, the artifact records BLOCKED — never a fabricated PASS.

### 7.2 ZEXDOC/ZEXALL feasibility & recommendation (in-or-out)
- **Recommendation: IN for M12**, as the primary opcode/flag parity oracle, because (a) it is emulator-independent (fixed CRCs from real Zilog silicon — the exact genuine-NMOS target), (b) it directly exercises WZ, SCF/CCF-Q, block-flag, and DAA behavior that the openMSX trace-diff cannot fully gate, and (c) the harness is modest: a CP/M BDOS shim (`CALL 5` → conout/print-string) over the existing CPU with a flat 64K bus. **ZEXDOC = hard gate (documented flags, must pass 100%).** **ZEXALL = hard gate at S6** (undocumented; passes with a correct Q-latch + WZ). Contingency: if the binary cannot be legally sourced (A-1), ZEX degrades to "assessed/deferred" and the openMSX A/B + unit nets carry the evidence — recorded honestly, no fabrication.

### 7.3 Evidence-gate mapping (every slice)
For **each** slice S1-S6, run and capture (never fabricate):
- `tools/validate-assets.ps1` — BIOS present + ≥1 ROM.
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — reproducible checksums (incl. ZEX binaries once present).
- `cmake --build build --config Debug` — build succeeds.
- `ctest --test-dir build -C Debug --output-on-failure` — full deterministic suite passes (M1-M12), run twice for determinism.
- Behavior-affecting slices (S3/S4/S5/S6): the openMSX A/B trace-diff (§7.1) and the ZEX gates (§7.2) as applicable.

### 7.4 Closure rule (DEC-0003 explicit)
- M12 auto-closes **without a further human release decision** iff, at S6: **100% pass of ALL unit AND system-integration tests (incl. the new CPU-over-M11-bus integration test and ZEXDOC; ZEXALL green), with ZERO regression across the M1-M12 suites, and QA sign-off = Pass.** Any failing/skipped test, any regression, or QA < Pass → **no auto-close; escalate to the human** (DEC-0003 CLOSE AUTHORIZATION). Grant applies to M12 only.

---

## 8. Risks

| ID | Risk | Severity | Verification / mitigation action |
|----|------|----------|----------------------------------|
| R-1 | WZ update rules are numerous; a missed site fails ZEXALL `bit` subtly | Med | Enumerate every `setMemPtr` site in `CPUCore.cc` (grep done: 364/789/2711/3247/3303/3320/3339...) and map 1:1 to our update sites in S3; ZEXALL `bit` CRC is the backstop |
| R-2 | SCF/CCF Q-latch (correct Zilog) **diverges from openMSX** A/B (openMSX uses (F\|A)) | Med | A-4: exclude SCF/CCF X/Y from openMSX field-diff; gate on fact-sheet Q rule + ZEXALL; document in `docs/m12-parity-trace-diff.md` |
| R-3 | ZEX binaries not legally sourceable in-environment (A-1) | Med | Verify placement + checksum; if absent, degrade ZEX to assessed/deferred and lean on openMSX A/B + unit nets — recorded, not fabricated |
| R-4 | LD A,I/R NMOS bug is a mid-instruction silicon race; instruction-atomic step can only approximate | Low | Model at the instruction boundary (irq-will-be-accepted → clear P/V); document the approximation; ZEXALL does not test the IRQ race so this is unit-proven only |
| R-5 | A WZ/Q/RETI fix accidentally changes a datasheet T-state and breaks an M11 oracle | Med | §4 invariant: fixes touch flags/IFF only, never `return` T-states or `increment_refresh_register` call sites; re-run the 8 M11 oracles each slice |
| R-6 | HALT-refresh change (#34) would alter the QA-signed `hbf1xv_cpu_step` oracle (22) | Med | Default: document + defer; change only via a `decisions.md` entry if ZEXALL/openMSX proves a real defect; never silently rewrite a signed oracle |
| R-7 | openMSX 19.1 vs 21.0 grounding mismatch (A-2) | Low | Record `openmsx --version` in the diff; documented behavior stable; same runtime used in M10/M11 gates |
| R-8 | Gratuitous refactor creep under "hardening" (DEC-0003 explicitly forbids) | Med | Every production edit must cite a §3 gap ID; S1/S2 are test-only; reviewer rejects opcode-dispatch restructuring |

### Assumptions recap (with verification) — see §1.3 (A-1..A-4). Each is actioned in the slice that depends on it.

---

## 9. Developer Handoff

- **Start at M12-S1** and proceed strictly S1→S6 (net first, fixes next, integrate+prove last). Do not begin a fix slice (S3-S5) before S1/S2 are green — the net must exist to catch regressions.
- **Production edits are confined to `src/devices/cpu/`** (`z80a_state.h`/`.cpp`, `z80a_cpu.cpp`, `z80a_cpu.h`) plus tests/tools. The only expected `src/machine/` change is the new integration **test** and deterministic default-init of the new WZ/Q fields if the machine snapshots CPU state.
- **Exact gaps to fix (nothing more):**
  - #3/#35/#4 — add `wz` to `Z80aRegisters`; write it at the enumerated rule sites; `execute_cb_prefixed:944` sources `BIT n,(HL)` X/Y from `wz>>8`.
  - #20/#21 — add `q_` latch; rewrite `alu_scf`/`alu_ccf` (`z80a_cpu.cpp:559-576`) to `((q^F)|A)` bits 5/3.
  - #30 — `execute_ed_prefixed:1295-1300`: make RETI also copy IFF2→IFF1.
  - #31 — `ld_a_ir:1081-1095`: clear P/V when an interrupt will be accepted at this boundary.
  - #34 — HALT refresh: **decision-gated**, default defer (Risk R-6).
- **Prove-don't-rebuild:** items #1/#2/#6-#19/#22-#33 are PRESENT — add vectors (S1) and let ZEX (S2) confirm; do not touch their code unless a vector fails.
- **Timing invariant (§4):** never change a datasheet T-state return or an `increment_refresh_register()` call site for a flag fix; keep the 8 M11 oracles green.
- **Evidence:** run and capture all four gates every slice (twice for determinism); capture the openMSX A/B diff and ZEX results at S6; write results into the implementation report and `docs/m12-parity-trace-diff.md`. **No fabricated parity/pass — BLOCKED is an acceptable honest outcome, a false green is not.**
- **Durable artifacts:** this package (`docs/m12-planner-package.md`), the forthcoming `docs/m12-implementation-report.md`, `docs/m12-parity-trace-diff.md`, and the QA sign-off `docs/m12-qa-signoff.md`.
- **Closure:** DEC-0003 auto-close bar — 100% unit + system-integration pass, zero M1-M12 regression, QA Pass — else escalate.
