#!/usr/bin/env bash
set -euo pipefail
root=$(pwd)
source "$root/port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh"
mapfile -t cfg < <(python3 - "$1" "$2" <<'CONFIG'
from pathlib import Path
import json,re,sys
r=Path.cwd();d=r/'port/gsi/logs/ios-feasibility/windows-rt-combined-run197'
c=json.loads(Path(sys.argv[1]).read_text());s=json.loads(Path(sys.argv[2]).read_text())
assert c['status']=='validated-main-seal'
if s['status']=='pipeline-stopped':
 proof=json.loads((d/'rejected-prepare-proof.json').read_text());assert proof['CRC_valid'] and proof['status']=='BASELINE_REJECTED_NO_CHANGE' and proof['volume_release_exit']==0 and proof['update_exit']==6 and not proof['candidate_applied']
else:
 assert s['status'] in ['diagnostic-terminal-await-disk-verification','install-failed-await-disk-verification','runtime-exception-await-collection','unexpected-second-runtime-restart','runtime-restarted-requires-install-disk-proof','diagnostic-timeout-await-file-proof','install-timeout-await-file-proof']
for k in ['service_name','data_directory','gpu_log_directory']:
 assert re.fullmatch(r'[A-Za-z0-9_-]{1,100}',c[k]);print(c[k])
CONFIG
)
[ "${#cfg[@]}" = 3 ]
service_name=${cfg[0]}; data_directory=${cfg[1]}; gpu_log_directory=${cfg[2]}
out="$root/port/xnu/out/private-windows-rt-combined-run197-diagnostics"
if ! adb devices | grep -q 'recovery$'; then
 if [ -z "$(fastboot devices)" ]; then
  UART=/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0 bash "$root/unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh"
  for ((i=0;i<90;i++)); do [ -n "$(fastboot devices)" ] && break; sleep 1; done
 fi
 fastboot getvar product 2>&1 | grep -qi 'product: *A04BR3'
 fastboot oem reboot-recovery
 for ((i=0;i<120;i++)); do [[ $(adb get-state 2>/dev/null) == recovery ]] && break; sleep 1; done
fi
[[ $(adb get-state) == recovery ]]
adb root || true
for ((i=0;i<30;i++)); do [[ $(adb shell id -u 2>/dev/null | tr -d '\r') == 0 ]] && break; sleep 1; done
[[ $(adb shell id -u | tr -d '\r') == 0 ]]
adb push "$root/port/xnu/out/windows-rt-recovery-ntfs3g/ntfs-3g-aarch64-static" /tmp/rt-ntfs3g
adb shell chmod 755 /tmp/rt-ntfs3g
python3 -B "$root/windows-rt/a04br3-windows-public-transport/tools/recovery/capture-windows-prebackup.py" capture --serial "$rt_target_serial" --out "$root/port/xnu/out/private-windows-rt-prebackup-run198" --target-sys mt8167-keypad.sys --target-sys mt6392-keys.sys --target-sys lc709203f.sys --target-sys mt8167-display-transport.sys --target-sys mt8167-display.sys --target-sys mt8167-ge8300.sys --target-sys ge8300-render.sys --target-sys mt8167-shared.sys --target-sys mt8167-audio.sys --target-sys mt8167-musb-hcd.sys --target-sys mt8167-usb20.sys --target-sys mt6630-runtime.sys --target-sys mt6630-combo-runtime.sys --target-sys mt6630-combo-bluetooth.sys --target-sys mt8167-toprgu.sys --target-sys mt8167-msdc.sys --target-sys mma8652.sys --target-sys mt6392-rtc.sys --target-sys mt8167-thermal.sys --allow-missing-sys --engine-probe-state --sd-component-state --allow-missing-engine-service-log --mount-driver ntfs-3g --ntfs3g-remote /tmp/rt-ntfs3g
mkdir -m 700 "$out"
adb shell 'mkdir /tmp/rt125-diag; /tmp/rt-ntfs3g -o ro,norecover,nosuid,nodev,noexec /dev/block/by-name/Windows /tmp/rt125-diag'
trap 'adb shell umount /tmp/rt125-diag || true' EXIT
for item in "Windows/System32/$service_name.exe" Windows/System32/ge8300-umd.dll Windows/System32/mt8167-dxgi-console-launch363.exe Windows/System32/mt8167-dxgi-present356.exe Windows/System32/winload.efi Windows/System32/mt8167-clock-snapshot.exe Windows/System32/Drivers/ge8300-mips.fw Windows/System32/winevt/Logs/System.evtx Windows/inf/setupapi.dev.log Windows/System32/MT8167DisplayRotationProbeV1/logs/display-rotation-probe-v1.log Windows/System32/MT8167CombinedFollowup124/logs/wlan-scan.txt; do
 if adb shell "test -f /tmp/rt125-diag/$item" </dev/null; then
  mkdir -p "$out/fixed/$(dirname "$item")"
  adb pull "/tmp/rt125-diag/$item" "$out/fixed/$item" </dev/null
 fi
done
managed="/tmp/rt125-diag/Windows/System32/$data_directory"
if adb shell "test -d $managed"; then
 adb shell "find $managed -maxdepth 3 -type f" > "$out/managed-files.txt"
 [ "$(wc -l < "$out/managed-files.txt")" -le 256 ]
 while IFS= read -r item; do
  item=${item%$'\r'}
  [[ "$item" == "$managed/"* && "$item" != *'/../'* ]]
  [[ "$item" =~ ^/tmp/rt125-diag/Windows/System32/[A-Za-z0-9_./-]+$ ]]
  [[ "$item" =~ \.(log|txt|bin|exe)$ ]] || continue
  size=$(adb shell stat -c %s "$item" </dev/null | tr -d '\r')
  [[ "$size" =~ ^[0-9]+$ ]] && [ "$size" -le 16777216 ]
  case "$item" in
   "$managed/mt8167-clock-snapshot-v1.bin") [ "$size" -le 128 ] ;;
   "$managed/logs/clock_snapshot1120.stdout.txt"|"$managed/logs/clock_snapshot1120.stderr.txt") [ "$size" -le 4096 ] ;;
   "$managed/logs/service.log") [ "$size" -le 1048576 ] ;;
  esac
  rel=${item#/tmp/rt125-diag/}
  mkdir -p "$out/fixed/$(dirname "$rel")"
  adb pull "$item" "$out/fixed/$rel" </dev/null
 done < "$out/managed-files.txt"
fi
if adb shell "test -d /tmp/rt125-diag/$gpu_log_directory"; then
 adb shell "find /tmp/rt125-diag/$gpu_log_directory -maxdepth 1 -type f -name '*.log'" > "$out/gpu-log-file-list.txt"
 [ "$(wc -l < "$out/gpu-log-file-list.txt")" -le 128 ]
 total=0
 while IFS= read -r item; do
  item=${item%$'\r'}
  [[ "$item" == "/tmp/rt125-diag/$gpu_log_directory/"* ]]
  name=${item##*/}; [[ "$name" =~ ^(clear|umd)-[a-zA-Z0-9_.-]+\.log$ ]]
  size=$(adb shell stat -c %s "$item" </dev/null | tr -d '\r')
  [[ "$size" =~ ^[0-9]+$ ]] && [ "$size" -le 16777216 ]
  total=$((total+size)); [ "$total" -le 67108864 ]
  adb pull "$item" "$out/" </dev/null
 done < "$out/gpu-log-file-list.txt"
fi
# New bounded evidence collection runs while the main-owned read-only mount exists.
# Preserve partial/missing evidence and complete native capture even on helper failure.
present_helper="$root/windows-rt/a04br3-windows-public-transport/tools/recovery/collect-present356.py"
[ "$(sha256sum "$present_helper" | cut -d' ' -f1)" = '08ee8ee84428356f2b9c954e8edf1f362a50445ab771afb2b5fb107df2832734' ]
python3 -B "$present_helper" --serial "$rt_target_serial" --out "$out/present356" > "$out/present356-collection.log" 2>&1 || printf '%s\n' 'PRESENT_COLLECTION_NEGATIVE_PRESERVED' >> "$out/present356-collection.log"
adb shell umount /tmp/rt125-diag
trap - EXIT
adb shell rmdir /tmp/rt125-diag
find "$out" -type f -exec chmod 600 '{}' +
python3 - <<'MANIFEST'
from pathlib import Path
import json,hashlib
r=Path.cwd();p=r/'port/xnu/out/private-windows-rt-combined-run197-diagnostics'
files=[{'path':str(f.relative_to(p)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in sorted(p.rglob('*')) if f.is_file()]
(p/'capture-index.json').write_text(json.dumps({'files':files,'windows_unmounted':True,'hardware_success_asserted':False},indent=2)+'\n')
MANIFEST
echo COMBINED_RUN197_DIAGNOSTICS_COLLECTED_NOT_A_SUCCESS_ASSERTION
