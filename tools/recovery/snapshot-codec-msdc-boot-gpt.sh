#!/usr/bin/env bash
set -euo pipefail
root=$(pwd)
source "$root/port/gsi/logs/ios-feasibility/windows-rt-target-tools.sh"
[[ $(adb get-state) == recovery ]]
[[ $(adb shell id -u | tr -d '\r') == 0 ]]
adb shell 'set -e; test -b /dev/block/by-name/boot; test -b /dev/block/mmcblk0; echo BOOT_SHA256; sha256sum /dev/block/by-name/boot; echo PRIMARY_GPT_SHA256; dd if=/dev/block/mmcblk0 bs=512 count=34 2>/dev/null | sha256sum; n=$(cat /sys/class/block/mmcblk0/size); test "$n" -gt 1000000; echo BACKUP_GPT_SHA256; dd if=/dev/block/mmcblk0 bs=512 skip=$((n-33)) count=33 2>/dev/null | sha256sum; echo EMMC_SECTORS=$n; echo WINDOWS_START=$(cat /sys/class/block/mmcblk0p20/start); echo WINDOWS_SIZE=$(cat /sys/class/block/mmcblk0p20/size)'
