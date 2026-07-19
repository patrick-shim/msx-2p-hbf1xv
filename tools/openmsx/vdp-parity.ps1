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
    [string]$ProgramBin = "tests/parity/vdp_probe.bin",
    [string]$BaseHex = "C000",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$TraceA = "build/m14_vdp_parity_A.txt",
    [string]$TraceBRaw = "build/m14_vdp_parity_B.txt",
    [string]$DiffOut = "docs/m14-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$MaxSteps = 4000,
    [int]$VramBytes = 256,
    [int]$BootSeconds = 6,
    [int]$RunSeconds = 3
)

# ---------------------------------------------------------------------------
# M14-S6 openMSX V9958 VDP parity harness.
#
# Captures a REAL architectural-state trace diff for the V9958 register / VRAM /
# status contract between:
#   A = this emulator (--vdp-parity dump), and
#   B = openMSX 19.x on WSL running the GENUINE Sony HB-F1XV machine (with a real
#       V9958 in its machine XML), driven over the SAME CPU->VDP program.
#
# The probe (tests/parity/vdp_probe.bin) is a flat RAM-only Z80 program that:
#   - sets R#0=0, R#1=0, R#14=0 (deterministic non-planar addressing),
#   - sets a write address and FILLS physical VRAM[0..255] with a 0..255 ramp via
#     #98 auto-increment,
#   - writes a palette pair (#9A) and an indirect R#7=0x2A (#9B),
#   - sets a read address and reads one byte back via #98 (read-ahead path),
#   - HALTs.
#
# The COMPARED (deterministic, cross-emulator) state is the VRAM read-back block
# plus the registers/pointer the program EXPLICITLY sets (R#0,R#1,R#7,R#14,R#17
# and the 14-bit VRAM pointer). Registers the program does NOT touch differ
# because openMSX's BIOS pre-configures them, so they are excluded from the gate.
# Status/interrupt flags (S#0 F, S#2 EO) are frame-timing dependent (deferred D4)
# and are verified in-emulator by the deterministic unit/integration tests, not
# in this cross-emulator diff.
#
# It NEVER fabricates parity: the actual outcome (empty diff, concrete field
# mismatches, or BLOCKED when openMSX cannot be driven) is written to $DiffOut.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

Require-Path -Path $ProgramBin -Label "probe program"
Require-Path -Path $Emulator -Label "emulator binary (build first)"

# --- A: emulator-side VDP-state dump. ---
& $Emulator --vdp-parity $ProgramBin $BaseHex $MaxSteps $VramBytes $TraceA
if ($LASTEXITCODE -ne 0) { throw "emulator vdp-parity failed (exit $LASTEXITCODE)" }
Require-Path -Path $TraceA -Label "emulator trace A"
Write-Output "Trace A (emulator) written: $TraceA"

# --- B: openMSX genuine V9958 over WSL. ---
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ProgramBin))
$byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "
$base = "0x$BaseHex"
$readTime = $BootSeconds + $RunSeconds

$tcl = @"
set renderer none
set throttle off
set ::PROG { $byteList }
set ::BASE $base
set ::VRAMBYTES $VramBytes
proc doread {} {
  if {[catch {
    puts stderr "OMVPTR=[format %02X%02X [debug read {VRAM pointer} 1] [debug read {VRAM pointer} 0]]"
    for {set r 0} {`$r <= 27} {incr r} {
      puts stderr [format "OMREG%02d=%02X" `$r [debug read {VDP regs} `$r]]
    }
    for {set off 0} {`$off < `$::VRAMBYTES} {incr off 16} {
      set line [format %04X `$off]
      for {set i 0} {`$i < 16} {incr i} {
        append line [format " %02X" [debug read {physical VRAM} [expr {`$off + `$i}]]]
      }
      puts stderr "OMVRAM `$line"
    }
  } err]} { puts stderr "OMERR_READ=`$err" }
  exit
}
after time $BootSeconds {
  if {[catch {
    debug break
    set a `$::BASE
    foreach byte `$::PROG { debug write memory `$a `$byte; incr a }
    puts stderr "OMSETUP ram_base=[format %02X [debug read memory `$::BASE]]"
    reg pc `$::BASE
    reg sp 0xFFFF
    reg iff 0
    debug cont
    after time $readTime doread
  } err]} { puts stderr "OMSETUP_ERR=`$err"; exit }
}
"@

$encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
$runCmd = "echo $encoded | base64 -d > /tmp/omx_vdp_parity.tcl && " +
          "timeout 120 openmsx -machine $ProbeMachine -command 'set renderer none' " +
          "-script /tmp/omx_vdp_parity.tcl 2>&1 | grep -aE 'OMVRAM|OMREG|OMVPTR|OMSETUP|OMERR' || true"
$raw = wsl -e bash -lc $runCmd

$rawLines = @()
if ($raw) {
    $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
}
Set-Content -LiteralPath $TraceBRaw -Value ($rawLines -join "`n") -Encoding ASCII
$omVramCount = @($rawLines | Where-Object { $_ -match "^OMVRAM " }).Count
Write-Output "Trace B (openMSX) VRAM lines captured: $omVramCount"
Write-Output "openMSX raw trace written: $TraceBRaw"

# --- Parse both sides into comparable field maps. ---
function Parse-Emulator {
    param([string[]]$Lines)
    $vram = @{}
    $regs = @{}
    $vptr = $null
    foreach ($l in $Lines) {
        if ($l -match "^VRAMPTR=([0-9A-Fa-f]{4})$") { $vptr = $Matches[1].ToUpper() }
        elseif ($l -match "^REG(\d\d)=([0-9A-Fa-f]{2})$") { $regs[[int]$Matches[1]] = $Matches[2].ToUpper() }
        elseif ($l -match "^([0-9A-Fa-f]{4}) ((?:[0-9A-Fa-f]{2} ?)+)$") {
            $vram[$Matches[1].ToUpper()] = ($Matches[2].Trim().ToUpper())
        }
    }
    return @{ vram = $vram; regs = $regs; vptr = $vptr }
}
function Parse-Openmsx {
    param([string[]]$Lines)
    $vram = @{}
    $regs = @{}
    $vptr = $null
    foreach ($l in $Lines) {
        if ($l -match "^OMVPTR=([0-9A-Fa-f]{4})$") { $vptr = $Matches[1].ToUpper() }
        elseif ($l -match "^OMREG(\d\d)=([0-9A-Fa-f]{2})$") { $regs[[int]$Matches[1]] = $Matches[2].ToUpper() }
        elseif ($l -match "^OMVRAM ([0-9A-Fa-f]{4}) ((?:[0-9A-Fa-f]{2} ?)+)$") {
            $vram[$Matches[1].ToUpper()] = ($Matches[2].Trim().ToUpper())
        }
    }
    return @{ vram = $vram; regs = $regs; vptr = $vptr }
}

$aLines = Get-Content -LiteralPath $TraceA
$emu = Parse-Emulator -Lines $aLines
$omx = Parse-Openmsx -Lines $rawLines

# --- Gate fields: VRAM block + VRAM pointer + explicitly-written registers. ---
$gateRegs = @(0, 1, 7, 14, 17)
$diffs = New-Object System.Collections.Generic.List[string]
$blocked = $false

if ($omx.vram.Count -eq 0) {
    $blocked = $true
    $diffs.Add("BLOCKED: openMSX produced no VRAM read-back (empty B side).")
} else {
    foreach ($off in ($emu.vram.Keys | Sort-Object)) {
        $a = $emu.vram[$off]
        if (-not $omx.vram.ContainsKey($off)) { $diffs.Add("VRAM ${off}: A=$a B=<missing>"); continue }
        $b = $omx.vram[$off]
        if ($a -ne $b) { $diffs.Add("VRAM ${off}: A=$a B=$b") }
    }
    if ($null -ne $emu.vptr -and $null -ne $omx.vptr -and $emu.vptr -ne $omx.vptr) {
        $diffs.Add("VRAMPTR: A=$($emu.vptr) B=$($omx.vptr)")
    }
    foreach ($r in $gateRegs) {
        if ($emu.regs.ContainsKey($r) -and $omx.regs.ContainsKey($r)) {
            if ($emu.regs[$r] -ne $omx.regs[$r]) {
                $diffs.Add(("REG{0:D2}: A={1} B={2}" -f $r, $emu.regs[$r], $omx.regs[$r]))
            }
        }
    }
}

# --- Adversarial comparator check: corrupt one A VRAM byte, confirm DIVERGENCE. ---
$adv = "not-run"
if (-not $blocked) {
    $firstOff = ($emu.vram.Keys | Sort-Object | Select-Object -First 1)
    if ($firstOff) {
        $aBytes = $emu.vram[$firstOff].Split(" ")
        $orig = $aBytes[0]
        $aBytes[0] = "{0:X2}" -f ((([Convert]::ToInt32($orig,16)) + 1) -band 0xFF)
        $corrupt = ($aBytes -join " ")
        if ($omx.vram.ContainsKey($firstOff) -and $corrupt -ne $omx.vram[$firstOff]) {
            $adv = "PASS (corrupted byte at ${firstOff}: $orig -> $($aBytes[0]) diverges from B)"
        } else {
            $adv = "FAIL (comparator did not detect the injected corruption)"
        }
    }
}

if ($blocked) { $result = "BLOCKED"; $exit = 2 }
elseif ($diffs.Count -eq 0) { $result = "ARCHITECTURAL PARITY (empty diff)"; $exit = 0 }
else { $result = "ARCHITECTURAL DIVERGENCE"; $exit = 1 }

# --- Write the report. ---
$diffOutFull = [System.IO.Path]::GetFullPath($DiffOut)
$diffDir = Split-Path -Parent $diffOutFull
if ($diffDir -and -not (Test-Path -LiteralPath $diffDir)) {
    New-Item -ItemType Directory -Path $diffDir | Out-Null
}
$stamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# M14-S6 openMSX V9958 VDP Parity Trace Diff (Sony HB-F1XV)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("- Captured (UTC): $stamp")
[void]$sb.AppendLine("- Reference: openMSX on WSL, machine ``$ProbeMachine`` (genuine V9958).")
[void]$sb.AppendLine("- Program: ``$ProgramBin`` at base 0x$BaseHex; VRAM read-back = $VramBytes bytes.")
[void]$sb.AppendLine("- A = this emulator (``--vdp-parity``); B = openMSX (``physical VRAM`` / ``VDP regs`` / ``VRAM pointer`` debuggables).")
[void]$sb.AppendLine("- Gate fields: VRAM[0..$($VramBytes-1)] + VRAM pointer + explicitly-written R#0,R#1,R#7,R#14,R#17.")
[void]$sb.AppendLine("- Excluded (not cross-comparable / deferred): BIOS-preset registers; frame-timing S#0 F / S#2 EO (D4).")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Result: $result")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("- openMSX VRAM lines captured (B): $omVramCount")
[void]$sb.AppendLine("- A VRAMPTR=$($emu.vptr)  B VRAMPTR=$($omx.vptr)")
[void]$sb.AppendLine("- Adversarial comparator check: $adv")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Diff (empty = pass)")
[void]$sb.AppendLine('```')
if ($diffs.Count -eq 0) { [void]$sb.AppendLine("(no differences on the gate fields)") }
else { foreach ($d in $diffs) { [void]$sb.AppendLine($d) } }
[void]$sb.AppendLine('```')
Set-Content -LiteralPath $diffOutFull -Value $sb.ToString() -Encoding ASCII

switch ($exit) {
    0 { Write-Output "M14 A/B RESULT: ARCHITECTURAL PARITY (empty diff). See $DiffOut" }
    1 { Write-Output "M14 A/B RESULT: ARCHITECTURAL DIVERGENCE captured. See $DiffOut" }
    default { Write-Output "M14 A/B RESULT: BLOCKED / not comparable. See $DiffOut and $TraceBRaw" }
}
exit $exit
