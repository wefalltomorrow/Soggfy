[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InputPath,
    [ValidateSet('mp3','aac','opus','flac')]
    [string]$Format = 'mp3',
    [switch]$DeleteOriginal,
    [switch]$Watch,
    [string]$FFmpegPath = 'ffmpeg',
    # One budget for encoding and validation. Zero selects a size-based budget.
    [ValidateRange(0, 21600)]
    [int]$TimeoutSeconds = 0
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ffmpeg = Get-Command $FFmpegPath -ErrorAction SilentlyContinue
if (-not $ffmpeg) {
    throw 'FFmpeg was not found in PATH.'
}

$InputPath = [Environment]::ExpandEnvironmentVariables($InputPath)
if (-not (Test-Path -LiteralPath $InputPath)) {
    throw "Input path does not exist: $InputPath"
}
$InputPath = (Resolve-Path -LiteralPath $InputPath).ProviderPath
if ($Watch -and -not (Test-Path -LiteralPath $InputPath -PathType Container)) {
    throw '-Watch requires a directory InputPath.'
}
if ($ffmpeg.CommandType -notin 'Application','ExternalScript' -or
    [IO.Path]::GetExtension($ffmpeg.Source) -notin '.exe','.ps1') {
    throw 'FFmpegPath must resolve to an executable (.exe) or PowerShell wrapper (.ps1).'
}
if (-not ('Soggfy.ConverterProcess' -as [type])) {
    Add-Type -Path (Join-Path $PSScriptRoot 'ConverterProcess.cs')
}

function Invoke-Converter([string[]]$Arguments, [Diagnostics.Stopwatch]$Clock, [long]$BudgetMs) {
    if ($Clock.ElapsedMilliseconds -ge $BudgetMs) { throw 'Conversion timed out; original retained.' }
    $exe = $ffmpeg.Source
    if ([IO.Path]::GetExtension($exe) -eq '.ps1') {
        # EncodedCommand avoids cmd.exe and preserves literal Unicode, quotes,
        # dollar signs and metacharacters when invoking a PowerShell wrapper.
        $tokens = @($exe) + $Arguments | ForEach-Object { "'" + $_.Replace("'", "''") + "'" }
        $command = '$ErrorActionPreference = ''Stop''; $global:LASTEXITCODE = 0; & ' + ($tokens -join ' ') + '; exit $LASTEXITCODE'
        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
        $exe = Join-Path $PSHOME 'pwsh.exe'
        if (-not (Test-Path -LiteralPath $exe)) { $exe = Join-Path $PSHOME 'powershell.exe' }
        $Arguments = @('-NoProfile','-NonInteractive','-EncodedCommand',$encoded)
    }
    $child = $null
    try {
        $child = [Soggfy.ConverterProcess]::new($exe, $Arguments, (Get-Location).ProviderPath)
        while (-not $child.Wait(100)) {
            if ($Clock.ElapsedMilliseconds -ge $BudgetMs) { throw 'Conversion timed out; original retained.' }
        }
        if ($Clock.ElapsedMilliseconds -ge $BudgetMs) { throw 'Conversion timed out; original retained.' }
        return $child.ExitCode
    } finally {
        if ($null -ne $child) { $child.Dispose() }
    }
}

function Convert-One([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    $sourceExt = [IO.Path]::GetExtension($Path).TrimStart('.').ToLowerInvariant()
    if ($sourceExt -notin @('ogg','flac')) { return }
    if ($sourceExt -eq $Format) { return }

    # AAC is stored in an M4A/MP4 container so metadata and attached artwork have a proper home.
    $outputExtension = if ($Format -eq 'aac') { 'm4a' } else { $Format }
    $dest = [IO.Path]::ChangeExtension($Path, $outputExtension)
    if (Test-Path -LiteralPath $dest) {
        Write-Verbose "Skipping existing output: $dest"
        return
    }

    $directory = [IO.Path]::GetDirectoryName($dest)
    $baseName = [IO.Path]::GetFileNameWithoutExtension($dest)
    $temp = Join-Path $directory "$baseName.soggfy-converting-$([guid]::NewGuid().ToString('N')).$outputExtension"
    $codec = switch ($Format) {
        'mp3'  { @('-c:a','libmp3lame','-b:a','320k','-id3v2_version','3') }
        'aac'  { @('-c:a','aac','-b:a','256k') }
        'opus' { @('-c:a','libopus','-b:a','192k') }
        'flac' { @('-c:a','flac') }
    }

    # A playable output does not prove all input frames decoded successfully.
    $args = @('-y','-hide_banner','-loglevel','warning','-xerror','-abort_on','empty_output','-nostdin','-i',$Path,'-map','0:a:0','-map_metadata','0')
    if ($Format -ne 'opus') {
        $args += @('-map','0:v?','-c:v','copy')
    }
    $args += $codec
    $args += $temp

    $sourceLock = $null
    try {
        # Permit FFmpeg to read, but prevent concurrent writers during conversion.
        $sourceLock = [Soggfy.ConversionSource]::new($Path)
        if ($sourceLock.Length -eq 0) { throw "Source is empty: '$Path'." }
        $budget = if ($TimeoutSeconds) { [long]$TimeoutSeconds * 1000 } else {
            [long][Math]::Min(21600000, 300000 + [Math]::Floor($sourceLock.Length / 65536) * 1000)
        }
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $exitCode = Invoke-Converter $args $clock $budget
        if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $temp -PathType Leaf) -or
            (Get-Item -LiteralPath $temp).Length -eq 0) {
            throw "FFmpeg failed (exit $exitCode) or produced empty output for '$Path'; original retained."
        }
        $exitCode = Invoke-Converter @('-hide_banner','-loglevel','error','-xerror','-abort_on','empty_output','-nostdin','-i',$temp,'-map','0:a:0','-f','null','-') $clock $budget
        if ($exitCode -ne 0 -or (Get-Item -LiteralPath $temp).Length -eq 0) {
            throw "Converted audio validation failed (exit $exitCode) for '$Path'; original retained."
        }
        # File.Move never overwrites or moves the output inside a raced directory.
        [IO.File]::Move($temp, $dest)
        Write-Host "$Path -> $dest"
        if ($DeleteOriginal) {
            try { $sourceLock.DeleteValidatedSource() }
            catch { Write-Warning "Output saved, but source was retained: $($_.Exception.Message)" }
        }
    } finally {
        if ($null -ne $sourceLock) { $sourceLock.Dispose() }
        if (Test-Path -LiteralPath $temp -PathType Leaf) {
            Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
        }
    }
}

$retry = @{}
function Convert-Tree {
    param([string]$Root)
    if (Test-Path -LiteralPath $Root -PathType Leaf) {
        Convert-One $Root
    } else {
        Get-ChildItem -LiteralPath $Root -File -Recurse -ErrorAction Continue |
            Where-Object { $_.Extension -in '.ogg','.flac' } |
            ForEach-Object {
                if (-not $Watch) { Convert-One $_.FullName; return }
                $path = $_.FullName
                $stamp = "$($_.Length):$($_.LastWriteTimeUtc.Ticks)"
                $previous = $retry[$path]
                if ($previous -and $previous.Stamp -eq $stamp -and [DateTime]::UtcNow -lt $previous.Next) { return }
                try {
                    Convert-One $path
                    $retry.Remove($path)
                } catch {
                    # A stable file size does not mean its writer has closed.
                    # Retry individual failures without stopping other conversions.
                    $delay = if ($previous -and $previous.Stamp -eq $stamp) { [Math]::Min(60, $previous.Delay * 2) } else { 2 }
                    if ($retry.Count -ge 4096 -and -not $retry.ContainsKey($path)) {
                        $oldest = $retry.Keys | Sort-Object { $retry[$_].Next } | Select-Object -First 1
                        $retry.Remove($oldest)
                    }
                    $retry[$path] = @{ Stamp = $stamp; Delay = $delay; Next = [DateTime]::UtcNow.AddSeconds($delay) }
                    Write-Warning "Conversion deferred for '$path': $($_.Exception.Message)"
                }
            }
    }
}

if ($Watch) {
    $watcher = [IO.FileSystemWatcher]::new($InputPath)
    $watcher.IncludeSubdirectories = $true
    $watcher.Filter = '*.*'
    $watcher.EnableRaisingEvents = $true

    Write-Host "Watching $InputPath for completed Ogg/FLAC captures. Press Ctrl+C to stop."
    try {
        while ($true) {
            # Reconciliation is authoritative. Events only shorten the idle wait:
            # events during encoding, overflow, startup and renamed directories
            # are all recovered on the next pass, without an unbounded event queue.
            Convert-Tree $InputPath
            foreach ($path in @($retry.Keys)) {
                if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $retry.Remove($path) }
            }
            $null = $watcher.WaitForChanged([IO.WatcherChangeTypes]'Created, Renamed, Changed', 2000)
        }
    } finally {
        $watcher.Dispose()
    }
} else {
    Convert-Tree $InputPath
}
