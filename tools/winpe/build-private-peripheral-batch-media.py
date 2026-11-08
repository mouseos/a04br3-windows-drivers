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

def main():
 ap=argparse.ArgumentParser()
 for n in ('workspace','map','service-package','prepared-system','prepared-result','comparator','base','tools','out'):ap.add_argument('--'+n,type=Path,required=True)
 for n in ('prepared-sha256','comparator-manifest-sha256','source-commit'):ap.add_argument('--'+n,required=True)
 ap.add_argument('--enable-peripheral-batch',action='store_true');a=ap.parse_args();root=a.workspace.resolve(strict=True);here=Path(__file__).resolve().parent;repo=here.parents[1]
 require(a.enable_peripheral_batch,'専用profile明示opt-inが必要');require(re.fullmatch('[0-9a-f]{40}',a.source_commit),'固定source必須')
 require(subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()==a.source_commit and not subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True).strip(),'clean固定source必須')
 p=load_map(a.map,root);service=rooted(root,a.service_package);sm=json.loads((service/'peripheral-service-manifest.json').read_text(encoding='utf-8-sig'))
 require(sm.get('source_commit')==a.source_commit and sm.get('source_dirty') is False and sm.get('profile')==PROFILE and sm.get('service')==SERVICE and sm.get('signature_status')=='Valid' and sm.get('signer_thumbprint')==SIGNER,'同sourceの専用署名service必須')
 require(sm['policy']==p and sha(service/'peripheral-policy.json')==sm['policy_manifest_sha256'] and sha(service/(SERVICE+'.exe'))==sm['exe_sha256'] and (service/'peripheral-batch-policy.h').read_text()==generate(p),'signed policy/header/EXE不一致')
 backup=rooted(root,p['backup_manifest']);b=json.loads(backup.read_text());result=rooted(root,a.prepared_result);system=rooted(root,a.prepared_system)
 native=prepared_system_check(system,result,a.prepared_sha256,root,b,SERVICE);nr=json.loads(result.read_text(encoding='utf-8-sig'))
 require(nr.get('peripheral_preservation_plan_sha256')==sha(service/'peripheral-preservation-plan.json') and nr.get('peripheral_old_services_preserved') is True and not nr.get('engine_rearm_plan_sha256'),'新freshの専用native armと旧service保持必須')
 comp=rooted(root,a.comparator);cm=comp/'render-service-manifest.json';require(sha(cm)==a.comparator_manifest_sha256,'既存comparator manifest pin不一致');c=json.loads(cm.read_text(encoding='utf-8-sig'))['comparator'];require(c['signature_status']=='Valid' and c['signer_thumbprint']==SIGNER and sha(comp/'compare-files.exe')==c['exe_sha256'],'comparator署名/hash不一致')
 base=rooted(root,a.base);tools=rooted(root,a.tools);require(sha(base/'maintenance-staged.wim')=='0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4' and sha(base/'maintenance-staged-esp.img')=='22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce','既存guard付きWinPE baseのみ')
 out=a.out.resolve();private_path(out,root);require(not out.exists(),'新規root内出力のみ');out.mkdir(mode=0o700);stage=out/'stage';stage.mkdir();files={}
 def copy(src,name):
  q=stage/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,q);files[name]=sha(q)
 copy(comp/'compare-files.exe','compare-files.exe');copy(service/(SERVICE+'.exe'),'payload/'+SERVICE+'.exe');copy(system,'payload/SYSTEM.patched')
 for role,v in p['packages'].items():
  for kind in ('inf','sys','cat'):copy(root/v['directory']/v['names'][kind],'payload/'+role+'/'+v['names'][kind])
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
 # Duplicate immutable payload witnesses let the native EOF comparator check
 # every copied stage byte before SYSTEM arm; WIM and manifest remain sealed.
 payload=dict(files)
 for i,(n,h) in enumerate(sorted(payload.items())):
  copy(stage/n,'stage-proof/'+str(i)+'.bin');prepare+=['X:\\a04br3\\compare-files.exe "X:\\a04br3\\'+n.replace('/','\\')+'" X:\\a04br3\\stage-proof\\'+str(i)+'.bin >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto baselinefail']
 prepare+=['if exist C:\\Windows\\System32\\'+DIRECTORY+' goto baselinefail','if exist C:\\Windows\\System32\\'+SERVICE+'.exe goto baselinefail','set "A04_CANDIDATE_STARTED=1"','> "%A04_BK%\\JOURNAL.txt" echo STATE=CANDIDATE_WRITE_STARTED','mkdir C:\\Windows\\System32\\'+DIRECTORY+'\\logs >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 for role,v in p['packages'].items():
  dst='C:\\Windows\\System32\\'+DIRECTORY+'\\'+role;prepare+=['mkdir '+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
  for kind in ('inf','sys','cat'):
   name=v['names'][kind];prepare+=['copy /b X:\\a04br3\\payload\\'+role+'\\'+name+' '+dst+'\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\payload\\'+role+'\\'+name+' '+dst+'\\'+name+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 for name,dst in [('boot-transition-authorized.bin',DIRECTORY+'\\boot-transition-authorized.bin'),(SERVICE+'.exe',SERVICE+'.exe')]:
  prepare+=['copy /b X:\\a04br3\\payload\\'+name+' C:\\Windows\\System32\\'+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\payload\\'+name+' C:\\Windows\\System32\\'+dst+' >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 # Last mutation only: main-selected capture-derived clean SYSTEM containing the one new service.
 prepare+=['copy /b /y X:\\a04br3\\payload\\SYSTEM.patched C:\\Windows\\System32\\config\\SYSTEM >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail','X:\\a04br3\\compare-files.exe X:\\a04br3\\payload\\SYSTEM.patched C:\\Windows\\System32\\config\\SYSTEM >> "%A04_REPORT%" 2>&1','if errorlevel 1 goto candidatefail']
 cmd=stage_script((here/'run-mt8167-shared-io-update.cmd.in').read_text(),prepare,token[:16],p['backup_manifest_sha256']);(stage/'run-update-once.cmd').write_bytes(cmd.replace('\r','').replace('\n','\r\n').encode());files['run-update-once.cmd']=sha(stage/'run-update-once.cmd')
 copy(base/'stage/startnet.cmd','startnet.cmd');require(b'run-update-once.cmd' in (stage/'startnet.cmd').read_bytes(),'既存once guard startnet必須')
 image=out/'update.wim';shutil.copyfile(base/'maintenance-staged.wim',image);env=os.environ.copy();env.update(TMPDIR=str(stage),LD_LIBRARY_PATH=str(tools/'wimtools'),PYTHONPATH=str(tools/'python-fat'),PYTHONDONTWRITEBYTECODE='1')
 commands='add "'+str(stage/'startnet.cmd')+'" /Windows/System32/startnet.cmd\n'
 for f in sorted(stage.rglob('*')):
  if f.is_file() and f.name!='startnet.cmd':commands+='add "'+str(f)+'" /a04br3/'+str(f.relative_to(stage))+'\n'
 subprocess.run([str(tools/'wimtools/wimlib-imagex'),'update',str(image),'1'],input=commands,text=True,env=env,check=True);subprocess.run([str(tools/'wimtools/wimlib-imagex'),'verify',str(image)],env=env,check=True)
 subprocess.run(['python3','-B',str(here/'replace-esp-wim.py'),'--template',str(base/'maintenance-staged-esp.img'),'--wim',str(image),'--output',str(out/'update-esp.img'),'--template-bcd-sha256','65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d','--template-wim-sha256','0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'],env=env,check=True)
 manifest={'profile':PROFILE,'source_commit':a.source_commit,'service':SERVICE,'policy':p,'native_prepared':native,'files':files,'esp_sha256':sha(out/'update-esp.img'),'wim_sha256':sha(image),'required_main_boot_transition':p['boot_transition'],'planned_windows_reboots':1,'no_gpu_or_engine_client':True,'software_or_userfs_rollback':False,'device_verified':False}
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
