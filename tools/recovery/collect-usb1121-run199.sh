#!/usr/bin/env bash
# UART停止/flush境界からRecoveryへ戻った後の、一度だけのファイル回収。
set -euo pipefail
root=$(pwd)
source "$root/port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh"
mapfile -t cfg < <(python3 - "$1" "$2" <<'PY'
from pathlib import Path
import json,sys
r=Path.cwd();c=json.loads(Path(sys.argv[1]).read_text());s=json.loads(Path(sys.argv[2]).read_text())
assert c['schema']=='MT8167_USB1121_RUN199_MAIN_SEAL_V1' and c['status']=='validated-main-seal'
assert s['status']=='recovery-awaiting-capture200' and s['prepare_crc_committed_released']
assert s.get('terminal') and not s['boot_written'] and not s['gpt_written']
p=(r/c['source_tree']).resolve(strict=True);assert p.is_relative_to(r);print(p)
PY
)
[ "${#cfg[@]}" = 1 ]; src=${cfg[0]}
out="$root/port/xnu/out/private-windows-rt-usb1121-run199-diagnostics"
[ ! -e "$out" ]
[ "$(adb get-state)" = recovery ]
[ "$(adb shell id -u | tr -d '\r')" = 0 ]
! adb shell cat /proc/mounts | grep -q ntfs
adb push "$root/port/xnu/out/windows-rt-recovery-ntfs3g/ntfs-3g-aarch64-static" /tmp/rt-ntfs3g
adb shell chmod 755 /tmp/rt-ntfs3g
args=()
for n in mt8167-keypad mt6392-keys lc709203f mt8167-display-transport mt8167-display mt8167-ge8300 ge8300-render mt8167-shared mt8167-audio mt8167-musb-hcd mt8167-usb20 mt6630-runtime mt6630-combo-runtime mt6630-combo-bluetooth mt8167-toprgu mt8167-msdc mma8652 mt6392-rtc mt8167-thermal; do args+=(--target-sys "$n.sys"); done
python3 -B "$src/tools/recovery/capture-windows-prebackup.py" capture --serial "$rt_target_serial" --out "$root/port/xnu/out/private-windows-rt-prebackup-run200" "${args[@]}" --allow-missing-sys --engine-probe-state --sd-component-state --allow-missing-engine-service-log --mount-driver ntfs-3g --ntfs3g-remote /tmp/rt-ntfs3g
mkdir -m 700 "$out"
adb shell 'mkdir /tmp/rt199-diag; /tmp/rt-ntfs3g -o ro,norecover,nosuid,nodev,noexec /dev/block/by-name/Windows /tmp/rt199-diag'
trap 'adb shell umount /tmp/rt199-diag || true' EXIT
base='/tmp/rt199-diag/Windows/System32/MT8167Usb1121Install198'
adb shell "find $base -maxdepth 8 -type f" > "$out/service-file-list.txt"
[ "$(wc -l < "$out/service-file-list.txt")" -le 256 ]
total=0
while IFS= read -r item; do
 item=${item%$'\r'}
 [[ "$item" == "$base/"* && "$item" != *'/../'* ]]
 [[ "$item" =~ ^/tmp/rt199-diag/Windows/System32/MT8167Usb1121Install198/[A-Za-z0-9_./-]+$ ]]
 size=$(adb shell stat -c %s "$item" </dev/null | tr -d '\r')
 [[ "$size" =~ ^[0-9]+$ ]] && [ "$size" -le 16777216 ]
 total=$((total+size)); [ "$total" -le 67108864 ]
 rel=${item#/tmp/rt199-diag/};mkdir -p "$out/fixed/$(dirname "$rel")"
 adb pull "$item" "$out/fixed/$rel" </dev/null
done < "$out/service-file-list.txt"
for n in Windows/System32/MT8167Usb1121Install198.exe Windows/System32/ge8300-umd.dll Windows/System32/winevt/Logs/System.evtx Windows/inf/setupapi.dev.log; do
 if adb shell "test -f /tmp/rt199-diag/$n" </dev/null; then
  size=$(adb shell stat -c %s "/tmp/rt199-diag/$n" </dev/null | tr -d '\r')
  [[ "$size" =~ ^[0-9]+$ ]] && [ "$size" -le 67108864 ]
  mkdir -p "$out/fixed/$(dirname "$n")";adb pull "/tmp/rt199-diag/$n" "$out/fixed/$n" </dev/null
 fi
done
adb shell umount /tmp/rt199-diag
trap - EXIT
adb shell rmdir /tmp/rt199-diag
find "$out" -type f -exec chmod 600 '{}' +
printf '%s\n' 'USB199_CAPTURE200_AND_DIAGNOSTICS_READY_NOT_USB_OR_POWER_SUCCESS'
