#!/usr/bin/env python3
"""GPU368専用。未取得fresh200を生成せず、実captureと署名helper入力を要求する。"""
import argparse,copy,hashlib,importlib.util,json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import rooted,sha,read,require
from codec_msdc_transaction import version,datevalue
sp=importlib.util.spec_from_file_location('base200',Path(__file__).with_name('prepare-peripheral-followup184.py'));base=importlib.util.module_from_spec(sp);sp.loader.exec_module(base)
PROFILE='combined-features-fresh200-gpu368-v1';SERVICE=DIRECTORY='MT8167CombinedFollowup200';INSTALL=['render'];ROLES=base.ROLES;TARGETS=base.TARGETS
base.PROFILE=PROFILE;base.SERVICE=base.DIRECTORY=SERVICE;base.INSTALL=INSTALL
SOURCE='a04e22489709db79d7c5a4355419bc7aaf95b057';MANIFEST='eef81706cf51711e375ddd7f6b46b6ef05fe7ed73c0ebd97dd64d484565fc32e'
CAPTURE=200
TIMING=None
THUMB='BCBF6C456C6F0DD0818434763B189488FBF13570'
PRESENT='ab17ed4cf69e6407fa5b017375d39318a89cf544a193ba89fb8a104173eff423';PRESENTER='eaa30760ebc38a1955ccb3bbd834ee5c3972ce8842b90e6a58bc5084109ef8d5'
def paths():return base.paths()
def inventory_contract(c,n,prior):
 require(c['capture_number']==CAPTURE and c['schema']==(f'GPU369_FRESH{CAPTURE}_CAPTURE_CONTRACT_V1' if TIMING else f'GPU368_FRESH{CAPTURE}_CAPTURE_CONTRACT_V1'),'実fresh200専用capture contract')
 names=c['service_names'];ids=c['named_value_ids']
 require(len(names)==len(set(names)) and names and len(names)<=128 and SERVICE not in names,'service集合の重複/範囲')
 require(len(ids)==len(set(ids)) and ids and len(ids)<=2048,'named値集合の重複/範囲')
 require(set(names)==set(n['old_services']) and set(prior['service_preservation'])<=set(names),'実service集合の省略/追加不一致')
 require(set(ids)==set(n['captured_named_values']),'実named値集合の省略/追加不一致')
 require({v['key']+'\\'+v['name'] for v in prior['captured_registry_values']}<=set(ids),'旧named値の観測範囲を省略しない')
 for name in names:
  v=n['old_services'][name];require(set(v)=={'present','root_sha256','subtree_sha256'} and type(v['present']) is bool,'全service fingerprint必須')
  if v['present']:require(all(re.fullmatch('[0-9a-f]{64}',v[k] or '') for k in ('root_sha256','subtree_sha256')),'実fingerprint必須')
 values=[]
 for ident in ids:
  key,name=ident.rsplit('\\',1);v=n['captured_named_values'][ident]
  require(key.startswith('SYSTEM\\CurrentControlSet\\') and key and name and v['type'] in (1,2,3,4,7,11),'実named値schema')
  require(re.fullmatch(r'(?:[0-9a-f]{2}){1,4096}',v['raw_hex']) and v['bytes']==len(bytes.fromhex(v['raw_hex'])),'実raw値の長さ')
  values.append(dict(key=key,name=name,type=v['type'],raw_hex=v['raw_hex']))
 return values

def validate_gpu368_manifest(gm):
 require(gm['schema']=='GPU368_SIGN_ONLY_FIXED198_V1' and gm['package_source_commit']==SOURCE and gm['umd_source_commit']==SOURCE and gm['inf_driverver']=='11/10/2026,0.0.3.68' and gm['hardware_verified'] is False,'新368 schema/固定source')
 require(gm['module_identity']['runtime_build_id']=='GPU359-start-gap-v1' and gm['module_identity']['inherited'] is True,'runtime identityとcompile sourceを分離')

BOOT0943='0943ab8a6008942d9701140542da6ecaa902f750bf34b864bb6a66808c08df42'
def retained_boot_chain_evidence(boot,transition,pin,pin_file):
 # 保存済みの出自・controller記録を検査する。端末bootを読まない。
 require(boot['physical_read_this_capture'] is False and boot['no_intervening_boot_or_gpt_write_reported'] is True,'保持履歴は再read済と表示しない')
 require(boot['boot_sha256']==transition['boot_sha256']==BOOT0943,'既確定matched boot0943のみ')
 host=boot['host_artifact'];require(host['path']==transition['boot_path'] and host['sha256']==BOOT0943,'保持host artifact identity')
 pin_file(host)
 sealed=pin(boot['matched_host_seal']);last_seal=pin(boot['last_write_seal']);last=pin(boot['last_write_result'])
 require(sealed['status']=='validated-main-seal' and sealed['matched_boot_sha256']==BOOT0943 and sealed['boot_path']==host['path'],'sealed matchedboot host出自')
 require(any(x['path']==host['path'] and x['sha256']==host['sha256'] for x in sealed['pinned_files']),'host artifactを実sealがpinしていること')
 require(last_seal['status']=='validated-main-seal' and last_seal['matched_boot_sha256']==BOOT0943 and last_seal['boot_path']==host['path'] and last_seal['boot_write_required'] is True and last_seal['gpt_written'] is False,'最後の書込み出自')
 require(last['transaction']==last_seal['transaction'] and last['boot_sha256']==BOOT0943 and last['boot_written'] is True and last['gpt_written'] is False and last['matched_boot_applied_before_normal'] is True,'計画だけでなく実書込みcontroller記録')
 for step in boot.get('retained_steps',[]):
  row=pin(step);require(row['boot_sha256']==BOOT0943 and row['boot_written'] is False and row['gpt_written'] is False and row['matched_boot_retained_from_previous_run'] is True,'中間保持記録が異なるboot/書込みを示す')
 seal199=pin(boot['trial199_seal']);controller199=pin(boot['trial199_controller'])
 require(seal199['schema']=='MT8167_USB1121_RUN199_MAIN_SEAL_V1' and seal199['status']=='validated-main-seal' and seal199['boot_written'] is False and seal199['gpt_written'] is False,'trial199 sealed no-write scope')
 require(controller199['boot_written'] is False and controller199['gpt_written'] is False,'trial199実controllerのno-write記録必須')
 if CAPTURE==202:
  trial_seal=pin(boot['trial201_seal']);trial=pin(boot['trial201_controller'])
  require(trial_seal['schema']=='GPU368_RUN201_MAIN_SEAL_V1' and trial_seal['status']=='validated-main-seal' and trial_seal['boot_written'] is False and trial_seal['gpt_written'] is False,'実201保持seal')
  media=pin(dict(path=trial_seal['media_directory']+'/manifest.json',sha256=trial_seal['media_manifest_sha256']))
  require(media['policy']['boot_transition']['boot_sha256']==BOOT0943 and media['policy']['boot_transition']['boot_write_required'] is False,'201実mediaのmatchedboot保持契約')
  require(trial['seal_sha256']==boot['trial201_seal']['sha256'] and trial['boot_written'] is False and trial['gpt_written'] is False and trial['capture_manifest_sha256']==boot['source_manifest_sha256'],'実201controllerとcapture202の結合')
 # 新しい物理byteの一致・全履歴の独立監視を主張しない。
 return dict(kind='retained-chain',physical_read_this_capture=False,boot_sha256=BOOT0943,host_artifact_verified=True,last_write_controller_verified=True,trial199_no_boot_or_gpt_write_verified=True,no_intervening_write_reported_by_main=True,evidence_scope='hash-bound saved write/controller/sealed-host provenance; no physical read or byte comparison this capture',hardware_success_inferred=False)

def validate_boot_witness(boot,transition,fresh_sha,recovery_boot_id,pin,pin_file):
 require(boot['source_manifest_sha256']==fresh_sha and boot['recovery_boot_id']==recovery_boot_id and boot['read_only'] is True,'実fresh200へのwitness結合')
 require(transition['boot_write_required'] is False and transition['gpt_written'] is False and transition['boot_sha256']==boot['boot_sha256'],'FW保持、boot/GPT書込みなし')
 if boot.get('kind')=='retained-chain':
  require(boot['schema']=='GPU368_RETAINED_BOOT_CHAIN_V1','retained-chain明示schema')
  return retained_boot_chain_evidence(boot,transition,pin,pin_file)
 require(boot.get('kind','capture-read')=='capture-read' and boot.get('physical_read_this_capture') is True,'read witnessは実readの明示が必要。新readは要求しない')
 return dict(kind='capture-read',physical_read_this_capture=True,boot_sha256=boot['boot_sha256'],hardware_success_inferred=False)

def build_policy(root,spec):
 pins={}
 def pin(ref):
  require(isinstance(ref,dict) and re.fullmatch('[0-9a-f]{64}',ref.get('sha256','')) and ref['sha256']!='0'*64,'未確定入力は拒否')
  q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'実入力SHA不一致 '+ref['path']);pins[q.relative_to(root).as_posix()]=ref['sha256'];return read(q)
 require(spec['profile']==PROFILE and spec['hardware_owner']=='main' and spec['install_targets']==INSTALL,'GPU368 renderのみ')
 require(spec['fresh']['path'].endswith(f'private-windows-rt-prebackup-run{CAPTURE}/manifest.json'),'実fresh200 manifest pathのみ')
 fm=pin(spec['fresh']);n=pin(spec['native']);c=pin(spec['capture_contract']);prior=pin(spec['template_policy'])
 require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[],'完了済み実capture必須')
 require(c['fresh_manifest_sha256']==spec['fresh']['sha256'] and c['native_receipt_sha256']==spec['native']['sha256'] and c['recovery_boot_id']==fm['recovery_boot_id'],'同capture/boot結合')
 require(n['source_manifest_sha256']==spec['fresh']['sha256'] and n['originals_unchanged'] is True and n['errors']==[] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'実native読取り証拠')
 require(n.get('new_service',{}).get('present') is False,'新service200は実captureで不在必須')
 originals={};origin_sources={}
 captures=[(spec['fresh'],fm)]
 if spec.get('supplement'):
  extra=pin(spec['supplement']);require(extra['source_manifest_sha256']==spec['fresh']['sha256'] and extra['recovery_boot_id']==fm['recovery_boot_id'] and extra['status']=='captured-unmounted' and extra['read_only'] is True and extra['unmounted'] is True and extra['errors']==[],'同boot実補足captureだけ')
  captures.append((spec['supplement'],extra))
 for ref,captured in captures:
  seen=set()
  for f in captured['files']:
   name=f['source_path'];require(name not in seen,'capture内重複path');seen.add(name)
   q=rooted(root,Path(ref['path']).parent/f['backup_path']);require(q.stat().st_size==f['bytes'] and sha(q)==f['sha256'],'実capture file不一致');pins[q.relative_to(root).as_posix()]=f['sha256']
   if name in originals:require(originals[name]['sha256']==f['sha256'] and originals[name]['bytes']==f['bytes'],'補足時に原本変化')
   else:originals[name]=f;origin_sources[name]=q.relative_to(root).as_posix()
 require(len(originals)==c['file_count'],'実combined file件数')
 for name,h in n['source_files'].items():require(originals['Windows/System32/config/'+name]['sha256']==h,'同SYSTEM trio')
 values=inventory_contract(c,n,prior)
 post=n['post_ownership_policy360'];require(post['controlset']=='ControlSet001' and post['name']=='DisableAutoAcpiPostDeivce' and post['raw']=='00000000','POST0実値保持')
 gm=pin(spec['GPU368_manifest']);hand=pin(spec['GPU368_handoff'])
 if TIMING:TIMING.validate_manifest(gm,hand,spec)
 else:
  require(spec['GPU368_manifest']['sha256']==MANIFEST,'署名済368 manifest pin');validate_gpu368_manifest(gm)
  require(hand['status']=='signed-artifact-verified-not-hardware' and hand['manifest_sha256']==MANIFEST and hand['compiled_source']==SOURCE and hand['hardware_verified'] is False,'main独立署名review')
 gp=Path(spec['GPU368_manifest']['path']).parent;hashes={x['name']:x['sha256'] for x in gm['outputs']}
 for name,h in hashes.items():
  q=rooted(root,gp/name);require(sha(q)==h,'署名済368出力');pins[q.relative_to(root).as_posix()]=h
 if TIMING:require(hand['outputs']==hashes,'実369全出力とreviewの一致')
 else:require(hashes['ge8300-render.sys']==hand['SYS_sha256'] and hashes['ge8300-umd.dll']==hand['UMD_sha256'],'SYS/UMD同manifest')
 rows=pin(spec['actual_package_rows']);require(set(rows)==set(ROLES),'全13実package行必須')
 if TIMING:rows=TIMING.package_rows(rows,pin)
 baselines={};packages={};preserved={};sources={};witnesses={}
 for role in ROLES:
  old=base.package(root,rows[role],role)
  for name in list(old['names'].values())+[x['name'] for x in old.get('extra_members',[])]:
   q=rooted(root,Path(old['directory'])/name);pins[q.relative_to(root).as_posix()]=sha(q)
  q=rooted(root,old['manifest']);pins[q.relative_to(root).as_posix()]=sha(q)
  row=n['nodes'][TARGETS[role][0]];sysfile=originals['Windows/System32/Drivers/'+TARGETS[role][3]]
  require(sysfile['sha256']==old['hashes']['sys'],'実保持SYSとpackage一致 '+role)
  if role=='usb' and row['present'] is False:
   # canonical2bの既optional legacy USB20経路のみ。新HCDへ差替えない。
   require(prior['packages']['usb']['hashes']['sys']==sysfile['sha256'] and prior['packages']['usb']['manifest_sha256']==old['manifest_sha256'],'旧legacy USB fileの履歴を保持')
   baselines[role]=dict(mode='retained',version_u64=old['version_u64'],date_filetime=old['date_filetime'],sys_sha256=sysfile['sha256'],inf=prior['baselines']['usb']['inf'],selection_present=False,version_origin='historical signed package; actual legacy node absent')
  else:
   require(row['present'] and len(row['instances'])==1,'実nodeの単一選択 '+role)
   inst=row['instances'][0];sel=n['bt_start']['selection'] if role=='bt' else inst['selected'];require(inst['values']['Service']['value'].lower()==TARGETS[role][1].lower(),'実service binding '+role)
   require(version(sel['DriverVersion']['value'])==old['version_u64'] and datevalue(sel['DriverDate']['value'].replace('-','/'))==old['date_filetime'],'実選択tupleとpackage行一致 '+role)
   baselines[role]=dict(mode='selected' if (role in INSTALL if TIMING else role=='render') else 'retained',version_u64=old['version_u64'],date_filetime=old['date_filetime'],sys_sha256=sysfile['sha256'],inf=sel['InfPath']['value'])
  if TIMING and role in INSTALL:
   old=TIMING.adopt(role,old,originals,hashes)
  elif role=='render':
   old.update(directory=gp.as_posix(),manifest=spec['GPU368_manifest']['path'],manifest_sha256=MANIFEST,source_commit=SOURCE,driver_version='0.0.3.68',driver_date='11/10/2026')
   for x in old['extra_members']:
    name='Windows/System32/'+x['installed'].replace('\\','/');f=originals.get(name)
    require(f is not None,'UMD/FWもfresh200の実captureが必要')
    x.update(baseline_sha256=f['sha256'],sha256=hashes[x['name']])
    if x['name']!='ge8300-umd.dll':require(x['sha256']==x['baseline_sha256'],'FW byte保持')
    if x['sha256']!=x['baseline_sha256']:witnesses[name]=dict(path=origin_sources[name],sha256=f['sha256'])
  packages[role]=base.package(root,old,role)
 if TIMING:TIMING.validate_transport(packages,hashes)
 else:
  require(packages['display']['driver_version']=='0.0.3.57' and packages['display']['hashes']['sys']==hashes['display-transport/mt8167-display-transport.sys'],'transport357 byte保持')
  require(packages['display']['hashes']['inf']==hashes['display-transport/mt8167-display-transport.inf'] and packages['display']['hashes']['cat']==hashes['display-transport/mt8167-display-transport.cat'],'transport357 INF/CAT保持')
 for name,f in originals.items():
  if name.startswith('Windows/System32/') and name not in witnesses and name not in ({'Windows/System32/Drivers/'+TARGETS[r][3] for r in INSTALL} if TIMING else {'Windows/System32/Drivers/ge8300-render.sys'}):
   if name.startswith('Windows/System32/Drivers/') or name.endswith(('.exe','.dll','.fw')):
    preserved[name]=f['sha256'];sources[name]=origin_sources[name]
 client=copy.deepcopy(prior['diagnostic_clients'][0]);require(client['name']=='gpu_present' and client['exe']=='mt8167-dxgi-console-launch363.exe' and client['sha256']==PRESENT and client['dependencies']==[dict(exe='mt8167-dxgi-present356.exe',sha256=PRESENTER)],'Present363/356固定')
 client['phase']=1
 for a in [client]+client['dependencies']:
  q=rooted(root,Path(client['directory'])/a['exe']);require(sha(q)==a['sha256'],'Present署名済実byte');pins[q.relative_to(root).as_posix()]=sha(q)
  require(originals['Windows/System32/'+a['exe']]['sha256']==a['sha256'],'Present assetsもfresh200で同byte確認')
 helper=pin(spec['globaldiag_manifest']);proof=pin(spec['globaldiag_import_proof']);directory=Path(spec['globaldiag_manifest']['path']).parent;exe='dxg-global-diag198.exe';q=rooted(root,directory/exe)
 require(spec['globaldiag_manifest']['sha256']=='1dcacdd1c70dacdc69a2e08e1294188d7805a2fbab77f9a24a73bcd28b1e28e6' and spec['globaldiag_import_proof']['sha256']=='896137838e17694cc3cedc506ae31aa9b25e1fffd228f4094e2ca6145f7d3828','実globaldiag署名/proof pin')
 require(helper['source_commit']=='a5d8048c3e1572015d09bd308e4304333d9a53f4' and helper['schema']=='DXG_GLOBAL_DIAG198_BUILD_V1' and helper['API_Type']==9 and helper['raw_capacity']==262144 and helper['executed'] is False and helper['compiled_TUs']==1 and helper['signer']==THUMB and helper['exe_sha256']==sha(q)=='9be80b8176696302a0e4408e6871d41792c780a37bcf6caca817c817b97061ac','実署名helper source/byte')
 require(proof['candidate_sha256']==sha(q) and proof['missing']==[] and proof['hardware_verified'] is False and proof['machine']=='ARMNT' and proof['subsystem']==[6,3] and len(proof['imports'])==14 and all(x['validation']['status']=='present' for x in proof['imports']),'実RT81の14imports解決')
 pins[q.relative_to(root).as_posix()]=sha(q)
 diag=dict(name='gpu_globaldiag',exe=exe,directory=directory.as_posix(),relative_directory='.',sha256=sha(q),dependencies=[],arguments=['"C:\\Windows\\System32\\'+SERVICE+'\\logs\\global-diag368"'],timeout_ms=60000,phase=1)
 boot=pin(spec['boot_witness']);transition=copy.deepcopy(prior['boot_transition'])
 def pin_file(ref):
  q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'保持host artifact SHA不一致');pins[q.relative_to(root).as_posix()]=ref['sha256']
 boot_evidence=(TIMING.boot(boot,transition,spec['fresh']['sha256'],fm['recovery_boot_id'],pin,pin_file) if TIMING else validate_boot_witness(boot,transition,spec['fresh']['sha256'],fm['recovery_boot_id'],pin,pin_file))
 mutable=pin(spec['mutable_baseline']);require(mutable['source_manifest_sha256']==spec['fresh']['sha256'] and mutable['recovery_boot_id']==fm['recovery_boot_id'],'同capture log baseline')
 ml=[]
 for name,ref in mutable['files'].items():
  require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name),'旧ログ名固定');q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'実旧ログSHA');pins[q.relative_to(root).as_posix()]=sha(q);ml.append(dict(relative_path='MT8167D3DClear/'+name,source=q.relative_to(root).as_posix(),sha256=sha(q),prefix_bytes=q.stat().st_size))
 p=copy.deepcopy(prior)
 for k in list(p):
  if re.match(r'GPU\d+_|baseline\d+_|previous_combined\d+_',k):p.pop(k)
 p.update(profile=PROFILE,service=SERVICE,data_directory=SERVICE,draft=False,normal_install_allowed=True,backup_manifest=spec['fresh']['path'],backup_manifest_sha256=spec['fresh']['sha256'],recovery_boot_id=fm['recovery_boot_id'],capture_selection=dict(schema=1,profile=PROFILE,capture_number=CAPTURE,manifest_sha256=spec['fresh']['sha256'],recovery_boot_id=fm['recovery_boot_id'],gpu_policy_sha256=n['policy_sha256'],unchanged_since_capture=True),source_files={name:{k:originals['Windows/System32/config/'+name][k] for k in ('bytes','sha256')} for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},packages=packages,baselines=baselines,service_preservation=n['old_services'],captured_registry_values=values,preserved_files=preserved,preserved_sources=sources,baseline_witnesses=witnesses,install_targets=INSTALL,diagnostic_clients=[client,diag],gpu_client_execute=True,gpu_client_count=2,diagnostic_only=False,driver_updates=True,planned_reboots=1,boot_transition=transition,first_install_mask=0,deferred_unbound_mask=0,GPU368_manifest=spec['GPU368_manifest'],GPU368_handoff=spec['GPU368_handoff'],capture_contract=spec['capture_contract'],fresh200_inventory=dict(service_names=c['service_names'],named_value_ids=c['named_value_ids'],service_count=len(c['service_names']),named_value_count=len(values),file_count=len(originals)),baseline200_module_identity_reference_only=n['module_identity'],post_ownership_policy_retained=post,rootfix_claim=False)
 p['mutable_diagnostic_logs'].update(files=ml,baseline=spec['mutable_baseline']);p['service_baseline_generation']['source']=spec['native'];p['observer_contract'].update(fresh_basis=CAPTURE,planned_reboots=1,GPU_client_index=0,globaldiag_client_index=1,diagnostic_clients=2,GPU_timeout_ms=900000,absolute_batch_timeout_seconds=1140)
 p['standard_present_contract'].update(diagnostic_clients=2,execution_index=0,driver_package_source_commit=SOURCE,driver_signed_manifest_sha256=MANIFEST)
 p['display_transition']['DIF_order']=['render'];p['display_transition']['old_DOD_retirement']='historical177; transport357 retained; GPU368 render only'
 p['globaldiag_contract']=dict(Present_existing_readiness_gate_retained=True,Present_skipped_if_GPU_not_ready=True,read_only_diag_runs_even_if_Present_skipped=True,execution_index=1,same_boot_as_present=True,maximum_invocations=1,prefix=diag['arguments'][0],result_files=['-response.bin','-raw.bin','-records.txt'],helper_manifest=spec['globaldiag_manifest'],helper_import_proof=spec['globaldiag_import_proof'],hardware_success_inferred=False)
 p['boot_witness']=spec['boot_witness'];p['boot_witness_evidence']=boot_evidence;p['physical_read_this_capture']=boot_evidence['physical_read_this_capture']
 if CAPTURE!=200:
  p[f'fresh{CAPTURE}_inventory']=p.pop('fresh200_inventory')
  p['service_baseline_generation'].pop('all18_next_generation_full_hashes_strict',None)
  p['service_baseline_generation']['all_retained_full_hashes_strict']=True
  p['service_baseline_generation']['retained_service_count']=len(n['old_services'])
  p[f'baseline{CAPTURE}_module_identity_reference_only']=p.pop('baseline200_module_identity_reference_only')
 if TIMING:TIMING.finish(p,spec,n,pin,pin_file)
 p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())];return p

def generate(p):
 require([x['name'] for x in p['diagnostic_clients']]==['gpu_present','gpu_globaldiag'] and all(x['phase']==1 for x in p['diagnostic_clients']),'同boot各1回')
 q=copy.deepcopy(p);q['gpu_client_count']=1
 # Keep the runtime command-line quoting; escape it in this profile's C emitter.
 old_cstr=base.cstr
 def cstr200(value):
  return 'L"'+value.replace('\\','\\\\').replace('"','\\"').replace('\r','\\r').replace('\n','\\n')+'"'
 try:
  base.cstr=cstr200
  header=base.generate(q)
 finally:
  base.cstr=old_cstr
 return ('#define A04_GPU368_DUAL_DIAGNOSTICS 1\n'+header).replace('/* run185:DIS0 System transport then GPU0 unified356; matchedboot required; BT retained. */','/* GPU368 render DIF1024/restart1; same-boot Present363 then globaldiag198 once. */')
def load_map(path,root):return build_policy(root,read(path))
def assert_paths(image,capture=False):
 b=Path(image).read_bytes()
 for n in (SERVICE,SERVICE+'.exe','mt8167-dxgi-console-launch363.exe','dxg-global-diag198.exe'):
  require(n.encode('utf-16le') in b,'固定service/client identity欠落 '+n)
 require('bt2013-install.bin'.encode('utf-16le') not in b,'BT再導入禁止')
def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args();require(x.finalize,'実fresh200固定後だけfinalize');root=x.workspace.resolve(strict=True);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'新規root内output');p=load_map(rooted(root,x.map),root);out.mkdir(parents=True)
 for n,s in [('peripheral-service-path-policy.h',paths()),('peripheral-batch-policy.h',generate(p))]: (out/n).write_text(s)
 for n in ['peripheral-policy.json','peripheral-preservation-plan.json']:(out/n).write_text(json.dumps(p,indent=2)+'\n')
 print(f'GPU368_FRESH{CAPTURE}_FINALIZED_NOT_DEPLOYED')
if __name__=='__main__':main()
