#!/usr/bin/env python3
"""AOSP v0/既存MTK packagerからRecovery FW imageをhostで再構成する。実機操作なし。"""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
import stat
import struct
import subprocess
import sys
from pathlib import Path, PurePosixPath

BOOT=struct.Struct("<8s10I16s512s32s1024s")
SECTION=struct.Struct("<II32s472s")
FIELDS=("magic","kernel_size","kernel_addr","ramdisk_size","ramdisk_addr","second_size",
        "second_addr","tags_addr","page_size","header_version","os_version","name","cmdline","id","extra_cmdline")
def sha(data):return hashlib.sha256(data).hexdigest()
def align(n,page):return (n+page-1)//page*page
def unsection(data,name):
    magic,size,label,pad=SECTION.unpack_from(data)
    if magic!=0x58881688 or label.rstrip(b"\0")!=name or size!=len(data)-SECTION.size or pad!=b"\xff"*472:
        raise ValueError("MTK section header/source profile mismatch: "+name.decode())
    return data[SECTION.size:]
def unpack(data):
    h=dict(zip(FIELDS,BOOT.unpack_from(data)))
    if h['magic']!=b'ANDROID!' or h['header_version']!=0 or h['second_size']!=0:
        raise ValueError('Only the source-established AOSP v0/no-second profile is supported')
    page=h['page_size']
    if page not in (2048,4096,8192,16384) or BOOT.size>page:raise ValueError('page/header mismatch')
    roff=page+align(h['kernel_size'],page);end=roff+align(h['ramdisk_size'],page)
    if end!=len(data):raise ValueError('Unknown footer/truncation: refuse to discard it')
    kernel=data[page:page+h['kernel_size']];ramdisk=data[roff:roff+h['ramdisk_size']]
    digest=hashlib.sha1()
    for part in (kernel,ramdisk,b''):digest.update(part);digest.update(struct.pack('<I',len(part)))
    if h['id']!=digest.digest().ljust(32,b'\0'):raise ValueError('parent image ID mismatch')
    return h,unsection(kernel,b'KERNEL'),unsection(ramdisk,b'RECOVERY')
def cpio_entries(data):
    pos=0;items=[];trailers=0
    while pos<len(data):
        if data[pos]==0:pos+=1;continue
        if pos%4 or data[pos:pos+6]!=b'070701':raise ValueError('invalid newc boundary')
        start=pos;v=[int(data[pos+6+i*8:pos+14+i*8],16) for i in range(13)];pos+=110
        if v[11]<1 or pos+v[11]>len(data):raise ValueError('newc name bounds')
        name_bytes=data[pos:pos+v[11]]
        if not name_bytes.endswith(b'\0'):raise ValueError('newc name termination')
        name=name_bytes[:-1].decode();pos=align(pos+v[11],4)
        if pos+v[6]>len(data):raise ValueError('newc content bounds')
        payload=data[pos:pos+v[6]];pos=align(pos+v[6],4)
        if name=='TRAILER!!!':trailers+=1;continue
        while name.startswith('./'):name=name[2:]
        if name not in ('','.'): 
            if name.startswith('/') or '..' in PurePosixPath(name).parts:raise ValueError('unsafe ramdisk name')
            items.append({'path':name,'mode':v[1],'uid':v[2],'gid':v[3],'mtime':v[5],
                'inode':v[0],'nlink':v[4],'rdev':[v[9],v[10]],'bytes':len(payload),
                'sha256':sha(payload),'data':payload,'archive_offset':start})
    if not trailers:raise ValueError('missing newc trailer')
    return items,trailers
def visible(h):
    return {k:(v.hex() if k=='id' else v.rstrip(b'\0').decode() if isinstance(v,bytes) else v) for k,v in h.items()}
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for key in ['work-root','parent-recovery','kernel-prebuilt','mkbootimg-source','mtk-wrapper-source','firmware','output']:
        ap.add_argument('--'+key,type=Path,required=True)
    ap.add_argument('--expected-parent-sha256',required=True)
    args=ap.parse_args();root=args.work_root.resolve(strict=True);out=args.output.resolve()
    paths={key:getattr(args,key).resolve(strict=True) for key in ['parent_recovery','kernel_prebuilt','mkbootimg_source','mtk_wrapper_source','firmware']}
    for p in [out,*paths.values()]:p.relative_to(root)
    parent=paths['parent_recovery'].read_bytes()
    if sha(parent)!=args.expected_parent_sha256:raise ValueError('parent SHA256 mismatch')
    header,kernel,ramdisk=unpack(parent)
    if kernel!=paths['kernel_prebuilt'].read_bytes():raise ValueError('kernel+DTB differs from recorded canonical prebuilt')
    raw=gzip.decompress(ramdisk);before,archives=cpio_entries(raw)
    lookup={v['path']:v for v in before}
    fwtool=Path(__file__).with_name('prepare-recovery-native-firmware.py')
    spec=importlib.util.spec_from_file_location('fixed_fw_input',fwtool);fw=importlib.util.module_from_spec(spec);spec.loader.exec_module(fw)
    payload=paths['firmware'].read_bytes()
    if sha(payload)!=fw.FW_SHA256:raise ValueError('fixed firmware SHA256 mismatch')
    target='lib/firmware/'+fw.FW_NAME
    if target in lookup:raise ValueError('parent already has target firmware: review instead of overwriting')
    added=[]
    for name in ['lib','lib/firmware']:
        if name in lookup:
            if not stat.S_ISDIR(lookup[name]['mode']):raise ValueError('parent directory is a non-directory: '+name)
        else:added.append((name,stat.S_IFDIR|0o755,b''))
    added.append((target,stat.S_IFREG|0o644,payload))
    overlay=fw.newc(added)
    if len(raw)%4:raise ValueError('parent cpio end is not aligned')
    changed=raw+overlay;after,new_archives=cpio_entries(changed)
    if after[:len(before)]!=before:raise ValueError('original cpio members changed')
    if [v['path'] for v in after[len(before):]]!=[v[0] for v in added]:raise ValueError('overlay entry mismatch')
    out.mkdir(parents=True,exist_ok=False);inputs=out/'inputs';inputs.mkdir();parts=out/'parts';parts.mkdir();tools=out/'tooling';tools.mkdir();(out/'tmp').mkdir()
    (inputs/'recovery-parent.img').write_bytes(parent);(inputs/fw.FW_NAME).write_bytes(payload)
    (parts/'kernel.Image.gz-dtb').write_bytes(kernel);(parts/'ramdisk-original.gz').write_bytes(ramdisk)
    (parts/'ramdisk-original.cpio').write_bytes(raw);(parts/'firmware-overlay.cpio').write_bytes(overlay)
    (parts/'ramdisk-with-firmware.cpio').write_bytes(changed)
    new_gzip=gzip.compress(changed,compresslevel=9,mtime=0);(parts/'ramdisk-with-firmware.gz').write_bytes(new_gzip)
    for key,name in [('mkbootimg_source','mkbootimg.py'),('mtk_wrapper_source','mtk_wrap_bootimg.py')]:
        (tools/name).write_bytes(paths[key].read_bytes())
    env=os.environ.copy();env['TMPDIR']=str(out/'tmp');env['PYTHONDONTWRITEBYTECODE']='1';commands=[]
    def pack(ramdisk_path,prefix):
        cmd=[sys.executable,'-B',str(tools/'mkbootimg.py'),'--kernel',str(parts/'kernel.Image.gz-dtb'),
             '--ramdisk',str(ramdisk_path),'--header_version','0','--base','0',
             '--kernel_offset',hex(header['kernel_addr']),'--ramdisk_offset',hex(header['ramdisk_addr']),
             '--second_offset',hex(header['second_addr']),'--tags_offset',hex(header['tags_addr']),
             '--pagesize',str(header['page_size']),'--board',header['name'].rstrip(b'\0').decode(),
             '--cmdline',(header['cmdline']+header['extra_cmdline']).rstrip(b'\0').decode(),
             '--output',str(out/(prefix+'.aosp.img'))]
        osver=header['os_version'];a=(osver>>25)&127;b=(osver>>18)&127;c=(osver>>11)&127
        cmd+=['--os_version',f'{a}.{b}.{c}']
        if osver&2047:cmd+=['--os_patch_level',f'{2000+((osver>>4)&127):04d}-{osver&15:02d}-01']
        commands.append(cmd);subprocess.run(cmd,env=env,check=True)
        cmd=[sys.executable,'-B',str(tools/'mtk_wrap_bootimg.py'),'--recovery',str(out/(prefix+'.aosp.img')),str(out/(prefix+'.img'))]
        commands.append(cmd);subprocess.run(cmd,env=env,check=True)
        return (out/(prefix+'.img')).read_bytes()
    if pack(parts/'ramdisk-original.gz','roundtrip')!=parent:raise ValueError('no-op source pack roundtrip is not byte-identical')
    built=pack(parts/'ramdisk-with-firmware.gz','recovery')
    final_h,final_k,final_r=unpack(built)
    for key in header:
        if key not in ('ramdisk_size','id') and header[key]!=final_h[key]:raise ValueError('unintended header change '+key)
    if final_k!=kernel or gzip.decompress(final_r)!=changed:raise ValueError('output section mismatch')
    if len(built)>16777216:raise ValueError('exceeds BoardConfig recovery partition size')
    def entries_manifest(items):return [{k:v for k,v in p.items() if k!='data'} for p in items]
    manifest={'type':'complete Android v0 + MTK RECOVERY image, host build verified; device boot untested',
        'inputs':{key:{'path':str(p.relative_to(root)),'sha256':sha(p.read_bytes())} for key,p in paths.items()},
        'base_header':visible(header),'output_header':visible(final_h),'source_format':'AOSP bootimg.h v0 and device tools/mtk_wrap_bootimg.py',
        'kernel_and_appended_dtb_sha256':sha(kernel),'firmware_target':'/'+target,'firmware_sha256':sha(payload),
        'original_cpio_sha256':sha(raw),'original_cpio_unchanged_prefix':True,'original_cpio_entries':len(before),
        'cpio_archives_before':archives,'cpio_archives_after':new_archives,'overlay_entries':entries_manifest(after[len(before):]),
        'no_op_roundtrip_byte_identical':True,'header_changes_only':['ramdisk_size','id'],
        'output_bytes':len(built),'output_sha256':sha(built),'partition_limit':16777216,
        'commands':commands,'device_accessed':False,'flashed':False,'boot_verified':False,
        'requires':'FW available before Recovery/minui first DRM open; verify no PVR BAD state on next permitted boot'}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n')
    (out/'ramdisk-members.json').write_text(json.dumps(entries_manifest(after),indent=2)+'\n')
    print(json.dumps({'output':str(out/'recovery.img'),'bytes':len(built),'sha256':sha(built),'kernel_dtb_unchanged':True,'roundtrip_identical':True,'boot_verified':False},indent=2))
if __name__=='__main__':main()
