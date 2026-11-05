#!/usr/bin/env bash
# Restore previously saved boot and ESP, only from recovery already reached by
# the operator. Never reset a running OS or infer completion from a timeout.
set -euo pipefail
if [[ $# != 7 || ( $1 != --verify-only && $1 != --execute ) ]]; then
  echo 'usage: restore-toprgu-diagnostic.sh --verify-only|--execute SERIAL BOOT BOOT_SHA256 ESP ESP_SHA256 LOG_DIR' >&2; exit 2
fi
mode=$1; serial=$2; boot=$3; boot_hash=$4; esp=$5; esp_hash=$6; log=$7
repo=$(cd "$(dirname "$0")/../.." && pwd -P)
workspace=$(cd "$repo/../.." && pwd -P)
log=$(realpath -m "$log")
[[ $log == "$workspace"/* ]] || { echo RESTORE_LOG_OUTSIDE_WORKSPACE >&2; exit 2; }
[[ $serial =~ ^[A-Za-z0-9._-]+$ && $boot_hash =~ ^[0-9a-f]{64}$ && $esp_hash =~ ^[0-9a-f]{64}$ ]] || exit 2
[[ -f $boot && -f $esp && $(stat -c %s "$esp") == 268435456 ]] || exit 2
[[ $(stat -c %s "$boot") -le 33554432 && $(head -c 8 "$boot") == ANDROID! ]] || exit 2
[[ $(sha256sum "$boot" | cut -d ' ' -f1) == "$boot_hash" && $(sha256sum "$esp" | cut -d ' ' -f1) == "$esp_hash" ]] || { echo RESTORE_HASH_MISMATCH >&2; exit 2; }
if [[ $mode == --verify-only ]]; then echo RESTORE_INPUTS_VALIDATED_NO_DEVICE_ACTION; exit 0; fi
[[ ! -e $log ]] || { echo RESTORE_LOG_ALREADY_EXISTS >&2; exit 2; }
mkdir -m 700 "$log"
exec > >(tee -a "$log/restore.log") 2>&1
printf 'SERIAL=%s\nBOOT_SHA256=%s\nESP_SHA256=%s\n' "$serial" "$boot_hash" "$esp_hash"
[[ $(timeout 3 adb -s "$serial" get-state 2>/dev/null | tr -d '\r') == recovery ]] || { echo RECOVERY_NOT_AVAILABLE_NO_WRITE; exit 3; }
[[ $(timeout 3 adb -s "$serial" shell id -u 2>/dev/null | tr -d '\r') == 0 ]] || { echo RECOVERY_NOT_ROOT_NO_WRITE; exit 3; }
require_marker() {
  local marker=$1 command=$2 output
  output=$(adb -s "$serial" shell "$command" 2>&1) || { printf '%s\n' "$output"; return 1; }
  printf '%s\n' "$output"
  printf '%s\n' "$output" | tr -d '\r' | grep -Fxq "$marker"
}
# Both destinations and both staging paths must pass before either write.
require_marker A04_RESTORE_GEOMETRY_OK "set -e; test \"\$(readlink -f /dev/block/by-name/boot)\" = /dev/block/mmcblk0p17; test \"\$(cat /sys/class/block/mmcblk0p17/start)\" = 294912; test \"\$(cat /sys/class/block/mmcblk0p17/size)\" = 65536; test \"\$(readlink -f /dev/block/by-name/EFI)\" = /dev/block/mmcblk0p18; test \"\$(cat /sys/class/block/mmcblk0p18/start)\" = 360448; test \"\$(cat /sys/class/block/mmcblk0p18/size)\" = 524288; ! grep -qE '^(/dev/block/mmcblk0p17|/dev/block/by-name/boot|/dev/block/mmcblk0p18|/dev/block/by-name/EFI) ' /proc/mounts; test ! -e /tmp/a04-toprgu-restore-boot.img; test ! -e /tmp/a04-toprgu-restore-esp.img; echo A04_RESTORE_GEOMETRY_OK"
adb -s "$serial" push "$boot" /tmp/a04-toprgu-restore-boot.img
adb -s "$serial" push "$esp" /tmp/a04-toprgu-restore-esp.img
require_marker A04_RESTORE_STAGING_OK "set -e; test \"\$(sha256sum /tmp/a04-toprgu-restore-boot.img | cut -d ' ' -f1)\" = $boot_hash; test \"\$(sha256sum /tmp/a04-toprgu-restore-esp.img | cut -d ' ' -f1)\" = $esp_hash; echo A04_RESTORE_STAGING_OK"
echo RESTORE_INPUTS_STAGED
# Partial success remains visible; a failed command never triggers reset.
require_marker A04_RESTORE_BOOT_SYNCED 'dd if=/tmp/a04-toprgu-restore-boot.img of=/dev/block/by-name/boot bs=1048576 conv=sync,fsync && sync && echo A04_RESTORE_BOOT_SYNCED'
echo RESTORE_BOOT_COMMAND_SUCCEEDED
require_marker A04_RESTORE_ESP_SYNCED 'dd if=/tmp/a04-toprgu-restore-esp.img of=/dev/block/by-name/EFI bs=1048576 conv=fsync && sync && echo A04_RESTORE_ESP_SYNCED'
echo RESTORE_ESP_COMMAND_SUCCEEDED
echo RESTORE_COMPLETE_NO_REBOOT
