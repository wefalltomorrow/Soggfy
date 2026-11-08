"""Check GNU objdump metadata for the MinGW validation helper before binding it."""
import os
from pathlib import Path
import re
import subprocess
import sys


def verify(metadata: str) -> set[str]:
    if not re.search(r"file format pei-x86-64(?:\s|$)", metadata):
        raise ValueError("validation helper must be an x64 PE executable")
    if not re.search(r"^architecture: i386:x86-64,", metadata, re.M):
        raise ValueError("unexpected helper architecture")
    # Delay-loaded dependencies must not bypass the ordinary import allowlist.
    delay = re.findall(r"^Entry d\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+Delay Import Directory", metadata, re.M)
    if len(delay) != 1 or any(int(value, 16) for value in delay[0]):
        raise ValueError("missing or nonempty delay-import directory")
    imports = {name.strip().lower() for name in
               re.findall(r"^\s*DLL Name:\s*([^\r\n]+)", metadata, re.M)}
    expected = {"kernel32.dll", "msvcrt.dll"}
    if imports != expected:
        raise ValueError(f"unexpected helper imports: {sorted(imports)}; expected {sorted(expected)}")
    return imports


def inspect(worker: Path, objdump: str) -> None:
    environment = dict(os.environ, LC_ALL="C")
    result = subprocess.run([objdump, "-f", "-p", str(worker)], check=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, env=environment)
    imports = verify(result.stdout)
    print(f"PASS: MinGW x64 validation helper imports: {', '.join(sorted(imports))}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_validation_worker_imports.py HELPER")
    inspect(Path(sys.argv[1]), os.environ.get("OBJDUMP", "objdump"))
