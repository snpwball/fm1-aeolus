#!/usr/bin/env python3
"""Losslessly pack precomputed 8-bit Aeolus waves in seekable blocks."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parent
# Repack the accepted waves, never regenerate stochastic Aeolus attacks.
original=(ROOT/'baseline/organ_quality_16_packed.h').read_text()
def nums(s): return list(map(int,re.findall(r'-?\d+',s)))
meta=re.findall(r'static const organ_info organ_meta_\d+\[\] = \{(.*?)\};',original,re.S)
infos=[list(zip(*[iter(nums(x))]*4)) for x in meta]
samples=[]
for i,data in enumerate(re.findall(r'static const uint8_t organ_samples_\d+\[\] = \{(.*?)\};',original,re.S)):
 data=nums(data);wave=[];off=0
 while off<len(data):
  base=data[off] if data[off]<128 else data[off]-256;bits=data[off+1];off+=2
  acc=int.from_bytes(bytes(data[off:off+2*bits]),'little');off+=2*bits
  wave.extend(base+((acc>>(bits*k))&((1<<bits)-1)) for k in range(16))
 lastinfo=infos[i][-1];samples.append(wave[:sum(lastinfo[:3])])
def old(name): return nums(re.search(r'static const uint8_t '+name+r'\[ORGAN_STOPS\]=\{(.*?)\}',original).group(1))
gaps,last,scale,low,high=[old(x) for x in ('organ_gaps','organ_last','organ_scale','organ_lo','organ_hi')]
RAW_STOPS = {1, 2, 3}  # Busy upperwork: read these waves directly in the audio ISR.
def pack(a):
 data=[];checks=[]
 for j in range(0,len(a),16):
  if j%128==0: checks.append(len(data))
  x=a[j:j+16];x+=[x[-1]]*(16-len(x));base=min(x);bits=(max(x)-base).bit_length();data.extend([base&255,bits]);acc=0
  for k,v in enumerate(x):acc|=(v-base)<<(bits*k)
  data.extend(acc.to_bytes(bits*2,'little'))
 assert len(data)<65536
 # Validate every packed byte against the source before writing the firmware header.
 restored=[]
 for j in range(0,len(a),16):
  block=j//16; off=checks[block//8]
  for _ in range(block%8): off+=2+2*data[off+1]
  base=data[off];base=base if base<128 else base-256;bits=data[off+1];off+=2
  val=int.from_bytes(bytes(data[off:off+2*bits]),'little')
  restored.extend(base+((val>>(bits*k)) & ((1<<bits)-1)) for k in range(16))
 assert restored[:len(a)]==a
 return data,checks
out=ROOT/'vendor/Felucca-smk37/firmware/src/organ_quality.h';total=0
with out.open('w') as f:
 f.write('/* Lossless packed Aeolus 8-bit waves; GPL-3.0-or-later. */\ntypedef struct { uint32_t offset; uint16_t attack, loop; uint8_t step; } organ_info;\n#define ORGAN_STOPS 16\n')
 f.write('#define ORGAN_RAW_MASK '+str(sum(1<<i for i in RAW_STOPS))+'u\n')
 def array(t,name,a):
  f.write(f'static const {t} {name}[] = {{\n')
  for j in range(0,len(a),24):f.write(','.join(map(str,a[j:j+24]))+',\n')
  f.write('};\n')
 for i,(info,wave) in enumerate(zip(infos,samples)):
  f.write(f'static const organ_info organ_meta_{i}[] = {{\n');f.writelines('{%d,%d,%d,%d},\n'%x for x in info);f.write('};\n')
  data,checks=([v&255 for v in wave],[0]) if i in RAW_STOPS else pack(wave);array('uint8_t',f'organ_samples_{i}',data);array('uint16_t',f'organ_checks_{i}',checks);total+=len(data)+len(checks)*2
 for typ,key,prefix in [('organ_info','organ_metas','organ_meta'),('uint8_t','organ_waves','organ_samples'),('uint16_t','organ_checks','organ_checks')]:
  f.write(f'static const {typ} *const {key}[ORGAN_STOPS]={{'+','.join(f'{prefix}_{i}' for i in range(16))+'};\n')
 for key,v in [('organ_gaps',gaps),('organ_last',last),('organ_scale',scale),('organ_lo',low),('organ_hi',high)]:f.write(f'static const uint8_t {key}[ORGAN_STOPS]={{'+','.join(map(str,v))+'};\n')
print('packed bytes including checkpoints',total,'scales',scale)
