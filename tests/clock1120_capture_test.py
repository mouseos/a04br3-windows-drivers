#!/usr/bin/env python3
"""合成回収資料で厳密拒否とGPU失敗の分離を確認。実機証拠ではない。"""
import copy,hashlib,importlib.util,json,struct,sys
from pathlib import Path
sys.dont_write_bytecode=True
S=Path(__file__).resolve().parents[1];root=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();out.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('capture',S/'tools/clock/verify-clock1120-capture.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,indent=2)+'\n')
fpath=root/'port/xnu/out/windows-rt-clock1120-batch185-fragment/fragment/fragment.json';f=json.loads(fpath.read_text());p={'packages':f['packages'],'diagnostic_clients':f['diagnostic_clients_prepend']+[{'name':'gpu_present','phase':1}],'install_targets':['render','parent','thermal'],'data_directory':'MODEL_ONLY_CLOCK185','service':'MODEL_ONLY_CLOCK185'}
manifest=out/'capture/manifest.json';fixed=out/'fixed';policy=out/'policy.json';receipt=out/'receipt.json';save(policy,p);token=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest();m={'files':[]}
for n,b in [('Windows/System32/config/'+x,b'model-only-hive-'+x.encode()) for x in ['SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2']]+[('Windows/System32/Drivers/'+f['packages'][x]['names']['sys'],(root/f['packages'][x]['directory']/f['packages'][x]['names']['sys']).read_bytes()) for x in ['parent','thermal']]:
 q=manifest.parent/'files'/n;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(b);m['files'].append({'source_path':n,'backup_path':'files/'+n,'sha256':sha(q),'bytes':len(b)})
save(manifest,m)
def typed(kind,value):return {'present':True,'kind':kind,'value':value}
clients=[0]*72;clients[:9]=[1,0,0,1234,1,0,1,0,0];nodes=[0]*104
for idx in [1,6]:nodes[idx*8:idx*8+8]=[1,0,0,8,0,0,0,0]
# GPU Problem43 and global21 must not veto independent parent/thermal raw.
nodes[10*8:10*8+8]=[1,0,0,8,43,0,0,0]
values={k:typed('DWord',v) for k,v in {'Complete':1,'Schema':2,'NodeCount':13,'ClientCount':2,'RunId':123,'ServicePid':456,'ObservedResult':21}.items()};values.update(PolicyHash=typed('String',token),Clients=typed('Binary',struct.pack('<72I',*clients).hex()),Nodes=typed('Binary',struct.pack('<104I',*nodes).hex()))
r={'policy_sha256':token,'source_manifest_sha256':sha(manifest),'originals_unchanged':True,'load_rc':0,'unload_rc':0,'privilege_restore_error':0,'combined':{'Diagnostic':{'values':values}},'source_files':{x['source_path'].split('/')[-1]:x['sha256'] for x in m['files'] if '/config/' in x['source_path']},'nodes':{}}
for role,hwid,svc in [('parent','ACPI\\BEN0001','Mt8167Shared'),('thermal','MT8167\\Thermal','Mt8167Thermal')]:r['nodes'][hwid]={'present':True,'instances':[{'instance':'model','values':{'Service':typed('String',svc)},'selected_present':True,'selected':{'DriverVersion':typed('String',f['packages'][role]['driver_version']),'MatchingDeviceId':typed('String',hwid)}}]}
q=fixed/'Windows/System32/mt8167-clock-snapshot.exe';q.parent.mkdir(parents=True);q.write_bytes((root/f['diagnostic_clients_prepend'][0]['directory']/q.name).read_bytes())
save(receipt,r);base=fixed/'Windows/System32'/p['data_directory'];(base/'logs').mkdir(parents=True);raw=base/f['raw_relative_to_base_directory'];raw.write_bytes(struct.pack('<6I2Q22I',1,128,56,2047,2047,11,100,101,*([0]*22)));stdout=base/f['stdout_relative_to_base_directory'];stdout.write_text('SNAPSHOT_FLAGS=00000038\nVALID_FIRST=000007FF\nVALID_SECOND=000007FF\nREGISTER_COUNT=0000000B\nEFFECTIVE_CPU_HZ_KNOWN=00000000\nDDR_HZ_KNOWN=00000000\nCLOCK_SNAPSHOT_RC=00000000\n');stderr=base/f['stderr_relative_to_base_directory'];stderr.write_bytes(b'');(base/'logs/service.log').write_text('PB 00000000 00000286 0000007B\nPB 00000000 00000287 000001C8\nPB 00000000 00000220 000004D2\nPB 00000000 00000221 00000000\n')
def run():return mod.verify(root,fixed,manifest,receipt,policy,fpath)
result=run();assert result['raw_evidence_consistent'] and result['global_observed_result']==21 and not result['hardware_verified'] and result['effective_CPU_Hz'] is None
cases=1
for mutation in ['length','ABI','mask','flags','stdout_RC','stdout_duplicate','stderr','SYS','policy_binding','client_exit','node_error','selected_version','hive_binding','partial','time_reverse','actual_UM','PID_log']:
 originals={q:q.read_bytes() for q in [raw,stdout,stderr,receipt,manifest,policy,fixed/'Windows/System32/mt8167-clock-snapshot.exe',base/'logs/service.log']};rr=copy.deepcopy(r)
 if mutation=='length':raw.write_bytes(raw.read_bytes()[:-1])
 elif mutation in ['ABI','mask','flags','partial','time_reverse']:
  words=list(struct.unpack('<6I2Q22I',raw.read_bytes()));index,v={'ABI':(0,2),'mask':(3,4095),'flags':(2,0),'partial':(3,1),'time_reverse':(7,99)}[mutation];words[index]=v;raw.write_bytes(struct.pack('<6I2Q22I',*words))
  text=stdout.read_text();text=text.replace('SNAPSHOT_FLAGS=00000038',f'SNAPSHOT_FLAGS={words[2]:08X}').replace('VALID_FIRST=000007FF',f'VALID_FIRST={words[3]:08X}');stdout.write_text(text)
 elif mutation=='stdout_RC':stdout.write_text(stdout.read_text().replace('CLOCK_SNAPSHOT_RC=00000000','CLOCK_SNAPSHOT_RC=00000015'))
 elif mutation=='stdout_duplicate':stdout.write_text(stdout.read_text()+'CLOCK_SNAPSHOT_RC=00000000\n')
 elif mutation=='stderr':stderr.write_bytes(b'unexpected')
 elif mutation=='PID_log':(base/'logs/service.log').write_text((base/'logs/service.log').read_text().replace('000004D2','000004D3'))
 elif mutation=='actual_UM':(fixed/'Windows/System32/mt8167-clock-snapshot.exe').write_bytes(b'bad')
 elif mutation=='SYS':
  x=copy.deepcopy(m);x['files'][-1]['sha256']='0'*64;save(manifest,x)
 elif mutation=='policy_binding':rr['policy_sha256']='0'*64
 elif mutation=='client_exit':
  z=clients[:];z[7]=1;rr['combined']['Diagnostic']['values']['Clients']['value']=struct.pack('<72I',*z).hex()
 elif mutation=='node_error':
  z=nodes[:];z[6*8+6]=21;rr['combined']['Diagnostic']['values']['Nodes']['value']=struct.pack('<104I',*z).hex()
 elif mutation=='selected_version':rr['nodes']['ACPI\\BEN0001']['instances'][0]['selected']['DriverVersion']['value']='2026.11.8.1117'
 elif mutation=='hive_binding':rr['source_files']['SYSTEM']='0'*64
 save(receipt,rr)
 try:run()
 except ValueError:cases+=1
 else:raise AssertionError(mutation+' accepted')
 for q,b in originals.items():q.write_bytes(b)
# The new fragment must use exact source package() contracts from 183 without editing it.
source=root/'windows-rt/a04br3-windows-combined183-fresh182/tools/recovery/prepare-peripheral-followup182.py';sp=importlib.util.spec_from_file_location('oldpolicy',source);old=importlib.util.module_from_spec(sp);sp.loader.exec_module(old)
for role,row in f['packages'].items():assert old.package(root,row,role)==row;cases+=1
text=(source.parents[1]/'winpe/mt8167-peripheral-update.c').read_text();body=text[text.index('static DWORD run_clients('):text.index('/* Fixed auxiliary signed package:')]
assert 'if(c->phase==1)' in body and 'NULL,base_directory,&start,&process)' in body and 'combined_gpu_node_ready' in body
summary={'model_cases':cases,'GPU_problem43_global21_clock_evidence_checked':True,'clock_hardware_success':False,'real_raw_capture':False,'old183_package_parser_rows_exact':True,'service_runtime_source_changed':False,'effective_CPU_Hz':None,'DDR_Hz':None}
save(out/'result.json',summary);print(json.dumps(summary))
