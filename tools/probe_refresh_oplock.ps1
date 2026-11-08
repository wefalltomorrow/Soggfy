[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
# Isolated synthetic fixtures only. This does not modify recordings or Spotify.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
public static class RefreshOplockProbe {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr CreateFileW(string p, uint a, uint s, IntPtr sec, uint c, uint f, IntPtr t);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern IntPtr CreateEventW(IntPtr s, bool manual, bool initial, IntPtr n);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool DeviceIoControl(IntPtr h, uint code, IntPtr input, uint ilen, IntPtr output, uint olen, IntPtr returned, IntPtr ov);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool CancelIoEx(IntPtr h, IntPtr ov);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool GetOverlappedResult(IntPtr h, IntPtr ov, out uint bytes, bool wait);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool SetFileAttributesW(string p, uint attrs);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool SetFileSecurityW(string p, uint info, byte[] descriptor);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool WriteFile(IntPtr h, byte[] b, uint size, out uint written, IntPtr ov);
    public sealed class Result {
        public string operation; public uint share; public int request_error;
        public bool granted, completed_before_release, break_before_release, mutation_succeeded;
        public int mutation_error; public long completion_status;
        public uint original_level, new_level, break_flags;
    }
    static int Mutate(string p, string op, byte[] descriptor) {
        if(op=="attributes") return SetFileAttributesW(p,2) ? 0 : Marshal.GetLastWin32Error();
        if(op=="dacl") return SetFileSecurityW(p,4,descriptor) ? 0 : Marshal.GetLastWin32Error();
        string target = op=="main-write" ? p : p + (op=="existing-stream" ? ":custom" : ":added");
        IntPtr h=CreateFileW(target,0x40000000,7,IntPtr.Zero,op=="new-stream" ? 1u : 3u,0,IntPtr.Zero);
        if(h==new IntPtr(-1)) return Marshal.GetLastWin32Error();
        try { uint n; byte[] bytes=System.Text.Encoding.UTF8.GetBytes("changed");
            if(!WriteFile(h,bytes,(uint)bytes.Length,out n,IntPtr.Zero)) return Marshal.GetLastWin32Error();
            return n==bytes.Length ? 0 : -1;
        } finally { CloseHandle(h); }
    }
    public static Result Run(string path,uint share,string operation,byte[] descriptor) {
        Result r=new Result(); r.share=share; r.operation=operation;
        IntPtr h=CreateFileW(path,0x80010000,share,IntPtr.Zero,3,0x48200000,IntPtr.Zero);
        if(h==new IntPtr(-1)) throw new IOException("Source open: "+Marshal.GetLastWin32Error());
        IntPtr ev=IntPtr.Zero, input=IntPtr.Zero, output=IntPtr.Zero, ov=IntPtr.Zero;
        bool pending=false; Task<int> task=null;
        try {
            ev=CreateEventW(IntPtr.Zero,true,false,IntPtr.Zero);
            if(ev==IntPtr.Zero) throw new IOException("Event creation failed");
            input=Marshal.AllocHGlobal(12); output=Marshal.AllocHGlobal(24);
            int ovSize=IntPtr.Size==8 ? 32 : 20; ov=Marshal.AllocHGlobal(ovSize);
            Marshal.Copy(new byte[24],0,output,24); Marshal.Copy(new byte[ovSize],0,ov,ovSize);
            Marshal.WriteIntPtr(ov,IntPtr.Size==8 ? 24 : 16,ev);
            Marshal.WriteInt16(input,0,1); Marshal.WriteInt16(input,2,12);
            // READ | WRITE | HANDLE caching; REQUEST_OPLOCK_INPUT_FLAG_REQUEST.
            Marshal.WriteInt32(input,4,7); Marshal.WriteInt32(input,8,1);
            bool immediate=DeviceIoControl(h,0x90240,input,12,output,24,IntPtr.Zero,ov);
            r.request_error=immediate ? 0 : Marshal.GetLastWin32Error();
            pending=r.granted=!immediate && r.request_error==997;
            if(!r.granted) return r;
            task=Task.Run(()=>Mutate(path,operation,descriptor));
            r.completed_before_release=task.Wait(1500);
            r.break_before_release=WaitForSingleObject(ev,0)==0;
            if(r.break_before_release) {
                uint bytes; GetOverlappedResult(h,ov,out bytes,true); pending=false;
                r.completion_status=Marshal.ReadIntPtr(ov).ToInt64();
                r.original_level=(uint)Marshal.ReadInt32(output,4);
                r.new_level=(uint)Marshal.ReadInt32(output,8);
                r.break_flags=(uint)Marshal.ReadInt32(output,12);
            }
        } finally {
            // Drain the asynchronous request before freeing its buffers. Closing
            // the oplock handle acknowledges any break and releases the writer.
            if(pending) { uint bytes; CancelIoEx(h,ov); GetOverlappedResult(h,ov,out bytes,true); }
            CloseHandle(h);
            if(ov!=IntPtr.Zero) Marshal.FreeHGlobal(ov);
            if(output!=IntPtr.Zero) Marshal.FreeHGlobal(output);
            if(input!=IntPtr.Zero) Marshal.FreeHGlobal(input);
            if(ev!=IntPtr.Zero) CloseHandle(ev);
        }
        if(task!=null) {
            if(!task.Wait(5000)) throw new IOException("Mutation did not finish after releasing oplock");
            r.mutation_error=task.Result; r.mutation_succeeded=task.Result==0;
        }
        return r;
    }
}
'@
$root = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$results = @()
foreach ($share in @(1,7)) {
    foreach ($operation in @('attributes','existing-stream','new-stream','dacl','main-write')) {
        $path = Join-Path $root ("$share-$operation.flac")
        [IO.File]::WriteAllText($path, 'original-audio-fixture')
        Set-Content -LiteralPath $path -Stream custom -Value 'original-stream' -NoNewline
        $acl = Get-Acl -LiteralPath $path
        $acl.SetAccessRuleProtection($true,$true)
        $descriptor = $acl.GetSecurityDescriptorBinaryForm()
        $r = [RefreshOplockProbe]::Run($path,$share,$operation,$descriptor)
        # Strip provider ETS properties before JSON serialization on PowerShell 5.1.
        $results += [pscustomobject]@{
            observation = $r
            audio = [IO.File]::ReadAllText($path)
            attributes = [IO.File]::GetAttributes($path).ToString()
            dacl_protected = (Get-Acl -LiteralPath $path).AreAccessRulesProtected
            custom_stream = [string]::Concat((Get-Content -LiteralPath $path -Stream custom -Raw), '')
            added_stream = $(if ($operation -eq 'new-stream') { [string]::Concat((Get-Content -LiteralPath $path -Stream added -Raw), '') } else { $null })
            streams = @(Get-Item -LiteralPath $path -Force -Stream * | ForEach-Object {
                [pscustomobject]@{ Stream=[string]::Concat($_.Stream, ''); Length=[long]$_.Length }
            })
        }
    }
}
$report = [ordered]@{ fixture_root=$root; powershell=$PSVersionTable.PSVersion.ToString(); os=[Environment]::OSVersion.VersionString; cases=$results }
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'result.json') -Encoding utf8
$report | ConvertTo-Json -Depth 8
if (@($results | Where-Object { -not $_.observation.granted }).Count) { throw 'Some oplock requests were not granted; inspect report' }


# Assert the narrow observed counterexamples plus positive break controls.
foreach ($case in $results) {
    $r = $case.observation
    if (-not $r.mutation_succeeded) { throw "Mutation failed: $($r.operation), error $($r.mutation_error)" }
    if ($r.operation -in @('attributes','existing-stream','new-stream')) {
        if (-not $r.completed_before_release -or $r.break_before_release) { throw 'Unreported-mutation expectation failed' }
    } else {
        if ($r.completed_before_release -or -not $r.break_before_release -or $r.completion_status -ne 0 -or $r.original_level -ne 7) { throw 'Positive oplock break control failed' }
    }
    $expectedAudio = if ($r.operation -eq 'main-write') { 'changedl-audio-fixture' } else { 'original-audio-fixture' }
    $expectedStream = if ($r.operation -eq 'existing-stream') { 'changedl-stream' } else { 'original-stream' }
    if ($case.audio -ne $expectedAudio -or $case.custom_stream -ne $expectedStream) { throw 'Unexpected data stream contents' }
    if ($r.operation -eq 'new-stream' -and $case.added_stream -ne 'changed') { throw 'New stream content mismatch' }
    if ($r.operation -eq 'attributes' -and $case.attributes -ne 'Hidden') { throw 'Attribute mutation missing' }
    if ($r.operation -eq 'dacl' -and -not $case.dacl_protected) { throw 'DACL mutation missing' }
}
