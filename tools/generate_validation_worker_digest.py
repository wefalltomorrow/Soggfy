"""Bind an already-finalized helper to its caller at build time, never runtime."""
import hashlib
from pathlib import Path
import sys


def generate(worker: Path, destination: Path) -> None:
    # A failed build must not leave a stale header available to its caller.
    if worker.resolve() == destination.resolve():
        raise ValueError("worker and generated header must be distinct")
    destination.unlink(missing_ok=True)
    with worker.open("rb") as source:
        digest = hashlib.sha256()
        size = 0
        for block in iter(lambda: source.read(65536), b""):
            size += len(block)
            if size > 64 * 1024 * 1024:
                raise ValueError("helper exceeds trust size limit")
            digest.update(block)
    if not size:
        raise ValueError("empty helper")
    destination.write_text(
        "// Generated from the finalized build artifact; do not edit.\n"
        "#pragma once\nnamespace history {\n"
        f'inline constexpr char ValidationWorkerBuildSha256[] = "{digest.hexdigest()}";\n'
        "}\n", encoding="ascii", newline="\n")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_validation_worker_digest.py HELPER HEADER")
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
