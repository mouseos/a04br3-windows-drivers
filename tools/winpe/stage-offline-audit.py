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
    p.add_argument('--scope-package', type=Path, required=True)
    p.add_argument('--wimlib', type=Path, required=True)
    p.add_argument('--osslsigncode', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    wim, esp, info, tool, verifier = map(builder.regular, (a.wim,a.esp,a.source_manifest,a.wimlib,a.osslsigncode))
    package = a.package.resolve(strict=True); scope = a.scope_package.resolve(strict=True); out = a.output.resolve()
    if WORKSPACE not in package.parents or not package.is_dir() or WORKSPACE not in scope.parents or not scope.is_dir() or WORKSPACE not in out.parents or out.exists():
        raise ValueError('package/output paths rejected')
    src = json.loads(info.read_text()); pkg = json.loads((package/'manifest.json').read_text(encoding='utf-8-sig'))
    if sha(wim)!=src['wim_sha256'] or sha(esp)!=src['esp_sha256']:
        raise ValueError('source manifest mismatch')
    names=['audit-volume-sys.exe','release-offline-volume.exe','uart-audit-stream.exe','audit-pnp-inf.exe','audit-copy-hives.exe','map-offline-windows.exe','observe-msdc-storage.exe','audit-test.cer']
    files = {n: builder.regular(package/n) for n in names}
    for n,f in files.items():
        if sha(f)!=pkg['files'][n].lower():raise ValueError('package hash mismatch')
    scope_manifest=json.loads((scope/'manifest.json').read_text(encoding='utf-8-sig'))
    scope_files={n:builder.regular(scope/n) for n in ('audit-pnp-inf.exe','audit-scope-test.cer')}
    for n,f in scope_files.items():
        if sha(f)!=scope_manifest['files'][n].lower():raise ValueError('scope package hash mismatch')
    scope_cert=scope_files['audit-scope-test.cer'].read_bytes();scope_thumb=hashlib.sha1(scope_cert).hexdigest().upper()
    if scope_thumb!=scope_manifest['thumbprint'].upper():raise ValueError('scope certificate thumbprint mismatch')
    files['audit-pnp-inf.exe']=scope_files['audit-pnp-inf.exe']
    files['audit-scope-test.cer']=scope_files['audit-scope-test.cer'];names.append('audit-scope-test.cer')
    cert = files['audit-test.cer'].read_bytes(); thumb = hashlib.sha1(cert).hexdigest().upper()
    if thumb != pkg['thumbprint'].upper():raise ValueError('certificate thumbprint mismatch')
    out.mkdir(parents=True); scratch=out/'scratch';scratch.mkdir()
    pem=scratch/'test.pem';pem.write_text(ssl.DER_cert_to_PEM_cert(cert))
    scope_pem=scratch/'scope.pem';scope_pem.write_text(ssl.DER_cert_to_PEM_cert(scope_cert))
    for n in (x for x in names if x.endswith('.exe')):
        data=files[n].read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]
        if data[:2]!=b'MZ' or data[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',data,pe+4)[0]!=0x1c4:
            raise ValueError('tool is not ARMNT')
        subprocess.run([str(verifier),'verify','-CAfile',str(scope_pem if n=='audit-pnp-inf.exe' else pem),'-in',str(files[n])],check=True,stdout=subprocess.DEVNULL)
    # The historical INF filename is misleading: pin the actual boot driver
    # package byte-for-byte and never substitute the non-operational probe.
    storage_pins={'a04-msdc-probe.inf':'9bbf73d73c6dcad9f392e294a2cee7ca23209f80e4d36801e6b6a5781acb7192',
                  'mt8167-msdc.sys':'28115a3ba588d3354e30faca4b37a7de28dcc0b288c7354e2dd3b1ddf3ef416f',
                  'mt8167-msdc.cat':'e88b0514f64cdca47f5b12d6351df51518fbbf47c6253ff469609e96ef3f021c'}
    storage_dir=scratch/'storage-input';storage_dir.mkdir()
    for name,digest in storage_pins.items():
        subprocess.run([str(tool),'extract',str(wim),'1','/a04br3/'+name,f'--dest-dir={storage_dir}'],check=True,stdout=subprocess.DEVNULL)
        if sha(storage_dir/name)!=digest:raise ValueError('MSDC boot package mismatch: '+name)
    msdc_sig=scratch/'msdc-signature.der';msdc_pem=scratch/'msdc-signer.pem';msdc_der=scratch/'msdc-signer.cer'
    subprocess.run([str(verifier),'extract-signature','-in',str(storage_dir/'mt8167-msdc.sys'),'-out',str(msdc_sig)],check=True,stdout=subprocess.DEVNULL)
    subprocess.run(['openssl','pkcs7','-inform','DER','-in',str(msdc_sig),'-print_certs','-out',str(msdc_pem)],check=True)
    subprocess.run(['openssl','x509','-in',str(msdc_pem),'-outform','DER','-out',str(msdc_der)],check=True)
    if sha(msdc_der)!='7120ebac5300d7d6d68b5d132f9b4d648ec7d14f475d655f07fccde207eba901':raise ValueError('MSDC signer mismatch')
    for name in ('mt8167-msdc.sys','mt8167-msdc.cat'):
        subprocess.run([str(verifier),'verify','-CAfile',str(msdc_pem),'-in',str(storage_dir/name)],check=True,stdout=subprocess.DEVNULL)
    before = builder.inventory(tool,wim)
    dests={'/a04br3/'+n:files[n] for n in names}
    dests['/a04br3/msdc-development.cer']=msdc_der
    dests['/a04br3/run-audit-once.cmd']=ROOT/'tools/winpe/run-audit-once.cmd'
    startup_dir=scratch/'original-startup';startup_dir.mkdir()
    subprocess.run([str(tool),'extract',str(wim),'1','/Windows/System32/startnet.cmd',f'--dest-dir={startup_dir}'],check=True,stdout=subprocess.DEVNULL)
    startup_original=startup_dir/'startnet.cmd';startup=scratch/'startnet.cmd'
    original_bytes=startup_original.read_bytes()
    if b'run-audit-once.cmd' in original_bytes:raise ValueError('audit already present')
    cpu=b'echo A04CPU_LOAD_LAUNCH\r\nstart "" /b X:\\a04br3\\cpu\\fourcpu-load.exe\r\n'
    marker=b'echo A04USB_OBSERVATION_EXIT=%errorlevel%\r\n'
    observer=b'echo A04CPU_OBSERVE_LAUNCH\r\nstart "" /b X:\\a04br3\\cpu\\cpu-observe.exe\r\n'
    if original_bytes.count(cpu)!=1 or original_bytes.count(marker)!=1 or original_bytes.count(observer)!=1:raise ValueError('unexpected CPU/USB startup layout')
    original_bytes=original_bytes.replace(cpu,b'').replace(observer,b'')
    original_bytes=original_bytes.replace(marker,marker+b'echo A04CPU_OBSERVE_LAUNCH\r\nX:\\a04br3\\cpu\\cpu-observe.exe\r\necho A04CPU_OBSERVE_EXIT=%errorlevel%\r\necho A04CPU_LOAD_LAUNCH\r\nX:\\a04br3\\cpu\\fourcpu-load.exe\r\necho A04CPU_LOAD_EXIT=%errorlevel%\r\n')
    startup.write_bytes(original_bytes+b'\r\ncall X:\\a04br3\\run-audit-once.cmd\r\n')
    dests['/Windows/System32/startnet.cmd']=startup
    if any(n in before for n in dests if n != '/Windows/System32/startnet.cmd'):
        raise ValueError('tool destination already exists')
    subprocess.run([str(tool),'extract',str(wim),'1','/Windows/System32/config/SOFTWARE',f'--dest-dir={scratch}'],check=True,stdout=subprocess.DEVNULL)
    original=scratch/'SOFTWARE';updated=scratch/'SOFTWARE.updated';shutil.copyfile(original,updated)
    old=RegistryHive(str(original)); old_stores={s:store_snapshot(old,s) for s in ('Root','TrustedPublisher')}
    certs={thumb:cert,scope_thumb:scope_cert,hashlib.sha1(msdc_der.read_bytes()).hexdigest().upper():msdc_der.read_bytes()}
    if len(certs)!=3:raise ValueError('certificate collision')
    commands=''
    blobs={}
    for t,der in certs.items():blobs[t]=struct.pack('<III',3,1,20)+hashlib.sha1(der).digest()+struct.pack('<III',32,1,len(der))+der
    for store in ('Root','TrustedPublisher'):
        for t,blob in blobs.items():
            existing=old_stores[store].get('\\'+t)
            if existing is not None:
                if len(existing)!=1 or existing[0][0]!='Blob' or existing[0][2]!=repr(blob.hex()):raise ValueError('existing certificate mismatch')
                continue
            commands+=f'cd \\Microsoft\\SystemCertificates\\{store}\\Certificates\nadd {t}\ncd {t}\nsetval 1\nBlob\nhex:3:{blob.hex()}\n'
    commands+='commit\nquit\n'
    subprocess.run(['hivexsh','-w',str(updated)],input=commands,text=True,check=True,capture_output=True)
    changed=RegistryHive(str(updated))
    for store in ('Root','TrustedPublisher'):
        current=store_snapshot(changed,store)
        for t,blob in blobs.items():
            entry=current.get('\\'+t) if '\\'+t in old_stores[store] else current.pop('\\'+t,None)
            if entry is None or len(entry)!=1 or entry[0][0]!='Blob' or entry[0][2]!=repr(blob.hex()):raise ValueError('certificate entry mismatch: '+store)
        if current!=old_stores[store]:raise ValueError('existing certificate store changed: '+store)
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
                    '--verified-winpe-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--wim',str(output_wim),
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
    result={'source_wim_sha256':sha(wim),'source_esp_sha256':sha(esp),'source_package_manifest_sha256':sha(package/'manifest.json'),'scope_package_manifest_sha256':sha(scope/'manifest.json'),'scope_certificate_thumbprint':scope_thumb,
            'storage_pins':storage_pins,'storage_cert_sha256':sha(msdc_der),'storage_cert_thumbprint':hashlib.sha1(msdc_der.read_bytes()).hexdigest().upper(),'output_wim_sha256':sha(output_wim),'output_esp_sha256':sha(output_esp),'test_certificate_sha256':sha(files['audit-test.cer']),
            'test_certificate_thumbprint':thumb,'existing_certificate_stores_preserved':True,'fat_free_bytes':free_bytes,
            'wimlib_sha256':sha(tool),'osslsigncode_sha256':sha(verifier),
            'status':'read-only observational audit candidate; non-atomic; target boot, signature, C mount, report and dismount unverified'}
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
