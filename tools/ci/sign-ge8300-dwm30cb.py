#!/usr/bin/env python3
"""Sign source30cb DWM-only outputs, not a timing369 deployment package."""
import argparse,datetime,hashlib,json,os,re,shutil,subprocess,zipfile
from pathlib import Path
from ge8300_panel_signing_common import require,decode_inf,unchanged_signed_payload
ROOT=Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def pin(p,h):require(sha(p)==h.lower(),'Hash mismatch: '+str(p))
def inf370(raw,old):
 text,encoding,bom=decode_inf(raw);text=text.replace('\r\n','\n');require('\r' not in text,'INF line endings')
 rows=list(re.finditer(r'(?m)^DriverVer=([^\n]+)$',text));require(len(rows)==1 and rows[0][1]==old,'Baseline INF identity')
 a,b=rows[0].span(1);date=datetime.datetime.now().strftime('%m/%d/%Y');return bom+(text[:a]+date+',0.0.3.70'+text[b:]).replace('\n','\r\n').encode(encoding)
def main():
 p=argparse.ArgumentParser();p.add_argument('--enable-candidate',action='store_true')
 for key in ['baseline-package','linked-directory','output','sign-tool','inf2cat']:p.add_argument('--'+key,type=Path,required=True)
 for key in ['build-manifest-sha256','sign-tool-sha256','inf2cat-sha256','powershell-sha256']:p.add_argument('--'+key,required=True)
 a=p.parse_args();require(a.enable_candidate and os.name=='nt','Explicit Windows signing required')
 require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']),'Clean signing source required')
 recipe=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip();spec=read(Path(__file__).with_name('ge8300-dwm30cb-signing-pins.json'))
 for name,h in spec['helper_hashes'].items():pin(Path(__file__).with_name(name),h)
 base=a.baseline_package.resolve();linked=a.linked_directory.resolve();out=a.output.resolve();compiled_tree=linked.parents[1]
 pin(base/'signed-manifest.json',spec['baseline_signed_manifest_sha256'])
 for name,h in spec['baseline_files'].items():pin(base/name,h)
 buildfile=linked/'source-build-manifest.json';pin(buildfile,a.build_manifest_sha256);build=read(buildfile)
 require(build['source_commit']==spec['build_source_commit'] and not build['source_dirty'] and build['SDK_rebuilt']==0 and build['compiler375_cache_reused'],'Wrong fullsource build provenance')
 require(subprocess.check_output(['git','-C',str(compiled_tree),'rev-parse','HEAD'],text=True).strip()==spec['build_source_commit'] and not subprocess.check_output(['git','-C',str(compiled_tree),'status','--porcelain']),'Compiled tree changed')
 # Exactly the actual seven-output manifest; no guessed success or source tuple.
 expected={'ge8300-render.sys','mt8167-display-transport.sys','ge8300-umd.dll','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','ge8300-d3d-texture-boundary.exe','ge8300-d3d-ui-texture.exe'}
 require({row['name'] for row in build['outputs']}==expected,'Build output set')
 for row in build['outputs']:pin(linked/row['name'],row['sha256'])
 for key,name in [('native_build_manifest_sha256','native/build-manifest.json'),('KMD_build_manifest_sha256','modules/build-manifest.json'),('UMD_build_manifest_sha256','umd/build-manifest.json'),('kernel_link_manifest_sha256','kernel-link.json'),('transport_link_manifest_sha256','transport-link.json')]:pin(linked/name,build[key])
 powershell=Path(shutil.which('powershell') or '');pin(a.sign_tool,a.sign_tool_sha256);pin(a.inf2cat,a.inf2cat_sha256);pin(powershell,a.powershell_sha256)
 require(not out.exists() and out.is_relative_to(ROOT),'Fresh signing-source-local output required')
 out.mkdir();(out/'evidence').mkdir();package=out/'package370-dwm-source';package.mkdir()
 receipt={'schema':'ge8300-dwm30cb-sign-only-v1','recipe_source_commit':recipe,'compiled_source_commit':spec['build_source_commit'],'input_build_manifest_sha256':sha(buildfile),'baseline_signed_manifest_sha256':sha(base/'signed-manifest.json'),'signer_thumbprint':spec['signer_thumbprint'],'compile_count':0,'link_count':0,'SDK_rebuilt':0,'Mesa365_recompiled':0,'timing369_consumer_included':False,'deployable_with_timing369_profile':False,'hardware_verified':False,'DWM_verified':False,'deployment_performed':False,'success':False,'receipts':[]}
 def run(args,name):
  result=subprocess.run(list(map(str,args)),capture_output=True,text=True,errors='replace');log=out/'evidence'/name;log.write_text(result.stdout+result.stderr)
  receipt['receipts'].append({'arguments':list(map(str,args)),'exit_code':result.returncode,'log':log.relative_to(out).as_posix(),'sha256':sha(log)});require(result.returncode==0,'Command failed: '+name)
 try:
  for name in spec['baseline_files']:
   dest=package/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(base/name,dest)
  transforms=[];signed=[]
  for name,relative in [('ge8300-render.sys','ge8300-render.sys'),('mt8167-display-transport.sys','display-transport/mt8167-display-transport.sys'),('ge8300-umd.dll','ge8300-umd.dll')]:
   raw=(linked/name).read_bytes();dest=package/relative;dest.write_bytes(raw)
   run([a.sign_tool,'sign','/fd','SHA256','/s','My','/sha1',spec['signer_thumbprint'],dest],name+'.sign.log');unchanged_signed_payload(raw,dest.read_bytes());signed.append(dest)
   transforms.append({'name':relative,'unsigned_sha256':hashlib.sha256(raw).hexdigest(),'signed_sha256':sha(dest),'non_signature_payload_unchanged':True})
  # Actual newly linked import library, never silently retain a stale UMD pair.
  importlib=linked/'umd/ge8300-umd.lib';require(importlib.is_file(),'New UMD import library absent');libhash=sha(importlib);shutil.copyfile(importlib,package/'ge8300-umd.lib')
  for name,old in [('ge8300-render.inf','11/10/2026,0.0.3.68'),('display-transport/mt8167-display-transport.inf','11/09/2026,0.0.3.57')]: (package/name).write_bytes(inf370((base/name).read_bytes(),old))
  for cat in package.rglob('*.cat'):cat.unlink()
  temporary=out/'transport-catalog-work';(package/'display-transport').rename(temporary)
  try:run([a.inf2cat,'/driver:'+str(package),'/os:6_3_ARM','/uselocaltime'],'render.inf2cat.log')
  finally:temporary.rename(package/'display-transport')
  run([a.inf2cat,'/driver:'+str(package/'display-transport'),'/os:6_3_ARM','/uselocaltime'],'transport.inf2cat.log')
  for name in ['ge8300-render.cat','display-transport/mt8167-display-transport.cat']:
   path=package/name;run([a.sign_tool,'sign','/fd','SHA256','/s','My','/sha1',spec['signer_thumbprint'],path],Path(name).name+'.sign.log');signed.append(path)
  for i,path in enumerate(signed):run([a.sign_tool,'verify','/pa','/v',path],str(i)+'.trust.log')
  for folder,cat,names in [('', 'ge8300-render.cat',['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']),('display-transport','mt8167-display-transport.cat',['mt8167-display-transport.inf','mt8167-display-transport.sys'])]:
   for name in names:run([a.sign_tool,'verify','/pa','/v','/c',package/folder/cat,package/folder/name],name+'.catalog.log')
  paths=out/'evidence/signature-paths.json';paths.write_text(json.dumps(list(map(str,signed)))+'\n')
  run([powershell,'-NoProfile','-NonInteractive','-File',ROOT/'tools/ci/verify-dwm-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',spec['signer_thumbprint'],'-ReportJson',out/'evidence/certificate-report.json'],'certificate-identity.log')
  certificates=read(out/'evidence/certificate-report.json')['files'];require(len(certificates)==5 and {r['path'] for r in certificates}==set(map(str,signed)),'Five individual trust receipts required')
  changed={'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat','ge8300-umd.dll','ge8300-umd.lib','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.inf','display-transport/mt8167-display-transport.cat'}
  actual={f.relative_to(package).as_posix():sha(f) for f in package.rglob('*') if f.is_file()};require(set(actual)==set(spec['baseline_files']),'Package file set changed')
  for name,h in actual.items():
   if name not in changed:require(h==spec['baseline_files'][name],'Retained FW/client/certificate changed: '+name)
  pin(importlib,libhash);pin(buildfile,a.build_manifest_sha256)
  for name,h in spec['baseline_files'].items():pin(base/name,h)
  require(subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()==recipe and not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']),'Signing source changed')
  require(subprocess.check_output(['git','-C',str(compiled_tree),'rev-parse','HEAD'],text=True).strip()==spec['build_source_commit'] and not subprocess.check_output(['git','-C',str(compiled_tree),'status','--porcelain']),'Compile source changed')
  for row in build['outputs']:pin(linked/row['name'],row['sha256'])
  receipt.update(success=True,binary_signing_transforms=transforms,UMD_import_library_sha256=libhash,retained_FW_clients_certificate=True,outputs=[{'name':n,'sha256':h} for n,h in sorted(actual.items())])
  (package/'signed-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
 except BaseException as error:receipt['error']=str(error);raise
 finally:
  (out/'signing-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
  with zipfile.ZipFile(out.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as archive:
   for file in out.rglob('*'):
    if file.is_file():archive.write(file,file.relative_to(out))
 print('DWM_SOURCE30CB_SIGNED_NOT_DEPLOYED_NOT_HARDWARE_SUCCESS')
if __name__=='__main__':main()
