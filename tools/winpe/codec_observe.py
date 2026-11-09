"""fresh92専用の一回限り観測。driver更新や一般的な同版retryは許可しない。"""
import hashlib,json,re
from pathlib import Path
from private_hardware_probe_input import sha
PROFILE='codec-render-observe-fresh92-v1'
FRESH='4d88127dbbcba9fbd5cb1f6a7eee5926cf6a969b1611c495b040086dd1b33320'
BINDINGS='f8c96cc344f1723cabd0a1f66c5f3b06812c6bdcdb695b17c671152274fa74a6'
OLD_MEDIA='c804720aaeec6371c3791d05df37b597e08a9c6ea95d4ffb7f2e43da1c47952a'
OLD_SERVICE='66184c3f65bf6f71a7ea926bdba2bbc09da1c17b1fa9557c68fea2d2bee121ee'
CLIENTS=(('capture_client','mt8167-capture-probe.exe','61106acf7ae7cfd7831f09dc0a37ead841221d73','25373f54f26098e91b3a5911eaabcc8954d340820c9e123ba12558a50f2c1c26','974518d0377995c74421e0b1000eea87dec30e1b2ea354a12cd98182da1db1cf','development.cer'),('gpu_client','ge8300-d3d-clear.exe','dcb21bc86fdf7f123cacc43f925af9c7d1739dfa','5c5d0d05d8c8665650f63aafeaf3c5e420fbdbcad12eeb28ee74380e73a1b660','d05036edd59c20feb4ad8625e92ade35810ab02e42b87cac13535b3694b8d052','mt8167-development.cer'))
def check_input(spec,root):
 from codec_returned_baseline import pinned_file
 if spec.get('backup_manifest_sha256')!=FRESH or spec.get('bindings',{}).get('sha256')!=BINDINGS:raise ValueError('ObserveOnlyは明示返却fresh92/7nodeに限定')
 pinned_file({'path':spec['backup_manifest'],'sha256':FRESH},root);pinned_file(spec['bindings'],root)
def load_clients(spec,root):
 from codec_returned_baseline import pinned_file
 from engine_io_package import require_armnt
 out=[]
 for role,name,source,manifest,digest,certname in CLIENTS:
  entry=spec[role]
  if set(entry)!={'directory','manifest_sha256'} or entry['manifest_sha256']!=manifest:raise ValueError('ObserveOnly固定署名client manifest不一致')
  d=Path(entry['directory']).resolve(strict=True)
  mf=pinned_file({'path':str(d/'signed-client-manifest.json'),'sha256':manifest},root)
  m=json.loads(mf.read_text(encoding='utf-8-sig'));exe=d/name;cert=d/certname
  require_armnt(exe)
  if sha(exe)!=digest or m.get('source_commit')!=source or hashlib.sha1(cert.read_bytes()).hexdigest().upper()!='BCBF6C456C6F0DD0818434763B189488FBF13570':raise ValueError('固定client source/EXE/cert不一致')
  if role=='gpu_client' and (m.get('fixed_arguments')!='--hardware=PowerVR' or m.get('external_timeout_ms')!=120000 or m.get('retry_count')!=0 or m.get('signature_status')!='Valid' or m.get('signed_sha256')!=digest or sha(d/'client-build-manifest.json')!=m.get('build_manifest_sha256')):raise ValueError('GPU loaderclient契約不一致')
  if role=='capture_client' and (m.get('driver_sources_unchanged') is not True or m.get('drivers_rebuilt') is not False or m.get('signer')!='BCBF6C456C6F0DD0818434763B189488FBF13570' or not any(x=={'path':name,'sha256':digest} for x in m['files'])):raise ValueError('capture client-only契約不一致')
  out.append((exe,cert,manifest))
 return out

def check_service(report):
 from codec_msdc_transaction import TARGETS
 d=report['deferred_update'];p=report['pnp_rebind'];defs=report['build_defines']
 expected={'api':'none','fixed_hardware_ids':[x[0] for x in TARGETS.values()],'directory':'System32\\MT8167EngineProbe\\drivers','maximum_calls_per_id':0,'parent_updated':False,'parent_policy':'verify-selected-version-and-installed-bytes-only','reboot_required_stops_clients':True,'catalog_signature_required':True,'problem_status':'CM_Get_DevNode_PropertyW/DEVPKEY_Device_ProblemStatus/DEVPROP_TYPE_NTSTATUS','phases':['observe-once'],'live_parent_replacement':False,'planned_reboot_count':0}
 if p!=expected or d.get('transaction')!=PROFILE or d.get('mode')!='engine_fixed_observe_v1' or d.get('planned_reboot_count')!=0 or d.get('pnp_install_count')!=0 or d.get('backup_manifest_sha256')!=FRESH or set(d.get('packages',{}))!=set(TARGETS) or defs.get('A04_ENGINE_DEFERRED_UPDATE')!='1' or defs.get('A04_ENGINE_CODEC_MSDC')!='1':raise ValueError('ObserveOnly compile/profile/install0/reboot0契約不一致')
 if d['clients']!={x[1]:x[4] for x in CLIENTS}:raise ValueError('固定2client hash不一致')

def rearm_binding(previous_path,previous,backup_path,backup,following,next_probe):
 from engine_rearm import package_state,STATE_FILES
 if sha(previous_path)!=OLD_MEDIA or sha(backup_path)!=FRESH or next_probe is None:raise ValueError('ObserveOnly再armにはrun91媒体/fresh92/newserviceが必要')
 check_service({'deferred_update':next_probe['deferred_update'],'pnp_rebind':next_probe['pnp_rebind'],'build_defines':{'A04_ENGINE_DEFERRED_UPDATE':'1','A04_ENGINE_CODEC_MSDC':'1'}})
 before=package_state(previous['components']);after=package_state(following)
 if before!=after:raise ValueError('ObserveOnlyでdriver component変更禁止')
 entries={x['source_path'].replace('\\','/'):x for x in backup['files']}
 if previous['probe']['exe_sha256']!=OLD_SERVICE or entries[STATE_FILES[2]]['sha256']!=OLD_SERVICE or next_probe['exe_sha256']==OLD_SERVICE:raise ValueError('既知消費済みservice/新ObserveOnly不一致')
 return {'kind':PROFILE,'maximum_rearms':1,'pnp_install_count':0,'planned_reboot_count':0,'client_sha256':{x[1]:x[4] for x in CLIENTS},'backup_manifest_sha256':FRESH,'previous_service_exe_sha256':OLD_SERVICE,'next_service_exe_sha256':next_probe['exe_sha256']}

def staging(cmd,policy,backup,previous):
 from codec_msdc_transaction import staging as legacy
 if policy.get('planned_reboot_count')!=0 or policy.get('pnp_install_count')!=0 or policy.get('backup_manifest_sha256')!=FRESH:raise ValueError('ObserveOnly staging pins不一致')
 cmd,info=legacy(cmd,policy,backup,previous)
 # No software trust or firmware changes: identical cert already installed by run91.
 start=cmd.index('reg.exe load HKLM\\Mt8167IoOffline ');end=cmd.index('copy /y X:\\a04br3\\dism-*.txt',start)
 cmd=cmd[:start]+cmd[end:]
 start=cmd.index(':adddriver\n');end=cmd.index(':trustfail\n',start)
 cmd=cmd[:start]+cmd[end:]
 if re.search(r'(?im)^(call :adddriver|dism\.exe|reg\.exe (load|add))',cmd):raise ValueError('ObserveOnly servicing/trust書込み残存')
 info.update(profile=PROFILE,updated_hardware_ids=[],observed_hardware_ids=[x[0] for x in __import__('codec_msdc_transaction').TARGETS.values()],planned_reboot_count=0,pnp_install_count=0,driver_files_changed=False)
 return cmd,info
