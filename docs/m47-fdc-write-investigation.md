# M47 — DEF-M47-DISKWRITE root-cause investigation (DEC-0072)

**Status:** root cause CONFIRMED (read-only investigation, 2026-07-13). Fix pending (developer).

## Forensic signature (coordinator + investigation)
- `games/disks/ys2/d2-corrupted.dsk` vs `d2-pristine.dsk`: EXACTLY 3 sectors differ — LBA 9/15/16
  = CHS **(T0,S1,sec1)/(T0,S1,sec7)/(T0,S1,sec8)** — ALL track 0, side 1. 510/482/507 of 512 bytes wrong.
- Wrong data is COHERENT, NOT in pristine, NOT in d1.dsk (hot-swap ruled out), NOT a byte-shift of the
  pristine sector = genuine external SAVE data. Corrupt LBA 15 & 16 share their first 158 bytes then
  diverge (two adjacent save-slot records of one format; NOT a controller buffer artifact).
- SRAM saves byte-perfect → game feeds correct bytes → pure FDC disk-WRITE bug. `DiskImage` persist +
  multi-disk swap verified faithful; `read_chs`/`write_chs` addressing symmetric.

## Root cause (datasheet + openMSX + fMSX all concur)
**PRIMARY:** `Wd2793::write_data` (`wd2793.cpp:282`) commits a byte ONLY inside
`if (phase_==WriteSector && transfer_drq(t))`; the **else path silently DROPS the byte** (data_reg_
updated, but buffer_/data_index_/data_available_ do NOT advance). This violates the hardware invariant
that Write Sector lays down EXACTLY 512 in-order bytes, substituting `0x00`+LostData for a genuinely
un-serviced byte **while the position ALWAYS advances** (fact-sheet `FDC for Sony HB-F1XV.md` line 168;
openMSX `WD2793.cc:742-782` `if(dataRegWritten) dataOutReg=dataReg; else {dataOutReg=0; LOST_DATA;}`;
fMSX `WD1793.c:344-370` stores every write unconditionally + advances). Our drop-without-advance means
any non-strictly-polled CPU cadence (early write / 2-bytes-per-DRQ burst / ISR-perturbed / fixed-cadence
ahead of our rotational first-DRQ) shifts the fully-committed sector → sporadic YS II save corruption.
**M45 (DEF-M45-WRITEDRQ) correctly removed the OLD level-triggered zero-substitution but replaced
hardware-correct "un-serviced byte → 0x00 + ADVANCE" with "→ DROP, no advance"; the M45 test only
exercises strict wait-for-DRQ-before-every-byte, so the else path was never covered.** Same defect in
Write Track (`:304-315`).

**SECONDARY (H4):** write commits its target CHS at command FINISH using LIVE `physical_track_`/`side_`
(`finish_write_sector:552` → `DiskDrive::write_sector` `disk_drive.cpp:122`), vs read captures at START
(`begin_read_sector:452`). `track` is protected (validated at start; a mid-write seek changes phase_ so
the write never commits) but **`side_` floats** — a `0x7FFC` side-latch write mid-transfer redirects the
committed sector. All 3 corrupt sectors are side 1. Real HW/openMSX fix the CHS when the address mark is
under the head and the head can't move while BUSY.

REFUTED: H1 (DRQ status bit == `transfer_drq`, same predicate/time source — cannot disagree for a
poll-then-write CPU); H3 (buffer reset per sector; the 158-byte overlap is save-format structure).

## Fix (universal; models real WD2793; preserves M45/M45b) — the openMSX/datasheet model
1. **`write_data` LATCHES:** store value → `data_reg_`, set `data_reg_fresh_`. Do NOT gate on
   `transfer_drq`; do NOT advance position in `write_data`.
2. **Position advances on the DRQ cadence in `sync(t)`:** while `phase_==WriteSector && data_available_>0
   && t>=drq_deadline_`, commit ONE byte: if `data_reg_fresh_` → `buffer_[data_index_++]=data_reg_`,
   clear flag; else → `buffer_[data_index_++]=0x00; status_|=kLostData` (un-serviced-slot substitution).
   `--data_available_; drq_deadline_+=cycles_per_byte()`; at 0 → `finish_write_sector`. Same for Write
   Track. Guarantees EXACTLY 512 in-order bytes for EVERY cadence — early/late/burst/perturbed — no
   shift, no drop, no stall.
3. **PRESERVE M45/M45b:** keep the first-byte CHECK_WRITE gate (`sync()` abort when `data_index_==0 &&
   t>=write_check_deadline_` → write NOTHING) so a genuinely-absent first byte still aborts (no all-zero
   sector); once the first byte lands, mid-transfer misses zero-substitute+advance (NOT abort). Keep the
   rotational first-DRQ latency + fast-disk as a pure timing scale. Read path UNTOUCHED.
4. **SECONDARY fix (H4):** latch `(physical_track_, side_, write_sector_num_)` at `begin_write_sector`;
   `finish_write_sector` commits to the LATCHED coords (new drive write-at-CHS path or staged position),
   so a mid-transfer `0x7FFC` side write can't redirect the committed sector.

## Acceptance / repro (new `tests/unit/devices/fdc/wd2793_write_earlybyte_unit_test.cpp`)
Reuse the M45 Fixture/FakeClock/payload/verify harness. Cases:
- **A** burst (inter-byte < cycles_per_byte): pre-fix sector NOT byte-exact; post-fix exactly-512-in-order.
- **B** single injected early write at byte 100: pre-fix tail shifted; post-fix position-preserved.
- **C** multi-sector 0xB0 at YS II geometry (T0/S1 sec7,8 = LBA15,16): each lands byte-exact on the right LBA.
- **D** ALL-CHS sweep (every track/side/sector) under the adversarial cadence, read-back byte-perfect (DEC-0072 "byte-perfect across all CHS").
- **E** H4: toggle `set_side(1)` mid WriteSector to side 0 → commits to side 0 (latched), not side 1.
- Non-tautology: adversarial revert (restore the else-drop) must fail A–C.
Plus an opt-in non-perturbing per-byte FDC **write-trace** observer (mirror `FdcSectorReadObserver`) logging
each data write (value/committed-vs-substituted/data_index_/drq_deadline_/target LBA) + finish (CHS/LBA/CRC)
for live YS II confirmation.

## Open items (coordinator decisions — resolved)
- Implement the FULL openMSX model (not the fMSX-minimal alternative): strictly more correct, handles OTIR/burst.
- No correct-hardware save reference disk exists to diff against; the all-CHS byte-perfect proof + the live
  YS II re-save-and-load (human) are the acceptance gates.
- Assumption: Sony/Philips FDC data port has no CPU wait-states (openMSX PhilipsFDC + our bus agree); the
  decoupled model handles OTIR/burst regardless.
