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

    NOTE this differs from DiskImage::synthesize()/tools/gen/boot-disk.py, which
    fill every sector with a deterministic NON-zero test pattern (for FDC
    read/write boundary tests). That pattern corrupts the root directory + data
    area, so it is NOT a valid empty formatted disk to save into. This tool
    produces the real empty-formatted medium a save can write files onto.

    PURE FUNCTION of constants only -- no wall-clock/host input feeds the bytes,
    so two runs produce a byte-identical file (auditable SHA256).

    BOOT BEHAVIOUR: the default LBA-0 boot-code region (offset 0x1E onward) is
    ALL ZERO. That is correct for the tool's primary purpose -- a save TARGET
    inserted in a running machine -- but it means the disk is NOT bootable: the
    MSX DISK ROM always executes the boot sector at offset 0x1E, so an all-zero
    region runs as NOPs that slide into the ROM header (byte 0x4004 = 0x76, the
    HALT opcode) and stop with interrupts disabled -- a permanent DI:HALT. This
    is authentic hardware behaviour (openMSX halts byte-identically on the same
    image; verified in the M41 disk A/B), not an emulator bug. To use the blank
    as a BOOT disk (e.g. to reach Disk-BASIC), pass -BootSectorFrom to splice a
    real boot loader from a disk you already have.

.PARAMETER OutFile
    Output .dsk path (required). Overwrites if present.

.PARAMETER BootSectorFrom
    Optional path to an existing bootable MSX .dsk. When supplied, this tool
    splices that disk's boot-code region (LBA 0, offset 0x1E..0x1FF) over the
    zero region, yielding a bootable EMPTY disk: on boot the real loader finds
    no system files and drops cleanly to BASIC "Ok" with the disk commands
    available (verified vs openMSX in M41-S4). The BPB / FAT / root dir / data
    stay the tool's deterministic empty-disk bytes; only the boot loader is
    taken from the source. NO proprietary boot code is embedded in this tool --
    it is copied at run time from the disk you point at. The source's BPB
    geometry (0x0B..0x1D) must match this tool's 720 KB FAT12 layout or the
    loader may expect a different medium; a mismatch is warned about.

.EXAMPLE
    PS> tools/gen/format-blank-disk.ps1 -OutFile disks/game-save.dsk
    # Non-bootable empty save target (the default).

.EXAMPLE
    PS> tools/gen/format-blank-disk.ps1 -OutFile disks/blank_boot.dsk -BootSectorFrom disks/msxdos22.dsk
    # Bootable empty disk: boots to Disk-BASIC "Ok" with an empty filesystem.
#>

param(
    [Parameter(Mandatory = $true)]
    [string]$OutFile,

    [Parameter(Mandatory = $false)]
    [string]$BootSectorFrom
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

# --- Optional: splice a real boot loader to make the empty disk BOOTABLE ---
# Copies ONLY the source disk's boot-code region (LBA 0, offset 0x1E..0x1FF)
# over our zero region; the BPB/FAT/root/data stay this tool's deterministic
# empty-disk bytes. No proprietary boot code lives in this tool -- it is read at
# run time from the disk the caller points at (see .PARAMETER BootSectorFrom).
$bootable = $false
if ($BootSectorFrom) {
    if (-not (Test-Path -LiteralPath $BootSectorFrom)) {
        throw "BootSectorFrom not found: $BootSectorFrom"
    }
    $srcBoot = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $BootSectorFrom).Path)
    if ($srcBoot.Length -lt $SectorSize) {
        throw "BootSectorFrom is smaller than one 512-byte sector: $BootSectorFrom"
    }
    # Warn if the source BPB geometry (0x0B..0x1D) differs from our 720 KB FAT12
    # layout -- the spliced loader reads BPB fields and may expect a different
    # medium. Non-fatal: the caller may knowingly use a compatible variant.
    for ($b = 11; $b -le 29; $b++) {
        if ($srcBoot[$b] -ne $image[$b]) {
            Write-Warning ("BootSectorFrom BPB byte 0x{0:X2} = 0x{1:X2} differs from this tool's 0x{2:X2}; the boot loader may expect a different geometry." -f $b, $srcBoot[$b], $image[$b])
            break
        }
    }
    # Splice boot-code region 0x1E..0x1FF (indices 30..511). The JMP/OEM/BPB
    # (0x00..0x1D) stay ours; the MSX DISK ROM enters at 0x1E, so this is the
    # region that governs boot behaviour.
    for ($i = 30; $i -lt $SectorSize; $i++) {
        $image[$i] = $srcBoot[$i]
    }
    $bootable = $true
}

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

$bootNote = if ($bootable) {
    "bootable (loader spliced from $BootSectorFrom)"
} else {
    "non-bootable save-target (zero boot code; pass -BootSectorFrom for a boot disk)"
}
Write-Host ("Wrote {0} ({1} bytes), {2}, sha256={3}" -f $absPath, $ImageBytes, $bootNote, $sha256)
