#!/usr/bin/env python3
"""Replace only boot.wim in an explicitly hashed diagnostic ESP copy."""
import argparse, hashlib, json, shutil, struct, subprocess
from pathlib import Path
from pyfatfs.PyFatFS import PyFatFS

def sha(path):
    with open(path,'rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def wim_header(path):
    with open(path,'rb') as f: b=f.read(128)
    if len(b)!=128 or b[:8]!=b'MSWIM\0\0\0' or struct.unpack_from('<I',b,8)[0]<208:
        raise ValueError('invalid WIM')
    return struct.unpack_from('<I',b,44)[0],struct.unpack_from('<I',b,120)[0]
def fat_files(fat):
    result={}
    for directory,_,files in fat.walk('/'):
        for info in files:
            path=(directory.rstrip('/')+'/'+info.name)
            with fat.openbin(path,'r') as f: result[path]=hashlib.file_digest(f,'sha256').hexdigest()
    return result

def main():
    p=argparse.ArgumentParser(); p.add_argument('--source-esp',required=True,type=Path);p.add_argument('--source-sha256',required=True)
    p.add_argument('--wim',required=True,type=Path);p.add_argument('--wim-sha256',required=True);p.add_argument('--output',required=True,type=Path)
    a=p.parse_args(); meta=a.output.with_suffix(a.output.suffix+'.json')
    if a.output.exists() or meta.exists(): raise SystemExit('output exists')
    if sha(a.source_esp)!=a.source_sha256.lower() or sha(a.wim)!=a.wim_sha256.lower(): raise SystemExit('input hash mismatch')
    if a.source_esp.stat().st_size!=268435456 or wim_header(a.wim)!=(1,1): raise SystemExit('invalid ESP size or WIM boot index')
    with PyFatFS(str(a.source_esp),read_only=True,preserve_case=True) as fat: original=fat_files(fat)
    matching=[path for path in original if path.lower()=='/sources/boot.wim']
    if len(matching)!=1: raise SystemExit('source ESP has no unique boot WIM')
    wim_name=matching[0]
    shutil.copyfile(a.source_esp,a.output)
    try:
        with PyFatFS(str(a.output),preserve_case=True) as fat:
            fat.remove(wim_name)
            with a.wim.open('rb') as src, fat.openbin(wim_name,'w') as dst: shutil.copyfileobj(src,dst,1024*1024)
        repaired=subprocess.run(['fsck.vfat','-a',str(a.output)])
        if repaired.returncode not in (0,1): raise RuntimeError('FAT metadata repair failed')
        subprocess.run(['fsck.vfat','-n',str(a.output)],check=True)
        with PyFatFS(str(a.output),read_only=True,preserve_case=True) as fat: final=fat_files(fat)
        expected=dict(original); expected[wim_name]=a.wim_sha256.lower()
        if final!=expected: raise RuntimeError('ESP contents changed beyond WIM or replacement mismatch')
        record={'source_esp_sha256':a.source_sha256.lower(),'source_wim_sha256':original[wim_name],
                'replacement_wim_sha256':a.wim_sha256.lower(),'output_esp_sha256':sha(a.output),
                'wim_images':1,'wim_boot_index':1,'other_files_verified':len(final)-1,
                'status':'diagnostic only; target boot and signature unverified; no disk servicing'}
        meta.write_text(json.dumps(record,indent=2)+'\n'); print(json.dumps(record,indent=2))
    except BaseException:
        a.output.unlink(missing_ok=True); raise
if __name__=='__main__':main()
