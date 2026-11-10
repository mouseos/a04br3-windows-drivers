#!/usr/bin/env python3
"""Windows ARM incremental client build. Preinstalled SDK/compiler/signer only."""
from pathlib import Path
import argparse,hashlib,json,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);a=p.parse_args();t=a.task_root
r=t/'source';o=t/'build';m=json.loads((t/'client-input.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
if o.exists():raise ValueError('New output required; preserve previous attempts')
o.mkdir();(o/'logs').mkdir()
for row in m['source_files']:
 if sha(r/row['path'])!=row['sha256']:raise ValueError('Source pin '+row['path'])
llvm=Path('C:/Program Files/LLVM/bin');flags=m['arguments'];units=[]
def run(args,name):
 q=subprocess.run([str(x) for x in args],capture_output=True,text=True);(o/'logs'/name).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(name+' '+q.stdout[-2500:]+q.stderr[-2500:])
 return q.stdout+q.stderr
for file in ['d3d-ui-texture.c','memory.c']:
 src=r/'drivers/gpu/mt8167/wddm-render/testclient'/file;obj=o/(file+'.obj')
 run([llvm/'clang.exe',*flags,'-H','-c',src,'-o',obj],file+'.compile.log')
 units.append({'source':str(src.relative_to(r)),'sha256':sha(src),'object':obj.name,'object_sha256':sha(obj),'arguments':flags})
libs=Path('C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/arm')
inputs=[libs/(s+'.lib') for s in ['kernel32','setupapi','dxgi','d3d11']]
exe=o/'ge8300-d3d-ui-texture.exe'
run([llvm/'lld-link.exe','/machine:arm','/subsystem:console,6.03','/entry:mainCRTStartup','/nodefaultlib',*[o/u['object'] for u in units],*inputs,'/out:'+str(exe),'/map:'+str(o/'client.map')],'link.log')
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe')
run([sign,'sign','/fd','SHA256','/sha1','BCBF6C456C6F0DD0818434763B189488FBF13570',exe],'sign.log')
run([sign,'verify','/pa','/v',exe],'signature-verify.log')
report={'schema':'ui-texture-client-build-v1','source_commit':m['source_commit'],'source_files':m['source_files'],'units':units,'toolchain':[{'path':str(f),'sha256':sha(f)} for f in [llvm/'clang.exe',llvm/'lld-link.exe',sign]],'libraries':[{'path':str(f),'sha256':sha(f)} for f in inputs],'exe_sha256':sha(exe),'SDK_setup':False,'PCO_rebuilt':False,'driver_rebuilt':False,'hardware_verified':False,'command':['ge8300-d3d-ui-texture.exe','--hardware=PowerVR'],'timeout_ms':900000}
(o/'build-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
with zipfile.ZipFile(t/'client-outputs.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in o.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(o))
print('CLIENT_BUILD_SIGN_COMPLETED',report['exe_sha256'])
