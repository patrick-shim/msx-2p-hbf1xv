# M39 / v1.0.39 QA Sign-off — Independent Regression Assessment

- Date: 2026-07-11
- Candidate: v1.0.39 (all commits on `main` since v1.0.38 = `2e464b7`; HEAD = `53e4eaf`).
- Build tree: the ONE canonical `build/` (SDL3=ON), clean rebuild (`--clean-first`).
- Verifier: msx-qa (read-only verification; NO source mutation performed — tree byte-clean
  throughout, HEAD stayed at `53e4eaf`, only `debug/` artifacts written which are gitignored).
- Supersedes the earlier draft of this file, which predated the border revert (`73968d7`) and the
  raster split-boundary fix (`53e4eaf`).

Commits under test (oldest→newest):
- `ed419cd` docs (no code) / `e825b9c` style: tree-wide comment tidy (ZERO logic) / `71b71e1` chore
  untrack local dirs / `09d12b5` docs README — non-fix range members.
- `b0db02b` fix(vdp): V9958 bitmap horizontal scroll (R#27 fine + multi-page) — already validated
  (Laydock live + F1 replay); "confirm NOT regressed".
- `2405a35` fix(audio): PSG sync-before-change (digitized voice) + additive 1-bit ClickDac —
  HUMAN-CONFIRMED working live.
- `7fac03d` fix(video): openMSX-matching border framing default — then REVERTED as default by
  `73968d7`.
- `73968d7` revert(video): default present back to bare edge-to-edge (Sony original); `--border`
  opt-in.
- `53e4eaf` **fix(video): raster split-boundary L+1→L+2** — the special-scrutiny commit.

## 1. Regression Scope

Affected subsystems: VDP frame renderer + horizontal scroll (`b0db02b`); the machine render-sync
seam that governs EVERY cycle-timed mid-frame VDP write (`53e4eaf`, `hbf1xv_machine.cpp` only);
audio mixer + PSG + new ClickDac + PPI (`2405a35`); SDL3 presentation/CLI vertical framing
(`7fac03d`/`73968d7`, frontend only). CPU/core deliberately untouched by every fix. Asset-dependent
flows (disk boot, FM-PAC SRAM) adjacent but unmodified.

The `53e4eaf` render-sync constant is the highest-blast-radius change: `sync_to_line(line+1)` →
`(line+2)` in `Hbf1xvMachine::VdpRenderSyncAdapter::on_before_state_change()` shifts the commit
boundary of ALL mid-frame R#23 scroll splits, R#1 BL bands, and R#7/palette raster bars for every
game. It is therefore the focus of this assessment.

## 2. Regression Matrix Status

| Gate | Result | Evidence |
|---|---|---|
| Clean rebuild `cmake --build build --config Debug --clean-first` | PASS (exit 0, 0 errors) | both exes rebuilt 09:08; `sony_msx_headless.exe` 2,154,496 B / `sony_msx_sdl3.exe` 2,072,576 B |
| Full suite `ctest -E hbf1xv_m24_zexall_system_test --output-on-failure` | **PASS 213/213, 0 failed, 72.07 s** | 214 tests total; #128 ZEXALL excluded per gate; captured run |
| Scrutiny tests re-run in isolation | PASS 14/14, 0 failed | #139/#149/#150/#152/#153/#183/#185/#186/#187/#191/#192/#193 all Passed |
| cpu/core behaviorally-empty (ZEXALL withhold safe) | PASS (behaviorally) | see §2.1 — only diff is comment-only `e825b9c` |
| M38 horizontal-scroll dumps sha256-identical pre/post `53e4eaf` | **PASS (independently regenerated)** | §2.2 |
| M32 per-line-latch + line-interrupt + split-screen | PASS | #185/#186/#187; `raster_display_line()` provably unchanged (§2.3) |
| M34 BL-latch oracle re-derived L+1→L+2, gating preserved | PASS | #192/#193; §3 M34 assessment |
| Voice idle-byte-identity (M29/M31/M34/M37 audio oracles) | PASS unchanged | all audio oracles green in 213/213; #149/#150/#152/#153/#168/#169 |
| Border revert oracle (default false, `--border` true, `--no-border` false) | PASS | `sdl3_cli_unit_test` #139; §2.4 |
| Metal Gear boots + reaches gameplay + BL blank works | PASS (headless) | §3 — SHA1-matched Konami cart; gameplay HUD complete; room-transition blanks fully |

### 2.1 cpu/core-empty proof

`git diff --stat 2e464b7..HEAD -- src/devices/cpu src/core` = only `src/devices/cpu/z80a_cpu.cpp`
(+20/−25); `src/core` = zero change. `git log 2e464b7..HEAD -- src/devices/cpu src/core` shows the
SOLE touching commit is `e825b9c "style: tidy source comments (ZERO logic change)"`. I filtered the
z80a diff: EVERY added/removed line is a `//` comment-prose line (the HALT phantom-M1 note and the
LD A,I/R P/V-bug note were reworded; `increment_refresh_register()`, `tstates = 4`, and the P/V flag
logic are byte-identical). No fix commit (`b0db02b`/`2405a35`/`7fac03d`/`73968d7`/`53e4eaf`) touches
cpu/core at all. The behavioral invariant the ZEXALL-withhold rests on HOLDS. The literal diff being
non-empty is a comment-tidy staging artifact, not a defect, and not Fail-level. (Optional
belt-and-suspenders: one ZEXALL sweep to close the memory-rule "direct cpu edit → sweep" loop; not
required since the edit is provably comment-only and the full sweep already ran after `e825b9c`.)

### 2.2 M38 no-regression — independently verified byte-identity

I regenerated two M38 scroll frame dumps with the CURRENT post-fix HEAD (`53e4eaf`) from the same
scenario carts and compared sha256 against the pre-fix reference dumps (captured 00:15, before the
08:58 fix):
- `s01_g4_baseline`: `a362aa83…ea36` (post-fix) == `a362aa83…ea36` (pre-fix) — MATCH.
- `s06_g4_multipage` (the R#27 multi-page target): `b37739c1…62b8` == `b37739c1…62b8` — MATCH.

The developer's own `debug/m39-hud/m38recheck/` dumps carry the identical hashes. M38 scroll frames
are static at the dump point (scroll registers set during setup, no mid-active-display writes to
shift), so the L+2 seam change has provably ZERO effect on scroll output. The M38 A/B report stands:
s01–s08/s10/s11 MATCH (0% mismatch); `s09_yjk_baseline` is the pre-existing YJK residual (6.25%),
a MISMATCH inherited from M38's own report — NOT a scroll scenario and NOT a v1.0.39 regression.

### 2.3 M32 no-regression — line-interrupt path untouched

`git diff 2e464b7..HEAD -- src/devices/video/v9958_vdp.cpp` contains ZERO changes to
`raster_display_line()` (grep of the diff for that symbol is empty; the only v9958 change is the
`e825b9c` comment tidy, non-comment lines empty). So line-interrupt matching (R#19 coincidence, S#2
VR/HR) is unchanged, mirroring openMSX where the render-sync rounding is independent of the VDP
line-interrupt counter. `hbf1xv_m32_split_screen_system_test` (#187) passes; its ±4-line tolerance
window (`boundary >= kSplitLine && boundary < kSplitLine+4`) absorbs the deliberate 1-line boundary
shift. `hbf1xv_m32_line_interrupt_integration_test` (#186) and the re-derived L+2 per-line-latch
oracle (#185) pass.

### 2.4 Border revert oracle — deliberate preference revert, not a weakening

`sdl3_cli.h` default `border_enabled = false` (bare edge-to-edge Sony-original); `--border` opts IN
(true); `--no-border` explicit false; last-wins on the linear scan. `sdl3_cli_unit_test`
(#139) asserts: `NoFlags_BorderDefaultsFalse_BareEdgeToEdgeSonyOriginal` (line 79),
`BorderFlag_OptsInToFramedCanvas_SetsTrue` (115), `NoBorderFlag_SetsFalse` (120), and both last-wins
orders (125–128). Presentation-only; the headless render/dump path is untouched. This is the
coordinator/owner's stated preference revert of `7fac03d`, correctly reflected in the oracle.

## 3. Per-commit verdicts

### `b0db02b` scroll — PASS (not regressed)
`vdp_frame_renderer.{cpp,h}` had ZERO commits after `b0db02b` (`git log b0db02b..HEAD -- …` empty) —
voice/vertical/revert/raster do not touch it. §2.2 proves the M38 output is byte-identical post-raster
-fix. Already validated live (Laydock + F1 replay) and by the M38 A/B (0% mismatch on all scroll
scenarios). No new risk in v1.0.39.

### `2405a35` voice — PASS (mechanism proven; real-time playback HUMAN-CONFIRMED)
Additive, cpu/core-free. Idle-byte-identity holds three ways (null ClickDac integrates to exactly 0;
5-source mixer overload delegates byte-identically with `nullptr` click; PSG sync gated on
`audio_sync_enabled_`+non-null cycle source, both default off). New tests non-vacuous:
`click_dac` (#149), `psg_sync_before_change` (#150), `machine_audio_mixer` (#152),
`machine_audio_mixer_click` (#153) all pass; all pre-existing audio oracles pass unchanged in the
213/213 run. Per the task, live playback is already HUMAN-CONFIRMED working — residual risk cleared.

### `7fac03d` + `73968d7` vertical/border — PASS (structural; visual preference is a human call)
`7fac03d` touched `src/frontend/` only; `73968d7` reverts the default to bare edge-to-edge. Net:
render/device path untouched (§2.2 byte-identity is the structural guarantee). Oracle correctly
tracks the revert (§2.4). The out-of-box appearance is a preference the human confirms (checklist b).

### `53e4eaf` raster split-boundary L+1→L+2 — PASS (openMSX-grounded, strengthened not weakened)
Diff is `hbf1xv_machine.cpp` ONLY (+40/−11): the render-sync commit boundary
`sync_to_line(line+1)` → `(line+2)`. `raster_display_line()` and the mid-display memoization sync are
intentionally untouched. openMSX grounding VERIFIED by reading the concrete files:
- `references/openmsx-21.0/src/video/PixelRenderer.cc:566-567` — the LINE-accuracy renderer rounds
  the commit with the active-display left margin: `limitY = (limitTicks + TICKS_PER_LINE - 400) /
  TICKS_PER_LINE`. Our raw `raster_display_line()` (floor of tstates/line, quantized at scanline
  START, no left-margin term) lands one display line earlier than this rounded commit — hence L+2,
  not L+1, matches openMSX for a typical mid-line write. Confirmed present as claimed.
- `PixelRenderer.cc:608-611` — with display disabled the whole line renders `DRAW_BORDER`
  (BL=0 blanks the line). Confirmed.
- `VDP.cc:1080-1082` (R#1 bit6 change → `syncAtNextLine(syncSetBlank)`) and `:1260-1269`
  (`syncAtNextLine`) — BL state flips at the next-line boundary. Confirmed.

Calibration is EMPIRICAL against openMSX 19.1 raw Sony_HB-F1XV captures
(`debug/m39-hud/omsx_hud_00..03.png`, `g2610_fixed*.png`): Aleste-2's three per-frame writes at
in-line tstates 44/98/169 all show a uniform +1-line delta, landing the split exactly on openMSX's
scanline (HUD glyph bottom row restored; blank band = display lines 15-16, matching openMSX). This is
the strongest possible grounding — direct A/B against openMSX raw frames.

## 3.1 M34 / Metal Gear assessment (the key regression risk)

**(a) Is the M34 BL oracle re-derivation openMSX-grounded, and does it PRESERVE BL-gating
(strengthened, not weakened)?** YES.
- The re-derived `hbf1xv_m34_bl_latch_integration_test` (#193) still asserts the FUNCTIONAL gate:
  after the split, BL=0 rows are PURE BACKDROP (`ref_off`) and BL=1 rows carry content. Only the
  split LINE moved 101→102 (L+1→L+2). BL=0-blanks-the-line is fully intact — grounded in
  `PixelRenderer.cc:608-611` (verified above) and cross-referenced to fMSX per-line
  `if(!ScreenON) ClearLine` in the test header. The change strengthens toward openMSX (it was made
  because the pre-fix L+1 clipped Aleste's HUD bottom row); it does not relax the gate. #192
  (`frame_renderer_bl`) and #191 (`m34_aleste_ultrasonic`) also pass.

**(b) Metal Gear headless sanity check.** Both `roms/metalgear.rom` (SHA1-matched to "Metal Gear",
Konami, via openMSX softwaredb) and `roms/metalgear2_scc.rom` (512 KB SCC) are present.
- Metal Gear 1 boots headlessly with no crash and, driven by the existing
  `tools/metalgear-m34-bl-transition-input.script`, reaches real gameplay (frame 4000): a scrolling
  room with the fixed bottom "LIFE/CLASS" status HUD rendered COMPLETE and unclipped in our post-fix
  render (`debug/m39-mgear/play4000.png`). This is the same fixed-HUD-over-playfield class the fix
  targets, and it renders correctly at L+2.
- Metal Gear's R#1 BL usage is a FULL-SCREEN room-transition blank (R#1 0x62→0x22, established by the
  M34 probe): frames 4176/4182/4188 are byte-identically FULLY BLACK
  (`debug/m39-mgear/trans_f4182.png`) — our render blanks the whole display exactly as expected. This
  is the lowest-risk BL usage: the L+2 shift affects only the SINGLE flip frame's partial line during
  an already-blanking transition — imperceptible.
- The M34 QA sign-off ALREADY recorded an openMSX A/B of this exact transition ("MG genuinely blanks:
  room transition our f4173→f4191 (r1 0x62→0x22); openMSX ref f4175→f4201 — PASS"). The M39 re-derivation
  moves the boundary 1 line TOWARD that openMSX reference and the oracle still passes.

A frame-exact openMSX 19.1 A/B of an INTERACTIVE in-game MG scene is NOT feasible in this headless QA
pass (it needs input-timing-aligned openMSX replay — the M38 static-cart harness cannot drive
interactive gameplay). I relied instead on: (i) our correct, complete post-fix MG render; (ii) the
existing M34 openMSX A/B of MG's BL blank; (iii) the Aleste openMSX raw-capture calibration of the
identical HUD-over-playfield pattern; and (iv) the formula reasoning below.

**Formula reasoning for the disclosed ±1 residual.** openMSX's BL state flip is scheduled via
`syncAtNextLine` with `offset = 144 + (horizontalAdjust-7)*4` = 144 ticks at neutral R#18
(`VDP.cc:1264-1265`); the net visible blank boundary flips to the next line once the in-line position
≥ 144 openMSX ticks. TICKS_PER_LINE=1368, our tstates/line=228 (ratio 6), so 144 ticks ≈ **in-line
tstate 24**. Our L+2 is applied UNCONDITIONALLY. Therefore L+2 matches openMSX for any write past
~tstate 24 — i.e. the last ~89% of a scanline, which covers Aleste's measured 44/98/169 and the vast
majority of real writes. The residual is a possible +1-line lateness ONLY for a write landing in the
first ~10% (tstate < ~24) of a scanline, and even then the effect is a 1-line cosmetic boundary
difference with functional gating fully preserved — never worse than the pre-fix L+1 for the typical
case that motivated the fix.

**Confidence: HIGH** that `53e4eaf` does not regress Metal Gear (or other R#23/R#1/palette raster
effects). MG's BL is a full-screen transition blank (lowest risk); its status-HUD split renders
complete at L+2; the change is calibrated to and validated against openMSX for the identical Aleste
pattern; and the ±1 residual is narrow, cosmetic, and sub-perceptual. A human Metal-Gear play-check
remains WARRANTED as belt-and-suspenders perceptual confirmation (checklist c) — not because a
concrete regression is predicted, but because the residual and the HUD-seam appearance are inherently
perceptual/real-time.

## 4. Failures and Risk Ranking

No blocker-, high-, or medium-severity code defects found. Residual risks are all
perceptual/real-time/preference items that CANNOT be verified headlessly by design:

1. LOW (perceptual, raster): Aleste-2 / F1 fixed-HUD glyph bottoms reading correctly IN MOTION at
   runtime — headless proves the split lands on openMSX's static scanline; in-motion smoothness is a
   human read.
2. LOW (perceptual, raster): Metal Gear status-HUD seam + room-transition blank looking right in
   live play — headless proves complete HUD render + full-screen transition blank; live perceptual
   confirmation is a human read. The ±1 extreme-early-in-line residual is a theoretical, cosmetic,
   sub-perceptual edge case (see §3.1).
3. LOW (preference, presentation): the reverted default (bare edge-to-edge, `--border` opt-in) is the
   desired out-of-box appearance — an owner preference call.
4. CLEARED (voice, real-time): live SDL3 voice playback — already HUMAN-CONFIRMED per the task.

## 5. Required Fixes

None (no code changes required for release). Two optional, non-blocking items:
- (Optional) One `hbf1xv_m24_zexall_system_test` sweep to formally close the memory-rule "cpu edit →
  sweep" loop, even though `z80a_cpu.cpp` changed comment-only.
- (Optional, follow-up) If a future milestone needs sub-line raster precision, the flat L+2 constant
  could be replaced by an in-line-tstate-aware boundary (matching openMSX's tstate-24 transition) to
  erase the ±1 residual. Not needed for v1.0.39.

## Sign-off Decision — CONDITIONAL PASS (pending perceptual human live-verify)

Rationale: every headlessly-verifiable gate passes — clean rebuild; **213/213** (0 failed); cpu/core
behaviorally empty (comment-only); M38 scroll output independently proven byte-identical pre/post the
raster fix; M32 line-interrupt path provably untouched and split-screen within tolerance; M34 BL
oracle re-derived to the openMSX-grounded L+2 with functional BL-gating preserved and passing; voice
idle-byte-identity intact with voice already human-confirmed; border revert oracle correct. The
special-scrutiny `53e4eaf` change is openMSX-grounded (PixelRenderer.cc:566-567 verified), empirically
calibrated to openMSX 19.1 raw captures, and shown not to regress Metal Gear headlessly (boots,
reaches gameplay with a complete status HUD, blanks fully during transitions). No blocker-level gap
remains. The residual items are inherently perceptual/real-time and warrant a Conditional Pass gate
before the v1.0.39 tag is declared "released", NOT a Fail.

### Human live-verify checklist (required before tagging v1.0.39 as released)
- [ ] (a) Launch a 212-line fixed-HUD game — Aleste 2 (and/or F1) — and CONFIRM the status-HUD glyph
      BOTTOMS read correctly in motion (no clipped bottom row) at the HUD/playfield seam.
- [ ] (b) CONFIRM the out-of-box default present (bare edge-to-edge, Sony-original) is the desired
      appearance, and that `--border` restores the openMSX-matching framed canvas.
- [ ] (c) Launch Metal Gear (`roms/metalgear.rom`), play into a room and trigger a room transition;
      CONFIRM the fixed bottom status bar's edge looks right and the room-transition blank is clean
      (no visible 1-line artifact at the split). (Voice is already confirmed — no re-check needed.)
- [ ] (d) (Optional, belt-and-suspenders) run the withheld `hbf1xv_m24_zexall_system_test` once.

Do NOT commit this file (docs/ is gitignored/local). QA performed no source changes; the tree stayed
byte-clean at `53e4eaf`. Do not tag until (a)/(b)/(c) are confirmed by the human.
