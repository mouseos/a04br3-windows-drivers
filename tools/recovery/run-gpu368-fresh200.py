#!/usr/bin/env python3
"""main専用GPU368共通engine。terminal後はWindows保持、回収は明示再開。"""
import argparse,hashlib,importlib.util,json,re,subprocess,time
from pathlib import Path

PROFILE='combined-features-fresh200-gpu368-v1'
SERVICE='MT8167CombinedFollowup200'
# 実行条件は既知の二組だけ。任意profile/run/serviceへの転用は許可しない。
KNOWN_PROFILES={
 'run201':dict(run=201,fresh=200,capture=202,profile=PROFILE,service=SERVICE),
 'run203':dict(run=203,fresh=202,capture=204,profile='combined-features-fresh202-gpu368-v1',service='MT8167CombinedFollowup202'),
 'run222-combined':dict(run=222,fresh=221,capture=223,profile='combined-features-fresh221-gpu376-battery18-wifi1031-client375-v1',service='MT8167CombinedFollowup221',client373=True,client_version=375,combined375=True),
 'run222':dict(run=222,fresh=221,capture=223,profile='combined-features-fresh221-gpu375-clients-v1',service='MT8167CombinedFollowup221',client373=True,client_version=375),
 'run219':dict(run=219,fresh=218,capture=220,profile='combined-features-fresh218-gpu374-clients-v1',service='MT8167CombinedFollowup218',client373=True,client_version=374),
 'run216':dict(run=216,fresh=215,capture=217,profile='combined-features-fresh215-gpu373-clients-v1',service='MT8167CombinedFollowup215',client373=True),
 'run213':dict(run=213,fresh=212,capture=214,profile='combined-features-fresh212-gpu371-output-v1',service='MT8167CombinedFollowup212',output371=True),
 'run209':dict(run=209,fresh=208,capture=210,profile='combined-features-fresh208-gpu369-timing-v1',service='MT8167CombinedFollowup208',timing=True),
}
MATCHED_BOOT='98de46d30349379669c4a7300d54d7652b40883f01febb10c5b65992a987e322'
MATCHED_FD='c731f8d68ed39c7a812b427d181468123c58b040fe07fbc666bd8b23e6f0b326'
# Recovery has no awk. A failing bare adb command stops the outer set -e shell;
# negated grep pipelines must not be used as write guards.
MOUNT_GUARD="""adb shell 'set -e
rows=0
while read -r dev mp fs opts dump pass; do
 rows=$((rows+1)); test -n "$pass"
 case "$fs" in *ntfs*|*fuseblk*) exit 1;; esac
 case "$dev" in /dev/block/mmcblk0p17|/dev/block/mmcblk0p18|/dev/block/by-name/boot|/dev/block/by-name/EFI) exit 1;; esac
done < /proc/mounts
test "$rows" -gt 0
rows=0
while read -r mid parent major rest; do
 rows=$((rows+1)); test -n "$rest"
 case "$major" in 179:17|179:18|179:20) exit 1;; esac
done < /proc/self/mountinfo
test "$rows" -gt 0'
"""
def contract(name):
 require(name in KNOWN_PROFILES,'未知のcontroller profile')
 k=dict(KNOWN_PROFILES[name]);k.update(
  schema=('GPU375_COMBINED' if k.get('combined375') else ('GPU%d'%k.get('client_version',373)) if k.get('client373') else 'GPU371' if k.get('output371') else 'GPU369' if k.get('timing') else 'GPU368')+'_RUN%d_MAIN_SEAL_V1'%k['run'],transaction='combined-run%d'%k['run'],
  capture_output='port/xnu/out/private-windows-rt-prebackup-run%d'%k['capture'],
  diagnostics_output='port/xnu/out/private-windows-rt-%s-run%d-diagnostics'%(('gpu%d'%k.get('client_version',373)) if k.get('client373') else 'gpu371' if k.get('output371') else 'gpu369' if k.get('timing') else 'gpu368',k['run']),
  runtime_output='port/gsi/logs/ios-feasibility/windows-rt-combined-run%d-runtime'%k['run'])
 return k

def resume_paths(k):
 # One fixed retry namespace; the original run222 outputs/latch stay immutable.
 require(k.get('combined375') is True,'限定combined222 resumeのみ')
 k=dict(k)
 for key in ('runtime_output','capture_output','diagnostics_output'):k[key]+='-resume223'
 return k

def rebind_module():
 sp=importlib.util.spec_from_file_location('rebind223',Path(__file__).with_name('combined222_rebind223.py'))
 module=importlib.util.module_from_spec(sp);sp.loader.exec_module(module);return module

STORAGE_TRACE=rb'A04WIN (?:StartIo fn=[0-9A-F]{8} opcode=[0-9A-F]{8}|blocks lba=[0-9A-F]{8} count=[0-9A-F]{8}|CDB=[0-9A-F]{8} status=[0-9A-F]{8})\r?\n'
PB=re.compile(rb'PB ([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8})')
COLLECTABLE={'diagnostic-terminal-await-disk-verification','install-failed-await-disk-verification','runtime-exception-await-collection','unexpected-second-runtime-restart','runtime-restarted-requires-install-disk-proof'}
TARGET_SYS=('mt8167-keypad','mt6392-keys','lc709203f','mt8167-display-transport','mt8167-display','mt8167-ge8300','ge8300-render','mt8167-shared','mt8167-audio','mt8167-musb-hcd','mt8167-usb20','mt6630-runtime','mt6630-combo-runtime','mt6630-combo-bluetooth','mt8167-toprgu','mt8167-msdc','mt8167-sd','mma8652','mt6392-rtc','mt8167-thermal')
def require(ok,message):
 if not ok:raise ValueError(message)
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def rooted(root,path):
 p=(root/path).resolve(strict=True);require(p.is_relative_to(root),'root外入力');return p

def stage_preflight(root,c,out,digest):
 """Read-only checks precede the execution latch; a stale snapshot writes nothing."""
 out.parent.mkdir(parents=True,exist_ok=True)
 log=out.parent/(out.name+'-preflight-'+digest[:12]+'.log')
 body='''set -euo pipefail
source "$1"
[ "$rt_target_serial" = "$5" ]
[ "$(adb get-state)" = recovery ]
[ "$(adb shell id -u | tr -d '\\r')" = 0 ]
'''+MOUNT_GUARD+'''python3 -B "$2" verify --serial "$rt_target_serial" --manifest "$3" --manifest-sha256 "$4"
adb shell 'set -e; test "$(readlink -f /dev/block/by-name/boot)" = /dev/block/mmcblk0p17; test "$(cat /sys/class/block/mmcblk0p17/start)" = 294912; test "$(cat /sys/class/block/mmcblk0p17/size)" = 65536; test "$(readlink -f /dev/block/by-name/EFI)" = /dev/block/mmcblk0p18; test "$(cat /sys/class/block/mmcblk0p18/start)" = 360448; test "$(cat /sys/class/block/mmcblk0p18/size)" = 524288'
'''
 with log.open('ab') as f:
  subprocess.run(['bash','-c',body,'ge8300-stage',str(root/c['target_tools']),str(root/c['capture_tool']),str(root/c['fresh_manifest']),c['fresh_manifest_sha256'],c['serial']],cwd=root,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)

def validate_seal(root,path,digest,known_profile="run201",entrypoint=None):
 k=contract(known_profile);service=k["service"]
 require(re.fullmatch('[0-9a-f]{64}',digest or '') and sha(path)==digest,'main seal SHA不一致')
 c=read(path)
 if c.get('baseline_reject_resume'):
  require(known_profile=='run222-combined','他候補へのresume転用不可');k=resume_paths(k)
 require(c['schema']==k['schema'] and c['status']=='validated-main-seal' and c['transaction']==k['transaction'],'確定main sealのみ')
 require(c['target_tools']=='port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh' and c['reset_script']=='unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh','既target/reset実装のみ')
 require(c['boot_written'] is False and c['gpt_written'] is False and c['hardware_success_claimed'] is False,'sealは実行前状態、GPT変更/成功主張は禁止')
 require(c['service_name']==c['data_directory']==service and re.fullmatch('[A-Za-z0-9_-]{1,100}',c['gpu_log_directory']),'service/data directory不一致')
 require(c['planned_windows_reboots'] in ((0,1) if k.get('client373') else (1,)) and c['GPU_client_index']==0 and c['globaldiag_client_index']==1 and c['diagnostic_client_count']==2,'render install/reboot1＋Present1/helper1のみ')
 require(c['install_timeout_seconds']==900 and c['diagnostic_boot_wait_seconds']==180 and c['diagnostic_timeout_seconds']==1140 and c['terminal_settle_seconds'] in range(3,46),'固定timeout契約不一致')
 pins={}
 for row in c['pinned_files']:
  p=rooted(root,row['path']);require(re.fullmatch('[0-9a-f]{64}',row['sha256']) and row['path'] not in pins and sha(p)==row['sha256'],'sealed leaf不一致');pins[row['path']]=row['sha256']
 def pinned(key):
  p=rooted(root,c[key]);require(p.relative_to(root).as_posix() in pins,'sealに必要tool/input pinがない: '+key);return p
 for key in ('capture_tool','crc_decoder','diagnostics_collector','ntfs3g','reset_script','target_tools','fresh_manifest','normal_esp','controller_path'):pinned(key)
 require(pinned('controller_path')==Path(entrypoint or __file__).resolve(),'別controllerのseal')
 if known_profile in ('run203','run209','run213','run216','run219','run222','run222-combined'):
  require(pinned('controller_engine')==Path(__file__).resolve(),'共通controller engine pin不一致')
  require(pinned('collector_present_helper')==Path(__file__).resolve().with_name('collect-present356.py'),'collector helper pin不一致')
 require(type(c.get('gpu_runtime_assets',False)) is bool,'Explicit optional runtime collection flag')
 if c.get('gpu_runtime_assets',False):require(pinned('collector_gpu_runtime_helper')==Path(__file__).resolve().with_name('collect-gpu-runtime-assets.py'),'Runtime collector helper pin required')
 source=rooted(root,c['source_tree']);media=rooted(root,c['media_directory']);m=read(media/'manifest.json');p=m['policy']
 require((media/'manifest.json').relative_to(root).as_posix() in pins and sha(media/'manifest.json')==c['media_manifest_sha256'],'media manifest pin不一致')
 require(sha(media/'update.wim')==m['wim_sha256']==c['wim_sha256'],'WIM pin不一致')
 require(sha(media/'update-esp.img')==m['esp_sha256']==c['esp_sha256'] and sha(pinned('normal_esp'))==c['normal_esp_sha256'],'ESP pin不一致')
 expected_targets=['display','render','wifi','battery'] if k.get('combined375') else (['wifi'] if c.get('wifi_activation') is True else []) if k.get('client373') else ['render'] if k.get('output371') else (['display','render','wifi'] if c.get('wifi_activation') is True else ['display','render']) if k.get('timing') else ['render']
 require(p['profile']==k['profile'] and p['service']==p['data_directory']==service and p['install_targets']==expected_targets and p['planned_reboots']==c['planned_windows_reboots'],'既知freshと更新対象の結合必須')
 require(p['backup_manifest_sha256']==c['fresh_manifest_sha256'] and p['capture_selection']['capture_number']==k['fresh'],'選択fresh結合不一致')
 if k.get('combined375'):
  ms=importlib.util.spec_from_file_location('combined375',Path(__file__).with_name('prepare-gpu375-combined-inputs.py'));variant=importlib.util.module_from_spec(ms);ms.loader.exec_module(variant)
  require(pinned('generator_path')==Path(__file__).resolve().with_name('prepare-gpu375-combined-inputs.py'),'明示combined generator pin')
  variant.validate_runtime_policy(root,p)
  for ref in p['input_pins']:require(pins.get(ref['path'])==ref['sha256'],'combined実入力closureのseal pin欠落')
  require(p['gpu375_combined_contract']['input']['path']==c['private_map'] and pins.get(c['private_map'])==p['gpu375_combined_contract']['input']['sha256'],'実combined map seal結合')
  require(c['planned_windows_reboots']==1 and c['boot_write_required'] is False and c['matched_boot_sha256']==MATCHED_BOOT and not c.get('wifi_activation',False),'combined restart1/boot0。旧wifi-only flag不可')
 elif k.get('client373'):
  import sys
  sys.path.insert(0,str(Path(__file__).resolve().parent))
  from gpu373_client_admission import admit,validate_runtime_policy
  require(pinned('generator_path')==Path(__file__).resolve().with_name('prepare-gpu%d-client-inputs.py'%k.get('client_version',373)),'固定373 generator pin')
  transition=p['gpu373_client_contract'];validate_runtime_policy(p,transition)
  if k.get('client_version') in (374,375):require(c['planned_windows_reboots']==0 and not c.get('wifi_activation',False) and p['install_targets']==[],'374 client-only/no restart')
  require(c['boot_write_required'] is False and c['matched_boot_sha256']==MATCHED_BOOT,'373 boot/GPT書込み0')
  require(transition['runtime']['planned_reboots']==c['planned_windows_reboots'] and bool(p['install_targets'])==c.get('wifi_activation',False),'wifi-only restart mode一致')
  # Every original admission input remains immutable and sealed; no flat manifest adapter.
  actual=admit(root,read(pinned('private_map')))
  require(actual==transition,'373実signed pair/capture/source admission再検証')
  for ref in (actual['capture'],actual['native'],actual['supplement']):require(pins.get(ref['path'])==ref['sha256'],'実215入力seal pin')
  for row in actual['client_replacements'].values():
   for key in ('candidate','manifest','import_proof'):require(pins.get(row[key]['path'])==row[key]['sha256'],'実署名pair seal pin')
 elif k.get('output371'):
  module_spec=importlib.util.spec_from_file_location('gpu371_fixed_inputs',Path(__file__).with_name('prepare-gpu371-output-inputs.py'))
  variant=importlib.util.module_from_spec(module_spec);module_spec.loader.exec_module(variant)
  require(pinned('generator_path')==Path(__file__).resolve().with_name('prepare-gpu371-output-inputs.py'),'固定371 generator pin必須')
  require(c['boot_write_required'] is False and p['boot_transition']['boot_write_required'] is False and p['boot_transition']['gpt_written'] is False,'371 boot/GPT書込み0')
  require(not c.get('wifi_activation',False) and not p.get('wifi_activation',False),'371はWiFi再DIF不可')
  def pin_ref(ref):
   q=rooted(root,ref['path']);require(pins.get(q.relative_to(root).as_posix())==ref['sha256']==sha(q),'371証拠のseal pin欠落/不一致');return q
  gref=p['GPU371_manifest'];require(gref['sha256']==variant.MANIFEST,'固定6496 manifestのみ');gm=read(pin_ref(gref));variant.validate_manifest(gm)
  rref=p['GPU371_handoff'];require(rref['sha256']==variant.REVIEW,'実main371 review pin');read(pin_ref(rref))
  render=p['packages']['render'];display=p['packages']['display'];wifi=p['packages']['wifi']
  require(render['driver_version']=='0.0.3.71' and render['manifest_sha256']==variant.MANIFEST and render['source_commit']==variant.RECIPE and render['hashes']['sys']==variant.RENDER_SYS,'固定371 renderのみ')
  require(display['driver_version']=='0.0.3.69' and display['source_commit']=='2ffc193078da9badbfff67cba2868f34a47b99f8' and wifi['driver_version']=='2026.11.11.1029' and wifi['source_commit']==variant.m.WIFI_SOURCE and wifi['manifest_sha256']==variant.m.WIFI_MANIFEST and wifi['hashes']['sys']=='70253172537b5600e23a7af761ebced1155db4ae2515d2701ba2665d70b231bc','実display369/WiFi1029保持')
  for kind,n in display['names'].items():require(display['hashes'][kind]==next(x['sha256'] for x in gm['outputs'] if x['name']=='display-transport/'+n),'実371 packageが保持したtransport369 trioのみ')
  for role in ('display','wifi'):
   entry=p['packages'][role];name='Windows/System32/Drivers/'+entry['names']['sys'];require(p['preserved_files'].get(name)==entry['hashes']['sys'] and p['baselines'][role]['mode']=='retained','保持SYS/選択baselineの省略不可')
  require(p['baselines']['render']['mode']=='selected' and p['baselines']['render']['sys_sha256']==variant.BASE_RENDER_SYS and 'Windows/System32/Drivers/ge8300-render.sys' not in p['preserved_files'],'renderだけselected更新')
  witness=read(pin_ref(p['boot_witness']))
  require(c['matched_boot']==witness['host_artifact']['path'] and c['matched_boot_manifest']==witness['firmware_composition_manifest']['path'],'保持host boot/manifest参照一致')
  evidence=variant.validate_boot_retention(witness,c['fresh_manifest_sha256'],c['recovery_boot_id'],lambda ref:read(pin_ref(ref)),lambda ref:pin_ref(ref))
  require(evidence==p['boot_witness_evidence'] and c['matched_boot_sha256']==p['boot_transition']['boot_sha256']==MATCHED_BOOT,'保持provenanceとboot98de一致')
 elif k.get('timing'):
  transition=p['boot_transition'];boot=pinned('matched_boot');manifest=read(pinned('matched_boot_manifest'))
  require(c['boot_write_required'] is True and transition['boot_write_required'] is True and transition['gpt_written'] is False,'timingはmatched bootのみ必須、GPT禁止')
  require(sha(boot)==c['matched_boot_sha256']==transition['boot_sha256']==manifest['boot_img_sha256']==MATCHED_BOOT and boot.relative_to(root).as_posix()==transition['boot_path'],'新boot98deの実byte/出自結合')
  require(manifest['fd_sha256']==MATCHED_FD and manifest['uefi_source_commit']=='a82e2713b95e4d272a22683851f876c170dcb3da' and manifest['firmware_build_defines']['A04BR3_PANEL_TIMING_HANDOFF']=='1','DTM0付きFDのみ')
  require(manifest['preserved_shim_defines']['A04BR3_FOUR_CPU_L1_CLEAN']==1 and manifest['baseline_reproduced_byte_identically'] is True,'実0943のL1 cleanと再現出自を保持')
  require(0<boot.stat().st_size<=33554432 and boot.open('rb').read(8)==b'ANDROID!','boot Android形式/容量')
  require(p['packages']['display']['driver_version']==p['packages']['render']['driver_version']=='0.0.3.69','timingの両SYS369結合')
  require(p.get('wifi_activation',False)==c.get('wifi_activation',False),'WiFi切替の明示指定不一致')
  if c.get('wifi_activation') is True:
   require(p['packages']['wifi']['source_commit']=='c11981553cc10e9730565390f5d9f9e09a919992' and p['packages']['wifi']['manifest_sha256']=='d974239fbcf96d7baad96b84fccaa7462a7481058a7c2d80c0d19d4745ef1599' and p['packages']['wifi']['driver_version']=='2026.11.11.1029','WiFi1029の既署名候補だけ')
   require(p['baselines']['wifi']['sys_sha256']=='97dcede955d813a7d72ba8d6e950ceae1215c94351187cf18c73cc15d3f47334','実1027からの明示切替だけ')
 else:
  require(p['boot_transition']['boot_write_required'] is False and p['boot_transition']['gpt_written'] is False,'旧368では新boot/GPT操作禁止')
 require(hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()==c['policy_sha256'],'policy semantic hash不一致')
 clients=p['diagnostic_clients'];require(len(clients)==2 and [x['name'] for x in clients]==['gpu_present','gpu_globaldiag'] and all(x['phase']==1 for x in clients),'Present/helper各1回のみ')
 require(clients[0]['timeout_ms']==900000 and clients[1]['timeout_ms']==60000 and p['observer_contract']['absolute_batch_timeout_seconds']==1140,'client timeout不一致')
 fm=read(pinned('fresh_manifest'));require(c['fresh_manifest']=='port/xnu/out/private-windows-rt-prebackup-run%d/manifest.json'%k['fresh'] and sha(pinned('fresh_manifest'))==c['fresh_manifest_sha256'],'旧freshの改名流用は禁止')
 require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[] and fm['recovery_boot_id']==c['recovery_boot_id'],'完了済み実fresh必須')
 require(c['prepare_token']==c['policy_sha256'][:16].upper(),'CRC token不一致')
 require(re.fullmatch('[A-Za-z0-9_-]{1,100}',c['serial']) and c['serial']==fm['serial'],'sealed serial/fresh不一致')
 require(str(subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True)).strip()==c['profile_source_commit'],'profile source commit不一致')
 for key in ('capture_output','diagnostics_output'):
  target=(root/c[key]).resolve();require(target.is_relative_to(root/'port/xnu/out'),'root内private出力のみ')
 require(c['capture_output']==k['capture_output'],'選択capture以外へ転用しない')
 require(c['diagnostics_collector'].endswith('/tools/recovery/collect-combined200.sh'),'既combined200 collectorのみ')
 if known_profile=='run201':
  require((source/'tools/recovery/collect-combined200.sh').resolve()==pinned('diagnostics_collector'),'collector source tree不一致')
 else:
  require(c['diagnostics_output']==k['diagnostics_output'],'run203専用diagnostics出力必須')
  require(Path(__file__).resolve().with_name('collect-combined200.sh')==pinned('diagnostics_collector'),'controller checkoutのcollectorのみ')
 if c.get('baseline_reject_resume'):
  ref=c['baseline_reject_resume'];require(pins.get(ref['path'])==ref['sha256'],'rebind sidecarはmain seal pin必須')
  require(pinned('rebind_helper')==Path(__file__).resolve().with_name('combined222_rebind223.py'),'固定resume validator pin必須')
  side=rebind_module().validate(root,ref,c)
  for row in side['refs']:require(pins.get(row['path'])==row['sha256'],'拒否/再結合証拠seal pin欠落')
 return c,m

class Observation:
 """実機APIを持たないPB状態機械。UART code0をGPU機能成功とは扱わない。"""
 def __init__(self,planned_reboots=1):
  require(planned_reboots in (0,1),'既知restart数のみ');self.planned_reboots=planned_reboots
  self.phase=0 if planned_reboots else 1;self.seen_os=False;self.await_resume=False;self.run=None;self.pid=None;self.flush=None;self.install_3010=False;self.waiter_3010=False;self.restarts=0;self.clients={0:{},1:{}};self.terminal=None
 def line(self,line):
  actions=[]
  if b'ExitBootServices event signaled' in line or b'A04GPU NATIVE D0 ENTER' in line:self.seen_os=True;self.await_resume=False
  event=PB.search(line)
  if event:
   i,e,code=[int(x,16) for x in event.groups()]
   if e==0x286 and i==0:
    require(self.seen_os and self.run is None,'同bootのservice START重複/未結合');self.run=code;actions.append(('service-start',code))
   elif e==0x287 and i==0:
    require(self.run is not None and self.pid is None and code>0,'service PID重複/未結合');self.pid=code
   else:
    require(self.seen_os and self.run is not None and self.pid is not None,'current boot/run/PIDに結合できないPB')
    if e==0x288 and i==0:self.flush=code
    if self.phase==1 and e in (0x220,0x221) and i in (0,1):
     row=self.clients[i]
     if e==0x220:require('pid' not in row and code>0,'同clientの再実行禁止');row['pid']=code;actions.append(('client-start',i))
     else:require('result' not in row,'client result重複');row['result']=code;actions.append(('client-end',i))
    if e==0x10b and i==0 and code==3010:
     require(self.phase==0 and self.install_3010 and self.flush==0,'reboot waiter証拠不一致');self.waiter_3010=True
    if e==0x207 and i==0:
     require(self.terminal is None,'terminal重複')
     if self.flush!=0:actions.append(('unproven-terminal',code))
     elif self.phase==0 and code==3010:self.install_3010=True
     else:
      if self.phase==1 and code==0:require(all(row.get('pid',0)>0 and row.get('result')==0 for row in self.clients.values()),'成功terminalにPresent/helper完了証拠がない')
      self.terminal=dict(phase=self.phase,code=code,run=self.run,PID=self.pid,flush=self.flush,clients=self.clients)
      actions.append(('terminal',code))
   actions.append(('event',dict(phase=self.phase,id=i,event=e,code=code,run=self.run,PID=self.pid)))
  if self.seen_os and b'BOOT_REASON' in line:
   self.restarts+=1;self.seen_os=False
   if self.phase!=0 or self.restarts!=1:actions.append(('unexpected-restart',self.restarts))
   elif not(self.install_3010 and self.waiter_3010 and self.flush==0):actions.append(('unproven-restart',1))
   else:self.phase=1;self.await_resume=True;self.run=self.pid=self.flush=None;actions.append(('planned-restart',1))
  if b'fastboot: processing commands' in line and self.await_resume and not self.seen_os:actions.append(('resume',1));self.await_resume=False
  if re.search(rb'A04(?:GPU|WIN) BUGCHECK(?: [0-9A-F]{8}| DATA_UNAVAILABLE)',line) or b'Unhandled Exception' in line:actions.append(('exception',1))
  return actions

TERMINAL_BOUNDARIES={'diagnostic-terminal-await-disk-verification','install-failed-await-disk-verification'}
HOLD_STATUS='terminal-windows-held'
def candidate_identity(c):
 return hashlib.sha256((c['fresh_manifest_sha256']+c['policy_sha256']+c['esp_sha256']).encode()).hexdigest()
def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'))
def replay_boundary(raw,planned_reboots=1):
 """保存UARTから同boot/run/PID/flushを再検証。新たな実機アクセスはしない。"""
 observer=Observation(planned_reboots)
 for line in raw.split(b'\n'):
  for action,value in observer.line(line):
   if action=='exception':return observer.terminal,'runtime-exception-await-collection'
   if action=='unexpected-restart':return observer.terminal,'unexpected-second-runtime-restart'
   if action=='unproven-restart':return observer.terminal,'runtime-restarted-requires-install-disk-proof'
   require(action!='unproven-terminal','flush未確認のterminalは回収許可にしない')
 # An exception marker can arrive before the final newline.
 if re.search(rb'A04(?:GPU|WIN) BUGCHECK(?: [0-9A-F]{8}| DATA_UNAVAILABLE)',raw) or b'Unhandled Exception' in raw:
  return observer.terminal,'runtime-exception-await-collection'
 return observer.terminal,None

def hold_terminal(out,st,c,digest):
 terminal,danger=replay_boundary((out/'normal-uart-deinterleaved.log').read_bytes(),c['planned_windows_reboots'])
 if danger:
  st.update(status=danger,safe_flush_terminal=False,late_runtime_fault_observed=True);return False
 require(st['status'] in TERMINAL_BOUNDARIES and terminal is not None and
         canonical(terminal)==canonical(st.get('terminal')) and terminal['flush']==0,
         'terminalと保存UARTのboot/run/PID/flushが一致しない')
 proof=dict(schema='GPU368_TERMINAL_HOLD_V1',transaction=st['transaction'],candidate=candidate_identity(c),
            seal_sha256=digest,collection_boundary=st['status'],terminal=terminal,
            uart_sha256=sha(out/'normal-uart-deinterleaved.log'))
 with (out/'terminal-proof.json').open('x') as f:json.dump(proof,f,indent=2);f.write('\n')
 st.update(status=HOLD_STATUS,collection_boundary=proof['collection_boundary'],safe_flush_terminal=True,
           terminal_proof_sha256=sha(out/'terminal-proof.json'),hardware_success=False)
 return True

def verify_collection(out,c,digest,k,latch):
 """既実行のheld境界だけを再開。latch/result/proof/UARTを一切変更しない。"""
 st=read(out/'result.json');receipt=read(latch);proof=read(out/'terminal-proof.json')
 require(st['status']==HOLD_STATUS and st['transaction']==k['transaction'] and
         st['seal_sha256']==digest and st.get('safe_flush_terminal') is True,'回収可能なheld resultがない')
 require(receipt['candidate']==candidate_identity(c) and receipt['seal_sha256']==digest and
         Path(receipt['output']).resolve()==out,'同candidateの既実行latchがない')
 require(sha(out/'terminal-proof.json')==st['terminal_proof_sha256'] and
         proof['schema']=='GPU368_TERMINAL_HOLD_V1' and proof['transaction']==k['transaction'] and
         proof['seal_sha256']==digest and proof['candidate']==receipt['candidate'],'terminal proof不一致')
 require(proof['collection_boundary'] in TERMINAL_BOUNDARIES and
         st['collection_boundary']==proof['collection_boundary'] and
         sha(out/'normal-uart-deinterleaved.log')==proof['uart_sha256'],'保存terminal UART変更検出')
 terminal,danger=replay_boundary((out/'normal-uart-deinterleaved.log').read_bytes(),c['planned_windows_reboots'])
 require(not danger and terminal is not None and terminal['flush']==0 and
         canonical(terminal)==canonical(proof['terminal'])==canonical(st['terminal']),
         '同boot/run/PID/flushのterminal再検証に失敗')
 require(proof['collection_boundary']==('diagnostic-terminal-await-disk-verification' if terminal['phase'] else 'install-failed-await-disk-verification'),'terminal phase不一致')
 return st

def main(known_profile="run201",entrypoint=None,argv=None):
 k=contract(known_profile)
 ap=argparse.ArgumentParser();ap.add_argument('--resume-baseline-reject',action='store_true',help='固定223 rebind sidecar付きsealによる未arm拒否だけの1回resume');ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--seal',type=Path,required=True);ap.add_argument('--seal-sha256',required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--validate-only',action='store_true');ap.add_argument('--collect-only','--resume-collect',dest='collect_only',action='store_true',help='held terminalから回収のみ。install/診断を再実行しない');a=ap.parse_args(argv)
 root=a.workspace.resolve(strict=True);seal=rooted(root,a.seal);c,m=validate_seal(root,seal,a.seal_sha256,known_profile,entrypoint)
 require(bool(c.get('baseline_reject_resume'))==a.resume_baseline_reject,'resume sealは明示--resume-baseline-rejectのみ')
 live_c=c
 if a.resume_baseline_reject:
  k=resume_paths(k);side=rebind_module().validate(root,c['baseline_reject_resume'],c)
  live_c=dict(c,fresh_manifest=side['fresh_manifest'],fresh_manifest_sha256=side['fresh_manifest_sha256'],recovery_boot_id=side['recovery_boot_id'])
 out=a.output.resolve();require(known_profile=='run201' or out==(root/k['runtime_output']).resolve(),'run203専用runtime出力必須')
 if a.validate_only and not a.collect_only:print('GPU%d_SEAL_VALIDATED_NO_DEVICE_OPERATION'%k['run']);return
 require(out.is_relative_to(root),'root内のログ出力必須')
 require(not (root/c['capture_output']).exists() and not (root/c['diagnostics_output']).exists(),'既存capture/diagnosticsを上書きしない')
 if (k.get('timing') or k.get('output371') or k.get('client373')) and not a.collect_only:stage_preflight(root,live_c,out,a.seal_sha256)
 latches=root/('port/gsi/logs/ios-feasibility/%s-run%d-executions'%(('gpu%d'%k.get('client_version',373)) if k.get('client373') else 'gpu371' if k.get('output371') else 'gpu369' if k.get('timing') else 'gpu368',k['run']));latch=latches/(k['transaction']+('-resume223' if a.resume_baseline_reject else '')+'.json')
 if a.collect_only:
  require(out.is_dir(),'既存held runtime出力必須')
  held_out=out;st=verify_collection(out,c,a.seal_sha256,k,latch)
  if a.validate_only:print('GPU%d_HELD_COLLECTION_VALIDATED_NO_DEVICE_OPERATION'%k['run']);return
  out=held_out/'collection'
  # Exclusive output prevents a second collector/reset after partial failure.
  out.mkdir(mode=0o700)
  st=dict(st,status=st['collection_boundary'],held_result_sha256=sha(held_out/'result.json'),collection_only=True)
 else:
  require(not out.exists(),'root内の新規ログ出力必須')
  latches.mkdir(exist_ok=True)
  # Global per-candidate latch, not merely a new log directory. No automatic retry.
  with latch.open('x') as f:json.dump(dict(candidate=candidate_identity(c),seal_sha256=a.seal_sha256,output=str(out),started=time.time()),f)
  out.mkdir(mode=0o700)
  st=dict(transaction=k['transaction'],baseline_reject_resume=c.get('baseline_reject_resume'),status='starting',seal_sha256=a.seal_sha256,events=[],planned_runtime_restarts=0,boot_written=False,gpt_written=False,hardware_success=False,same_candidate_reexecution=False)
 uart=None if a.collect_only else rooted(root,'port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log')
 def save():(out/'result.json').write_text(json.dumps(st,indent=2)+'\n')
 def command(body,name,args=(),timeout=400,flash=False):
  require(sha(root/c['target_tools'])==next(x['sha256'] for x in c['pinned_files'] if x['path']==c['target_tools']),'target tools変更検出')
  with (out/name).open('wb') as f:
   try:subprocess.run(['bash','-c','set -euo pipefail\nsource "$1"\n[ "$rt_target_serial" = "'+c['serial']+'" ]\n'+body,'gpu%d'%k['run'],str(root/c['target_tools']),*map(str,args)],cwd=root,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=timeout)
   finally:
    f.flush()
    if flash:
     with (root/'port/gsi/logs/flash.log').open('ab') as log:log.write((out/name).read_bytes())
 def recovery(name):
  command('''if [ "$(adb get-state 2>/dev/null || true)" != recovery ]; then
 if [ -z "$(fastboot devices)" ]; then
  UART=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0 bash "$2"
  for ((i=0;i<90;i++)); do [ -n "$(fastboot devices)" ] && break; sleep 1; done
 fi
 info=$(fastboot getvar product 2>&1); [[ "$info" =~ product:[[:space:]]*A04BR3 ]]
 fastboot oem reboot-recovery
 for ((i=0;i<120;i++)); do [ "$(adb get-state 2>/dev/null || true)" = recovery ] && break; sleep 1; done
fi
[ "$(adb get-state)" = recovery ]; adb root || true
for ((i=0;i<30;i++)); do [ "$(adb shell id -u 2>/dev/null | tr -d '\\r')" = 0 ] && break; sleep 1; done
[ "$(adb shell id -u | tr -d '\\r')" = 0 ]
'''+MOUNT_GUARD,name,(root/c['reset_script'],),flash=True)
 def deploy(image,digest,name,initial=False):
  require(sha(image)==digest,'ESP変更検出');offset=uart.stat().st_size
  gate=('python3 -B "$2" verify --serial "$rt_target_serial" --manifest "$3" --manifest-sha256 '+live_c['fresh_manifest_sha256']+'\n') if initial else ''
  command('''[ "$(adb get-state)" = recovery ]
'''+MOUNT_GUARD+gate+'''esp_tmp=/tmp/gpu"$6"-esp.img
adb push "$4" "$esp_tmp"
[ "$(adb shell sha256sum "$esp_tmp" | cut -d ' ' -f 1)" = "$5" ]
adb shell 'set -e; test "$(id -u)" = 0; test "$(readlink -f /dev/block/by-name/EFI)" = /dev/block/mmcblk0p18; test "$(cat /sys/class/block/mmcblk0p18/start)" = 360448; test "$(cat /sys/class/block/mmcblk0p18/size)" = 524288'
'''+MOUNT_GUARD+'''adb shell "test -b /dev/block/by-name/EFI && dd if=$esp_tmp of=/dev/block/by-name/EFI bs=4M conv=fsync && sync && rm $esp_tmp"
adb reboot
''',name,(root/c['capture_tool'],root/(live_c['fresh_manifest'] if initial else c['fresh_manifest']),image,digest,k['run']),flash=True)
  return offset
 def matched_boot():
  # Same Recovery, already verified before claiming the execution latch.
  # No reboot here: the maintenance ESP must be staged before the next boot.
  image=root/c['matched_boot'];require(sha(image)==c['matched_boot_sha256'],'matched boot変更検出')
  st.update(status='matched-boot-writing',boot_write_attempted=True);save()
  command('''[ "$(adb get-state)" = recovery ]
'''+MOUNT_GUARD+'''adb push "$5" /tmp/gpu369-matched-boot.img
[ "$(adb shell sha256sum /tmp/gpu369-matched-boot.img | cut -d ' ' -f 1)" = "$6" ]
adb shell 'set -e; test "$(id -u)" = 0; test "$(readlink -f /dev/block/by-name/boot)" = /dev/block/mmcblk0p17; test "$(cat /sys/class/block/mmcblk0p17/start)" = 294912; test "$(cat /sys/class/block/mmcblk0p17/size)" = 65536'
'''+MOUNT_GUARD+'''adb shell 'set -e; dd if=/tmp/gpu369-matched-boot.img of=/dev/block/by-name/boot bs=1048576 conv=fsync; sync'
''','matched-boot-write.log',(root/c['capture_tool'],root/c['fresh_manifest'],c['fresh_manifest_sha256'],image,c['matched_boot_sha256']),flash=True)
  st.update(boot_written=True,boot_sha256=c['matched_boot_sha256'],matched_boot_applied_before_maintenance=True,matched_boot_applied_before_normal=True);save()
 try:
  if not a.collect_only:
   save()
   if k.get('timing'):matched_boot()
   offset=deploy(root/c['media_directory']/'update-esp.img',c['esp_sha256'],'prepare-write.log',not (k.get('timing') or k.get('output371') or k.get('client373')));st.update(status='prepare-observing',prepare_uart_offset=offset);save()
   spec=importlib.util.spec_from_file_location('gpu%d_crc'%k['run'],root/c['crc_decoder']);decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
   report=None;deadline=time.monotonic()+900
   with uart.open('rb') as f:
    f.seek(offset);raw=b''
    while time.monotonic()<deadline:
     time.sleep(2);raw+=f.read();(out/'prepare-uart.log').write_bytes(raw)
     if b'Unhandled Exception' in raw:raise RuntimeError('WinPE例外。normal bootへ進まない')
     if re.search(rb'A04AU2E '+c['prepare_token'].encode()+rb' [0-9A-F]{8}\r?\n',raw):report=decoder.decode(raw,c['prepare_token']);break
   require(report is not None,'CRC terminalなし。timeout reset/normal boot禁止')
   (out/'prepare-report.txt').write_bytes(report);lines=report.decode().splitlines();require(all(lines.count(x)==1 for x in ('UPDATE_STATUS=COMMITTED','UPDATE_RELEASE_EXIT=0','UPDATE_EXIT=0')),'CRC内のCOMMITTED/release0/exit0が必要')
   st.update(status='prepared-volume-released',prepare_crc_committed_released=True);save();recovery('prepare-to-recovery.log');offset=deploy(root/c['normal_esp'],c['normal_esp_sha256'],'normal-esp-write.log')
   st.update(status='install-observing',normal_uart_offset=offset);save();observer=Observation(c['planned_windows_reboots']);deadline=time.monotonic()+900;batch_cap=None;finished=False
   with uart.open('rb') as f:
    f.seek(offset);raw=b'';processed=0
    while time.monotonic()<deadline and not finished:
     time.sleep(.25);raw+=f.read();(out/'normal-uart.log').write_bytes(raw);clean=re.sub(STORAGE_TRACE,b'',raw);(out/'normal-uart-deinterleaved.log').write_bytes(clean);parts=clean.split(b'\n')[:-1]
     for line in parts[processed:]:
      for action,value in observer.line(line):
       now=time.monotonic()
       if action=='event':value['host_time']=time.time();st['events'].append(value)
       elif action=='planned-restart':st.update(planned_runtime_restarts=1,status='post-reboot-diagnostic-observing');deadline=now+180
       elif action=='resume':command('info=$(fastboot getvar product 2>&1)\n[[ "$info" =~ product:[[:space:]]*A04BR3 ]]\nfastboot reboot\n','planned-restart-resume.log',timeout=45,flash=True)
       elif action=='service-start' and observer.phase==1:batch_cap=now+1140;deadline=min(batch_cap,now+180)
       elif action=='client-start':require(batch_cap is not None,'diagnostic service未結合');deadline=min(batch_cap,now+(930 if value==0 else 90))
       elif action=='client-end':
        if value==1 and batch_cap is not None:deadline=min(batch_cap,now+60)
       elif action=='terminal':st.update(terminal=observer.terminal,status='diagnostic-terminal-await-disk-verification' if observer.phase else 'install-failed-await-disk-verification');finished=True
       elif action=='exception':st.update(status='runtime-exception-await-collection',runtime_exception_observed=True,safe_flush_terminal=False);finished=True
       elif action in ('unexpected-restart','unproven-restart'):st.update(status='unexpected-second-runtime-restart' if action=='unexpected-restart' else 'runtime-restarted-requires-install-disk-proof',safe_flush_terminal=False);finished=True
       elif action=='unproven-terminal':raise RuntimeError('terminalはあるが同boot flush0未確認。resetせずmain review')
      if finished:break
     processed=len(parts);save()
   require(finished,'観測timeout。実行中を推測resetせず証拠保持')
   if st.get('terminal'):st['safe_flush_terminal']=True;save();time.sleep(c['terminal_settle_seconds'])
   with uart.open('rb') as f:f.seek(offset);drained=f.read()
   (out/'normal-uart.log').write_bytes(drained);(out/'normal-uart-deinterleaved.log').write_bytes(re.sub(STORAGE_TRACE,b'',drained))
   if st['status'] in TERMINAL_BOUNDARIES:
    if hold_terminal(out,st,c,a.seal_sha256):
     save();print('GPU%d_TERMINAL_WINDOWS_HELD_NOT_GPU_FUNCTIONAL_SUCCESS'%k['run']);return
    save()
  save();recovery('terminal-to-recovery.log');st['collection_boundary']=st['status'];save()
  command('''[ "$(adb get-state)" = recovery ]
'''+MOUNT_GUARD+'''adb push "$2" /tmp/rt-ntfs3g
adb shell chmod 755 /tmp/rt-ntfs3g
''','prepare-capture%d.log'%k['capture'],(root/c['ntfs3g'],))
  args=['python3','-B',str(root/c['capture_tool']),'capture','--serial',c['serial'],'--out',str(root/c['capture_output'])]
  for name in TARGET_SYS:args+=['--target-sys',name+'.sys']
  args+=['--allow-missing-sys','--engine-probe-state','--sd-component-state','--allow-missing-engine-service-log','--mount-driver','ntfs-3g','--ntfs3g-remote','/tmp/rt-ntfs3g']
  with (out/('capture%d.log'%k['capture'])).open('wb') as f:subprocess.run(args,cwd=root,stdout=f,stderr=subprocess.STDOUT,check=True)
  capture=read(root/c['capture_output']/'manifest.json');require(capture['status']=='captured-unmounted' and capture['read_only'] is True and capture['unmounted'] is True and capture['errors']==[],'実capture完了未確認')
  collect_args=['bash',str(root/c['diagnostics_collector']),str(seal),str(out/'result.json'),str(root/c['diagnostics_output'])]
  if c.get('gpu_runtime_assets',False):collect_args+=['--gpu-runtime-assets']
  with (out/'collect-combined200.log').open('wb') as f:subprocess.run(collect_args,cwd=root,stdout=f,stderr=subprocess.STDOUT,check=True)
  index=root/c['diagnostics_output']/'capture-index.json';require(read(index)['windows_unmounted'] is True,'diagnostics unmount未確認')
  st.update(status='capture%d-and-diagnostics-ready-for-main-review'%k['capture'],capture_manifest_sha256=sha(root/c['capture_output']/'manifest.json'),diagnostics_index_sha256=sha(index),hardware_success=False);save();print('GPU%d_CAPTURE%d_READY_NOT_GPU_FUNCTIONAL_SUCCESS'%(k['run'],k['capture']))
 except Exception as e:
  if st.get('boot_write_attempted') and not st.get('boot_written'):
   st['boot_written']=None;st['boot_write_state']='unconfirmed; normal boot prohibited until main reviews the write log'
  st.update(prior_status=st['status'],status='pipeline-stopped',error=str(e),hardware_success=False);save();raise
if __name__=='__main__':main()
