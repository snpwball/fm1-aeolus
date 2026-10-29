#!/usr/bin/env python3
"""Inspect one fixed note of the actual Aeolus-exported pipes and firmware representation."""
from pathlib import Path
import csv, re, struct
import numpy as np
root=Path(__file__).resolve().parents[1]
rows=list(csv.DictReader(open(root/'prototype/aeolus49.csv')))
selection=[22,23,24,34,27,30,31,3,10,35,36,38]
header=(root/'vendor/Felucca-smk37/firmware/src/organ_quality.h').read_text()
peaks=[float(x) for x in re.findall(r'/\* [^\n]+: wave peak ([0-9.]+) \*/',header)]
scales=list(map(int,re.search(r'organ_scale\[ORGAN_STOPS\]=\{([^}]+)',header).group(1).split(',')))
assert len(peaks)==12

def read(aep,note):
 b=aep.read_bytes();offset=8
 for n in range(48,85):
  attack,loop,step,release=struct.unpack_from('<4I',b,offset);offset+=16
  sam=np.frombuffer(b,dtype='<i2',count=attack+loop,offset=offset);offset+=2*(attack+loop)
  if n==note:return attack,loop,step,sam.astype(np.float64)
 raise ValueError(note)

def sustained(a,l,step,x):
 pos=0; result=[]
 for _ in range(44100):
  result.append(x[pos]);pos+=1 if pos<a else step
  if pos>=a+l:pos-=l
 return np.array(result[22050:])
print('stop,aeolus_rms,encoded_rms,residual_db')
for i,idx in enumerate(selection):
 path=root/'prototype/build/quality'/(rows[idx]['source']+'.aep')
 a,l,step,s=read(path,60); ref=sustained(a,l,step,s)*(peaks[i]/30000)
 # Encoder is 8-bit rounding; scale is restored relative to the largest bank peak.
 q=np.clip(np.rint(s/256),-128,127); enc=sustained(a,l,step,q)*(scales[i]/127)*(max(peaks)/127)
 # Best common gain quantifies representation error at fixed C4; it does not assess musical voicing.
 k=np.dot(ref,enc)/max(np.dot(enc,enc),1e-30); err=ref-k*enc
 rms=lambda z:float(np.sqrt(np.mean(z*z)))
 print(f'{rows[idx]["source"]},{rms(ref):.6f},{rms(k*enc):.6f},{20*np.log10(max(rms(err),1e-15)/max(rms(ref),1e-15)):.1f}')
