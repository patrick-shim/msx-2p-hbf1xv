# M54 Implementation Report — macOS Support (DEC-0081 / REQ-M54-002)

- Milestone: M54 (DEC-0081), slices S0–S4
- Developer Owner: MSX Developer Agent (macOS session)
- Coordinator: main session (executed S0; persisted this report — the developer agent's harness
  routes findings to its final message rather than writing `.md` files)
- Date: 2026-07-15
- Package: `docs/m54-planner-package.md` rev-2 (amended)
- Machine: Darwin 25.5.0 arm64 · AppleClang 21.0.0 · cmake 4.3.4 · ninja 1.13.2 · SDL3 3.4.12 · pwsh 7.6.3
- Tree state: HEAD `b9314fe`, **nothing committed**, ONE `build/` tree

## Outcome

**macOS build + suite GREEN.** 253/253 fast subset; 254/254 including the full ZEXALL sweep. All
three targets build as Mach-O arm64. Cross-platform byte-determinism **proven** (AC-M3). The Windows
re-gate (AC-W1/W2/W3 + AC-M2 p3) remains outstanding and is owner-executed —
see `docs/m54-windows-regate.md`.

## S0 — tree hygiene (coordinator)

The transfer from Windows brought contamination the rev-1 package never modelled. Runbook Step 0
Option A said "exclude `build/`"; it came anyway.

- **(a) Stale Windows `build/` DELETED.** It held `CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026`
  and `CMAKE_HOME_DIRECTORY:INTERNAL=D:/Projects/sony-msx-hbf1xv`. Any Mac configure hard-fails
  against it on **two** independent counts (source-dir mismatch *before* generator mismatch).
  Deleting **upholds** DEC-0041 (one tree, regenerated for the host); creating `build-mac/` to dodge
  the error would have been the violation.
- **(b) CRLF restored** on `CMakeLists.txt`, `README.md`, `sony_msx_hbf1xv.xml` via
  `git checkout --`. **Load-bearing:** all three were CRLF-on-disk over LF blobs. S1's first action
  edits `CMakeLists.txt` — committing it as CRLF would have flipped the tracked blob in a ~206-line
  whole-file rewrite and propagated to Windows, violating the package's own AC-W3. Content-identity
  was proven first (`git diff --ignore-cr-at-eol` EMPTY; 604/604 mirror diff), so the restore
  discarded only carriage returns.
- **(c) skip-worktree** on `roms/fmpac.rom.sram` verified intact (`S`) — survived because `.git` was
  copied rather than re-cloned. No risk of committing FM-PAC save churn to the public remote
  (DEC-0047-AMENDMENT-C).
- **(d) Prereqs installed:** `ninja` 1.13.2 + `pwsh` 7.6.3. cmake/SDL3/AppleClang were already
  present. **Package defect found here:** Step 1's `brew install --cask powershell` FAILS — Homebrew
  moved PowerShell from a cask to a **formula**; correct command is `brew install powershell`. Fixed
  in rev-2.

## S1 — CMake platform matrix + `.gitattributes`

`CMakeLists.txt` **+16 / −5, one hunk** (not a whole-file rewrite → R12 did not fire):

```cmake
 if(MSVC)
     add_compile_options(/utf-8)
+else()
+    add_compile_options(-Wall -Wextra)
 endif()
```

The `/utf-8` line is an **unchanged context line** and all 5 deletions are comment lines → the MSVC
path is byte-equivalent. `else()` rather than `if(APPLE)` so a future Linux/GCC host inherits the
same regime. **No `-Werror`** — MSVC has no `/WX` counterpart, so adding it would make the toolchains
asymmetric.

New root **`.gitattributes`**: 9 binary pins + `*.sh text eol=lf`. **No `* text=auto`** (§3.2 forbids
it — that is the mass-renormalization event). QA proved the pins **inert empirically**: all 16 tracked
pinned-extension files are already `i/-text` without the file.

## S2 — first-compile pass: **EMPTY DIFF**

Zero source edits needed. 0 errors across 601 build steps. The package predicted a "mechanical
include-only fix pass"; measurement (`clang++ -fsyntax-only`: 87 src TUs + 254 test TUs, 0 failures)
said there was nothing to fix, and there wasn't. **The empty diff is the correct outcome** —
manufacturing edits to "complete" the slice was explicitly forbidden and did not happen.

## S3 — tooling

- New **`tools/bootstrap-build.sh`** (bash, LF, `+x`, `bash -n` clean) mirroring
  `bootstrap-build.ps1`'s flow for Ninja + Homebrew SDL3; flags `--run-tests` / `--slow` /
  `--config` / `--vendored-sdl3`. Default passes **no** `-DCMAKE_PREFIX_PATH` (verified
  unnecessary — SDL3 resolves natively from `/opt/homebrew`). Never creates a second tree.
- Produces a `CMakeCache` **identical** to the manual runbook — the strongest form of "same `build/`
  state" (AC-S3).
- **Honest disclosure:** `--vendored-sdl3` is **UNTESTED**. The developer initially wrote README text
  implying it worked, then corrected it rather than guess; testing it would have overwritten the
  verified-green cache (`CMAKE_PREFIX_PATH` is sticky until `rm -rf build`). Limitation documented in
  both the script header and README.
- **`*.sh text eol=lf` is inert here**: `/tools/` is gitignored (`.gitignore:220`), so the script is
  local-only like its `.ps1` twin. Kept (zero-churn, correct for a future tracked `.sh`).

## S4 — docs

- **`README.md`** — "Build and test" split into Windows / macOS / Both. macOS prereqs, Ninja commands
  (single-config: **no** `-C`), binaries at `build/` not `build/Debug/`, and the
  `SDL3=OFF → 237-not-253` caveat. Windows instructions intact. LF preserved.
- **`CLAUDE.md`** — `games/` inventory rewritten to the **verified tree**; build-flow and
  evidence-gates sections now document both platforms.
- **`agent-protocol/state/current-phase.md:11`** — "clone" → full working-tree transfer (Option A),
  correcting a line that contradicted the package's own R7/A5.
- **`src/frontend/sdl3_main.cpp` (+32/−32), `sdl3_app.cpp` (+8/−3)** — help/comment text only.
  Every changed line sits inside an `R"HELP(...)HELP"` raw string or a `//` comment; no identifier,
  expression, or control flow touched. Backslash→forward-slash, `.exe`-drop, plus
  `msxdos22.dsk`→`msxdos23.dsk` and `roms\aleste.rom`→`games/roms/game.rom`. No test asserts on this
  text.

## Results

- **Build:** Ninja, `[601/601]`, 0 errors. Three Mach-O arm64 targets + the POST_BUILD
  `diskutils/msx-disk` copy (no suffix).
- **Fast subset: 253/253**, 0 failed. Verified from a **from-scratch** `rm -rf build` rebuild.
- **Skip-set:** exactly the 8 enumerated in rev-2 §6.4.2, name-for-name, no unenumerated skipper.
  Tests 8/9 (`m28_c5`, `diskutils_bpb`) correctly **RUN**.
- **AC-M3 — the headline result:** `msx-disk --create` → SHA256
  `6f1a79835e0421178b01207b196fa245c127c976fa0c6abc3aaa57e6b0ce5188`, **byte-identical to the golden
  generated by Windows x64**. Reproduced twice, `cmp` IDENTICAL, and independently re-run by both QA
  and the coordinator. Closes assumptions A2 (little-endian) and A8 (arch-neutral golden) with
  measurement rather than reasoning.
- **AC-M4:** exit matrix 0/1/2/3 matches `src/diskutils/cli.cpp:202`.
- **ZEXALL (QA initiative, no AC required it): `Passed 1559.21 sec` → 254/254.** The Z80 core is
  instruction-exact under a compiler it had never been built with.
- Both pwsh gates exit 0; all 8 asset hashes match the Windows record byte-for-byte;
  `docs/asset-checksums.txt` **untouched** (Mac output → `debug/m54/asset-checksums-macos.txt`) —
  overwriting it would have destroyed the `Files: 1` record that *is* the AC-M2 p3 evidence.

## Hard rules held

Zero edits to `src/core`, `src/devices`, `tests/`, or any timing constant (ZEXALL stayed disarmed by
scope — and passed anyway). ONE build tree. CR=0 on all touched files. Additive, platform-guarded
edits only. NG-1 (`games/` path fixes) and NG-2 (`definition-of-done.yaml` backfill) untouched.
Nothing committed; nothing pushed.

## Corrections made to the planning artifacts

1. **"Gradius3 does not exist"** (rev-2 §S4) — **wrong**. It exists: `games/disks/gradius_3/G3.DSK`,
   737,280 B, uppercase extension (a lowercase `*.dsk` glob missed it). The developer documented the
   real file rather than propagate the claim; QA fact-checked the whole rewritten inventory against
   disk and found it accurate, including "no `metalgear2_scc.rom` anywhere" (TRUE).
2. **§6.4.2 row 10** — says test 10 skips "Case 1 only"; reality is **Cases 1 AND 3**.
3. **§6 Step 4/5** — prescribed `build/msx_disk`; real name is `build/msx-disk` (`OUTPUT_NAME`,
   `CMakeLists.txt:134`). The package contradicted itself (§2.5 was right). Fixed by the coordinator.

## Outstanding

**AC-W1 / AC-W2 / AC-W3 and AC-M2 part 3 are NOT run and are NOT claimed.** They require the Windows
machine (owner-confirmed available). QA verdict: **CONDITIONAL PASS** — see
`docs/m54-qa-signoff.md`; commands in `docs/m54-windows-regate.md`.
