#!/usr/bin/env python3
"""fresh144専用の後段結合。未完のpinを成功/arm可能へ変換しない。"""
import argparse,hashlib,json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import rooted,sha,read,require,cstr,cbytes,inftext
from codec_msdc_transaction import version,datevalue
from peripheral_next_policy import TARGETS as OLD_TARGETS
COMBINED_GPU_ADAPTER_READY=True # signed340 and final pixel client seals required
CLIENT_NAMES=('gpu_suite',)
PROFILE='combined-features-fresh144-v1';SERVICE='MT8167CombinedFollowup144'
FRESH='0faa9fd09cce293d411ad025e64e54c27d5975948b54935c00e25a3feff15a60';BOOT='c35ceece-d15b-4e07-98f9-5f1d2accd64f'
ROLES=('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render')
TARGETS={**OLD_TARGETS,'wifi':('A04BR3\\MT6630SDIO','Mt6630Runtime','{4D36E972-E325-11CE-BFC1-08002BE10318}','mt6630-combo-runtime.sys'),'render':('ACPI\\MOU0006','GE8300Render','{4D36E968-E325-11CE-BFC1-08002BE10318}','ge8300-render.sys'),'audio':('MT8167\\AFE','Mt8167Audio','{4D36E96C-E325-11CE-BFC1-08002BE10318}','mt8167-audio.sys'),'keypad':('MT6392\\Buttons','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-keypad.sys'),'battery':('ACPI\\MOU0008','LC709203F','{72631E54-78A4-11D0-BCF7-00AA00B7B32A}','lc709203f.sys')}
def boot_adoption_contract():
 # The sole source-owned location for replacement boot pins and adoption state.
 return read(Path(__file__).with_name('combined144-boot-adoption.json'))

def validate_boot_adoption(root,m,pin):
 c=boot_adoption_contract()
 require(c['schema']==1 and c['adoption_status']=='main-artifacts-and-recovery-state-verified','Boot adoption blocked: failed139 cannot be retried; fixed boot and main state proof required')
 require(m['matched_boot']==c['matched_boot'],'Map boot differs from the single source adoption contract')
 mb=c['matched_boot'];require(mb['sha256'] not in c['rejected_boot_sha256'],'Observed failing boot must not be retried after metadata rebind');require(sha(rooted(root,mb['path']))==mb['sha256'],'Adopted boot byte pin mismatch')
 require(m['packages']['parent']['source_commit']==c['required_parent_source_commit'] and sha(rooted(root,Path(m['packages']['parent']['directory'])/m['packages']['parent']['names']['sys']))==c['required_parent_sys_sha256'],'Boot must retain the explicitly matched parent source and SYS')
 require(c['required_firmware_record_version']==3 and c['required_thermal_interface_version']==3 and c['required_thermal_parent_build']==1104 and c['required_headphone_interface_version']==2 and c['required_headphone_parent_build']==1107,'Boot and firmware/QI record versions must remain distinct and compatible')
 _,producer=pin(mb['producer_pins']);_,compatibility=pin(mb['producer_compatibility']);_,seal=pin(mb['main_seal'])
 require(producer['bootsha']==mb['sha256'] and producer['source']=='9ba84ee56d4f65b0e7d0b26e095a73d6ca9f2662' and len(compatibility)==c['compatibility_checks'] and all(v is True for v in compatibility.values()),'Fixed MMIO boot producer/compatibility proof required')
 for leaf,h in producer['files'].items():require(sha(rooted(root,Path(mb['producer_pins']['path']).parent/leaf))==h,'Fixed MMIO boot artifact mismatch')
 require(seal['status']=='main-independent-artifact-seal' and seal['source']==producer['source'] and seal['boot_sha256']==mb['sha256'] and seal['compatibility_checks']==c['compatibility_checks'],'Main independent fixed boot artifact seal required')
 _,state=pin(c['state_rebind_proof'])
 require(state['source_manifest_sha256']==m['fresh']['sha256'] and state['originals_unchanged'] is True and state['errors']==[] and all(state[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')) and c['fresh_manifest_sha256']==m['fresh']['sha256'] and c['fresh_recovery_boot_id']==m['fresh']['recovery_boot_id'],'Fresh142 actual native state required')
 _,bootproof=pin(c['current_boot_proof']);require(bootproof['boot_sha256']==mb['sha256'] and bootproof['matched_boot_applied_before_normal'] and bootproof['matched_boot_maintained_no_rewrite'],'Current boot1fef main proof required')
 return mb

def inf_class_guid(text):
 section=None;values=[]
 for raw in text.splitlines():
  line=raw.split(';',1)[0].strip()
  if line.startswith('[') and line.endswith(']'):section=line[1:-1].strip().lower();continue
  if section=='version' and '=' in line:
   name,value=line.split('=',1)
   if name.strip().lower()=='classguid':values.append(value.strip())
 require(len(values)==1 and re.fullmatch(r'\{[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}\}',values[0]),'One literal Version ClassGuid required')
 return values[0]

def paths():
 b='C:\\Windows\\System32\\'+SERVICE
 return '\n'.join('#define '+k+' '+cstr(v) for k,v in [('A04_PERIPHERAL_SERVICE',SERVICE),('A04_PERIPHERAL_DIRECTORY',SERVICE),('A04_CAPTURE_DIRECTORY',b+'\\capture'),('A04_CAPTURE_LOG_PATH',b+'\\capture\\capture.log'),('A04_CAPTURE_MUTEX',SERVICE+'CaptureOnce'),('A04_WLAN_SCAN_LOG_PATH',b+'\\logs\\wlan-scan.txt'),('A04_WLAN_WIFI_ENUM_PATH','SYSTEM\\CurrentControlSet\\Enum\\A04BR3\\MT6630SDIO\\3&7ce9159&0&0')])+'\n'
def validate(root,path):
 m=read(path);require(not any(m['packages'].get(r,{}).get('pending',False) for r in ('parent','audio')) and not m['diagnostics'].get('audio_consumer',{}).get('pending',False),'Signed1115/audio1033/silent consumer pending; draft cannot validate or arm');require(m['schema']==1 and m['profile']==PROFILE and m['service']==m['data_directory']==SERVICE and m['hardware_owner']=='main' and m['hardware_operations_allowed'] is False,'Explicit followup124 contract required')
 def pin(e):
  p=rooted(root,e['path']);require(sha(p)==e['sha256'],'Pinned input mismatch');return p,read(p)
 f,fm=pin(m['fresh']);require(sha(f)==FRESH and fm['recovery_boot_id']==m['fresh']['recovery_boot_id']==BOOT and fm['status']=='captured-unmounted' and fm['unmounted'] and fm['read_only'] and fm['errors']==[],'Only current fresh144 eligible')
 files={x['source_path']:x for x in fm['files']};require(len(files)==39,'Exact fresh14438 files required')
 for x in files.values():p=rooted(root,f.parent/x['backup_path']);require(sha(p)==x['sha256'] and p.stat().st_size==x['bytes'],'Original fresh144 changed')
 _,raw=pin(m['native_two']);require(raw['source_manifest_sha256']==FRESH and raw['errors']==[] and raw['originals_unchanged'] and all(raw[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Native cleanup/source failure')
 keypad=raw['nodes']['MT6392\\Buttons'];require(keypad['present'] and len(keypad['instances'])==1,'One actual Buttons node required');k=keypad['instances'][0];v=k['values'];sel=k.get('selected',{})
 require(k['instance']=='3&7ce9159&0&0' and k['selected_present'] and v['Service']=={'present':True,'kind':'String','value':'mshidkmdf'} and v['ClassGUID']=={'present':True,'kind':'String','value':'{745a17a0-74d3-11d0-b6fe-00a0c90f57da}'} and v['LowerFilters']=={'present':True,'kind':'MultiString','value':['Mt6392Keys']} and v['HardwareID']=={'present':True,'kind':'MultiString','value':['MT6392\\Buttons']} and sel['DriverVersion']['value']=='0.0.1.2' and sel['InfPath']['value']=='oem138.inf','Only actual known keypad1.2 selection; no unknown provider accepted')
 _,bindings=pin(m['bindings_eleven']);require(bindings['source_manifest_sha256']==FRESH and bindings['errors']==[] and all(bindings[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Same current native selections required')
 gp=rooted(root,m['gpu']['ledger_proof']);g=read(gp);require(sha(gp)==m['gpu']['ledger_proof_sha256'] and g['errors']==[] and g['originals_unchanged'] and g['old_gpu_service']['Start']['value']==4 and m['gpu']['start']==4 and m['gpu']['client_execute'] is False,'GPU Start4/full121 ledger must remain intact')
 require(set(m['packages'])==set(ROLES) and m['install_targets']==['thermal','wifi','render'],'Only the explicitly pinned four adopted roles; root fixes require a new reviewed policy revision')
 packages={};baselines={}
 for role in ROLES:
  e=m['packages'][role];d=rooted(root,e['directory']);mp=rooted(root,e['manifest']);require(sha(mp)==e['manifest_sha256'],'Signed package manifest mismatch');pm=read(mp)
  require(pm.get('package_source_commit')==e['source_commit'] if role=='render' else pm['source_commit']==e['source_commit'],'Package/source mismatch');members={x['sha256'].lower() for x in pm.get('files',pm.get('outputs',[]))};hs={}
  for kind,n in e['names'].items():require(kind in ('inf','sys','cat') and re.fullmatch('[A-Za-z0-9_.-]+',n),'Fixed trio leaves only');hs[kind]=sha(d/n);require(hs[kind] in members,'Package member absent from signed manifest')
  require(e['names']['sys']==TARGETS[role][3],'Exact role SYS leaf required');inf=inftext(d/e['names']['inf']);require('DriverVer='+e['driver_date']+','+e['driver_version'] in inf and TARGETS[role][0].lower() in inf.lower(),'Exact INF version/date/HWID required')
  require(inf_class_guid(inf).lower()==TARGETS[role][2].lower(),'Signed INF ClassGuid differs from exact role target');require(set(hs)=={'inf','sys','cat'},'Exact trio required');packages[role]={**e,'hashes':hs,'version_u64':version(e['driver_version']),'date_filetime':datevalue(e['driver_date'])}
  if role=='render':
   require([x['name'] for x in e['extra_members']]==['ge8300-umd.dll','ge8300-mips.fw'] and [x['installed'] for x in e['extra_members']]==['ge8300-umd.dll','Drivers\\ge8300-mips.fw'],'Exact327 extra files only')
   for x in e['extra_members']:require(sha(d/x['name'])==x['sha256'] and x['sha256'] in members,'Extra CAT package member hash mismatch')
  if role in ('thermal','wifi','render') and role in m['install_targets']:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])<packages[role]['version_u64'],'Exact older installed role required');baselines[role]={'mode':'selected','version_u64':version(node[0]['selected']['DriverVersion']),'date_filetime':datevalue(node[0]['selected']['DriverDate'].replace('-','/')),'sys_sha256':files['Windows/System32/Drivers/'+('mt6630-runtime.sys' if role=='wifi' else TARGETS[role][3])]['sha256'],'inf':node[0]['selected']['InfPath']}
  else:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])==packages[role]['version_u64'] and files['Windows/System32/Drivers/'+('mt6630-runtime.sys' if role=='wifi' else TARGETS[role][3])]['sha256']==hs['sys'],'Retained roles must be current exact bytes/version; never reinstall/downgrade');baselines[role]={'mode':'retained','version_u64':packages[role]['version_u64'],'date_filetime':packages[role]['date_filetime'],'sys_sha256':hs['sys'],'inf':node[0]['selected']['InfPath']}
 require(baselines['render']['version_u64']==version('0.0.3.38') and baselines['render']['sys_sha256']=='875e18d8c0db80ff241b025dfea08de38b7ecff1817886889e572692c0a0e623','Exact installed335 package with retained332 KMD required')
 # Independent candidate/source and full baseline44 joins, never version-only.
 for role in ('parent','audio','wifi'):
  e=m['packages'][role];require(e.get('producer_pins') is not None,'Changed role needs completed producer pins');pin(e['producer_pins']);pin(e['main_seal'])
 gpu=m['candidate_gpu_suite'];mp=rooted(root,gpu['manifest']);pm=read(mp)
 require(sha(mp)==gpu['manifest_sha256']=='504247e51d88d04a050b6689b9e3c7d758124dabd663e9b2d72822c8b276f803' and pm['package_source_commit']==pm['kernel_binary_source_commit']=='03f35eacd064ec7876147bb74c87f8a39fb2444e' and not pm['root_reset_fix_proven'] and not pm['client_changed'] and not pm['umd_changed'],'Exact persistent-copy340 required')
 for e in pm['outputs']+pm['retained_provenance']:require(sha(rooted(root,Path(gpu['directory'])/e['name']))==e['sha256'],'340 output/provenance mismatch')
 require(sha(rooted(root,Path(gpu['directory'])/pm['build_manifest_file']))==pm['build_manifest_sha256'],'340 build provenance mismatch')
 _,seal=pin(gpu['main_seal']);pin(gpu['producer_pins']);require(seal['status']=='main-independent-final-GPU340-artifact-seal' and seal['package_source']==pm['package_source_commit'] and seal['manifest_sha256']==sha(mp) and seal['SYS_sha256']=='b7dfedcc95ec8eba4ce46c48879a6c21f66e605f0588f526e93c849e1031c0ab' and seal['compiled_TUs_verified']==6 and seal['BCBF_valid_signatures']==6,'340 main seal required')
 for role,status,source in [('thermal','main-independent-artifact-seal','b4ef62e16afd8d9adfb18313cfb634d26de31b75'),('wifi','main-independent-combo-artifact-seal','701182c18999e45dbf26704c3feffb51f72b5c19')]:
  _,s=pin(m['packages'][role]['main_seal']);require(s['status']==status and s['source']==source,'Fixed thermal/combo seal required')
 bt=m['bt_first_install'];_,br=pin(bt['native_baseline']);require(br['source_manifest_sha256']==FRESH and not br['errors'] and br['originals_unchanged'] and all(br[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'BT actual native baseline required')
 _,scan=pin(bt['root_hardware_scan']);require(scan['source_manifest_sha256']==FRESH and scan['root_hardware_matches']==[] and not scan['errors'] and scan['originals_unchanged'],'All ROOT hardware-ID matches must be absent at exact144 baseline')
 for key in ('ControlSet001\\Enum\\ROOT\\MT6630COMBO','ControlSet001\\Enum\\A04BR3\\MT6630HCI','ControlSet001\\Services\\Mt6630ComboBluetooth'):require(br['paths'][key]['present'] is False,'BT must be exact known first install; unknown/existing never ignored')
 require(bt['first_install'] and bt['driver_version']=='2026.11.8.2013' and bt['hardware_id']=='ROOT\\MT6630COMBO','Exact BT root2013 only')
 for f in bt['files']:require(sha(rooted(root,Path(bt['directory'])/f['name']))==f['sha256'],'BT2013 bytes mismatch')
 parent=m['packages']['parent'];pm=read(rooted(root,parent['manifest']))
 require(parent['driver_version']=='2026.11.8.1115' and m['parent_contract']=={'thermal_interface_version':3,'thermal_parent_build':1104,'headphone_interface_version':2,'headphone_parent_build':1107,'usb_producer_build':1014,'stock_record_version':3},'Exact integrated1115 contract required')
 _,binding=pin(m['parent_tuple']);pin(m['parent_tuple_pins'])
 require(binding['source_commit']==parent['source_commit']=='df7892ef316e8ce7ffdf01d9f2fbbbbf3840cf2d' and binding['parent']['sys']['sha256']==packages['parent']['hashes']['sys'] and binding['ABI']['thermal_firmware_record_for_this_boot']==3 and binding['ABI']['HP_build']==1107 and binding['ABI']['USB_HCD_build']==1014,'Final1115/stockv3/USB1014 tuple mismatch')
 for e in binding['inputs']:require(sha(rooted(root,e['path']))==e['sha256'],'Final parent tuple input mismatch')
 require(m['packages']['audio']['driver_version']=='2026.11.8.1033','Final unified HP audio1033 only; never stage1031/1032 together')
 mb=validate_boot_adoption(root,m,pin);rp,rs=pin(m['rtc_adoption_seal']);require(rs['current_device_loader_pin_verified'] and rs['installed_loader_sha256']=='219464f17d75b3bf4322804590394fc817f5afe8efc576086f0afb32832d4fe2' and sha(rooted(root,rs['installed_loader_path']))==rs['installed_loader_sha256'],'Actual loader pin required')
 bp,bm=pin(m['gpu_client_log_baseline']);require(bm['source_fresh_manifest_sha256']==FRESH and len(bm['files'])==57,'Current44 log baseline required')
 for name,h in bm['files'].items():require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name) and sha(rooted(root,Path(bm['source_directory'])/name))==h,'Baseline44 actual file mismatch')
 ex=m['unified_execution'];require(ex['install_phase_timeout_seconds']==900 and ex['postreboot_diagnostic_total_timeout_seconds']==1500 and ex['gpu_client_timeout_seconds']==900 and ex['rotation_client_timeout_seconds']==300,'Explicit final phase budgets required')
 require(m['gpu_client_execute'] is True and m['gpu_client_count']==1 and set(m['diagnostics'])=={'gpu_suite'},'One final new pixel client with actual rootfix340 required')
 v=m['diagnostics']['gpu_suite'];require(v['exe']=='ge8300-d3d-texture-pixel-boundary.exe' and v['sha256']=='9934ba6bf9741630a544bf096ea3261c717d45873f91b899cc7e3eae62466add' and v['phase']==1 and v['timeout_ms']==900000 and v['one_shot'] and v['arguments']==['--hardware=PowerVR'],'Exact pixel client/timeout/args')
 _,cs=pin(m['pixel_client_seal']);require(cs['source']=='c0909fcf61c7f2ff1225c9ef8f2d0805472c209a' and cs['client_sha256']==v['sha256'] and cs['only_with_new_GPU_rootfix_trial'],'Independent new pixel client seal required')
 require(m['rotation_execute'] is False and m['wlan_scan_execute'] is False and m['planned_reboots']==1 and m['install_targets']==['thermal','wifi','render'],'No repeated scan/rotation/DIF/reboot')
 companion=m['companion_packages']['hcd1016'];_,hseal=pin(companion['main_seal']);pin(companion['producer_pins']);require(hseal['status']=='main-independent-artifact-seal' and hseal['source']=='17263318b44b81d2f9a8b27edb477b1e414324af','Main HCD1016 leaf seal required');require(companion['stage_only'] and not companion['DIF'] and not companion['rebuilt'],'HCD1016 import-only contract required')
 cp=rooted(root,companion['manifest']);cm=read(cp);require(sha(cp)==companion['manifest_sha256'] and cm['signed'] and cm['source_commit']=='17263318b44b81d2f9a8b27edb477b1e414324af','Retained signed1015 provenance required')
 require([f['name'] for f in companion['files']]==['mt8167-musb-hcd.sys','mt8167-musb-hcd.inf','mt8167-musb-hcd.cat'],'Fixed HCD trio only')
 for f in companion['files']:require(sha(rooted(root,Path(companion['directory'])/f['name']))==f['sha256'] and any(e['path']==f['name'] and e['sha256']==f['sha256'] for e in cm['files']),'HCD trio manifest mismatch')
 blockers=list(m['blockers'])
 return m,packages,baselines,blockers

def header(packages,baselines,policy=None):
 lines=['/* Fixed fresh144 profile; preview remains non-armable. */','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(k) for k in CURRENT_INSTALL))+'u','#define A04_DIAGNOSTIC_ONLY 0','#define A04_READONLY_PERIPHERAL_STATUS 1','#define A04_FEATURE144 1','#define A04_DIAGNOSTIC_CLIENT_COUNT 1u','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0x0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0x0u','#define A04_UNBOUND_KEYPAD_CONFIG 0x60u','static const char deferred_token[]="'+(hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest() if policy is not None else 'DRAFT-NOT-ARMED')+'";','static const struct peripheral_target targets[11]={']
 for role in ROLES:
  t=TARGETS[role];p=packages[role];b=baselines[role];lines.append('{'+','.join(cstr(x) for x in (t[0],t[1],t[2],role,p['names']['inf'],t[3],p['names']['cat']))+','+str(p['version_u64'])+'ULL,'+str(p['date_filetime'])+'ULL,{'+','.join(cbytes(p['hashes'][k]) for k in ('inf','sys','cat'))+'},'+str(b['version_u64'])+'ULL,'+str(b['date_filetime'])+'ULL,'+cbytes(b['sys_sha256'])+','+cstr(b['inf'])+'},')
 lines+=['};','#define A04_CANDIDATE_EXTRA_COUNT 2u','static const struct candidate_extra_file candidate_extras[2]={']
 for e in packages['render']['extra_members']:lines.append('{'+cstr(e['name'])+','+cstr(e['installed'])+','+cbytes(e['sha256'])+','+cbytes(e['baseline_sha256'])+'},')
 lines+=['};','#define A04_AUX_HCD1014_IMPORT 1','#define A04_AUX_HCD_DIRECTORY '+cstr('hcd1016'),'#define A04_AUX_HCD_PROOF_NAME '+cstr('hcd1016-import.bin'),'#define A04_USB_PC_ROLE_OPTIONAL_LEGACY_NODE 1','static const struct preserved_file aux_hcd_files[3]={']
 for f in policy['companion_packages']['hcd1016']['files'] if policy else CURRENT_COMPANION['hcd1016']['files']:lines.append('{'+cstr(f['name'])+','+cbytes(f['sha256'])+'},')
 lines+=['};','static const struct preserved_file preserved_files[]={']
 for name,h in CURRENT_PRESERVED.items():lines.append('{'+cstr(name[len('Windows/System32/'):].replace('/','\\'))+','+cbytes(h)+'},')
 lines+=['};','static const struct preserved_service preserved_services[]={']
 for name,v in CURRENT_OLD_SERVICES.items():lines.append('{'+cstr(name)+','+str(int(v['present']))+','+cbytes(v['root_sha256'] if v['present'] else '0'*64)+'},')
 lines+=['};','static const struct diagnostic_client diagnostic_clients[]={']
 for v in CURRENT_CLIENTS:lines.append('{'+cstr(v['exe'])+','+cstr(' '.join(v['arguments']))+','+cstr(v['name'])+','+cstr(v.get('relative_directory',SERVICE+'\\clients'))+','+cbytes(v['sha256'])+','+str(v.get('timeout_ms',10000))+'u,'+str(v.get('phase',0))+'u},')
 lines+=['};']
 bt=policy['bt_first_install'] if policy else CURRENT_BT
 lines+=['static const struct preserved_file aux_bt_files[3]={']
 for f in bt['files']:lines.append('{'+cstr(f['name'])+','+cbytes(f['sha256'])+'},')
 lines+=['};','#define A04_BT_VERSION '+str(version(bt['driver_version']))+'ULL','#define A04_BT_DATE_LOW '+str(datevalue(bt['driver_date'])&0xffffffff)+'u','#define A04_BT_DATE_HIGH '+str(datevalue(bt['driver_date'])>>32)+'u']
 return '\n'.join(lines)+'\n'

def build_policy(root,path,finalize=False):
 root=Path(root).resolve(strict=True);m,pkgs,base,blocks=validate(root,rooted(root,path))
 if finalize:require(COMBINED_GPU_ADAPTER_READY and not blocks and m['draft'] is False and m['normal_install_allowed'] is True and base['battery']['sys_sha256']!='0'*64,'Finalization blocked by actual witness or final candidate validation')
 global CURRENT_PRESERVED,CURRENT_OLD_SERVICES,CURRENT_CLIENTS,CURRENT_INSTALL,CURRENT_COMPANION,CURRENT_BT
 CURRENT_INSTALL=m['install_targets'];CURRENT_COMPANION=m['companion_packages'];CURRENT_BT=m['bt_first_install']
 fm=read(rooted(root,m['fresh']['path']));new_sys={TARGETS[k][3] for k in m['install_targets']}
 CURRENT_PRESERVED={e['source_path']:e['sha256'] for e in fm['files'] if (e['source_path'].startswith('Windows/System32/Drivers/') and e['source_path'].split('/')[-1] not in new_sys) or e['source_path']=='Windows/System32/MT8167EngineProbe.exe'}
 for name in ('Windows/System32/MT8167CombinedFollowup142.exe','Windows/System32/MT8167CombinedFollowup124.exe','Windows/System32/MT8167CombinedFollowup126.exe','Windows/System32/MT8167CombinedFollowup128.exe','Windows/System32/MT8167CombinedFollowup130.exe','Windows/System32/MT8167CombinedFollowup132.exe','Windows/System32/MT8167CombinedFollowup134.exe','Windows/System32/MT8167CombinedFollowup136.exe','Windows/System32/MT8167CombinedFollowup140.exe','Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw'):
  e=next((e for e in fm['files'] if e['source_path']==name),None)
  witness=rooted(root,m['current_render_witnesses']['umd']['path']) if name=='Windows/System32/ge8300-umd.dll' else (rooted(root,Path(m['fresh']['path']).parent/e['backup_path']) if e else rooted(root,('port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/' if name=='Windows/System32/MT8167RenderSingleGpu.exe' else 'port/xnu/out/private-windows-rt-combined-run129-diagnostics/fixed/')+name))
  CURRENT_PRESERVED[name]=sha(witness)
 ne=m['native_baseline'];np=rooted(root,ne['path']);require(sha(np)==ne['sha256'],'Old service snapshot pin mismatch');n=read(np);require(n['source_manifest_sha256']==FRESH and n['errors']==[] and n['originals_unchanged'] and set(n['old_services'])==set(m['old_services']),'Seven old service snapshot source mismatch');CURRENT_OLD_SERVICES=n['old_services']
 CURRENT_CLIENTS=[]
 for name in CLIENT_NAMES:
  v=m['diagnostics'][name];require(sha(rooted(root,v['directory'])/v['exe'])==v['sha256'],'Signed client byte pin mismatch')
  for dep in v.get('dependencies',[]):require(sha(rooted(root,v['directory'])/dep['exe'])==dep['sha256'],'Signed client dependency byte pin mismatch')
  CURRENT_CLIENTS.append({**v,'name':name})
 preserved_sources={e['source_path']:(Path(m['fresh']['path']).parent/e['backup_path']).as_posix() for e in fm['files'] if e['source_path'] in CURRENT_PRESERVED}
 preserved_sources['Windows/System32/ge8300-umd.dll']=m['current_render_witnesses']['umd']['path']
 for name in CURRENT_PRESERVED:
  if name not in preserved_sources:preserved_sources[name]=('port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/' if name=='Windows/System32/MT8167RenderSingleGpu.exe' else 'port/xnu/out/private-windows-rt-combined-run129-diagnostics/fixed/')+name
 # Preserve all already-deployed GPU clients and the exact53 log baseline without executing them.
 prior_fixed=Path('port/xnu/out/private-windows-rt-combined-run141-diagnostics/fixed')
 for witness in sorted(rooted(root,prior_fixed/'Windows/System32/MT8167CombinedFollowup140/clients').glob('*')):
  if witness.is_file():
   name='Windows/System32/MT8167CombinedFollowup140/clients/'+witness.name;CURRENT_PRESERVED[name]=sha(witness);preserved_sources[name]=witness.relative_to(root).as_posix()
 baseline=read(rooted(root,m['gpu_client_log_baseline']['path']))
 for leaf,h in baseline['files'].items():
  require(sha(rooted(root,Path(baseline['source_directory'])/leaf))==h,'Exact saved GPU log baseline changed')
 clients=[{**v,'relative_directory':v.get('relative_directory',SERVICE+'\\clients'),'dependencies':v.get('dependencies',[])} for v in CURRENT_CLIENTS]
 policy={'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':SERVICE,'draft':not finalize,'normal_install_allowed':bool(finalize),'backup_manifest':m['fresh']['path'],'backup_manifest_sha256':FRESH,'recovery_boot_id':BOOT,'capture_selection':{'schema':1,'profile':PROFILE,'capture_number':144,'manifest_sha256':FRESH,'recovery_boot_id':BOOT,'gpu_policy_sha256':m['gpu']['policy_sha256'],'unchanged_since_capture':True},'source_files':{n:{k:next(e for e in fm['files'] if e['source_path']=='Windows/System32/config/'+n)[k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'packages':pkgs,'baselines':base,'service_preservation':CURRENT_OLD_SERVICES,'preserved_files':CURRENT_PRESERVED,'preserved_sources':preserved_sources,'planned_reboots':1,'install_targets':m['install_targets'],'stage_order':list(ROLES),'first_install_mask':0,'deferred_unbound_mask':0,'battery_baseline_sys_sha256':base['battery']['sys_sha256'],'diagnostic_clients':clients,'gpu_install':True,'diagnostic_only':False,'engine_retest':False,'device_verified':False,'boot_transition':{'mode':'retain-current-mmio-fixed-boot1fef','boot_path':m['matched_boot']['path'],'boot_sha256':m['matched_boot']['sha256'],'main_handoff_required':True,'boot_write_required':False,'gpt_written':False}}
 policy['observer_contract']=m['unified_execution'];policy['gpu_client_log_baseline']=m['gpu_client_log_baseline'];policy['gpu_new_log_contract']=m['gpu_new_log_contract'];policy['gpu_candidate']=m['candidate_gpu_suite'];policy['gpu_client_execute']=True;policy['gpu_client_count']=1;policy['rtc_adoption_seal']=m['rtc_adoption_seal'];policy['prior_rotation_log']=None;policy['reuse_rotation_directory_without_prior_active_log']=False;policy['wlan_fixed_log_directory']='MT8167CombinedFollowup124';policy['keypad_deferred_unbound']=False
 policy['boot_adoption_contract']=boot_adoption_contract();policy['companion_packages']=m['companion_packages'];policy['usb_pc_role_legacy_absence_separate_result']=True;policy['parent_tuple']=m['parent_tuple'];policy['parent_tuple_pins']=m['parent_tuple_pins'];policy['prior_rotation_executables']=[];policy['gpu_delayed_log_collection']={'source_run':143,'baseline_count':57,'execute_new_client':True,'same_normal_mount':True,'preserve_prior_file_changes':baseline.get('prior_file_changes',{}),'binary_boundary_logs_require_GPR1_decoder':True};policy['normal_function_expectations']=m['normal_function_expectations']
 policy['baseline_witnesses']={'Windows/System32/ge8300-umd.dll':m['current_render_witnesses']['umd'],'Windows/System32/Drivers/ge8300-render.sys':m['current_render_witnesses']['sys']};require(sha(rooted(root,m['current_render_witnesses']['sys']['path']))==m['current_render_witnesses']['sys']['sha256']==base['render']['sys_sha256'],'Actual oldSYS witness required');require(sha(rooted(root,m['current_render_witnesses']['umd']['path']))==m['current_render_witnesses']['umd']['sha256']=='3bd57ffe4e5310b14ab00ebff3ab5b1a95f029d41bb1533e00bda42b6950eb3c','Exact installed335 UMD witness required')
 files_lc=next(e for e in fm['files'] if e['source_path']=='Windows/System32/Drivers/lc709203f.sys')
 policy['baseline_witnesses']['Windows/System32/Drivers/lc709203f.sys']={'path':(Path(m['fresh']['path']).parent/files_lc['backup_path']).as_posix(),'sha256':base['battery']['sys_sha256']}
 require(len(clients)==1 and clients[0]['name']=='gpu_suite' and clients[0]['phase']==1,'One final pixel client only');policy['bt_first_install']=m['bt_first_install'];policy['module_identity_collection']=m['module_identity_collection']
 return policy,m,blocks

def load_map(path,root):return build_policy(root,path,True)[0]
def generate(p):
 global CURRENT_PRESERVED,CURRENT_OLD_SERVICES,CURRENT_CLIENTS,CURRENT_INSTALL,CURRENT_COMPANION,CURRENT_BT
 require(p['profile']==PROFILE and p['service']==SERVICE and not p['draft'] and p['normal_install_allowed'],'Final fixed profile only')
 CURRENT_BT=p['bt_first_install'];CURRENT_COMPANION=p['companion_packages'];CURRENT_PRESERVED=p['preserved_files'];CURRENT_OLD_SERVICES=p['service_preservation'];CURRENT_CLIENTS=p['diagnostic_clients'];CURRENT_INSTALL=p['install_targets']
 return header(p['packages'],p['baselines'],p)
def assert_paths(image,capture=False):
 b=Path(image).read_bytes();require((SERVICE+'.exe').encode('utf-16le') in b and SERVICE.encode('utf-16le') in b,'Compiled service/data UTF16 identity missing')
 require('thermal-readonly.bin'.encode('utf-16le') in b,'Required thermal readonly proof missing')
 require('ge8300-d3d-texture-pixel-boundary.exe'.encode('utf-16le') in b,'Pinned pixel client leaf missing');require('MT8167CombinedFollowup136'.encode('utf-16le') in b,'Old136 service preservation missing')

def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args();root=x.workspace.resolve(strict=True);p,m,blocks=build_policy(root,x.map,x.finalize);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root output required');out.mkdir(mode=0o700)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(header(p['packages'],p['baselines'],p if x.finalize else None))
 for n in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/n).write_text(json.dumps(p,indent=2)+'\n')
 (out/'validation.json').write_text(json.dumps({'schema':1,'profile':PROFILE,'draft':not x.finalize,'normal_install_allowed':x.finalize,'blockers':blocks,'device_verified':False},indent=2)+'\n');print('COMBINED144_FINAL_INPUT_VALIDATED' if x.finalize else 'COMBINED144_DRAFT_VALIDATED_NOT_ARMABLE')
if __name__=='__main__':main()
