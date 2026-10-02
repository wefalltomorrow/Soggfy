[CmdletBinding()]
param(
    [string]$SpotifyPath = (Join-Path $env:APPDATA 'Spotify'),
    [switch]$RunSpotX,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Fail([string]$Message) {
    throw "Soggfy install: $Message"
}

$SpotifyPath = [Environment]::ExpandEnvironmentVariables($SpotifyPath)
$SpotifyExe = Join-Path $SpotifyPath 'Spotify.exe'
if (-not (Test-Path -LiteralPath $SpotifyExe -PathType Leaf)) {
    Fail "Spotify.exe was not found at '$SpotifyPath'. Microsoft Store installs are not currently supported."
}

$running = Get-Process -Name Spotify -ErrorAction SilentlyContinue
if ($running) {
    if (-not $Force) {
        Fail 'Spotify is running. Quit Spotify first, or rerun with -Force to close it.'
    }
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
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

if ($RunSpotX) {
    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

    $spotxUrl = 'https://spotx-official.github.io/SpotX/run.ps1'
    $spotxScript = Join-Path ([IO.Path]::GetTempPath()) 'soggfy-spotx-run.ps1'
    Invoke-WebRequest -UseBasicParsing -Uri $spotxUrl -OutFile $spotxScript
    Write-Host 'Running the explicitly requested SpotX installer...'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $spotxScript
    if ($LASTEXITCODE -ne 0) {
        Fail "SpotX exited with code $LASTEXITCODE. Soggfy itself is already installed."
    }
}

Write-Host 'Done. Start Spotify, open To Disk, and enable Downloads.'
