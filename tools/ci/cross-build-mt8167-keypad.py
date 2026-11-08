#!/usr/bin/env python3
"""Build only MT8167 keypad using recovered WDK8.1 headers/libs, never an SDK setup."""
from pathlib import Path
import argparse,hashlib,json,subprocess,os,shutil
p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=a.output.resolve();cache=a.cache.resolve()
if not out.is_relative_to(r) or out.exists():raise SystemExit('専用checkout内の未作成outputが必要です')
if subprocess.check_output(['git','-C',str(r),'status','--porcelain']):raise SystemExit('sourceを先にcommitしてください')
commit=subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip();inc=cache/'sdk-headers/wdk/Include';lib=cache/'compiler-libs/wdk/Lib'
(out/'tmp').mkdir(parents=True);(out/'obj').mkdir();(out/'package').mkdir()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def entry(p):
 if p.is_file():return {'type':'file','name':p.name,'external-contents':str(p)}
 return {'type':'directory','name':p.name,'contents':[entry(x) for x in sorted(p.iterdir())]}
node=entry(inc);node['name']=str(inc);overlay=out/'wdk-case-overlay.json';overlay.write_text(json.dumps({'version':0,'case-sensitive':False,'roots':[node]}))
flags=['--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fms-extensions','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-mfpu=none','-D__ldrexd=__mt6392_keys_wdk81_ldrexd_declaration','-D_KERNEL_MODE','-D_ARM_','-DARM','-DNTDDI_VERSION=0x06030000','-D_WIN32_WINNT=0x0603','-DKMDF_VERSION_MAJOR=1','-DKMDF_VERSION_MINOR=13','-ivfsoverlay',str(overlay)]
for x in ('km','km/crt','shared','um','wdf/kmdf/1.13'):flags+=['-isystem',str(inc/x)]
env=os.environ.copy();env['TMPDIR']=str(out/'tmp');commands=[]
def run(cmd):
 commands.append(cmd);subprocess.run(cmd,env=env,check=True)
compiled=[];objects=[]
for rel in ('drivers/input/mt8167-keypad/wdf_keys.c','drivers/input/a04br3-buttons/button_state.c','drivers/input/a04br3-buttons/button_pipeline.c'):
 src=r/rel;obj=out/'obj'/(src.stem+'.obj');run(['clang',*flags,'-c',str(src),'-o',str(obj)]);objects.append(obj);compiled.append({'path':rel,'source_sha256':sha(src),'object_sha256':sha(obj)})
libs=[lib/'wdf/kmdf/arm/1.13'/x for x in ('wdfdriverentry.lib','wdfldr.lib')]+[lib/'winv6.3/km/arm'/x for x in ('ntoskrnl.lib','hal.lib','bufferoverflowfastfailk.lib','libcntpr.lib')]
sys=out/'package/mt8167-keypad.sys';run(['lld-link','/driver','/subsystem:native','/machine:arm','/entry:FxDriverEntry','/nodefaultlib',*map(str,objects),*map(str,libs),'/out:'+str(sys)])
# INF checkout may be LF on Linux; Windows canonical checkout policy is CRLF.
inf=r/'drivers/input/mt8167-keypad/mt8167-keypad.inf';(out/'package'/inf.name).write_bytes(inf.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
inputs=[{'path':str(x.relative_to(r)),'sha256':sha(x)} for x in sorted((r/'drivers/input/mt8167-keypad').iterdir()) if x.is_file()]
for rel in ('drivers/bus/mt8167-shared/client_interface.h','drivers/input/a04br3-buttons/shared_parent_contract.h','drivers/input/a04br3-buttons/button_pipeline.h','drivers/input/a04br3-buttons/button_state.h','tools/ci/cross-build-mt8167-keypad.py','tools/ci/sign-mt8167-keypad.ps1','tests/mt8167_keypad_test.c'):inputs.append({'path':rel,'sha256':sha(r/rel)})
manifest={'status':'unsigned-cross-built-with-recovered-WDK81','source_commit':commit,'source_dirty':False,'target':'MT6392\\Buttons','service':'Mt6392Keys','version':'0.0.1.1','compiler':subprocess.check_output(['clang','--version'],text=True).splitlines()[0],'linker':subprocess.check_output(['lld-link','--version'],text=True).strip(),'compiled':compiled,'inputs':inputs,'libraries':[{'path':str(x),'sha256':sha(x)} for x in libs],'commands':commands,'codegen':flags,'parent_changed':False,'firmware_changed':False,'device_tested':False,'wake_implemented':False,'unsigned_outputs':[{'path':str(x.relative_to(out)),'bytes':x.stat().st_size,'sha256':sha(x)} for x in sorted((out/'package').iterdir()) if x.is_file()]}
(out/'unsigned-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('MT8167_KEYPAD_CROSS_BUILD_NOT_DEVICE_TESTED')
