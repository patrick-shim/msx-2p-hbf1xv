param(
    [string]$CartridgeRom = "roms/aleste.rom",
    [string]$Emulator = "build/Debug/sony_msx_headless.exe",
    [string]$OutDoc = "docs/m30-identification-ab.md",
    [string]$ProbeMachine = "Sony_HB-F1XV",
    [string]$ExpectedSha1 = "e93d0840c59c6eba273df546d22148d486a150a6",
    [int]$BootSeconds = 4
)

# ---------------------------------------------------------------------------
# M30-S6 openMSX A/B check -- cartridge AUTO-IDENTIFICATION agreement
# (backlog G2, docs/m30-planner-package.md section 2.5).
#
# Identification is frontend/composition policy with no hardware analogue, so
# per-register A/B would be fabricated relevance (planner section 2.5). The ONE
# meaningful check performed here: BOTH emulators, given the SAME
# roms/aleste.rom with NO type forcing, must land on the SAME mapper
# identification (KonamiSCC).
#
#   - Side A (this emulator): `sony_msx_headless --cart1 <rom>` with NO
#     --cart1-type -> exit 0 + the verbatim message-A stderr line naming
#     KonamiSCC (resolved via our own softwaredb SHA1 lookup + parser).
#   - Side B (openMSX on WSL): `-carta <rom>` with NO -romtype -> openMSX
#     resolves its "auto" mappertype via ITS OWN software database
#     (references/openmsx-21.0/src/memory/RomFactory.cc:176-201) and writes
#     the RESOLVED type back into the device config
#     (RomFactory.cc:212-219). We query it live over Tcl.
#
# R-M30-6 precondition -- the Side-B Tcl query mechanism VERIFIED IN SOURCE
# FIRST (the A-M29-3 syntax-verification precedent):
#   - references/openmsx-21.0/src/MSXMotherBoard.cc:1227-1252 -- the
#     `machine_info device` InfoTopic: with a device-name argument it calls
#     device->getDeviceInfo(result).
#   - references/openmsx-21.0/src/memory/MSXRom.cc:56-63 -- MSXRom overrides
#     getExtraDeviceInfo -> getInfo, which addDictKeyValues "mappertype"
#     (the RESOLVED type, per the comment at MSXRom.cc:26-27: "'auto' is
#     already changed to actual type") plus "actualSHA1" -- the SHA1 key we
#     use to select the cartridge device among all MSXRom devices (the
#     system BIOS/SubROM/Kanji ROMs are MSXRom too and also report a
#     mappertype).
#   - references/openmsx-21.0/src/config/HardwareConfig.cc:115-122 -- with
#     no -romtype option the generated cart config's <mappertype> is "auto".
#   - Live-verified against the installed WSL openMSX before scripting
#     (developer probe, recorded in the artifact below).
#
# Fallback (planner section 2.5): if the Tcl query is genuinely unavailable, a
# loader-banner screenshot comparison is the sanctioned fallback; if WSL
# openMSX itself is unavailable (contra A-M30-4), this records BLOCKED with
# the concrete failure output -- never fabricated.
# Exit codes: 0 = AGREEMENT, 1 = DIVERGENCE, 2 = BLOCKED.
# ---------------------------------------------------------------------------

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
}

Require-Path -Path $Emulator -Label "emulator binary (build first)"
Require-Path -Path $CartridgeRom -Label "cartridge ROM (local dev asset)"

$openmsxPath = (wsl -e bash -lc "command -v openmsx || true").Trim()
$openmsxVersion = ""
$blocked = $false
$blockedReason = ""
if ([string]::IsNullOrWhiteSpace($openmsxPath)) {
    $blocked = $true
    $blockedReason = "openMSX not found in WSL PATH (A-M30-4 unavailable at evidence time)"
} else {
    $openmsxVersion = (wsl -e bash -lc "openmsx --version 2>/dev/null || true").Trim()
}
Write-Output "openMSX: $openmsxPath ($openmsxVersion), machine: $ProbeMachine"

# =====================================================================
# Side A: this emulator, NO --cart1-type.
# =====================================================================
# (2>&1 turns native stderr into ErrorRecords; relax EAP around the call so
# the identification report lines -- which ARE stderr by design -- don't
# trip Set-StrictMode/Stop, then stringify them.)
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$sideAOutput = & $Emulator --cart1 $CartridgeRom 2>&1 | ForEach-Object { "$_" }
$sideAExit = $LASTEXITCODE
$ErrorActionPreference = $prevEap
$sideAIdentLine = $sideAOutput | Where-Object { $_ -match "identified as" } | Select-Object -First 1
$sideAType = ""
if ($sideAIdentLine -and $sideAIdentLine -match 'identified as "(?<title>[^"]*)" \((?<type>[^)]*)\) via (?<how>softwaredb SHA1 match|ROM format signature)') {
    $sideAType = $Matches['type']
    $sideATitle = $Matches['title']
    $sideAHow = $Matches['how']
} else {
    $sideATitle = ""
    $sideAHow = ""
}
Write-Output "Side A exit=$sideAExit type='$sideAType' title='$sideATitle' via '$sideAHow'"
Write-Output "Side A line: $sideAIdentLine"

# =====================================================================
# Side B: openMSX (WSL), -carta with NO -romtype; live Tcl query of the
# RESOLVED mappertype, cartridge device selected by actualSHA1.
# =====================================================================
$sideBType = ""
$sideBDevice = ""
$rawLines = @()
if (-not $blocked) {
    $cartRomAbs = (Resolve-Path -LiteralPath $CartridgeRom).Path
    $cartRomWsl = "/mnt/" + $cartRomAbs.Substring(0,1).ToLower() + $cartRomAbs.Substring(2).Replace('\','/')

    $tcl = @"
after time $BootSeconds {
  if {[catch {
    foreach d [machine_info device] {
      if {[catch {set info [machine_info device `$d]} e]} { continue }
      if {[dict exists `$info mappertype] && [dict exists `$info actualSHA1]} {
        puts stderr "OMIDENT mappertype=[dict get `$info mappertype] sha1=[dict get `$info actualSHA1] device=`$d"
      }
    }
  } err]} { puts stderr "OMIDENT ERROR msg=`$err" }
  exit
}
"@
    $encoded = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($tcl))
    $tclPathWsl = "/tmp/omx_m30_ident.tcl"
    $runCmd = "echo $encoded | base64 -d > $tclPathWsl && " +
              "timeout 120 openmsx -machine $ProbeMachine -carta '$cartRomWsl' " +
              "-command 'set renderer none' -script $tclPathWsl 2>&1 | grep -a OMIDENT || true"
    $raw = wsl -e bash -lc $runCmd
    if ($raw) { $rawLines = @($raw -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }) }
    foreach ($line in $rawLines) {
        if ($line -match "OMIDENT mappertype=(?<type>\S+) sha1=(?<sha>[0-9a-fA-F]{40}) device=(?<dev>.*)$") {
            if ($Matches['sha'].ToLower() -eq $ExpectedSha1.ToLower()) {
                $sideBType = $Matches['type']
                $sideBDevice = $Matches['dev']
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($sideBType)) {
        $blocked = $true
        $blockedReason = "openMSX ran but no OMIDENT line matched the cartridge SHA1 $ExpectedSha1 (raw: $($rawLines -join ' | '))"
    }
}
Write-Output "Side B type='$sideBType' device='$sideBDevice'"

# =====================================================================
# Verdict.
# =====================================================================
$verdict = "BLOCKED"
$exitCode = 2
if (-not $blocked) {
    if (($sideAExit -eq 0) -and ($sideAType -ne "") -and ($sideAType -eq $sideBType)) {
        $verdict = "AGREEMENT"
        $exitCode = 0
    } else {
        $verdict = "DIVERGENCE"
        $exitCode = 1
    }
}
Write-Output "RESULT: $verdict (A='$sideAType', B='$sideBType')"

# =====================================================================
# Compose the artifact.
# =====================================================================
$lines = @()
$lines += "# M30-S6 openMSX A/B Check -- Cartridge Auto-Identification Agreement"
$lines += ""
$lines += "- Milestone: M30 (universal cartridge auto-identification, backlog G2), slice S6."
$lines += "- Subject-A emulator: ``sony_msx_headless`` (this repo, Debug build)."
$lines += "- Reference-B emulator: $openmsxVersion (WSL, ``$openmsxPath``), machine ``$ProbeMachine``."
$lines += "- ROM under test: ``$CartridgeRom`` (local dev asset; SHA1 ``$ExpectedSha1``)."
$lines += "- NEITHER side forces a mapper type: Side A has NO ``--cart1-type``; Side B has NO ``-romtype``."
$lines += ""
$lines += "## Why this is the ONE meaningful A/B check (planner section 2.5)"
$lines += ""
$lines += "Identification is frontend/composition POLICY (a pure function of file bytes + database"
$lines += "bytes), not device behavior -- there is no hardware register to compare, so per-register"
$lines += "A/B would be fabricated relevance. The meaningful cross-emulator fact is the RESOLVED"
$lines += "mapper type for the same input file with no forcing on either side."
$lines += ""
$lines += "## R-M30-6: Side-B Tcl query mechanism -- VERIFIED IN SOURCE FIRST"
$lines += ""
$lines += "- ``references/openmsx-21.0/src/MSXMotherBoard.cc:1227-1252`` -- ``machine_info device <name>`` -> ``device->getDeviceInfo(result)``."
$lines += "- ``references/openmsx-21.0/src/memory/MSXRom.cc:56-63`` -- ``getExtraDeviceInfo`` -> ``getInfo`` -> dict keys ``mappertype`` (the RESOLVED type; MSXRom.cc:26-27: *'auto' is already changed to actual type*) + ``actualSHA1`` (our cartridge-device selector -- system BIOS/SubROM/Kanji ROMs are MSXRom devices too and also report a mappertype)."
$lines += "- ``references/openmsx-21.0/src/config/HardwareConfig.cc:115-122`` -- with no ``-romtype``, the generated cart config's ``<mappertype>`` is ``auto``."
$lines += "- ``references/openmsx-21.0/src/memory/RomFactory.cc:176-219`` -- ``auto`` resolves via openMSX's own softwaredb SHA1 lookup, and the RESOLVED type is written back into the config the query reads."
$lines += ""
$lines += "## Side A (this emulator, NO --cart1-type)"
$lines += ""
$lines += '```'
$lines += "`$ $Emulator --cart1 $CartridgeRom"
foreach ($l in $sideAOutput) { $lines += "$l" }
$lines += "exit code: $sideAExit"
$lines += '```'
$lines += ""
$lines += "## Side B (openMSX WSL, -carta with NO -romtype; live Tcl query)"
$lines += ""
$lines += '```'
foreach ($l in $rawLines) { $lines += "$l" }
$lines += '```'
$lines += ""
$lines += "Cartridge device selected by actualSHA1 == ``$ExpectedSha1``: device ``$sideBDevice``, mappertype ``$sideBType``."
$lines += ""
$lines += "## Result"
$lines += ""
switch ($verdict) {
    "AGREEMENT" {
        $lines += "- **Result: AGREEMENT.** Side A resolved **$sideAType** (via $sideAHow, title ``$sideATitle``); Side B resolved **$sideBType** (openMSX's own softwaredb ``auto`` resolution). Both sides identify the same file as the same mapper type with zero forcing."
    }
    "DIVERGENCE" {
        $lines += "- **Result: DIVERGENCE.** Side A: exit=$sideAExit type='$sideAType'; Side B: type='$sideBType'. See raw outputs above."
    }
    default {
        $lines += "- **Result: BLOCKED.** $blockedReason"
    }
}
$lines += ""
$lines += "## Reproduce"
$lines += ""
$lines += '```powershell'
$lines += "cmake --build build --config Debug"
$lines += "tools/openmsx-m30-identification-ab.ps1"
$lines += '```'
$lines += ""

$outFull = [System.IO.Path]::GetFullPath($OutDoc)
$outDir = Split-Path -Parent $outFull
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
Set-Content -LiteralPath $OutDoc -Value ($lines -join "`n") -Encoding UTF8

Write-Output ""
Write-Output "Artifact written to $OutDoc"
exit $exitCode
