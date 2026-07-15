# M54 QA Sign-off — macOS Support (DEC-0081)

- Milestone: M54 (DEC-0081, REQ-M54-002)
- QA Owner: MSX QA Agent (macOS session, 2026-07-15)
- Package under test: `docs/m54-planner-package.md` rev-2 (amended)
- Machine: Darwin 25.5.0 arm64, AppleClang 21.0.0, cmake 4.3.4, ninja 1.13.2, SDL3 3.4.12, pwsh 7.6.3
- Tree state at assessment: HEAD `b9314fe`, **nothing committed**
- Persisted by: coordinator (the QA agent's harness routes findings to its final message; this file
  is a faithful transcription, not a re-derivation).

## Decision: **CONDITIONAL PASS**

The macOS side is genuinely done. Every macOS AC was **independently reproduced by QA** (not accepted
from the developer's report). Full Pass is withheld for exactly two reasons, both cheap to discharge
and both carrying **zero current exposure** because nothing is committed.

## 1. Regression scope

Build system (`CMakeLists.txt` warning matrix), repo EOL policy (new root `.gitattributes`), tooling
(`tools/bootstrap-build.sh`), docs (`README.md`, `CLAUDE.md`, `current-phase.md`), and two
`src/frontend` string/comment edits.

**Behavior-affecting: NO.** `src/core` and `src/devices` diffs are empty; the only `src/` changes are
string literals + a comment. openMSX A/B genuinely N/A per DEC-0081 (`decisions.md:1074`).

## 2. Regression matrix — QA-reproduced

| AC | Verdict | Evidence QA personally produced |
|---|---|---|
| **AC-M1** Ninja/AppleClang, 3 targets | **PASS** | clean `rm -rf build` → configure exit 0 → `[601/601]` exit 0; three Mach-O arm64 binaries |
| **AC-M2 p1** count | **PASS** | `100% tests passed, 0 tests failed out of 253` (119.41 s); cache confirms `SONY_MSX_ENABLE_SDL3:BOOL=ON`, `CMAKE_GENERATOR:INTERNAL=Ninja` |
| **AC-M2 p2** skip-set | **PASS** | `ctest -V` → skips are **exactly** tests #100, 101, 161, 190, 197, 198, 221, 236 = the enumerated 8. No unenumerated skipper. Tests 8/9 **RUN** (`#202 m28_c5 … Passed 8.68s` — log proves it read the real `msxdos23.dsk`; `#250 diskutils_bpb … Passed`) |
| **AC-M2 p3** delta==0 | **NOT PROVEN** | split verdict — see §4 |
| **AC-M3** determinism | **PASS** | `--create` twice → both `6f1a7983…b0ce5188`, `cmp` IDENTICAL, 737,280 B; POST_BUILD `diskutils/msx-disk` copy also matches |
| **AC-M4** exit matrix | **PASS** | 0 create/read/format · 1 no-args & unknown-flag · 2 missing file · 3 size guard · `--force`→0 reformats to golden |
| **AC-S0..S4** | **PASS** | see developer report |
| **AC-W1/W2/W3** | **NOT RUN** | requires the Windows machine; nobody has run them |

### ZEXALL — run despite no AC requiring it (QA initiative)

The planner scoped ZEXALL re-arming to *source* edits of `src/core`. QA judged this a **blind spot**:
M54 compiles the entire Z80 core with a **new compiler for the first time** — precisely where latent
UB surfaces — and the fast subset *excludes* ZEXALL.

`hbf1xv_m24_zexall_system_test … Passed 1559.21 sec` → **254/254 green on macOS.** The Z80 core is
instruction-exact under AppleClang. This retires a toolchain-change risk no AC covered.

## 3. Adversarial checks

- **(a) Tests weakened? NO — and the obvious check is vacuous.** `git diff tests/` is empty **but
  proves nothing**: `tests/` is gitignored (`.gitignore:219`), `git ls-files tests/` returns **0**.
  Git *cannot* detect a weakened test here. Real evidence, by mtime: newest file under `tests/` is
  `2026-07-15 15:50:02` — **before** the M53 commit (16:17) and the entire M54 window (17:06→23:18).
  Corroborated by 22 `-Wunused-result` warnings proving tests still invoke the real APIs.
- **(b) MSVC path changed? NO.** `if(MSVC)`, `add_compile_options(/utf-8)`, `endif()` are all
  **unchanged context**; the 5 deletions are **all comment lines**. MSVC flags byte-identical.
- **(c) `.gitattributes` churn? PROVEN INERT — empirically.** QA moved it aside (non-destructive
  `mv`) and re-measured: all **16** tracked pinned-extension files are already `i/-text w/-text`
  **without** it; only the `attr/` column differs. Restored byte-identical (`9f812d76…`).
  Decisive precondition: Mac `core.autocrlf=false`, all touched files `i/lf w/lf`, CR=0 → **the
  commit carries LF blobs; no flip.** R12 mitigated at source.
- **(d) Help-string edit breaks a test? NO.** No test asserts on sdl3 help text; 253/253 confirms.
- **(e) ONE build tree? YES.** Only `build`; `CMAKE_HOME_DIRECTORY=/Users/pashim/...`, Ninja.
  DEC-0041 holds.

## 4. AC-M2 part 3 — split verdict (the honest reading)

- **The 4 `roms/`-gated skips (tests 4, 5-partial, 6, 7) ARE measured on Windows.**
  `docs/asset-checksums.txt` was generated on Windows at `2026-07-15T15:50:56+09:00` — 26 min before
  the M53 baseline commit (16:17), i.e. contemporaneous with the 253 baseline. It records
  `Directory: D:\...\roms` / `Files: 1` (fmpac only). QA independently recomputed all 8 hashes:
  **0 mismatches, byte-for-byte.** So `roms/aleste.rom` and `roms/metalgear2_scc.rom` were absent on
  Windows too. Real evidence.
- **The 4 `games/`-gated skips (tests 1, 2, 3, 10) are NOT measured** — provenance inference only
  (`games/` was copied from Windows, so its layout is the Windows layout). The inference is strong
  but is reasoning, not measurement. **The Windows *count* cannot settle it**: had
  `games/roms/aleste2.rom` existed flat on Windows those tests would have RUN, probably PASSED, and
  the count would still read 253. This is exactly why AC-M2 p3 demands the literal skip list.
  Cost to discharge: ~2 minutes, riding along free with the AC-W1 run.

## 5. Residual risk

- **BLOCKER — AC-W1/W2/W3 unrun.** §7.2 calls these the HARD regression bar. By the package's own
  criteria M54 cannot close. **AC-W3.2 is not merely unrun — it is currently *unrunnable***: its real
  test is "does a Windows `git pull` churn the checkout", which requires the commit to exist.
  Sequence matters (Mac commit → push → Windows pull → gate).
- **MINOR — AC-W1's "after every portability edit" cadence not followed.** S1–S4 were batched with
  zero Windows re-gates. Unavoidable from the Mac and harmless given the tiny diff, but it is a
  criteria relaxation. **Ratify or record it — do not silently waive it.**
- **LOW — §4 case audit not fully discharged.** The volume is **APFS case-insensitive**, so a compile
  here cannot prove case-correct includes. Mitigating: zero `-Wnonportable-include-path` warnings
  (Clang enables it by default).
- **LOW — `-Wall -Wextra` newly surfaces 35 warnings, 0 errors.** 22 `-Wunused-result` **all in
  `tests/`** on deliberate side-effecting calls (`fdc.read_status()` clears INTRQ — discarding is
  intentional); `-Wunused-const-variable` at `src/devices/fdc/wd2793.cpp:37,41` (`kEFlag`/`kA0Flag`);
  `-Wunused-private-field` at `src/devices/video/vdp_scanline_accumulator.h:138`. None blocking
  (`-Werror` deliberately absent — with it, the first compile would have HARD-FAILED). → backlog.
- **LOW — pre-existing, platform-symmetric:** test #244 logs `softwaredb not found at
  references/openmsx-21.0/share/softwaredb.xml`. The file exists at repo root; the default path is
  CWD-relative by design (`src/machine/cartridge_identifier.h:36-39`) and ctest's cwd is
  `build/tests`. Identical on Windows → delta 0, not M54's. Cartridge SHA1 ID silently degraded to
  heuristic there. → backlog note.
- **LOW — `--vendored-sdl3` untested**, disclosed honestly in the script header and README, with a
  documented sticky-cache foot-gun (`CMAKE_PREFIX_PATH` persists until `rm -rf build`).

## 6. Defects found in the planning/dev artifacts

1. **Planner (new, QA-found):** §6 Step 4/5 said the binary is `build/msx_disk`; `OUTPUT_NAME` is
   `msx-disk` (`CMakeLists.txt:134`) so the prescribed command fails. The package contradicted itself
   (§2.5 was right). **FIXED by coordinator 2026-07-15.**
2. **Planner (developer-caught, QA-confirmed):** §6.4.2 row 10 said test 10 skips "Case 1 only".
   Reality: **Cases 1 AND 3** both skip.
3. **Planner (developer-caught, QA-confirmed):** "Gradius3 does NOT exist" — it does:
   `games/disks/gradius_3/G3.DSK`, 737,280 B, uppercase extension. QA fact-checked **every** claim in
   the developer's rewritten `CLAUDE.md` inventory against disk — all accurate, including "no
   `metalgear2_scc.rom` anywhere" (TRUE).
4. **Evidence hygiene (developer):** `debug/m54/s2-skip-list.txt` had an empty skip-set section and
   `debug/m54/s3s4-skips.txt` was 0 bytes — both from anchoring `grep '^SKIP'` against lines prefixed
   `NNN: `. Not fabrication (authoritative `s2-skip-set-final.txt` is correct and matched QA's run
   name-for-name) but misreadable as "no skips". **FIXED by coordinator** (removed; `final-state.txt`
   regenerated with the correct anchor; the stale 23:00 snapshot relabelled
   `s2-state-snapshot-2300-PRE-S4.txt`).

## 7. Conditions to clear before Pass

1. **Windows re-gate** (AC-W1/W2/W3 **+ AC-M2 p3**) — owner-executed. Blocker.
2. ~~Fix/annotate the broken evidence artifacts~~ — **DONE** (coordinator).
3. ~~Correct package §6 Step 4/5 binary name~~ — **DONE** (coordinator).
4. **Ratify or record** the AC-W1 per-edit-cadence relaxation.

## 8. Owner-executed Windows gate

Sequence is load-bearing: **AC-W3.2 cannot be tested until the commit exists.** Mac commit + push
first, then Windows pull + gate. The exact command set is reproduced in
`docs/m54-windows-regate.md`.
