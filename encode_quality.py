#!/usr/bin/env python3
"""Generate compact 8-bit Aeolus ranks at native sample rate (GPL-3.0-or-later)."""
from pathlib import Path
import csv, struct, subprocess, re, math
root=Path(__file__).resolve().parents[1]
rows=list(csv.DictReader(open(root/'prototype/aeolus49.csv')))
# Each item: index in Aeolus definition, semitone spacing, usable notes from C3.
selection=[(22,3,36),(23,4,36),(24,3,36),(34,3,36),(27,3,36),(30,3,36),(31,4,36),(3,6,36),(10,6,36),(35,4,36),(36,3,12),(38,3,12)]
dir=root/'prototype/build/quality';dir.mkdir(parents=True,exist_ok=True)
raws=[]
for index,gap,last in selection:
 name=rows[index]['source']; dst=dir/(name+'.aep')
 p=subprocess.run([str(root/'prototype/build/export'),str(root/'vendor/stops-0.4.0'),name,str(dst)],text=True,capture_output=True,check=True)
 peak=float(re.search(r'peak ([0-9.]+)',p.stdout).group(1));b=dst.read_bytes();off=8; samples=[]; info=[]
 hdr=(root/'vendor/stops-0.4.0'/name).read_bytes();lo=hdr[28];hi=96 if hdr[29]==46 else hdr[29]
 for n in range(37):
  attack,loop,step,release=struct.unpack_from('<4I',b,off);off+=16
  count=attack+loop; vals=struct.unpack_from(f'<{count}h',b,off);off+=count*2
  if n%gap==0 and n<=last:
   info.append((len(samples),attack,loop,step));samples.extend(max(-128,min(127,round(x/256))) for x in vals)
 assert off==len(b);raws.append((name,gap,last,peak,lo,hi,info,samples))
peakmax=max(r[3] for r in raws)
out=root/'vendor/Felucca-smk37/firmware/src/organ_quality.h';total=0
with out.open('w') as f:
 f.write('/* Original-rate Aeolus-generated waves; GPL-3.0-or-later. */\n')
 f.write('typedef struct { uint32_t offset; uint16_t attack, loop; uint8_t step; } organ_info;\n')
 for i,(name,gap,last,peak,lo,hi,info,samples) in enumerate(raws):
  f.write(f'/* {name}: wave peak {peak:.6f} */\nstatic const organ_info organ_meta_{i}[] = {{\n')
  for v in info:f.write('{%d,%d,%d,%d},\n'%v)
  f.write(f'}};\nstatic const int8_t organ_samples_{i}[] = {{\n')
  for j in range(0,len(samples),32):f.write(','.join(map(str,samples[j:j+32]))+',\n')
  f.write('};\n');total+=len(samples)
 f.write('#define ORGAN_STOPS 12\n')
 f.write('static const organ_info *const organ_metas[ORGAN_STOPS]={'+','.join(f'organ_meta_{i}' for i in range(len(raws)))+'};\n')
 f.write('static const int8_t *const organ_waves[ORGAN_STOPS]={'+','.join(f'organ_samples_{i}' for i in range(len(raws)))+'};\n')
 for key,vals in [('organ_gaps',[r[1] for r in raws]),('organ_last',[r[2] for r in raws]),('organ_scale',[round(r[3]/peakmax*127) for r in raws]),('organ_lo',[r[4] for r in raws]),('organ_hi',[r[5] for r in raws])]:
  f.write('static const uint8_t '+key+'[ORGAN_STOPS]={'+','.join(map(str,vals))+'};\n')
print('ranks',len(raws),'wave bytes',total,'scales',[round(r[3]/peakmax*127) for r in raws])
