#!/system/bin/sh
# Run only in already-authorized root recovery. Read the ESP using a temporary
# read-only mount; absence is accepted only after successful mount and unmount.
set -u
fail() { echo A04_ESP_JOURNAL_UNKNOWN; exit 5; }
[ "$(id -u)" = 0 ] || fail
[ "$(readlink -f /dev/block/by-name/EFI)" = /dev/block/mmcblk0p18 ] || fail
[ "$(cat /sys/class/block/mmcblk0p18/start)" = 360448 ] || fail
[ "$(cat /sys/class/block/mmcblk0p18/size)" = 524288 ] || fail
while read -r source rest; do
 resolved=$(readlink -f "$source" 2>/dev/null || true)
 [ "$resolved" != /dev/block/mmcblk0p18 ] || fail
done < /proc/mounts
mnt=$(mktemp -d /tmp/a04-esp-guard.XXXXXX) || fail
mounted=0
cleanup() { if [ "$mounted" = 1 ]; then umount "$mnt" >/dev/null 2>&1 || true; fi; rmdir "$mnt" >/dev/null 2>&1 || true; }
trap cleanup EXIT HUP INT TERM
mount -t vfat -o ro /dev/block/mmcblk0p18 "$mnt" || fail
mounted=1
ls -a "$mnt" >/dev/null 2>&1 || fail
if [ -e "$mnt/A04BACKUP" ]; then result=present; else result=absent; fi
umount "$mnt" || fail
mounted=0
rmdir "$mnt" || fail
trap - EXIT HUP INT TERM
if [ "$result" = present ]; then echo A04_ESP_JOURNAL_PRESENT; exit 4; fi
echo A04_ESP_JOURNAL_ABSENT
