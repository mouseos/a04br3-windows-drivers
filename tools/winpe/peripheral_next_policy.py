#!/usr/bin/env python3
"""main明示capture122以降・7stage/5DIF・現GPU/8range保持の明示profile。"""
import argparse,datetime,hashlib,json,re,subprocess
from pathlib import Path
from peripheral_batch_policy import rooted,sha,read,require,cstr,cbytes,SIGNER,inftext
from codec_msdc_transaction import version,datevalue,RETAINED_FILES
PROFILE='peripheral-usb1010-wifi1020-sd8-dod17-parent1102-next-v1'
SERVICE=DIRECTORY='MT8167PeripheralNext122'
# Every generation is supplied through one explicit main approval receipt.
MIN_CAPTURE=122
ROLES=('rtc','thermal','usb','wifi','sd','display','parent');INSTALL=ROLES[2:]
# hardware id, service, class, sys; each supplied package has immutable manifest pin.
TARGETS={
 'rtc':('MT6392\\RTC','Mt6392Rtc','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt6392-rtc.sys'),
 'thermal':('MT8167\\Thermal','Mt8167Thermal','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-thermal.sys'),
 'usb':('MT8167\\USB20','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-usb20.sys'),
 'wifi':('A04BR3\\MT6630SDIO','Mt6630Runtime','{4D36E972-E325-11CE-BFC1-08002BE10318}','mt6630-runtime.sys'),
 'sd':('ACPI\\MOU0013','Mt8167Sd','{4D36E97B-E325-11CE-BFC1-08002BE10318}','mt8167-sd.sys'),
 'display':('ACPI\\MOU0007','MT8167Display','{4D36E968-E325-11CE-BFC1-08002BE10318}','mt8167-display.sys'),
 'parent':('ACPI\\BEN0001','Mt8167Shared','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-shared.sys')}
PINS={
 'rtc':('c39a4d605803a47cdd3a6f175164721597f193d6b0ef3441c37146033a421a02','3da6c7dd7cc5279fccc45581ce2b20405e08291b42ebf1fbf45e2a7a2e49d7eb'),
 'thermal':('c39a4d605803a47cdd3a6f175164721597f193d6b0ef3441c37146033a421a02','ca211e366704380966ef639c24a1d49b71425b39cd7272ebdeeb03c3b412408b'),
 'usb':('3ee0f2a9785607f13873eca836aee66297ef2a4998c140b86860656f0bc91f7e','4d547ed45597395088c89bd52de2ffd83180322b753c9ab30842966cc50fee31'),
 'wifi':('027a4eee780141dbe6481d6b2dab461b119ce3c09cb47f5f2137d943b24670ac','8137670089c6e69cd3dc1e1e05415269e013573f0af8c640c4d358b22aae8b5c'),
 'sd':('511c390a4309617260d1ca30106d3d721ad8f525510f4a409339deb71f0d7f55','ab152989bb6752a3a80d267a1f1775b8e333c2299c3fea247a692aa675cb65d9'),
 'display':('88264ad474d7cc16d8633b448357a07043f0d6a449382ba57c9741bb4e0cbd41','ad78a231d881811efb7bd53422ac0879cb64eb50046f0c609964474c547c32ca'),
 'parent':('db77ed5fdfb5343d061441e09158bf4f0f3883ba8c075751ef2fa2dba2e05e3f','0b98648791827433a252f2787b97ab326aa90f4e4707dce27e42aa628e5bb502')}
def generate_paths():
 sep=chr(92);base=sep.join(('C:','Windows','System32',DIRECTORY))
 return '\n'.join(['#define A04_PERIPHERAL_SERVICE '+cstr(SERVICE),'#define A04_PERIPHERAL_DIRECTORY '+cstr(DIRECTORY),'#define A04_CAPTURE_DIRECTORY '+cstr(base+sep+'capture'),'#define A04_CAPTURE_LOG_PATH '+cstr(base+sep+'capture'+sep+'capture.log'),'#define A04_CAPTURE_MUTEX '+cstr(SERVICE+'CaptureOnce')])+'\n'
def assert_paths(image,capture=False):
 b=Path(image).read_bytes();sep=chr(92);expected=sep.join(('C:','Windows','System32',DIRECTORY,'capture','capture.log')) if capture else DIRECTORY
 require(expected.encode('utf-16le')+b'\0\0' in b,'Compiled UTF16 runtime path missing')
 require(sep.join(('C:','Windows','System32','MT8167CaptureProbe','capture.log')).encode('utf-16le') not in b,'Old capture output path retained')
 if not capture:require('MT8167PeripheralBatch.exe'.encode('utf-16le') not in b,'Old batch exe path retained')
def load_map(path,root):
 root=root.resolve(strict=True);s=read(rooted(root,path));require(set(s)=={'schema','profile','draft','fresh','native','seven','packages','capture_client','accdet_client','rotation_client','runtime_retained','boot','approved_capture','gpu_seal','gpu_media'},'Exact next input schema required')
 require(s['schema']==1 and s['profile']==PROFILE and s['draft'] is False,'Explicit final main-approved capture required')
 approval=s['approved_capture'];require(set(approval)=={'path','sha256'},'Main approval path/hash required');ap=rooted(root,approval['path']);require(sha(ap)==approval['sha256'],'Main approval receipt hash mismatch');ack=read(ap)
 require(set(ack)=={'schema','profile','capture_number','manifest_sha256','recovery_boot_id','gpu_policy_sha256','unchanged_since_capture'} and ack['schema']==1 and ack['profile']==PROFILE and type(ack['capture_number']) is int and ack['capture_number']>=MIN_CAPTURE and ack['unchanged_since_capture'] is True and all(re.fullmatch('[0-9a-f]{64}',ack[k]) for k in ('manifest_sha256','gpu_policy_sha256')) and re.fullmatch('[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}',ack['recovery_boot_id']),'Explicit main approved capture122+ required')
 FRESH=ack['manifest_sha256'];BOOT=ack['recovery_boot_id'];GPU=ack['gpu_policy_sha256']
 mf=rooted(root,s['fresh']['path']);require(s['fresh']['sha256']==sha(mf)==FRESH and mf.as_posix().endswith('private-windows-rt-prebackup-run'+str(ack['capture_number'])+'/manifest.json'),'Approved capture identity/hash mismatch');m=read(mf)
 require(m['status']=='captured-unmounted' and m['read_only'] is True and m['unmounted'] is True and m['errors']==[] and m['recovery_boot_id']==BOOT,'Fixed Recovery identity/unmounted required')
 files={};sources={}
 for e in m['files']:
  p=rooted(root,mf.parent/e['backup_path']);name=e['source_path'];require(p.is_relative_to(mf.parent) and sha(p)==e['sha256'] and p.stat().st_size==e['bytes'] and name not in files,'Fresh file hash/size/duplicate mismatch');files[name]=e;sources[name]=p.relative_to(root).as_posix()
 def pinned(entry):
  require(set(entry)=={'path','sha256'},'Explicit path/hash only');p=rooted(root,entry['path']);require(sha(p)==entry['sha256'] and re.fullmatch('[0-9a-f]{64}',entry['sha256']),'Explicit input hash mismatch');return p,read(p)
 np,n=pinned(s['native']);sp,seven=pinned(s['seven'])
 require(n['source_manifest_sha256']==seven['source_manifest_sha256']==FRESH and n['recovery_boot_id']==BOOT and n['gpu_policy_sha256']==GPU and n['original_unchanged'] is True and n.get('gpu_service_start')==4,'Same capture/native/GPU policy and disabled GPU service required')
 for receipt in (n,seven):require(receipt['errors']==[] and all(type(receipt[k]) is int and receipt[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'Native cleanup failed')
 for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):
  require(n['source_files'][name]==seven['source_files'][name]['sha256']==files['Windows/System32/config/'+name]['sha256'],'Original trio mismatch')
 require(set(n['service_preservation'])=={'MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch'},'All six old service snapshots required')
 require(set(s['packages'])==set(ROLES),'Fixed seven-stage tuple only');pkgs={};baselines={}
 for role in ROLES:
  e=s['packages'][role];require(set(e)=={'directory','manifest','manifest_sha256','source_commit','names','driver_version','driver_date'},'Fixed package fields required');d=rooted(root,e['directory']);mp=rooted(root,e['manifest']);require(sha(mp)==e['manifest_sha256']==PINS[role][0],'Signed manifest fixed pin mismatch');pm=read(mp)
  rows=pm.get('files',pm.get('outputs',[]));members={x.get('path',x.get('name')):x['sha256'].lower() for x in rows}
  hashes={}
  for kind,name in e['names'].items():
   require(kind in ('inf','sys','cat') and re.fullmatch('[A-Za-z0-9_.-]+',name),'Fixed package leaf only');p=d/name;h=sha(p);require(h in members.values(),'Package member absent from signed manifest');hashes[kind]=h
  require(set(hashes)=={'inf','sys','cat'} and hashes['sys']==PINS[role][1],'Package SYS fixed pin mismatch')
  inf=inftext(d/e['names']['inf']);t=TARGETS[role]
  require('DriverVer='+e['driver_date']+','+e['driver_version'] in inf and t[0].lower() in inf.lower(),'Exact INF date/version/HWID required')
  require(e['source_commit']==pm.get('source_commit'),'Package source provenance mismatch')
  if role=='parent':
   require(pm['baseline_parent_source']=='1d248a71a95360966ff867e4b063504ac764e074' and pm['baseline_parent_signed_sys_sha256']=='398fd007079ab4710404d67a60dc7db46b84dd149ce3ae69e3bbc730264e823c' and pm['flag_delta']=={'A04_SHARED_ACCDET_READ':'1'} and pm['eight_range_source_unchanged'] is True and pm['gpio_write_added'] is False and pm['wifi_or_bt_change_added'] is False,'Parent1102 must retain1101 tuple plus read-only ACCDET only')
  if role=='display':require(all(v==(1 if k=='A04_MT8167_DISPLAY_BRIGHTNESS' else 0) for k,v in pm['effective_flags'].items()),'DOD must preserve all GPU flags off, brightness on')
  pkgs[role]={**e,'hashes':hashes,'version_u64':version(e['driver_version']),'date_filetime':datevalue(e['driver_date'])}
  if role in INSTALL:
   rows=n['nodes'].get(t[0],seven['nodes'].get(t[0]));require(isinstance(rows,list) and len(rows)==1,'One exact existing devnode required');row=rows[0];old=row['selected'];entry=files['Windows/System32/Drivers/'+t[3]]
   require(row['Service'].lower()==t[1].lower() and row['ClassGUID'].lower()==t[2].lower() and old['MatchingDeviceId'].lower()==t[0].lower(),'Current service/class/hardware mismatch')
   require(version(old['DriverVersion'])<version(e['driver_version']),'No same candidate reinstall or downgrade')
   baselines[role]={'version_u64':version(old['DriverVersion']),'date_filetime':datevalue(datetime.datetime.strptime(old['DriverDate'].replace('-','/'),'%m/%d/%Y').strftime('%m/%d/%Y')),'sys_sha256':entry['sha256'],'inf':old['InfPath']}
 gsp,gs=pinned(s['gpu_seal']);gmp,gm=pinned(s['gpu_media'])
 require(gs['media_manifest_sha256']==sha(gmp) and gs['policy_hash']==GPU and gm['policy']['data_directory']==gs['data_directory'] and hashlib.sha256(json.dumps(gm['policy'],sort_keys=True,separators=(',',':')).encode()).hexdigest()==GPU,'Main current GPU seal/policy/media mismatch')
 require(files['Windows/System32/Drivers/ge8300-render.sys']['sha256']==gs['gpu_sys_sha256']==gm['expected_installed_files']['ge8300-render.sys'] and version(seven['nodes']['ACPI\\MOU0006'][0]['selected']['DriverVersion'])==gm['policy']['packages']['render']['version_u64'],'Actual GPU selection/SYS must match current explicit seal')
 require(files['Windows/System32/Drivers/mt8167-audio.sys']['sha256']=='4facb36eed0b45edf32ae234ec3afa325ea1a9bb58884caa7042139a72121699','Capture1026 audio tuple required')
 clients=[]
 for key in ('capture_client','accdet_client','rotation_client'):
  e=s[key]
  if e is None:
   require(key=='capture_client','Required signed client absent');continue
  require(set(e)=={'directory','manifest','manifest_sha256','exe','sha256'},'Fixed client fields only');d=rooted(root,e['directory']);mp=rooted(root,e['manifest']);require(sha(mp)==e['manifest_sha256'] and sha(d/e['exe'])==e['sha256'],'Client pin mismatch')
  cm=read(mp)
  if key=='capture_client':
   require(cm['schema']=='capture-next122-log-isolation-v1' and cm['data_directory']==DIRECTORY and cm['signer_thumbprint']==SIGNER and cm['signature_status']=='Valid' and cm['source_sha256']==sha(Path(__file__).parents[1]/'audio/mt8167-audio-capture-probe.c'),'New capture source/log/signature contract required');assert_paths(d/e['exe'],True)
  elif key=='accdet_client':require(e['sha256']=='56650bc0d0339564dfdfd3287153a8b782a92fb127a54e29ffb9654a2921b85f' and sha(mp)==PINS['parent'][0],'Fixed signed ACCDET client only')
  else:
   require(sha(mp)=='30a6d4bafae4ff518a2b192c6922b49a3394dc6695da7e4e12a3e707e95d34dc' and cm['schema']=='mt8167-display-rotation-probe-v1' and cm['source_commit']=='d84cc5723bca17c1472f1df2275dadb6f5fb05bb' and cm['certificate_thumbprint']==SIGNER and cm['fixed_directory']=='Windows\\System32\\MT8167DisplayRotationProbeV1' and cm['command_arguments']==[] and e['sha256']=='0455f9b0f026aae287198dd1f5f9c8269cc6167bc44f90c94ad1938c55dfca6b','Fixed three-EXE rotation wrapper contract required')
  dependencies=[]
  if key=='rotation_client':
   for name,h in [('mt8167-display-rotation-session.exe','b9cbf5154cae43091cfbbb545691623f5cfa7bc5907282e6d8133b212d5cfb08'),('mt8167-display-rotation-user.exe','3b8221e25e80d522a947a0af94d323286640c842a6a01f12376e57ea2c7ea33b')]:
    require(sha(d/name)==h,'Rotation dependency byte mismatch');dependencies.append({'exe':name,'sha256':h})
  clients.append({**e,'arguments':'--read' if key=='accdet_client' else '', 'timeout_ms':300000 if key=='rotation_client' else (120000 if key=='capture_client' else 10000),'name':key.removesuffix('_client'),'relative_directory':'MT8167DisplayRotationProbeV1' if key=='rotation_client' else DIRECTORY+'\\clients','dependencies':dependencies})
 update_sys={TARGETS[x][3] for x in INSTALL};preserved={k:v['sha256'] for k,v in files.items() if k.startswith('Windows/System32/Drivers/') and k.split('/')[-1] not in update_sys or k=='Windows/System32/MT8167EngineProbe.exe'}
 require(set(s['runtime_retained'])=={'Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw'},'Current sealed GPU runtime fixed comparison witnesses required')
 expected={'Windows/System32/Drivers/ge8300-mips.fw':gm['expected_installed_files']['ge8300-mips.fw'],'Windows/System32/ge8300-umd.dll':gs['umd_sha256'],'Windows/System32/MT8167RenderSingleGpu.exe':gm['service_exe_sha256']}
 for name,e in s['runtime_retained'].items():
  p=rooted(root,e['path']);require(sha(p)==e['sha256'] and (name not in expected or e['sha256']==expected[name]),'Current sealed GPU runtime witness mismatch');preserved[name]=e['sha256'];sources[name]=p.relative_to(root).as_posix()
 bp,bm=pinned(s['boot']);require(bm['uefi_source_commit']=='8e2597c32bff4c0513faafe38f34b7976c32bd7e' and bm['boot_img_sha256']=='125f42b54ae9503e1919cc67cba6be53cfb08928497ffc5a9c10cae85738b887' and sha(bp.parent/'boot.img')==bm['boot_img_sha256'],'Exact source-built thermal observation matched8range candidate required')
 firmware=bp.parent.parent;fd=firmware/'SILICIUM_UEFI.fd';require(sha(fd)=='bf2c5f37a0165623db295ed3b1883df36facc154c7c6e72d8cf9941d09fa4696','Exact matched8range FD required')
 require(sha(firmware/'manifest.json')=='40fd7caf884044d2c228d88679ee8974292f0a4cb6ada7acd476dda7b3c705aa','Pinned thermal firmware compatibility evidence mismatch')
 require(sha(firmware/'baseline-compatibility.json')=='4368788b4147bae4044b9c81b1d0c2a39da839f2cf658ed17adb92d577ee3af5','Pinned thermal firmware compatibility evidence mismatch')

 selection={'schema':1,'profile':PROFILE,'capture_number':ack['capture_number'],'manifest_sha256':FRESH,'recovery_boot_id':BOOT,'gpu_policy_sha256':GPU,'unchanged_since_capture':True}
 return {'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':DIRECTORY,'draft':False,'normal_install_allowed':True,'backup_manifest':mf.relative_to(root).as_posix(),'backup_manifest_sha256':FRESH,'recovery_boot_id':BOOT,'capture_selection':selection,'gpu_seal_sha256':sha(gsp),'gpu_media_manifest_sha256':sha(gmp),'main_approval_sha256':sha(ap),'native_snapshot_sha256':sha(np),'seven_snapshot_sha256':sha(sp),'source_files':{name:{k:files['Windows/System32/config/'+name][k] for k in ('bytes','sha256')} for name in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'packages':pkgs,'baselines':baselines,'service_preservation':n['service_preservation'],'preserved_files':preserved,'preserved_sources':{k:sources[k] for k in preserved},'boot_transition':{'mode':'main-controlled-matched8range-handoff','boot_sha256':bm['boot_img_sha256'],'source_commit':bm['uefi_source_commit'],'boot_path':(bp.parent/'boot.img').relative_to(root).as_posix(),'fd_path':fd.relative_to(root).as_posix(),'fd_sha256':sha(fd),'firmware_manifest_sha256':sha(firmware/'manifest.json'),'main_handoff_required':True,'next_windows_boot_blocked_until_matched_boot':True,'gpt_written':False},'planned_reboots':1,'install_targets':list(INSTALL),'stage_order':list(ROLES),'diagnostic_clients':clients,'gpu_install':False,'engine_retest':False,'device_verified':False}
def generate(p):
 require(p['profile']==PROFILE and p['draft'] is False and p['install_targets']==list(INSTALL) and p['stage_order']==list(ROLES),'Exact next profile only');token=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
 lines=['#define A04_PERIPHERAL_INSTALL_MASK 0x7cu','static const char deferred_token[]="'+token+'";','static const struct peripheral_target targets[7]={']
 for role,t in TARGETS.items():
  v=p['packages'][role];b=p['baselines'].get(role,{'version_u64':0,'date_filetime':0,'sys_sha256':'0'*64,'inf':''});lines.append('{'+','.join(cstr(x) for x in (t[0],t[1],t[2],role,v['names']['inf'],t[3],v['names']['cat']))+','+str(v['version_u64'])+'ULL,'+str(v['date_filetime'])+'ULL,{'+','.join(cbytes(v['hashes'][k]) for k in ('inf','sys','cat'))+'},'+str(b['version_u64'])+'ULL,'+str(b['date_filetime'])+'ULL,'+cbytes(b['sys_sha256'])+','+cstr(b['inf'])+'},')
 lines+=['};','static const struct preserved_file preserved_files[]={']
 for name,h in p['preserved_files'].items():lines.append('{'+cstr(name[len('Windows/System32/'):].replace('/','\\'))+','+cbytes(h)+'},')
 lines+=['};','static const struct preserved_service preserved_services[]={']
 for name,v in p['service_preservation'].items():lines.append('{'+cstr(name)+','+str(int(v['present']))+','+cbytes(v['root_sha256'] if v['present'] else '0'*64)+'},')
 lines+=['};','static const struct diagnostic_client diagnostic_clients[]={']
 for v in p['diagnostic_clients']:lines.append('{'+cstr(v['exe'])+','+cstr(v['arguments'])+','+cstr(v['name'])+','+cstr(v['relative_directory'])+','+cbytes(v['sha256'])+','+str(v['timeout_ms'])+'u},')
 return '\n'.join(lines+['};'])+'\n'
def main():
 a=argparse.ArgumentParser();a.add_argument('--workspace',type=Path,required=True);a.add_argument('--map',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a.add_argument('--enable-final-main-capture',action='store_true');x=a.parse_args();require(x.enable_final_main_capture,'Explicit main-capture opt-in required');p=load_map(x.map,x.workspace);out=x.out.resolve();require(out.is_relative_to(x.workspace.resolve()) and not out.exists(),'New root output required');out.mkdir(parents=True)
 for name,value in [('peripheral-policy.json',p),('peripheral-preservation-plan.json',p)]: (out/name).write_text(json.dumps(value,indent=2)+'\n')
 (out/'peripheral-batch-policy.h').write_text(generate(p));(out/'peripheral-service-path-policy.h').write_text(generate_paths())
if __name__=='__main__':main()
