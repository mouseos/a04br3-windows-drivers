#!/usr/bin/env python3
"""fresh128専用の後段結合。未完のpinを成功/arm可能へ変換しない。"""
import argparse,hashlib,json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import rooted,sha,read,require,cstr,cbytes,inftext
from codec_msdc_transaction import version,datevalue
from peripheral_next_policy import TARGETS as OLD_TARGETS
COMBINED_GPU_ADAPTER_READY=True
PROFILE='combined-peripheral-gpu327-fresh128-v1';SERVICE='MT8167CombinedFollowup128'
FRESH='bed13bd516128dd8df75a41e7bd68ecb3c5770f8394c73a70cceb7ced260a99d';BOOT='336b05be-a058-484b-b5fb-5c8e5842882e'
ROLES=('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render')
TARGETS={**OLD_TARGETS,'render':('ACPI\\MOU0006','GE8300Render','{4D36E968-E325-11CE-BFC1-08002BE10318}','ge8300-render.sys'),'audio':('MT8167\\AFE','Mt8167Audio','{4D36E96C-E325-11CE-BFC1-08002BE10318}','mt8167-audio.sys'),'keypad':('MT6392\\Buttons','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-keypad.sys'),'battery':('ACPI\\MOU0008','LC709203F','{72631E54-78A4-11D0-BCF7-00AA00B7B32A}','lc709203f.sys')}
def paths():
 b='C:\\Windows\\System32\\'+SERVICE
 return '\n'.join('#define '+k+' '+cstr(v) for k,v in [('A04_PERIPHERAL_SERVICE',SERVICE),('A04_PERIPHERAL_DIRECTORY',SERVICE),('A04_CAPTURE_DIRECTORY',b+'\\capture'),('A04_CAPTURE_LOG_PATH',b+'\\capture\\capture.log'),('A04_CAPTURE_MUTEX',SERVICE+'CaptureOnce'),('A04_WLAN_SCAN_LOG_PATH',b+'\\logs\\wlan-scan.txt'),('A04_WLAN_WIFI_ENUM_PATH','SYSTEM\\CurrentControlSet\\Enum\\A04BR3\\MT6630SDIO\\3&7ce9159&0&0')])+'\n'
def validate(root,path):
 m=read(path);require(m['schema']==1 and m['profile']==PROFILE and m['service']==m['data_directory']==SERVICE and m['hardware_owner']=='main' and m['hardware_operations_allowed'] is False,'Explicit followup124 contract required')
 def pin(e):
  p=rooted(root,e['path']);require(sha(p)==e['sha256'],'Pinned input mismatch');return p,read(p)
 f,fm=pin(m['fresh']);require(sha(f)==FRESH and fm['recovery_boot_id']==m['fresh']['recovery_boot_id']==BOOT and fm['status']=='captured-unmounted' and fm['unmounted'] and fm['read_only'] and fm['errors']==[],'Only current fresh128 eligible')
 files={x['source_path']:x for x in fm['files']};require(len(files)==32,'Exact fresh128 28 files required')
 for x in files.values():p=rooted(root,f.parent/x['backup_path']);require(sha(p)==x['sha256'] and p.stat().st_size==x['bytes'],'Original fresh128 changed')
 _,raw=pin(m['native_two']);require(raw['source_manifest_sha256']==FRESH and raw['errors']==[] and raw['originals_unchanged'] and all(raw[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Native cleanup/source failure')
 keypad=raw['nodes']['MT6392\\Buttons'];require(keypad['present'] and len(keypad['instances'])==1,'One actual Buttons node required');k=keypad['instances'][0];v=k['values'];sel=k.get('selected',{})
 require(k['instance']=='3&7ce9159&0&0' and k['selected_present'] and v['Service']=={'present':True,'kind':'String','value':'mshidkmdf'} and v['ClassGUID']=={'present':True,'kind':'String','value':'{745a17a0-74d3-11d0-b6fe-00a0c90f57da}'} and v['LowerFilters']=={'present':True,'kind':'MultiString','value':['Mt6392Keys']} and v['HardwareID']=={'present':True,'kind':'MultiString','value':['MT6392\\Buttons']} and sel['DriverVersion']['value']=='0.0.1.2' and sel['InfPath']['value']=='oem138.inf','Only actual known keypad1.2 selection; no unknown provider accepted')
 battery=raw['nodes']['ACPI\\MOU0008']['instances'];require(len(battery)==1 and battery[0]['instance']=='0' and battery[0]['selected_present'] and battery[0]['selected']['DriverVersion']['value']=='0.0.1.0' and battery[0]['selected']['InfPath']['value']=='oem53.inf','Actual battery1.0 baseline required')
 _,bindings=pin(m['bindings_eleven']);require(bindings['source_manifest_sha256']==FRESH and bindings['errors']==[] and all(bindings[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Same current native selections required')
 gp=rooted(root,m['gpu']['ledger_proof']);g=read(gp);require(sha(gp)==m['gpu']['ledger_proof_sha256'] and g['errors']==[] and g['originals_unchanged'] and g['old_gpu_service']['Start']['value']==4 and m['gpu']['start']==4 and m['gpu']['client_execute'] is False,'GPU Start4/full121 ledger must remain intact')
 require(set(m['packages'])==set(ROLES) and m['install_targets']==['battery','audio','render'],'Only the explicitly pinned four adopted roles; root fixes require a new reviewed policy revision')
 packages={};baselines={}
 for role in ROLES:
  e=m['packages'][role];d=rooted(root,e['directory']);mp=rooted(root,e['manifest']);require(sha(mp)==e['manifest_sha256'],'Signed package manifest mismatch');pm=read(mp)
  require(pm.get('package_source_commit')==e['source_commit'] if role=='render' else pm['source_commit']==e['source_commit'],'Package/source mismatch');members={x['sha256'].lower() for x in pm.get('files',pm.get('outputs',[]))};hs={}
  for kind,n in e['names'].items():require(kind in ('inf','sys','cat') and re.fullmatch('[A-Za-z0-9_.-]+',n),'Fixed trio leaves only');hs[kind]=sha(d/n);require(hs[kind] in members,'Package member absent from signed manifest')
  require(e['names']['sys']==TARGETS[role][3],'Exact role SYS leaf required');inf=inftext(d/e['names']['inf']);require('DriverVer='+e['driver_date']+','+e['driver_version'] in inf and TARGETS[role][0].lower() in inf.lower(),'Exact INF version/date/HWID required')
  require(set(hs)=={'inf','sys','cat'},'Exact trio required');packages[role]={**e,'hashes':hs,'version_u64':version(e['driver_version']),'date_filetime':datevalue(e['driver_date'])}
  if role=='render':
   require([x['name'] for x in e['extra_members']]==['ge8300-umd.dll','ge8300-mips.fw'] and [x['installed'] for x in e['extra_members']]==['ge8300-umd.dll','Drivers\\ge8300-mips.fw'],'Exact327 extra files only')
   for x in e['extra_members']:require(sha(d/x['name'])==x['sha256'] and x['sha256'] in members,'Extra CAT package member hash mismatch')
  if role=='battery':baselines[role]={'mode':'selected','version_u64':version('0.0.1.0'),'date_filetime':datevalue('11/04/2026'),'sys_sha256':'0'*64,'inf':'oem53.inf'}
  elif role in ('audio','render') and role in m['install_targets']:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])<packages[role]['version_u64'],'Exact older installed role required');baselines[role]={'mode':'selected','version_u64':version(node[0]['selected']['DriverVersion']),'date_filetime':datevalue(node[0]['selected']['DriverDate'].replace('-','/')),'sys_sha256':files['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256'],'inf':node[0]['selected']['InfPath']}
  else:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])==packages[role]['version_u64'] and files['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256']==hs['sys'],'Retained roles must be current exact bytes/version; never reinstall/downgrade');baselines[role]={'mode':'retained','version_u64':packages[role]['version_u64'],'date_filetime':packages[role]['date_filetime'],'sys_sha256':hs['sys'],'inf':node[0]['selected']['InfPath']}
 # Independent candidate/source and full baseline44 joins, never version-only.
 for role in m['install_targets']:
  e=m['packages'][role];require(e.get('main_seal') is not None,'Changed role needs explicit main seal');pin(e['main_seal'])
 gp=gpu=m['candidate_gpu_suite'];mp=rooted(root,gpu['manifest']);pm=read(mp)
 require(sha(mp)==gpu['manifest_sha256']=='7e4ccf146cb8081a93e85b7f83ac24a50ea6f66f8888f4403ccaf08f19433663' and pm['schema']=='texture327-independent-sources-v1' and pm['package_source_commit']==gpu['package_source']=='41cc2a01cdcab1b06f10268860d4d2550b66466d' and pm['kernel_binary_source_commit']==pm['native_owner_source_commit']==pm['umd_source_commit']==pm['new_client_source_commit']==gpu['binary_source']=='df849a21f4d6391e7674ee11f34efbb17b6d39d3','Exact327 package and independently attributed binary sources required')
 require(pm['adapter_contract_version']==4 and pm['driver_wire_version']==3 and pm['sampler_profile_version']==4 and pm['texture_storage_version']==2 and pm['new_client_sha256']==gpu['client_sha256']=='2727328f3072d9cfcacc4758acf3701c7795471c76860eeac70206c82d759206','327 adapter4/wire3/client tuple mismatch')
 for name,key,h in [('texture327-native-dependency-closure.json','native_dependency_closure_sha256','0e31786dcb038f2e76a2110eba44eb11f14267a0afb98f5e5c4b1fb94d10c03d'),('texture327-build-manifest.json','build_manifest_sha256','1c74110363c68d2270513d3754f06f554632b36d184fa3803290f8cd07fc47c6')]:
  f=rooted(root,Path(gpu['directory'])/name);require(sha(f)==h and pm[key]==h,'327 closure/build pin mismatch')
 _,gs=pin(gpu['main_seal']);require(gs['source_commit']==gpu['package_source'] and gs['signed_manifest_sha256']==sha(mp) and gs['outputs_verified']==8 and gs['new_client_verified'] and gs['retained_firmware_certificate_old_client_verified'],'Main327 independent seal mismatch')
 pm=read(rooted(root,m['packages']['parent']['manifest']));require(pm['source_commit']=='97b87a4b0d226eb093507cd5b886acfdbf2c0391' and pm['native_interface_version']==3 and pm['native_interface_parent_build']==1104 and pm['child_resource_count']==0 and pm['accdet_status_ioctl']=='0x00226558' and pm['accdet_qualification_relaxed'] is False,'Atomic1104/thermal18 V3 ABI1104 pair required')
 mb=m['matched_boot'];bf=rooted(root,mb['path']);require(sha(bf)==mb['sha256']=='7a10525ccdf69b89ba7dc9d6f5245784ea34effb7b2aa9ccf7694c2e32271a32','DSB matchedboot pin mismatch');pin(mb['main_seal'])
 bp,bm=pin(m['gpu_client_log_baseline']);require(bm['baseline_count']==44 and bm['fresh_manifest_sha256']==FRESH and bm['previous44_preserved'] and bm['run127_gpu_client_executed'] is False and len(bm['files'])==44,'Current44 log baseline required')
 for name,h in bm['files'].items():require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name) and sha(rooted(root,Path(bm['source_directory'])/name))==h,'Baseline44 actual file mismatch')
 ex=m['unified_execution'];require(ex['install_phase_timeout_seconds']==900 and ex['postreboot_diagnostic_total_timeout_seconds']==1500 and ex['gpu_client_timeout_seconds']==900 and ex['rotation_client_timeout_seconds']==300,'Explicit final phase budgets required')
 require(m['diagnostics']['gpu_suite']['arguments']==['--hardware=PowerVR'] and m['diagnostics']['gpu_suite']['phase']==1 and m['diagnostics']['gpu_suite']['timeout_ms']==900000,'Only one new hardware327 suite last; no old325 client')
 blockers=list(m['blockers']);baselines['battery']['sys_sha256']=files['Windows/System32/Drivers/lc709203f.sys']['sha256']
 return m,packages,baselines,blockers

def header(packages,baselines,policy=None):
 lines=['/* Fixed fresh128 profile; preview remains non-armable. */','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(k) for k in CURRENT_INSTALL))+'u','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0x0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0x0u','#define A04_UNBOUND_KEYPAD_CONFIG 0x60u','static const char deferred_token[]="'+(hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest() if policy is not None else 'DRAFT-NOT-ARMED')+'";','static const struct peripheral_target targets[11]={']
 for role in ROLES:
  t=TARGETS[role];p=packages[role];b=baselines[role];lines.append('{'+','.join(cstr(x) for x in (t[0],t[1],t[2],role,p['names']['inf'],t[3],p['names']['cat']))+','+str(p['version_u64'])+'ULL,'+str(p['date_filetime'])+'ULL,{'+','.join(cbytes(p['hashes'][k]) for k in ('inf','sys','cat'))+'},'+str(b['version_u64'])+'ULL,'+str(b['date_filetime'])+'ULL,'+cbytes(b['sys_sha256'])+','+cstr(b['inf'])+'},')
 lines+=['};','#define A04_CANDIDATE_EXTRA_COUNT 2u','static const struct candidate_extra_file candidate_extras[2]={']
 for e in packages['render']['extra_members']:lines.append('{'+cstr(e['name'])+','+cstr(e['installed'])+','+cbytes(e['sha256'])+','+cbytes(e['baseline_sha256'])+'},')
 lines+=['};','static const struct preserved_file preserved_files[]={']
 for name,h in CURRENT_PRESERVED.items():lines.append('{'+cstr(name[len('Windows/System32/'):].replace('/','\\'))+','+cbytes(h)+'},')
 lines+=['};','static const struct preserved_service preserved_services[]={']
 for name,v in CURRENT_OLD_SERVICES.items():lines.append('{'+cstr(name)+','+str(int(v['present']))+','+cbytes(v['root_sha256'] if v['present'] else '0'*64)+'},')
 lines+=['};','static const struct diagnostic_client diagnostic_clients[]={']
 for v in CURRENT_CLIENTS:lines.append('{'+cstr(v['exe'])+','+cstr(' '.join(v['arguments']))+','+cstr(v['name'])+','+cstr(v.get('relative_directory',SERVICE+'\\clients'))+','+cbytes(v['sha256'])+','+str(v.get('timeout_ms',10000))+'u,'+str(v.get('phase',0))+'u},')
 return '\n'.join(lines+['};'])+'\n'

def build_policy(root,path,finalize=False):
 root=Path(root).resolve(strict=True);m,pkgs,base,blocks=validate(root,rooted(root,path))
 if finalize:require(COMBINED_GPU_ADAPTER_READY and not blocks and m['draft'] is False and m['normal_install_allowed'] is True and base['battery']['sys_sha256']!='0'*64 and m['diagnostics']['wifi_scan']['client_package'] is not None,'Finalization blocked by actual witness or final candidate validation')
 global CURRENT_PRESERVED,CURRENT_OLD_SERVICES,CURRENT_CLIENTS,CURRENT_INSTALL
 CURRENT_INSTALL=m['install_targets']
 fm=read(rooted(root,m['fresh']['path']));new_sys={TARGETS[k][3] for k in m['install_targets']}
 CURRENT_PRESERVED={e['source_path']:e['sha256'] for e in fm['files'] if (e['source_path'].startswith('Windows/System32/Drivers/') and e['source_path'].split('/')[-1] not in new_sys) or e['source_path']=='Windows/System32/MT8167EngineProbe.exe'}
 for name in ('Windows/System32/MT8167CombinedFollowup124.exe','Windows/System32/MT8167CombinedFollowup126.exe','Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw'):
  e=next((e for e in fm['files'] if e['source_path']==name),None)
  witness=rooted(root,Path(m['fresh']['path']).parent/e['backup_path']) if e else rooted(root,('port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/' if name=='Windows/System32/MT8167RenderSingleGpu.exe' else 'port/xnu/out/private-windows-rt-combined-run127-diagnostics/fixed/')+name)
  if name!='Windows/System32/ge8300-umd.dll':CURRENT_PRESERVED[name]=sha(witness)
 ne=m['native_baseline'];np=rooted(root,ne['path']);require(sha(np)==ne['sha256'],'Old service snapshot pin mismatch');n=read(np);require(n['source_manifest_sha256']==FRESH and n['errors']==[] and n['originals_unchanged'] and set(n['old_services'])==set(m['old_services']),'Seven old service snapshot source mismatch');CURRENT_OLD_SERVICES=n['old_services']
 CURRENT_CLIENTS=[]
 for name in ('battery_temperature','accdet_status','afe_jack','rotation','wlan_scan','gpu_suite'):
  v=m['diagnostics'][name];require(sha(rooted(root,v['directory'])/v['exe'])==v['sha256'],'Signed client byte pin mismatch')
  for dep in v.get('dependencies',[]):require(sha(rooted(root,v['directory'])/dep['exe'])==dep['sha256'],'Signed client dependency byte pin mismatch')
  CURRENT_CLIENTS.append({**v,'name':name})
 preserved_sources={e['source_path']:(Path(m['fresh']['path']).parent/e['backup_path']).as_posix() for e in fm['files'] if e['source_path'] in CURRENT_PRESERVED}
 for name in CURRENT_PRESERVED:
  if name not in preserved_sources:preserved_sources[name]=('port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/' if name=='Windows/System32/MT8167RenderSingleGpu.exe' else 'port/xnu/out/private-windows-rt-combined-run127-diagnostics/fixed/')+name
 clients=[{**v,'relative_directory':v.get('relative_directory',SERVICE+'\\clients'),'dependencies':v.get('dependencies',[])} for v in CURRENT_CLIENTS]
 policy={'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':SERVICE,'draft':not finalize,'normal_install_allowed':bool(finalize),'backup_manifest':m['fresh']['path'],'backup_manifest_sha256':FRESH,'recovery_boot_id':BOOT,'capture_selection':{'schema':1,'profile':PROFILE,'capture_number':128,'manifest_sha256':FRESH,'recovery_boot_id':BOOT,'gpu_policy_sha256':m['gpu']['policy_sha256'],'unchanged_since_capture':True},'source_files':{n:{k:next(e for e in fm['files'] if e['source_path']=='Windows/System32/config/'+n)[k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'packages':pkgs,'baselines':base,'service_preservation':CURRENT_OLD_SERVICES,'preserved_files':CURRENT_PRESERVED,'preserved_sources':preserved_sources,'planned_reboots':1,'install_targets':m['install_targets'],'stage_order':list(ROLES),'first_install_mask':0,'deferred_unbound_mask':0,'battery_baseline_sys_sha256':base['battery']['sys_sha256'],'diagnostic_clients':clients,'gpu_install':True,'engine_retest':False,'device_verified':False,'boot_transition':{'mode':'preserve-current-matched8range','boot_path':m['matched_boot']['path'],'boot_sha256':m['matched_boot']['sha256'],'main_handoff_required':True,'boot_write_required':False,'gpt_written':False}}
 policy['observer_contract']=m['unified_execution'];policy['gpu_client_log_baseline']=m['gpu_client_log_baseline'];policy['gpu_new_log_contract']=m['gpu_new_log_contract'];policy['gpu_candidate']=m['candidate_gpu_suite'];policy['prior_rotation_log']=None;policy['reuse_rotation_directory_without_prior_active_log']=True;policy['wlan_fixed_log_directory']='MT8167CombinedFollowup124';policy['keypad_deferred_unbound']=False
 policy['prior_rotation_executables']=[]
 for name in ('mt8167-display-rotation-probe.exe','mt8167-display-rotation-session.exe','mt8167-display-rotation-user.exe'):
  source='Windows/System32/MT8167DisplayRotationProbeV1/'+name;witness='port/xnu/out/mt8167-display-rotation-jobfix-20261108/package/'+name
  policy['prior_rotation_executables'].append({'source':source,'witness':witness,'sha256':sha(rooted(root,witness))})
 policy['baseline_witnesses']={'Windows/System32/ge8300-umd.dll':{'path':'port/xnu/out/private-windows-rt-combined-run127-diagnostics/fixed/Windows/System32/ge8300-umd.dll','sha256':'716398511b3e1ed737c96a621c9083ab54d1274f6263b700335af167ac3d3815'}}
 files_lc=next(e for e in fm['files'] if e['source_path']=='Windows/System32/Drivers/lc709203f.sys')
 policy['baseline_witnesses']['Windows/System32/Drivers/lc709203f.sys']={'path':(Path(m['fresh']['path']).parent/files_lc['backup_path']).as_posix(),'sha256':base['battery']['sys_sha256']}
 require([c.get('phase',0) for c in clients]==[0,0,0,0,0,1],'Only one GPU client last after five peripheral clients')
 return policy,m,blocks

def load_map(path,root):return build_policy(root,path,True)[0]
def generate(p):
 global CURRENT_PRESERVED,CURRENT_OLD_SERVICES,CURRENT_CLIENTS,CURRENT_INSTALL
 require(p['profile']==PROFILE and p['service']==SERVICE and not p['draft'] and p['normal_install_allowed'],'Final fixed profile only')
 CURRENT_PRESERVED=p['preserved_files'];CURRENT_OLD_SERVICES=p['service_preservation'];CURRENT_CLIENTS=p['diagnostic_clients'];CURRENT_INSTALL=p['install_targets']
 return header(p['packages'],p['baselines'],p)
def assert_paths(image,capture=False):
 b=Path(image).read_bytes();require((SERVICE+'.exe').encode('utf-16le') in b and SERVICE.encode('utf-16le') in b,'Compiled service/data UTF16 identity missing')
 require('ge8300-d3d-texture.exe'.encode('utf-16le') in b,'New327 client missing')
def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args();root=x.workspace.resolve(strict=True);p,m,blocks=build_policy(root,x.map,x.finalize);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root output required');out.mkdir(mode=0o700)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(header(p['packages'],p['baselines'],p if x.finalize else None))
 for n in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/n).write_text(json.dumps(p,indent=2)+'\n')
 (out/'validation.json').write_text(json.dumps({'schema':1,'profile':PROFILE,'draft':not x.finalize,'normal_install_allowed':x.finalize,'blockers':blocks,'device_verified':False},indent=2)+'\n');print('COMBINED128_FINAL_INPUT_VALIDATED' if x.finalize else 'COMBINED128_DRAFT_VALIDATED_NOT_ARMABLE')
if __name__=='__main__':main()
