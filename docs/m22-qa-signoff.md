# M22 QA Sign-off — Sprite Rendering + Collision Detection, VDP Command Engine, D7 Closure

- Milestone ID: M22
- QA Owner: MSX QA Agent
- Inputs reviewed: `docs/m22-planner-package.md` (993 lines, read in full), `docs/m22-implementation-report.md`
  (read in full), `docs/m22-parity-trace-diff.md` (read in full, line-by-line), `agent-protocol/state/deferred-backlog.md`
  (read in full), source under `src/devices/video/{sprite_engine,vdp_command_engine,vdp_command_address.h,
  vdp_frame_renderer,v9958_vdp}.*`, all 8 new unit test files, both new integration tests, the new system test,
  and the corresponding sections of `references/openmsx-21.0/src/video/{SpriteChecker.cc,SpriteConverter.hh,
  VDPCmdEngine.cc,VDPCmdEngine.hh,VDP.cc,VDP.hh}`.

---

## 1. Regression Scope

M22 adds two new device-internal classes owned inside `V9958Vdp` (`SpriteEngine`, `VdpCommandEngine`), a new
header-only coordinate-address module (`vdp_command_address.h`), and additive extensions to `VdpFrameRenderer`
(sprite compositing) and `V9958Vdp` (register dispatch to R#32-46, status-register live wiring for S#0/S#2/
S#3-S#9). No machine-level wiring, no CPU/Z80A core changes, no memory/slot changes, no FDC/PSG/YM2413/RTC/
peripheral changes. Regression-relevant surfaces:

- `src/devices/video/v9958_vdp.{h,cpp}` — `change_register()`, `read_status()`, `peek_status_register()`,
  `on_vsync()`, `recompute_mode()`, `reset()`, new accessors. **Risk of touching `effective_address()`
  (D7's CPU-port piece, already closed at M21) or the R#0-31 register file.**
- `src/devices/video/vdp_frame_renderer.{h,cpp}` — new `composite_sprites()` call site inside `render_line()`.
  **Risk of altering existing background-rendering output for modes with no sprites configured.**
- Pre-existing M14 status/IRQ tests and M14/M21 VRAM-pointer/planar-transform tests must remain green,
  unmodified, since M22 explicitly claims "zero CPU-visible timing change" and "zero edits to
  `effective_address()`."
- Full M1-M21 suite (106 tests) plus 11 new M22 tests (8 unit + 2 integration + 1 system) = 117 total.

## 2. Regression Matrix Status

| Area | Status | Evidence |
|---|---|---|
| Full historical suite (M1-M21, 106 tests) | PASS | Independently re-built and re-run this cycle (below) |
| New M22 tests (11) | PASS | Same run |
| `effective_address()` untouched (D7 CPU-port piece) | CONFIRMED | `git diff v1.0.21 -- src/devices/video/v9958_vdp.cpp` shows 93 insertions/17 deletions (94/18 incl. diff header lines, matching the developer's claim), zero mention of `effective_address` anywhere in the diff |
| `V9958Vdp::kNumControlRegs` unchanged (A-M22-2) | CONFIRMED | `change_register()` diff shows the R#32-46 dispatch is an ADDITION inside the existing `if (reg >= kNumControlRegs)` branch, not a widening of the control-register array |
| Sprite-enable regression (self-caught by developer) | CONFIRMED FIXED, GROUNDING VERIFIED | `spriteEnabled = (controlRegs[8] & 0x02) == 0` (`VDP.cc:446`), `displayEnabled = (controlRegs[1] & 0x40) != 0` (`VDP.cc:437`) — both formulas match `sprite_engine.cpp`'s gate exactly; pre-existing `devices_v9958_status_irq_unit_test`/`machine_hbf1xv_vdp_io_integration_test` pass in the fresh run |
| Determinism (no clock/cycle consumer) | CONFIRMED | `grep` for `elapsed_cycles\|clock\|EmuTime\|cycles_` in `sprite_engine.cpp`/`vdp_command_engine.cpp` returns nothing except an unrelated comment ("EC: early clock"); dedicated no-clock-dependency test passes |
| Five command-address functions vs. reference | BYTE-EXACT MATCH | `vdp_command_address.h` vs. `VDPCmdEngine.cc:175-410`'s five `addressOf(x,y,false)` bodies — compared line-by-line, identical formulas |
| EC/CC/IC bit positions (A-M22-10) | CONFIRMED | `SpriteChecker.cc:150` (`colorAttrib & 0x80`, EC), `SpriteConverter.hh:159/179` (`& 0x40`, CC), `SpriteChecker.cc:438/446` (`& 0x60` exclusion mask, IC by elimination) |
| IC-does-not-suppress-rendering (A-M22-11) | CONFIRMED, dual-test | `SpriteConverter.hh:144-205` never tests bit `0x20`; project's own `Mode2_IC_ExcludesCollision` (collision half) + `Mode2_IC_StillDrawn` (rendering half) both present and pass |
| Color-0/TP source-vs-fact-sheet resolution (A-M22-12) | CONFIRMED | `SpriteConverter.hh:110-114/170-171` conditions on `transparency`; implementation matches; both TP-enabled/disabled tests pass in both sprite modes |
| S#0 read clears only bits7/6/5 (A-M22-14) | CONFIRMED | `SpriteChecker.hh:104-110`'s `& 0x1F`; dedicated test confirms stale-number survival |
| S#5 (not S#3/4/6) triggers `reset_collision()` | CONFIRMED | `VDP.cc:975-977`: `case 5: spriteChecker->resetCollision(); break;` |
| R#2-bypass for command addressing (R-M22-7) | CONFIRMED | None of the 5 address formulas reference R#2; dedicated test drives DY=300 (page1) with R#2 forced to page0 and page1, same physical address both times |
| Full 34-row deferred-backlog review | CONFIRMED CONSISTENT | All 34 rows present and correctly re-affirmed in `agent-protocol/state/deferred-backlog.md`; D2/D3/D7 correctly show "READY TO CLOSE (M22, pending QA)", not yet force-closed |
| A/B evidence (see §5 discussion) | CONDITIONAL — see below | Raw VRAM-byte claim holds; STATUS-divergence narrative is directionally correct but not fully rigorous in its digit-level explanation (non-blocking) |

## 3. Independent Verification Performed

**Full rebuild + full suite (fresh, this cycle):**

```
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF
cmake --build build --config Debug        (exit 0, all targets including sony_msx_headless.exe built)
ctest --test-dir build -C Debug --output-on-failure
  100% tests passed, 0 tests failed out of 117
  Total Test time (real) = 2.93 sec
```

This matches the developer's claimed 117/117 (106 prior + 11 new) exactly — independently reproduced, not
rubber-stamped. `tools/validate-assets.ps1` (run via `powershell.exe`) confirmed `Asset validation result: True`
with the same 7 BIOS files + 2 ROM files (`aleste.rom`, `metalgear.rom`) the report claims. `docs/asset-checksums.txt`
is present and shows a fresh modification versus the M21 tag (`git diff --stat v1.0.21`).

**Byte-exact semantics cross-check (read directly, not from memory):**

- `references/openmsx-21.0/src/video/SpriteChecker.cc:150` — `if (colorAttrib & 0x80) sip.x -= 32;` confirmed
  (EC = bit7), also present at the mode-2 planar/non-planar call sites (:320/:370 region).
- `SpriteChecker.cc:438,446` — `if (colorAttrib1 & 0x60) continue;` / `if (colorAttrib2 & 0x60) continue;`
  confirmed (IC/CC combined exclusion mask, by elimination IC = bit5).
- `SpriteConverter.hh:110-114,170-171` — `if (colIndex == 0 && transparency) continue;` confirmed, including
  the source's own "Verified on real V9958: TP bit also has effect in sprite mode 1" comment at line 112-113.
  `SpriteConverter.hh:159,179` — `colorAttrib & 0x40` confirmed (CC).
- `VDPCmdEngine.cc:175-410`'s five `Graphic4Mode`/`Graphic5Mode`/`Graphic6Mode`/`Graphic7Mode`/`NonBitmapMode::
  addressOf(x, y, false)` bodies were read and compared term-for-term against `vdp_command_address.h`'s five
  functions — **identical**, including the non-obvious G6 bank-select-bit-is-`x&2` (not `x&1`) detail. None of
  the five reference formulas (nor the project's five) reference R#2 anywhere.
- `references/openmsx-21.0/src/video/VDP.hh:299-319` (`isDisplayEnabled`, `spritesEnabled`, `spritesEnabledFast`)
  and `VDP.cc:437,446` (`displayEnabled`/`spriteEnabled` assignment from `controlRegs[1]&0x40`/`controlRegs[8]&0x02`)
  were read directly and confirmed to match the self-caught regression fix's gate exactly.
- `VDPCmdEngine.cc:869-964` (`startLine`/`executeLine`, the Bresenham state machine) was read and hand-traced
  against `run_line()` — the X-major and Y-major branches' ANX/ASX/ADX/DY update order and the unsigned-wraparound
  border-hit test (`adx & pixels_per_line`) match exactly.
- `VDPCmdEngine.cc:1094-1153` (`startLmmm`/`executeLmmm`) confirms the reference genuinely mutates the shared
  `ASX` class member (not a local variable) during LMMM — see §4 residual finding below.

**`effective_address()` confinement:**

```
git diff v1.0.21 -- src/devices/video/v9958_vdp.cpp   → 93 insertions(+), 17 deletions(-)
grep -n "effective_address" <that diff>               → no matches
```

Confirmed genuinely additive; the command engine's addressing is independently implemented, not routed through
`effective_address()`.

**IC dual-test requirement:** confirmed satisfied — `tests/unit/devices/video_sprite_engine_mode2_unit_test.cpp`'s
`Mode2_IC_ExcludesCollision` proves the collision-exclusion half; `tests/unit/devices/
video_vdp_frame_renderer_sprites_unit_test.cpp`'s `Mode2_IC_StillDrawn` proves the rendering half. Together they
cover both halves of A-M22-11, split across the two modules that respectively own each behavior — an acceptable
test organization, not an incomplete verification.

## 4. Failures and Risk Ranking

No blocking defects found. Two non-blocking residual findings, independently discovered (beyond what the
implementation report disclosed):

**(Low severity, non-blocking) Finding 1 — ASX (S#8/S#9) is not persisted across LINE/LMMM/HMMM, a broader
scope than the report's own disclosure states.** The implementation report's header comment in
`vdp_command_engine.h` states ASX is "a genuine persistent member... used by SRCH and LMCM," implying the
scope of the disclosed "no in-place register mutation" simplification is limited to SX/SY/DX/DY/NX/NY. Reading
`VDPCmdEngine.cc:1101` (`ASX = SX;`) and `:1148` (`ASX += TX;`) confirms the reference's LMMM (and, by the same
pattern, HMMM) command **also** genuinely mutates the shared `ASX` member — meaning real hardware/openMSX would
show LMMM/HMMM's own final ASX value if S#8/S#9 were polled afterward. This project's `run_lmmm()`/`run_hmmm()`/
`run_line()` all use a **local** `asx` variable that is never written back to the persistent `asx_` member, so
S#8/S#9 would show a stale value (from a prior SRCH/LMCM/reset) after any of these three commands. This is the
same *category* of simplification already disclosed (working registers not fully persisted), just broader in
actual scope than the report's own wording suggests. No acceptance criterion or risk item (§5/§6 of the planner
package) requires ASX to reflect LINE/LMMM/HMMM specifically — the only named requirement ties S#8/S#9 to "SRCH
result" (§2.1's file table) — so this is a legitimate, in-scope depth limit, not a violation. **Recommended
follow-up (non-blocking):** broaden the disclosure comment to name all three commands explicitly, so a future
milestone doesn't rediscover this gap as a "regression."

**(Low severity, non-blocking) Finding 2 — S#7/S#8/S#9 are silently excluded from every A/B probe's comparison
gate, not just narrowed per-probe.** `docs/m22-parity-trace-diff.md`'s own preamble states the gate is "raw VRAM
bytes..., S#0/S#2/S#3-S#6 status bytes, and R#0 (+R#32-46...)" — S#1/S#7/S#8/S#9 are never compared in ANY probe.
The raw per-probe status dumps show S#8 diverging sharply on 2 of 3 command-engine probes (`cmd_graphic6_planar`:
A=00/B=D2; `cmd_lmmc_transfer`: A=00/B=D2) that are never surfaced in the "Diff" section or in the implementation
report's narrative (which only discusses S#0/S#5/REG38/REG42). This is disclosed in the raw file's own header
(a careful reader who reads the file, as instructed, catches it) but is not called out in the higher-level
report discussion. Given AC10 never requires S#8/S#9 comparison specifically, and given Finding 1 above already
explains why this project's S#8/S#9 would legitimately differ post-LMMV/LINE/HMMV-family commands, this is a
plausible, low-risk consequence of the same disclosed simplification — but the report should have named it
rather than leaving it only implicit in the raw dump.

## 5. Independent Assessment of the A/B Divergence Framing (the critical-scrutiny item)

I read every line of `docs/m22-parity-trace-diff.md` directly, not the implementation report's summary of it.

**(a) Do the Diff sections ever show a VRAM address/byte line?** No. All 7 probes' "Diff" sections show only
`STATUS[Sx]` or `REG38`/`REG42` lines — never a VRAM byte. This part of the report's framing is accurate: the
narrower "raw VRAM-byte parity" claim is not contradicted anywhere in the raw file.

**(b) STATUS[S0]/STATUS[S5] divergences — do the actual numbers support the report's causal story?** I decoded
these by hand rather than accepting the narrative:

- `sprite_mode1_collision`/`sprite_tp_color0`: S5 A=0x09, B=0x08 (collision-Y off by exactly one line — line=1
  vs line=0). This is fully consistent with a display-timing reference-point difference of one line between the
  two engines' definitions of "line zero" (this project's own, deliberately modeled A-M22-9 1-pixel-shift
  convention). I consider this specific divergence genuinely benign and well-explained.
- `sprite_mode1_fifth`: S0 A=0xC4 (F=1, 5S=1, C=0, number=4), B=0x85 (F=1, 5S=0, C=0, number=5). The report's
  explanation ("B already read/cleared S#0 once, stale number survives per A-M22-14") does **not** arithmetically
  hold up: A-M22-14 explicitly establishes that an S#0 read clears only bits7/6/5, leaving the stale
  sprite-number field (bits4-0) **unchanged**. If B's underlying computation had genuinely matched A's (5th
  sprite = index 4) and merely had bits7/6/5 cleared by an intervening BIOS read, B's bits4-0 would still show
  4, not 5. The fact that B shows a **different** number (5, not 4) means B's status does not reflect the
  same computed outcome as A's at all — not just a "bits cleared" difference.
- `sprite_mode2_ninth_ic`: S0 A=0xE8 (9S=1, number=8), B=0xBF (9S=0 — **no** ninth-sprite condition detected at
  all —, number=31=`min(sprite,31)`, i.e., B processed all 32 sprites without ever hitting the Y=216 sentinel).
  This is not a "bits cleared" pattern either; it looks like B's snapshot reflects a **completely different
  sprite-table state** than the probe's own injected test data (most consistent with B still showing BIOS's own
  leftover/default VRAM content, or a scenario where openMSX's own per-scanline `SpriteChecker` had not yet
  processed the freshly-written test sprites by the time the snapshot was taken).

  **My independent conclusion:** the report's specific "S#0-read-clears-bits, stale-number-persists" narrative
  is not a fully accurate characterization of what is actually shown in the raw numbers for the 5th/9th-sprite
  probes — a more accurate framing would be "B's snapshot likely does not reflect having processed the identical
  injected test scenario at all, given the disclosed cold-boot-vs-live-BIOS architecture of this A/B harness."
  That said, I do **not** believe this points to a logic defect in this project's own `SpriteEngine`: I
  independently, line-by-line compared `sprite_engine.cpp`'s check/collision/5th-sprite algorithm against
  `SpriteChecker.cc:44-241` (mode 1) and `:262-479` (mode 2) and found it to be a faithful, byte-exact
  re-derivation (identical loop structure, identical `(status&0xC0)==0`-equivalent gating reasoning, identical
  `+12`/`+8` collision offsets, identical `can0collide`/`canSpriteColor0Collide()` formula). Combined with the
  full, independently-reproduced pass of every dedicated unit/integration/system test that exercises these exact
  scenarios deterministically (5th-sprite-at-earliest-line, 9th-sprite, S#0 stale-field survival, IC-exclusion),
  I conclude the underlying **conclusion** ("no code defect, a harness/methodology artifact") is correct, but
  the report's **narrative mechanism** for why is imprecise and should be corrected in the ledger for accuracy
  (non-blocking; see Finding 2's twin issue above).

**(c) Was in-place SX/SY/DX/DY/NX/NY (or ASX beyond SRCH/LMCM) mutation ever a stated acceptance criterion?**
I re-read planner package §2.3 and all 13 items of §5 directly. §2.3 only specifies the register file's storage/
bit-width/dispatch/scrMode/address-resolution/execution-model contract; it never states registers must reflect
live, observable in-place mutation as an externally-visible side effect. §5's 13 acceptance criteria likewise
never mention this. The only S#8/S#9-adjacent commitment is AC4's "S#7/S#8/S#9 border-X (SRCH result)" — tying
S#8/S#9 to the SRCH use case specifically, not a general per-command guarantee. Treating the DY/NY/ASX
non-mutation as an out-of-scope depth limit (not a defect) is therefore a reasonable, textually-grounded reading
of the actual planner text, not merely the implementation report's paraphrase of it.

**(d) Overall verdict on the "PARITY on all 7 probes" framing:** the **narrow, explicitly-scoped** claim ("raw
VRAM-byte comparison: genuine PARITY on all 7 probes") is accurate and independently verifiable — no VRAM byte
divergence appears anywhere in the raw file. The **broader narrative** explaining the residual STATUS
divergences is directionally correct in its top-level conclusion (these are harness/live-BIOS artifacts, not
code defects) but overstates the precision of its own causal mechanism for the 5th/9th-sprite S#0 cases
specifically, and omits the S#8/S#9 divergence from its discussion entirely. I do **not** consider this
overstated to the point of being misleading about release readiness — the underlying VRAM-write and
deterministic-test evidence independently and separately supports correctness — but I recommend the ledger
entry be corrected to soften the specific "stale-field-survives-a-read" claim to the more defensible "harness
cannot guarantee the B-side snapshot reflects the identical injected scenario" framing, as a non-blocking
follow-up item.

## 6. Required Fixes

None required before sign-off. Two non-blocking follow-ups recorded (§4, Findings 1-2) — recommended for a
documentation-only correction in a future cycle, not a code change, and not a gate on this milestone's closure.

## 7. Sign-off Decision: **Pass**

Rationale: the full regression suite (117/117, independently rebuilt and re-run) is green with zero regression
against the M1-M21 baseline; every named risk item (R-M22-1 through R-M22-13) has a corresponding, passing,
independently-inspected test; the D7-closing address functions are byte-exact against the reference source;
`effective_address()` is confirmed genuinely untouched; the self-caught sprite-enable-gate regression fix is
correctly grounded; the deferred-backlog's full 34-row review is consistent and D2/D3/D7 are correctly left at
"READY TO CLOSE (pending QA)" rather than force-closed; and the A/B evidence's core, load-bearing claim (raw
VRAM-byte parity) survives independent scrutiny. The two non-blocking findings above do not rise to
blocker-level gaps — they are documentation-precision issues in the A/B narrative and a scope-note completeness
gap in a header comment, neither of which affects VRAM content correctness, status-handshake correctness, or
any acceptance criterion. D2, D3, and D7 (in full) are ready to close as M22; the coordinator may proceed to the
release-decision/tag step per the standing M21-M23 pre-authorization.
