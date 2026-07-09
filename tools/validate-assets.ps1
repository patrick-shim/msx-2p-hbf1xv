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
    [string]$BiosDir = "bios",
    [string]$RomsDir = "roms",
    [switch]$Json
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$requiredBios = @(
    "f1xvbios.rom",
    "f1xvdisk.rom",
    "f1xvext.rom",
    "f1xvfirm.rom",
    "f1xvkdr.rom",
    "f1xvkfn.rom",
    "f1xvmus.rom"
)

function Resolve-RepoPath {
    param([string]$Path)

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if ($null -ne $resolved) {
        return $resolved.Path
    }

    return [System.IO.Path]::GetFullPath($Path)
}

$biosPath = Resolve-RepoPath -Path $BiosDir
$romsPath = Resolve-RepoPath -Path $RomsDir

$missingBios = @()
$presentBios = @()

foreach ($name in $requiredBios) {
    $candidate = Join-Path $biosPath $name
    if (Test-Path -LiteralPath $candidate) {
        $presentBios += $name
    } else {
        $missingBios += $name
    }
}

$romFiles = @()
if (Test-Path -LiteralPath $romsPath) {
    $romFiles = @(
        Get-ChildItem -LiteralPath $romsPath -File -Filter "*.rom" |
            Select-Object -ExpandProperty Name
    )
}

$hasAtLeastOneRom = $romFiles.Count -gt 0
$ok = ($missingBios.Count -eq 0) -and $hasAtLeastOneRom

$result = [ordered]@{
    ok = $ok
    biosDir = $biosPath
    romsDir = $romsPath
    presentBios = $presentBios
    missingBios = $missingBios
    romCount = $romFiles.Count
    romFiles = $romFiles
}

if ($Json) {
    $result | ConvertTo-Json -Depth 5
} else {
    Write-Output "Asset validation result: $($result.ok)"
    Write-Output "BIOS directory: $($result.biosDir)"
    Write-Output "ROM directory:  $($result.romsDir)"

    if ($result.presentBios.Count -gt 0) {
        Write-Output "Present BIOS files:"
        foreach ($name in $result.presentBios) {
            Write-Output "  - $name"
        }
    }

    if ($result.missingBios.Count -gt 0) {
        Write-Output "Missing BIOS files:"
        foreach ($name in $result.missingBios) {
            Write-Output "  - $name"
        }
    }

    Write-Output "ROM file count: $($result.romCount)"
    if ($result.romFiles.Count -gt 0) {
        Write-Output "ROM files:"
        foreach ($name in $result.romFiles) {
            Write-Output "  - $name"
        }
    }
}

if (-not $ok) {
    exit 1
}

exit 0
