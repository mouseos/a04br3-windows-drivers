#!/usr/bin/env bash
# RecoveryでESPの試験用ドライバ一式だけを更新する。初回は対応WinPEが必要。
set -euo pipefail
[[ $# == 1 && -d $1 ]] || { echo 'usage: stage-ge8300-package.sh PACKAGE_DIR' >&2; exit 2; }
package=$1
serial=${A04_SERIAL:-}
[[ $serial =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'A04_SERIALを指定してください' >&2; exit 2; }
files=(mt8167-ge8300.inf mt8167-ge8300.sys mt8167-ge8300.cat ge8300-mips.fw ge8300-status.exe ge8300-pnp-status.exe)
for file in "${files[@]}"; do [[ -f $package/$file ]] || { echo "不足: $file" >&2; exit 2; }; done
fastboot -s "$serial" getvar product 2>&1 | grep -q 'product: A04BR3' || { echo '機種を確認できません' >&2; exit 1; }
fastboot -s "$serial" oem reboot-recovery
ready=0
for ((i=0; i<90; ++i)); do
  [[ $(timeout 3 adb -s "$serial" get-state 2>/dev/null || true) == recovery ]] && { ready=1; break; }
  sleep 1
done
[[ $ready == 1 ]] || { echo 'Recoveryが起動しません' >&2; exit 1; }
timeout 12 adb -s "$serial" root || true
ready=0
for ((i=0; i<30; ++i)); do
  [[ $(timeout 3 adb -s "$serial" shell id -u 2>/dev/null | tr -d '\r') == 0 ]] && { ready=1; break; }
  sleep 1
done
[[ $ready == 1 ]] || { echo 'Recoveryのroot権限を取得できません' >&2; exit 1; }
adb -s "$serial" shell 'set -e; test ! -e /tmp/a04-ge8300-stage; mkdir /tmp/a04-ge8300-stage'
for file in "${files[@]}"; do
  adb -s "$serial" push "$package/$file" "/tmp/a04-ge8300-stage/$file"
  digest=$(sha256sum "$package/$file" | cut -d ' ' -f1)
  adb -s "$serial" shell "test \"\$(sha256sum /tmp/a04-ge8300-stage/$file | cut -d ' ' -f1)\" = $digest"
done
# 既存ディレクトリは直前状態として保持し、置換失敗時は戻す。
adb -s "$serial" shell 'set -e
 test "$(readlink -f /dev/block/by-name/EFI)" = /dev/block/mmcblk0p18
 test "$(cat /sys/class/block/mmcblk0p18/start)" = 360448
 test "$(cat /sys/class/block/mmcblk0p18/size)" = 524288
 ! grep -qE "^(/dev/block/mmcblk0p18|/dev/block/by-name/EFI) " /proc/mounts
 test ! -e /tmp/a04-esp
 mkdir /tmp/a04-esp
 mount -t vfat -o rw /dev/block/by-name/EFI /tmp/a04-esp
 test ! -e /tmp/a04-esp/a04-ge8300-new
 test ! -e /tmp/a04-esp/a04-ge8300-old
 mkdir /tmp/a04-esp/a04-ge8300-new
 cp /tmp/a04-ge8300-stage/* /tmp/a04-esp/a04-ge8300-new/
 sync
 if test -e /tmp/a04-esp/a04-ge8300; then mv /tmp/a04-esp/a04-ge8300 /tmp/a04-esp/a04-ge8300-old; fi
 if ! mv /tmp/a04-esp/a04-ge8300-new /tmp/a04-esp/a04-ge8300; then
   if test -e /tmp/a04-esp/a04-ge8300-old; then mv /tmp/a04-esp/a04-ge8300-old /tmp/a04-esp/a04-ge8300; fi
   exit 1
 fi
 sync
 if test -e /tmp/a04-esp/a04-ge8300-old; then rm -r /tmp/a04-esp/a04-ge8300-old; fi
 sync
 umount /tmp/a04-esp
'
adb -s "$serial" reboot
