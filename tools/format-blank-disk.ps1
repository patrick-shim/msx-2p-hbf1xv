# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator — Formatted-Blank 720 KB Disk Generator (M36-S-c)
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
    Deterministically writes a TRUE formatted-blank 720 KB (2DD) FAT12 disk
    image, the write target for a game DISK save (M36-S-c, DEC-0050).

.DESCRIPTION
    Produces a 737,280-byte image (80 cyl x 2 sides x 9 sectors x 512 bytes,
    media descriptor 0xF9) that is a genuinely EMPTY formatted floppy:
      * Boot sector + BPB at LBA 0 (the standard MSX 720 KB FAT12 geometry,
        matching src/devices/fdc/disk_image.cpp DiskImage::synthesize()'s BPB).
      * Two FAT copies at LBA 1 and LBA 4, each seeded with the media byte
        (0xF9 0xFF 0xFF) and otherwise zero.
      * Root directory + data area: ALL ZERO (an empty filesystem).

    NOTE this differs from DiskImage::synthesize()/gen-m16-boot-disk.py, which
    fill every sector with a deterministic NON-zero test pattern (for FDC
    read/write boundary tests). That pattern corrupts the root directory + data
    area, so it is NOT a valid empty formatted disk to save into. This tool
    produces the real empty-formatted medium a save can write files onto.

    PURE FUNCTION of constants only -- no wall-clock/host input feeds the bytes,
    so two runs produce a byte-identical file (auditable SHA256).

.PARAMETER OutFile
    Output .dsk path (required). Overwrites if present.

.EXAMPLE
    PS> tools/format-blank-disk.ps1 -OutFile disks/ys2_save.dsk
#>

param(
    [Parameter(Mandatory = $true)]
    [string]$OutFile
)

$ErrorActionPreference = "Stop"

$SectorSize      = 512
$SectorsPerTrack = 9
$Sides           = 2
$Tracks          = 80
$TotalSectors    = $SectorsPerTrack * $Sides * $Tracks   # 1440
$ImageBytes      = $TotalSectors * $SectorSize           # 737280
$MediaDescriptor = 0xF9

# Zero-initialized image (empty formatted filesystem).
$image = New-Object byte[] $ImageBytes

# --- Boot sector + BPB at LBA 0 (standard MSX 720 KB FAT12) ---
$image[0] = 0xEB   # JMP short
$image[1] = 0xFE
$image[2] = 0x90   # NOP
# OEM name "SONYMSX "
$oem = [System.Text.Encoding]::ASCII.GetBytes("SONYMSX ")
for ($i = 0; $i -lt 8; $i++) { $image[3 + $i] = $oem[$i] }
# BPB (offset 11):
$image[11] = 0x00  # bytes/sector = 512
$image[12] = 0x02
$image[13] = 0x02  # sectors/cluster = 2
$image[14] = 0x01  # reserved sectors = 1
$image[15] = 0x00
$image[16] = 0x02  # number of FATs = 2
$image[17] = 0x70  # root dir entries = 112
$image[18] = 0x00
$image[19] = 0xA0  # total sectors = 1440
$image[20] = 0x05
$image[21] = $MediaDescriptor
$image[22] = 0x03  # sectors/FAT = 3
$image[23] = 0x00
$image[24] = 0x09  # sectors/track = 9
$image[25] = 0x00
$image[26] = 0x02  # heads = 2
$image[27] = 0x00
$image[28] = 0x00  # hidden sectors = 0
$image[29] = 0x00
# Boot signature
$image[510] = 0x55
$image[511] = 0xAA

# --- FAT copies at LBA 1 and LBA 4 (1 reserved + 3 sectors/FAT) ---
foreach ($fatLba in @(1, 4)) {
    $base = $fatLba * $SectorSize
    $image[$base + 0] = $MediaDescriptor
    $image[$base + 1] = 0xFF
    $image[$base + 2] = 0xFF
    # rest of FAT stays zero (empty)
}

# Root directory (LBA 7..13) + data area: already zero.

$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

# Resolve to an absolute path so WriteAllBytes is CWD-independent. Combine the
# (now-existing) parent directory's absolute path with the leaf file name.
$absDir = if ($outDir) { (Resolve-Path $outDir).Path } else { (Get-Location).Path }
$absPath = Join-Path $absDir (Split-Path -Leaf $OutFile)
[System.IO.File]::WriteAllBytes($absPath, $image)

$sha256 = [System.BitConverter]::ToString(
    [System.Security.Cryptography.SHA256]::Create().ComputeHash($image)).Replace("-", "").ToLower()

Write-Host ("Wrote {0} ({1} bytes), sha256={2}" -f $absPath, $ImageBytes, $sha256)
