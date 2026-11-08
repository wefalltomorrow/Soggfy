$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('soggfy-installer-test-' + [guid]::NewGuid().ToString('N'))
$state = @{ case = ''; spotify = ''; package = ''; backup = ''; network = 0; failRestore = $false }
# All process/network operations are intercepted; only temporary files are used.
function Get-CimInstance { throw 'No desktop process lookup in tests' }
function Get-Process { param($Name, $ErrorAction) return $null }
function Stop-Process { throw 'No process control permitted in installer tests' }
function Get-Item {
    [CmdletBinding()]param([string]$LiteralPath, [switch]$Force)
    $item = Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath -Force:$Force
    if ($LiteralPath -eq (Join-Path $state.spotify 'Spotify.exe')) {
        return [pscustomobject]@{ VersionInfo = @{ FileVersion = $(if ($state.case -eq 'wrong-version') { '1.3.1.2340' } else { '1.3.1.234' }) } }
    }
    return $item
}
function Copy-Item {
    [CmdletBinding()]param([string]$LiteralPath, [string]$Destination, [switch]$Force)
    if ($state.case -in 'ini-failure','rollback-failure' -and $LiteralPath -eq (Join-Path $state.package 'SpotifyHistory.ini')) {
        $state.failRestore = $state.case -eq 'rollback-failure'
        throw 'Injected INI publication failure'
    }
    if ($state.failRestore -and $Destination -eq (Join-Path $state.spotify 'version.dll')) { throw 'Injected restore failure' }
    Microsoft.PowerShell.Management\Copy-Item -LiteralPath $LiteralPath -Destination $Destination -Force:$Force
}
function Invoke-WebRequest {
    param([switch]$UseBasicParsing, $Uri, $OutFile)
    $state.network++
    [IO.File]::WriteAllText($OutFile, '# fake SpotX')
    if ($state.case -eq 'download-failure') { throw 'Injected download failure' }
}
function powershell.exe {
    [IO.File]::WriteAllText((Join-Path $state.spotify 'Spotify.dll'), 'changed-by-SpotX')
    [IO.File]::WriteAllText((Join-Path $state.spotify 'new.dll'), 'new-by-SpotX')
    [IO.Directory]::CreateDirectory((Join-Path $state.spotify 'new-dir')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $state.spotify 'new-dir/new-file'), 'new')
    Remove-Item -LiteralPath (Join-Path $state.spotify 'keep.txt')
    $global:LASTEXITCODE = 0
    'Script is stopped'
}
try {
    [IO.Directory]::CreateDirectory($testRoot) | Out-Null
    $source = Join-Path $PSScriptRoot '../Scripts/Install.ps1'
    $tokens=$null; $errors=$null
    $ast=[System.Management.Automation.Language.Parser]::ParseFile($source,[ref]$tokens,[ref]$errors)
    Assert ($errors.Count -eq 0) 'Installer failed parsing'
    # Remove only the terminal UX trap so expected failures can be caught here.
    $trap = $ast.Find({param($node) $node -is [System.Management.Automation.Language.TrapStatementAst]}, $false)
    $installerText = [IO.File]::ReadAllText($source).Remove($trap.Extent.StartOffset, $trap.Extent.EndOffset-$trap.Extent.StartOffset)
    foreach ($case in 'success','ini-failure','rollback-failure','spotx-failure','download-failure','wrong-version','backup-conflict') {
        $state.case=$case; $state.failRestore=$false; $state.network=0
        $folder=Join-Path $testRoot $case
        $state.package=Join-Path $folder 'package'; $state.spotify=Join-Path $folder 'spotify'
        $scripts=Join-Path $state.package 'Scripts'
        [IO.Directory]::CreateDirectory($scripts) | Out-Null
        [IO.Directory]::CreateDirectory((Join-Path $state.spotify 'Apps')) | Out-Null
        [IO.File]::WriteAllText((Join-Path $scripts 'Install.ps1'),$installerText)
        foreach ($file in 'version.dll','SpotifyHistory.ini') { [IO.File]::WriteAllText((Join-Path $state.package $file),'package-'+$file) }
        foreach ($file in 'Spotify.exe','Spotify.dll','version.dll','keep.txt') { [IO.File]::WriteAllText((Join-Path $state.spotify $file),'original-'+$file) }
        [IO.File]::WriteAllText((Join-Path $state.spotify 'Apps/xpui.spa'),'original-damaged-xpui')
        Add-Type -AssemblyName System.IO.Compression
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip=[IO.Compression.ZipFile]::Open((Join-Path $state.spotify 'Apps/xpui.bak'),[IO.Compression.ZipArchiveMode]::Create)
        foreach ($name in 'index.html','xpui.js') { $writer=[IO.StreamWriter]::new($zip.CreateEntry($name).Open()); $writer.Write('fixture');$writer.Dispose() }
        $zip.Dispose()
        if ($case -eq 'backup-conflict') { [IO.File]::WriteAllText((Join-Path $state.spotify 'version.dll.soggfy-backup'),'preserved-backup') }
        $before=@{}; Get-ChildItem -LiteralPath $state.spotify -Recurse -File | ForEach-Object { $before[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash }
        $failed=$false; $failure=''
        try {
            & (Join-Path $scripts 'Install.ps1') -SpotifyPath $state.spotify -RunSpotX:($case -in 'spotx-failure','download-failure') -SkipSpotX:($case -notin 'spotx-failure','download-failure')
        } catch { $failed=$true; $failure=$_.Exception.Message }
        Assert ($failed -eq ($case -ne 'success')) "Unexpected result for $case / $failure"
        if ($case -eq 'success') {
            Assert ([IO.File]::ReadAllText((Join-Path $state.spotify 'version.dll')) -eq 'package-version.dll') 'DLL not installed'
            Assert ([IO.File]::ReadAllText((Join-Path $state.spotify 'version.dll.soggfy-backup')) -eq 'original-version.dll') 'Original DLL backup lost'
        } elseif ($case -eq 'rollback-failure') {
            Assert ($failure -match "Recovery files retained at '([^']+)'") "Recovery location missing: $failure"
            $state.backup=$Matches[1]
            Assert ([IO.File]::ReadAllText((Join-Path $state.backup 'files/version.dll')) -eq 'original-version.dll') 'Rollback failure discarded original DLL'
            Assert (Test-Path -LiteralPath (Join-Path $state.backup 'manifest.json')) 'Recovery manifest missing'
        } else {
            $after=@(Get-ChildItem -LiteralPath $state.spotify -Recurse -File)
            Assert ($after.Count -eq $before.Count) "Unexpected files after rollback: $case"
            foreach ($item in $after) { Assert ($before[$item.FullName] -eq (Get-FileHash -LiteralPath $item.FullName).Hash) "Content not restored: $case $($item.Name)" }
            Assert (-not (Test-Path -LiteralPath (Join-Path $state.spotify 'new-dir'))) 'New SpotX directory not removed'
        }
        if ($case -in 'wrong-version','backup-conflict') { Assert ($state.network -eq 0) 'Preflight invoked SpotX' }
    }
    Write-Host 'PASS: installation, INI failure, SpotX rollback, pre-repair snapshot, backup retention and preflight'
} finally {
    $state.failRestore=$false
    foreach ($path in @($testRoot,$state.backup)) {
        if (-not $path) { continue }
        $resolved=[IO.Path]::GetFullPath($path)
        $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
        if ((Split-Path -Parent $resolved) -ne $temp -or (Split-Path -Leaf $resolved) -notmatch '^soggfy-(installer-test|install)-[0-9a-f]{32}$') { throw 'Unsafe test cleanup' }
        if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    }
}

