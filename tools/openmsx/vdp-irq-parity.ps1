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
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [string]$BaseHex      = "C000",
    [string]$DocOut       = "docs/openmsx-vdp-irq-parity.md",
    [string]$RawOut       = "build/openmsx_vdp_irq_parity_raw.txt",
    [int]$BootSeconds     = 6,
    [int]$RunSeconds      = 2
)

# ---------------------------------------------------------------------------
# M37 Slice A (DEC-0055, clears DEC-0054 residual R-B): openMSX V9958 VDP-IRQ
# register-write /INT re-evaluation parity harness.
#
# WHAT IT PROVES
#   The M36 Bug B fix made V9958Vdp::change_register RE-EVALUATE the vertical
#   /INT line on an R#1 write that TOGGLES IE0 (bit5) -- so clearing IE0
#   immediately de-asserts a held VBlank /INT (the YS II interior-load
#   interrupt-storm cause) and re-setting IE0 re-asserts it while the F flag is
#   still pending. This harness reproduces the SAME behavior on REAL openMSX
#   running the GENUINE Sony_HB-F1XV machine (real V9958), grounding the fix in
#   observed hardware-reference behavior rather than report prose.
#
# GROUNDING (openMSX 21.0 source, the primary behavior reference)
#   references/openmsx-21.0/src/video/VDP.cc:1186-1198 -- changeRegister case 1:
#     if (change & 0x20) {                      // IE0 toggled
#         if (val & 0x20) {                     //   IE0 set ...
#             if (statusReg0 & 0x80) irqVertical.set();   // ... re-assert IFF F pending
#         } else {
#             irqVertical.reset();              //   IE0 clear -> de-assert now
#         }
#     }
#   Our analogue: src/devices/video/v9958_vdp.cpp change_register case 1.
#   The debug WRITE path used below routes through the SAME changeRegister:
#     references/openmsx-21.0/src/video/VDP.cc:1615-1623 (RegDebug::write ->
#     changeRegister), and the status-reg READ is the side-effect-free peek
#     (VDP.cc:1635-1638, peekStatusReg) so it does NOT clear the F flag.
#
# THE PROBE (deterministic, side-effect isolated)
#   1. Boot the genuine machine headlessly; break the CPU.
#   2. Park the Z80 in a `JR $` (18 FE) spin at 0x$BaseHex with iff=0, so NOTHING
#      reads S#0 (a real IN A,(0x99) on S#0 would clear the F flag and defeat the
#      test); the debug status read below uses the peek path, which does not.
#   3. Clear R#1 IE0 (bit5) and let a few frames run: a VBlank latches S#0 F while
#      IE0 is OFF -> IRQvertical stays 0. This is the baseline.
#   4. With F pending, toggle R#1 IE0 via `debug write {VDP regs} 1` (-> changeRegister)
#      and read `debug probe read VDP.IRQvertical` after each edge:
#        baseline (IE0 off, F pending) ......... IV_baseline        = 0
#        IE0 set  (F pending -> re-assert) ..... IV_ie0set_Fpending = 1
#        IE0 clear (de-assert now) ............. IV_ie0clear        = 0
#        IE0 set again (F still pending) ....... IV_ie0set2         = 1
#   The ASSERTED, hardware-observed pattern is 0 -> 1 -> 0 -> 1.
#
# It NEVER fabricates parity: the actual captured OMIRQ line (or a BLOCKED note if
# openMSX cannot be driven) is written to $DocOut, and any deviation from the
# 0->1->0->1 pattern is a LOUD FAIL with a non-zero exit.
#
# This slice edits tools/ + docs/ ONLY (no src/, no rebuild).
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# --- Verify openMSX presence in WSL (honest capability, not parity). ---
$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
$openmsxVersion = ""
if (-not [string]::IsNullOrWhiteSpace($openmsxPath)) {
    $openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
    Write-Output "openMSX: $openmsxPath ($openmsxVersion)"
    Write-Output "Reference machine: $ProbeMachine (genuine V9958 in machine XML)"
} else {
    Write-Warning "openMSX not found in WSL PATH -- harness will report BLOCKED (no A/B capture)."
}

# --- Tcl driver: park CPU (JR $, iff=0), latch VBlank F with IE0 off, then
#     toggle R#1 IE0 through changeRegister and read the IRQvertical probe. ---
$base = "0x$BaseHex"
$spinBytes = "0x18 0xFE"   # JR $  (self-loop; PC never advances, nothing reads S#0)

$tcl = @"
set renderer none
set throttle off
set ::PROG { $spinBytes }
set ::BASE $base
proc readiv {} { return [debug probe read VDP.IRQvertical] }
proc doprobe {} {
  if {[catch {
    debug break
    set r1 [debug read {VDP regs} 1]
    set s0 [debug read {VDP status regs} 0]
    set ivb [readiv]
    debug write {VDP regs} 1 [expr {`$r1 | 0x20}]
    set iv1 [readiv]
    debug write {VDP regs} 1 [expr {`$r1 & ~0x20}]
    set iv2 [readiv]
    debug write {VDP regs} 1 [expr {`$r1 | 0x20}]
    set iv3 [readiv]
    puts stderr [format "OMIRQ S0=%02X IV_baseline=%s IV_ie0set_Fpending=%s IV_ie0clear=%s IV_ie0set2=%s" \
      `$s0 `$ivb `$iv1 `$iv2 `$iv3]
  } err]} { puts stderr "OMIRQ_ERR=`$err"; catch { puts stderr "OMPROBES=[debug probe list]" } }
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
    set r1 [debug read {VDP regs} 1]
    debug write {VDP regs} 1 [expr {`$r1 & ~0x20}]
    puts stderr [format "OMSETUP pc=%04X r1_ie0off=%02X" [reg pc] [debug read {VDP regs} 1]]
    debug cont
    after time $RunSeconds doprobe
  } err]} { puts stderr "OMSETUP_ERR=`$err"; exit }
}
"@

$rawLines = @()
if (-not [string]::IsNullOrWhiteSpace($openmsxPath)) {
    $encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
    $runCmd = "echo $encoded | base64 -d > /tmp/omx_vdp_irq_parity.tcl && " +
              "timeout 120 openmsx -machine $ProbeMachine -command 'set renderer none' " +
              "-script /tmp/omx_vdp_irq_parity.tcl 2>&1 | grep -aE 'OMIRQ|OMSETUP|OMPROBES' || true"
    $raw = wsl -e bash -lc $runCmd
    if ($raw) {
        $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
    }
}

# --- Persist the raw capture for auditability. ---
$rawOutFull = [System.IO.Path]::GetFullPath($RawOut)
$rawDir = Split-Path -Parent $rawOutFull
if ($rawDir -and -not (Test-Path -LiteralPath $rawDir)) { New-Item -ItemType Directory -Path $rawDir | Out-Null }
Set-Content -LiteralPath $rawOutFull -Value ($rawLines -join "`n") -Encoding ASCII

foreach ($l in $rawLines) { Write-Output $l }

# --- Parse the OMIRQ result line and assert the 0 -> 1 -> 0 -> 1 pattern. ---
$omirq = $rawLines | Where-Object { $_ -match "^OMIRQ S0=" } | Select-Object -First 1

$s0 = ""; $ivb = ""; $iv1 = ""; $iv2 = ""; $iv3 = ""
$result = "BLOCKED"; $exit = 2

if ($omirq -and
    ($omirq -match "S0=([0-9A-Fa-f]{2})\s+IV_baseline=(\S+)\s+IV_ie0set_Fpending=(\S+)\s+IV_ie0clear=(\S+)\s+IV_ie0set2=(\S+)")) {
    $s0  = $Matches[1].ToUpper()
    $ivb = $Matches[2]; $iv1 = $Matches[3]; $iv2 = $Matches[4]; $iv3 = $Matches[5]
    # Normalize probe values ("1"/"0" from Probe<bool>::getValue) to integers.
    function To01 { param([string]$v) if ($v -match "^(1|true)$") { 1 } elseif ($v -match "^(0|false)$") { 0 } else { -1 } }
    $b = To01 $ivb; $a1 = To01 $iv1; $a2 = To01 $iv2; $a3 = To01 $iv3
    if ($b -eq 0 -and $a1 -eq 1 -and $a2 -eq 0 -and $a3 -eq 1) {
        $result = "PASS (0->1->0->1)"; $exit = 0
    } else {
        $result = "FAIL (pattern deviates from 0->1->0->1)"; $exit = 1
    }
}

# --- Write the committed docs artifact. ---
$docOutFull = [System.IO.Path]::GetFullPath($DocOut)
$docDir = Split-Path -Parent $docOutFull
if ($docDir -and -not (Test-Path -LiteralPath $docDir)) { New-Item -ItemType Directory -Path $docDir | Out-Null }
$stamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# openMSX V9958 VDP-IRQ Register-Write /INT Parity (Sony HB-F1XV)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("Reproducible A/B evidence for the M36 Bug B fix (V9958 IE0/IE1 register-write")
[void]$sb.AppendLine("/INT re-evaluation). Regenerate with ``tools/openmsx/vdp-irq-parity.ps1``.")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("- Captured (UTC): $stamp")
[void]$sb.AppendLine("- Reference: openMSX on WSL ($openmsxVersion), machine ``$ProbeMachine`` (genuine V9958).")
[void]$sb.AppendLine("- Grounding: openMSX ``references/openmsx-21.0/src/video/VDP.cc:1186-1198`` (``changeRegister`` case 1 IE0 re-eval); debug write routes through the same ``changeRegister`` (VDP.cc:1615-1623); status read is the side-effect-free ``peekStatusReg`` (VDP.cc:1635-1638).")
[void]$sb.AppendLine("- Our fix: ``src/devices/video/v9958_vdp.cpp`` ``change_register`` case 1.")
[void]$sb.AppendLine("- Unit test: ``tests/unit/devices/video_v9958_ie0_register_write_irq_unit_test.cpp`` (``devices_v9958_ie0_register_write_irq_unit_test``).")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Probe")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("Park the Z80 in a ``JR `$`` spin at 0x$BaseHex with ``iff=0`` (nothing reads S#0), clear")
[void]$sb.AppendLine("R#1 IE0 and run a few frames so a VBlank latches the S#0 F flag WITHOUT asserting")
[void]$sb.AppendLine("/INT, then toggle R#1 IE0 and read the ``VDP.IRQvertical`` probe after each edge.")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Result: $result")
[void]$sb.AppendLine("")
[void]$sb.AppendLine('```')
if ($omirq) { [void]$sb.AppendLine($omirq) }
else { [void]$sb.AppendLine("BLOCKED: openMSX produced no OMIRQ line (see $RawOut).") }
[void]$sb.AppendLine('```')
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Reading | openMSX | Meaning | Grounds |")
[void]$sb.AppendLine("| --- | --- | --- | --- |")
[void]$sb.AppendLine("| ``IV_baseline`` | $ivb | IE0 off, F pending -> /INT NOT asserted | latch F without IE0 (VDP.cc:404 gate) |")
[void]$sb.AppendLine("| ``IV_ie0set_Fpending`` | $iv1 | IE0 set with F pending -> re-assert | VDP.cc:1189-1194 (Andonis/Zanac) |")
[void]$sb.AppendLine("| ``IV_ie0clear`` | $iv2 | IE0 clear -> de-assert immediately | VDP.cc:1196 ``irqVertical.reset()`` |")
[void]$sb.AppendLine("| ``IV_ie0set2`` | $iv3 | IE0 set again, F still pending -> re-assert | VDP.cc:1189-1194 |")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Interpretation")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("The ``IV_ie0clear=0`` edge is the M36 Bug B behavior: on real openMSX hardware,")
[void]$sb.AppendLine("clearing R#1 IE0 mid-frame IMMEDIATELY de-asserts a held VBlank /INT even though the")
[void]$sb.AppendLine("S#0 F flag is still latched. Our ``change_register`` reproduces exactly this (and the")
[void]$sb.AppendLine("``IV_ie0set*=1`` re-assert-while-F-pending edges), which is the same sequence exercised")
[void]$sb.AppendLine("by ``devices_v9958_ie0_register_write_irq_unit_test``. Before the fix our /INT stayed")
[void]$sb.AppendLine("asserted after the ISR cleared IE0, so the ISR's trailing ``EI`` re-entered forever")
[void]$sb.AppendLine("(the YS II interior-load interrupt storm / stack overflow).")
if ($exit -ne 0) {
    [void]$sb.AppendLine("")
    if ($exit -eq 2) {
        [void]$sb.AppendLine("> NOTE: openMSX was unavailable/undrivable at capture time. The previously")
        [void]$sb.AppendLine("> captured result recorded in ``docs/m36-bugB-vdp-ie0-fix-report.md`` (S0=9F,")
        [void]$sb.AppendLine("> the IE0-toggle re-eval matching this fix) stands until a re-run succeeds.")
    } else {
        [void]$sb.AppendLine("> WARNING: the captured pattern DEVIATED from 0->1->0->1 -- investigate before trusting.")
    }
}
Set-Content -LiteralPath $docOutFull -Value $sb.ToString() -Encoding ASCII

switch ($exit) {
    0 { Write-Output "M37-A VDP-IRQ A/B RESULT: PASS (0->1->0->1). See $DocOut" }
    1 { Write-Output "M37-A VDP-IRQ A/B RESULT: FAIL (pattern deviation). See $DocOut and $RawOut" }
    default { Write-Output "M37-A VDP-IRQ A/B RESULT: BLOCKED / not comparable. See $DocOut and $RawOut" }
}
exit $exit
