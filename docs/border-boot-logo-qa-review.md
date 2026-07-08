# QA Review — Border-Box + Boot-Logo Targeted Defect Cycle

- Date: 2026-07-08
- Reviewer: MSX QA Agent
- Scope under review: the uncommitted working-tree changes documented in
  `docs/border-and-boot-logo-investigation.md` (diff vs HEAD `a354e8f`, which already carries
  the committed DEC-0029 sprite fix `289a40f` and the DEC-0030 fMSX reference registration).
- Precedent: coordinator-authorized targeted defect cycle (DEC-0026/DEC-0029 discipline,
  `agent-protocol/channels/decisions.md`).
- Cadence: per the human-approved third-revision rule, the ZEXALL/ZEXDOC slow sweep was NOT
  run — this cycle touches neither `src/devices/cpu/` nor `src/core/` (verified: both diffs
  empty vs HEAD). The two fast subsets are the complete regression gate.

## 1. Regression Scope

Issue A (border/backdrop never presented): `src/frontend/border_composer.{h,cpp}` (new),
`src/frontend/sdl3_video_presenter.{h,cpp}`, `tools/frame-to-png.py --with-border`.

Issue B (boot logo never appeared — three stacked root causes + one latent crash):
`src/devices/chipset/reset_status_register.{h,cpp}` (new, #F4), `src/machine/hbf1xv_machine.{h,cpp}`
(wiring + cold-boot clear), `src/devices/video/sprite_engine.{h,cpp}` + `v9958_vdp.{h,cpp}`
(line-granular S#0 collision re-latch), `src/devices/video/vdp_command_engine.{h,cpp}`
(pre-armed COL transfer unit), `src/devices/video/vdp_frame_renderer.cpp` (GRAPHIC5/6
sprite-space clip). Tests: 5 new unit + 1 new integration, 2 updated. Every diff hunk was read.

## 2. Regression Matrix Status (all evidence captured from my own runs)

| Gate | Result |
| --- | --- |
| Headless build (`build-qa`, fresh QA configure+build from working tree) | exit 0 |
| Headless fast subset `ctest -C Debug -LE m24_slow_full_sweep` | **152/152 passed** (17.42 s) |
| SDL3-ON build (`build/sdl3-on`, rebuilt from working tree) | exit 0 |
| SDL3-ON fast subset (dummy video/audio drivers) | **161/161 passed** (18.31 s) |
| E2E: `sony_msx_sdl3.exe --bios-dir bios --hidden-window --max-frames 300` (dummy drivers) | exit 0 |
| Boot-logo integration binary (direct run, 4 binaries × repeated) | `entry=75 exit=259 f4_end=0x80`, reproducible byte-for-byte |
| `tools/validate-assets.ps1` | pass (7 BIOS files, 2 ROMs) |
| `docs/asset-checksums.txt` | regenerated; timestamp-only diff, all hashes stable |
| Scope check: `git diff HEAD -- src/devices/cpu/ src/core/ src/main.cpp` | empty (main.cpp byte-identical; diag scaffolding fully reverted) |
| Game-specific keying | none found in any diff (program names appear only in grounding comments) |

### Reference grounding — independently re-verified (not taken on trust)

- **#F4:** `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:80-83` declares
  `<ResetStatusRegister>` with `<inverted>false</inverted>` at `io base 0xF4` — the
  **non-inverted** variant. `references/openmsx-21.0/src/MSXResetStatusRegister.cc:13-35`:
  non-inverted power-up `status = 0x00`, write `status = (status & 0x20) | (value & 0xA0)`.
  Our `ResetStatusRegister` matches both exactly (behavioral re-expression, no code copied).
  Empirical corroboration: `bios/f1xvbios.rom` contains `DB F4` (IN A,(0F4h)) at 0x146A and
  `D3 F4` (OUT (0F4h),A) at 0x146F — verified by hex dump.
- **Collision granularity:** `references/openmsx-21.0/src/video/SpriteChecker.hh:78-102`
  (`sync(time)` → `checkUntil(time)`, progressive) and `VDP.cc:903-908` (S#0 read syncs the
  sprite checker to the current emulation time before returning) — line/sub-frame granular.
  `references/fmsx-60/source/fMSX/MSX.c:2178-2184` — collision checked once per frame at
  ScanLine==192. Disagreement is real, explicitly recorded in the investigation doc §3.2, and
  the arbitration is sound (see §3 below).
- **COL pre-load:** `references/openmsx-21.0/src/video/VDPCmdEngine.cc:1856-1863` (any COL
  write → `transfer = true`), `:1303-1305`/`:1732-1733` (startLmmc/startHmmc do NOT arm —
  bug#1014). `references/fmsx-60/source/fMSX/V9938.c:852-857` (VDPWrite clears TR),
  `:624-651` (LmmcEngine consumes current COL when TR clear), `:1020-1024` (command start
  immediately runs the engine → pre-load becomes pixel 0). **Both references agree**; our
  `transfer_pending_` model follows openMSX's stricter arm-on-COL-write-only variant, with the
  minor fMSX divergence recorded in the doc.
- **G5/G6 sprite clip:** `references/openmsx-21.0/src/video/SpriteConverter.hh` mode-2 buffer
  contract (256-pixel sprite space, `pixelPtr[x*2+0/1]` doubled writes) — the `x_limit = w/2`
  clip is correct; in sprite mode 1 `x_limit == w` (TMS modes are 256-wide), so no behavior
  change there.
- **Border geometry:** fact-sheet `references/fact-sheets/Yamaha V9958 VDP.md` §7 (line 124:
  left border [202,258), display [258,1282), right border [1282,1341); line 125: NTSC LN
  border tables), `references/openmsx-21.0/src/video/SDLRasterizer.cc:26-76` (translateX,
  320/640 extended-border canvas centered on the visible middle) and `:174-179`
  (lineRenderTop, 14/14 for 212 lines), `references/openmsx-21.0/src/video/VDP.hh:598-604`
  (V99x8 text mode +9 pixels). All five anchors (32/41/64/82 × 24/14) are consistent with
  these sources and with the measured openMSX screenshots.

### Adversarial verification (new tests genuinely fail pre-fix)

Method: `git worktree add --detach … HEAD` (pre-fix baseline for THIS cycle — includes the
committed 289a40f sprite fix, excludes all working-tree changes), new tests compiled against
the pre-fix library, run via ctest. Worktree removed afterwards.

| Probe | Pre-fix result |
| --- | --- |
| `devices_vdp_command_engine_pending_col_unit_test` (verbatim copy) | **FAILED** — 8 cases: pre-load not consumed at start, `1+(N-1)` protocol never completes; bug#1014 no-preload cases still pass (pre-fix contract intact) |
| Collision re-latch, V9958-level S#0 block (extracted; the engine-level hooks do not exist pre-fix so the full file cannot compile there) | **FAILED** — `V9958_NextLine_CRelatched_TheBootLogoPacer` and `V9958_Line12_CRelatchedAgain`: C stays clear for the rest of the frame after one read-clear, the exact frame-granular defect |
| `machine_hbf1xv_boot_logo_integration_test` (verbatim copy, bonus third probe) | **FAILED** — 9 cases; checkpoint `entry=-1 exit=-1 f4_end=0xff`: #F4 open-bus 0xFF, GRAPHIC5 logo phase never entered — the exact human-reported defect |

All three probes pass on the fixed tree (part of the 152/152 run).

### Visual evidence (all PNGs viewed by this reviewer)

- `debug/frames/border-before-f0300-active-area-only.png`: bare MSX-blue active area only.
- `debug/frames/border-after-f0300-basic-skyblue-border.png`: blue active area inside a
  sky-blue border box on the 320x240 canvas — matches the concept shown by
  `border-openmsx-reference-basic-t15s.png` (which additionally shows the BASIC banner; ours
  is blank at f300 due to the documented pre-existing no-disk drive-detect residual, §7 of the
  investigation doc).
- `debug/frames/boot-logo-before-f0135-no-logo.png`: blank SCREEN 1, no logo (pre-fix).
- `debug/frames/boot-logo-after-f0150-letters-sliding.png` / `boot-logo-after-f0210-mainram64k.png`:
  MSX logo animation and the completed logo with "Main RAM:64Kbytes" — matches
  `boot-logo-openmsx-reference-t2500ms.png` / `-t3500ms.png` (Sony variant, correct per the
  64 KB target machine specification).

## 3. Verdict on the collision-granularity reference disagreement

**Sound, and correctly recorded per DEC-0030 discipline.** fMSX's once-per-frame check
(MSX.c:2178-2184) is exactly the pre-fix model and demonstrably cannot run the Sony logo
wobble on schedule: the loop needs 84 S#0-poll exits, and openMSX's measured wobble window
(~0.5–0.75 s ≈ 30–45 frames) is arithmetically impossible at one poll-exit per frame — the
protocol therefore requires sub-frame (line) re-latching, which is also what openMSX's
progressive `SpriteChecker::sync()` models and what the end-to-end A/B timeline confirms
(our post-fix phase timeline matches openMSX phase-for-phase; final registers byte-identical).
The "real BIOS's own polling protocol proves line granularity" argument is grounded in
observable behavior, not just one emulator's word. One caveat noted: I did not independently
disassemble the SUB-ROM wobble loop (the `0x7A5D-0x7A74` trace claim); the observable
protocol shape is independently proven by the adversarial probe and the end-to-end test, so
nothing load-bearing rests on the unverified address detail.

## 4. Judgment on the M22 system-test update

**Legitimate correction, not a weakening** (DEC-0029 R#5=0x07 precedent). The old ordering
(start LMMC, then write all NX*NY pixels) additionally relied on the HMMV fill color 0x5A
left armed in R#44 being ignored at start — per BOTH references that stale byte would have
become LMMC pixel 0 on real hardware, i.e. the old test encoded the pre-fix,
reference-divergent contract. The new program pre-loads pixel 0 before CMD (the authentic
BIOS protocol) and keeps the identical expected VRAM bytes. Coverage is strictly stronger:
the no-preload bug#1014 path remains covered by the new pending-COL unit test.

## 5. Failures and Risk Ranking

No blocker, high, or medium findings. Three low-severity observations:

1. **Low — stale checkpoint number in the investigation doc.** §5/§3.4 report observed
   `exit=270`; all four binaries (my QA build, developer headless + SDL3-ON builds) reproduce
   `entry=75 exit=259 f4_end=0x80` byte-identically. Determinism is intact and 259 is inside
   the asserted [200,330] window; the doc number is simply stale. Fix the doc line before
   commit.
2. **Low — duplicated evidence PNG.** `boot-logo-before-f0135-no-logo.png` is byte-identical
   (same SHA-256) to `border-after-f0300-basic-skyblue-border.png`. Content-consistent (both
   are the static blank SCREEN 1 frame composed with border; the pre-fix boot had reached
   blank SCREEN 1 by f135), and the underlying pre-fix claims are independently proven by the
   adversarial worktree run — but distinct captures would be cleaner evidence hygiene.
3. **Info — unrelated working-tree items riding along:** `.claude/settings.json` typo fix
   ("sonnect"→"sonnet"), untracked `disks/games/`, and QA build dirs `build-qa*/`. None are
   part of this cycle's reviewed change; the coordinator should keep them out of this cycle's
   commit.

## 6. Required Fixes

None gating. Recommended at commit time: correct the `exit=270` → `exit=259` doc line
(finding 1); optionally regenerate a distinct border-after capture (finding 2).

## 7. Sign-off Decision

**PASS.**

Both human-reported defects are root-caused, fixed universally (no program-specific keying),
grounded in the references with the one genuine reference disagreement properly arbitrated and
recorded, protected by new deterministic regression tests that demonstrably fail on pre-fix
code, and verified by my own independent runs of both fast subsets (152/152 headless,
161/161 SDL3-ON), the end-to-end SDL3 boot (exit 0), and the openMSX A/B evidence set.
Recommended: commit as the next targeted-fix decision entry with the two low-severity
polish items addressed in the same commit.
