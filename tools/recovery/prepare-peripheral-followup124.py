#!/usr/bin/env python3
"""fresh124専用の後段結合。未完のpinを成功/arm可能へ変換しない。"""
import argparse,hashlib,json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import rooted,sha,read,require,cstr,cbytes
from codec_msdc_transaction import version,datevalue
from peripheral_next_policy import TARGETS as OLD_TARGETS
COMBINED_GPU_ADAPTER_READY=False
PROFILE='combined-peripheral-gpu327-fresh124-v1';SERVICE='MT8167CombinedFollowup124'
FRESH='aef54b0e2d3d70c083626c76d1c18cfe16b1eff8ecd6237233e83e257bd801da';BOOT='0adf9f8e-8773-47b3-8d2d-e8440426f9e7'
ROLES=('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio')
TARGETS={**OLD_TARGETS,'audio':('MT8167\\AFE','Mt8167Audio','{4D36E96C-E325-11CE-BFC1-08002BE10318}','mt8167-audio.sys'),'keypad':('MT6392\\Buttons','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-keypad.sys'),'battery':('ACPI\\MOU0008','LC709203F','{72631E54-78A4-11D0-BCF7-00AA00B7B32A}','lc709203f.sys')}
def paths():
 b='C:\\Windows\\System32\\'+SERVICE
 return '\n'.join('#define '+k+' '+cstr(v) for k,v in [('A04_PERIPHERAL_SERVICE',SERVICE),('A04_PERIPHERAL_DIRECTORY',SERVICE),('A04_CAPTURE_DIRECTORY',b+'\\capture'),('A04_CAPTURE_LOG_PATH',b+'\\capture\\capture.log'),('A04_CAPTURE_MUTEX',SERVICE+'CaptureOnce'),('A04_WLAN_SCAN_LOG_PATH',b+'\\logs\\wlan-scan.txt'),('A04_WLAN_WIFI_ENUM_PATH','SYSTEM\\CurrentControlSet\\Enum\\A04BR3\\MT6630SDIO\\3&7ce9159&0&0')])+'\n'
def validate(root,path):
 m=read(path);require(m['schema']==1 and m['profile']==PROFILE and m['service']==m['data_directory']==SERVICE and m['hardware_owner']=='main' and m['hardware_operations_allowed'] is False,'Explicit followup124 contract required')
 def pin(e):
  p=rooted(root,e['path']);require(sha(p)==e['sha256'],'Pinned input mismatch');return p,read(p)
 f,fm=pin(m['fresh']);require(sha(f)==FRESH and fm['recovery_boot_id']==m['fresh']['recovery_boot_id']==BOOT and fm['status']=='captured-unmounted' and fm['unmounted'] and fm['read_only'] and fm['errors']==[],'Only current fresh124 eligible')
 files={x['source_path']:x for x in fm['files']};require(len(files)==28,'Exact fresh124 28 files required')
 for x in files.values():p=rooted(root,f.parent/x['backup_path']);require(sha(p)==x['sha256'] and p.stat().st_size==x['bytes'],'Original fresh124 changed')
 _,raw=pin(m['native_two']);require(raw['source_manifest_sha256']==FRESH and raw['errors']==[] and raw['originals_unchanged'] and all(raw[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Native cleanup/source failure')
 keypad=raw['nodes']['MT6392\\Buttons'];require(keypad['present'] and len(keypad['instances'])==1,'One actual Buttons child required');k=keypad['instances'][0];v=k['values'];require(k['instance']=='3&7ce9159&0&0' and not k['selected_present'] and v['HardwareID']=={'kind':'MultiString','value':['MT6392\\Buttons'],'present':True} and v['ConfigFlags']=={'kind':'DWord','value':64,'present':True} and all(v[n]=={'present':False} for n in ('Service','ClassGUID','Driver')) and raw['services']['Mt6392Keys']=={'present':False},'Only exact observed unbound keypad shape can opt into first install')
 battery=raw['nodes']['ACPI\\MOU0008']['instances'];require(len(battery)==1 and battery[0]['instance']=='0' and battery[0]['selected_present'] and battery[0]['selected']['DriverVersion']['value']=='0.0.1.0' and battery[0]['selected']['InfPath']['value']=='oem53.inf','Actual battery1.0 baseline required')
 _,bindings=pin(m['bindings_eleven']);require(bindings['source_manifest_sha256']==FRESH and bindings['errors']==[] and all(bindings[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Same current native selections required')
 gp=rooted(root,m['gpu']['ledger_proof']);g=read(gp);require(sha(gp)==m['gpu']['ledger_proof_sha256'] and g['same_gpu_engine_ledger'] and g['same_gpu_service_values'] and g['gpu_service_start']==4 and m['gpu']['start']==4 and m['gpu']['client_execute'] is False,'GPU Start4/full121 ledger must remain intact')
 require(set(m['packages'])==set(ROLES) and m['install_targets']==(['keypad','battery','audio'] if m.get('pending_parent1104') else ['parent','keypad','battery','audio']),'Only the explicitly pinned four adopted roles; root fixes require a new reviewed policy revision')
 packages={};baselines={}
 for role in ROLES:
  e=m['packages'][role];d=rooted(root,e['directory']);mp=rooted(root,e['manifest']);require(sha(mp)==e['manifest_sha256'],'Signed package manifest mismatch');pm=read(mp)
  require(pm['source_commit']==e['source_commit'],'Package/source mismatch');members={x['sha256'].lower() for x in pm.get('files',pm.get('outputs',[]))};hs={}
  for kind,n in e['names'].items():require(kind in ('inf','sys','cat') and re.fullmatch('[A-Za-z0-9_.-]+',n),'Fixed trio leaves only');hs[kind]=sha(d/n);require(hs[kind] in members,'Package member absent from signed manifest')
  require(set(hs)=={'inf','sys','cat'},'Exact trio required');packages[role]={**e,'hashes':hs,'version_u64':version(e['driver_version']),'date_filetime':datevalue(e['driver_date'])}
  if role=='keypad':
   _,seal=pin(e['main_seal']);require(seal['source']==e['source_commit']=='7bb166c71475be286cfa13478f0456a67210c817' and seal['version']=='0.0.1.2' and seal['home_field_usage']==[7,227] and seal['descriptor_checked'] and seal['pmic_keypad_lifetime_code_unchanged'],'Keypad1.2 only; 1.1 must not be finalized');baselines[role]={'mode':'exact-unbound','version_u64':0,'date_filetime':0,'sys_sha256':'0'*64,'inf':''}
  elif role=='battery':baselines[role]={'mode':'selected','version_u64':version('0.0.1.0'),'date_filetime':datevalue('11/04/2026'),'sys_sha256':'0'*64,'inf':'oem53.inf'}
  elif role in ('parent','audio') and role in m['install_targets']:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])<packages[role]['version_u64'],'Exact older installed role required');baselines[role]={'mode':'selected','version_u64':version(node[0]['selected']['DriverVersion']),'date_filetime':datevalue('11/08/2026' if role=='parent' else '11/07/2026'),'sys_sha256':files['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256'],'inf':node[0]['selected']['InfPath']}
  else:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])==packages[role]['version_u64'] and files['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256']==hs['sys'],'Retained roles must be current exact bytes/version; never reinstall/downgrade');baselines[role]={'mode':'retained','version_u64':packages[role]['version_u64'],'date_filetime':packages[role]['date_filetime'],'sys_sha256':hs['sys'],'inf':node[0]['selected']['InfPath']}
 blockers=list(m['blockers']);supp=m['battery_baseline']['actual_sys_supplement']
 if supp is not None:
  sp,sm=pin(supp);require(sm['schema']==1 and sm['source_manifest_sha256']==FRESH and sm['recovery_boot_id']==BOOT and sm['read_only'] and sm['unmounted'] and sm['errors']==[],'Supplement must bind exact fresh124 Recovery identity')
  rows=[x for x in sm['files'] if x['source_path']=='Windows/System32/Drivers/lc709203f.sys'];require(len(rows)==1,'One actual battery SYS required');row=rows[0];p=rooted(root,sp.parent/row['backup_path']);require(sha(p)==row['sha256'] and p.stat().st_size==row['bytes'],'Actual battery SYS witness mismatch');baselines['battery']['sys_sha256']=row['sha256'];blockers=[x for x in blockers if not x.startswith('LC709203F actual SYS')]
 return m,packages,baselines,blockers

def header(packages,baselines,policy=None):
 lines=['/* Fixed fresh124 profile; preview remains non-armable. */','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(k) for k in CURRENT_INSTALL))+'u','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0x80u','static const char deferred_token[]="'+(hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest() if policy is not None else 'DRAFT-NOT-ARMED')+'";','static const struct peripheral_target targets[10]={']
 for role in ROLES:
  t=TARGETS[role];p=packages[role];b=baselines[role];lines.append('{'+','.join(cstr(x) for x in (t[0],t[1],t[2],role,p['names']['inf'],t[3],p['names']['cat']))+','+str(p['version_u64'])+'ULL,'+str(p['date_filetime'])+'ULL,{'+','.join(cbytes(p['hashes'][k]) for k in ('inf','sys','cat'))+'},'+str(b['version_u64'])+'ULL,'+str(b['date_filetime'])+'ULL,'+cbytes(b['sys_sha256'])+','+cstr(b['inf'])+'},')
 lines+=['};','static const struct preserved_file preserved_files[]={']
 for name,h in CURRENT_PRESERVED.items():lines.append('{'+cstr(name[len('Windows/System32/'):].replace('/','\\'))+','+cbytes(h)+'},')
 lines+=['};','static const struct preserved_service preserved_services[]={']
 for name,v in CURRENT_OLD_SERVICES.items():lines.append('{'+cstr(name)+','+str(int(v['present']))+','+cbytes(v['root_sha256'] if v['present'] else '0'*64)+'},')
 lines+=['};','static const struct diagnostic_client diagnostic_clients[]={']
 for v in CURRENT_CLIENTS:lines.append('{'+cstr(v['exe'])+','+cstr(' '.join(v['arguments']))+','+cstr(v['name'])+','+cstr(SERVICE+'\\clients')+','+cbytes(v['sha256'])+','+str(v.get('timeout_ms',10000))+'u},')
 return '\n'.join(lines+['};'])+'\n'

def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args();root=x.workspace.resolve(strict=True);m,pkgs,base,blocks=validate(root,rooted(root,x.map));out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root output required')
 if x.finalize:require(COMBINED_GPU_ADAPTER_READY and not blocks and m['draft'] is False and m['normal_install_allowed'] is True and base['battery']['sys_sha256']!='0'*64 and not m.get('pending_parent1104') and m.get('candidate_gpu_suite') is not None and m['diagnostics']['wifi_scan']['client_package'] is not None,'Finalization blocked by missing actual witness/candidate pins; draft cannot arm')
 global CURRENT_PRESERVED,CURRENT_OLD_SERVICES,CURRENT_CLIENTS,CURRENT_INSTALL
 CURRENT_INSTALL=m['install_targets']
 fm=read(rooted(root,m['fresh']['path']));new_sys={TARGETS[k][3] for k in m['install_targets']}
 CURRENT_PRESERVED={e['source_path']:e['sha256'] for e in fm['files'] if (e['source_path'].startswith('Windows/System32/Drivers/') and e['source_path'].split('/')[-1] not in new_sys) or e['source_path']=='Windows/System32/MT8167EngineProbe.exe'}
 for name in ('Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw'):
  witness=rooted(root,'port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/'+name);CURRENT_PRESERVED[name]=sha(witness)
 ne=m['native_baseline'];np=rooted(root,ne['path']);require(sha(np)==ne['sha256'],'Old service snapshot pin mismatch');n=read(np);require(n['source_manifest_sha256']==FRESH and n['errors']==[] and n['originals_unchanged'] and set(n['old_services'])==set(m['old_services']),'Seven old service snapshot source mismatch');CURRENT_OLD_SERVICES=n['old_services']
 CURRENT_CLIENTS=[]
 for name in ('battery_temperature','accdet_status','afe_jack'):
  v=m['diagnostics'][name];require(sha(rooted(root,v['directory'])/v['exe'])==v['sha256'],'Signed client byte pin mismatch');CURRENT_CLIENTS.append({**v,'name':name})
 preserved_sources={e['source_path']:(Path(m['fresh']['path']).parent/e['backup_path']).as_posix() for e in fm['files'] if e['source_path'] in CURRENT_PRESERVED}
 for name in CURRENT_PRESERVED:
  if name not in preserved_sources:preserved_sources[name]='port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/'+name
 clients=[{**v,'relative_directory':SERVICE+'\\clients','dependencies':[]} for v in CURRENT_CLIENTS]
 policy={'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':SERVICE,'draft':not x.finalize,'normal_install_allowed':bool(x.finalize),'backup_manifest':m['fresh']['path'],'backup_manifest_sha256':FRESH,'recovery_boot_id':BOOT,'capture_selection':{'schema':1,'profile':PROFILE,'capture_number':124,'manifest_sha256':FRESH,'recovery_boot_id':BOOT,'gpu_policy_sha256':m['gpu']['policy_sha256'],'unchanged_since_capture':True},'source_files':{n:{k:next(e for e in fm['files'] if e['source_path']=='Windows/System32/config/'+n)[k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'packages':pkgs,'baselines':base,'service_preservation':CURRENT_OLD_SERVICES,'preserved_files':CURRENT_PRESERVED,'preserved_sources':preserved_sources,'planned_reboots':1,'install_targets':m['install_targets'],'stage_order':list(ROLES),'first_install_mask':128,'battery_baseline_sys_sha256':base['battery']['sys_sha256'],'diagnostic_clients':clients,'gpu_install':False,'engine_retest':False,'device_verified':False,'boot_transition':{'mode':'retain-current-matched8range-boot','boot_path':'port/xnu/out/mt8167-thermal-stage1-readback-20261108/firmware/boot/boot.img','boot_sha256':'125f42b54ae9503e1919cc67cba6be53cfb08928497ffc5a9c10cae85738b887','main_handoff_required':False,'boot_write_required':False,'gpt_written':False}}
 out.mkdir(mode=0o700);(out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(header(pkgs,base,policy if x.finalize else None));
 (out/'peripheral-policy.json').write_text(json.dumps(policy,indent=2)+'\n');(out/'peripheral-preservation-plan.json').write_text(json.dumps(policy,indent=2)+'\n');(out/'validation.json').write_text(json.dumps({'schema':1,'profile':PROFILE,'draft':True,'normal_install_allowed':False,'source_fresh_sha256':FRESH,'packages':pkgs,'baselines':base,'blockers':blocks,'compiled_runtime_not_device_verified':True,'no_hardware_operations':True},indent=2)+'\n');print('FOLLOWUP124_DRAFT_VALIDATED_NOT_ARMABLE')
if __name__=='__main__':main()
