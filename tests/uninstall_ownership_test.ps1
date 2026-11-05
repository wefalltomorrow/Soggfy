$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('soggfy-uninstall-' + [guid]::NewGuid().ToString('N'))
function Assert($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
$uninstallTestState = @{ queries = 0; running = $false }
# No test may enumerate or stop a real Spotify process.
function Get-Process { param($Name, $ErrorAction)
    $uninstallTestState.queries++
    if ($uninstallTestState.running) { [pscustomobject]@{ Name = 'fake-Spotify' } }
}
function Stop-Process { param([Parameter(ValueFromPipeline)]$InputObject, [switch]$Force)
    throw 'Test must never stop a process'
}
try {
    $package = Join-Path $testRoot 'package'
    $scripts = Join-Path $package 'Scripts'
    [IO.Directory]::CreateDirectory($scripts) | Out-Null
    foreach ($file in 'Uninstall.ps1','ConverterProcess.cs') {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "../Scripts/$file") -Destination $scripts
    }
    $uninstaller = Join-Path $scripts 'Uninstall.ps1'
    foreach ($case in 'matching','keep-settings','foreign','foreign-force','missing-package','missing-target','absent','running') {
        $folder = Join-Path $testRoot $case
        [IO.Directory]::CreateDirectory($folder) | Out-Null
        $target = Join-Path $folder 'version.dll'
        $backup = Join-Path $folder 'version.dll.soggfy-backup'
        $settings = Join-Path $folder 'SpotifyHistory.ini'
        $packageDll = Join-Path $package 'version.dll'
        [IO.File]::WriteAllText($packageDll, 'our-dll')
        [IO.File]::WriteAllText((Join-Path $folder 'Spotify.exe'), 'test-only')
        [IO.File]::WriteAllText($settings, 'settings')
        if ($case -notin 'missing-target','absent') {
            [IO.File]::WriteAllText($target, $(if ($case -like 'foreign*') { 'another-tool' } else { 'our-dll' }))
        }
        if ($case -ne 'absent') { [IO.File]::WriteAllText($backup, 'original-dll') }
        if ($case -eq 'missing-package') { Remove-Item -LiteralPath $packageDll }
        $uninstallTestState.queries = 0
        $uninstallTestState.running = $case -eq 'running'
        $failed = $false
        try { & $uninstaller -SpotifyPath $folder -RemoveSettings:($case -ne 'keep-settings') -Force:($case -eq 'foreign-force') }
        catch { $failed = $true; $failure = $_.Exception.Message }
        $expectedFailure = $case -in 'foreign','foreign-force','missing-package','missing-target','running'
        if ($failed -and -not $expectedFailure) { throw "Unexpected failure for ${case}: $failure" }
        Assert ($failed -eq $expectedFailure) "Unexpected outcome: $case"
        if ($expectedFailure) {
            Assert ([IO.File]::ReadAllText($backup) -eq 'original-dll') "Backup changed: $case"
            Assert ([IO.File]::ReadAllText($settings) -eq 'settings') "Settings changed: $case"
            if ($case -eq 'missing-target') { Assert (-not (Test-Path -LiteralPath $target)) 'Restored unverified backup' }
            else { Assert ([IO.File]::ReadAllText($target) -eq $(if ($case -like 'foreign*') { 'another-tool' } else { 'our-dll' })) "DLL changed: $case" }
            if ($case -ne 'running') { Assert ($uninstallTestState.queries -eq 0) 'Ownership failure reached process control' }
        } elseif ($case -ne 'absent') {
            Assert ([IO.File]::ReadAllText($target) -eq 'original-dll') 'Backup not restored'
            Assert (-not (Test-Path -LiteralPath $backup)) 'Backup was copied instead of moved'
        }
        if (-not $expectedFailure) { Assert ((Test-Path -LiteralPath $settings) -eq ($case -eq 'keep-settings')) 'Unexpected settings removal' }
    }
    Write-Host 'PASS: uninstall ownership, backup restoration and settings preservation'
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        $resolved = [IO.Path]::GetFullPath($testRoot)
        $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if (-not $resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase) -or
            -not ([IO.Path]::GetFileName($resolved)).StartsWith('soggfy-uninstall-')) { throw 'Unsafe test cleanup path' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
