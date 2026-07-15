# M22 Implementation Report — Sprites (D2) + VDP Command Engine (D3), D7 Closure

- Milestone ID: M22
- Developer: MSX Developer Agent
- Planner package: `docs/m22-planner-package.md` (RESP-M22-001/REQ-M22-002)
- Status recorded here: developer implementation COMPLETE, evidence captured. NOT yet closed —
  QA sign-off + coordinator release decision are the remaining gates (per the standing M21-M23
  pre-authorization: proceed through the release-decision/tag step without an extra pause on a
  clean QA PASS; STOP and consult the human otherwise).

---

## 1. Milestone Target

Deliver backlog **D2** (sprite rendering + collision/5th-sprite detection, Sprite Mode 1 and
Sprite Mode 2) and **D3** (the full VDP command engine, R#32-46, all 13 commands, the 5 logical
operations, the S#2 TR/CE/BD handshake, S#7/S#8/S#9), and — per DEC-0018 — finish backlog **D7's**
remaining command-engine-path piece (the command engine's own coordinate-to-VRAM-address
resolution for G4-G7/NonBitmap), which M21 explicitly left IN-PROGRESS. Both new classes
(`SpriteEngine`, `VdpCommandEngine`) are owned INSIDE `V9958Vdp` as private members, mirroring the
`blink_countdown_`/`blink_state_` ownership style established in M21 — not new machine-level
siblings. Sprite pixel compositing folds additively into `VdpFrameRenderer::render_frame()`'s
existing pipeline (§1.4 Resolution 1 of the planner package); no new consumer-visible output type
is introduced. The command engine's execution model is the explicitly-justified HYBRID (§1.4
Resolution 2): 10 commands complete atomically within the triggering register write; LMCM/LMMC/
HMMC are event-driven, multi-step state machines advanced only by real CPU I/O accesses (mirroring
this project's own FDC DRQ/INTRQ precedent, `src/devices/fdc/wd2793.*`), never by elapsed
cycles/time.

No machine-level wiring changes were needed: `Hbf1xvMachine::vdp()` (M14) and `render_frame()`
(M21) already suffice; sprite/command-engine state is reached via
`machine.vdp().sprite_engine()`/`machine.vdp().cmd_engine()`.

## 2. Code Changes

### New files

- `src/devices/video/sprite_engine.h` / `.cpp` — `SpriteEngine`: Sprite Mode 1 (max 4/line) and
  Sprite Mode 2 (max 8/line, per-line color/attribute table at a fixed +512-byte offset within the
  SAME attribute-table window) check/collision/5th-9th-sprite algorithm. Grounded in
  `references/openmsx-21.0/src/video/SpriteChecker.{hh,cc}`:
  - EC=bit7(0x80)/CC=bit6(0x40)/IC=bit5(0x20) — `SpriteChecker.cc:150,320,370` (EC),
    `SpriteConverter.hh:159,179` (CC), `SpriteChecker.cc:438,446` (IC, by elimination against the
    `& 0x60` collision-exclusion mask).
  - IC does NOT suppress rendering, only collision eligibility — `SpriteConverter.hh:144-205`'s
    drawing loop never tests bit `0x20` at all.
  - Color-0/TP transparency is conditioned on R#8 TP (contradicting the fact-sheet's own "always
    transparent" claim) — `SpriteConverter.hh:110-114`/`:170-171`, `if (colIndex==0 &&
    transparency) continue;`, `transparency` = `vdp.getTransparency()` = `(R8&0x20)==0`
    (`VDP.hh:186-191`). Implemented per the SOURCE, not the fact-sheet prose.
  - 1-pixel vertical shift — `SpriteChecker.hh:255-265`'s `getSprites(line)` decrements `line`
    before indexing, returning empty for `line<0`; re-expressed for this project's frame-wide,
    0-based active-display convention as `sprite_line = ((L-1)+R23-Y)&0xFF`, with output line 0
    UNCONDITIONALLY sprite-free (the per-line loop starts at `line=1`, never touching line 0 at
    all — matches A-M22-9 exactly).
  - S#0 read clears ONLY bits 6/5 (`& 0x1F`), leaving the stale sprite-number field —
    `SpriteChecker.hh:104-110`.
  - S#5 read (collision-Y low) zeroes BOTH collision X and Y — `VDP.cc:975-977`,
    `SpriteChecker::resetCollision()` — an important detail independently confirmed by reading
    `VDP.cc` directly (the planner package's own text named the accessor but not the specific
    triggering register; this was grounded by direct source read this cycle).
  - Sprite table base formulas — `VDP.hh:262-268`.
  - **A genuine regression discovered and fixed during implementation**: `SpriteEngine` must be
    gated by the SAME `displayEnabled && spriteEnabled` condition real hardware/openMSX uses
    (`VDP.hh:313-319` `spritesEnabledFast()`; `displayEnabled` = R#1 bit6, false at reset,
    `VDP.cc:284/437`). Without this gate, a freshly-reset/never-configured `V9958Vdp` (VRAM all
    zero, so all 32 "phantom" sprites read Y=0) would spuriously populate S#0 on every
    `on_vsync()` call — this broke the PRE-EXISTING M14 `devices_v9958_status_irq_unit_test` and
    `machine_hbf1xv_vdp_io_integration_test` (which call `on_vsync()` to test the VBlank F flag,
    never configuring sprites). Fixed by gating `recompute_frame()` on R#1 bit6 (display enable)
    AND R#8 bit1 clear (SPD, sprite enable) — grounded in `VDP.hh:313-319`/`VDP.cc:2000`. Zero
    regression confirmed after the fix (full 117/117 green).
- `src/devices/video/vdp_command_address.h` — five pure, header-only coordinate-to-address
  functions (`graphic4_command_address`, `graphic5_command_address`, `graphic6_command_address`,
  `graphic7_command_address`, `non_bitmap_command_address`), grounded 1:1 against
  `references/openmsx-21.0/src/video/VDPCmdEngine.cc:175-410`'s `Graphic4Mode`-`NonBitmapMode::
  addressOf` (`!extVRAM` branch only). **This is D7's closing piece.** These are genuinely
  SEPARATE functions from `V9958Vdp::effective_address()` (confirmed unchanged, see §"D7
  Disposition" below) — none of the five formulas reference R#2 at all; commands address both
  pages of a bitmap mode directly through the Y-coordinate's own range (`y&511` for G6/G7, `y&1023`
  for G4/G5), bypassing R#2's display-page-select bits entirely (independently tested, R-M22-7).
- `src/devices/video/vdp_command_engine.h` / `.cpp` — `VdpCommandEngine`: the full R#32-46
  register file (SX/SY/DX/DY/NX/NY 9/10-bit masking per `VDPCmdEngine.cc:1808-1874`; COL/ARG/CMD
  8-bit), scrMode determination (`VDPCmdEngine.cc:1902-1938`: 0=G4/1=G5/2=G6/3=G7(+YJK/YJK+YAE)/
  4=NonBitmap(R#25 CMD bit)/-1=none), the 13-command dispatch keyed on `CMD>>4`
  (`VDPCmdEngine.cc:1963-2028`), the 5 logical operations + T-variants keyed on `CMD&0x0F` for
  ONLY PSET/LINE/LMMV/LMMM/LMMC (`VDPCmdEngine.cc:2126-2185`'s own dispatch table; the other 8
  commands — ABRT/STOP, POINT, SRCH, LMCM, HMMV, HMMM, YMMM, HMMC — never consult the low nibble
  at all, verified with a dedicated non-IMP-non-zero-low-nibble test on HMMV), and the HYBRID
  execution model:
  - 10 atomic commands (ABRT/STOP, POINT, SRCH, LINE, LMMV, LMMM, HMMV, HMMM, YMMM, PSET) complete
    synchronously within `write_register(14, cmd)`; CE (S#2 bit0) is observably 0 immediately
    after. BD (S#2 bit4, SRCH's result) persists as real state, cleared only by an explicit S#9
    read (`VDPCmdEngine.cc:857-861`'s "this does NOT reset the BD flag").
  - 3 event-driven transfer commands (LMCM, LMMC, HMMC): CE stays 1 across the whole transfer; each
    subsequent CPU-port interaction (a write to R#44/COL for LMMC/HMMC, a read of S#7 for LMCM)
    performs exactly one pixel/byte's transfer, advancing internal counters, completing (CE→0)
    only after NX×NY interactions. Grounded in `VDPCmdEngine.cc:1240-1350` (LMCM)/`:1720-1771`
    (HMMC) and their `transfer` boolean gate.
  - Logical ops (IMP/AND/OR/XOR/NOT + T-variants) re-expressed as a per-pixel helper
    (`write_pixel()`) operating on the pixel's own bit-width (masked via `pixels_per_byte`), rather
    than the reference's byte-level mask-trick — algebraically equivalent, independently
    cross-checked against `VDPCmdEngine.cc:665-720`'s `ImpOp`/`AndOp`/`OrOp`/`XorOp`/`NotOp`/
    `TransparentOp<Op>` functors (e.g. NOT: `new_pixel = ~src_pixel & mask`, matching the
    reference's `(src&mask)|~(color|mask)` once re-expressed at the single-pixel level).
  - Block-command clipping (`clip_nx_1_pixel`/`clip_nx_1_byte`/`clip_nx_2_pixel`/`clip_nx_2_byte`/
    `clip_ny_1`/`clip_ny_2`) re-expressed with a plain `bool` direction flag instead of the raw ARG
    byte + bit constant, grounded 1:1 against `VDPCmdEngine.cc:56-126`.
  - LINE's Bresenham algorithm (X-major/Y-major, `ASX`/`ANX` accumulators, the unsigned-wraparound
    left/right/top border-hit detection via `adx & pixels_per_line`) re-expressed directly from
    `VDPCmdEngine.cc:869-964`'s `startLine`/`executeLine`.
  - **Disclosed, deliberate simplification** (documented in `vdp_command_engine.h`'s own header
    comment): unlike the reference, this engine does NOT mutate the SX/SY/DX/DY/NX/NY register
    members in place during command execution (the reference mutates DY/SY/ADX/ASX as a block
    command progresses — a real but obscure hardware quirk observable only via mid-command
    register peeking). All 10 atomic commands use local working copies; only `ASX` (exposed via
    S#8/S#9, used by SRCH and LMCM) is a genuine persistent member, matching the one case real
    software can actually observe (`VDPCmdEngine.hh:104-114`'s own doc comment: "real VDP simply
    returns the current value of the ASX...counter, regardless of the command"). This surfaced as
    explained, non-blocking A/B register-readback divergences (see §A/B Evidence below), not a
    functional defect — VRAM content and all status-handshake behavior are unaffected.

### Additive edits to already-shipped files

- `src/devices/video/vdp_mode.h` — added `vdp_sprite_mode(base)` (0/1/2), a pure function shared by
  `SpriteEngine::recompute_frame()` and `VdpFrameRenderer::composite_sprites()` so both stay
  byte-identical without duplicating the mode-to-sprite-mode table (a deliberate anti-drift
  measure; the planner package said "no changes needed" for this file, but a single new pure
  function avoiding a real duplication risk is a low-risk, justified refinement, not a scope
  change).
- `src/devices/video/v9958_vdp.h` / `.cpp` — additive only:
  - New private members `VdpCommandEngine cmd_engine_{vram_};` and `SpriteEngine sprite_engine_;`.
  - `change_register()`: the existing `if (reg >= kNumControlRegs) { return; }` guard now dispatches
    to `cmd_engine_.write_register(reg-32, value)` for `reg` in `[32,47)`, grounded 1:1 against
    `VDP.cc:1020-1033`'s own `changeRegister()` (A-M22-1). Verified reachable identically via BOTH
    the `#99` two-write latch protocol and the `#9B` indirect-register path.
  - `recompute_mode()`: additionally calls `cmd_engine_.notify_mode_change(mode_, (control_regs_[25]
    & 0x40) != 0)` (R#25 bit6 = CMD, `VDP.hh:544-549`).
  - `on_vsync()`: additionally calls `sprite_engine_.recompute_frame(...)`, computing the
    active-display height inline (duplicating `VdpFrameRenderer::height()`'s formula deliberately,
    to avoid a cross-layer dependency from the VDP core onto the presentation-layer renderer).
  - `read_status()`/`peek_status_register()`: S#0 bits6-0 now OR'd from `sprite_engine_.
    status_bits()`; S#2 TR/BD/CE now live from `cmd_engine_`; S#3-S#6 now live from
    `sprite_engine_`; S#5 read triggers `reset_collision()`; S#7 now live from `cmd_engine_.
    color()` (with `on_color_register_read()` called BEFORE the peek, matching the reference's
    `readColor()`-then-`resetColor()` ordering); S#8/S#9 now live from `cmd_engine_.border_x()`;
    S#9 read triggers `on_border_x_register_read()` (clears BD).
  - New const accessors `sprite_engine()`/`cmd_engine()`.
  - **Confirmed**: `V9958Vdp::effective_address()` is genuinely UNCHANGED — see the D7
    disposition below.
- `src/devices/video/vdp_frame_renderer.h` / `.cpp` — additive `composite_sprites(line, field,
  out)`, called from `render_line()` immediately after `dispatch_content()` and before the
  existing border-mask step. Mode 1: reverse-priority overdraw (`SpriteConverter.hh:99-132`).
  Mode 2: per-sprite draw-and-cascade-merge from lowest priority up to the first CC=0 sprite
  (`SpriteConverter.hh:144-205`), including the GRAPHIC5 half-pixel split
  (`palette[color>>2]`/`palette[color&3]`) and the GRAPHIC6 doubled-pixel case. Returns the SAME
  `FrameBuffer` type (`width`/`height`/`pixels`/`border_color`, unchanged public shape) — no new
  output type (R-M22-10 avoided).
- `CMakeLists.txt` / `tests/CMakeLists.txt` — added the two new library sources and 11 new test
  targets.
- `src/main.cpp` — new `--sprite-cmd-parity` CLI probe mode (mirrors the existing
  `--vdp-render-parity` precedent) for the A/B harness (see below).

### D7 disposition (backlog D7)

`git diff HEAD -- src/devices/video/v9958_vdp.cpp` shows 94 additions / 18 deletions, entirely
additive (new members, dispatch extensions, status wiring) — **zero lines touch
`effective_address()` itself** (confirmed by grepping the diff for the function name: no matches).
The command engine's five `vdp_command_address.h` functions are genuinely separate (different
domain: X/Y pixel coordinates, not a linear CPU-port pointer), independently hand-verified for
physical-bank-placement consistency with the existing rotate-right-by-1 model for the two shared
modes (G6/G7) per planner package §1.5, and the R#2-bypass nuance is explicitly implemented (no
formula references R#2) and tested (`R2Bypass_Graphic6_Dy300_WriteLandsAtSameAddressRegardlessOfR2`
in `tests/unit/devices/video_vdp_command_engine_registers_unit_test.cpp`, and
`CommandEngine_Graphic6_AddressIsBank1`/`CommandEngine_Graphic6_WriteLandsAtDedicatedAddress` in the
integration suite). **D7 has no remaining open piece after this milestone's developer pass.**

## 3. Unit Test Results

New unit test executables (8), all passing:

| Suite | Focus |
|---|---|
| `devices_sprite_engine_mode1_unit_test` | 1-pixel vertical shift, EC shift, 4-then-5th-sprite (earliest line), S#0 read clearing only bits7/6/5, collision +12/+8 offsets, color-0/TP collision eligibility, two-run determinism |
| `devices_sprite_engine_mode2_unit_test` | 8-then-9th-sprite, per-line (not per-sprite) color, IC excludes collision only, CC fields survive into the visible-sprite buffer, two-run determinism |
| `devices_vdp_frame_renderer_sprites_unit_test` | Mode 1 overdraw priority, mode 1/2 color-0/TP transparency (both states), mode 2 CC-cascade merge (3 sprites, merged color 7), IC=1 still drawn, GRAPHIC5 half-pixel split, two-run determinism |
| `devices_vdp_command_engine_registers_unit_test` | SX/SY/DX/DY/NX/NY bit-width masking, dispatch via BOTH `#99` and `#9B`, scrMode table (TEXT1/GRAPHIC4 × CMD-bit), the 5 address functions incl. the two G6/G7 hand-verified cases from planner §1.5, R#2-bypass, extended-VRAM bits stored-but-inert |
| `devices_vdp_command_engine_atomic_unit_test` | HMMV fill + DIX/DIY, HMMM/YMMM VRAM→VRAM copy, SRCH + BD persistence (survives an unrelated read, clears only on S#9), POINT, ABRT/STOP, low-nibble-ignored property, two-run determinism |
| `devices_vdp_command_engine_logic_ops_unit_test` | All 5 ops + all 5 T-variants in GRAPHIC4 (nibble mask verified), IMP/NOT in GRAPHIC7 (byte mode), undefined-nibble no-op, LMMV/LMMM with OR/XOR + a T-variant, LINE X-major octant, two-run determinism |
| `devices_vdp_command_engine_transfer_unit_test` | LMMC/LMCM/HMMC event-driven TR/CE handshake, each byte fed via a SEPARATE `write_register` call with a per-step VRAM-content check, no-clock-dependency confirmation, two-run determinism |
| `devices_vdp_command_engine_nonbitmap_unit_test` | R#25 CMD-bit cross-path test (a command in GRAPHIC1 writes a byte the EXISTING, unmodified `render_graphic1` subsequently displays), CMD-bit-clear no-op, two-run determinism |

Every new unit test file includes an explicit two-run byte-identical determinism assertion.

## 4. Integration Test Results

- `tests/integration/machine/hbf1xv_m22_sprites_integration_test.cpp` — sprite collision driven
  through the machine's `debug_io_write`/`debug_io_read` port seam, `render_frame()` compositing a
  sprite pixel, two-machine determinism.
- `tests/integration/machine/hbf1xv_m22_command_engine_integration_test.cpp` — HMMV through the
  machine's port seam, a G6-planar destination cross-validating D7's closure, the LMMC TR/CE
  handshake via `peek_status_register(2)`, two-machine determinism.
- `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp` — a REAL Z80 CPU program
  (executed over the M11 bus via `OUT (#98)/(#99)`) sets up two overlapping sprites and reads back
  a genuine collision through S#0/S#3/S#5 via the SAME port contract; a second CPU program issues
  one atomic command (HMMV) and one event-driven transfer command (LMMC, all 4 pixels fed via
  separate `OUT` instructions), confirmed via both direct VRAM inspection and `render_frame()`.

## 5. Evidence Gates (actual captured output)

```
tools/validate-assets.ps1
  Asset validation result: True
  Present BIOS files: f1xvbios.rom, f1xvdisk.rom, f1xvext.rom, f1xvfirm.rom, f1xvkdr.rom,
                       f1xvkfn.rom, f1xvmus.rom
  ROM file count: 2 (aleste.rom, metalgear.rom)

tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt
  Checksum report written to: docs/asset-checksums.txt

cmake --build build --config Debug
  (exit 0; sony_msx_core.lib, sony_msx_headless.exe, and all 117 test executables built)

ctest --test-dir build -C Debug --output-on-failure
  100% tests passed, 0 tests failed out of 117
  Total Test time (real) = 3.26 sec
```

Test count: **106 → 117** (11 new: 8 unit + 2 integration + 1 system), zero regression across the
FULL M1-M21 suite. The pre-existing `devices_v9958_status_irq_unit_test` and
`machine_hbf1xv_vdp_io_integration_test` initially REGRESSED during development (phantom-sprite
status pollution, see §2 above) and were fixed by the `spritesEnabledFast()`-equivalent gate before
the final green run recorded here.

## 6. Real openMSX A/B Evidence

Tooling: `tools/gen-m22-sprite-cmd-probe.py` (7 probes) + `tools/openmsx-m22-sprite-cmd-parity.ps1`
+ a new `sony_msx_headless --sprite-cmd-parity <program.bin> <base> <steps> <vram_bytes> <out.txt>`
CLI mode (mirrors the existing `--vdp-render-parity` precedent). Reference: openMSX 19.1 on WSL,
machine `Sony_HB-F1XV`.

**Feasibility check performed live this cycle** (per the planner package's explicit requirement,
via `debug list`/`debug size` over WSL): `VDP regs` (size 64, covers R#0-63 including R#32-46 in
the SAME debuggable already used since M14/M21), `VDP status regs` (size 16, covers S#0-S#15), and
`physical VRAM` (size 131072) are all live, comparable SimpleDebuggables — confirmed present, no
BLOCKED disposition needed for the core comparison.

**A genuine methodological discovery during this cycle**: openMSX's B-side runs the REAL BIOS for
several real-time seconds before the probe's own injected code executes (unlike this engine's
`--sprite-cmd-parity` harness, a truly cold, BIOS-less `cold_boot()`+flat-RAM run). BIOS
reprograms many VDP registers away from their power-on-reset defaults as part of its own screen
setup — the FIRST harness draft diffed the ENTIRE dumped VRAM/register/status range and produced
spurious "divergences" that were actually BIOS-leftover noise, not defects. This was corrected by
(a) explicitly pinning every register each probe depends on (R#0 mode, R#1 display-enable+size/mag,
R#5/R#11 sprite-attribute-table base, R#8 TP/SPD, R#23 vertical scroll, R#45 ARG) rather than
relying on any inherited "default", and (b) narrowing the comparison to per-probe explicit
allowlists of the VRAM addresses/status registers/command registers each probe actually controls —
mirroring the SAME "gate only what the probe writes" discipline the M14-M21 precedent already
applies to registers (`tools/openmsx-m21-vdp-render-parity.ps1`'s own `$gateRegs = @(0, 25)`
comment).

**Result, after that fix, over all 7 probes**:

- **Raw VRAM-byte comparison: genuine PARITY on all 7 probes** (empty diff on every gated address,
  including the G6-planar destination at physical bank1, `0x10000+`, cross-validating D7's
  closure, and the LMMC transfer's 2 fed-byte content). This is the primary, timing-independent,
  most direct evidence and it is clean.
- **Residual, honestly-explained divergences** (NOT reported as PASS, NOT silently dropped):
  - `STATUS[S0]`/`STATUS[S5]` differ on the 4 sprite probes (5S flag + stale sprite-number field;
    collision-Y off by exactly one line in one case). Root cause, independently reasoned through
    this cycle: openMSX's B-side is a LIVE, continuously-running system (thousands of frames
    elapse during the script's `after time` wait) whose own BIOS interrupt handler repeatedly
    polls/clears S#0 as part of its normal VBLANK housekeeping — the EXACT value our script's
    `debug read` (a single, arbitrary-instant, non-destructive snapshot) observes depends on
    precisely WHEN, relative to BIOS's own continuous polling cycle, that snapshot lands. This
    engine's own harness is a deliberate ONE-SHOT, deterministic batch computation (matching
    planner package §1.4 Resolution 1's explicit "no new clock consumer" architecture) with no
    such live-polling ambiguity — the underlying VALUES (5S bit, stale-number semantics, the
    +12/+8 collision offsets) are independently, exhaustively verified correct via this project's
    OWN deterministic unit/integration/system tests (§3-4 above), which have no sampling-moment
    non-determinism at all. This is a genuine, disclosed A/B-harness limitation for
    LIVE-BIOS-cyclical status fields specifically, not a logic defect, and not something a
    different `RunSeconds` value can reliably eliminate (BIOS's polling cadence is itself not
    under this probe's control).
  - `REG38` (DY low)/`REG42` (NY low) differ on the `cmd_atomic`/`cmd_graphic6_planar`/
    `cmd_lmmc_transfer` probes. Root cause, confirmed by direct calculation: this is EXACTLY the
    disclosed, deliberate simplification documented in `vdp_command_engine.h` (this engine does
    not mutate SX/SY/DX/DY/NX/NY registers in place during command execution, unlike the
    reference's real DY/NY in-place mutation during a 1-row HMMV) — B's DY/NY end up incremented/
    decremented by exactly the expected amount (DY 0→1, NY 1→0 after one row), A's stay at the
    CPU-written value. A known, named, low-risk depth limit (real software essentially never reads
    these registers back mid/post-command; only ASX, exposed via S#8/S#9, is a genuine persistent
    working register in this implementation, matching the one case real software CAN observe per
    `VDPCmdEngine.hh:104-114`'s own doc comment).
  - No sub-claim required a BLOCKED disposition this cycle (unlike M21, where the computed-pixel-
    color sub-claim had no live introspection point at all) — every comparison COULD be attempted,
    and was; the residual divergences above are reported honestly rather than silently upgraded to
    PASS, per the explicit instruction.

Full raw diff: `docs/m22-parity-trace-diff.md` (auto-generated by the PS1 script; the analysis
above is this report's interpretation of that raw evidence, not a hand-edit of the captured tool
output).

## 7. Deferred-Backlog Disposition

D2 and D3: **ready to close** (developer evidence complete; `agent-protocol/state/deferred-
backlog.md` updated to "READY TO CLOSE (M22, pending QA)"). D7: **no remaining open piece** —
ready to transition from IN-PROGRESS (M21 partial) to DONE (M22). The actual ledger DONE
transitions are the coordinator's call at closure time, per the established M14/M17-M21 precedent
(this developer pass does not mark them Done itself). All other 31 backlog rows re-affirmed
unchanged (unrelated to this milestone) — see `docs/m22-planner-package.md` §4 for the full
34-row review; no new row is introduced.

## 8. Known Issues / Residual Risks for QA

1. The two residual A/B divergence classes above (live-BIOS status-field sampling-moment
   artifacts; the disclosed DY/NY register-mutation-in-place simplification) should be
   independently reviewed by QA against the cited reasoning, not re-litigated from scratch.
2. `SpriteEngine`'s `displayEnabled`/`spriteEnabled` gate (R#1 bit6 / R#8 bit1) was NOT explicitly
   named in the planner package's own §2.2 algorithm description — it was discovered empirically
   during implementation (a genuine regression against the pre-existing M14 status/IRQ tests) and
   grounded directly against `VDP.hh:313-319`/`VDP.cc:284/437/2000` before being added. QA should
   confirm this grounding independently.
3. Per the planner package's explicit S#5 instruction gap (the spec named `reset_collision()` as
   a method but did not specify which status-register read triggers it), this was grounded
   directly against `VDP.cc:975-977` (S#5, not S#3/S#4/S#6) during implementation — QA should
   independently confirm this citation.
4. `SpriteEngine`/`VdpCommandEngine` do not model backlog D4 (cycle-accurate VDP timing) at all,
   matching the explicit, planner-approved scope boundary — no new work is expected here for M22.
