#!/usr/bin/env python3
"""固定周辺batch専用WinPE stage。署名済service/新fresh候補のみ、driver DISMなし。"""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
from peripheral_batch_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,rooted,sha,SIGNER,require
from private_hardware_probe_input import prepared_system_check,private_path

def stage_script(template,prepare,token,fresh):
 start=template.index('reg.exe load HKLM\\Mt8167IoOffline ')
 end=template.index('copy /y X:\\a04br3\\dism-*.txt',start)
 cmd=template[:start]+'rem Driver-store/DIF operations run only in the dedicated normal-OS service.\n'+template[end:]
 start=cmd.index(':adddriver\n');end=cmd.index(':dismfail\n',start);cmd=cmd[:start]+cmd[end:]
 cmd='\n'.join(s for s in cmd.splitlines() if not s.startswith('copy /y X:\\a04br3\\dism-*.txt'))+'\n'
 cmd=cmd.replace('shared-io-@JOURNAL@','peripheral-batch-@JOURNAL@').replace('STATE=DISM_STARTED','STATE=PRECHECK').replace('DISM_FAILED_RESTORE_FROM_HOST_IMAGE','CANDIDATE_WRITE_FAILED').replace('goto dismfail','goto candidatefail').replace(':dismfail\n',':candidatefail\n')
 cmd=cmd.replace('setlocal EnableExtensions DisableDelayedExpansion','setlocal EnableExtensions DisableDelayedExpansion\nset "A04_CANDIDATE_STARTED=0"')
 baseline=':baselinefail\n> "%A04_BK%\\JOURNAL.txt" echo STATE=BASELINE_REJECTED_NO_CHANGE\n>> "%A04_REPORT%" echo UPDATE_STATUS=BASELINE_REJECTED_NO_CHANGE\nset "A04_RC=6"\ngoto release\n'
 cmd=cmd.replace(':candidatefail\n',baseline+':candidatefail\n')
 cmd=cmd.replace('@TOKEN@',token.upper()).replace('@JOURNAL@',token).replace('@HOST_BACKUP_SHA@',fresh).replace('@PROBE_PREPARE@','\n'.join(prepare))
 require(not re.search('@[A-Z_]+@',cmd),'未置換stage template')
 require(not re.search(r'(?im)^\s*(reg(?:\.exe)?\s+(?:load|unload|add)|dism(?:\.exe)?|call :adddriver|.*(?:ge8300|EngineProbe\.exe).*--)',cmd),'旧reg/DISM/Engine/GPU起動がstageへ混入')
 require('/ForceUnsigned' not in cmd and 'RESTORE_FROM' not in cmd,'fallback/rollback禁止')
 return cmd

def baseline_failure_reports(prepare):
 # Keep comparator bytes/order/exit handling. Emit metadata only in its failure arm.
 result=[];ordinal=0;prefix='X:\\a04br3\\'
 kinds={'fresh-input':'FRESH_INPUT','preserved-proof':'PRESERVED_FILE',
        'baseline-proof':'EXTRA_BASELINE','rotation-proof':'ROTATION_BASELINE',
        'rotation-reuse-proof':'ROTATION_REUSE'}
 for i,line in enumerate(prepare):
  if line!='if errorlevel 1 goto baselinefail' or not i or not prepare[i-1].startswith(prefix+'compare-files.exe '):
   result.append(line);continue
  args=[quoted or bare for quoted,bare in re.findall(r'"([^"]*)"|(\S+)',prepare[i-1].split(' >> ',1)[0])]
  require(len(args)==3,'Fixed comparator command requires two file operands')
  left,right=args[1:];require(left.startswith(prefix),'Baseline proof must be a fixed WinPE input')
  if right.startswith('C:\\'):
   kind=kinds.get(left[len(prefix):].split('\\',1)[0]);relative=right[3:].replace('\\','/')
   require(kind is not None,'Unknown baseline check kind must be classified explicitly')
  else:
   require(right.startswith(prefix+'stage-proof\\'),'Unknown stage baseline destination')
   kind='STAGE_SELF';relative=left[len(prefix):].replace('\\','/')
  require(re.fullmatch(r'[A-Za-z0-9_./ -]+',relative) and not relative.startswith('/') and '..' not in relative.split('/'),'Unsafe report-relative path')
  ordinal+=1
  result+=['if errorlevel 1 (',
           '>> "%A04_REPORT%" echo BASELINE_FAILURE_KIND='+kind,
           '>> "%A04_REPORT%" echo BASELINE_FAILURE_RELATIVEPATH='+relative,
           '>> "%A04_REPORT%" echo BASELINE_FAILURE_COMPARISON='+str(ordinal),
           'goto baselinefail',')']
 return result

def bt_payload_prepare(directory,bt):
 # WIM staging alone does not deploy a normal-OS service input directory.
 names=[f['name'] for f in bt['files']]
 require(names==['mt6630-combo-bluetooth.inf','mt6630-combo-bluetooth.sys','mt6630-combo-bluetooth.cat'],'Exact signed BT2013 trio only')
 require(re.fullmatch(r'MT8167CombinedFollowup[0-9]+',directory),'Fixed service directory required')
 sep=chr(92);dst=sep.join(('C:','Windows','System32',directory,'bt2013'))
 commands=['mkdir '+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 for name in names:
  src=sep.join(('X:','a04br3','payload','bt2013',name));target=dst+sep+name
  commands+=['copy /b '+src+' '+target+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail',sep.join(('X:','a04br3','compare-files.exe'))+' '+src+' '+target+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 return commands

def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','map','service-package','prepared-system','prepared-result','comparator','base','tools','out'):ap.add_argument('--'+n,type=Path,required=True)
 for n in ('prepared-sha256','comparator-manifest-sha256','source-commit'):ap.add_argument('--'+n,required=True)
 ap.add_argument('--combined164-profile',action='store_true');ap.add_argument('--combined162-profile',action='store_true');ap.add_argument('--combined160-profile',action='store_true');ap.add_argument('--combined158-profile',action='store_true');ap.add_argument('--combined156-profile',action='store_true');ap.add_argument('--combined154-profile',action='store_true');ap.add_argument('--combined152-profile',action='store_true');ap.add_argument('--combined150-profile',action='store_true');ap.add_argument('--combined148-profile',action='store_true');ap.add_argument('--combined146-profile',action='store_true');ap.add_argument('--combined144-profile',action='store_true');ap.add_argument('--combined142-profile',action='store_true');ap.add_argument('--combined140-profile',action='store_true');ap.add_argument('--combined138-profile',action='store_true');ap.add_argument('--combined136-profile',action='store_true');ap.add_argument('--next-profile',action='store_true');ap.add_argument('--service-source-commit');ap.add_argument('--enable-peripheral-batch',action='store_true');a=ap.parse_args();root=a.workspace.resolve(strict=True);here=Path(__file__).resolve().parent;repo=here.parents[1]
 if a.combined164_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile,a.combined150_profile,a.combined152_profile,a.combined154_profile,a.combined156_profile,a.combined158_profile,a.combined160_profile,a.combined162_profile]),'Profiles mutually exclusive')
  from combined164_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined162_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile,a.combined150_profile,a.combined152_profile,a.combined154_profile,a.combined156_profile,a.combined158_profile,a.combined160_profile]),'Profiles mutually exclusive')
  from combined162_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined160_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile,a.combined150_profile,a.combined152_profile,a.combined154_profile,a.combined156_profile,a.combined158_profile]),'Profiles mutually exclusive')
  from combined160_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined158_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile,a.combined150_profile,a.combined152_profile,a.combined154_profile,a.combined156_profile]),'Profiles mutually exclusive')
  from combined158_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined156_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile,a.combined150_profile,a.combined152_profile,a.combined154_profile]),'Profiles mutually exclusive')
  from combined156_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined154_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile,a.combined150_profile,a.combined152_profile]),'Profiles mutually exclusive')
  from combined154_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined152_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile,a.combined150_profile]),'Profiles mutually exclusive')
  from combined152_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined150_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile,a.combined148_profile]),'Profiles mutually exclusive')
  from combined150_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined148_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile,a.combined146_profile]),'Profiles mutually exclusive')
  from combined148_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined146_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile,a.combined144_profile]),'Profiles mutually exclusive')
  from combined146_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined144_profile:
  require(not any([a.next_profile,a.combined136_profile,a.combined138_profile,a.combined140_profile,a.combined142_profile]),'Profiles mutually exclusive')
  from combined144_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined142_profile:
  require(not a.next_profile and not a.combined136_profile and not a.combined138_profile and not a.combined140_profile,'Profiles mutually exclusive')
  from combined142_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined140_profile:
  require(not a.next_profile and not a.combined136_profile and not a.combined138_profile,'Profiles mutually exclusive')
  from combined140_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined138_profile:
  require(not a.next_profile and not a.combined136_profile,'Profiles mutually exclusive')
  from combined138_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.combined136_profile:
  require(not a.next_profile,'Profiles mutually exclusive')
  from combined136_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 elif a.next_profile:
  from peripheral_next_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY,generate_paths,assert_paths
 else:
  from peripheral_batch_policy import load_map,generate,PROFILE,SERVICE,DIRECTORY
 require(a.enable_peripheral_batch,'専用profile明示opt-inが必要');require(re.fullmatch('[0-9a-f]{40}',a.source_commit),'固定source必須')
 require(subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()==a.source_commit and not subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True).strip(),'clean固定source必須')
 p=load_map(a.map,root);service=rooted(root,a.service_package);sm=json.loads((service/'peripheral-service-manifest.json').read_text(encoding='utf-8-sig'))
 signed_source=a.service_source_commit or a.source_commit
 require(re.fullmatch('[0-9a-f]{40}',signed_source),'署名source pin必須')
 if signed_source!=a.source_commit:
  subprocess.run(['git','-C',str(repo),'merge-base','--is-ancestor',signed_source,a.source_commit],check=True)
  changed=set(subprocess.check_output(['git','-C',str(repo),'diff','--name-only',signed_source,a.source_commit],text=True).splitlines())
  require(changed and changed<={'tools/recovery/prepare-peripheral-followup154.py','tools/recovery/read-combined154-post.ps1','tools/recovery/read-gpu-native-checkpoint.ps1','tools/recovery/read-combined152-post.ps1','tools/recovery/read-combined146-post.ps1','tools/ci/read-ge8300-boundary332.py','tools/winpe/build-private-peripheral-batch-media.py','tools/winpe/private_hardware_probe_input.py','docs/peripheral-batch.md','tools/recovery/prepare-offline-system-service.ps1','tools/ci/build-combined136-service.ps1','tools/ci/build-combined138-service.ps1','tools/recovery/prepare-peripheral-followup136.py','tools/recovery/verify-combined138-preservation.ps1','tests/combined138_contract_test.py'},'署名後の差分は明示media toolingだけを許可')
 require(sm.get('source_commit')==signed_source and sm.get('source_dirty') is False and sm.get('profile')==PROFILE and sm.get('service')==SERVICE and sm.get('signature_status')=='Valid' and sm.get('signer_thumbprint')==SIGNER,'同sourceの専用署名service必須')
 require(sm['policy']==p and sha(service/'peripheral-policy.json')==sm['policy_manifest_sha256'] and sha(service/(SERVICE+'.exe'))==sm['exe_sha256'] and (service/'peripheral-batch-policy.h').read_text()==generate(p),'signed policy/header/EXE不一致')
 if a.next_profile or a.combined136_profile or a.combined138_profile or a.combined140_profile or a.combined142_profile or a.combined144_profile or a.combined146_profile or a.combined148_profile or a.combined150_profile or a.combined152_profile or a.combined154_profile or a.combined156_profile or a.combined158_profile or a.combined160_profile or a.combined162_profile or a.combined164_profile:
  require((service/'peripheral-service-path-policy.h').read_text()==generate_paths(),'Signed service path header mismatch');assert_paths(service/(SERVICE+'.exe'))
 backup=rooted(root,p['backup_manifest']);b=json.loads(backup.read_text());result=rooted(root,a.prepared_result);system=rooted(root,a.prepared_system)
 native=prepared_system_check(system,result,a.prepared_sha256,root,b,SERVICE);nr=json.loads(result.read_text(encoding='utf-8-sig'))
 for phase in ('patch','verify'):
  require(json.loads(nr[phase]['state']['related_fingerprints_before']['peripheral_old_services'])==p['service_preservation'],'native worker旧service snapshotと署名plan不一致')
 require(nr.get('peripheral_preservation_plan_sha256')==sha(service/'peripheral-preservation-plan.json') and nr.get('peripheral_old_services_preserved') is True and not nr.get('engine_rearm_plan_sha256'),'新freshの専用native armと旧service保持必須')
 comp=rooted(root,a.comparator);cm=comp/'render-service-manifest.json';require(sha(cm)==a.comparator_manifest_sha256,'既存comparator manifest pin不一致');c=json.loads(cm.read_text(encoding='utf-8-sig'))['comparator'];require(c['signature_status']=='Valid' and c['signer_thumbprint']==SIGNER and sha(comp/'compare-files.exe')==c['exe_sha256'],'comparator署名/hash不一致')
 base=rooted(root,a.base);tools=rooted(root,a.tools);require(sha(base/'maintenance-staged.wim')=='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' and sha(base/'maintenance-staged-esp.img')=='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce','既存guard付きWinPE baseのみ')
 out=a.out.resolve();private_path(out,root);require(not out.exists(),'新規root内出力のみ');out.mkdir(mode=0o700);stage=out/'stage';stage.mkdir();files={}
 def copy(src,name):
  q=stage/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,q);files[name]=sha(q)
 copy(comp/'compare-files.exe','compare-files.exe');copy(service/(SERVICE+'.exe'),'payload/'+SERVICE+'.exe');copy(system,'payload/SYSTEM.patched')
 for role,v in p['packages'].items():
  for name in list(v['names'].values())+[e['name'] for e in v.get('extra_members',[])]:copy(root/v['directory']/name,'payload/'+role+'/'+name)
 for key,v in p.get('companion_packages',{}).items():
  require((a.combined138_profile or a.combined140_profile or a.combined142_profile or a.combined144_profile or a.combined146_profile) and key in ('hcd1014','hcd1015','hcd1016') and v['stage_only'] and not v['DIF'],'Fixed import-only companion')
  for f in v['files']:copy(root/v['directory']/f['name'],'payload/'+key+'/'+f['name'])
 if a.combined144_profile or a.combined146_profile:
  for f in p['bt_first_install']['files']:copy(root/p['bt_first_install']['directory']/f['name'],'payload/bt2013/'+f['name'])
 for client in p.get('diagnostic_clients',[]):
  copy(root/client['directory']/client['exe'],'payload/clients/'+client['exe'])
  for dep in client['dependencies']:copy(root/client['directory']/dep['exe'],'payload/clients/'+dep['exe'])
 token=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest();(stage/'payload/boot-transition-authorized.bin').write_bytes(token.encode()+b'\0');files['payload/boot-transition-authorized.bin']=sha(stage/'payload/boot-transition-authorized.bin')
 prepare=[]
 for i,e in enumerate(x for x in b['files'] if x['source_path'] in {'Windows/System32/config/'+n+s for n in ('SYSTEM','DRIVERS') for s in ('','.LOG1','.LOG2')}):
  copy(backup.parent/e['backup_path'],'fresh-input/'+str(i)+'.bin')
  prepare+=['X:\\a04br3\\compare-files.exe X:\\a04br3\\fresh-input\\'+str(i)+'.bin "C:\\'+e['source_path'].replace('/','\\')+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
 require(len(prepare)==12,'SYSTEM/DRIVERSの実trio各3必須')
 # Preserve exact current GPU/Engine/other bytes before arming SYSTEM.
 for i,(name,want) in enumerate(p['preserved_files'].items()):
  witness=rooted(root,p['preserved_sources'][name]);require(sha(witness)==want,'保持capture witness hash不一致')
  copy(witness,'preserved-proof/'+str(i)+'.bin')
  prepare+=['X:\\a04br3\\compare-files.exe X:\\a04br3\\preserved-proof\\'+str(i)+'.bin "C:\\'+name.replace('/','\\')+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
 # Extra baseline witnesses (current UMD and actual battery) are immutable inputs.
 for i,(name,e) in enumerate(p.get('baseline_witnesses',{}).items()):
  witness=rooted(root,e['path']);require(sha(witness)==e['sha256'],'Actual baseline witness mismatch');copy(witness,'baseline-proof/'+str(i)+'.bin')
  prepare+=['X:\\a04br3\\compare-files.exe X:\\a04br3\\baseline-proof\\'+str(i)+'.bin "C:\\'+name.replace('/','\\')+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
 rotation=p.get('prior_rotation_log');reuse_rotation=p.get('reuse_rotation_directory_without_prior_active_log',False)
 if rotation:
  require(a.combined136_profile and rotation['overwrite_or_delete'] is False,'Explicit immutable old rotation archive required')
  witness=rooted(root,rotation['witness']);require(sha(witness)==rotation['sha256'],'Old rotation log witness mismatch');copy(witness,'rotation-proof/log.bin')
  old='C:\\'+rotation['source'].replace('/','\\');archive=old.rsplit('\\',1)[0]+'\\'+rotation['archive_name']
  prepare+=['if exist "'+archive+'" goto baselinefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\rotation-proof\\log.bin "'+old+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
  rotation_client=next(c for c in p['diagnostic_clients'] if c['name']=='rotation')
  for i,e in enumerate(p['prior_rotation_executables']):
   witness=rooted(root,e['witness']);require(sha(witness)==e['sha256'],'Old rotation EXE witness mismatch');copy(witness,'rotation-proof/'+str(i)+'.bin')
   prepare+=['X:\\a04br3\\compare-files.exe X:\\a04br3\\rotation-proof\\'+str(i)+'.bin "C:\\'+e['source'].replace('/','\\')+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
 if reuse_rotation:
  require(a.combined136_profile and not rotation,'Explicit no-rerun rotation reuse contract required')
  rotation_client=next(c for c in p['diagnostic_clients'] if c['name']=='rotation')
  prepare+=['if exist C:\\Windows\\System32\\MT8167DisplayRotationProbeV1\\logs\\display-rotation-probe-v1.log goto baselinefail','if exist C:\\Windows\\System32\\MT8167CombinedFollowup124\\logs\\wlan-scan.txt goto baselinefail']
  for i,e in enumerate(p['prior_rotation_executables']):
   witness=rooted(root,e['witness']);require(sha(witness)==e['sha256'],'Existing rotation EXE byte pin mismatch');copy(witness,'rotation-reuse-proof/'+str(i)+'.bin')
   prepare+=['X:\\a04br3\\compare-files.exe X:\\a04br3\\rotation-reuse-proof\\'+str(i)+'.bin "C:\\'+e['source'].replace('/','\\')+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
 # Duplicate immutable payload witnesses let the native EOF comparator check
 # every copied stage byte before SYSTEM arm; WIM and manifest remain sealed.
 payload=dict(files)
 for i,(n,h) in enumerate(sorted(payload.items())):
  copy(stage/n,'stage-proof/'+str(i)+'.bin');prepare+=['X:\\a04br3\\compare-files.exe "X:\\a04br3\\'+n.replace('/','\\')+'" X:\\a04br3\\stage-proof\\'+str(i)+'.bin >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
 for directory in sorted({c['relative_directory'] for c in p.get('diagnostic_clients',[]) if not c['relative_directory'].startswith(DIRECTORY+chr(92)) and not ((rotation or reuse_rotation) and c['name']=='rotation')}):prepare+=['if exist '+chr(92).join(('C:','Windows','System32',directory))+' goto baselinefail']
 prepare+=['if exist C:\\Windows\\System32\\'+DIRECTORY+' goto baselinefail','if exist C:\\Windows\\System32\\'+SERVICE+'.exe goto baselinefail','set "A04_CANDIDATE_STARTED=1"','> "%A04_BK%\\JOURNAL.txt" echo STATE=CANDIDATE_WRITE_STARTED','mkdir C:\\Windows\\System32\\'+DIRECTORY+'\\logs >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 if rotation:
  prepare+=['move "'+old+'" "'+archive+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\rotation-proof\\log.bin "'+archive+'" >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','if exist "'+old+'" goto candidatefail']
 for role,v in p['packages'].items():
  dst='C:\\Windows\\System32\\'+DIRECTORY+'\\'+role;prepare+=['mkdir '+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
  for name in list(v['names'].values())+[e['name'] for e in v.get('extra_members',[])]:
   prepare+=['copy /b X:\\a04br3\\payload\\'+role+'\\'+name+' '+dst+'\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\payload\\'+role+'\\'+name+' '+dst+'\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 for key,v in p.get('companion_packages',{}).items():
  dst='C:\\Windows\\System32\\'+DIRECTORY+'\\'+key;prepare+=['mkdir '+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
  for f in v['files']:
   name=f['name'];src='X:\\a04br3\\payload\\'+key+'\\'+name
   prepare+=['copy /b '+src+' '+dst+'\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe '+src+' '+dst+'\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 if p.get('bt_first_install') and p.get('bt_first_install_execute',True):
  prepare+=bt_payload_prepare(DIRECTORY,p['bt_first_install'])
 if p.get('diagnostic_clients'):
  sep=chr(92);system=sep.join(('C:','Windows','System32'))+sep
  for directory in sorted({c['relative_directory'] for c in p['diagnostic_clients']}):
   if (rotation or reuse_rotation) and directory==rotation_client['relative_directory']:continue
   prepare+=['mkdir '+system+directory+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
  for client in p['diagnostic_clients']:
   for name in [client['exe']]+[d['exe'] for d in client['dependencies']]:
    src=sep.join(('X:','a04br3','payload','clients',name));dst=system+client['relative_directory']+sep+name
    prepare+=['copy /b '+('/y ' if (rotation or reuse_rotation) and client['name']=='rotation' else '')+src+' '+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail',sep.join(('X:','a04br3','compare-files.exe'))+' '+src+' '+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 for name,dst in [('boot-transition-authorized.bin',DIRECTORY+'\\boot-transition-authorized.bin'),(SERVICE+'.exe',SERVICE+'.exe')]:
  prepare+=['copy /b X:\\a04br3\\payload\\'+name+' C:\\Windows\\System32\\'+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\payload\\'+name+' C:\\Windows\\System32\\'+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 # Last mutation only: main-selected capture-derived clean SYSTEM containing the one new service.
 prepare+=['copy /b /y X:\\a04br3\\payload\\SYSTEM.patched C:\\Windows\\System32\\config\\SYSTEM >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\payload\\SYSTEM.patched C:\\Windows\\System32\\config\\SYSTEM >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 prepare=baseline_failure_reports(prepare)
 cmd=stage_script((here/'run-mt8167-shared-io-update.cmd.in').read_text(),prepare,token[:16],p['backup_manifest_sha256']);(stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode());files['run-update-once.cmd']=sha(stage/'run-update-once.cmd')
 (stage/'startnet.cmd').write_bytes(b'@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\nif errorlevel 1 exit /b 2\r\ncall X:\\a04br3\\run-update-once.cmd\r\n');files['startnet.cmd']=sha(stage/'startnet.cmd')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image);env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 commands='add "'+str(stage/'startnet.cmd')+'" /Windows/System32/startnet.cmd\n'
 for f in sorted(stage.rglob('*')):
  if f.is_file() and f.name!='startnet.cmd':commands+='add "'+str(f)+'" /a04br3/'+str(f.relative_to(stage))+'\n'
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True);subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3','-B',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 manifest={'profile':PROFILE,'source_commit':a.source_commit,'service_source_commit':signed_source,'tooling_only_changes_since_signed_service':signed_source!=a.source_commit,'service':SERVICE,'policy':p,'native_prepared':native,'files':files,'esp_sha256':sha(out/'update-esp.img'),'wim_sha256':sha(image),'required_main_boot_transition':p['boot_transition'],'planned_windows_reboots':p['planned_reboots'],'no_gpu_or_engine_client':not any(c['phase']==1 for c in p['diagnostic_clients']),'engine_client_execute':False,'old_gpu_client_execute':False,'new_gpu_client_count':sum(c['phase']==1 for c in p['diagnostic_clients']),'software_or_userfs_rollback':False,'device_verified':False}
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
