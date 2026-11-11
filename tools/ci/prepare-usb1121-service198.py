#!/usr/bin/env python3
"""main用の固定USB1121 SCM入力を生成。Windows/端末操作なし。"""
import argparse,hashlib,importlib.util,json,re,shutil,zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('fresh198',HERE/'prepare-usb1121-fresh198-bundle.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
ZIP='e37b36fc550b170b3d233cd7585121f72df309f7e29f7325cd67c2566829e6b1'
MANIFEST='656ab72fda2cd79518aa47ddabdcd008db86673c675e83ccbb7875073b501064'
DLL='b0c3585525ab406218e03eaa06a3e453db4c92796898baa72bd7bc54786474ba'
SERVICE='MT8167Usb1121Install198'
def signed_outputs(root, manifest_path):
 gm=b.read(manifest_path)
 b.require(gm.get('signed') is True and gm.get('success') is True and gm.get('signer_thumbprint')=='BCBF6C456C6F0DD0818434763B189488FBF13570','実署名済みGPU manifestが必要')
 outputs={x['name']:x['sha256'] for x in gm['outputs']}
 b.require(outputs and len(outputs)==len(gm['outputs']),'GPU outputの空集合または重複')
 for n,h in outputs.items():
  q=b.below(root,manifest_path.parent/n)
  b.require(q.is_relative_to(manifest_path.parent) and b.sha(q)==h,'GPU署名leaf drift')
 return gm,outputs

def bind_gpu(root, policy, manifest_path, manifest_pin):
 """版数はpolicy/実INFから導出。renderとretained transportの出自を別々に検証。"""
 b.require(b.sha(manifest_path)==manifest_pin,'GPU manifest pin drift')
 gm,outputs=signed_outputs(root,manifest_path)
 render=policy['packages']['render']
 b.require(render['manifest_sha256']==manifest_pin and
           b.sha(b.below(root,render['manifest']))==manifest_pin and
           re.fullmatch(r'[0-9a-f]{40}',gm.get('source_commit','')) and
           render['source_commit']==gm['source_commit'] and
           render.get('binary_source_commit',gm['source_commit'])==gm['source_commit'],
           'render manifest/sourceの出自不一致')
 version=render['driver_version']
 b.require(isinstance(version,str) and re.fullmatch(r'(?:[0-9]+\.){3}[0-9]+',version),'GPU version不正')
 retained={}
 for role in ('render','display'):
  entry=policy['packages'][role];directory=b.below(root,entry['directory'])
  mp=b.below(root,entry['manifest'])
  b.require(b.sha(mp)==entry['manifest_sha256'],'GPU package manifest pin drift')
  package,leaves=signed_outputs(root,mp)
  source=package.get('source_commit') if role=='render' else package.get('new_display_source_commit',package.get('source_commit'))
  b.require(source==entry['source_commit'] and re.fullmatch(r'[0-9a-f]{40}',source or ''),'GPU package source不一致')
  b.require(set(entry['names'])=={'inf','sys','cat'} and set(entry['hashes'])=={'inf','sys','cat'},'GPU trio不完全')
  for kind,n in entry['names'].items():
   q=b.below(root,directory/n)
   # retained transportは別directoryにあっても、最終manifest内の同leaf byteへ結合する。
   matches=[h for name,h in outputs.items() if Path(name).name==q.name]
   b.require(q.is_relative_to(directory) and b.sha(q)==entry['hashes'][kind] and
             matches==[entry['hashes'][kind]] and
             leaves.get(q.relative_to(mp.parent).as_posix())==entry['hashes'][kind],
             '最終policyと実署名manifestのGPU leaf不一致')
   if kind=='sys':retained['Windows/System32/Drivers/'+q.name]=dict(path=q.relative_to(root).as_posix(),sha256=entry['hashes'][kind])
  raw=b.below(root,directory/entry['names']['inf']).read_bytes()
  inf=raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
  versions=re.findall(r'^\s*DriverVer\s*=\s*[^,;]+,\s*([0-9.]+)\s*(?:;[^\r\n]*)?$',inf,re.M|re.I)
  b.require(versions==[entry['driver_version']],'実GPU INF DriverVer不一致')
 for e in render['extra_members']:
  q=b.below(root,Path(render['directory'])/e['name'])
  b.require(b.sha(q)==e['sha256']==outputs.get(q.relative_to(manifest_path.parent).as_posix()),'GPU extra member不一致')
  installed=Path(e['installed'].replace('\\','/'))
  b.require(not installed.is_absolute() and '..' not in installed.parts,'GPU installed path不正')
  retained['Windows/System32/'+installed.as_posix()]=dict(path=q.relative_to(root).as_posix(),sha256=e['sha256'])
 return dict(target_gpu_version=version,integration_phase='after-final-gpu-before-final-flush',
             final_gpu_source_commit=gm['source_commit']),retained


def captured_gpu_sources(root, fresh_path, fresh_pin, gpu_sources, supplement=None, supplement_pin=None):
 fm=b.read(fresh_path);captured={x['source_path']:(fresh_path,x) for x in fm['files']}
 if supplement is not None:
  sp=b.below(root,supplement);b.require(supplement_pin and b.sha(sp)==supplement_pin,'GPU補足capture pin不一致')
  extra=b.read(sp)
  b.require(extra.get('source_manifest_sha256')==fresh_pin and extra.get('recovery_boot_id')==fm['recovery_boot_id'] and extra.get('status')=='captured-unmounted' and extra.get('read_only') is True and extra.get('unmounted') is True and extra.get('errors')==[],'同一freshのGPU補足captureが必要')
  for x in extra['files']:
   name=x['source_path']
   b.require(name not in captured or captured[name][1]['sha256']==x['sha256'],'GPU補足captureの競合')
   captured[name]=(sp,x)
 result={}
 for name,entry in gpu_sources.items():
  b.require(name in captured,'現在GPUの実capture witness不足: '+name)
  mp,f=captured[name];q=b.below(root,mp.parent/f['backup_path'])
  b.require(b.sha(q)==f['sha256']==entry['sha256'],'保持GPUと現在captureのbyte不一致')
  result[name]=dict(path=q.relative_to(root).as_posix(),sha256=f['sha256'])
 return result

def prepare(root,out,*,fresh_manifest=None,fresh_sha256=None,native_receipt=None,native_sha256=None,final_policy=None,final_policy_sha256=None,gpu_manifest=None,gpu_manifest_sha256=None,retain_current_gpu=False,gpu_capture_manifest=None,gpu_capture_sha256=None):
 root=root.resolve(strict=True);out=out.resolve();rebound=fresh_manifest is not None
 if rebound:
  b.require(all((fresh_sha256,native_receipt,native_sha256,final_policy,final_policy_sha256,gpu_manifest,gpu_manifest_sha256)),'final実入力の全pinが必要')
  # Fail before producing any bundle/profile when main has not sealed final inputs.
  fp=b.below(root,fresh_manifest);np=b.below(root,native_receipt);pp=b.below(root,final_policy);gp0=b.below(root,gpu_manifest)
  b.require(b.sha(fp)==fresh_sha256 and b.sha(np)==native_sha256 and b.sha(pp)==final_policy_sha256 and b.sha(gp0)==gpu_manifest_sha256,'final source pin drift')
  final=b.read(pp)
  comparison=b.admit_native(root,fp,fresh_sha256,np,native_sha256,final)
  gpu_binding,gpu_retained=bind_gpu(root,final,gp0,gpu_manifest_sha256)
  if retain_current_gpu:
   gpu_retained=captured_gpu_sources(root,fp,fresh_sha256,gpu_retained,gpu_capture_manifest,gpu_capture_sha256)
   original={x['source_path']:x for x in b.read(fp)['files']}
   b.require('Windows/System32/'+SERVICE+'.exe' in original,'existing USB EXEの実baseline captureが必要')
   gpu_binding.update(integration_phase='standalone-retained-gpu',gpu_update=False,current_gpu_capture_verified=True,preserved_gpu_witnesses=gpu_retained)
 b.require(not retain_current_gpu or rebound,'current standaloneには実captureの再結合が必要')
 b.require(not (gpu_capture_manifest or gpu_capture_sha256) or retain_current_gpu,'GPU補足captureはstandalone専用')
 b.require(out.is_relative_to(root) and not out.exists(),'root内新規出力必須')
 zpath=b.below(root,'port/xnu/out/windows-rt-usb1121-helper42796448681/usb1121-helper-signed.zip');b.require(b.sha(zpath)==ZIP,'signed ZIP drift')
 with zipfile.ZipFile(zpath) as z:
  b.require(z.testzip() is None and set(z.namelist())=={'MT8167UsbAbi3Pnp.dll','helper-manifest.json','build-plan.json','build-receipt.json'},'signed ZIP members/CRC')
  mh=hashlib.sha256(z.read('helper-manifest.json')).hexdigest();dh=hashlib.sha256(z.read('MT8167UsbAbi3Pnp.dll')).hexdigest()
  b.require(mh==MANIFEST and dh==DLL,'signed helper drift')
  m=json.loads(z.read('helper-manifest.json'));b.require(m['frozen_helper_source_commit']=='90e4132b7d7e62e9629d1bd768e5c09ad063c517' and m['fixed_recipe_source_commit']=='1a753dccd604641e767e03748418c8a5c5c9d352' and m['certificate_status']=='Valid','helper provenance')
  plan=b.prepare(root,out/'payload/bundle',**(dict(fresh_manifest=fresh_manifest,fresh_sha256=fresh_sha256,native_receipt=native_receipt,native_sha256=native_sha256,final_policy=final_policy,final_policy_sha256=final_policy_sha256) if rebound else {}))
  hp=out/'payload/helper';hp.mkdir()
  for name in z.namelist():(hp/name).write_bytes(z.read(name))
 b.require(m['source_sha256']==plan['files']['recipe/UsbAbi3Pnp.cs'],'unchanged C# source')
 payload=out/'payload';(payload/'logs').mkdir();launch=HERE.parents[1]/'tools/usb/run-usb1121-service.ps1';shutil.copyfile(launch,payload/launch.name)
 fm_path=b.below(root,fresh_manifest or 'port/xnu/out/private-windows-rt-prebackup-run198/manifest.json');fm=b.read(fm_path)
 fresh={x['source_path']:x for x in fm['files']}
 # All captured originals are checked offline; only SYSTEM will be replaced in media.
 for f in fm['files']:b.require(b.sha(b.below(root,fm_path.parent/f['backup_path']))==f['sha256'],'fresh original drift')
 # Baseline originals and target GPU are distinct provenance, never relabel old receipts.
 retained={k:dict(path=(fm_path.parent/f['backup_path']).relative_to(root).as_posix(),sha256=f['sha256']) for k,f in fresh.items() if k.startswith('Windows/System32/') and '/config/' not in k and not k.endswith('.log') and k != 'Windows/System32/'+SERVICE+'.exe' and not k.startswith('Windows/System32/'+SERVICE+'/')}
 if rebound:
  policy_path=b.below(root,final_policy);gp=b.below(root,gpu_manifest)
  b.require(b.sha(policy_path)==final_policy_sha256 and b.sha(gp)==gpu_manifest_sha256,'final policy/GPU manifest drift')
  prior=b.read(policy_path)
  b.require(prior.get('draft') is False and prior.get('normal_install_allowed') is True and prior['backup_manifest_sha256']==fresh_sha256 and prior['recovery_boot_id']==fm['recovery_boot_id'],'final same-fresh deployable GPU policy required; pending/template不可')
  retained.update(gpu_retained)
 else:
  prior=b.read(b.below(root,'port/xnu/out/windows-rt-combined197-fresh196/service/peripheral-policy.json'))
  b.require(fresh['Windows/System32/Drivers/ge8300-render.sys']['sha256']==prior['packages']['render']['hashes']['sys'],'actual historical SYS367 mismatch')
 for e in ([] if rebound else prior['packages']['render']['extra_members']):
  name='Windows/System32/'+e['installed'].replace('\\','/');q=b.below(root,Path(prior['packages']['render']['directory'])/e['name']);b.require(b.sha(q)==e['sha256'],'GPU extra member source drift')
  retained[name]=dict(path=q.relative_to(root).as_posix(),sha256=e['sha256'])

 files={p.relative_to(payload).as_posix():b.sha(p) for p in payload.rglob('*') if p.is_file()}
 profile=dict(schema='MT8167_USB1121_FRESH198_SERVICE_V1',service=SERVICE,fresh_sha256=(fresh_sha256 if rebound else b.FRESH),recovery_boot_id=fm['recovery_boot_id'],native_receipt_sha256=(native_sha256 if rebound else b.NATIVE),plan_sha256=b.sha(payload/'bundle/plan.json'),helper_manifest_sha256=MANIFEST,helper_build_plan_sha256=m['install_plan_sha256'],helper_build_plan_is_historical_only=True,files=files,retained_files={k:v['sha256'] for k,v in retained.items() if Path(k).name != Path(plan['rollback']['parent']['names']['sys']).name},retained_sources=retained,source_ON=False,force_host=False,root_PDO_creation=False,driver_rebuilt=False,helper_rebuilt=False,SDK_rebuilt=False,boot_GPT_changes=0,old_service_changes=0,POST_value=0,hardware_applied=False,power_verified=False)
 if rebound:
  profile.update(schema='MT8167_USB1121_REBOUND_SERVICE_V2',final_policy_sha256=final_policy_sha256,final_gpu_manifest_sha256=gpu_manifest_sha256,**gpu_binding,activation='main existing combined phase only; no automatic native activation')
  profile['preservation_comparison']=comparison
 (payload/'profile.json').write_text(json.dumps(profile,indent=2)+'\n')
 header='/* Generated exact input pins, no runtime role override. */\n'
 for name,h in [('launcher',b.sha(payload/launch.name)),('profile',b.sha(payload/'profile.json'))]:header+='static const BYTE '+name+'_hash[32]={'+','.join('0x'+h[i:i+2] for i in range(0,64,2))+'};\n'
 (out/'usb1121-service-pins.h').write_text(header)
 for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):
  f=fresh['Windows/System32/config/'+n];d=out/'fresh-system'/n;d.parent.mkdir(exist_ok=True);shutil.copyfile(b.below(root,fm_path.parent/f['backup_path']),d)
 if retain_current_gpu:
  prov=out/'provenance';prov.mkdir()
  shutil.copyfile(fp,prov/'fresh-manifest.json');shutil.copyfile(np,prov/'native-receipt.json');shutil.copyfile(pp,prov/'final-policy.json')
  if gpu_capture_manifest:shutil.copyfile(b.below(root,gpu_capture_manifest),prov/'gpu-capture-manifest.json')
 closure={p.relative_to(out).as_posix():b.sha(p) for p in out.rglob('*') if p.is_file()}
 (out/'input-closure.json').write_text(json.dumps(dict(schema='MT8167_USB1121_SERVICE_INPUTS_V1',files=closure,profile_sha256=b.sha(payload/'profile.json'),fresh_sha256=profile['fresh_sha256'],native_sha256=profile['native_receipt_sha256'],binding_schema=profile['schema'],final_policy_sha256=final_policy_sha256,final_gpu_manifest_sha256=gpu_manifest_sha256,integration_phase=profile.get('integration_phase'),target_gpu_version=profile.get('target_gpu_version'),final_gpu_source_commit=profile.get('final_gpu_source_commit'),gpu_update=profile.get('gpu_update'),gpu_capture_sha256=gpu_capture_sha256,signed_helper_zip_sha256=ZIP),indent=2)+'\n')
 return profile
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
 for n in ('fresh-manifest','native-receipt','final-policy','gpu-manifest'):ap.add_argument('--'+n,type=Path)
 for n in ('fresh-sha256','native-sha256','final-policy-sha256','gpu-manifest-sha256'):ap.add_argument('--'+n)
 ap.add_argument('--retain-current-gpu',action='store_true')
 ap.add_argument('--gpu-capture-manifest',type=Path);ap.add_argument('--gpu-capture-sha256')
 ap.add_argument('--native-admission-only',action='store_true',help='実fresh/nativeと確定policyのみ比較。配備入力を生成しない')
 a=ap.parse_args();values={k:v for k,v in vars(a).items() if k not in ('workspace','out','native_admission_only','retain_current_gpu','gpu_capture_manifest','gpu_capture_sha256')}
 if a.native_admission_only:
  b.require(not a.retain_current_gpu and not a.gpu_capture_manifest and not a.gpu_capture_sha256,'admission-onlyは配備modeと混在不可')
  b.require(all(values[k] for k in ('fresh_manifest','fresh_sha256','native_receipt','native_sha256','final_policy','final_policy_sha256')) and not a.gpu_manifest and not a.gpu_manifest_sha256,'native admissionには6 pinのみ必要')
  root=a.workspace.resolve(strict=True);pp=b.below(root,a.final_policy)
  b.require(b.sha(pp)==a.final_policy_sha256,'policy pin drift')
  result=b.admit_native(root,b.below(root,a.fresh_manifest),a.fresh_sha256,b.below(root,a.native_receipt),a.native_sha256,b.read(pp))
  result.update(deployment_inputs_generated=False,final_policy_sha256=a.final_policy_sha256,fresh_sha256=a.fresh_sha256,native_receipt_sha256=a.native_sha256)
  out=a.out.resolve();b.require(out.is_relative_to(root) and not out.exists(),'root内新規出力必須')
  out.mkdir(parents=True);(out/'native-admission.json').write_text(json.dumps(result,indent=2)+'\n')
  print('USB1121_NATIVE_POLICY_ADMISSION_ONLY_NO_DEPLOYMENT');raise SystemExit(0)
 b.require(not any(values.values()) or all(values.values()),'再結合入力8項目は全指定必須')
 b.require(bool(a.gpu_capture_manifest)==bool(a.gpu_capture_sha256),'GPU補足captureはpath/hash両方必要')
 prepare(a.workspace,a.out,**values,retain_current_gpu=a.retain_current_gpu,gpu_capture_manifest=a.gpu_capture_manifest,gpu_capture_sha256=a.gpu_capture_sha256);print('USB1121_EXACT_INPUTS_PREPARED_NO_HARDWARE')
