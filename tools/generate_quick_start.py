#!/usr/bin/env python3
"""Draw the release quick-start sheet with the current physical control mapping."""
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'output/pdf/trumpagotchi-quick-start.pdf'
FONTS=Path('/Users/adamwilson/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/libreoffice-headless/libreoffice/LibreOfficeDev.app/Contents/Resources/fonts/truetype')
for name,file in [('Guide','LiberationSans-Regular.ttf'),('GuideBold','LiberationSans-Bold.ttf')]:
    pdfmetrics.registerFont(TTFont(name,str(FONTS/file)))
W,H=A4
INK='#243540'; MUTED='#5E6B70'; LINE='#D7DDD9'; PAPER='#FFFFFF'; WARM='#F6F1E7'; TEAL='#087A76'; BLUE='#275E9A'; MINT='#E9F4F0'; PALEBLUE='#EBF1F9'; GOLD='#D5A23B'
OUT.parent.mkdir(parents=True,exist_ok=True)
c=canvas.Canvas(str(OUT),pagesize=A4)
c.setTitle('Trumpagotchi - Quick Start for M-VAVE FM-1')
c.setAuthor('Trumpagotchi project')

def rect(x,y,w,h,fill,stroke=None,r=0):
    c.setFillColor(HexColor(fill));c.setStrokeColor(HexColor(stroke or fill));c.setLineWidth(.8)
    if r:c.roundRect(x,H-y-h,w,h,r,fill=1,stroke=bool(stroke))
    else:c.rect(x,H-y-h,w,h,fill=1,stroke=bool(stroke))
def text(x,y,s,size=10,color=INK,bold=False,align='left'):
    c.setFillColor(HexColor(color));c.setFont('GuideBold' if bold else 'Guide',size)
    yy=H-y-size*.85
    if align=='center':c.drawCentredString(x,yy,s)
    elif align=='right':c.drawRightString(x,yy,s)
    else:c.drawString(x,yy,s)
def line(x,y,x2,y2,color=LINE,width=.8):
    c.setStrokeColor(HexColor(color));c.setLineWidth(width);c.line(x,H-y,x2,H-y2)
def dot(x,y,r,fill):
    c.setFillColor(HexColor(fill));c.circle(x,H-y,r,fill=1,stroke=0)

def wrapped(x,y,s,width,size=10,color=INK,bold=False,leading=None):
    words=s.split();cur='';lines=[];font='GuideBold' if bold else 'Guide'
    for word in words:
        trial=(cur+' '+word).strip()
        if pdfmetrics.stringWidth(trial,font,size)>width and cur:lines.append(cur);cur=word
        else:cur=trial
    if cur:lines.append(cur)
    for i,row in enumerate(lines):text(x,y+i*(leading or size*1.35),row,size,color,bold)
    return len(lines)*(leading or size*1.35)

text(36,30,'TRUMPAGOTCHI',28,bold=True)
text(36,65,'QUICK START  /  M-VAVE FM-1',12,MUTED,bold=True)
text(36,88,'Feed. Praise. Clean. Let him nap. Then play something ridiculous.',10.5)
line(36,113,W-36,113,TEAL,2)

# A top-down schematic follows the physical panel and the simulator's control positions.
x0,y0,scale=36,134,(W-72)/1000

def R(x,y,w,h,fill,stroke=None,r=0):rect(x0+x*scale,y0+y*scale,w*scale,h*scale,fill,stroke,r*scale)
def T(x,y,s,size=17,color=INK,bold=True):text(x0+x*scale,y0+y*scale,s,size*scale,color,bold,'center')
def D(x,y,r,fill):dot(x0+x*scale,y0+y*scale,r*scale,fill)
def L(x,y,x2,y2,color=LINE,width=1.5):line(x0+x*scale,y0+y*scale,x0+x2*scale,y0+y2*scale,color,width*scale)
R(4,4,992,632,WARM,'#B7B2A8',38)
R(300,44,236,236,INK,r=23)
screenshot=ROOT/'firmware/build/pet/preview/idle.png'
if screenshot.exists():
    c.drawImage(str(screenshot),x0+310*scale,H-(y0+270*scale),width=216*scale,height=216*scale)
else:
    R(310,54,216,216,MINT);T(418,122,'FOOD / MOOD / ENERGY',14);T(418,164,'YOUR PET',23)

for x,y,label,active in [(105,100,'MASTER',True),(215,100,'SELECT',True),(105,205,'PRESET',False),(215,205,'ALGO',False)]:
    D(x,y,30,'#D6D2CA');D(x,y,24,INK if active else '#B4B6B3')
    L(x,y-18,x,y-7,PAPER,3);T(x,y-51,label,16,INK if active else MUTED)
for i in range(4):
    x=610+i*104;D(x,100,27,'#D6D2CA');D(x,100,22,'#B4B6B3');L(x,84,x,94,PAPER,3);T(x,51,'K'+str(i+1),17,MUTED)
rows=[['FX','SEL','ENV','LFO','EDIT','GLO'],['HOME','SAVE','ARP','SEQ','PLAY','REC']]
active={'SEL','GLO','HOME','SAVE','PLAY'}
R(568,160,400,140,'#E7E1D7',r=17)
for row,names in enumerate(rows):
    for i,label in enumerate(names):
        x,y=582+i*63,174+row*62
        R(x,y,54,48,TEAL if label in active else '#D5D1CA',r=8)
        T(x+27,y+17,label,14,PAPER if label in active else MUTED)
        if label=='PLAY':T(x+27,y+33,'STOP',9,PAPER)
R(58,258,210,58,'#E7E1D7',r=12)
for x,label in [(72,'OCT-'),(170,'OCT+')]:R(x,268,82,38,'#D5D1CA',r=8);T(x+41,279,label,14,MUTED)

# The FM-1 pads are in two rows. Only the first four upper/black positions care.
R(36,340,928,270,'#E7E1D7',r=16)
pitch=(928-24)/16;kw=pitch*.78
for i in range(16):
    x=48+i*pitch+(pitch-kw)/2
    R(x,340+270*.43,kw,270*.52,PALEBLUE,'#B5CADF',kw/2)
    T(x+kw/2,548,['C','D','E','G','A'][i%5],14,BLUE)
black_after=[0,1,2,4,5,7,8,9,11,12,14]
for k,a in enumerate(black_after):
    x=48+(a+1)*pitch-kw/2
    R(x,340+270*.06,kw,270*.33,TEAL if k<4 else '#C7C5BF',r=kw/2)
    if k<4:T(x+kw/2,387,str(k+1),26,PAPER)
T(203,324,'FIRST FOUR LIT UPPER KEYS = CARE',17,TEAL)
T(636,324,'OTHER UPPER KEYS: NO CARE ACTION',14,MUTED)
T(500,615,'LOWER / WHITE KEYS = MUSICAL NOTES',16,BLUE)
text(36,478,'UPPER / BLACK KEYS: THE FIRST FOUR, LEFT TO RIGHT',10,TEAL,bold=True)
cardw=(W-72-3*8)/4
cards=[('1','FEED','Give him a burger.'),('2','PRAISE','Lift his mood.'),('3','CLEAN','Remove the mess.'),('4','REST','Sleep / wake.')]
for i,(n,label,desc) in enumerate(cards):
    x=36+i*(cardw+8);rect(x,496,cardw,53,MINT,r=7)
    dot(x+16,512,9,TEAL);text(x+16,507,n,10,PAPER,True,'center')
    text(x+31,505,label,11,TEAL,True);text(x+12,530,desc,9.5)

rect(36,561,W-72,45,PALEBLUE,r=7)
text(48,569,'LOWER / WHITE KEYS: PLAY MUSIC',10.5,BLUE,True)
text(48,586,'Goofy pentatonic notes. When happy, he joins on bongos - except during the news.',9.5)

col=(W-72-24)/2;right=36+col+24
text(36,616,'BUTTONS & SELECT',12,bold=True)
text(right,616,'SOUND & SAVING',12,bold=True)
# Tight, plain-language control reference; labels match the physical panel.
y=635
for label,desc in [('PLAY / STOP','Pause or continue. Needs, time and animation freeze.'),('SELECT / SEL','In GLO only: SELECT chooses; SEL confirms and lights.'),('HOME','Open / close Status and the key guide.'),('GLO','Start Over defaults to No. HOME / GLO cancels.')]:
    text(36,y,label,9.5,TEAL,True);dy=wrapped(36,y+13,desc,col,9.5);y+=13+dy+7

y=635
for label,desc in [('MASTER','Overall volume.'),('KEYBOARD SOUND','Dry notes, without reverb or delay. K1-K4 are unused.'),('SAVE','Save when music and speech are quiet. Changed progress also autosaves every minute.'),('YOUR THREE BARS','Food, Mood and Energy. Rest refills energy and helps mood; nights pass faster while asleep.')]:
    text(right,y,label,9.5,TEAL,True);dy=wrapped(right,y+13,desc,col,9.5);y+=13+dy+7

line(36,787,W-36,787)
text(36,799,'GAME OVER',9.5,bold=True)
text(108,799,'All three bars at zero? A short sound plays. Press any button to start fresh.',9)
text(36,820,'Greyed controls are unused in this version. Unofficial parody; no endorsement implied.',8,MUTED)
c.showPage();c.save()
print(OUT)
