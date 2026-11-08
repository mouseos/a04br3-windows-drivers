#!/usr/bin/env python3
"""fresh134専用の後段結合。未完のpinを成功/arm可能へ変換しない。"""
import argparse,hashlib,json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import rooted,sha,read,require,cstr,cbytes,inftext
from codec_msdc_transaction import version,datevalue
from peripheral_next_policy import TARGETS as OLD_TARGETS
COMBINED_GPU_ADAPTER_READY=True
CLIENT_NAMES=('clock_compare','battery_temperature','accdet_status','afe_jack','gpu_suite')
PROFILE='combined-gpu332-fresh134-v1';SERVICE='MT8167CombinedFollowup134'
FRESH='9d4b078f989a589166ad7ef53a211251628e6d32a93c8c66fe4858233f29bee2';BOOT='3d353a0c-b6e9-4697-957d-aa2b8b82cbe7'
ROLES=('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render')
TARGETS={**OLD_TARGETS,'render':('ACPI\\MOU0006','GE8300Render','{4D36E968-E325-11CE-BFC1-08002BE10318}','ge8300-render.sys'),'audio':('MT8167\\AFE','Mt8167Audio','{4D36E96C-E325-11CE-BFC1-08002BE10318}','mt8167-audio.sys'),'keypad':('MT6392\\Buttons','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-keypad.sys'),'battery':('ACPI\\MOU0008','LC709203F','{72631E54-78A4-11D0-BCF7-00AA00B7B32A}','lc709203f.sys')}
def paths():
 b='C:\\Windows\\System32\\'+SERVICE
 return '\n'.join('#define '+k+' '+cstr(v) for k,v in [('A04_PERIPHERAL_SERVICE',SERVICE),('A04_PERIPHERAL_DIRECTORY',SERVICE),('A04_CAPTURE_DIRECTORY',b+'\\capture'),('A04_CAPTURE_LOG_PATH',b+'\\capture\\capture.log'),('A04_CAPTURE_MUTEX',SERVICE+'CaptureOnce'),('A04_WLAN_SCAN_LOG_PATH',b+'\\logs\\wlan-scan.txt'),('A04_WLAN_WIFI_ENUM_PATH','SYSTEM\\CurrentControlSet\\Enum\\A04BR3\\MT6630SDIO\\3&7ce9159&0&0')])+'\n'
def validate(root,path):
 m=read(path);require(m['schema']==1 and m['profile']==PROFILE and m['service']==m['data_directory']==SERVICE and m['hardware_owner']=='main' and m['hardware_operations_allowed'] is False,'Explicit followup124 contract required')
 def pin(e):
  p=rooted(root,e['path']);require(sha(p)==e['sha256'],'Pinned input mismatch');return p,read(p)
 f,fm=pin(m['fresh']);require(sha(f)==FRESH and fm['recovery_boot_id']==m['fresh']['recovery_boot_id']==BOOT and fm['status']=='captured-unmounted' and fm['unmounted'] and fm['read_only'] and fm['errors']==[],'Only current fresh134 eligible')
 files={x['source_path']:x for x in fm['files']};require(len(files)==35,'Exact fresh134 28 files required')
 for x in files.values():p=rooted(root,f.parent/x['backup_path']);require(sha(p)==x['sha256'] and p.stat().st_size==x['bytes'],'Original fresh134 changed')
 _,raw=pin(m['native_two']);require(raw['source_manifest_sha256']==FRESH and raw['errors']==[] and raw['originals_unchanged'] and all(raw[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Native cleanup/source failure')
 keypad=raw['nodes']['MT6392\\Buttons'];require(keypad['present'] and len(keypad['instances'])==1,'One actual Buttons node required');k=keypad['instances'][0];v=k['values'];sel=k.get('selected',{})
 require(k['instance']=='3&7ce9159&0&0' and k['selected_present'] and v['Service']=={'present':True,'kind':'String','value':'mshidkmdf'} and v['ClassGUID']=={'present':True,'kind':'String','value':'{745a17a0-74d3-11d0-b6fe-00a0c90f57da}'} and v['LowerFilters']=={'present':True,'kind':'MultiString','value':['Mt6392Keys']} and v['HardwareID']=={'present':True,'kind':'MultiString','value':['MT6392\\Buttons']} and sel['DriverVersion']['value']=='0.0.1.2' and sel['InfPath']['value']=='oem138.inf','Only actual known keypad1.2 selection; no unknown provider accepted')
 _,bindings=pin(m['bindings_eleven']);require(bindings['source_manifest_sha256']==FRESH and bindings['errors']==[] and all(bindings[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Same current native selections required')
 gp=rooted(root,m['gpu']['ledger_proof']);g=read(gp);require(sha(gp)==m['gpu']['ledger_proof_sha256'] and g['errors']==[] and g['originals_unchanged'] and g['old_gpu_service']['Start']['value']==4 and m['gpu']['start']==4 and m['gpu']['client_execute'] is False,'GPU Start4/full121 ledger must remain intact')
 require(set(m['packages'])==set(ROLES) and m['install_targets']==['render'],'Only the explicitly pinned four adopted roles; root fixes require a new reviewed policy revision')
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
  if role in ('audio','render') and role in m['install_targets']:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])<packages[role]['version_u64'],'Exact older installed role required');baselines[role]={'mode':'selected','version_u64':version(node[0]['selected']['DriverVersion']),'date_filetime':datevalue(node[0]['selected']['DriverDate'].replace('-','/')),'sys_sha256':files['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256'],'inf':node[0]['selected']['InfPath']}
  else:
   node=bindings['nodes'][TARGETS[role][0]];require(len(node)==1 and version(node[0]['selected']['DriverVersion'])==packages[role]['version_u64'] and files['Windows/System32/Drivers/'+TARGETS[role][3]]['sha256']==hs['sys'],'Retained roles must be current exact bytes/version; never reinstall/downgrade');baselines[role]={'mode':'retained','version_u64':packages[role]['version_u64'],'date_filetime':packages[role]['date_filetime'],'sys_sha256':hs['sys'],'inf':node[0]['selected']['InfPath']}
 require(baselines['render']['version_u64']==version('0.0.3.27') and baselines['render']['sys_sha256']=='4392efe1d9ad1bf8d254db3f3565cf9d7730c0f4edbfae0960b53e23ed63146a','Only actual fresh134 installed327 may update to332')
 # Independent candidate/source and full baseline44 joins, never version-only.
 for role in m['install_targets']:
  e=m['packages'][role];require(e.get('main_seal') is not None,'Changed role needs explicit main seal');pin(e['main_seal'])
 gp=gpu=m['candidate_gpu_suite'];mp=rooted(root,gpu['manifest']);pm=read(mp)
 require(sha(mp)==gpu['manifest_sha256']=='55a4d2e6c918275aeccb0364c8af823c1c4f25defd54cf1f3df9753748d753d8' and pm['schema']=='readback-safety332-independent-sources-v1' and pm['package_source_commit']==pm['kernel_binary_source_commit']==pm['native_owner_source_commit']==pm['umd_source_commit']==pm['new_client_source_commit']==gpu['binary_source']=='7c6ef6a83197b655f0f3fb27d4749102e8127940','Exact332 runtime source attribution required')
 require(pm['adapter_contract_version']==7 and pm['driver_wire_version']==5 and pm['shader_draw_version']==4 and pm['texture_draw_version']==3 and pm['sampler_profile_version']==4 and pm['new_client_file']=='ge8300-d3d-texture-boundary.exe' and pm['DMA_IRQL_fix_source_commit']=='9d43d5a072a7b2adf8ae027c4039262b26aab5d7' and pm['readback_busy_timeout_ms']==30000 and pm['durable_progress_bytes']==4096 and pm['root_reset_fix_proven'] is False,'332 private contract/safety evidence mismatch')
 for name,h in [('native332-build-manifest.json','f4885121b25bbab16417d0cd60df2333717cb3ec4e4759e1113aeddc5ac60729'),('native-dependency-closure.json','f5245d5af584770af7eb92ac12ac37fa6a5aab39876086771acfd4bfd9d28526'),('baseline331-signed-manifest.json','1cd4aeee160dd5ebe167b2eaa05f060af1df07802b2fcac7163db3d4fe982a5b')]:
  f=rooted(root,Path(gpu['directory'])/name);require(sha(f)==h,'332 independent closure/build/base pin mismatch')
 require(pm['build_manifest_sha256']=='f4885121b25bbab16417d0cd60df2333717cb3ec4e4759e1113aeddc5ac60729','Do not confuse build manifest with signed manifest')
 _,gs=pin(gpu['main_seal']);require(gs['status']=='main-artifact-source-pins-verified' and gs['source_commit']==gpu['binary_source'] and gs['manifest_sha256']==sha(mp) and gs['verified_output_and_retained_count']==36 and gs['runtime_driver_delta_since_build'] is False and gs['later_docs_decoder_commit']=='e41c7321db10cb796635c17aadabb1c473439139','Main332 independent seal/source distinction required')
 _,producer=pin(gpu['producer_pins']);require(producer['signed_manifest_sha256']==sha(mp) and producer['source_commit']==gpu['binary_source'] and producer['outputs']==pm['outputs'],'Producer332 pins must join exact9 runtime outputs')
 require(len(pm['outputs'])==9 and len({x['name'] for x in pm['outputs']})==9,'Exact332 output set')
 for x in pm['outputs']:require(sha(rooted(root,Path(gpu['directory'])/x['name']))==x['sha256'],'Exact332 output byte mismatch')
 pm=read(rooted(root,m['packages']['parent']['manifest']));require(pm['source_commit']=='97b87a4b0d226eb093507cd5b886acfdbf2c0391' and pm['native_interface_version']==3 and pm['native_interface_parent_build']==1104 and pm['child_resource_count']==0 and pm['accdet_status_ioctl']=='0x00226558' and pm['accdet_qualification_relaxed'] is False,'Atomic1104/thermal18 V3 ABI1104 pair required')
 mb=m['matched_boot'];bf=rooted(root,mb['path']);require(sha(bf)==mb['sha256']=='4b67b67aedc4beb9863bfe1791de8713f925d8eedc6dfc702ea66df82895927b','DSB matchedboot pin mismatch');pin(mb['main_seal']);rp,rs=pin(m['rtc_adoption_seal']);require(rs['current_device_loader_pin_verified'] and rs['installed_loader_sha256']=='219464f17d75b3bf4322804590394fc817f5afe8efc576086f0afb32832d4fe2' and sha(rooted(root,rs['installed_loader_path']))==rs['installed_loader_sha256'],'Actual loader pin required')
 bp,bm=pin(m['gpu_client_log_baseline']);require(bm['baseline_count']==47 and bm['fresh_manifest_sha256']==FRESH and bm['previous44_preserved'] and len(bm['files'])==47,'Current44 log baseline required')
 for name,h in bm['files'].items():require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name) and sha(rooted(root,Path(bm['source_directory'])/name))==h,'Baseline44 actual file mismatch')
 ex=m['unified_execution'];require(ex['install_phase_timeout_seconds']==900 and ex['postreboot_diagnostic_total_timeout_seconds']==1500 and ex['gpu_client_timeout_seconds']==900 and ex['rotation_client_timeout_seconds']==300,'Explicit final phase budgets required')
 require(m['gpu_client_execute'] is True and m['diagnostics']['gpu_suite']['exe']=='ge8300-d3d-texture-boundary.exe' and m['diagnostics']['gpu_suite']['sha256']=='055bbff39b5e28a8e64f5a1b358b6084f68e03bfcdcebf7425d2e1b60fae6789' and m['diagnostics']['gpu_suite']['arguments']==['--hardware=PowerVR'] and m['diagnostics']['gpu_suite']['phase']==1 and m['diagnostics']['gpu_suite']['timeout_ms']==900000 and m['diagnostics']['gpu_suite']['one_shot'],'Exactly one signed332 hardware client last')
 expected_clients={'clock_compare':('mt6392-clock-compare.exe','9aedebc8ed1d8d09f469657010ab8f612bde250e7cd2a388199b53dfa06c833c',[]),'battery_temperature':('lc709203f-temperature.exe','d1dfab34f139848964d68b8765489c06bbe3781165e8f9c114c2e312c5e06861',[]),'accdet_status':('mt8167-accdet-status.exe','bf0fb4dc3ea00efde7c19fbfec84cffbc3626a1c67a2c607ce14b09dedfd4a21',['--status']),'afe_jack':('mt8167-afe-jack-read.exe','dc3d705301b97d6263da69f0a94ffde11439b5fdd96e6a0df9fc5ab46d606d4e',['--read'])}
 for name,(exe,h,args) in expected_clients.items():
  v=m['diagnostics'][name];require(v['exe']==exe and v['sha256']==h and v['arguments']==args and v['one_shot'] and v['phase']==0 and v['timeout_ms']==10000 and v['relative_directory']==SERVICE+'\\clients' and not v.get('dependencies',[]),'Exact readonly client pin/args/path required')
 require(m['rotation_execute'] is False and m['wlan_scan_execute'] is False and m['planned_reboots']==1 and m['install_targets']==['render'],'No repeated scan/rotation/DIF/reboot')
 blockers=list(m['blockers'])
 return m,packages,baselines,blockers

def header(packages,baselines,policy=None):
 lines=['/* Fixed fresh134 profile; preview remains non-armable. */','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(k) for k in CURRENT_INSTALL))+'u','#define A04_DIAGNOSTIC_ONLY 0','#define A04_READONLY_PERIPHERAL_STATUS 1','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0x0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0x0u','#define A04_UNBOUND_KEYPAD_CONFIG 0x60u','static const char deferred_token[]="'+(hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest() if policy is not None else 'DRAFT-NOT-ARMED')+'";','static const struct peripheral_target targets[11]={']
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
 for name in ('Windows/System32/MT8167CombinedFollowup124.exe','Windows/System32/MT8167CombinedFollowup126.exe','Windows/System32/MT8167CombinedFollowup128.exe','Windows/System32/MT8167CombinedFollowup130.exe','Windows/System32/MT8167CombinedFollowup132.exe','Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw'):
  e=next((e for e in fm['files'] if e['source_path']==name),None)
  witness=rooted(root,Path(m['fresh']['path']).parent/e['backup_path']) if e else rooted(root,('port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/' if name=='Windows/System32/MT8167RenderSingleGpu.exe' else 'port/xnu/out/private-windows-rt-combined-run129-diagnostics/fixed/')+name)
  if name!='Windows/System32/ge8300-umd.dll':CURRENT_PRESERVED[name]=sha(witness)
 ne=m['native_baseline'];np=rooted(root,ne['path']);require(sha(np)==ne['sha256'],'Old service snapshot pin mismatch');n=read(np);require(n['source_manifest_sha256']==FRESH and n['errors']==[] and n['originals_unchanged'] and set(n['old_services'])==set(m['old_services']),'Seven old service snapshot source mismatch');CURRENT_OLD_SERVICES=n['old_services']
 CURRENT_CLIENTS=[]
 for name in CLIENT_NAMES:
  v=m['diagnostics'][name];require(sha(rooted(root,v['directory'])/v['exe'])==v['sha256'],'Signed client byte pin mismatch')
  for dep in v.get('dependencies',[]):require(sha(rooted(root,v['directory'])/dep['exe'])==dep['sha256'],'Signed client dependency byte pin mismatch')
  CURRENT_CLIENTS.append({**v,'name':name})
 preserved_sources={e['source_path']:(Path(m['fresh']['path']).parent/e['backup_path']).as_posix() for e in fm['files'] if e['source_path'] in CURRENT_PRESERVED}
 for name in CURRENT_PRESERVED:
  if name not in preserved_sources:preserved_sources[name]=('port/xnu/out/private-windows-rt-peripheral-run123-diagnostics/fixed/' if name=='Windows/System32/MT8167RenderSingleGpu.exe' else 'port/xnu/out/private-windows-rt-combined-run129-diagnostics/fixed/')+name
 clients=[{**v,'relative_directory':v.get('relative_directory',SERVICE+'\\clients'),'dependencies':v.get('dependencies',[])} for v in CURRENT_CLIENTS]
 policy={'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':SERVICE,'draft':not finalize,'normal_install_allowed':bool(finalize),'backup_manifest':m['fresh']['path'],'backup_manifest_sha256':FRESH,'recovery_boot_id':BOOT,'capture_selection':{'schema':1,'profile':PROFILE,'capture_number':134,'manifest_sha256':FRESH,'recovery_boot_id':BOOT,'gpu_policy_sha256':m['gpu']['policy_sha256'],'unchanged_since_capture':True},'source_files':{n:{k:next(e for e in fm['files'] if e['source_path']=='Windows/System32/config/'+n)[k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'packages':pkgs,'baselines':base,'service_preservation':CURRENT_OLD_SERVICES,'preserved_files':CURRENT_PRESERVED,'preserved_sources':preserved_sources,'planned_reboots':1,'install_targets':m['install_targets'],'stage_order':list(ROLES),'first_install_mask':0,'deferred_unbound_mask':0,'battery_baseline_sys_sha256':base['battery']['sys_sha256'],'diagnostic_clients':clients,'gpu_install':True,'diagnostic_only':False,'engine_retest':False,'device_verified':False,'boot_transition':{'mode':'preserve-current-matched-rtc560cf','boot_path':m['matched_boot']['path'],'boot_sha256':m['matched_boot']['sha256'],'main_handoff_required':True,'boot_write_required':False,'gpt_written':False}}
 policy['observer_contract']=m['unified_execution'];policy['gpu_client_log_baseline']=m['gpu_client_log_baseline'];policy['gpu_new_log_contract']=m['gpu_new_log_contract'];policy['gpu_candidate']=m['candidate_gpu_suite'];policy['gpu_client_execute']=True;policy['rtc_adoption_seal']=m['rtc_adoption_seal'];policy['prior_rotation_log']=None;policy['reuse_rotation_directory_without_prior_active_log']=False;policy['wlan_fixed_log_directory']='MT8167CombinedFollowup124';policy['keypad_deferred_unbound']=False
 policy['prior_rotation_executables']=[]
 policy['baseline_witnesses']={'Windows/System32/ge8300-umd.dll':m['current_render_witnesses']['umd'],'Windows/System32/Drivers/ge8300-render.sys':m['current_render_witnesses']['sys']};require(sha(rooted(root,m['current_render_witnesses']['sys']['path']))==m['current_render_witnesses']['sys']['sha256']==base['render']['sys_sha256'],'Actual oldSYS witness required');require(sha(rooted(root,m['current_render_witnesses']['umd']['path']))==m['current_render_witnesses']['umd']['sha256']=='1fd19d9a851f1316592c097cbe46e6ea65a2c949b23b7a6915ae19c39c916c85','Actual current327 UMD witness required')
 files_lc=next(e for e in fm['files'] if e['source_path']=='Windows/System32/Drivers/lc709203f.sys')
 policy['baseline_witnesses']['Windows/System32/Drivers/lc709203f.sys']={'path':(Path(m['fresh']['path']).parent/files_lc['backup_path']).as_posix(),'sha256':base['battery']['sys_sha256']}
 require([c.get('phase',0) for c in clients]==[0,0,0,0,1],'Only one GPU client last after five peripheral clients')
 return policy,m,blocks

def load_map(path,root):return build_policy(root,path,True)[0]
def generate(p):
 global CURRENT_PRESERVED,CURRENT_OLD_SERVICES,CURRENT_CLIENTS,CURRENT_INSTALL
 require(p['profile']==PROFILE and p['service']==SERVICE and not p['draft'] and p['normal_install_allowed'],'Final fixed profile only')
 CURRENT_PRESERVED=p['preserved_files'];CURRENT_OLD_SERVICES=p['service_preservation'];CURRENT_CLIENTS=p['diagnostic_clients'];CURRENT_INSTALL=p['install_targets']
 return header(p['packages'],p['baselines'],p)
def assert_paths(image,capture=False):
 b=Path(image).read_bytes();require((SERVICE+'.exe').encode('utf-16le') in b and SERVICE.encode('utf-16le') in b,'Compiled service/data UTF16 identity missing')
 require('mt6392-clock-compare.exe'.encode('utf-16le') in b and 'mt8167-afe-jack-read.exe'.encode('utf-16le') in b,'Required RTC/AFE clients missing');require('ge8300-d3d-texture-boundary.exe'.encode('utf-16le') in b and ('ge8300-d3d-texture.exe'+chr(0)).encode('utf-16le') not in b,'Only new332 client path must be executable');require('MT8167CombinedFollowup132'.encode('utf-16le') in b,'Old service132 preservation missing')
def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args();root=x.workspace.resolve(strict=True);p,m,blocks=build_policy(root,x.map,x.finalize);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root output required');out.mkdir(mode=0o700)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(header(p['packages'],p['baselines'],p if x.finalize else None))
 for n in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/n).write_text(json.dumps(p,indent=2)+'\n')
 (out/'validation.json').write_text(json.dumps({'schema':1,'profile':PROFILE,'draft':not x.finalize,'normal_install_allowed':x.finalize,'blockers':blocks,'device_verified':False},indent=2)+'\n');print('COMBINED134_FINAL_INPUT_VALIDATED' if x.finalize else 'COMBINED134_DRAFT_VALIDATED_NOT_ARMABLE')
if __name__=='__main__':main()
