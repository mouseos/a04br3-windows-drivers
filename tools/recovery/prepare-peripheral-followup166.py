#!/usr/bin/env python3
"""fresh166原本専用run167/GPU349/BT2017/provider1026 policy。既存媒体と実機へ操作しない。"""
import argparse, copy, hashlib, json, re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'winpe'))
from peripheral_batch_policy import rooted, sha, read, require, cstr, cbytes, inftext
from codec_msdc_transaction import version, datevalue
import importlib.util
_spec=importlib.util.spec_from_file_location("gpu342_artifacts",Path(__file__).with_name("prepare-combined153-draft.py"))
_gpu=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_gpu)
PROFILE='combined-features-fresh166-v1'
SERVICE=DIRECTORY='MT8167CombinedFollowup166'
ROLES=('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render','msdc','bt')
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
 'bt':('ROOT\\MT6630COMBO','Mt6630ComboBluetooth','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt6630-combo-bluetooth.sys'),
 'msdc':('ACPI\\MOU0001','a04msdcboot','{4D36E97B-E325-11CE-BFC1-08002BE10318}','mt8167-msdc.sys')}
INSTALL=['wifi','bt','render']

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
 require(spec['schema']==1 and spec['profile']==PROFILE and spec['hardware_owner']=='main' and spec['install_targets']==INSTALL and spec['gpu_client_execute'] is True and spec['gpu_client_count']==1,'Exact167 GPU349/BT2017/provider1026 scope required')
 fm=pin(spec['fresh']);require(spec['fresh']['sha256']=='e53d4c13df2cbba7a744c49ba345f9d6acbaa33146f1d97a4101b6d92e009a3e' and fm['status']=='captured-unmounted' and fm['unmounted'] and fm['read_only'] and not fm['errors'] and fm['recovery_boot_id']=='910c5a11-562e-4fe3-a1ee-e3ea31e2eef7' and len(fm['files'])==41,'Exact fresh166 required')
 originals={f['source_path']:f for f in fm['files']}
 for f in fm['files']:
  q=rooted(root,Path(spec['fresh']['path']).parent/f['backup_path']);require(q.stat().st_size==f['bytes'] and sha(q)==f['sha256'],'Original166 mismatch');pins[q.relative_to(root).as_posix()]=f['sha256']
 native=pin(spec['native']);init=native;btactual=native;require(not btactual['errors'] and btactual['originals_unchanged'] and all(btactual[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Exact native164 BT selection required')
 for n in (native,init):
  require(n['source_manifest_sha256']==spec['fresh']['sha256'] and not n['errors'] and n['originals_unchanged'] and n['full_service_preservation_equal'] and n['rejected149_service_absent'] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Clean166 receipt required')
  for name,h in n['source_files'].items():require(originals['Windows/System32/config/'+name]['sha256']==h,'Native trio mismatch')
 require(init['gpu_init']['reached_mask']==0x7ff and all(init['gpu_init']['values'][x]['value']==0 for x in ('DriverEntry','Contract','DxgkInitialize','AddDevice','DeviceInformation','NativeOwnerCreate','ConfigureAperture','StartOwner','DrawOpsV1','DrawOpsV2','StartDevice')),'Actual348 init boundary required')
 require(native['module_identity']['observed_buildid']=='GPU348-submit-worker-v1' and native['module_identity']['expected_identity_matches'],'Same166 identity required')
 prior=pin(spec['prior165_policy']);require(prior['profile']=='combined-features-fresh164-v1' and prior['gpu_client_execute'],'Frozen165 source reference required')
 require(native['old_services']==prior['service_preservation'] and len(native['old_services'])==18,'18 actual fingerprints required')
 values=copy.deepcopy(prior['captured_registry_values'])
 for v in values:
  got=native['captured_named_values'][v['key']+'\\'+v['name']];require(got['type']==v['type'] and got['bytes']==len(bytes.fromhex(v['raw_hex'])) and got['raw_hex']==v['raw_hex'],'Actual stopped value mismatch')
 previous=native['new_service'];require(previous['present'] and previous['values']['Start']['value']==4 and previous['values']['ImagePath']['value']=='"C:\\Windows\\System32\\MT8167CombinedFollowup164.exe"','Current164 must be stopped')
 values+=value_contract('SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup164',previous['values']);require(len(values)==50,'18 full plus50 named values required')
 require(all(spec.get(k) is not None for k in ('GPU349_main_seal','GPU349_manifest','GPU349_pins')),'GPU347 formal artifact/seal pending; no fallback/no arm')
 gs=pin(spec['GPU349_main_seal']);gm=pin(spec['GPU349_manifest'])
 require(gs['manifest_sha256']==spec['GPU349_manifest']['sha256'] and gs['source_commit']==gm['package_source_commit']=='c7e0c7ceb4950ad68a3242f7162244d6edec752f' and gm['kernel_binary_source_commit']==gs['original_compile_source_commit']=='26c3ec0277c1c2885c561cba2771f695d3f71732' and gs['compiled_TUs_verified']==4 and len(gs['changed_archive_members'])==4 and gs['unchanged_linked_objects_verified']==76 and gs['BCBF_valid_signatures']==6 and not gs['hardware_verified'] and not gs['root_reset_fix_proven'],'Exact formal348 main seal/source required')
 require(gs['imports_added']==[] and gs['imports_removed']==[],'348 imports must retain actual347 contract')
 gp=Path(spec['GPU349_manifest']['path']).parent;gpu_files=[]
 for f in gm['outputs']+gm['retained_provenance']:
  q=rooted(root,gp/f['name']);require(sha(q)==f['sha256'],'348 member mismatch');pins[q.relative_to(root).as_posix()]=f['sha256'];gpu_files.append({'name':f['name'],'sha256':f['sha256']})
 for f in pin(spec['GPU349_pins']):
  q=rooted(root,f['path']);require(sha(q)==f['sha256'] and q.stat().st_size==f['bytes'],'348 provenance pin mismatch');pins[f['path']]=f['sha256']
 ts=pin(spec['thermal_main_seal']);require(ts['parent_source_commit']=='b2c1a3136a5b8eadf1900f71a8d330a6b0a31ac8' and ts['firmware_source_commit']=='b4ce1992af66adcefa02e34357d5dbf4313eaca2' and ts['parent_SYS_sha256']=='bbced952c499ba0588c77b861b20141043929773d703cdd327ffa92e61990979' and ts['boot_sha256']=='2e6cba6ab96cd5b586cefbd4bd004c6cee0484866ca6710ea454144da568c451' and not ts['mixed_old_new_allowed'] and not ts['EDK_common_changed'],'Matched1117 seal required')
 require(spec['thermal_pins']['sha256']==ts['pins_sha256'],'Thermal pins seal mismatch')
 for f in pin(spec['thermal_pins']):
  q=rooted(root,f['path']);require(sha(q)==f['sha256'],'Thermal member mismatch');pins[f['path']]=f['sha256']
 pkgs=copy.deepcopy(prior['packages']);pkgs['render'].update(directory=gp.as_posix(),manifest=spec['GPU349_manifest']['path'],manifest_sha256=spec['GPU349_manifest']['sha256'],source_commit=gm['package_source_commit'],binary_source_commit=gs['source_commit'],driver_version='0.0.3.49',driver_date=gm['inf_driverver'].split(',')[0],main_seal=spec['GPU349_main_seal'])
 tp=Path(spec['thermal_main_seal']['path']).parent/'package';tm=tp/'signed-manifest.json';parent=read(rooted(root,tm))
 pkgs['parent'].update(directory=(tp/'parent').as_posix(),manifest=tm.as_posix(),manifest_sha256=sha(rooted(root,tm)),source_commit=parent['source_commit'],driver_version=parent['parent_version'],names={'inf':'shared-parent-usb1117.inf','sys':'mt8167-shared.sys','cat':'mt8167-shared-usb1117.cat'},main_seal=spec['thermal_main_seal'])
 match=re.search(r'^\s*DriverVer\s*=\s*([^,\r\n]+),',inftext(rooted(root,tp/'parent/shared-parent-usb1117.inf')),re.M|re.I);require(match,'Parent DriverVer required');pkgs['parent']['driver_date']=match[1].strip()
 bt=pin(spec['BT2017_batch']);bp=pin(spec['BT2017_pins'])
 require(bt['source_commit']==bp['source_commit']=='99ef927d42b8b7be4319c73dde73c1df23e9c6a4' and bt['versions']=={'client':2017,'provider':1026} and bt['requires_both'] and bt['new_root_registration']==0,'Exact paired2017/1026 required')
 for f in bp['files']:
  q=rooted(root,f['path']);require(sha(q)==f['sha256'] and q.stat().st_size==f['bytes'],'BT2016 provenance mismatch');pins[f['path']]=f['sha256']
 for role,sub in [('wifi','provider'),('bt','client')]:
  directory=Path(bt[sub]);manifest=directory/'signed-manifest.json';bm=read(rooted(root,manifest))
  require(bm['source_commit']==bt['source_commit'] and bm['signature_status']=='Valid' and bm['signer_thumbprint']=='BCBF6C456C6F0DD0818434763B189488FBF13570','BT2016 signed source required')
  pkgs[role].update(directory=directory.as_posix(),manifest=manifest.as_posix(),manifest_sha256=sha(rooted(root,manifest)),source_commit=bm['source_commit'],driver_version=bm['version'],driver_date='11/08/2026',extra_members=[])
 packages={r:package(root,pkgs[r],r) for r in ROLES};baselines={}
 for r in ROLES:
  row=native['nodes'][TARGETS[r][0]];require(row['present'] and len(row['instances'])==1,'One actual selected node required');v=row['instances'][0];require(v['values']['Service']['value'].lower()==TARGETS[r][1].lower(),'Actual service required')
  if r=='bt':sel=btactual['bt_start']['selection']
  else:require(v['selected_present'],'Actual selected metadata required');sel=v['selected']
  b={'mode':'selected' if r in INSTALL else 'retained','version_u64':version(sel['DriverVersion']['value']),'date_filetime':datevalue(sel['DriverDate']['value'].replace('-','/')),'sys_sha256':originals['Windows/System32/Drivers/'+TARGETS[r][3]]['sha256'],'inf':sel['InfPath']['value']}
  if r=='render':require(b['version_u64']==version('0.0.3.48') and b['sys_sha256']=='e267b43f4bd5c3b161b7212385f2b277102aec8e32de3c719488cd3f839d3aa4','Actual348 baseline required')
  elif r in ('wifi','bt'):
   require(b['version_u64']==prior['packages'][r]['version_u64'] and b['sys_sha256']==prior['packages'][r]['hashes']['sys'],'Actual previous2015/1024 required')
  else:require(b['version_u64']==packages[r]['version_u64'] and b['sys_sha256']==packages[r]['hashes']['sys'],'Retained role mismatch: '+r)
  baselines[r]=b
 require(packages['thermal']['driver_version']==packages['msdc']['driver_version']=='0.0.1.10','Retain leaf/MSDC1.10')
 baseline=pin(spec['mutable_baseline']);require(baseline['source_transaction']=='run165' and len(baseline['files'])==89,'Exact166 archived log baseline required')
 mutable=[]
 for n,h in baseline['files'].items():
  require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',n),'Fixed log name required');q=rooted(root,Path('port/xnu/out/private-windows-rt-combined-run165-diagnostics')/n);require(sha(q)==h,'Log archive mismatch');pins[q.relative_to(root).as_posix()]=h;mutable.append({'relative_path':'MT8167D3DClear/'+n,'source':q.relative_to(root).as_posix(),'sha256':h,'prefix_bytes':q.stat().st_size})
 preserved={};sources={}
 for n,f in originals.items():
  if (n.startswith('Windows/System32/Drivers/') and Path(n).name not in ('ge8300-render.sys','mt6630-combo-runtime.sys','mt6630-combo-bluetooth.sys')) or (n.startswith('Windows/System32/') and n.endswith('.exe')):
   preserved[n]=f['sha256'];sources[n]=(Path(spec['fresh']['path']).parent/f['backup_path']).as_posix()
 for extra in packages['render']['extra_members']:
  n='Windows/System32/'+extra['installed'].replace('\\','/');require(extra['sha256']==extra['baseline_sha256'],'Retain UMD/FW');q=rooted(root,Path('port/xnu/out/private-windows-rt-combined-run165-diagnostics/fixed')/n);require(sha(q)==extra['sha256'] and sha(rooted(root,gp/extra['name']))==extra['sha256'],'Actual UMD/FW mismatch');preserved[n]=extra['sha256'];sources[n]=q.relative_to(root).as_posix()
 boot={'mode':'retain-current-matched-parent1117-ATCV1-ATCO-boot2e6','boot_path':str(tp.parent/'firmware/boot-final/boot.img').replace('/',chr(92)),'boot_sha256':ts['boot_sha256'],'baseline_boot_sha256':prior['boot_transition']['boot_sha256'],'main_handoff_required':True,'boot_write_required':False,'gpt_written':False,'mixed_old_new_allowed':False}
 require(sha(rooted(root,boot['boot_path'].replace(chr(92),'/')))==boot['boot_sha256'],'Matched boot mismatch')
 client=_gpu.fixed_client(root);client.update(name='gpu_suite',relative_directory=SERVICE+'\\clients',phase=1,dependencies=[]);pin(client['main_seal'])
 bc=pin(spec['BT_status_client']);require(bc['arguments']==['--status'] and bc['timeout_ms']==45000 and bc['software_wait_ms']==35000 and bc['source_TUs']==1 and bc['registration_execute'] is False,'Exact one-invocation software wait35 client required')
 bclient={'name':'bt_status','directory':bc['directory'],'exe':bc['exe'],'arguments':['--status'],'sha256':bc['exe_sha256'],'manifest':spec['BT_status_client']['path'],'manifest_sha256':spec['BT_status_client']['sha256'],'relative_directory':SERVICE+'\\clients','phase':0,'one_shot':True,'timeout_ms':45000,'dependencies':[]}
 require(sha(rooted(root,Path(bclient['directory'])/bclient['exe']))==bclient['sha256'],'BT client byte pin required')
 require(native['nodes']['ROOT\\MT6630COMBO']['present'] and len(native['nodes']['ROOT\\MT6630COMBO']['instances'])==1,'Existing BT root required')
 p=copy.deepcopy(prior);p.update(profile=PROFILE,service=SERVICE,data_directory=SERVICE,draft=False,normal_install_allowed=True,hardware_owner='main',hardware_operations_allowed=False,backup_manifest=spec['fresh']['path'],backup_manifest_sha256=spec['fresh']['sha256'],recovery_boot_id=fm['recovery_boot_id'],capture_selection={'schema':1,'profile':PROFILE,'capture_number':166,'manifest_sha256':spec['fresh']['sha256'],'recovery_boot_id':fm['recovery_boot_id'],'gpu_policy_sha256':native['policy_sha256'],'unchanged_since_capture':True},source_files={n:{k:originals['Windows/System32/config/'+n][k] for k in ('bytes','sha256')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},packages=packages,baselines=baselines,service_preservation=native['old_services'],captured_registry_values=values,previous_combined164_service=previous,preserved_files=preserved,preserved_sources=sources,planned_reboots=1,install_targets=INSTALL,stage_order=list(ROLES),diagnostic_clients=[bclient,client],boot_transition=boot,thermal_tuple_main_seal=spec['thermal_main_seal'],GPU349_main_seal=spec['GPU349_main_seal'],GPU349_provenance={'directory':gp.as_posix(),'files':gpu_files,'main_seal':spec['GPU349_main_seal'],'manifest':spec['GPU349_manifest'],'compiled_TUs':4,'runtime_source_commit':gs['source_commit'],'recipe_source_commit':gs['source_commit'],'runtime_buildid':gs['runtime_build_id']},gpu_client_log_baseline=spec['mutable_baseline'],mutable_diagnostic_logs={'kind':'observation-not-install-admission','preserve_in_place':True,'overwrite_or_delete':False,'comparison':'saved exact prefix/append/truncation/replacement/missing observation only','files':mutable,'baseline':spec['mutable_baseline']},normal_function_expectations={'temperature_execute':False,'thermal_status_execute':False,'audio_consumer_execute':False,'WLAN_readonly_execute':False,'BT_readonly_execute':True,'GPU_execute':True,'GPU349_real_success':False},observer_contract={**prior['observer_contract'],'fresh_basis':166,'GPU_client_index':1,'GPU_start_event':'PB target1/event0x220','temperature_count':0,'thermal_status_count':0},baseline166_module_identity_reference_only=native['module_identity'],baseline166_init_reference_only=init['gpu_init'])
 for key in ('GPU348_main_seal','GPU348_provenance','baseline164_module_identity_reference_only','baseline164_init_reference_only','previous_combined162_service','GPU347_main_seal','GPU347_provenance','baseline162_module_identity_reference_only','baseline162_init_reference_only','previous_combined160_service','GPU342_main_seal','GPU342_provenance','GPU343_main_seal','GPU343_provenance','previous_combined150_service','previous_combined152_service','previous_combined154_service','previous_combined156_service','previous_combined158_service','GPU346_main_seal','GPU346_provenance','baseline160_module_identity_reference_only','baseline160_init_reference_only','GPU345_main_seal','GPU345_provenance','baseline158_module_identity_reference_only','baseline158_init_reference_only','GPU344_main_seal','GPU344_provenance','baseline156_module_identity_reference_only','baseline156_init_reference_only'):p.pop(key,None)
 p['module_identity_collection'].update(status='required-GPU349-submit-irq-identity',expected_buildid=gm['runtime_build_id'],expected_sourcebase='eb2bc820c3b67168b16f50dbe3fbf27f513ddfae',same_boot_correlation_required=True,BuildID_alone_distinguishes_relink=False)
 p['BT2017_batch']=spec['BT2017_batch'];p['bt_existing_update_execute']=True;p['bt_readonly_execute']=True;p['bt_first_install_execute']=False
 p['parent_contract'].update(ACPI_consume_version=1,ACPI_consume_method='ATCO',matched_parent_version='2026.11.8.1117')
 p['factory_progress_collection']={'key':'ControlSet001/Services/GE8300Render/Parameters/FactoryCheckpoint','entry_records':['R06','R07','R08','R09','R15'],'entry_status0_means_arrival_only':True,'fixed_records':['R46','R47','R06','R07','R08','R09','R15','R20','R21','R22','R34','R35','R36','R37','R38','R39'],'format_version':2,'record_bytes':40,'same_capture_required':True,'old_unmatched_FILETIME_records_are_not_current_proof':True,'APC_guard_is_preventive_not_proven_reset_cause':True,'guard_skip_can_leave_old_record':True,'no_GPU_payload_rootfix_claim':True}
 p['factory_progress_collection']['R46']={'fields':['stage0','before','after','mode'],'zero_means':'mask readback only; not FW sync or storm root proof'}
 p['factory_progress_collection']['R47']={'fields':['stage1','before','after','execute_result'],'zero_means':'mask readback only; not FW sync or storm root proof'}
 p['factory_progress_collection']['R37_helper_return_not_proven_by_persistence']=True
 p['BT_runtime_predicate_unresolved']=True
 p['BT_admission_observation']={'execute':True,'single_client_invocation':True,'software_wait_ms':35000,'client_timeout_ms':45000,'poll_ms':250,'max_samples':141,'terminal_CM_problem_or_success_ends_early':True,'HCI_OPEN_registration_RF_reexecute':False,'D0Return0_is_HCI_success':False}
 p['factory_progress_collection']['fixed_records']+=['R48','R49','R50','R51']
 p['factory_progress_collection']['R48']={'fields':['kind','slot','generation','fence'],'status0_means':'CPUqueue claim arrival only'}
 p['factory_progress_collection']['R49']={'fields':['kind','slot','generation','fence'],'status0_means':'real prepare/Submit returned; not GPU completion'}
 p['factory_progress_collection']['R50']={'fields':['kind','slot','generation','attempt'],'meaning':'exact native claimed IRQ continuation only; not completion'}
 p['factory_progress_collection']['R51']={'fields':['kind','slot','generation','attempt'],'meaning':'direct service result only; not GPU completion'}
 p['input_pins']=[{'path':n,'sha256':h} for n,h in sorted(pins.items())]
 return p

def generate(p):
 require(p['profile']==PROFILE and p['stage_order']==list(ROLES) and p['install_targets']==INSTALL and p['gpu_client_execute'] is True and p['gpu_client_count']==1 and p['bt_first_install_execute'] is False,'Final167 contract only')
 lines=['/* run167:GPU349 paired MT6630; retained1117/boot2e6; BT0 GPU1. */','#define A04_FEATURE160 1','#define A04_FEATURE148 1','#define A04_FEATURE150 1','#define A04_FEATURE152 1','#define A04_GPU_PHASE1_ENABLED 1','#define A04_RUN_WLAN_READ 0','#define A04_RUN_BT_READ 0','#define A04_FEATURE144 1','#define A04_RENDER_TARGET_INDEX '+str(ROLES.index('render'))+'u','#define A04_PERIPHERAL_INSTALL_MASK '+hex(sum(1<<ROLES.index(r) for r in INSTALL))+'u','#define A04_DIAGNOSTIC_ONLY 0','#define A04_READONLY_PERIPHERAL_STATUS 1','#define A04_RUN_THERMAL_READ 0','#define A04_DIAGNOSTIC_CLIENT_COUNT 2u','#define A04_CANDIDATE_EXTRA_COUNT 2u','#define A04_AUX_HCD1014_IMPORT 0','#define A04_USB_PC_ROLE_OPTIONAL_LEGACY_NODE 1','#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0u','#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0u','static const char deferred_token[]="'+hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";','static const unsigned install_order[13]={3,12,10,0,1,2,4,5,6,7,8,9,11};','static const struct peripheral_target targets[13]={']
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
 require('mt8167-silent-pcm-consumer.exe'.encode('utf-16le') not in b and 'mt8167-thermal-ioctl-status.exe'.encode('utf-16le') not in b,'Unchanged peripheral clients must not be linked')

def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--finalize',action='store_true');x=a.parse_args()
 require(x.finalize,'Explicit finalization required; missing signed leaf must fail, never fallback')
 root=x.workspace.resolve(strict=True);policy=load_map(rooted(root,x.map),root);out=x.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output only');out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(policy))
 for name in ('peripheral-policy.json','peripheral-preservation-plan.json'):(out/name).write_text(json.dumps(policy,indent=2)+'\n')
 print('COMBINED167_GPU349_BT2017_PROVIDER1026_ACTUAL166_VALIDATED')
if __name__=='__main__':main()
