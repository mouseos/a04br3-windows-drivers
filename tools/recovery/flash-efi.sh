#!/usr/bin/env bash
# Recovery must already be installed. Writes one complete, locally verified ESP.
set -euo pipefail
if [[ $# != 1 || ! -f $1 ]]; then
  printf 'usage: %s /path/to/256MiB-esp.img\n' "$0" >&2
  exit 2
fi
image=$1
[[ $(stat -c %s "$image") == 268435456 ]] || { echo 'ESP image size mismatch' >&2; exit 1; }
image_sha=$(sha256sum "$image" | cut -d ' ' -f 1)
serial=${A04_SERIAL:-}
if [[ -z $serial ]]; then
  mapfile -t devices < <(fastboot devices | awk '$2 == "fastboot" {print $1}')
  [[ ${#devices[@]} == 1 ]] || { echo 'Set A04_SERIAL or connect exactly one fastboot device' >&2; exit 1; }
  serial=${devices[0]}
fi
[[ $serial =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid serial' >&2; exit 1; }
fastboot -s "$serial" oem reboot-recovery
ready=0
for ((i=0; i<90; ++i)); do
  state=$(timeout 3 adb -s "$serial" get-state 2>/dev/null || true)
  if [[ $state == recovery ]]; then ready=1; break; fi
  sleep 1
done
[[ $ready == 1 ]] || { echo 'ADB recovery did not appear' >&2; exit 1; }
timeout 12 adb -s "$serial" root || true
ready=0
for ((i=0; i<30; ++i)); do
  if [[ $(timeout 3 adb -s "$serial" shell id -u 2>/dev/null | tr -d '\r') == 0 ]]; then ready=1; break; fi
  sleep 1
done
[[ $ready == 1 ]] || { echo 'Recovery ADB root unavailable' >&2; exit 1; }
adb -s "$serial" push "$image" /tmp/a04-recovery-esp.img
# Avoid reading the written device. The staging file is verified before dd.
adb -s "$serial" shell "set -e; test \"\$(id -u)\" = 0; test \"\$(readlink -f /dev/block/by-name/EFI)\" = /dev/block/mmcblk0p18; test \"\$(cat /sys/class/block/mmcblk0p18/start)\" = 360448; test \"\$(cat /sys/class/block/mmcblk0p18/size)\" = 524288; ! grep -q ' /dev/block/mmcblk0p18 ' /proc/mounts; test \"\$(sha256sum /tmp/a04-recovery-esp.img | cut -d ' ' -f 1)\" = $image_sha; dd if=/tmp/a04-recovery-esp.img of=/dev/block/by-name/EFI bs=1048576 conv=fsync; sync"
adb -s "$serial" reboot
