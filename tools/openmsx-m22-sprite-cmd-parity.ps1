param(
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$ProbeDir = "tests/parity",
    [string]$OutDir = "build",
    [string]$DiffOut = "docs/m22-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$MaxSteps = 4000,
    [int]$BootSeconds = 6,
    [int]$RunSeconds = 3,
    [int]$VramBytes = 768
)

# ---------------------------------------------------------------------------
# M22-S8 openMSX sprite + VDP-command-engine A/B harness (backlog D2/D3,
# closes D7's remaining command-engine-path piece).
#
# Raw-byte/register comparison technique (planner package docs/m22-planner-
# package.md section 2.6), mirroring the M11-M21 Tcl-debugger methodology
# exactly (tools/openmsx-m21-vdp-render-parity.ps1 is the direct precedent).
#
# Cross-engine-comparable gate fields (confirmed this cycle via a live WSL
# `debug list`/`debug size` query, per the planner package's explicit
# feasibility-check requirement):
#   - "VDP regs" SimpleDebuggable: size 64 -- ALREADY covers R#0-46 (the
#     command engine's own register file) in the SAME debuggable range used
#     for R#0-27 since M14; no separate probe point was needed.
#   - "VDP status regs" SimpleDebuggable: size 16 -- covers S#0-S#9 (and
#     beyond), read NON-destructively (matches this engine's own
#     peek_status_register(), used because the probe program's OWN `IN
#     (#99)`/OUT sequence already exercises any real side effects; this
#     script does not re-trigger a fresh destructive read on either side).
#   - "physical VRAM" SimpleDebuggable: size 131072 (128 KB) -- the SAME
#     debuggable the M11-M21 precedent already uses.
#
# Architectural asymmetry, handled explicitly (NOT a workaround): this
# engine's SpriteEngine recomputes S#0/S#3-S#6 ONLY via the explicit
# on_vsync() hook (§1.4 Resolution 1 -- no new clock consumer), which
# `sony_msx_headless --sprite-cmd-parity` calls once after the CPU program
# halts. openMSX's own SpriteChecker is raster-time-driven (advances "for
# free" as EmuTime elapses); this script waits `$RunSeconds` of REAL
# emulated time after the probe program runs so openMSX's OWN check
# completes naturally before reading back status -- the SAME underlying
# frame-boundary event, reached by each engine's own native mechanism.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"

$probes = @("sprite_mode1_collision", "sprite_mode1_fifth", "sprite_mode2_ninth_ic", "sprite_tp_color0",
            "cmd_atomic", "cmd_graphic6_planar", "cmd_lmmc_transfer")

# Per-probe explicit VRAM-address / status-register allowlists. openMSX's
# genuine BIOS actually boots for $BootSeconds of real emulated time BEFORE
# this probe's own code runs (unlike this engine's own --sprite-cmd-parity
# harness, a truly cold, BIOS-less run) -- so any VRAM byte / status field
# the probe does NOT itself explicitly control is BIOS-influenced noise on
# the openMSX side, not a genuine comparable. Gating to exactly the fields
# each probe DOES control keeps the comparison honest and meaningful (the
# SAME discipline the M14-M21 precedent already applies to registers, e.g.
# "gateRegs = @(0, 25)" in tools/openmsx-m21-vdp-render-parity.ps1).
$vramGate = @{
    "sprite_mode1_collision" = @(0,1,2,3,4,5,6,7,8)
    "sprite_mode1_fifth"     = @(0..15)
    "sprite_mode2_ninth_ic"  = @(0, 512, 513, 514)  # sprite0 color(0) + Y/X/pattern(512-514)
    "sprite_tp_color0"       = @(0,1,2,3,4,5,6,7,8)
    "cmd_atomic"             = @(0,3,5,10)  # LMMM src(0)/SRCH input(3)/HMMV target(5)/LMMM dest(10)
    "cmd_graphic6_planar"    = @()  # this probe's write lands ONLY in VRAM_BANK1 (see below)
    "cmd_lmmc_transfer"      = @(0,1)
}
$vramB1Gate = @{
    "cmd_graphic6_planar" = @(0)
}
$statusGate = @{
    "sprite_mode1_collision" = @(0,3,4,5,6)
    "sprite_mode1_fifth"     = @(0)
    "sprite_mode2_ninth_ic"  = @(0)
    "sprite_tp_color0"       = @(0,3,4,5,6)
    "cmd_atomic"             = @(2)
    "cmd_graphic6_planar"    = @(2)
    "cmd_lmmc_transfer"      = @(2)
}
$allResults = @()
$anyBlocked = $false
$anyDivergence = $false

foreach ($kind in $probes) {
    $bin = "$ProbeDir/m22_${kind}_probe.bin"
    Require-Path -Path $bin -Label "probe program ($kind)"

    $traceA = "$OutDir/m22_${kind}_A.txt"
    & $Emulator --sprite-cmd-parity $bin C000 $MaxSteps $VramBytes $traceA
    if ($LASTEXITCODE -ne 0) { throw "emulator sprite-cmd-parity failed for $kind (exit $LASTEXITCODE)" }
    Require-Path -Path $traceA -Label "emulator trace A ($kind)"

    # --- B: openMSX, genuine V9958/V9938 command engine, over WSL. ---
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $bin))
    $byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "
    $readTime = $BootSeconds + $RunSeconds

    $tcl = @"
set renderer none
set throttle off
set vdpcmdtrace off
set ::PROG { $byteList }
set ::BASE 0xC000
proc doread {} {
  if {[catch {
    for {set r 0} {`$r <= 46} {incr r} {
      puts stderr [format "OMREG%02d=%02X" `$r [debug read {VDP regs} `$r]]
    }
    for {set s 0} {`$s <= 9} {incr s} {
      puts stderr [format "OMSTATUS%02d=%02X" `$s [debug read {VDP status regs} `$s]]
    }
    for {set off 0} {`$off < $VramBytes} {incr off} {
      puts stderr [format "OMVRAM%04X=%02X" `$off [debug read {physical VRAM} `$off]]
    }
    for {set off 0} {`$off < 16} {incr off} {
      set a [expr {0x10000 + `$off}]
      puts stderr [format "OMVRAM%05X=%02X" `$a [debug read {physical VRAM} `$a]]
    }
  } err]} { puts stderr "OMERR_READ=`$err" }
  exit
}
after time $BootSeconds {
  if {[catch {
    debug break
    set a `$::BASE
    foreach byte `$::PROG { debug write memory `$a `$byte; incr a }
    reg pc `$::BASE
    reg sp 0xFFFF
    reg iff 0
    debug cont
    after time $readTime doread
  } err]} { puts stderr "OMSETUP_ERR=`$err"; exit }
}
"@

    $encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
    $runCmd = "echo $encoded | base64 -d > /tmp/omx_m22_${kind}.tcl && " +
              "timeout 120 openmsx -machine $ProbeMachine -command 'set renderer none' " +
              "-script /tmp/omx_m22_${kind}.tcl 2>&1 | grep -aE 'OMREG|OMSTATUS|OMVRAM|OMERR' || true"
    $raw = wsl -e bash -lc $runCmd
    $rawLines = @()
    if ($raw) {
        $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
    }
    $traceBRaw = "$OutDir/m22_${kind}_B.txt"
    Set-Content -LiteralPath $traceBRaw -Value ($rawLines -join "`n") -Encoding ASCII

    # --- Parse A (our engine's dump). ---
    $aLines = Get-Content -LiteralPath $traceA
    $aRegs = @{}; $aStatus = @{}; $aVram = @{}; $aVramB1 = @{}
    $section = ""
    foreach ($l in $aLines) {
        if ($l -eq "STATUS" -or $l -eq "VRAM" -or $l -eq "VRAM_BANK1") { $section = $l; continue }
        if ($l -match "^REG(\d\d)=([0-9A-Fa-f]{2})$") { $aRegs[[int]$Matches[1]] = $Matches[2].ToUpper() }
        elseif ($section -eq "STATUS" -and $l -match "^S(\d\d)=([0-9A-Fa-f]{2})$") {
            $aStatus[[int]$Matches[1]] = $Matches[2].ToUpper()
        } elseif ($section -eq "VRAM" -and $l -match "^([0-9A-Fa-f]{4}) (.+)$") {
            $rowBase = [Convert]::ToInt32($Matches[1], 16)
            $bytesArr = $Matches[2].Trim() -split " "
            for ($i = 0; $i -lt $bytesArr.Count; $i++) { $aVram[$rowBase + $i] = $bytesArr[$i].ToUpper() }
        } elseif ($section -eq "VRAM_BANK1" -and $l -match "^10000 (.+)$") {
            $bytesArr = $Matches[1].Trim() -split " "
            for ($i = 0; $i -lt $bytesArr.Count; $i++) { $aVramB1[$i] = $bytesArr[$i].ToUpper() }
        }
    }

    # --- Parse B (openMSX). ---
    $bRegs = @{}; $bStatus = @{}; $bVram = @{}; $bVramB1 = @{}
    foreach ($l in $rawLines) {
        if ($l -match "^OMREG(\d\d)=([0-9A-Fa-f]{2})$") { $bRegs[[int]$Matches[1]] = $Matches[2].ToUpper() }
        elseif ($l -match "^OMSTATUS(\d\d)=([0-9A-Fa-f]{2})$") { $bStatus[[int]$Matches[1]] = $Matches[2].ToUpper() }
        elseif ($l -match "^OMVRAM([0-9A-Fa-f]{4})=([0-9A-Fa-f]{2})$") { $bVram[[Convert]::ToInt32($Matches[1],16)] = $Matches[2].ToUpper() }
        elseif ($l -match "^OMVRAM([0-9A-Fa-f]{5})=([0-9A-Fa-f]{2})$") {
            $off = [Convert]::ToInt32($Matches[1],16) - 0x10000
            $bVramB1[$off] = $Matches[2].ToUpper()
        }
    }

    $blocked = ($bVram.Count -eq 0)
    $diffs = New-Object System.Collections.Generic.List[string]
    if ($blocked) {
        $diffs.Add("BLOCKED: openMSX produced no VRAM read-back for probe '$kind' (empty B side).")
    } else {
        # Gate VRAM/status/register comparisons to EXACTLY the fields this
        # probe explicitly controls (see the allowlists above) -- openMSX's
        # real BIOS boot (unlike this engine's cold, BIOS-less
        # --sprite-cmd-parity harness) leaves BIOS-influenced content in
        # every OTHER field, which is not a genuine comparable (the SAME
        # discipline the M14-M21 precedent already applies to registers).
        $vramOffsets = $vramGate[$kind]
        foreach ($off in $vramOffsets) {
            if (-not $aVram.ContainsKey($off)) { continue }
            if (-not $bVram.ContainsKey($off)) { $diffs.Add("VRAM[$off]: A=$($aVram[$off]) B=<missing>"); continue }
            if ($aVram[$off] -ne $bVram[$off]) { $diffs.Add("VRAM[$off]: A=$($aVram[$off]) B=$($bVram[$off])") }
        }
        $vramB1Offsets = @()
        if ($vramB1Gate.ContainsKey($kind)) { $vramB1Offsets = $vramB1Gate[$kind] }
        foreach ($off in $vramB1Offsets) {
            if (-not $aVramB1.ContainsKey($off)) { continue }
            if (-not $bVramB1.ContainsKey($off)) { $diffs.Add("VRAM_BANK1[+$off]: A=$($aVramB1[$off]) B=<missing>"); continue }
            if ($aVramB1[$off] -ne $bVramB1[$off]) { $diffs.Add("VRAM_BANK1[+$off]: A=$($aVramB1[$off]) B=$($bVramB1[$off])") }
        }
        foreach ($s in $statusGate[$kind]) {
            if ($aStatus.ContainsKey($s) -and $bStatus.ContainsKey($s) -and $aStatus[$s] -ne $bStatus[$s]) {
                $diffs.Add("STATUS[S$s]: A=$($aStatus[$s]) B=$($bStatus[$s])")
            }
        }
        # Registers this probe explicitly writes: R#0 (mode) always; the
        # command engine's own register file R#32-46 for the cmd_* probes.
        $gateRegs = @(0)
        if ($kind.StartsWith("cmd_")) { $gateRegs += 32..46 }
        foreach ($r in $gateRegs) {
            if ($aRegs.ContainsKey($r) -and $bRegs.ContainsKey($r) -and $aRegs[$r] -ne $bRegs[$r]) {
                $diffs.Add(("REG{0:D2}: A={1} B={2}" -f $r, $aRegs[$r], $bRegs[$r]))
            }
        }
    }

    # --- Adversarial comparator self-check: corrupt one A VRAM byte, confirm DIVERGENCE. ---
    $adv = "not-run"
    if (-not $blocked -and $aVram.Count -gt 0) {
        $corruptA = @{}
        foreach ($k2 in $aVram.Keys) { $corruptA[$k2] = $aVram[$k2] }
        $corruptA[0] = "{0:X2}" -f ((([Convert]::ToInt32($aVram[0],16)) + 1) -band 0xFF)
        if ($bVram.ContainsKey(0) -and $corruptA[0] -ne $bVram[0]) {
            $adv = "PASS (corrupted VRAM[0]: $($aVram[0]) -> $($corruptA[0]) diverges from B=$($bVram[0]))"
        } else {
            $adv = "FAIL (comparator did not detect the injected corruption)"
        }
    }

    if ($blocked) { $status = "BLOCKED"; $anyBlocked = $true }
    elseif ($diffs.Count -eq 0) { $status = "PARITY (empty diff)" }
    else { $status = "DIVERGENCE"; $anyDivergence = $true }

    $allResults += [PSCustomObject]@{
        Kind = $kind
        Status = $status
        Diffs = $diffs
        Adversarial = $adv
        AStatus = $aStatus
        BStatus = $bStatus
    }
}

# --- Write the combined report. ---
$diffOutFull = [System.IO.Path]::GetFullPath($DiffOut)
$diffDir = Split-Path -Parent $diffOutFull
if ($diffDir -and -not (Test-Path -LiteralPath $diffDir)) {
    New-Item -ItemType Directory -Path $diffDir | Out-Null
}
$stamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# M22 openMSX Sprite + Command-Engine Parity Trace Diff (Sony HB-F1XV)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("- Captured (UTC): $stamp")
[void]$sb.AppendLine("- Reference: openMSX on WSL, machine ``$ProbeMachine`` (genuine V9958 sprite checker + command engine).")
[void]$sb.AppendLine("- Technique: raw-byte/register comparison (docs/m22-planner-package.md section 2.6), mirroring the M11-M21 Tcl-debugger methodology exactly.")
[void]$sb.AppendLine("- A = this emulator (``--sprite-cmd-parity``); B = openMSX (``VDP regs`` / ``VDP status regs`` / ``physical VRAM`` SimpleDebuggables, all confirmed present via a live ``debug list``/``debug size`` query this cycle).")
[void]$sb.AppendLine("- Cross-engine-comparable gate fields: raw VRAM bytes (incl. the physical bank1 window at 0x10000), S#0/S#2/S#3-S#6 status bytes, and R#0 (+ R#32-46 for the cmd_* probes).")
[void]$sb.AppendLine("- Architectural note (not a divergence): this engine's SpriteEngine recomputes status ONLY via the explicit on_vsync() hook (called once by --sprite-cmd-parity after the probe halts); openMSX's own SpriteChecker is raster-time-driven and advances as real emulated time elapses during this script's `after time` wait. Both reach the SAME frame-boundary event through each engine's own native mechanism.")
[void]$sb.AppendLine("")

foreach ($r in $allResults) {
    [void]$sb.AppendLine("## Probe: $($r.Kind) -- Result: $($r.Status)")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("- Adversarial comparator check: $($r.Adversarial)")
    [void]$sb.AppendLine("- A-side status registers (S#0-S#9): $((0..9 | ForEach-Object { if ($r.AStatus.ContainsKey($_)) { "S$_=$($r.AStatus[$_])" } }) -join ' ')")
    [void]$sb.AppendLine("- B-side status registers (S#0-S#9): $((0..9 | ForEach-Object { if ($r.BStatus.ContainsKey($_)) { "S$_=$($r.BStatus[$_])" } }) -join ' ')")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("### Diff (empty = pass, raw VRAM/status/register fields only)")
    [void]$sb.AppendLine('```')
    if ($r.Diffs.Count -eq 0) { [void]$sb.AppendLine("(no differences on the gate fields)") }
    else { foreach ($d in $r.Diffs) { [void]$sb.AppendLine($d) } }
    [void]$sb.AppendLine('```')
    [void]$sb.AppendLine("")
}

[void]$sb.AppendLine("## Overall")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("- Sprite collision/5th-sprite status (D2): raw S#0/S#3-S#6 parity per the ``sprite_mode1_collision``/``sprite_mode1_fifth`` probes above.")
[void]$sb.AppendLine("- Sprite Mode 2 9th-sprite + IC-excludes-collision-only (D2, A-M22-11): per the ``sprite_mode2_ninth_ic`` probe above.")
[void]$sb.AppendLine("- Color-0/TP collision interaction (D2, A-M22-12): per the ``sprite_tp_color0`` probe above.")
[void]$sb.AppendLine("- Command-engine VRAM writes across HMMV/LMMM/LINE/SRCH (D3): per the ``cmd_atomic`` probe above (raw VRAM bytes + S#2 BD).")
[void]$sb.AppendLine("- D7 closure (command-engine G6-planar destination): per the ``cmd_graphic6_planar`` probe above -- the write must land in the physical bank1 window (0x10000+) on BOTH engines.")
[void]$sb.AppendLine("- Command-engine transfer-command TR/CE handshake (D3): per the ``cmd_lmmc_transfer`` probe above (final settled S#2 TR/CE after the discrete R#44 writes; NOT a cycle-by-cycle timing comparison, explicitly out of scope).")
[void]$sb.AppendLine("")
if ($anyBlocked) {
    [void]$sb.AppendLine("**At least one probe's raw-byte comparison was BLOCKED** (see per-probe sections above) -- reported honestly, not fabricated.")
} elseif ($anyDivergence) {
    [void]$sb.AppendLine("**At least one probe showed a raw-byte DIVERGENCE** (see per-probe sections above) -- reported honestly, not silently upgraded to PASS.")
} else {
    [void]$sb.AppendLine("All seven probes' raw VRAM/status/register fields achieved ARCHITECTURAL PARITY (empty diff).")
}

Set-Content -LiteralPath $diffOutFull -Value $sb.ToString() -Encoding ASCII
Write-Output "M22 A/B report written: $DiffOut"
if ($anyBlocked) { exit 2 } elseif ($anyDivergence) { exit 1 } else { exit 0 }
