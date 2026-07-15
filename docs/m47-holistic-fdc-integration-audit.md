# M47 — Holistic FDC-integration + save-corruption root-cause audit (YS II)

**Status:** read-only investigation, 2026-07-13. No `src/` edited. Builds on
`docs/m47-fdc-write-investigation.md` + `docs/m47-qa-signoff.md` and the newer coordinator
evidence that the FDC WRITE is byte-perfect and the corruption is UPSTREAM. Deliverable per the
holistic-audit task.

> **Headline reframe.** The M47 write-path fix (`commit_write_sector_byte`, never-drop) was a
> legitimate WD2793 fidelity improvement, but it is **not** the YS II save-corruption root cause.
> A per-byte write trace of a real corrupting save (`debug/traces/stream_f005648_..._fdcwrite.log`,
> 513 lines, **0 substituted bytes**) proves the WD2793 committed exactly the 512 bytes the CPU
> handed it. The save buffer in main RAM was **already corrupt before the FDC touched it.** The
> defect is in the game's *live state / save-data build*, not in any FDC/disk code.

---

## 0. Top-3 candidates + the single most-likely root cause

The corruption is a **single wrong scalar** (`base`/`seed`) in YS II's live save-data working
copy: the confirmed forensic fingerprint is a contiguous **122-byte table every byte offset by an
identical constant `-0x8A`** (118 of 122 bytes exact). A *uniform* offset can only be produced by a
wrong value **established once, before the table was assembled** — not by damage sprayed into the
buffer mid-build. Everything downstream (FDC write, disk image, reload) faithfully propagates that
one wrong value until the reloaded save decodes to garbage pointers and the CPU derails
(`LD SP,HL` with a computed junk `SP=0x0330` → stack runaway → crash).

| # | Candidate divergence (all produce a wrong upstream scalar) | Evidence fit | Confidence |
|---|---|---|---|
| **1** | **VDP-derived value read during state maintenance diverges** — the game reads a VDP status bit / VRAM-address-latched byte whose value depends on command-engine/status **timing**, and our timing differs from real HW when a command is pending (`TRANSFER_PENDING=1` at save time). Feeds one wrong `base`. | Coordinator's flagged anomaly (pending cmd); game does heavy DI-bracketed VDP port-98/99 I/O around state code; constant-scalar = "read one wrong VDP value". | **Medium** — needs watchpoint to confirm the seed source. |
| **2** | **Interrupt-delivery *timing* / re-entrancy divergence (M36 "Bug B" family)** during the *build* phase (before the DSKIO `DI`), perturbing an ISR-shared accumulator. | Crash is a nested-interrupt-flavoured stack runaway; whole-project history is VDP/`/INT`. | **Low-Med** — interrupt *count/rate* measured CORRECT (≈1/frame, 59 736 cyc); a *constant* offset argues against mid-loop perturbation. |
| **3** | **A device/timer seed read (RTC RP5C01, PSG) diverges** and seeds the save transform. | "Accumulates over play time" fits a clock/timer; RTC is the classic time-accumulating source. | **Low-Med** — RTC advances off the (correct-rate) scheduler; needs value-level A/B. |

**#1 — my single most-likely root cause, with reasoning.** The save table is corrupted by *one*
wrong byte the game read while building it, and the strongest-supported source is a **VDP
read whose result depends on emulated command-engine / status-bit timing.** Reasoning chain:

1. The `-0x8A` is **uniform and contiguous** ⇒ a single scalar wrong *before* the build loop, not
   mid-build damage (§4). This eliminates the "interrupt hit the copy loop" class of theory.
2. The FDC path, the FDC data phase, the VDP command engine writing main RAM, interrupt *count*,
   and slot/FM-PAC paging are all **ruled out by direct evidence** (§3). What remains is "the game
   computed/read one wrong value."
3. The coordinator's own anomaly is a **pending VDP command-engine transfer at save time**, and the
   trace shows the game constantly doing **`DI`-bracketed VDP `OUT (C)` to port 0x99** (VRAM
   address latch) + port 0x98 (VRAM data) around its state code. Our command-engine finish/status
   timing is a **prediction** (`calcFinishTime`, openMSX model) and our VDP status-bit timing is a
   line-granular approximation (documented in `poll_line_interrupt` / M32 "±1 line"); either can
   hand the game a value real hardware would not, once, feeding the wrong `base`.
4. This is the only candidate that simultaneously explains: the pending-command anomaly, the
   constant-scalar signature, the VDP-heavy state code, the long VDP/`/INT` bug lineage, **and**
   is consistent with openMSX proving the command engine cannot itself scribble main RAM (the
   corruption is the game *reading* a mistimed VDP value, not the engine writing RAM).

**This #1 is a ranked hypothesis, not a proven fault.** The `-0x8A` scalar's exact origin is the
one thing the existing crash artifacts cannot pin down; the decisive step is the memory-write
watchpoint on the save-table build + an openMSX A/B of the live scalar (§6). Rank order 1→2→3 is
the recommended investigation order.

---

## 1. Confirmed forensic signature (this investigation)

Source capture: the streamed crash bundle `debug/snapshot/stream_f005648_c0000000337417575/` +
`debug/traces/stream_f005648_..._{trace.txt,fdcwrite.log,fdc.log}` (a real corrupting run,
finalize_reason `sp_underflow sp=0330`).

- **The FDC write is faithful.** `fdcwrite.log` = 513 lines for LBA 9 (CHS 0/1/1), **every byte
  `cpu` (genuine), 0 substituted.** The controller wrote exactly the buffer it was fed.
- **The buffer feeding it is pre-corrupt.** The streamed bytes (idx 0..9 =
  `00 00 31 37 7C CC CC CE D0 10`) diverge from the correct save (FM-PAC `.sram` offset 0x10 =
  `00 00 31 37 7C CC CF D4 59 9A`) starting at **idx 6** — exactly the coordinator's fingerprint.
- **The corruption is a uniform additive offset.** Over the 184 differing bytes,
  `correct - corrupt (mod 256)` is **`0x8A` for 118 bytes (64%)**; within the main contiguous block
  **[idx 6..127]** it is `0x8A` for **118 of 122** bytes (the other 4 are a 3-byte header field +
  one edge). The remaining scattered ±1/±2 diffs are baseline noise (the `.sram` reference was
  re-saved at a slightly different game moment). Tool + numbers reproducible via
  `tools/m47-save-corruption-diff.py`.
- **The buffer is streamed from main RAM ~0x2000** (page 0, slot 3-0 RAM), by the **stock Sony disk
  driver polled loop** — trace PC `0x7596-0x759F`:
  `LD A,(BC=7FFF); ADD A,A; RET P; JP C; LD A,(HL); LD (DE=7FFB),A; INC HL; JP` — **byte-identical
  to the real Sony HBD-50 driver** (`C75C0`, web-sourced, §7). `IFF1=0` (DI) for the whole loop.
- **The crash is downstream of a corrupt *reload*.** After the save (frame 5884) the game re-reads
  a few sectors (`fdc.log`: LBA 1-4) and derails; at `PC=0xAA74` it executes `LD SP,HL` with
  `HL=0x0330` (a value computed from corrupt reloaded pointers), giving `SP=0x0330` → the
  `sp_underflow` finalize. Crash CPU: `IFF1=0 IFF2=0 IM=1 MASKABLE_PENDING=1` — a stalled maskable
  request, **not** an in-flight interrupt (openMSX + our core both refuse to inject a maskable IRQ
  while `IFF1=0`, §3.3), i.e. the crash is corrupt-data execution, not an interrupt fault.

---

## 2. Sony/HB-F1XV disk-interface integration (the task's angle 1) — MODEL IS CORRECT

Grounded in openMSX + the real Sony driver + the WD1793 datasheet (web, §7). Our wiring matches:

- The WD2793 is memory-mapped at CPU **0x7FF8-0x7FFF** (`SonyFdc`, `src/devices/fdc/sony_fdc.cpp`),
  decode identical to openMSX `PhilipsFDC` (`Sony`/`Philips` → `PhilipsFDC`,
  `DeviceFactory.cc:118`). Status/cmd 0x7FF8 … data **0x7FFB** … side 0x7FFC … drive 0x7FFD …
  **INTRQ/DRQ status 0x7FFF (bit6=!INTRQ, bit7=!DTRQ, active-low)**.
- **No CPU wait states, DRQ/INTRQ NOT wired to `/INT` or `/WAIT`.** openMSX states this verbatim:
  *"Drive control IRQ and DRQ lines are not connected to Z80 interrupt request"*
  (`references/openmsx-21.0/src/fdc/PhilipsFDC.cc:98-99`). Our `SonyFdc` likewise exposes DRQ/INTRQ
  only as the polled 0x7FFF status bits and inserts no wait cycles — **correct.**
- **The real transfer runs under `DI`.** The stock Sony HBD-50/HBD-F1 driver brackets the whole
  sector byte-loop with `DISINT;DI … EI;ENAINT` (web, §7); the trace confirms `IFF1=0` throughout
  our streaming loop. So **no ISR can run during the FDC data phase**, and a "missed DRQ" is a
  can't-happen on healthy MSX (Q5, §7). This is why the M47 never-drop write-path model, though
  correct, could never have been the YS II fault: nothing perturbs the DI-protected feed.

Net: the FDC↔CPU↔bus integration is faithful and is **not** a corruption source. (Assumption we
inherit and re-confirm: the Sony data port has no wait-states — openMSX + our bus agree.)

---

## 3. What is RULED OUT (with evidence)

1. **FDC Write-Sector / data-phase corruption** — `fdcwrite.log` shows 0 substitutions; the feed
   runs under `DI`. The buffer is corrupt before the FDC reads it. *(§1)*
2. **The VDP command engine scribbling main RAM** — structurally impossible: `VDPCmdEngine` holds
   only `VDP&` + `VDPVRAM&`, every access is `vram.cmdWrite/…readNP` masked to VRAM
   (`references/openmsx-21.0/src/video/VDPCmdEngine.{hh,cc}`; our
   `src/devices/video/vdp_command_engine.cpp` is the same shape — no RAM/CPU reference). It can
   *contend* for VRAM slots with the CPU but cannot reach 0x2000.
3. **Emulator injecting a maskable IRQ while `IFF1=0`** — our core gates on
   `state_.maskable_interrupt_pending() && state_.iff1()` (`z80a_cpu.cpp:69`); openMSX gates
   identically (`CPUCore.cc:2465`). The `IFF1=0 + MASKABLE_PENDING=1` crash state is a *stalled*
   request, not a stack-pushing interrupt. The runaway is corrupt-data execution (`LD SP,HL`).
4. **Interrupt *count* drift in normal play** — measured VBLANK inter-arrival is a rock-steady
   **59 736 cyc = 262 × 228 = correct NTSC frame**, ≈1 IRQ/frame. No extra/dropped VBLANKs during
   gameplay (the dense IRQs in the trace are the post-derail RST-38 crash loop, not gameplay).
5. **Slot / FM-PAC-paging corruption of the buffer** — `#A8` in the entire trace is only
   `FF`/`FC`/`3F`; it **never selects primary slot 1 or 2** (where the FM-PAC lives). The FM-PAC is
   never paged into CPU space during this save, so the buffer is built purely from slot-3-0 main
   RAM; the `.sram` is only a *correct reference copy*, not the buffer's source.
6. **FM-PAC SRAM re-lock / ROM substitution** — the corrupt band matches FM-PAC ROM at only
   **2-7 / 185** bytes (any bank); it is NOT ROM bytes, NOT a re-lock, NOT a byte-shift. The
   `sram_enabled_`/magic-latch path (`cartridge_fmpac_rom.cpp`) is trivial (`sram_.read(rel)`, no
   banking/timing) and irrelevant here anyway (point 5).
7. **Block-instruction (LDIR) interrupt handling** — our `block_transfer` rewinds `PC-=2` on repeat
   (`z80a_cpu.cpp:1241`), matching openMSX (`CPUCore.cc:4110-4112`) and fMSX (`CodesED.h:212-225`):
   an interrupt between iterations resumes correctly. Not a corruption path.

---

## 4. The central finding: an upstream single-scalar divergence

The `-0x8A` is **uniform across a contiguous 122-byte region**. That constrains the mechanism hard:

- A game builds such a table with a loop shaped like `table[i] = base + delta[i]` (positions,
  pointers, stat rows relative to an origin), or `dst[i] = src[i] ± key`. A **uniform** offset ⇒
  the single scalar `base`/`key` was wrong **before the loop ran**.
- This **eliminates "an interrupt/timing event corrupted the buffer during the fill"** — that would
  produce localized, position-dependent, run-varying damage, not a clean constant across 122 bytes.
- Therefore the fault is a **wrong value the game read or accumulated once**, then applied
  uniformly. The FDC, the disk image, and the reload then propagate it verbatim.
- The `.sram` (raw, correct) vs disk (`raw - 0x8A`) relationship also resolves the old
  "SRAM byte-perfect ⇒ FDC bug" reasoning in `m47-fdc-write-investigation.md`: the SRAM save and
  the disk save are **not the same bytes** — the disk copy is *derived from a live scalar that is
  0x8A wrong*, while the independently-written SRAM copy is fine. "SRAM perfect" never implied
  "disk should equal SRAM."

"Accumulates over play / first save works" is consistent with either (a) the wrong scalar only
*matters* once the table it seeds is populated by mid-game progress, or (b) a value that integrates
per-frame until its low byte reaches `0x8A`.

---

## 5. Ranked candidate divergences (concrete, each with fix + verification)

Every candidate is an instance of §4 (a wrong upstream scalar). Investigate in this order.

### C1 — VDP status/command-engine *timing* read during state maintenance  ★ leading
- **Where:** `src/devices/video/vdp_command_engine.cpp` (status/`CE`/`TR` + `transfer_pending_`
  timing, lines ~150-240, 690-740) and the VDP status-read path in `src/devices/video/v9958_vdp.cpp`;
  consumed by game VDP port-98/99 I/O.
- **Reference/spec:** openMSX predicts command finish via `calcFinishTime`/`setStatusChangeTime`
  and syncs lazily (`VDPCmdEngine.cc:725-747`, `peekStatus2`); status bits `TR/CE` are timing-exact
  there. Our finish/status timing is an approximation (and `poll_line_interrupt` documents a
  ±1-line status approximation, `hbf1xv_machine.cpp:787-791`).
- **Why this signature:** at save time `TRANSFER_PENDING=1`; if the game reads S#2 (`CE`/`TR`) or a
  VRAM byte whose value depends on when the command finished, a *single* mistimed read yields a
  wrong `base` → uniform offset. Explains the coordinator's pending-command anomaly + the
  VDP-heavy DI-bracketed state code + the constant scalar, without violating "engine is VRAM-only."
- **Proposed fix (after confirmation):** make `CE`/`TR`/`transfer_pending_` transitions and command
  finish time match openMSX's demand-driven `sync`/`calcFinishTime` model so a status/VRAM read
  while a command is pending returns the hardware-correct byte.
- **Verify:** watchpoint (§6) shows the wrong scalar is loaded from a VDP read; openMSX A/B of that
  exact read at the same cycle returns the value that makes the table `+0x8A` (i.e. correct).

### C2 — Residual interrupt-delivery *timing* / re-entrancy (M36 "Bug B" family)
- **Where:** `hbf1xv_machine.cpp:685-687` (level-sample `if (vdp_.irq_active()) request_maskable`)
  + `poll_line_interrupt` (line IRQ) + `v9958_vdp.cpp` `change_register`/`update_irq`/`on_vsync`
  (the IE0/IE1 re-eval the M36 fix touched).
- **Reference/spec:** `docs/m36-bugB-vdp-ie0-fix-report.md` (nested-VBLANK storm from missing
  IE0-write `/INT` re-eval); openMSX `VDP.cc:389-407` (`execVScan` raise), `CPUCore.cc:2462-2467`.
- **Why this signature:** if a VBLANK/line IRQ is accepted at a divergent instruction boundary
  during the *build* (before the DSKIO `DI`), the ISR could leave a shared accumulator off by a
  fixed amount that later seeds the table. **Caveat:** interrupt *count* is correct (§3.4) and a
  constant offset fits a mid-loop perturbation poorly — hence ranked below C1.
- **Proposed fix:** audit for any remaining `/INT` edge our `change_register`/`update_irq` or the
  level-sample re-assert handles differently from openMSX (esp. an IRQ re-asserted the same step it
  was released, or an EI-shadow off-by-one). Align to openMSX exactly.
- **Verify:** watchpoint shows the wrong scalar is written by the ISR path; A/B the ISR entry cycle
  vs openMSX.

### C3 — RTC (RP5C01) / PSG device seed read
- **Where:** `src/devices/rtc/rp5c01.*` (value + tick), or `src/devices/audio/psg_ym2149.*`.
- **Reference/spec:** openMSX `RP5C01`; our RTC advances off `RtcClock` (scheduler cycles).
- **Why this signature:** many RPGs stir a device/clock byte into a save seed/checksum; a wrong
  RTC digit or PSG read gives a constant offset and "accumulates over time."
- **Proposed fix:** correct the specific register/tick value once localized.
- **Verify:** watchpoint shows the scalar sourced from an `IN` (port 0xB5 RTC / 0xA2 PSG); A/B the
  read.

### C4 — CPU edge case not covered by ZEXALL
- **Where:** `src/devices/cpu/z80a_cpu.cpp` (undocumented opcode / flag / `WZ` / block-op edge).
- **Reference/spec:** ZEXALL/ZEXDOC (`references/zexall/`) — passing, but it does not exercise every
  MSX-specific timing/undocumented-flag interaction with real ROM code.
- **Why this signature:** a rare-opcode/flag miscompute in the transform could bias one accumulator.
  Low prior (ZEXALL is strong) but cheap to exclude via the A/B trace diff.
- **Verify:** cycle-exact CPU-trace A/B vs openMSX over the build window (the M16/M28 parity harness).

### C5 — Memory/mapper aliasing (64K vs 512K)
- **Where:** `src/devices/memory/memory_mapper_ram.*`, `src/machine/*` slot/mapper wiring.
- **Why this signature:** config-independent per the coordinator (64K & 512K both corrupt) makes a
  size-specific alias unlikely, but a segment/mirror edge touching the 0x2000/0x3400 working set is
  worth a pass. Lowest priority.
- **Verify:** watchpoint that the scalar's backing address is not aliased mid-build.

---

## 6. Diagnostic tools (built) + the decisive next steps

**Built now (no `src/` change):**
- `tools/m47-save-corruption-diff.py` — parses a captured `fdcwrite.log` sector and diffs it against
  the FM-PAC `.sram`, printing the delta histogram + contiguous corrupt regions. This is what
  surfaced the `-0x8A` constant-offset signature; reusable for any future corrupting capture
  (`python tools/m47-save-corruption-diff.py <fdcwrite.log> <lba> roms/fmpac.rom.sram`).

**The decisive instrument (developer, once the repro from `docs/m47-ys2-repro.md` lands):**
1. **Save-table write watchpoint.** Extend the existing non-perturbing `MemWatchObserver`
   (`hbf1xv_machine.h`, already watches 0x0039/0x003A/0xA5E1) — or a standalone headless probe using
   the mapper-RAM `MemWriteObserver` seam — to log **PC + value + prior value** for every write to
   the save working set (**0x2000-0x21FF** and the pre-image region the build reads, incl. ~0x3400).
   This directly answers "*what instruction writes the `-0x8A`-off bytes, and where does its `base`
   come from*". Trace `base` back to its defining `IN`/status/ISR write ⇒ that read is the
   divergence; its port/register names the culprit candidate (C1-C5).
2. **openMSX A/B of the live scalar.** Run the identical `--input-script` on WSL openMSX
   (`tools/openmsx-ab-smoke.ps1` shape) and compare the `base` value (and the disk save bytes) at
   the save cycle. The single value that differs by `0x8A` between us and openMSX pinpoints the
   defect.
3. **Cross-check** by re-capturing a second corrupting save and re-running
   `tools/m47-save-corruption-diff.py`: confirm the offset is again a single constant (validates the
   "one wrong scalar" model over "random damage").

Acceptance for a fix: the localized read/value matches openMSX bit-for-bit at that cycle; a fresh
YS II in-game re-save reloads byte-identical (owner live gate); `tools/m47-save-corruption-diff.py`
reports 512/512 match against a correct reference.

---

## 7. References (grounding)

Web (via research):
- WD1793 datasheet (DRQ gen, Lost-Data zero-fill, poll-vs-DMA option): https://hansotten.file-hunter.com/technical-info/wd1793/
- MSX low-level disk (polled model, ~114 T/byte slack): https://map.grauw.nl/articles/low-level-disk/
- Portar MSX tech doc (register map; DMA/INT "not used in MSX"): https://problemkaputt.de/portar.htm
- **Real Sony HBD-50 driver** (the exact `DI…EI` polled `0x7FFF`/`0x7FFB` loop our trace matches): https://raw.githubusercontent.com/apoloval/msx-system/master/disk/drivers/hbd-50/driver.asm
- Byte timing (250 kbps → 32 µs/byte): http://nerdlypleasures.blogspot.com/2021/04/floppy-drives-single-density-and-double.html
- MSX disk I/O disables interrupts: https://www.msx.org/forum/msx-talk/hardware/fast-disk-rom-what-exactly-does-it-do

Repository (read-only grounding):
- `references/openmsx-21.0/src/fdc/PhilipsFDC.cc` (Sony=PhilipsFDC; "IRQ/DRQ not connected"),
  `WD2793.cc` (write-data model), `share/machines/Sony_HB-F1XV.xml` (slot map, MSX-MUSIC no sram).
- `references/openmsx-21.0/src/video/VDPCmdEngine.{hh,cc}` (engine is VRAM-only; `calcFinishTime`),
  `VDP.cc` (`execVScan` `/INT`), `src/cpu/CPUCore.cc` (IRQ gate on IFF1, block-op resume).
- `references/fmsx-60/source/EMULib/WD1793.c` (stores every write, no timing-loss),
  `source/Z80/Z80.c` (`IntZ80` gates on `IFF_1`, clears IFF1/2 on entry).
- `references/fact-sheets/FDC for Sony HB-F1XV.md`; `references/zexall/`.
- Our code: `src/devices/fdc/{sony_fdc,wd2793}.cpp`, `src/devices/cartridge/cartridge_fmpac_rom.cpp`,
  `src/devices/cpu/z80a_cpu.cpp`, `src/devices/video/vdp_command_engine.cpp`,
  `src/machine/hbf1xv_machine.cpp`; prior `docs/m36-bugB-vdp-ie0-fix-report.md`.

Evidence artifacts: `debug/traces/stream_f005648_c0000000337417575_{trace.txt,fdcwrite.log,fdc.log}`,
`debug/snapshot/stream_f005648_c0000000337417575/CRASH_f005926_c0000000354046551/`.
