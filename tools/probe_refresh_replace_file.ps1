[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
# Isolated diagnostic only. All fixtures and reports are retained.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class RefreshReplaceProbe {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern SafeFileHandle CreateFileW(string path, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool ReplaceFileW(string source, string replacement, string backup, uint flags, IntPtr exclude, IntPtr reserved);
}
'@
function Write-Stream([string]$Path, [string]$Value) {
    $handle = [RefreshReplaceProbe]::CreateFileW($Path, 0x40000000, 7, [IntPtr]::Zero, 2, 0, [IntPtr]::Zero)
    if ($handle.IsInvalid) { $handle.Dispose(); throw 'Cannot create fixture stream' }
    $stream = [IO.FileStream]::new($handle, [IO.FileAccess]::Write)
    try { $bytes = [Text.Encoding]::UTF8.GetBytes($Value); $stream.Write($bytes, 0, $bytes.Length) }
    finally { $stream.Dispose() }
}
function Read-Stream([string]$Path) {
    $handle = [RefreshReplaceProbe]::CreateFileW($Path, [uint32]2147483648, 7, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    if ($handle.IsInvalid) { $handle.Dispose(); return $null }
    $stream = [IO.FileStream]::new($handle, [IO.FileAccess]::Read)
    $reader = [IO.StreamReader]::new($stream)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}
$root = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$results = @()
foreach ($mode in @('source-held', 'staging-held', 'unlocked', 'stale-stream-copy')) {
    $directory = Join-Path $root $mode
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $source = Join-Path $directory 'source.flac'
    $staging = Join-Path $directory 'staging.flac'
    $backup = Join-Path $directory 'original.flac'
    [IO.File]::WriteAllText($source, 'original-audio')
    [IO.File]::WriteAllText($staging, 'validated-retagged-audio')
    Write-Stream ($source + ':custom') 'initial-stream'
    $acl = Get-Acl -LiteralPath $source
    $acl.SetAccessRuleProtection($true, $true)
    Set-Acl -LiteralPath $source -AclObject $acl
    $beforeSddl = (Get-Acl -LiteralPath $source).Sddl
    if ($mode -eq 'stale-stream-copy') { Write-Stream ($staging + ':custom') 'initial-stream' }
    # A deterministic edit after a hypothetical snapshot, before publication.
    Write-Stream ($source + ':custom') 'newer-stream'
    Write-Stream ($source + ':added') 'new-stream'
    $held = $null
    try {
        if ($mode -eq 'source-held') {
            $held = [RefreshReplaceProbe]::CreateFileW($source, [uint32]2147549184, 1, [IntPtr]::Zero, 3, 0x08200000, [IntPtr]::Zero)
        } elseif ($mode -eq 'staging-held') {
            $held = [RefreshReplaceProbe]::CreateFileW($staging, [uint32]3221291008, 1, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
        }
        if ($null -ne $held -and $held.IsInvalid) { throw 'Fixture lock failed' }
        $success = [RefreshReplaceProbe]::ReplaceFileW($source, $staging, $backup, 0, [IntPtr]::Zero, [IntPtr]::Zero)
        $errorCode = if ($success) { 0 } else { [Runtime.InteropServices.Marshal]::GetLastWin32Error() }
    } finally { if ($null -ne $held) { $held.Dispose() } }
    $result = [ordered]@{
        mode = $mode; succeeded = $success; win32_error = $errorCode
        source_audio = Read-Stream $source
        source_custom = Read-Stream ($source + ':custom')
        source_added = Read-Stream ($source + ':added')
        backup_audio = Read-Stream $backup
        backup_custom = Read-Stream ($backup + ':custom')
        staging_exists = [IO.File]::Exists($staging)
        dacl_protected = (Get-Acl -LiteralPath $source).AreAccessRulesProtected
        dacl_sddl_matches = (Get-Acl -LiteralPath $source).Sddl -eq $beforeSddl
    }
    $results += [pscustomobject]$result
}
$report = [ordered]@{ fixture_root = $root; powershell = $PSVersionTable.PSVersion.ToString(); cases = $results }
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root 'result.json') -Encoding utf8
$report | ConvertTo-Json -Depth 5
# Assert only the specific local compatibility claims, not complete preservation.
foreach ($case in $results) {
    if ($case.mode -in @('source-held', 'staging-held')) {
        if ($case.succeeded -or $case.win32_error -ne 32 -or $case.source_audio -ne 'original-audio' -or
            -not $case.staging_exists -or $null -ne $case.backup_audio) { throw 'Held-handle expectation failed' }
    } else {
        $expectedStream = if ($case.mode -eq 'stale-stream-copy') { 'initial-stream' } else { 'newer-stream' }
        if (-not $case.succeeded -or $case.source_audio -ne 'validated-retagged-audio' -or
            $case.source_custom -ne $expectedStream -or $case.source_added -ne 'new-stream' -or
            $case.backup_audio -ne 'original-audio' -or $case.backup_custom -ne 'newer-stream' -or
            $case.staging_exists -or -not $case.dacl_protected -or -not $case.dacl_sddl_matches) {
            throw 'Unlocked replacement expectation failed'
        }
    }
}
