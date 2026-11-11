#!/usr/bin/env python3
"""Collect fixed OS images inside a main-owned Recovery read-only mount.

No mount, reboot, hive load or device write. The capture manifest remains intact;
the separate receipt binds the logical copies to its Recovery generation.
"""
import argparse, hashlib, json, re, subprocess
from pathlib import Path

MOUNT='/tmp/rt125-diag'
ASSETS=('Windows/System32/user32.dll','Windows/System32/win32k.sys')
MAX_BYTES=16*1024*1024
ANCHOR='Windows/System32/config/SYSTEM'

def sha(data):return hashlib.sha256(data).hexdigest()
def bounded_stream(serial,remote,size):
    p=subprocess.Popen(['adb','-s',serial,'exec-out','cat',remote],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
    try:
        data=p.stdout.read(size+1)
        if len(data)!=size:raise ValueError('Source size changed during bounded stream')
        if p.wait(timeout=30):raise ValueError('Readonly stream failed')
        return data
    finally:
        if p.poll() is None:p.kill();p.wait()
        p.stdout.close()

def collect(root,serial,manifest,out,run=subprocess.run,stream=bounded_stream,include_cdd=False):
    root=Path(root).resolve(strict=True);manifest=Path(manifest).resolve(strict=True);out=Path(out).resolve()
    if not manifest.is_relative_to(root) or not out.is_relative_to(root):raise ValueError('Root-local paths required')
    original=manifest.read_bytes();cap=json.loads(original)
    assets=ASSETS+('Windows/System32/cdd.dll',) if include_cdd else ASSETS
    if not (cap['status']=='captured-unmounted' and cap['read_only'] is True and cap['unmounted'] is True and cap['errors']==[] and cap['serial']==serial):raise ValueError('Completed actual same-serial capture required')
    anchor=next(x for x in cap['files'] if x['source_path']==ANCHOR)
    out.mkdir(parents=True,exist_ok=True)
    receipt=out/'gpu-runtime-assets.json'
    if receipt.exists() or any((out/'fixed'/x).exists() for x in assets):raise ValueError('Never overwrite runtime evidence')
    rows=[];errors=[];before=None;after=None
    def adb(*args):
        return run(['adb','-s',serial,*args],check=True,capture_output=True,timeout=60).stdout.decode().strip()
    def identity():
        if adb('get-state')!='recovery':raise ValueError('Existing Recovery only')
        boot=adb('shell','cat','/proc/sys/kernel/random/boot_id')
        if boot!=cap['recovery_boot_id']:raise ValueError('Recovery generation differs from capture')
        mounts=[x.split() for x in adb('shell','cat','/proc/mounts').splitlines()]
        mounted=[x for x in mounts if len(x)>=4 and x[1]==MOUNT]
        if len(mounted)!=1 or 'ro' not in mounted[0][3].split(','):raise ValueError('Main-owned readonly mount required')
        device=adb('shell','readlink','-f',mounted[0][0])
        if device!=cap['source_partition']:raise ValueError('Different captured Windows partition')
        h=adb('shell','sha256sum',MOUNT+'/'+ANCHOR).split()[0]
        if h!=anchor['sha256']:raise ValueError('Captured SYSTEM generation changed')
        return dict(recovery_boot_id=boot,source_partition=device,SYSTEM_sha256=h)
    try:
        before=identity()
        for name in assets:
            row=dict(source_path=name,status='not-collected');rows.append(row)
            remote=MOUNT+'/'+name
            present=adb('shell','sh','-c',"'if [ -f "+remote+" ] && [ ! -L "+remote+" ]; then echo PRESENT; elif [ ! -e "+remote+" ]; then echo ABSENT; else echo REJECTED; fi'")
            if present=='ABSENT':row['status']='missing';continue
            if present!='PRESENT':raise ValueError('Runtime source is not a regular non-symlink file')
            resolved=adb('shell','readlink','-f',remote)
            if resolved!=remote:raise ValueError('Reparse/symlink ancestor rejected')
            metadata=adb('shell','stat','-c','%s:%Y:%i',remote)
            size=int(metadata.split(':')[0])
            if not 0<size<=MAX_BYTES:raise ValueError('Runtime asset exceeds fixed size limit')
            source_hash=adb('shell','sha256sum',remote).split()[0]
            if not re.fullmatch('[0-9a-f]{64}',source_hash):raise ValueError('Invalid source digest')
            data=stream(serial,remote,size)
            if len(data)!=size or sha(data)!=source_hash:raise ValueError('Runtime stream size/digest mismatch')
            if adb('shell','stat','-c','%s:%Y:%i',remote)!=metadata or adb('shell','sha256sum',remote).split()[0]!=source_hash:raise ValueError('Source changed across capture')
            dst=out/'fixed'/name;dst.parent.mkdir(parents=True,exist_ok=True)
            with dst.open('xb') as f:f.write(data)
            dst.chmod(0o600);row.update(status='captured',backup_path=str(dst.relative_to(out)),bytes=size,sha256=source_hash)
        after=identity()
    except Exception as e:errors.append(str(e))
    unchanged=manifest.read_bytes()==original
    if not unchanged:errors.append('Original capture manifest changed')
    result=dict(schema='GPU_RUNTIME_ASSETS_LOGICAL_RO_V1',source_manifest={'path':str(manifest.relative_to(root)),'sha256':sha(original)},source_manifest_unchanged=unchanged,
        read_only=True,mount_owner='main',mount_modified=False,physical_partition_readback=False,max_file_bytes=MAX_BYTES,
        before=before,after=after,same_recovery_and_SYSTEM_generation=before is not None and before==after,
        files=rows,missing_files=[x for x in rows if x['status']=='missing'],errors=errors,
        complete=not errors and len(rows)==len(assets) and all(x['status']=='captured' for x in rows),hardware_success_asserted=False)
    receipt.write_text(json.dumps(result,indent=2)+'\n');receipt.chmod(0o600)
    if errors:raise ValueError('; '.join(errors))
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--workspace',type=Path,required=True);p.add_argument('--serial',required=True)
    p.add_argument('--capture-manifest',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--include-cdd',action='store_true',help='Also collect the OS canonical display driver for source-bound analysis')
    a=p.parse_args()
    collect(a.workspace,a.serial,a.capture_manifest,a.out,include_cdd=a.include_cdd)
