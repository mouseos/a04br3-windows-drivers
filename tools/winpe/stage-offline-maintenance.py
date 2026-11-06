#!/usr/bin/env python3
"""Stage test-signed tools and their public certificate into a WIM copy only."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import secrets
import shutil
import ssl
import struct
import subprocess

from regipy.registry import RegistryHive
from pyfatfs.PyFatFS import PyFatFS

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT.parents[1]
spec = importlib.util.spec_from_file_location('maintenance_builder', ROOT / 'tools/winpe/build-maintenance-esp.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
sha = builder.sha

def store_snapshot(hive, store):
    root = hive.get_key('\\Microsoft\\SystemCertificates\\' + store + '\\Certificates')
    out = {}
    def visit(key, path):
        vals = []
        for v in key.iter_values(max_len=8 * 1024 * 1024):
            vals.append((v.name, v.value_type, repr(v.value)))
        out[path] = tuple(sorted(vals, key=lambda x: str(x[0])))
        for sub in key.iter_subkeys():
            visit(sub, path + '\\' + sub.name)
    visit(root, '')
    return out

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--wim', type=Path, required=True)
    p.add_argument('--esp', type=Path, required=True)
    p.add_argument('--source-manifest', type=Path, required=True)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--wimlib', type=Path, required=True)
    p.add_argument('--osslsigncode', type=Path, required=True)
    p.add_argument('--driver-trust', type=Path, help='verified exact-driver trust payload')
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    wim, esp, info, tool, verifier = map(builder.regular, (a.wim,a.esp,a.source_manifest,a.wimlib,a.osslsigncode))
    package = a.package.resolve(strict=True); out = a.output.resolve()
    if WORKSPACE not in package.parents or not package.is_dir() or WORKSPACE not in out.parents or out.exists():
        raise ValueError('package/output paths rejected')
    src = json.loads(info.read_text()); pkg = json.loads((package/'manifest.json').read_text(encoding='utf-8-sig'))
    if sha(wim)!=src['wim_sha256'] or sha(esp)!=src['esp_sha256']:
        raise ValueError('source manifest mismatch')
    names=['offline-backup.exe','release-offline-volume.exe','maintenance-test.cer']
    if a.driver_trust:names.append('offline-hive-apply.exe')
    files = {n: builder.regular(package/n) for n in names}
    for n,f in files.items():
        if sha(f)!=pkg['files'][n].lower():raise ValueError('package hash mismatch')
    cert = files['maintenance-test.cer'].read_bytes(); thumb = hashlib.sha1(cert).hexdigest().upper()
    if thumb != pkg['thumbprint'].upper():raise ValueError('certificate thumbprint mismatch')
    out.mkdir(parents=True); scratch=out/'scratch';scratch.mkdir()
    pem=scratch/'test.pem';pem.write_text(ssl.DER_cert_to_PEM_cert(cert))
    for n in (x for x in names if x.endswith('.exe')):
        data=files[n].read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]
        if data[:2]!=b'MZ' or data[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',data,pe+4)[0]!=0x1c4:
            raise ValueError('tool is not ARMNT')
        subprocess.run([str(verifier),'verify','-CAfile',str(pem),'-in',str(files[n])],check=True,stdout=subprocess.DEVNULL)
    before = builder.inventory(tool,wim)
    dests={'/a04br3/offline-backup.exe':files['offline-backup.exe'],
           '/a04br3/release-offline-volume.exe':files['release-offline-volume.exe'],
           '/a04br3/restore-offline-backup.cmd':ROOT/'tools/winpe/restore-offline-backup.cmd',
           '/a04br3/maintenance-test.cer':files['maintenance-test.cer'],
           '/Windows/System32/startnet.cmd':ROOT/'tools/winpe/startnet-offline-maintenance.cmd'}
    if a.driver_trust:
        trust=a.driver_trust.resolve(strict=True)
        if WORKSPACE not in trust.parents or not trust.is_dir():raise ValueError('driver trust path rejected')
        trust_manifest=json.loads(builder.regular(trust/'manifest.json').read_text())
        trust_names=('toprgu.cer','shared.cer','toprgu-cert.sha256','shared-cert.sha256','toprgu-sys.sha256','shared-sys.sha256','touch-sys.sha256')
        for name in trust_names:
            item=builder.regular(trust/name)
            if sha(item)!=trust_manifest.get('files',{}).get(name):raise ValueError('driver trust payload hash mismatch')
            dests['/a04br3/driver-trust/'+name]=item
        dests['/a04br3/offline-hive-apply.exe']=files['offline-hive-apply.exe']
        dests['/a04br3/apply-offline-hives.cmd']=ROOT/'tools/winpe/apply-offline-hives.cmd'

    if any(n in before for n in dests if n != '/Windows/System32/startnet.cmd'):
        raise ValueError('tool destination already exists')
    subprocess.run([str(tool),'extract',str(wim),'1','/Windows/System32/config/SOFTWARE',f'--dest-dir={scratch}'],check=True,stdout=subprocess.DEVNULL)
    original=scratch/'SOFTWARE';updated=scratch/'SOFTWARE.updated';shutil.copyfile(original,updated)
    old=RegistryHive(str(original)); old_stores={s:store_snapshot(old,s) for s in ('Root','TrustedPublisher')}
    if any('\\'+thumb in x for x in old_stores.values()):raise ValueError('certificate already present')
    blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
    commands=''
    for store in ('Root','TrustedPublisher'):
        commands+=f'cd \\Microsoft\\SystemCertificates\\{store}\\Certificates\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\n'
    commands+='commit\nquit\n'
    subprocess.run(['hivexsh','-w',str(updated)],input=commands,text=True,check=True,capture_output=True)
    changed=RegistryHive(str(updated))
    for store in ('Root','TrustedPublisher'):
        current=store_snapshot(changed,store); entry=current.pop('\\'+thumb,None)
        if entry is None or current!=old_stores[store] or len(entry)!=1 or entry[0][0]!='Blob' or entry[0][2]!=repr(blob.hex()):
            raise ValueError('certificate store preservation failed: '+store)
    dests['/Windows/System32/config/SOFTWARE']=updated
    output_wim=out/'maintenance-staged.wim';shutil.copyfile(wim,output_wim)
    update=''.join(f'add "{source}" "{dest}"\n' for dest,source in dests.items())
    env=dict(os.environ,TMPDIR=str(scratch))
    subprocess.run([str(tool),'update',str(output_wim),'1','--check','--rebuild'],input=update,text=True,env=env,check=True)
    subprocess.run([str(tool),'verify',str(output_wim)],env=env,check=True)
    after=builder.inventory(tool,output_wim)
    untouched={k:v for k,v in before.items() if k not in dests}
    if any(after.get(k)!=v for k,v in untouched.items()):raise ValueError('unrelated WIM record changed')
    for name,source in dests.items():
        folder=scratch/('check'+str(len(list(scratch.glob('check*')))));folder.mkdir()
        subprocess.run([str(tool),'extract',str(output_wim),'1',name,f'--dest-dir={folder}'],check=True,stdout=subprocess.DEVNULL)
        found=[x for x in folder.rglob('*') if x.is_file()]
        if len(found)!=1 or sha(found[0])!=sha(source):raise ValueError('staged WIM file mismatch: '+name)
    stage=out/'stage.json';stage.write_text(json.dumps({'version':1,'token':secrets.token_hex(16),'wim_sha256':sha(output_wim)})+'\n')
    output_esp=out/'maintenance-staged-esp.img'
    subprocess.run(['python3',str(ROOT/'tools/winpe/prepare-mma8652-esp.py'), '--source-esp',str(esp),'--source-sha256',sha(esp),
                    '--verified-winpe-bcd-sha256',src['bcd_sha256'],'--wim',str(output_wim),
                    '--stage-manifest',str(stage),'--output',str(output_esp)],env=env,check=True)
    with PyFatFS(str(esp),read_only=True,preserve_case=True) as f:old_fat=builder.fat_files(f)
    with PyFatFS(str(output_esp),read_only=True,preserve_case=True) as f:new_fat=builder.fat_files(f)
    for name,(_,digest) in old_fat.items():
        want=sha(output_wim) if name=='/sources/boot.wim' else digest
        if name not in new_fat or new_fat[name][1]!=want:raise ValueError('ESP retained file mismatch: '+name)
    if set(new_fat)!=set(old_fat):raise ValueError('unexpected ESP files')
    fsck=subprocess.run(['fsck.vfat','-vn',str(output_esp)],capture_output=True,text=True,check=True)
    import re
    free_match=re.search(r'(\d+)/(\d+) clusters',fsck.stdout)
    if not free_match:raise ValueError('ESP free-space measurement failed')
    with output_esp.open('rb') as disk: boot=disk.read(512)
    cluster_size=struct.unpack_from('<H',boot,11)[0]*boot[13]
    free_bytes=(int(free_match.group(2))-int(free_match.group(1)))*cluster_size
    result={'source_wim_sha256':sha(wim),'source_esp_sha256':sha(esp),'source_package_manifest_sha256':sha(package/'manifest.json'),
            'output_wim_sha256':sha(output_wim),'output_esp_sha256':sha(output_esp),'test_certificate_sha256':sha(files['maintenance-test.cer']),
            'test_certificate_thumbprint':thumb,'driver_trust_manifest_sha256':sha(trust/'manifest.json') if a.driver_trust else None,'existing_certificate_stores_preserved':True,'fat_free_bytes':free_bytes,
            'wimlib_sha256':sha(tool),'osslsigncode_sha256':sha(verifier),
            'status':'WinPE test trust only; boot, RT policy, OS hive update and recovery unverified'}
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
