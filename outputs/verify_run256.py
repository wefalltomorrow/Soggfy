import hashlib
import json
import re
import struct
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

root = Path(__file__).resolve().parents[1]
source_commit = "ac232c3719a971c18849257b28b2eec3426e9f97"
source_parent = "c99be2db84bbe16d7c6367588b32d8a73fa2a8e6"
outer_path = root / "outputs/Soggfy-run256-artifact.zip"
expected_outer_size = 3327152
expected_outer_sha256 = "85b76b2b04902cb6c48b92a20addea96436937584b3d679f5da485afa27081d4"
changed_source_blob_shas = {
    "README.md": "9291df6c4bf848975d28e3dc0c0a0b708007a675",
    "RELEASE_NOTES.md": "af36d8b02876453e7206ac68436c6b761ce78371",
    "MILESTONES.md": "0f98dd95bb6a96bc7a0d2e201e02d288d05925ee",
}
sha256 = lambda data: hashlib.sha256(data).hexdigest()
outer_bytes = outer_path.read_bytes()
assert len(outer_bytes) == expected_outer_size
assert sha256(outer_bytes) == expected_outer_sha256
assert subprocess.run(["git", "cat-file", "-e", source_commit + "^{commit}"], cwd=root).returncode == 0

with zipfile.ZipFile(outer_path) as outer:
    assert outer.testzip() is None
    assert outer.namelist() == ["Soggfy-v3.0.0-rc.88-Windows-x64.zip"]
    inner_bytes = outer.read(outer.namelist()[0])

def parse_pe(data):
    assert data[:2] == b"MZ"
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0"
    machine, count, _, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", data, pe + 4)
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    directory = opt + 112 + 8
    import_rva, _ = struct.unpack_from("<II", data, directory)
    section_base = opt + opt_size
    sections = []
    for index in range(count):
        off = section_base + 40 * index
        vsize, va, raw_size, raw = struct.unpack_from("<IIII", data, off + 8)
        sections.append((va, max(vsize, raw_size), raw, raw_size))
    def offset(rva):
        for va, span, raw, raw_size in sections:
            if va <= rva < va + span:
                pos = raw + rva - va
                assert pos < len(data) and rva - va < raw_size
                return pos
        raise AssertionError(f"unmapped PE RVA {rva:x}")
    imports = []
    if import_rva:
        pos = offset(import_rva)
        for _ in range(512):
            oft, stamp, forwarder, name_rva, first = struct.unpack_from("<IIIII", data, pos)
            if not any((oft, stamp, forwarder, name_rva, first)):
                break
            begin = offset(name_rva)
            end = data.index(bytes([0]), begin)
            imports.append(data[begin:end].decode("ascii").lower())
            pos += 20
        else:
            raise AssertionError("unterminated PE import table")
    return {"machine": hex(machine), "optional_header": hex(magic), "imports": imports}

with zipfile.ZipFile(__import__("io").BytesIO(inner_bytes)) as package:
    assert package.testzip() is None
    names = package.namelist()
    assert len(names) == 35 and len(set(names)) == 35
    assert all(not PurePosixPath(n).is_absolute() and ".." not in PurePosixPath(n).parts and chr(92) not in n and not re.match(r"^[A-Za-z]:", n) for n in names)
    contents = {name: package.read(name) for name in names}
    parsed = {}
    for line in contents["SHA256SUMS.txt"].decode("utf-8").splitlines():
        digest, name = line.split("  ", 1)
        assert re.fullmatch(r"[0-9a-f]{64}", digest) and name not in parsed
        assert sha256(contents[name]) == digest, name
        parsed[name] = digest
    assert len(parsed) == 7
    helper_digest = contents["VALIDATION_WORKER.txt"].decode("ascii").strip()
    helper_name = f"SoggfyValidation-{helper_digest}.exe"
    assert re.fullmatch(r"[0-9a-f]{64}", helper_digest)
    assert parsed[helper_name] == helper_digest
    assert contents[helper_name] == contents[f"Launcher/{helper_name}"]
    assert sha256(contents[helper_name]) == helper_digest
    assert helper_digest.encode("ascii") in contents["version.dll"]
    assert contents["version.dll"] == contents["Launcher/Soggfy.dll"]
    required = {"version.dll", "Launcher/Soggfy.exe", "Launcher/Soggfy.dll", "SpotifyHistory.ini", "Launcher/SpotifyHistory.ini", "BUILDINFO.txt", "SHA256SUMS.txt", "README.md", "Scripts/Install.ps1", "Scripts/Uninstall.ps1", "Scripts/PostProcess.ps1", "Scripts/ConverterProcess.cs", "Scripts/Diagnose.ps1", "Scripts/ValidationWorker.ps1", "VALIDATION_WORKER.txt", "third-party/libvorbis/LICENSE.txt", "third-party/libFLAC/LICENSE.txt"}
    assert required <= set(names)
    source_map = {
        "VERSION":"VERSION", "README.md":"README.md", "CHANGELOG.md":"CHANGELOG.md", "RELEASE_NOTES.md":"RELEASE_NOTES.md", "UPSTREAMS.md":"UPSTREAMS.md", "MILESTONES.md":"MILESTONES.md", "NATIVE_IDENTITY_AUDIT.md":"NATIVE_IDENTITY_AUDIT.md", "METADATA_REFRESH_DESIGN.md":"METADATA_REFRESH_DESIGN.md", "LICENSE":"LICENSE", "SpotifyHistory.ini":"SpotifyHistory.ini",
        "Scripts/Install.ps1":"Scripts/Install.ps1", "Scripts/ValidationWorker.ps1":"Scripts/ValidationWorker.ps1", "Scripts/Uninstall.ps1":"Scripts/Uninstall.ps1", "Scripts/PostProcess.ps1":"Scripts/PostProcess.ps1", "Scripts/ConverterProcess.cs":"Scripts/ConverterProcess.cs", "Scripts/Diagnose.ps1":"Scripts/Diagnose.ps1", "Launcher/SpotifyHistory.ini":"SpotifyHistory.ini",
        "third-party/minhook/LICENSE.txt":"native/vendor/minhook/LICENSE.txt", "third-party/libogg/LICENSE.txt":"native/vendor/libogg/LICENSE.txt", "third-party/cef/LICENSE.txt":"native/vendor/cef/LICENSE.txt", "third-party/soggfy/LICENSE.txt":"native/vendor/soggfy/LICENSE.txt", "third-party/libvorbis/LICENSE.txt":"native/vendor/libvorbis/COPYING", "third-party/libFLAC/LICENSE.txt":"native/vendor/libFLAC/COPYING.Xiph", "third-party/libpng/LICENSE.txt":"native/vendor/libpng/LICENSE", "third-party/zlib/LICENSE.txt":"native/vendor/zlib/LICENSE", "third-party/libjpeg-turbo/LICENSE.txt":"native/vendor/libjpeg-turbo/LICENSE.md", "third-party/libjpeg-turbo/README.ijg":"native/vendor/libjpeg-turbo/README.ijg"
    }
    assert set(changed_source_blob_shas) <= set(source_map.values())
    for archived, source in source_map.items():
        packaged = contents[archived].replace(bytes((13, 10)), bytes((10,)))
        revision = source_commit if source in changed_source_blob_shas else source_parent
        committed = subprocess.run(["git", "show", f"{revision}:{source}"], cwd=root, check=True, stdout=subprocess.PIPE).stdout.replace(bytes((13, 10)), bytes((10,)))
        if source in changed_source_blob_shas:
            git_blob = hashlib.sha1(b"blob " + str(len(committed)).encode("ascii") + bytes([0]) + committed).hexdigest()
            assert git_blob == changed_source_blob_shas[source], (source, git_blob)
        assert packaged == committed, archived
    pe = {"version.dll": parse_pe(contents["version.dll"]), "Launcher/Soggfy.exe": parse_pe(contents["Launcher/Soggfy.exe"])}
    assert all(value["machine"] == "0x8664" and value["optional_header"] == "0x20b" for value in pe.values())
    forbidden = {"libwinpthread-1.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll"}
    for item in pe.values():
        assert "msvcrt.dll" in item["imports"]
        assert not (forbidden & set(item["imports"]))

package_path = root / "outputs/Soggfy-v3.0.0-rc.88-Windows-x64-run256.zip"
package_path.write_bytes(inner_bytes)
report = {
    "verified_at_local_date":"2026-11-26", "source_commit":source_commit, "source_parent_commit":source_parent,
    "changed_source_git_blob_shas":changed_source_blob_shas,
    "github_commit_changed_files":["MILESTONES.md", "README.md", "RELEASE_NOTES.md"],
    "ci_conclusion":"success", "ci_job_conclusions":{"native-tests":"success", "sanitizer-tests":"success", "windows-release-build":"success"},
    "workflow_run_id":45822129144, "artifact_id":15120580115, "artifact_name":"Soggfy-Windows-x64",
    "artifact_size":len(outer_bytes), "artifact_sha256":sha256(outer_bytes), "package":"Soggfy-v3.0.0-rc.88-Windows-x64.zip",
    "package_size":len(inner_bytes), "package_sha256":sha256(inner_bytes), "entry_count":len(names), "manifest_entry_count":len(parsed),
    "helper_sha256":helper_digest, "source_files_matched_source_commit":len(source_map), "pe_checks":pe,
    "zip_integrity":"passed", "manifest_hashes":"passed", "safety_and_required_content":"passed"
}
(root / "outputs/Soggfy-run256-package-evidence.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
