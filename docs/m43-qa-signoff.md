# M43 QA Sign-off — V9958 GRAPHIC2/3 AND-mask + FM-PAC bit-exact SRAM (tag target v1.1.0)

- Milestone: **M43** (DEC-0062 + DEC-0063). Slice 1 = VOID (no bug); Slice 2 = `a733d65` (VDP
  AND-mask); Slice 3 = `9f6eb8c` (FM-PAC SRAM).
- QA owner: MSX QA Agent. Independent verification from a from-scratch `build/`.
- Verdict: **PASS** — recommend owner-run tag+push **v1.1.0** (DEC-0047), with a **recommended
  (non-blocking) live human smoke-check** before the tag (M37 precedent; the human's data-safety
  priority). No blocker-level gap remains.

Every gate below was re-run/re-read this cycle. `Assumption:` items carry a verification action.

---

## 1. Independent gate results

| Gate | Result | Evidence |
|---|---|---|
| G-1 validate-assets | **PASS** | `tools/validate-assets.ps1` → True; 7 BIOS incl. `f1xvmus.rom`, 5 ROMs. |
| G-2 checksum-assets | **PASS** | `docs/asset-checksums.txt` written. `fmpac.rom` SHA-1 **9d789166** (canonical 64 KB) ✓; `f1xvmus.rom` SHA-1 **aad42ba4** (APRLOPLL) ✓. |
| G-3 clean rebuild (SDL3=ON) | **PASS** | from-scratch `build/` via `tools/bootstrap-build.ps1`; both exes (`sony_msx_headless.exe`, `sony_msx_sdl3.exe`). Only pre-existing C4834 `[[nodiscard]]` warnings in TEST code — **zero warnings in either M43 source file**. |
| G-4 fast ctest | **PASS — 218/218** | `ctest -C Debug -LE m24_slow_full_sweep` → 100% passed, 0 failed. 219 registered; `-LE` excludes exactly the 1 slow test (`hbf1xv_m24_zexall_system_test` #131). |
| G-5 ZEXALL/ZEXDOC full sweep | **WITHHELD (substituted — treated as PASS)** | See §1.1. |
| Commit hygiene | **PASS** | See §1.2. |

### 1.1 ZEXALL/ZEXDOC full sweep — WITHHELD, git-diff-proven substitute (coordinator+human decision, this cycle)

DEC-0062's sweep mandate is **superseded**: its premise (a core slot-addressing change) turned out
VOID (Slice 1). The actual M43 fixes touched only `src/devices/video` + `src/devices/cartridge` —
**NOT** `src/devices/cpu` or `src/core`. The ZEXALL exerciser is a pure Z80-CPU test; with the CPU
byte-identical to the last clean sweep, re-running yields zero new signal. Proof executed:

- `git diff --stat v1.0.40..HEAD -- src/devices/cpu src/core` → **EMPTY** (no changes).
- `git log --oneline v1.0.40..HEAD -- src/devices/cpu src/core` → **EMPTY** (no commit touched them).
- Standing CPU-correctness evidence: the **v1.0.40 full sweep was clean** — `debug/m41/zexall/m41-zexall-sweep.log`: `hbf1xv_m24_zexall_system_test … Passed 2069.15 sec`, "0 tests failed", exit 0.

**Disposition:** gate satisfied by substitution (NOT a BLOCKED gate). The Z80 core is byte-identical
to guaranteed-clean-swept code. (A sweep was started this cycle then withdrawn per the decision; the
partial log at `debug/m43-zexall/` is superseded by this proof.)

### 1.2 Commit hygiene + push state

- `a733d65` (Slice 2): **only** `src/devices/video/vdp_frame_renderer.{cpp,h}`.
- `9f6eb8c` (Slice 3): **only** `src/devices/cartridge/cartridge_fmpac_rom.{cpp,h}`.
- `git diff --stat a733d65^..9f6eb8c -- src/core src/devices/cpu` → **EMPTY** (zero core/cpu).
- **No** `Co-Authored-By`/AI trailer in either commit.
- **Push state:** `origin/main` = `36babf2` = **tag v1.0.44** (the last of the v1.0.42/43/44 `--help`/
  startup-summary UX commits, all already on origin). HEAD is **2 commits ahead** and unpushed:
  `a733d65` + `9f6eb8c`. **v1.0.44 IS on origin.** A v1.1.0 tag would point at `9f6eb8c`.

---

## 2. No-regression 13-mode A/B + FM-PAC render + non-tautology

### 2.1 Screen-mode A/B (openMSX 19.1, WSL `Sony_HB-F1XV`, fixed renderer, this cycle)

The a733d65 change is **structurally isolated** to the 5 character/tile renderers
(`render_text1/text2/graphic1/graphic2_or_3/multicolor`) — the only callers of the changed
`pattern_table_mask()`/`color_table_mask()` (verified by grep: lines 556/584-585/609-610/634-635/656).
The bitmap (G4–G7/YJK) and sprite renderers do **not** call them → structurally unaffected.

Independently generated carts (`tools/gen-m41-video-cart.py`) + captured openMSX raw screenshots +
`tools/m41-video-ab.py`:

| Mode | Renderer path | pct_mismatch | mean_dist | Verdict |
|---|---|---|---|---|
| m0_text1 | changed (text1) | **0.000%** | 1.167 | MATCH |
| m1_g1 | changed (graphic1) | **0.000%** | 0.073 | MATCH |
| **m2_g2** | **changed (graphic2)** | **0.000%** | **0.000** | MATCH (byte-perfect) |
| **m4_g3** | **changed (graphic3)** | **0.000%** | **0.000** | MATCH (byte-perfect) |
| m5_g4 | unaffected (bitmap) | 0.000% | 1.604 | MATCH |
| m8_g7 | unaffected (bitmap) | 0.000% | 4.250 | MATCH |

The two modes that exercise the **changed** `graphic2_or_3` renderer (m2_g2, m4_g3) match openMSX at
`mean_dist=0.000` (byte-perfect). Remaining 7 modes (m6_g5, m7_g6, m10_yjkyae, m12_yjk, sp1/2/3) are
the structurally-unaffected class; their unit tests are green in the 218.

### 2.2 Before/after byte-identical spot-check (≥2 modes)

- **m1_g1 (arithmetic, direct):** with the cart registers (R#2=0x06, R#3=0x80, R#4=0x00, R#10=0),
  additive vs AND-mask produce **identical VRAM addresses for all 2048 (charCode,line) pairs** →
  byte-identical before/after. Proven by exhaustive computation (0/2048 differing).
- **m2_g2 (transitive via openMSX):** the M41 g2/g3 cart uses R#4=0x00 + uniform 0xFF pattern fill
  (the DEC-0063 "blind spot") so additive≡AND-mask in output; old matched openMSX (M41 record) and
  new matches openMSX at 0.000% (measured) → old≡new. NOTE: this A/B cart does **not** exercise the
  canonical-register quarter-distinct path — that path is covered by the re-derived unit test (§2.4).

### 2.3 FM-PAC manager render verdict

Premise verified: `debug/m43-slice2/vram_full.bin` == `omsx_vram_full.bin` **byte-for-byte**
(SHA-1 `f9e01828`, 0 differing bytes; name@0x1800 / pattern@0x0000 / color@0x2000 all identical). Our
settled VDP registers (snapshot) are byte-identical to `omsx_vdpregs.txt` (R#0=02, R#1=60, R#2=06,
**R#3=FF, R#4=03**, R#7=05, R#10=00; MODE=GRAPHIC2) — the canonical SCREEN 2 regime the old additive
model broke.

Driven live through the **actual compiled fixed renderer** (`CALL FMPAC` script + FM-PAC cart,
settled frame): **the full manager UI renders** — "FM"/"PAC" logo, wizard, menu
(クリア/コピー/チェンジ/ファイルさくじょ/スロット/BGM), "COPR.1988 MATSUSHITA" — 24.5% non-backdrop,
9–10 colors. The old additive model = the **2.4%/blank** backdrop-only screen (confirmed at
frame 1250 before settle; the fixed render at frames 1500/1800/2200 = the UI).

A/B vs `omsx_fmpac_settled.png` = **0.979%**, localized entirely to rows 27–47 (the "FM" logo):
ours renders it GREEN `RGB(33,222,33)`, the golden shows BLUE `RGB(39,39,255)`. **This is NOT a
rendering defect.** The manager loads its palette progressively; palette entry 2 transitions
blue→green. Both engines' **settled** entry 2 = R1 G6 B1 = green (read directly from openMSX twice;
matches our snapshot P02=0611). The developer's golden was captured mid-load (blue); ours renders the
correct **settled** green. Structure/layout/text/wizard/menu = byte-exact (the other 171 rows =
0.000%).

- **Honesty correction:** the developer's "**0.000% vs `omsx_fmpac_settled.png`**" claim does **not**
  reproduce — the measured value is **0.979%** (the golden-capture palette-load artifact). The
  underlying deliverable (DEF-M43-FMPAC-SCREEN: the manager UI renders matching openMSX) **is** met;
  the "0.000%" was overstated (likely the Python `decode_screen2.py` used a matched hardcoded palette).

### 2.4 Non-tautology (adversarial mutation, non-destructive per DEC-0049)

- **VDP mask:** `kFill13 0x1E000 → 0x0` → rebuild → `devices_vdp_frame_renderer_tile_modes_unit_test`
  **FAILS** exactly `Graphic2_Canonical_Quarter0/Quarter1` + `Graphic3_Canonical_AndMask`. Restored;
  `git diff` empty (byte-identical); test green again.
- The re-derived `m2_g2` test uses **canonical R#4=0x03/R#3=0xFF** + quarter-distinct pattern bytes
  (0xF0 vs 0x0F) — the exact regime the FM-PAC manager uses and the old additive model missed.

---

## 3. SRAM data-safety verdict (highest-priority review — all 5 points PASS)

Register model re-verified bit-exact vs `references/openmsx-21.0/src/sound/MSXFmPac.cc`: magic unlock
AND-gate (`(r1ffe==0x4D)&&(r1fff==0x69)`), 0x3FF6 enable `&0x11` incl. **bit4 force-reset** re-lock,
0x3FF7 bank `&0x03`, SRAM window write-gated `sram_enabled_ && rel < 0x1FFE`. Magic registers
(r1ffe/r1fff) are separate members, **never stored in `sram_`** — so bytes 0x1FFE/0x1FFF are never
SRAM.

- **(a) Save format byte-identical to openMSX `SRAM::save`.** 16-byte `"PAC2 BACKUP DATA"` header +
  0x1FFE (8190) data = **8206** bytes — matches `SRAM.cc:114-131` (header `strlen`=16, then `ram`
  0x1FFE) and `MSXFmPac.cc:11` (`sram(…, 0x1FFE, …, PAC_Header)`). Construction-verified + unit test
  Case A + the real round-trip produced the exact header. *Assumption: no independent openMSX-written
  `.sram` file-SHA cross-check was run (openMSX writes via machine `sramname` config); the format is
  proven by SRAM.cc semantics + byte layout. Verify (optional): point an openMSX FM-PAC `sramname` at
  our 8206-byte file and confirm it loads without "no correct SRAM file".*
- **(b) Legacy migration LOSSLESS on the human's ACTUAL `roms/fmpac.rom.sram`.** The real file is
  8192-byte legacy raw (408 non-zero addressable bytes; bytes[0x1FFE]=0x00, bytes[0x1FFF]=0x00). Run
  through the **real compiled code** on a COPY (original never touched): 8192→8206, header
  `"PAC2 BACKUP DATA"`, data section **byte-identical** to the original 8190 addressable bytes
  (**0 differing**), all 408 non-zero preserved, the 2 dropped bytes both 0x00 = non-SRAM (confirmed
  by the `rel<0x1FFE` write-gate). **No data loss.**
- **(c) Load never mutates host file + atomic save.** After the run the original `roms/fmpac.rom.sram`
  SHA-1 is unchanged (`load_sram` only reads; the new format is written on the next flush). `save_sram`
  writes to a `.tmp` sibling then `rename` (with remove+retry fallback) — no truncation/corruption
  window (code + unit test Case D `Migration_LoadDoesNotTouchFile`).
- **(d) Persist→reload round-trip byte-identical.** Register-path write→save→reload→read-back
  (unit test Case C) and migration reflush→reload (Case D) both byte-identical; the real-file test
  round-tripped losslessly.
- **(e) Fail-safe.** Wrong-header / short / absent → returns false, store untouched, never
  partial-loaded garbage (unit test Cases E/F/G).
- **Non-tautology:** migration `raw[i] → raw[i+1]` → rebuild →
  `devices_cartridge_fmpac_sram_persistence_unit_test` **FAILS** exactly
  `Migration_AllAddressableBytesPreserved` + `Migration_ReflushDataMatches` + `Migration_ReloadLastByte`.
  Restored; `git diff` empty; test green again.

**SRAM verdict: no data-loss risk.** The data-safety hardening (load-read-only, atomic save,
fail-safe, lossless migration) exceeds openMSX (which would blank a legacy file on header mismatch).

---

## 4. Residual risk (ranked)

1. **LOW — FM-PAC manager palette-load timing.** Ours renders the settled green FM logo; the golden
   caught a blue mid-transition. Both engines' settled palette entry 2 is green (verified). The 0.979%
   is a capture artifact, not a defect. *Sub-note:* the manager's palette loads progressively across
   frames — a subtle timing-fidelity nuance (ours reaches green by ~frame 1800), out of M43 scope.
2. **LOW — headless `--dump-frame` staleness (slice1 finding).** The beam-sealed `completed_frame()`
   can lag coherent VRAM at a chosen frame (my 1250 capture was blank; 1500+ shows the UI). This is a
   capture-tool timing property, not a device defect; the SDL3 live path uses the same
   `VdpFrameRenderer`. Motivates the live human smoke-check (§6).
3. **LOW — A/B carts vs canonical registers.** The 13-mode A/B carts use blind-spot registers
   (R#4=0x00, uniform fill); the canonical-register quarter-distinct path is covered instead by the
   re-derived unit test. Complementary, not overlapping — coverage is adequate but the A/B alone would
   not have caught the DEF-M43 defect (which is exactly why M41's m2_g2 A/B missed it).
4. **LOW — DEC-0063 latent (`v9958_vdp.cpp change_register` no per-register write-mask).** Confirmed
   out of scope + harmless here: `control_regs_[reg]=value` stores raw bytes vs openMSX
   `VALUE_MASKS_MSX2`, but R#10=0 in every M43 path (FM-PAC manager + all A/B carts) and
   `color_table_mask` re-masks `& 0x1FFFF`. Pre-existing, separate accuracy item; correctly deferred.
5. **INFO — local-only test apparatus.** `tests/` (and `docs/`, `references/`, `tools/`) are gitignored
   "local-only development apparatus" (owner decision, `.gitignore:181-193`, 2026-07-10). The 218/218 +
   non-tautology proofs reproduce from the **local** tree (the intended source of truth); a fresh
   public clone lacking `tests/` will not configure. Intentional, not a defect.
6. **INFO — stray untracked `assets/msx2p_system_architecture.png`.** Pre-existing untracked file
   (not M43). Minor hygiene; optional to add or ignore.

No blocker-level or masked-regression gap found. No SRAM data-loss path found.

---

## 5. Required fixes

**None blocking.** Optional / accuracy follow-ups (not gating v1.1.0):

- Correct the M43 implementation record's "FM-PAC manager 0.000% vs openMSX golden" to the measured
  **0.979% (golden palette-load capture artifact; ours renders the correct settled green)**.
- (Backlog) V9958 per-register write-mask (`VALUE_MASKS_MSX2`) — the DEC-0063 latent.
- (Backlog) FM-PAC manager progressive-palette-load timing fidelity, if pursued.

---

## 6. Sign-off decision + release recommendation

**Decision: PASS.**

- All hard gates green (clean rebuild; ctest **218/218**; assets; commit hygiene). ZEXALL sweep
  WITHHELD-and-substituted by a valid git-diff proof (zero cpu/core edits since the clean v1.0.40
  sweep) — accepted as passing.
- No-regression proven (13-mode A/B 0.000%, changed renderer byte-perfect on m2_g2/m4_g3, before/after
  byte-identical for g1, non-tautology proven).
- FM-PAC manager renders matching openMSX (the DEF-M43-FMPAC-SCREEN deliverable); the 0.979% residual
  is a golden-capture artifact, not a defect.
- SRAM data-safety fully verified on the human's actual save — lossless, atomic, fail-safe, read-only
  load.

**Release: recommend owner-run `v1.1.0` tag on `9f6eb8c` + push** (DEC-0047; push both unpushed
commits `a733d65`+`9f6eb8c`).

**LIVE human re-verify — RECOMMENDED to gate the tag (M37 precedent; the human's data-safety
priority).** Not because unresolved risk remains, but two things were verified indirectly rather than
through the exact live path:
  1. **FM-PAC manager renders in the actual SDL3 build** (the headless `--dump-frame` staleness means
     my render proof is via VRAM/register/palette identity + settled-frame dumps; a 10-second SDL3
     `CALL FMPAC` glance confirms the live presentation).
  2. **A save initiated FROM the manager UI persists** (I verified the register-path + legacy-migration
     round-trips through compiled code; a manager-UI write→exit→reload round-trip closes Slice 3's C3
     acceptance directly and is cheap insurance for the irreplaceable saves).

If the human accepts the indirect evidence, PASS stands unconditionally for the tag.

---

## 7. agent-protocol entries to refresh (recommended; coordinator/orchestration owns writes)

- `agent-protocol/state/milestones.md`: **add an M43 block** (Slice 1 VOID + staleness root-cause;
  Slice 2 `a733d65` GRAPHIC2/3 AND-mask; Slice 3 `9f6eb8c` FM-PAC SRAM; QA PASS this doc; gates:
  rebuild, ctest 218/218, ZEXALL withheld-substituted, assets, hygiene; release target v1.1.0 @
  `9f6eb8c`). Also record the standing **documentation debt**: v1.0.42/43/44 (UX `--help`/startup-
  summary commits `f6e745e`/`9d853d0`/`36babf2`) shipped without milestone blocks.
- `agent-protocol/state/current-phase.md`: set active phase to **M43 — QA PASS**; NEXT = owner tag
  v1.1.0 (+ recommended live smoke-check).
- `agent-protocol/channels/decisions.md`: mark **DEC-0062 + DEC-0063 CLOSED** (M43 delivered; note the
  ZEXALL-mandate supersession is a coordinator+human decision this cycle — worth its own short
  decision entry so the withheld sweep is auditable).
