param([Parameter(Mandatory)][string]$ConverterPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ConverterPath = (Resolve-Path -LiteralPath $ConverterPath).Path
$script = Join-Path $PSScriptRoot '../Scripts/PostProcess.ps1'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('soggfy-manual-test-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
function Assert($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
$saved = @{}
foreach ($name in 'SGF_TEST_MODE','SGF_TEST_FINAL','SGF_TEST_ARGS','SGF_TEST_PID') { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    foreach ($mode in 'fail','empty','corrupt','race','directory','ok','keep','existing','source-lock','hang','validation-hang') {
        $source = Join-Path $testRoot "$mode.ogg"
        $dest = [IO.Path]::ChangeExtension($source, 'mp3')
        [IO.File]::WriteAllText($source, 'native-original')
        $env:SGF_TEST_MODE = $mode
        $env:SGF_TEST_FINAL = $dest
        $env:SGF_TEST_ARGS = Join-Path $testRoot 'arguments.txt'
        $env:SGF_TEST_PID = Join-Path $testRoot 'child.pid'
        Remove-Item -LiteralPath $env:SGF_TEST_PID -ErrorAction SilentlyContinue
        if ($mode -eq 'existing') { [IO.File]::WriteAllText($dest, 'existing-output') }
        $failed = $false
        $clock = [Diagnostics.Stopwatch]::StartNew()
        try { & $script -InputPath $source -FFmpegPath $ConverterPath -DeleteOriginal:($mode -ne 'keep') -TimeoutSeconds 2 }
        catch { $failed = $true }
        $expectedFailure = $mode -in 'fail','empty','corrupt','race','directory','hang','validation-hang'
        Assert ($failed -eq $expectedFailure) "Unexpected outcome for $mode"
        if ($mode -in 'ok','source-lock') { Assert (-not (Test-Path -LiteralPath $source)) 'Validated conversion did not delete source' }
        else { Assert ([IO.File]::ReadAllText($source) -eq 'native-original') "Original changed for $mode" }
        if ($mode -in 'ok','keep','source-lock') {
            Assert ([IO.File]::ReadAllText($dest) -eq 'valid-audio') 'Expected validated output'
            $arguments = [IO.File]::ReadAllText($env:SGF_TEST_ARGS)
            Assert ($arguments.Contains('-b:a' + [char]10 + '320k' + [char]10) -and -not $arguments.Contains('-q:a')) 'MP3 must use 320 kbps'
        } elseif ($mode -eq 'race') { Assert ([IO.File]::ReadAllText($dest) -eq 'other-file') 'Raced output overwritten' }
        elseif ($mode -eq 'directory') { Assert (Test-Path -LiteralPath $dest -PathType Container) 'Raced directory replaced' }
        elseif ($mode -eq 'existing') { Assert ([IO.File]::ReadAllText($dest) -eq 'existing-output') 'Existing output overwritten' }
        else { Assert (-not (Test-Path -LiteralPath $dest)) "Failed conversion published output for $mode" }
        Assert (@(Get-ChildItem -LiteralPath $testRoot -Recurse -Filter '*.soggfy-converting-*').Count -eq 0) 'Temporary output leaked'
        if ($mode -in 'hang','validation-hang') {
            Assert ($clock.Elapsed.TotalSeconds -lt 8) 'Deadline did not bound conversion'
            Assert (Test-Path -LiteralPath $env:SGF_TEST_PID) 'Hung converter did not spawn its child'
            $child = Get-Process -Id ([int][IO.File]::ReadAllText($env:SGF_TEST_PID)) -ErrorAction SilentlyContinue
            if ($null -ne $child) { Assert ($child.WaitForExit(2000)) 'Timeout leaked a descendant' }
        }
    }
    # Force the close/reopen gap without timing: deletion must reject a different
    # file and a changed same-identity file, while leaving their bytes untouched.
    foreach ($replacement in $true,$false) {
        $source = Join-Path $testRoot 'identity.flac'
        [IO.File]::WriteAllText($source, 'original')
        $guard = [Soggfy.ConversionSource]::new($source)
        $guard.Dispose()
        if ($replacement) { [IO.File]::Move($source, (Join-Path $testRoot 'moved.flac')) }
        [IO.File]::WriteAllText($source, 'replacement-bytes')
        $failed = $false
        try { $guard.DeleteValidatedSource() } catch { $failed = $true }
        Assert $failed 'Changed source was deleted'
        Assert ([IO.File]::ReadAllText($source) -eq 'replacement-bytes') 'Replacement changed'
    }
    if ($env:SGF_REAL_FFMPEG) {
        $source = Join-Path $testRoot 'real.flac'
        & $env:SGF_REAL_FFMPEG -y -hide_banner -loglevel error -f lavfi -i 'sine=frequency=440:duration=1' -c:a flac $source
        Assert ($LASTEXITCODE -eq 0) 'Real FLAC generation failed'
        & $script -InputPath $source -FFmpegPath $env:SGF_REAL_FFMPEG -DeleteOriginal
        Assert (-not (Test-Path -LiteralPath $source)) 'Real converted source not deleted'
        $dest = [IO.Path]::ChangeExtension($source,'mp3')
        & $env:SGF_REAL_FFMPEG -hide_banner -loglevel error -xerror -i $dest -f null -
        Assert ($LASTEXITCODE -eq 0) 'Real MP3 decode failed'
    }
    Write-Host 'PASS: manual conversion validation, 320k arguments, original retention/deletion, conflicts and cleanup'
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    $resolved = [IO.Path]::GetFullPath($testRoot)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not ([IO.Path]::GetFileName($resolved)).StartsWith('soggfy-manual-test-')) { throw 'Unexpected test cleanup path' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
