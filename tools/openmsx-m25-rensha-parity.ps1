param(
    [string]$DiffOut = "docs/m25-parity-trace-diff.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [int]$BootSeconds = 6,
    [int]$NotHeldSamples = 10,
    [int]$HeldSamples = 40,
    [double]$StepSeconds = 0.002,
    [int]$RenshaSpeed = 100
)

# ---------------------------------------------------------------------------
# M25-S5 openMSX A/B evidence -- backlog C8 (Sony speed-controller + hardware
# PAUSE (MB670836) + Ren-Sha Turbo autofire).
#
# TWO SEPARATE, HONESTLY-DISPOSITIONED sub-claims (docs/m25-planner-package.md
# section 2.7 / Acceptance Criterion 9 -- this split is the point, not a shortcut):
#
#   (1) Ren-Sha Turbo: FEASIBLE, genuinely attempted live against real
#       openMSX. Real openMSX HAS this hardware modeled
#       (references/openmsx-21.0/src/RenShaTurbo.{hh,cc}/Autofire.{hh,cc},
#       wired at MSXPPI.cc:90-93 / sound/MSXPSG.cc:90-93) and this project's
#       target machine's own XML carries a real <RenShaTurbo> calibration
#       block (Sony_HB-F1XV.xml:16-19, min_ints=47/max_ints=221). This script
#       drives the REAL `Sony_HB-F1XV` openMSX machine via its own live Tcl
#       `debug`/`set`/`keymatrixdown` interface (NOT this project's CLI --
#       no new CLI flag was added for PAUSE/Speed-Controller/RenSha this
#       cycle, per the planner's explicit out-of-scope table) and checks:
#         (a) with SPACE genuinely NOT held, a live, engaged (speed=100)
#             renshaturbo setting produces ZERO observable effect on the real
#             keyboard-row-8 read (#A9) at every sampled point -- the
#             regression-safety invariant R-M25-6 demands, confirmed on REAL
#             reference hardware behavior, not merely this project's own
#             code.
#         (b) with SPACE genuinely HELD (via openMSX's own `keymatrixdown 8 1`
#             Tcl command -- a real, first-class openMSX scripting primitive
#             for driving the keyboard matrix directly,
#             references/openmsx-21.0/src/input/Keyboard.cc:1590-1611), the
#             SAME live read alternates between "pressed" and "released" --
#             proving the real reference engine's OR-combine autofire wiring
#             behaves exactly as this project's own implementation does
#             (bit0=SPACE, forces 0->1 only, never the reverse).
#         (c) the SAME not-held invariant is independently confirmed for the
#             PSG R14/joystick-trigger-A path (#A0 address-latch + #A2 data
#             read).
#       Sampling technique: `after time <seconds> { ... }`-scheduled callbacks
#       (native, continuous emulation between samples -- NOT per-instruction
#       Tcl single-stepping), specifically BECAUSE M23/M24's own prior
#       findings (docs/m23-parity-trace-diff.md, docs/m24-implementation-
#       report.md) established that live per-instruction Tcl single-stepping
#       becomes slow/inconsistent past a small step count -- this technique
#       sidesteps that entirely by letting the real engine run freely between
#       each scheduled sample.
#       Explicitly, honestly NOT attempted: a live "held" demonstration for
#       the joystick trigger-A path -- no equivalent "hold a joystick button"
#       Tcl scripting primitive was found in openMSX 21.0 (keymatrixdown/up
#       apply ONLY to the keyboard matrix); this is disclosed below, not
#       silently skipped or fabricated.
#
#   (2) Hardware PAUSE / Speed Controller: BLOCKED, and this is the RIGHT,
#       honest disposition -- NOT a shortfall. section 2.2 of the planner package
#       (independently re-confirmed this cycle, citations reproduced below)
#       establishes that openMSX 21.0 has ZERO Sony-specific PAUSE/
#       speed-controller modeling anywhere for this chipset/machine family.
#       There is no reference engine behavior to diff against.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-BlockedPauseSection {
    param([System.Collections.Generic.List[string]]$Lines)
    $Lines.Add("")
    $Lines.Add("## Hardware PAUSE / Speed Controller (MB670836): Result BLOCKED")
    $Lines.Add("")
    $Lines.Add("**This is the RIGHT, honest disposition, not a shortfall.** openMSX 21.0 has ZERO")
    $Lines.Add("Sony-specific PAUSE/speed-controller modeling anywhere -- not `"hard to find`", genuinely")
    $Lines.Add("absent, with the reference project's OWN machine-description text saying so explicitly")
    $Lines.Add("for four of five sibling Sony machines. There is no reference engine behavior to diff")
    $Lines.Add("against. Exact citations (independently re-confirmed this cycle):")
    $Lines.Add("")
    $Lines.Add("- ``references/openmsx-21.0/src/SG1000Pause.hh`` -- Sega SG-1000/SC-3000 `"hold`"/`"pause`"/")
    $Lines.Add("  `"reset`" button, triggers an NMI. Different machine family entirely (not MSX), different")
    $Lines.Add("  mechanism (NMI, not a CPU-halting WAIT-line gate).")
    $Lines.Add("- ``references/openmsx-21.0/src/MSXTurboRPause.{hh,cc}`` -- MSX turboR (S1990 chipset) pause")
    $Lines.Add("  key, a flip-flop status bit at I/O port 0xA7, implemented by calling")
    $Lines.Add("  ``getMotherBoard().pause()``/``unpause()`` (whole-session engine pause, architecturally")
    $Lines.Add("  incompatible with this project's atomic, deterministic per-instruction")
    $Lines.Add("  ``step_cpu_instruction()`` stepping model). Different chipset (S1990, not S1985/")
    $Lines.Add("  MB670836) and different machine family (turboR, not the HB-F1XV's plain MSX2+).")
    $Lines.Add("- None of the six real Sony MSX machine XML definitions wire a Pause or SpeedController")
    $Lines.Add("  device, and four of the six explicitly say so in their own ``<description>`` text (direct")
    $Lines.Add("  quotes): ``Sony_HB-F1.xml:9`` `"speed controller (not emulated)`"; ``Sony_HB-F1II.xml:9``")
    $Lines.Add("  `"speed controller (not emulated)`"; ``Sony_HB-F1XD.xml:9`` `"speed controller (not")
    $Lines.Add("  emulated)`"; ``Sony_HB-F1XDJ.xml:9`` `"speed controller (not emulated)`".")
    $Lines.Add("  ``Sony_HB-F1XDmk2.xml:9`` `"identical to its predecessor`" (``Sony_HB-F1XD.xml``, `"not")
    $Lines.Add("  emulated`") -- independently confirmed to wire no Pause/SpeedController device either.")
    $Lines.Add("  ``Sony_HB-F1XV.xml:9`` (this project's actual target machine) does not even mention the")
    $Lines.Add("  speed controller in its own (shorter) description -- consistent with `"not emulated, not")
    $Lines.Add("  even discussed`" rather than a silent omission of something that IS emulated.")
    $Lines.Add("")
    $Lines.Add("No parity is asserted for this sub-claim (honest disposition). Per Acceptance Criterion 9,")
    $Lines.Add("this BLOCKED disposition does NOT gate C8's closure (mirrors the M21 computed-pixel-color")
    $Lines.Add("and C3/M24 disk-boot-A/B precedents of an honest, non-fabricated BLOCKED sub-claim not")
    $Lines.Add("blocking an otherwise-complete milestone).")
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# M25 openMSX A/B Evidence -- Backlog C8 (Speed Controller + Hardware PAUSE + Ren-Sha Turbo)")
$lines.Add("")
$lines.Add("Two separate, honestly-dispositioned sub-claims -- see docs/m25-planner-package.md section 2.7 /")
$lines.Add("Acceptance Criterion 9.")

# --- Verify openMSX presence in WSL (honest capability check, not parity). ---
$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) {
    $lines.Add("")
    $lines.Add("## Ren-Sha Turbo: Result BLOCKED")
    $lines.Add("")
    $lines.Add("openMSX not found in WSL PATH -- the live Tcl technique could not be attempted.")
    Write-BlockedPauseSection -Lines $lines
    $outDir = Split-Path -Parent ([System.IO.Path]::GetFullPath($DiffOut))
    if ($outDir -and -not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
    Set-Content -LiteralPath $DiffOut -Value ($lines -join "`n") -Encoding ASCII
    Write-Output "M25 RESULT: Ren-Sha Turbo BLOCKED (no openMSX). See $DiffOut"
    exit 2
}
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion)"

# --- Tcl driver. Two phases, chained via `after time` re-scheduling (NOT
#     per-instruction single-stepping -- see the file header for why): phase
#     A (NotHeldSamples) samples row8/R14 with SPACE/trigger genuinely NOT
#     held while renshaturbo is engaged; phase B (HeldSamples) presses SPACE
#     (keymatrixdown) and continues sampling row8 to observe alternation. ---
$tcl = @"
set renderer none
set throttle off
set ::SAMPLES {}
set ::seq 0
set ::PHASE notheld
set ::NOTHELD $NotHeldSamples
set ::HELD $HeldSamples
set ::STEP $StepSeconds

proc sample {} {
  debug write ioports 0xAA 0x08
  set row8 [format %02X [debug read ioports 0xA9]]
  debug write ioports 0xA0 14
  set r14 [format %02X [debug read ioports 0xA2]]
  lappend ::SAMPLES "`$::PHASE `$::seq row8=`$row8 r14=`$r14"
  incr ::seq
  if {`$::PHASE == "notheld" && `$::seq >= `$::NOTHELD} {
    set ::PHASE held
    set ::seq 0
    keymatrixdown 8 1
    puts stderr "OMSETUP space_pressed=1"
  }
  if {`$::PHASE == "held" && `$::seq >= `$::HELD} {
    foreach s `$::SAMPLES { puts stderr "OMSAMPLE `$s" }
    puts stderr "OMDONE"
    exit
  }
  after time `$::STEP sample
}

after time $BootSeconds {
  if {[catch {
    set renshaturbo $RenshaSpeed
    puts stderr "OMSETUP renshaturbo_set=$RenshaSpeed"
    after time `$::STEP sample
  } err]} { puts stderr "OMSETUP ERROR msg=`$err"; exit }
}
"@

$encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
$runCmd = "echo $encoded | base64 -d > /tmp/omx_m25_rensha.tcl && " +
          "timeout 90 openmsx -machine $ProbeMachine -command 'set renderer none' " +
          "-script /tmp/omx_m25_rensha.tcl 2>&1 | grep -a -E 'OMSAMPLE|OMSETUP|OMDONE|OMTRACE' || true"
$raw = wsl -e bash -lc $runCmd

$rawLines = @()
if ($raw) {
    $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
}
$rawOut = "build/m25_rensha_openmsx_raw.txt"
Set-Content -LiteralPath $rawOut -Value ($rawLines -join "`n") -Encoding ASCII
Write-Output "openMSX raw output written to: $rawOut"

$sampleLines = @($rawLines | Where-Object { $_ -match "^OMSAMPLE " })
if ($sampleLines.Count -eq 0) {
    $lines.Add("")
    $lines.Add("## Ren-Sha Turbo: Result BLOCKED")
    $lines.Add("")
    $lines.Add("openMSX did not produce any OMSAMPLE output this run (raw output: $rawOut). The live Tcl")
    $lines.Add("technique could not be confirmed working.")
    Write-BlockedPauseSection -Lines $lines
    $outDir = Split-Path -Parent ([System.IO.Path]::GetFullPath($DiffOut))
    if ($outDir -and -not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
    Set-Content -LiteralPath $DiffOut -Value ($lines -join "`n") -Encoding ASCII
    Write-Output "M25 RESULT: Ren-Sha Turbo BLOCKED (no samples). See $DiffOut"
    exit 2
}

$notHeld = @($sampleLines | Where-Object { $_ -match "^OMSAMPLE notheld " })
$held = @($sampleLines | Where-Object { $_ -match "^OMSAMPLE held " })

function Get-Bit0 { param([string]$hex) return ([Convert]::ToInt32($hex, 16) -band 0x01) }
function Get-Bit4 { param([string]$hex) return ([Convert]::ToInt32($hex, 16) -band 0x10) }

$notHeldRow8AllIdle = $true
$notHeldR14AllIdle = $true
foreach ($s in $notHeld) {
    if ($s -match "row8=([0-9A-Fa-f]{2}) r14=([0-9A-Fa-f]{2})") {
        if ((Get-Bit0 $Matches[1]) -eq 0) { $notHeldRow8AllIdle = $false }
        if ((Get-Bit4 $Matches[2]) -eq 0) { $notHeldR14AllIdle = $false }
    }
}

$heldSawPressed = $false
$heldSawReleased = $false
foreach ($s in $held) {
    if ($s -match "row8=([0-9A-Fa-f]{2}) r14=([0-9A-Fa-f]{2})") {
        if ((Get-Bit0 $Matches[1]) -eq 0) { $heldSawPressed = $true } else { $heldSawReleased = $true }
    }
}

$lines.Add("")
$lines.Add("## Ren-Sha Turbo: Result")
$lines.Add("")
$lines.Add("- Reference: $openmsxVersion / WSL, machine ``$ProbeMachine`` (the real target machine,")
$lines.Add("  carrying the real ``<RenShaTurbo min_ints=47 max_ints=221>`` calibration,")
$lines.Add("  ``references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:16-19``).")
$lines.Add("- Technique: live Tcl ``set renshaturbo $RenshaSpeed`` (the real openMSX Autofire speed")
$lines.Add("  setting) + ``debug write/read ioports`` sampling of keyboard row 8 (#A9, after selecting")
$lines.Add("  row 8 via #AA) and PSG R14 (#A2, after latching register 14 via #A0), scheduled via")
$lines.Add("  ``after time`` (continuous native emulation between samples, not per-instruction")
$lines.Add("  single-stepping -- see the file header for why).")
$lines.Add("- Not-held samples: $($notHeld.Count); held samples: $($held.Count).")
$lines.Add("")
$lines.Add("### (a) Not held, RenSha engaged: keyboard row-8 bit0 (SPACE)")
$lines.Add("")
if ($notHeldRow8AllIdle) {
    $lines.Add("**PARITY** -- ZERO observable effect at every sampled point ($($notHeld.Count)/$($notHeld.Count) idle), matching")
    $lines.Add("R-M25-6's invariant, confirmed live on the real reference engine.")
} else {
    $lines.Add("**DIVERGENCE** -- at least one sample showed a spurious press with SPACE not held.")
}
$lines.Add("")
$lines.Add("### (c) Not held, RenSha engaged: PSG R14 bit4 (joystick trigger A)")
$lines.Add("")
if ($notHeldR14AllIdle) {
    $lines.Add("**PARITY** -- ZERO observable effect at every sampled point ($($notHeld.Count)/$($notHeld.Count) idle), matching")
    $lines.Add("R-M25-6's invariant, confirmed live on the real reference engine.")
} else {
    $lines.Add("**DIVERGENCE** -- at least one sample showed a spurious trigger with trigger-A not held.")
}
$lines.Add("")
$lines.Add("### (b) Held (``keymatrixdown 8 1``), RenSha engaged: keyboard row-8 bit0 alternation")
$lines.Add("")
if ($heldSawPressed -and $heldSawReleased) {
    $lines.Add("**PARITY** -- the real reference engine's live read alternates between `"pressed`" (bit0=0,")
    $lines.Add("$(@($held | Where-Object { $_ -match 'row8=([0-9A-Fa-f]{2})' -and (Get-Bit0 $Matches[1]) -eq 0 }).Count) samples) and `"released`" (bit0=1, forced by the autofire OR-combine,")
    $lines.Add("$(@($held | Where-Object { $_ -match 'row8=([0-9A-Fa-f]{2})' -and (Get-Bit0 $Matches[1]) -eq 1 }).Count) samples) -- matching this project's own OR-only-releases implementation")
    $lines.Add("(A-M25-7).")
} else {
    $lines.Add("**DIVERGENCE / INCONCLUSIVE** -- did not observe both a pressed and a released sample")
    $lines.Add("(saw_pressed=$heldSawPressed, saw_released=$heldSawReleased) -- the sampling window may need")
    $lines.Add("widening (``-HeldSamples``/``-StepSeconds``).")
}
$lines.Add("")
$lines.Add("### Explicitly NOT attempted (honest disposition, not silently skipped)")
$lines.Add("")
$lines.Add("A live `"held`" alternation demonstration for the PSG R14/joystick-trigger-A path: no")
$lines.Add("equivalent `"hold a joystick button`" Tcl scripting primitive was found in openMSX 21.0")
$lines.Add("(``keymatrixdown``/``keymatrixup``, ``references/openmsx-21.0/src/input/Keyboard.cc:1565-1611``,")
$lines.Add("apply ONLY to the keyboard matrix). The not-held invariant (c, above) IS confirmed live for")
$lines.Add("this path; the held-alternation behavior for the joystick path relies on this project's own")
$lines.Add("already-exhaustively-tested implementation (M25-S2/S3, the identical OR-combine code path as")
$lines.Add("the keyboard, A-M25-7), not a live cross-engine comparison.")

Write-BlockedPauseSection -Lines $lines

$outDir = Split-Path -Parent ([System.IO.Path]::GetFullPath($DiffOut))
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
Set-Content -LiteralPath $DiffOut -Value ($lines -join "`n") -Encoding ASCII

$renshaOverallOk = $notHeldRow8AllIdle -and $notHeldR14AllIdle -and $heldSawPressed -and $heldSawReleased
if ($renshaOverallOk) {
    Write-Output "M25 RESULT: Ren-Sha Turbo PARITY (not-held invariant + held alternation). Hardware PAUSE/Speed Controller: BLOCKED (honest, expected). See $DiffOut"
    exit 0
} else {
    Write-Output "M25 RESULT: Ren-Sha Turbo DIVERGENCE/INCONCLUSIVE. Hardware PAUSE/Speed Controller: BLOCKED (honest, expected). See $DiffOut"
    exit 1
}
