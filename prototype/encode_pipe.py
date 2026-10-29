#!/usr/bin/env python3
"""Convert the AEP1 export to the 8-bit read-only table used by Felucca."""
import pathlib
import struct
import sys

source, target = map(pathlib.Path, sys.argv[1:])
b = source.read_bytes()
if b[:8] != b'AEP1\x30\x25\x00\x00':
    raise SystemExit('unexpected bank format')
offset = 8
waves = []
meta = []
for _ in range(37):
    attack, loop, step, release = struct.unpack_from('<4I', b, offset)
    offset += 16
    count = attack + loop
    if not loop or step not in (1, 2, 3) or not release or offset + count * 2 > len(b):
        raise SystemExit('invalid wave')
    samples = struct.unpack_from(f'<{count}h', b, offset)
    offset += count * 2
    meta.append((len(waves), attack, loop, step))
    waves.extend(max(-128, min(127, round(x / 256))) for x in samples)
if offset != len(b):
    raise SystemExit('unexpected trailing data')
with target.open('w') as f:
    f.write('/* Aeolus-generated Principal 8, 8-bit. GPL-3.0-or-later. */\n')
    f.write('static const struct { uint32_t offset, attack, loop, step; } organ_meta[37] = {\n')
    for row in meta:
        f.write(' {%d,%d,%d,%d},\n' % row)
    f.write('};\nstatic const int8_t organ_samples[] = {\n')
    for i in range(0, len(waves), 32):
        f.write(','.join(map(str, waves[i:i+32])) + ',\n')
    f.write('};\n')
