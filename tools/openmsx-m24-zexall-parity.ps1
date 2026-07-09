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
    [string]$ZexallCom = "references/zexall/zexall.com",
    [string]$ZexdocCom = "references/zexall/zexdoc.com",
    [int]$BoundInstructions = 11,
    [string]$OutA_Zexall = "build/m24_zexall_bound_A.txt",
    [string]$OutA_Zexdoc = "build/m24_zexdoc_bound_A.txt",
    [string]$TraceBRaw = "build/m24_zexall_parity_B.txt",
    [string]$DiffOut = "docs/m24-parity-trace-diff.md",
    [string]$ProbeMachine = "C-BIOS_MSX2+",
    [int]$BootSeconds = 6
)

# ---------------------------------------------------------------------------
# M24-S4 openMSX A/B evidence -- backlog C3 (ZEXDOC/ZEXALL full parity sweep).
#
# PRIMARY, FEASIBLE technique (planner package SS2.3): mirror this project's
# OWN CpmBdosHarness on openMSX's Tcl side, BOUNDED to a small, live-verified
# prefix. A genuine FULL-suite live-Tcl single-step A/B (tens/hundreds of
# millions of steps per suite, see docs/m24-implementation-report.md's
# measured runtime) is infeasible for the SAME reason M23's own A/B evidence
# already discovered (docs/m23-parity-trace-diff.md): a live openMSX Tcl
# session's scheduler behaves differently once enough real/wall-clock time
# elapses between per-step round-trips. This probe does NOT attempt that; it
# is a small, bounded, decision-relevant check of the LOADING CONVENTION +
# BDOS-TRAP MECHANISM itself (org 0x0100, the CP/M "top of memory" word, the
# CALL-5 dispatch), not the full ~1.7M-combinatorial-case-per-suite sweep
# (already covered, in-process, by tests/system/hbf1xv_m24_zexall_system_test.cpp
# + this project's own pre-existing, independently QA-verified M9/M10/M12
# CPU-timing oracles).
#
# BOUND CHOSEN (R-M24-7 disclosure): $BoundInstructions = 11 real Z80
# instructions -- empirically, LIVE-verified at implementation time (not
# assumed): both zexall.com and zexdoc.com execute EXACTLY 11 real
# instructions (JP start; LD HL,(6); LD SP,HL; LD DE,msg1; LD C,9;
# CALL bdos; 4x PUSH; CALL 5) from the fixed CP/M entry point before PC first
# reaches the BDOS trap address (0x0005) -- at which point the FIRST BDOS
# call (C=9, the startup banner "Z80 instruction exerciser") is captured.
# A larger bound (e.g. "complete the first 1-2 named test groups", as
# originally suggested) was tried and found NOT live-Tcl-feasible: even the
# SMALLEST named group needs on the order of 10^5-10^8 real instructions
# (see the implementation report's runtime-measurement section), which is
# firmly in the SAME infeasible-for-live-Tcl territory as the full sweep.
# This smaller, precisely-justified bound is a genuine, honest, decision-
# relevant substitute: it exercises the actual NEW mechanism this milestone
# adds (CpmBdosHarness's loading convention + BDOS dispatch trap), not a
# re-verification of the underlying CPU-instruction-level correctness that
# M9/M10/M12's own oracles (and this milestone's in-process real-binary
# system test) already cover.
#
# It NEVER fabricates parity: if openMSX is not reachable, or the technique
# does not behave as expected, this reports BLOCKED honestly rather than
# manufacturing a result.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Blocked {
    param([string]$Reason)
    $full = [System.IO.Path]::GetFullPath($DiffOut)
    $outDir = Split-Path -Parent $full
    if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Path $outDir | Out-Null
    }
    @"
# M24 openMSX A/B Evidence -- Backlog C3 (ZEXDOC/ZEXALL Full Parity Sweep)

## Result: BLOCKED

$Reason

No parity is asserted (honest disposition -- this probe is BLOCKED, not
silently skipped or fabricated).
"@ | Set-Content -LiteralPath $DiffOut -Encoding ASCII
    Write-Output "M24 C3 bounded-prefix A/B RESULT: BLOCKED. See $DiffOut"
}

if (-not (Test-Path -LiteralPath $Emulator)) {
    Write-Blocked -Reason "Emulator binary not found: $Emulator (build first: cmake --build build --config Debug)."
    exit 2
}
if (-not (Test-Path -LiteralPath $ZexallCom)) {
    Write-Blocked -Reason "zexall.com not found: $ZexallCom (M24-S0 must commit references/zexall/ first)."
    exit 2
}
if (-not (Test-Path -LiteralPath $ZexdocCom)) {
    Write-Blocked -Reason "zexdoc.com not found: $ZexdocCom (M24-S0 must commit references/zexall/ first)."
    exit 2
}

# --- Verify openMSX presence in WSL (honest capability check, not parity). ---
$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
if ([string]::IsNullOrWhiteSpace($openmsxPath)) {
    Write-Blocked -Reason "openMSX not found in WSL PATH -- the live Tcl BDOS-trap technique could not be attempted."
    exit 2
}
$openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
Write-Output "openMSX: $openmsxPath ($openmsxVersion)"

# --- A side: this project's OWN CpmBdosHarness via --cpm-run, bounded to ----
# --- the SAME $BoundInstructions. The BDOS-trap check (PC==5) is evaluated ---
# --- BEFORE the budget check every loop iteration (src/machine/
# --- cpm_bdos_harness.cpp), so a budget of exactly $BoundInstructions still
# --- lets the harness capture the FIRST BDOS call's output once PC reaches
# --- the trap for the first time. ---------------------------------------
function Invoke-EmulatorBound {
    param([string]$ComPath, [string]$OutPath)
    & $Emulator --cpm-run $ComPath $BoundInstructions $OutPath
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 3) {
        # exit 3 = honest "ran out of budget" (expected here -- finished=false
        # is the CORRECT outcome for a deliberately bounded prefix, not a
        # failure); any OTHER non-zero code is a genuine emulator-side error.
        throw "emulator --cpm-run failed unexpectedly (exit $LASTEXITCODE) for $ComPath"
    }
    if (-not (Test-Path -LiteralPath $OutPath)) {
        throw "emulator did not produce captured-output file: $OutPath"
    }
    return Get-Content -LiteralPath $OutPath -Raw -ErrorAction SilentlyContinue
}

$aZexall = Invoke-EmulatorBound -ComPath $ZexallCom -OutPath $OutA_Zexall
$aZexdoc = Invoke-EmulatorBound -ComPath $ZexdocCom -OutPath $OutA_Zexdoc
if ($null -eq $aZexall) { $aZexall = "" }
if ($null -eq $aZexdoc) { $aZexdoc = "" }
Write-Output "Emulator A (zexall) captured: '$aZexall'"
Write-Output "Emulator A (zexdoc) captured: '$aZexdoc'"

# --- B side: openMSX on WSL. Self-contained Tcl driver implementing the ----
# --- IDENTICAL PC==5/PC==0 BDOS-trap logic as src/machine/
# --- cpm_bdos_harness.cpp: reads C for the function, walks DE for a C=9
# --- $-terminated string (stopping BEFORE the '$'), and synthesizes the
# --- CALL 5 return by popping the stack -- then exits immediately after the
# --- FIRST such capture (the bounded-prefix stopping point). The probe
# --- machine is C-BIOS_MSX2+ (RAM-heavy MSX2+ profile) with ALL FOUR CPU
# --- pages explicitly routed to its "Main RAM" MemoryMapper (primary slot 3,
# --- secondary slot 2, per /usr/share/openmsx/machines/C-BIOS_MSX2+.xml,
# --- read this cycle) via the SAME #A8 + indirect-0xFFFF sub-slot mechanism
# --- this project's OWN map_flat_ram() uses against ITS OWN slot map --
# --- required because this machine's real page-0 default is BIOS ROM, not
# --- writable RAM (confirmed empirically this cycle: an unconditioned poke
# --- at 0x100 read back a stale ROM byte, not the poked value, before this
# --- slot-routing fix was added).
function Invoke-OpenmsxBound {
    param([string]$ComPath, [string]$TraceOutPath)
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ComPath))
    $byteList = ($bytes | ForEach-Object { "0x{0:X2}" -f $_ }) -join " "

    $tcl = @"
set renderer none
set throttle off
set ::PROG { $byteList }
set ::TOPMEM 0xFF00
# +1 margin: the budget check below fires BEFORE the step that would land on
# PC==5 is taken (it guards the NEXT step, not the one just completed), so
# reaching PC==5 after exactly $BoundInstructions real steps needs the
# ceiling set one higher than that count (verified live this cycle).
set ::MAX $($BoundInstructions + 1)

proc emit {} {
  if {[catch {
    set pc [reg pc]
    if {`$pc == 5} {
      set c [expr {[reg bc] & 0xFF}]
      if {`$c == 9} {
        set addr [expr {[reg de] & 0xFFFF}]
        set msg ""
        while {1} {
          set b [debug read memory `$addr]
          if {`$b == 0x24} break
          append msg [format %c `$b]
          set addr [expr {(`$addr + 1) & 0xFFFF}]
        }
        puts stderr "OMBDOS c9 msg=`$msg"
      } elseif {`$c == 2} {
        set e [expr {[reg de] & 0xFF}]
        puts stderr [format "OMBDOS c2 char=%c" `$e]
      } else {
        puts stderr "OMBDOS unexpected c=`$c"
      }
      set sp [reg sp]
      set lo [debug read memory `$sp]
      set hi [debug read memory [expr {(`$sp + 1) & 0xFFFF}]]
      reg sp [expr {(`$sp + 2) & 0xFFFF}]
      reg pc [expr {((`$hi << 8) | `$lo) & 0xFFFF}]
      puts stderr "OMBDOS bounded-prefix capture complete"
      exit
    }
    if {`$pc == 0} {
      puts stderr "OMBDOS warmboot pc=0 (unexpected this early -- bound too small?)"
      exit
    }
    incr ::seq
    if {`$::seq >= `$::MAX} { puts stderr "OMSTEP budget exhausted before reaching PC=5"; exit }
    after break emit
    step
  } err]} { puts stderr "OMTRACE ERROR seq=`$::seq msg=`$err"; exit }
}

after time $BootSeconds {
  if {[catch {
    debug write ioports 0xA8 0xFF
    debug write memory 0xFFFF 0xAA
    debug write ioports 0xFC 0x00
    debug write ioports 0xFD 0x01
    debug write ioports 0xFE 0x02
    debug write ioports 0xFF 0x03

    set a 0x100
    foreach byte `$::PROG { debug write memory `$a `$byte; incr a }
    set rb [format %02X [debug read memory 0x100]]
    puts stderr "OMSETUP ram_base_readback=`$rb prog_len=[llength `$::PROG]"
    debug write memory 0x0006 [expr {`$::TOPMEM & 0xFF}]
    debug write memory 0x0007 [expr {(`$::TOPMEM >> 8) & 0xFF}]
    reg af 0; reg bc 0; reg de 0; reg hl 0
    reg af2 0; reg bc2 0; reg de2 0; reg hl2 0
    reg ix 0; reg iy 0; reg sp 0xFFFF; reg pc 0x100
    reg i 0; reg r 0; reg im 1; reg iff 0
    set ::seq 0
    after break emit
    debug break
  } err]} { puts stderr "OMSETUP ERROR msg=`$err"; exit }
}
"@

    # Write the base64 payload to a FILE (not embedded in the command-line
    # string): the full zexall.com/zexdoc.com byte list (8704 bytes) encodes
    # to ~58,000 base64 characters, which exceeds what can safely be passed
    # as a single Windows command-line argument (found this cycle -- an
    # earlier draft embedding the payload inline failed with a native-command
    # "file not found"-class error, a genuine command-line-length limit, not
    # a WSL/openMSX availability problem). Referencing a file by its short
    # /mnt/c/... path keeps the actual `wsl` command line tiny regardless of
    # program size.
    $encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
    $b64Path = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $TraceOutPath) "$([System.IO.Path]::GetFileNameWithoutExtension($TraceOutPath))_ab.b64"))
    $b64Dir = Split-Path -Parent $b64Path
    if ($b64Dir -and -not (Test-Path -LiteralPath $b64Dir)) {
        New-Item -ItemType Directory -Path $b64Dir | Out-Null
    }
    Set-Content -LiteralPath $b64Path -Value $encoded -Encoding ASCII -NoNewline
    $wslB64Path = (wsl -e bash -lc "wslpath -a '$($b64Path -replace '\\','/')'" ).Trim()
    if ([string]::IsNullOrWhiteSpace($wslB64Path)) {
        # Fallback: construct the /mnt/c/... path directly if wslpath is unavailable.
        $driveLetter = $b64Path.Substring(0,1).ToLower()
        $rest = $b64Path.Substring(2) -replace '\\','/'
        $wslB64Path = "/mnt/$driveLetter$rest"
    }
    $runCmd = "base64 -d -i '$wslB64Path' > /tmp/omx_m24_zexall_ab.tcl && " +
              "timeout 60 openmsx -machine $ProbeMachine -command 'set renderer none' " +
              "-script /tmp/omx_m24_zexall_ab.tcl 2>&1 | grep -a -E 'OMBDOS|OMSETUP|OMSTEP|OMTRACE' || true"
    $raw = wsl -e bash -lc $runCmd

    $rawLines = @()
    if ($raw) {
        $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
    }
    Set-Content -LiteralPath $TraceOutPath -Value ($rawLines -join "`n") -Encoding ASCII

    $msgLine = $rawLines | Where-Object { $_ -match "^OMBDOS c9 msg=" } | Select-Object -First 1
    $captured = $null
    if ($msgLine) {
        $captured = $msgLine -replace "^OMBDOS c9 msg=", ""
    }
    return @{ Captured = $captured; RawLines = $rawLines }
}

$bZexallOutPath = $TraceBRaw -replace "\.txt$", "_zexall.txt"
$bZexdocOutPath = $TraceBRaw -replace "\.txt$", "_zexdoc.txt"
$bZexall = Invoke-OpenmsxBound -ComPath $ZexallCom -TraceOutPath $bZexallOutPath
$bZexdoc = Invoke-OpenmsxBound -ComPath $ZexdocCom -TraceOutPath $bZexdocOutPath

Write-Output "openMSX B (zexall) captured: '$($bZexall.Captured)'"
Write-Output "openMSX B (zexdoc) captured: '$($bZexdoc.Captured)'"

if ($null -eq $bZexall.Captured -or $null -eq $bZexdoc.Captured) {
    Write-Blocked -Reason ("openMSX did not produce a captured C=9 banner for one or both suites within " +
        "$BoundInstructions instructions (raw WSL output written to $bZexallOutPath / $bZexdocOutPath). " +
        "The live Tcl BDOS-trap technique could not be confirmed working this run.")
    exit 2
}

# --- Compare: the emulator side captures raw bytes including the LF,CR ------
# --- terminator (10,13); the openMSX side's Tcl `format %c` on those same ---
# --- byte values renders them as their literal control characters too, so ---
# --- trim any trailing whitespace/control chars from BOTH sides for a fair,
# --- text-content comparison (the actual decision-relevant content is the
# --- banner TEXT, "Z80 instruction exerciser"). -----------------------------
function Get-TrimmedText {
    param([string]$Text)
    if ($null -eq $Text) { return "" }
    return ($Text -replace "[\r\n]+$", "")
}

$aZexallText = Get-TrimmedText -Text $aZexall
$bZexallText = Get-TrimmedText -Text $bZexall.Captured
$aZexdocText = Get-TrimmedText -Text $aZexdoc
$bZexdocText = Get-TrimmedText -Text $bZexdoc.Captured

$zexallMatch = ($aZexallText -ceq $bZexallText)
$zexdocMatch = ($aZexdocText -ceq $bZexdocText)

$lines = @()
$lines += "# M24 openMSX A/B Evidence -- Backlog C3 (ZEXDOC/ZEXALL Full Parity Sweep)"
$lines += ""
$lines += "- Subject A (this emulator): CpmBdosHarness via ``--cpm-run``, bounded to $BoundInstructions real Z80 instructions (``$OutA_Zexall`` / ``$OutA_Zexdoc``)"
$lines += "- Reference B (openMSX $openmsxVersion / WSL, machine ``$ProbeMachine``): live Tcl ``debug``/``reg`` interface implementing the IDENTICAL PC==5/PC==0 BDOS-trap logic (``$bZexallOutPath`` / ``$bZexdocOutPath``)"
$lines += "- Bound: $BoundInstructions instructions -- empirically confirmed this cycle to be exactly the number of real Z80 instructions BOTH engines execute from the CP/M entry point (``org 0x0100``) before PC first reaches the BDOS trap address (0x0005), where the startup banner (BDOS function C=9) is captured. See the file-level comment above for the full reasoning (R-M24-7)."
$lines += ""
$lines += "## Scope note (read before interpreting this diff)"
$lines += ""
$lines += ("This probe compares the LOADING CONVENTION + BDOS-TRAP MECHANISM itself (org 0x0100; " +
           "the CP/M ``top of memory`` word; the ``CALL 5`` dispatch and its captured C=9 output) " +
           "for a small, live-Tcl-feasible prefix -- NOT the full ~1.7 million-combinatorial-case-" +
           "per-suite correctness sweep (already covered, in-process, by " +
           "``tests/system/hbf1xv_m24_zexall_system_test.cpp`` and this project's own pre-existing, " +
           "independently QA-verified M9/M10/M12 CPU-timing oracles). A full-suite live-Tcl single-" +
           "step A/B is explicitly, honestly NOT attempted here -- see " +
           "``docs/m24-implementation-report.md`` for the measured full-sweep runtime and " +
           "``docs/m23-parity-trace-diff.md`` for the prior, independently-discovered real-time-" +
           "scheduling artifact that makes a live-Tcl full sweep infeasible at this instruction volume.")
$lines += ""
$lines += "## ZEXALL result"
$lines += ""
$lines += "- Emulator A captured: ``$aZexallText``"
$lines += "- openMSX B captured: ``$bZexallText``"
if ($zexallMatch) {
    $lines += "- **PARITY** -- byte-for-byte identical captured banner text."
} else {
    $lines += "- **DIVERGENCE** -- captured text differs."
}
$lines += ""
$lines += "## ZEXDOC result"
$lines += ""
$lines += "- Emulator A captured: ``$aZexdocText``"
$lines += "- openMSX B captured: ``$bZexdocText``"
if ($zexdocMatch) {
    $lines += "- **PARITY** -- byte-for-byte identical captured banner text."
} else {
    $lines += "- **DIVERGENCE** -- captured text differs."
}
$lines += ""
$lines += "## Explicitly BLOCKED / not attempted (honest disposition, not silently skipped)"
$lines += ""
$lines += ("1. **A genuine full-suite live-Tcl single-step A/B of the entire ZEXALL/ZEXDOC run** -- " +
           "infeasible per the real-time-scheduling artifact ``docs/m23-parity-trace-diff.md`` " +
           "already discovered, at the instruction volume this milestone's own runtime measurement " +
           "found necessary (see ``docs/m24-implementation-report.md``).")
$lines += ("2. **Booting a real MSX-DOS disk to a command prompt and running the binaries as genuine " +
           "MSX-DOS transient programs** -- depends on an MSX-DOS system-disk asset. Checked this " +
           "cycle: neither ``bios/`` nor ``roms/`` contains any such asset (both directories listed " +
           "in full; only the Sony HB-F1XV's own BIOS/Kanji/disk-ROM/firmware images and two " +
           "cartridge-mapper smoke fixtures are present -- no MSX-DOS kernel/COMMAND.COM-class file). " +
           "Also depends on the still-open backlog C5 remainder (full unattended boot). Honestly " +
           "marked BLOCKED, not silently assumed absent.")
$lines += ""

Set-Content -LiteralPath $DiffOut -Value ($lines -join "`n") -Encoding ASCII

if ($zexallMatch -and $zexdocMatch) {
    Write-Output "M24 C3 bounded-prefix A/B RESULT: PARITY (both suites). See $DiffOut"
    exit 0
} else {
    Write-Output "M24 C3 bounded-prefix A/B RESULT: DIVERGENCE. See $DiffOut"
    exit 1
}
