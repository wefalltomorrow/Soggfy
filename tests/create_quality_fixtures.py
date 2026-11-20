"""Header-only fixtures for Windows quality/publication plumbing tests."""
import struct,sys
from pathlib import Path
root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
def ogg(bitrate):
    packet=b'\x01vorbis'+struct.pack('<IBIiiiBB',0,2,44100,0,bitrate,0,0xb8,1)
    page=bytearray(b'OggS'+bytes([0,2])+struct.pack('<QIII',0,123,0,0)+bytes([1,len(packet)])+packet)
    crc=0
    for value in page:
        crc ^= value<<24
        for _ in range(8):crc=((crc<<1)^ (0x04c11db7 if crc&0x80000000 else 0))&0xffffffff
    page[22:26]=struct.pack('<I',crc);return page
(root/'low.ogg').write_bytes(ogg(160000));(root/'high.ogg').write_bytes(ogg(320000))
info=bytearray(34);info[:4]=struct.pack('>HH',4096,4096)
info[10:18]=((44100<<44)|(1<<41)|(15<<36)|44100).to_bytes(8,'big')
(root/'existing.flac').write_bytes(b'fLaC\x80\x00\x00\x22'+info)
(root/'unknown.ogg').write_bytes(b'unrecognized quality fixture')

def page(packet, sequence=1):
    lacing = bytes([255] * (len(packet)//255) + [len(packet)%255])
    result=bytearray(b'OggS'+bytes([0,0])+struct.pack('<QIII',0,123,sequence,0)+bytes([len(lacing)])+lacing+packet)
    crc=0
    for value in result:
        crc ^= value<<24
        for _ in range(8):crc=((crc<<1)^(0x04c11db7 if crc&0x80000000 else 0))&0xffffffff
    result[22:26]=struct.pack('<I',crc)
    return result

for identity in ('A','B','C'):
    uri='spotify:track:'+identity*22
    field=('SPOTIFY_URI='+uri).encode()
    comments=struct.pack('<II',0,1)+struct.pack('<I',len(field))+field
    for bitrate in (160000,320000):
        (root/f'{identity}-{bitrate}.ogg').write_bytes(ogg(bitrate)+page(b'\x03vorbis'+comments+b'\x01'))
    (root/f'{identity}.flac').write_bytes(b'fLaC\x00\x00\x00\x22'+info+b'\x84'+len(comments).to_bytes(3,'big')+comments)
    text=b'\x03SPOTIFY_URI\x00'+uri.encode()
    frame=b'TXXX'+len(text).to_bytes(4,'big')+b'\x00\x00'+text
    length=len(frame)
    size=bytes((length>>shift)&127 for shift in (21,14,7,0))
    (root/f'{identity}.mp3').write_bytes(b'ID3\x03\x00\x00'+size+frame+b'\xff\xfb')
