# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator — Playtest Capture Wrapper (M36-S2)
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

<#
.SYNOPSIS
    M36-S2 Playtest frame-capture wrapper: runs sony_msx_headless with
    --debug-session, --input-script, and captures frame(s) via --dump-frame,
    then converts each frame dump to PNG via tools/convert/frame-to-png.py.

.DESCRIPTION
    Deterministic observe-act loop harness. Given:
      - Goal name (e.g. "boot_to_basic")
      - BIOS directory path
      - Optional disk image path
      - Optional input script file (starts from scratch if empty/missing)
      - Frame count to run (uses frame-loop shape for deterministic VBlank)

    This wrapper:
      1. Runs sony_msx_headless.exe --debug-session <bios> N \
         --frames <frame_count> --disk <disk> --input-script <script> \
         --dump-frame frame_<frameN>
      2. Converts each frame_N.txt dump to frame_N.png via tools/convert/frame-to-png.py
      3. Returns the output directory (ready for agent vision processing)

    Example:
      $result = & tools/capture/playtest.ps1 \
        -Goal "boot_to_basic" \
        -BiosDir bios \
        -Frames 100 \
        -InputScript "debug/playtest_boot.script"
      # Produces: debug/playtest_boot_to_basic/<timestamp>/frame_*.png

.PARAMETER Goal
    Human-readable goal name (e.g., "boot_to_basic", "game_title").
    Used in output directory path: debug/playtest_<goal>_<timestamp>/

.PARAMETER BiosDir
    Path to BIOS directory (required, passed to sony_msx_headless).

.PARAMETER Frames
    Number of frames to execute (in frame-loop shape, default 50).

.PARAMETER Disk
    Optional disk image path to attach via --disk flag.

.PARAMETER InputScript
    Optional input script file path. If provided and exists, passed via --input-script.
    If not provided or empty string, the emulator starts from scratch.

.PARAMETER OutputDir
    Optional override for the output directory root. Defaults to "debug/playtest_<goal>_<timestamp>".

.EXAMPLE
    # Boot to BASIC "Ok" prompt
    PS> & tools/capture/playtest.ps1 -Goal "boot_to_basic" -BiosDir bios -Frames 100

    # Boot a game disk and run for 30 frames
    PS> & tools/capture/playtest.ps1 -Goal "game_boot" -BiosDir bios -Disk "disks/games/<title>/game-d1.dsk" -Frames 30

    # Continue from an existing script
    PS> & tools/capture/playtest.ps1 -Goal "game_title" -BiosDir bios -Disk "disks/games/<title>/game-d1.dsk" \
          -InputScript "debug/playtest_game_boot_*.script" -Frames 50
#>

param(
    [Parameter(Mandatory = $true)]
    [string]$Goal,

    [Parameter(Mandatory = $true)]
    [string]$BiosDir,

    [int]$Frames = 50,

    [string]$Disk = "",

    [string]$InputScript = "",

    [string]$OutputDir = "",

    # M46 (DEC-0071): the emulator now defaults to convenience (512 KB RAM,
    # fast-disk on, FM-PAC auto-load). This wrapper passes --stock by default so
    # captures stay REPRODUCIBLE and match the pre-M46 golden behavior; pass
    # -Convenience to opt INTO the flipped defaults for a playtest that wants them.
    [switch]$Convenience
)

$ErrorActionPreference = "Stop"

# ============================================================================
# Utility: Find emulator executable
# ============================================================================

function Find-Emulator {
    $headless = "build/Debug/sony_msx_headless.exe"
    if (Test-Path $headless) {
        return (Resolve-Path $headless).Path
    }

    Write-Error "Emulator not found at $headless. Run cmake --build build --config Debug first."
    exit 1
}

# ============================================================================
# Utility: Timestamp for output directory
# ============================================================================

function Get-Timestamp {
    return (Get-Date -Format "yyyyMMdd_HHmmss")
}

# ============================================================================
# Utility: Run emulator and capture frames
# ============================================================================

function Invoke-PlaytestRun {
    param(
        [string]$Emulator,
        [string]$BiosDir,
        [int]$Frames,
        [string]$Disk,
        [string]$InputScript,
        [string]$OutputDir,
        [bool]$Convenience
    )

    # Build output directory for frame dumps
    $frameDir = Join-Path $OutputDir "frames_raw"
    New-Item -ItemType Directory -Path $frameDir -Force | Out-Null

    # Build sony_msx_headless command line. M46 (DEC-0071): pass --stock by
    # default for reproducible captures (the emulator's implicit default is now
    # convenience: 512 KB / fast-disk / FM-PAC slot 2); -Convenience opts in.
    $cmdArgs = @(
        "--debug-session",
        $BiosDir,
        "999999999"  # max_steps (unused when --frames is set)
    )
    if (-not $Convenience) {
        $cmdArgs += @("--stock")
    }
    $cmdArgs += @(
        "--frames", $Frames.ToString(),
        "--debug-root", $OutputDir,
        "--dump-frame", "frame_dump"
    )

    if ($Disk -and (Test-Path $Disk)) {
        $cmdArgs += @("--disk", $Disk)
    }

    if ($InputScript -and (Test-Path $InputScript)) {
        $cmdArgs += @("--input-script", $InputScript)
    }

    Write-Host "Running emulator..." -ForegroundColor Cyan
    Write-Host "Command: $Emulator $($cmdArgs -join ' ')"

    # The emulator writes its one-line diagnostic to STDERR. In PowerShell 5.1
    # NonInteractive, ANY in-process capture of a native command's stderr (a pipe
    # OR a 2> redirect) wraps each line in a NativeCommandError that TERMINATES
    # the run even on exit 0. Start-Process redirects stderr at the OS level, so
    # it never touches the PS error stream -- the robust fix.
    $emulatorLog = Join-Path $OutputDir "emulator.log"
    $emulatorOut = Join-Path $OutputDir "emulator.out"
    $proc = Start-Process -FilePath $Emulator -ArgumentList $cmdArgs `
        -NoNewWindow -Wait -PassThru `
        -RedirectStandardError $emulatorLog -RedirectStandardOutput $emulatorOut
    $exitCode = $proc.ExitCode
    if (Test-Path $emulatorLog) {
        Get-Content $emulatorLog | ForEach-Object { Write-Host "  [emu] $_" }
    }

    if ($exitCode -ne 0) {
        Write-Error "Emulator failed with exit code $exitCode"
        return $false
    }

    return $true
}

# ============================================================================
# Utility: Convert frame dumps to PNG
# ============================================================================

function Convert-FramesToPng {
    param(
        [string]$OutputDir
    )

    # Check both frame dump locations:
    # 1. <debug-root>/frames/ (where machine.write_frame_dump() writes)
    # 2. <debug-root>/frames_raw/ (alternative location for multi-frame captures)

    $frameDir = Join-Path $OutputDir "frames"
    $altFrameDir = Join-Path $OutputDir "frames_raw"

    if (-not (Test-Path $frameDir)) {
        if (Test-Path $altFrameDir) {
            $frameDir = $altFrameDir
        } else {
            Write-Error "No frame directory found (checked $frameDir and $altFrameDir)"
            return $false
        }
    }

    # Find all frame_dump* files (no extension expected)
    $frames = @(Get-ChildItem -Path $frameDir -Filter "frame_dump*" -ErrorAction SilentlyContinue)

    if ($frames.Count -eq 0) {
        Write-Error "No frame dump files found in $frameDir"
        return $false
    }

    $pngDir = Join-Path $OutputDir "frames_png"
    New-Item -ItemType Directory -Path $pngDir -Force | Out-Null

    Write-Host "Converting $($frames.Count) frame(s) to PNG..." -ForegroundColor Cyan

    $pythonScript = "tools/convert/frame-to-png.py"
    if (-not (Test-Path $pythonScript)) {
        Write-Error "tools/convert/frame-to-png.py not found at $pythonScript"
        return $false
    }

    $index = 0
    foreach ($frame in $frames | Sort-Object Name) {
        $pngPath = Join-Path $pngDir "frame_$($index.ToString('0000')).png"
        Write-Host "  Converting $($frame.Name) -> frame_$($index.ToString('0000')).png"

        python $pythonScript $frame.FullName -o $pngPath
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to convert $($frame.Name)"
            return $false
        }
        $index++
    }

    Write-Host "Converted $index frame(s) to PNG" -ForegroundColor Green
    return $true
}

# ============================================================================
# Main
# ============================================================================

function Main {
    # Find emulator
    $emulator = Find-Emulator

    # Resolve BIOS directory
    if (-not (Test-Path $BiosDir)) {
        Write-Error "BIOS directory not found: $BiosDir"
        exit 1
    }
    $BiosDir = (Resolve-Path $BiosDir).Path

    # Create output directory with timestamp
    if (-not $OutputDir) {
        $timestamp = Get-Timestamp
        $OutputDir = "debug/playtest_${Goal}_${timestamp}"
    }

    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    $OutputDir = (Resolve-Path $OutputDir).Path

    Write-Host "Playtest Capture: $Goal" -ForegroundColor Green
    Write-Host "  Output: $OutputDir"
    Write-Host "  BIOS: $BiosDir"
    Write-Host "  Frames: $Frames"
    if ($Disk) { Write-Host "  Disk: $Disk" }
    if ($InputScript) { Write-Host "  Input Script: $InputScript" }
    Write-Host ""

    # Run emulator
    if (-not (Invoke-PlaytestRun -Emulator $emulator -BiosDir $BiosDir -Frames $Frames `
              -Disk $Disk -InputScript $InputScript -OutputDir $OutputDir -Convenience:$Convenience)) {
        exit 1
    }

    # Convert frames to PNG
    if (-not (Convert-FramesToPng -OutputDir $OutputDir)) {
        exit 1
    }

    Write-Host "Playtest complete: $OutputDir/frames" -ForegroundColor Green
    Write-Output $OutputDir
}

Main
