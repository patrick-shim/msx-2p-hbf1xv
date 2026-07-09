param(
    [string]$MgRom = "roms/metalgear.rom",
    [string]$WorkDir = "build/m34-ab",
    [int]$Frames = 4500
)

# ---------------------------------------------------------------------------
# M34 openMSX A/B harness -- Metal Gear R#1 BL (display-enable) transitions
# (DEC-0043 Defect B; docs/m34-planner-package.md section 3.3 item 2).
#
# Side A (ours, captured by the recorded recipe): the scratchpad BL probe run
# over tools/metalgear-m34-bl-transition-input.script (SPACE 300/600 lineage
# holds; UP at 1150 aborts the attract demo; SPACE at 1250 starts a REAL game
# from the waiting title; RETURN taps 2700..3900 advance/close the intro
# transceiver call; UP held from 4100 walks Snake north off the beach-dock
# screen). Artifacts: debug/logs/m34-mg-bl-probe.txt (every R#1 bit6
# transition with frame + display line) and debug/frames/m34-mg-bl-f*.png.
# Our measured player-driven room transition: BL=0 from frame 4173 (line 11)
# to frame 4191 (line 189) -- an 18-frame blank while the game rewrites VRAM
# for the next room.
#
# Side B (real openMSX on WSL, /usr/bin/openmsx, Sony_HB-F1XV,
# power-cycle-fresh session): the SAME script transposed to frame-indexed
# `keymatrixdown`/`keymatrixup` injection (MSX matrix row 8 bit0 = SPACE, row 8 bit5 = UP,
# row 7 bit7 = RETURN -- the m32 Tcl-injection precedent), with R#1 polled
# via the debugger every frame (`debug read {VDP regs} 1`, an `after frame`
# tick). Output: a per-transition log of the reference's BL windows plus a
# raw screenshot taken inside the first long post-game-start blank window
# (throttle is briefly re-enabled for the screenshot -- the m32 finding that
# `screenshot -raw` returns stale frames under `set throttle off`).
#
# STRUCTURAL COMPARISON (three-outcome disposition pre-agreed in package
# section 3.3.3): does the reference show BL=0 windows of the same classes
# (game-load blank, transceiver open/close blanks, an ~18-frame walk
# transition blank) at the same trajectory points (within a small frame
# skew -- different BIOS-image timing tolerances between the two stacks)?
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path([string]$Path, [string]$Label) {
    if (-not (Test-Path $Path)) { throw "missing ${Label}: ${Path}" }
}
Require-Path $MgRom "Metal Gear ROM (local dev asset)"
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

$RepoRoot = (Get-Location).Path
$WslRoot = "/mnt/" + ($RepoRoot.Substring(0, 1).ToLower()) + ($RepoRoot.Substring(2) -replace "\\", "/")
$WslWork = "$WslRoot/$WorkDir"

$tcl = @"
set throttle off
set FRAME 0
set PREV_BL 1
set SHOT_TAKEN 0
set BLANK_START -1
set LOG [open "$WslWork/side_b_mg_bl.log" w]
puts `$LOG "# openMSX side-B R#1 BL transitions (frame-indexed keymatrix script)"
proc tick {} {
    global FRAME PREV_BL LOG SHOT_TAKEN BLANK_START
    incr FRAME
    # --- frame-indexed input (mirrors tools/metalgear-m34-bl-transition-input.script) ---
    if {`$FRAME == 300}  { keymatrixdown 8 0x01 }
    if {`$FRAME == 315}  { keymatrixup 8 0x01 }
    if {`$FRAME == 600}  { keymatrixdown 8 0x01 }
    if {`$FRAME == 615}  { keymatrixup 8 0x01 }
    if {`$FRAME == 1150} { keymatrixdown 8 0x20 }
    if {`$FRAME == 1250} { keymatrixdown 8 0x01 }
    if {`$FRAME == 1265} { keymatrixup 8 0x01 }
    if {`$FRAME == 1601} { keymatrixup 8 0x20 }
    foreach t {2700 3000 3300 3600 3900} {
        if {`$FRAME == `$t} { keymatrixdown 7 0x80 }
        if {`$FRAME == `$t + 13} { keymatrixup 7 0x80 }
    }
    if {`$FRAME == 4100} { keymatrixdown 8 0x20 }
    # --- R#1 poll ---
    set r1 [debug read {VDP regs} 1]
    set bl [expr {(`$r1 >> 6) & 1}]
    if {`$bl != `$PREV_BL} {
        puts `$LOG "BL frame=`$FRAME old=`$PREV_BL new=`$bl r1=[format 0x%02x `$r1]"
        flush `$LOG
        if {`$bl == 0} { set BLANK_START `$FRAME }
    }
    set PREV_BL `$bl
    # Screenshot inside the first LONG blank window after the game started
    # (>= 6 frames into a blank, past frame 3950 -- the walk transition).
    if {`$bl == 0 && `$BLANK_START > 0 && `$FRAME > 3950 && `$SHOT_TAKEN == 0 \
            && `$FRAME - `$BLANK_START >= 6} {
        set SHOT_TAKEN 1
        set throttle on
        after frame {
            catch { screenshot -raw "$WslWork/openmsx_mg_blank_window.png" }
            puts `$LOG "# screenshot taken at frame `$FRAME (inside blank window from `$BLANK_START)"
            flush `$LOG
            set throttle off
        }
    }
    if {`$FRAME >= $Frames} {
        puts `$LOG "# end at frame `$FRAME"
        close `$LOG
        exit
    }
    after frame tick
}
after frame tick
"@
Set-Content -Path "$WorkDir/side_b_mg_bl.tcl" -Value $tcl -Encoding ascii

Write-Host "== side B: openMSX Sony_HB-F1XV + Metal Gear via WSL (R#1 BL poll) =="
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
cmd /c "wsl.exe -- openmsx -machine Sony_HB-F1XV -carta `"$WslRoot/$MgRom`" -script `"$WslWork/side_b_mg_bl.tcl`" 2>nul" | Out-Null
$ErrorActionPreference = $prevEap

Require-Path "$WorkDir/side_b_mg_bl.log" "side-B log"
Write-Host "== side B R#1 BL transitions =="
Get-Content "$WorkDir/side_b_mg_bl.log" | ForEach-Object { Write-Host "  [side B] $_" }
Write-Host "== compare against side A: debug/logs/m34-mg-bl-probe.txt =="
Write-Host "(record the structural disposition in docs/m34-implementation-report.md)"
