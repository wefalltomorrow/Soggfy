"""Build offline JPEG tests from pinned source; no downloads or CMake required."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

os.chdir(Path(__file__).resolve().parent.parent)
dest=Path(sys.argv[1]);dest.mkdir(parents=True,exist_ok=True)
cc=os.environ.get('CC','gcc')
flags=['-O2']
if os.environ.get('SGF_SANITIZE')=='1':
    flags=['-g','-O1','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-sanitize-recover=all']
vendor=Path('native/vendor/libjpeg-turbo')
manifest=json.loads((vendor/'SOURCE.json').read_text())
for name,digest in manifest['files'].items():
    if hashlib.sha256((vendor/name).read_bytes()).hexdigest()!=digest:
        raise RuntimeError('Pinned JPEG dependency differs: '+name)
# Portable non-SIMD configuration for the two supported 64-bit GCC/Clang hosts.
# Keep optional intrinsics disabled so LLP64 and LP64 use the same source paths.
settings=dict(BUILD='soggfy-offline',HIDDEN='',INLINE='__inline__ __attribute__((always_inline))',
              THREAD_LOCAL='__thread',CMAKE_PROJECT_NAME='libjpeg-turbo',VERSION='3.2.1',
              SIZE_T='8',SIMD_ARCHITECTURE='NONE',JPEG_LIB_VERSION='62',
              LIBJPEG_TURBO_VERSION_NUMBER='3002001',COPYRIGHT_YEAR='2026')
for name in ['jconfig.h','jconfigint.h','jversion.h']:
    template=(vendor/'src'/(name+'.in')).read_text()
    template=re.sub(r'^#cmakedefine\s+(\w+).*$',r'/* #undef \1 */',template,flags=re.M)
    template=re.sub(r'@(\w+)@',lambda m:settings[m[1]],template)
    if name=='jconfigint.h':
        template+='\n#include <stddef.h>\n_Static_assert(sizeof(size_t)==8,"JPEG configuration requires 64-bit size_t");\n'
    (dest/name).write_text(template)
includes=['-I'+str(dest),'-I'+str(vendor/'src')]
# Neither environment helper is needed by the standalone decoder. NO_PUTENV
# also avoids an unused POSIX setenv helper under strict C11 on Linux.
definitions=['-DNO_GETENV','-DNO_PUTENV']
objects=[]
for unit in manifest['decoder_units']:
    obj=dest/(Path(unit).stem+'.o')
    subprocess.run([cc,'-std=c11',*flags,*includes,*definitions,'-c',str(vendor/unit),'-o',str(obj)],check=True)
    objects.append(str(obj))
subprocess.run([cc,'-std=c11','-UNDEBUG','-Wall','-Wextra','-Werror',*flags,*includes,*definitions,
                'native/jpeg_validation.c','tests/jpeg_validation_test.c',*objects,'-lm',
                '-o',str(dest/'jpeg-validation-test.exe')],check=True)
