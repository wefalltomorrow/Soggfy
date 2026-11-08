[CmdletBinding()]
param(
    [string]$SpotifyPath = (Join-Path $env:APPDATA 'Spotify'),
    [switch]$RunSpotX,
    [switch]$SkipSpotX,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Test-ExplorerLaunch {
    try {
        $self = Get-CimInstance Win32_Process -Filter "ProcessId=$PID" -ErrorAction Stop
        $parent = Get-Process -Id $self.ParentProcessId -ErrorAction Stop
        return $parent.ProcessName -ieq 'explorer'
    }
    catch {
        return $false
    }
}

$script:PauseAtExit = Test-ExplorerLaunch

function Pause-IfDirectLaunch {
    if (-not $script:PauseAtExit) {
        return
    }
    Write-Host
    [void](Read-Host 'Press Enter to close this window')
}

trap {
    Write-Error $_
    Pause-IfDirectLaunch
    exit 1
}

$InstallerVersion = 'RC75'
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

# Snapshot the installation before any repair, SpotX or Soggfy mutation. The
# transaction covers this directory only; third-party changes elsewhere are not
# represented as reversible by this installer.
function Get-InstallTree([string]$Root) {
    $rootItem = Get-Item -LiteralPath $Root -Force
    if (-not $rootItem.PSIsContainer -or ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Installation root must be a real directory: $Root"
    }
    $items = @(Get-ChildItem -LiteralPath $Root -Force -Recurse)
    foreach ($item in $items) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Installation contains a reparse point: $($item.FullName)"
        }
    }
    return $items
}

function New-InstallTransaction([string]$Root) {
    $Root = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if ($Root -eq [IO.Path]::GetPathRoot($Root).TrimEnd('\')) { throw 'Cannot snapshot a volume root' }
    $items = @(Get-InstallTree $Root)
    $backup = Join-Path ([IO.Path]::GetTempPath()) ('soggfy-install-' + [guid]::NewGuid().ToString('N'))
    $payload = Join-Path $backup 'files'
    [IO.Directory]::CreateDirectory($payload) | Out-Null
    $entries = @()
    foreach ($item in $items) {
        $relative = $item.FullName.Substring($Root.Length + 1)
        $entry = [pscustomobject]@{ Path = $relative; Directory = $item.PSIsContainer; Hash = '' }
        $destination = Join-Path $payload $relative
        if ($item.PSIsContainer) { [IO.Directory]::CreateDirectory($destination) | Out-Null }
        else {
            [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
            $entry.Hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
            Copy-Item -LiteralPath $item.FullName -Destination $destination -Force
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.Hash -or
                (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash -ne $entry.Hash) {
                throw "Installation changed while taking backup; no installation changes made. Backup: $backup"
            }
        }
        $entries += $entry
    }
    $transaction = [pscustomobject]@{ Root = $Root; Backup = $backup; Entries = $entries }
    $transaction | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $backup 'manifest.json') -Encoding UTF8
    return $transaction
}

function Restore-InstallTransaction($Transaction) {
    $payload = Join-Path $Transaction.Backup 'files'
    # Validate every recovery file before changing the destination.
    foreach ($entry in $Transaction.Entries) {
        if (-not $entry.Directory -and
            (Get-FileHash -LiteralPath (Join-Path $payload $entry.Path) -Algorithm SHA256).Hash -ne $entry.Hash) {
            throw "Recovery backup failed verification: $($entry.Path)"
        }
    }
    $original = @{}
    foreach ($entry in $Transaction.Entries) { $original[$entry.Path] = $entry }
    $current = @(Get-InstallTree $Transaction.Root)
    foreach ($item in ($current | Sort-Object { $_.FullName.Length } -Descending)) {
        $relative = $item.FullName.Substring($Transaction.Root.Length + 1)
        if (-not $original.ContainsKey($relative) -or $original[$relative].Directory -ne $item.PSIsContainer) {
            # Children were considered first; never recursively delete a computed path.
            if ($item.PSIsContainer) { [IO.Directory]::Delete($item.FullName, $false) }
            else { Remove-Item -LiteralPath $item.FullName -Force }
        }
    }
    foreach ($entry in ($Transaction.Entries | Sort-Object { $_.Path.Length })) {
        $destination = Join-Path $Transaction.Root $entry.Path
        if ($entry.Directory) { [IO.Directory]::CreateDirectory($destination) | Out-Null }
        else {
            [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
            Copy-Item -LiteralPath (Join-Path $payload $entry.Path) -Destination $destination -Force
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.Hash) {
                throw "Restored file failed verification: $destination"
            }
        }
    }
}

function Remove-InstallTransaction($Transaction) {
    $path = [IO.Path]::GetFullPath($Transaction.Backup)
    $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    if ((Split-Path -Parent $path) -ne $temp -or (Split-Path -Leaf $path) -notmatch '^soggfy-install-[0-9a-f]{32}$') {
        throw 'Invalid transaction cleanup path'
    }
    # Reject reparse points before recursive cleanup, including the root itself.
    Get-InstallTree $path | Out-Null
    Remove-Item -LiteralPath $path -Recurse -Force
}

if ($RunSpotX -and $SkipSpotX) {
    Fail 'Use either -RunSpotX or -SkipSpotX, not both.'
}

# Check the package before stopping Spotify or changing its installation.
$packageRoot = Split-Path -Parent $PSScriptRoot
$sourceDll = Join-Path $packageRoot 'version.dll'
$sourceIni = Join-Path $packageRoot 'SpotifyHistory.ini'
if (-not (Test-Path -LiteralPath $sourceDll -PathType Leaf) -or (Get-Item -LiteralPath $sourceDll).Length -eq 0) {
    Fail "version.dll is missing or empty in '$packageRoot'. Extract the full release ZIP first."
}
$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceDll).Hash

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
    $installedVersion -notmatch ('^' + [regex]::Escape($PinnedSpotifyVersion) + '(?:$|[. ]g[0-9a-f]+$)')) {
    Fail "$InstallerVersion targets Spotify $PinnedSpotifyFullVersion x64. Installed Spotify reports '$installedVersion'. Install the pinned Spotify build first."
}
Write-Host "Spotify $installedVersion detected ($InstallerVersion baseline)."

$targetDll = Join-Path $SpotifyPath 'version.dll'
$backupDll = Join-Path $SpotifyPath 'version.dll.soggfy-backup'
if ((Test-Path -LiteralPath $targetDll -PathType Leaf) -and
    (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash -ne $sourceHash -and
    (Test-Path -LiteralPath $backupDll) -and -not $Force) {
    Fail "A backup already exists at '$backupDll'. Move/remove it or rerun with -Force if you know it is safe."
}

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

$transaction = New-InstallTransaction $SpotifyPath
$transactionResolved = $false
try {
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
    $spotxScript = Join-Path ([IO.Path]::GetTempPath()) ('soggfy-spotx-run-' + [Guid]::NewGuid().ToString('N') + '.ps1')

    try {
        Write-Host 'Downloading SpotX...'
        Invoke-WebRequest -UseBasicParsing -Uri $spotxUrl -OutFile $spotxScript
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

    }
    finally {
        Remove-Item -LiteralPath $spotxScript -Force -ErrorAction SilentlyContinue
    }

}
else {
    Write-Warning "SpotX was skipped. Soggfy $InstallerVersion does not block ads or Spotify updates itself; only telemetry blocking remains native."
}

$targetDll = Join-Path $SpotifyPath 'version.dll'
$backupDll = Join-Path $SpotifyPath 'version.dll.soggfy-backup'

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
if ((Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash -ne $sourceHash) {
    throw 'Installed DLL failed SHA256 verification.'
}
Write-Host "Installed Soggfy -> $targetDll"

$targetIni = Join-Path $SpotifyPath 'SpotifyHistory.ini'
if (-not (Test-Path -LiteralPath $targetIni) -and (Test-Path -LiteralPath $sourceIni)) {
    Copy-Item -LiteralPath $sourceIni -Destination $targetIni
    Write-Host "Created $targetIni"
}

$transactionResolved = $true
}
catch {
    $installFailure = $_.Exception.Message
    try {
        Restore-InstallTransaction $transaction
        $transactionResolved = $true
    }
    catch {
        throw "Install failed: $installFailure. Rollback also failed: $($_.Exception.Message). Recovery files retained at '$($transaction.Backup)'."
    }
    throw "Install failed: $installFailure. Installation files were restored."
}
finally {
    if ($transactionResolved) {
        try { Remove-InstallTransaction $transaction }
        catch { Write-Warning "Transaction finished; backup cleanup failed. Recovery files remain at '$($transaction.Backup)'." }
    }
}

if ($installSpotX) {
    Write-Host 'Done. SpotX handles ad/update blocking; Soggfy keeps its separate telemetry blocker.'
}
else {
    Write-Host 'Done. Soggfy installed without SpotX.'
}
Write-Host 'Start Spotify and use the Soggfy Downloads button in the top bar.'
Pause-IfDirectLaunch
