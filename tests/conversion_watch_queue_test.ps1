$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -Path (Join-Path $PSScriptRoot '../Scripts/ConverterProcess.cs')
function Assert($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
$root=Join-Path ([IO.Path]::GetTempPath()) ('soggfy-watch-queue-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$watch=$null
try {
    $watch=[Soggfy.ConversionWatch]::new($root)
    Assert ($watch.TakeRescan()) 'Startup must reconcile'
    Assert (-not $watch.TakeRescan()) 'Rescan flag did not reset'
    # Feed deterministic bursts through the exact callback sink; actual OS event
    # delivery and encoding are exercised separately in manual_watch_test.py.
    $record=$watch.GetType().GetMethod('Record',[Reflection.BindingFlags]'Instance,NonPublic')
    for ($i=0; $i -lt 5000; $i++) { $null=$record.Invoke($watch,@([string](Join-Path $root "$i.flac"))) }
    Assert ($watch.TakeRescan()) 'Overflow did not request reconciliation'
    $paths=$watch.Drain()
    Assert ($paths.Count -eq 4096) 'Queue not bounded to 4096 distinct paths'
    Assert ($watch.Drain().Count -eq 0) 'Drain retained stale events'
    $null=$record.Invoke($watch,@([string](Join-Path $root 'same.ogg')))
    $null=$record.Invoke($watch,@([string](Join-Path $root 'SAME.OGG')))
    $null=$record.Invoke($watch,@([string](Join-Path $root 'ignored.mp3')))
    Assert ($watch.Drain().Count -eq 1) 'Duplicate coalescing or extension filtering failed'
    $failed=$watch.GetType().GetMethod('Failed',[Reflection.BindingFlags]'Instance,NonPublic')
    $null=$failed.Invoke($watch,@($null,[IO.ErrorEventArgs]::new([IO.InternalBufferOverflowException]::new())))
    Assert ($watch.TakeRescan()) 'Watcher error did not request reconciliation'
    $watch.Dispose()
    $null=$record.Invoke($watch,@([string](Join-Path $root 'late.flac')))
    Assert ($watch.Drain().Count -eq 0) 'Late callback queued after disposal'
    $watch.Dispose()
    Write-Host 'PASS: bounded watch queue, overflow recovery, coalescing and late callbacks'
} finally {
    if ($null -ne $watch) { $watch.Dispose() }
    $resolved=[IO.Path]::GetFullPath($root)
    if ((Split-Path -Parent $resolved) -ne [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') -or
        (Split-Path -Leaf $resolved) -notmatch '^soggfy-watch-queue-[0-9a-f]{32}$') { throw 'Unsafe test cleanup' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

