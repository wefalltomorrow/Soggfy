[CmdletBinding()]
param(
    [string]$SpotifyPath = (Join-Path $env:APPDATA 'Spotify'),
    [string]$OutputPath = (Join-Path ([Environment]::GetFolderPath('UserProfile')) ('Downloads\Soggfy-Diagnostics-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.txt'))
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$SpotifyPath = [Environment]::ExpandEnvironmentVariables($SpotifyPath)
$SpotifyExe = Join-Path $SpotifyPath 'Spotify.exe'
$Dll = Join-Path $SpotifyPath 'version.dll'
$Ini = Join-Path $SpotifyPath 'SpotifyHistory.ini'
$Backup = Join-Path $SpotifyPath 'version.dll.soggfy-backup'

$report = [System.Collections.Generic.List[string]]::new()
function Add-Line([string]$Text = '') { $report.Add($Text) }

Add-Line 'Soggfy diagnostics'
Add-Line ('Generated: ' + (Get-Date -Format o))
Add-Line ('Windows: ' + [Environment]::OSVersion.VersionString)
Add-Line ('PowerShell: ' + $PSVersionTable.PSVersion)
Add-Line ('Spotify path: ' + $SpotifyPath)
Add-Line ''

if (Test-Path -LiteralPath $SpotifyExe -PathType Leaf) {
    $spotify = Get-Item -LiteralPath $SpotifyExe
    Add-Line ('Spotify.exe: present')
    Add-Line ('Spotify file version: ' + $spotify.VersionInfo.FileVersion)
    Add-Line ('Spotify product version: ' + $spotify.VersionInfo.ProductVersion)
} else {
    Add-Line 'Spotify.exe: MISSING'
}

if (Test-Path -LiteralPath $Dll -PathType Leaf) {
    Add-Line ('version.dll: present')
    Add-Line ('version.dll SHA256: ' + (Get-FileHash -Algorithm SHA256 -LiteralPath $Dll).Hash)
    Add-Line ('version.dll bytes: ' + (Get-Item -LiteralPath $Dll).Length)
} else {
    Add-Line 'version.dll: MISSING'
}
Add-Line ('pre-Soggfy version.dll backup: ' + [bool](Test-Path -LiteralPath $Backup -PathType Leaf))
Add-Line ''

$saveRoot = $null
if (Test-Path -LiteralPath $Ini -PathType Leaf) {
    Add-Line 'SpotifyHistory.ini:'
    $iniLines = Get-Content -LiteralPath $Ini
    foreach ($line in $iniLines) { Add-Line ('  ' + $line) }

    $saveLine = $iniLines | Where-Object { $_ -match '^\s*Save Location\s*=' } | Select-Object -First 1
    if ($saveLine) {
        $raw = ($saveLine -split '=',2)[1].Trim()
        if ($raw) { $saveRoot = [Environment]::ExpandEnvironmentVariables($raw) }
    }
} else {
    Add-Line 'SpotifyHistory.ini: MISSING'
}
Add-Line ''

if (-not $saveRoot) {
    $music = [Environment]::GetFolderPath('MyMusic')
    if ($music) { $saveRoot = Join-Path $music 'Spotify' }
}
if ($saveRoot) {
    Add-Line ('Resolved save root: ' + $saveRoot)
    $logPath = Join-Path $saveRoot 'Soggfy.log'
    if (Test-Path -LiteralPath $logPath -PathType Leaf) {
        Add-Line ('Log: ' + $logPath)
        Add-Line 'Last 80 log lines:'
        Get-Content -LiteralPath $logPath -Tail 80 | ForEach-Object { Add-Line ('  ' + $_) }
    } else {
        Add-Line ('Log not found: ' + $logPath)
    }
}

$parent = Split-Path -Parent $OutputPath
if ($parent -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}
$report | Set-Content -LiteralPath $OutputPath -Encoding UTF8
Write-Host "Diagnostics written to $OutputPath"
