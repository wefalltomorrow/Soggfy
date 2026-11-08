"""Dependency policy regressions; Windows CI also checks the actual linked PE."""
from pathlib import Path
import runpy

verify = runpy.run_path(str(Path(__file__).resolve().parents[1] /
                          "tools/verify_validation_worker_imports.py"))["verify"]
metadata = """worker.exe:     file format pei-x86-64
architecture: i386:x86-64, flags 0x00000103:
Entry d 0000000000000000 00000000 Delay Import Directory
        DLL Name: KERNEL32.dll
        DLL Name: msvcrt.dll
"""
assert verify(metadata) == {"kernel32.dll", "msvcrt.dll"}
invalid = [
    "",
    metadata.replace("pei-x86-64", "pei-i386"),
    metadata.replace("architecture: i386:x86-64", "architecture: i386"),
    metadata.replace("msvcrt.dll", "ucrtbase.dll"),
    metadata.replace("msvcrt.dll", "api-ms-win-crt-runtime-l1-1-0.dll"),
    metadata.replace("DLL Name: msvcrt.dll", ""),
    metadata.replace("Delay Import Directory", "Unknown Directory"),
    metadata.replace("Entry d 0000000000000000", "Entry d 0000000000001000"),
    metadata.replace("00000000 Delay", "00000020 Delay"),
    metadata + "Entry d 0000000000000000 00000000 Delay Import Directory\n",
]
for dependency in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",
                   "libFLAC.dll", "libvorbis.dll", "unknown.dll"):
    invalid.append(metadata + f"        DLL Name: {dependency}\n")
for candidate in invalid:
    try:
        verify(candidate)
    except ValueError:
        pass
    else:
        raise AssertionError(f"unsafe dependency metadata accepted: {candidate}")
print("PASS: helper architecture, CRT, codec/runtime dependencies and delay imports")
