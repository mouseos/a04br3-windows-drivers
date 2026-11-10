#!/usr/bin/env python3
"""Run187: fresh186 plus one signed GPU update; retain actual POST/parent/thermal."""
from pathlib import Path
import argparse,copy,hashlib,importlib.util,json,re,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'winpe'))
from peripheral_batch_policy import rooted,sha,read,require
from codec_msdc_transaction import version,datevalue
_s=importlib.util.spec_from_file_location('_combined186_base',Path(__file__).with_name('prepare-peripheral-followup184.py'))
_b=importlib.util.module_from_spec(_s);_s.loader.exec_module(_b)
PROFILE='combined-features-fresh186-v1';SERVICE=DIRECTORY='MT8167CombinedFollowup186';INSTALL=['render'];ROLES=_b.ROLES;TARGETS=_b.TARGETS
_b.PROFILE=PROFILE;_b.SERVICE=_b.DIRECTORY=SERVICE;_b.INSTALL=INSTALL
FRESH_SHA='80a9002888ff0c7cc5bc4051eb5a51482d429275ffcab4b68ec8dca056835baa'
BOOT_SHA='0943ab8a6008942d9701140542da6ecaa902f750bf34b864bb6a66808c08df42'
BUILD_ID='GPU359-start-gap-v1';SOURCE_BASE='e0727eb0d840883a31d3c501b0ee17652ebdcad4'
def paths():return _b.paths()
def build_policy(root,spec):
 pins={}
 def pin(ref):
  q=rooted(root,ref['path']);require(sha(q)==ref['sha256'],'Input hash '+ref['path']);pins[ref['path']]=ref['sha256'];return read(q)
 require(spec['profile']==PROFILE and spec['install_targets']==INSTALL and spec['hardware_owner']=='main','Exact run187 scope')
 fm=pin(spec['fresh']);require(spec['fresh']['sha256']==FRESH_SHA and fm['recovery_boot_id']=='490fb4f5-e27b-4979-864e-009fbf4e8b88' and fm['status']=='captured-unmounted' and fm['read_only'] and fm['unmounted'] and not fm['errors'] and len(fm['files'])==42,'Exact immutable fresh186')
 originals={f['source_path']:f for f in fm['files']}
 for f in fm['files']:
  q=rooted(root,Path(spec['fresh']['path']).parent/f['backup_path']);require(q.stat().st_size==f['bytes'] and sha(q)==f['sha256'],'Fresh original '+f['source_path']);pins[q.relative_to(root).as_posix()]=f['sha256']
 n=pin(spec['native']);prior=pin(spec['prior185_policy'])
 require(n['source_manifest_sha256']==FRESH_SHA and not n['errors'] and n['originals_unchanged'] and n['full_service_preservation_equal'] and all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Clean native186 receipt')
 for name,h in n['source_files'].items():require(originals['Windows/System32/config/'+name]['sha256']==h,'Exact SYSTEM trio')
 require(n['module_identity']['observed_buildid']==BUILD_ID and n['module_identity']['observed_sourcebase']==SOURCE_BASE and n['module_identity']['expected_identity_matches'],'Actual baseline GPU359 identity')
 require(prior['profile']=='combined-features-fresh184-v1' and n['old_services']==prior['service_preservation'] and len(n['old_services'])==18,'Keep18 full service fingerprints')
 post=n['post_ownership_policy360'];require(post['controlset']=='ControlSet001' and post['name']=='DisableAutoAcpiPostDeivce' and post['raw']=='00000000','Retain actual standard POST policy0')
 values=copy.deepcopy(prior['captured_registry_values']);require(len(values)==95,'Prior95 named values')
 for v in values:
  got=n['captured_named_values'][v['key']+'\\'+v['name']];require(got['type']==v['type'] and got['raw_hex']==v['raw_hex'] and got['bytes']==len(bytes.fromhex(v['raw_hex'])),'Prior named value changed')
 previous=n['new_service'];require(previous['present'] and previous['values']['Start']['value']==4 and previous['values']['ImagePath']['value']=='"C:\\Windows\\System32\\MT8167CombinedFollowup184.exe"','Previous service disabled')
 values+=_b.value_contract('SYSTEM\\CurrentControlSet\\Services\\MT8167CombinedFollowup184',previous['values']);require(len(values)==100,'Exact100 named values')
 gm=pin(spec['GPU362_manifest']);handoff=pin(spec['GPU362_handoff']);require(gm['schema']=='GPU-unified362-source-overlay-v1' and gm['inf_driverver']=='11/10/2026,0.0.3.62' and gm['runtime_build_id']==BUILD_ID and gm['required_palette_rejection'] is True and gm['hardware_verified'] is False and handoff['hardware_verified'] is False,'Signed GPU362 candidate only')
 gp=Path(spec['GPU362_manifest']['path']).parent
 for f in gm['outputs']:
  q=rooted(root,gp/f['name']);require(sha(q)==f['sha256'],'New GPU output '+f['name']);pins[q.relative_to(root).as_posix()]=f['sha256']
 pkgs=copy.deepcopy(prior['packages']);e=pkgs['render'];old_extra={x['name']:x['sha256'] for x in e['extra_members']};e.update(directory=gp.as_posix(),manifest=spec['GPU362_manifest']['path'],manifest_sha256=spec['GPU362_manifest']['sha256'],source_commit=gm['package_source_commit'],driver_version='0.0.3.62',driver_date='11/10/2026')
 hashes={f['name']:f['sha256'] for f in gm['outputs']}
 for x in e['extra_members']:x.update(sha256=hashes[x['name']],baseline_sha256=old_extra[x['name']])
 packages={r:_b.package(root,pkgs[r],r) for r in ROLES};baselines={}
 for r in ROLES:
  row=n['nodes'][TARGETS[r][0]];require(row['present'] and len(row['instances'])==1,'One baseline node '+r);v=row['instances'][0];sel=n['bt_start']['selection'] if r=='bt' else v['selected'];require(v['values']['Service']['value'].lower()==TARGETS[r][1].lower(),'Exact service '+r)
  b={'mode':'selected' if r in INSTALL else 'retained','version_u64':version(sel['DriverVersion']['value']),'date_filetime':datevalue(sel['DriverDate']['value'].replace('-','/')),'sys_sha256':originals['Windows/System32/Drivers/'+TARGETS[r][3]]['sha256'],'inf':sel['InfPath']['value']}
  require(b['version_u64']==prior['packages'][r]['version_u64'] and b['sys_sha256']==prior['packages'][r]['hashes']['sys'],'Actual baseline tuple '+r);baselines[r]=b
 preserved={};sources={};witnesses={}
 for name,f in originals.items():
  if (name.startswith('Windows/System32/Drivers/') and Path(name).name!='ge8300-render.sys') or (name.startswith('Windows/System32/') and name.endswith('.exe')):preserved[name]=f['sha256'];sources[name]=(Path(spec['fresh']['path']).parent/f['backup_path']).as_posix()
 for x in packages['render']['extra_members']:
  name='Windows/System32/'+x['installed'].replace('\\','/');q=rooted(root,Path(spec['diagnostics'])/'fixed'/name);require(sha(q)==x['baseline_sha256'],'Actual baseline UMD/FW');pins[q.relative_to(root).as_posix()]=sha(q)
  if x['sha256']==x['baseline_sha256']:preserved[name]=sha(q);sources[name]=q.relative_to(root).as_posix()
  else:witnesses[name]={'path':q.relative_to(root).as_posix(),'sha256':sha(q)}
 baseline=pin(spec['mutable_baseline']);require(baseline['source_transaction']=='run185','Latest sameboot client logs');mutable=[]
 for name,h in baseline['files'].items():
  require(re.fullmatch(r'(clear|umd)-[A-Za-z0-9_.-]+\.log',name),'Fixed log name');q=rooted(root,Path(spec['diagnostics'])/name);require(sha(q)==h,'Baseline log hash');pins[q.relative_to(root).as_posix()]=h;mutable.append({'relative_path':'MT8167D3DClear/'+name,'source':q.relative_to(root).as_posix(),'sha256':h,'prefix_bytes':q.stat().st_size})
 # Retain, rather than repeat, the actual signed standard-Present test assets.
 client=copy.deepcopy(prior['diagnostic_clients'][1]);require(client['name']=='gpu_present' and client['phase']==1 and client['exe']=='mt8167-dxgi-console-launch356.exe' and client['sha256']=='1bb21b6157e674ab1cde10eac9aff7d5c7bfd5f63e568ad02fe3049838396497' and client['dependencies']==[{'exe':'mt8167-dxgi-present356.exe','sha256':'eaa30760ebc38a1955ccb3bbd834ee5c3972ce8842b90e6a58bc5084109ef8d5'}],'Exact standard Present client')
 for c in [client]+client['dependencies']:
  q=rooted(root,Path(client['directory'])/c['exe']);require(sha(q)==c['sha256'],'Signed Present asset');pins[q.relative_to(root).as_posix()]=c['sha256'];name='Windows/System32/'+c['exe'];q=rooted(root,Path(spec['diagnostics'])/'fixed'/name);require(sha(q)==c['sha256'],'Deployed Present asset');preserved[name]=sha(q);sources[name]=q.relative_to(root).as_posix();pins[q.relative_to(root).as_posix()]=sha(q)
 p=copy.deepcopy(prior)
 for k in list(p):
  if re.match(r'GPU[0-9]+_(?:handoff|manifest|pins|review|provenance|main_seal|boundary_contract)',k) or re.match(r'baseline[0-9]+_',k) or re.match(r'previous_combined[0-9]+_service',k) or k.startswith('clock_') or k=='post_ownership_policy360':p.pop(k)
 p.update(profile=PROFILE,service=SERVICE,data_directory=SERVICE,backup_manifest=spec['fresh']['path'],backup_manifest_sha256=FRESH_SHA,recovery_boot_id=fm['recovery_boot_id'],capture_selection={'schema':1,'profile':PROFILE,'capture_number':186,'manifest_sha256':FRESH_SHA,'recovery_boot_id':fm['recovery_boot_id'],'gpu_policy_sha256':n['policy_sha256'],'unchanged_since_capture':True},source_files={name:{k:originals['Windows/System32/config/'+name][k] for k in ('bytes','sha256')} for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},packages=packages,baselines=baselines,service_preservation=n['old_services'],captured_registry_values=values,previous_combined184_service=previous,preserved_files=preserved,preserved_sources=sources,baseline_witnesses=witnesses,install_targets=INSTALL,diagnostic_clients=[client],baseline186_module_identity_reference_only=n['module_identity'],baseline186_init_reference_only=n['gpu_init'],GPU362_manifest=spec['GPU362_manifest'],GPU362_handoff=spec['GPU362_handoff'],rootfix_claim=False,post_ownership_policy_retained={'controlset':'ControlSet001','name':'DisableAutoAcpiPostDeivce','raw':'00000000','mutated':False})
 p['gpu_client_log_baseline']=spec['mutable_baseline'];p['mutable_diagnostic_logs'].update(files=mutable,baseline=spec['mutable_baseline']);p['service_baseline_generation']['source']=spec['native']
 for key in ['clock_client_index','clock_timeout_ms']:p['observer_contract'].pop(key,None)
 p['observer_contract'].update(fresh_basis=186,GPU_client_index=0,diagnostic_clients=1,GPU_start_event='phase1 PB target0/event0x220 launcher PID',GPU_timeout_ms=900000,absolute_batch_timeout_seconds=1080)
 p['standard_present_contract'].update(diagnostic_clients=1,execution_index=0,driver_package_source_commit=gm['package_source_commit'],driver_signed_manifest_sha256=spec['GPU362_manifest']['sha256'])
 p['present_collect']['join']=p['present_collect']['join'].replace('SCM client1','SCM client0');p['display_transition']['DIF_order']=['render'];p['display_transition']['old_DOD_retirement']='historical177; retain DIS0 transport; GPU362 only'
 require(p['boot_transition']['boot_sha256']==BOOT_SHA and not p['boot_transition']['boot_write_required'] and not p['boot_transition']['gpt_written'],'Keep matched firmware, no GPT write')
 require(p['packages']['parent']['driver_version']=='2026.11.10.1120' and p['packages']['thermal']['driver_version']=='0.0.1.11','Keep actual clock1120/thermal111')
 for r in ROLES:
  e=p['packages'][r]
  for name in list(e['names'].values())+[x['name'] for x in e.get('extra_members',[])]:
   q=rooted(root,Path(e['directory'])/name);pins[q.relative_to(root).as_posix()]=sha(q)
  q=rooted(root,e['manifest']);pins[q.relative_to(root).as_posix()]=sha(q)
 p['input_pins']=[{'path':k,'sha256':v} for k,v in sorted(pins.items())];return p

def generate(p):
 require(len(p['diagnostic_clients'])==1 and p['diagnostic_clients'][0]['name']=='gpu_present','One GPU client, no clock repeat')
 text=_b.generate(p);require(text.count('#define A04_DIAGNOSTIC_CLIENT_COUNT 2u')==1,'Known base declaration');return text.replace('#define A04_DIAGNOSTIC_CLIENT_COUNT 2u','#define A04_DIAGNOSTIC_CLIENT_COUNT 1u').replace('/* run185:DIS0 System transport then GPU0 unified356; matchedboot required; BT retained. */','/* run187: GPU362 only; actual POST0, parent1120, thermal111 and matched FW retained. */')
def load_map(path,root):return build_policy(root,read(path))
def assert_paths(image,capture=False):return _b.assert_paths(image,capture)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--map',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--finalize',action='store_true');a=ap.parse_args();require(a.finalize,'Explicit finalization only');root=a.workspace.resolve(strict=True);p=load_map(rooted(root,a.map),root);out=a.out.resolve();require(out.is_relative_to(root) and not out.exists(),'New root-local output');out.mkdir(parents=True)
 (out/'peripheral-service-path-policy.h').write_text(paths());(out/'peripheral-batch-policy.h').write_text(generate(p))
 for n in ['peripheral-policy.json','peripheral-preservation-plan.json']:(out/n).write_text(json.dumps(p,indent=2)+'\n')
 print('COMBINED187_GPU362_FRESH186_FINALIZED_NO_HARDWARE_OPERATIONS')
if __name__=='__main__':main()
