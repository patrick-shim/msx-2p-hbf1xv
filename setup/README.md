# setup/ — build bootstrap

The one-command way to build and test the Sony HB-F1XV emulator from a fresh
clone. One codebase, two toolchains, **one** build tree at `../build/` (single-build
policy, DEC-0041). These scripts are the published, cross-platform entry point;
the day-to-day dev/test/debug helpers live in `../tools/` (which is not published).

| Host                                   | Script            | Generator                    | Executables land in |
| -------------------------------------- | ----------------- | ---------------------------- | ------------------- |
| **Windows** (Visual Studio / MSVC)     | `setup/build.ps1` | multi-config (`--config`)    | `build/Debug/*.exe` |
| **macOS / Linux / Raspberry Pi**       | `setup/build.sh`  | single-config (`CMAKE_BUILD_TYPE`) | `build/` (no suffix) |

## Windows

```powershell
powershell -ExecutionPolicy Bypass -File setup/build.ps1 -RunTests
```

Builds SDL3 once from the bundled `src/external/sdl3/` source into
`build/_sdl3_install` (only if missing), configures `build/` with
`-DSONY_MSX_ENABLE_SDL3=ON` (the superset: both executables + all tests), builds
Debug, and runs the fast ctest subset. Add `-Slow` to include the ZEXALL/ZEXDOC
sweep; `-Config Release` for a Release build.

Requirements: CMake, a C++20 MSVC toolchain (VS 2022+, "Desktop development with
C++"), PowerShell. No separate SDL3 install needed.

## macOS / Linux / Raspberry Pi

```bash
setup/build.sh --run-tests
```

Uses the **system** SDL3 dev package by default — `find_package(SDL3 CONFIG
REQUIRED)` locates it with no `CMAKE_PREFIX_PATH`:

- macOS: `brew install sdl3 cmake ninja` (`brew install powershell` only if you
  want to run the `tools/*.ps1` asset gates).
- Debian / Raspberry Pi OS: `sudo apt install cmake ninja-build build-essential libsdl3-dev`.

Add `--slow` for the full ZEXALL/ZEXDOC ctest, `--config Release` for Release, or
`--vendored-sdl3` to build SDL3 from `src/external/sdl3/` instead of the system
package (the same fallback the Windows bootstrap always uses).

The script prefers Ninja when installed and otherwise falls back to Unix
Makefiles — both are single-config, so the `build/` layout and the "no `--config`
at build/test time" rule are identical either way.

## Incremental rebuild

After the first bootstrap, a normal edit just needs an incremental rebuild of the
existing tree (no reconfigure):

```bash
cmake --build build                       # macOS / Linux / Raspberry Pi
cmake --build build --config Debug        # Windows (multi-config)
```

## Notes

- Wiping `build/` is always safe — a re-run of the bootstrap restores everything.
- The headless-only fallback (`-DSONY_MSX_ENABLE_SDL3=OFF`) drops the SDL3-gated
  tests and therefore **changes the test count** — it is a different configuration,
  not a regression. See the root [README](../README.md) "Build and test".
