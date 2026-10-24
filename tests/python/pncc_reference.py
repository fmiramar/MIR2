#!/usr/bin/env python3
"""Independent offline oracle for Kim/Stern 2016 Eqs. 3–19.

Fixture uses deterministic band powers, zero padding at negative time, M=2,
N=4, 10ms frames, first-positive online mean initialization, orthonormal DCT.
No third-party implementation is imported. Regenerate the tracked fixture via:
python3 tests/python/pncc_reference.py tests/reference/pncc-stages.csv
"""
import csv,math,sys
from pathlib import Path
BANDS=8

def powers(t):
    if t<8 or 100<=t<120:return [0.0]*BANDS
    base=[(i+1)*.02 for i in range(BANDS)]
    if 25<=t<60:base=[v*(3 if i in [2,3] else 1) for i,v in enumerate(base)]
    if 45<=t<70:base=[v*(1+.6*math.sin(t*.7+i)**2) for i,v in enumerate(base)]
    if t>=70:base=[v*10 for v in base]
    return base

def follow(x,y):
    coefficient=.999 if x>=y else .5
    return coefficient*y+(1-coefficient)*x

power=[powers(t) for t in range(160)]
noise=[0.0]*BANDS;lower=[0.0]*BANDS;peak=[0.0]*BANDS;mean=0.0
rows=[]
for current in range(2,len(power)):
    # Offline direct neighborhood sum, independent of the C++ ring indexing.
    center=current-2
    q=[sum(power[t][b] if t>=0 else 0 for t in range(center-2,center+3))/5 for b in range(BANDS)]
    noise=[.9*x if current==2 else follow(x,y) for x,y in zip(q,noise)]
    rectified=[max(0,x-y) for x,y in zip(q,noise)]
    lower=[x if current==2 else follow(x,y) for x,y in zip(rectified,lower)]
    masked=[r if r>=.85*p else .2*p for r,p in zip(rectified,peak)]
    selected=[max(m,f) if x>=2*n else f for x,n,m,f in zip(q,noise,masked,lower)]
    peak=[max(.85*p,x) for p,x in zip(peak,rectified)]
    gains=[y/x if x>1e-30 else 0 for y,x in zip(selected,q)]
    weighted=[]
    for b in range(BANDS):
        neighbors=gains[max(0,b-4):min(BANDS,b+5)]
        weighted.append(power[center][b]*sum(neighbors)/len(neighbors))
    average=sum(weighted)/BANDS
    mean=average if mean==0 and average>1e-30 else .999*mean+.001*average
    compressed=[(v/max(1e-30,mean))**(1/15) if v>0 else 0 for v in weighted]
    coefficients=[sum(x*math.cos(math.pi*c*(b+.5)/BANDS) for b,x in enumerate(compressed))*math.sqrt((1 if c==0 else 2)/BANDS) for c in range(4)]
    rows.append([current,mean,*q,*noise,*lower,*weighted,*coefficients])
path=Path(sys.argv[1]);path.parent.mkdir(parents=True,exist_ok=True)
with path.open('w',newline='') as stream:
    writer=csv.writer(stream, lineterminator='\n')
    writer.writerow(['frame','mean']+[f'{stage}_{b}' for stage in ['medium','noise','floor','weighted'] for b in range(BANDS)]+[f'c{i}' for i in range(4)])
    writer.writerows(rows)
print(f'{len(rows)} PNCC oracle frames written')
