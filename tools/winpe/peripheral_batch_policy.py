#!/usr/bin/env python3
"""GPUとは分離したUSB1009/MMA1.10/RTC-thermal1101。main指定capture未結合は発火不可。"""
import argparse, datetime, hashlib, json, re
from pathlib import Path
from render_profile_policy import require, rooted as checked_rooted, sha, hexpin, SIGNER
from codec_msdc_transaction import version, datevalue
PROFILE='peripheral-usb1009-mma110-emmc19-parent1101-v1'
SERVICE='MT8167PeripheralBatch'
DIRECTORY='MT8167PeripheralBatch'
BOOT_SOURCE='6c96c6f60149f2b01d068909c8d0c308e5b4f3ee'
BOOT_SHA='ece47d799e04e794bffcf902d2e4ef32c7e898f60079eff98134467afda693d8'
FD_SHA='9cc5249132a9d0383854312186e5859560df42abfadbfe9ae9dd31ea12777b6a'
# Order is a contract: child store staging precedes every parent selection.
TARGETS={
 'rtc':('MT6392\\RTC','Mt6392Rtc','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt6392-rtc.inf','mt6392-rtc.sys','mt6392-rtc.cat','0.0.1.0'),
 'thermal':('MT8167\\Thermal','Mt8167Thermal','{4D36E97D-E325-11CE-BFC1-08002BE10318}','mt8167-thermal.inf','mt8167-thermal.sys','mt8167-thermal.cat','0.0.1.7'),
 'usb':('MT8167\\USB20','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mt8167-usb20.inf','mt8167-usb20.sys','mt8167-usb20.cat','2026.11.8.1009'),
 'mma':('ACPI\\MOU0010','mshidkmdf','{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}','mma8652.inf','mma8652.sys','mma8652.cat','0.0.1.10'),
 'emmc':('ACPI\\MOU0001','a04msdcboot','{4D36E97B-E325-11CE-BFC1-08002BE10318}','mt8167-msdc.inf','mt8167-msdc.sys','mt8167-msdc.cat','0.0.1.9'),
 'parent':('ACPI\\BEN0001','Mt8167Shared','{4D36E97D-E325-11CE-BFC1-08002BE10318}','shared-parent.inf','mt8167-shared.sys','mt8167-shared-rtc-thermal.cat','2026.11.8.1101')}
PRESERVED_SERVICES=('MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs')
FIXED={'usb':('204c46c88e1c16a4a12c75d5bb5e868615ccb7b0','9f8d3e49d13efe6fa0b84d71c061c3829482c7352ed8f409ac8914d586320a7a'),
       'parent':('1d248a71a95360966ff867e4b063504ac764e074','398fd007079ab4710404d67a60dc7db46b84dd149ce3ae69e3bbc730264e823c'),
       'rtc':('1d248a71a95360966ff867e4b063504ac764e074','3da6c7dd7cc5279fccc45581ce2b20405e08291b42ebf1fbf45e2a7a2e49d7eb'),
       'thermal':('1d248a71a95360966ff867e4b063504ac764e074','ca211e366704380966ef639c24a1d49b71425b39cd7272ebdeeb03c3b412408b'),
       'emmc':('c6b5e319656a1f19a103bb5afc1c41294150777f','b3ce17a8a36b1afba25ca53503735f57aba4b47723357a960169dd47b36f9bda'),
       'mma':('9ad5ef0afff9712399f86e429787464e6bf710cd','998329ec0541a7738c893c3185b591439e433ec55489aee657670a5b89f52388')}
MANIFESTS_FIXED={'rtc': 'c39a4d605803a47cdd3a6f175164721597f193d6b0ef3441c37146033a421a02', 'thermal': 'c39a4d605803a47cdd3a6f175164721597f193d6b0ef3441c37146033a421a02', 'usb': 'f2c0f7a7fc83288d4edade0f4ff5cb7977821b3e9d9f8d1759a575d58d536cdf', 'mma': '2f1abf975d0770706778dc2710ce965bdf9edcc21768bd33145af8aeee33980c', 'emmc': '1380d745fee09616c0117401de081f95379e46c2b85cacb5778253a4cf71b4d3', 'parent': 'c39a4d605803a47cdd3a6f175164721597f193d6b0ef3441c37146033a421a02'}
BASELINES_FIXED={'parent':('2026.11.7.1026','f176836aafb34447b423398d301ffa01d9787f3bc197c8c29f371145a6e756e1'),'usb':('2026.11.7.1005','db1e26ebb856d197022e284f42da5669128e3bf58b25b3080563c4288ffc9fdf'),'mma':('0.0.1.0','44760ee15335cbbc9cbf6277e68b7645f9bddfd6ae8ba93cb276a074d96158e1'),'emmc':('0.0.1.8','f92ee2d4e058e345539567153e0b4a20b13669f34f84de017dece2e45707dadd')}
def rooted(root,path):
 p=Path(path)
 return checked_rooted(root,p if p.is_absolute() else root/p)

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def inftext(p):
 b=p.read_bytes();return b.decode('utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig')
def packages(spec,root):
 require(set(spec)==set(TARGETS),'固定6package以外は禁止')
 out={}
 for role,t in TARGETS.items():
  e=spec[role];require(set(e)=={'directory','manifest','manifest_sha256','source_commit','files','certificate'},'固定package入力schema不一致')
  require(e['manifest_sha256']==MANIFESTS_FIXED[role],'固定採用manifest以外拒否')
  d=rooted(root,e['directory']);mf=rooted(root,e['manifest']);require(sha(mf)==hexpin(e['manifest_sha256']),'署名manifest pin不一致')
  source=hexpin(e['source_commit'],40);m=read(mf)
  require(m.get('source_commit',m.get('package_source_commit'))==source,'package source pin不一致')
  require(m.get('signer_thumbprint',m.get('signing_thumbprint',m.get('signer')))==SIGNER,'BCBF署名記録が必要')
  require(set(e['files'])=={'inf','sys','cat'},'INF/SYS/CAT/CER固定4fileが必要')
  names={};hashes={}
  for kind in ('inf','sys','cat'):
   f=e['files'][kind];require(set(f)=={'name','sha256','manifest_member'} and re.fullmatch(r'[A-Za-z0-9_.-]+',f['name']),'flat package fileのみ')
   p=rooted(root,d/f['name']);require(p.parent==d and sha(p)==hexpin(f['sha256']),'package実byte不一致');names[kind]=f['name'];hashes[kind]=sha(p)
  entries=m.get('files',m.get('outputs',{}))
  listed={k:(v['sha256'] if isinstance(v,dict) else v) for k,v in entries.items()} if isinstance(entries,dict) else {x.get('path',x.get('name')):x['sha256'] for x in entries}
  require(all(str(listed.get(e['files'][k]['manifest_member'],'')).lower()==hashes[k] for k in ('inf','sys','cat')),'各package fileは署名manifestにもhash結合が必要')
  require(names['sys']==t[4] and names['cat']==t[5],'固定driver/sys/catalog以外拒否')
  require(names['inf']==t[3] or (role=='thermal' and names['inf']=='mt8167-thermal-native.inf') or (role=='parent' and names['inf']=='shared-parent-rtc-thermal1101.inf'),'固定INF basename不一致')
  cert=e['certificate'];require(set(cert)=={'path','sha256'},'certificate schema不一致');cp=rooted(root,cert['path']);require(sha(cp)==hexpin(cert['sha256']) and hashlib.sha1(cp.read_bytes()).hexdigest().upper()==SIGNER,'BCBF証明書実byte不一致')
  text=inftext(d/names['inf']);v=re.search(r'(?im)^DriverVer\s*=\s*([^\r\n;]+)',text);require(v and v[1].split(',')[-1]==t[6],'固定DriverVer不一致')
  require(t[0].lower() in text.lower() and re.search(r'(?im)^AddService\s*=\s*'+re.escape(t[1])+r'\s*,',text),'固定ID/service以外拒否')
  require(re.search(r'(?im)^ClassGuid\s*=\s*'+re.escape(t[2])+r'\s*$',text) and re.search(r'(?im)^CatalogFile\s*=\s*'+re.escape(t[5])+r'\s*$',text),'class/catalog不一致')
  require(not re.search(r'(?im)^(CoInstallers32|RegisterDlls|UnregisterDlls|RunPreSetupCommands|RunPostSetupCommands)\s*=',text),'実行性INF拡張禁止')
  if role in FIXED:require(source==FIXED[role][0] and hashes['sys']==FIXED[role][1],'USB/MMA固定候補以外拒否')
  date=v[1].split(',')[0];datevalue(date)
  if role=='emmc':require(re.search(r'(?im)^StartType\s*=\s*0\s*$',text) and re.search(r'(?im)^LoadOrderGroup\s*=\s*SCSI Miniport\s*$',text) and 'AJTD4R' in text,'AJTD4R boot-start/group保持契約不一致')
  if role in ('parent','rtc','thermal'):require(m.get('hfp_105x_merged') is False and m.get('rtc_binaries_unchanged') is True and m.get('frozen_rtc_manifest_sha256')=='49f698b8770d0382d478385d743d6217571831da9973081224897f828814d386','1101 tuple/HFP105x非混入/RTC凍結保持が必要')
  out[role]={'directory':d.relative_to(root).as_posix(),'source_commit':source,'manifest_sha256':sha(mf),'names':names,'hashes':hashes,'version':t[6],'version_u64':version(t[6]),'date':date,'date_filetime':datevalue(date),'parent_flags':m.get('flags') if role=='parent' else None}
 return out

def validate_selection(selection,manifest,manifest_sha,boot_id):
 require(set(selection)=={'schema','profile','capture_number','manifest_sha256','recovery_boot_id','gpu_policy_sha256','unchanged_since_capture'},'main指定capture schema不一致')
 require(selection['schema']==1 and selection['profile']==PROFILE and type(selection['capture_number']) is int and selection['capture_number']>=114,'fresh114以降のmain指定capture必須')
 require(re.search(r'(?:^|/)private-windows-rt-prebackup-run'+str(selection['capture_number'])+r'/manifest\.json$',manifest),'指定capture世代不一致')
 require(selection['manifest_sha256']==hexpin(manifest_sha) and selection['recovery_boot_id']==boot_id and re.fullmatch(r'[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}',boot_id or '') and selection['unchanged_since_capture'] is True,'capture hash/boot id/無変更明示が必要')
 hexpin(selection['gpu_policy_sha256'])

def bind_capture(f,root):
 base={'manifest','sha256','bindings','bindings_sha256','main_selection','main_selection_sha256'}
 require(set(f) in (base,base|{'supplement','supplement_sha256'}),'明示main selectionとnative snapshotのpin必須')
 mf=rooted(root,f['manifest']);require(sha(mf)==hexpin(f['sha256']),'capture manifest pin不一致')
 b=read(mf);sp=rooted(root,f['main_selection']);require(sha(sp)==hexpin(f['main_selection_sha256']),'main selection pin不一致');selection=read(sp)
 validate_selection(selection,mf.relative_to(root).as_posix(),f['sha256'],b.get('recovery_boot_id'))
 require(b.get('status')=='captured-unmounted' and b.get('read_only') is True and b.get('unmounted') is True and b.get('errors')==[],'完了Recovery原本のみ')
 files={}
 for e in b['files']:
  p=rooted(root,mf.parent/e['backup_path']);name=e['source_path'].replace('\\','/').lower()
  require(name not in files and p.is_relative_to(mf.parent) and sha(p)==hexpin(e['sha256']) and p.stat().st_size==e['bytes'],'capture重複/byte不一致');files[name]=e
 if 'supplement' in f:
  supplement=rooted(root,f['supplement']);require(sha(supplement)==hexpin(f['supplement_sha256']),'補足capture pin不一致');extra=read(supplement)
  require(set(extra)=={'schema','source_manifest_sha256','recovery_boot_id','status','read_only','unmounted','errors','files'} and extra['schema']==1 and extra['source_manifest_sha256']==f['sha256'] and extra['recovery_boot_id']==b['recovery_boot_id'] and extra['status']=='captured-unmounted' and extra['read_only'] is True and extra['unmounted'] is True and extra['errors']==[],'同boot read-only補足captureのみ許可')
  allowed={'Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw'}
  require(len(extra['files'])==3 and {e['source_path'] for e in extra['files']}==allowed,'補足は固定GPU保持3fileだけ')
  for e in extra['files']:
   q=rooted(root,supplement.parent/e['backup_path']);name=e['source_path'].lower()
   require(name not in files and q.is_relative_to(supplement.parent) and 0<e['bytes']<=16*1024*1024 and q.stat().st_size==e['bytes'] and sha(q)==hexpin(e['sha256']),'補足file重複/hash/size不一致');files[name]={**e,'absolute_capture_path':str(q)}
 bindings=rooted(root,f['bindings']);require(sha(bindings)==hexpin(f['bindings_sha256']),'bindings pin不一致');binding=read(bindings)
 require(binding.get('source_manifest_sha256')==f['sha256'] and binding.get('recovery_boot_id')==selection['recovery_boot_id'] and binding.get('gpu_policy_sha256')==selection['gpu_policy_sha256'] and binding.get('original_unchanged') is True and binding.get('errors')==[] and all(type(binding.get(k)) is int and binding[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')),'native capture/policy/boot出自不一致')
 for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2'):
  require(binding.get('source_files',{}).get(n)==files['windows/system32/config/'+n.lower()]['sha256'],'native trio pin不一致')
 return mf,b,files,binding,selection

def load_map(path,root):
 root=root.resolve(strict=True);s=read(rooted(root,path))
 require(set(s)=={'schema','profile','draft','fresh','packages','parent_flags','preserved_files','service_preservation','boot_transition'},'固定batch input schema不一致')
 require(s['schema']==1 and s['profile']==PROFILE,'専用batch profileのみ')
 require(s['draft'] is False and s['fresh'] is not None,'main指定capture未結合draftはbuild/arm/stage不可')
 f=s['fresh'];mf,b,files,binding,selection=bind_capture(f,root)
 pkgs=packages(s['packages'],root)
 flags=s['parent_flags'];require(pkgs['parent']['parent_flags']==flags['candidate'],'親実manifest flags不一致');require(set(flags)=={'baseline','candidate','added'},'親flags前後が必要')
 require(flags['added']=={'MT6392_RTC_CALENDAR':'1','A04_SHARED_THERMAL_DIAGNOSTIC':'1','A04_SHARED_THERMAL_NATIVE_EXPERIMENT':'1','A04_SHARED_GPU_THERMAL_PROFILE':'1'} and flags['candidate']=={**flags['baseline'],**flags['added']},'親退行/無関係flags変更は禁止')
 require(all(flags['baseline'].get(k)=='1' for k in ('A04_SHARED_GPU_DOMAIN_SPM','A04_SHARED_GPU_PLATFORM','A04_SHARED_SD_POWER','A04_SHARED_CAPTURE','A04_SHARED_ENGINE_PCM','A04_AUDIO_CHILD_RESOURCES','A04_SHARED_WIFI_RUNTIME')),'既存親機能保持が必要')
 baselines={}
 for role in ('usb','mma','emmc','parent'):
  t=TARGETS[role];rows=binding['nodes'].get(t[0],[]);require(len(rows)==1,'既存target単一instance必須');row=rows[0];selected=row['selected'];require(re.fullmatch(r'[A-Za-z0-9_.-]+\.inf',selected['InfPath'],re.I),'保持OEM INF basename不正');old=version(selected['DriverVersion'])
  require(old<pkgs[role]['version_u64'],'同版再install/親退行は禁止')
  require(str(row['Service']).lower()==t[1].lower() and str(row['ClassGUID']).lower()==t[2].lower(),'既存target ID/service/class不一致')
  entry=files.get('windows/system32/drivers/'+t[4]);require(entry,'既存SYS capture必須');require(selected['DriverVersion']==BASELINES_FIXED[role][0] and entry['sha256']==BASELINES_FIXED[role][1],'固定実搭載基点以外への退行/合流は禁止')
  baselines[role]={'version_u64':old,'date_filetime':datevalue(datetime.datetime.strptime(selected['DriverDate'].replace('-','/'),'%m/%d/%Y').strftime('%m/%d/%Y')),'sys_sha256':entry['sha256'],'inf':selected['InfPath']}
 require(s['service_preservation']==binding['service_preservation'],'native snapshotと保持指定の一致必須')
 require(set(s['service_preservation'])==set(PRESERVED_SERVICES),'旧サービス全固定集合の保持が必要')
 for n,d in s['service_preservation'].items():
  require(isinstance(d,dict) and set(d)=={'present','root_sha256','subtree_sha256'},'旧service fingerprint schema不正')
  require(type(d['present']) is bool and ((not d['present'] and d['root_sha256'] is None and d['subtree_sha256'] is None) or (d['present'] and re.fullmatch('[0-9a-f]{64}',d['root_sha256']) and re.fullmatch('[0-9a-f]{64}',d['subtree_sha256']))),'旧service fingerprint不正')
 transition=s['boot_transition'];require(set(transition)=={'source_commit','boot_path','boot_sha256','fd_path','fd_sha256','mode','next_windows_boot_blocked_until_matched_boot','handoff_receipt','handoff_receipt_sha256'},'同時boot更新契約必須')
 require(transition['source_commit']==BOOT_SOURCE and transition['boot_sha256']==BOOT_SHA and transition['fd_sha256']==FD_SHA and sha(rooted(root,transition['boot_path']))==BOOT_SHA and sha(rooted(root,transition['fd_path']))==FD_SHA,'親1101/8range POST bootの固定tuple不一致')
 require(transition['mode']=='main-controlled-fastboot-handoff' and transition['next_windows_boot_blocked_until_matched_boot'] is True,'旧6range bootへの自動resume禁止')
 receipt=rooted(root,transition['handoff_receipt']);require(sha(receipt)==hexpin(transition['handoff_receipt_sha256']),'main boot handoff receipt pin必須');ack=read(receipt)
 require(ack=={'profile':PROFILE,'recovery_boot_id':b['recovery_boot_id'],'capture_manifest_sha256':f['sha256'],'gpu_policy_sha256':selection['gpu_policy_sha256'],'mode':'main-controlled-fastboot-handoff','block_next_windows_boot':True,'current_parent_sys_sha256':BASELINES_FIXED['parent'][1],'target_parent_sys_sha256':FIXED['parent'][1],'target_boot_sha256':BOOT_SHA,'target_fd_sha256':FD_SHA},'GPU自動resumeとは分離したmain boot handoffの明示結合が必要')
 preserved={}
 for e in s['preserved_files']:
  require(set(e)=={'path','sha256'} and re.fullmatch(r'Windows/System32/(?:Drivers/)?[A-Za-z0-9_.-]+',e['path']),'保持file path不正')
  actual=files.get(e['path'].lower());require(actual and actual['sha256']==hexpin(e['sha256']),'保持file capture不一致');require(e['path'].split('/')[-1] not in {TARGETS[x][4] for x in ('usb','mma','emmc','parent')},'更新対象を保持表へ混在禁止');preserved[e['path']]=e['sha256']
 require(len(preserved)>=7 and {'Windows/System32/MT8167EngineProbe.exe','Windows/System32/MT8167RenderSingleGpu.exe','Windows/System32/Drivers/mt8167-audio.sys','Windows/System32/Drivers/mt6630-runtime.sys','Windows/System32/Drivers/mt8167-display.sys','Windows/System32/Drivers/ge8300-render.sys','Windows/System32/Drivers/mt8167-toprgu.sys','Windows/System32/ge8300-umd.dll','Windows/System32/Drivers/ge8300-mips.fw','Windows/System32/Drivers/mt8167-ge8300.sys'}.issubset(preserved),'旧Engine/render EXE等の保持pinが必要')
 return {'schema':1,'profile':PROFILE,'service':SERVICE,'data_directory':DIRECTORY,'draft':False,'normal_install_allowed':True,'backup_manifest':mf.relative_to(root).as_posix(),'backup_manifest_sha256':f['sha256'],'recovery_boot_id':b['recovery_boot_id'],'capture_selection':selection,'capture_selection_sha256':f['main_selection_sha256'],'native_snapshot_sha256':f['bindings_sha256'],'source_files':{n:{k:files['windows/system32/config/'+n.lower()][k] for k in ('sha256','bytes')} for n in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')},'packages':pkgs,'baselines':baselines,'parent_flags':flags,'preserved_files':preserved,'preserved_sources':{n:Path(files[n.lower()].get('absolute_capture_path',mf.parent/files[n.lower()]['backup_path'])).relative_to(root).as_posix() for n in preserved},'supplement_sha256':f.get('supplement_sha256'),'service_preservation':s['service_preservation'],'boot_transition':transition,'planned_reboots':1,'install_targets':['usb','mma','emmc','parent'],'stage_order':list(TARGETS),'post_reboot_required':list(TARGETS),'readonly_status':['rtc','thermal'],'gpu_install':False,'engine_retest':False,'device_verified':False}

def cstr(v):return 'L"'+v.replace('\\','\\\\')+'"'
def cbytes(h):return '{'+','.join('0x'+h[i:i+2] for i in range(0,64,2))+'}'
def generate(p):
 require(p.get('draft') is False and p.get('profile')==PROFILE and p.get('normal_install_allowed') is True,'発火可能headerはfinal fresh結合後だけ')
 hexpin(p.get('capture_selection_sha256'));hexpin(p.get('native_snapshot_sha256'))
 validate_selection(p.get('capture_selection',{}),p.get('backup_manifest',''),p.get('backup_manifest_sha256'),p.get('recovery_boot_id'))
 require(list(p.get('packages',{}))==list(TARGETS) and p.get('planned_reboots')==1 and p.get('install_targets')==['usb','mma','emmc','parent'] and p.get('boot_transition',{}).get('next_windows_boot_blocked_until_matched_boot') is True,'固定stage/DIF/boot契約が必要')
 token=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
 lines=['/* Generated fixed peripheral contract; no GPU/Engine client. */','#define A04_PERIPHERAL_INSTALL_MASK 0x3cu','static const char deferred_token[]="'+token+'";','static const struct peripheral_target targets[6]={']
 for role,t in TARGETS.items():
  v=p['packages'][role];base=p['baselines'].get(role,{'version_u64':0,'date_filetime':0,'sys_sha256':'0'*64,'inf':''})
  lines.append('{'+','.join(cstr(x) for x in (t[0],t[1],t[2],role,v['names']['inf'],t[4],t[5]))+','+str(v['version_u64'])+'ULL,'+str(v['date_filetime'])+'ULL,{'+','.join(cbytes(v['hashes'][k]) for k in ('inf','sys','cat'))+'},'+str(base['version_u64'])+'ULL,'+str(base['date_filetime'])+'ULL,'+cbytes(base['sys_sha256'])+','+cstr(base['inf'])+'},')
 lines+=['};','static const struct preserved_file preserved_files[]={']
 for n,h in p['preserved_files'].items():lines.append('{'+cstr(n[len('Windows/System32/'):].replace('/','\\'))+','+cbytes(h)+'},')
 lines+=['};','static const struct preserved_service preserved_services[]={']
 for n,v in p['service_preservation'].items():lines.append('{'+cstr(n)+','+str(int(v['present']))+','+cbytes(v['root_sha256'] if v['present'] else '0'*64)+'},')
 return '\n'.join(lines+['};'])+'\n'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--map',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();p=load_map(a.map,a.workspace);out=a.out.resolve();require(out.is_relative_to(a.workspace.resolve()) and not out.exists(),'新規root内出力必須');out.mkdir(parents=True)
 (out/'peripheral-policy.json').write_text(json.dumps(p,indent=2)+'\n');(out/'peripheral-batch-policy.h').write_text(generate(p));(out/'peripheral-preservation-plan.json').write_text(json.dumps(p,indent=2)+'\n')
if __name__=='__main__':main()
