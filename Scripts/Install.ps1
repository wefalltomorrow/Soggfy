[CmdletBinding()]
param(
    [string]$SpotifyPath = (Join-Path $env:APPDATA 'Spotify'),
    [switch]$RunSpotX,
    [switch]$SkipSpotX,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$PinnedSpotifyVersion = '1.3.1.234'
$PinnedSpotifyFullVersion = '1.3.1.234.g59d6bf59'

function Fail([string]$Message) {
    throw "Soggfy install: $Message"
}

if ($RunSpotX -and $SkipSpotX) {
    Fail 'Use either -RunSpotX or -SkipSpotX, not both.'
}

$SpotifyPath = [Environment]::ExpandEnvironmentVariables($SpotifyPath)
$SpotifyExe = Join-Path $SpotifyPath 'Spotify.exe'
if (-not (Test-Path -LiteralPath $SpotifyExe -PathType Leaf)) {
    Fail "Spotify.exe was not found at '$SpotifyPath'. Microsoft Store installs are not currently supported."
}

$installedVersion = (Get-Item -LiteralPath $SpotifyExe).VersionInfo.FileVersion
if ([string]::IsNullOrWhiteSpace($installedVersion) -or
    -not $installedVersion.StartsWith($PinnedSpotifyVersion, [StringComparison]::OrdinalIgnoreCase)) {
    Fail "RC40 targets Spotify $PinnedSpotifyFullVersion x64. Installed Spotify reports '$installedVersion'. Install the pinned Spotify build first."
}
Write-Host "Spotify $installedVersion detected (RC40 baseline)."

$running = Get-Process -Name Spotify -ErrorAction SilentlyContinue
if ($running) {
    if (-not $Force) {
        Fail 'Spotify is running. Quit Spotify first, or rerun with -Force to close it.'
    }
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

$installSpotX = $RunSpotX.IsPresent
if (-not $RunSpotX -and -not $SkipSpotX) {
    do {
        $answer = Read-Host 'Install/update SpotX for ad blocking and Spotify update blocking? [Y/n]'
        if ([string]::IsNullOrWhiteSpace($answer)) {
            $installSpotX = $true
            break
        }
        if ($answer -match '^(?i:y|yes)$') {
            $installSpotX = $true
            break
        }
        if ($answer -match '^(?i:n|no)$') {
            $installSpotX = $false
            break
        }
        Write-Host 'Please answer Y or N.'
    } while ($true)
}

if ($installSpotX) {
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

    $spotxUrl = 'https://raw.githubusercontent.com/SpotX-Official/SpotX/refs/heads/main/run.ps1'
    $spotxScript = Join-Path ([IO.Path]::GetTempPath()) 'soggfy-spotx-run.ps1'

    Write-Host "Downloading SpotX..."
    Invoke-WebRequest -UseBasicParsing -Uri $spotxUrl -OutFile $spotxScript

    try {
        Write-Host "Running SpotX: ad blocking enabled, Spotify updates blocked, client pinned to $PinnedSpotifyFullVersion..."
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $spotxScript `
            -version $PinnedSpotifyFullVersion `
            -SpotifyPath $SpotifyPath `
            -block_update_on `
            -no_pause

        if ($LASTEXITCODE -ne 0) {
            Fail "SpotX exited with code $LASTEXITCODE. Soggfy was not installed."
        }
    }
    finally {
        Remove-Item -LiteralPath $spotxScript -Force -ErrorAction SilentlyContinue
    }
}
else {
    Write-Warning 'SpotX was skipped. Soggfy RC40 does not block ads or Spotify updates itself; only telemetry blocking remains native.'
}

$packageRoot = Split-Path -Parent $PSScriptRoot
$sourceDll = Join-Path $packageRoot 'version.dll'
$sourceIni = Join-Path $packageRoot 'SpotifyHistory.ini'
if (-not (Test-Path -LiteralPath $sourceDll -PathType Leaf)) {
    Fail "version.dll is missing from '$packageRoot'. Extract the full release ZIP first."
}

$targetDll = Join-Path $SpotifyPath 'version.dll'
$backupDll = Join-Path $SpotifyPath 'version.dll.soggfy-backup'

$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceDll).Hash
if (Test-Path -LiteralPath $targetDll -PathType Leaf) {
    $targetHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetDll).Hash
    if ($targetHash -ne $sourceHash) {
        if (Test-Path -LiteralPath $backupDll -PathType Leaf) {
            if (-not $Force) {
                Fail "A backup already exists at '$backupDll'. Move/remove it or rerun with -Force if you know it is safe."
            }
        } else {
            Move-Item -LiteralPath $targetDll -Destination $backupDll
            Write-Host "Backed up existing version.dll -> $backupDll"
        }
    }
}

Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
Write-Host "Installed Soggfy -> $targetDll"

$targetIni = Join-Path $SpotifyPath 'SpotifyHistory.ini'
if (-not (Test-Path -LiteralPath $targetIni) -and (Test-Path -LiteralPath $sourceIni)) {
    Copy-Item -LiteralPath $sourceIni -Destination $targetIni
    Write-Host "Created $targetIni"
}

if ($installSpotX) {
    Write-Host 'Done. SpotX handles ad/update blocking; Soggfy keeps its separate telemetry blocker.'
}
else {
    Write-Host 'Done. Soggfy installed without SpotX.'
}
Write-Host 'Start Spotify and use the Soggfy Downloads button in the top bar.'
