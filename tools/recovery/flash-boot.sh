#!/usr/bin/env bash
# Write one verified boot image from the already running A04BR3 recovery.
# The destination is never read back. Run only for the selected device.
set -euo pipefail
if [[ $# != 2 || ! -f $1 || ! $2 =~ ^[0-9a-f]{64}$ ]]; then
  printf 'usage: %s /path/to/boot.img expected-sha256\n' "$0" >&2
  exit 2
fi
image=$1
expected=$2
[[ $(sha256sum "$image" | cut -d ' ' -f 1) == "$expected" ]] || { echo 'boot image hash mismatch' >&2; exit 1; }
size=$(stat -c %s "$image")
[[ $size -gt 0 && $size -le 33554432 ]] || { echo 'boot image size invalid' >&2; exit 1; }
[[ $(head -c 8 "$image") == ANDROID! ]] || { echo 'boot image header invalid' >&2; exit 1; }
serial=${A04_SERIAL:-}
[[ $serial =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Set A04_SERIAL to the target' >&2; exit 1; }
[[ $(timeout 3 adb -s "$serial" get-state 2>/dev/null) == recovery ]] || { echo 'Recovery ADB unavailable' >&2; exit 1; }
timeout 12 adb -s "$serial" root || true
ready=0
for ((i=0; i<30; ++i)); do
  if [[ $(timeout 3 adb -s "$serial" shell id -u 2>/dev/null | tr -d '\r') == 0 ]]; then ready=1; break; fi
  sleep 1
done
[[ $ready == 1 ]] || { echo 'Recovery ADB root unavailable' >&2; exit 1; }
adb -s "$serial" push "$image" /tmp/a04-test-boot.img
adb -s "$serial" shell "set -e; test \"\$(id -u)\" = 0; test \"\$(readlink -f /dev/block/by-name/boot)\" = /dev/block/mmcblk0p17; test \"\$(cat /sys/class/block/mmcblk0p17/start)\" = 294912; test \"\$(cat /sys/class/block/mmcblk0p17/size)\" = 65536; ! grep -qE '^(/dev/block/mmcblk0p17|/dev/block/by-name/boot) ' /proc/mounts; test \"\$(sha256sum /tmp/a04-test-boot.img | cut -d ' ' -f 1)\" = $expected; dd if=/tmp/a04-test-boot.img of=/dev/block/by-name/boot bs=1048576 conv=sync,fsync; sync"
adb -s "$serial" reboot
