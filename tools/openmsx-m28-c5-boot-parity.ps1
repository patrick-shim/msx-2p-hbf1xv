param(
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$BiosDir = "bios",
    [string]$Machine = "Sony_HB-F1XV",
    [string]$WorkDir = "build",
    [string]$DiffOut = "docs/m28-parity-trace-diff.md",
    [int]$BootTraceSteps = 3000,
    [string]$Disk = "disks/msxdos22.dsk",
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M28-S2 openMSX A/B parity harness (C5 full-boot / auto-disk-boot-trigger
# investigation, docs/m28-planner-package.md Section 2.1a).
#
# Extends the M16 precedent (tools/openmsx-m16-boot-parity.ps1 Subject 1,
# docs/m16-parity-trace-diff.md) with ONE new variable: a REAL, bootable
# MSX-DOS system disk (disks/msxdos22.dsk by default) mounted on BOTH sides
# from the very first instruction of a real, authentic cold reset -- M16's
# own Subject 1 only had the FDC's default synthesized (non-bootable) medium.
#
# Both sides: authentic reset (#A8=0, PC=0x0000, all registers zero, SP=
# 0xFFFF, IM1), single-stepped for $BootTraceSteps instructions, emitting the
# full architectural trace. This emulator uses --debug-session --disk
# --trace-cpu (M27 tooling); openMSX is driven via a live Tcl script
# (-diska + reset + a chained break/step loop), mirroring the M11-M27
# established live-Tcl PC/register readback technique.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $BiosDir -Label "bios directory"
Require-Path -Path $Disk -Label "disk image"

if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir | Out-Null }

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) { throw "openMSX not found in WSL PATH" }
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion), machine: $Machine, disk: $Disk"

# =====================================================================
# Subject: real BIOS-boot checkpoint WITH a real MSX-DOS disk mounted
# =====================================================================
$bootA = Join-Path $WorkDir "m28_boot_A.txt"
$bootBRaw = Join-Path $WorkDir "m28_boot_B.txt"
$bootDiff = Join-Path $WorkDir "m28_boot_diff.md"
$debugRoot = Join-Path $WorkDir "m28_debug"

if (Test-Path -LiteralPath $debugRoot) { Remove-Item -Recurse -Force $debugRoot }
& $Emulator --debug-session $BiosDir $BootTraceSteps --disk $Disk --trace-cpu "m28_boot_a.txt" --debug-root $debugRoot
if ($LASTEXITCODE -ne 0) { throw "emulator debug-session failed (exit $LASTEXITCODE)" }
Copy-Item -LiteralPath (Join-Path $debugRoot "traces/m28_boot_a.txt") -Destination $bootA -Force
$bootALineCount = (Get-Content -LiteralPath $bootA).Count
Write-Output "Subject: boot trace A (emulator, real disk mounted) instructions: $bootALineCount"

$tcl1 = @"
set ::MAX $BootTraceSteps
set ::seq 0
proc emit {} {
  if {[catch {
    set pc [reg pc]
    set b0 [debug read memory `$pc]
    set oplen 1
    if {`$b0 == 0xCB || `$b0 == 0xED || `$b0 == 0xDD || `$b0 == 0xFD} { set oplen 2 }
    set op ""
    for {set k 0} {`$k < `$oplen} {incr k} {
      append op [format %02X [debug read memory [expr {(`$pc + `$k) & 0xFFFF}]]]
    }
    puts stderr [format "OMTRACE seq=%d pc=%04X op=%s af=%04X bc=%04X de=%04X hl=%04X af2=%04X bc2=%04X de2=%04X hl2=%04X ix=%04X iy=%04X sp=%04X i=%02X r=%02X im=%d iff=%d" \
      `$::seq [reg pc] `$op [reg af] [reg bc] [reg de] [reg hl] [reg af2] [reg bc2] [reg de2] [reg hl2] [reg ix] [reg iy] [reg sp] [reg i] [reg r] [reg im] [reg iff]]
    incr ::seq
    if {`$::seq >= `$::MAX} { exit }
    after break emit
    step
  } err]} { puts stderr "OMTRACE ERROR seq=`$::seq msg=`$err"; exit }
}
after time $BootSeconds {
  if {[catch {
    reset
    reg af 0; reg bc 0; reg de 0; reg hl 0
    reg af2 0; reg bc2 0; reg de2 0; reg hl2 0
    reg ix 0; reg iy 0; reg sp 0xFFFF; reg pc 0x0000
    reg i 0; reg r 0; reg im 1; reg iff 0
    after break emit
    debug break
  } err]} { puts stderr "OMSETUP ERROR msg=`$err"; exit }
}
"@
$encoded1 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl1))

# Convert the Windows disk path to its WSL-visible equivalent for -diska.
$diskFullPath = (Resolve-Path -LiteralPath $Disk).Path
$diskDrive = $diskFullPath.Substring(0,1).ToLower()
$diskRest = $diskFullPath.Substring(2) -replace '\\','/'
$diskWslPath = "/mnt/$diskDrive$diskRest"

$runCmd1 = "cp '$diskWslPath' /tmp/m28_boot.dsk && " +
           "echo $encoded1 | base64 -d > /tmp/omx_m28_boot.tcl && " +
           "timeout 400 openmsx -machine $Machine -diska /tmp/m28_boot.dsk " +
           "-command 'set renderer none' -script /tmp/omx_m28_boot.tcl 2>&1 | grep -a -E 'OMTRACE|OMSETUP' || true"
$raw1 = wsl -e bash -lc $runCmd1
$rawLines1 = @()
if ($raw1) { $rawLines1 = @($raw1 -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) }
Set-Content -LiteralPath $bootBRaw -Value ($rawLines1 -join "`n") -Encoding ASCII
$bootBCount = @($rawLines1 | Where-Object { $_ -match "OMTRACE seq=" }).Count
Write-Output "Subject: boot trace B (openMSX, real disk mounted) instructions captured: $bootBCount"

python tools/trace-diff.py $bootA $bootBRaw $bootDiff
$bootExit = $LASTEXITCODE
switch ($bootExit) {
    0 { Write-Output "Subject RESULT: ARCHITECTURAL PARITY over $BootTraceSteps instructions (real disk mounted)" }
    1 { Write-Output "Subject RESULT: DIVERGENCE captured (see $bootDiff) -- first divergence point recorded" }
    default { Write-Output "Subject RESULT: BLOCKED / not comparable" }
}

Write-Output ""
Write-Output "Artifacts: $bootA $bootBRaw $bootDiff"
Write-Output "Compose the final artifact into $DiffOut."
exit 0
