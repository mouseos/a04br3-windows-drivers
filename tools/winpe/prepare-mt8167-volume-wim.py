#!/usr/bin/env python3
"""Add a fixed UART volume-release listener to the shared touch WinPE copy."""
import argparse, hashlib, json, os, shutil, struct, subprocess, tempfile, zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent; ROOT=HERE.parents[3]
WIM='50ec117d5097bdf44ccc0049431ed50c089156f3593788b30bf557e19a7f3466'
ESP='7181c8d92b45d30366ff012a799e7a5042ee4f763a4a71f0926098a6ec2981f0'
BCD='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
PKG='2dfc72705cab4388f751c8aa1fa91892492b1ae93c119f18483b691ee254097f'
BOOT='2cc9dfbff72b37bbb9af530c92575dce9971346de7b2a7fbe1af504280eec049'
PREVIOUS_CERT='B80231026FFEFE30224048CA83EE7631EFF3FABC'
FILES={'guard-windows-volume.exe','mt8167-toprgu.cat','mt8167-toprgu.cer','mt8167-toprgu.inf','mt8167-toprgu.sys','mt8167-volume-listener.exe','release-toprgu-audit-volume.cmd'}
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(cmd,**kw):return subprocess.run(cmd,check=True,**kw)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source-wim','source-esp','package','boot','output'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    if not out.is_relative_to(ROOT) or not out.is_dir() or any(out.iterdir()):raise SystemExit('output must be an empty workspace directory')
    for path,want in ((a.source_wim,WIM),(a.source_esp,ESP),(a.package,PKG),(a.boot,BOOT)):
        if not path.resolve().is_relative_to(ROOT) or sha(path)!=want:raise SystemExit('input provenance mismatch: '+str(path))
    repo=HERE.parents[1];paths=[HERE/'prepare-mt8167-volume-wim.py',HERE/'startnet-mt8167-volume.cmd',HERE/'replace-esp-wim.py']
    run(['git','-C',str(repo),'diff','--quiet','HEAD','--',*(str(x) for x in paths)])
    commit=run(['git','-C',str(repo),'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
    env=os.environ.copy();env.update(TMPDIR=str(out),PYTHONDONTWRITEBYTECODE='1')
    with tempfile.TemporaryDirectory(prefix='volume-',dir=out) as d:
        tmp=Path(d);src=tmp/'package';src.mkdir()
        with zipfile.ZipFile(a.package) as z:
            entries={i.filename.replace('\\','/'):i for i in z.infolist() if not i.is_dir()}
            if set(entries)!=FILES:raise ValueError('unexpected package members')
            for n,info in entries.items():(src/n).write_bytes(z.read(info))
        cert=(src/'mt8167-toprgu.cer').read_bytes();run(['openssl','x509','-inform','DER','-in',str(src/'mt8167-toprgu.cer'),'-noout'])
        thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
        run(['wimlib-imagex','extract',str(a.source_wim.resolve()),'1','/Windows/System32/config/SOFTWARE','/Windows/System32/config/SYSTEM',f'--dest-dir={tmp}'],env=env)
        hive=tmp/'SOFTWARE';system=tmp/'SYSTEM';system_hash=sha(system);base='\\Microsoft\\SystemCertificates';stores=[];old_cert={}
        for store in ('Root','TrustedPublisher'):
            path=base+'\\'+store+'\\Certificates';stores.append(path)
            old_cert[store]=run(['hivexget',str(hive),path+'\\'+PREVIOUS_CERT,'Blob'],capture_output=True).stdout
            if not old_cert[store]:raise ValueError('shared touch signer missing')
            listing=run(['hivexsh',str(hive)],input=f'cd {path}\nls\n',text=True,capture_output=True)
            if listing.stderr or thumb in listing.stdout:raise ValueError('certificate store unavailable or already present')
            cmds=f'cd {path}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n'
            result=run(['hivexsh','-w',str(hive)],input=cmds,text=True,capture_output=True)
            if result.stderr:raise ValueError('certificate store update failed')
        start=tmp/'startnet.cmd';start.write_bytes((HERE/'startnet-mt8167-volume.cmd').read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
        wim=out/'volume.wim';shutil.copyfile(a.source_wim,wim)
        dest={n:'/a04br3/'+n for n in FILES if n not in ('mt8167-toprgu.sys','mt8167-toprgu.cer')};dest['mt8167-toprgu.sys']='/Windows/System32/Drivers/mt8167-toprgu.sys'
        cmds=[f'add {hive} /Windows/System32/config/SOFTWARE',f'add {start} /Windows/System32/startnet.cmd']+[f'add {src/n} {path}' for n,path in sorted(dest.items())]
        if any(' ' in str(x.resolve()) for x in (tmp,a.source_wim,out)):raise ValueError('WIM paths contain spaces')
        run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join(cmds)+'\n',text=True,env=env);run(['wimlib-imagex','verify',str(wim)],env=env)
        for source,target in [(hive,'/Windows/System32/config/SOFTWARE'),(system,'/Windows/System32/config/SYSTEM'),(start,'/Windows/System32/startnet.cmd')]+[(src/n,path) for n,path in dest.items()]:
            check=tmp/('check-'+hashlib.sha256(target.encode()).hexdigest()[:12]);check.mkdir()
            run(['wimlib-imagex','extract',str(wim),'1',target,f'--dest-dir={check}'],env=env)
            files=[x for x in check.rglob('*') if x.is_file()]
            if len(files)!=1 or sha(files[0])!=sha(source):raise ValueError('WIM content mismatch: '+target)
        if sha(system)!=system_hash:raise ValueError('SYSTEM unexpectedly changed')
        for store in stores:
            if run(['hivexget',str(hive),store+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('new signer missing')
            key=store.rsplit('\\',2)[-2]
            if run(['hivexget',str(hive),store+'\\'+PREVIOUS_CERT,'Blob'],capture_output=True).stdout!=old_cert[key]:raise ValueError('shared touch signer changed')
        start_mode=run(['hivexget',str(system),r'\ControlSet001\services\Mt8167Toprgu','Start'],capture_output=True).stdout.strip()
        if start_mode!=b'0':raise ValueError('TOPRGU is not boot-start')
        esp=out/'volume-esp.img';run(['python3',str(HERE/'replace-esp-wim.py'),'--template',str(a.source_esp.resolve()),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM],env=env)
        boot=out/'boot.img';shutil.copyfile(a.boot,boot)
        meta={'tool_commit':commit,'driver_commit':'0190f2d','package_sha256':PKG,'source_wim_sha256':WIM,'source_esp_sha256':ESP,'wim_sha256':sha(wim),'esp_sha256':sha(esp),'boot_sha256':sha(boot),'system_hive_sha256':system_hash,'certificate_sha1':thumb,'limits':['device boot, driver trust and UART volume release unverified','mountvol /N does not release an existing mount','listener requires four CPU ownership and waits for screenshot completion','failed or missing UART acknowledgement is not a safe reset signal']}
        (out/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps(meta,indent=2))
if __name__=='__main__':main()
