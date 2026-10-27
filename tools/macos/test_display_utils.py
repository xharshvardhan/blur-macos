#!/usr/bin/env python3
"""[Blur2PC][macOS] Independent standard image fixtures for actual engine loading."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

def dds(width,height,levels,compressed=False):
    header=bytearray(128);header[:4]=b'DDS '
    def put(offset,value):struct.pack_into('<I',header,offset,value)
    for offset,value in ((4,124),(8,0x2100f),(12,height),(16,width),(28,levels),(76,32),(108,0x401008)):put(offset,value)
    if compressed:put(80,4);put(84,0x30545844+int(compressed)*0x1000000)
    else:
        for offset,value in ((80,0x41),(88,32),(92,0xff0000),(96,0xff00),(100,0xff),(104,0xff000000)):put(offset,value)
    data=bytearray(header)
    for level in range(levels):
        if compressed:
            block=struct.pack('<HHI',[0xf800,0x07e0,0x001f,0xffff][level],0,0)
            if compressed==3:block=b'\xff'*8+block
            if compressed==5:block=b'\xff'+b'\0'*7+block
            data.extend(block*((width+3)//4)*((height+3)//4))
        else:
            for y in range(height):
                for x in range(width):data.extend(bytes((7+level*11+x*3+y,(y+1)*29+level*5,(x+1)*19+level*3,128+level*16+(x+y)%7)))
        width=max(1,width//2);height=max(1,height//2)
    return data

palette=[0xff0000ff,0xff00ff00,0xffff0000,0x80808080,0xff123456,0x00000000]
with tempfile.TemporaryDirectory(prefix='display-utils-',dir=sys.argv[2]) as temporary:
    root=Path(temporary);rgba=dds(5,3,3);(root/'rgba.dds').write_bytes(rgba);(root/'bc.dds').write_bytes(dds(8,8,4,True))
    (root/'bc3.dds').write_bytes(dds(8,8,4,3));(root/'bc5.dds').write_bytes(dds(8,8,4,5))
    (root/'short.dds').write_bytes(rgba[:127]);(root/'truncated.dds').write_bytes(rgba[:-1])
    bad=bytearray(rgba);struct.pack_into('<I',bad,28,32);(root/'bad-levels.dds').write_bytes(bad)
    for top,right in ((False,False),(True,True)):
        header=bytearray(18);header[0]=5;header[2]=2;struct.pack_into('<HH',header,12,3,2);header[16]=32;header[17]=8|(32 if top else 0)|(16 if right else 0)
        data=bytearray(header+b'hello')
        for y in (range(2) if top else reversed(range(2))):
            for x in (reversed(range(3)) if right else range(3)):data.extend(struct.pack('<I',palette[y*3+x]))
        (root/('top-right.tga' if top else 'bottom.tga')).write_bytes(data)
    (root/'short.tga').write_bytes(data[:-1])
    for top in (False,True):
        data=bytearray(70);struct.pack_into('<2sIHHI',data,0,b'BM',94,0,0,70)
        struct.pack_into('<IiiHHIIiiII',data,14,40,3,-2 if top else 2,1,24,0,24,0,0,0,0)
        data[54:70]=b'Z'*16
        for y in (range(2) if top else reversed(range(2))):
            for x in range(3):data.extend(struct.pack('<I',palette[y*3+x])[:3])
            data.extend(b'\xe7'*3)
        (root/('top.bmp' if top else 'bottom.bmp')).write_bytes(data)
    (root/'short.bmp').write_bytes(data[:-1]);(root/'pixel.bmsl').write_bytes(b'bad')
    subprocess.run([sys.argv[1],str(root)],check=True,timeout=30)
