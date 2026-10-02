#!/usr/bin/env python3
"""Create a deterministic Windows x64 release package."""
import hashlib
from pathlib import Path
import shutil
import zipfile

VERSION = '3.0.0-rc.1'
root = Path(__file__).resolve().parent
out = root / 'dist'
out.mkdir(exist_ok=True)
release_name = f'Soggfy-v{VERSION}-Windows-x64.zip'

for old_archive in out.glob('Soggfy-v*-Windows-x64.zip'):
    if old_archive.name != release_name:
        old_archive.unlink()

shutil.copy2(root / 'build' / 'version.dll', out / 'version.dll')
digest = hashlib.sha256((out / 'version.dll').read_bytes()).hexdigest()
(out / 'SHA256SUMS.txt').write_text(f'{digest}  version.dll\n', encoding='utf-8')

files = [
    ('version.dll', out / 'version.dll'),
    ('SHA256SUMS.txt', out / 'SHA256SUMS.txt'),
    ('README.md', root / 'README.md'),
    ('CHANGELOG.md', root / 'CHANGELOG.md'),
    ('UPSTREAMS.md', root / 'UPSTREAMS.md'),
    ('LICENSE', root / 'LICENSE'),
    ('SpotifyHistory.ini', root / 'SpotifyHistory.ini'),
    ('Scripts/Install.ps1', root / 'Scripts' / 'Install.ps1'),
    ('Scripts/Uninstall.ps1', root / 'Scripts' / 'Uninstall.ps1'),
    ('Scripts/PostProcess.ps1', root / 'Scripts' / 'PostProcess.ps1'),
]
for vendor in ['minhook', 'libogg', 'cef', 'soggfy']:
    files.append((f'third-party/{vendor}/LICENSE.txt',
                  root / 'native' / 'vendor' / vendor / 'LICENSE.txt'))

with zipfile.ZipFile(out / release_name, 'w') as archive:
    for arcname, path in files:
        info = zipfile.ZipInfo(arcname, (1980, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, path.read_bytes())

print(f'DLL: {out / "version.dll"}')
print(f'SHA256: {digest}')
print(f'ZIP: {out / release_name}')
