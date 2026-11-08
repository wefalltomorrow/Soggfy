using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace Soggfy {
    // Event callbacks run on CLR threads, never on a PowerShell runspace.
    // Bounded coalescing survives events arriving while FFmpeg is busy.
    public sealed class ConversionWatch : IDisposable {
        private readonly object gate = new object();
        private readonly System.Collections.Generic.HashSet<string> paths =
            new System.Collections.Generic.HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private readonly FileSystemWatcher watcher;
        private bool rescan = true, disposed;
        public ConversionWatch(string root) {
            watcher = new FileSystemWatcher(root);
            try {
            watcher.IncludeSubdirectories = true;
            watcher.NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName |
                NotifyFilters.LastWrite | NotifyFilters.Size;
            watcher.Created += Changed; watcher.Changed += Changed; watcher.Deleted += Changed;
            watcher.Renamed += Renamed; watcher.Error += Failed;
            watcher.EnableRaisingEvents = true;
            } catch { watcher.Dispose(); throw; }
        }
        private void Changed(object sender, FileSystemEventArgs e) { Record(e.FullPath); }
        private void Renamed(object sender, RenamedEventArgs e) { Record(e.OldFullPath); Record(e.FullPath); }
        private void Failed(object sender, ErrorEventArgs e) { lock (gate) { if (!disposed) rescan = true; } }
        private void Record(string path) {
            lock (gate) {
                if (disposed) return;
                // Renaming a populated directory need not emit child events.
                if (Directory.Exists(path)) { rescan = true; return; }
                string extension = Path.GetExtension(path);
                if (!String.Equals(extension, ".ogg", StringComparison.OrdinalIgnoreCase) &&
                    !String.Equals(extension, ".flac", StringComparison.OrdinalIgnoreCase)) return;
                if (paths.Count >= 4096 && !paths.Contains(path)) { rescan = true; return; }
                paths.Add(path);
            }
        }
        public string[] Drain() {
            lock (gate) {
                var result = new string[paths.Count]; paths.CopyTo(result); paths.Clear(); return result;
            }
        }
        public bool TakeRescan() { lock (gate) { bool result = rescan; rescan = false; return result; } }
        public void Dispose() {
            lock (gate) { if (disposed) return; disposed = true; paths.Clear(); }
            watcher.Dispose();
        }
    }

    public sealed class ConversionSource : IDisposable {
        private FileStream stream;
        private readonly string path;
        private FileInfo original;
        [StructLayout(LayoutKind.Sequential)]
        private struct FileInfo {
            public uint attributes;
            public System.Runtime.InteropServices.ComTypes.FILETIME creation, access, write;
            public uint volume, sizeHigh, sizeLow, links, indexHigh, indexLow;
        }
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetFileInformationByHandle(SafeFileHandle handle, out FileInfo info);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern SafeFileHandle CreateFile(string path, uint access, uint share, IntPtr security, uint mode, uint flags, IntPtr template);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetFileInformationByHandle(SafeFileHandle handle, int kind, ref byte info, uint size);
        public ConversionSource(string path) {
            this.path = path;
            stream = File.Open(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            if (!GetFileInformationByHandle(stream.SafeFileHandle, out original)) {
                int error = Marshal.GetLastWin32Error(); Dispose(); throw new Win32Exception(error);
            }
        }
        public long Length { get { return stream.Length; } }
        public void DeleteValidatedSource() {
            // FFmpeg readers do not necessarily share DELETE access. Reopen
            // after decoding, then compare identity and delete by that handle.
            Dispose();
            using (var handle = CreateFile(path, 0x80010000, 1, IntPtr.Zero, 3, 0, IntPtr.Zero)) {
                if (handle.IsInvalid) throw new Win32Exception();
                FileInfo current;
                if (!GetFileInformationByHandle(handle, out current)) throw new Win32Exception();
                if (current.volume != original.volume || current.indexHigh != original.indexHigh ||
                    current.indexLow != original.indexLow || current.sizeHigh != original.sizeHigh ||
                    current.sizeLow != original.sizeLow || current.write.dwHighDateTime != original.write.dwHighDateTime ||
                    current.write.dwLowDateTime != original.write.dwLowDateTime ||
                    current.creation.dwHighDateTime != original.creation.dwHighDateTime ||
                    current.creation.dwLowDateTime != original.creation.dwLowDateTime)
                    throw new IOException("Source changed after conversion; replacement retained.");
                byte remove = 1;
                if (!SetFileInformationByHandle(handle, 4, ref remove, 1)) throw new Win32Exception();
            }
        }
        public void Dispose() { if (stream != null) { stream.Dispose(); stream = null; } }
    }

    // A suspended launch prevents a wrapper from escaping supervision before
    // assignment. Closing the job also stops descendants if PowerShell exits.
    public sealed class ConverterProcess : IDisposable {
        private IntPtr job, process, errorReader;
        private bool reportedError;
        [StructLayout(LayoutKind.Sequential)]
        private struct Security { public int size; public IntPtr descriptor; public int inherit; }
        [StructLayout(LayoutKind.Sequential)]
        private struct ExtendedStartup { public Startup startup; public IntPtr attributes; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct Startup {
            public int cb; public string reserved, desktop, title;
            public int x, y, width, height, charsX, charsY, fill, flags;
            public short show, reservedSize; public IntPtr reservedBytes, input, output, error;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct ProcessInfo { public IntPtr process, thread; public int pid, tid; }
        [StructLayout(LayoutKind.Sequential)]
        private struct BasicLimits {
            public long processTime, jobTime; public uint flags;
            public UIntPtr minWorking, maxWorking; public uint active;
            public UIntPtr affinity; public uint priority, scheduling;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct Limits {
            public BasicLimits basic;
            public ulong readOps, writeOps, otherOps, readBytes, writeBytes, otherBytes;
            public UIntPtr processMemory, jobMemory, peakProcess, peakJob;
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateJobObject(IntPtr attributes, string name);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetInformationJobObject(IntPtr job, int kind, ref Limits limits, int size);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool CreateProcess(string exe, StringBuilder command, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string cwd, ref ExtendedStartup startup, out ProcessInfo info);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CreatePipe(out IntPtr reader, out IntPtr writer, ref Security security, uint size);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetHandleInformation(IntPtr handle, uint mask, uint flags);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateFile(string name, uint access, uint share, ref Security security, uint mode, uint flags, IntPtr template);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool InitializeProcThreadAttributeList(IntPtr list, int count, uint flags, ref IntPtr size);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool UpdateProcThreadAttribute(IntPtr list, uint flags, IntPtr attribute, IntPtr value, IntPtr size, IntPtr previous, IntPtr returned);
        [DllImport("kernel32.dll")] private static extern void DeleteProcThreadAttributeList(IntPtr list);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool PeekNamedPipe(IntPtr pipe, IntPtr buffer, uint size, IntPtr read, out uint available, IntPtr remaining);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint ResumeThread(IntPtr thread);
        [DllImport("kernel32.dll")] private static extern bool TerminateProcess(IntPtr process, uint code);
        [DllImport("kernel32.dll")] private static extern bool TerminateJobObject(IntPtr job, uint code);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern uint WaitForSingleObject(IntPtr handle, uint ms);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool GetExitCodeProcess(IntPtr process, out uint code);
        [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);

        // Windows argv quoting, including empty arguments and trailing slashes.
        public static string Quote(string value) {
            var result = new StringBuilder("\""); int slashes = 0;
            foreach (char c in value) {
                if (c == '\\') { ++slashes; continue; }
                result.Append('\\', c == '"' ? slashes * 2 + 1 : slashes);
                result.Append(c); slashes = 0;
            }
            return result.Append('\\', slashes * 2).Append('"').ToString();
        }
        public ConverterProcess(string exe, string[] arguments, string cwd) {
            ProcessInfo info = new ProcessInfo();
            IntPtr writer = IntPtr.Zero, nullFile = new IntPtr(-1), attributes = IntPtr.Zero, handles = IntPtr.Zero;
            bool initialized = false;
            try {
                job = CreateJobObject(IntPtr.Zero, null);
                if (job == IntPtr.Zero) throw new Win32Exception();
                var limits = new Limits(); limits.basic.flags = 0x2000;
                if (!SetInformationJobObject(job, 9, ref limits, Marshal.SizeOf(limits))) throw new Win32Exception();
                var command = new StringBuilder(Quote(exe));
                foreach (string arg in arguments) command.Append(' ').Append(Quote(arg));
                var security = new Security(); security.size = Marshal.SizeOf(security); security.inherit = 1;
                if (!CreatePipe(out errorReader, out writer, ref security, 4096) ||
                    !SetHandleInformation(errorReader, 1, 0)) throw new Win32Exception();
                nullFile = CreateFile("NUL", 0xc0000000, 3, ref security, 3, 0, IntPtr.Zero);
                if (nullFile == new IntPtr(-1)) throw new Win32Exception();
                IntPtr size = IntPtr.Zero;
                InitializeProcThreadAttributeList(IntPtr.Zero, 1, 0, ref size);
                attributes = Marshal.AllocHGlobal(size);
                if (!InitializeProcThreadAttributeList(attributes, 1, 0, ref size)) throw new Win32Exception();
                initialized = true;
                handles = Marshal.AllocHGlobal(2 * IntPtr.Size);
                Marshal.WriteIntPtr(handles, writer); Marshal.WriteIntPtr(handles, IntPtr.Size, nullFile);
                if (!UpdateProcThreadAttribute(attributes, 0, new IntPtr(0x20002), handles,
                    new IntPtr(2 * IntPtr.Size), IntPtr.Zero, IntPtr.Zero)) throw new Win32Exception();
                var startup = new ExtendedStartup(); startup.startup.cb = Marshal.SizeOf(startup);
                startup.attributes = attributes; startup.startup.flags = 0x100;
                startup.startup.input = startup.startup.output = nullFile; startup.startup.error = writer;
                if (!CreateProcess(exe, command, IntPtr.Zero, IntPtr.Zero, true, 0x08080004, IntPtr.Zero, cwd, ref startup, out info)) throw new Win32Exception();
                process = info.process;
                if (!AssignProcessToJobObject(job, process) || ResumeThread(info.thread) == uint.MaxValue) {
                    int error = Marshal.GetLastWin32Error();
                    TerminateProcess(process, 1);
                    throw new Win32Exception(error);
                }
            } catch { Dispose(); throw; }
            finally {
                if (info.thread != IntPtr.Zero) CloseHandle(info.thread);
                if (initialized) DeleteProcThreadAttributeList(attributes);
                if (attributes != IntPtr.Zero) Marshal.FreeHGlobal(attributes);
                if (handles != IntPtr.Zero) Marshal.FreeHGlobal(handles);
                if (writer != IntPtr.Zero) CloseHandle(writer);
                if (nullFile != new IntPtr(-1)) CloseHandle(nullFile);
            }
        }
        public bool Wait(uint milliseconds) {
            uint result = WaitForSingleObject(process, milliseconds);
            if (result == uint.MaxValue) throw new Win32Exception();
            uint available;
            if (!PeekNamedPipe(errorReader, IntPtr.Zero, 0, IntPtr.Zero, out available, IntPtr.Zero)) {
                int error = Marshal.GetLastWin32Error();
                if (error != 109) throw new Win32Exception(error); // closed pipe
            } else if (available != 0) {
                // Fail on the first error byte. No unbounded log storage or
                // drain thread; a pipe-filling child is terminated as well.
                reportedError = true; TerminateJobObject(job, 1);
            }
            return result == 0;
        }
        public uint ExitCode { get { uint code; if (!GetExitCodeProcess(process, out code)) throw new Win32Exception(); return reportedError && code == 0 ? 1 : code; } }
        public void Dispose() {
            if (job != IntPtr.Zero) { TerminateJobObject(job, 1); CloseHandle(job); job = IntPtr.Zero; }
            if (process != IntPtr.Zero) { WaitForSingleObject(process, 2000); CloseHandle(process); process = IntPtr.Zero; }
            if (errorReader != IntPtr.Zero) { CloseHandle(errorReader); errorReader = IntPtr.Zero; }
            GC.SuppressFinalize(this);
        }
        ~ConverterProcess() { Dispose(); }
    }
}
