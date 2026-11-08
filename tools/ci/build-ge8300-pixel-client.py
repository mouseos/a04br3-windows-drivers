#!/usr/bin/env python3
"""One ARM client TU; reuse pinned332 memory support and existing SDK/signing tools."""
import argparse,hashlib,json,subprocess,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--baseline-build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--um-libs',type=Path,required=True);p.add_argument('--sign-tool',type=Path,required=True);a=p.parse_args()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
run=lambda cmd:subprocess.run([str(x) for x in cmd],check=True)
if not a.output.resolve().is_relative_to(R):raise ValueError('output must stay in source project')
b=load(a.baseline_build/'build-manifest.json');u=next(x for x in b['compiled_units'] if x['object']=='testclient_boundary.obj');m=next(x for x in b['compiled_units'] if x['object']=='testclient_memory.obj');memory=a.baseline_build/'client'/m['object']
if sha(memory)!=m['object_sha256']:raise ValueError('memory support object pin')
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);src=R/'drivers/gpu/mt8167/wddm-render/testclient';obj=out/'pixel-client.obj';exe=out/'ge8300-d3d-texture-pixel-boundary.exe'
flags=[f.replace('\\\\','\\') for f in u['arguments']]
run(['clang',*flags,'-c',src/'d3d-texture-pixel-boundary.c','-o',obj])
run(['lld-link','/subsystem:console','/machine:arm','/entry:mainCRTStartup','/nodefaultlib',obj,memory,*[a.um_libs/n for n in ['kernel32.lib','setupapi.lib','dxgi.lib','d3d11.lib']],'/out:'+str(exe)])
thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,exe]);run([a.sign_tool,'verify','/pa',exe])
manifest={'schema':'pixel-result-client-only-v1','source_commit':subprocess.check_output(['git','-C',str(R),'rev-parse','HEAD'],text=True).strip(),'source_hashes':{str(x.relative_to(R)):sha(x) for x in src.iterdir() if x.is_file()},'flags':flags,'compiled_TUs':1,'object_sha256':sha(obj),'retained_memory_object_sha256':sha(memory),'exe_sha256':sha(exe),'client_timeout_seconds':900,'draw_logic_changed':False,'driver_changed':False,'hardware_verified':False,'root_reset_fix_proven':False}
(out/'signed-client-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
