# M54 Planner Package — macOS Support (Single Codebase, Dual Toolchain: MSVC + AppleClang)

- Milestone ID: M54
- Decision: DEC-0081
- Request: REQ-M54-001
- **Revision: rev-2 (AMENDED 2026-07-15)** — supersedes rev-1 (2026-07-15, planning-only cycle).
- Type: PLANNING cycle. NO production code, NO CMake edits **by the planner**. rev-2 is written to be
  **executed on the Mac** (the tree is now ON the Mac — see the amendment log).
- Spec Owner: MSX Planner Agent
- Developer Owner: Mac-side developer session (real-hardware evidence).
- QA Owner: follows the Mac-side developer evidence.
- Baseline: post-M53, fast-subset ctest = **253/253** with **`SONY_MSX_ENABLE_SDL3=ON`**
  (developer-pinned; ledger `state/milestones.md` M53 entry). Windows byte-identity of this baseline
  is the hard regression bar. **SDL3=OFF yields 237, not 253** — see §6 Step 4 note.

This package is written to be executed on the Mac by the owner **plus a fresh Claude Code session
that does not have this session's context**. The MACOS RUNBOOK (§6) is the operative deliverable;
everything else grounds it.

---

## Amendment Log

### rev-2 — 2026-07-15 (Mac-side review + owner decisions)

rev-1 was reviewed **on the Mac** (the working tree had already been transferred), which converted a
block of rev-1 assumptions into measured facts and exposed four rev-1 defects. The owner reviewed the
reviewer's NEEDS-REWORK verdict and authorized this amendment cycle. Changes:

| # | Change | Why |
|---|---|---|
| 1 | **NEW slice S0 (tree hygiene), gating S1** — restore the 3 CRLF-dirty tracked files to their LF blobs BEFORE S1 edits `CMakeLists.txt`; stale Windows `build/` deleted. | The tree arrived by full-directory transfer carrying a Windows-shaped `build/` and CRLF working-copy bytes; both would corrupt S1's output. Owner approved the `build/` deletion (already executed). |
| 2 | **§3.2 rewritten** around the **transfer-induced CRLF-over-LF hazard** (rev-1 modelled only the `.gitattributes` churn question, not the live dirty-worktree hazard). | S1 rewrites `CMakeLists.txt`; on a CRLF working copy that flips the tracked blob LF→CRLF repo-wide and propagates to Windows — an AC-W3 violation. |
| 3 | **§6 Step 4 skip table + AC-M2 rewritten.** rev-1 mis-attributed 4 tests to `games/`; they read `SONY_MSX_ROMS_DIR` (= `<source>/roms`). 2 tests listed as skipping actually RUN. A 10th (partial) skipper was missing. rev-1's "delta = 3" was **wrong: the delta is ZERO**. | Verified against the test sources + `tests/CMakeLists.txt:652`. AC-M2 was also tautological (any outcome satisfied it). |
| 4 | **§6 Step 1 `brew install --cask powershell` → `brew install powershell`.** | Coordinator-verified: the cask no longer exists ("Cask 'powershell' is unavailable"); Homebrew moved PowerShell to a **formula** (7.6.3, bottled). rev-1's line was a hard failure. |
| 5 | **AC-M1 → Ninja-only.** Xcode generator demoted to optional/deferred. | Owner decision: the Xcode generator needs a sudo system-wide `xcode-select -s` switch — out of M54 scope. |
| 6 | **`-DCMAKE_PREFIX_PATH` dropped from Step 2's required command** (kept as fallback). **R3 retired to LOW/LOW.** | Coordinator-verified: SDL3 3.4.12 at `/opt/homebrew/lib/cmake/SDL3` resolves **without** it. |
| 7 | **R1 → LOW/LOW; S2 expected to be a NO-OP.** | Developer measured the real exposure: `clang++ -fsyntax-only -std=c++20` over **87 src TUs → 0 failures** and **254 test TUs → 0 failures**. The include-only STOP-and-escalate guardrail is retained verbatim. |
| 8 | **A1 / A4 / A9 marked VERIFIED; R9 moot; new R12 (transfer CRLF hazard).** | Coordinator-verified on the Mac: arm64, SDL3 config found, `skip-worktree` intact, macOS SDK 26.5. |
| 9 | **§5 note:** AC-W1/W2/W3 stay as the hard regression bar; **no** "Windows retired" fallback branch is introduced and **no DEC-0082 is needed**. | Owner decision: the Windows machine **remains available**. M54 closes on its own criteria. |
| 10 | **S4 doc-sweep scope widened**: stale `games/` inventory in `CLAUDE.md:242-246`, the "clone" wording in `current-phase.md:11`, cosmetic `.exe`/backslash help text. | Found during the Mac-side review; all docs-only. |
| 11 | **Two explicit NON-GOALS recorded**: the `definition-of-done.yaml` milestone-record drift, and fixing the `games/` test paths. | Both need their own decision entries; backfilling DoD flags retroactively would fabricate evidence. |

**Owner requirement reaffirmed (rev-2):** the **ONE codebase must support BOTH macOS and Windows,
auto-detected by target system** — no fork, no parallel build files, no platform branching beyond
CMake `if(MSVC)` / `if(APPLE)` / `if(WIN32)`. This was already rev-1's thesis (§1.1); rev-2 makes it
explicit as an owner directive.

**Unchanged from rev-1 (reviewer-confirmed correct):** §2.0 portability audit, §2.4 universal-binary
NON-GOAL, §3.1 binary-I/O determinism finding, §3.2's refusal of `* text=auto`, §3.2's binary pins,
S2's include-only guardrail, R7/A5's full-tree mandate, AC-M3, AC-M4.

**Evidence-provenance note:** facts labelled *coordinator-verified* in rev-2 were executed by the
coordinator on the Mac and reported to the planner; the planner re-read every cited `file:line` that
still exists. The one exception is `build/CMakeCache.txt` (§S0), which was **read by the coordinator
before the approved deletion** and can no longer be re-read — it is cited as coordinator-verified,
not as a planner read.

---

## 1. Scope and Assumptions

### 1.1 Scope

**OWNER DIRECTIVE (rev-2, explicit):** the **ONE codebase must support BOTH macOS and Windows,
auto-detected by the target system.** No fork. No parallel/per-platform build files. No platform
branching beyond CMake `if(MSVC)` / `if(APPLE)` / `if(WIN32)`. Neither platform is a "port of" the
other — one tree, one CMake graph, two toolchains, selected at configure time with **zero** user
intervention beyond the generator choice.

Concretely: make the ONE repository compile and pass its deterministic suite on **both** Windows
(MSVC) and macOS (AppleClang). Three targets must build on macOS: `sony_msx_headless`,
`sony_msx_sdl3`, `msx_disk` (output name `msx-disk`, no `.exe`).

This directive is also the **standing constraint on every slice below**: any proposal that would
require a second build file, a `#ifdef __APPLE__` in `src/`, or a divergent source path is out of
scope and requires a decision entry. (The audit in §2.0 shows the tree already satisfies this —
`src/` has zero platform conditionals today, and rev-2's measured 0-failure syntax sweep (§9 R1)
says it will keep having zero.)

**In scope (Mac-side execution, planned here):** CMake platform branches + AppleClang warning
flags; a root `.gitattributes`; a mechanical first-compile include-fix pass; tooling/bootstrap
portability; a README macOS section; a dual-platform evidence run.

**Out of scope (DEC-0081):** any device/timing behavior change (→ openMSX A/B N/A; ZEXALL only if
`src/core`/`src/devices/cpu` is ever touched — planned NOT to be); macOS `.app`/`.icns` packaging
polish (plain binaries this milestone); universal (fat) binaries (see §2.4 verdict).

### 1.2 Assumptions (full verification actions in §10)

- **A5 (decisive) — VINDICATED, and it happened:** The owner transferred the **full working tree**
  to the Mac rather than a bare `git clone`. `.gitignore:214-224` marks `tests/ tools/ docs/
  references/ agent-protocol/ .claude/ debug/` and `CLAUDE.md`/`src/CLAUDE.md` as **local-only
  (untracked)**; a public-remote clone therefore contains none of them — including *this package* and
  the entire test suite. rev-1 was right to mandate the full transfer. **But rev-1's verify was
  incomplete** — it checked *presence* only, not the three properties the transfer actually
  perturbed: staleness (`build/`), working-copy line endings, and index flags. §S0 + Step 0 now
  verify all three.
- **A1 — VERIFIED (coordinator, Mac):** Apple Silicon **arm64**, Darwin 25.5.0. Intel (x86_64) would
  work identically; only the native arch differs.
- **A2:** Both platforms are little-endian → golden hashes/fixtures carry over byte-identically.
  (Still to be proven by AC-M3; it is the point of AC-M3.)
- **A4 — VERIFIED (coordinator, Mac):** SDL3 **3.4.12** at `/opt/homebrew/lib/cmake/SDL3` — and it
  resolves **without** `-DCMAKE_PREFIX_PATH`.
- **A7:** 253 (SDL3=ON) is the current fast-subset baseline (M53 close). Re-confirm on Windows before
  any edit. **The Windows machine remains available** (owner, rev-2) → A7 stays verifiable and
  AC-W1/W2/W3 stay enforceable.
- **A9 — VERIFIED (coordinator, Mac):** the `roms/fmpac.rom.sram` **skip-worktree flag survived the
  transfer** (`git ls-files -v roms/fmpac.rom.sram` → `S`); the "NOT TO BE ALTERED" pair is not dirty.

**Toolchain, coordinator-verified present on the Mac (2026-07-15):** ninja 1.13.2, pwsh 7.6.3,
cmake 4.3.4 (`/opt/homebrew/bin`), Apple clang 21.0.0 (`/Library/Developer/CommandLineTools`),
SDL3 3.4.12 (`/opt/homebrew/lib/cmake/SDL3`), macOS SDK 26.5, Darwin 25.5.0 arm64. cmake ≥ 3.21 is
satisfied (`CMakeLists.txt:14`).

---

## 2. CMake Platform Matrix (design — implemented Mac-side in slice S1)

### 2.0 What is ALREADY portable (grounded — reduces M54 to a small milestone)

The build system was written defensively; most platform-specific bits are already guarded and
become no-ops on macOS **without any edit**:

| Concern | Location | macOS behavior today | Action |
|---|---|---|---|
| `/utf-8` source-encoding flag | `CMakeLists.txt:29-31` `if(MSVC)` | not applied (Clang is UTF-8 native) | none |
| SDL3 DLL staging | `CMakeLists.txt:160-168` `sony_msx_stage_sdl3_dll` → `if(WIN32)` | not applied (dylib resolved via install/rpath) | none |
| Windows app icon `.rc` | `CMakeLists.txt:188-195` `if(WIN32)` | not compiled | none |
| msx-disk POST_BUILD copy | `CMakeLists.txt:140-143`; `$<TARGET_FILE:msx_disk>` | resolves to `msx-disk` (no suffix), copied to `diskutils/`; `make_directory` (:141) guards a fresh clone | none (comment at :138-139 already states this) |
| `.gitignore` for the macOS binary | `.gitignore:26` `/diskutils/msx-disk` | already covers the suffixless binary | none |
| tests subdir guard | `CMakeLists.txt:201-203` `if(EXISTS tests/CMakeLists.txt)` | builds tests when the tree is present | none |
| C++ standard | `CMakeLists.txt:20-22` C++20, `CMAKE_CXX_EXTENSIONS OFF` | portable | none |

`src/` has **zero** Win32 usage (grep for `windows.h`/`_WIN32`/`__declspec`/`__forceinline`/
`WinMain`/`<io.h>`/`<direct.h>`/`Sleep(`/`QueryPerformance` → no matches). SDL3 is included as the
cross-platform `#include <SDL3/SDL.h>` (`src/frontend/sdl3_app.h:16`,
`sdl3_video_presenter.h:16`, `sdl3_audio_presenter.h:16`, `sdl3_input_mapper.h:16`).

### 2.1 The one CMake addition needed — warning flags

There is currently **no** MSVC warning regime beyond `/utf-8` (no `/W4`, no `/WX`). "Mirror of the
MSVC intent" is therefore modest. Design (S1):

```cmake
if(MSVC)
    add_compile_options(/utf-8)
else()               # AppleClang / GCC / Clang
    add_compile_options(-Wall -Wextra)
endif()
```

- `-Werror` is **deliberately NOT added** in M54 (MSVC has no `/WX`; adding `-Werror` would let a
  benign warning block the first compile). It may be a future hardening item, not this milestone.
- Rationale: keep the warning intent symmetric-but-non-blocking; real portability defects surface
  as **hard errors** (missing includes) regardless of `-Wall`.
- `if(APPLE)` vs the generic `else()`: use `else()` so a future Linux CI also benefits; the owner's
  only two toolchains are MSVC and AppleClang, both correctly handled.

### 2.2 Single-config vs multi-config invocation table (R6)

Windows uses the **Visual Studio multi-config** generator (build/test with `-C Debug`). macOS has
two clean options; the invocation differs and MUST be documented precisely:

| Generator | Config selection | Build command | ctest command | Executable path |
|---|---|---|---|---|
| **Ninja** (macOS, RECOMMENDED, single-config) | `-DCMAKE_BUILD_TYPE=Debug` at **configure** | `cmake --build build` | `ctest --test-dir build --output-on-failure` (**no `-C`**) | `build/sony_msx_headless` |
| **Xcode** (macOS, alternate, multi-config) | none at configure | `cmake --build build --config Debug` | `ctest --test-dir build -C Debug --output-on-failure` | `build/Debug/sony_msx_headless` |
| **Unix Makefiles** (macOS fallback, single-config) | `-DCMAKE_BUILD_TYPE=Debug` at configure | `cmake --build build` | `ctest --test-dir build --output-on-failure` (**no `-C`**) | `build/sony_msx_headless` |
| **Visual Studio** (Windows, unchanged, multi-config) | none at configure | `cmake --build build --config Debug` | `ctest --test-dir build -C Debug --output-on-failure` | `build/Debug/sony_msx_headless.exe` |

**DECIDED (rev-2): Ninja on macOS is the SOLE proof path** — fast, minimal, installed (1.13.2,
coordinator-verified), and the fast-subset filter works identically
(`ctest --test-dir build -LE m24_slow_full_sweep`). **Xcode is OPTIONAL/DEFERRED and is NOT an M54
gate** (owner decision): `-G Xcode` needs a full Xcode.app selected system-wide via
`sudo xcode-select -s`, a privileged system change outside M54's scope. Its row stays in the table
for future IDE debugging, not as an obligation. The single-config binary path (`build/`, not
`build/Debug/`) is the most common Mac-side trip hazard — the runbook calls it out.

### 2.3 SDL3 on macOS (vendored/arm64)

Two supply routes; **recommend Homebrew** for the runbook, with the vendored build as the
context-free fallback that mirrors `tools/bootstrap-build.ps1:44-59`:

1. **Homebrew (RECOMMENDED — and VERIFIED PRESENT):** `brew install sdl3` installs
   `SDL3Config.cmake` under `$(brew --prefix)/lib/cmake/SDL3`. **Coordinator-verified on this Mac:
   SDL3 3.4.12 at `/opt/homebrew/lib/cmake/SDL3`** — already installed, and `find_package(SDL3 CONFIG
   REQUIRED)` (`CMakeLists.txt:147`) resolves it **WITHOUT** `-DCMAKE_PREFIX_PATH` (CMake searches
   the Homebrew prefix by default on arm64). rev-1 listed that flag as required; **rev-2 drops it
   from the Step 2 command and keeps it as a fallback only.** No DLL/dylib staging is needed — the
   `if(WIN32)` guard (`CMakeLists.txt:161`) skips it, and the Homebrew dylib is found via its install
   path.
2. **Vendored (fallback, no Homebrew):** build SDL3 once from `references/sdl3` into
   `build/_sdl3_install`, exactly like the PowerShell bootstrap. Requires `references/` present
   (full-tree transfer). macOS SDL3 uses Metal + CoreAudio natively
   (`references/sdl3/docs/README-macos.md`; command-line CMake build at lines 13-19). Deployment
   target ≥ 10.13 and Xcode ≥ 12.2 / macOS 11.0 SDK per `README-macos.md:33`.

### 2.4 Universal-binary verdict — explicit **NON-GOAL** for M54

Build the **native architecture only** (arm64 on Apple Silicon, or x86_64 on Intel). Justification:

- The milestone goal is "compiles + runs + passes the suite on the owner's Mac," not distribution.
- Determinism/byte-identity of golden oracles is **arch-independent** here: both arches are
  little-endian and every oracle compares **logical machine state** (frame hashes, disk SHA256,
  debug-dump text), never a host binary — so a native arm64 build produces the same golden bytes as
  Windows x64 (verify via A2 in §10).
- A fat binary would additionally require a **universal SDL3**
  (`references/sdl3/docs/README-macos.md:22-31`, `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`) and
  roughly doubles build time for zero test value.
- Consistent with DEC-0081's recorded packaging non-goal (".app/.icns polish deferred"). A universal
  build + `.app` bundle can be a later, separate milestone.

### 2.5 msx-disk on macOS

No change required (see §2.0). `set_target_properties(msx_disk PROPERTIES OUTPUT_NAME msx-disk)`
(`CMakeLists.txt:134`) + `$<TARGET_FILE:msx_disk>` (`:143`) yield `msx-disk` with no suffix; the
POST_BUILD `copy_if_different` lands it in `diskutils/`; `.gitignore:26` (`/diskutils/msx-disk`)
already ignores it and `diskutils/.gitkeep` keeps the folder.

---

## 3. Line-Ending Hazard + `.gitattributes` Policy (slices S0 / S1)

### 3.1 Finding — the determinism risk is NIL

**Every** file-stream open in both `src/` and `tests/` uses `std::ios::binary` (verified by
grepping all `ifstream`/`ofstream`/`fstream` construction sites and all `.open(` calls — zero
text-mode fixture/golden readers; the only `std::getline` uses are on in-memory
`std::istringstream` of an already-binary-read buffer, e.g. `tests/unit/machine/frame_dump_unit_test.cpp:71`,
`tests/unit/frontend/psg_audio_dump_unit_test.cpp:109`, not on files). Binary reads see raw bytes
regardless of the host's CRLF/LF convention, so **golden-oracle determinism is already
line-ending-immune**. Representative binary reads: `src/machine/rom_asset_loader.cpp:59`,
`src/devices/fdc/disk_image.cpp:166`, `tests/integration/machine/machine_diskutil_fdc_mount_integration_test.cpp:229`,
`tests/unit/machine/hbf1xv_slot_map_unit_test.cpp:58`.

### 3.2 The TRANSFER-INDUCED CRLF-over-LF hazard (rev-2 — read before S1)

**This is the highest-value finding in the package and the reason S0 exists.** rev-1 modelled the
`.gitattributes` churn question correctly but did not model the hazard the transfer itself created.

**The situation.** The tree arrived on the Mac by full-directory copy (A5), so the working-copy bytes
are whatever Windows had on disk. `git status` on the Mac reports three tracked files modified:

```
 M CMakeLists.txt
 M README.md
 M sony_msx_hbf1xv.xml
```

**The mechanism.** These files are **CRLF on disk over LF blobs in the index**. On the Windows
machine `core.autocrlf=true` made that invisible (git converted on the fly, `git status` stayed
clean). On the Mac, autocrlf is not normalizing — so the CRLF bytes are seen as *real content
differences* against the LF blobs. Diagnostic: the diff is whole-file, not semantic.

**Why it is dangerous — the chain to an AC-W3 violation.** S1 edits `CMakeLists.txt` (the `if(MSVC)`/
`else()` warning branch, §2.1). `CMakeLists.txt` is ~206 lines. If S1 writes that file while the
working copy is CRLF, the resulting commit flips the **tracked blob** from LF to CRLF in a
**whole-file rewrite**:

- the real 4-line change is buried in a 206-line diff → unreviewable;
- the LF→CRLF blob flip **propagates to Windows** on the next pull, churning that checkout — the
  precise thing DEC-0081 forbids and **AC-W3 tests for**;
- the same trap arms for `README.md` in S4.

**The fix is S0, and it must be the developer's FIRST action** — restore the LF blobs *before* S1
touches anything:

```bash
git checkout -- CMakeLists.txt README.md sony_msx_hbf1xv.xml
```

This discards **only** line-ending noise (the blobs are byte-identical modulo EOL — confirm with
`git diff --stat` showing whole-file churn and `git diff --ignore-all-space` showing nothing). No
authored content is lost. **Do not** "fix" this with `core.autocrlf` on the Mac and do not commit
the CRLF bytes.

**Guardrail for S1/S4:** after editing `CMakeLists.txt` (S1) or `README.md` (S4), re-run
`git diff --stat` — the diff must be **small and semantic**. A whole-file diff means the LF blob was
flipped: revert and redo from restored state.

### 3.2.1 Policy — minimal, ZERO-churn (hard requirement; UNCHANGED from rev-1)

The repo currently has **no root `.gitattributes`** (only two vendored ones exist deep under
`references/`: `references/sdl3/src/render/gpu/shaders/.gitattributes`, `references/zexall/.gitattributes`).
git reports mixed CRLF/LF and the existing Windows checkout must **not** be churned (DEC-0081 hard
requirement). Therefore:

- **ADD a root `.gitattributes` that pins binary asset extensions to `binary`** (which is `-text
  -diff` → git never does EOL conversion on them). git already auto-detects these as binary, so the
  attribute adds **zero churn** while making the byte-stability explicit and identical on both
  platforms:

  ```gitattributes
  # Byte-exact assets — never EOL-convert (bytes are load-bearing: goldens, ROMs, disks)
  *.rom  binary
  *.dsk  binary
  *.sram binary
  *.bin  binary
  *.dump binary
  *.frame binary
  *.png  binary
  *.wav  binary
  *.ico  binary
  ```

- **DO NOT add a blanket `* text=auto` / `eol=lf` normalization.** That would make git want to
  renormalize the many CRLF-committed text/source files, churning the existing Windows checkout —
  a direct violation of the no-churn requirement. Mixed CRLF/LF in `.cpp/.h/.md/.txt` compiles fine
  under both MSVC and AppleClang (Clang tolerates CRLF), so no correctness need forces
  normalization.
- **New shell scripts only:** if slice S3 adds `bootstrap-build.sh`, pin it `*.sh text eol=lf` so
  it always checks out with LF (required for the shebang/`chmod +x` to work on macOS). This affects
  only a **new** file → still zero churn to the existing tree.
- A deliberate whole-repo LF normalization (`git add --renormalize .`) is explicitly **deferred**
  as a separate, optional, one-time future decision — it is a churn event and is NOT part of M54.
  **Note (rev-2):** §3.2's hazard is NOT an argument for renormalizing now. The correct M54 response
  is S0's per-file restore (3 files, zero tracked-blob change), not a repo-wide EOL event. The
  binary-pins below remain the only `.gitattributes` content M54 adds.

---

## 4. Filename-Case Audit Obligation (slice S1/S2)

macOS default volumes (APFS) are **case-insensitive** but the owner may use a case-sensitive volume,
and AppleClang emits warnings on `#include` case mismatches even on case-insensitive filesystems.
Obligation for the Mac-side developer (audit, then fix only if a mismatch is found):

1. **Asset filenames referenced in code vs on disk.** The 7 BIOS defaults are lowercase in
   `src/machine/emulator_config.h:48-54` (`f1xvbios.rom`, `f1xvext.rom`, `f1xvkdr.rom`,
   `f1xvdisk.rom`, `f1xvmus.rom`, `f1xvkfn.rom`, `f1xvfirm.rom`) and the on-disk names are lowercase
   (`bios/f1xvbios.rom` … `bios/f1xvmus.rom`) → **match confirmed on Windows**; re-verify no
   case drift after transfer. Also audit `roms/fmpac.rom`, `roms/fmpac.rom.sram`,
   `disks/msxdos23.dsk`, and the softwaredb default path
   `references/openmsx-21.0/share/softwaredb.xml` (`src/machine/cartridge_identifier.h:39`).
2. **`#include` directive casing** across all of `src/` and `tests/` must exactly match the header
   filenames on disk (all project headers are lowercase snake_case; SDL3 uses `<SDL3/SDL.h>` — the
   exact case Homebrew/SDL install). Method: on a case-sensitive volume the compile itself is the
   audit (a mismatch is a hard error); on APFS-insensitive, `-Wnonportable-include-path` (implied by
   normal Clang diagnostics) flags mismatches.

Expected outcome: **no mismatches** (Windows already tolerates none that would matter, and the code
is uniformly lowercase). This is a verification obligation, not an anticipated fix.

---

## 5. Tooling Strategy

### 5.0 Windows availability — SETTLED (rev-2, owner decision)

**The Windows machine REMAINS AVAILABLE.** Consequently:

- **AC-W1 / AC-W2 / AC-W3 stay exactly as written** (§7.2) as the **hard regression bar**, including
  AC-W1's "**after every portability edit**" cadence. They are not softened, not deferred, and not
  made conditional.
- **A7 stays verifiable** — the Windows 253/253 re-run is a real, executable gate, not an assumption.
- **No DEC-0082 is required.** M54 closes on its own criteria; no "Windows retired" fallback branch
  exists in this package and none is to be introduced. Any future change to Windows availability
  would be a new decision, not an M54 amendment.

### 5.1 `tools/*.ps1` on macOS — pwsh INSTALLED (rev-2)

The evidence-gate scripts are **dev-only** (they are NOT invoked inside ctest — tests are pure C++
executables). **pwsh 7.6.3 is installed on the Mac (coordinator-verified)** → run the gates
verbatim; the manual fallback below is now a contingency, not a branch.

- **pwsh (verbatim — the path of record):** `brew install powershell` (see §6 Step 1 — **formula,
  NOT `--cask`**) gives `pwsh`; run `pwsh -File tools/validate-assets.ps1` and
  `pwsh -File tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` unchanged.
  (`-ExecutionPolicy Bypass` is **Windows-only** and simply omitted on macOS — execution-policy N/A.)
  **Proven on macOS (coordinator-verified):** `pwsh -File tools/validate-assets.ps1` → **exit 0**,
  all 7 BIOS present, `ROM file count: 1`.
- **Manual equivalents (contingency only — pwsh is present, so this should not be needed):** validate
  = confirm `bios/f1xvbios.rom` + ≥1 `roms/*.rom` exist (`ls bios roms`); checksums =
  `shasum -a 256 bios/* roms/* > debug/m54/asset-checksums-macos.txt` (note: format differs from the
  PowerShell tool's output; use it for spot-checks, not byte-diffing against the Windows
  `docs/asset-checksums.txt`).

**⚠ rev-2 — do NOT clobber `docs/asset-checksums.txt` from the Mac.** That file is a
**Windows-generated artifact** (`:6` `Directory: D:\Projects\sony-msx-hbf1xv\bios`, `:8` `Files: 7`;
`:18` `Directory: D:\Projects\sony-msx-hbf1xv\roms`, `:20` `Files: 1`) and its `roms/ Files: 1` line
is **load-bearing evidence** for the delta-zero reconciliation (§6.4.3). Write Mac checksum output to
`debug/m54/asset-checksums-macos.txt` and **diff the hashes** instead; re-generating
`docs/asset-checksums.txt` in place would destroy the Windows-side record mid-milestone and is a
gratuitous tracked-file churn.

Position: `pwsh` **is** installed → the gates run verbatim and produce comparable output. The manual
`shasum` path stays documented as the zero-dependency fallback for a fresh machine.

### 5.2 openMSX A/B — native on macOS, but **N/A for M54**

- M54 changes **zero** device/timing behavior → openMSX A/B is **Not Applicable** this milestone
  (DEC-0081). It is listed only for completeness / future use.
- When ever needed on macOS, openMSX runs **natively** (`brew install openmsx` or the openMSX.app),
  **no WSL**. **Harness path assumption to flag:** `tools/openmsx-*.ps1` (the A/B smoke / parity
  harness) assume the WSL Linux binary at `/usr/bin/openmsx`; on macOS the binary is at
  `$(brew --prefix)/bin/openmsx` (arm64: `/opt/homebrew/bin/openmsx`) or inside
  `/Applications/openMSX.app/Contents/...`. If a future behavior milestone needs A/B on macOS, the
  harness needs a path override (a parameter or env var) — a **future** portability note, not an M54
  obligation.

### 5.3 `bootstrap-build.ps1` disposition — recommend a **`.sh` twin** (S3), manual commands authoritative

- Do **not** try to run `bootstrap-build.ps1` under macOS pwsh: it hard-codes SDL3-from-`references/sdl3`
  and Windows-shaped `cmake --build ... --config` calls (`tools/bootstrap-build.ps1:48-53,62-78`) and
  would not match the recommended Ninja/Homebrew Mac path.
- **Recommendation:** slice S3 adds a parallel **`tools/bootstrap-build.sh`** (bash, LF via the S1
  `.gitattributes` rule) that mirrors the .ps1 intent for macOS: prefer Homebrew SDL3, else vendored
  build; configure Ninja `-DCMAKE_BUILD_TYPE=Debug`; build; optional `-RunTests` equivalent. Because
  it cannot be tested this planning cycle, the **RUNBOOK's explicit manual commands (§6) are the
  authoritative ground truth**; the `.sh` is a convenience the Mac-side developer writes and proves
  during S3/S5. Keep `bootstrap-build.ps1` unchanged (Windows path of record).

---

## 6. MACOS RUNBOOK (owner-executable; self-contained)

> Written so the owner + a fresh Claude Code session on the Mac can execute it **without this
> session's context**. Assumes Apple Silicon; Intel is identical except `uname -m` = `x86_64` and
> the Homebrew prefix is `/usr/local`.

### Step 0 — Get the FULL working tree onto the Mac (MANDATORY — read first)

A bare `git clone` of the public remote is **not enough**: `.gitignore:214-224` makes
`tests/ tools/ docs/ references/ agent-protocol/ .claude/ debug/` and `CLAUDE.md` **local-only**,
and `roms/` (non-`fmpac`), `disks/` content, and all of `games/` are untracked (`.gitignore:165-175`).
A clone would lack the test suite, the SDL3 vendored source, the softwaredb, the game/disk assets,
**and this package itself**. Transfer the whole working directory instead.

- **Option A (RECOMMENDED — one shot, includes assets):** archive the entire repo working tree
  (excluding only `build/`) on Windows and copy it to the Mac (USB/rsync/scp/cloud). This brings
  `src/ tests/ tools/ docs/ references/ bios/ roms/ disks/ games/ diskutils/ CMakeLists.txt
  sony_msx_hbf1xv.xml README.md` in a single transfer → full 253/253 is reachable and no separate
  asset-copy step is needed. Example (from the Mac, pulling over ssh):
  `rsync -av --exclude build/ user@windows-host:/path/to/sony-msx-hbf1xv/ ~/sony-msx-hbf1xv/`
- **Option B (git baseline + copy the rest):** `git clone <remote>` for the tracked baseline (`src/`,
  `bios/`, `CMakeLists.txt`, `sony_msx_hbf1xv.xml`, `README.md`), then copy the untracked
  `tests/ tools/ docs/ references/` and the assets `roms/ disks/ games/` from Windows on top.
  **rev-2 caveat — Option B has a trap Option A does not:** a fresh clone builds a **new index**, so
  the `skip-worktree` bit on `roms/fmpac.rom.sram` is **NOT** carried over (it is index-local, not
  repo-carried — R14). The "NOT TO BE ALTERED" FM-PAC SRAM pair (`roms/README.md`) then becomes
  writable-dirty and can be committed by accident. If Option B is ever used, **re-apply and verify the
  flag** (`git ls-files -v roms/fmpac.rom.sram` → `S`) as part of Step 0. **Option A remains
  RECOMMENDED** — it copies `.git/` and therefore preserves the index flags (verified intact on this
  Mac). Option A's own trap — it also copies a stale `build/` and CRLF working-copy bytes — is handled
  by **S0 (Step 0.5)**.
- **Verify Step 0 (rev-2 — STRENGTHENED; presence alone is NOT enough):** rev-1 verified only that
  files *exist*. The transfer perturbs three further properties, each of which must be checked —
  this is exactly slice **S0**:

  ```bash
  # (1) Presence (rev-1's check — still required)
  ls tests/CMakeLists.txt tools/bootstrap-build.ps1 references/sdl3/CMakeLists.txt \
     docs/m54-planner-package.md bios/f1xvbios.rom
  # (2) Staleness: NO stale Windows build tree may survive the transfer
  test ! -e build && echo "OK: no stale build/"
  # (3) Line endings: the tracked tree must be CLEAN before any edit (§3.2)
  git status --porcelain          # expect EMPTY; if 3 files show ' M', run S0 step (b)
  # (4) Index flags: the NOT-TO-BE-ALTERED FM-PAC SRAM must still be skip-worktree
  git ls-files -v roms/fmpac.rom.sram   # expect a leading 'S'
  ```

### Step 0.5 — S0 TREE HYGIENE (MANDATORY — do this BEFORE Step 1)

Status on the owner's Mac as of 2026-07-15 (coordinator-verified):

| # | Action | Status |
|---|---|---|
| (a) | `rm -rf build` — delete the stale **Windows** build tree | **DONE** (owner-approved) |
| (b) | `git checkout -- CMakeLists.txt README.md sony_msx_hbf1xv.xml` — restore LF blobs **before** S1 edits `CMakeLists.txt` (§3.2) | **NOT DONE — must be the developer's FIRST action** |
| (c) | Verify `roms/fmpac.rom.sram` is still `skip-worktree` (`git ls-files -v` → `S`) | **DONE — flag INTACT** |
| (d) | Prerequisites present (Step 1) | **DONE** — ninja/pwsh/cmake/clang/SDL3 all verified |

**Why (a) was mandatory.** The copied `build/` was a **Windows** tree: the coordinator read
`build/CMakeCache.txt` before deletion and found `CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026` and
`CMAKE_HOME_DIRECTORY:INTERNAL=D:/Projects/sony-msx-hbf1xv`. Every cached path pointed at a `D:\`
drive that does not exist on the Mac, and the cached generator is unavailable there. Configuring over
it fails or, worse, half-succeeds. (Planner note: `build/` is now gone, so this cache line cannot be
re-read — it is cited as **coordinator-verified**, not as a planner read.)

**This UPHOLDS DEC-0041, it does not violate it.** DEC-0041's single-build policy says there is
exactly **ONE** build tree named `build/`. Deleting a stale, foreign-platform `build/` so the ONE
tree can be re-created natively **is** the policy working. The violation would be creating a
**second** tree — `build-mac/`, `build-arm64/`, `build-ninja/` — to sidestep the stale cache. Do not
do that. One tree: `build/`. If it is ever wrong, delete it and reconfigure.

**(c) note:** the skip-worktree flag survived only because `.git/` was **copied**, not re-cloned. A
**fresh clone would NOT have it** — the flag lives in the index, not the repo. Re-check (c) after any
future clone, or the "NOT TO BE ALTERED" FM-PAC SRAM pair (`roms/README.md`) becomes writable-dirty.

### Step 1 — Prerequisites

**All of the following are already satisfied on the owner's Mac (coordinator-verified 2026-07-15):**
ninja 1.13.2, pwsh 7.6.3, cmake 4.3.4, Apple clang 21.0.0, SDL3 3.4.12, macOS SDK 26.5. Retained for
a fresh machine:

```bash
xcode-select --install                 # Xcode Command Line Tools → AppleClang + git + make
# Homebrew (if absent): https://brew.sh
brew install ninja                     # the generator (the ONLY thing that needed installing here)
brew install cmake sdl3                # already satisfied on this Mac (4.3.4 / 3.4.12) -> no-op
brew install powershell                # pwsh, to run tools/*.ps1 gates verbatim
uname -m                               # expect arm64 (A1 -- VERIFIED)
cmake --version                        # expect >= 3.21 (CMakeLists.txt:14) -- 4.3.4 OK
```

> **rev-2 DEFECT FIX — do NOT use `brew install --cask powershell`.** rev-1 shipped that line; it
> **FAILS**: *"Cask 'powershell' is unavailable: No Cask with this name exists."* Homebrew moved
> PowerShell from a **cask** to a **formula**. The correct command is `brew install powershell`
> (stable 7.6.3, bottled). Coordinator-verified.

### Step 2 — Configure + build (Ninja + Homebrew SDL3 — the path of record)

Prerequisite: **S0 (Step 0.5) is complete** — no stale `build/`, `git status` clean.

```bash
cd ~/sony-msx-hbf1xv
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DSONY_MSX_ENABLE_SDL3=ON
cmake --build build
```

> **rev-2:** `-DCMAKE_PREFIX_PATH="$(brew --prefix)"` has been **dropped as a requirement** —
> coordinator-verified that `find_package(SDL3 CONFIG REQUIRED)` (`CMakeLists.txt:147`) resolves
> Homebrew's SDL3 3.4.12 at `/opt/homebrew/lib/cmake/SDL3` **without** it (CMake searches the
> Homebrew prefix by default on arm64). **Fallback only:** if configure reports `Could NOT find
> SDL3`, re-add `-DCMAKE_PREFIX_PATH="$(brew --prefix)"`.

**Xcode generator — OPTIONAL / DEFERRED (rev-2 owner decision).** Proving the Xcode generator is
**NOT** an M54 obligation and is **not** part of AC-M1. Reason: `-G Xcode` requires a full Xcode.app
selected system-wide via `sudo xcode-select -s`, which the Command Line Tools install alone does not
provide; that sudo system switch is out of M54's scope. The invocation stays documented in §2.2 for
whoever later wants IDE debugging, but **Ninja is the sole proof path**:

```bash
# OPTIONAL / NOT an M54 gate -- needs a full Xcode.app + `sudo xcode-select -s`
cmake -S . -B build -G Xcode -DSONY_MSX_ENABLE_SDL3=ON
cmake --build build --config Debug
```

Headless-only fallback (no SDL3 available): add `-DSONY_MSX_ENABLE_SDL3=OFF` (builds
`sony_msx_headless` + `msx_disk` + tests; skips the SDL3 target). **Note the test count changes —
see Step 4's SDL3=OFF warning: OFF gives 237, not 253.**

SDL3 **vendored fallback** (no Homebrew SDL3): build it once from `references/sdl3`, then point
`CMAKE_PREFIX_PATH` at the install (mirrors `tools/bootstrap-build.ps1:48-56`):

```bash
cmake -S references/sdl3 -B build/_sdl3_src_build -DCMAKE_INSTALL_PREFIX=build/_sdl3_install \
      -DSDL_SHARED=ON -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_EXAMPLES=OFF -DSDL_TESTS=OFF -DSDL_INSTALL=ON
cmake --build build/_sdl3_src_build && cmake --install build/_sdl3_src_build
# then configure the project with -DCMAKE_PREFIX_PATH="$PWD/build/_sdl3_install"
```

### Step 3 — Run the deterministic suite (fast subset)

```bash
# Ninja / Makefiles (single-config): NO -C
ctest --test-dir build --output-on-failure -LE m24_slow_full_sweep
# Xcode (multi-config): add -C Debug
ctest --test-dir build -C Debug --output-on-failure -LE m24_slow_full_sweep
```

### Step 4 — Expected results (rev-2: table CORRECTED, delta reconciled)

- Three executables present: `build/sony_msx_headless`, `build/sony_msx_sdl3`, `build/msx-disk`
  (+ POST_BUILD-copied `diskutils/msx-disk`). (Single-config Ninja → `build/`, **not** `build/Debug/`.)
- Fast ctest: **253/253** with `SONY_MSX_ENABLE_SDL3=ON`. Tests that print `SKIP: … (returns 0)`
  still **pass**, so the count stays green even where an asset is absent.

> **SDL3=OFF changes the denominator: OFF yields 237, not 253.** 16 test targets are registered
> inside `if(SONY_MSX_ENABLE_SDL3)` (`tests/CMakeLists.txt:1794`–`:1980`; the staging `foreach` at
> `:1961-1977` enumerates all 16). Total `add_test(NAME …)` registrations = **254**, of which exactly
> one — `hbf1xv_m24_zexall_system_test` — carries `LABELS "m24_slow_full_sweep"`
> (`tests/CMakeLists.txt:1010-1013`) and is excluded by `-LE m24_slow_full_sweep`. Hence
> **254 − 1 = 253 (SDL3=ON)** and **253 − 16 = 237 (SDL3=OFF)**. **Use SDL3=ON for the 253 baseline**;
> a 237 result is not a regression, it is the wrong configuration.

#### 4.1 rev-1 DEFECT — the skip table was wrong

rev-1 attributed four tests to `games/`. **They do not read `games/`.** They read
`SONY_MSX_ROMS_DIR`, defined as `set(SONY_MSX_ROMS_DIR_ABS "${CMAKE_SOURCE_DIR}/roms")`
(`tests/CMakeLists.txt:652`) — i.e. `<source>/roms`, which holds **only** `fmpac.rom` +
`fmpac.rom.sram` (+ `.gitkeep`, `README.md`). They skip because `roms/aleste.rom` and
`roms/metalgear2_scc.rom` **do not exist** — and have not since the asset reorg — **on either
platform**. rev-1 also listed two tests as skipping that actually **RUN**, and missed a tenth
(partial) skipper.

#### 4.2 CORRECTED skip table (every path re-read at the cited line)

| # | Test | Actual gate | Verdict |
|---|---|---|---|
| 1 | `machine_hbf1xv_m19_aleste_smoke_integration_test` | `<roms>/../games/roms/aleste2.rom` (`hbf1xv_m19_aleste_smoke_integration_test.cpp:204-210`) | **SKIP** — flat path; real layout is nested |
| 2 | `machine_hbf1xv_m19_metalgear_smoke_integration_test` | `<roms>/../games/roms/metal_gear.rom` (`…m19_metalgear_smoke…:282-288`) | **SKIP** — same |
| 3 | `hbf1xv_m27_replay_determinism_system_test` | `games/roms/aleste2.rom` (`…m27_replay…:197-200`) | **SKIP** — same |
| 4 | `machine_hbf1xv_m30_cartridge_identification_integration_test` | **`roms/aleste.rom`** (`…m30…:90`) — **NOT games/** | **SKIP** — `roms/` holds only fmpac |
| 5 | `machine_hbf1xv_m31_fm_title_probe_integration_test` | **PARTIAL.** Assertion 1 (`APRLOPLL` ID in `bios/f1xvmus.rom`, `…:133-150`) **RUNS**; the probe gate at **`…:154`** is **`roms/aleste.rom`** | **PARTIAL SKIP** — real coverage still executes |
| 6 | `machine_hbf1xv_m31_metalgear2_scc_integration_test` | **`roms/metalgear2_scc.rom`** (`…m31_metalgear2_scc…:169`) — **NOT games/** | **SKIP** |
| 7 | `hbf1xv_m34_aleste_ultrasonic_regression_system_test` | **`roms/aleste.rom`** (`…m34…:206`) — **NOT games/** | **SKIP** |
| 8 | `hbf1xv_m28_c5_disk_boot_investigation_system_test` | `disks/msxdos23.dsk` (`…m28_c5…:151-156`) | **RUNS** — asset **present** (`disks/msxdos23.dsk`) |
| 9 | `diskutils_bpb_matches_machine_unit_test` | `disks/msxdos23.dsk` geometry cross-check (`…:102-106`) | **RUNS** — asset **present** |
| 10 | `frontend_sdl3_cli_session_integration_test` *(rev-2 addition)* | Case 1 only: `games/roms/aleste2.rom` **and** `disks/msxdos23.dsk` (`sdl3_cli_session_integration_test.cpp:140-151`; registered `tests/CMakeLists.txt:1837`) | **PARTIAL SKIP** — Cases 2/3/4 run |

Tests 4, 6, 7 read `SONY_MSX_ROMS_DIR` (`tests/CMakeLists.txt:652`); test 5's *first* assertion reads
`SONY_MSX_BIOS_DIR`. Tests gated on the **tracked** `bios/` and `roms/fmpac.rom` do **not** skip.

#### 4.3 The `games/` reconciliation — the Mac-vs-Windows delta is **ZERO**

rev-1 implied macOS would skip more than Windows (a "delta = 3"). **That was wrong.** The actual
`games/` layout is **nested-with-spaces**, while tests 1/2/3/10 look for **flat** paths:

| Test expects (flat) | Reality on disk (nested, spaces) |
|---|---|
| `games/roms/aleste2.rom` | `games/roms/Aleste 2/aleste2.rom` |
| `games/roms/metal_gear.rom` | `games/roms/Metal Gear/metal_gear.rom` |

(Also present: `games/roms/Metal Gear 2/metal_gear2.rom`, `games/roms/King's Valley 2/…`,
`games/roms/Space Manbow/…`, `games/roms/firebird/…`; and `games/disks/ys2/ys2-d1.dsk`,
`games/disks/F1/f1-d1.dsk`, `games/disks/laydock_2/…`, `games/disks/Psycho World/…`, etc.)

**Three independent proofs the delta is zero:**

1. **The checksum record was generated ON WINDOWS and already says one ROM.**
   `docs/asset-checksums.txt:18` reads `Directory: D:\Projects\sony-msx-hbf1xv\roms` and `:20` reads
   `Files: 1` — a Windows-side artifact recording exactly one file in `roms/` (fmpac).
2. **The Mac gate agrees.** `pwsh -File tools/validate-assets.ps1` on macOS → `ROM file count: 1`
   (coordinator-verified).
3. **`games/` was copied FROM Windows wholesale**, so its nested-with-spaces layout **IS** the
   Windows layout. The flat-path lookups failed there too.

**Therefore the M53 253/253 baseline ALREADY contained every one of these skips.** Parity holds; the
skips are **INHERITED DRIFT** (from the asset reorg that moved the game library), **NOT an M54
regression**, and **not macOS-specific**. Fixing the paths is an **explicit M54 NON-GOAL** (§7.5).

**Honesty note:** rev-1's claim *"With Option A all of them run for genuine 253/253"* is **DELETED as
false** — a full transfer does **not** make tests 1-7 or 10 run, because the assets they name
(`roms/aleste.rom`, `roms/metalgear2_scc.rom`, flat `games/roms/*.rom`) do not exist under those
paths on **either** machine. 253/253 is reached **with** these skips, on both platforms.

### Step 5 — Smoke the three binaries (sanity)

```bash
./build/sony_msx_headless --help              # (path build/Debug/… under Xcode)
./build/msx-disk --create /tmp/scratch.dsk    # then --read /tmp/scratch.dsk --sector 0
# SDL3 window (interactive): ./build/sony_msx_sdl3   (Metal/CoreAudio native)
```

### Step 6 — Evidence to capture for the Mac-side developer gate

Capture under `debug/m54/` (create it): `uname -a`, `cmake --version`, the configure log
(SDL3 discovery line), the build log (clean, all 3 targets), the full `ctest` output (pass count +
the exact list of any SKIPs), a golden-oracle byte-identity check (see §7 AC), and a
`shasum -a 256` of one created `.dsk` compared to the golden `6f1a7983…b0ce5188`
(`tools/format-blank-disk.ps1` blank; DEC-0080).

**rev-2 additions — required to discharge the amended ACs:**

- **S0 evidence (AC-S0):** `git status --porcelain` **before** S1's first edit (must be EMPTY),
  `test ! -e build`, and `git ls-files -v roms/fmpac.rom.sram` (→ `S`).
- **Skip-set evidence (AC-M2 part 2):** the **literal list** of SKIP lines from the ctest output —
  not just the count. `ctest … --output-on-failure 2>&1 | grep -i '^SKIP'` (or capture full output
  and extract). Compare **name-for-name** against §6.4.2.
- **Delta evidence (AC-M2 part 3):** the **Windows** skip list from the AC-W1 re-run, diffed
  literally against the macOS list → must be **empty both ways**.
- **Diff-shape evidence (AC-W3.2 / R12):** `git diff --stat` for every M54 commit — proving
  `CMakeLists.txt`/`README.md` changed by a handful of lines, not wholesale.
- **Config evidence:** confirm the ctest run was `SONY_MSX_ENABLE_SDL3=ON` (a 237 count means OFF).
- **Checksum evidence:** write the Mac checksums to `debug/m54/asset-checksums-macos.txt` — do **not**
  overwrite the Windows-generated `docs/asset-checksums.txt` (§8.1 caution).

### Step 7 — Troubleshooting playbook (first-compile)

- **`fatal error: 'X' file not found` / `use of undeclared identifier 'std::…'`** → AppleClang/libc++
  is stricter than MSVC about transitive includes (R1). Fix = add the missing `#include`
  (`<cstdint> <cstddef> <cstring> <algorithm> <memory> <array> <limits> <utility>`) to the flagged
  file. **Mechanical only.** Any change beyond adding an include **STOPS and escalates** (S2 rule).
- **`Could NOT find SDL3` at configure** (`CMakeLists.txt:147`) → *not expected* (SDL3 3.4.12 is
  installed and resolves flag-free); if it happens, pass `-DCMAKE_PREFIX_PATH="$(brew --prefix)"`, or
  `brew install sdl3`, or use the vendored fallback (Step 2), or configure
  `-DSONY_MSX_ENABLE_SDL3=OFF` to unblock headless+diskutil first (**→ 237 tests, not 253**).
- **`ctest` reports "No tests were found"** → you used a bare clone without `tests/` (Step 0), or you
  passed `-C Debug` to a single-config Ninja build (omit it for Ninja).
- **Configure fails with `D:/…` paths / "generator Visual Studio 18 2026 not found"** → a **stale
  Windows `build/` tree** survived the transfer (R13). `rm -rf build` and reconfigure. **Do NOT
  create `build-mac/`** — that violates DEC-0041's single-build policy. One tree: `build/`.
- **`git status` shows `CMakeLists.txt`/`README.md`/`sony_msx_hbf1xv.xml` modified but you changed
  nothing** → transferred CRLF bytes over LF blobs (R12/§3.2). `git checkout --` those three files
  **before** editing anything. Do not set `core.autocrlf`; do not commit the CRLF bytes.
- **Your `CMakeLists.txt` diff is ~206 lines for a 4-line change** → the LF blob got flipped to CRLF.
  Revert, run S0(b), redo the edit. This is an **AC-W3 failure** if committed.
- **`ctest` count is 237, not 253** → you configured `-DSONY_MSX_ENABLE_SDL3=OFF`; 16 tests are
  SDL3-gated (`tests/CMakeLists.txt:1794-1980`). Reconfigure with `=ON`.
- **Binary not where expected** → single-config generators emit to `build/`, not `build/Debug/`.
- **`brew install --cask powershell` → "Cask 'powershell' is unavailable"** → PowerShell is now a
  **formula**: `brew install powershell`.
- **`-ExecutionPolicy Bypass` error** → that flag is Windows-only; drop it (use `pwsh -File …`).
- **`msx-disk` not in `diskutils/`** → the POST_BUILD copy runs on a successful `msx_disk` build;
  rebuild that target; `make_directory` (`CMakeLists.txt:141`) creates `diskutils/` if absent.

---

## 7. Acceptance Criteria

### 7.1 Planning-cycle AC

- **AC-P1:** `docs/m54-planner-package.md` delivered, covering DEC-0081 items (1)-(10). ✔ (this doc)
- **AC-P2:** Portability audit cites concrete `file:line` for every platform-sensitive surface (§2.0,
  §3.1, §4). ✔
- **AC-P3:** MACOS RUNBOOK is executable without this session's context, incl. the full-tree-transfer
  step, the generator invocations, expected 253 + enumerated skips, and a troubleshooting playbook (§6). ✔
- **AC-P4:** Deferral note recorded (developer/QA execute Mac-side); M54 stays OPEN in PLANNING (§8).
  ✔ (rev-1; the coordinator persisted the RESP/deferral entry.)
- **AC-P5:** Zero code / zero CMake change by the planner (grep/read only). ✔ (holds for rev-2: this
  amendment edits only this package.)
- **AC-P6 (rev-2):** Every rev-1 defect surfaced by the Mac-side review is corrected in-place, each
  correction grounded in a re-read `file:line`, with an amendment log stating what changed and why. ✔

### 7.2 Milestone AC — Windows (the HARD regression bar — UNCHANGED, and ENFORCEABLE)

**The Windows machine REMAINS AVAILABLE (owner decision, rev-2)** → these are real executable gates,
not aspirations. No "Windows retired" fallback exists; **no DEC-0082 is needed**; M54 closes on its
own criteria.

- **AC-W1:** **After every portability edit** (cadence as originally written — not batched to the
  end), a clean Windows rebuild from the ONE `build/` tree passes **253/253** fast-subset ctest
  (SDL3=ON) — **name-for-name unchanged** from the M53 baseline.
- **AC-W2:** Golden-oracle tests are **byte-identical** to pre-M54 (frame hashes, disk SHA256,
  debug-dump goldens): zero behavior diff. (Expected trivially — edits are includes + CMake +
  `.gitattributes` only.)
- **AC-W3 (rev-2 — RESTATED to test the real hazard):** the existing Windows checkout is **not
  churned**. Two distinct sub-claims, both required:
  1. **No mass renormalization** from the new root `.gitattributes` (its binary-pins match git's
     existing auto-detection → the add is inert). `git status` on Windows shows only the intended
     tracked adds/edits.
  2. **Blob-level line-ending stability of every S1/S4-touched file (the R12 test).** For each file
     M54 modifies — minimally `CMakeLists.txt` (S1) and `README.md` (S4) — the **committed blob's**
     line endings are **unchanged** from their pre-M54 state (LF stays LF). Test: `git diff --stat`
     for the M54 commits shows a **small semantic** diff on those files, **never a whole-file
     rewrite**; and a Windows `git pull` produces **no** spurious modification of files M54 did not
     intend to change. A whole-file diff on `CMakeLists.txt` is an **AC-W3 FAILURE** even if the
     build and all 253 tests pass — the harm is the propagated CRLF blob, not a test outcome.

### 7.3 Milestone AC — macOS

- **AC-M1 (rev-2 — NINJA ONLY):** Clean AppleClang build of **all three** targets
  (`sony_msx_headless`, `sony_msx_sdl3`, `msx_disk`→`msx-disk`) with the **Ninja** generator
  (`-G Ninja -DCMAKE_BUILD_TYPE=Debug -DSONY_MSX_ENABLE_SDL3=ON`), zero errors.
  **The Xcode generator is explicitly OUT of AC-M1** (owner decision, rev-2): it requires a full
  Xcode.app selected system-wide via `sudo xcode-select -s`, which is out of M54 scope. Xcode is
  optional/deferred; **no Xcode proof is required for M54 to close.**
- **AC-M2 (rev-2 — RESTATED NON-TAUTOLOGICALLY):** rev-1's AC-M2 was satisfied by *any* outcome
  ("253/253 **or** 253 green with skips recorded") and therefore tested nothing. It is replaced by a
  **three-part conjunction, ALL of which must hold**:
  1. **Count:** fast-subset ctest (SDL3=**ON**) reports **253/253 PASS**, zero failures.
  2. **Skip-set identity:** the set of tests reporting `SKIP` is **exactly** the enumerated §6.4.2
     list — tests **1, 2, 3, 4, 6, 7** (full skips) plus **5** and **10** (partial skips), and
     tests **8** and **9** **RUN** (they must NOT appear as skips; if they do, `disks/msxdos23.dsk`
     went missing → investigate, do not accept). **No unenumerated test may skip.** A new skipper is
     a **finding**, not a pass.
  3. **Cross-platform delta:** the macOS skip-set **minus** the Windows skip-set == **∅**, and the
     Windows skip-set minus the macOS skip-set == **∅** → **delta == 0** (§6.4.3). Evidence: capture
     both platforms' skip lists and diff them literally.

  Rationale: a green count alone cannot distinguish "ported correctly" from "silently skipped
  everything." Parts 2 and 3 are what make AC-M2 falsifiable.
- **AC-M3:** Determinism byte-identity: a golden-oracle test's output on macOS equals the Windows
  value (e.g., a frame-dump hash, the `--create` disk SHA256 = `6f1a7983…b0ce5188`, a debug-dump
  golden) — proving the little-endian assumption (A2) and cross-platform determinism.
- **AC-M4:** `diskutils/msx-disk` (no suffix) is produced and functional (`--create/--read/--format`
  exit-code matrix 0/1/2/3 behaves as on Windows).

### 7.4 Per-slice AC

- **AC-S0 (tree hygiene — rev-2, GATES S1):** (a) no `build/` directory exists at slice start (the
  stale Windows tree is gone) and **no second tree** (`build-mac/` etc.) was created — DEC-0041 holds;
  (b) `git status --porcelain` is **EMPTY** before S1's first edit (the 3 CRLF-dirty files restored
  via `git checkout --`); (c) `git ls-files -v roms/fmpac.rom.sram` shows `S` (skip-worktree intact);
  (d) prerequisites present. **S1 MUST NOT start until (b) is true.**
- **AC-S1 (CMake matrix + `.gitattributes`):** platform branch compiles under both toolchains; the
  AppleClang warning branch active on macOS, `/utf-8` still MSVC-only; root `.gitattributes` adds the
  binary-pins with **zero** renormalization churn (AC-W3). Windows stays 253/253 (AC-W1).
  **rev-2 addition:** after editing `CMakeLists.txt`, `git diff --stat` shows a **small, semantic**
  diff — **not** a whole-file rewrite. A whole-file diff = the LF blob was flipped to CRLF (§3.2) →
  revert, re-run S0(b), redo.
- **AC-S2 (first-compile fix pass):** macOS build reaches a clean compile using **include-only**
  edits; a diff review confirms **no** behavioral/logic line changed; Windows rebuild stays
  253/253 byte-identical. Any non-include need was escalated, not silently applied.
  **rev-2 expectation — S2 is very likely a NO-OP and an EMPTY diff is the SUCCESS case.** The
  measured exposure is zero: `clang++ -fsyntax-only -std=c++20` over **87 `src/` TUs → 0 failures**
  and **254 test TUs → 0 failures** (developer-measured). **Manufacturing edits to "complete" S2 is
  FORBIDDEN** — adding unnecessary `#include`s to make the slice look productive is churn and is a
  protocol violation. If nothing is missing, S2 closes with "no edits required; syntax sweep clean"
  and that is a full pass. The include-only STOP-and-escalate guardrail stays armed verbatim in case
  the real build surfaces something the syntax sweep did not.
- **AC-S3 (tooling/bootstrap):** `bootstrap-build.sh` (if added) produces the same `build/` state as
  the manual commands, or the manual commands are proven authoritative; asset gates run (pwsh or
  `shasum`). No effect on the deterministic suite.
- **AC-S4 (docs):** README gains a macOS build section (published doc) mirroring the runbook; the
  owner doc-sweep expectation (README + CLAUDE.md files + agents + baseline) is satisfied for the
  new capability.
- **AC-S5 (dual-platform evidence):** both platforms' evidence captured (§6 Step 6 on macOS; a
  Windows 253/253 re-run) proving AC-W1..3 and AC-M1..4.

### 7.5 Explicit NON-GOALS (rev-2) — do NOT do these in M54

Each needs its **own decision entry** in `agent-protocol/channels/decisions.md` before any work.

- **NG-1 — Fixing the `games/` asset paths is OUT OF SCOPE.** Tests 1/2/3/10 (§6.4.2) look for flat
  `games/roms/aleste2.rom` while the library is nested-with-spaces (`games/roms/Aleste 2/aleste2.rom`).
  Correcting this would change **which tests execute** — it is **behavior-adjacent**: it un-skips
  smoke/replay/ultrasonic tests that have not run since the reorg, whose outcomes are **unknown** and
  could legitimately fail. That is a real investigation, not a portability edit, and it would
  contaminate M54's "zero behavior change" premise (which is what makes openMSX A/B N/A). It affects
  **both** platforms equally (delta = 0, §6.4.3), so it is **not** a macOS blocker.
  → **Recorded as a deferred-backlog candidate** ("re-align game-asset test paths to the nested
  `games/` layout; expect newly-executing tests; budget for genuine failures").
- **NG-2 — `definition-of-done.yaml` drift is NOT backfilled.** `agent-protocol/state/definition-of-done.yaml`'s
  `milestone_records` **ends at M37** (`:670`) — M38..M53 have no records, though those milestones
  closed. This is real ledger drift, but **M54 does not touch it**: backfilling ~16 milestones of
  DoD flags now would mean **fabricating retroactive evidence** for cycles this planner did not
  observe — a direct violation of the evidence rule ("never claim a file, test, build, or command
  output that was not read or executed"). Writing `spec_defined: true` for M42 because it probably
  was true is exactly the failure mode the rule exists to prevent.
  → Needs a **separate decision**: either a deliberate reconstruct-from-reports cycle (each flag
  grounded in that milestone's actual report/sign-off), or an explicit "records discontinued after
  M37" note. **Do not silently backfill.**

---

## 8. Evidence Gates + Deferral Protocol

### 8.1 Gates (executed Mac-side by the developer, then QA)

- `pwsh -File tools/validate-assets.ps1` — BIOS present + ≥1 ROM. **PROVEN ON macOS (rev-2,
  coordinator-verified): exit 0, all 7 BIOS present, `ROM file count: 1`** (fmpac.rom only — which
  matches the Windows-generated `docs/asset-checksums.txt:20` `Files: 1`; see §6.4.3). pwsh 7.6.3 is
  installed → the gate runs **verbatim**, no manual `ls` substitute needed.
- `pwsh -File tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — **CAUTION (rev-2):**
  running this on the Mac will **overwrite** the Windows-generated record (whose
  `Directory: D:\Projects\…` headers are the §6.4.3 delta-zero evidence). Capture the Mac output to a
  **separate** file (e.g. `debug/m54/asset-checksums-macos.txt`) and **diff the hashes**, rather than
  clobbering the Windows artifact mid-milestone.
- `cmake --build build` — all 3 targets build clean under AppleClang (Ninja: **no** `--config`).
- `ctest --test-dir build --output-on-failure -LE m24_slow_full_sweep` — **253/253 with SDL3=ON**
  (Ninja: **no** `-C Debug`). Verify against the **three-part AC-M2**: count **and** exact skip-set
  **and** delta==0 — not the count alone. (SDL3=OFF → 237; wrong configuration, not a regression.)
- Determinism byte-identity check (AC-M3).
- **openMSX A/B — N/A** (zero behavior change, DEC-0081). **ZEXALL — N/A** unless a shim ever touches
  `src/core`/`src/devices/cpu` (planned NOT to; scope to avoid — if S2 ever needs a CPU-file edit,
  STOP and escalate, and the ZEXALL sweep re-arms).
- **Windows re-gate:** after the portability edits land, re-run the Windows build + fast ctest to
  prove AC-W1/W2/W3 (byte-identity is the hard bar).

### 8.2 Deferral protocol — **DISCHARGED (rev-2)**

**rev-1's deferral premise no longer holds: the tree is now ON the Mac and the toolchain is
installed.** macOS build/test evidence is *executable today*. History, then the current position:

- **rev-1 (historical):** macOS evidence could not be produced from the Windows machine → developer
  and QA gates were **DEFERRED, not skipped** (orchestration-validated per DEC-0081); no REQ-M54-002
  was issued; the cycle ended with the package + a deferral note in
  `agent-protocol/channels/responses.md` + a `state/current-phase.md` HOLD. **M54 stayed OPEN in
  PLANNING.** That was correct at the time.
- **rev-2 (now):** the working tree was transferred (A5 — full tree, not a clone), the toolchain is
  verified present, and rev-1 has been reviewed **on the Mac**, converting A1/A4/A9 to VERIFIED and
  measuring R1 to zero. **The blocker that justified the deferral is gone.** Normal ordering resumes:
  **developer-evidence (S0 → S1 → S2 → S3 → S4 → S5) → QA sign-off → human-release decision** (NORMAL
  gate; blast radius LOW-MED / build-system). Issuing the developer handoff is the **coordinator's**
  call, not the planner's — this package does not self-authorize it.
- **Pickup context for a fresh Mac-side session:** resume from THIS package (rev-2 —
  `docs/m54-planner-package.md`) + the ledger (DEC-0081, the M54 entry in `state/milestones.md`,
  `current-phase.md`). **Read the Amendment Log first** — rev-1 contained four defects (a failing
  `brew` line, a wrong skip table, a tautological AC-M2, and no S0), and rev-2's corrections are the
  operative text wherever the two differ.
- **Windows gates are NOT deferred** (§5.0): the Windows machine remains available, so AC-W1/W2/W3
  are executed, not waived.

---

## 9. Risks and Mitigations

| ID | Risk | Likelihood / Impact | Mitigation |
|---|---|---|---|
| **R1** | AppleClang/libc++ stricter transitive includes → first-compile errors | ~~MED / LOW~~ → **LOW / LOW (rev-2, MEASURED)** | **Exposure measured, not estimated:** `clang++ -fsyntax-only -std=c++20` over **87 src TUs → 0 failures** and **254 test TUs → 0 failures**. S2 is expected to be a **NO-OP**. Guardrail retained verbatim (include-only; STOP-and-escalate) in case the real build differs from the syntax sweep; **an EMPTY S2 diff is the success case — do NOT manufacture edits.** |
| **R2** | Line-ending fixture divergence | LOW / LOW | All fixture reads are `std::ios::binary` (§3.1) → immune; `.gitattributes` binary-pins are belt-and-suspenders (§3.2). |
| **R3** | `find_package(SDL3 CONFIG REQUIRED)` fails on the Mac prefix (`CMakeLists.txt:147`) | ~~MED / MED~~ → **LOW / LOW (rev-2, RETIRED)** | **Coordinator-verified resolved:** SDL3 **3.4.12** present at `/opt/homebrew/lib/cmake/SDL3` and found **without** `-DCMAKE_PREFIX_PATH`. Fallbacks retained but unexercised: re-add `CMAKE_PREFIX_PATH=$(brew --prefix)`; vendored build from `references/sdl3`; `-DSONY_MSX_ENABLE_SDL3=OFF` (→ 237 tests, not 253). |
| **R4** | Case-sensitive-FS asset/`#include` traps | LOW / MED | §4 filename-case audit; code + BIOS names are uniformly lowercase and already match on disk. |
| **R5** | PowerShell tooling gap on macOS | LOW / LOW | `pwsh` via Homebrew, or documented `shasum`/`ls` equivalents; gates are dev-only, not in ctest. |
| **R6** | Single- vs multi-config ctest/build invocation confusion | MED / LOW | §2.2 invocation table + runbook; Ninja recommended (no `-C`, binaries in `build/`). |
| **R7** | **Bare `git clone` lacks tests/tools/docs/references + untracked assets** → can't reach 253 or even read this package | HIGH / HIGH | Runbook Step 0 mandates a **full working-tree transfer** and verifies it before any build. |
| **R8** | Universal-binary expectation creep | LOW / LOW | Declared explicit NON-goal (§2.4) with justification; native arch only. |
| **R9** | macOS SDK / deployment-target mismatch for SDL3 (needs Xcode ≥12.2, macOS 11.0 SDK — `references/sdl3/docs/README-macos.md:33`) | **MOOT (rev-2)** | **Coordinator-verified: macOS SDK 26.5 + Apple clang 21.0.0 installed** — vastly exceeds the 11.0-SDK floor; Homebrew SDL3 3.4.12 is prebuilt for this host. No action. |
| **R10** | A future behavior milestone needs openMSX A/B on macOS but the harness assumes WSL `/usr/bin/openmsx` | LOW (out of M54) / MED | Documented in §5.2 as a future portability note (path override); N/A for M54. |
| **R11** | An S2 include fix accidentally drifts behavior (Windows regression) | LOW / HIGH | AC-W1/W2 hard bar: Windows rebuild must stay 253/253 byte-identical after every edit; diff review that only includes changed. |
| **R12** | **TRANSFER-INDUCED CRLF-over-LF blob flip (rev-2, NEW).** The transferred working copy has `CMakeLists.txt`, `README.md`, `sony_msx_hbf1xv.xml` as **CRLF on disk over LF blobs**; `git status` shows all three modified (autocrlf is not normalizing on the Mac). S1 rewrites `CMakeLists.txt` (~206 lines) and S4 rewrites `README.md` → the tracked blob flips LF→CRLF in a whole-file diff and **propagates to Windows**, churning that checkout = **AC-W3 violation** + an unreviewable diff. | **HIGH / HIGH** (certain to fire if S0(b) is skipped) | **S0(b) is the mitigation and it is MANDATORY and FIRST:** `git checkout -- CMakeLists.txt README.md sony_msx_hbf1xv.xml` **before** S1's first edit (restores LF blobs; discards only EOL noise — verify with `git diff --ignore-all-space` = empty). Then per-edit: `git diff --stat` must show a **small semantic** diff, never whole-file. Do **not** set `core.autocrlf` on the Mac; do **not** commit CRLF bytes; do **not** "solve" it with repo-wide renormalization (§3.2.1). AC-S0(b) + AC-S1's diff-shape check + AC-W3 all test for this. |
| **R13** | **Stale foreign build tree (rev-2).** The copied `build/` was a **Windows/Visual Studio** cache (`CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026`, `CMAKE_HOME_DIRECTORY:INTERNAL=D:/Projects/sony-msx-hbf1xv` — coordinator-read pre-deletion) → configure fails/half-succeeds on the Mac; tempting "fix" = create `build-mac/` = **DEC-0041 violation**. | **RESOLVED (rev-2)** | `rm -rf build` **executed** (owner-approved) — this **upholds** DEC-0041 (one tree named `build/`, recreated natively). Codified as **S0(a)/AC-S0(a)**, which also forbids any second tree. |
| **R14** | **Skip-worktree flag loss on a future clone (rev-2).** `roms/fmpac.rom.sram`'s `skip-worktree` bit lives in the **index**, not the repo — it survived only because `.git/` was copied. A fresh clone silently loses it → the "NOT TO BE ALTERED" FM-PAC SRAM pair (`roms/README.md`) becomes writable-dirty and can be committed by accident. | LOW (now) / MED | **Verified intact today** (`git ls-files -v roms/fmpac.rom.sram` → `S`). Codified as S0(c)/AC-S0(c) and as a Step 0 verify line; **re-check after ANY fresh clone** on any machine. |

---

## 10. Assumptions with Verification Actions

| ID | Assumption | Status (rev-2) | Verification action |
|---|---|---|---|
| **A1** | Mac is Apple Silicon (arm64) | **VERIFIED** (coordinator) | `uname -m` → **`arm64`**, Darwin 25.5.0. Closed. |
| **A2** | Both platforms little-endian → goldens carry over | OPEN (by design) | Run one golden-oracle test on macOS; compare its hash/SHA to the Windows value (**AC-M3** — this assumption *is* the thing AC-M3 proves). |
| **A3** | AppleClang/libc++ compiles the C++20 code (`std::filesystem`, `std::optional`, etc.) | **STRONGLY INDICATED** | `clang++ -fsyntax-only -std=c++20`: **87 src TUs → 0 failures; 254 test TUs → 0 failures**. Full proof = the S2 real build (link included). |
| **A4** | Homebrew SDL3 provides a `find_package(SDL3 CONFIG)`-compatible config | **VERIFIED** (coordinator) | SDL3 **3.4.12** at `/opt/homebrew/lib/cmake/SDL3`; resolves **WITHOUT** `-DCMAKE_PREFIX_PATH` → dropped from Step 2 as a requirement (fallback only). R3 retired. |
| **A5** | Owner transfers the **full** working tree (not a bare clone) | **VINDICATED — but its verify was INCOMPLETE** | rev-1's `ls …` checks **presence only**. It does **not** detect the three things the transfer actually broke: **staleness** (a Windows `build/` rode along — R13), **line endings** (CRLF-over-LF working copy — R12/§3.2), **index flags** (skip-worktree — R14). **Strengthened Step 0 verify** now adds `test ! -e build`, `git status --porcelain` (must be EMPTY), and `git ls-files -v roms/fmpac.rom.sram` (must show `S`). |
| **A6** | No MSVC/Windows-only std API in `src/` | HOLDING | Grounded: grep found zero `windows.h`/`_WIN32`/`__declspec`/etc.; reinforced by the 0-failure syntax sweep; confirmed at the S2 compile. |
| **A7** | 253 is the current fast-subset baseline (**SDL3=ON**) | OPEN — **and VERIFIABLE** | **The Windows machine REMAINS AVAILABLE** (owner, rev-2) → re-run the Windows fast ctest before any edit; expect **253/253**. Structurally corroborated: 254 `add_test` registrations − 1 `m24_slow_full_sweep` (`tests/CMakeLists.txt:1010-1013`) = 253. **With SDL3=OFF the baseline is 237** (16 SDL3-gated tests, `tests/CMakeLists.txt:1794-1980`) — configure **ON**. |
| **A8** | msx-disk golden SHA is arch-neutral | OPEN (by design) | Mac `--create` SHA256 == `6f1a7983…b0ce5188` (DEC-0080) — part of AC-M3/AC-M4. |
| **A9** | `roms/fmpac.rom.sram` skip-worktree flag survived the transfer (the "NOT TO BE ALTERED" pair stays clean) | **VERIFIED** (coordinator) | `git ls-files -v roms/fmpac.rom.sram` → **`S`** — flag **INTACT**, file not dirty. It survived because `.git/` was **copied, not re-cloned**. **MUST be re-checked after any fresh clone** (the bit is index-local, not repo-carried) — see R14 + S0(c). |

---

## Developer Handoff (Mac-side slice decomposition — S0 → S5)

Ordered slices; each is behavior-neutral and gated by AC-W1 (Windows stays 253/253) after landing.

- **S0 — TREE HYGIENE (rev-2, NEW — GATES S1; do this FIRST).** The transferred tree is not yet in a
  safe state to edit. (a) `rm -rf build` — **DONE** (owner-approved; the stale Windows/VS cache would
  break configure — R13; deleting it **upholds** DEC-0041, creating `build-mac/` would be the
  violation). (b) **`git checkout -- CMakeLists.txt README.md sony_msx_hbf1xv.xml`** — **NOT DONE;
  this is the developer's FIRST action.** Restores the LF blobs before S1 edits `CMakeLists.txt`,
  preventing the CRLF blob flip that would churn Windows (R12/§3.2/AC-W3). (c) skip-worktree verified
  — **DONE** (`S` intact — R14). (d) prerequisites — **DONE** (ninja/pwsh/cmake/clang/SDL3).
  Prove: AC-S0 — no `build/`, no second tree, `git status --porcelain` **EMPTY**, `git ls-files -v
  roms/fmpac.rom.sram` → `S`. **S1 must not start until (b) is true.**
- **S1 — Build-matrix CMake branches + `.gitattributes`.** Add the `if(MSVC)/else()` warning branch
  (§2.1); confirm the existing `if(WIN32)`/`if(APPLE)`-shaped guards already no-op on macOS (§2.0);
  add the root `.gitattributes` binary-pins (§3.2). No SDL3 code change. Prove: macOS configures with
  Ninja+Homebrew SDL3; Windows rebuild 253/253 byte-identical; no `.gitattributes` churn.
- **S2 — First-compile fix pass (mechanical includes ONLY).** Build under AppleClang; add missing
  `#include`s surfaced as hard errors. **Any behavioral/logic edit STOPS and escalates** (and, if it
  would touch `src/core`/`src/devices/cpu`, re-arms ZEXALL). Prove: clean macOS compile of all 3
  targets; Windows byte-identity held; diff = includes only.
- **S3 — Tooling / bootstrap portability.** Add `tools/bootstrap-build.sh` (bash, LF) mirroring the
  Ninja+SDL3 flow, OR ratify the manual runbook commands as authoritative; wire the pwsh/`shasum`
  gate equivalents. No suite effect.
- **S4 — Docs.** Add a macOS build section to `README.md` (published) mirroring §6; satisfy the owner
  doc-sweep expectation (README + CLAUDE.md files + agents + baseline reflect dual-platform support —
  **the ONE codebase supports BOTH macOS and Windows, auto-detected**, §1.1).
  **⚠ S4 edits `README.md` → S0(b) MUST have restored its LF blob first, or S4 flips the blob to CRLF
  in a whole-file diff (R12). Check `git diff --stat` after editing.**

  **rev-2 — additional stale-doc findings from the Mac-side review (all docs-only, zero behavior):**

  | Finding | Location (read + verified) | Correction |
  |---|---|---|
  | **`games/` inventory is stale** — claims a **flat** `games/roms/*.rom` layout and lists titles that do not exist, while omitting several that do | `CLAUDE.md:242-246` | Reality on disk: **nested-with-spaces** — `games/roms/Aleste 2/aleste2.rom`, `games/roms/Metal Gear/metal_gear.rom`, `games/roms/Metal Gear 2/metal_gear2.rom`, `games/roms/King's Valley 2/…`; disks are `games/disks/ys2/ys2-d1.dsk` + `ys2-d2.dsk` (**not** `d1.dsk`/`d2.dsk` as `:243` claims), plus `ys2-save.dsk`. **`Gradius3` (`:244`) does NOT exist.** Undocumented but present: `games/roms/Space Manbow/`, `games/roms/firebird/`, `games/disks/Psycho World/`, `games/disks/Load Runner/`. **Note:** this is the *documentation* fix only — **changing the test paths is NG-1, out of scope.** |
  | **"clone" wording contradicts the package's own R7/A5** | `agent-protocol/state/current-phase.md:11` — "the owner will **clone** on their Mac and continue there" | R7/A5 mandate a **full working-tree transfer** precisely because a clone lacks `tests/ tools/ docs/ references/` and this package itself (`.gitignore:214-224`). Reword to "transfer the full working tree" so the ledger does not instruct the exact failure the package forbids. (What actually happened: a full transfer.) |
  | **Windows-shaped help text** (cosmetic) | `src/frontend/sdl3_main.cpp:69-91` — every example reads `sony_msx_sdl3.exe` with **backslash** paths (`roms\aleste.rom`, `disks\msxdos22.dsk`); comment at `src/frontend/sdl3_app.cpp:247-251` describes "the Windows taskbar button" / "the .exe's Explorer icon" | **Cosmetic only — these are string literals and comments, NOT behavior.** On macOS the binary is `sony_msx_sdl3` (no `.exe`) and paths use `/`. Also note `:78` references `disks\msxdos22.dsk`, an image that no longer exists (`disks/` standardized on `msxdos23.dsk`). Lowest priority; if touched, it is a **string/comment-only** edit — the same STOP-and-escalate discipline as S2 applies, and Windows must stay 253/253 byte-identical. |
- **S5 — Dual-platform evidence run.** Capture the macOS gate evidence (§6 Step 6) and a Windows
  253/253 re-run; assemble the QA package. QA sign-off → NORMAL human-release gate.

**Handoff note (rev-2 — SUPERSEDES rev-1's).** rev-1 said: *"This is a PLANNING-ONLY cycle. No
developer handoff (REQ-M54-002) is issued now; the deferral is recorded and M54 stays OPEN."* That
reflected a tree that was still on Windows. **It no longer describes reality:** the full working tree
is on the Mac, the toolchain is verified, and rev-1 has been reviewed there (§8.2). The planner's
deliverable for this amendment cycle is **rev-2 of this package only** — no code, no CMake. Whether
to issue the developer handoff is the **coordinator's** decision; this package neither issues nor
assumes it.

**If/when the Mac-side developer starts, the first three things to internalize:**

1. **S0(b) FIRST — `git checkout -- CMakeLists.txt README.md sony_msx_hbf1xv.xml`** before touching
   anything. Skipping it silently corrupts S1's diff and breaks AC-W3 on Windows (R12/§3.2).
2. **S2 is expected to be a NO-OP.** 0 syntax failures across 87 src + 254 test TUs. An **empty diff
   is the success case**; manufacturing includes to look productive is forbidden (AC-S2).
3. **The `games/`-gated skips are pre-existing and platform-symmetric** (delta = 0, §6.4.3). They are
   not yours to fix (NG-1) and not evidence of a bad port. **Verify AC-M2's three parts** — count,
   exact skip-set, zero delta — not the count alone.

**Slice order is a dependency chain, not a preference:** S0 gates S1 (clean tree before the CMake
edit); S1 gates S2 (build system must configure before anything compiles); S5 aggregates. AC-W1 fires
**after every** edit that lands, not once at the end.
