"""Pinned controlled JPEG corpus, truncation, scan coverage and resource limits."""
import base64
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

decoder=str(Path(sys.argv[1]).resolve())
ffmpeg=sys.argv[2] if len(sys.argv)>2 else 'ffmpeg'
manifest=json.loads(Path(__file__).with_name('jpeg_fixtures.json').read_text())
fixtures={}
for name,item in manifest['fixtures'].items():
    data=base64.b64decode(item['base64'],validate=True)
    assert hashlib.sha256(data).hexdigest()==item['sha256'],name
    fixtures[name]=data

def markers(data):
    at=2;result=[]
    while at<len(data):
        start=at;assert data[at]==255
        while data[at]==255:at+=1
        code=data[at];at+=1
        if code==0xd9:
            result.append((start,code,at));break
        length=int.from_bytes(data[at:at+2],'big');assert length>=2
        at+=length;result.append((start,code,at))
        if code!=0xda:continue
        while at<len(data):
            if data[at]!=255:at+=1;continue
            begin=at
            while data[at]==255:at+=1
            if data[at]==0 or 0xd0<=data[at]<=0xd7:at+=1;continue
            at=begin;break
    return result

accepted=rejected=0
with tempfile.TemporaryDirectory(prefix='soggfy-jpeg-') as directory:
    path=Path(directory)/'fixture.jpg'
    def check(label,data,valid,expected=None):
        global accepted,rejected
        path.write_bytes(data)
        p=subprocess.run([decoder,str(path)],capture_output=True,text=True,timeout=20)
        assert p.returncode==(0 if valid else 1),(label,p.returncode,p.stdout,p.stderr)
        assert not p.stderr,(label,p.stderr)
        if valid:
            accepted+=1
            fields=list(map(int,p.stdout.split()))
            if expected is not None:assert fields==expected,(label,fields)
        else:rejected+=1
    def pixels(label,data):
        path.write_bytes(data)
        p=subprocess.run([ffmpeg,'-v','error','-i',str(path),'-f','rawvideo','-pix_fmt','rgb24','-'],capture_output=True,timeout=20)
        assert p.returncode==0 and not p.stderr and len(p.stdout)==17*19*3,(label,p.stderr)
        return p.stdout

    for name,data in fixtures.items():
        if name=='large-progressive':
            check(name,data,False);continue
        expected=[17,19,1 if name.startswith('gray') else 3,int('progressive' in name)]
        check(name,data,True,expected)
        original=pixels(name,data)
        for label,changed in [('missing-eoi',data[:-2]),('trailing',data+b'extra'),
                              ('concatenated',data+data),('duplicate-soi',data[:2]+data)]:
            check(name+'-'+label,changed,False)
        parsed=markers(data)
        sos=next(end for start,code,end in parsed if code==0xda)
        check(name+'-short-entropy',data[:sos+1]+b'\xff\xd9',False)
        if name in ['baseline','progressive']:
            for n in range(len(data)):check(name+'-prefix-'+str(n),data[:n],False)
        if 'progressive' in name:
            ends=[parsed[i+1][0] for i,(_,code,_) in enumerate(parsed) if code==0xda]
            for n,end in enumerate(ends[:-1]):
                shorter=data[:end]+b'\xff\xd9'
                check(name+'-through-scan-'+str(n+1),shorter,True,expected)
                assert pixels(name,shorter)!=original
    data=fixtures['baseline'];parsed=markers(data)
    frame=next(start for start,code,end in parsed if code==0xc0)
    for label,offset,value in [('precision',frame+4,12),('zero-height',frame+5,0),
                               ('lossless',frame+1,0xc3),('arithmetic',frame+1,0xc9),
                               ('unknown-frame',frame+1,0xc8)]:
        changed=bytearray(data);changed[offset]=value
        if label=='zero-height':changed[offset+1]=0
        check(label,changed,False)
    for label,w,h in [('dimension-limit',8193,1),('pixel-limit',8192,2049)]:
        changed=bytearray(data);changed[frame+5:frame+9]=h.to_bytes(2,'big')+w.to_bytes(2,'big')
        check(label,changed,False)
    check('unknown-app',data[:2]+b'\xff\xef\x00\x02'+data[2:],True,[17,19,3,0])
    # Parser count includes EOI, excludes initial SOI. Baseline has no restarts.
    padding=b'\xff\xef\x00\x02'*(4096-len(parsed))
    check('marker-limit',data[:2]+padding+data[2:],True,[17,19,3,0])
    check('marker-over-limit',data[:2]+padding+b'\xff\xef\x00\x02'+data[2:],False)
    check('input-limit',data+b'\x00'*(4*1024*1024+1-len(data)),False)
    for code in [0x00,0x01,0xd0,0xd8,0xdc,0xde,0xdf]:
        check('unsupported-marker-'+str(code),data[:2]+bytes([255,code,0,2])+data[2:],False)
    check('duplicate-frame',data[:frame]+data[frame:next(end for start,code,end in parsed if start==frame)]+data[frame:],False)
    for code,label in [(0xc4,'missing-huffman'),(0xdb,'missing-quantization')]:
        changed=data
        for start,kind,end in reversed(parsed):
            if kind==code:changed=changed[:start]+changed[end:]
        check(label,changed,False)
    for start,code,end in parsed:
        if code==0xc4:
            changed=bytearray(data);changed[start+5]=255
            check('oversubscribed-huffman-'+str(start),changed,False)
        if code==0xdb:
            changed=bytearray(data);changed[start+4]=0x20
            check('invalid-quantization-precision-'+str(start),changed,False)
    changed=bytearray(data);changed[frame+11]=0
    check('zero-sampling-factor',changed,False)
print(f'PASS: JPEG {accepted} valid/scan-boundary cases and {rejected} malformed/resource-limit cases; clean independent FFmpeg decoding')
