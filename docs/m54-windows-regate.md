# M54 Windows Re-gate — owner-executed runbook (DEC-0081)

Discharges the only outstanding M54 conditions: **AC-W1 / AC-W2 / AC-W3** (the §7.2 HARD regression
bar) and **AC-M2 part 3** (Mac-vs-Windows skip delta == 0).

Authored by the MSX QA Agent (macOS session, 2026-07-15); persisted by the coordinator.

> **Sequence is load-bearing.** AC-W3.2's real test is *"does a Windows `git pull` churn the
> checkout?"* — which requires the commit to exist. Mac commit + push **first**, then Windows pull +
> gate. Running the Windows steps against the old HEAD proves nothing.

## Step A — on the Mac (creates the commit AC-W3.2 tests)

```bash
cd ~/sony-msx-hbf1xv
git add .gitattributes CMakeLists.txt README.md src/frontend/sdl3_app.cpp src/frontend/sdl3_main.cpp
git commit -m "feat(build): macOS support - AppleClang/Ninja, .gitattributes binary pins (DEC-0081/M54)"
git diff --stat HEAD~1 HEAD   # AC-W3.2: MUST be small+semantic, NEVER whole-file
git push
```

Expected diff shape — **a ~206-line whole-file `CMakeLists.txt` diff is an AC-W3 FAILURE even if all
253 tests pass** (it means CRLF was reintroduced and will churn the Windows checkout):

| File | Expected |
|---|---|
| `CMakeLists.txt` | 16 / −5 |
| `README.md` | 75 / −9 |
| `src/frontend/sdl3_main.cpp` | 16 / 16 |
| `src/frontend/sdl3_app.cpp` | 5 / 3 |

## Step B — on Windows (PowerShell, repo root)

```powershell
cd D:\Projects\sony-msx-hbf1xv

# 0. BASELINE BEFORE PULL (AC-W3.1 needs a before/after)
git status --porcelain                 # expect clean
git rev-parse HEAD                     # expect b9314fe

# 1. Pull M54
git pull

# 2. AC-W3.1 - no mass renormalization / zero churn
git status --porcelain                 # MUST be empty: no spurious ' M' on untouched files
git diff --stat HEAD                   # MUST be empty
git ls-files --eol CMakeLists.txt README.md sony_msx_hbf1xv.xml
#    -> expect i/lf  (w/crlf is fine and expected with autocrlf=true)

# 3. AC-W3.2 - blob EOL stability + diff shape
git diff --stat HEAD~1 HEAD            # same 4-file shape as Step A
git ls-files -v roms/fmpac.rom.sram    # expect 'S' (skip-worktree, DEC-0047-AMENDMENT-C)

# 4. AC-W1 - clean rebuild, ONE build tree (VS multi-config -> -C Debug)
Remove-Item -Recurse -Force build
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -LE m24_slow_full_sweep
#    -> MUST be 253/253, 0 failed

# 5. AC-M2 PART 3 - the LITERAL Windows skip list (this is what closes the delta)
ctest --test-dir build -C Debug -V -LE m24_slow_full_sweep 2>&1 |
  Tee-Object -FilePath debug\m54\win-ctest-verbose.txt |
  Select-String -Pattern '^\d+: .*SKIP'

# 6. AC-W2 / AC-M3 - golden byte-identity on Windows
.\build\Debug\msx-disk.exe --create $env:TEMP\w.dsk
Get-FileHash $env:TEMP\w.dsk -Algorithm SHA256
#    -> MUST be 6F1A79835E0421178B01207B196FA245C127C976FA0C6ABC3AAA57E6B0CE5188

# 7. Asset gate - re-confirm roms/ Files: 1 unchanged
pwsh -ExecutionPolicy Bypass -File tools\validate-assets.ps1
```

> **Note the `^\d+:` anchor in Step 5.** ctest prefixes each line with `NNN: `. Anchoring on `^SKIP`
> is exactly the bug that silently emptied two of the Mac-side evidence artifacts — it returns
> nothing and reads as "no skips".

## Step 5 pass criteria — the exact expected skip-set

Must print **exactly these 8 tests / 9 SKIP lines**:

```
machine_hbf1xv_m19_aleste_smoke_integration_test
machine_hbf1xv_m19_metalgear_smoke_integration_test
hbf1xv_m27_replay_determinism_system_test
machine_hbf1xv_m30_cartridge_identification_integration_test
machine_hbf1xv_m31_fm_title_probe_integration_test          (partial)
machine_hbf1xv_m31_metalgear2_scc_integration_test
hbf1xv_m34_aleste_ultrasonic_regression_system_test
frontend_sdl3_cli_session_integration_test                  (partial: Case 1 AND Case 3 - 2 lines)
```

`hbf1xv_m28_c5_disk_boot_investigation_system_test` and `diskutils_bpb_matches_machine_unit_test`
**must NOT appear** — they must RUN.

**Any extra skipper is a FINDING, not a pass.** If a `games/`-gated test (1, 2, 3, 10) *runs* on
Windows, then the Mac-vs-Windows delta is **NOT zero** and AC-M2 p3 **FAILS** — report it rather than
accepting the 253 count, because the count reads 253 either way. That is the whole reason this step
exists.

## On success

All four conditions discharged → QA upgrades **CONDITIONAL PASS → PASS** → NORMAL human-release gate
(blast radius LOW-MED, build-system) → tag/README sync at the owner's direction.
