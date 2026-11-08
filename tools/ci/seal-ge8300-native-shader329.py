#!/usr/bin/env python3
"""Verify completed compile/link/sign checkpoints and seal only the catalog."""
import argparse,hashlib,json,subprocess,ntpath
from pathlib import Path
root=Path(__file__).resolve().parents[2];p=argparse.ArgumentParser()
for name in ['build','baseline-build','baseline-package','inf2cat','sign-tool']:p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--build-source',required=True);a=p.parse_args();build=a.build.resolve();package=build/'package329'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def run(cmd):subprocess.run(list(map(str,cmd)),check=True)
def capture(cmd):return subprocess.check_output(list(map(str,cmd)),text=True).strip()
def pin(p,h):
 if sha(p)!=h:raise ValueError('Hash mismatch: '+str(p))
head=capture(['git','-C',root,'rev-parse','HEAD'])
if capture(['git','-C',root,'status','--porcelain']):raise ValueError('Clean source required')
if not build.is_relative_to(root) or len(a.build_source)!=40:raise ValueError('Pinned build/source required')
changes=capture(['git','-C',root,'diff','--name-only',a.build_source,head]).splitlines()
if any(n not in ['tools/ci/seal-ge8300-native-shader329.py','tools/ci/build-ge8300-native-shader329.py','README.md','docs/ge8300-native-shader329.md'] for n in changes):raise ValueError('Runtime changed after completed compile/link/sign')
run(['python','-B',root/'tools/ci/verify-ge8300-fullsource.py'])
pin(a.baseline_package/'signed-manifest.json','7e4ccf146cb8081a93e85b7f83ac24a50ea6f66f8888f4403ccaf08f19433663');pin(a.baseline_build/'build-manifest.json','1c74110363c68d2270513d3754f06f554632b36d184fa3803290f8cd07fc47c6')
base=load(a.baseline_build/'build-manifest.json');old=load(a.baseline_package/'signed-manifest.json');records=load(build/'compiled-checkpoint.json')
for parent,m in [(a.baseline_package,old),(a.baseline_build,base)]:
 for row in m['outputs']:pin(parent/row['name'],row['sha256'])
for r in records:
 pin(root/r['source'],r['source_sha256']);pin(build/('umd' if r['role']=='entry' else r['role'])/r['object'],r['object_sha256'])
# Independently parse MS/COFF archive containers; inspect metadata, not code.
def members(path):
 data=Path(path).read_bytes();at=8;names=b'';rows={}
 if data[:8]!=b'!<arch>\n':raise ValueError('Archive magic')
 while at+60<=len(data):
  h=data[at:at+60];size=int(h[48:58]);n=h[:16].decode().strip();b=data[at+60:at+60+size]
  if len(b)!=size:raise ValueError('Truncated archive')
  if n=='//':names=b
  elif n not in ['/','/SYM64/']:
   if n.startswith('/') and n[1:].isdigit():n=names[int(n[1:]):].split(b'\0')[0].split(b'\n')[0].decode().rstrip('/')
   else:n=n.rstrip('/')
   n=ntpath.basename(n)
   if n in rows:raise ValueError('Duplicate archive leaf')
   rows[n]=hashlib.sha256(b).hexdigest()
  at+=60+size+(size&1)
 return rows
archive_specs=[('native','ge8300-native-owner-core.lib',a.baseline_build/'ge8300-native-owner-core.lib',{'wddm-render-frontend-shader_draw_protocol.obj','wddm-render-compiler-witness_codec.obj','wddm-render-compiler-witness_info.obj'}),('kmd','ge8300-pair-kmd.lib',a.baseline_build/'ge8300-pair-kmd.lib',set()),('umd','ge8300-native-umd.lib',a.baseline_build/'ge8300-native-umd.lib',{'frontend_shader_draw_protocol.obj'}),('pco','ge8300-pco-arm32.lib',Path('C:/mt8167-sm4-lane324/dependencies/compiler/ge8300-pco-arm32.lib'),{'witness_producer.obj','witness_codec.obj','witness_info.obj'})]
# The PCO core input is the exact pinned327 compiler input.
pin(archive_specs[-1][2],next(r['sha256'] for r in base['existing_compiler_libraries'] if r['name']=='ge8300-pco-arm32.lib'))
archives=[]
for tag,leaf,prior,extra in archive_specs:
 before=members(prior);after=members(build/leaf)
 if set(after)!=set(before)|extra:raise ValueError('Changed archive membership: '+tag)
 compiled={r['object']:r for r in records if r['role']==tag};rows=[]
 for n,h in after.items():
  pin(build/tag/n,h)
  if before.get(n)!=h and n not in compiled:raise ValueError('Unattributed member change: '+tag+'/'+n)
  if n in compiled and compiled[n]['object_sha256']!=h:raise ValueError('Compiler checkpoint/member mismatch')
  rows.append({'name':n,'sha256':h,'before_sha256':before.get(n),'compiled':n in compiled})
 archives.append({'role':tag,'file':leaf,'sha256':sha(build/leaf),'members':rows})
thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';signed=[]
for n in ['ge8300-render.sys','ge8300-umd.dll']:
 pin(package/n,sha(build/n));run([a.sign_tool,'verify','/pa',package/n]);signed.append({'name':n,'sha256':sha(package/n)})
# Exact retained payloads; no driver/client/firmware rebuild in this seal step.
for n in ['ge8300-mips.fw','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','mt8167-development.cer']:pin(package/n,sha(a.baseline_package/n))
expected=(a.baseline_package/'ge8300-render.inf').read_text(encoding='utf-16').replace('DriverVer=11/07/2026,0.0.3.27','DriverVer=11/07/2026,0.0.3.29')
if (package/'ge8300-render.inf').read_text(encoding='utf-16')!=expected:raise ValueError('INF difference beyond exact version')
pin(a.inf2cat.parent/'WindowsProtectedFiles.xml','8d56b37b414a61796b9a764aae502030a9e08fd98fa283ec997e8b3926e03643')
pin(a.inf2cat,sha(Path('C:/mt8167-sm4-lane324/dependencies/inf2cat/Inf2Cat.exe')))
run([a.inf2cat,'/driver:'+str(package),'/os:6_3_ARM','/uselocaltime']);run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,package/'ge8300-render.cat'])
for n in ['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']:run([a.sign_tool,'verify','/pa','/c',package/'ge8300-render.cat',package/n])
m={'schema':'native-alu329-independent-sources-v1','package_source_commit':head,'build_source_commit':a.build_source,'kernel_binary_source_commit':a.build_source,'native_owner_source_commit':a.build_source,'umd_source_commit':a.build_source,'baseline_manifest_file':'baseline327-signed-manifest.json','baseline_manifest_sha256':sha(a.baseline_package/'signed-manifest.json'),'client_source_commit':old['client_source_commit'],'new_client_source_commit':old['new_client_source_commit'],'new_client_file':'ge8300-d3d-texture.exe','inf_driverver':'11/07/2026,0.0.3.29','compiled_units':records,'archives':archives,'adapter_contract_version':5,'shader_draw_version':4,'texture_draw_version':3,'sampler_profile_version':4,'driver_wire_version':4,'native_resolve_ABI_version':1,'control_version':2,'shader_witness_ISA':0x32901,'shader_admission_scope':'memoryless float32 ALU; POSITION4; VS no CB; PS first4 CB vectors; USC192 bytes/stage','standard_profile_qualified':False,'PCO_core_rebuilt':False,'SDK_rebuilt':False,'kernel_changed':True,'native_owner_changed':True,'umd_changed':True,'firmware_changed':False,'client_changed':False,'hardware_verified':False,'runtime_verified':False,'SYS_DLL_resigned_in_seal':False,'inf2cat_xml_sha256':sha(a.inf2cat.parent/'WindowsProtectedFiles.xml'),'outputs':[{'name':n,'sha256':sha(package/n)} for n in ['ge8300-render.inf','ge8300-render.cat','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','mt8167-development.cer']]}
(build/'build-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(package/'native329-build-manifest.json').write_bytes((build/'build-manifest.json').read_bytes())
m['build_manifest_file']='native329-build-manifest.json';m['build_manifest_sha256']=sha(package/'native329-build-manifest.json')
m['retained_provenance']=[{'name':f.name,'sha256':sha(f)} for f in package.iterdir() if f.is_file() and f.name not in [r['name'] for r in m['outputs']]]
(package/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('NATIVE_ALU329_CATALOG_AND_ALL_CLOSURE_PINS_VERIFIED',head,len(records))
