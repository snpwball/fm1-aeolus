#!/usr/bin/env python3
"""Encode Aeolus generated waves for 49 ranks (GPL-3.0-or-later)."""
from pathlib import Path
import csv, struct, sys
import numpy as np
from scipy.signal import resample_poly
rows=list(csv.DictReader(open('aeolus49.csv')))
out=Path(sys.argv[1]); source=Path(sys.argv[2]); allbytes=bytearray(); meta=[]; names=[]; ranges=[]
# 4-bit signed companding: more precision near silence, full scale at index 15.
levels=np.array([-32767,-23000,-15500,-9700,-5400,-2500,-800,0,0,800,2500,5400,9700,15500,23000,32767],dtype=np.int32)
for row in rows:
 b=(source/(row['source']+'.aep')).read_bytes(); off=8; rank=[]
 raw=(Path('vendor/stops-0.4.0')/row['source']).read_bytes(); lo=raw[28]; hi=raw[29]; hi=96 if hi==46 else hi
 ranges.append((lo,hi)); names.append(row['source'])
 for n in range(37):
  attack,loop,step,release=struct.unpack_from('<4I',b,off);off+=16
  values=np.frombuffer(b,dtype='<i2',count=attack+loop,offset=off).astype(np.float64);off+=2*(attack+loop)
  if n%6: continue
  # Filter attack and periodic loop separately; wrap at the loop seam.
  a=resample_poly(values[:attack],1,4,padtype='line') if attack else np.zeros(0)
  l=resample_poly(values[attack:],1,4,padtype='wrap')
  x=np.concatenate((a,l)); x=np.clip(np.rint(x),-32767,32767).astype(np.int32)
  codes=np.searchsorted((levels[:-1]+levels[1:])//2,x).astype(np.uint8)
  if len(codes)%2: codes=np.append(codes,np.uint8(8))
  offset=len(allbytes); packed=codes[::2] | codes[1::2]<<4; allbytes.extend(packed.tobytes())
  rank.append((offset,len(a),len(l),step))
 assert len(rank)==7
 meta.append(rank)
with out.open('w') as f:
 f.write('/* 49 Aeolus ranks: 4-bit quantized filtered pipes, GPL-3.0-or-later. */\n')
 f.write('typedef struct { uint32_t offset; uint16_t attack, loop; uint8_t step; } organ_info;\n')
 f.write('static const organ_info organ_metas[49][7]={\n')
 for rank in meta:
  f.write('{'+','.join('{%d,%d,%d,%d}'%v for v in rank)+'},\n')
 f.write('};\nstatic const uint8_t organ_ranges[49][2]={'+','.join('{%d,%d}'%r for r in ranges)+'};\n')
 f.write('static const int16_t organ_levels[16]={'+','.join(map(str,levels))+ '};\n')
 f.write('static const uint8_t organ_waves[]={\n')
 for i in range(0,len(allbytes),32): f.write(','.join(map(str,allbytes[i:i+32]))+',\n')
 f.write('};\n#define ORGAN_STOPS 49\n')
print('wave bytes',len(allbytes),'metadata entries',49*7)
