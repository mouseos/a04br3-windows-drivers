#!/usr/bin/env python3
"""Create a fixed-input, untested GE8300/BQ diagnostic WIM and ESP."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile

WIM_SHA='1a585e63f8d5f0dbfc3e1e9d738874de5ff673881d1910a659d9bc949235a9df'
ESP_SHA='b325b60ee3fe8df8732f67e79e53776a30337a021fc823d7df353e2e5e4a0316'
BCD_SHA='65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'
BOOT_SHA='7752660ba55bc205307fc9496fb125c737fd2eb4d4b50210307ea92857dfde44'
BQ_ZIP_SHA='e04d406bb3fed817db2cf1146ec5dde2ab53d7d06f621e675c1ade40d434a39b'
BQ_COMMIT='3d97d1e7ad2b4bc12e0e7b3aa9c06309a35a6f7b'
GPU_FILES={
'mt8167-ge8300.sys':'2ec88aa79736fdc77effb9cb0b3bb62d41f11ff0126f435c0c27ad2cae6043ef',
'mt8167-ge8300.inf':'73afb7f2e63469d577317b68ed505c01340a81b36373eda416883ce5ae4edc0b',
'mt8167-ge8300.cat':'8c71978e1587250423e978c021769f6f8aaca611307d517969020b40cfcbf6de',
'ge8300-mips.fw':'5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1',
'ge8300-test.cer':'47d6b7658b2349a76a59d53110cb056731f83fca2e7adfbf5778bf738ac433da',
'ge8300-status.exe':'02cb501f2e3aca0f03070a9c7f4802827b1d458e2564df1823b4415a1c7f45b7',
'ge8300-pnp-status.exe':'b562fd2458be699e34cc5701fb5e851a2d6d3b25e4593f65106b18e48d0b96cb',
'ge8300-vendor-stage-status.exe':'a2dce0be29710ed9f7f182d2fac3be9ed9955655a5f9867cb7c93e87851c8199',
'ge8300-vendor-start-status.exe':'36695e69f6deef1a11d1c6995d4b617fb9632551fbe80bc2e337ff73fb58ec67'}
NAMES=('lc709203f.inf','lc709203f.sys','lc709203f.cat','lc709203f-status.exe','lc709203f-pnp-status.exe','lc709203f-test.cer')
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3].resolve()

def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()

def run(args,**kw): return subprocess.run(args,check=True,**kw)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source-wim','source-esp','bq-package','boot','output'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--wimlib',default='wimlib-imagex')
    a=p.parse_args();out=a.output.resolve()
    if not out.is_relative_to(ROOT): raise SystemExit('output outside task root')
    repo=HERE.parents[1]
    source_paths=['tools/winpe/prepare-ge8300-bq-cold.py','tools/winpe/startnet-ge8300-bq-cold.cmd','tools/winpe/prepare-lc709203f-refresh.py','tools/winpe/replace-esp-wim.py']
    run(['git','-C',str(repo),'diff','--quiet','HEAD','--',*source_paths])
    tool_commit=run(['git','-C',str(repo),'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
    if not out.is_dir() or any((out/n).exists() for n in ('combined.wim','combined-esp.img','combined-manifest.json','boot-691f907.img')): raise SystemExit('output missing or occupied')
    for path,want in ((a.source_wim,WIM_SHA),(a.source_esp,ESP_SHA),(a.bq_package,BQ_ZIP_SHA),(a.boot,BOOT_SHA)):
        if sha(path)!=want: raise SystemExit('input hash mismatch: '+str(path))
    env=os.environ.copy();env['TMPDIR']=str(out);env['PYTHONDONTWRITEBYTECODE']='1'
    if Path(a.wimlib).is_absolute():env['PATH']=str(Path(a.wimlib).parent)+os.pathsep+env.get('PATH','')
    with tempfile.TemporaryDirectory(prefix='build-',dir=out) as temp:
        tmp=Path(temp)
        with zipfile.ZipFile(a.bq_package) as z:
            if set(z.namelist())!=set(NAMES)|{'manifest.json'}:raise SystemExit('BQ package members mismatch')
            m=json.loads(z.read('manifest.json').decode('utf-8-sig'))
            files={v['name']:v['sha256'].lower() for v in m['files']}
            cert=z.read('lc709203f-test.cer')
            if m['commit']!=BQ_COMMIT or set(files)!=set(NAMES):raise SystemExit('BQ provenance mismatch')
            for name in NAMES:
                if hashlib.sha256(z.read(name)).hexdigest()!=files[name]:raise SystemExit('BQ member mismatch: '+name)
            with zipfile.ZipFile(tmp/'battery.zip','w',compression=zipfile.ZIP_DEFLATED) as dest:
                for name in NAMES: dest.writestr(name,z.read(name))
        norm={'commit':BQ_COMMIT,'zip_sha256':sha(tmp/'battery.zip'),'files':files}
        (tmp/'manifest.json').write_text(json.dumps(norm))
        staged=tmp/'staged.wim'
        run(['python3',str(HERE/'prepare-lc709203f-refresh.py'),'--source',str(a.source_wim.resolve()),'--package',str(tmp/'battery.zip'),'--manifest',str(tmp/'manifest.json'),'--output',str(staged),'--source-sha',WIM_SHA],env=env)
        command=tmp/'startnet.cmd';command.write_bytes((HERE/'startnet-ge8300-bq-cold.cmd').read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
        final=out/'combined.wim';shutil.copyfile(staged,final)
        run([a.wimlib,'update',str(final),'1'],input=f'add {command} /Windows/System32/startnet.cmd\n',text=True,env=env)
        run([a.wimlib,'verify',str(final)],env=env)
        check=tmp/'check';run([a.wimlib,'extract',str(final),'1','/Windows/System32/startnet.cmd','/a04br3/lc709203f','/Windows/System32/config/SOFTWARE','/Windows/System32/config/SYSTEM','/Windows/System32/Drivers/mt8167-toprgu.sys','/a04br3/vendor-m1-11',f'--dest-dir={check}'],env=env)
        if sha(check/'startnet.cmd')!=sha(command):raise SystemExit('startup command mismatch')
        for name in NAMES:
            if name.endswith('.cer'):continue
            if sha(check/'lc709203f'/name)!=files[name]:raise SystemExit('WIM BQ mismatch '+name)
        for name,want in GPU_FILES.items():
            if sha(check/'vendor-m1-11'/name)!=want:raise SystemExit('WIM GPU mismatch '+name)
        # The selected 3-CPU UEFI keeps CPU1; retain the checked early TOPRGU
        # binary and SYSTEM hive rather than silently changing watchdog policy.
        if sha(check/'mt8167-toprgu.sys')!='52a1a1cb0cd66821099a87db9aca5bd82797061ab0e99a047d495d7fe06c2b3e':raise SystemExit('TOPRGU binary mismatch')
        base=tmp/'base';run([a.wimlib,'extract',str(a.source_wim),'1','/Windows/System32/config/SYSTEM',f'--dest-dir={base}'],env=env)
        if sha(base/'SYSTEM')!=sha(check/'SYSTEM'):raise SystemExit('SYSTEM hive changed')
        start=run(['hivexget',str(check/'SYSTEM'),r'\ControlSet001\services\Mt8167Toprgu','Start'],capture_output=True,env=env).stdout.strip()
        if start!=b'0':raise SystemExit('TOPRGU start value changed')
        thumb=hashlib.sha1(cert).hexdigest().upper();blob=struct.pack('<III',32,1,len(cert))+cert
        for store in ('Root','TrustedPublisher'):
            key=f'\\Microsoft\\SystemCertificates\\{store}\\Certificates\\{thumb}'
            if run(['hivexget',str(check/'SOFTWARE'),key,'Blob'],capture_output=True,env=env).stdout!=blob:
                raise SystemExit('BQ certificate mismatch in '+store)
        run(['python3',str(HERE/'replace-esp-wim.py'),'--template',str(a.source_esp.resolve()),'--wim',str(final),'--output',str(out/'combined-esp.img'),'--template-bcd-sha256',BCD_SHA,'--template-wim-sha256',WIM_SHA],env=env)
        shutil.copyfile(a.boot,out/'boot-691f907.img')
        if sha(out/'boot-691f907.img')!=BOOT_SHA:raise SystemExit('boot copy mismatch')
        result={'tool_repository':'https://github.com/mouseos/a04br3-windows-drivers','tool_commit':tool_commit,'source_wim_sha256':WIM_SHA,'source_esp_sha256':ESP_SHA,'bq_zip_sha256':BQ_ZIP_SHA,
                'bq_commit':BQ_COMMIT,'gpu_commit':'6fabf08','uefi_boot_sha256':BOOT_SHA,
                'uefi_commit':'691f907c6835bcc9992479c29e6aed0eba2f116c',
                'uefi_configuration':'3 CPU; CPU1 firmware watchdog/UART worker; BQHR and GPU ACPI',
                'combined_wim_sha256':sha(final),'combined_esp_sha256':sha(out/'combined-esp.img'),
                'startnet_sha256':sha(command),'bq_files_sha256':files,'gpu_files_sha256':GPU_FILES,
                'toprgu_sys_sha256':sha(check/'mt8167-toprgu.sys'),'system_hive_sha256':sha(check/'SYSTEM'),
                'bq_certificate_sha1':thumb,
                'warning':'Device-untested experimental diagnostic. GPU DMA quiescence and cross-component safety are not established. BQ reads start watchdog; D0 RESET changes charger defaults.'}
        (out/'combined-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))
if __name__=='__main__':main()
