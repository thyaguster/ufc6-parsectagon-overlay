import pathlib,sys
from PIL import Image

data=pathlib.Path(sys.argv[1]).read_bytes()[256:]
w,h=int(sys.argv[2]),int(sys.argv[3]);mode=sys.argv[5] if len(sys.argv)>5 else 'linear'
im=Image.new('L',(w,h))
for y in range(h//4):
    for x in range(w//4):
        if mode=='linear':i=(y*(w//4)+x)*8
        else:i=sum((((x>>k)&1)<<(2*k)|((y>>k)&1)<<(2*k+1)) for k in range(5))*8
        raw=data[i:i+8]
        a0,a1=raw[:2];alpha=[a0,a1]
        if a0>a1:alpha.extend(((7-j)*a0+j*a1)//7 for j in range(1,7))
        else:alpha.extend(((5-j)*a0+j*a1)//5 for j in range(1,5));alpha.extend([0,255])
        indices=int.from_bytes(raw[2:],'little')
        for yy in range(4):
            for xx in range(4):im.putpixel((x*4+xx,y*4+yy),alpha[(indices>>(3*(yy*4+xx)))&7])
im.resize((w*5,h*5)).save(sys.argv[4])
