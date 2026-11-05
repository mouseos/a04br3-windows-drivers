#!/usr/bin/env python3
"""Compare the four-CPU screen template and diagnostic watchdog components."""
import argparse, hashlib, json, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--base',required=True,type=Path);p.add_argument('--candidate',required=True,type=Path);p.add_argument('--wimlib',default='wimlib-imagex');a=p.parse_args()
paths=['/Windows/System32/config/SYSTEM','/Windows/System32/Drivers/mt8167-toprgu.sys','/a04br3/uart-screen.exe']
expected={'SYSTEM':'cc151bf339dd2683b313e95d42179cfe571f6392c7a294b9a98f19a8a83e5214','mt8167-toprgu.sys':'52a1a1cb0cd66821099a87db9aca5bd82797061ab0e99a047d495d7fe06c2b3e','uart-screen.exe':'655bfc0fa4ae4369b7aa487a747dbe9b41780b3078bc335261fa9c83e53236d7'}
with tempfile.TemporaryDirectory(prefix='a04-render-base-') as tmp:
 root=Path(tmp)
 for name,src in (('base',a.base),('candidate',a.candidate)):
  dest=root/name;dest.mkdir();subprocess.run([a.wimlib,'extract',str(src),'1',*paths,f'--dest-dir={dest}'],check=True)
  for f,h in expected.items():
   if hashlib.sha256((dest/f).read_bytes()).hexdigest()!=h:raise SystemExit(f'{name}: {f} mismatch')
  node='\\ControlSet001\\Services\\Mt8167Toprgu'
  for key,value in (('Start',b'0\n'),('Group',b'System Bus Extender\n')):
   if subprocess.run(['hivexget',str(dest/'SYSTEM'),node,key],capture_output=True,check=True).stdout!=value:raise SystemExit(f'{name}: {key} mismatch')
print(json.dumps({'compared_sha256':expected,'toprgu_start':0,'toprgu_group':'System Bus Extender','device_test':False},indent=2))
