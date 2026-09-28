from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import ctypes as C
ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
FONT='C:/Windows/Fonts/bahnschrift.ttf'
BG='#090D1C'; PANEL='#141B32'; FG='#F1F5FF'; MUT='#A9B7D0'; CY='#35E4FF'; MG='#F35AC8'; AM='#FFC857'; RED='#FF445E'
def font(n): return ImageFont.truetype(FONT,n)
def text(d,xy,s,n=14,c=FG): d.text(xy,s,font=font(n),fill=c)
def base(kind,title='Engine',state='Demo'):
 im=Image.new('RGB',(320,172),BG);d=ImageDraw.Draw(im)
 text(d,(12,8),title,16)
 d.rounded_rectangle((260,8,308,26),3,fill=PANEL)
 text(d,(266,9),state,12,CY if state=='Demo' else AM)
 if kind=='A':d.line((12,33,308,33),fill='#26304B')
 else:d.line((12,32,24,32,32,40,308,40),fill=MG,width=2)
 return im,d
def footer(d,left='K1 Next',right='K2 Select'):
 d.line((12,146,308,146),fill='#26304B')
 text(d,(12,153),left,12,MUT); text(d,(220,153),right,12,MUT)
def rpm(kind,stale=False):
 im,d=base(kind,state='Stale' if stale else 'Demo')
 if kind=='A':
  for i in range(23):
   x=12+i*13; col='#222B42' if stale or i>13 else CY if i<18 else MG
   d.rectangle((x,44,x+9,57),fill=col)
  text(d,(12,61),'0',12,MUT);text(d,(146,61),'4',12,MUT);text(d,(299,61),'8',12,MUT)
  text(d,(10,78),'--' if stale else '4820',54,FG)
  text(d,(175,107),'rpm',16,MUT)
  text(d,(230,88),'Gateway',14,MUT)
  text(d,(230,110),'No data' if stale else 'Demo',14,AM if stale else CY)
 else:
  points=[(14,128),(14,75),(50,48),(267,48),(304,76),(304,128)]
  d.line(points,fill='#26304B',width=8)
  if not stale:d.line([(14,128),(14,75),(50,48),(205,48)],fill=CY,width=6)
  d.line((273,54,300,76),fill=MG,width=6)
  text(d,(87,68),'--' if stale else '4820',48)
  text(d,(137,118),'rpm',14,MUT)
  text(d,(25,128),'No data' if stale else 'Gateway demo',12,AM if stale else MUT)
 footer(d);return im
def menu(kind):
 im,d=base(kind,'Menu')
 for j,label in enumerate(['Engine','Connection','Update']):
  y=44+j*31
  if j==2:
   d.rounded_rectangle((12,y,308,y+27),3,fill=PANEL)
   d.rectangle((12,y,15,y+27),fill=CY if kind=='A' else MG)
  text(d,(24,y+4),label,16,CY if j==2 else MUT)
  if j==2:text(d,(282,y+3),'>',18,CY)
 footer(d);return im
def qrimg():
 lib=C.CDLL(str(ROOT/'.test-build/qr-codegen.dll'));U=C.c_uint8
 lib.qrcodegen_getMinFitVersion.argtypes=[C.c_int,C.c_size_t]
 lib.qrcodegen_encodeBinary.argtypes=[C.POINTER(U),C.c_size_t,C.POINTER(U),C.c_int,C.c_int,C.c_int,C.c_int,C.c_bool]
 lib.qrcodegen_getModule.argtypes=[C.POINTER(U),C.c_int,C.c_int]
 lib.qrcodegen_getModule.restype=C.c_bool
 data=(U*4096)();qr=(U*4096)();payload=b'WIFI:T:WPA;S:BMW-Wheel-DESIGN;P:abcdefghjkmnpqrs;;'
 low=lib.qrcodegen_getMinFitVersion(1,len(payload));ver=min(range(low,low+3),key=lambda v:148%(17+4*(v+1)))
 C.memmove(data,payload,len(payload));lib.qrcodegen_encodeBinary(data,len(payload),qr,1,ver,ver,-1,True)
 side=lib.qrcodegen_getSize(qr);scale=148//(17+4*(ver+1));m=8+(148-side*scale)//2
 im=Image.new('RGB',(164,164),'white');d=ImageDraw.Draw(im)
 for y in range(side):
  for x in range(side):
   if lib.qrcodegen_getModule(qr,x,y):d.rectangle((m+x*scale,m+y*scale,m+(x+1)*scale-1,m+(y+1)*scale-1),fill='black')
 return im
def update(kind):
 im=Image.new('RGB',(320,172),BG);d=ImageDraw.Draw(im);im.paste(qrimg(),(0,4))
 text(d,(173,10),'Wi-Fi setup',18,CY if kind=='A' else MG)
 for y,s in [(44,'1 Scan to join'),(68,'2 Open browser'),(92,'192.168.4.1')]:text(d,(173,y),s,14)
 text(d,(173,125),'Demo QR only',12,AM);text(d,(173,149),'Hold K1 Back',12,MUT)
 return im
def progress(kind):
 im,d=base(kind,'Update','OTA')
 text(d,(12,46),'Gateway verified',16,CY)
 text(d,(12,76),'Wheel downloading',16)
 text(d,(246,66),'32%',28)
 d.rectangle((12,110,308,120),fill=PANEL);d.rectangle((12,110,106,120),fill=CY if kind=='A' else MG)
 text(d,(12,130),'Keep both units powered',14,MUT)
 return im
def error(kind):
 im,d=base(kind,'Update','Error')
 d.rectangle((12,46,15,131),fill=RED)
 text(d,(26,46),'Download interrupted',20)
 text(d,(26,78),'Current firmware retained.',14,MUT)
 text(d,(26,101),'Check Wi-Fi and retry.',14)
 footer(d,'Hold K1 Back','K2 Check');return im
states={'rpm':rpm,'stale':lambda k:rpm(k,True),'menu':menu,'wifi':update,'ota':progress,'error':error}
for kind in ['A','B']:
 for name,fn in states.items():fn(kind).save(OUT/f'{kind.lower()}-{name}.png')
sheet=Image.new('RGB',(688,6*208+48),'#202638');d=ImageDraw.Draw(sheet)
text(d,(20,12),'A / Ribbon (recommended)',20);text(d,(356,12),'B / Apex',20)
for row,name in enumerate(states):
 y=48+row*208
 for col,kind in enumerate(['a','b']):
  sheet.paste(Image.open(OUT/f'{kind}-{name}.png'),(16+col*336,y))
  text(d,(16+col*336,y+176),name,14)
sheet.save(OUT/'comparison.png')
print('Rendered 12 native 320x172 screens and comparison.png')
