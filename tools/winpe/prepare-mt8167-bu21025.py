#!/usr/bin/env python3
"""Build an untested shared MT8167/BU21025 diagnostic WIM and ESP."""
import argparse, hashlib, json, os, shutil, struct, subprocess, tempfile, zipfile
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
WIM='1a585e63f8d5f0dbfc3e1e9d738874de5ff673881d1910a659d9bc949235a9df'
ESP='b325b60ee3fe8df8732f67e79e53776a30337a021fc823d7df353e2e5e4a0316'
BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
PKG='de4c2f607ec2d24b4b5443c0abb1f8234a7d640f92855b8f45b5023ef0386f80'
BOOT='2cc9dfbff72b37bbb9af530c92575dce9971346de7b2a7fbe1af504280eec049'
FILES={'parent/mt8167-shared.sys','parent/shared-parent.cat','parent/shared-parent.inf',
       'touch/bu21025.cat','touch/bu21025.inf','touch/bu21025.sys','shared-touch-test.cer'}

def sha(p):
    with p.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def run(cmd,**kw): return subprocess.run(cmd,check=True,**kw)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source-wim','source-esp','package','boot','output'): p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    if not out.is_relative_to(ROOT) or not out.is_dir() or any(out.iterdir()): raise SystemExit('output must be an empty directory inside workspace')
    for path,want in ((a.source_wim,WIM),(a.source_esp,ESP),(a.package,PKG),(a.boot,BOOT)):
        if not path.resolve().is_relative_to(ROOT) or sha(path)!=want: raise SystemExit('input provenance mismatch: '+str(path))
    repo=HERE.parents[1]; run(['git','-C',str(repo),'diff','--quiet','HEAD','--',str(HERE/'prepare-mt8167-bu21025.py'),str(HERE/'startnet-mt8167-bu21025.cmd'),str(HERE/'replace-esp-wim.py')])
    commit=run(['git','-C',str(repo),'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
    env=os.environ.copy();env.update(TMPDIR=str(out),PYTHONDONTWRITEBYTECODE='1')
    with tempfile.TemporaryDirectory(prefix='shared-',dir=out) as name:
        tmp=Path(name); src=tmp/'package';src.mkdir()
        with zipfile.ZipFile(a.package) as z:
            entries={i.filename.replace('\\','/'):i for i in z.infolist() if not i.is_dir()}
            if set(entries)!=FILES: raise SystemExit('unexpected package members')
            for n,info in entries.items():
                target=src/n;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(z.read(info))
        cert=(src/'shared-touch-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper()
        run(['openssl','x509','-inform','DER','-in',str(src/'shared-touch-test.cer'),'-noout'])
        blob=struct.pack('<III',32,1,len(cert))+cert
        run(['wimlib-imagex','extract',str(a.source_wim.resolve()),'1','/Windows/System32/config/SOFTWARE',f'--dest-dir={tmp}'],env=env)
        hive=tmp/'SOFTWARE';base='\\Microsoft\\SystemCertificates';stores=[]
        def ensure(parent,child):
            listing=run(['hivexsh',str(hive)],input=f'cd {parent}\nls\n',text=True,capture_output=True)
            if listing.stderr:raise ValueError('certificate parent missing')
            if child.casefold() not in {x.strip().casefold() for x in listing.stdout.splitlines()}:
                result=run(['hivexsh','-w',str(hive)],input=f'cd {parent}\nadd {child}\ncommit\n',text=True,capture_output=True)
                if result.stderr:raise ValueError('certificate parent creation failed')
        for store in ('Root','TrustedPublisher'):
            ensure(base,store);ensure(base+'\\'+store,'Certificates');path=base+'\\'+store+'\\Certificates';stores.append(path)
            listing=run(['hivexsh',str(hive)],input=f'cd {path}\nls\n',text=True,capture_output=True)
            if listing.stderr or thumb in listing.stdout:raise ValueError('certificate store missing or certificate already exists')
            cmd=f'cd {path}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n'
            result=run(['hivexsh','-w',str(hive)],input=cmd,text=True,capture_output=True)
            if result.stderr or run(['hivexget',str(hive),path+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('certificate insertion failed')
        start=tmp/'startnet.cmd';start.write_bytes((HERE/'startnet-mt8167-bu21025.cmd').read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
        wim=out/'shared.wim';shutil.copyfile(a.source_wim,wim)
        commands=[f'add {hive} /Windows/System32/config/SOFTWARE',f'add {start} /Windows/System32/startnet.cmd']
        for n in sorted(FILES-{'shared-touch-test.cer'}):
            part,file=n.split('/');dest='mt8167-shared' if part=='parent' else 'bu21025-shared'
            commands.append(f'add {src/n} /a04br3/{dest}/{file}')
        if any(' ' in str(x.resolve()) for x in (tmp,a.source_wim,out)):raise ValueError('WIM update paths contain spaces')
        run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(commands)+'\n',text=True,env=env);run(['wimlib-imagex','verify',str(wim)],env=env)
        check=tmp/'check';run(['wimlib-imagex','extract',str(wim),'1','/Windows/System32/config/SOFTWARE','/Windows/System32/startnet.cmd','/a04br3/mt8167-shared','/a04br3/bu21025-shared',f'--dest-dir={check}'],env=env)
        if sha(check/'startnet.cmd')!=sha(start):raise ValueError('startup mismatch')
        for n in FILES-{'shared-touch-test.cer'}:
            part,file=n.split('/');dest='mt8167-shared' if part=='parent' else 'bu21025-shared'
            if sha(check/dest/file)!=sha(src/n):raise ValueError('package member mismatch '+n)
        for store in stores:
            if run(['hivexget',str(check/'SOFTWARE'),store+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('WIM certificate mismatch')
        esp=out/'shared-esp.img'
        run(['python3',str(HERE/'replace-esp-wim.py'),'--template',str(a.source_esp.resolve()),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
        boot=out/'boot.img';shutil.copyfile(a.boot,boot)
        result={'tool_commit':commit,'driver_commit':'81b550e','package_sha256':PKG,'source_wim_sha256':WIM,'source_esp_sha256':ESP,'wim_sha256':sha(wim),'esp_sha256':sha(esp),'boot_sha256':sha(boot),'certificate_sha1':thumb,'limits':['unverified on device','self-signed test drivers; acceptance unknown','mountvol /N does not establish existing Windows volume dismount','shared parent touch, audio and USB ownership unverified','UART screen does not prove physical touch operation']}
        (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__': main()
