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

$targetDll = Join-Path $SpotifyPath 'version.dll'
$backupDll = Join-Path $SpotifyPath 'version.dll.soggfy-backup'
$sourceLock = $null
$workerLock = $null
$retainWorkerForBackup = $false
try {
    if (Test-Path -LiteralPath $targetDll) {
        $packageDll = Join-Path $PSScriptRoot '../version.dll'
        if (-not (Test-Path -LiteralPath $packageDll -PathType Leaf)) {
            throw 'Soggfy uninstall: ownership cannot be verified. Use the complete package matching the installed version.'
        }
        if (-not ('Soggfy.ConversionSource' -as [type])) {
            Add-Type -Path (Join-Path $PSScriptRoot 'ConverterProcess.cs')
        }
        $sourceLock = New-Object Soggfy.ConversionSource($targetDll)
        if ($sourceLock.Length -eq 0 -or
            (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $packageDll -Algorithm SHA256).Hash) {
            throw 'Soggfy uninstall: version.dll does not match this package. It may belong to another tool or release; DLL, backup and settings were retained. Use the matching Soggfy package.'
        }
        . (Join-Path $PSScriptRoot 'ValidationWorker.ps1')
        $worker = Get-SoggfyValidationWorker (Split-Path -Parent $PSScriptRoot)
        $workerPath = Join-Path $SpotifyPath $worker.Name
        if (Test-Path -LiteralPath $workerPath) {
            $workerLock = New-Object Soggfy.ConversionSource($workerPath)
            Assert-SoggfyValidationWorker $workerPath $worker.Hash
        }
        # A restored DLL may share this helper build. Retain it when a backup
        # exists; removing rollback dependencies based on version is unsafe.
        $retainWorkerForBackup = Test-Path -LiteralPath $backupDll
    } elseif (Test-Path -LiteralPath $backupDll) {
        throw 'Soggfy uninstall: version.dll is missing, so backup ownership cannot be verified. Backup and settings were retained for manual recovery.'
    }

    $running = Get-Process -Name Spotify -ErrorAction SilentlyContinue
    if ($running) {
        if (-not $Force) {
            throw 'Soggfy uninstall: Spotify is running. Quit Spotify first, or rerun with -Force to close it.'
        }
        $running | Stop-Process -Force
        Start-Sleep -Milliseconds 500
    }

    if ($null -ne $sourceLock) {
        $sourceLock.DeleteValidatedSource()
        Write-Host "Removed $targetDll"
    }

    if (Test-Path -LiteralPath $backupDll -PathType Leaf) {
        Move-Item -LiteralPath $backupDll -Destination $targetDll
        Write-Host 'Restored the version.dll that existed before Soggfy.'
    }
    if ($null -ne $workerLock) {
        if ($retainWorkerForBackup) {
            Write-Host 'Retained the validation helper for the restored DLL.'
        } else {
            $workerLock.DeleteValidatedSource()
            Write-Host "Removed $workerPath"
        }
    }

    if ($RemoveSettings) {
        $ini = Join-Path $SpotifyPath 'SpotifyHistory.ini'
        if (Test-Path -LiteralPath $ini) {
            Remove-Item -LiteralPath $ini -Force
            Write-Host "Removed $ini"
        }
    }

    Write-Host 'Soggfy uninstalled. SpotX is managed separately and was not removed.'
} finally {
    if ($null -ne $sourceLock) { $sourceLock.Dispose() }
    if ($null -ne $workerLock) { $workerLock.Dispose() }
}
