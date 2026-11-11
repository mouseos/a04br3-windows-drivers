#!/usr/bin/env python3
"""既存USB媒体builder。旧新規作成と明示current rearmを検証し、GPUを更新しない。"""
import argparse,hashlib,importlib.util,json,os,re,shutil,struct,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'ci'))
_s=importlib.util.spec_from_file_location('usb198',Path(__file__).resolve().parents[1]/'ci/prepare-usb1121-service198.py');u=importlib.util.module_from_spec(_s);_s.loader.exec_module(u)
b=u.b
def verify_rearm_raw(before, after):
 expected={'Type':(4,(16).to_bytes(4,'little')),'Start':(4,(4).to_bytes(4,'little')),
           'ErrorControl':(4,(1).to_bytes(4,'little')),
           'ImagePath':(1,('"C:\\Windows\\System32\\'+u.SERVICE+'.exe"\0').encode('utf-16-le')),
           'ObjectName':(1,'LocalSystem\0'.encode('utf-16-le')),
           'DisplayName':(1,'MediaTek MT8167 USB ABI3 Installation\0'.encode('utf-16-le'))}
 b.require(before.keys()==after.keys(),'rearmでservice value集合が変化')
 for name,(kind,data) in expected.items():
  for values,raw in ((before,data),(after,(2).to_bytes(4,'little') if name=='Start' else data)):
   v=values[name];b.require(type(v['type']) is int and v['type']==kind and type(v['bytes']) is int and v['bytes']==len(raw) and v['raw_hex']==raw.hex(),'native raw identity/Start不一致: '+name)
 b.require(all(before[n]==after[n] for n in before if n!='Start'),'Start以外のservice valueが変化')

def verify_rearm_evidence(nr, expected):
 b.require(nr.get('schema')=='MT8167_USB1121_NATIVE_REARM_V2' and nr.get('mode')=='existing-disabled-start-only' and nr.get('service_created') is False and 'new_service' not in nr and nr.get('existing_service')==u.SERVICE and nr.get('source_commit')==expected['source_commit'] and nr.get('fresh_sha256')==expected['fresh_sha256'] and nr.get('recovery_boot_id')==expected['recovery_boot_id'] and nr.get('input_closure_sha256')==expected['input_closure_sha256'] and nr.get('profile_sha256')==expected['profile_sha256'] and nr.get('native_receipt_sha256')==expected['native_receipt_sha256'],'same-fresh explicit rearm native result required')
 for key in ('old_services_and_GraphicsDrivers_equal','originals_unchanged','snapshot_after_equal','snapshot_reloaded_equal','snapshot_security_included','subkeys_preserved','service_other_values_preserved','native_patch_complete'):b.require(nr.get(key) is True,'native preservation incomplete: '+key)
 for key in ('load_rc','reload_rc','unload_rc','privilege_restore_error','ACL_changes','boot_GPT_changes','POST_value'):b.require(type(nr.get(key)) is int and nr[key]==0,'native cleanup/preservation failure: '+key)
 b.require(nr.get('preservation_exclusion')==r'ControlSet001\Services\MT8167Usb1121Install198:Start only','whole existing subtree exemption forbidden')
 b.require(nr['native_helper_sha256']==expected['native_helper_sha256'] and nr['native_recipe_sha256']==expected['native_recipe_sha256'],'native recipe source drift')
 verify_rearm_raw(nr['raw_before'],nr['raw_after'])

def current_witnesses(fm, profile, files):
 original={f['source_path']:f for f in fm['files']}
 absent={f['source_path'] for f in fm.get('missing_files',[]) if f.get('reason')=='absent'}
 b.require('Windows/System32/'+u.SERVICE+'.exe' in original,'旧service EXEの実capture不足')
 destinations={}
 for n in files:
  rel=n[len('payload/'):]
  b.require(n.startswith('payload/') and re.fullmatch(r'[A-Za-z0-9_./-]+',rel) and '..' not in rel.split('/'),'safe USB payload path')
  target='Windows/System32/'+(u.SERVICE+'.exe' if rel==u.SERVICE+'.exe' else u.SERVICE+'/'+rel)
  b.require(target in original or target in absent,'上書き先の実baseline/不存在witness不足: '+target)
  destinations[n]=target
 return destinations

def validate_current(root, inp, sign, native, fm_path, source, manifest_pin, closure_pin, native_pin):
 cm=b.read(sign/'component-manifest.json');cl=b.read(inp/'input-closure.json');profile=b.read(inp/'payload/profile.json')
 b.require(b.sha(sign/'component-manifest.json')==manifest_pin and b.sha(inp/'input-closure.json')==closure_pin,'component/input closure pin drift')
 b.require(cm.get('schema')=='MT8167_USB1121_PS_IMAGE_COMPONENT_V1' and cm.get('source_commit')==source and cm.get('runtime_source_commit')=='5c39277db113faa98cd3a0f26c1072a7ce3d76c2' and cm.get('status')=='signed-final-pinned-payload-not-hardware' and cm.get('signature_status')=='Valid' and cm.get('signer_thumbprint')=='BCBF6C456C6F0DD0818434763B189488FBF13570','same committed signed component required')
 b.require(cm['fresh_sha256']==cl['fresh_sha256']==profile['fresh_sha256']==b.sha(fm_path) and cm['profile_sha256']==cl['profile_sha256']==b.sha(inp/'payload/profile.json') and cm['input_closure_sha256']==closure_pin and cm['native_receipt_sha256']==cl['native_sha256']==profile['native_receipt_sha256'],'same actual capture component/profile required')
 b.require(profile.get('schema')=='MT8167_USB1121_REBOUND_SERVICE_V2' and profile.get('integration_phase')=='standalone-retained-gpu' and cm.get('integration_phase')==profile['integration_phase'] and profile.get('gpu_update') is False and cm.get('gpu_update') is False and profile.get('current_gpu_capture_verified') is True and cm.get('current_gpu_capture_verified') is True,'USB単体の現在GPU保持profileが必要')
 b.require(set(cm['source_closure'])=={'tools/usb/mt8167-usb1121-install-service.c','tools/usb/usb1121-service-uart.h','tools/usb/usb1121-system-volume-flush.h','drivers/system/mt8167-toprgu/uart_screen.h','tools/usb/powershell-system-image-pin.json','tools/usb/usb1121-uart198-contract.json'},'component runtime closure incomplete')
 for n,h in cm['source_closure'].items():b.require(b.sha(b.below(root,Path(__file__).resolve().parents[2]/n))==h,'component source closure drift')
 contract=Path(__file__).resolve().parents[1]/'usb/usb1121-uart198-contract.json'
 b.require(cm['source_closure'].get('tools/usb/usb1121-uart198-contract.json')==b.sha(contract),'U198 run/PID terminal source pin required')
 b.require(cm.get('launcher_source_sha256')==b.sha(inp/'payload/run-usb1121-service.ps1')==b.sha(Path(__file__).resolve().parents[1]/'usb/run-usb1121-service.ps1'),'same source launcher required')
 for n,h in cl['files'].items():b.require(b.sha(b.below(root,inp/n))==h,'input leaf drift')
 prepared={n:h for n,h in cl['files'].items() if n.startswith('payload/')}
 b.require(set(cm['files'])==set(prepared)|{'payload/'+u.SERVICE+'.exe'} and all(cm['files'][n]==h for n,h in prepared.items()),'component payload closure mismatch')
 for n,h in cm['files'].items():b.require(n.startswith('payload/') and b.sha(b.below(root,sign/n))==h,'signed component leaf drift')
 b.require(cm['files']['payload/'+u.SERVICE+'.exe']==cm['signed_exe_sha256'],'signed EXE pin mismatch')
 fm=b.read(fm_path);prov=inp/'provenance'
 b.require(b.sha(prov/'fresh-manifest.json')==cl['fresh_sha256'] and b.sha(prov/'native-receipt.json')==cl['native_sha256'] and b.sha(prov/'final-policy.json')==profile['final_policy_sha256']==cm['final_policy_sha256'],'actual immutable provenance required')
 b.admit_native(root,fm_path,cl['fresh_sha256'],prov/'native-receipt.json',cl['native_sha256'],b.read(prov/'final-policy.json'))
 b.require(fm['recovery_boot_id']==profile['recovery_boot_id'],'same Recovery boot required')
 captured={f['source_path']:f for f in fm['files']}
 if (prov/'gpu-capture-manifest.json').exists():
  sp=prov/'gpu-capture-manifest.json';extra=b.read(sp)
  b.require(b.sha(sp)==cl['gpu_capture_sha256'] and extra.get('source_manifest_sha256')==cl['fresh_sha256'] and extra.get('recovery_boot_id')==fm['recovery_boot_id'] and extra.get('status')=='captured-unmounted' and extra.get('read_only') is True and extra.get('unmounted') is True and extra.get('errors')==[],'same-fresh GPU supplement required')
  for f in extra['files']:
   b.require(f['source_path'] not in captured or captured[f['source_path']]['sha256']==f['sha256'],'GPU capture witness conflict')
   captured[f['source_path']]=f
 b.require(profile.get('preserved_gpu_witnesses'),'retained GPU witness missing')
 for n,e in profile['preserved_gpu_witnesses'].items():
  b.require(n in captured and captured[n]['sha256']==e['sha256']==profile['retained_files'][n] and b.sha(b.below(root,e['path']))==e['sha256'],'current GPU witness mismatch')
 current_witnesses(fm,profile,cm['files'])
 nr=b.read(native/'result.json');b.require(b.sha(native/'result.json')==native_pin,'native result pin drift')
 verify_rearm_evidence(nr,dict(source_commit=source,fresh_sha256=cl['fresh_sha256'],recovery_boot_id=fm['recovery_boot_id'],input_closure_sha256=closure_pin,profile_sha256=cl['profile_sha256'],native_receipt_sha256=cl['native_sha256'],native_helper_sha256=b.sha(Path(__file__).resolve().parents[1]/'recovery/Usb1121OfflineHiveNative.cs'),native_recipe_sha256=b.sha(Path(__file__).resolve().parents[1]/'recovery/prepare-usb1121-system198.ps1')))
 b.require(b.sha(native/'SYSTEM')==nr['SYSTEM_sha256'],'native SYSTEM byte drift')
 cm.update(signer=cm['signer_thumbprint'],uart_contract=b.read(contract),uart_contract_sha256=b.sha(contract),prepared_SYSTEM_sha256=nr['SYSTEM_sha256'],native_preparation_source_commit=nr['source_commit'])
 return cm,cl,profile,nr,fm

def main():
 ap=argparse.ArgumentParser()
 for k in ('workspace','inputs','signed-package','base','tools','out'):ap.add_argument('--'+k,type=Path,required=True)
 ap.add_argument('--expected-service-manifest-sha256',required=True)
 ap.add_argument('--mode',choices=('legacy-new','existing-service-rearm'),default='legacy-new')
 ap.add_argument('--native-directory',type=Path);ap.add_argument('--expected-native-result-sha256')
 ap.add_argument('--fresh-manifest',type=Path);ap.add_argument('--expected-input-closure-sha256')
 ap.add_argument('--validate-only',action='store_true')
 ap.add_argument('--reboot-after-commit',action='store_true')
 a=ap.parse_args();root=a.workspace.resolve(strict=True)
 inp,sign,base,tools=[b.below(root,getattr(a,k)) for k in ('inputs','signed_package','base','tools')]
 out=a.out.resolve();b.require(out.is_relative_to(root) and not out.exists(),'new root-local private output required')
 source=subprocess.check_output(['git','-C',str(Path(__file__).resolve().parents[2]),'rev-parse','HEAD'],text=True).strip()
 current=a.mode=='existing-service-rearm'
 if current:
  b.require(not subprocess.check_output(['git','-C',str(Path(__file__).resolve().parents[2]),'status','--porcelain'],text=True).strip(),'clean committed current media source required')
  b.require(all((a.native_directory,a.expected_native_result_sha256,a.fresh_manifest,a.expected_input_closure_sha256)),'current rearm requires explicit native/fresh/closure pins')
  native=b.below(root,a.native_directory);fm_path=b.below(root,a.fresh_manifest)
  sm,cl,profile,nr,fm=validate_current(root,inp,sign,native,fm_path,source,a.expected_service_manifest_sha256,a.expected_input_closure_sha256,a.expected_native_result_sha256)
  system=native/'SYSTEM';fresh_pin=cl['fresh_sha256'];native_receipt_pin=cl['native_sha256']
 else:
  b.require(not any((a.native_directory,a.expected_native_result_sha256,a.fresh_manifest,a.expected_input_closure_sha256)),'current pins require explicit rearm mode')
  sm=b.read(sign/'service-manifest.json');b.require(sm.get('recipe_source_commit',sm['source_commit'])==source,'same committed native/media recipe required');b.require(b.sha(sign/'service-manifest.json')==a.expected_service_manifest_sha256,'signed manifest pin')
  contract_path=Path(__file__).resolve().parents[1]/'usb/usb1121-uart198-contract.json'
  b.require(sm.get('uart_contract')==b.read(contract_path) and sm.get('uart_contract_sha256')==b.sha(contract_path),'UART terminal contract required; do not build old unobservable media')
  b.require(sm.get('native_reused_byte_identical') is True and sm.get('native_preparation_source_commit')=='7ec02fc5d4030fe8cedb3c1e4fe135c5804c2869' and sm.get('native_ready_manifest_sha256')=='5447b112e3be16a7b6fd13c1733cc4411bab6b2eee3abaa6da5a4da3ed0f3d7b','exact completed native7ec reuse proof required')
  for n,h in sm['source_header_hashes'].items():b.require(b.sha(b.below(root,Path(__file__).resolve().parents[2]/n))==h,'signed UART header drift')
  for n,h in sm['native_output_hashes'].items():b.require(b.sha(b.below(root,sign/'native'/n))==h,'native reused member drift')
  cl=b.read(inp/'input-closure.json');profile=b.read(inp/'payload/profile.json')
  b.require(sm['schema']=='MT8167_USB1121_SIGNED_SERVICE198_V1' and sm['signature_status']=='Valid' and sm['signer']=='BCBF6C456C6F0DD0818434763B189488FBF13570' and sm['input_closure_sha256']==b.sha(inp/'input-closure.json') and sm['profile_sha256']==b.sha(inp/'payload/profile.json')==cl['profile_sha256'] and sm['fresh_sha256']==b.FRESH and sm['native_receipt_sha256']==b.NATIVE,'same fresh198 signed contract')
  for n,h in cl['files'].items():b.require(b.sha(b.below(root,inp/n))==h,'input drift')
  for n,h in sm['files'].items():
   b.require(n.startswith('payload/') and b.sha(b.below(root,sign/n))==h,'signed output drift')
  # Signed payload retains every prepared leaf; only the one SCM EXE is added.
  prepared={n:h for n,h in cl['files'].items() if n.startswith('payload/')}
  b.require(set(sm['files'])==set(prepared)|{'payload/'+u.SERVICE+'.exe'} and all(sm['files'][n]==h for n,h in prepared.items()),'unexpected signed payload/member changes')
  nr=b.read(sign/'native/result.json');system=sign/'native/SYSTEM'
  b.require(b.sha(sign/'native/result.json')==sm['native_result_sha256'] and nr['fresh_sha256']==b.FRESH and nr['input_closure_sha256']==b.sha(inp/'input-closure.json') and nr['old_services_and_GraphicsDrivers_equal'] and nr['originals_unchanged'] and nr['POST_value']==0 and nr['boot_GPT_changes']==0 and b.sha(system)==nr['SYSTEM_sha256']==sm['prepared_SYSTEM_sha256'],'native preservation proof required')
  fresh_pin=b.FRESH;native_receipt_pin=b.NATIVE
 header=system.read_bytes()[:4096];b.require(len(header)==4096 and header[:4]==b'regf' and struct.unpack_from('<II',header,20)==(1,5) and struct.unpack_from('<I',header,4)==struct.unpack_from('<I',header,8),'clean native SYSTEM required')
 checksum=0
 for n in struct.unpack('<127I',header[:508]):checksum^=n
 checksum=0xfffffffe if checksum==0xffffffff else 1 if checksum==0 else checksum
 b.require(checksum==struct.unpack_from('<I',header,508)[0],'SYSTEM checksum')
 if a.validate_only:
  out.mkdir(mode=0o700);(out/'validation.json').write_text(json.dumps(dict(mode=a.mode,fresh_sha256=fresh_pin,valid=True,media_generated=False,hardware_applied=False),indent=2)+'\n');return
 b.require(b.sha(base/'maintenance-staged.wim')=='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' and b.sha(base/'maintenance-staged-esp.img')=='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce','fixed guarded base')
 comp=b.below(root,'port/xnu/out/windows-rt-run121-single-gpu/service');cm=comp/'render-service-manifest.json'
 b.require(b.sha(cm)=='33ad4f9ddf3c7cc0fcf3086f4724ebf7688c0dca2d5861a4a5b73f6b62a351fd','comparator manifest')
 c=b.read(cm)['comparator'];b.require(c['signature_status']=='Valid' and c['signer_thumbprint']==sm['signer'] and b.sha(comp/'compare-files.exe')==c['exe_sha256'],'comparator byte/trust receipt')
 if not current:fm_path=b.below(root,'port/xnu/out/private-windows-rt-prebackup-run198/manifest.json');b.require(b.sha(fm_path)==b.FRESH,'fresh198 drift');fm=b.read(fm_path)
 out.mkdir(mode=0o700);stage=out/'stage';stage.mkdir();files={}
 def copy(src,n):
  d=stage/n;d.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,d);files[n]=b.sha(d)
 copy(comp/'compare-files.exe','compare-files.exe');copy(system,'private-system/SYSTEM.patched')
 prepare=[]
 def compare(left,right,fail):return ['X:\\a04br3\\compare-files.exe "'+left+'" "'+right+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto '+fail]
 witnesses={}
 for f in fm['files']:
  b.require(re.fullmatch(r'[A-Za-z0-9_./-]+',f['source_path']) and '..' not in f['source_path'].split('/'),'safe baseline witness path')
  if current or f['source_path'].startswith('Windows/System32/config/'):
   witnesses[f['source_path']]={'path':(fm_path.parent/f['backup_path']).relative_to(root).as_posix(),'sha256':f['sha256']}
 witnesses.update(profile['retained_sources'])
 for i,(n,e) in enumerate(sorted(witnesses.items())):
  src=b.below(root,e['path']);b.require(b.sha(src)==e['sha256'],'retained witness drift');name='baseline/'+str(i)+'.bin';copy(src,name);prepare+=compare('X:\\a04br3\\'+name.replace('/','\\'),'C:\\'+n.replace('/','\\'),'baselinefail')
 dest='C:\\Windows\\System32\\'+u.SERVICE
 if current:
  prepare+=['if not exist '+dest+' goto baselinefail','if not exist '+dest+'.exe goto baselinefail','if exist '+dest+'\\transaction goto baselinefail']
  for target in current_witnesses(fm,profile,sm['files']).values():
   if target not in {f['source_path'] for f in fm['files']}:prepare+=['if exist "C:\\'+target.replace('/','\\')+'" goto baselinefail']
  prepare+=['if not exist '+dest+'\\logs mkdir '+dest+'\\logs >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 else:
  prepare+=['if exist '+dest+' goto baselinefail','if exist '+dest+'.exe goto baselinefail','mkdir '+dest+'\\logs >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 for n,h in sorted(sm['files'].items()):
  rel=n[len('payload/'):];b.require(re.fullmatch(r'[A-Za-z0-9_./-]+',rel) and '..' not in rel.split('/'),'safe payload path');copy(sign/n,n)
  target=dest+'.exe' if rel==u.SERVICE+'.exe' else dest+'\\'+rel.replace('/','\\')
  parent=target.rsplit('\\',1)[0]
  prepare+=['if not exist "'+parent+'" mkdir "'+parent+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','copy /b /y "X:\\a04br3\\'+n.replace('/','\\')+'" "'+target+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']+compare('X:\\a04br3\\'+n.replace('/','\\'),target,'candidatefail')
 # All payload checks complete before the only SYSTEM mutation.
 prepare+=['copy /b /y X:\\a04br3\\private-system\\SYSTEM.patched C:\\Windows\\System32\\config\\SYSTEM >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']+compare('X:\\a04br3\\private-system\\SYSTEM.patched','C:\\Windows\\System32\\config\\SYSTEM','candidatefail')
 here=Path(__file__).resolve().parent;template=(here/'run-private-system-apply.cmd.in').read_text()
 start=template.index('rem No REG LOAD');end=template.index('> "%A04_BK%\\JOURNAL.txt" echo STATE=COMMITTED',start)
 cmd=template[:start]+'\n'.join(prepare)+'\n'+template[end:]
 cmd=cmd.replace('SYSTEM_COPY_FAILED','CANDIDATE_WRITE_FAILED')
 cmd=cmd.replace(':dismfail\n',':baselinefail\n>> "%A04_REPORT%" echo UPDATE_STATUS=BASELINE_REJECTED_NO_CHANGE\nset "A04_RC=6"\ngoto release\n:candidatefail\n')
 token=hashlib.sha256((b.sha(inp/'payload/profile.json')+sm['prepared_SYSTEM_sha256']).encode()).hexdigest()[:16]
 for k,v in {'TOKEN':token.upper(),'JOURNAL':'usb1121-'+token,'HOST_BACKUP_SHA':fresh_pin,'SYSTEM_SHA':b.sha(system)}.items():cmd=cmd.replace('@'+k+'@',v)
 b.require(not re.search('@[A-Z_]+@',cmd) and not re.search(r'(?im)^\s*(reg(?:\.exe)?\s|dism(?:\.exe)?\s)',cmd),'no offline PnP/registry commands')
 if a.reboot_after_commit:
  cmd=cmd.replace('exit /b %A04_RC%\n','set "A04_STREAM_RC=%errorlevel%"\nif not "%A04_RC%"=="0" exit /b %A04_RC%\nif not "%A04_RELEASE_RC%"=="0" exit /b 7\nif not "%A04_STREAM_RC%"=="0" exit /b 8\nX:\\Windows\\System32\\wpeutil.exe reboot\nexit /b 9\n')
 (stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode())
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image)
 env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 commands='add "'+str(stage/'startnet.cmd')+'" /Windows/System32/startnet.cmd\n'
 for f in sorted(stage.rglob('*')):
  if f.is_file() and f.name!='startnet.cmd':commands+='add "'+str(f)+'" /a04br3/'+f.relative_to(stage).as_posix()+'\n'
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True)
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run([sys.executable,'-B',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 result=dict(schema=('MT8167_USB1121_CURRENT_REARM_MEDIA_V2' if current else 'MT8167_USB1121_MEDIA198_V1'),mode=a.mode,gpu_update=False if current else None,uart_contract=sm['uart_contract'],uart_contract_sha256=sm['uart_contract_sha256'],native_preparation_source_commit=sm['native_preparation_source_commit'],source_commit=source,service_source_commit=sm['source_commit'],service_manifest_sha256=a.expected_service_manifest_sha256,fresh_sha256=fresh_pin,native_receipt_sha256=native_receipt_pin,profile_sha256=b.sha(inp/'payload/profile.json'),files={p.relative_to(stage).as_posix():b.sha(p) for p in stage.rglob('*') if p.is_file()},esp_sha256=b.sha(out/'update-esp.img'),wim_sha256=b.sha(image),boot_GPT_changes=0,normal_install_performed=False,hardware_applied=False,power_verified=False)
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
