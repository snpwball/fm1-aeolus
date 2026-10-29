#!/usr/bin/env python3
"""Make read-only Felucca tables from Aeolus AEP1 banks."""
import pathlib, struct, sys
output=pathlib.Path(sys.argv[1]); banks=[]
for bank_index, name in enumerate(sys.argv[2:]):
    b=pathlib.Path(name).read_bytes(); assert b[:8]==b'AEP1\x30\x25\x00\x00'
    off=8; meta=[]; wave=[]; meta_source=[]
    for _ in range(37):
        attack,loop,step,release=struct.unpack_from('<4I',b,off); off+=16
        count=attack+loop
        if not loop or step not in (1,2,3) or not release or off+count*2>len(b): raise ValueError('bad pipe')
        values=struct.unpack_from(f'<{count}h',b,off); off+=count*2
        if len(meta_source) % 3 == 0 and (bank_index != 8 or len(meta) < 5):
            meta.append((len(wave),attack,loop,step))
            wave.extend(max(-128,min(127,round(x/256))) for x in values)
        meta_source.append(1)
    if off!=len(b): raise ValueError('bad size')
    if bank_index == 8: meta.extend([meta[-1]] * (13 - len(meta)))
    banks.append((meta,wave))
with output.open('w') as f:
    f.write('/* Aeolus-generated pipe waves, 8-bit. GPL-3.0-or-later. */\n')
    f.write('typedef struct { uint32_t offset, attack, loop, step; } organ_info;\n')
    for i,(meta,wave) in enumerate(banks):
        f.write(f'static const organ_info organ_meta_{i}[13] = {{\n')
        for row in meta: f.write(' {%d,%d,%d,%d},\n'%row)
        f.write(f'}};\nstatic const int8_t organ_samples_{i}[] = {{\n')
        for j in range(0,len(wave),32): f.write(','.join(map(str,wave[j:j+32]))+',\n')
        f.write('};\n')
    f.write(f'#define ORGAN_STOPS {len(banks)}\n')
    f.write('static const organ_info *const organ_metas[ORGAN_STOPS] = {'+','.join(f'organ_meta_{i}' for i in range(len(banks)))+'};\n')
    f.write('static const int8_t *const organ_waves[ORGAN_STOPS] = {'+','.join(f'organ_samples_{i}' for i in range(len(banks)))+'};\n')
