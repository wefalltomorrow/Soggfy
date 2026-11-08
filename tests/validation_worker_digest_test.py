"""Failed generation cannot reuse a stale build trust header."""
import hashlib
from pathlib import Path
import runpy
import tempfile

generate = runpy.run_path(str(Path(__file__).resolve().parents[1] /
                            "tools/generate_validation_worker_digest.py"))["generate"]
with tempfile.TemporaryDirectory(prefix="soggfy-build-trust-") as folder:
    root = Path(folder)
    worker, header = root / "worker.exe", root / "digest.h"
    worker.write_bytes(b"first build")
    generate(worker, header)
    first = header.read_bytes()
    assert hashlib.sha256(worker.read_bytes()).hexdigest().encode() in first
    generate(worker, header)
    assert header.read_bytes() == first
    worker.write_bytes(b"next build")
    generate(worker, header)
    assert header.read_bytes() != first
    assert hashlib.sha256(worker.read_bytes()).hexdigest().encode() in header.read_bytes()
    for invalid in ("missing", "empty", "oversized"):
        header.write_bytes(first)
        worker.unlink(missing_ok=True)
        if invalid != "missing":
            with worker.open("wb") as output:
                output.truncate(64 * 1024 * 1024 + 1 if invalid == "oversized" else 0)
        try:
            generate(worker, header)
        except (OSError, ValueError):
            pass
        else:
            raise AssertionError(invalid)
        assert not header.exists(), invalid
    worker.write_bytes(b"retain input")
    try:
        generate(worker, worker)
    except ValueError:
        pass
    else:
        raise AssertionError("input/output alias accepted")
    assert worker.read_bytes() == b"retain input"
print("PASS: deterministic build digest, changed build, failed generation and stale-header removal")
