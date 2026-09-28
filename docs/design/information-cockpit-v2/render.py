from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
import math
O=Path(__file__).parent
BG='#080D18'; SUR='#111D2C'; LINE='#2B4055'; WHITE='#F1F5FF'; MUT='#9DAFC4'; CY='#35E4FF'; MG='#F35AC8'; AM='#FFC857'
F='C:/Windows/Fonts/bahnschrift.ttf'
def txt(d,x,y,s,n=14,c=WHITE):d.text((x,y),s,font=ImageFont.truetype(F,n),fill=c)
def center(d,x,y,s,n=14,c=WHITE):
 w=d.textlength(s,font=ImageFont.truetype(F,n));txt(d,x-w/2,y,s,n,c)
def base():
 im=Image.new('RGB',(320,172),BG);d=ImageDraw.Draw(im)
 txt(d,10,3,'E90',14,MUT);txt(d,258,3,'Demo',14,CY)
 return im,d
def temp(d,x,name,val,oil=False):
 if oil:
  d.polygon([(x+5,139),(x,147),(x+1,152),(x+9,152),(x+10,147)],outline=MG)
 else:
  d.rounded_rectangle((x+3,139,x+7,149),2,outline=CY);d.ellipse((x+1,147,x+9,155),outline=CY)
 txt(d,x+17,137,name,14,MUT);txt(d,x+82,134,val,20)
 d.line((x+17,159,x+142,159),fill=LINE);d.line((x+17,159,x+93,159),fill=LINE if val=='--' else MG if oil else CY,width=2)
def dashA(missing=False):
 im,d=base()
 # Sloped shoulder and segmented rail unify engine readout with cluster.
 d.polygon([(10,47),(36,26),(309,26),(309,44),(42,44),(22,59)],fill=SUR)
 for i in range(28):
  x=40+i*9
  col=LINE if missing or i>15 else CY if i<23 else MG
  d.polygon([(x,29),(x+7,29),(x+7,41),(x,41)],fill=col)
 for x,s in [(40,'0'),(104,'2'),(167,'4'),(230,'6'),(292,'8')]:txt(d,x,46,s,12,MUT)
 d.line((10,66,61,66,79,83,79,121),fill=LINE,width=2)
 d.line((241,66,310,66,310,121,260,121),fill=LINE,width=2)
 d.line((84,120,84,84,107,63),fill='#264C63',width=2)
 d.line((212,63,236,84,236,120),fill='#513250',width=2)
 txt(d,18,76,'--' if missing else 'D',28,WHITE);txt(d,17,107,'Gear',14,MUT)
 center(d,160,60,'--' if missing else '108',48)
 center(d,160,110,'km/h',14,MUT)
 center(d,273,76,'--' if missing else '3420',20);center(d,273,103,'rpm',14,MUT)
 d.line((10,130,310,130),fill=LINE)
 temp(d,10,'Water','--' if missing else '94°');temp(d,170,'Oil','--' if missing else '102°',True)
 if missing:txt(d,93,3,'Data unavailable',14,AM)
 return im
def dashB(missing=False):
 im,d=base()
 for cx,col,val,unit,maxval in [(80,CY,'108','km/h',240),(241,MG,'3420','rpm',8000)]:
  box=(cx-68,27,cx+68,155)
  d.arc(box,155,385,fill=LINE,width=6)
  if not missing:d.arc(box,155,155+int(230*int(val)/maxval),fill=col,width=6)
  for a in range(155,386,23):
   r=math.radians(a);d.line((cx+55*math.cos(r),91+52*math.sin(r),cx+60*math.cos(r),91+57*math.sin(r)),fill=MUT,width=1)
  center(d,cx,64,'--' if missing else val,40 if unit=='km/h' else 28)
  center(d,cx,108,unit,14,MUT)
 center(d,160,68,'--' if missing else 'D',20)
 center(d,160,100,'Gear',12,MUT)
 d.rectangle((0,143,319,171),fill=BG)
 txt(d,10,149,'Water --' if missing else 'Water 94°',16,CY);txt(d,202,149,'Oil --' if missing else 'Oil 102°',16,MG)
 return im
def car(d,cx,top,opened):
 # Original symmetric sedan silhouette, front up.
 body=[(cx-16,top),(cx+16,top),(cx+23,top+12),(cx+26,top+80),(cx+20,top+109),(cx-20,top+109),(cx-26,top+80),(cx-23,top+12)]
 d.polygon(body,fill='#182839',outline=MUT)
 d.polygon([(cx-18,top+23),(cx+18,top+23),(cx+15,top+41),(cx-15,top+41)],fill='#2B4960',outline=MUT)
 d.rounded_rectangle((cx-15,top+45,cx+15,top+73),3,outline=LINE)
 d.polygon([(cx-15,top+79),(cx+15,top+79),(cx+18,top+92),(cx-18,top+92)],fill='#2B4960',outline=MUT)
 d.line((cx-14,top+5,cx+14,top+5),fill=WHITE,width=2)
 d.line((cx-16,top+103,cx-5,top+103),fill=MG,width=2);d.line((cx+5,top+103,cx+16,top+103),fill=MG,width=2)
 for key,side,y in [('FL',-1,29),('FR',1,29),('RL',-1,59),('RR',1,59)]:
  x=cx+side*25
  if key in opened:
   p=[(x,top+y),(x+side*28,top+y+11),(x+side*26,top+y+27),(x,top+y+23)]
   d.polygon(p,fill='#593F20',outline=AM);d.line((p[0],p[1],p[2]),fill=AM,width=2)
  else:d.line((x,top+y,x,top+y+24),fill=LINE,width=2)
  txt(d,cx-69 if side<0 else cx+65,top+y+4,key,12,AM if key in opened else MUT)
 center(d,cx,top-19,'Front',12,MUT)
def door(opened,unknown=False):
 im,d=base();txt(d,10,26,'Door data lost' if unknown else 'Door open',18,AM)
 txt(d,10,57,'--' if unknown else '0',40);txt(d,48,81,'km/h',14,MUT)
 txt(d,10,110,'Gear',14,MUT);txt(d,62,102,'--' if unknown else 'P',28)
 d.line((130,28,130,144),fill=LINE)
 car(d,233,40,set() if unknown else opened)
 if unknown:txt(d,157,155,'State unknown',14,AM)
 else:txt(d,10,151,'Demo / '+', '.join(sorted(opened)),14,AM)
 return im
for k,fn in [('a',dashA),('b',dashB)]:
 fn().save(O/f'{k}-cockpit.png');fn(True).save(O/f'{k}-unavailable.png')
for state in [('FL',),('FR',),('RL',),('RR',),('FL','RR')]:door(set(state)).save(O/('doors-'+'-'.join(state)+'.png'))
door(set(),True).save(O/'doors-unknown.png')
rows=[('a-cockpit','b-cockpit'),('a-unavailable','b-unavailable'),('doors-FL','doors-FR'),('doors-RL','doors-RR'),('doors-FL-RR','doors-unknown')]
im=Image.new('RGB',(672,48+208*len(rows)),'#1B2433');d=ImageDraw.Draw(im)
txt(d,12,12,'A / Angular cluster',20);txt(d,348,12,'B / Dual arc',20)
for y,row in enumerate(rows):
 for x,n in enumerate(row):im.paste(Image.open(O/f'{n}.png'),(8+x*336,48+y*208));txt(d,8+x*336,222+y*208,n,12,MUT)
im.save(O/'comparison.png')
print('10 native previews plus comparison')
