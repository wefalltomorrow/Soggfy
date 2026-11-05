using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace Soggfy {
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
        private IntPtr job, process;
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
        private static extern bool CreateProcess(string exe, StringBuilder command, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string cwd, ref Startup startup, out ProcessInfo info);
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
            try {
                job = CreateJobObject(IntPtr.Zero, null);
                if (job == IntPtr.Zero) throw new Win32Exception();
                var limits = new Limits(); limits.basic.flags = 0x2000;
                if (!SetInformationJobObject(job, 9, ref limits, Marshal.SizeOf(limits))) throw new Win32Exception();
                var command = new StringBuilder(Quote(exe));
                foreach (string arg in arguments) command.Append(' ').Append(Quote(arg));
                var startup = new Startup(); startup.cb = Marshal.SizeOf(startup);
                if (!CreateProcess(exe, command, IntPtr.Zero, IntPtr.Zero, false, 0x08000004, IntPtr.Zero, cwd, ref startup, out info)) throw new Win32Exception();
                process = info.process;
                if (!AssignProcessToJobObject(job, process) || ResumeThread(info.thread) == uint.MaxValue) {
                    int error = Marshal.GetLastWin32Error();
                    TerminateProcess(process, 1);
                    throw new Win32Exception(error);
                }
            } catch { Dispose(); throw; }
            finally { if (info.thread != IntPtr.Zero) CloseHandle(info.thread); }
        }
        public bool Wait(uint milliseconds) {
            uint result = WaitForSingleObject(process, milliseconds);
            if (result == uint.MaxValue) throw new Win32Exception();
            return result == 0;
        }
        public uint ExitCode { get { uint code; if (!GetExitCodeProcess(process, out code)) throw new Win32Exception(); return code; } }
        public void Dispose() {
            if (job != IntPtr.Zero) { TerminateJobObject(job, 1); CloseHandle(job); job = IntPtr.Zero; }
            if (process != IntPtr.Zero) { WaitForSingleObject(process, 2000); CloseHandle(process); process = IntPtr.Zero; }
            GC.SuppressFinalize(this);
        }
        ~ConverterProcess() { Dispose(); }
    }
}
