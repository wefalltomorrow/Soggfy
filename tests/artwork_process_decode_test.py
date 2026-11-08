"""Exercise real image decoding through the build-pinned Windows supervisor."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

def native_path(path):
    path=Path(path).resolve()
    if os.name=='nt':return str(path)
    mapper=shutil.which('cygpath') or shutil.which('wslpath')
    if not mapper:raise RuntimeError('Windows artwork test requires cygpath or wslpath')
    return subprocess.check_output([mapper,'-w',str(path)],text=True).strip()
exe=str(Path(sys.argv[1]).resolve())
worker=native_path(sys.argv[2])
def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
def png(depth=8,color=2,raw=b'\0\xff\0\0'):
    return (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,depth,color,0,0,0))+
            chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
accepted=rejected=0
with tempfile.TemporaryDirectory(prefix='soggfy-artwork-') as directory:
    path=Path(directory)/'artwork.bin'
    def check(label,data,expected=None):
        global accepted,rejected
        path.write_bytes(data)
        p=subprocess.run([exe,native_path(path),worker],capture_output=True,text=True,timeout=30)
        assert p.returncode==(0 if expected else 1),(label,p.returncode,p.stdout,p.stderr)
        assert not p.stderr,(label,p.stderr)
        if expected:
            assert list(map(int,p.stdout.split()))==expected,(label,p.stdout)
            accepted+=1
        else:rejected+=1
    valid=png()
    check('empty',b'')
    check('png',valid,[1,1,8,3,0])
    check('png-gray1',png(1,0,b'\0\0'),[1,1,1,1,0])
    check('png-gray16',png(16,0,b'\0\0\0'),[1,1,16,1,0])
    check('png-rgba',png(8,6,b'\0\xff\0\0\xff'),[1,1,8,4,0])
    for label,data in [('png-end',valid[:-12]),('png-trailing',valid+b'x'),
                       ('png-short-row',png(raw=b'\0\xff')),('png-extra-row',png(raw=b'\0\xff\0\0'*2)),
                       ('png-filter',png(raw=b'\5\xff\0\0'))]:
        check(label,data)
    changed=bytearray(valid);changed[29]^=1
    check('png-crc',changed)
    for n in [1,8,20,len(valid)//2,len(valid)-1]:check('png-prefix-'+str(n),valid[:n])
    manifest=json.loads(Path(__file__).with_name('jpeg_fixtures.json').read_text())
    for name,item in manifest['fixtures'].items():
        data=base64.b64decode(item['base64'],validate=True)
        assert hashlib.sha256(data).hexdigest()==item['sha256']
        if name=='large-progressive':
            check(name,data);continue
        check(name,data,[17,19,8,1 if name.startswith('gray') else 3,int('progressive' in name)])
        check(name+'-eoi',data[:-2]);check(name+'-trailing',data+b'x')
        check(name+'-prefix',data[:len(data)//2])
    check('input-limit',valid+bytes(4*1024*1024+1-len(valid)))
    path.write_bytes(valid)
    p=subprocess.run([exe,'--faults',native_path(path),worker],capture_output=True,text=True,timeout=30)
    assert p.returncode==0 and not p.stderr,(p.returncode,p.stdout,p.stderr)
    print(p.stdout.strip())
print(f'PASS: supervised artwork {accepted} valid and {rejected} malformed/resource-limit cases')
