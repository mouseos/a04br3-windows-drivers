#!/usr/bin/env python3
"""fresh148原本専用run149/12target policy。既存媒体と実機へ操作しない。"""
import argparse, copy, hashlib, json, re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'winpe'))
from peripheral_batch_policy import rooted, sha, read, require, cstr, cbytes, inftext
from codec_msdc_transaction import version, datevalue
import importlib.util
_spec=importlib.util.spec_from_file_location("adoption149",Path(__file__).with_name("prepare-combined151-input.py"))
_adoption=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_adoption)
PROFILE='combined-features-fresh150-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup150'
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
INSTALL=['parent','thermal','msdc']

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
 root=Path(root).resolve(strict=True)
 # Revalidate immutable originals, native cleanup, selected instances and all65 logs.
 adoption=_adoption.prepare(root)
 seal=spec['thermal_tuple_main_seal'];require(sha(rooted(root,seal['path']))==seal['sha256'],'Main parent/leaf seal byte pin mismatch')
 mainseal=read(rooted(root,seal['path']))
 require(mainseal['status']=='main-independent-thermal1116-leaf110-tuple-artifact-seal' and mainseal['parent_source']=='49bf368d9fc746fe9c78bc045d8aa390968cab8d' and mainseal['leaf_source']=='740800733fe9741023038026a717fd54d43cd7c3' and mainseal['signatures_verified']==5 and mainseal['CAT_memberships_verified']==4 and mainseal['hardware_verified'] is False,'Exact independently sealed thermal tuple required')
 for f in mainseal['files']:
  member=rooted(root,f['path']);require(member.stat().st_size==f['bytes'] and sha(member)==f['sha256'],'Main sealed thermal member mismatch')
 require(spec['schema']==1 and spec['profile']==PROFILE and spec['hardware_owner']=='main','Explicit run149 map required')
 require(spec['gpu_client_execute'] is False and spec['gpu_client_count']==0 and spec['install_targets']==INSTALL,'No GPU execution or extra install scope')
 oldpath=rooted(root,spec['retained_map']['path']);require(sha(oldpath)==spec['retained_map']['sha256'],'Retained package map mismatch');old=read(oldpath)
 pkgs=copy.deepcopy(old['packages'])
 par=adoption['parent1116'];pd=Path(par['directory'])/'parent'
 pkgs['parent']={'directory':str(pd),'manifest':str(Path(par['directory'])/'signed-manifest.json'),'manifest_sha256':par['manifest_sha256'],'source_commit':par['source_commit'],'names':{'inf':'shared-parent-usb1116.inf','sys':'mt8167-shared.sys','cat':'mt8167-shared-usb1116.cat'},'driver_date':'11/08/2026','driver_version':'2026.11.8.1116'}
 pkgs['msdc']={'directory':adoption['msdc110']['directory'],'manifest':adoption['msdc110']['manifest']['path'],'manifest_sha256':adoption['msdc110']['manifest']['sha256'],'source_commit':adoption['msdc110']['source_commit'],'names':{'inf':'mt8167-msdc.inf','sys':'mt8167-msdc.sys','cat':'mt8167-msdc.cat'},'driver_date':'11/08/2026','driver_version':'0.0.1.10'}
 leaf=spec['thermal110'];require(leaf['source_commit']=='740800733fe9741023038026a717fd54d43cd7c3' and leaf['driver_version']=='0.0.1.10','Exact leaf1.10 source required');pkgs['thermal']=leaf
 packages={r:package(root,pkgs[r],r) for r in ROLES}
 require(packages['audio']['driver_version']=='2026.11.8.1034' and packages['wifi']['driver_version']=='2026.11.8.1023' and packages['render']['driver_version']=='0.0.3.41','Keep current audio/WiFi/render')
 raw=read(rooted(root,adoption['native_actual']['path']));fresh=read(rooted(root,adoption['fresh']['path']));originals={f['source_path']:f for f in fresh['files']}
 baselines={}
 for role in ROLES:
  rows=raw['nodes'][TARGETS[role][0]]['instances'];require(len(rows)==1 and rows[0]['selected_present'],'One actual selected node required');row=rows[0];selected=row['selected']
  require(row['values']['Service']['value'].lower()==TARGETS[role][1].lower(),'Actual service mismatch')
  b={'version_u64':version(selected['DriverVersion']['value']),'date_filetime':datevalue(selected['DriverDate']['value'].replace('-','/')),'sys_sha256':originals['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256'],'inf':selected['InfPath']['value'],'mode':'selected' if role in INSTALL else 'retained'}
  if role in INSTALL:require(b['version_u64']<packages[role]['version_u64'],'Only exact older versions may update')
  else:require(b['version_u64']==packages[role]['version_u64'] and b['sys_sha256']==packages[role]['hashes']['sys'],'Retained role byte/version mismatch')
  baselines[role]=b
 # Parent proof API fix keeps the adopted MMIO-fixed boot and existing ABI.
 boot=old['matched_boot'];require(sha(rooted(root,boot['path']))==boot['sha256']=='1fef20aa28be71ae0bd826ec91619df3b492abd23631632d5b0d679e38ca560a','Retain fixed boot only')
 for ref in ('producer_pins','producer_compatibility','main_seal'):
  require(sha(rooted(root,boot[ref]['path']))==boot[ref]['sha256'],'Boot provenance mismatch')
 changed={TARGETS[r][3] for r in INSTALL};preserved={};sources={}
 for n,f in originals.items():
  if (n.startswith('Windows/System32/Drivers/') and Path(n).name not in changed) or (n.endswith('.exe') and n.startswith('Windows/System32/')) or n=='Windows/System32/ge8300-umd.dll':
   preserved[n]=f['sha256'];sources[n]=str(_adoption.BASE/f['backup_path'])
 # Extra GPU package members are retained, never installed or executed.
 for extra in packages['render']['extra_members']:
  p=rooted(root,Path(packages['render']['directory'])/extra['name']);require(sha(p)==extra['sha256']==extra['baseline_sha256'],'Retained render extra mismatch')
  # Actual firmware/UMD witnesses must already exist in sealed prior same-installed tuple.
  witness=old['current_render_witnesses']['umd']['path'] if extra['name']=='ge8300-umd.dll' else 'port/xnu/out/private-windows-rt-combined-run129-diagnostics/fixed/Windows/System32/Drivers/ge8300-mips.fw'
  if 'Windows/System32/'+extra['installed'].replace('\\','/') in originals:witness=str(_adoption.BASE/originals['Windows/System32/'+extra['installed'].replace('\\','/')]['backup_path'])
  require(sha(rooted(root,witness))==extra['sha256'],'Retained extra witness missing')
  n='Windows/System32/'+extra['installed'].replace('\\','/');preserved[n]=extra['sha256'];sources[n]=witness
 baseline=read(rooted(root,adoption['gpu_client_log_baseline']))
 mutable=[]
 for name,h in baseline['files'].items():
  source=Path(baseline['source_directory'])/name
  mutable.append({'relative_path':'MT8167D3DClear/'+name,'source':source.as_posix(),'sha256':h,'prefix_bytes':rooted(root,source).stat().st_size})
 values=value_contract('SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup146',adoption['previous_combined146_service']['values'])
 bt=raw['nodes']['ROOT\\MT6630COMBO']['instances'][0]
 require(bt['registry_path']=='ControlSet001\\Enum\\ROOT\\SYSTEM\\0001','Exact existing BT root path required')
 values+=value_contract('SYSTEM\\CurrentControlSet\\Enum\\ROOT\\SYSTEM\\0001',bt['values'])
 require(len(adoption['native_service_preservation'])==18 and len(values)==10,'18 full fingerprints plus5+5 named values required')
 client=copy.deepcopy(old['diagnostics']['audio_consumer']);client.update(name='audio_consumer',relative_directory=SERVICE+'\\clients',dependencies=[])
 require(client['arguments']==['--speaker-silence'] and client['phase']==0 and client['timeout_ms']==60000 and sha(rooted(root,Path(client['directory'])/client['exe']))==client['sha256']=='886bb9f62aebf8286157f3dd0a3e745217936b8bf0bceac79ef0dd5106487770','One bounded existing silent consumer')
 status=copy.deepcopy(spec['thermal_status_client'])
 require(status['exe']=='mt8167-thermal-ioctl-status.exe' and status['arguments']==['--status'] and status['sha256']=='542c7393ca79bba6fd244318747bb3fdf59d4469792566f4755085ba28c7cc8d' and sha(rooted(root,Path(status['directory'])/status['exe']))==status['sha256'],'Exact signed readonly64B client required')
 status.update(name='thermal_status',relative_directory=SERVICE+'\\clients',dependencies=[],phase=0,timeout_ms=10000)
 policy={'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':SERVICE,'draft':False,'normal_install_allowed':True,'hardware_owner':'main','hardware_operations_allowed':False,'backup_manifest':adoption['fresh']['path'],'backup_manifest_sha256':_adoption.FRESH,'recovery_boot_id':_adoption.BOOT,
 'capture_selection':{'schema':1,'profile':PROFILE,'capture_number':150,'manifest_sha256':_adoption.FRESH,'recovery_boot_id':_adoption.BOOT,'gpu_policy_sha256':raw['policy_sha256'],'unchanged_since_capture':True},
 'source_files':{n:{k:originals['Windows/System32/config/'+n][k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'packages':packages,'baselines':baselines,'service_preservation':adoption['native_service_preservation'],'captured_registry_values':values,'previous_combined146_service':adoption['previous_combined146_service'],'existing_bt_root':adoption['bt']['root'],
 'preserved_files':preserved,'preserved_sources':sources,'planned_reboots':1,'install_targets':INSTALL,'stage_order':list(ROLES),'first_install_mask':0,'deferred_unbound_mask':0,'battery_baseline_sys_sha256':baselines['battery']['sys_sha256'],'diagnostic_clients':[status,client],
 'gpu_install':False,'gpu_client_execute':False,'gpu_client_count':0,'bt_first_install_execute':False,'diagnostic_only':False,'engine_retest':False,'device_verified':False,
 'boot_transition':{'mode':'retain-current-mmio-fixed-boot1fef','boot_path':boot['path'],'boot_sha256':boot['sha256'],'main_handoff_required':True,'boot_write_required':False,'gpt_written':False},
 'gpu_client_log_baseline':{'path':adoption['gpu_client_log_baseline'],'sha256':sha(rooted(root,adoption['gpu_client_log_baseline']))},'readonly_diagnostic_scope':['thermal-temperature-once','thermal-state-no-retry','wlan-state','bt-radio-state','silent-speaker-once'],
 'observer_contract':{'planned_reboots':1,'fresh_basis':150,'install_phase_timeout_seconds':900,'postreboot_diagnostic_total_timeout_seconds':180,'gpu_client_execute':False,'gpu_suite_last':False},
 'bt_first_install':old['bt_first_install'],'companion_packages':{},'parent_contract':{'thermal_interface_version':3,'thermal_parent_build':1104,'headphone_interface_version':2,'headphone_parent_build':1107,'stock_record_version':3},
 'module_identity_collection':old['module_identity_collection'],'rotation_execute':False,'wlan_scan_execute':False,'normal_function_expectations':{'audible_output_verified':False,'physical_latency_measurement_performed':False,'bt_radio_success':False},'input_pins':adoption['pinned_files'],'native_continuity':adoption['native_continuity'],'required_absent_services':['MT8167CombinedFollowup148'],'mutable_diagnostic_logs':{'kind':'observation-not-install-admission','preserve_in_place':True,'overwrite_or_delete':False,'comparison':'exact-saved-prefix-and-append; replacement/truncation reported separately','files':mutable,'baseline':{'path':adoption['gpu_client_log_baseline'],'sha256':sha(rooted(root,adoption['gpu_client_log_baseline']))}},'thermal_tuple_main_seal':seal}
 # Keep the BT2013 package as a retained identity/signature input; no import/install call.
 for f in policy['bt_first_install']['files']:require(sha(rooted(root,Path(policy['bt_first_install']['directory'])/f['name']))==f['sha256'],'BT2013 retained package mismatch')
 # Workspace artifact references use one serialization on Linux and Windows.
 # Registry paths, hardware IDs and normal-OS paths retain their Win32 spelling.
 def canonical(v):
  if isinstance(v,str) and v.startswith(('port\\','windows-rt\\')):return v.replace('\\','/')
  if isinstance(v,dict):return {k:canonical(x) for k,x in v.items()}
  if isinstance(v,list):return [canonical(x) for x in v]
  return v
 return canonical(policy)

def generate(p):
 require(p['profile']==PROFILE and p['stage_order']==list(ROLES) and p['install_targets']==INSTALL and p['gpu_client_execute'] is False and p['gpu_client_count']==0 and p['bt_first_install_execute'] is False,'Final149 contract only')
 lines=['/* run149:12 explicit targets; GPUclient0; retained BT root. */','#define A04_FEATURE148 1','#define A04_FEATURE150 1','#define A04_FEATURE144 1','#define A04_RENDER_TARGET_INDEX '+str(ROLES.index('render'))+'u','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(r) for r in INSTALL))+'u','#define A04_DIAGNOSTIC_ONLY 0','#define A04_READONLY_PERIPHERAL_STATUS 1','#define A04_RUN_THERMAL_READ 1','#define A04_DIAGNOSTIC_CLIENT_COUNT 2u','#define A04_CANDIDATE_EXTRA_COUNT 2u','#define A04_AUX_HCD1014_IMPORT 0','#define A04_USB_PC_ROLE_OPTIONAL_LEGACY_NODE 1','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0u','static const char deferred_token[]="'+hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const unsigned install_order[12]={6,1,11,0,2,3,4,5,7,8,9,10};','static const struct peripheral_target targets[12]={']
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
 for c in p['diagnostic_clients']:lines.append('{'+','.join(cstr(x) for x in (c['exe'],' '.join(c['arguments']),c['name'],c['relative_directory']))+','+cbytes(c['sha256'])+','+str(c['timeout_ms'])+'u,0u},')
 # Declarations shared with the read-only BT helper; first-install code is unreachable.
 lines+=['};','static const struct preserved_file aux_bt_files[3]={']
 for f in p['bt_first_install']['files']:lines.append('{'+cstr(f['name'])+','+cbytes(f['sha256'])+'},')
 bt=p['bt_first_install'];date=datevalue(bt['driver_date'])
 lines+=['};','#define A04_BT_VERSION '+str(version(bt['driver_version']))+'ULL','#define A04_BT_DATE_LOW '+str(date&0xffffffff)+'u','#define A04_BT_DATE_HIGH '+str(date>>32)+'u']
 return '\n'.join(lines)+'\n'

def load_map(path,root):return build_policy(root,read(path))
def assert_paths(image,capture=False):
 b=Path(image).read_bytes()
 for s in (SERVICE+'.exe',SERVICE,'MT8167CombinedFollowup148','MT8167CombinedFollowup146','bt2013-radio.bin','mt8167-thermal-ioctl-status.exe','mt8167-msdc.sys','mt8167-silent-pcm-consumer.exe'):
  require(s.encode('utf-16le') in b,'Missing required UTF16 identity: '+s)
 require('bt2013-install.bin'.encode('utf-16le') not in b,'BT first-install code must not be linked')
 require('ge8300-d3d-texture-pixel-boundary.exe'.encode('utf-16le') not in b,'GPU execution client must not be linked')

def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args()
 require(x.finalize,'Explicit finalization required; missing signed leaf must fail, never fallback')
 root=x.workspace.resolve(strict=True);policy=load_map(rooted(root,x.map),root);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output only');out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(policy))
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(policy,indent=2)+'\n')
 print('COMBINED149_12TARGET_INPUT_VALIDATED_GPU0_BT_RETAIN')
if __name__=='__main__':main()
