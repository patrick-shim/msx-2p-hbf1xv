# QA Review — Sprite Mode 2 Attribute-Table Mask Fix (Metal Gear Sprite Invisibility)

- Date: 2026-07-08
- Reviewer: MSX QA Agent
- Subject: coordinator-authorized targeted bug fix (DEC-0026/DEC-0028 precedent), UNCOMMITTED
  working-tree change vs `v1.0.28` + `67fd3b8` (DEC-0028), reviewed before commit.
- Developer report: `docs/sprite-invisibility-investigation.md` (read in full).
- Regression Scope: `src/devices/video/sprite_engine.cpp` (mode-2 attribute addressing only);
  test collateral (`tests/unit/devices/video_sprite_engine_mode2_attribute_masking_unit_test.cpp`
  new, two existing sprite tests' register programming, `tests/CMakeLists.txt` registration);
  `docs/asset-checksums.txt` regeneration. Affected flows: every sprite-mode-2 title
  (MSX2/2+ SCREEN 4-8), sprite mode 1 (must be unchanged), planar (G6/G7) sprite table reads,
  S#0/collision status derivation, frame rendering determinism.
- Test Matrix Reference: full deterministic suite (147 registered tests; fast subset = 146,
  slow label `m24_slow_full_sweep` = `hbf1xv_m24_zexall_system_test` only), plus targeted
  adversarial and end-to-end checks described below.

## 1. Independent verification of the fix semantics (checklist item 1)

Re-derived from the references (read directly, not from memory):

- `references/openmsx-21.0/src/video/VDP.cc:1357-1371` (`updateSpriteAttributeBase`):
  `baseMask = (R#11<<15) | (R#5<<7) | ~(~0u<<7)`, `indexMask = ~0u<<7` (mode 1) / `~0u<<10`
  (mode 2).
- `references/openmsx-21.0/src/video/VDPVRAM.hh:263-279` (`readNP`/`readPlanar`): effective
  address = `effectiveBaseMask & index` with the caller setting unused index bits to one, i.e.
  `baseMask & (indexMask | tableIndex)`.
- `references/openmsx-21.0/src/video/SpriteChecker.cc:283-330` (`checkSprites2`): Y/X/pattern
  block at table index 512 (`getReadArea<32*4>(512)` / `getReadAreaPlanar<32*4>(512)`), per-line
  color at index `(~0u<<10) | (sprite*16 + spriteLine)` (lines 312/357).

The fix's `mode2_attr_addr(index) = ((R#11<<15)|(R#5<<7)|0x7F) & (~0x3FFu | index)` with indices
`512 + sprite*4 + {0,1,2}` and `sprite*16 + line` is exactly the reference formula. I recomputed
the concrete cases by hand: Metal Gear (`R#5=0xE7`,`R#11=0x01`) -> baseMask `0xF3FF`, Y/X/pattern
at `0xF200+off`, colors at `0xF000+off`; BIOS SCREEN5 (`R#5=0xEF`,`R#11=0`) -> `0x7600`/`0x7400`;
pre-fix naive reads at `0xF580`/`0x7980`. All match the investigation and the new test's
expectations.

### 1a. Mode 1 genuinely unchanged — VERIFIED

The diff touches only the mode-2 branch and the mode-2 per-line color read. The retained mode-1
plain-add (`attrib_base + sprite*4 + k`) is provably equivalent to the reference mask formula:
mode-1 indices are < 128 (bits 0-6 only), `baseMask`'s low 7 bits are hardwired to 1 by the
`|0x7F` term, and `attrib_base`'s bits 0-6 are zero by construction, so
`baseMask & (indexMask|index) == attrib_base | index == attrib_base + index`. In mode 1 all 8
R#5 bits are genuine base bits (indexMask covers only bits 0-6). Same argument holds for the
sprite pattern table (`R#6<<11`, offsets < 2048 vs `|0x7FF`): the retained plain-add is exact.

### 1b. Planar (G6/G7) equivalence — VERIFIED EQUIVALENT (the high-risk claim, checked)

openMSX transforms the MASKS into planar space (`VDP.cc:1366-1368`) and then permutes the INDEX
in `readPlanar` (`VDPVRAM.hh:274-279`: `index = ((index&1)<<16) | ((index&0x1FFFE)>>1)`); our
code combines mask and index in logical space first, then applies `planar_address()`
(`sprite_engine.cpp:20-23`, the same `(masked>>1) | ((masked&1)<<16)` permutation). Let `p` be
the 17-bit rotate-right-by-1 bit permutation (bit0->bit16, bit k->bit k-1):

- openMSX's transformed baseMask `((baseMask<<16)|(baseMask>>1)) & 0x1FFFF` is exactly
  `p(baseMask)` on the 17-bit space; its transformed indexMask is `p(indexMask)` (checked by
  direct bit computation: `~0u<<10` -> bits 9-15 set, bit16 clear).
- openMSX effective planar address = `p(baseMask) & p(indexMask | index)`; ours =
  `p(baseMask & (indexMask | index))`. A bijective bit permutation distributes over bitwise
  AND/OR, so these are equal for ALL register values and ALL indices — not merely the mask-bits-
  set software convention. No planar edge case exists where the two formulations differ.
- Residual difference in read granularity: openMSX masks ONCE per 128-byte area for the
  Y/X/pattern block (`getReadArea`/`getReadAreaPlanar`) and indexes contiguously; the fix masks
  EVERY read. These coincide because indices 512-639 vary only in bits 0-6 and baseMask's low 7
  bits are hardwired 1 — openMSX itself asserts this precondition
  (`VDPVRAM.hh:253: assert((areaBits & effectiveBaseMask) == areaBits)`). Per-read masking is
  the strictly more general form and agrees on every reachable address.
- Planar predicate identical: `vdp_base_is_planar` (`src/devices/video/vdp_mode.h:73-75`,
  `(base & 0x14) == 0x14`) == `DisplayMode::isPlanar()`
  (`references/openmsx-21.0/src/video/DisplayMode.hh:140-143`).

Verdict: the developer's equivalence assertion is correct, and stronger than claimed (holds for
arbitrary register values, including cleared mask bits, reproducing hardware aliasing).

Informational (pre-existing, unchanged): `V9958Vdp` stores control-register writes unmasked
(`v9958_vdp.cpp:281`), unlike openMSX's per-register `controlValueMasks` (e.g. R#11 -> 0x03).
No observable divergence results here because both pipelines truncate effective addresses to 17
bits; noted only for completeness.

## 2. Universal-fix requirement (checklist item 2) — PASS

The diff contains zero game-specific logic: pure register/address semantics derived from the
V9958's documented addressing model. "Metal Gear" appears only in comments/test names. Both
test cases exercise generic register shapes (the game's and the BIOS default's), and the fix
provably applies to every sprite-mode-2 title.

## 3. Discriminating-test check (checklist item 3) — PASS (adversarial validation)

- Pre-fix snapshot: `git worktree add --detach <scratch>/wt-v1028 v1.0.28`; the snapshot's
  `sprite_engine.cpp` verified content-identical to `git show v1.0.28:...` (CRLF-only diff).
  New test file copied in and registered; built against the pre-fix tree (build at a short path
  after an MSVC MAX_PATH failure at the scratchpad depth).
- Result on PRE-FIX code: exit 1 with exactly the predicted 5 case failures —
  `MetalGearShape_SpriteVisibleOnLine177`, `MetalGearShape_FrameBufferShowsSpritePixel`,
  `MetalGearShape_FrameBufferShows16thColumn`, `BiosScreen5Shape_RealSpriteVisibleOnLine101`,
  and critically `BiosScreen5Shape_DecoyAtNaiveAddressNotRead` (the decoy planted at the naive
  pre-fix address 0x7980 IS read by the buggy engine).
- Result on CURRENT (fixed) tree: exit 0 (also passing inside the full suite run below).
- Worktree and its build directory removed afterwards; `git worktree list` shows only the main
  tree.

The test discriminates in both directions and would catch a regression of this exact bug.

## 4. Existing-test updates (checklist item 4) — legitimate correction, no weakening

Both updated files add ONLY `set_register(vdp, 5, 0x07)` in their mode-2 table-select helpers
(diff-verified; no assertion changed). Their layouts (colors at 0, Y/X/pattern at 512,
`kAttribBase=0`/`kYxpBase=512` and `kM2AttribBase=0`/`kM2YxpBase=512`) are unchanged. With
`R#5=0x07` the baseMask is `0x3FF`, mapping table index -> address identically (colors 0-511,
Y/X/pattern 512-1023), i.e. the exact layout the tests already used — now programmed the way
real hardware/software requires. The old `R#5=0x00` only worked because the engine was wrong in
the same direction (under correct semantics it aliases all reads into 0x00-0x7F). The oracles
these tests verify are untouched; on pre-fix code the updated tests would now fail, which is
correct behavior for hardware-true programming.

## 5. Regression execution (checklist item 5, per the refined slow-cadence rule)

- Fresh clean build: `cmake -S . -B build-qa2 -DSONY_MSX_ENABLE_SDL3=OFF` +
  `cmake --build build-qa2 --config Debug` — exit 0.
- QA fast subset (independent, clean build): `ctest --test-dir build-qa2 -C Debug
  -LE m24_slow_full_sweep` — **146/146 passed** (12.95-13.18 s, two runs), including the new
  masking test (#112) and both updated sprite tests. Durable log:
  `build-qa2/Testing/Temporary/LastTest.log` (146 "Test Passed" sections, 0 failed).
- QA full-suite run: intentionally ABORTED mid-`hbf1xv_m24_zexall_system_test` by the
  coordinator per the human's refined cadence directive; **126/126 tests passed** before the
  abort (captured in the task log). Not a failure of the change.
- Refined cadence applicability: the change touches only `src/devices/video/sprite_engine.cpp`,
  a pull-model renderer component never exercised by ZEXALL/ZEXDOC — the slow sweep is NOT
  required for this change under the refined rule.
- Developer's full-suite claim (147/147 incl. sweep, both `error_markers==0`): stated in
  `docs/sprite-invisibility-investigation.md` §7 and confirmed by the coordinator from
  `build/Testing/Temporary/LastTest.log`. At QA review time that file had already been
  overwritten by the developer's subsequent fast-subset run (146 passed / 0 failed sections,
  session start 19:13) — the full-sweep log is therefore no longer independently verifiable on
  disk. Non-gating under the refined rule (sweep not required for this component); see
  follow-up F3.
- Asset gates re-run by QA: `tools/validate-assets.ps1` — pass (7 BIOS files, 2 ROMs incl.
  `metalgear.rom`); `tools/checksum-assets.ps1` re-run to a scratch file — matches
  `docs/asset-checksums.txt` exactly except the "Generated at" timestamp line.

## 6. Scope check (checklist item 6) — PASS

- `git diff v1.0.28 --stat -- src/devices/cpu/ src/core/` — empty.
- Uncommitted change set (`git diff HEAD --name-only`): `src/devices/video/sprite_engine.cpp`,
  `tests/CMakeLists.txt`, the two updated test files, `docs/asset-checksums.txt`, plus
  `.claude/settings.json` (unrelated pre-existing local edit — see F2) and the new untracked
  test + investigation doc. `src/main.cpp` byte-identical to `v1.0.28` (diagnostic scaffolding
  genuinely reverted).

## 7. Evidence PNGs (checklist item 7) — PASS

All three read and viewed directly:

- `debug/frames/mg-sprite-invisibility-before.png`: MG first gameplay screen (dock), no player
  sprite anywhere.
- `debug/frames/mg-sprite-invisibility-after.png`: same scene, player sprite visible at the
  bottom-left (x~42, y~177).
- `debug/frames/mg-sprite-invisibility-openmsx-reference.png`: openMSX raw screenshot of the
  same scene with the sprite at the same position — corroborates expected appearance.

Identical file sizes (163,106 bytes) for before/after/title were checked and are benign: the
PNG writer emits uncompressed stored blocks, so equal-dimension frames yield equal-size files;
SHA256 hashes differ.

## 8. Independent end-to-end reproduction (checklist item 8) — PASS, byte-exact

QA wrote its OWN harness (scratchpad-only, public machine API, per the investigation §1 recipe:
900 frames -> SPACE 5 frames -> 600 frames, `step_cpu_instruction()` to each
`frame_cycles_per_frame()` boundary + `on_vsync_boundary()`), linked against the built core lib:

- FIXED tree: gameplay register file `R#0=06 R#1=62 R#2=1F R#5=E7 R#6=1F R#8=08 R#9=80 R#11=01
  R#23=00 R#25=00` — byte-identical to the investigation's and the documented openMSX A/B
  sample. Sprite engine: 16 consecutive visible-sprite lines starting at line 177, x=42 (the
  16x16 player at SAT Y=0xB0/X=0x2A). Frame dump -> `tools/frame-to-png.py` -> PNG SHA256
  `e67845b1...39ac` — **byte-identical to the developer's after.png**.
- PRE-FIX tree (same harness linked against a v1.0.28 build): same register file, phantom
  Y=0/X=0 sprites on lines 1-16 only (zero pattern/color -> invisible), nothing at line 177;
  PNG SHA256 `1e7dc436...bcd` — **byte-identical to the developer's before.png**.

This independently reproduces the entire headline evidence chain from scratch, and doubles as a
cross-harness determinism confirmation.

## 9. Failures and Risk Ranking

No Critical/High/Medium findings. The change is correct, grounded, universal, and regression-
clean on all executed evidence.

Low / follow-ups (none blocking):

- **F1 (Low, process):** append the decisions-channel entry for this fix (DEC-0026/DEC-0028
  precedent) to `agent-protocol/channels/decisions.md` in the closure commit.
- **F2 (Low, hygiene):** `.claude/settings.json` carries an unrelated pre-existing local edit
  ("sonnect"->"sonnet"); it must not be swept into the fix commit.
- **F3 (Low, process):** when a full slow sweep IS required and cited as evidence, capture it to
  a durable file (precedent: `build/full_sweep_vrhr.log`) — this cycle's full-sweep LastTest.log
  was overwritten by a later fast run before QA could re-read it.
- **F4 (Low, recorded):** M22 probe re-baseline with mask-bit-correct `R#5` programming
  (`tools/gen-m22-sprite-cmd-probe.py:157` pins `R#5=0x00`; verified) stays deferred per
  investigation §6.3; the new unit test covers the semantics directly in the meantime.

## 10. Sign-off Decision

**PASS.**

Sign-off conditions (commit-time actions, not re-review triggers): F1 and F2 above.

The fix is a faithful, universally-applicable implementation of the V9958 sprite-mode-2
attribute-table AND-mask semantics, independently verified against the openMSX reference
sources at the formula level (including the planar transform-order equivalence, proven, not
assumed), guarded by a regression test proven to discriminate against the pre-fix code, and
its end-to-end evidence was reproduced by QA byte-for-byte from scratch on both sides of the
fix.
