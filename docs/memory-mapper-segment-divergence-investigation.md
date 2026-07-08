# Memory-Mapper Segment/RAM-Content Divergence Investigation (instruction #145,503)

- Type: targeted, coordinator-authorized bug investigation (mirrors the DEC-0026 precedent) —
  **not** a new milestone slice, no scope change.
- Investigator: developer agent, 2026-07-08, following up on
  `docs/vdp-vr-hr-boot-hang-fix-report.md` §4's open finding (recorded via DEC-0026,
  `agent-protocol/channels/decisions.md`).
- Files touched (final state): **none in `src/`**. A temporary, uncommitted diagnostic CLI mode
  was added to `src/main.cpp` for this investigation and fully reverted via
  `git checkout -- src/main.cpp` before finishing (confirmed via `git diff --stat -- src/main.cpp`
  returning empty). This document only adds a short, clearly-labeled correction addendum to
  `docs/vdp-vr-hr-boot-hang-fix-report.md` §4 (original text preserved, not rewritten).
- Verdict, up front: **the segment-register hypothesis was disconfirmed** (segment registers are
  byte-identical between this emulator and real openMSX at the divergence point). Digging further
  found the *actual* root cause: **the originally-reported "physical RAM content mismatch" at
  instruction #145,503 was a false positive caused by a flaw in the ad-hoc live-openMSX-Tcl
  comparison technique used to obtain the openMSX-side value** — not a defect in this project's
  memory-mapper or RAM code. Once the comparison technique is corrected (a genuine openMSX
  power-cycle instead of a bare Tcl `reset`), this emulator and real openMSX produce **byte-for-byte
  identical** results at instruction #145,503 and for a further 300,000-instruction window beyond
  it. **No code fix was needed or applied.**

---

## 1. What was checked (per the assigned investigation plan)

### Step 1 — independently re-verify this emulator's own segment register at instruction #145,503

A temporary `--diag-mapper-seg <bios_dir> <target_instr_count>` CLI mode was added to
`src/main.cpp` (never committed — reverted at the end of this investigation), driving a real
`Hbf1xvMachine::cold_boot()` boot with the CPU stepped via the exact loop shape
`src/frontend/sdl3_app.cpp`'s `Sdl3App::run_one_frame()` uses (`step_cpu_instruction()` sub-loop to
a frame boundary, then `on_vsync_boundary()` — never `run_frame()`), since `--debug-session` never
calls `on_vsync_boundary()` and would not reproduce the scenario (per this task's own briefing).

At `total_instructions == 145503` (i.e. immediately after the 145503rd real
`step_cpu_instruction()` call, matching `tools/trace-diff.py`'s trace-sequence-number convention —
see §3's methodology note on this), captured:

```
DIAG seq=145503 pc=455 hl=8001 af=ff00 sp=0
DIAG port_fc=0x83 port_fd=0x82 port_fe=0x81 port_ff=0x80
DIAG mem[0x8001]=0xff
```

This independently reproduces the original finding exactly: `PC=0x0455`, `HL=0x8001`,
`A=0xFF` (read of logical `0x8001`), port `#FE` (page 2's mapper segment register,
`src/devices/chipset/mapper_io.h`) reads back `0x81`.

`0x81 & 0x1F` (this project's real S1985 5-bit readback mask, `mapper_io.h:26`) `= 0x01`, so page
2's TRUE segment (used by `MemoryMapperRam::physical_address()`,
`src/devices/memory/memory_mapper_ram.cpp:15-21`, `segment & 3`) is **1** — physical RAM offset
`1*0x4000 + 1 = 0x4001`, exactly the address the original finding named.

### Step 2 — get the same information from real openMSX at the same instruction count

openMSX 19.1 (WSL, `/usr/bin/openmsx`), machine `Sony_HB-F1XV`, driven via a live Tcl script
(the established M11-M28 technique, `tools/openmsx-m28-c5-boot-parity.ps1`'s pattern: authentic
reset, `reg af 0; reg bc 0; ...; reg sp 0xFFFF; reg pc 0x0000; reg im 1; reg iff 0`, then
single-stepped). Two key device-state debuggables (real openMSX Tcl capabilities, read for
understanding only, never copied into `src/`):

- `debug read ioports <port>` — the raw I/O-port readback (`MSXCPUInterface::IODebug::read`,
  `references/openmsx-21.0/src/cpu/MSXCPUInterface.cc:1302-1306`, calls `peekIO`, non-destructive).
- `debug read "Main RAM regs" <page>` — the memory mapper's own TRUE, masked-at-write-time
  segment register (`MSXMemoryMapperBase::Debuggable`, name = `getName() + " regs"`,
  `references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:113-124`; device id `"Main RAM"` per
  `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:126`).

**First (naive) attempt — using only a bare `reset` (matching the exact idiom every prior
M11-M28 A/B harness in this project has used, e.g. `tools/openmsx-m28-c5-boot-parity.ps1`) —
reproduced the original finding**: `af=0000` (A=0x00) at the equivalent instruction, `port_fe=81`,
and **`seg2=01`** — the TRUE mapper segment for page 2. **This is the pivotal data point: segment 2
is `01` on BOTH sides.** The segment-register-divergence hypothesis this task was primarily meant to
test is therefore **disconfirmed** — real openMSX and this emulator select the *identical* physical
segment (1) for page 2 at this exact instruction. `port_fc/fd/ff` also matched exactly
(`83/82/80`), i.e. the full page→segment assignment (`3,2,1,0` for pages `0,1,2,3` — openMSX's own
documented normal MSX2+ BIOS convention, `MSXMemoryMapperBase.cc:50`: *"On MSX2 and higher, the
BIOS will select segments 3..0 for pages 0..3"*) is identical between the two emulators.

So the divergence is **not** a segment-selection bug — per the investigation plan's own step 4,
this meant proceeding to trace back the physical-RAM write history at offset `0x4001`.

## 2. The real root cause: a comparison-methodology artifact, not a code bug

### 2.1 This emulator's own write history at physical offset 0x4001 (ground truth, own side)

Extending the temporary diagnostic to log every change to `machine.dram().read(0x4001)`
(`machine.dram()` is the raw, unmapped physical `MemoryRegion`, `src/machine/hbf1xv_machine.h:178-179`)
across the full boot up to and beyond instruction #145,503:

```
DIAG dram[0x4001] initial (post cold_boot, 0 steps) = 0xff
DIAG WRITE dram[0x4001] seq=145505 pc_before=456 old=0xff new=0x0  segs_before(fc,fd,fe,ff)=0x83,0x82,0x81,0x80
DIAG WRITE dram[0x4001] seq=145508 pc_before=459 old=0x0  new=0xff segs_before(fc,fd,fe,ff)=0x83,0x82,0x81,0x80
```

This physical byte is **never written before instruction #145,503** in this emulator — the read
at #145,503 (`A=0xFF`) is reading the untouched, deterministic cold-boot RAM pattern. The two
writes immediately AFTER (seq 145505, 145508) are exactly the `LD (HL),A` steps of the classic
non-destructive `LD A,(HL); CPL; LD (HL),A; CP (HL); CPL; LD (HL),A` probe the investigation
briefing named: read `0xFF` → complement to `0x00` → write `0x00` (seq 145505) → compare → complement
back to `0xFF` → restore (seq 145508). Fully self-consistent, deterministic, sane behavior — no
internal inconsistency in this project's mapper/RAM code.

Extending the same run to 300,000 instructions: no further write to physical `0x4001` occurs; the
byte ends the window back at `0xFF` (restored by the probe), and every port/segment readback stays
constant. This project's own behavior is completely stable and self-consistent.

### 2.2 Why real openMSX appeared to show 0x00 there — the methodology bug

The established M10-M28 live-Tcl harness pattern (`tools/openmsx-m28-c5-boot-parity.ps1` and
predecessors) starts openMSX, lets it **free-run for several real seconds** under its own default
autoboot (`after time $BootSeconds { reset; reg af 0; ...; debug break }`), and only THEN issues a
Tcl `reset` before single-stepping. This is safe for **CPU-register-only** comparisons (every
prior M10-M28 A/B check in this project's history only compares `trace-diff.py`'s architectural
register/flag/PC field set — confirmed by reading `tools/trace-diff.py`'s `ARCH_FIELDS` list, which
contains no memory-content field at all) because a Tcl `reset`, combined with the explicit
`reg af 0` / etc. commands, genuinely puts the CPU into a clean, deterministic state matching this
project's own `cold_boot()`.

**It is NOT safe for RAM-content comparisons**, because on real hardware — and openMSX faithfully
models this — a **reset button does not clear RAM**; only a genuine **power-on** does. Grounded
directly in openMSX source (read for understanding, never copied into `src/`):

- `references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:47-52` — `reset()` only does
  `std::ranges::fill(registers, 0)` (the mapper's own segment registers) — it never touches RAM
  content.
- `references/openmsx-21.0/src/memory/MSXMemoryMapperBase.cc:41-45` — `powerUp()` is the ONLY path
  that calls `checkedRam.clear()` (RAM content reload) before also calling `reset()`.
- `references/openmsx-21.0/src/memory/CheckedRam.cc:85-89` — `CheckedRam::clear()` delegates to the
  underlying `Ram::clear()` (content) plus its own UMR-tracking bookkeeping (unrelated to content).
- `references/openmsx-21.0/src/memory/Ram.cc:37-78` — `Ram::clear()` reloads the machine XML's
  deterministic `initialContent` pattern (decoded once, at construction, and again on every
  `clear()` call) — this is the actual power-on RAM pattern.
- `references/openmsx-21.0/src/MSXMotherBoard.cc:699-736` — `MSXMotherBoard::powerUp()` iterates
  `d->powerUp(time)` over every device; only reachable via the global boolean `"power"` Setting
  (`references/openmsx-21.0/src/GlobalSettings.cc:12-13`), whose observer calls
  `motherBoard.powerUp()` when set true (`MSXMotherBoard.cc:1317-1319`) — i.e. the Tcl-level
  `set power off` / `set power on` idiom is the genuine power-cycle equivalent; a bare `reset`
  command is not.

Because the harness's free-run happens under openMSX's own **normal, uninterrupted, continuing**
BIOS boot (which — per this project's own M16/M28 findings — runs the RAM-sizing loop for roughly
200,000-300,000 instructions and continues well beyond it within a couple of real seconds), that
free-run had **already legitimately written `0x00` to physical `0x4001`** by the time the harness's
`reset` fired — and since `reset` does not clear RAM, that leftover byte silently survived into the
"post-reset" comparison state, masquerading as if it were the deterministic post-reset value. This
is a **stale, uncontrolled, free-run-duration-dependent artifact of the comparison technique** —
not a real property of a genuine cold-booted openMSX instance.

### 2.3 Confirmation (four independent, converging pieces of evidence)

1. **Power-cycle fix.** Inserting `set power off; set power on` before the existing `reset` +
   register-zeroing block (a genuine, source-grounded power-cycle, §2.2) and re-running the
   identical instruction-145503 check:

   ```
   OMINIT mainram4001=FF mainram0001=FF
   OMDIAG seq=145504 pc=0455 hl=8001 af=FF00 mem8001=FF mainram4001=FF watch_hits=0
   ```

   `af=FF00` → **A=0xFF**, exactly matching this emulator's `af=ff00`. `mem8001` (logical read) and
   `mainram4001` (`debug read "Main RAM" 0x4001"`, the `Ram`/`RamDebuggable` physical-content
   debuggable, `references/openmsx-21.0/src/memory/Ram.cc:86-109`, registered under the mapper's own
   device name) both read `FF`. **Full match — the original divergence disappears entirely.**

2. **Non-determinism vs. free-run duration (without the power-cycle fix).** Re-running the SAME
   bare-`reset` (no power-cycle) harness with different pre-script free-run durations:
   - `BootSeconds=6` (original): `mainram4001=00` (contaminated — matches the original report).
   - `BootSeconds=2`: `mainram4001=00` (still contaminated — the natural free-run had already
     passed this point within 2 real seconds, ~7M cycles).
   - `BootSeconds=0.1`: `mainram4001=FF` (clean — the natural free-run had NOT yet reached this
     point) — and the subsequent 145,502-step run from the register-reset state ALSO reads
     `af=FF00`/`mem8001=FF`/`mainram4001=FF`, matching this emulator exactly.

   A value that flips between `0x00` and `0xFF` purely as a function of an *unrelated, uncontrolled*
   wall-clock free-run duration **before** the point being measured is definitionally not a genuine,
   deterministic property of the post-reset boot sequence — it is leftover contamination.

3. **Write-history cross-check (openMSX watchpoints vs. this emulator's own write log).** Using
   openMSX's live watchpoint mechanism (`debug set_watchpoint write_mem <addr> {} {...}`, one per
   CPU page's `+1` offset — `0x0001/0x4001/0x8001/0xC001` — with each callback checking
   `debug read "Main RAM regs" <page>` to filter for segment 1, using the documented
   `::wp_last_address`/`::wp_last_value` globals,
   `references/openmsx-21.0/src/debugger/Debugger.cc:1378-1380`), extended to a 300,000-instruction
   window under the power-cycled (clean) setup:

   ```
   OMINIT mainram4001=FF mainram0001=FF
   OMWATCH n=145505 page=2 seg=01 addr=8001 value=00 pc=0456
   OMWATCH n=145508 page=2 seg=01 addr=8001 value=FF pc=0459
   OMDIAG seq=300001 pc=7D68 hl=EE1F af=FF7A mem8001=FF mainram4001=FF watch_hits=2
   ```

   **Exactly two writes, at the exact same relative instruction offsets (145505, 145508) and the
   exact same old→new value transitions (`FF→00`, then `00→FF`) as this emulator's own independently
   logged write history in §2.1.** Both emulators execute byte-identical control flow AND
   byte-identical physical memory effects through at least 300,000 instructions once the comparison
   is done correctly.

4. **Cross-check of the initial-content formula itself.** Before finding the power-cycle
   explanation, the machine XML's own `initialContent` blob
   (`references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:129`, decoded programmatically for
   verification only, never reproduced as new data in `src/` or `docs/` — it is a short,
   already-periodic pattern already fully described by the same textual formula this project's own
   `src/machine/hbf1xv_machine.cpp:308-320` (`initialize_dram_pattern()`) already cites and
   implements) was independently decoded and checked byte-for-byte against
   `initialize_dram_pattern()`'s own formula: **exact match**, including the specific byte at
   pattern position 1 (`0xFF`, the byte that ends up at physical offset `0x4001`). This ruled out
   an initial-content-formula bug before the power-cycle root cause was found, and remains true
   regardless — this project's own RAM power-on pattern is correct.

## 3. A methodology note for future live-Tcl instruction-count investigations

Getting the openMSX-side data required correcting a genuine off-by-one in the recursive
`after break tick; step` idiom (the SAME idiom `tools/openmsx-m28-c5-boot-parity.ps1` uses, just
applied here to a raw instruction-count target instead of a full per-step trace emit): each `tick`
invocation happens **before** the step it is about to request completes, so `N` `tick()`
invocations correspond to only `N-1` real completed `step` calls. To capture the CPU/memory state
matching `tools/trace-diff.py`'s trace-sequence convention (`seq=145503`, i.e. the state as of
exactly 145503 real completed steps), the Tcl script's own target counter must be set to
`145503 + 1 = 145504`. This was verified empirically by cross-checking against this emulator's own
`total_instructions==145502` vs. `==145503` diagnostic output (`pc=0454`/`af=0000` for 145502 vs.
`pc=0455`/`af=ff00` for 145503) before locking in the correct openMSX-side target. This is recorded
here so a future investigation reusing this exact instruction-count-targeting technique does not
have to rediscover it. (A plain synchronous `for {step}` loop without the `after break` recursion
was also tried first and silently produced a no-op — zero steps actually took effect before the
subsequent register read — confirming `step` in this context is not safely usable outside the
established `after break`-driven idiom.)

## 4. Conclusion

- **Segment-register hypothesis: disconfirmed.** Page 2's mapper segment is `1` on both sides at
  instruction #145,503 — proven directly, independently, on both emulators.
- **Root cause of the originally-reported "physical RAM content mismatch": a false positive**,
  caused by comparing this emulator's genuinely cold-booted RAM state against a real openMSX
  instance whose RAM had been silently contaminated by an uncontrolled pre-script free-run that a
  bare Tcl `reset` (correctly, per real hardware semantics) does not clear. This is a flaw in the
  **one-off manual memory-content check** added on top of the standard harness for that specific
  finding — **not** a flaw in `tools/trace-diff.py`'s regular architectural-field comparison (which
  never reads memory content, and whose "instructions 0-145,502 match exactly on every CPU-visible
  field" claim remains independently correct and unaffected by this bug), and **not** a flaw in any
  of this project's own `src/` code.
- **No fix applied** — `src/devices/memory/memory_mapper_ram.cpp`, `src/devices/chipset/mapper_io.cpp`,
  and their wiring in `src/machine/hbf1xv_machine.cpp` are confirmed correct as-is, independently
  re-derived from openMSX's own real S1985/mapper semantics (§2.2, and the pre-existing
  `mapper_io.h` doc comments this investigation cross-checked and confirmed still accurate).
- **No new backlog row, no ledger status change requested.** This document adds a short, clearly
  labeled correction addendum to `docs/vdp-vr-hr-boot-hang-fix-report.md` §4 (original text kept
  intact, not rewritten, mirroring the project's own established "stale-figure caveat" precedent in
  `docs/m28-parity-trace-diff.md`). Updating `agent-protocol/channels/decisions.md`,
  `agent-protocol/state/current-phase.md`, and `agent-protocol/state/deferred-backlog.md`'s own
  cross-references to this now-superseded finding is left to the coordinator, per this project's
  standing convention that ledger entries are coordinator-authored.

## 5. What remains genuinely open (not investigated further here, out of this task's scope)

`docs/vdp-vr-hr-boot-hang-fix-report.md` §4 also mentioned a separate, later observation: this
emulator, run on its own (not an A/B comparison), eventually (~2.8-2.9M instructions) executes
garbage from an unintended memory region, corrupts the stack via a repeating `RST 38`, and settles
into a permanent `HALT`, described there as "downstream of this content mismatch." Since the
"content mismatch" that downstream narrative was anchored to is now shown to be a false positive
(§2-§3 above) — and the non-destructive probe this emulator executes at instruction #145,503 is, by
construction, insensitive to whether the probed byte started at `0xFF` or `0x00` (it complements
and restores whatever it finds) — **that downstream dead-end's actual cause, and whether it is a
genuine divergence from real hardware/openMSX at all, is now an open question**, not confirmed
either way by this investigation (its own A/B status was not independently re-checked here — the
original report did not present it as an A/B comparison at all). This is flagged here, honestly, as
unresolved and worth a dedicated future investigation, but was **not** in this task's assigned
scope (which was specifically the #145,503 segment/RAM-content finding) and was not pursued
further.

## 6. Evidence gates run this cycle

- Temporary diagnostic added to `src/main.cpp`, used interactively, then reverted:
  `git diff --stat -- src/main.cpp` → empty (confirmed clean revert).
- `cmake --build build --config Debug` → succeeded (only pre-existing, unrelated codepage
  warnings for non-ASCII characters in `.h` file headers; zero errors).
- `git diff v1.0.28 --name-only -- src/devices/ src/peripherals/ src/core/` → empty — no existing
  production file under those directories was touched, so per the standing
  `feedback_slow_test_cadence.md` rule the slow ZEXALL/ZEXDOC full sweep is not mechanically
  required this cycle (no CPU/core-adjacent change was made — indeed no `src/` change survived at
  all).
- `ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure` →
  **145/145 passed**, identical to the pre-investigation v1.0.28 baseline (zero regression, as
  expected since zero production code changed).
- No new unit/integration tests were added — there is no new or changed production behavior to
  test; this is a closed investigation with a "no bug found here" outcome, per this task's own
  explicit "dual-outcome acceptance" ground rules.
