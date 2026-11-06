#!/usr/bin/env python3
"""既存のオフライン更新方式でGE8300/表示ドライバの適用媒体を生成する。"""
import argparse, hashlib, json, os, re, shutil, struct, subprocess
from pathlib import Path

def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def main():
    ap=argparse.ArgumentParser()
    for name in ('workspace','base','provider','display','out','tools'):
        ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--source-commit',required=True)
    a=ap.parse_args();root=a.workspace.resolve(strict=True)
    if not re.fullmatch('[0-9a-f]{40}',a.source_commit):raise SystemExit('完全なソースcommitが必要です')
    base,provider,display,out,tools=(getattr(a,n).resolve() for n in ('base','provider','display','out','tools'))
    if any(not p.is_relative_to(root) for p in (base,provider,display,out,tools)) or out.exists():
        raise SystemExit('作業ルート内の入力と新規出力先が必要です')
    if sha(base/'maintenance-staged.wim')!='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' or sha(base/'maintenance-staged-esp.img')!='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce':
        raise SystemExit('検証済み更新用baseと一致しません')
    names={'provider':('mt8167-ge8300.inf','mt8167-ge8300.sys','mt8167-ge8300.cat','ge8300-mips.fw','gpu-test.cer'),
           'display':('mt8167-display.inf','mt8167-display.sys','mt8167-display.cat')}
    files={}
    for kind,pkg in (('provider',provider),('display',display)):
        for name in names[kind]:files[kind+'/'+name]=sha(pkg/name)
        text=(pkg/names[kind][0]).read_text()
        v=re.search(r'DriverVer\s*=\s*(\d{2}/\d{2}/\d{4}),([\d.]+)',text,re.I)
        old=(0,0,1,2) if kind=='provider' else (0,0,1,0)
        if not v or tuple(map(int,v[2].split('.')))<=old:
            raise SystemExit('旧版より新しいINF DriverVerが必要です: '+kind)
    if files['provider/ge8300-mips.fw']!='5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1':
        raise SystemExit('m1.11の固定firmwareと一致しません')
    out.mkdir();stage=out/'stage';stage.mkdir()
    token=files['provider/mt8167-ge8300.sys'][:16].upper()
    cert=(provider/'gpu-test.cer').read_bytes();thumb=hashlib.sha1(cert).hexdigest().upper()
    blob=struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
    trust=[]
    for store in ('Root','TrustedPublisher'):
        key='HKLM\\Mt8167GpuOffline\\Microsoft\\SystemCertificates\\'+store+'\\Certificates\\'+thumb
        trust+=['reg.exe add "'+key+'" /v Blob /t REG_BINARY /d '+blob.hex()+' /f >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto trustfail']
    here=Path(__file__).resolve().parent
    cmd=(here/'run-update-ge8300-once.cmd.in').read_text().replace('@JOURNAL@',token.lower()).replace('@TOKEN@',token).replace('@TRUST_COMMANDS@','\n'.join(trust))
    if re.search(r'@[A-Z_]+@',cmd):raise SystemExit('未置換の変数があります')
    (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode())
    start='@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n'
    (stage/'startnet.cmd').write_bytes(start.encode())
    image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
    env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
    wim=tools/'wimtools/wimlib-imagex'
    commands=''.join('add "'+str(src)+'" '+dst+'\n' for src,dst in ((stage/'startnet.cmd','/Windows/System32/startnet.cmd'),(stage/'run-update-once.cmd','/a04br3/run-update-once.cmd'),(provider,'/a04br3/upd/provider'),(display,'/a04br3/upd/display')))
    subprocess.run([str(wim),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
    subprocess.run([str(wim),'verify',str(image)],env=env,check=True)
    subprocess.run(['python3',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
    (out/'manifest.json').write_text(json.dumps({'source_commit':a.source_commit,'token':token,'package_sha256':files,'provider_signer_sha1':thumb,'wim_sha256':sha(image),'esp_sha256':sha(out/'update-esp.img'),'status':'generated; offline installation and installed Windows operation unverified'},indent=2)+'\n')

if __name__=='__main__':main()
