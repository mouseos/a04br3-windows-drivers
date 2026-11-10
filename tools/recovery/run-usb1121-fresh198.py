#!/usr/bin/env python3
"""固定媒体を一度だけ適用し、実UARTの完了証拠から回収へ進む。"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,re,subprocess,time

def main():
 ap=argparse.ArgumentParser()
 for k in ('workspace','contract','output'):ap.add_argument('--'+k,type=Path,required=True)
 ap.add_argument('--contract-sha256',required=True)
 a=ap.parse_args();r=a.workspace.resolve(strict=True);out=a.output.resolve()
 def rooted(p):
  q=(r/p).resolve(strict=True);assert q.is_relative_to(r);return q
 def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
 cp=rooted(a.contract);assert sha(cp)==a.contract_sha256;c=json.loads(cp.read_text())
 assert c['schema']=='MT8167_USB1121_RUN199_MAIN_SEAL_V1' and c['status']=='validated-main-seal'
 assert c['fresh_manifest_sha256']=='4cfe2edfded6ff752ff145b46c33af910b30370e99079f9c75704cf17e2467a3'
 assert c['boot_written'] is False and c['gpt_written'] is False
 assert c['max_planned_reboots'] in (1,2,3) and c['hardware_success_claimed'] is False
 for x in c['pinned_files']:assert sha(rooted(x['path']))==x['sha256']
 src=rooted(c['source_tree']);media=rooted(c['media_directory']);m=json.loads((media/'manifest.json').read_text())
 assert m['schema']=='MT8167_USB1121_MEDIA198_V1' and m['fresh_sha256']==c['fresh_manifest_sha256']
 assert sha(media/'manifest.json')==c['media_manifest_sha256'] and sha(media/'update-esp.img')==m['esp_sha256']
 assert sha(rooted(c['normal_esp']))==c['normal_esp_sha256']
 assert out.is_relative_to(r) and not out.exists();out.mkdir(mode=0o700)
 uart=rooted('port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log')
 st=dict(status='starting',events=[],planned_reboots=0,boot_written=False,gpt_written=False,hardware_success=False)
 def save(): (out/'result.json').write_text(json.dumps(st,indent=2)+'\n')
 def command(body,name,args=(),timeout=400):
  with (out/name).open('wb') as f:
   try:subprocess.run(['bash','-c','set -euo pipefail\nsource "$1/port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh"\n'+body,'usb199',str(r),*map(str,args)],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=timeout)
   finally:
    f.flush()
    with (r/'port/gsi/logs/flash.log').open('ab') as log:log.write((out/name).read_bytes())
 def recovery(name):
  command('''if [ "$(adb get-state 2>/dev/null || true)" != recovery ]; then
 if [ -z "$(fastboot devices)" ]; then
  UART=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0 bash "$1/unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh"
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
''',name)
 def deploy(image,digest,name,initial=False):
  assert sha(image)==digest;offset=uart.stat().st_size
  gate=''
  if initial:
   gate='python3 -B "$2" verify --serial "$rt_target_serial" --manifest "$3" --manifest-sha256 '+c['fresh_manifest_sha256']+'\n'
  command('[ "$(adb get-state)" = recovery ]\n! adb shell cat /proc/mounts | grep -q ntfs\n'+gate+'''adb push "$4" /tmp/usb199-esp.img
[ "$(adb shell sha256sum /tmp/usb199-esp.img | cut -d ' ' -f 1)" = "$5" ]
adb shell 'test -b /dev/block/by-name/EFI && dd if=/tmp/usb199-esp.img of=/dev/block/by-name/EFI bs=4M conv=fsync && sync && rm /tmp/usb199-esp.img'
adb reboot
''',name,(src/'tools/recovery/capture-windows-prebackup.py',rooted(c['fresh_manifest']),image,digest))
  return offset
 try:
  save();offset=deploy(media/'update-esp.img',m['esp_sha256'],'prepare-write.log',True)
  st.update(status='prepare-observing',prepare_uart_offset=offset);save()
  spec=importlib.util.spec_from_file_location('audit',src/'tools/recovery/decode-audit-stream.py');dec=importlib.util.module_from_spec(spec);spec.loader.exec_module(dec)
  token=c['prepare_token'];assert re.fullmatch('[0-9A-F]{16}',token)
  deadline=time.monotonic()+900;report=None
  with uart.open('rb') as f:
   f.seek(offset);raw=b''
   while time.monotonic()<deadline:
    time.sleep(2);raw+=f.read();(out/'prepare-uart.log').write_bytes(raw)
    if b'Unhandled Exception' in raw:raise RuntimeError('prepare exception; no normal boot')
    if re.search(rb'A04AU2E '+token.encode()+rb' [0-9A-F]{8}\r?\n',raw):report=dec.decode(raw,token);break
  if report is None:raise RuntimeError('prepare terminal missing; no timeout reset')
  (out/'prepare-report.txt').write_bytes(report);lines=report.decode().splitlines()
  assert all(lines.count(x)==1 for x in ('UPDATE_STATUS=COMMITTED','UPDATE_RELEASE_EXIT=0','UPDATE_EXIT=0'))
  st['prepare_crc_committed_released']=True;save()
  recovery('prepare-to-recovery.log');offset=deploy(rooted(c['normal_esp']),c['normal_esp_sha256'],'normal-esp-write.log')
  st.update(status='normal-service-observing',normal_uart_offset=offset);save()
  deadline=time.monotonic()+1200;processed=0;raw=b'';runs={};seen_os=False;pending=False;terminal=None
  with uart.open('rb') as f:
   f.seek(offset)
   while time.monotonic()<deadline:
    time.sleep(.5);raw+=f.read();(out/'normal-uart.log').write_bytes(raw)
    clean=re.sub(rb'A04WIN (?:StartIo fn=[0-9A-F]{8} opcode=[0-9A-F]{8}|blocks lba=[0-9A-F]{8} count=[0-9A-F]{8}|CDB=[0-9A-F]{8} status=[0-9A-F]{8})\r?\n',b'',raw)
    (out/'normal-uart-deinterleaved.log').write_bytes(clean);parts=clean.split(b'\n')[:-1]
    for line in parts[processed:]:
     if b'ExitBootServices event signaled' in line:seen_os=True
     event=re.search(rb'U198 000000C6 ([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8})',line)
     if event:
      run,pid,e,code=[int(x,16) for x in event.groups()];key=(st['planned_reboots'],run,pid)
      if e==1:
       assert seen_os and key not in runs;runs[key]={}
      assert key in runs,'event without current-boot START';runs[key][e]=code
      st['events'].append(dict(boot=key[0],run=run,pid=pid,event=e,code=code,time=time.time()))
      if e==6:
       q=runs[key];assert q.get(1)==0 and 2 in q and q.get(4)==0 and q.get(5)==0,'terminal missing matching start/result/stop/flush'
       if code==3010:
        assert q.get(2)==3010 and q.get(3)==3010 and q.get(7)==0
        pending=True;st['status']='planned-reboot-pending'
       else:
        terminal=dict(run=run,pid=pid,code=code,boot=key[0]);st['status']='terminal-safe-for-collection'
     if seen_os and b'BOOT_REASON' in line:
      assert pending and terminal is None,'unexpected runtime restart; main failure review required'
      st['planned_reboots']+=1;assert st['planned_reboots']<=c['max_planned_reboots'];seen_os=False
     if b'fastboot: processing commands' in line and pending and not seen_os:
      command('info=$(fastboot getvar product 2>&1)\n[[ "$info" =~ product:[[:space:]]*A04BR3 ]]\nfastboot reboot\n','planned-resume-'+str(st['planned_reboots'])+'.log',timeout=45)
      pending=False;st['status']='resume-service-observing'
     if re.search(rb'A04(?:GPU|WIN) BUGCHECK(?: [0-9A-F]{8}| DATA_UNAVAILABLE)',line) or b'Unhandled Exception' in line:
      st['runtime_exception_observed']=True;save();recovery('runtime-exception-to-recovery.log');raise RuntimeError('runtime exception; separate failed-boot collection required')
    processed=len(parts);save()
    if terminal:break
  if terminal is None:raise RuntimeError('service terminal absent; keep evidence and do not reset on timeout')
  st['terminal']=terminal;save();time.sleep(3)
  recovery('terminal-to-recovery.log');st['status']='recovery-awaiting-capture200';save()
  print('USB199_TERMINAL_RECOVERY_READY_NOT_USB_OR_POWER_SUCCESS',flush=True)
 except Exception as e:
  st.update(prior_status=st['status'],status='stopped-requires-specific-review',error=str(e));save();raise

if __name__=='__main__':main()
