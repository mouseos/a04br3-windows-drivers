#!/usr/bin/env python3
"""Build a WinPE copy using the validated read-only storage and signed tools."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

BASE_SHA='8ebf8a4c14d16b233d6416f848a39acbedd9a72d945452bfe6442f549dc30835'
STORAGE={'mt8167-msdc.sys':'61357807398ddf45bd0daaafae2ef82805422e2329b823b6bbff388faa1a2d15',
         'mt8167-msdc-readonly.inf':'7406f2d65ee081d00efaac732bc66735541edd7639730d68de2cbcbdd4579a7b',
         'mt8167-msdc-readonly.cat':'1e667e3485a11b0b6d0fef676d9a1b5dcec4dcabbe411a939cfa039743906ba5'}
STRESS_SHA='caaa86425719ca625546a1f1c3772913f875e1497cf43e492fa181bb4d743451'
BATTERY_NAMES=('lc709203f.inf','lc709203f.sys','lc709203f.cat','lc709203f-test.cer',
               'lc709203f-status.exe','lc709203f-pnp-status.exe')
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def signed_arm(path):
    b=path.read_bytes()
    if len(b)<64 or b[:2]!=b'MZ': raise SystemExit(f'Not PE: {path}')
    off=struct.unpack_from('<I',b,60)[0]
    if off>len(b)-160 or b[off:off+4]!=b'PE\0\0' or struct.unpack_from('<H',b,off+4)[0]!=0x1c4 or struct.unpack_from('<H',b,off+24)[0]!=0x10b:
        raise SystemExit(f'Not ARM Thumb PE32: {path}')
    pos,length=struct.unpack_from('<II',b,off+24+96+4*8)
    if not pos or not length or pos>len(b) or length>len(b)-pos: raise SystemExit(f'No embedded signature: {path}')
p=argparse.ArgumentParser()
p.add_argument('--source-wim',required=True,type=Path);p.add_argument('--battery-package',required=True,type=Path)
p.add_argument('--storage-package',required=True,type=Path);p.add_argument('--stress-exe',required=True,type=Path)
p.add_argument('--cert-helper',required=True,type=Path);p.add_argument('--cert-helper-sha256',required=True)
p.add_argument('--output',required=True,type=Path);p.add_argument('--wimlib',default='wimlib-imagex')
a=p.parse_args();root=Path(__file__).resolve().parents[2]
files={'/Windows/System32/startnet.cmd':root/'tools/winpe/startnet-bqhr-battery-storage.cmd',
       '/a04br3/msdc-read-stress.exe':a.stress_exe,'/a04br3/bqhr-test-cert.exe':a.cert_helper}
files.update({f'/a04br3/lc709203f/{n}':a.battery_package/n for n in BATTERY_NAMES})
for n in STORAGE:files[f'/a04br3/storage-readonly/{n}']=a.storage_package/n
for path in [a.source_wim,*files.values()]:
    if not path.is_file():raise SystemExit(f'Missing input: {path}')
    if any(c.isspace() or c in '\"\'' for c in str(path.resolve())):raise SystemExit('Input path contains unsupported whitespace or quotes')
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.partial').exists():raise SystemExit('Output or partial output already exists')
if digest(a.source_wim)!=BASE_SHA or digest(a.stress_exe)!=STRESS_SHA:raise SystemExit('Base WIM or signed SHA-256 utility differs from expected')
if digest(a.cert_helper)!=a.cert_helper_sha256.lower():raise SystemExit('Certificate helper hash mismatch')
for name,expected in STORAGE.items():
    if digest(a.storage_package/name)!=expected:raise SystemExit(f'Read-only storage package differs: {name}')
for path in (a.stress_exe,a.cert_helper,a.battery_package/'lc709203f.sys',a.battery_package/'lc709203f-status.exe',a.battery_package/'lc709203f-pnp-status.exe',a.storage_package/'mt8167-msdc.sys'):
    signed_arm(path)
sys=(a.battery_package/'lc709203f.sys').read_bytes()
if b'A04BAT BQHR STOPPED VERIFIED' not in sys or b'A04BAT CHARGER STOPPED' in sys:
    raise SystemExit('Battery SYS is not the expected diagnostic-only build')
if b'MSDC_REFERENCE_SHA256=' not in a.stress_exe.read_bytes():raise SystemExit('SHA-256 stress marker missing')
a.output.parent.mkdir(parents=True,exist_ok=True);partial=a.output.with_suffix(a.output.suffix+'.partial')
with a.source_wim.open('rb') as src,partial.open('xb') as dst:shutil.copyfileobj(src,dst,1024*1024)
commands=['delete --force /Windows/System32/winpeshl.ini']+[f'add {source.resolve()} {dest}' for dest,source in files.items()]
subprocess.run([a.wimlib,'update',str(partial),'1'],input='\n'.join(commands)+'\n',text=True,check=True)
subprocess.run([a.wimlib,'verify',str(partial)],check=True)
with tempfile.TemporaryDirectory(prefix='a04-bqhr-wim-') as tmp:
    subprocess.run([a.wimlib,'extract',str(partial),'1','/a04br3','/Windows/System32/startnet.cmd',f'--dest-dir={tmp}'],check=True)
    for dest,original in files.items():
        relative=dest.removeprefix('/a04br3/') if dest.startswith('/a04br3/') else 'startnet.cmd'
        extracted=Path(tmp)/('a04br3' if dest.startswith('/a04br3/') else '')/relative
        if digest(extracted)!=digest(original):raise SystemExit(f'WIM comparison failed: {dest}')
partial.rename(a.output)
manifest={'source_wim_sha256':BASE_SHA,'wim_sha256':digest(a.output),'android_recovery_lba':'0xE1000','android_recovery_bytes':16777216,'android_recovery_sha256':'473ec59979015aa168b251f3d31892baf325526adc85e61e04e6538ae74ba0f1','inputs':{dest:digest(src) for dest,src in files.items()},'limitations':'Test signatures; WinPE-only cert trust. BQHR requires same-boot UART marker. Reference hash may be stale after writes. No hardware run.'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Created and verified:',a.output)
