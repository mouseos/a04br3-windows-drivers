#!/usr/bin/env python3
"""main専用run201。固定sealから1回だけEFI更新し、実UART境界でcapture202へ進む。"""
import argparse,hashlib,importlib.util,json,re,subprocess,time
from pathlib import Path

PROFILE='combined-features-fresh200-gpu368-v1'
SERVICE='MT8167CombinedFollowup200'
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

def validate_seal(root,path,digest):
 require(re.fullmatch('[0-9a-f]{64}',digest or '') and sha(path)==digest,'main seal SHA不一致')
 c=read(path);require(c['schema']=='GPU368_RUN201_MAIN_SEAL_V1' and c['status']=='validated-main-seal' and c['transaction']=='combined-run201','確定main sealのみ')
 require(c['target_tools']=='port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh' and c['reset_script']=='unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh','既target/reset実装のみ')
 require(c['boot_written'] is False and c['gpt_written'] is False and c['hardware_success_claimed'] is False,'boot/GPT変更と成功主張は禁止')
 require(c['service_name']==c['data_directory']==SERVICE and re.fullmatch('[A-Za-z0-9_-]{1,100}',c['gpu_log_directory']),'service/data directory不一致')
 require(c['planned_windows_reboots']==1 and c['GPU_client_index']==0 and c['globaldiag_client_index']==1 and c['diagnostic_client_count']==2,'render install/reboot1＋Present1/helper1のみ')
 require(c['install_timeout_seconds']==900 and c['diagnostic_boot_wait_seconds']==180 and c['diagnostic_timeout_seconds']==1140 and c['terminal_settle_seconds'] in range(3,46),'固定timeout契約不一致')
 pins={}
 for row in c['pinned_files']:
  p=rooted(root,row['path']);require(re.fullmatch('[0-9a-f]{64}',row['sha256']) and row['path'] not in pins and sha(p)==row['sha256'],'sealed leaf不一致');pins[row['path']]=row['sha256']
 def pinned(key):
  p=rooted(root,c[key]);require(p.relative_to(root).as_posix() in pins,'sealに必要tool/input pinがない: '+key);return p
 for key in ('capture_tool','crc_decoder','diagnostics_collector','ntfs3g','reset_script','target_tools','fresh_manifest','normal_esp','controller_path'):pinned(key)
 require(pinned('controller_path')==Path(__file__).resolve(),'別controllerのseal')
 source=rooted(root,c['source_tree']);media=rooted(root,c['media_directory']);m=read(media/'manifest.json');p=m['policy']
 require((media/'manifest.json').relative_to(root).as_posix() in pins and sha(media/'manifest.json')==c['media_manifest_sha256'],'media manifest pin不一致')
 require(sha(media/'update.wim')==m['wim_sha256']==c['wim_sha256'],'WIM pin不一致')
 require(sha(media/'update-esp.img')==m['esp_sha256']==c['esp_sha256'] and sha(pinned('normal_esp'))==c['normal_esp_sha256'],'ESP pin不一致')
 require(p['profile']==PROFILE and p['service']==p['data_directory']==SERVICE and p['install_targets']==['render'] and p['planned_reboots']==1,'fresh200 render-only profile必須')
 require(p['backup_manifest_sha256']==c['fresh_manifest_sha256'] and p['capture_selection']['capture_number']==200,'fresh200結合不一致')
 require(p['boot_transition']['boot_write_required'] is False and p['boot_transition']['gpt_written'] is False,'新boot/GPT操作は禁止')
 require(hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()==c['policy_sha256'],'policy semantic hash不一致')
 clients=p['diagnostic_clients'];require(len(clients)==2 and [x['name'] for x in clients]==['gpu_present','gpu_globaldiag'] and all(x['phase']==1 for x in clients),'Present/helper各1回のみ')
 require(clients[0]['timeout_ms']==900000 and clients[1]['timeout_ms']==60000 and p['observer_contract']['absolute_batch_timeout_seconds']==1140,'client timeout不一致')
 fm=read(pinned('fresh_manifest'));require(c['fresh_manifest'].endswith('private-windows-rt-prebackup-run200/manifest.json') and sha(pinned('fresh_manifest'))==c['fresh_manifest_sha256'],'旧freshの改名流用は禁止')
 require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[] and fm['recovery_boot_id']==c['recovery_boot_id'],'完了済み実fresh200必須')
 require(c['prepare_token']==c['policy_sha256'][:16].upper(),'CRC token不一致')
 require(re.fullmatch('[A-Za-z0-9_-]{1,100}',c['serial']) and c['serial']==fm['serial'],'sealed serial/fresh不一致')
 require(str(subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True)).strip()==c['profile_source_commit'],'profile source commit不一致')
 for key in ('capture_output','diagnostics_output'):
  target=(root/c[key]).resolve();require(target.is_relative_to(root/'port/xnu/out'),'root内private出力のみ')
 require(c['capture_output']=='port/xnu/out/private-windows-rt-prebackup-run202','capture202以外へ転用しない')
 require(c['diagnostics_collector'].endswith('/tools/recovery/collect-combined200.sh'),'既combined200 collectorのみ')
 require((source/'tools/recovery/collect-combined200.sh').resolve()==pinned('diagnostics_collector'),'collector source tree不一致')
 return c,m

class Observation:
 """実機APIを持たないPB状態機械。UART code0をGPU機能成功とは扱わない。"""
 def __init__(self):
  self.phase=0;self.seen_os=False;self.await_resume=False;self.run=None;self.pid=None;self.flush=None;self.install_3010=False;self.waiter_3010=False;self.restarts=0;self.clients={0:{},1:{}};self.terminal=None
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

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--seal',type=Path,required=True);ap.add_argument('--seal-sha256',required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--validate-only',action='store_true');a=ap.parse_args()
 root=a.workspace.resolve(strict=True);seal=rooted(root,a.seal);c,m=validate_seal(root,seal,a.seal_sha256)
 if a.validate_only:print('GPU201_SEAL_VALIDATED_NO_DEVICE_OPERATION');return
 out=a.output.resolve();require(out.is_relative_to(root) and not out.exists(),'root内の新規ログ出力必須')
 require(not (root/c['capture_output']).exists() and not (root/c['diagnostics_output']).exists(),'既存capture/diagnosticsを上書きしない')
 identity=hashlib.sha256((c['fresh_manifest_sha256']+c['policy_sha256']+c['esp_sha256']).encode()).hexdigest();latches=root/'port/gsi/logs/ios-feasibility/gpu368-run201-executions';latches.mkdir(exist_ok=True)
 # Global per-candidate latch, not merely a new log directory. No automatic retry.
 with (latches/'combined-run201.json').open('x') as f:json.dump(dict(candidate=identity,seal_sha256=a.seal_sha256,output=str(out),started=time.time()),f)
 out.mkdir(mode=0o700);uart=rooted(root,'port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log')
 st=dict(transaction='combined-run201',status='starting',seal_sha256=a.seal_sha256,events=[],planned_runtime_restarts=0,boot_written=False,gpt_written=False,hardware_success=False,same_candidate_reexecution=False)
 def save():(out/'result.json').write_text(json.dumps(st,indent=2)+'\n')
 def command(body,name,args=(),timeout=400,flash=False):
  require(sha(root/c['target_tools'])==next(x['sha256'] for x in c['pinned_files'] if x['path']==c['target_tools']),'target tools変更検出')
  with (out/name).open('wb') as f:
   try:subprocess.run(['bash','-c','set -euo pipefail\nsource "$1"\n[ "$rt_target_serial" = "'+c['serial']+'" ]\n'+body,'gpu201',str(root/c['target_tools']),*map(str,args)],cwd=root,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=timeout)
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
! adb shell cat /proc/mounts | grep -q ntfs
''',name,(root/c['reset_script'],),flash=True)
 def deploy(image,digest,name,initial=False):
  require(sha(image)==digest,'ESP変更検出');offset=uart.stat().st_size
  gate=('python3 -B "$2" verify --serial "$rt_target_serial" --manifest "$3" --manifest-sha256 '+c['fresh_manifest_sha256']+'\n') if initial else ''
  command('''[ "$(adb get-state)" = recovery ]
! adb shell cat /proc/mounts | grep -q ntfs
'''+gate+'''adb push "$4" /tmp/gpu201-esp.img
[ "$(adb shell sha256sum /tmp/gpu201-esp.img | cut -d ' ' -f 1)" = "$5" ]
adb shell 'test -b /dev/block/by-name/EFI && dd if=/tmp/gpu201-esp.img of=/dev/block/by-name/EFI bs=4M conv=fsync && sync && rm /tmp/gpu201-esp.img'
adb reboot
''',name,(root/c['capture_tool'],root/c['fresh_manifest'],image,digest),flash=True)
  return offset
 try:
  save();offset=deploy(root/c['media_directory']/'update-esp.img',c['esp_sha256'],'prepare-write.log',True);st.update(status='prepare-observing',prepare_uart_offset=offset);save()
  spec=importlib.util.spec_from_file_location('gpu201_crc',root/c['crc_decoder']);decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
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
  st.update(status='install-observing',normal_uart_offset=offset);save();observer=Observation();deadline=time.monotonic()+900;batch_cap=None;finished=False
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
  if re.search(rb'A04(?:GPU|WIN) BUGCHECK(?: [0-9A-F]{8}| DATA_UNAVAILABLE)',drained[len(raw):]) or b'Unhandled Exception' in drained[len(raw):]:st.update(status='runtime-exception-await-collection',late_runtime_exception_observed=True)
  recovery('terminal-to-recovery.log');st['collection_boundary']=st['status'];save()
  command('''[ "$(adb get-state)" = recovery ]
! adb shell cat /proc/mounts | grep -q ntfs
adb push "$2" /tmp/rt-ntfs3g
adb shell chmod 755 /tmp/rt-ntfs3g
''','prepare-capture202.log',(root/c['ntfs3g'],))
  args=['python3','-B',str(root/c['capture_tool']),'capture','--serial',c['serial'],'--out',str(root/c['capture_output'])]
  for name in TARGET_SYS:args+=['--target-sys',name+'.sys']
  args+=['--allow-missing-sys','--engine-probe-state','--sd-component-state','--allow-missing-engine-service-log','--mount-driver','ntfs-3g','--ntfs3g-remote','/tmp/rt-ntfs3g']
  with (out/'capture202.log').open('wb') as f:subprocess.run(args,cwd=root,stdout=f,stderr=subprocess.STDOUT,check=True)
  capture=read(root/c['capture_output']/'manifest.json');require(capture['status']=='captured-unmounted' and capture['read_only'] is True and capture['unmounted'] is True and capture['errors']==[],'実capture202完了未確認')
  with (out/'collect-combined200.log').open('wb') as f:subprocess.run(['bash',str(root/c['diagnostics_collector']),str(seal),str(out/'result.json'),str(root/c['diagnostics_output'])],cwd=root,stdout=f,stderr=subprocess.STDOUT,check=True)
  index=root/c['diagnostics_output']/'capture-index.json';require(read(index)['windows_unmounted'] is True,'diagnostics unmount未確認')
  st.update(status='capture202-and-diagnostics-ready-for-main-review',capture_manifest_sha256=sha(root/c['capture_output']/'manifest.json'),diagnostics_index_sha256=sha(index),hardware_success=False);save();print('GPU201_CAPTURE202_READY_NOT_GPU_FUNCTIONAL_SUCCESS')
 except Exception as e:
  st.update(prior_status=st['status'],status='pipeline-stopped',error=str(e),hardware_success=False);save();raise
if __name__=='__main__':main()
