import hashlib
import json
import re
import struct
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

root = Path(__file__).resolve().parents[1]
source_commit = "978b1d9195360e767b39097a7d65ae6ccacd12e8"
outer_path = root / "outputs/Soggfy-Windows-x64-run231.zip"
expected_outer = "6ac306da54af3c1f368642250e40b0999c93ed95017fcd4e5750f6bf74630427"
sha = lambda data: hashlib.sha256(data).hexdigest()
outer_bytes = outer_path.read_bytes()
assert len(outer_bytes) == 3318096
assert sha(outer_bytes) == expected_outer
with zipfile.ZipFile(outer_path) as outer:
    assert outer.testzip() is None
    assert outer.namelist() == ["Soggfy-v3.0.0-rc.88-Windows-x64.zip"]
    inner_bytes = outer.read(outer.namelist()[0])
assert len(inner_bytes) > 0

def parse_pe(data):
    assert data[:2] == b"MZ"
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    assert data[pe:pe+4] == b"PE\0\0"
    machine, sections_count, _, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", data, pe+4)
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    assert magic == 0x20b
    directory = opt + 112 + 8
    import_rva, import_size = struct.unpack_from("<II", data, directory)
    section_base = opt + opt_size
    sections = []
    for index in range(sections_count):
        off = section_base + 40*index
        vsize, va, raw_size, raw = struct.unpack_from("<IIII", data, off+8)
        sections.append((va, max(vsize, raw_size), raw, raw_size))
    def offset(rva):
        for va, span, raw, raw_size in sections:
            if va <= rva < va + span:
                result = raw + rva - va
                assert result < len(data) and rva-va < raw_size
                return result
        raise AssertionError(f"unmapped PE RVA {rva:x}")
    imports = []
    if import_rva:
        pos = offset(import_rva)
        for _ in range(512):
            oft, timestamp, forwarder, name_rva, first = struct.unpack_from("<IIIII", data, pos)
            if not any((oft, timestamp, forwarder, name_rva, first)):
                break
            start = offset(name_rva)
            end = data.index(b"\0", start)
            imports.append(data[start:end].decode("ascii").lower())
            pos += 20
        else:
            raise AssertionError("unterminated PE import table")
    return {"machine": hex(machine), "optional_header": hex(magic), "imports": imports}

with zipfile.ZipFile(__import__("io").BytesIO(inner_bytes)) as package:
    assert package.testzip() is None
    names = package.namelist()
    assert len(names) == 35 and len(set(names)) == 35
    assert all(not PurePosixPath(n).is_absolute() and ".." not in PurePosixPath(n).parts for n in names)
    contents = {name: package.read(name) for name in names}
    manifest = contents["SHA256SUMS.txt"].decode("utf-8").splitlines()
    parsed = {}
    for line in manifest:
        digest, name = line.split("  ", 1)
        assert re.fullmatch(r"[0-9a-f]{64}", digest)
        assert name not in parsed
        assert sha(contents[name]) == digest, name
        parsed[name] = digest
    assert len(parsed) == 7
    helper_digest = contents["VALIDATION_WORKER.txt"].decode("ascii").strip()
    assert re.fullmatch(r"[0-9a-f]{64}", helper_digest)
    helper_name = f"SoggfyValidation-{helper_digest}.exe"
    assert parsed[helper_name] == helper_digest
    assert contents[helper_name] == contents[f"Launcher/{helper_name}"]
    assert sha(contents[helper_name]) == helper_digest
    assert helper_digest.encode("ascii") in contents["version.dll"]
    assert contents["version.dll"] == contents["Launcher/Soggfy.dll"]
    required = {"version.dll", "Launcher/Soggfy.exe", "Launcher/Soggfy.dll", "SpotifyHistory.ini", "Launcher/SpotifyHistory.ini", "BUILDINFO.txt", "SHA256SUMS.txt", "README.md", "Scripts/Install.ps1", "Scripts/Uninstall.ps1", "Scripts/PostProcess.ps1", "Scripts/ConverterProcess.cs", "Scripts/Diagnose.ps1", "Scripts/ValidationWorker.ps1", "VALIDATION_WORKER.txt", "third-party/libvorbis/LICENSE.txt", "third-party/libFLAC/LICENSE.txt"}
    assert required <= set(names)
    source_map = {
      "VERSION":"VERSION", "README.md":"README.md", "CHANGELOG.md":"CHANGELOG.md", "RELEASE_NOTES.md":"RELEASE_NOTES.md", "UPSTREAMS.md":"UPSTREAMS.md", "MILESTONES.md":"MILESTONES.md", "NATIVE_IDENTITY_AUDIT.md":"NATIVE_IDENTITY_AUDIT.md", "METADATA_REFRESH_DESIGN.md":"METADATA_REFRESH_DESIGN.md", "LICENSE":"LICENSE", "SpotifyHistory.ini":"SpotifyHistory.ini",
      "Scripts/Install.ps1":"Scripts/Install.ps1", "Scripts/ValidationWorker.ps1":"Scripts/ValidationWorker.ps1", "Scripts/Uninstall.ps1":"Scripts/Uninstall.ps1", "Scripts/PostProcess.ps1":"Scripts/PostProcess.ps1", "Scripts/ConverterProcess.cs":"Scripts/ConverterProcess.cs", "Scripts/Diagnose.ps1":"Scripts/Diagnose.ps1", "Launcher/SpotifyHistory.ini":"SpotifyHistory.ini",
      "third-party/minhook/LICENSE.txt":"native/vendor/minhook/LICENSE.txt", "third-party/libogg/LICENSE.txt":"native/vendor/libogg/LICENSE.txt", "third-party/cef/LICENSE.txt":"native/vendor/cef/LICENSE.txt", "third-party/soggfy/LICENSE.txt":"native/vendor/soggfy/LICENSE.txt", "third-party/libvorbis/LICENSE.txt":"native/vendor/libvorbis/COPYING", "third-party/libFLAC/LICENSE.txt":"native/vendor/libFLAC/COPYING.Xiph", "third-party/libpng/LICENSE.txt":"native/vendor/libpng/LICENSE", "third-party/zlib/LICENSE.txt":"native/vendor/zlib/LICENSE", "third-party/libjpeg-turbo/LICENSE.txt":"native/vendor/libjpeg-turbo/LICENSE.md", "third-party/libjpeg-turbo/README.ijg":"native/vendor/libjpeg-turbo/README.ijg"
    }
    for archived, source in source_map.items():
        packaged = contents[archived].replace(bytes((13, 10)), bytes((10,)))
        committed = subprocess.run(
            ["git", "show", f"{source_commit}:{source}"],
            cwd=root, check=True, stdout=subprocess.PIPE
        ).stdout.replace(bytes((13, 10)), bytes((10,)))
        assert packaged == committed, archived
    pe = {"version.dll": parse_pe(contents["version.dll"]), "Launcher/Soggfy.exe": parse_pe(contents["Launcher/Soggfy.exe"])}
    assert all(v["machine"] == "0x8664" and v["optional_header"] == "0x20b" for v in pe.values())
    forbidden = {"libwinpthread-1.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll"}
    for item in pe.values():
        assert "msvcrt.dll" in item["imports"]
        assert not (forbidden & set(item["imports"]))

report = {
  "verified_at_local_date": "2026-11-26",
  "source_commit": source_commit,
  "workflow_run_id": 45729218270,
  "artifact_id": 15076671809,
  "artifact_name": "Soggfy-Windows-x64",
  "artifact_size": len(outer_bytes),
  "artifact_sha256": sha(outer_bytes),
  "package": "Soggfy-v3.0.0-rc.88-Windows-x64.zip",
  "package_size": len(inner_bytes),
  "package_sha256": sha(inner_bytes),
  "entry_count": len(names),
  "manifest_entry_count": len(parsed),
  "helper_sha256": helper_digest,
  "source_files_matched_source_commit": len(source_map),
  "pe_checks": pe,
  "zip_integrity": "passed",
  "manifest_hashes": "passed",
  "safety_and_required_content": "passed"
}
(root / "outputs/Soggfy-run231-package-evidence.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
