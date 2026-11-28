[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
# Diagnostic only: retains synthetic fixtures and result.json for inspection.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class RefreshPreservationProbe {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern SafeFileHandle CreateFileW(string path, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool CopyFileExW(string source, string target, IntPtr callback, IntPtr data, IntPtr cancel, uint flags);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool SetFileSecurityW(string path, uint information, byte[] descriptor);
}
'@
function Write-ProbeStream([string]$Path, [string]$Text) {
    $handle = [RefreshPreservationProbe]::CreateFileW($Path, 0x40000000, 7, [IntPtr]::Zero, 2, 0, [IntPtr]::Zero)
    if ($handle.IsInvalid) { throw "Stream create failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
    $stream = [IO.FileStream]::new($handle, [IO.FileAccess]::Write)
    try { $bytes = [Text.Encoding]::UTF8.GetBytes($Text); $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
}
function Read-ProbeStream([string]$Path) {
    $handle = [RefreshPreservationProbe]::CreateFileW($Path, [uint32]2147483648, 7, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    if ($handle.IsInvalid) { throw "Stream open failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
    $stream = [IO.FileStream]::new($handle, [IO.FileAccess]::Read)
    $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose(); $stream.Dispose() }
}
function Test-ProbeAce($Acl) {
    foreach ($rule in $Acl.GetAccessRules($true, $false, [Security.Principal.SecurityIdentifier])) {
        if ($rule.IdentityReference.Value -eq 'S-1-1-0' -and
            $rule.AccessControlType -eq [Security.AccessControl.AccessControlType]::Allow -and
            ($rule.FileSystemRights -band [Security.AccessControl.FileSystemRights]::ReadAttributes)) { return $true }
    }
    return $false
}
$root = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$source = Join-Path $root 'source.flac'
$copy = Join-Path $root 'copy.flac'
[IO.File]::WriteAllText($source, 'original-audio-fixture')
Write-ProbeStream ($source + ':custom') 'original-stream'
$acl = Get-Acl -LiteralPath $source
$acl.SetAccessRuleProtection($true, $true)
Set-Acl -LiteralPath $source -AclObject $acl
$held = [RefreshPreservationProbe]::CreateFileW($source, [uint32]2147549184, 1, [IntPtr]::Zero, 3, 0x08200000, [IntPtr]::Zero)
if ($held.IsInvalid) { throw "Source lock failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
$result = [ordered]@{ source = $source; source_access = 'GENERIC_READ|DELETE'; source_share = 'FILE_SHARE_READ' }
try {
    $writer = [RefreshPreservationProbe]::CreateFileW($source, 0x40000000, 7, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    $result.main_writer_blocked = $writer.IsInvalid
    $writer.Dispose()
    $stream = [RefreshPreservationProbe]::CreateFileW(($source + ':custom'), 0x40000000, 7, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
    $result.alternate_writer_opened = -not $stream.IsInvalid
    if (-not $stream.IsInvalid) {
        $file = [IO.FileStream]::new($stream, [IO.FileAccess]::Write)
        try { $bytes = [Text.Encoding]::UTF8.GetBytes('concurrent-stream-edit'); $file.SetLength(0); $file.Write($bytes,0,$bytes.Length); $file.Flush() } finally { $file.Dispose() }
    } else { $stream.Dispose() }
    $newStream = [RefreshPreservationProbe]::CreateFileW(($source + ':added'), 0x40000000, 7, [IntPtr]::Zero, 1, 0, [IntPtr]::Zero)
    $result.alternate_stream_creation_allowed = -not $newStream.IsInvalid
    $newStream.Dispose()
    try { [IO.File]::SetAttributes($source, [IO.FileAttributes]::Hidden); $result.attribute_change_allowed = $true } catch { $result.attribute_change_allowed = $false }
    try {
        $heldAcl = Get-Acl -LiteralPath $source
        $heldAcl.SetAccessRuleProtection($true, $true)
        Set-Acl -LiteralPath $source -AclObject $heldAcl
        $result.security_write_allowed = $true
    } catch { $result.security_write_allowed = $false; $result.security_write_error = $_.Exception.Message }
    # Test DACL-only mutation directly: Set-Acl may request unrelated SACL privileges.
    $beforeNativeAcl = Get-Acl -LiteralPath $source
    $descriptor = $beforeNativeAcl.GetSecurityDescriptorBinaryForm()
    if (Test-ProbeAce $beforeNativeAcl) { throw 'Synthetic fixture already contains the probe ACE' }
    $changedAcl = Get-Acl -LiteralPath $source
    $probeSid = [Security.Principal.SecurityIdentifier]::new('S-1-1-0')
    $probeRule = [Security.AccessControl.FileSystemAccessRule]::new($probeSid, [Security.AccessControl.FileSystemRights]::ReadAttributes, [Security.AccessControl.AccessControlType]::Allow)
    $changedAcl.AddAccessRule($probeRule)
    $result.native_dacl_write_allowed = [RefreshPreservationProbe]::SetFileSecurityW($source, [uint32]4, $changedAcl.GetSecurityDescriptorBinaryForm())
    $result.native_dacl_write_error = if ($result.native_dacl_write_allowed) { 0 } else { [Runtime.InteropServices.Marshal]::GetLastWin32Error() }
    $result.native_dacl_change_observed = (Get-Acl -LiteralPath $source).Sddl -ne $beforeNativeAcl.Sddl
    $result.native_probe_ace_observed = Test-ProbeAce (Get-Acl -LiteralPath $source)
    # Remove the probe ACE; Windows may normalize ACE order/control bits.
    if ($result.native_dacl_write_allowed) {
        if (-not [RefreshPreservationProbe]::SetFileSecurityW($source, [uint32]4, $descriptor)) {
            throw "Native DACL restore failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }
        $result.native_dacl_restore_exact = (Get-Acl -LiteralPath $source).Sddl -eq $beforeNativeAcl.Sddl
        $result.native_probe_ace_removed = -not (Test-ProbeAce (Get-Acl -LiteralPath $source))
        if (-not $result.native_probe_ace_removed -or -not (Get-Acl -LiteralPath $source).AreAccessRulesProtected) { throw 'Probe ACE removal/protected DACL restoration failed' }
    }
    $result.copy_from_held_source = [RefreshPreservationProbe]::CopyFileExW($source,$copy,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,1)
    $result.copy_error = if ($result.copy_from_held_source) { 0 } else { [Runtime.InteropServices.Marshal]::GetLastWin32Error() }
} finally { $held.Dispose() }
$result.original_audio_retained = [IO.File]::ReadAllText($source) -eq 'original-audio-fixture'
$result.alternate_stream_after = Read-ProbeStream ($source + ':custom')
if ($result.copy_from_held_source) {
    $result.copy_has_alternate_stream = (Read-ProbeStream ($copy + ':custom')) -eq $result.alternate_stream_after
    $sourceAcl = Get-Acl -LiteralPath $source
    $copyAcl = Get-Acl -LiteralPath $copy
    $result.source_dacl_protected = $sourceAcl.AreAccessRulesProtected
    $result.copy_dacl_protected = $copyAcl.AreAccessRulesProtected
    $result.copy_security_matches = $sourceAcl.Sddl -eq $copyAcl.Sddl
}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'result.json') -Encoding utf8
$result | ConvertTo-Json


