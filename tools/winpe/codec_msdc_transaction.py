"""MT8167 codec1026 + MSDC quiet1.8: main返却の最新manifestへ明示結合する固定延期更新。"""
import datetime, hashlib, json, re
from pathlib import Path, PureWindowsPath
from engine_io_package import SignedPackage, check_inf, require_armnt
from private_hardware_probe_input import sha
PROFILE='codec1026-msdc-quiet-explicit-fresh-v2'
CERT='BCBF6C456C6F0DD0818434763B189488FBF13570'
CAPTURE='2d6231727e8ed72b6d7dca4148debc101ac5aeef'
MSDC='e2db926f1ddd12b4cae341a7df50f832e20c16e5'
BASE_PARENT='13ae9f85c37331a3dd7324278e932956b9a840ef'
BASE_MSDC='f8f54c9b162cc0b9cc342fb0dc94953c1f59cae5'
TARGETS={
 'parent':('ACPI\\BEN0001','shared-parent','mt8167-shared','Mt8167Shared','2026.11.7.1026'),
 'audio':('MT8167\\AFE','mt8167-afe-engine-capture','mt8167-audio','Mt8167Audio','2026.11.7.1026'),
 'emmc':('ACPI\\MOU0001','mt8167-msdc','mt8167-msdc','a04msdcboot','0.0.1.8')}
BASELINES={
 'parent':('2026.11.7.1015','e7df942265a8216ba393152f1fe781e47d48ebe09b6da02f235dd8c2ba7d9752'),
 'audio':('2026.11.7.1007','b917801c0f673aa5ab36be25c4133ad5d1370f1130936bbc4fda7086cf374edc'),
 'emmc':('0.0.1.0','fb4d168baa3d8ec8297c1fe33b195a9e1af85a79bf4f4267103b677bbd24fa2a')}
BASELINE_DATES={'parent':'11/07/2026','audio':'11/06/2026','emmc':'11/02/2026'}
# Actual installed USB1005 is 44899b9, not the re-signed audit build.
RETAINED={
 'usb':('MT8167\\USB20','A04Usb20Keyboard','2026.11.7.1005','mt8167-usb20.sys','db1e26ebb856d197022e284f42da5669128e3bf58b25b3080563c4288ffc9fdf'),
 'wifi':('A04BR3\\MT6630SDIO','MT6630Runtime','2026.11.6.1012','mt6630-runtime.sys','5044f8b47b5e89383923c36af8c5a29e21bf1224885b42ede55155a59fce4351'),
 'display':('ACPI\\MOU0007','MT8167Display','0.0.1.6','mt8167-display.sys','8a2cf298b782acad669cf95b6dd81ced062761e1e9ecd5d095316dedfff3a4b0'),
 'render':('ACPI\\MOU0006','GE8300Render','0.0.3.12','ge8300-render.sys','adf92be2b28f2b353eb0a247620cc23e8d9051841bea9aacbd2522ae08525d4b')}
RETAINED_FILES={x[3]:x[4] for x in RETAINED.values()}
RETAINED_FILES.update({
 'mt8167-ge8300.sys':'a8f226b33fa11915e88b3424c5ad3ed3aaa62ff35deb7f91902b04e7df4bf090',
 'mt8167-toprgu.sys':'8caa8d85dc27251f0f00bf5f60e05657e30c6c3ff4140147f121d3467518c554',
 'ge8300-mips.fw':'5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1',
 'ge8300-umd.dll':'f8a711915e878e7aeb58ef996db8abbeee15351bd10bf8e0fdcabe12009f922b'})
GPU_SOURCE='9f31ddcf89fbe6d5c3a7b5abcceadb223212d378'
CLIENT='mt8167-capture-probe.exe'
def version(v):
 p=tuple(map(int,v.split('.')))
 if len(p)!=4 or any(x<0 or x>65535 for x in p):raise ValueError('DriverVersion不正')
 return sum(x<<((3-i)*16) for i,x in enumerate(p))
def datevalue(s):return int((datetime.datetime.strptime(s,'%m/%d/%Y')-datetime.datetime(1601,1,1)).total_seconds())*10000000
def load(path,root,*,draft=False):
 spec=json.loads(path.read_text(encoding='utf-8-sig'))
 if set(spec)!={'schema','transaction','backup_manifest','backup_manifest_sha256','bindings','boot','packages'} or spec['schema']!=1 or spec['transaction']!=PROFILE:raise ValueError('codec/eMMC固定map schema不一致')
 if set(spec['packages'])!={'capture','emmc','parent1015','render312'}:raise ValueError('固定4署名inputが必要です')
 selected={};components={};certs={};packages={}
 for key,source in [('capture',CAPTURE),('emmc',MSDC),('parent1015',BASE_PARENT)]:
  e=spec['packages'][key]
  if set(e)!={'directory','manifest_sha256'}:raise ValueError('package fields不一致')
  p=SignedPackage(Path(e['directory']),root)
  if sha(p.manifest)!=e['manifest_sha256'] or p.record.get('source_commit')!=source or p.record.get('source_dirty') is not False:raise ValueError('固定署名source/hash/clean不一致: '+key)
  certname='development.cer' if key in ('capture','emmc') else 'mt8167-development.cer'
  cp=p.file(certname);der=cp.read_bytes()
  if hashlib.sha1(der).hexdigest().upper()!=CERT:raise ValueError('固定署名証明書不一致')
  packages[key]=p;certs[key]=der;selected['codec-'+key+'-development.cer']=cp
 gpu=spec['packages']['render312'];gp=Path(gpu['directory']).resolve(strict=True)
 if not gp.is_relative_to(root.resolve()) or set(gpu)!={'directory','manifest_sha256'}:raise ValueError('GPU3.12保持input不一致')
 gm=gp/'signed-manifest.json';g=json.loads(gm.read_text(encoding='utf-8-sig'))
 if sha(gm)!=gpu['manifest_sha256'] or g.get('package_source_commit')!=GPU_SOURCE or g.get('signer_thumbprint')!=CERT or g.get('signature_status')!='Valid':raise ValueError('GPU3.12署名出自不一致')
 wanted={'ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-mips.fw','ge8300-umd.dll','mt8167-development.cer'}
 if {x['name'] for x in g['outputs']}!=wanted or len(g['outputs'])!=6:raise ValueError('GPU3.12署名outputs不一致')
 for f in g['outputs']:
  if sha(gp/f['name'])!=f['sha256'].lower():raise ValueError('GPU3.12署名output hash不一致')
 for name in ('ge8300-render.sys','ge8300-mips.fw','ge8300-umd.dll'):
  if sha(gp/name)!=RETAINED_FILES[name]:raise ValueError('保持GPU3.12固定hash不一致')
 if sha(gp/'linked-manifest.json')!=g['linked_manifest_sha256'].lower():raise ValueError('GPU3.12 linked source不一致')
 if hashlib.sha1((gp/'mt8167-development.cer').read_bytes()).hexdigest().upper()!=CERT:raise ValueError('GPU3.12 cert不一致')
 old=packages['parent1015'].json('parent-manifest.json');new=packages['capture'].json('parent/parent-manifest.json')
 oldflags=old['flags'];newflags=new['flags']
 if newflags!={**oldflags,'A04_SHARED_CAPTURE':'1'} or oldflags.get('A04_SHARED_WIFI_RTC_BORROW')!='1':raise ValueError('1015 flags/RTC borrow保持不一致')
 msdc=packages['emmc'].record
 if (msdc.get('baseline_source_commit')!=BASE_MSDC or msdc.get('baseline_signed_sys_sha256')!=BASELINES['emmc'][1] or msdc.get('logs_only_change') is not True or msdc.get('flags',{}).get('A04_MSDC_VERBOSE_IO_TRACE')!='0'):raise ValueError('boot-critical MSDC source/quiet出自不一致')
 for role,(hwid,inf,sys,service,v) in TARGETS.items():
  p=packages['emmc' if role=='emmc' else 'capture'];prefix='' if role=='emmc' else role+'/'
  names=[inf+'.inf',sys+'.sys',inf+'.cat'];paths=[p.file(prefix+n) for n in names]
  ver='11/07/2026,'+v;check_inf(paths[0],ver)
  text=paths[0].read_text()
  if hwid.casefold() not in text.casefold() or not re.search(r'(?im)^AddService\s*=\s*'+re.escape(service)+r'\s*,',text):raise ValueError('固定hardware/service INF不一致')
  if role=='emmc' and (not re.search(r'(?im)^StartType\s*=\s*0\s*$',text) or not re.search(r'(?im)^LoadOrderGroup\s*=\s*SCSI Miniport\s*$',text)):raise ValueError('MSDC boot Start/Group不一致')
  if re.search(r'(?im)^(CoInstallers32|RegisterDlls|RunPreSetupCommands|RunPostSetupCommands)\s*=',text):raise ValueError('追加実行INFは禁止')
  files={role+'/'+n:sha(f) for n,f in zip(names,paths)};selected.update({role+'/'+n:f for n,f in zip(names,paths)})
  item={'wrapper_source_commit':p.record['source_commit'],'source_commit':p.record['source_commit'],'driver_ver':ver,'signed_manifest_sha256':sha(p.manifest),'certificate_thumbprint':CERT,'files':files,'version_u64':version(v),'date_filetime':datevalue('11/07/2026')}
  components[role]=item;components[role]['binary_source_commit']=p.record['source_commit'];packages[role]=item
 client=packages['capture'].file('client/'+CLIENT);selected['clients/'+CLIENT]=client
 from codec_returned_baseline import load_boot
 boot_path,boot_info=load_boot(spec['boot'],root)
 if draft:
  if any(spec[n] is not None for n in ('backup_manifest','backup_manifest_sha256','bindings')):raise ValueError('draftに旧backupを結合してはいけません')
  return selected,components,certs,{'schema':1,'transaction':PROFILE,'ready_for_build':False,'retained_boot':{'manifest_sha256':sha(boot_path),'source_commit':boot_info['uefi_source_commit'],'image_sha256':boot_info['boot_img_sha256'],'action':'preserve only; hardware state unverified'},'status':'source-policy-only; 返却fresh未結合、署名service/媒体未生成','packages':{r:packages[r] for r in TARGETS},'clients':{CLIENT:sha(client)},'baseline_sources':{'parent':BASE_PARENT,'audio':'3d83f6fee554455434e2c03112c56bbf351e4c4b','emmc':BASE_MSDC},'baseline':BASELINES,'retained_gpu_source_commit':GPU_SOURCE,'retained_gpu_signed_manifest_sha256':sha(gm),'retained':{k:list(v) for k,v in RETAINED.items()},'retained_files':RETAINED_FILES,'existing_parent_flags':oldflags,'added_parent_flags':{'A04_SHARED_CAPTURE':'1'},'planned_reboot_count':1,'hardware_operations_performed':False}
 from codec_returned_baseline import load_returned
 backup,record,selection,retained_boot,bindings_sha=load_returned(spec,root,TARGETS,RETAINED)
 actual={f['source_path'].replace('\\','/').lower():f for f in record['files']}
 wanted={**{TARGETS[k][2]+'.sys':v[1] for k,v in BASELINES.items()},**RETAINED_FILES}
 oldfiles={}
 for name,digest in wanted.items():
  item=actual.get(('windows/system32/' if name=='ge8300-umd.dll' else 'windows/system32/drivers/')+name)
  if item is None and not name.endswith('.sys'):continue # FW/DLL are pinned and verified at runtime; collector may contain SYS only.
  if not item or item['sha256'].lower()!=digest:raise ValueError('返却fresh配置SYS/FW/DLL不一致: '+name)
  oldfiles[name]={'sha256':digest,'backup_path':item['backup_path'],'bytes':item['bytes']}
 # Read capture bytes already hash-checked by backup_check; never load SYSTEM.
 policy={'schema':1,'mode':'engine_fixed_deferred_v1','returned_selection':selection,'bindings_sha256':bindings_sha,'retained_boot':retained_boot,'transaction':PROFILE,'staging_profile':'normal-deferred','backup_manifest_sha256':sha(backup),'recovery_boot_id':record['recovery_boot_id'],'packages':{r:packages[r] for r in TARGETS},'clients':{CLIENT:sha(client)},'baseline':{r:{'version_u64':selection[r]['version_u64'],'date_filetime':selection[r]['date_filetime'],'sys_sha256':h} for r,(v,h) in BASELINES.items()},'retained_gpu_source_commit':GPU_SOURCE,'retained_gpu_signed_manifest_sha256':sha(gm),'retained':{k:list(v) for k,v in RETAINED.items()},'retained_files':RETAINED_FILES,'installed_before':oldfiles,'existing_parent_flags':oldflags,'added_parent_flags':{'A04_SHARED_CAPTURE':'1'},'msdc_baseline_source_commit':BASE_MSDC,'msdc_baseline_sys_sha256':BASELINES['emmc'][1],'msdc_candidate_sys_sha256':packages['emmc']['files']['emmc/mt8167-msdc.sys'],'dma_poll_trace_changed':False,'physical_operations_authorized':False,'planned_reboot_count':1}
 return selected,components,certs,policy

def generate(policy):
 if policy.get('transaction')!=PROFILE or not policy.get('recovery_boot_id') or not policy.get('backup_manifest_sha256'):raise ValueError('返却fresh結合前に実行可能headerを生成してはいけません')
 from engine_deferred_policy import cbytes
 lines=['/* MT8167 codec1026 + MSDC quiet1.8; explicit returned-fresh bound. */','#define DEFERRED_CODEC_MSDC 1','#define DEFERRED_TARGET_COUNT 3','#define DEFERRED_SD_UPDATE 0','#define DEFERRED_GPU_UPDATE 0','#define DEFERRED_BRIGHTNESS_SCOPED_LOG 0','static const struct deferred_pin deferred_pins[3]={']
 for r in TARGETS:
  p=policy['packages'][r];lines.append('{'+str(p['version_u64'])+'ULL,'+str(p['date_filetime'])+'ULL,{'+','.join(cbytes(v) for v in p['files'].values())+'}},')
 lines+=['};','static const struct deferred_pin codec_baselines[3]={']
 for r in TARGETS:
  p=policy['baseline'][r];lines.append('{'+str(p['version_u64'])+'ULL,'+str(p['date_filetime'])+'ULL,{{0},'+cbytes(p['sys_sha256'])+'}},')
 lines+=['};','static const WCHAR *const codec_baseline_infs[3]={'+','.join('L"'+policy['returned_selection'][r]['selected']['InfPath']+'"' for r in TARGETS)+'};','static const WCHAR *const codec_retained_infs[4]={'+','.join('L"'+policy['returned_selection'][r]['selected']['InfPath']+'"' for r in RETAINED)+'};','static const ULONGLONG codec_retained_dates[4]={'+','.join(str(policy['returned_selection'][r]['date_filetime'])+'ULL' for r in RETAINED)+'};','static const ULONGLONG codec_retained_versions[4]={'+','.join(str(version(x[2]))+'ULL' for x in RETAINED.values())+'};','static const BYTE codec_retained_hashes[][32]={'+','.join(cbytes(h) for h in RETAINED_FILES.values())+'};','static const BYTE deferred_client_hashes[1][32]={'+cbytes(policy['clients'][CLIENT])+'};','static const char deferred_token[]="'+hashlib.sha256(json.dumps(policy,sort_keys=True,separators=(',',':')).encode()).hexdigest()+'";']
 return '\n'.join(lines)+'\n'

def staging(cmd,policy,backup,previous):
 if not previous or previous.get('selected_service')!='MT8167EngineProbe' or previous.get('audio_mode')!='engine_pcm_retained':raise ValueError('固定Engine再arm媒体が必要です')
 if backup.get('recovery_boot_id')!=policy['recovery_boot_id']:raise ValueError('返却fresh boot不一致')
 actual={x['source_path'].replace('\\','/').lower():x['sha256'].lower() for x in backup['files']}
 for name,item in policy['installed_before'].items():
  if actual.get(('windows/system32/' if name=='ge8300-umd.dll' else 'windows/system32/drivers/')+name)!=item['sha256']:raise ValueError('staging前返却fresh installed hash不一致')
 lines=cmd.splitlines();out=[];i=0;removed=[]
 while i<len(lines):
  line=lines[i]
  if re.fullmatch(r'call :adddriver [A-Z]+ .+',line) or line.startswith('copy /y X:\\a04br3\\shared-fw\\'):
   if i+1>=len(lines) or lines[i+1]!='if errorlevel 1 goto dismfail':raise ValueError('固定stage template不一致')
   removed.append(line);i+=2;continue
  out.append(line);i+=1
 if len([x for x in removed if x.startswith('call :adddriver ')])!=5:raise ValueError('固定5DISM削除契約不一致')
 return '\n'.join(out)+'\n',{'profile':PROFILE,'offline_driver_policy':'no DISM or installed SYS copy; only Engine staging and native service SYSTEM','updated_hardware_ids':[x[0] for x in TARGETS.values()],'retained_files':policy['retained_files'],'planned_reboot_count':1}

if __name__=='__main__':
 import argparse
 ap=argparse.ArgumentParser();ap.add_argument('--package-map',type=Path,required=True);ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--draft',action='store_true');ap.add_argument('--fresh-manifest',type=Path);ap.add_argument('--fresh-manifest-sha256');ap.add_argument('--binding-receipt',type=Path);ap.add_argument('--binding-receipt-sha256')
 a=ap.parse_args();root=a.workspace.resolve(strict=True);out=a.output.resolve()
 if not out.is_relative_to(root) or out.exists():raise ValueError('root内新規出力が必要です')
 spec=json.loads(a.package_map.read_text(encoding='utf-8-sig'))
 if a.draft:
  if any((a.fresh_manifest,a.fresh_manifest_sha256,a.binding_receipt,a.binding_receipt_sha256)):raise ValueError('draftへfreshを結合してはいけません')
 else:
  if not all((a.fresh_manifest,a.fresh_manifest_sha256,a.binding_receipt,a.binding_receipt_sha256)):raise ValueError('返却最新manifestとnative receiptの明示引数/full SHA256が必要です')
  if str(a.fresh_manifest.resolve(strict=True))!=str(Path(spec['backup_manifest']).resolve(strict=True)) or a.fresh_manifest_sha256!=spec['backup_manifest_sha256'] or str(a.binding_receipt.resolve(strict=True))!=str(Path(spec['bindings']['path']).resolve(strict=True)) or a.binding_receipt_sha256!=spec['bindings']['sha256']:raise ValueError('明示引数と固定mapの返却freshが不一致です')
 _,_,_,policy=load(a.package_map.resolve(strict=True),root,draft=a.draft)
 out.mkdir();(out/('draft-policy.json' if a.draft else 'deferred-policy-manifest.json')).write_text(json.dumps(policy,indent=2)+'\n')
 if not a.draft:(out/'engine-deferred-policy.h').write_text(generate(policy))
