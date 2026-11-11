#!/usr/bin/env python3
"""実captureの生値と製品validatorをhost上で検証。manifest/payload/native hiveを生成しない。"""
import argparse,ast,copy,importlib.util,json,socket,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
root=a.workspace.resolve();repo=Path(__file__).resolve().parents[1];out=a.out.resolve();assert out.is_relative_to(root) and not out.exists();out.mkdir()
sp=importlib.util.spec_from_file_location('media',repo/'tools/winpe/build-usb1121-fresh198-media.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m);b=m.b
np=root/'port/gsi/logs/ios-feasibility/windows-rt-post233-native/main-recovered/out233/receipt.json';r=b.read(np);pin=b.sha(np)
raw={k.rsplit('\\',1)[1]:v for k,v in r['captured_named_values'].items() if 'MT8167Usb1121Install198' in k};after=copy.deepcopy(raw);after['Start']['raw_hex']='02000000'
results=[]
def check(name,fn,reject=False):
 try:v=fn()
 except (ValueError,KeyError,FileNotFoundError,TypeError) as e:
  if not reject:raise
  results.append(dict(name=name,status='passed',rejected=True,reason=str(e)));return
 if reject:raise AssertionError(name)
 results.append(dict(name=name,status='passed'))
check('actual233-raw-start4-to2-contract',lambda:m.verify_rearm_raw(raw,after))
for field in raw:
 for target in ['type','bytes','raw_hex']:
  before=copy.deepcopy(raw);before[field][target]=3 if target=='type' else 1 if target=='bytes' else 'ff'
  check('reject-before-'+field+'-'+target,lambda:m.verify_rearm_raw(before,after),True)
check('reject-unrelated-value-deletion',lambda:m.verify_rearm_raw(dict(raw,Extra={'type':4,'bytes':4,'raw_hex':'00000000'}),after),True)
# validator専用のメモリ上のcontract fixture。native実行receiptや署名manifestとして保存しない。
expected=dict(source_commit='source-identity',fresh_sha256=r['source_manifest_sha256'],recovery_boot_id=r['recovery_boot_id'],input_closure_sha256='closure-pin',profile_sha256='profile-pin',native_receipt_sha256=pin,native_helper_sha256=b.sha(repo/'tools/recovery/Usb1121OfflineHiveNative.cs'),native_recipe_sha256=b.sha(repo/'tools/recovery/prepare-usb1121-system198.ps1'))
proof=dict(expected,schema='MT8167_USB1121_NATIVE_REARM_V2',mode='existing-disabled-start-only',service_created=False,existing_service=m.u.SERVICE,preservation_exclusion=r'ControlSet001\Services\MT8167Usb1121Install198:Start only',raw_before=raw,raw_after=after)
for key in ('old_services_and_GraphicsDrivers_equal','originals_unchanged','snapshot_after_equal','snapshot_reloaded_equal','snapshot_security_included','subkeys_preserved','service_other_values_preserved','native_patch_complete'):proof[key]=True
for key in ('load_rc','reload_rc','unload_rc','privilege_restore_error','ACL_changes','boot_GPT_changes','POST_value'):proof[key]=0
check('native-rearm-evidence-validator-contract',lambda:m.verify_rearm_evidence(proof,expected))
for key,value in [('schema','MT8167_USB1121_NATIVE198_V1'),('service_created',True),('mode','create-new-service'),('fresh_sha256','future235-renamed'),('recovery_boot_id','another-boot'),('profile_sha256','other-profile'),('source_commit','old198-source'),('preservation_exclusion',r'ControlSet001\Services\MT8167Usb1121Install198'),('subkeys_preserved',False),('snapshot_security_included',False),('snapshot_reloaded_equal',False),('privilege_restore_error',1300),('unload_rc',5),('native_helper_sha256','old-helper')]:
 pp=copy.deepcopy(proof);pp[key]=value;check('reject-evidence-'+key,lambda:m.verify_rearm_evidence(pp,expected),True)
f=root/'port/xnu/out/private-windows-rt-prebackup-run233/manifest.json';fm=b.read(f)
check('actual233-without-old-payload-witness-rejected',lambda:m.current_witnesses(fm,{}, {'payload/'+m.u.SERVICE+'.exe':'unused'}),True)
# witness集合の純粋関数テスト（配備inputは生成しない）。
fixture={'files':[{'source_path':'Windows/System32/'+m.u.SERVICE+'.exe'},{'source_path':'Windows/System32/'+m.u.SERVICE+'/profile.json'}],'missing_files':[{'source_path':'Windows/System32/'+m.u.SERVICE+'/new-file.json','reason':'absent'}]}
files={'payload/'+m.u.SERVICE+'.exe':'pin','payload/profile.json':'pin','payload/new-file.json':'pin'}
check('captured-existing-and-explicit-absent-file-witness',lambda:m.current_witnesses(fixture,{},files))
check('missing-overwrite-witness-rejected',lambda:m.current_witnesses(fixture,{},dict(files,**{'payload/not-captured.json':'pin'})),True)
check('unsafe-payload-destination-rejected',lambda:m.current_witnesses(fixture,{}, {'payload/../escape':'pin'}),True)
policy=b.read(root/'port/xnu/out/peripheral233-wifi-keypad-final-d276449b/peripheral-policy.json');gm=root/policy['packages']['render']['manifest'];binding,sources=m.u.bind_gpu(root,policy,gm,b.sha(gm));supp=root/'port/xnu/out/private-windows-rt-prebackup-run233-gpu-assets/manifest.json'
check('actual233-plus-samefresh-supplement-retains-GPU379',lambda:m.u.captured_gpu_sources(root,f,b.sha(f),sources,supp,b.sha(supp)))
check('actual233-without-UMD-FW-capture-rejected',lambda:m.u.captured_gpu_sources(root,f,b.sha(f),sources),True)
check('wrong-fresh-supplement-rejected',lambda:m.u.captured_gpu_sources(root,f,'future235-renamed',sources,supp,b.sha(supp)),True)
changed=copy.deepcopy(sources);changed[next(iter(changed))]['sha256']='0'*64
check('GPU-update-disguised-as-retention-rejected',lambda:m.u.captured_gpu_sources(root,f,b.sha(f),changed,supp,b.sha(supp)),True)
old=root/'port/xnu/out/windows-rt-usb1121-ps-image-component/signed';inp=root/'port/xnu/out/windows-rt-usb1121-fresh198-prepared/inputs-final'
check('actual-old198-signed-component-not-current',lambda:m.validate_current(root,inp,old,old/'native',f,'current-source',b.sha(old/'component-manifest.json'),b.sha(inp/'input-closure.json'),'not-a-native-pin'),True)
for path in ['tools/winpe/build-usb1121-fresh198-media.py','tools/ci/prepare-usb1121-service198.py']:ast.parse((repo/path).read_text())
assert b.sha(np)==pin and b.read(np)['full_service_preservation_equal'] is None
(out/'result.json').write_text(json.dumps(dict(status='passed',host=socket.gethostname(),cases=len(results),results=results,actual233_native_receipt_sha256=pin,native_hive_execution=False,final_generation=False,sign=False,VM=False,REA=False,hardware=False),indent=2)+'\n')
print('USB_CURRENT_MEDIA_HOST_PASS cases='+str(len(results)))
