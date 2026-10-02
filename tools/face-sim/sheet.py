import sys
from PIL import Image, ImageDraw
data=open(sys.argv[1],'rb').read(); n=len(data)//(32+8192)
frames=[(data[i*8224:i*8224+32].split(b'\0')[0].decode(), data[i*8224+32:(i+1)*8224]) for i in range(n)]
per=int(sys.argv[3]) if len(sys.argv)>3 else 1
S=3; W=128*S; H=64*S; pad=14; cols=4 if per==1 else per
groups=[frames[i:i+per] for i in range(0,n,per)]
if per==1:
    rows=(len(frames)+cols-1)//cols
    img=Image.new('RGB',(cols*(W+pad)+pad, rows*(H+pad+18)+pad),(40,40,40))
    d=ImageDraw.Draw(img)
    for k,(lab,b) in enumerate(frames):
        x=pad+(k%cols)*(W+pad); y=pad+(k//cols)*(H+pad+18)
        d.text((x,y),lab,fill=(255,200,120)); y+=16
        fr=Image.frombytes('L',(128,64),bytes(255 if v else 10 for v in b)).resize((W,H),Image.NEAREST).convert('RGB')
        img.paste(fr,(x,y))
else:
    img=Image.new('RGB',(per*(W+pad)+pad, len(groups)*(H+pad+18)+pad),(40,40,40)); d=ImageDraw.Draw(img)
    for r,g in enumerate(groups):
        for c,(lab,b) in enumerate(g):
            x=pad+c*(W+pad); y=pad+r*(H+pad+18)
            d.text((x,y),f"{lab} #{c}",fill=(255,200,120)); y+=16
            img.paste(Image.frombytes('L',(128,64),bytes(255 if v else 10 for v in b)).resize((W,H),Image.NEAREST).convert('RGB'),(x,y))
img.save(sys.argv[2])
