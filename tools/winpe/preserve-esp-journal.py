#!/usr/bin/env python3
"""Preserve a verified recovery journal in a new local ESP image, never flash."""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import shutil
import struct
import subprocess
from pyfatfs.PyFatFS import PyFatFS
from fat32_fsinfo import mark_free_space_unknown

ROOT=Path(__file__).resolve().parents[2]
WORKSPACE=ROOT.parents[1]
spec=importlib.util.spec_from_file_location('maintenance_builder',ROOT/'tools/winpe/build-maintenance-esp.py')
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)

def verify_journal(fat, files):
    key='/a04backup/ready'
    if key not in files:raise ValueError('complete recovery journal is required')
    with fat.openbin(files[key][0], 'r') as f: data=f.read()
    if len(data)<48 or hashlib.sha256(data[:-32]).digest()!=data[-32:]:raise ValueError('journal manifest hash mismatch')
    magic,version,count,reserved=struct.unpack_from('<IIII',data)
    if magic!=0x41303442 or (version in (1,2) and reserved) or (version in (3,4) and not reserved):raise ValueError('journal manifest identity rejected')
    if version==1: capacity,entry_size=11,72
    elif version in (2,3,4):capacity,entry_size=99,328
    else:raise ValueError('unsupported journal version')
    if not (13 if version==4 else 11)<=count<=capacity or len(data)!=16+capacity*entry_size+32:raise ValueError('journal manifest size/count mismatch')
    names=set()
    for i in range(count):
        offset=16+i*entry_size;present=struct.unpack_from('<I',data,offset)[0]
        length=struct.unpack_from('<Q',data,offset+32)[0];digest=data[offset+40:offset+72].hex()
        if present>1 or (i in (0,4) and not present) or (version in (2,3,4) and i>=11 and not present):raise ValueError('invalid presence')
        if version in (2,3,4):
            raw=data[offset+72:offset+328]
            if raw[-2:]!=b'\0\0':raise ValueError('unterminated journal name')
            name=raw.decode('utf-16le').split('\0',1)[0]
            if i<11 and name:raise ValueError('unexpected fixed-entry name')
            if i>=11:
                if version==4 and i<13:
                    if name != ('FIXED-oem33.inf' if i==11 else 'FIXED-oem35.inf'):raise ValueError('invalid fixed INF name')
                    names.add(name.lower())
                    if present:
                        item=f'/a04backup/item{i:02}.bin'
                        if item not in files or files[item][1]!=digest or fat.getsize(files[item][0])!=length:raise ValueError(f'journal item {i} mismatch')
                    continue
                txr=name.startswith('TxR\\');base=name[4:] if txr else name
                prefix=base.startswith('{') if txr else base.upper().startswith(('SYSTEM{','SOFTWARE{'))
                if (not prefix or not re.fullmatch(r'[A-Za-z0-9{}._-]{8,119}',base) or '}' not in base or
                    not base.lower().endswith(('.blf','.regtrans-ms')) or name.lower() in names):
                    raise ValueError('invalid or duplicate sidecar name')
                names.add(name.lower())
        if present:
            item=f'/a04backup/item{i:02}.bin'
            if item not in files or files[item][1]!=digest or fat.getsize(files[item][0])!=length:
                raise ValueError(f'journal item {i} mismatch')
    intent_key='/a04backup/intent'
    if intent_key in files:
        with fat.openbin(files[intent_key][0],'r') as f: intent=f.read()
        if (len(intent)!=76 or struct.unpack_from('<III',intent)!=(0x544e4934,1,reserved) or
            version not in (3,4) or intent[12:44]!=data[-32:] or hashlib.sha256(intent[:-32]).digest()!=intent[-32:]):
            raise ValueError('invalid transaction intent')
    commit_key='/a04backup/commit'
    if commit_key in files:
        with fat.openbin(files[commit_key][0],'r') as f: committed=f.read()
        expected=('A04COMMIT_V1 '+hashlib.sha256(data).hexdigest()+'\n').encode('ascii')
        if committed!=expected:raise ValueError('invalid transaction completion marker')
    return {'version':version,'count':count,'target_serial':reserved if version in (3,4) else None,
            'armed':intent_key in files,'complete':commit_key in files,'manifest_sha256':hashlib.sha256(data).hexdigest()}

def main():
    p=argparse.ArgumentParser();p.add_argument('--journal-esp',type=Path,required=True);p.add_argument('--journal-esp-sha256',required=True)
    p.add_argument('--candidate-esp',type=Path,required=True);p.add_argument('--candidate-esp-sha256',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    source,candidate=map(builder.regular,(a.journal_esp,a.candidate_esp));out=a.output.resolve()
    if WORKSPACE not in out.parents or out.exists() or out.with_suffix(out.suffix+'.json').exists():raise ValueError('output must be new in workspace')
    if source==candidate or builder.sha(source)!=a.journal_esp_sha256.lower() or builder.sha(candidate)!=a.candidate_esp_sha256.lower():raise ValueError('input identity mismatch')
    if source.stat().st_size!=256*1024*1024 or candidate.stat().st_size!=source.stat().st_size:raise ValueError('ESP size rejected')
    for image in (source,candidate):subprocess.run(['fsck.vfat','-n',str(image)],check=True,stdout=subprocess.DEVNULL)
    with PyFatFS(str(source),read_only=True,preserve_case=True) as fat:
        source_files=builder.fat_files(fat); journal=verify_journal(fat,source_files)
        for directory,dirs,_ in fat.walk('/A04BACKUP'):
            if dirs:raise ValueError('nested recovery journal directory rejected')
        recovery={k:v for k,v in source_files.items() if k.startswith('/a04backup/')}
    with PyFatFS(str(candidate),read_only=True,preserve_case=True) as fat:candidate_files=builder.fat_files(fat)
    if any(k.startswith('/a04backup/') for k in candidate_files):raise ValueError('candidate already has recovery files')
    out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(candidate,out)
    mark_free_space_unknown(out)
    with PyFatFS(str(source),read_only=True,preserve_case=True) as src, PyFatFS(str(out),preserve_case=True) as dst:
        dst.makedir('/A04BACKUP')
        for _,(name,_) in sorted(recovery.items()):
            with src.openbin(name,'r') as i,dst.openbin('/A04BACKUP/'+name.rsplit('/',1)[1],'w') as o:shutil.copyfileobj(i,o,1024*1024)
    check=subprocess.run(['fsck.vfat','-n',str(out)],capture_output=True,text=True)
    if check.returncode!=0:raise ValueError('output FAT check failed; original images retained')
    with PyFatFS(str(out),read_only=True,preserve_case=True) as fat:
        after=builder.fat_files(fat);verified=verify_journal(fat,after)
    expected=dict(candidate_files);expected.update(recovery)
    if {k:v[1] for k,v in after.items()}!={k:v[1] for k,v in expected.items()} or verified!=journal:
        raise ValueError('retained files or journal differ')
    result={'journal_esp_sha256':builder.sha(source),'candidate_esp_sha256':builder.sha(candidate),
            'output_esp_sha256':builder.sha(out),'journal':journal,
            'preserved_recovery_files':{v[0]:v[1] for v in recovery.values()},
            'status':'local image only; capture freshness, exclusive access and device overwrite are not established'}
    out.with_suffix(out.suffix+'.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
