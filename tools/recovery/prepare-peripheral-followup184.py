#!/usr/bin/env python3
"""fresh184原本専用run185/GPU358/BT2018/provider1027 policy。既存媒体と実機へ操作しない。"""
import argparse, copy, hashlib, json, re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'winpe'))
from peripheral_batch_policy import rooted, sha, read, require, cstr, cbytes, inftext
from codec_msdc_transaction import version, datevalue
PROFILE='combined-features-fresh184-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup184'
ROLES=('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render','msdc','bt')
# Explicit semantic targets: no positional guess can classify MSDC as render.
TARGETS={
 'rtc':('MT6392\\RTC','Mt6392Rtc','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt6392-rtc.sys'),
 'thermal':('MT8167\\Thermal','Mt8167Thermal','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-thermal.sys'),
 'usb':('MT8167\\USB20','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-usb20.sys'),
 'wifi':('A04BR3\\MT6630SDIO','Mt6630Runtime','{4D36E972-E325-11CE-BFC1-08002BE10318}','mt6630-combo-runtime.sys'),
 'sd':('ACPI\\MOU0013','Mt8167Sd','{4D36E97B-E325-11CE-BFC1-08002BE10318}','mt8167-sd.sys'),
 'display':('ACPI\\MOU0007','MT8167DisplayTransport','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-display-transport.sys'),
 'parent':('ACPI\\BEN0001','Mt8167Shared','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-shared.sys'),
 'keypad':('MT6392\\Buttons','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-keypad.sys'),
 'battery':('ACPI\\MOU0008','LC709203F','{72631E54-78A4-11D0-BCF7-00AA00B7B32A}','lc709203f.sys'),
 'audio':('MT8167\\AFE','Mt8167Audio','{4D36E96C-E325-11CE-BFC1-08002BE10318}','mt8167-audio.sys'),
 'render':('ACPI\\MOU0006','GE8300Render','{4D36E968-E325-11CE-BFC1-08002BE10318}','ge8300-render.sys'),
 'bt':('ROOT\\MT6630COMBO','Mt6630ComboBluetooth','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt6630-combo-bluetooth.sys'),
 'msdc':('ACPI\\MOU0001','a04msdcboot','{4D36E97B-E325-11CE-BFC1-08002BE10318}','mt8167-msdc.sys')}
INSTALL=['parent','thermal']

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
  q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'Input pin mismatch: '+ref['path']);pins[ref['path']]=ref['sha256'];return read(q)
 require(spec['profile']==PROFILE and spec['install_targets']==INSTALL and spec['hardware_owner']=='main','Exact179 bounded-start scope required')
 fm=pin(spec['fresh']);require(spec['fresh']['sha256']=='ce675a15cbef38ba91696d9f7f863e364de0fc44ee9f4a15824714c59ecd9775' and fm['recovery_boot_id']=='7e0680c8-216e-4575-81e0-24c1eb562df3' and fm['status']=='captured-unmounted' and fm['read_only'] and fm['unmounted'] and not fm['errors'] and len(fm['files'])==42,'Exact fresh184 only')
 originals={f['source_path']:f for f in fm['files']}
 for f in fm['files']:
  q=rooted(root,Path(spec['fresh']['path']).parent/f['backup_path']);require(q.stat().st_size==f['bytes'] and sha(q)==f['sha256'],'Original41 mismatch');pins[q.relative_to(root).as_posix()]=f['sha256']
 n=pin(spec['native']);prior=pin(spec['prior183_policy'])
 require(n['source_manifest_sha256']==spec['fresh']['sha256'] and not n['errors'] and n['originals_unchanged'] and n['full_service_preservation_equal'] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Clean same-trio176 native proof required')
 for name,h in n['source_files'].items():require(originals['Windows/System32/config/'+name]['sha256']==h,'Trio mismatch')
 require(n['module_identity']['observed_buildid']=='GPU359-start-gap-v1' and n['module_identity']['observed_sourcebase']=='e0727eb0d840883a31d3c501b0ee17652ebdcad4' and n['module_identity']['expected_identity_matches'],'Exact actual359 baseline required')
 require(prior['profile']=='combined-features-fresh182-v1' and n['old_services']==prior['service_preservation'] and len(n['old_services'])==18,'18 exact full service fingerprints')
 values=copy.deepcopy(prior['captured_registry_values']);require(len(values)==90,'90 prior values')
 for v in values:
  got=n['captured_named_values'][v['key']+'\\'+v['name']];require(got['type']==v['type'] and got['raw_hex']==v['raw_hex'] and got['bytes']==len(bytes.fromhex(v['raw_hex'])),'Prior named value changed')
 previous=n['new_service'];require(previous['present'] and previous['values']['Start']['value']==4 and previous['values']['ImagePath']['value']=='"C:\\Windows\\System32\\MT8167CombinedFollowup182.exe"','Actual174 stopped required')
 values+=value_contract('SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup182',previous['values']);require(len(values)==95,'95 exact named required')
 gm=pin(spec['GPU359_manifest']);handoff=pin(spec['GPU359_handoff']);gp=Path(spec['GPU359_manifest']['path']).parent
 require(spec['GPU359_manifest']['sha256']=='3ecbc3a5df565251f3969e7e45c4465d281b183ba6333df1d761da01cfc9f140' and gm['runtime_build_id']=='GPU359-start-gap-v1' and handoff['hardware_verified'] is False,'Adopt r2 only')
 for f in pin(spec['GPU359_pins'])['pins']:
  q=rooted(root,f['path']);require(sha(q)==f['sha256'] and q.stat().st_size==f['bytes'],'356 full closure pin');pins[f['path']]=f['sha256']
 for f in gm['outputs']:
  q=rooted(root,gp/f['name']);require(sha(q)==f['sha256'],'356 output pin');pins[q.relative_to(root).as_posix()]=f['sha256']
 pkgs=copy.deepcopy(prior['packages']);rows=pin(spec['clock_package_rows']);pkgs.update(rows)
 require(set(rows)=={'parent','thermal'},'Exact clock1120 parent/thermal fragment')
 outputs={f['name']:f['sha256'] for f in gm['outputs']}
 for e in pkgs['render']['extra_members']:require(e['sha256']==outputs[e['name']],'Retained GPU359 exact bytes');e['baseline_sha256']=e['sha256']
 packages={r:package(root,pkgs[r],r) for r in ROLES};baselines={}
 for r in ROLES:
  row=n['nodes'][TARGETS[r][0]];require(row['present'] and len(row['instances'])==1,'One exact baseline instance');v=row['instances'][0];require(v['instance']==('0' if r in ('display','render') else v['instance']),'Exact GPU0/DIS0 instance')
  require(v['values']['Service']['value'].lower()==TARGETS[r][1].lower(),'Baseline service')
  sel=n['bt_start']['selection'] if r=='bt' else v['selected'];oldsys=TARGETS[r][3]
  b={'mode':'selected' if r in INSTALL else 'retained','version_u64':version(sel['DriverVersion']['value']),'date_filetime':datevalue(sel['DriverDate']['value'].replace('-','/')),'sys_sha256':originals['Windows/System32/Drivers/'+oldsys]['sha256'],'inf':sel['InfPath']['value']}
  require(b['version_u64']==prior['packages'][r]['version_u64'] and b['sys_sha256']==prior['packages'][r]['hashes']['sys'],'Actual baseline tuple mismatch: '+r);baselines[r]=b
 preserved={};sources={}
 for name,f in originals.items():
  if (name.startswith('Windows/System32/Drivers/') and Path(name).name not in ('mt8167-shared.sys','mt8167-thermal.sys')) or (name.startswith('Windows/System32/') and name.endswith('.exe')):
   preserved[name]=f['sha256'];sources[name]=(Path(spec['fresh']['path']).parent/f['backup_path']).as_posix()
 witnesses={}
 for e in packages['render']['extra_members']:
  name='Windows/System32/'+e['installed'].replace('\\','/');q=rooted(root,Path(spec['diagnostics'])/'fixed'/name);require(sha(q)==e['baseline_sha256'],'Actual UMD/FW baseline')
  pins[q.relative_to(root).as_posix()]=sha(q)
  if e['sha256']==e['baseline_sha256']:preserved[name]=sha(q);sources[name]=q.relative_to(root).as_posix()
  else:witnesses[name]={'path':q.relative_to(root).as_posix(),'sha256':sha(q)}
 baseline=pin(spec['mutable_baseline']);require(baseline['source_transaction']=='run183','Latest176 logs');mutable=[]
 for name,h in baseline['files'].items():
  require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name),'Exact historical log');q=rooted(root,Path(spec['diagnostics'])/name);require(sha(q)==h,'Baseline log pin');pins[q.relative_to(root).as_posix()]=h;mutable.append({'relative_path':'MT8167D3DClear/'+name,'source':q.relative_to(root).as_posix(),'sha256':h,'prefix_bytes':q.stat().st_size})
 resetproof=pin(spec['firmware_reset_invariants']);require(resetproof['bounded_recovery_define_preserved'] and resetproof['old_defines_equal_except_unified356'] and not resetproof['mu_common_source_changed'],'Reset/watchdog/UART source invariants')
 boot=pin(spec['boot_manifest']);invariants=pin(spec['firmware_invariants']);require(boot['boot_img_sha256']=='0943ab8a6008942d9701140542da6ecaa902f750bf34b864bb6a66808c08df42' and invariants['mu_common_source_changed'] is False,'Matched356 firmware')
 launcher=pin(spec['launcher_manifest']);require(launcher['sha256']=='1bb21b6157e674ab1cde10eac9aff7d5c7bfd5f63e568ad02fe3049838396497','Fixed signed console launcher')
 client={'name':'gpu_present','directory':spec['client_directory'],'exe':'mt8167-dxgi-console-launch356.exe','sha256':launcher['sha256'],'arguments':[],'timeout_ms':900000,'phase':1,'relative_directory':'.','manifest':spec['launcher_manifest']['path'],'dependencies':[{'exe':'mt8167-dxgi-present356.exe','sha256':'eaa30760ebc38a1955ccb3bbd834ee5c3972ce8842b90e6a58bc5084109ef8d5'}]}
 for c in [client]+client['dependencies']:
  q=rooted(root,Path(client['directory'])/c['exe']);require(sha(q)==c['sha256'],'Console/client pin');pins[q.relative_to(root).as_posix()]=c['sha256']
 p=copy.deepcopy(prior)
 for key in list(p):
  if re.match(r'GPU[0-9]+_(review|provenance|main_seal|boundary_contract)',key) or re.match(r'baseline[0-9]+_',key) or re.match(r'previous_combined[0-9]+_service',key):p.pop(key)
 p.update(profile=PROFILE,service=SERVICE,data_directory=SERVICE,draft=False,normal_install_allowed=True,hardware_owner='main',hardware_operations_allowed=False,backup_manifest=spec['fresh']['path'],backup_manifest_sha256=spec['fresh']['sha256'],recovery_boot_id=fm['recovery_boot_id'],capture_selection={'schema':1,'profile':PROFILE,'capture_number':182,'manifest_sha256':spec['fresh']['sha256'],'recovery_boot_id':fm['recovery_boot_id'],'gpu_policy_sha256':n['policy_sha256'],'unchanged_since_capture':True},source_files={name:{k:originals['Windows/System32/config/'+name][k] for k in ('bytes','sha256')}for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},packages=packages,baselines=baselines,service_preservation=n['old_services'],captured_registry_values=values,previous_combined182_service=previous,preserved_files=preserved,preserved_sources=sources,baseline_witnesses=witnesses,planned_reboots=1,install_targets=INSTALL,stage_order=list(ROLES),diagnostic_clients=[pin(spec['clock_client']),client],boot_transition={'mode':'matched-unified-display356','boot_path':spec['boot_image']['path'],'boot_sha256':boot['boot_img_sha256'],'baseline_boot_sha256':prior['boot_transition']['boot_sha256'],'boot_write_required':False,'gpt_written':False,'mixed_old_new_allowed':False,'next_windows_boot_blocked_until_matched_boot':False,'firmware_manifest':spec['boot_manifest'],'firmware_invariants':spec['firmware_invariants'],'firmware_reset_invariants':spec['firmware_reset_invariants']},gpu_client_log_baseline=spec['mutable_baseline'],mutable_diagnostic_logs={'kind':'observation-not-install-admission','preserve_in_place':True,'overwrite_or_delete':False,'comparison':'exact saved prefix/append/truncation/replacement/missing','files':mutable,'baseline':spec['mutable_baseline']},baseline184_module_identity_reference_only=n['module_identity'],baseline184_init_reference_only=n['gpu_init'],normal_function_expectations={'temperature_execute':False,'thermal_status_execute':False,'audio_consumer_execute':False,'WLAN_readonly_execute':False,'BT_readonly_execute':False,'GPU_execute':True},GPU359_handoff=spec['GPU359_handoff'],GPU359_manifest=spec['GPU359_manifest'],GPU359_pins=spec['GPU359_pins'],standard_present_contract=read(Path(__file__).parents[0]/'ge8300-dxgi-console-launch356-contract.json'),display_transition=spec['display_transition'],present_collect=read(Path(__file__).with_name('present356-collection-contract.json')),power9F_unresolved=True,root_reset_fix_proven=False)
 require(sha(rooted(root,spec['boot_image']['path']))==boot['boot_img_sha256'],'Matched boot actual bytes');pins[spec['boot_image']['path']]=boot['boot_img_sha256']
 for c in [client]+client['dependencies']:
  name='Windows/System32/'+c['exe'];q=rooted(root,Path(spec['diagnostics'])/'fixed'/name);require(sha(q)==c['sha256'],'Retained console asset');pins[q.relative_to(root).as_posix()]=sha(q);p['preserved_files'][name]=sha(q);p['preserved_sources'][name]=q.relative_to(root).as_posix()
 p['module_identity_collection'].update(expected_buildid=gm['runtime_build_id'],expected_sourcebase='e0727eb0d840883a31d3c501b0ee17652ebdcad4',status='required-GPU359-start-gap-identity')
 p['factory_progress_collection']['runtime_build_id']=gm['runtime_build_id']
 p['service_baseline_generation']={'source':spec['native'],'raw_Mt6392Keys_snapshot':n['old_services']['Mt6392Keys'],'raw_WDF_timestamp':n['keypad_wdf_runtime']['timestamp'],'historical_strict_negative_retained':True,'all18_next_generation_full_hashes_strict':True}
 p['observer_contract'].update(fresh_basis=184,GPU_client_index=1,GPU_start_event='phase1 PB target1/event0x220 launcher PID (not evaluator)',temperature_count=0,thermal_status_count=0)
 p['startup_gap359']=read(Path(__file__).with_name('ge8300-start-gap359-contract.json'));p['startup_predicates357']=read(Path(__file__).with_name('ge8300-start357-contract.json'));p['rootfix_claim']=False
 clock=p['diagnostic_clients'][0]
 require(clock['name']=='clock_snapshot1120' and clock['phase']==0 and clock['timeout_ms']==10000 and clock['sha256']=='eae9efe8c2872dda76928120cf9e4919c1e87457cbd61aed2a749a73e1d85b5f','Fixed clock client')
 q=rooted(root,Path(clock['directory'])/clock['exe']);require(sha(q)==clock['sha256'],'Actual signed clock bytes');pins[q.relative_to(root).as_posix()]=sha(q)
 require(all(Path(f['source_path']).name!=clock['exe'] for f in fm['files']),'Clock exe not silently replacing fresh original')
 p['post_ownership_policy360']={'path':'ControlSet001\\Control\\GraphicsDrivers','name':'DisableAutoAcpiPostDeivce','before_raw':'01000000','after_raw':'00000000','controlset_current':1,'strict_one_shot':True,'os_post_success_proven':False}
 p['clock_collection']=pin(spec['clock_collection']);p['clock_fragment']=spec['clock_fragment']
 p['observer_contract']['diagnostic_clients']=2
 p['observer_contract']['clock_client_index']=0
 p['observer_contract']['clock_timeout_ms']=10000
 p['observer_contract']['GPU_timeout_ms']=900000
 p['observer_contract']['absolute_batch_timeout_seconds']=1080
 p['display_transition']=copy.deepcopy(p['display_transition'])
 p['display_transition']['DIF_order']=[]
 p['display_transition']['old_DOD_retirement']='historical177 only; GPU359/DIS0 unchanged, no display DIF in185'
 p['present_collect']=copy.deepcopy(p['present_collect'])
 p['present_collect']['join']=p['present_collect']['join'].replace('SCM client0','SCM client1')
 p['present_collect']['sameboot_required']=p['present_collect']['sameboot_required'].replace('ModuleIdentity356','ModuleIdentity359')
 p['input_pins']=[{'path':k,'sha256':v}for k,v in sorted(pins.items())]
 return p

def generate(p):
 require(p['profile']==PROFILE and p['stage_order']==list(ROLES) and p['install_targets']==INSTALL and p['gpu_client_execute'] is True and p['gpu_client_count']==1 and p['bt_first_install_execute'] is False,'Final185 contract only')
 lines=['/* run185:DIS0 System transport then GPU0 unified356; matchedboot required; BT retained. */','#define A04_UNIFIED_DISPLAY177 1','#define A04_FEATURE160 1','#define A04_FEATURE148 1','#define A04_FEATURE150 1','#define A04_FEATURE152 1','#define A04_GPU_PHASE1_ENABLED 1','#define A04_RUN_WLAN_READ 0','#define A04_RUN_BT_READ 0','#define A04_FEATURE144 1','#define A04_RENDER_TARGET_INDEX '+str(ROLES.index('render'))+'u','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(r) for r in INSTALL))+'u','#define A04_DIAGNOSTIC_ONLY 0','#define A04_READONLY_PERIPHERAL_STATUS 1','#define A04_RUN_THERMAL_READ 0','#define A04_DIAGNOSTIC_CLIENT_COUNT 2u','#define A04_CANDIDATE_EXTRA_COUNT 2u','#define A04_AUX_HCD1014_IMPORT 0','#define A04_USB_PC_ROLE_OPTIONAL_LEGACY_NODE 1','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0u','static const char deferred_token[]="'+hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const unsigned install_order[13]={6,1,5,10,3,12,0,2,4,7,8,9,11};','static const struct peripheral_target targets[13]={']
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
 for s in (SERVICE+'.exe',SERVICE,'MT8167CombinedFollowup148','MT8167CombinedFollowup146','mt8167-msdc.sys','mt8167-dxgi-console-launch356.exe'):
  require(s.encode('utf-16le') in b,'Missing required UTF16 identity: '+s)
 require('bt2013-install.bin'.encode('utf-16le') not in b,'BT first-install code must not be linked')
 require('mt8167-silent-pcm-consumer.exe'.encode('utf-16le') not in b and 'mt8167-thermal-ioctl-status.exe'.encode('utf-16le') not in b,'Unchanged peripheral clients must not be linked')

def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args()
 require(x.finalize,'Explicit finalization required; missing signed leaf must fail, never fallback')
 root=x.workspace.resolve(strict=True);policy=load_map(rooted(root,x.map),root);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output only');out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(policy))
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(policy,indent=2)+'\n')
 print('COMBINED177_UNIFIED356_ACTUAL176_VALIDATED')
if __name__=='__main__':main()
