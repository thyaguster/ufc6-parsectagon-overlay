"""Inspect a UFC 4 GNF BC7 texture using Pillow's DDS decoder."""
import pathlib, struct, sys
from PIL import Image

src = pathlib.Path(sys.argv[1]).read_bytes()[256:]
w, h = map(int, sys.argv[2:4])
mode = sys.argv[5] if len(sys.argv) > 5 else "tile8"
bw, bh = (w+3)//4, (h+3)//4
pitch = 8 if len(src) <= 2048 else 16
out = bytearray(bw*bh*16)
for y in range(bh):
    for x in range(bw):
        if mode == "linear": i = (y*pitch+x)*16
        elif mode in ("tile8", "tile8b", "tile8c"):
            tx,ty=x//8,y//8
            xx,yy=x%8,y%8
            if mode=="tile8": z=sum(((xx>>k)&1)<<(2*k)|((yy>>k)&1)<<(2*k+1) for k in range(3))
            elif mode=="tile8b": z=sum(((yy>>k)&1)<<(2*k)|((xx>>k)&1)<<(2*k+1) for k in range(3))
            else:z=yy*8+xx
            i=((ty*(pitch//8)+tx)*64+z)*16
        else:raise ValueError(mode)
        out[(y*bw+x)*16:(y*bw+x+1)*16]=src[i:i+16]

header = bytearray(148)
header[:4]=b'DDS '
struct.pack_into('<I',header,4,124)
struct.pack_into('<I',header,8,0x81007)
struct.pack_into('<II',header,12,h,w)
struct.pack_into('<I',header,20,bw*16)
struct.pack_into('<I',header,76,32)
struct.pack_into('<I',header,80,4)
header[84:88]=b'DX10'
struct.pack_into('<I',header,108,0x1000)
struct.pack_into('<IIIII',header,128,98,3,0,1,0)
im=Image.open(__import__('io').BytesIO(header+out)).convert('RGBA')
im.resize((w*6,h*6)).save(sys.argv[4])
