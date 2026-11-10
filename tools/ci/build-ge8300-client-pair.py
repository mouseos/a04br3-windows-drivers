#!/usr/bin/env python3
"""Two separate one-TU builds. The launcher digest must be a real Git commit."""
import argparse,hashlib,json,os,re,shutil,struct,subprocess,time,zipfile
from pathlib import Path
from ge8300_client_signing_common import require,pe,unchanged_signed_payload,dependencies
RECIPE=Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def fixed(root,commit):
 require(subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()==commit,'Exact runtime source HEAD required')
 require(not subprocess.check_output(['git','-C',str(root),'status','--porcelain']),'Clean runtime source required')
 return commit

def main():
 p=argparse.ArgumentParser();p.add_argument('--enable-build',action='store_true');p.add_argument('--phase',choices=['evaluator','launcher'],required=True)
 for n in ['source-root','memory-object','output','um-include','km-include','arm-libraries','clang','linker','signtool']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--resume-compile-receipt-sha256');p.add_argument('--resume-object-sha256');p.add_argument('--resume-dependency-sha256');p.add_argument('--source-commit',required=True);p.add_argument('--evaluator',type=Path);p.add_argument('--evaluator-receipt',type=Path);p.add_argument('--evaluator-receipt-sha256');a=p.parse_args()
 require(a.enable_build and os.name=='nt','Explicit Windows one-TU build only')
 root=a.source_root.resolve();fixed(root,a.source_commit);recipe=fixed(RECIPE,subprocess.check_output(['git','-C',str(RECIPE),'rev-parse','HEAD'],text=True).strip())
 for name in ['CL','_CL_','LINK','_LINK_']:require(not os.environ.get(name),'Unpinned inherited compiler/linker options')
 tools={'clang':(a.clang,'57e32b1ea2456f12f97bf802453171bbff12d56b2c486b4eca0c75343c0cda2c'),'linker':(a.linker,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'),'signtool':(a.signtool,'854c356a9d9977fb2ed22ce04b531ab6f4417da06a4dba6e3d3d628b9cfb988c'),'powershell':(Path(shutil.which('powershell') or ''),'38f4384643b3fa0de714d2367b712c2e0fa1c89e2cfd131ae6b831ad962b1033')}
 for tool,h in tools.values():require(sha(tool)==h,'Fixed existing tool mismatch '+str(tool))
 require(sha(a.memory_object)=='0a12d6b58d5fc714e6883772599f29c8b02a8eb8a78d6e90eafbd4f3982e2255','Actual recovered freestanding memory input required')
 require(struct.unpack_from('<H',a.memory_object.read_bytes())[0]==0x1c4,'ARMNT memory object required')
 client=root/'drivers/gpu/mt8167/wddm-render/testclient';require(sha(client/'memory.c')=='3eb20e9c612fe2e5515f2c8e55d57b47dbb0113d8f9d579684b41eb629b6fe94','Changed memory source requires a separate explicit decision')
 source=client/('dxgi-present356.c' if a.phase=='evaluator' else 'dxgi-console-launch363.c');name='mt8167-dxgi-present356.exe' if a.phase=='evaluator' else 'mt8167-dxgi-console-launch363.exe'
 evaluator_sha=None;evaluator_source=None
 if a.phase=='launcher':
  require(a.evaluator and a.evaluator_receipt and a.evaluator_receipt_sha256,'Actual signed evaluator and pinned first-stage receipt required')
  require(sha(a.evaluator_receipt)==a.evaluator_receipt_sha256,'Evaluator receipt SHA')
  e=read(a.evaluator_receipt);evaluator_sha=sha(a.evaluator);evaluator_source=e['runtime_source_commit']
  require(e['success'] and e['phase']=='evaluator' and e['signed_sha256']==evaluator_sha and e['signature']['status']=='Valid','Actual successful first stage required')
  require(sha(client/'dxgi-present356.c')==e['runtime_source_sha256'],'Evaluator source changed between formal commits')
  for rel,h in e['repository_include_closure'].items():require(sha(root/rel)==h,'Evaluator include changed between commits '+rel)
  m=re.search(r'static const BYTE expected\[32\]\s*=\s*\{([^}]+)\}',source.read_text());require(m is not None,'Fixed launcher digest declaration required')
  values=re.findall(r'0x([0-9a-fA-F]{2})',m[1]);require(len(values)==32 and ''.join(values).lower()==evaluator_sha,'Launcher expected must contain actual signed evaluator SHA in this formal Git source commit')
 libs=['kernel32','user32','d3d11','dxgi','setupapi'] if a.phase=='evaluator' else ['kernel32','advapi32','bcrypt']
 libraries=[a.arm_libraries/(n+'.lib') for n in libs];library_pins={str(x.resolve()):sha(x) for x in libraries}
 out=a.output.resolve();prior=None
 if out.exists():
  require(a.resume_compile_receipt_sha256 and a.resume_object_sha256 and a.resume_dependency_sha256,'Existing output requires explicit actual compiled receipt/object/original dependency SHA pins')
  require(sha(out/'receipt.json')==a.resume_compile_receipt_sha256 and sha(out/'client.obj')==a.resume_object_sha256 and sha(out/'client-original.d')==a.resume_dependency_sha256,'Actual completed compile input SHA mismatch')
  prior=read(out/'receipt.json');require(not prior['success'] and prior['phase']==a.phase and prior['runtime_source_commit']==a.source_commit and prior['runtime_source_sha256']==sha(source),'Wrong completed compile provenance')
  require(len(prior['commands'])==1 and prior['commands'][0]['exit_code']==0 and prior['commands'][0]['log']=='logs/compile.log','Exactly one real completed compile before link required')
  require(sha(out/'logs/compile.log')==prior['commands'][0]['sha256'] and not (out/'unsigned.exe').exists() and not (out/name).exists(),'Original compile log and prelink-only checkpoint required')
 else:out.mkdir(parents=True);(out/'logs').mkdir()
 env=dict(os.environ,TEMP=str(out/'logs'),TMP=str(out/'logs'),VSLANG='1033')
 receipt=dict(schema='ge8300-one-TU-client-pair-v1',phase=a.phase,recipe_source_commit=recipe,runtime_source_commit=a.source_commit,runtime_source_sha256=sha(source),compiled_TUs=1,memory_object_sha256=sha(a.memory_object),memory_object_recompiled=0,memory_historical_include_closure_recorded=False,memory_input_kind='SHA-fixed recovered 356r2/363 freestanding prebuilt object',SDK_rebuilt=0,GPU_rebuilt=0,PCO_rebuilt=0,driver_package_modified=False,hardware_verified=False,evaluator_sha256=evaluator_sha,evaluator_source_commit=evaluator_source,library_inputs_current_prelink_hashes=library_pins,commands=[],success=False)
 def run(args,label):
  start=time.monotonic();q=subprocess.run(list(map(str,args)),cwd=root,env=env,capture_output=True,text=True,errors='replace');log=out/'logs'/(label+'.log');log.write_text(q.stdout+q.stderr)
  receipt['commands'].append(dict(arguments=list(map(str,args)),exit_code=q.returncode,seconds=time.monotonic()-start,log=log.relative_to(out).as_posix(),sha256=sha(log)));require(q.returncode==0,'Failed '+label+'; preserve output and do not recompile blindly');return q.stdout
 try:
  flags=['--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access','-D_ARM_','-DARM','-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-I'+str(a.um_include/'shared'),'-I'+str(a.um_include/'um'),'-Werror=incompatible-pointer-types','-Werror=implicit-function-declaration']
  if a.phase=='evaluator':flags+=['-fms-extensions','-DD3D_UMD_INTERFACE_VERSION=0x4002','-DDXGKDDI_INTERFACE_VERSION=0x4002','-I'+str(a.km_include),'-D__ldrexd=__geo_typed_wdk81_ldrexd_declaration']
  obj=out/'client.obj';dep=out/'client-original.d'
  compile_command=list(map(str,[a.clang,*flags,'-MD','-MF',dep,'-MT','ge8300-unit','-c',source,'-o',obj]))
  if prior:
   require(prior['commands'][0]['arguments']==compile_command,'Original actual compile arguments changed')
   require(prior['memory_object_sha256']==sha(a.memory_object),'Retained helper input changed')
   normalize=lambda rows:{str(Path(k).resolve()).casefold():v for k,v in rows.items()}
   require(normalize(prior['library_inputs_current_prelink_hashes'])==normalize(library_pins),'Actual planned link inputs changed since compile')
   shutil.copyfile(out/'receipt.json',out/'compile-complete-original-receipt.json')
   receipt.update(commands=prior['commands'].copy(),additional_compiles=0,compile_recipe_source_commit=prior['recipe_source_commit'],compile_complete_original_receipt_sha256=a.resume_compile_receipt_sha256,retained_actual_object_sha256=a.resume_object_sha256,retained_original_dependency_sha256=a.resume_dependency_sha256)
  else:run(compile_command,'compile');receipt['additional_compiles']=1
  headers=[Path(x).resolve(strict=True) for x in dependencies(dep)];require(headers,'Compile-time dependency trace required')
  receipt['compile_include_closure']={str(x):sha(x) for x in headers};receipt['repository_include_closure']={x.relative_to(root).as_posix():sha(x) for x in headers if x.is_relative_to(root)};receipt['original_dependency_sha256']=sha(dep);receipt['object_sha256']=sha(obj)
  unsigned=out/'unsigned.exe';run([a.linker,'/nologo','/machine:arm','/subsystem:console,6.03','/entry:mainCRTStartup','/nodefaultlib','/largeaddressaware','/tsaware:no',obj,a.memory_object,*libraries,'/out:'+str(unsigned)],'link')
  raw=unsigned.read_bytes();info=pe(raw);offset=struct.unpack_from('<I',raw,60)[0]+24;require(struct.unpack_from('<HH',raw,offset+48)==(6,3),'RT8.1 subsystem6.3')
  signed=out/name;shutil.copyfile(unsigned,signed)
  run([a.signtool,'sign','/fd','SHA256','/s','My','/sha1','BCBF6C456C6F0DD0818434763B189488FBF13570',signed],'sign');unchanged_signed_payload(raw,signed.read_bytes())
  run([a.signtool,'verify','/pa','/v',signed],'trust')
  run([tools['powershell'][0],'-NoProfile','-NonInteractive','-File',RECIPE/'tools/ci/verify-client-pair-signature.ps1','-Image',signed,'-Report',out/'signature.json'],'certificate')
  signature=read(out/'signature.json');require(signature['sha256']==sha(signed),'Actual signature file hash binding')
  fixed(root,a.source_commit);fixed(RECIPE,recipe)
  for f,h in library_pins.items():require(sha(f)==h,'Live library input changed')
  for tool,h in tools.values():require(sha(tool)==h,'Live tool input changed')
  require(sha(source)==receipt['runtime_source_sha256'] and sha(a.memory_object)==receipt['memory_object_sha256'],'Runtime source or retained object changed')
  if a.phase=='launcher':require(sha(a.evaluator)==evaluator_sha,'Signed evaluator changed');shutil.copyfile(a.evaluator,out/'mt8167-dxgi-present356.exe')
  receipt.update(success=True,signed_sha256=sha(signed),unsigned_sha256=sha(unsigned),signature=signature,non_signature_payload_unchanged=True,outputs={x.name:sha(x) for x in out.glob('*.exe') if x.name!='unsigned.exe'},launcher_formal_commit_required_next=(a.phase=='evaluator'))
 finally:
  (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
  with zipfile.ZipFile(out.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as z:
   for f in out.rglob('*'):
    if f.is_file():z.write(f,f.relative_to(out).as_posix())
 print(json.dumps({k:receipt[k] for k in ['phase','runtime_source_commit','signed_sha256','success','launcher_formal_commit_required_next']}))
if __name__=='__main__':main()
