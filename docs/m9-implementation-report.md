# M9 Implementation Report (REQ-M9-003)

- Milestone ID: M9
- Source Request: REQ-M9-003
- Status: Partial

## Summary

Implemented the first substantial Z80A slice for HB-F1XV:

- New Z80A state model and CPU execution core.
- Deterministic instruction-step pipeline with T-state accumulation.
- Interrupt baseline covering IM mode state transitions, NMI, maskable interrupts, RETN/RETI, and EI delay handling.
- Machine integration with CPU stepping and memory load helpers.
- New deterministic unit/integration/system tests.

This slice is functional and test-green, but not opcode-complete.

## Changed Files

- src/devices/cpu/z80a_state.h
- src/devices/cpu/z80a_state.cpp
- src/devices/cpu/z80a_cpu.h
- src/devices/cpu/z80a_cpu.cpp
- src/machine/hbf1xv_machine.h
- src/machine/hbf1xv_machine.cpp
- CMakeLists.txt
- tests/CMakeLists.txt
- tests/unit/devices/z80a_state_unit_test.cpp
- tests/unit/devices/z80a_unprefixed_unit_test.cpp
- tests/unit/devices/z80a_interrupts_unit_test.cpp
- tests/integration/machine/hbf1xv_cpu_step_integration_test.cpp
- tests/system/cpu_bootstrap_im1_system_test.cpp

## Evidence Gates

- tools/validate-assets.ps1: pass
- tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt: pass
- cmake --build build --config Debug: pass
- ctest --test-dir build -C Debug --output-on-failure: pass (11/11)
- tools/openmsx-ab-smoke.ps1: executed, report refreshed in docs/openmsx-ab-smoke.md

## Functional Coverage in This Slice

- Unprefixed subset (deterministic): NOP, LD B,n, LD A,n, INC A, DEC B, XOR A, JP nn, HALT, DI, EI, ED prefix dispatch.
- ED subset: RETN, RETI, IM 0, IM 1, IM 2.
- Interrupt service baseline: NMI path and maskable interrupt path with IM1 and IM2 handling.

## Outstanding Gaps

- Full unprefixed table not complete.
- CB family not implemented.
- ED family only partially implemented.
- DD/FD and DDCB/FDCB families not implemented.
- IM0/IM2 device-vector acknowledge behavior is simplified and not yet full-architecture fidelity.

## Next Steps

1. Complete unprefixed and CB families with full flag/timing tests.
2. Complete remaining ED family operations and block instruction behavior.
3. Implement DD/FD and DDCB/FDCB indexed families with displacement/timing coverage.
4. Harden interrupt acknowledge fidelity for IM0/IM2 and extend parity evidence.

---

# M9-S2 Slice Addendum (REQ-M9-005) — 2026-07-05

- Slice ID: M9-S2
- Source Request: REQ-M9-005
- Status: Completed (unprefixed table + full CB family; gates green)

## What Was Implemented

Replaced the previous sparse unprefixed `switch` with a complete, decode-driven
Z80A unprefixed instruction table and added the full CB-prefixed family. Scope is
strictly M9-S2: no ED full set, no DD/FD, no DDCB/FDCB (deferred to S3/S4).

- Undocumented flags modeled: added `kFlagY` (bit 5) and `kFlagX` (bit 3) to
  `Z80aState`; ALU/rotate/shift results now propagate S,Z,Y,H,X,P/V,N,C.
- Unprefixed coverage (opcode-family complete):
  - 8-bit ALU (ADD/ADC/SUB/SBC/AND/XOR/OR/CP) against `r[]` and immediate, with
    correct half-carry, signed-overflow, and CP's operand-sourced X/Y quirk.
  - `INC`/`DEC` on all `r[]` including `(HL)`; 16-bit `INC`/`DEC`/`ADD HL,rp`.
  - Full `LD r,r'` matrix (with `HALT` at 0x76), `LD r,n`, `LD (HL),n`,
    immediate 16-bit loads, `(BC)/(DE)/(nn)` indirect loads/stores, `LD SP,HL`.
  - Accumulator ops `RLCA/RRCA/RLA/RRA`, `DAA`, `CPL`, `SCF`, `CCF`.
  - Control flow: `JR`/`JR cc`/`DJNZ` (relative), `JP`/`JP cc`, `JP (HL)`,
    `CALL`/`CALL cc`, `RET`/`RET cc`, `RST`, `PUSH`/`POP` (rp2), `RETI/RETN` via ED.
  - Exchange group: `EX DE,HL`, `EX AF,AF'`, `EXX`, `EX (SP),HL`.
  - Deterministic taken/not-taken T-state accounting for every conditional op.
- CB-prefixed family (complete):
  - Rotate/shift `RLC/RRC/RL/RR/SLA/SRA/SLL/SRL` on all `r[]` and `(HL)`.
  - `BIT`/`RES`/`SET` for all 8 bit positions across all `r[]` and `(HL)`.
  - Timings: register ops 8T, `(HL)` rotate/shift/RES/SET 15T, `BIT n,(HL)` 12T.

## Changed / Added Files

- src/devices/cpu/z80a_state.h (added X/Y flag constants)
- src/devices/cpu/z80a_cpu.h (decode/ALU/CB helper declarations)
- src/devices/cpu/z80a_cpu.cpp (full unprefixed decode + CB dispatch + ALU helpers)
- tests/unit/devices/z80a_unprefixed_ops_unit_test.cpp (new)
- tests/unit/devices/z80a_cb_prefixed_unit_test.cpp (new)
- tests/integration/machine/hbf1xv_cb_program_integration_test.cpp (new)
- tests/CMakeLists.txt (registered 3 new test targets)

## Tests Added

- Suite `Devices_Z80AUnprefixedOps_Unit` (z80a_unprefixed_ops_unit_test): 45 vectors
  covering ALU flag edges, INC/DEC (HL), ADD HL,rp carries, DAA/CPL/SCF/CCF,
  exchange group, JR/DJNZ/CALL/RET taken vs not-taken timing, stack order,
  16-bit loads, LD r,r' matrix, accumulator rotates, and IN/OUT stubs.
- Suite `Devices_Z80ACbPrefixed_Unit` (z80a_cb_prefixed_unit_test): 18 vectors
  covering every rotate/shift op, (HL) variants, and BIT/RES/SET incl. the
  operand-sourced undocumented X/Y behavior and carry preservation on BIT.
- Suite `Machine_Hbf1xvCbProgram_Integration` (hbf1xv_cb_program_integration_test):
  end-to-end DJNZ loop mixing RLC A + LD (nn),A + HALT with a deterministic
  89-T-state cycle oracle and final register/memory assertions.

## Evidence Gates (actual output, 2026-07-05 cycle)

- tools/validate-assets.ps1: `Asset validation result: True` (7 BIOS files, 1 ROM), exit 0.
- tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt: report written, exit 0.
- cmake --build build --config Debug: BUILD_EXIT=0 (all targets linked).
- ctest --test-dir build -C Debug --output-on-failure: `100% tests passed, 0 tests
  failed out of 14` (CTEST_EXIT=0). Prior baseline was 11; three new suites added.
- tools/openmsx-ab-smoke.ps1: executed successfully (exit 0); docs/openmsx-ab-smoke.md
  refreshed. openMSX 19.1 confirmed available on WSL at /usr/bin/openmsx.

## Parity Note (Assumption / Verification Action)

Assumption: the openMSX A/B smoke script only records binary availability/version and
A/B input metadata; it does not emit an instruction-level opcode trace comparison for
M9-S2. It therefore confirms the reference toolchain is present but does NOT by itself
prove opcode-level parity. Verification action: introduce a deterministic CPU trace
harness (per-instruction register/flag/T-state dump) and diff it against an openMSX
debugger trace for the same program in a later parity-focused slice (M9-S5).

Assumption: undocumented X/Y for `BIT n,(HL)` is modeled from the tested value (a common
simplification); true hardware sources those bits from the internal WZ/MEMPTR high byte.
Verification action: confirm against zexall/zexdoc-style vectors during S5 hardening.

## Residual Gaps (open for M9 closure)

- ED full set (block ops LDIR/CPIR/etc., ED-arithmetic, IN/OUT (C), LD A,I/R, etc.):
  only the RETN/RETI/IM subset from S1 exists. (S3)
- DD/FD indexed prefixes (IX/IY): currently decoded as deterministic 4T no-ops. (S4)
- DDCB/FDCB indexed bit/rotate operations. (S4)
- IM0/IM2 acknowledge/vector-source fidelity remains simplified. (S5)
- Port I/O backing: `IN A,(n)`/`OUT (n),A` timing + PC are correct but data is a
  deterministic open-bus stub (no I/O device bus wired yet). (later milestone)
- Milestone-specific opcode-trace parity evidence vs openMSX not yet produced. (S5)

---

# M9-S3 Slice Addendum (REQ-M9-007) — 2026-07-06

- Slice ID: M9-S3
- Source Request: REQ-M9-007
- Status: Completed (full ED-prefixed family; gates green). Resolves QA blocker R1.

## What Was Implemented

Replaced the S1 stub `execute_ed_prefixed` (which handled only RETN/RETI/IM and
no-op'd everything else) with a complete x/y/z/p/q decode-driven ED table. Scope
is strictly M9-S3: no DD/FD, no DDCB/FDCB, and no change to IM0/IM2 vector
fidelity (those remain S4/S5).

- Block transfer/search (`x==2`, `z<=3`, `y>=4`):
  - `LDI` / `LDD` / `LDIR` / `LDDR` — (DE)<-(HL), HL/DE ±1, BC-1; H=N=0,
    P/V=(BC!=0); undocumented X (bit 3) / Y (bit 1) sourced from `(value + A)`.
  - `CPI` / `CPD` / `CPIR` / `CPDR` — compare A with (HL), HL±1, BC-1; N=1,
    S/Z/H from `A-(HL)`, P/V=(BC!=0), C preserved; undocumented X/Y from
    `result - H`.
  - Repeat-vs-terminate timing: repeating iteration = 21T with PC rewound by 2
    (re-executes as a fresh `step()`); final/terminating iteration = 16T with PC
    advanced. `CPIR`/`CPDR` terminate early on match (result==0) even with BC
    remaining.
- 16-bit arithmetic (`x==1`, `z==2`): `SBC HL,rp` (q=0) and `ADC HL,rp` (q=1),
  rp∈{BC,DE,HL,SP}. Full S,Z,H(bit-11),P/V(signed overflow),N,C plus X/Y from
  result high byte. 15T.
- `NEG` (`x==1`, `z==4`, all `y` aliases including the redundant ED opcodes).
  A=0-A; N=1, C=(A!=0), P/V=(A==0x80), H=((A&0x0F)!=0). 8T.
- I/O (`x==1`): `IN r,(C)` (`z==0`) sets S,Z,P,X,Y from data with H=N=0, C
  preserved; `y==6` is `IN (C)`/`IN F,(C)` — flags only, no register write.
  `OUT (C),r` (`z==1`), `y==6` is `OUT (C),0`. Port data remains a deterministic
  open-bus stub (0xFF read, writes discarded) — no I/O device bus wired yet. 12T.
- Interrupt/refresh registers (`x==1`, `z==7`): `LD I,A`, `LD R,A` (no flags, 9T);
  `LD A,I`, `LD A,R` set S,Z,X,Y with P/V=IFF2, H=N=0, C preserved (9T).
- `RRD` / `RLD` (`x==1`, `z==7`, `y==4/5`): nibble rotate through A and (HL);
  S,Z,P,X,Y from new A, H=N=0, C preserved. 18T.
- 16-bit load ED forms (`x==1`, `z==3`): `LD (nn),rp` (q=0) and `LD rp,(nn)`
  (q=1) for rp∈{BC,DE,HL,SP}, little-endian. 20T.
- Block I/O (`x==2`, `z==2/3`): `INI/IND/INIR/INDR` and `OUTI/OUTD/OTIR/OTDR`
  implemented with correct B decrement, (HL) memory effect, HL±1, PC, and
  repeat-vs-terminate timing (21T repeat / 16T final). Port data is the same
  open-bus stub; the undocumented flag formula is the standard documented model.
  (Not in the strict S3 enumeration but implemented to complete the ED table and
  remove the silent-mis-decode hazard; flag precision is asserted only for the
  deterministic core — see limitations.)
- Preserved RETN (all y except y=1) / RETI (y=1) and IM 0/1/2, now table-driven
  (IM decode via `im[y]` table). RETN still copies IFF2->IFF1.

## Changed / Added Files

- src/devices/cpu/z80a_cpu.h (ED helper declarations)
- src/devices/cpu/z80a_cpu.cpp (full ED decode + adc16/sbc16/neg/rrd/rld,
  ld_a_ir, io_in_flags, block_transfer/compare/input/output helpers)
- tests/unit/devices/z80a_ed_prefixed_unit_test.cpp (new)
- tests/integration/machine/hbf1xv_ldir_program_integration_test.cpp (new)
- tests/CMakeLists.txt (registered 2 new test targets)

## Tests Added

- Suite `Devices_Z80AEdPrefixed_Unit` (z80a_ed_prefixed_unit_test): 35 vectors
  covering LDI/LDD/LDIR (mid-loop 21T repeat + rewind, final 16T), CPI/CPD/CPIR
  (early-terminate-on-match 16T, no-match repeat 21T), ADC/SBC HL,rp flag edges,
  NEG (0x01/0x00/0x80 + redundant ED4C alias), IN B,(C)/IN (C) flags-only/OUT
  (C),B, LD I,A/LD R,A/LD A,I (P/V=IFF2 both states), RRD/RLD, LD (nn),rp / LD
  rp,(nn) for BC/DE/SP, preserved RETN(IFF restore)/RETI/IM2, INI/INIR/OUTI
  deterministic core, and undefined-ED 8T NOP. Full flag bytes asserted for all
  documented ops.
- Suite `Machine_Hbf1xvLdirProgram_Integration` (hbf1xv_ldir_program_integration_test):
  end-to-end LD HL/DE/BC + LDIR 3-byte block copy + HALT through the machine
  boundary, asserting destination memory, final HL/DE/BC, and a hardcoded 92-T
  cycle oracle (10+10+10 + [21+21+16] + 4) with a bounded guard loop (no
  wall-clock).

## Evidence Gates (actual output, 2026-07-06 cycle)

- tools/validate-assets.ps1: `Asset validation result: True` (7 BIOS files:
  f1xvbios/f1xvdisk/f1xvext/f1xvfirm/f1xvkdr/f1xvkfn/f1xvmus.rom, 1 ROM:
  aleste.rom), exit 0.
- tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt: report written,
  exit 0.
- cmake --build build --config Debug: BUILD_EXIT=0 (all targets linked,
  including the two new test executables).
- ctest --test-dir build -C Debug --output-on-failure: `100% tests passed, 0
  tests failed out of 16` (CTEST_EXIT=0). Prior baseline was 14; two new suites
  added (devices_z80a_ed_prefixed_unit_test, machine_hbf1xv_ldir_program_integration_test).
- tools/openmsx-ab-smoke.ps1: executed (exit 0); docs/openmsx-ab-smoke.md
  refreshed. openMSX 19.1 confirmed available on WSL at /usr/bin/openmsx.

## Parity Note (Assumption / Verification Action)

Assumption: the openMSX A/B smoke report (`docs/openmsx-ab-smoke.md`) remains
availability/version + A/B input metadata only; it does NOT emit an
instruction-level opcode trace comparison for the ED family. It confirms the
reference toolchain is present but does not by itself prove ED opcode-level
parity. Verification action: build a deterministic CPU trace harness (per-
instruction register/flag/T-state dump) and diff it against an openMSX debugger
trace for ED-heavy programs in the parity-focused slice (M9-S5).

Assumption: block-I/O (INI/IND/OUTI/OUTD and repeats) undocumented flag bits use
the standard "Undocumented Z80" formula, and port data is an open-bus 0xFF stub;
these ops are outside the strict S3 enumeration and their exact flag byte is NOT
asserted in unit tests (only B/memory/HL/PC/timing are). Verification action:
confirm block-I/O flags against zexall-style vectors and a wired I/O bus in a
later slice.

Assumption: LDIR/CPIR undocumented X/Y during repeat use the per-iteration
`(value+A)` / `(result-H)` model rather than the PC-high-byte quirk of true
hardware on the non-final iterations. Verification action: confirm against
zexall/zexdoc during S5 hardening.

## Residual Gaps (open for M9 closure)

- DD/FD indexed prefixes (IX/IY): still decoded as deterministic 4T no-ops. (S4)
- DDCB/FDCB indexed bit/rotate operations. (S4)
- IM0/IM2 acknowledge/vector-source fidelity remains simplified. (S5)
- Port I/O data backing: IN/OUT and block-I/O data are deterministic open-bus
  stubs; timing/PC/counter/memory effects are correct. (later milestone)
- Milestone-specific opcode-trace parity evidence vs openMSX not yet produced;
  block-I/O undocumented flag precision unverified. (S5)

---

# M9-S4 Slice Addendum (REQ-M9-008) — 2026-07-06

- Slice ID: M9-S4
- Source Request: REQ-M9-008
- Status: Completed (DD/FD indexed + DDCB/FDCB indexed-bit families; gates green).
  Resolves QA blockers R2 and R3.

## What Was Implemented

Replaced the previous DD/FD 4T no-op (which consumed only the prefix byte and
left the following opcode to be silently mis-decoded as an unprefixed
instruction on the next `step()`) with a full x/y/z/p/q decode-driven indexed
path. Scope is strictly M9-S4: no change to IM0/IM2 vector fidelity (S5).

- DD/FD prefix retargeting (`execute_indexed` -> `execute_indexed_opcode`):
  - HL -> IX/IY for the 16-bit forms: `LD IX,nn`, `LD (nn),IX` / `LD IX,(nn)`,
    `INC IX` / `DEC IX`, `ADD IX,rp` (rp[2] becomes IX/IY, i.e. `ADD IX,IX`),
    `PUSH IX` / `POP IX`, `JP (IX)`, `LD SP,IX`, `EX (SP),IX`.
  - Undocumented half-register operands: base opcodes using H/L (r[] index 4/5)
    operate on IXH/IXL/IYH/IYL for `INC`/`DEC`, `LD r,n`, `LD r,r'`, and the
    ALU group — EXCEPT when the operand is (HL), which becomes (IX+d)/(IY+d).
  - (IX+d)/(IY+d) displacement addressing with a signed displacement byte and
    the correct extra T-state penalties for `LD r,(IX+d)` / `LD (IX+d),r` (19T),
    `LD (IX+d),n` (19T), `alu A,(IX+d)` (19T), and `INC/DEC (IX+d)` (23T).
  - Documented Z80 subtlety honored: when (IX+d) is one operand of an `LD`
    (i.e. y==6 or z==6 in x==1), the OTHER register operand's H/L is the REAL
    H/L, NOT IXH/IXL. Register-to-register (neither operand memory) DOES
    translate H/L to IXH/IXL for both sides.
- Prefix chaining / fallthrough rule implemented (see "Prefix-chaining rule").
- DDCB/FDCB (`execute_indexed_cb`), byte order DD/FD CB <disp> <sub-opcode>
  (neither the displacement nor the sub-opcode is an M1 fetch, so R is not
  incremented for them):
  - Full rotate/shift (RLC/RRC/RL/RR/SLA/SRA/SLL/SRL) and RES/SET on (IX+d),
    result written to memory, with the undocumented register-copy variants:
    when the sub-opcode's z field != 6, the result is ALSO copied into the REAL
    register r[z] (z==4/5 are real H/L, not IXH/IXL). BIT never copies. Timing
    23T for rotate/shift/RES/SET.
  - `BIT n,(IX+d)`: 20T; Z/H/PV/S semantics as CB BIT, with the undocumented
    X/Y flags sourced from the HIGH byte of the computed (IX+d) address
    (the WZ/MEMPTR high byte), not the tested value.

## Prefix-chaining rule (as implemented)

A DD or FD prefix applies to the immediately following opcode. `execute_indexed`
fetches that opcode and:

1. If it is another DD/FD: the earlier prefix is discarded (costs 4T + one R
   increment) and the newest DD/FD re-latches the effective index register
   (a later FD overrides an earlier DD and vice-versa; only the last prefix in
   a run selects IX vs IY). Implemented by recursion accumulating 4T per
   redundant prefix.
2. If it is ED: the index prefix is ignored (the ED instruction is never
   index-modified) and `execute_ed_prefixed` executes it normally.
3. If it is any opcode with no index form (does not reference HL/H/L/(HL), e.g.
   `INC A`, `JP nn`, `EX DE,HL`, conditional jumps/calls): the prefix is
   ignored but still costs its 4T + R increment, and the instruction executes
   in its base unprefixed form via `execute_unprefixed`.

This removes the prior silent mis-decode: e.g. `DD 21 00 40` now correctly
executes `LD IX,0x4000` (14T) instead of consuming DD as a no-op and running
`21 00 40` as `LD HL,0x4000` on the following step.

## T-state accounting convention

`execute_indexed` returns the cost EXCLUDING the 4T of the prefix that led into
it; each prefix's 4T M1 is added by its consumer (the `execute_unprefixed`
DD/FD case, or the recursive `execute_indexed` for chained prefixes). Verified
totals: `LD IX,nn`=14, `ADD IX,rp`=15, `INC IX`=10, `INC IXH`=8, `LD r,IXH`=8,
`LD r,(IX+d)`=19, `INC (IX+d)`=23, `JP (IX)`=8, `LD SP,IX`=10, `PUSH IX`=15,
`POP IX`=14, `EX (SP),IX`=23, DDCB rot/shift/RES/SET=23, `BIT n,(IX+d)`=20,
`DD FD LD IY,nn`=18, `DD DD INC A`=12, `DD ED NEG`=12.

## Changed / Added Files

- src/devices/cpu/z80a_cpu.h (indexed decode/accessor declarations)
- src/devices/cpu/z80a_cpu.cpp (execute_indexed / execute_indexed_opcode /
  execute_indexed_cb / indexed_inc_dec + index accessors; DD/FD dispatch wired
  into execute_unprefixed)
- tests/unit/devices/z80a_indexed_unit_test.cpp (new)
- tests/unit/devices/z80a_indexed_cb_unit_test.cpp (new)
- tests/integration/machine/hbf1xv_indexed_program_integration_test.cpp (new)
- tests/CMakeLists.txt (registered 3 new test targets)

## Tests Added

- Suite `Devices_Z80AIndexed_Unit` (z80a_indexed_unit_test): 22 vectors covering
  LD IX,nn / LD IY,(nn) / LD (nn),IX; half-register ALU (ADD A,IXL full flags),
  INC IXH, LD IXH,IXL translation; (IX+d) loads/stores/ALU with signed AND
  negative displacement (LD B,(IX-5), DEC (IX-16), ADD A,(IX+1) Y-flag), the
  real-H-not-IXH subtlety for LD (IX+d),H; ADD IX,DE, INC IX, PUSH/POP IX,
  EX (SP),IX, JP (IX), LD SP,IX; and prefix chaining/fallthrough (DD FD -> IY,
  DD DD -> INC A @12T, DD ED -> NEG @12T with full flag byte). Full flag bytes
  and exact T-states asserted.
- Suite `Devices_Z80AIndexedCb_Unit` (z80a_indexed_cb_unit_test): 7 vectors
  covering RLC (IX+d) with register copy to B and the z==6 no-copy variant,
  SLA (IX+d) copy to D, RRC (IY-1) copy to E (negative disp, FD path),
  BIT 0,(IX+0) with X/Y from address high byte 0x28, and RES/SET (IX+d) with
  register copy asserting flags-unaffected. Memory, copied register, full flag
  byte, and T-states (23 rot/shift/RES/SET, 20 BIT) asserted.
- Suite `Machine_Hbf1xvIndexedProgram_Integration`
  (hbf1xv_indexed_program_integration_test): end-to-end DD-prefixed program
  (LD IX,0x4000; LD (IX+0),0xAB; LD (IX+1),0xCD; LD A,(IX+0); ADD A,(IX+1);
  HALT) through the machine boundary, asserting IX, both displacement stores,
  the wrapped accumulator (0x78), and a hardcoded 94-T cycle oracle
  (14+19+19+19+19+4) with a bounded guard loop (no wall-clock).

## Evidence Gates (actual output, 2026-07-06 cycle)

- tools/validate-assets.ps1: `Asset validation result: True` (7 BIOS files:
  f1xvbios/f1xvdisk/f1xvext/f1xvfirm/f1xvkdr/f1xvkfn/f1xvmus.rom, 1 ROM:
  aleste.rom), exit 0.
- tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt: report written,
  exit 0.
- cmake --build build --config Debug: build succeeded (all targets linked,
  including the three new test executables).
- ctest --test-dir build -C Debug --output-on-failure: `100% tests passed, 0
  tests failed out of 19` (prior baseline 16; three new suites added). Re-ran
  ctest a second time -> identical `19/19` (determinism confirmed).
- tools/openmsx-ab-smoke.ps1: executed (exit 0); docs/openmsx-ab-smoke.md
  refreshed (timestamp 2026-07-06T00:27:48+09:00). openMSX 19.1 confirmed
  available on WSL at /usr/bin/openmsx.

## Parity Note (Assumption / Verification Action)

Assumption: `docs/openmsx-ab-smoke.md` remains availability/version + A/B input
metadata only; it does NOT emit an instruction-level opcode trace comparison for
the DD/FD or DDCB/FDCB families. It confirms the reference toolchain is present
but does not by itself prove indexed opcode-level parity. Verification action:
build a deterministic CPU trace harness (per-instruction register/flag/T-state
dump) and diff it against an openMSX debugger trace for IX/IY-heavy programs in
the parity-focused slice (M9-S5).

Assumption: `BIT n,(IX+d)` X/Y flags are sourced from the high byte of the
computed (IX+d) address, and register-to-register indexed timing/half-register
edge behavior follows the "Undocumented Z80 Documented" model. Verification
action: confirm against zexall/zexdoc-style vectors during S5 hardening.

## Residual Gaps (open for M9 closure)

- IM0/IM2 acknowledge/vector-source fidelity remains simplified (IM0 shares the
  IM1 fixed vector; IM2 reads (I<<8 | 0xFF) with no device-supplied vector bus).
  (S5)
- Port I/O data backing: IN/OUT and block-I/O data are deterministic open-bus
  stubs; timing/PC/counter/memory effects are correct. (later milestone)
- Milestone-specific opcode-trace parity evidence vs openMSX not yet produced
  (covers all opcode families incl. the new indexed ones); block-I/O and
  indexed-BIT undocumented-flag precision unverified against zexall. (S5)

---

# M9-S5 Slice Addendum (REQ-M9-009) — 2026-07-06

- Slice ID: M9-S5
- Source Request: REQ-M9-009
- Status: Partial. Resolves QA blocker R4 (IM0/IM2 acknowledge/vector fidelity).
  QA blocker R5 (opcode-trace parity vs openMSX) remains UNRESOLVED with an
  honest, reproducible blocker; recommended for an explicit decisions.md waiver.

## R4 — IM0/IM2 acknowledge/vector fidelity (implemented)

Replaced the simplified interrupt-acknowledge model (IM0 shared IM1's fixed
0x0038 vector; IM2 hardcoded the vector-table low byte to 0xFF) with an
injectable, deterministic acknowledge-bus model.

- Acknowledge hook: added a device-supplied data-bus byte to the CPU/state.
  `request_maskable_interrupt()` (no arg) leaves the bus floating (0xFF, no
  device driving it); `request_maskable_interrupt(std::uint8_t bus_vector)`
  models the acknowledging device driving a known byte onto the data bus. The
  value is stored in `Z80aState` (`interrupt_bus_vector_` /
  `interrupt_vector_supplied_`), snapshotted at service time, and reset on
  `clear_maskable_interrupt()` / `reset()` — fully deterministic and reproducible.
- IM0: on acknowledge the CPU executes the instruction the device drives onto
  the data bus, in place (PC is NOT advanced for that opcode fetch, since the
  byte comes from the device, not memory). Implemented as
  `2 + execute_unprefixed(opcode)` — two acknowledge wait states plus the
  opcode's own execution. With no device vector the bus floats to 0xFF, which
  decodes as RST 38h (push PC, jump 0x0038) at 11+2 = 13T — matching the classic
  IM0-RST figure. IM1's fixed 0x0038 path is no longer hardcoded into IM0; the
  default now falls out of the floating-bus opcode.
- IM1: unchanged — fixed restart to 0x0038, 13T; ignores any device bus byte.
- IM2: vector-table address = (I << 8) | bus_vector (device-driven low byte;
  floats to 0xFF when no device drives it). Reads the 16-bit ISR address
  little-endian and jumps there, 19T. Backward compatible with the prior
  (I<<8 | 0xFF) default.
- NMI preserved: 0x0066, IFF1->0, IFF2 retained; RETN restores IFF1<-IFF2;
  RETI returns (IFF left as post-acknowledge). The interrupt-acknowledge M1 now
  ticks the refresh register (R) once, modeled and asserted.

## Changed / Added Files (S5)

- src/devices/cpu/z80a_state.h (interrupt bus-vector state + accessors + overload)
- src/devices/cpu/z80a_state.cpp (overload, getters, reset/clear wiring)
- src/devices/cpu/z80a_cpu.h (request overload declaration)
- src/devices/cpu/z80a_cpu.cpp (service_maskable_interrupt IM0/IM1/IM2 rewrite;
  kIm0OrIm1Vector renamed to kIm1Vector; R tick on acknowledge)
- tests/unit/devices/z80a_interrupt_modes_unit_test.cpp (new)
- tests/integration/machine/hbf1xv_interrupt_ack_integration_test.cpp (new)
- tests/CMakeLists.txt (2 new targets)
- tools/openmsx-ab-smoke.ps1 (added a real, non-fabricated R5 opcode-trace
  capability probe; ASCII-only report content)

## Tests Added (S5)

- Suite `Devices_Z80AInterruptModes_Unit` (z80a_interrupt_modes_unit_test):
  9 cases asserting PC, SP (pushed return address bytes on the stack), IFF1/IFF2,
  and exact T-states — IM0 default->RST 38h (13T), IM0 device-supplied RST 20h
  (0xE7 -> 0x0020, 13T), IM0 device-supplied NOP (0x00 -> 6T, no vectoring),
  IM1 ignores the bus and uses 0x0038 (13T), IM2 (I=0x40, bus=0x0A) table lookup
  at 0x400A -> ISR 0xBEEF (19T), IM2 floating-bus default low byte 0xFF, NMI
  vector 0x0066 with IFF1 cleared / IFF2 retained then RETN restoring IFF1<-IFF2,
  RETI returning with IFF left cleared, and the refresh-register single tick on
  acknowledge.
- Suite `Machine_Hbf1xvInterruptAck_Integration`
  (hbf1xv_interrupt_ack_integration_test): 2 end-to-end scenarios through the
  machine boundary — IM2 device vector 0x08 selecting an ISR (LD A,0x55; RETI)
  via table entry 0x4008 with a 44-T oracle, and IM0 device-driven RST 18h (0xDF)
  running an ISR (LD B,0x99; RET) with a 34-T oracle. Both assert ISR side
  effects, correct return address, stack balance on return, and the cycle total,
  with a bounded guard loop (no wall-clock).

## Evidence Gates (actual output, 2026-07-06 cycle)

- tools/validate-assets.ps1: `Asset validation result: True` (7 BIOS files:
  f1xvbios/f1xvdisk/f1xvext/f1xvfirm/f1xvkdr/f1xvkfn/f1xvmus.rom, 1 ROM:
  aleste.rom), exit 0.
- tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt: report written.
- cmake --build build --config Debug: build succeeded (build_exit=0), both new
  test executables linked (devices_z80a_interrupt_modes_unit_test,
  machine_hbf1xv_interrupt_ack_integration_test).
- ctest --test-dir build -C Debug --output-on-failure: `100% tests passed, 0
  tests failed out of 21` (ctest_exit=0; prior baseline 19, two new suites).
  Re-ran ctest -> identical `21/21` (determinism confirmed).
- tools/openmsx-ab-smoke.ps1 -> docs/openmsx-ab-smoke.md: executed; ran a real
  opcode-trace capability probe against openMSX 19.1 (see R5 below).

## R5 — opcode-trace parity vs openMSX: UNRESOLVED (honest blocker)

A genuine attempt was made to produce a real instruction/opcode trace parity
comparison. The A/B smoke script now runs a headless openMSX Tcl capability
probe and records the ACTUAL captured results into `docs/openmsx-ab-smoke.md`.
Probe output (real, this cycle, machine C-BIOS_MSX2+):

```
PARITYPROBE cpu_regs_debuggable=1
PARITYPROBE step_pc0=0000
PARITYPROBE step_pc1=0000
PARITYPROBE step_advanced=0
PARITYPROBE ram_write_c000_readback=FF
```

Concrete, reproducible blockers (no parity result fabricated):

1. openMSX only executes within a complete MSX machine memory map
   (slots/subslots/mapper). Loading a flat test program into a fixed RAM page to
   mirror this emulator's bare 64K flat RAM fails at reset — page 0xC000 reads
   back 0xFF (unmapped) until the BIOS configures the slot registers, i.e. it
   requires exactly the machine wiring that M9 explicitly excludes.
2. The openMSX debugger `step` command does not advance the CPU PC in the
   headless `-script` startup context (pc stays 0x0000 across steps), so a
   deterministic per-instruction step trace cannot be captured this way.
3. This emulator has no per-instruction trace-export facility yet; the headless
   binary only reports frame/cycle summary counters.

Because a faithful, state-comparable instruction trace cannot be produced in
this environment, NO parity diff is asserted (anti-fabrication). openMSX 19.1
availability, the `CPU regs` debuggable, and the concrete non-comparability are
all recorded from real execution.

Verification action to close R5 (deferred / waiver candidate):
1. Add a deterministic CPU trace-export mode to this emulator (per-instruction
   PC/registers/flags/T-state dump for a loaded flat program).
2. Build a comparable openMSX side by either implementing enough MSX slot/RAM
   machine wiring to load the same program and drive openMSX via `-control`
   stdio single-stepping after boot, or by using a zexall/zexdoc self-checking
   ROM whose pass/fail signature is comparable across both emulators.
3. Diff the traces and record the actual diff outcome.

Recommendation: record an explicit R5 parity waiver in
`agent-protocol/channels/decisions.md` (currently template-only, no waiver),
scoping opcode-trace parity to a later machine-wiring milestone, OR schedule a
dedicated trace-harness slice. R5 must NOT be claimed resolved without a real
diff artifact.

## Residual Gaps (open after S5)

- R5 opcode-trace parity vs openMSX: UNRESOLVED (blocker + verification action
  above; waiver candidate).
- Port-I/O and block-I/O data remain deterministic open-bus stubs
  (timing/PC/counter/memory correct). (later milestone)
- Undocumented-flag precision for block-I/O and indexed-BIT unverified against
  zexall/zexdoc (needs the trace/self-check harness from R5).
- IM0 models single-byte device-driven opcodes (RST is the canonical case);
  multi-byte/prefixed IM0 acknowledge sequences (e.g. device-driven CALL) are
  not modeled — out of scope and not used by MSX (which uses IM1/IM2).
