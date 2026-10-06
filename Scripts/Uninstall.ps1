[CmdletBinding()]
param(
    [string]$SpotifyPath = (Join-Path $env:APPDATA 'Spotify'),
    [switch]$RemoveSettings,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$SpotifyPath = [Environment]::ExpandEnvironmentVariables($SpotifyPath)
$SpotifyExe = Join-Path $SpotifyPath 'Spotify.exe'
if (-not (Test-Path -LiteralPath $SpotifyExe -PathType Leaf)) {
    throw "Soggfy uninstall: Spotify.exe was not found at '$SpotifyPath'."
}

$running = Get-Process -Name Spotify -ErrorAction SilentlyContinue
if ($running) {
    if (-not $Force) {
        throw 'Soggfy uninstall: Spotify is running. Quit Spotify first, or rerun with -Force to close it.'
    }
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

$targetDll = Join-Path $SpotifyPath 'version.dll'
$backupDll = Join-Path $SpotifyPath 'version.dll.soggfy-backup'

if (Test-Path -LiteralPath $targetDll -PathType Leaf) {
    Remove-Item -LiteralPath $targetDll -Force
    Write-Host "Removed $targetDll"
}

if (Test-Path -LiteralPath $backupDll -PathType Leaf) {
    Move-Item -LiteralPath $backupDll -Destination $targetDll
    Write-Host 'Restored the version.dll that existed before Soggfy.'
}

if ($RemoveSettings) {
    $ini = Join-Path $SpotifyPath 'SpotifyHistory.ini'
    if (Test-Path -LiteralPath $ini) {
        Remove-Item -LiteralPath $ini -Force
        Write-Host "Removed $ini"
    }
}

Write-Host 'Soggfy uninstalled. SpotX is managed separately and was not removed.'
