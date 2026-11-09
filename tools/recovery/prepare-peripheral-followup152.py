#!/usr/bin/env python3
"""fresh148原本専用run149/12target policy。既存媒体と実機へ操作しない。"""
import argparse, copy, hashlib, json, re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'winpe'))
from peripheral_batch_policy import rooted, sha, read, require, cstr, cbytes, inftext
from codec_msdc_transaction import version, datevalue
import importlib.util
_spec=importlib.util.spec_from_file_location("gpu342_artifacts",Path(__file__).with_name("prepare-combined153-draft.py"))
_gpu=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_gpu)
PROFILE='combined-features-fresh152-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup152'
ROLES=('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render','msdc')
# Explicit semantic targets: no positional guess can classify MSDC as render.
TARGETS={
 'rtc':('MT6392\\RTC','Mt6392Rtc','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt6392-rtc.sys'),
 'thermal':('MT8167\\Thermal','Mt8167Thermal','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-thermal.sys'),
 'usb':('MT8167\\USB20','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-usb20.sys'),
 'wifi':('A04BR3\\MT6630SDIO','Mt6630Runtime','{4D36E972-E325-11CE-BFC1-08002BE10318}','mt6630-combo-runtime.sys'),
 'sd':('ACPI\\MOU0013','Mt8167Sd','{4D36E97B-E325-11CE-BFC1-08002BE10318}','mt8167-sd.sys'),
 'display':('ACPI\\MOU0007','Mt8167Display','{4D36E968-E325-11CE-BFC1-08002BE10318}','mt8167-display.sys'),
 'parent':('ACPI\\BEN0001','Mt8167Shared','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-shared.sys'),
 'keypad':('MT6392\\Buttons','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-keypad.sys'),
 'battery':('ACPI\\MOU0008','LC709203F','{72631E54-78A4-11D0-BCF7-00AA00B7B32A}','lc709203f.sys'),
 'audio':('MT8167\\AFE','Mt8167Audio','{4D36E96C-E325-11CE-BFC1-08002BE10318}','mt8167-audio.sys'),
 'render':('ACPI\\MOU0006','GE8300Render','{4D36E968-E325-11CE-BFC1-08002BE10318}','ge8300-render.sys'),
 'msdc':('ACPI\\MOU0001','a04msdcboot','{4D36E97B-E325-11CE-BFC1-08002BE10318}','mt8167-msdc.sys')}
INSTALL=['render']

def paths():
 b='C:\\Windows\\System32\\'+SERVICE
 return '\n'.join('#define '+k+' '+cstr(v) for k,v in [('A04_PERIPHERAL_SERVICE',SERVICE),('A04_PERIPHERAL_DIRECTORY',SERVICE),('A04_CAPTURE_DIRECTORY',b+'\\capture'),('A04_CAPTURE_LOG_PATH',b+'\\capture\\capture.log'),('A04_CAPTURE_MUTEX',SERVICE+'CaptureOnce'),('A04_WLAN_SCAN_LOG_PATH',b+'\\logs\\wlan-scan.txt'),('A04_WLAN_WIFI_ENUM_PATH','SYSTEM\\CurrentControlSet\\Enum\\A04BR3\\MT6630SDIO\\3&7ce9159&0&0')])+'\n'

def raw_value(v):
 require(v['present'],'Only captured present values may be asserted')
 kind=v['kind'];x=v['value']
 if kind=='DWord':return 4,struct.pack('<I',x)
 if kind=='String' or kind=='ExpandString':return (1 if kind=='String' else 2),(x+'\0').encode('utf-16le')
 if kind=='MultiString':return 7,('\0'.join(x)+'\0\0').encode('utf-16le')
 raise ValueError('Unsupported captured value type')

def value_contract(key,values):
 return [{'key':key,'name':n,'type':raw_value(v)[0],'raw_hex':raw_value(v)[1].hex()} for n,v in values.items() if v['present']]

def package(root,e,role):
 e=copy.deepcopy(e);mp=rooted(root,e['manifest']);pm=read(mp)
 require(sha(mp)==e['manifest_sha256'],'Package manifest byte pin mismatch')
 require(pm.get('package_source_commit',pm.get('source_commit'))==e['source_commit'],'Package source mismatch')
 require(set(e['names'])=={'inf','sys','cat'} and e['names']['sys']==TARGETS[role][3],'Exact role trio required')
 hashes={}
 members=pm.get('files',pm.get('outputs',[]))
 for k,n in e['names'].items():
  require(re.fullmatch(r'[A-Za-z0-9_.-]+',n),'Fixed trio basename required')
  hashes[k]=sha(rooted(root,Path(e['directory'])/n))
  # Both name and bytes must match, not any other manifest member with same hash.
  require(any(Path(f.get('path',f.get('name','')).replace('\\','/')).name==n and f['sha256'].lower()==hashes[k] for f in members),'Trio absent from manifest: '+role)
 text=inftext(rooted(root,Path(e['directory'])/e['names']['inf']))
 match=re.search(r'^\s*DriverVer\s*=\s*([^,\r\n]+),\s*([^;\r\n]+)',text,re.M|re.I)
 require(match is not None and match[1].strip()==e['driver_date'] and match[2].strip()==e['driver_version'],'Exact DriverVer required')
 require(TARGETS[role][0].lower() in text.lower(),'Exact HWID required')
 section='';guids=[]
 for raw in text.splitlines():
  line=raw.split(';')[0].strip()
  if line.startswith('[') and line.endswith(']'):section=line[1:-1].lower()
  elif section=='version' and '=' in line and line.split('=',1)[0].strip().lower()=='classguid':guids.append(line.split('=',1)[1].strip().lower())
 require(guids==[TARGETS[role][2].lower()],'Exact Version ClassGuid required')
 e.update(hashes=hashes,version_u64=version(e['driver_version']),date_filetime=datevalue(e['driver_date']))
 return e

def build_policy(root,spec):
 root=Path(root).resolve(strict=True);pins={}
 def pin(ref):
  p=rooted(root,ref['path']);require(sha(p)==ref['sha256'],'Pinned input mismatch: '+ref['path']);pins[ref['path'].replace('\\','/')]=ref['sha256'];return read(p)
 require(spec['schema']==1 and spec['profile']==PROFILE and spec['hardware_owner']=='main' and spec['install_targets']==INSTALL and spec['gpu_client_execute'] is True and spec['gpu_client_count']==1,'Exact153 GPU342-only scope required')
 fm=pin(spec['fresh']);require(spec['fresh']['sha256']=='fee4081fc3842390e854753aa6b2ce4659fc5b58645246c406ed205679c55ba3' and fm['status']=='captured-unmounted' and fm['unmounted'] and fm['read_only'] and not fm['errors'] and fm['recovery_boot_id']=='72ea1a1d-06b9-4671-88d2-f957198f3362' and len(fm['files'])==41,'Exact main-confirmed fresh152 required')
 originals={f['source_path']:f for f in fm['files']}
 for f in fm['files']:
  q=rooted(root,Path(spec['fresh']['path']).parent/f['backup_path']);require(q.stat().st_size==f['bytes'] and sha(q)==f['sha256'],'Original152 mismatch');pins[q.relative_to(root).as_posix()]=f['sha256']
 native=pin(spec['native']);require(native['source_manifest_sha256']==spec['fresh']['sha256'] and not native['errors'] and native['originals_unchanged'] and native['full_service_preservation_equal'] and native['rejected149_service_absent'] and all(native[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Exact clean152 native receipt required')
 for n,h in native['source_files'].items():require(originals['Windows/System32/config/'+n]['sha256']==h,'Native source trio mismatch')
 prior=pin(spec['prior151_policy']);require(prior['profile']=='combined-features-fresh150-v1' and prior['gpu_client_execute'] is False,'Frozen151 policy reference required')
 require(native['old_services']==prior['service_preservation'] and len(native['old_services'])==18,'18 actual full fingerprints required')
 # Native explicitly read these ten raw values. Keep them as values, never invented hashes.
 values=copy.deepcopy(prior['captured_registry_values'])
 for v in values:
  got=native['captured_named_values'][v['key']+'\\'+v['name']]
  require(got['type']==v['type'] and got['bytes']==len(bytes.fromhex(v['raw_hex'])) and got['raw_hex']==v['raw_hex'],'Actual146/BT named value mismatch')
 previous=native['new_service'];require(previous['present'] and previous['values']['Start']['value']==4 and previous['values']['ImagePath']['value']=='"C:\\Windows\\System32\\MT8167CombinedFollowup150.exe"','Current150 must be actually stopped')
 values+=value_contract('SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup150',previous['values']);require(len(values)==15,'18 full plus15 named values required')
 gpu=_gpu.sealed342(root);gpu['directory']=_gpu.PACKAGE.as_posix();client=_gpu.fixed_client(root);client.update(name='gpu_suite',relative_directory=SERVICE+'\\clients',phase=1,dependencies=[])
 require(gpu['main_seal']==spec['GPU342_main_seal'] and gpu['manifest']==spec['GPU342_manifest'],'Exact main342 tuple refs required')
 for ref in (gpu['main_seal'],gpu['manifest'],gpu['build_manifest'],gpu['public_fullsource_closure']['gate_log'],client['main_seal']):pin(ref)
 pkgs=copy.deepcopy(prior['packages']);pkgpath=Path(gpu['directory']);pkgs['render'].update(directory=pkgpath.as_posix(),manifest=gpu['manifest']['path'],manifest_sha256=gpu['manifest']['sha256'],source_commit=_gpu.RUNTIME_SOURCE,binary_source_commit=_gpu.RUNTIME_SOURCE,driver_version='0.0.3.42',driver_date='11/08/2026',main_seal=gpu['main_seal'])
 packages={r:package(root,pkgs[r],r) for r in ROLES};baselines={}
 for r in ROLES:
  row=native['nodes'][TARGETS[r][0]];require(row['present'] and len(row['instances'])==1,'One actual selected node required');v=row['instances'][0];require(v['selected_present'] and v['values']['Service']['value'].lower()==TARGETS[r][1].lower(),'Actual selected service required');sel=v['selected']
  base={'mode':'selected' if r=='render' else 'retained','version_u64':version(sel['DriverVersion']['value']),'date_filetime':datevalue(sel['DriverDate']['value'].replace('-','/')),'sys_sha256':originals['Windows/System32/Drivers/'+TARGETS[r][3]]['sha256'],'inf':sel['InfPath']['value']}
  if r=='render':require(base['version_u64']==version('0.0.3.41') and base['sys_sha256']=='78d2bff6804a04d121a2fc2908e80c8bfc9a43edf80ffb1c08ff8d828dc671dd','Actual341 baseline required')
  else:require(base['version_u64']==packages[r]['version_u64'] and base['sys_sha256']==packages[r]['hashes']['sys'],'Retained role mismatch: '+r)
  baselines[r]=base
 require(packages['parent']['driver_version']=='2026.11.8.1116' and packages['thermal']['driver_version']=='0.0.1.10' and packages['msdc']['driver_version']=='0.0.1.10','Retain152 installed parent/thermal/MSDC; no new AML fix')
 baseline=pin(spec['mutable_baseline']);require(baseline['source_fresh_manifest_sha256']==spec['fresh']['sha256'] and baseline['capture_number']==152 and len(baseline['files'])==67 and not baseline['missing_prior_files'] and baseline['new151_GPU_execution'] is False,'Exact152 mutable snapshot required')
 mutable=[]
 for n,h in baseline['files'].items():
  require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',n),'Fixed diagnostic basename required');q=rooted(root,Path(baseline['source_directory'])/n);require(sha(q)==h,'Mutable archive mismatch');pins[q.relative_to(root).as_posix()]=h;mutable.append({'relative_path':'MT8167D3DClear/'+n,'source':q.relative_to(root).as_posix(),'sha256':h,'prefix_bytes':q.stat().st_size})
 preserved={};sources={}
 for n,f in originals.items():
  if (n.startswith('Windows/System32/Drivers/') and Path(n).name!='ge8300-render.sys') or (n.startswith('Windows/System32/') and n.endswith('.exe')):
   preserved[n]=f['sha256'];sources[n]=(Path(spec['fresh']['path']).parent/f['backup_path']).as_posix()
 for extra in packages['render']['extra_members']:
  n='Windows/System32/'+extra['installed'].replace('\\','/');require(extra['sha256']==extra['baseline_sha256'],'Retain UMD/FW bytes');q=rooted(root,prior['preserved_sources'][n]);require(sha(q)==extra['sha256'] and sha(rooted(root,pkgpath/extra['name']))==extra['sha256'],'Actual current UMD/FW witness mismatch');preserved[n]=extra['sha256'];sources[n]=q.relative_to(root).as_posix()
 boot=prior['boot_transition'];require(boot['boot_sha256']=='1fef20aa28be71ae0bd826ec91619df3b492abd23631632d5b0d679e38ca560a' and sha(rooted(root,boot['boot_path']))==boot['boot_sha256'] and not boot['boot_write_required'] and not boot['gpt_written'],'Retain boot1fef only')
 require(native['nodes']['ROOT\\MT6630COMBO']['present'] and len(native['nodes']['ROOT\\MT6630COMBO']['instances'])==1,'Retain existing BT ROOT')
 p={'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':SERVICE,'draft':False,'normal_install_allowed':True,'hardware_owner':'main','hardware_operations_allowed':False,'backup_manifest':spec['fresh']['path'],'backup_manifest_sha256':spec['fresh']['sha256'],'recovery_boot_id':fm['recovery_boot_id'],
 'capture_selection':{'schema':1,'profile':PROFILE,'capture_number':152,'manifest_sha256':spec['fresh']['sha256'],'recovery_boot_id':fm['recovery_boot_id'],'gpu_policy_sha256':native['policy_sha256'],'unchanged_since_capture':True},'source_files':{n:{k:originals['Windows/System32/config/'+n][k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},
 'packages':packages,'baselines':baselines,'service_preservation':native['old_services'],'captured_registry_values':values,'required_absent_services':['MT8167CombinedFollowup148'],'previous_combined150_service':previous,'preserved_files':preserved,'preserved_sources':sources,'planned_reboots':1,'install_targets':INSTALL,'stage_order':list(ROLES),'first_install_mask':0,'deferred_unbound_mask':0,'battery_baseline_sys_sha256':baselines['battery']['sys_sha256'],'diagnostic_clients':[client],
 'gpu_install':True,'gpu_client_execute':True,'gpu_client_count':1,'bt_first_install_execute':False,'diagnostic_only':False,'engine_retest':False,'device_verified':False,'boot_transition':boot,'GPU342_main_seal':gpu['main_seal'],'GPU342_provenance':gpu,'gpu_client_log_baseline':spec['mutable_baseline'],
 'mutable_diagnostic_logs':{'kind':'observation-not-install-admission','preserve_in_place':True,'overwrite_or_delete':False,'comparison':'saved exact prefix/append/truncation/replacement/missing observation only','files':mutable,'baseline':spec['mutable_baseline']},'observer_contract':{'planned_reboots':1,'fresh_basis':152,'install_phase_timeout_seconds':900,'boot_wait_seconds':180,'pre_GPU_phase_timeout_seconds':180,'gpu_client_timeout_seconds':900,'postreboot_diagnostic_total_timeout_seconds':1080,'GPU_client_count':1,'GPU_client_last':True,'deadline_update_per_phase_max':1},
 'bt_first_install':prior['bt_first_install'],'companion_packages':{},'parent_contract':prior['parent_contract'],'thermal_tuple_main_seal':prior['thermal_tuple_main_seal'],'module_identity_collection':{**prior['module_identity_collection'],'status':'required-exact-GPU342-identity','expected_buildid':'GPU342-tq-clear-v1','expected_sourcebase':read(rooted(root,gpu['manifest']['path']))['module_identity_runtime_source_base']},'baseline341_module_identity_reference_only':prior['module_identity_collection'],'rotation_execute':False,'wlan_scan_execute':False,'normal_function_expectations':{'temperature_execute':False,'thermal_status_execute':False,'audio_consumer_execute':False,'WLAN_readonly_execute':False,'BT_readonly_execute':False,'GPU_execute':True,'GPU342_real_success':False},'input_pins':[{'path':n,'sha256':h} for n,h in sorted(pins.items())]}
 def canonical(v):
  if isinstance(v,str) and v.startswith(('port\\','windows-rt\\')):return v.replace('\\','/')
  if isinstance(v,dict):return {k:canonical(x) for k,x in v.items()}
  if isinstance(v,list):return [canonical(x) for x in v]
  return v
 return canonical(p)

def generate(p):
 require(p['profile']==PROFILE and p['stage_order']==list(ROLES) and p['install_targets']==INSTALL and p['gpu_client_execute'] is True and p['gpu_client_count']==1 and p['bt_first_install_execute'] is False,'Final149 contract only')
 lines=['/* run149:12 explicit targets; GPUclient0; retained BT root. */','#define A04_FEATURE148 1','#define A04_FEATURE150 1','#define A04_FEATURE152 1','#define A04_GPU_PHASE1_ENABLED 1','#define A04_RUN_WLAN_READ 0','#define A04_RUN_BT_READ 0','#define A04_FEATURE144 1','#define A04_RENDER_TARGET_INDEX '+str(ROLES.index('render'))+'u','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(r) for r in INSTALL))+'u','#define A04_DIAGNOSTIC_ONLY 0','#define A04_READONLY_PERIPHERAL_STATUS 1','#define A04_RUN_THERMAL_READ 0','#define A04_DIAGNOSTIC_CLIENT_COUNT 1u','#define A04_CANDIDATE_EXTRA_COUNT 2u','#define A04_AUX_HCD1014_IMPORT 0','#define A04_USB_PC_ROLE_OPTIONAL_LEGACY_NODE 1','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0u','static const char deferred_token[]="'+hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const unsigned install_order[12]={10,0,1,2,3,4,5,6,7,8,9,11};','static const struct peripheral_target targets[12]={']
 for r in ROLES:
  t=TARGETS[r];e=p['packages'][r];b=p['baselines'][r]
  lines.append('{'+','.join(cstr(x) for x in (t[0],t[1],t[2],r,e['names']['inf'],t[3],e['names']['cat']))+','+str(e['version_u64'])+'ULL,'+str(e['date_filetime'])+'ULL,{'+','.join(cbytes(e['hashes'][k]) for k in ('inf','sys','cat'))+'},'+str(b['version_u64'])+'ULL,'+str(b['date_filetime'])+'ULL,'+cbytes(b['sys_sha256'])+','+cstr(b['inf'])+'},')
 lines+=['};','static const struct candidate_extra_file candidate_extras[2]={']
 for e in p['packages']['render']['extra_members']:lines.append('{'+cstr(e['name'])+','+cstr(e['installed'])+','+cbytes(e['sha256'])+','+cbytes(e['baseline_sha256'])+'},')
 lines+=['};','static const struct preserved_file preserved_files[]={']
 for n,h in p['preserved_files'].items():lines.append('{'+cstr((n[len('Windows/System32/'):] if n.startswith('Windows/System32/') else '/'+n).replace('/','\\'))+','+cbytes(h)+'},')
 lines+=['};','static const struct preserved_service preserved_services[]={']
 for n,v in p['service_preservation'].items():lines.append('{'+cstr(n)+','+str(int(v['present']))+','+cbytes(v['root_sha256'] if v['present'] else '0'*64)+'},')
 lines+=['};','struct captured_registry_value {const WCHAR *key,*name;DWORD type,bytes;const BYTE *data;};']
 for i,v in enumerate(p['captured_registry_values']):lines.append('static const BYTE captured_data_'+str(i)+'[]={'+','.join(str(x) for x in bytes.fromhex(v['raw_hex']))+'};')
 lines+=['static const struct captured_registry_value captured_values[]={']
 for i,v in enumerate(p['captured_registry_values']):lines.append('{'+cstr(v['key'])+','+cstr(v['name'])+','+str(v['type'])+'u,'+str(len(bytes.fromhex(v['raw_hex'])))+'u,captured_data_'+str(i)+'},')
 lines+=['};','static const struct diagnostic_client diagnostic_clients[]={']
 for c in p['diagnostic_clients']:lines.append('{'+','.join(cstr(x) for x in (c['exe'],' '.join(c['arguments']),c['name'],c['relative_directory']))+','+cbytes(c['sha256'])+','+str(c['timeout_ms'])+'u,1u},')
 # Declarations shared with the read-only BT helper; first-install code is unreachable.
 lines+=['};','static const struct preserved_file aux_bt_files[3]={']
 for f in p['bt_first_install']['files']:lines.append('{'+cstr(f['name'])+','+cbytes(f['sha256'])+'},')
 bt=p['bt_first_install'];date=datevalue(bt['driver_date'])
 lines+=['};','#define A04_BT_VERSION '+str(version(bt['driver_version']))+'ULL','#define A04_BT_DATE_LOW '+str(date&0xffffffff)+'u','#define A04_BT_DATE_HIGH '+str(date>>32)+'u']
 return '\n'.join(lines)+'\n'

def load_map(path,root):return build_policy(root,read(path))
def assert_paths(image,capture=False):
 b=Path(image).read_bytes()
 for s in (SERVICE+'.exe',SERVICE,'MT8167CombinedFollowup148','MT8167CombinedFollowup146','mt8167-msdc.sys','ge8300-d3d-texture-pixel-boundary.exe'):
  require(s.encode('utf-16le') in b,'Missing required UTF16 identity: '+s)
 require('bt2013-install.bin'.encode('utf-16le') not in b,'BT first-install code must not be linked')
 require('mt8167-silent-pcm-consumer.exe'.encode('utf-16le') not in b and 'mt8167-thermal-ioctl-status.exe'.encode('utf-16le') not in b,'Unchanged peripheral clients must not be linked')

def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args()
 require(x.finalize,'Explicit finalization required; missing signed leaf must fail, never fallback')
 root=x.workspace.resolve(strict=True);policy=load_map(rooted(root,x.map),root);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output only');out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(policy))
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(policy,indent=2)+'\n')
 print('COMBINED149_12TARGET_INPUT_VALIDATED_GPU0_BT_RETAIN')
if __name__=='__main__':main()
