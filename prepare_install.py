#!/usr/bin/env python3
import hashlib
import json
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parent
dist=ROOT/'installer/dist'
dist.mkdir(parents=True,exist_ok=True)
for path in (ROOT/'installer/src').iterdir():
    shutil.copyfile(path,dist/path.name)
for name in ['fm1ota.js','fm1pkg.js']:
    shutil.copyfile(ROOT/'vendor/Felucca-smk37/web'/name,dist/name)
source=ROOT/'vendor/Felucca-smk37/build/felucca-smk37.fwsc'
data=source.read_bytes()
sha=hashlib.sha256(data).hexdigest()
banks=json.loads((ROOT/'build/organ-banks.json').read_text())
header=(ROOT/'vendor/Felucca-smk37/firmware/src/organ_quality.h').read_text()
assert f'"{banks["build_id"]}"' in header
assert banks['build_id'].encode()+b'\0' in (ROOT/'vendor/Felucca-smk37/build/felucca.bin').read_bytes()
banks['firmware_sha256']=sha
for bank in banks['banks']:
    path=ROOT/'build'/bank['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==bank['sha256']
    shutil.copyfile(path,dist/bank['file'])
(dist/'organ-banks.json').write_text(json.dumps(banks,indent=2)+'\n')
shutil.copyfile(ROOT/'install_stop_bank.mjs',dist/'install_stop_bank.mjs')
shutil.copyfile(source,dist/'organ-smk37-pro.fwsc')
shutil.copyfile(source,dist/'organ-raw-stops-emu.fwsc')
(dist/'build-info.json').write_text(json.dumps(dict(product='SMK-37 Pro_900',sha256=sha,size=len(data)),indent=2)+'\n')
(dist/'emu').mkdir(exist_ok=True)
shutil.copyfile(ROOT/'emulator_stop_bank.mjs',dist/'emu/stop_bank.mjs')
main=dist/'emu/main.js'
if main.exists():
    text=main.read_text()
    if 'seedStopBanks' not in text:
        text='import {seedStopBanks} from "./stop_bank.mjs";\n'+text
        text=text.replace('const sectors = record?.sectors || [];','const sectors = await seedStopBanks(f,record?.sectors || [],packageHash);')
    main.write_text(text)
catalog_path=dist/'emu/firmwares.json'
catalog=json.loads(catalog_path.read_text()) if catalog_path.exists() else dict(default='organ18',firmwares=[dict(id='organ18',name='Pipe organ',version='local',author='this computer',license='GPL-3.0')])
entry=catalog['firmwares'][0]
entry['package']='../organ-raw-stops-emu.fwsc?v='+sha[:12]
entry['about']='16 uncompressed Aeolus-derived organ stops.'
entry['stopBanks']=[dict(file='../'+b['file']+'?v='+b['sha256'][:12],sha256=b['sha256'],slot=b['slot']) for b in banks['banks']]
(dist/'emu/firmwares.json').write_text(json.dumps(catalog,indent=2)+'\n')
import re
for name in ['emu/main.js','emu/index.html']:
    p=dist/name
    if not p.exists():continue
    s=re.sub(r'(firmwares\.json\?v=|main\.js\?v=)[^"\s]+',lambda m:m[1]+'raw-stops-'+sha[:8],p.read_text())
    p.write_text(s)
p=dist/'index.html'
client_sha=hashlib.sha256((dist/'app.js').read_bytes()).hexdigest()[:12]
p.write_text(p.read_text().replace('src="app.js"','src="app.js?v='+sha[:12]+'-'+client_sha+'"'))
print('Local installer prepared:',sha)
