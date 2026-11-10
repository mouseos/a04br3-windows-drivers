#!/usr/bin/env python3
"""main回収済みローカル資料だけを照合。mount/ADB/実機/clock制御なし。"""
import argparse,hashlib,importlib.util,json,re,struct
from pathlib import Path

def require(ok,msg):
 if not ok:raise ValueError(msg)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def bounded(p,limit):
 require(p.is_file() and not p.is_symlink() and p.stat().st_size<=limit,'missing/symlink/oversize: '+str(p));return p.read_bytes()
def verify(root,fixed,manifest,receipt,policy,fragment):
 root=root.resolve(strict=True)
 for p in [fixed,manifest,receipt,policy,fragment]:require(p.resolve().is_relative_to(root),'input outside root')
 f=read(fragment);p=read(policy);r=read(receipt);m=read(manifest);evidence={}
 for e in f['input_pins']:require(sha(root/e['path'])==e['sha256'],'fragment input drift')
 c=p['diagnostic_clients'];require(2<=len(c)<=8 and c[0]==f['diagnostic_clients_prepend'][0],'clock must be exact client0')
 require(c[-1]['phase']==1 and c[-1]['name']=='gpu_present' and all(x['phase']==0 for x in c[:-1]),'GPU must be sole phase1 last')
 for role,row in f['packages'].items():
  require(all(p['packages'][role][k]==v for k,v in row.items()),'package row drift: '+role)
 require(set(['parent','thermal'])<=set(p['install_targets']),'missing explicit DIF bits')
 canonical=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
 require(r['policy_sha256']==canonical and r['source_manifest_sha256']==sha(manifest),'reader binding mismatch')
 require(r['originals_unchanged'] and r['load_rc']==r['unload_rc']==r['privilege_restore_error']==0,'reader not safely completed')
 # GPU/peripheral global errors may coexist with valid independent clock evidence.
 values=r['combined']['Diagnostic']['values']
 def val(k,kind):
  x=values[k];require(x['present'] and x['kind']==kind,'missing typed CombinedResult '+k);return x['value']
 require(val('Complete','DWord')==1 and val('Schema','DWord')==2 and val('PolicyHash','String')==canonical,'incomplete/stale CombinedResult')
 require(val('NodeCount','DWord')==13 and val('ClientCount','DWord')==len(c),'snapshot counts mismatch')
 clients=bytes.fromhex(val('Clients','Binary'));nodes=bytes.fromhex(val('Nodes','Binary'));require(len(clients)==8*36 and len(nodes)==13*32,'snapshot ABI lengths')
 row=struct.unpack_from('<9I',clients);require(row[0:3]==(1,0,0) and row[3]>0 and row[3]!=0xffffffff and row[4:]==(1,0,1,0,0),'client0 did not complete successfully')
 # No requirement on ObservedResult or GPU slot: GPU readiness cannot veto clock.
 for role,index in f['role_indices'].items():
  n=struct.unpack_from('<8I',nodes,index*32);require(n[0]==1 and n[1]==n[2]==n[4]==n[5]==n[6]==0 and n[7] in (0,2) and n[3]&8,'clock parent/thermal node not qualified')
  hwid,service=('ACPI\\BEN0001','Mt8167Shared') if role=='parent' else ('MT8167\\Thermal','Mt8167Thermal')
  matches=r['nodes'][hwid];require(matches['present'] and len(matches['instances'])==1,'ambiguous selected node')
  instance=matches['instances'][0];require(instance['selected_present'],'no selected metadata')
  def selected_string(fields,key):
   x=fields[key];require(x['present'] and x['kind']=='String' and isinstance(x['value'],str),'missing typed selected string '+key);return x['value']
  require(selected_string(instance['values'],'Service').lower()==service.lower(),'selected service mismatch')
  selected=instance['selected'];require(selected_string(selected,'DriverVersion')==f['packages'][role]['driver_version'],'selected version mismatch')
  require(selected_string(selected,'MatchingDeviceId').lower()==hwid.lower(),'selected HWID mismatch')
  if n[7]==2:
   # This helper admits only the observed ERROR_FILE_NOT_FOUND, never all
   # errors accepted by some future caller. Locator-stage proof follows below.
   require(selected_string(selected,'InfPath').lower()==f['packages'][role]['names']['inf'].lower(),'exact2 selected INF mismatch')
   require(selected_string(selected,'InfSection')=='Install','exact2 selected section mismatch')
   date='-'.join(str(int(x)) for x in f['packages'][role]['driver_date'].split('/'))
   require(selected_string(selected,'DriverDate')==date,'exact2 selected date mismatch')
   require(selected_string(instance['values'],'ClassGUID').lower()=='{4d36e97d-e325-11ce-bfc1-08002be10318}','exact2 selected class mismatch')
  evidence[role]={'instance':instance['instance'],'selected':selected,'node_record':list(n),'inf_location_error':n[7],'optional_locator_missing_exact2':n[7]==2}
 files={x['source_path']:x for x in m['files']}
 for name in ['SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2']:
  x=files['Windows/System32/config/'+name];q=manifest.parent/x['backup_path'];require(q.resolve().is_relative_to(manifest.parent.resolve()),'capture path escape');require(sha(q)==x['sha256']==r['source_files'][name],'hive receipt differs from actual capture')
 for role in ['parent','thermal']:
  name=f['packages'][role]['names']['sys'];x=files['Windows/System32/Drivers/'+name];q=manifest.parent/x['backup_path'];require(q.resolve().is_relative_to(manifest.parent.resolve()),'capture path escape');require(sha(q)==x['sha256']==f['packages'][role]['hashes']['sys'],'actual SYS mismatch')
 actual_exe=fixed/'Windows/System32'/c[0]['exe'];bounded(actual_exe,1048576);require(sha(actual_exe)==c[0]['sha256'],'actual UM executable mismatch')
 directory=p['data_directory'];require(re.fullmatch(r'[A-Za-z0-9_-]{1,100}',directory) and directory==p['service'],'service data directory mismatch')
 base=fixed/'Windows/System32'/directory
 service_log=base/'logs/service.log';logtext=bounded(service_log,1048576).decode('ascii');runid=val('RunId','DWord');servicepid=val('ServicePid','DWord')
 header=f'PB 00000000 00000286 {runid:08X}\nPB 00000000 00000287 {servicepid:08X}\n';logtext=logtext.replace('\r\n','\n');require(logtext.count(header)==1,'ambiguous/missing service RunId/Pid log boundary')
 segment=logtext.split(header,1)[1].split('PB 00000000 00000286 ',1)[0]
 started=f'PB 00000000 00000220 {row[3]:08X}\n';ended='PB 00000000 00000221 00000000\n';require(segment.count(started)==segment.count(ended)==1 and segment.index(started)<segment.index(ended),'client0 PID/exit log mismatch')
 for role,index in f['role_indices'].items():
  if evidence[role]['inf_location_error']!=2:continue
  events={}
  for event in [0x255,0x253,0x219,0x21a]:
   matches=list(re.finditer(rf'^PB {index:08X} {event:08X} ([0-9A-F]{{8}})$',segment,re.M));require(len(matches)==1,'exact2 missing/ambiguous locator provenance')
   events[event]=(int(matches[0][1],16),matches[0].start())
  stage=events[0x255][0]
  require(stage in (2,3,4,5,6,8) and [events[x][0] for x in [0x253,0x219,0x21a]]==[2,0,0],'exact2 not a source-qualified optional locator failure')
  require([events[x][1] for x in [0x255,0x253,0x219,0x21a]]==sorted(events[x][1] for x in [0x255,0x253,0x219,0x21a]),'exact2 provenance order mismatch')
  evidence[role]['locator_probe_stage']=stage
 rawpath=base/f['raw_relative_to_base_directory'];raw=bounded(rawpath,128);require(len(raw)==128,'raw length not128')
 outpath=base/f['stdout_relative_to_base_directory'];text=bounded(outpath,4096).decode('ascii');errpath=base/f['stderr_relative_to_base_directory'];require(bounded(errpath,4096)==b'','stderr not empty')
 stdout={}
 for line in text.splitlines():
  match=re.fullmatch(r'([A-Z_]+)=([0-9A-F]{8})',line);require(match is not None,'unknown stdout line');key,value=match.groups();require(key not in stdout,'duplicate stdout field');stdout[key]=int(value,16)
 v,size,flags,a,b,count=struct.unpack_from('<6I',raw)
 require(stdout=={'SNAPSHOT_FLAGS':flags,'VALID_FIRST':a,'VALID_SECOND':b,'REGISTER_COUNT':count,'EFFECTIVE_CPU_HZ_KNOWN':0,'DDR_HZ_KNOWN':0,'CLOCK_SNAPSHOT_RC':0},'stdout/raw disagreement')
 spec=importlib.util.spec_from_file_location('clockdecoder',Path(__file__).resolve().parents[1]/'cpu/decode-mt8167-clock-snapshot.py');decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder);decoded=decoder.decode(raw)
 require(decoded['complete'] and (v,size,count)==(1,128,11),'partial/error snapshot not successful evidence')
 require(0xffffffff not in decoded['first'].values() and 0xffffffff not in decoded['second'].values(),'all-ones raw rejected')
 require(bool(flags&1)==(decoded['first']!=decoded['second']),'UNSTABLE flag inconsistent with raw sweeps')
 return {'schema':'MT8167_CLOCK1120_CAPTURE_CHECK_V1','raw_evidence_consistent':True,'raw_sha256':hashlib.sha256(raw).hexdigest(),'raw_capture_path':str(rawpath.relative_to(root)),'service_log_sha256':sha(service_log),'stdout_sha256':sha(outpath),'stderr_sha256':sha(errpath),'manifest_sha256':sha(manifest),'reader_receipt_sha256':sha(receipt),'policy_file_sha256':sha(policy),'policy_token':canonical,'CombinedRunId':val('RunId','DWord'),'ServicePid':val('ServicePid','DWord'),'ClockPid':row[3],'global_observed_result':val('ObservedResult','DWord'),'actual_selected_tuple':evidence,'decoded':decoded,'effective_CPU_Hz':None,'DDR_Hz':None,'sameboot_UART_and_fresh_directory_proof_pending_main':True,'hardware_verified':False,'helper_executed_ARM':False}
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__)
 for k in ['root','fixed','manifest','receipt','policy','fragment','out']:ap.add_argument('--'+k,type=Path,required=True)
 a=ap.parse_args();require(a.out.resolve().is_relative_to(a.root.resolve()/'port/xnu/out') and not a.out.exists(),'new root/out output required');a.out.mkdir(parents=True)
 try:result=verify(a.root,a.fixed,a.manifest,a.receipt,a.policy,a.fragment)
 except (ValueError,KeyError,TypeError,OSError) as e:
  (a.out/'rejected.json').write_text(json.dumps({'rejected':True,'reason':str(e),'hardware_verified':False},indent=2)+'\n');ap.exit(1,str(e)+'\n')
 rawpath=a.root.resolve()/result['raw_capture_path']
 collected={}
 for name,source in [('raw128.bin',rawpath),('clock_snapshot1120.stdout.txt',rawpath.parent/'logs/clock_snapshot1120.stdout.txt'),('clock_snapshot1120.stderr.txt',rawpath.parent/'logs/clock_snapshot1120.stderr.txt'),('service.log',rawpath.parent/'logs/service.log')]:
  target=a.out/name;target.write_bytes(source.read_bytes());collected[name]=sha(target)
 result['collected_files']=collected
 (a.out/'checked.json').write_text(json.dumps(result,indent=2)+'\n')
