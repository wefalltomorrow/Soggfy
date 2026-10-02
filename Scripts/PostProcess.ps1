[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InputPath,
    [ValidateSet('mp3','aac','opus','flac')]
    [string]$Format = 'mp3',
    [switch]$DeleteOriginal,
    [switch]$Watch
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
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

    $dest = [IO.Path]::ChangeExtension($Path, $Format)
    if (Test-Path -LiteralPath $dest) {
        Write-Verbose "Skipping existing output: $dest"
        return
    }

    $directory = [IO.Path]::GetDirectoryName($dest)
    $baseName = [IO.Path]::GetFileNameWithoutExtension($dest)
    $temp = Join-Path $directory "$baseName.soggfy-converting.$Format"
    $codec = switch ($Format) {
        'mp3'  { @('-c:a','libmp3lame','-q:a','0') }
        'aac'  { @('-c:a','aac','-b:a','256k') }
        'opus' { @('-c:a','libopus','-b:a','192k') }
        'flac' { @('-c:a','flac') }
    }

    $args = @('-hide_banner','-loglevel','warning','-nostdin','-i',$Path,'-map','0:a:0','-map_metadata','0')
    if ($Format -ne 'opus') {
        $args += @('-map','0:v?','-c:v','copy')
    }
    $args += $codec
    $args += @('-y',$temp)

    & $ffmpeg.Source @args
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $temp)) {
        Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
        throw "FFmpeg failed for '$Path'."
    }

    Move-Item -LiteralPath $temp -Destination $dest
    Write-Host "$Path -> $dest"
    if ($DeleteOriginal) {
        Remove-Item -LiteralPath $Path -Force
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
