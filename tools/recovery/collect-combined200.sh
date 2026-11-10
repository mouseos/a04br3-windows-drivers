#!/usr/bin/env bash
set -euo pipefail
root=$(pwd)
source "$root/port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh"
mapfile -t cfg < <(python3 - "$1" "$2" <<'CONFIG'
from pathlib import Path
import json,re,sys
r=Path.cwd();d=Path(sys.argv[1]).resolve().parent
c=json.loads(Path(sys.argv[1]).read_text());s=json.loads(Path(sys.argv[2]).read_text())
known={
 'GPU368_RUN201_MAIN_SEAL_V1':('combined-run201','MT8167CombinedFollowup200'),
 'GPU368_RUN203_MAIN_SEAL_V1':('combined-run203','MT8167CombinedFollowup202'),
 'GPU374_RUN219_MAIN_SEAL_V1':('combined-run219','MT8167CombinedFollowup218'),
 'GPU373_RUN216_MAIN_SEAL_V1':('combined-run216','MT8167CombinedFollowup215'),
 'GPU371_RUN213_MAIN_SEAL_V1':('combined-run213','MT8167CombinedFollowup212'),
 'GPU369_RUN209_MAIN_SEAL_V1':('combined-run209','MT8167CombinedFollowup208'),
}
assert c['schema'] in known
transaction,service=known[c['schema']]
assert c['status']=='validated-main-seal' and c['transaction']==transaction and s['transaction']==transaction
assert c['service_name']==c['data_directory']==service
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
out=$(realpath -m "$3")
[[ "$out" == "$root/port/xnu/out/"* && ! -e "$out" ]]
# Recoveryへの遷移/リセットはmain担当。collectorは既Recoveryのread-only回収だけ。
[[ $(adb get-state) == recovery ]]
[[ $(adb shell id -u | tr -d '\r') == 0 ]]
adb shell test -x /tmp/rt-ntfs3g
mkdir -m 700 "$out"
adb shell 'mkdir /tmp/rt125-diag; /tmp/rt-ntfs3g -o ro,norecover,nosuid,nodev,noexec /dev/block/by-name/Windows /tmp/rt125-diag'
trap 'adb shell umount /tmp/rt125-diag || true' EXIT
for item in Windows/System32/config/SYSTEM Windows/System32/config/SYSTEM.LOG1 Windows/System32/config/SYSTEM.LOG2 Windows/System32/dxg-global-diag198.exe "Windows/System32/$service_name.exe" Windows/System32/ge8300-umd.dll Windows/System32/mt8167-dxgi-console-launch363.exe Windows/System32/mt8167-dxgi-present356.exe Windows/System32/winload.efi Windows/System32/mt8167-clock-snapshot.exe Windows/System32/Drivers/ge8300-mips.fw Windows/System32/winevt/Logs/System.evtx Windows/inf/setupapi.dev.log Windows/System32/MT8167DisplayRotationProbeV1/logs/display-rotation-probe-v1.log Windows/System32/MT8167CombinedFollowup124/logs/wlan-scan.txt; do
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
   "$managed/logs/global-diag368-response.bin") { [ "$size" -eq 0 ] || [ "$size" -eq 262148 ]; } ;;
   "$managed/logs/global-diag368-raw.bin") [ "$size" -le 262144 ] ;;
   "$managed/logs/global-diag368-records.txt") [ "$size" -le 16777216 ] ;;
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
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
present_helper="$here/collect-present356.py"
[ "$(sha256sum "$present_helper" | cut -d' ' -f1)" = '08ee8ee84428356f2b9c954e8edf1f362a50445ab771afb2b5fb107df2832734' ]
python3 -B "$present_helper" --serial "$rt_target_serial" --out "$out/present356" > "$out/present356-collection.log" 2>&1 || printf '%s\n' 'PRESENT_COLLECTION_NEGATIVE_PRESERVED' >> "$out/present356-collection.log"
adb shell umount /tmp/rt125-diag
trap - EXIT
adb shell rmdir /tmp/rt125-diag
find "$out" -type f -exec chmod 600 '{}' +
python3 - "$out" <<'MANIFEST'
from pathlib import Path
import json,hashlib,sys
p=Path(sys.argv[1])
files=[{'path':str(f.relative_to(p)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in sorted(p.rglob('*')) if f.is_file()]
(p/'capture-index.json').write_text(json.dumps({'files':files,'windows_unmounted':True,'hardware_success_asserted':False,'globaldiag_expected_files':['logs/global-diag368-response.bin','logs/global-diag368-raw.bin','logs/global-diag368-records.txt'],'missing_outputs_are_not_GPU_success':True},indent=2)+'\n')
MANIFEST
echo GPU368_DIAGNOSTICS_COLLECTED_NOT_A_SUCCESS_ASSERTION
