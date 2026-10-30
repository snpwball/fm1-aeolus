#!/usr/bin/env python3
import hashlib
import json
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SLOT_SIZE = 0x14000
DATA_OFFSET = 512
CAPACITY = SLOT_SIZE - DATA_OFFSET
meta = json.loads((ROOT / 'stops/stops.json').read_text())
waves = [(ROOT / 'stops' / s['file']).read_bytes() for s in meta['stops']]
for stop, wave in zip(meta['stops'], waves):
    assert hashlib.sha256(wave).hexdigest() == stop['sha256']
    assert len(wave) == sum(stop['pipes'][-1][:3])

candidates = []
for mask in range(1 << len(waves)):
    size = sum(len(wave) for i, wave in enumerate(waves) if mask >> i & 1)
    if size <= CAPACITY:
        candidates.append((size, mask))
candidates.sort(reverse=True)
best = (0, 0, 0)
for size, mask in candidates:
    if size + candidates[0][0] <= best[0]:
        break
    for other_size, other_mask in candidates:
        if size + other_size <= best[0]:
            break
        if not mask & other_mask:
            best = (size + other_size, mask, other_mask)
            break

build = ROOT / 'build'
build.mkdir(exist_ok=True)
locations = [(255, 0)] * len(waves)
banks = []
for slot, mask in enumerate(best[1:]):
    data = bytearray()
    for i, wave in enumerate(waves):
        if mask >> i & 1:
            locations[i] = (slot, len(data))
            data.extend(wave)
    used = len(data)
    data.extend(b'\xff' * (-len(data) % 256))
    assert len(data) <= CAPACITY
    crc = zlib.crc32(data)
    header = bytearray(b'\xff' * DATA_OFFSET)
    header[:480] = bytes(480)
    struct.pack_into('<IHBB8s4I', header, 0, 0x504D5346, 1, 1, 0,
                     f'ORGSTOP{slot}'.encode(), len(data), crc, 0x4F524731, slot)
    struct.pack_into('<5Ihh4B', header, 32, 0, 2, 0, 1, 65536, 960, 0, 0, 48, 84, 0)
    image = bytes(header + data)
    name = f'organ-bank-{slot}.bin'
    (build / name).write_bytes(image)
    banks.append(dict(file=name, slot=slot, size=len(image), used=used,
                      data_size=len(data), crc32=crc, sha256=hashlib.sha256(image).hexdigest()))
(build / 'organ-banks.json').write_text(json.dumps(dict(banks=banks), indent=2) + '\n')

with (ROOT / 'vendor/Felucca-smk37/firmware/src/organ_quality.h').open('w') as f:
    f.write('/* Aeolus-derived stop waves; GPL-3.0-or-later. */\n')
    f.write('typedef struct { uint32_t offset; uint16_t attack, loop; uint8_t step; } organ_info;\n#define ORGAN_STOPS 16\n')
    def array(typ, name, values):
        f.write(f'static const {typ} {name}[] = {{\n')
        for j in range(0, len(values), 24):
            f.write(','.join(map(str, values[j:j+24])) + ',\n')
        f.write('};\n')
    for i, (stop, wave) in enumerate(zip(meta['stops'], waves)):
        f.write(f'static const organ_info organ_meta_{i}[] = {{\n')
        for pipe in stop['pipes']:
            f.write('{%d,%d,%d,%d},\n' % tuple(pipe))
        f.write('};\n')
        if locations[i][0] == 255:
            array('uint8_t', f'organ_samples_{i}', wave)
    f.write('static const organ_info *const organ_metas[ORGAN_STOPS]={' + ','.join(f'organ_meta_{i}' for i in range(16)) + '};\n')
    f.write('static const uint8_t *const organ_waves[ORGAN_STOPS]={' + ','.join(f'organ_samples_{i}' if locations[i][0] == 255 else '0' for i in range(16)) + '};\n')
    array('uint8_t', 'organ_bank_slot', [x[0] for x in locations])
    array('uint32_t', 'organ_bank_offset', [x[1] for x in locations])
    array('uint32_t', 'organ_bank_length', [x['data_size'] for x in banks])
    array('uint32_t', 'organ_bank_crc', [x['crc32'] for x in banks])
    for key in ['gaps', 'last', 'scale', 'lo', 'hi']:
        array('uint8_t', 'organ_' + key, meta[key])
print(f'Stop area: {best[0]} wave bytes, {sum(b["size"] for b in banks)} of {2*SLOT_SIZE} bytes including headers and padding')
print(f'Application waves: {sum(len(w) for i,w in enumerate(waves) if locations[i][0] == 255)} bytes')

header = ROOT / 'vendor/Felucca-smk37/firmware/src/organ_quality.h'
digest = hashlib.sha256(header.read_bytes())
for source in sorted((ROOT / 'vendor/Felucca-smk37/firmware').rglob('*')):
    if source.is_file() and source.suffix in ('.c', '.h', '.S', '.ld') and source != header:
        digest.update(str(source.relative_to(ROOT)).encode())
        digest.update(source.read_bytes())
build_id = digest.hexdigest()[:16]
with header.open('a') as f:
    f.write('static const char organ_build_id[]="' + build_id + '";\n')
(build / 'organ-banks.json').write_text(json.dumps(dict(build_id=build_id, banks=banks), indent=2) + '\n')
