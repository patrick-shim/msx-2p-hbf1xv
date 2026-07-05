param(
    [string]$BiosDir = "bios",
    [string]$RomsDir = "roms",
    [string]$Algorithm = "SHA256",
    [switch]$Json,
    [string]$OutFile = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$validAlgorithms = @("SHA1", "SHA256", "SHA384", "SHA512", "MD5")
if ($validAlgorithms -notcontains $Algorithm.ToUpperInvariant()) {
    throw "Unsupported algorithm '$Algorithm'. Supported: $($validAlgorithms -join ', ')"
}

function Get-AssetChecksums {
    param(
        [string]$TargetDir,
        [string]$Label,
        [string]$HashAlgorithm
    )

    if (-not (Test-Path -LiteralPath $TargetDir)) {
        return [ordered]@{
            label = $Label
            directory = [System.IO.Path]::GetFullPath($TargetDir)
            exists = $false
            files = @()
        }
    }

    $fullDir = (Resolve-Path -LiteralPath $TargetDir).Path
    $items = Get-ChildItem -LiteralPath $fullDir -File -Filter "*.rom" | Sort-Object Name
    $files = @()

    foreach ($item in $items) {
        $hash = Get-FileHash -LiteralPath $item.FullName -Algorithm $HashAlgorithm
        $files += [ordered]@{
            name = $item.Name
            size = $item.Length
            algorithm = $hash.Algorithm
            checksum = $hash.Hash.ToLowerInvariant()
        }
    }

    return [ordered]@{
        label = $Label
        directory = $fullDir
        exists = $true
        files = $files
    }
}

$algo = $Algorithm.ToUpperInvariant()
$bios = Get-AssetChecksums -TargetDir $BiosDir -Label "bios" -HashAlgorithm $algo
$roms = Get-AssetChecksums -TargetDir $RomsDir -Label "roms" -HashAlgorithm $algo

$result = [ordered]@{
    generatedAt = (Get-Date -Format o)
    algorithm = $algo
    bios = $bios
    roms = $roms
}

if ($Json) {
    $output = $result | ConvertTo-Json -Depth 8
} else {
    $lines = @()
    $lines += "Asset checksum report"
    $lines += "Generated at: $($result.generatedAt)"
    $lines += "Algorithm: $($result.algorithm)"

    foreach ($section in @($bios, $roms)) {
        $lines += ""
        $lines += "[$($section.label)]"
        $lines += "Directory: $($section.directory)"
        $lines += "Exists:    $($section.exists)"
        $lines += "Files:     $($section.files.Count)"

        foreach ($file in $section.files) {
            $lines += "- $($file.name) | $($file.size) bytes | $($file.checksum)"
        }
    }

    $output = ($lines -join [Environment]::NewLine)
}

if ($OutFile -ne "") {
    $outPath = [System.IO.Path]::GetFullPath($OutFile)
    $outDir = Split-Path -Parent $outPath
    if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Path $outDir | Out-Null
    }
    Set-Content -LiteralPath $outPath -Value $output -Encoding UTF8
    Write-Output "Checksum report written to: $outPath"
} else {
    Write-Output $output
}
