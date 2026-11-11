#!/usr/bin/env python3
"""Canonical unsigned metadata only; never compile, link, sign, or change payload."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile


def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def require(ok,text):
    if not ok:raise ValueError(text)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--expected-raw-manifest-sha256',required=True)
    p.add_argument('--expected-source-commit',required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();repo=Path(__file__).resolve().parents[2]
    require(subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()==a.expected_source_commit,'Metadata source commit mismatch')
    require(not subprocess.check_output(['git','-C',str(repo),'status','--porcelain']),'Clean metadata source required')
    raw=a.build/'unsigned-manifest.json'
    require(sha(raw)==a.expected_raw_manifest_sha256,'Raw manifest binding')
    m=json.loads(raw.read_text(encoding='utf-8-sig'))
    require(m['schema']=='mt8167-keypad14-incremental-unsigned-v1' and m['version']=='0.0.1.4' and m['source_commit']=='39966436b4dc65c28386b02ba5481ce9513a894e' and m['recipe_source_commit']=='e2eb21f55449e2ffcb2af3d2a6d57c0cac4815ac' and m['compiled_TUs']==1 and not m['signed'],'Actual completed unsigned candidate required')
    for row in m['unsigned_outputs']+m['objects']:
        rel=row['path'].replace('\\','/')
        require('..' not in Path(rel).parts and not Path(rel).is_absolute(),'Unsafe artifact path')
        require(sha(a.build/rel)==row['sha256'] and (a.build/rel).stat().st_size==row['bytes'],'Payload/object hash mismatch')
    require(len(m['objects'])==3 and len(m['unsigned_outputs'])==2,'Expected 3objects/SYS+INF')
    require(sha(a.build/'actual-dependencies.json')==m['actual_dependencies_sha256'] and len(m['actual_dependencies'])==88,'Actual dependency receipt mismatch')
    require(json.loads((a.build/'actual-dependencies.json').read_text())==m['actual_dependencies'],'Dependency rows mismatch')
    for row in m['repo_inputs']:require(sha(repo/row['path'])==row['sha256'],'Runtime source/header/INF changed')
    system=a.build/'package/mt8167-keypad.sys';inf=a.build/'package/mt8167-keypad.inf'
    require(sha(system)=='bb86f2926be95aa9a666b7b28225225101c6a1478539bd98349cbfa046eb8bfe' and sha(inf)=='5bd471ba9651f790e78cd0bb8415d1f19fa03a3c2bb6624fd20304ce647e9a83','Actual14 unsigned payload binding')
    b=system.read_bytes();offset=struct.unpack_from('<I',b,60)[0]
    require(b[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',b,offset+4)[0]==0x1c4,'Unsigned SYS ARMNT')
    require(b'DriverVer=11/11/2026,0.0.1.4' in inf.read_bytes(),'INF version/date')
    require(not a.output.exists(),'Fresh metadata output required')
    a.output.mkdir(parents=True);(a.output/'package').mkdir()
    for f in (system,inf):shutil.copyfile(f,a.output/'package'/f.name)
    # Raw receipt remains byte intact; compiled/recipe source identities are not replaced.
    shutil.copyfile(raw,a.output/'raw-unsigned-manifest.json')
    m['target']='MT6392\\Buttons'
    for row in m['unsigned_outputs']+m['objects']:row['path']=row['path'].replace('\\','/')
    m.update(metadata_source_commit=a.expected_source_commit,raw_unsigned_manifest_sha256=sha(raw),metadata_only=True,payload_changed=False,new_compile=0,new_link=0,new_sign=0)
    (a.output/'unsigned-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    with zipfile.ZipFile(a.output/'mt8167-keypad14-unsigned.zip','w',zipfile.ZIP_DEFLATED) as z:
        for rel in ('package/mt8167-keypad.sys','package/mt8167-keypad.inf','unsigned-manifest.json','raw-unsigned-manifest.json'):z.write(a.output/rel,rel)
    print('KEYPAD14_METADATA_ONLY_UNSIGNED_PAYLOAD_UNCHANGED')


if __name__=='__main__':main()
