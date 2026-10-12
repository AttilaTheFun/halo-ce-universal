#!/usr/bin/env python3
"""Extract the retail Xbox title icon into ignored, local Apple build assets.

Reads XDVDFS -> default.xbe -> $$XTIMAGE -> XPR0/DXT1. No retail bytes are
committed. Format references: https://xboxdevwiki.net/XPR and
https://www.caustik.com/cxbx/download/xbe.htm
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/internal-artwork'

def extract(image):
    with image.open('rb') as f:
        length = f.seek(0, 2)
        for base in (0, 0xFD90000, 0x2080000, 0x18300000):
            f.seek(base+32*2048); header = f.read(2048)
            if header[:20] == b'MICROSOFT*XBOX*MEDIA': break
        else: raise ValueError('Xbox filesystem not found')
        sector, size = struct.unpack_from('<II', header, 20)
        if size > 16*1024*1024 or base+sector*2048+size > length: raise ValueError('Invalid directory')
        f.seek(base+sector*2048); directory = f.read(size)
        pending, seen = [0], set()
        while pending:
            offset = pending.pop()
            if offset in seen or offset+14 > len(directory): raise ValueError('Invalid directory tree')
            seen.add(offset)
            left, right, sector, size, attr, count = struct.unpack_from('<HHIIBB', directory, offset)
            if offset+14+count > len(directory): raise ValueError('Invalid name')
            name = directory[offset+14:offset+14+count].lower()
            if name == b'default.xbe' and not attr & 16:
                if size > 64*1024*1024 or base+sector*2048+size > length: raise ValueError('Invalid XBE extent')
                f.seek(base+sector*2048); xbe = f.read(size); break
            pending += [v*4 for v in (left, right) if v]
        else: raise ValueError('default.xbe not found')
    if xbe[:4] != b'XBEH': raise ValueError('Invalid XBE')
    base = struct.unpack_from('<I', xbe, 0x104)[0]
    count, table = struct.unpack_from('<II', xbe, 0x11c)
    if count > 256 or table < base or table-base+count*56 > len(xbe): raise ValueError('Invalid sections')
    for i in range(count):
        fields = struct.unpack_from('<9I', xbe, table-base+i*56)
        name = fields[5]-base
        if 0 <= name < len(xbe) and xbe[name:name+10] == b'$$XTIMAGE\0':
            start, size = fields[3:5]
            if size > 1024*1024 or start+size > len(xbe): raise ValueError('Invalid title image')
            return xbe[start:start+size]
    raise ValueError('Xbox title image not found')

def png(xpr):
    if xpr[:4] != b'XPR0' or len(xpr) < 32: raise ValueError('Invalid XPR')
    total, start = struct.unpack_from('<II', xpr, 4)
    fmt = struct.unpack_from('<I', xpr, 24)[0]
    w, h = 1 << ((fmt >> 20) & 15), 1 << ((fmt >> 24) & 15)
    if (fmt >> 8) & 255 != 0x0c or not 4 <= w <= 1024 or not 4 <= h <= 1024:
        raise ValueError('Expected a DXT1 Xbox title texture')
    if total > len(xpr) or start < 32 or start+w*h//2 > total: raise ValueError('Truncated texture')
    pixels = bytearray(w*h*4)
    def rgb(c): return ((c >> 11)*255//31, ((c >> 5)&63)*255//63, (c&31)*255//31, 255)
    for y in range(0, h, 4):
        for x in range(0, w, 4):
            a,b,bits = struct.unpack_from('<HHI', xpr, start); start += 8
            ca,cb = rgb(a),rgb(b)
            colors = [ca,cb]
            if a > b: colors += [tuple((2*ca[i]+cb[i])//3 for i in range(4)),tuple((ca[i]+2*cb[i])//3 for i in range(4))]
            else: colors += [tuple((ca[i]+cb[i])//2 for i in range(4)),(0,0,0,0)]
            for j in range(16):
                p=((y+j//4)*w+x+j%4)*4
                pixels[p:p+4]=bytes(colors[(bits>>(2*j))&3])
    def chunk(kind, data): return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    scan=b''.join(b'\0'+pixels[y*w*4:(y+1)*w*4] for y in range(h))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')

def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('iso',type=Path)
    args=parser.parse_args(); OUT.mkdir(parents=True,exist_ok=True)
    title=OUT/'XboxTitle.png'; title.write_bytes(png(extract(args.iso)))
    catalog=OUT/'Assets.xcassets'; icons=catalog/'AppIcon.appiconset'; icons.mkdir(parents=True,exist_ok=True)
    icon=icons/'AppIcon.png'
    subprocess.run(['sips','-z','1024','1024',str(title),'--out',str(icon)],check=True,stdout=subprocess.DEVNULL)
    info={'author':'xcode','version':1}
    (catalog/'Contents.json').write_text(json.dumps({'info':info}))
    (icons/'Contents.json').write_text(json.dumps({'images':[{'filename':'AppIcon.png','idiom':'universal','platform':'ios','size':'1024x1024'}],'info':info}))
    splash=catalog/'RingArtwork.imageset'; splash.mkdir(exist_ok=True)
    (splash/'Title.png').write_bytes(icon.read_bytes())
    (splash/'Contents.json').write_text(json.dumps({'images':[{'filename':'Title.png','idiom':'universal'}],'info':info}))
    print(f'Extracted Xbox title icon: {title}')

if __name__=='__main__': main()
