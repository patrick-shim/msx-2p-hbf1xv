# M54 Planner Package — macOS Support (Single Codebase, Dual Toolchain: MSVC + AppleClang)

- Milestone ID: M54
- Decision: DEC-0081
- Request: REQ-M54-001
- Type: PLANNING-ONLY cycle (owner directive). NO production code, NO CMake edits this cycle.
- Spec Owner: MSX Planner Agent
- Developer Owner: DEFERRED — executes on the owner's macOS machine (real-hardware evidence).
- QA Owner: DEFERRED — follows the Mac-side developer evidence.
- Baseline: post-M53, fast-subset ctest = **253/253** (developer-pinned; ledger `state/milestones.md`
  M53 entry). Windows byte-identity of this baseline is the hard regression bar.

This package is written to be executed on the Mac by the owner **plus a fresh Claude Code session
that does not have this session's context**. The MACOS RUNBOOK (§6) is the operative deliverable;
everything else grounds it.

---

## 1. Scope and Assumptions

### 1.1 Scope

Make the ONE repository / ONE codebase compile and pass its deterministic suite on **both**
Windows (MSVC) and macOS (AppleClang), with the target platform **auto-detected at build time**
via CMake `if(MSVC)`/`if(WIN32)`/`if(APPLE)` branching only. No fork, no platform branches, no
separate build files. Three targets must build on macOS: `sony_msx_headless`, `sony_msx_sdl3`,
`msx_disk` (output name `msx-disk`, no `.exe`).

**In scope (Mac-side execution, planned here):** CMake platform branches + AppleClang warning
flags; a root `.gitattributes`; a mechanical first-compile include-fix pass; tooling/bootstrap
portability; a README macOS section; a dual-platform evidence run.

**Out of scope (DEC-0081):** any device/timing behavior change (→ openMSX A/B N/A; ZEXALL only if
`src/core`/`src/devices/cpu` is ever touched — planned NOT to be); macOS `.app`/`.icns` packaging
polish (plain binaries this milestone); universal (fat) binaries (see §2.4 verdict).

### 1.2 Assumptions (full verification actions in §10)

- **A5 (decisive):** The owner will transfer the **full working tree** to the Mac, NOT rely on a
  bare `git clone`. `.gitignore:214-224` marks `tests/ tools/ docs/ references/ agent-protocol/
  .claude/ debug/` and `CLAUDE.md`/`src/CLAUDE.md` as **local-only (untracked)**; a public-remote
  clone therefore contains none of them — including *this package* and the entire test suite. The
  runbook Step 0 makes the full-tree transfer mandatory and verifies it.
- **A1:** The Mac is Apple Silicon (arm64). Verify `uname -m`. Intel (x86_64) works identically —
  only the native arch differs.
- **A2:** Both platforms are little-endian → golden hashes/fixtures carry over byte-identically.
- **A7:** 253 is the current fast-subset baseline (M53 close). Re-confirm on Windows before any edit.

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

**Recommendation: Ninja on macOS** — fast, minimal, and the fast-subset filter works identically
(`ctest --test-dir build -LE m24_slow_full_sweep`). Xcode is the alternate for IDE-driven
debugging. The single-config binary path (`build/`, not `build/Debug/`) is the most common Mac-side
trip hazard — the runbook calls it out.

### 2.3 SDL3 on macOS (vendored/arm64)

Two supply routes; **recommend Homebrew** for the runbook, with the vendored build as the
context-free fallback that mirrors `tools/bootstrap-build.ps1:44-59`:

1. **Homebrew (RECOMMENDED):** `brew install sdl3` installs `SDL3Config.cmake` under
   `$(brew --prefix)/lib/cmake/SDL3` (arm64 prefix `/opt/homebrew`). `find_package(SDL3 CONFIG
   REQUIRED)` (`CMakeLists.txt:147`) resolves it when configured with
   `-DCMAKE_PREFIX_PATH=$(brew --prefix)`. No DLL/dylib staging is needed — the `if(WIN32)` guard
   (`CMakeLists.txt:161`) skips it, and the Homebrew dylib is found via its install path.
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

## 3. Line-Ending / `.gitattributes` Policy (slice S1)

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

### 3.2 Policy — minimal, ZERO-churn (hard requirement)

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

### 5.1 `tools/*.ps1` on macOS

The evidence-gate scripts are **dev-only** (they are NOT invoked inside ctest — tests are pure C++
executables). Two options, both acceptable:

- **pwsh (verbatim):** `brew install --cask powershell` gives `pwsh`; run
  `pwsh -File tools/validate-assets.ps1` and `pwsh -File tools/checksum-assets.ps1 -OutFile
  docs/asset-checksums.txt` unchanged. (`-ExecutionPolicy Bypass` is **Windows-only** and simply
  omitted on macOS — R: execution-policy N/A.)
- **Manual equivalents (no pwsh):** validate = confirm `bios/f1xvbios.rom` + ≥1 `roms/*.rom` exist
  (`ls bios roms`); checksums = `shasum -a 256 bios/* roms/* > docs/asset-checksums.txt` (note:
  format differs from the PowerShell tool's output; use it for spot-checks, not byte-diffing against
  the Windows `docs/asset-checksums.txt`).

Recommendation: install `pwsh` so the gates run verbatim and produce comparable output; document the
manual `shasum` path as the zero-dependency fallback.

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
- **Verify Step 0:** `ls tests/CMakeLists.txt tools/bootstrap-build.ps1 references/sdl3/CMakeLists.txt
  docs/m54-planner-package.md bios/f1xvbios.rom` — all must exist before proceeding.

### Step 1 — Prerequisites

```bash
xcode-select --install                 # Xcode Command Line Tools → AppleClang + git + make
# Homebrew (if absent): https://brew.sh
brew install cmake ninja sdl3          # build system + generator + SDL3 (SDL3Config.cmake)
brew install --cask powershell         # OPTIONAL: run tools/*.ps1 gates verbatim
uname -m                               # expect arm64 (A1)
cmake --version                        # expect >= 3.21 (CMakeLists.txt:14)
```

### Step 2 — Configure + build (RECOMMENDED: Ninja + Homebrew SDL3)

```bash
cd ~/sony-msx-hbf1xv
cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DSONY_MSX_ENABLE_SDL3=ON \
      -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build
```

Alternate generator (Xcode, IDE debugging):

```bash
cmake -S . -B build -G Xcode -DSONY_MSX_ENABLE_SDL3=ON -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build --config Debug
```

Headless-only fallback (no SDL3 available): add `-DSONY_MSX_ENABLE_SDL3=OFF` (builds
`sony_msx_headless` + `msx_disk` + tests; skips the SDL3 target).

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

### Step 4 — Expected results

- Three executables present:
  - Ninja/Makefiles: `build/sony_msx_headless`, `build/sony_msx_sdl3`, `build/msx_disk` (+ copied
    `diskutils/msx-disk`).
  - Xcode: same names under `build/Debug/`.
- Fast ctest: **253/253** *iff* all assets were transferred (Option A). Tests that print
  `SKIP: … (returns 0)` still **pass**, so counts stay green even where an asset is absent.
- **Asset-gated tests that SKIP (pass) if their untracked asset is missing** (enumerated honestly —
  these need `games/`, `disks/msxdos23.dsk`, a non-`fmpac` `roms/` ROM, or the untracked
  `references/…/softwaredb.xml`):

  | Test | Needs (untracked) |
  |---|---|
  | `machine_hbf1xv_m19_aleste_smoke_integration_test` | `games/roms/aleste2.rom` (`hbf1xv_m19_aleste_smoke_integration_test.cpp:207-210`) |
  | `machine_hbf1xv_m19_metalgear_smoke_integration_test` | `games/roms/metal_gear.rom` (`…metalgear_smoke…:286`) |
  | `hbf1xv_m27_replay_determinism_system_test` | `games/roms/aleste2.rom` (`…m27_replay…:199`) |
  | `hbf1xv_m28_c5_disk_boot_investigation_system_test` | `disks/msxdos23.dsk` (`…m28_c5…:153`) |
  | `machine_hbf1xv_m30_cartridge_identification_integration_test` | game ROM (SHA-gated) + softwaredb (`…m30…:93,112`) |
  | `machine_hbf1xv_m31_fm_title_probe_integration_test` | game ROM (`…m31_fm_title_probe…:138`) |
  | `machine_hbf1xv_m31_metalgear2_scc_integration_test` | `games/roms/metalgear2_scc.rom` + softwaredb (`…m31_metalgear2_scc…:172,189`) |
  | `hbf1xv_m34_aleste_ultrasonic_regression_system_test` | game ROM (SHA-gated) (`…m34…:209,216`) |
  | `diskutils_bpb_matches_machine_unit_test` | `disks/msxdos23.dsk` (M53 cross-check, `diskutils_bpb_matches_machine_unit_test.cpp:104`) |

  ≈ **9 of 253** are asset-gated on untracked content. With Option A (full transfer incl.
  `games/ disks/ references/`) all of them run for genuine 253/253. If any asset is intentionally
  not copied, that test SKIPs (green) — record which skipped so the count is honest. Tests gated on
  the **tracked** `bios/` and `roms/fmpac.rom` do **not** skip on a full transfer.

### Step 5 — Smoke the three binaries (sanity)

```bash
./build/sony_msx_headless --help              # (path build/Debug/… under Xcode)
./build/msx_disk --create /tmp/scratch.dsk    # then --read /tmp/scratch.dsk --sector 0
# SDL3 window (interactive): ./build/sony_msx_sdl3   (Metal/CoreAudio native)
```

### Step 6 — Evidence to capture for the Mac-side developer gate

Capture under `debug/m54/` (create it): `uname -a`, `cmake --version`, the configure log
(SDL3 discovery line), the build log (clean, all 3 targets), the full `ctest` output (pass count +
the exact list of any SKIPs), a golden-oracle byte-identity check (see §7 AC), and a
`shasum -a 256` of one created `.dsk` compared to the golden `6f1a7983…b0ce5188`
(`tools/format-blank-disk.ps1` blank; DEC-0080).

### Step 7 — Troubleshooting playbook (first-compile)

- **`fatal error: 'X' file not found` / `use of undeclared identifier 'std::…'`** → AppleClang/libc++
  is stricter than MSVC about transitive includes (R1). Fix = add the missing `#include`
  (`<cstdint> <cstddef> <cstring> <algorithm> <memory> <array> <limits> <utility>`) to the flagged
  file. **Mechanical only.** Any change beyond adding an include **STOPS and escalates** (S2 rule).
- **`Could NOT find SDL3` at configure** (`CMakeLists.txt:147`) → pass
  `-DCMAKE_PREFIX_PATH="$(brew --prefix)"`, or `brew install sdl3`, or use the vendored fallback
  (Step 2), or configure `-DSONY_MSX_ENABLE_SDL3=OFF` to unblock headless+diskutil first.
- **`ctest` reports "No tests were found"** → you used a bare clone without `tests/` (Step 0), or you
  passed `-C Debug` to a single-config Ninja build (omit it for Ninja).
- **Binary not where expected** → single-config generators emit to `build/`, not `build/Debug/`.
- **`-ExecutionPolicy Bypass` error** → that flag is Windows-only; drop it (use `pwsh -File …`).
- **`msx-disk` not in `diskutils/`** → the POST_BUILD copy runs on a successful `msx_disk` build;
  rebuild that target; `make_directory` (`CMakeLists.txt:141`) creates `diskutils/` if absent.

---

## 7. Acceptance Criteria

### 7.1 Planning-cycle AC (THIS cycle — closeable now)

- **AC-P1:** `docs/m54-planner-package.md` delivered, covering DEC-0081 items (1)-(10). ✔ (this doc)
- **AC-P2:** Portability audit cites concrete `file:line` for every platform-sensitive surface (§2.0,
  §3.1, §4). ✔
- **AC-P3:** MACOS RUNBOOK is executable without this session's context, incl. the full-tree-transfer
  step, both generators, expected 253 + enumerated skips, and a troubleshooting playbook (§6). ✔
- **AC-P4:** Deferral note recorded (developer/QA execute Mac-side); **M54 stays OPEN in PLANNING**
  (§8). ✔ (coordinator persists the RESP/deferral entry.)
- **AC-P5:** Zero code / zero CMake change this cycle (grep/read only). ✔

### 7.2 Milestone AC — Windows (the HARD regression bar)

- **AC-W1:** After every portability edit, a clean Windows rebuild from the ONE `build/` tree passes
  **253/253** fast-subset ctest — **name-for-name unchanged** from the M53 baseline.
- **AC-W2:** Golden-oracle tests are **byte-identical** to pre-M54 (frame hashes, disk SHA256,
  debug-dump goldens): zero behavior diff. (Expected trivially — edits are includes + CMake +
  `.gitattributes` only.)
- **AC-W3:** The existing Windows checkout is **not churned** by `.gitattributes` (no mass
  renormalization diff; `git status` shows only the intended tracked adds/edits).

### 7.3 Milestone AC — macOS

- **AC-M1:** Clean AppleClang build of **all three** targets (`sony_msx_headless`, `sony_msx_sdl3`,
  `msx_disk`→`msx-disk`) with the recommended Ninja generator (Xcode alternate proven at least once).
- **AC-M2:** Fast-subset ctest reaches **parity with the Windows 253 baseline modulo the enumerated
  §6 asset-gated skips** — i.e., 253/253 with a full asset transfer, or 253 green with the exact
  skipped set recorded when an asset is intentionally absent. No unexpected failures.
- **AC-M3:** Determinism byte-identity: a golden-oracle test's output on macOS equals the Windows
  value (e.g., a frame-dump hash, the `--create` disk SHA256 = `6f1a7983…b0ce5188`, a debug-dump
  golden) — proving the little-endian assumption (A2) and cross-platform determinism.
- **AC-M4:** `diskutils/msx-disk` (no suffix) is produced and functional (`--create/--read/--format`
  exit-code matrix 0/1/2/3 behaves as on Windows).

### 7.4 Per-slice AC

- **AC-S1 (CMake matrix + `.gitattributes`):** platform branch compiles under both toolchains; the
  AppleClang warning branch active on macOS, `/utf-8` still MSVC-only; root `.gitattributes` adds the
  binary-pins with **zero** renormalization churn (AC-W3). Windows stays 253/253 (AC-W1).
- **AC-S2 (first-compile fix pass):** macOS build reaches a clean compile using **include-only**
  edits; a diff review confirms **no** behavioral/logic line changed; Windows rebuild stays
  253/253 byte-identical. Any non-include need was escalated, not silently applied.
- **AC-S3 (tooling/bootstrap):** `bootstrap-build.sh` (if added) produces the same `build/` state as
  the manual commands, or the manual commands are proven authoritative; asset gates run (pwsh or
  `shasum`). No effect on the deterministic suite.
- **AC-S4 (docs):** README gains a macOS build section (published doc) mirroring the runbook; the
  owner doc-sweep expectation (README + CLAUDE.md files + agents + baseline) is satisfied for the
  new capability.
- **AC-S5 (dual-platform evidence):** both platforms' evidence captured (§6 Step 6 on macOS; a
  Windows 253/253 re-run) proving AC-W1..3 and AC-M1..4.

---

## 8. Evidence Gates + Deferral Protocol

### 8.1 Gates (executed Mac-side by the developer, then QA)

- `pwsh -File tools/validate-assets.ps1` (or the `ls` manual equivalent) — BIOS present + ≥1 ROM.
- `pwsh -File tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` (or `shasum -a 256`).
- `cmake --build build [--config Debug]` — all 3 targets build clean under AppleClang.
- `ctest --test-dir build [-C Debug] --output-on-failure -LE m24_slow_full_sweep` — 253 parity
  (modulo enumerated skips).
- Determinism byte-identity check (AC-M3).
- **openMSX A/B — N/A** (zero behavior change, DEC-0081). **ZEXALL — N/A** unless a shim ever touches
  `src/core`/`src/devices/cpu` (planned NOT to; scope to avoid — if S2 ever needs a CPU-file edit,
  STOP and escalate, and the ZEXALL sweep re-arms).
- **Windows re-gate:** after the portability edits land, re-run the Windows build + fast ctest to
  prove AC-W1/W2/W3 (byte-identity is the hard bar).

### 8.2 Deferral protocol (this cycle)

- macOS build/test evidence **cannot** be produced on this Windows machine → the developer and QA
  gates are **DEFERRED, not skipped** (orchestration-validated protocol-compliant per DEC-0081).
  **No REQ-M54-002 developer handoff is issued this cycle.**
- This cycle ends with: (a) this package, and (b) a recorded **deferral note** in
  `agent-protocol/channels/responses.md` (coordinator persists) + `state/current-phase.md` HOLD.
  **M54 stays OPEN in PLANNING.**
- **Pickup on the Mac:** a fresh Claude Code session resumes from THIS package (`docs/m54-planner-package.md`)
  + the ledger (DEC-0081, the M54 entry in `state/milestones.md`, `current-phase.md`). Normal
  ordering resumes there: developer-evidence (slices S1→S5) → QA sign-off → human-release decision
  (NORMAL gate; blast radius LOW-MED / build-system).

---

## 9. Risks and Mitigations

| ID | Risk | Likelihood / Impact | Mitigation |
|---|---|---|---|
| **R1** | AppleClang/libc++ stricter transitive includes → first-compile errors | MED / LOW | S2 mechanical include-only fix pass; troubleshooting playbook §6.7; STOP-and-escalate on any non-include edit. |
| **R2** | Line-ending fixture divergence | LOW / LOW | All fixture reads are `std::ios::binary` (§3.1) → immune; `.gitattributes` binary-pins are belt-and-suspenders (§3.2). |
| **R3** | `find_package(SDL3 CONFIG REQUIRED)` fails on the Mac prefix (`CMakeLists.txt:147`) | MED / MED | Homebrew SDL3 + `CMAKE_PREFIX_PATH=$(brew --prefix)`; vendored fallback from `references/sdl3`; or `-DSONY_MSX_ENABLE_SDL3=OFF` to unblock the other two targets. |
| **R4** | Case-sensitive-FS asset/`#include` traps | LOW / MED | §4 filename-case audit; code + BIOS names are uniformly lowercase and already match on disk. |
| **R5** | PowerShell tooling gap on macOS | LOW / LOW | `pwsh` via Homebrew, or documented `shasum`/`ls` equivalents; gates are dev-only, not in ctest. |
| **R6** | Single- vs multi-config ctest/build invocation confusion | MED / LOW | §2.2 invocation table + runbook; Ninja recommended (no `-C`, binaries in `build/`). |
| **R7** | **Bare `git clone` lacks tests/tools/docs/references + untracked assets** → can't reach 253 or even read this package | HIGH / HIGH | Runbook Step 0 mandates a **full working-tree transfer** and verifies it before any build. |
| **R8** | Universal-binary expectation creep | LOW / LOW | Declared explicit NON-goal (§2.4) with justification; native arch only. |
| **R9** | macOS SDK / deployment-target mismatch for SDL3 (needs Xcode ≥12.2, macOS 11.0 SDK — `references/sdl3/docs/README-macos.md:33`) | LOW / MED | Prerequisites step installs current Xcode CLT; Homebrew SDL3 is prebuilt for the host. |
| **R10** | A future behavior milestone needs openMSX A/B on macOS but the harness assumes WSL `/usr/bin/openmsx` | LOW (out of M54) / MED | Documented in §5.2 as a future portability note (path override); N/A for M54. |
| **R11** | An S2 include fix accidentally drifts behavior (Windows regression) | LOW / HIGH | AC-W1/W2 hard bar: Windows rebuild must stay 253/253 byte-identical after every edit; diff review that only includes changed. |

---

## 10. Assumptions with Verification Actions

| ID | Assumption | Verification action |
|---|---|---|
| **A1** | Mac is Apple Silicon (arm64) | `uname -m` → `arm64` (Intel `x86_64` works identically). |
| **A2** | Both platforms little-endian → goldens carry over | Run one golden-oracle test on macOS; compare its hash/SHA to the Windows value (AC-M3). |
| **A3** | AppleClang/libc++ compiles the C++20 code (`std::filesystem`, `std::optional`, etc.) | The S2 first compile is the proof; fix missing includes mechanically. |
| **A4** | Homebrew SDL3 provides a `find_package(SDL3 CONFIG)`-compatible config | Configure succeeds Mac-side; else vendored fallback from `references/sdl3`. |
| **A5** | Owner transfers the **full** working tree (not a bare clone) | Runbook Step 0 verify: `ls tests/CMakeLists.txt references/sdl3/CMakeLists.txt docs/m54-planner-package.md`. |
| **A6** | No MSVC/Windows-only std API in `src/` | Grounded: grep found zero `windows.h`/`_WIN32`/`__declspec`/etc.; confirmed at S2 compile. |
| **A7** | 253 is the current fast-subset baseline | Re-run Windows fast ctest before any edit; expect 253/253. |
| **A8** | msx-disk golden SHA is arch-neutral | Mac `--create` SHA256 == `6f1a7983…b0ce5188` (DEC-0080). |

---

## Developer Handoff (Mac-side slice decomposition — execute later)

Ordered slices; each is behavior-neutral and gated by AC-W1 (Windows stays 253/253) after landing.

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
  doc-sweep expectation (README + CLAUDE.md files + agents + baseline reflect dual-platform support).
- **S5 — Dual-platform evidence run.** Capture the macOS gate evidence (§6 Step 6) and a Windows
  253/253 re-run; assemble the QA package. QA sign-off → NORMAL human-release gate.

**Handoff note:** This is a PLANNING-ONLY cycle. No developer handoff (REQ-M54-002) is issued now;
the deferral is recorded and M54 stays OPEN. The Mac-side developer/QA resume from this package and
the ledger.
