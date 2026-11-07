"""委任されたcodec/MSDC試験: EFIだけ更新し、boot/GPTの前後hashを保持する。"""
from pathlib import Path
import hashlib,importlib.util,json,os,re,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[4];LOG=ROOT/'port/gsi/logs/ios-feasibility';UART=LOG/'windows-rt-uart-workspace.log'
MEDIA=Path(sys.argv[1]).resolve();OUT=Path(sys.argv[2]).resolve()
assert MEDIA.is_relative_to(ROOT) and OUT.is_relative_to(ROOT)
RESUME=sys.argv[3:]==['--resume']
if RESUME:
 assert json.loads((OUT/'result.json').read_text())['status']=='prepare-incomplete-no-reset'
else:pass
manifest=json.loads((MEDIA/'manifest.json').read_text())
proof=json.loads((MEDIA/'trial-pins.json').read_text())
assert hashlib.sha256((MEDIA/'manifest.json').read_bytes()).hexdigest()==proof['media_manifest_sha256']
assert manifest['probe']['source_commit']==proof['service_source_commit']
assert manifest['probe']['deferred_update']['transaction']=='codec1026-msdc-quiet-fresh84'
assert manifest['selected_service']=='MT8167EngineProbe'
assert manifest['host_backup_manifest_sha256']==proof['fresh84_manifest_sha256']

if not RESUME:OUT.mkdir(mode=0o700)
NORMAL=ROOT/'port/xnu/out/external-tab-a04br3-import/windows-rt/mma8652-hid/recovery-esp-r4.img'
def deploy(image,want,name,backup_gate=False,firmware=None):
    assert firmware is None, 'Codec/MSDC update must not modify boot or GPT'
    with image.open('rb') as f:assert hashlib.file_digest(f,'sha256').hexdigest()==want
    # All paths are workspace-fixed and cannot contain shell metacharacters.
    assert re.fullmatch(r'[/A-Za-z0-9_.-]+',str(image))
    script='''set -euo pipefail
source "$1/port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh"
if ! adb devices | grep -q 'recovery$'; then
 UART=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0 bash "$1/unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh"
 for i in $(seq 90); do fastboot devices | grep -q fastboot && break; sleep 1; done
 fastboot getvar product 2>&1 | grep -qi 'product: *A04BR3'
 fastboot oem reboot-recovery
 for i in $(seq 120); do adb devices | grep -q 'recovery$' && break; sleep 1; done
fi
adb devices | grep -q 'recovery$'
if [ "$(adb shell id -u | tr -d '\\r')" != 0 ]; then
 adb root || true
 sleep 3
 adb wait-for-recovery
fi
[ "$(adb shell id -u | tr -d '\\r')" = 0 ]
if adb shell cat /proc/mounts | grep -q ntfs; then echo WINDOWS_VOLUME_STILL_MOUNTED; exit 8; fi
adb push "$2" /tmp/esp.img
[ "$(adb shell sha256sum /tmp/esp.img | cut -d ' ' -f 1)" = "$3" ]
if [ "$4" = backup ]; then
 python3 "$1/windows-rt/a04br3-windows-codec-msdc-fresh84/tools/recovery/capture-windows-prebackup.py" verify --serial "$rt_target_serial" --manifest "$5" --manifest-sha256 "$6"
fi
adb shell 'dd if=/tmp/esp.img of=/dev/block/by-name/EFI bs=4M conv=fsync && sync && rm /tmp/esp.img'
adb reboot
'''
    offset=UART.stat().st_size
    with (OUT/name).open('wb') as out:
        try:subprocess.run(['bash','-c',script,'rt-pnp',str(ROOT),str(image),want,'backup' if backup_gate else 'none',str(ROOT/'port/xnu/out/private-windows-rt-prebackup-run84/manifest.json'),manifest.get('host_backup_manifest_sha256',''),'none','none'],stdout=out,stderr=subprocess.STDOUT,check=True,timeout=500)
        finally:
            out.flush()
            with (LOG.parent/'flash.log').open('ab') as f:f.write((OUT/name).read_bytes())
    return offset
def save(result,offset,filename):
    with UART.open('rb') as f:f.seek(offset);(OUT/filename).write_bytes(f.read())
    (OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if RESUME:offset=int((OUT/'prepare-offset.txt').read_text())
else:
 offset=deploy(MEDIA/'update-esp.img',manifest['esp_sha256'],'prepare-write.log',True);(OUT/'prepare-offset.txt').write_text(str(offset))

spec=importlib.util.spec_from_file_location('audit',ROOT/'windows-rt/a04br3-windows-toprgu-transaction/tools/recovery/decode-audit-stream.py');decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
report=None;deadline=time.monotonic()+900
while time.monotonic()<deadline:
 time.sleep(2)
 with UART.open('rb') as f:f.seek(offset);data=f.read()
 if b'A04AU2E '+manifest['token'].encode() not in data:continue
 try:report=decoder.decode(data,manifest['token'])
 except ValueError as error:
  save({'status':'prepare-report-invalid-no-reset','decode_error':str(error)},offset,'prepare-uart.log')
  raise SystemExit('準備報告欠損。全報告確認まで通常起動しない')
 break
if report is None:
 save({'status':'prepare-incomplete-no-reset'},offset,'prepare-uart.log');raise SystemExit('準備完了未確認。進行中resetはしない')
(OUT/'prepare-report.txt').write_bytes(report)
lines=report.decode(errors='replace').splitlines()
if any(lines.count(s)!=1 for s in ('UPDATE_STATUS=COMMITTED','UPDATE_RELEASE_EXIT=0','UPDATE_EXIT=0')):
 save({'status':'prepare-failed-no-reset'},offset,'prepare-uart.log');raise SystemExit('準備失敗、通常Windowsへ進めない')
save({'status':'prepared-volume-released'},offset,'prepare-uart.log')
offset=deploy(NORMAL,'67ac0145d4917e1d53bfed15aff88a1585cf88ac73648733178e11ca3e64534a','normal-write.log')
(OUT/'normal-offset.txt').write_text(str(offset))

result={'status':'batch-unverified','restarts':0,'lk_resume':False,'service_results':[],'native_clear':[],'native_triangle_texture':[],'trace':[]}
deadline=time.monotonic()+720;finished_at=None;stream=UART.open('rb');stream.seek(offset);raw=b'';processed_lines=0;windows_seen=False
try:
 while time.monotonic()<deadline:
  time.sleep(.25);raw+=stream.read();clean,removed=re.subn(rb'A04WIN (?:StartIo fn=[0-9A-F]{8} opcode=[0-9A-F]{8}|blocks lba=[0-9A-F]{8} count=[0-9A-F]{8}|CDB=[0-9A-F]{8} status=[0-9A-F]{8})\r?\n',b'',raw);result['removed_complete_storport_records']=removed;parts=clean.split(b'\n')[:-1]
  (OUT/'normal-uart-deinterleaved.log').write_bytes(clean)
  for line in parts[processed_lines:]:
   if any(t in line for t in (b'A04HW ',b'A04GPU',b'A04TRACE',b'GEO',b'DXGK',b'BOOT_REASON',b'fastboot: processing commands',b'BUGCHECK')):result['trace'].append(line.decode(errors='replace'))
   if b'A04TRACE ExitBootServices event signaled' in line:
    windows_seen=True;result['handoff_attempt_observed']=True
   if b'A04GPU NATIVE D0 ENTER' in line or b'A04HW RUN=' in line:windows_seen=True
   if windows_seen and b'BOOT_REASON' in line:
    result['restarts']+=1;windows_seen=False
    if result['restarts']!=1 or not result.get('planned_update_reboot'):
     result['status']='unexpected-codec-msdc-runtime-restart';finished_at=time.monotonic()
   if b'fastboot: processing commands' in line and result['restarts']==1 and result.get('planned_update_reboot') and not result['lk_resume'] and not finished_at:
    with (OUT/'planned-fastboot-resume.log').open('wb') as log:
     subprocess.run(['bash','-c','set -euo pipefail; source "$1/port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh"; fastboot getvar product 2>&1 | grep -qi "product: *A04BR3"; fastboot reboot','rt-resume',str(ROOT)],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
    result['lk_resume']=True
   if result['restarts']==0 and re.search(rb'ID=00000000 EVT=0000010B CODE=00000BC2',line):result['planned_update_reboot']=True
   end=re.search(rb'A04HW RUN=([0-9A-F]{8}) ID=00000000 EVT=0000000A CODE=([0-9A-F]{8})',line)
   if end:
    result['service_results'].append({'run':end[1].decode(),'win32':int(end[2],16),'restarts':result['restarts']})
    save(result,offset,'normal-uart.log')
    if int(end[2],16)==3010 and result['restarts']==0:result['planned_update_reboot']=True
    if result['restarts'] or int(end[2],16)!=3010:result['status']='batch-service-ended-await-disk-verification';finished_at=time.monotonic()
   flush=re.search(rb'A04HW RUN=([0-9A-F]{8}) ID=00000000 EVT=00000110 CODE=([0-9A-F]{8})',line)
   if flush and result['restarts']==0 and int(flush[2],16)==0:
    result['first_phase_saved']=True;result['planned_update_reboot']=True
   if flush and result['restarts']==1:
    result['diagnostic_completion']={'run':flush[1].decode(),'restarts':result['restarts'],'finalization_status':int(flush[2],16),'evidence':'service finalization event before SCM STOPPED; snapshot and service result require disk verification'}
    result['status']='batch-service-ended-await-disk-verification';finished_at=finished_at or time.monotonic()
    save(result,offset,'normal-uart.log')
   if b'A04GPU NATIVE CLEAR STATUS=' in line:result['native_clear'].append(line.decode(errors='replace'))
   if b'A04GPU NATIVE TRIANGLE_TEXTURE STATUS=' in line:result['native_triangle_texture'].append(line.decode(errors='replace'))
   if re.search(rb'A04GPU BUGCHECK(?: [0-9A-F]{8}| DATA_UNAVAILABLE)',line) or any(t in line for t in (b'Unhandled Exception',b'Exception PC at',b'Kernel panic')):
    result['status']='runtime-exception';finished_at=time.monotonic()
    subprocess.run(['bash',str(ROOT/'unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30)
    break
  processed_lines=len(parts)
  if finished_at and time.monotonic()-finished_at>30:break
finally:stream.close();save(result,offset,'normal-uart.log')
print(json.dumps(result,ensure_ascii=False),flush=True)
