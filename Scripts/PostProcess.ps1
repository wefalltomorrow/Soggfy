[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InputPath,
    [ValidateSet('mp3','aac','opus','flac')]
    [string]$Format = 'mp3',
    [switch]$DeleteOriginal,
    [switch]$Watch,
    [string]$FFmpegPath = 'ffmpeg'
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

    $args = @('-y','-hide_banner','-loglevel','warning','-nostdin','-i',$Path,'-map','0:a:0','-map_metadata','0')
    if ($Format -ne 'opus') {
        $args += @('-map','0:v?','-c:v','copy')
    }
    $args += $codec
    $args += $temp

    $sourceLock = $null
    try {
        # Permit FFmpeg to read, but prevent concurrent writers during conversion.
        $sourceLock = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        if ($sourceLock.Length -eq 0) { throw "Source is empty: '$Path'." }
        & $ffmpeg.Source @args
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $temp -PathType Leaf) -or
            (Get-Item -LiteralPath $temp).Length -eq 0) {
            throw "FFmpeg failed or produced empty output for '$Path'; original retained."
        }
        & $ffmpeg.Source -hide_banner -loglevel error -xerror -abort_on empty_output -nostdin -i $temp -map 0:a:0 -f null -
        if ($LASTEXITCODE -ne 0 -or (Get-Item -LiteralPath $temp).Length -eq 0) {
            throw "Converted audio validation failed for '$Path'; original retained."
        }
        # File.Move never overwrites or moves the output inside a raced directory.
        [IO.File]::Move($temp, $dest)
        $sourceLock.Dispose(); $sourceLock = $null
        Write-Host "$Path -> $dest"
        if ($DeleteOriginal) { Remove-Item -LiteralPath $Path -Force }
    } finally {
        if ($null -ne $sourceLock) { $sourceLock.Dispose() }
        if (Test-Path -LiteralPath $temp -PathType Leaf) {
            Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
        }
    }
}

function Convert-Tree {
    param([string]$Root)
    if (Test-Path -LiteralPath $Root -PathType Leaf) {
        Convert-One $Root
    } else {
        Get-ChildItem -LiteralPath $Root -File -Recurse |
            Where-Object { $_.Extension -in '.ogg','.flac' } |
            ForEach-Object { Convert-One $_.FullName }
    }
}

Convert-Tree $InputPath

if ($Watch) {
    if (-not (Test-Path -LiteralPath $InputPath -PathType Container)) {
        throw '-Watch requires a directory InputPath.'
    }

    $watcher = [IO.FileSystemWatcher]::new($InputPath)
    $watcher.IncludeSubdirectories = $true
    $watcher.Filter = '*.*'
    $watcher.EnableRaisingEvents = $true

    Write-Host "Watching $InputPath for completed Ogg/FLAC captures. Press Ctrl+C to stop."
    try {
        while ($true) {
            $event = $watcher.WaitForChanged([IO.WatcherChangeTypes]'Created, Renamed', 1000)
            if ($event.TimedOut) { continue }
            $path = Join-Path $InputPath $event.Name
            if ([IO.Path]::GetExtension($path).ToLowerInvariant() -notin '.ogg','.flac') { continue }

            $last = -1L
            for ($i = 0; $i -lt 20; $i++) {
                Start-Sleep -Milliseconds 250
                if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
                $size = (Get-Item -LiteralPath $path).Length
                if ($size -gt 0 -and $size -eq $last) { break }
                $last = $size
            }
            Convert-One $path
        }
    } finally {
        $watcher.Dispose()
    }
}
