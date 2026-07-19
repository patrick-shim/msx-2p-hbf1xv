# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator
#  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
#
#  LEGAL NOTICE - Personal reference only.
#  This source code is made available solely for personal, non-commercial
#  reference and educational study. Commercial use, sale, or redistribution
#  for profit is not permitted without the author's written consent.
#  Provided "AS IS", without warranty of any kind.
#  Proprietary BIOS/ROM/disk assets remain the property of their respective
#  rights holders and are NOT licensed by this notice.
# ============================================================================

param(
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$SplitSystemTest = "build/tests/Debug/hbf1xv_m32_split_screen_system_test.exe",
    [string]$WorkDir = "build/m32-ab",
    [string]$DiffOut = "docs/m32-parity-trace-diff.md",
    [int]$BasicBootSeconds = 12,
    [int]$RunSeconds = 4
)

# ---------------------------------------------------------------------------
# M32 openMSX A/B harness -- the synthetic split-screen scenario
# (docs/m32-planner-package.md section 2.7, AC-8).
#
# Side A (ours): tests/system/hbf1xv_m32_split_screen_system_test.cpp's real
# arm, dumped via its additive `--dump-frame` argv (the ctest invocation is
# unchanged): in-RAM Z80 program, IE1 line-interrupt handler rewrites R#23
# mid-frame, frame taken at an on_vsync_boundary().
#
# Side B (openMSX on WSL, /usr/bin/openmsx, Sony_HB-F1XV, power-cycle-fresh
# session per the DEC-0028 standing rule): the SAME split protocol as a
# 512-byte Z80 program (tools/gen/split-screen-probe.py) injected into a live
# BASIC session via the debugger Tcl surface -- `debug write memory` +
# `reg pc`, the exact mechanism tools/openmsx/ab-smoke.ps1 already uses
# (R-M30-6 source-verify: openMSX's debug command set, src/debugger/
# Debugger.cc; the `reg` command, src/cpu/CPUCore). The program does its own
# page-0-to-RAM slot dance, IM1 hook install, Z80-side VRAM signature fill,
# and arms R#19=80/IE0+IE1. Capture: `screenshot -raw` AFTER re-enabling
# throttle -- an empirically REQUIRED step (with `set throttle off` openMSX
# frame-skips aggressively and `screenshot -raw` returns a STALE frame;
# discovered during this harness's bring-up and encoded here).
#
# Comparison gate (REGION-STRUCTURAL, tools/analyze/split-screen-ab.py --
# never byte-level: different palette/scaler pipelines): both sides
# independently analyzed through two palette-free structural properties of
# the signature VRAM (row r byte c = (r+c) & 0xFF): P1 exactly ONE
# shift-chain break inside [80, 85) = the split boundary; P2 the offset-128
# wrap (display row 128+k == row k for k<64). PARITY additionally requires
# the two boundaries within +/-2 lines of each other. Adversarial arms: the
# comparator self-check (corrupted copy must FLAG) and the IE1-off probe
# variant on BOTH sides (must show NO split).
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path([string]$Path, [string]$Label) {
    if (-not (Test-Path $Path)) { throw "missing ${Label}: ${Path}" }
}

Require-Path $Emulator "emulator (build first)"
Require-Path $SplitSystemTest "split system test (build first)"
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

$RepoRoot = (Get-Location).Path
$WslWork = "/mnt/" + ($RepoRoot.Substring(0, 1).ToLower()) + ($RepoRoot.Substring(2) -replace "\\", "/") + "/$WorkDir"

# --- Side A: our split frame ------------------------------------------------
Write-Host "== side A: running the split system test (dump-frame mode) =="
& $SplitSystemTest --dump-frame "$WorkDir/ours_split.frame"
if ($LASTEXITCODE -ne 0) { throw "split system test failed (exit $LASTEXITCODE)" }

# --- Probe programs ----------------------------------------------------------
Write-Host "== generating the side-B probe programs =="
python tools/gen/split-screen-probe.py -o $WorkDir
if ($LASTEXITCODE -ne 0) { throw "probe generation failed" }

# --- Side B driver (both arms) -----------------------------------------------
function Invoke-SideB([string]$ProbeBin, [string]$ShotName, [string]$LogName) {
    $tcl = @"
set throttle off
set LOG [open "$WslWork/$LogName" w]
proc L {msg} { global LOG; puts `$LOG `$msg; flush `$LOG }
proc poke_program {path org} {
    set f [open `$path rb]
    fconfigure `$f -translation binary
    set data [read `$f]
    close `$f
    set n [string length `$data]
    for {set i 0} {`$i < `$n} {incr i} {
        binary scan [string index `$data `$i] cu byte
        debug write memory [expr {`$org + `$i}] `$byte
    }
    return `$n
}
after time $BasicBootSeconds {
    poke_program "$WslWork/$ProbeBin" 0xC000
    reg sp 0xF380
    reg pc 0xC000
    after time $RunSeconds {
        set throttle on
        after time 1 {
            if {[catch {
                L "counters fh=[debug read memory 0xC300] vb=[debug read memory 0xC301]"
                L "vdpregs r0=[debug read {VDP regs} 0] r19=[debug read {VDP regs} 19] r23=[debug read {VDP regs} 23]"
                screenshot -raw "$WslWork/$ShotName"
                L "screenshot saved"
            } err]} { L "ERROR: `$err" }
            exit
        }
    }
}
"@
    Set-Content -Path "$WorkDir/side_b_run.tcl" -Value $tcl -Encoding ascii
    Write-Host "== side B ($ProbeBin): openMSX Sony_HB-F1XV via WSL =="
    # cmd /c with 2>nul: WSL/ALSA writes benign noise to stderr, which
    # PowerShell's Stop preference would otherwise turn into a terminating
    # NativeCommandError.
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    cmd /c "wsl.exe -- openmsx -machine Sony_HB-F1XV -script `"$WslWork/side_b_run.tcl`" 2>nul" | Out-Null
    $ErrorActionPreference = $prevEap
    Get-Content "$WorkDir/$LogName" | ForEach-Object { Write-Host "  [side B] $_" }
}

Invoke-SideB "m32_split_basic.bin" "openmsx_split_raw.png" "side_b_split.log"
Invoke-SideB "m32_split_basic_noie1.bin" "openmsx_noie1_raw.png" "side_b_noie1.log"

# --- Comparison gates ---------------------------------------------------------
Write-Host "== comparator self-check (adversarial) =="
python tools/analyze/split-screen-ab.py --ours "$WorkDir/ours_split.frame" --self-check
if ($LASTEXITCODE -ne 0) { throw "comparator self-check FAILED to flag a corrupted region" }

Write-Host "== structural A/B: split arm =="
python tools/analyze/split-screen-ab.py --ours "$WorkDir/ours_split.frame" --openmsx "$WorkDir/openmsx_split_raw.png" |
    Tee-Object -Variable SplitResult
$SplitOk = ($LASTEXITCODE -eq 0)

Write-Host "== structural check: side-B IE1-off arm shows NO split =="
$NoIe1Log = Get-Content "$WorkDir/side_b_noie1.log" -Raw
$NoIe1FhZero = $NoIe1Log -match "fh=0 "
python tools/analyze/split-screen-ab.py --openmsx "$WorkDir/openmsx_noie1_raw.png" --expect-no-split
$NoIe1Ok = ($LASTEXITCODE -eq 0)

$Disposition = if ($SplitOk -and $NoIe1Ok -and $NoIe1FhZero) { "PARITY" } else { "DIVERGENCE" }
Write-Host "== disposition: $Disposition =="
Write-Host "(record the run's outputs in $DiffOut per the package section 2.7)"
if ($Disposition -ne "PARITY") { exit 1 }
