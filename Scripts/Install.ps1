[CmdletBinding()]
param(
    [string]$SpotifyPath = (Join-Path $env:APPDATA 'Spotify'),
    [switch]$RunSpotX,
    [switch]$SkipSpotX,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$InstallerVersion = 'RC45'
$PinnedSpotifyVersion = '1.3.1.234'
$PinnedSpotifyFullVersion = '1.3.1.234.g59d6bf59'

function Fail([string]$Message) {
    throw "Soggfy install: $Message"
}

function Test-XpuiArchive([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = $null
    try {
        $archive = [IO.Compression.ZipFile]::OpenRead($Path)
        $index = $archive.GetEntry('index.html')
        $xpui = $archive.GetEntry('xpui.js')
        $snapshot = $archive.GetEntry('xpui-snapshot.js')
        return ($null -ne $index -and ($null -ne $xpui -or $null -ne $snapshot))
    }
    catch {
        return $false
    }
    finally {
        if ($null -ne $archive) {
            $archive.Dispose()
        }
    }
}

function Test-SpotXMarker([string]$Path) {
    if (-not (Test-XpuiArchive $Path)) {
        return $false
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = $null
    $reader = $null
    try {
        $archive = [IO.Compression.ZipFile]::OpenRead($Path)
        $entry = $archive.GetEntry('xpui.js')
        if ($null -eq $entry) {
            return $false
        }
        $reader = [IO.StreamReader]::new($entry.Open())
        return $reader.ReadToEnd().IndexOf('patched by spotx', [StringComparison]::OrdinalIgnoreCase) -ge 0
    }
    catch {
        return $false
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        if ($null -ne $archive) {
            $archive.Dispose()
        }
    }
}

function Backup-SpotXFiles([string]$Root, [string]$BackupRoot) {
    $relativeFiles = @(
        'Spotify.exe',
        'Spotify.dll',
        'chrome_elf.dll',
        'Apps\xpui.spa'
    )

    New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
    foreach ($relative in $relativeFiles) {
        $source = Join-Path $Root $relative
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            continue
        }
        $destination = Join-Path $BackupRoot $relative
        $parent = Split-Path -Parent $destination
        if ($parent) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }
}

function Restore-SpotXFiles([string]$Root, [string]$BackupRoot) {
    $relativeFiles = @(
        'Spotify.exe',
        'Spotify.dll',
        'chrome_elf.dll',
        'Apps\xpui.spa'
    )

    foreach ($relative in $relativeFiles) {
        $backup = Join-Path $BackupRoot $relative
        if (-not (Test-Path -LiteralPath $backup -PathType Leaf)) {
            continue
        }
        $destination = Join-Path $Root $relative
        $parent = Split-Path -Parent $destination
        if ($parent) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        Copy-Item -LiteralPath $backup -Destination $destination -Force
    }
}

if ($RunSpotX -and $SkipSpotX) {
    Fail 'Use either -RunSpotX or -SkipSpotX, not both.'
}

$SpotifyPath = [Environment]::ExpandEnvironmentVariables($SpotifyPath)
$SpotifyPath = [IO.Path]::GetFullPath($SpotifyPath).TrimEnd('\')
$defaultSpotifyPath = [IO.Path]::GetFullPath((Join-Path $env:APPDATA 'Spotify')).TrimEnd('\')
$isDefaultSpotifyPath = [string]::Equals(
    $SpotifyPath,
    $defaultSpotifyPath,
    [StringComparison]::OrdinalIgnoreCase
)

$SpotifyExe = Join-Path $SpotifyPath 'Spotify.exe'
if (-not (Test-Path -LiteralPath $SpotifyExe -PathType Leaf)) {
    Fail "Spotify.exe was not found at '$SpotifyPath'. Microsoft Store installs are not currently supported."
}

$installedVersion = (Get-Item -LiteralPath $SpotifyExe).VersionInfo.FileVersion
if ([string]::IsNullOrWhiteSpace($installedVersion) -or
    -not $installedVersion.StartsWith($PinnedSpotifyVersion, [StringComparison]::OrdinalIgnoreCase)) {
    Fail "$InstallerVersion targets Spotify $PinnedSpotifyFullVersion x64. Installed Spotify reports '$installedVersion'. Install the pinned Spotify build first."
}
Write-Host "Spotify $installedVersion detected ($InstallerVersion baseline)."

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
    $xpuiSpa = Join-Path $SpotifyPath 'Apps\xpui.spa'
    $xpuiBak = Join-Path $SpotifyPath 'Apps\xpui.bak'

    if (-not (Test-XpuiArchive $xpuiSpa)) {
        if (Test-XpuiArchive $xpuiBak) {
            Write-Warning 'Spotify Apps\xpui.spa is damaged. Restoring the valid SpotX xpui.bak before continuing.'
            Copy-Item -LiteralPath $xpuiBak -Destination $xpuiSpa -Force
        }
        else {
            Fail "Spotify Apps\xpui.spa is not a valid Spotify archive and no valid Apps\xpui.bak is available. Repair/reinstall Spotify $PinnedSpotifyFullVersion first. Soggfy and SpotX were not changed."
        }
    }

    [Net.ServicePointManager]::SecurityProtocol =
        [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

    $spotxUrl = 'https://raw.githubusercontent.com/SpotX-Official/SpotX/refs/heads/main/run.ps1'
    $spotxScript = Join-Path ([IO.Path]::GetTempPath()) 'soggfy-spotx-run.ps1'
    $spotxBackup = Join-Path ([IO.Path]::GetTempPath()) ('soggfy-spotx-backup-' + [Guid]::NewGuid().ToString('N'))

    Write-Host 'Downloading SpotX...'
    Invoke-WebRequest -UseBasicParsing -Uri $spotxUrl -OutFile $spotxScript
    Backup-SpotXFiles -Root $SpotifyPath -BackupRoot $spotxBackup

    $spotxSucceeded = $false
    try {
        Write-Host "Running SpotX: ad blocking enabled, Spotify updates blocked, client pinned to $PinnedSpotifyFullVersion..."

        $spotxArgs = @(
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $spotxScript,
            '-version', $PinnedSpotifyFullVersion,
            '-block_update_on',
            '-podcasts_on',
            '-defender_exclusions_off',
            '-language', 'en',
            '-no_pause'
        )

        # The default %APPDATA%\Spotify install should be allowed to use SpotX's
        # normal repair/update path. A custom -SpotifyPath disables that path in
        # upstream SpotX, so only pass it when the user really selected one.
        if (-not $isDefaultSpotifyPath) {
            $spotxArgs += @('-SpotifyPath', $SpotifyPath)
        }

        $spotxOutput = @(& powershell.exe @spotxArgs 2>&1)
        $spotxExitCode = $LASTEXITCODE
        foreach ($line in $spotxOutput) {
            Write-Host $line
        }
        $spotxText = ($spotxOutput | ForEach-Object { [string]$_ }) -join "`n"

        # Upstream SpotX uses bare 'Exit' in several failure paths, which returns
        # process code 0. Do not trust LASTEXITCODE alone.
        $spotxReportedFailure =
            $spotxText -match 'Location of Spotify files is broken' -or
            $spotxText -match 'Script is stopped' -or
            $spotxText -match 'xpui\.spa not found'

        if ($spotxExitCode -ne 0 -or $spotxReportedFailure) {
            throw "SpotX reported a failure (exit code $spotxExitCode)."
        }
        if (-not (Test-XpuiArchive $xpuiSpa)) {
            throw 'SpotX left Apps\xpui.spa unreadable.'
        }
        if (-not (Test-SpotXMarker $xpuiSpa)) {
            throw 'SpotX completed without its expected xpui.js patch marker.'
        }

        $spotxSucceeded = $true
    }
    catch {
        Write-Warning $_.Exception.Message
        Write-Warning 'SpotX did not complete cleanly. Restoring the pre-SpotX Spotify files.'
        Restore-SpotXFiles -Root $SpotifyPath -BackupRoot $spotxBackup
        Fail 'SpotX failed, so Soggfy was not installed. Spotify files were restored to their pre-SpotX state.'
    }
    finally {
        Remove-Item -LiteralPath $spotxScript -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $spotxBackup -Recurse -Force -ErrorAction SilentlyContinue
    }

    if (-not $spotxSucceeded) {
        Fail 'SpotX did not complete successfully.'
    }
}
else {
    Write-Warning "SpotX was skipped. Soggfy $InstallerVersion does not block ads or Spotify updates itself; only telemetry blocking remains native."
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
        }
        else {
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
