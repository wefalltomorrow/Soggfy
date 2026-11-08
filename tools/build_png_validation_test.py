"""Build bounded PNG validation tests with pinned libpng/zlib sources."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

os.chdir(Path(__file__).resolve().parent.parent)
dest=Path(sys.argv[1]);dest.mkdir(parents=True,exist_ok=True)
cc=os.environ.get('CC','gcc')
flags=['-O2']
if os.environ.get('SGF_SANITIZE')=='1':
    flags=['-g','-O1','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-sanitize-recover=all']
png=Path('native/vendor/libpng');z=Path('native/vendor/zlib')
for root in [png,z]:
    manifest=json.loads((root/'SOURCE.json').read_text())
    for name,digest in manifest['files'].items():
        if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Pinned PNG dependency differs: '+str(root/name))
shutil.copyfile(png/'scripts/pnglibconf.h.prebuilt',dest/'pnglibconf.h')
includes=['-I'+str(dest),'-I'+str(png),'-I'+str(z)]
definitions=['-DPNG_INTEL_SSE_OPT=0']
sources=[png/(n+'.c') for n in 'png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil pngset pngtrans pngwio pngwrite pngwtran pngwutil'.split()]
sources += [z/(n+'.c') for n in 'adler32 crc32 inflate inftrees inffast zutil deflate trees compress uncompr'.split()]
objects=[]
for source in sources:
    obj=dest/(source.stem+'.o')
    subprocess.run([cc,'-std=c11',*flags,*includes,*definitions,'-c',str(source),'-o',str(obj)],check=True)
    objects.append(str(obj))
subprocess.run([cc,'-std=c11','-UNDEBUG','-Wall','-Wextra','-Werror',*flags,*includes,*definitions,
                'native/png_validation.c','tests/png_validation_test.c',*objects,'-lm',
                '-o',str(dest/'png-validation-test.exe')],check=True)
