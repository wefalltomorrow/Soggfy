"""Generated PNG corpus: complete decode, strict framing and resource limits."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

exe=str(Path(sys.argv[1]).resolve())
ffmpeg=sys.argv[2] if len(sys.argv)>2 else 'ffmpeg'
signature=b'\x89PNG\r\n\x1a\n'
def chunk(kind,data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
def header(w=1,h=1,depth=8,color=2,interlace=0):
    return chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,depth,color,0,0,interlace))
def image(raw=b'\0\xff\0\0',prefix=None,between=b'',suffix=b''):
    return signature+(header() if prefix is None else prefix)+between+chunk(b'IDAT',zlib.compress(raw))+suffix+chunk(b'IEND',b'')
valid=image()
compressed=zlib.compress(b'\0\xff\0\0')
def packed_idat(data):
    return signature+header()+chunk(b'IDAT',data)+chunk(b'IEND',b'')
bad={
    'missing_iend':valid[:-12],'trailing':valid+b'extra',
    'duplicate_header':signature+header()+valid[8:],
    'unknown_critical':image(between=chunk(b'ABCD',b'')),
    'extra_scanline':image(b'\0\xff\0\0'*2),
    'short_scanline':image(b'\0\xff\0'),
    'invalid_filter':image(b'\5\xff\0\0'),
    'trailing_zlib':packed_idat(compressed+b'extra'),
    'second_zlib':packed_idat(compressed+compressed),
    'bad_adler':packed_idat(compressed[:-1]+bytes([compressed[-1]^1])),
    'noncontiguous':signature+header()+chunk(b'IDAT',compressed[:6])+chunk(b'tEXt',b'x\0y')+chunk(b'IDAT',compressed[6:])+chunk(b'IEND',b''),
    'late_idat':image(suffix=chunk(b'tEXt',b'x\0y')+chunk(b'IDAT',b'')),
    'animated':image(between=chunk(b'acTL',struct.pack('>II',1,0))),
    'zero_width':image(prefix=header(w=0)),
    'large_dimension':image(prefix=header(w=8193)),
    'pixel_limit':image(prefix=header(w=8192,h=8192)),
    'invalid_depth':image(prefix=header(depth=4)),
    'invalid_color':image(prefix=header(color=1)),
    'invalid_interlace':image(prefix=header(interlace=2)),
    'missing_palette':image(b'\0\0',prefix=header(color=3)),
    'bad_palette_index':image(b'\0\1',prefix=header(color=3),between=chunk(b'PLTE',b'\0\0\0')),
    'duplicate_palette':image(between=chunk(b'PLTE',b'\0\0\0')*2),
    'late_palette':image(suffix=chunk(b'PLTE',b'\0\0\0')),
    'too_many_chunks':image(between=chunk(b'raNd',b'')*4094),
    'reserved_chunk_bit':image(between=chunk(b'rand',b'')),
    'invalid_chunk_character':image(between=chunk(b'ra1d',b'')),
    'bad_text_compression':image(between=chunk(b'zTXt',b'key\0\0invalid')),
    'large_text_expansion':image(between=chunk(b'zTXt',b'key\0\0'+zlib.compress(b'x'*(4*1024*1024+1)))),
    'aggregate_memory_limit':image(between=chunk(b'zTXt',b'key\0\0'+zlib.compress(b'x'*(4*1024*1024-1)))*10),
}
for label,offset in [('ihdr',32),('idat',len(valid)-13),('iend',len(valid)-1)]:
    mutated=bytearray(valid);mutated[offset]^=1;bad['crc_'+label]=mutated
for n in range(len(valid)):
    bad['truncated_'+str(n)]=valid[:n]
with tempfile.TemporaryDirectory(prefix='soggfy-png-') as directory:
    root=Path(directory)
    def run(label,data,expected,dimensions=None):
        path=root/(label+'.png');path.write_bytes(data)
        p=subprocess.run([exe,str(path)],capture_output=True,timeout=30)
        assert p.returncode==expected,(label,p.returncode,p.stdout,p.stderr)
        assert not p.stderr,(label,p.stderr)
        if dimensions:
            assert p.stdout.decode().strip()==dimensions,(label,p.stdout)
        return path
    run('red',valid,0,'1 1 8 3')
    channels={0:1,2:3,3:1,4:2,6:4}
    depths={0:[1,2,4,8,16],2:[8,16],3:[1,2,4,8],4:[8,16],6:[8,16]}
    adam7=[(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]
    count=0
    for color,ds in depths.items():
        for depth in ds:
            for interlace in [0,1]:
                for w,h in [(1,1),(9,7),(1,9)]:
                    raw=bytearray()
                    for x,y,dx,dy in (adam7 if interlace else [(0,0,1,1)]):
                        pw=max(0,(w-x+dx-1)//dx);ph=max(0,(h-y+dy-1)//dy)
                        if pw and ph:
                            row=(pw*channels[color]*depth+7)//8
                            raw.extend(bytes(row+1)*ph)
                    name=f'valid-{color}-{depth}-{interlace}-{w}-{h}'
                    data=image(raw,prefix=header(w,h,depth,color,interlace),between=chunk(b'PLTE',b'\0\0\0') if color==3 else b'')
                    path=run(name,data,0,f'{w} {h} {depth} {channels[color]}')
                    # Independent decode checks the generated valid corpus itself.
                    decoded=subprocess.run([ffmpeg,'-v','error','-i',str(path),'-f','rawvideo','-pix_fmt','rgb24','-'],capture_output=True,timeout=30)
                    assert decoded.returncode==0 and not decoded.stderr,(name,decoded.stderr)
                    assert decoded.stdout==bytes(w*h*3),(name,len(decoded.stdout))
                    count+=1
    for filter_type in range(5):
        run('filter-'+str(filter_type),image(bytes([filter_type,0,0,0])*2,prefix=header(h=2)),0,'1 2 8 3')
    run('ancillary',image(between=chunk(b'tEXt',b'key\0value')+chunk(b'raNd',b'opaque')),0,'1 1 8 3')
    run('split_idat',signature+header()+chunk(b'IDAT',compressed[:6])+chunk(b'IDAT',compressed[6:])+chunk(b'IEND',b''),0,'1 1 8 3')
    run('chunk_limit',image(between=chunk(b'raNd',b'')*4093),0,'1 1 8 3')
    run('bounded_metadata',image(between=chunk(b'zTXt',b'key\0\0'+zlib.compress(b'x'*(1024*1024)))*5),0,'1 1 8 3')
    for label,data in bad.items():run(label,data,1)
    print(f'PASS: PNG {count} independently decoded layouts, 5 filters, ancillary/split/boundary cases and {len(bad)} malformed inputs')
