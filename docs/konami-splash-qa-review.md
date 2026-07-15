# QA Review — Konami-Splash Color-0 Backdrop Fix + Opt-In Border Presentation

- Date: 2026-07-08
- Reviewer: MSX QA Agent
- Scope: uncommitted working tree on top of HEAD `6f89552` (DEC-0031), per the
  coordinator-authorized targeted defect cycle (DEC-0026/DEC-0029/DEC-0031 precedent).
- Developer artifact reviewed in full: `docs/konami-splash-regression-investigation.md`.
- Every claim below was independently re-derived or re-executed by QA (builds, ctest runs,
  worktree adversarial builds, reference reads, pixel comparisons); nothing was accepted on
  the developer's word alone.

## 1. Regression Scope

| Area | Change | Risk |
| --- | --- | --- |
| `src/devices/video/vdp_frame_renderer.{h,cpp}` | color-0 backdrop substitution (`content_pal16`/`content_pal16_g5`) at 9 content call sites | HIGH (every palette-mode content pixel path) |
| `src/frontend/sdl3_video_presenter.{h,cpp}` | border composition now opt-in, default = bare active area | MEDIUM (presentation default flip) |
| `src/frontend/sdl3_cli.{h,cpp}`, `sdl3_app.{h,cpp}`, `sdl3_main.cpp` | `--border` flag plumbing | LOW (additive flag) |
| Tests | 1 new unit suite, 3 extended/rewritten suites, CMake registration | LOW |
| Untouched (verified `git diff HEAD --name-only` = 0 files) | `src/devices/cpu/`, `src/core/`, `src/main.cpp` | — (slow-sweep exemption valid) |

Zero game-specific logic: the full renderer diff was read line-by-line — every new branch
conditions ONLY on VDP register state (R#7/R#8) and display mode; no title, checksum, or
cartridge keying anywhere.

## 2. Regression Matrix Status

| # | Check | Method (QA-executed) | Result |
| --- | --- | --- | --- |
| 1 | TP polarity (the polarity trap) | Read `references/openmsx-21.0/src/video/VDP.hh:189-191` (`getTransparency()` == "True iff color 0 is transparent" == `(R#8 & 0x20) == 0`) and `references/fact-sheets/Yamaha V9958 VDP.md:72` + `:169` ("When … TP=0 (colour 0 transparent)") | **PASS** — implementation `color0_transparent()` = `(R#8 & 0x20)==0` → TP CLEAR = substitute backdrop; TP SET = raw palette 0. Direction correct. |
| 2 | Content call sites match reference | Read `SDLRasterizer.cc:99-101` (palFg feeds character+bitmap converters; palBg feeds sprites+border), `CharacterConverter.cc:144-145/189-195/268-269/295-311/328-329`, `BitmapConverter.cc:108-157/159-183/271-275` | **PASS** — all 9 substituted sites correspond 1:1 to palFg reads; TEXT2 blink colors stay raw (palBg, `:194-195`) exactly as in our `render_text2` lines 459-460. |
| 3 | GRAPHIC5 even/odd backdrop split | `SDLRasterizer.cc:364-372` (`palFg[0]=palBg[tpIndex>>2]`, `palFg[16]=palBg[tpIndex&3]`) vs `BitmapConverter.cc:152-155` pixel banks | **PASS** — even pixels (bits 7-6/3-2) → backdrop>>2, odd (bits 5-4/1-0) → backdrop&3; mapping exact. |
| 4 | Exclusions (sprites/border/G7/pure-YJK/blank) | Renderer lines 331/382-391 (sprites raw + TP skip), 241/243 (border raw), 612-613 (G7 fixed decode), 543 (blank pal15); `SDLRasterizer.cc:349-352` (G7 forces transparency off), `SpriteConverter.hh:111-114/171` (TP-conditioned sprite skip, "Verified on real V9958") | **PASS** — all exclusions match openMSX's model; sprite TP-skip is pre-existing (A-M22-12), untouched. |
| 5 | Exoneration forensics (DEC-0031 window empty) | (a) Re-analyzed the developer's captured hash files (`hashes-289a40f.txt` 620 frames / `hashes-head.txt` 800 frames) with an independent script: 215/215 splash frames (f405-f619 vs f581-f795) hash-identical at offset 176, 0 mismatches; hash-change cadence aligns exactly (400↔576, 412↔588 …). (b) Built + ran the NEW unit test at a clean `289a40f` worktree | **PASS** — same 5 case failures at 289a40f as at HEAD: the black background exists verbatim at the human's "known good" commit. Regression window empirically empty; DEC-0031 exonerated. |
| 6 | Adversarial: new test discriminates | Clean detached worktree at HEAD `6f89552` (pre-fix), test file + registration injected, built, ran | **PASS** — exactly **5 case failures** (G4 backdrop ×2, G1 nibble, G5 even/odd ×2; cases 2/5/6 pass pre-fix by design — matches developer's claim). Passes on working tree. Worktrees removed + pruned after. |
| 7 | Headless fast subset | Full rebuild + `ctest -C Debug -LE m24_slow_full_sweep` | **PASS — 153/153** (14.78 s). |
| 8 | SDL3-ON fast subset | Full rebuild + ctest with `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy` | **PASS — 162/162** (19.05 s). |
| 9 | Boot-logo sequence unperturbed | Direct run of `machine_hbf1xv_boot_logo_integration_test` + pixel comparison | **PASS** — checkpoint `entry=75 exit=259 f4_end=0x80` identical to DEC-0031 QA-verified values; `boot-logo-post-colorfix-f0210-mainram64k.png` active area is **pixel-identical** (PIL diff bbox = None) to the committed `boot-logo-after-f0210-mainram64k.png` cropped at the border anchor (64,24,512×192). |
| 10 | Splash evidence | Viewed all three PNGs | **PASS** — before: black background; after: white background, orange/red Konami symbol, gray "KONAMI(R)"; matches `konami-splash-openmsx-reference-t12s.png`. |
| 11 | Border default-off is bit-exact pre-DEC-0031 | Read presenter/cli/app diffs + rewritten `sdl3_video_presenter_pixel_integration_test` (default path asserts BARE frame bit-for-bit at (0,0) with target exactly frame-sized; `--border` path asserts composed anchor + surround; `Presenter_DefaultConstruction_BorderOff` guards the constructor default); ran the test individually | **PASS**. |
| 12 | Real-exe smoke, both paths | `sony_msx_sdl3.exe --hidden-window --max-frames 60` with and without `--border` (dummy drivers) | **PASS** — exit 0 both. |
| 13 | Pending-COL lifecycle guard | Read extended `video_vdp_command_engine_pending_col_unit_test.cpp` stale-arm case; verified openMSX `VDPCmdEngine.cc` set/clear sites (`:1856-1863` arm on COL write, `:1279/:1338/:1759` the ONLY clears) during review; ran test | **PASS** — pins both protocols; matches both references. |
| 14 | BASIC-screen impact | QA-built temp harness (outside repo, `Temp/qa-basic/`, mirrors the boot-logo test's frame loop; zero repo modification) → real-BIOS frame 300 dump → `tools/frame-to-png.py` | **PASS** — uniform deep-blue active area RGB (33,33,255), border 0x277F sky-blue: identical center-pixel color to the committed DEC-0031 `border-after-f0300-basic-skyblue-border.png` evidence; consistent with the openMSX BASIC references. TEXT/BASIC screens use non-zero R#7 nibbles (15/4), so `content_pal16` is an identity there — no BASIC visual change, confirmed empirically. |
| 15 | Scaffolding reverted / scope clean | `git status` + `git diff HEAD --name-only -- src/main.cpp src/devices/cpu src/core` | **PASS** — 0 files; `--splash-diag` gone; only the §6-listed files modified. |

fMSX arbitration soundness (DEC-0030 discipline): the disagreement is recorded explicitly
(investigation doc §3 + header doc comment + test header). QA read
`references/fmsx-60/source/fMSX/Common.h:76` directly:
`XPal[0]=(!BGColor||SolidColor0)? XPal0:XPal[BGColor]` — the same slot-0 backdrop
substitution but unconditional (fMSX does not model the TP bit at all; `SolidColor0` is a
user toggle, not hardware). The V9938/V9958 data-book bit definition (fact-sheet `:72` R#8
TP + `:169` "TP=0 (colour 0 transparent)") supports the TP-conditioned model, as does the
TMS9918 lineage (color 0 = transparent-showing-backdrop is the reset-default,
V99x8-compatible behavior; TP=1 is the added opt-out). **openMSX's model is the
data-book-correct one; the arbitration is sound.**

## 3. Failures and Risk Ranking

No Critical/High/Medium findings.

- **F1 (Low)** — Regenerating `docs/openmsx-ab-smoke.md` via `tools/openmsx-ab-smoke.ps1`
  clobbered the hand-added "R5 UPDATE (M10-S4): RESOLVED" header note that pointed to
  `docs/m10-parity-trace-diff.md`. The file now again presents R5 as an unresolved
  capability gap, which contradicts the retained historical record. Restore the note at
  commit time (or teach the tool to preserve a header block).
- **F2 (Info)** — `.claude/settings.json` working-tree edit (`"sonnect"`→`"sonnet"` typo
  fix) is unrelated to this cycle; benign, no permission change. DEC-0031 precedent: keep
  it out of this commit (coordinator handles separately).
- **F3 (Info)** — Fact-sheet erratum: `references/fact-sheets/Yamaha V9958 VDP.md:169`
  says "R#0 TP"; TP lives in R#8 (line 72 of the same sheet). The polarity statement there
  is correct and was used as corroboration; the register number is a typo. Candidate for a
  fact-sheet correction, not a defect of this change.
- **F4 (Info)** — Untracked stragglers `build-qa/`, `build-qa2/`, `disks/games/` remain in
  the working tree (already noted and excluded in DEC-0031's entry; unchanged status).
- **F5 (Info)** — Honest residuals in the investigation doc §8 (15-frame MATERIALIZE
  sampling granularity note, GRAPHIC5 dual-border-color simplification) are accurately
  stated; QA's own hash re-analysis actually compared **every** frame in f405-f619 — the
  captured evidence is stronger than the doc's conservative wording.

## 4. Required Fixes

None blocking. At commit time: apply F1 (restore the R5 UPDATE note in
`docs/openmsx-ab-smoke.md`) and keep F2's `.claude/settings.json` out of the cycle commit
per DEC-0031 precedent.

## 5. Sign-off Decision

**PASS** (unconditional; F1 is a docs-only nit to apply at commit).

- The reported "regression" is proven NOT to be one: the new unit test fails with the
  identical 5 cases at 289a40f, and 215/215 splash frame hashes are identical across the
  DEC-0031 window at offset 176 — both re-derived by QA from primary artifacts.
- The real defect (missing V99x8 color-0 transparency) is fixed at the universal hardware
  level with the correct TP polarity, exact reference call-site mapping, correct
  exclusions, and a discriminating regression guard.
- Border presentation default matches the human's decision, with the pre-DEC-0031 bare
  presentation proven bit-exact and the composed path preserved behind `--border`.
- Full fast-subset evidence re-run by QA: headless 153/153, SDL3-ON 162/162; boot logo
  pixel-identical; BASIC screen empirically unchanged and correct.
