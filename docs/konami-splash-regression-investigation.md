# Konami-Splash Background Investigation + Border Presentation Default

- Date: 2026-07-08
- Trigger: human report from live SDL3 play (post-DEC-0031): Metal Gear's Konami splash screen
  (`--cart1 roms/metalgear.rom --cart1-type Konami`) renders its background BLACK, with the
  border surround WHITE; the human recalls a white background at commit 289a40f (DEC-0029) and
  identified the DEC-0031 commit (6f89552) as the one-commit regression window. A second,
  smaller human decision: the sky-blue border box added by DEC-0031 is not wanted by default.
- Status: **root-caused and fixed** — the black background is a real, universal renderer defect
  (missing V99x8 color-0/backdrop transparency), but the "regression window" is empirically
  EMPTY: frame forensics prove DEC-0031 changed nothing in this content (§2). Border
  composition is now opt-in via `--border` (default = bare active area, pre-DEC-0031
  presentation). All changes uncommitted for coordinator/QA review.
- Precedent: coordinator-authorized targeted defect cycle (DEC-0026/DEC-0029/DEC-0031
  discipline); fMSX second cross-reference per DEC-0030.

## 1. Reproduction

Temporary headless diagnostic (`--splash-diag` in src/main.cpp, mirroring
`Sdl3App::run_one_frame()`'s exact loop — step_cpu_instruction() to the frame boundary +
on_vsync_boundary(); reverted after use, per the DEC-0031 `--boot-logo-diag` precedent).
Metal Gear timeline on pre-fix HEAD: boot logo frames ~75–259, cart SCREEN 1 phase, Konami
splash enters GRAPHIC4 (SCREEN 5) at frame ~585, display on at ~600, settled logo by ~690.
Registers during the splash (every sampled frame): `R#7=0x0F R#8=0x08 pal0=0x000 pal15=0x777`.
Frame dumps confirmed the reported symptom exactly: active-area background RGB555 `0x0000`
(black), `FrameBuffer.border_color = 0x7FFF` (white).

- Pre-fix evidence: `debug/frames/konami-splash-before-f0750-black-background.png`.

## 2. Bisect result: the DEC-0031 regression window is EMPTY

The same diagnostic was built in a git worktree at 289a40f (the human's "known good" commit)
and both timelines were captured with a per-frame FNV-1a-64 hash of the full serialized frame
dump:

- 289a40f: splash mode-change f405, display-on f420. HEAD: f585/f600 — a constant offset of
  **176 frames**, exactly the boot-logo phase DEC-0031 restored (missing #F4 register made the
  pre-DEC-0031 BIOS skip the logo entirely).
- Aligned at offset 176, **all 215 sampled splash frames hash-identical** (mismatch count 0);
  the settled-splash dumps are byte-identical files (sha256
  `711dd97a…` on both sides). The splash content — including the black background — is
  bit-for-bit unchanged by DEC-0031.

Conclusion: none of the DEC-0031 changes (pre-armed COL unit, collision re-latch, sprite
edge-clip, border composition) altered this content. The black background predates DEC-0031;
it became conspicuous in live play because DEC-0031's border composition put a correct WHITE
border around the incorrect black active area (pre-DEC-0031 the window was black edge-to-edge
with no contrasting frame). The prime-suspect pre-armed-COL lifecycle was independently
verified anyway (§4).

## 3. Root cause: missing V99x8 color-0 transparency (backdrop substitution)

Metal Gear's splash leaves its background pixels at **color index 0** with `R#7 = 0x0F`
(backdrop = palette 15 = white) and `R#8 = 0x08` (TP bit CLEAR). On real hardware color 0 is
then TRANSPARENT: it displays the **backdrop color** — white — for both the border AND
color-0 content pixels. Our `VdpFrameRenderer` resolved every content color through the raw
palette (`pal16(0)` = palette entry 0 = black).

Grounding (read, never copied):

- Fact sheet: `references/fact-sheets/Yamaha V9958 VDP.md:72` — R#8 "TP colour0 transparent".
- openMSX: `references/openmsx-21.0/src/video/VDP.hh:189-191` `getTransparency()` ==
  `(R#8 & 0x20) == 0`; `references/openmsx-21.0/src/video/SDLRasterizer.cc:346-373`
  `precalcColorIndex0()` — `palFg[0] = palBg[tpIndex]`, `tpIndex = transparency ? bgColor : 0`;
  GRAPHIC7 forces transparency off (`:349-352`); GRAPHIC5 splits the backdrop nibble into
  even/odd 2-bit halves (`:364-372`). The substituted `palFg` table feeds EVERY content path
  (`SDLRasterizer.cc:99-100`: characterConverter takes `subspan<16>(palFg)`, bitmapConverter
  takes `palFg`), while sprites (`:101`, palBg) and the border (`:375-393`, palBg) read the
  raw palette.
- fMSX cross-reference (independent corroboration):
  `references/fmsx-60/source/fMSX/Common.h:76` / `Wide.h:44` —
  `XPal[0]=(!BGColor||SolidColor0)? XPal0:XPal[BGColor]` — the same slot-0 backdrop
  substitution, applied unconditionally (fMSX does not model the TP bit; a user option
  `SolidColor0` can disable it). **Disagreement + arbitration (DEC-0030):** openMSX's
  TP-conditioned model matches the V9938/V9958 data-book bit definition and is followed.
- Live ground truth: openMSX 19.1 (WSL, Sony_HB-F1XV, same metalgear.rom) screenshots at the
  splash show the WHITE background —
  `debug/frames/konami-splash-openmsx-reference-t12s.png`.

### The fix (universal, zero game-specific logic)

`src/devices/video/vdp_frame_renderer.{h,cpp}`: new `content_pal16()` /
`content_pal16_g5()` — the palFg-equivalent lookup (color 0 → backdrop when
`color0_transparent()`), applied at exactly the reference's content call sites:

| Path | Reference grounding |
| --- | --- |
| TEXT1 fg/bg (R#7 nibbles) | CharacterConverter.cc:144-145 (palFg) |
| TEXT2 plain fg/bg (blink colors stay raw) | CharacterConverter.cc:189-195 (palFg vs palBg) |
| GRAPHIC1 color-table nibbles | CharacterConverter.cc:268-269 |
| GRAPHIC2/3 color-table nibbles | CharacterConverter.cc:295-311 |
| MULTICOLOR nibbles | CharacterConverter.cc:328-329 |
| GRAPHIC4 4bpp pixels | BitmapConverter.cc:108-143 (palette16 == palFg) |
| GRAPHIC5 2bpp pixels (even/odd split) | BitmapConverter.cc:145-157 + SDLRasterizer.cc:364-372 |
| GRAPHIC6 4bpp planar pixels | BitmapConverter.cc:159-183 |
| YJK+YAE palette branch | BitmapConverter.cc:271-275 |

Unchanged (deliberately, per the references): GRAPHIC7 (fixed 256-color decode, transparency
forced off), pure YJK (no palette in the pixel path), sprites (palBg + the existing TP
color-0 skip), border color (palBg), render_blank (palette 15).

- Post-fix evidence: `debug/frames/konami-splash-after-f0750-white-background.png` — white
  background, orange/red Konami symbol, gray "KONAMI(R)" — matches the openMSX reference.

## 4. Pre-armed COL transfer flag: full lifecycle audit (suspect exonerated)

The task's prime suspect was DEC-0031's `transfer_pending_`. Complete lifecycle in both
references, verified line-by-line:

- openMSX `VDPCmdEngine.cc` — SET: any COL write (`:1856-1863` setCmdReg case 0x0C
  `transfer = true`) and startLmcm (`:1251`). CLEARED: ONLY by the three transfer-execute
  consumption sites (`:1279` executeLmcm, `:1338` executeLmmc, `:1759` executeHmmc).
  startLmmc/startHmmc deliberately do NOT arm (`:1303-1305`, `:1732-1733`, bug#1014).
  `commandDone()` (`:2610-2619`) and `executeCommand()` (`:1940-2029`) never touch it;
  `reset()` (`:1797-1806`) writes COL=0 through setCmdReg, LEAVING transfer armed.
- fMSX `V9938.c` — the TR bit doubles as the flag: `VDPWrite()` (`:852-857`) clears TR
  ("byte pending") on every R#44 write; only the transfer engines re-set it after consuming
  (`:600/632` etc.); `VDPDraw()` (`:909-1024`) never resets it at command start; atomic
  engines (HmmvEngine etc.) never touch it.

Both references therefore agree: a stale armed unit survives non-transfer commands and IS
consumed as byte 0 by a later LMMC/HMMC started without a fresh pre-load — our implementation
matches this model exactly (and §2 proves it does not perturb Metal Gear). Both protocols
coexist by construction: the data-book pre-load protocol (COL then CMD then N-1 writes — the
boot logo) and the start-then-N-writes protocol (bug#1014's game) — no special-casing.
Pinned by a new stale-arm/old-protocol case added to
`tests/unit/devices/video_vdp_command_engine_pending_col_unit_test.cpp`, so a future "fix"
that clears the flag at command start (breaking the boot logo) or at atomic completion
(diverging from both references) trips loudly.

## 5. Border presentation: default OFF, opt-in `--border` (human decision)

The human saw the DEC-0031 sky-blue border live and decided against it. `border_composer` is
kept intact (tested, hardware-grounded, QA-verified); composition is now OPT-IN:

- `Sdl3VideoPresenter(renderer, border_enabled = false)` — default `blit_frame()` uploads the
  BARE active-area FrameBuffer edge-to-edge (byte-for-byte the pre-DEC-0031 presentation);
  with `border_enabled` it composes the border canvas first (unchanged geometry/colors).
- New `--border` flag: `sdl3_cli.{h,cpp}` (`ParsedSdl3Cli::border_enabled`, bare-boolean,
  mirrors `--hidden-window`) → `Sdl3AppConfig::border_enabled` → presenter construction
  (`sdl3_app.{h,cpp}`, `sdl3_main.cpp` incl. usage text).
- `tools/frame-to-png.py --with-border` unchanged (already opt-in).
- `frontend_border_composer_unit_test` unchanged (the composer itself is untouched).

## 6. Code changes (all uncommitted, for review)

| File | Change |
| --- | --- |
| `src/devices/video/vdp_frame_renderer.{h,cpp}` | color-0 backdrop substitution (`content_pal16`/`content_pal16_g5`) at the reference content call sites |
| `src/frontend/sdl3_video_presenter.{h,cpp}` | border composition opt-in (`border_enabled`, default off) |
| `src/frontend/sdl3_cli.{h,cpp}` | `--border` flag |
| `src/frontend/sdl3_app.{h,cpp}`, `src/frontend/sdl3_main.cpp` | `Sdl3AppConfig::border_enabled` wiring + usage text |
| `tests/unit/devices/video_vdp_frame_renderer_color0_backdrop_unit_test.cpp` | NEW regression guard (6 case groups) |
| `tests/unit/devices/video_vdp_command_engine_pending_col_unit_test.cpp` | + stale-arm/old-protocol lifecycle case |
| `tests/unit/frontend/sdl3_cli_unit_test.cpp` | + `--border` parse cases, default-false guard |
| `tests/integration/frontend/sdl3_video_presenter_pixel_integration_test.cpp` | covers BOTH paths: default = bare bit-exact (pre-DEC-0031 assertions), `--border` = composed geometry assertions |
| `tests/CMakeLists.txt` | new test registration |
| `.gitignore` | evidence-PNG exceptions |

Temporary `--splash-diag` scaffolding in `src/main.cpp` reverted (`git checkout --`);
the 289a40f bisect worktree removed.

## 7. Tests + verification (all captured from real runs)

- NEW `devices_vdp_frame_renderer_color0_backdrop_unit_test`: the exact Konami-splash shape
  (GRAPHIC4, R#7=0x0F, R#8=0x08 → white backdrop, content == border color), TP-set raw
  palette-0 case, GRAPHIC1 color-table-nibble backdrop, GRAPHIC5 even/odd backdrop split,
  GRAPHIC7 exclusion, determinism. **Verified failing pre-fix**: built at the 289a40f
  worktree → 5 case failures; passes on HEAD+fix.
- `tools/validate-assets.ps1` — pass (7 BIOS files, 2 ROMs);
  `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — regenerated.
- `cmake --build build --config Debug` and `cmake --build build/sdl3-on --config Debug` —
  0 errors.
- Headless fast subset (`ctest -LE m24_slow_full_sweep`): **153/153 passed** (18.75 s;
  152 pre-existing + 1 new).
- SDL3-ON fast subset (dummy drivers): **162/162 passed** (19.07 s).
- `machine_hbf1xv_boot_logo_integration_test` (direct run): PASS, checkpoint
  `entry=75 exit=259 f4_end=0x80` — identical to the DEC-0031 QA-verified values; post-fix
  logo frame `debug/frames/boot-logo-post-colorfix-f0210-mainram64k.png` visually matches the
  committed `boot-logo-openmsx-reference-t3500ms.png` (blue surround, black box, white MSX).
- `hbf1xv_m22_sprites_command_engine_system_test`: PASS (in both subsets).
- Bounded real-exe sanity runs (dummy drivers, `--hidden-window --max-frames 30`) with and
  without `--border`: both exit 0.
- ZEXALL/ZEXDOC slow sweep NOT run — change touches `src/devices/video/` + `src/frontend/`
  only (never `src/devices/cpu/`/`src/core/`); RC-checkpoint-only cadence per the standing
  rule.
- `tools/openmsx-ab-smoke.ps1` → `docs/openmsx-ab-smoke.md` regenerated (R5 status unchanged:
  UNRESOLVED/waiver candidate — pre-existing environment limitation). The substantive A/B for
  THIS change is the live openMSX splash screenshot pair in §3.

## 8. Honest residuals

- The human's recollection of a white splash at 289a40f is not reproducible: the 289a40f and
  HEAD frame streams are hash-identical at every sampled splash frame (§2), so the live
  window content at that commit was the same black-background active area. The perceived
  before/after difference most plausibly came from DEC-0031's white border making the black
  content conspicuous, or from a concurrent openMSX/fMSX session. The defect itself is real
  and is now fixed at the universal hardware level.
- Frames during the splash MATERIALIZE animation were only compared at 15-frame sampling
  granularity (settled frames byte-identical; all 215 sampled hashes identical at offset 176).
- The BASIC screen and other color-0 content now show the backdrop color where they showed
  palette-0 black before — by design (that is the hardware behavior); no committed test
  asserted the old raw-palette-0 pixels (both fast subsets pass unmodified except the files
  listed in §6).
- GRAPHIC5's dual border colors remain collapsed to the even half in `border_color()` (the
  pre-existing documented FrameBuffer simplification) — unrelated to, and unchanged by, the
  content-pixel split implemented here.
