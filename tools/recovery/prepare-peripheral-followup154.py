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
PROFILE='combined-features-fresh154-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup154'
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
INSTALL=['parent','render']

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
  q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'Pinned input mismatch: '+ref['path']);pins[ref['path']]=ref['sha256'];return read(q)
 require(spec['schema']==1 and spec['profile']==PROFILE and spec['hardware_owner']=='main' and spec['install_targets']==INSTALL and spec['gpu_client_execute'] is True and spec['gpu_client_count']==1,'Exact155 matched343/1117 scope required')
 fm=pin(spec['fresh']);require(spec['fresh']['sha256']=='a26e82c937733617d932bb92201a593885575f27e89f26791612f791ca17b8da' and fm['status']=='captured-unmounted' and fm['unmounted'] and fm['read_only'] and not fm['errors'] and fm['recovery_boot_id']=='9d795d5d-8d58-47a2-b208-e5c0149ecd19' and len(fm['files'])==41,'Exact fresh154 required')
 originals={f['source_path']:f for f in fm['files']}
 for f in fm['files']:
  q=rooted(root,Path(spec['fresh']['path']).parent/f['backup_path']);require(q.stat().st_size==f['bytes'] and sha(q)==f['sha256'],'Original154 mismatch');pins[q.relative_to(root).as_posix()]=f['sha256']
 native=pin(spec['native']);init=pin(spec['init_native'])
 for n in (native,init):
  require(n['source_manifest_sha256']==spec['fresh']['sha256'] and not n['errors'] and n['originals_unchanged'] and n['full_service_preservation_equal'] and n['rejected149_service_absent'] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Clean154 receipt required')
  for name,h in n['source_files'].items():require(originals['Windows/System32/config/'+name]['sha256']==h,'Native trio mismatch')
 require(init['gpu_init']['ReachedMask']['value']==7 and (init['gpu_init']['DxgkInitialize']['value']&0xffffffff)==0xc000003b,'Actual154 init boundary required')
 require(native['module_identity']['observed_buildid']=='GPU342-tq-clear-v1' and native['module_identity']['expected_identity_matches'],'Same154 identity required')
 prior=pin(spec['prior153_policy']);require(prior['profile']=='combined-features-fresh152-v1' and prior['gpu_client_execute'],'Frozen153 source reference required')
 require(native['old_services']==prior['service_preservation'] and len(native['old_services'])==18,'18 actual fingerprints required')
 values=copy.deepcopy(prior['captured_registry_values'])
 for v in values:
  got=native['captured_named_values'][v['key']+'\\'+v['name']];require(got['type']==v['type'] and got['bytes']==len(bytes.fromhex(v['raw_hex'])) and got['raw_hex']==v['raw_hex'],'Actual stopped value mismatch')
 previous=native['new_service'];require(previous['present'] and previous['values']['Start']['value']==4 and previous['values']['ImagePath']['value']=='"C:\\Windows\\System32\\MT8167CombinedFollowup152.exe"','Current152 must be stopped')
 values+=value_contract('SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup152',previous['values']);require(len(values)==20,'18 full plus20 named values required')
 gs=pin(spec['GPU343_main_seal']);gm=pin(spec['GPU343_manifest']);require(gs['status']=='main-independent-final-GPU343-artifact-seal' and gs['manifest_sha256']==spec['GPU343_manifest']['sha256']=='ceb561330a8448afc3ad1843eb07db3306dd4f9348a16b715f03f8c0daa024e4' and gs['SYS_sha256']=='e884f33652078618ead6e02faaeccf606e15e9d3074e0cffabe681c8eb1db898' and gs['compiled_TUs']==0 and gs['archive_members_reused']==80 and gs['BCBF_valid_signatures']==6 and gs['PE_contract']['characteristics']==0x122 and gs['PE_contract']['dll_characteristics']==0x140,'Exact main343 relink seal required')
 require(gs['runtime_source_commit']=='59ded879bab1c4c54abac73bd2b673adada33d4c' and gm['runtime_build_id']==gs['runtime_build_id']=='GPU342-tq-clear-v1' and gm['package_source_commit']==gs['package_recipe_commit'],'Runtime/relink source distinction required')
 gp=Path(spec['GPU343_manifest']['path']).parent;gpu_files=[]
 for f in gm['outputs']+gm['retained_provenance']:
  q=rooted(root,gp/f['name']);require(sha(q)==f['sha256'],'343 member mismatch');pins[q.relative_to(root).as_posix()]=f['sha256'];gpu_files.append({'name':f['name'],'sha256':f['sha256']})
 for f in pin(spec['GPU343_pins']):
  q=rooted(root,f['file']);require(sha(q)==f['sha256'] and q.stat().st_size==f['bytes'],'343 provenance pin mismatch');pins[f['file']]=f['sha256']
 ts=pin(spec['thermal_main_seal']);require(ts['parent_source_commit']=='b2c1a3136a5b8eadf1900f71a8d330a6b0a31ac8' and ts['firmware_source_commit']=='b4ce1992af66adcefa02e34357d5dbf4313eaca2' and ts['parent_SYS_sha256']=='bbced952c499ba0588c77b861b20141043929773d703cdd327ffa92e61990979' and ts['boot_sha256']=='2e6cba6ab96cd5b586cefbd4bd004c6cee0484866ca6710ea454144da568c451' and not ts['mixed_old_new_allowed'] and not ts['EDK_common_changed'],'Matched1117 seal required')
 require(spec['thermal_pins']['sha256']==ts['pins_sha256'],'Thermal pins seal mismatch')
 for f in pin(spec['thermal_pins']):
  q=rooted(root,f['path']);require(sha(q)==f['sha256'],'Thermal member mismatch');pins[f['path']]=f['sha256']
 pkgs=copy.deepcopy(prior['packages']);pkgs['render'].update(directory=gp.as_posix(),manifest=spec['GPU343_manifest']['path'],manifest_sha256=spec['GPU343_manifest']['sha256'],source_commit=gm['package_source_commit'],binary_source_commit=gs['runtime_source_commit'],driver_version='0.0.3.43',driver_date=gm['inf_driverver'].split(',')[0],main_seal=spec['GPU343_main_seal'])
 tp=Path(spec['thermal_main_seal']['path']).parent/'package';tm=tp/'signed-manifest.json';parent=read(rooted(root,tm))
 pkgs['parent'].update(directory=(tp/'parent').as_posix(),manifest=tm.as_posix(),manifest_sha256=sha(rooted(root,tm)),source_commit=parent['source_commit'],driver_version=parent['parent_version'],names={'inf':'shared-parent-usb1117.inf','sys':'mt8167-shared.sys','cat':'mt8167-shared-usb1117.cat'},main_seal=spec['thermal_main_seal'])
 match=re.search(r'^\s*DriverVer\s*=\s*([^,\r\n]+),',inftext(rooted(root,tp/'parent/shared-parent-usb1117.inf')),re.M|re.I);require(match,'Parent DriverVer required');pkgs['parent']['driver_date']=match[1].strip()
 packages={r:package(root,pkgs[r],r) for r in ROLES};baselines={}
 for r in ROLES:
  row=native['nodes'][TARGETS[r][0]];require(row['present'] and len(row['instances'])==1,'One actual selected node required');v=row['instances'][0];require(v['selected_present'] and v['values']['Service']['value'].lower()==TARGETS[r][1].lower(),'Actual service required');sel=v['selected']
  b={'mode':'selected' if r in INSTALL else 'retained','version_u64':version(sel['DriverVersion']['value']),'date_filetime':datevalue(sel['DriverDate']['value'].replace('-','/')),'sys_sha256':originals['Windows/System32/Drivers/'+TARGETS[r][3]]['sha256'],'inf':sel['InfPath']['value']}
  if r=='render':require(b['version_u64']==version('0.0.3.42') and b['sys_sha256']=='431c06ade452b0ca00a920419bb39fe5261080469750a0bdf9f61670cccbec52','Actual342 baseline required')
  elif r=='parent':require(b['version_u64']==version('2026.11.8.1116'),'Actual1116 parent required')
  else:require(b['version_u64']==packages[r]['version_u64'] and b['sys_sha256']==packages[r]['hashes']['sys'],'Retained role mismatch: '+r)
  baselines[r]=b
 require(packages['thermal']['driver_version']==packages['msdc']['driver_version']=='0.0.1.10','Retain leaf/MSDC1.10')
 baseline=pin(spec['mutable_baseline']);require(baseline['source_fresh_manifest_sha256']==spec['fresh']['sha256'] and baseline['capture_number']==154 and len(baseline['files'])==67 and not baseline['missing_prior_files'] and baseline['new153_GPU_execution'] is False,'Actual154 mutable baseline required')
 mutable=[]
 for n,h in baseline['files'].items():
  require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',n),'Fixed log name required');q=rooted(root,Path(baseline['source_directory'])/n);require(sha(q)==h,'Log archive mismatch');pins[q.relative_to(root).as_posix()]=h;mutable.append({'relative_path':'MT8167D3DClear/'+n,'source':q.relative_to(root).as_posix(),'sha256':h,'prefix_bytes':q.stat().st_size})
 preserved={};sources={}
 for n,f in originals.items():
  if (n.startswith('Windows/System32/Drivers/') and Path(n).name not in ('ge8300-render.sys','mt8167-shared.sys')) or (n.startswith('Windows/System32/') and n.endswith('.exe')):
   preserved[n]=f['sha256'];sources[n]=(Path(spec['fresh']['path']).parent/f['backup_path']).as_posix()
 for extra in packages['render']['extra_members']:
  n='Windows/System32/'+extra['installed'].replace('\\','/');require(extra['sha256']==extra['baseline_sha256'],'Retain UMD/FW');q=rooted(root,prior['preserved_sources'][n]);require(sha(q)==extra['sha256'] and sha(rooted(root,gp/extra['name']))==extra['sha256'],'Actual UMD/FW mismatch');preserved[n]=extra['sha256'];sources[n]=q.relative_to(root).as_posix()
 boot={'mode':'matched-parent1117-ATCV1-ATCO-boot2e6','boot_path':str(tp.parent/'firmware/boot-final/boot.img'),'boot_sha256':ts['boot_sha256'],'baseline_boot_sha256':prior['boot_transition']['boot_sha256'],'main_handoff_required':True,'boot_write_required':True,'gpt_written':False,'mixed_old_new_allowed':False}
 require(sha(rooted(root,boot['boot_path']))==boot['boot_sha256'],'Matched boot mismatch')
 client=_gpu.fixed_client(root);client.update(name='gpu_suite',relative_directory=SERVICE+'\\clients',phase=1,dependencies=[]);pin(client['main_seal'])
 oldthermal=pin(spec['prior151_policy']);status=copy.deepcopy(oldthermal['diagnostic_clients'][0]);require(status['name']=='thermal_status' and status['sha256']=='542c7393ca79bba6fd244318747bb3fdf59d4469792566f4755085ba28c7cc8d' and status['arguments']==['--status'] and status['phase']==0 and status['timeout_ms']==10000,'Exact status client required');status['relative_directory']=SERVICE+'\\clients';require(sha(rooted(root,Path(status['directory'])/status['exe']))==status['sha256'],'Status bytes changed')
 require(native['nodes']['ROOT\\MT6630COMBO']['present'] and len(native['nodes']['ROOT\\MT6630COMBO']['instances'])==1,'Existing BT root required')
 p=copy.deepcopy(prior);p.update(profile=PROFILE,service=SERVICE,data_directory=SERVICE,draft=False,normal_install_allowed=True,hardware_owner='main',hardware_operations_allowed=False,backup_manifest=spec['fresh']['path'],backup_manifest_sha256=spec['fresh']['sha256'],recovery_boot_id=fm['recovery_boot_id'],capture_selection={'schema':1,'profile':PROFILE,'capture_number':154,'manifest_sha256':spec['fresh']['sha256'],'recovery_boot_id':fm['recovery_boot_id'],'gpu_policy_sha256':native['policy_sha256'],'unchanged_since_capture':True},source_files={n:{k:originals['Windows/System32/config/'+n][k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},packages=packages,baselines=baselines,service_preservation=native['old_services'],captured_registry_values=values,previous_combined152_service=previous,preserved_files=preserved,preserved_sources=sources,planned_reboots=1,install_targets=INSTALL,stage_order=list(ROLES),diagnostic_clients=[status,client],boot_transition=boot,thermal_tuple_main_seal=spec['thermal_main_seal'],GPU343_main_seal=spec['GPU343_main_seal'],GPU343_provenance={'directory':gp.as_posix(),'files':gpu_files,'main_seal':spec['GPU343_main_seal'],'manifest':spec['GPU343_manifest'],'compiled_TUs':0,'runtime_source_commit':gs['runtime_source_commit'],'recipe_source_commit':gs['package_recipe_commit'],'runtime_buildid':gs['runtime_build_id']},gpu_client_log_baseline=spec['mutable_baseline'],mutable_diagnostic_logs={'kind':'observation-not-install-admission','preserve_in_place':True,'overwrite_or_delete':False,'comparison':'saved exact prefix/append/truncation/replacement/missing observation only','files':mutable,'baseline':spec['mutable_baseline']},normal_function_expectations={'temperature_execute':True,'thermal_status_execute':True,'audio_consumer_execute':False,'WLAN_readonly_execute':False,'BT_readonly_execute':False,'GPU_execute':True,'GPU343_real_success':False},observer_contract={**prior['observer_contract'],'fresh_basis':154,'GPU_client_index':1,'GPU_start_event':'PB target1/event0x220','temperature_count':1,'thermal_status_count':1},baseline154_module_identity_reference_only=native['module_identity'],baseline154_init_reference_only=init['gpu_init'])
 for key in ('GPU342_main_seal','GPU342_provenance','previous_combined150_service'):p.pop(key,None)
 p['module_identity_collection'].update(status='required-GPU343-pure-relink-identity-with-SYS-and-INF',expected_buildid='GPU342-tq-clear-v1',expected_sourcebase='eb2bc820c3b67168b16f50dbe3fbf27f513ddfae',same_boot_correlation_required=True,BuildID_alone_distinguishes_relink=False)
 p['parent_contract'].update(ACPI_consume_version=1,ACPI_consume_method='ATCO',matched_parent_version='2026.11.8.1117')
 p['input_pins']=[{'path':n,'sha256':h} for n,h in sorted(pins.items())]
 return p

def generate(p):
 require(p['profile']==PROFILE and p['stage_order']==list(ROLES) and p['install_targets']==INSTALL and p['gpu_client_execute'] is True and p['gpu_client_count']==1 and p['bt_first_install_execute'] is False,'Final155 contract only')
 lines=['/* run155:matched1117/boot and343; temperature/status then GPU index1. */','#define A04_FEATURE148 1','#define A04_FEATURE150 1','#define A04_FEATURE152 1','#define A04_GPU_PHASE1_ENABLED 1','#define A04_RUN_WLAN_READ 0','#define A04_RUN_BT_READ 0','#define A04_FEATURE144 1','#define A04_RENDER_TARGET_INDEX '+str(ROLES.index('render'))+'u','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(r) for r in INSTALL))+'u','#define A04_DIAGNOSTIC_ONLY 0','#define A04_READONLY_PERIPHERAL_STATUS 1','#define A04_RUN_THERMAL_READ 1','#define A04_DIAGNOSTIC_CLIENT_COUNT 2u','#define A04_CANDIDATE_EXTRA_COUNT 2u','#define A04_AUX_HCD1014_IMPORT 0','#define A04_USB_PC_ROLE_OPTIONAL_LEGACY_NODE 1','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0u','static const char deferred_token[]="'+hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const unsigned install_order[12]={6,10,0,1,2,3,4,5,7,8,9,11};','static const struct peripheral_target targets[12]={']
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
 for c in p['diagnostic_clients']:lines.append('{'+','.join(cstr(x) for x in (c['exe'],' '.join(c['arguments']),c['name'],c['relative_directory']))+','+cbytes(c['sha256'])+','+str(c['timeout_ms'])+'u,'+str(c['phase'])+'u},')
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
 require('mt8167-silent-pcm-consumer.exe'.encode('utf-16le') not in b and 'mt8167-thermal-ioctl-status.exe'.encode('utf-16le') in b,'Unchanged peripheral clients must not be linked')

def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args()
 require(x.finalize,'Explicit finalization required; missing signed leaf must fail, never fallback')
 root=x.workspace.resolve(strict=True);policy=load_map(rooted(root,x.map),root);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output only');out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(policy))
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(policy,indent=2)+'\n')
 print('COMBINED155_12TARGET_MATCHED1117_GPU343_VALIDATED')
if __name__=='__main__':main()
