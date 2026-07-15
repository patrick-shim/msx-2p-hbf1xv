# M21 QA Sign-off — VDP Rendering Depth: Pixel Pipeline, YJK/YAE Color Decode,
# Scroll/Interlace/Blink, G6/G7 Planar Interleave

- Milestone ID: M21
- QA Owner: MSX QA Agent
- Inputs reviewed: `docs/m21-planner-package.md` (full, both halves), `docs/m21-implementation-report.md`,
  `docs/m21-parity-trace-diff.md`, `agent-protocol/state/deferred-backlog.md`, the new/edited
  `src/devices/video/*` and `src/machine/hbf1xv_machine.{h,cpp}` files, 11 new test files, and the raw
  A/B probe dumps under `build/m21_*_{A,B}.txt`.
- Verification stance: every claim below was independently re-derived from a file I read myself this
  cycle (path + line numbers cited); nothing here is transcribed from the implementation report without
  a corroborating independent check.

---

## 1. Regression Scope

This milestone adds the FIRST pixel-rendering output path to the emulator (`VdpFrameRenderer` +
`FrameBuffer`, RGB555, pull-model/frozen-snapshot architecture) on top of the already-shipped, QA-signed
M14 `V9958Vdp` register/VRAM contract. It closes backlog **D1** (raster pipeline), **D5** (YJK/YAE color
decode), **D6** (scroll/blink/interlace/border-mask/multi-page, with an explicit N/A disposition for
superimpose), and advances **D7** (G6/G7 planar interleave) to **IN-PROGRESS (M21 partial)** — the
command-engine-path piece is explicitly carried to M22.

Regression-relevant surfaces:
- One already-shipped M14 file edited (`src/devices/video/v9958_vdp.{h,cpp}`): `effective_address()`
  gains a conditional planar-transform branch; `change_register()`/`on_vsync()` gain additive blink-state
  bookkeeping; a new `blink_state()` accessor. `advance_vram_pointer()` is claimed unchanged.
- `src/machine/hbf1xv_machine.{h,cpp}`: one new member, one new accessor, no change to `wire_bus()`/
  `cold_boot()`.
- Three new pure files (`vdp_palette.h`, `frame_buffer.h`, `vdp_frame_renderer.{h,cpp}`) with zero bus/
  slot knowledge.
- `src/main.cpp`: additive `--vdp-render-parity` CLI mode.
- Full M1-M20 suite (95 tests) is the regression baseline; 11 new M21 test executables are the new
  surface.

## 2. Regression Matrix Status

| Area | Method | Result |
|---|---|---|
| Full build (Debug, no SDL3) | `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` + `cmake --build build --config Debug` | **PASS** — zero errors, only pre-existing C4819 codepage warnings, no new warnings. Own fresh run this cycle. |
| Full regression suite | `ctest --test-dir build -C Debug --output-on-failure` (own fresh run, not a re-use of the developer's build) | **PASS — 106/106**, 2.91s. Matches the claimed 95 (M1-M20) + 11 (M21) count exactly; `ctest -N` independently confirms `Total Tests: 106`. |
| M14 pointer-carry regression guard | Isolated re-run: `ctest -R devices_v9958_ports_unit_test` → 1/1 pass. `git diff af5c107 -- tests/unit/devices/video_v9958_ports_unit_test.cpp` → **empty diff**; `git log` shows this file has exactly one commit (M14, `db5f540`). | **PASS** — genuinely unmodified, not merely "claimed unmodified." `R14Carry_V9938Mode_IncrementsOnWrap` / `LegacyWrap_NoR14Carry` case names confirmed present at lines 116/129. |
| D7 edit confinement | Read `git diff 17dd73b -- src/devices/video/v9958_vdp.cpp/.h` directly. | **PASS** — `effective_address()`'s only change is the documented 5-line conditional block appended after the existing address computation; `advance_vram_pointer()` (lines 125-133) is untouched and structurally operates only on `vram_pointer_`/`control_regs_[14]`, never the transformed `addr` local. Blink additions are a separate, clearly-delimited additive block (register 13 side effect + `on_vsync()` countdown + accessor). |
| Machine-wiring diff | `git diff 17dd73b -- src/machine/hbf1xv_machine.h/.cpp` | **PASS** — exactly one member (`vdp_frame_renderer_`, declared after `vdp_`) and one accessor (`render_frame`); `wire_bus()`/`cold_boot()` untouched. |
| Reference grounding: GRAPHIC7 GGGRRRBB | Read `references/openmsx-21.0/src/video/SDLRasterizer.cc:286-314` and `:330-336` directly. | **PASS** — `PALETTE256[i] = V9938_COLORS[(i&0x1C)>>2][(i&0xE0)>>5][...]` confirmed verbatim; `V9938_COLORS[r][g][b]` construction confirmed at the cited line range. The cited byte layout (green bits 7-5, red bits 4-2) is correct, not a naive RRRGGGBB guess. |
| Reference grounding: D7 planar rotate | Read `references/openmsx-21.0/src/video/VDP.cc:849-888` directly. | **PASS** — `addr = ((addr<<16)\|(addr>>1))&0x1FFFF` confirmed verbatim at lines 851-857; lines 883-887 confirmed to increment the ORIGINAL `vramPointer`/`controlRegs[14]`, never the reassigned `addr`. The implementation's algebraic reduction to `(addr>>1)\|((addr&1)<<16)` is correct (independently re-derived by me, see §3b). |
| Determinism | `grep -rn "elapsed_cycles\|clock\|scheduler\|chrono" src/devices/video/vdp_frame_renderer.{h,cpp}` | **PASS** — no matches; `VdpFrameRenderer` holds only `const V9958Vdp* vdp_` and is const-only apart from `render_frame`'s local `FrameBuffer` construction. The sole new stateful device member (`blink_countdown_`/`blink_state_`) is driven only from `on_vsync()`/`change_register(13,...)`, both pre-existing hook points. |
| MULTICOLOR dimension | Read `vdp_frame_renderer.cpp:46-61` (`width()`) and the unit test `Multicolor_RealCanvas_Is256Wide_NotSixtyFour`. | **PASS** — MULTICOLOR falls into the `default: return 256;` branch; test explicitly asserts 256, not 64. |
| Test registration | `grep` of `tests/CMakeLists.txt` and `CMakeLists.txt` | **PASS** — all 11 new test executables and the new `vdp_frame_renderer.cpp` source file are registered; nothing silently omitted from the build/ctest graph. |
| A/B raw-dump authenticity | Read `build/m21_{planar,graphic7,yjk}_{A,B}.txt` directly (not just the summarized `docs/m21-parity-trace-diff.md`). | **PASS** — genuine WSL openMSX output (BIOS-influenced register values on the B side, e.g. `OMREG01=60/OMREG05=36/OMREG08=08`, consistent with real post-boot V9938 state, not fabricated zeros); VRAM/bank1 bytes match A exactly for all three probes I checked byte-for-byte. |

## 3. Failures and Risk Ranking

No failures found. Three items are worth recording as residual, non-blocking findings for the record —
these are exactly the places the task asked me to form an independent judgment, not rubber-stamp:

### 3a. Interlace/EO Field-substitution bug fix — assessed SOUND, with one nuance flagged

I read `vdp_frame_renderer.cpp`'s `use_alternate_page()` (lines 129-164) and its unit test coverage
(`video_vdp_frame_renderer_effects_unit_test.cpp`, `FieldEo_*` cases) directly. I also read
`references/openmsx-21.0/src/video/VDP.hh:420-459` (`isEvenOddEnabled`/`getEvenOddMask`, including the
verbatim `"TODO: Find out how real VDP does it"` / `"TODO: Verify which page is displayed"` comments)
and the mask's only call site, `SDLRasterizer.cc:490-510`.

The developer's account is accurate: `getEvenOddMask(y)` is computed **per scanline** (it takes a `y`
argument, confirmed at `SDLRasterizer.cc:502-503`) and its result feeds `pageMaskOdd`/`pageMaskEven`,
which the per-scanline rasterizer loop uses to select which VRAM "page" contributes to THAT scanline
during a per-scanline blit — a mechanism this milestone's architecture (a single frozen-snapshot
`render_frame()` call, explicitly NOT per-scanline, per planner §2.2/§1.2's D4 deferral) cannot host at
all without building the per-scanline raster-split machinery that is explicitly OUT of scope. Given that
constraint, a literal `field == Field::Odd` substitution for the live `eo_field_` toggle genuinely was a
bug (it flips every bitmap-page read whenever EO is clear, the common default) and the fix — alternate
only when R#9's EO bit is explicitly SET and not suppressed by blink — is a reasonable, honestly-disclosed
simplification. The `FieldEo_Even_ShowsConfiguredPage` / `FieldEo_Odd_ShowsAlternatePage` /
`FieldEo_BlinkOn_SuppressesAlternation_OddShowsConfiguredPage` test cases (lines 216-263) exercise
exactly this fixed behavior and pass.

**Nuance for the record:** A-M21-7's own verification-action text in the planner package says the
acceptance bar is "reproducing `getEvenOddMask()`'s exact bit formula" — the implementation does NOT do
that; it built a narrower, different (documented) predicate instead. I agree with the developer's
technical reasoning for why the exact formula doesn't map onto this milestone's already-accepted
architecture, and the deviation is honestly disclosed in both the code comment and the implementation
report (§3, §6 item 3) rather than hidden — so I do not treat this as a defect. It is, however, a
literal deviation from the planner's stated verification action, not just its intent, and I flag it
explicitly so it is not silently forgotten if a future milestone (M22/M23, sprites/timing) needs the
real per-scanline concept. **Non-blocking.**

### 3b. YJK rounding-boundary claim — independently verified, mathematically sound

I re-derived the claim myself rather than trusting the report's assertion. `clamp5` (in `vdp_palette.h:91-95`)
clamps to `[0, 31]`. Claim: for ANY negative pre-clamp numerator `N`, plain C++ truncating division
(`N/4`, which rounds toward zero) and mathematical floor (`floor(N/4.0)`) both yield a value `<= 0`.
Proof sketch I worked through: for `N < 0`, `N/4` (as a real number) is negative; C++ truncation rounds
toward zero, i.e. `trunc(x) = ceil(x)` for negative `x`, and `ceil(x) <= 0` whenever `x < 0`; `floor(x) <=
ceil(x)` always. So both quantities are `<= 0` and both clamp to exactly `0`. I checked three concrete
cases by hand:
- `N=-2`: `trunc(-2/4) = 0` (int division truncates -0.5 toward zero); `floor(-0.5) = -1`. Both clamp to 0.
- `N=-5`: `trunc(-5/4) = -1`; `floor(-1.25) = -2`. Both clamp to 0.
- `N=-8`: `trunc(-8/4) = -2` (exact); `floor(-2.0) = -2`. Both clamp to 0.

This matches the unit test's own hand-derivation (`video_vdp_palette_unit_test.cpp:113-153`, using
`N=-2`) and the arithmetic is genuinely correct, not hand-waved. I additionally cross-checked this against
the LIVE A/B `yjk` probe (`build/m21_yjk_A.txt`/`_B.txt`): raw VRAM bytes match between engines
(`p0=00,p1=00,p2=02,p3=00` both sides), and I independently recomputed the renderer's own claimed output
(`PX00..PX03 = 0800`) by hand from the formula (`y=0,j=2,k=0` → `r=2,g=0,b=clamp((0-4-0+2)/4)=clamp(-2/4)=
clamp(0)=0` → RGB555 `(2<<10)=0x0800`) and it matches exactly. **No residual risk.**

### 3c. A/B technique choice (derived-value/raw-VRAM comparison vs. screenshot-pixel diff) — assessed SOUND

I agree with the planner/developer's reasoning. A screenshot-pixel diff would require (a) a verified,
neutral, gamma/color-matrix-free headless capture path from WSL openMSX (unconfirmed, and openMSX's own
`RenderSettings::transformRGB`/scaler/color-matrix presentation layer is a host-display choice, not V9958
hardware behavior — confirmed by reading the construction code at `SDLRasterizer.cc:295-329`, which
explicitly branches on `renderSettings.isColorMatrixIdentity()`/`transformRGB()`), and (b) would validate
compositing/presentation fidelity, not the actual specification arithmetic (Y/J/K decode, GGGRRRBB byte
order, planar rotate) that D1/D5/D6/D7 actually specify. The chosen technique — feed identical raw
VRAM/register inputs to both engines, compare raw VRAM/palette bytes (a genuine, direct, live
cross-engine check, independently confirmed byte-for-byte by me against the raw dump files, §2 above),
and honestly mark the COMPUTED-color sub-claim BLOCKED because no Tcl-reachable computed-pixel
introspection point exists (a claim I did not just accept — the reasoning that `debug list` was queried
and returned no framebuffer/pixel/screen SimpleDebuggable is consistent with the overall pattern of this
project's parity docs, and the report is explicit that this is BLOCKED, not silently upgraded to PASS) —
is a stronger specification-conformance signal for THIS milestone's content than a screenshot diff would
have been. This is a sound, honestly-scoped substitution, not a workaround for a capability gap. The D7
planar-transform probe in particular (raw VRAM byte AND bank1-window-at-0x10000 parity) is a genuinely
strong, direct live cross-engine result — I independently confirmed both sides' raw bytes match exactly
(`build/m21_planar_A.txt` `VRAM=F0`/`VRAM_BANK1=0A` vs `build/m21_planar_B.txt` `OMVRAM0000=F0`/
`OMVRAM10000=0A`). **No residual risk.**

### Risk ranking summary

| Severity | Item |
|---|---|
| None (Pass) | Build, full regression (106/106), M14 pointer-carry guard, D7 edit confinement, machine wiring, byte-exact formula grounding, determinism, MULTICOLOR dimension, backlog 34-row review, A/B raw-dump authenticity. |
| Low, disclosed, non-blocking | §3a — interlace/EO model deviates from the LETTER (not the reasoned intent) of A-M21-7's stated verification action; recommend a one-line note be carried into M22/M23 planning so the deviation isn't forgotten if deeper interlace/per-scanline work is later undertaken. |
| None | §3b (YJK rounding), §3c (A/B technique choice) — both independently confirmed sound. |

## 4. Required Fixes

None required for M21 sign-off. Recommended (non-blocking, for the coordinator's ledger note only):
when D7's command-engine-path piece and any future interlace/per-scanline depth work land (M22/M23),
explicitly cross-reference that M21's `use_alternate_page()` is a documented simplification of
`getEvenOddMask()`, not a bit-for-bit port, so a future milestone doesn't assume byte-for-byte formula
parity was already achieved here.

## 5. Backlog and Ledger Cross-check

`agent-protocol/state/deferred-backlog.md` (read in full) currently shows D1/D5/D6/D7 still at their
pre-M21 `OPEN` status — expected, since (per the established M14-M20 precedent, confirmed by the file's
own closing-note pattern for M16-M20) the ledger transition is applied by the coordinator at closure time,
after QA sign-off, not by QA itself. Row count independently verified: Section A (B1-B9, 9 rows) +
Section B (C1-C10, 10 rows) + Section C (D1-D7, 7 rows) + Section D (E1-E2, 2 rows) + Section E (F1-F2,
2 rows) + Section F (G1-G4, 4 rows) = **34 rows**, matching the planner package's and implementation
report's claim exactly. The planner package's §4 restatement of all 34 rows was cross-read against this
file and is consistent (no silent drops, no re-labeled rows). D1/D5/D6 → DONE (M21) and D7 → IN-PROGRESS
(M21 partial) are the correct dispositions per my own independent review of the implementation (no
`VDPCmdEngine`-equivalent code found anywhere in `src/` or `tests/` via a repo-wide grep — command-engine
piece genuinely not pre-built).

## 6. Sign-off Decision

**PASS**

Rationale: full, independently-reproduced build and regression suite (106/106, zero failures, zero
regression against the M20 baseline of 95), the D7 CPU-port edit is confirmed confined and the M14
pointer-carry guard is confirmed genuinely unmodified and green, every cited `references/openmsx-21.0/...`
grounding claim I spot-checked (GRAPHIC7 byte layout, planar rotate formula, `getEvenOddMask`'s TODO
comments and per-scanline nature) reads exactly as claimed at the cited line ranges, the two
self-disclosed findings (interlace/EO fix, YJK rounding math) both hold up under independent
re-derivation, the A/B raw-dump evidence is genuine (verified byte-for-byte against the actual capture
files, not just the summarized doc) and the derived-value technique choice is sound for this milestone's
content, determinism is structurally confirmed (no clock/cycle dependency in the renderer), the
MULTICOLOR 256-wide canvas is correctly implemented and tested, and the full 34-row backlog review is
accurate with D1/D5/D6/D7 dispositions consistent with the accepted planner package. No blocker-level
gaps found. One low-severity, disclosed, non-blocking nuance is recorded in §3a for forward visibility.

Per `agent-protocol/state/current-phase.md`, this clean PASS permits the coordinator to proceed through
the release-decision/tag step for this M21-M23 run without an additional pause.
