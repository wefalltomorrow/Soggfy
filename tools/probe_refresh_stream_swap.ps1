[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference="Stop"
Set-StrictMode -Version Latest
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
public static class RefreshStreamSwapProbe {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 static extern IntPtr CreateFileW(string path,uint access,uint share,IntPtr sec,uint creation,uint flags,IntPtr t);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool CloseHandle(IntPtr h);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetFileAttributesW(string p,uint attrs);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool MoveFileExW(string from,string to,uint flags);
 [StructLayout(LayoutKind.Sequential)] struct Info { public uint attrs,c0,c1,a0,a1,w0,w1,volume,sizeHigh,sizeLow,links,indexHigh,indexLow; }
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetFileInformationByHandle(IntPtr h,out Info info);
 public static string Identity(string path) {
   IntPtr h=CreateFileW(path,0x80,7,IntPtr.Zero,3,0,IntPtr.Zero);
   if(h==new IntPtr(-1))throw new IOException("identity open failed");
   try {Info i;if(!GetFileInformationByHandle(h,out i))throw new IOException("identity read failed");
    return i.volume.ToString("x8")+":"+i.indexHigh.ToString("x8")+i.indexLow.ToString("x8");
   }finally{CloseHandle(h);}
 }
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetFileInformationByHandle(IntPtr h,int cls,IntPtr info,uint size);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteFile(IntPtr h,byte[] b,uint count,out uint written,IntPtr ov);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool FlushFileBuffers(IntPtr h);
 public sealed class Result { public uint source_share,staging_share; public bool competing_writer; public int source_open,staging_open,source_move=-1,staging_move=-1,rollback=-1,namespace_rename=-1; }
 static int Rename(IntPtr h,string name,bool replace) {
   byte[] text=System.Text.Encoding.Unicode.GetBytes(name);
   int offset=IntPtr.Size==8 ? 20 : 12;
   IntPtr p=Marshal.AllocHGlobal(offset+text.Length+4);
   try { Marshal.Copy(new byte[offset+text.Length+4],0,p,offset+text.Length+4);
     Marshal.WriteByte(p,replace ? (byte)1 : (byte)0);
     Marshal.WriteInt32(p,offset-4,text.Length);Marshal.Copy(text,0,IntPtr.Add(p,offset),text.Length);
     return SetFileInformationByHandle(h,3,p,(uint)(offset+text.Length+4)) ? 0 : Marshal.GetLastWin32Error();
   } finally {Marshal.FreeHGlobal(p);}
 }
 static void Write(IntPtr h,string text) { uint written;byte[] b=System.Text.Encoding.UTF8.GetBytes(text);
   if(!WriteFile(h,b,(uint)b.Length,out written,IntPtr.Zero)||written!=b.Length||!FlushFileBuffers(h))throw new IOException("write failed");
 }
 public static Result Run(string path,uint sourceShare,uint stagingShare,bool competing) {
   Result r=new Result();r.source_share=sourceShare;r.staging_share=stagingShare;r.competing_writer=competing;
   IntPtr source=CreateFileW(path,0x80010000,sourceShare,IntPtr.Zero,3,0,IntPtr.Zero);
   if(source==new IntPtr(-1)){r.source_open=Marshal.GetLastWin32Error();return r;}
   IntPtr stage=new IntPtr(-1);
   try {
     stage=CreateFileW(path+":candidate",0xc0010000,stagingShare,IntPtr.Zero,1,0,IntPtr.Zero);
     if(stage==new IntPtr(-1)){r.staging_open=Marshal.GetLastWin32Error();return r;}
     Write(stage,"validated-retagged-audio");
     // Edits after source/staging handles are acquired must survive publication.
     foreach(string stream in new string[]{":custom",":added"}) {
       IntPtr metadata=CreateFileW(path+stream,0x40000000,7,IntPtr.Zero,2,0,IntPtr.Zero);
       if(metadata==new IntPtr(-1))throw new IOException("metadata stream open failed");
       try{Write(metadata,"concurrent-stream");}finally{CloseHandle(metadata);}
     }
     if(!SetFileAttributesW(path,2))throw new IOException("attribute edit failed");
     if(sourceShare==1) {
       r.namespace_rename=MoveFileExW(path,path+".moved",0) ? 0 : Marshal.GetLastWin32Error();
       if(r.namespace_rename!=32)throw new IOException("source name guard failed");
     }
     r.source_move=Rename(source,":previous:$DATA",false);
     if(r.source_move!=0)return r;
     if(competing) {
       IntPtr other=CreateFileW(path,0x40000000,7,IntPtr.Zero,3,0,IntPtr.Zero);
       if(other==new IntPtr(-1))throw new IOException("competitor open: "+Marshal.GetLastWin32Error());
       try {Write(other,"competing-audio");}finally{CloseHandle(other);}
     }
     r.staging_move=Rename(stage,"::$DATA",true);
     if(r.staging_move!=0)r.rollback=Rename(source,"::$DATA",true);
   }finally{if(stage!=new IntPtr(-1))CloseHandle(stage);CloseHandle(source);}
   return r;
 }
}
'@
$root=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString("N"))
[IO.Directory]::CreateDirectory($root)|Out-Null
$cases=@()
foreach($sourceShare in @(1,5)){foreach($stageShare in @(1,5)){foreach($competing in @($false,$true)){
 $path=Join-Path $root ("$sourceShare-$stageShare-$competing.flac")
 [IO.File]::WriteAllText($path,"original-audio")
 Set-Content -LiteralPath $path -Stream custom -Value "custom-stream" -NoNewline
 $acl=Get-Acl -LiteralPath $path;$acl.SetAccessRuleProtection($true,$true);Set-Acl -LiteralPath $path -AclObject $acl
 $before=(Get-Acl -LiteralPath $path).Sddl
 $identity=[RefreshStreamSwapProbe]::Identity($path)
 $r=[RefreshStreamSwapProbe]::Run($path,$sourceShare,$stageShare,$competing)
 $streams=@(Get-Item -LiteralPath $path -Force -Stream * | ForEach-Object { [pscustomobject]@{name=[string]::Concat($_.Stream,"");length=[long]$_.Length;content=[string]::Concat((Get-Content -LiteralPath $path -Stream $_.Stream -Raw),"")} })
 $cases += [pscustomobject]@{result=$r;path=$path;identity_unchanged=[RefreshStreamSwapProbe]::Identity($path) -eq $identity;attributes=[IO.File]::GetAttributes($path).ToString();dacl_unchanged=(Get-Acl -LiteralPath $path).Sddl -eq $before;streams=$streams}
}}}
$report=[ordered]@{root=$root;powershell=$PSVersionTable.PSVersion.ToString();cases=$cases}
$report|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $root "result.json") -Encoding utf8
$report|ConvertTo-Json -Depth 7

# Assert only these isolated compatibility observations, not crash recovery.
foreach($case in $cases) {
 $r=$case.result
 if(-not $case.dacl_unchanged -or -not $case.identity_unchanged -or $r.source_open -ne 0) {throw 'File metadata/identity mismatch'}
 $streams=@{};foreach($stream in $case.streams){$streams[$stream.name]=$stream.content}
 if($r.staging_share -eq 1) {
   if($r.staging_open -ne 32 -or $r.source_move -ne -1 -or $streams[':$DATA'] -ne 'original-audio' -or $streams['custom'] -ne 'custom-stream' -or $streams.Count -ne 2) {throw 'Failed staging changed source'}
 } else {
   if($r.staging_open -ne 0 -or $r.source_move -ne 0 -or $streams['custom'] -ne 'concurrent-stream' -or $streams['added'] -ne 'concurrent-stream' -or -not ([IO.FileAttributes]$case.attributes -band [IO.FileAttributes]::Hidden) -or $streams['previous'] -ne 'original-audio') {throw 'Metadata or original stream lost'}
   if($r.source_share -eq 1 -and $r.namespace_rename -ne 32){throw 'Namespace guard failed'}
   if($r.competing_writer) {
     if($r.staging_move -ne 87 -or $r.rollback -ne 87 -or $streams[':$DATA'] -ne 'competing-audio' -or $streams['candidate'] -ne 'validated-retagged-audio' -or $streams.Count -ne 5) {throw 'Competing default stream was not preserved'}
   } else {
     if($r.staging_move -ne 0 -or $streams[':$DATA'] -ne 'validated-retagged-audio' -or $streams.Count -ne 4) {throw 'Stream publication failed'}
   }
 }
}
