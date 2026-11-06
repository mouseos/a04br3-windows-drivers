#!/usr/bin/env python3
"""Build an offline WinPE ADC rejection diagnostic; never writes the device."""
import argparse, hashlib, json, os, shutil, struct, subprocess, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
BASE = ROOT / 'port/xnu/out/windows-rt-ge8300-copy-apmixed714-5ee5528'
TOOLS = ROOT / 'port/xnu/out/windows-rt-codec-media-tools'
EXPECTED = {'volume.wim':'f794d86e11525a7508c02308b9ca43db1b3b86f0c8fc147f4e0d6c89843c3070',
            'volume-esp.img':'78c99778bd885a410bc1e8bf780a39fd7faf83d49b06305a153b86aaa991005d',
            'boot.img':'94c3ea8ec2201ca5412562042ce05e5329793cbe3efb416f063d8555e226b34d'}
BCD = '65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'

def sha(p):
    with p.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--packages',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args(); packages=a.packages.resolve(); out=a.output.resolve()
    if not packages.is_relative_to(ROOT) or not out.is_relative_to(ROOT) or not out.is_dir() or any(out.iterdir()):
        raise SystemExit('Packages and empty output must be within the project root')
    for name,digest in EXPECTED.items():
        if sha(BASE/name)!=digest: raise SystemExit('Base hash mismatch: '+name)
    if any(c.isspace() or c in "\"'" for c in str(out)): raise SystemExit('Output path contains unsupported characters')
    env=os.environ.copy(); env['TMPDIR']=str(out);env['PYTHONDONTWRITEBYTECODE']='1'
    env['PATH']=str(TOOLS/'runtime/usr/bin')+os.pathsep+env['PATH']
    env['LD_LIBRARY_PATH']=str(TOOLS/'runtime/usr/lib/x86_64-linux-gnu')
    env['PYTHONPATH']=str(TOOLS/'python')
    def run(args,**kw): return subprocess.run(args,check=True,env=env,**kw)
    top=packages/'toprgu'; thermal=packages/'thermal'; shared=packages/'shared/package'
    files=[top/'mt8167-toprgu.sys',top/'mt8167-toprgu.cat',top/'mt8167-toprgu.inf',top/'mt8167-toprgu-test.cer',
           thermal/'mt8167-thermal.sys',thermal/'mt8167-thermal.cat',thermal/'mt8167-thermal.inf',thermal/'mt8167-thermal-test.cer',
           shared/'parent/mt8167-shared.sys',shared/'parent/shared-parent.cat',shared/'parent/shared-parent.inf',
           shared/'touch/bu21025.sys',shared/'touch/bu21025.cat',shared/'touch/shared-touch.inf',shared/'shared-touch-test.cer']
    if not all(f.is_file() and f.resolve().is_relative_to(ROOT) for f in files):raise SystemExit('A signed package file is missing')
    manifests=[json.loads((d/'manifest.json').read_text(encoding='utf-8-sig')) for d in (top,thermal,packages/'shared')]
    if len({m['source_commit'] for m in manifests})!=1:raise SystemExit('Package source commits differ')
    if not all(manifests[0].get(k)==1 for k in ('cpu1_handoff','uart_screen','uart_volume','thermal_stop')) or not all(manifests[1].get(k)==1 for k in ('shared_parent','oneshot','codec_adc')) or not all(manifests[2].get(k)==1 for k in ('thermal_parent','touch_lease')):
        raise SystemExit('Required diagnostic build flags missing')
    for d,m in zip((top,thermal),manifests):
        for item in m['files']:
            if sha(d/item['name'])!=item['sha256'].lower():raise SystemExit('Package hash mismatch')
    with tempfile.TemporaryDirectory(prefix='adc-media-',dir=out) as td:
        tmp=Path(td)
        def extract(path,name):
            d=tmp/name;d.mkdir();run(['wimlib-imagex','extract',str(BASE/'volume.wim'),'1',path,'--dest-dir='+str(d)],stdout=subprocess.DEVNULL)
            f=[p for p in d.rglob('*') if p.is_file()]
            if len(f)!=1:raise ValueError('Could not extract '+path)
            return f[0]
        software=extract('/Windows/System32/config/SOFTWARE','software')
        start=extract('/Windows/System32/startnet.cmd','startnet')
        raw=start.read_bytes(); marker=b'echo MT8167_BU21025_DRVLOAD_RETURNED\r\n'
        if raw.count(marker)!=1:raise ValueError('Unexpected startnet')
        raw=raw.replace(marker,marker+b'echo A04ADC_THERMAL_DRVLOAD_START\r\ndrvload X:\\a04br3\\codec-adc\\mt8167-thermal.inf\r\necho A04ADC_THERMAL_DRVLOAD_RETURNED=%errorlevel%\r\n')
        start.write_bytes(raw)
        for cert in (top/'mt8167-toprgu-test.cer',thermal/'mt8167-thermal-test.cer',shared/'shared-touch-test.cer'):
            der=cert.read_bytes(); thumb=hashlib.sha1(der).hexdigest().upper(); blob=struct.pack('<III',32,1,len(der))+der
            run(['openssl','x509','-inform','DER','-in',str(cert),'-noout'])
            for store in ('Root','TrustedPublisher'):
                base='\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
                listing=run(['hivexsh',str(software)],input='cd '+base+'\nls\n',text=True,capture_output=True)
                if listing.stderr or thumb in listing.stdout:raise ValueError('Certificate store unexpected')
                cmd='cd '+base+'\nadd '+thumb+'\ncd '+thumb+'\nsetval 1\nBlob\nhex:3:'+blob.hex()+'\ncommit\n'
                result=run(['hivexsh','-w',str(software)],input=cmd,text=True,capture_output=True)
                if result.stderr or run(['hivexget',str(software),base+'\\'+thumb,'Blob'],capture_output=True).stdout!=blob:raise ValueError('Certificate update failed')
        updates={'/Windows/System32/config/SOFTWARE':software,'/Windows/System32/startnet.cmd':start,
                 '/Windows/System32/Drivers/mt8167-toprgu.sys':top/'mt8167-toprgu.sys'}
        for f in files:
            if f.parent==top: dest='/a04br3/toprgu/'+f.name
            elif f.parent==thermal: dest='/a04br3/codec-adc/'+f.name
            elif f.parent==shared/'parent':dest='/a04br3/mt8167-shared/'+f.name
            elif f.parent==shared/'touch':dest='/a04br3/bu21025-shared/'+f.name
            else:continue
            updates[dest]=f
        wim=out/'diagnostic.wim';shutil.copyfile(BASE/'volume.wim',wim)
        run(['wimlib-imagex','update',str(wim),'1'],input='\n'.join('add '+str(src)+' '+dst for dst,src in updates.items())+'\n',text=True)
        run(['wimlib-imagex','verify',str(wim)])
        for i,(dest,source) in enumerate(updates.items()):
            d=tmp/('check'+str(i));d.mkdir();run(['wimlib-imagex','extract',str(wim),'1',dest,'--dest-dir='+str(d)],stdout=subprocess.DEVNULL)
            got=[p for p in d.rglob('*') if p.is_file()]
            if len(got)!=1 or sha(got[0])!=sha(source):raise ValueError('WIM readback mismatch '+dest)
        esp=out/'diagnostic-esp.img'
        run(['python3',str(Path(__file__).with_name('replace-esp-wim.py')),'--template',str(BASE/'volume-esp.img'),'--wim',str(wim),'--output',str(esp),'--template-bcd-sha256',BCD,'--template-wim-sha256',EXPECTED['volume.wim']])
        boot=out/'boot.img';shutil.copyfile(BASE/'boot.img',boot)
        if sha(boot)!=EXPECTED['boot.img']:raise ValueError('Boot image mismatch')
        manifest={'driver_source_commit':manifests[0]['source_commit'],'base':EXPECTED,'package_files':{str(p.relative_to(packages)):sha(p) for p in files},'wim':sha(wim),'esp':sha(esp),'boot':sha(boot),'limitations':['Test signatures and device operation unverified','ADC route and mute gate rejects measurements','UART queue acceptance is not transmission','No device writes performed']}
        (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
