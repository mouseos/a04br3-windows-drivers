#!/usr/bin/env bash
# Offline staging runner. After launching the test ESP, no reset or restore is
# attempted without a fail-closed, two-frame monitor success. In particular,
# timeout, failure, SIGINT and shell exit must not trigger a restore trap.
set -euo pipefail
if [[ $# != 7 ]]; then
  echo 'usage: run-mma8652-offline-stage.sh SERIAL TEST_BOOT TEST_ESP RESTORE_BOOT RESTORE_ESP ESP_MANIFEST OUTPUT_DIR' >&2
  exit 2
fi
serial=$1; test_boot=$2; test_esp=$3; restore_boot=$4; restore_esp=$5; manifest=$6; out=$7
# Disabled until the unbounded-watchdog boot is rebuilt and validated from
# the required canonical unixnu/uefi-a04br3 tree. The legacy edk2-mtk build
# is not an acceptable provenance and a 420-second boot can interrupt DISM.
echo MMA_CANONICAL_UNBOUNDED_UEFI_NOT_VALIDATED_NO_DEVICE_ACTION >&2
exit 2
[[ $serial =~ ^[A-Za-z0-9._-]+$ ]] || exit 2
for input in "$test_boot" "$test_esp" "$restore_boot" "$restore_esp"; do [[ -f $input ]] || exit 2; done
[[ $(stat -c %s "$test_esp") == 268435456 && $(stat -c %s "$restore_esp") == 268435456 ]] || exit 2
[[ ! -e $out ]] || exit 2
root=$(cd "$(dirname "$0")/../.." && pwd)
uart=${A04_UART:-/dev/ttyUSB0}
uart_log=${A04_UART_LOG:-/media/yuma/Expansion/TAB-A04-BR3/uart-recovery/uart-ttyUSB0-921600-before-restart.log}
flash_log="$root/../../port/gsi/logs/flash.log"
[[ -c $uart && -f $uart_log ]] || exit 2
[[ -f $manifest ]] || exit 2
python3 -c 'import cv2, numpy, PIL' || exit 2
# Unknown, active or activating reset guards must never coexist with servicing.
guard_state=$(systemctl --user show a04br3-stuck-guard.service --property=ActiveState --value) || exit 2
[[ $guard_state == inactive || $guard_state == failed ]] || { echo MMA_RESET_GUARD_ACTIVE_OR_UNKNOWN >&2; exit 2; }
token=$(python3 - "$manifest" "$test_esp" <<'PYCODE'
import hashlib, json, re, sys
with open(sys.argv[1]) as f: m=json.load(f)
h=hashlib.sha256()
with open(sys.argv[2], 'rb') as f:
    for block in iter(lambda:f.read(1048576), b''): h.update(block)
t=m.get('token','')
if m.get('version') != 1 or not re.fullmatch('[0-9a-f]{32}',t) or m.get('image_sha256') != h.hexdigest(): sys.exit(2)
print(t)
PYCODE
) || exit 2
for name in A04_TEST_BOOT_SHA256 A04_TEST_ESP_SHA256 A04_RESTORE_BOOT_SHA256 A04_RESTORE_ESP_SHA256; do
  [[ ${!name:-} =~ ^[0-9a-f]{64}$ ]] || exit 2
done
mkdir -p "$out"
sha256sum "$test_boot" "$test_esp" "$restore_boot" "$restore_esp" > "$out/inputs.sha256"
mapfile -t hashes < <(cut -d ' ' -f1 "$out/inputs.sha256")
[[ ${hashes[0]} == "$A04_TEST_BOOT_SHA256" && ${hashes[1]} == "$A04_TEST_ESP_SHA256" &&
   ${hashes[2]} == "$A04_RESTORE_BOOT_SHA256" && ${hashes[3]} == "$A04_RESTORE_ESP_SHA256" ]] || exit 2
# Main operator must hand over the target already in fastboot. Do not reset it.
fastboot devices | awk '{print $1}' | grep -Fxq "$serial" || exit 2
fastboot -s "$serial" getvar product 2>&1 | grep -q 'product: A04BR3' || exit 2
# Claim the nonce permanently before any device write. A crash cannot release it.
ledger=${A04_OFFLINE_NONCE_LEDGER:-$HOME/.local/state/a04br3-mma8652-nonces}
mkdir -p "$ledger"
( set -o noclobber; printf '%s\n' "$A04_TEST_ESP_SHA256" > "$ledger/$token" ) || { echo MMA_NONCE_ALREADY_USED >&2; exit 2; }
start=$(stat -c %s "$uart_log")
fastboot -s "$serial" flash boot "$test_boot" >> "$flash_log" 2>&1
# After this point even an unsuccessful command may have started WinPE.
# No trap and no failure branch is allowed to reset or restore.
if ! A04_SERIAL="$serial" "$root/tools/recovery/flash-efi.sh" "$test_esp" >> "$flash_log" 2>&1; then
  echo MMA_TEST_LAUNCH_UNCERTAIN_NO_RESTORE >&2; exit 3
fi
if ! python3 "$root/tools/recovery/monitor-mma8652-offline.py" --uart "$uart" --uart-log "$uart_log" \
    --boot-offset "$start" --output "$out" --capture "$root/tools/uart/capture-framebuffer.py" \
    --token "$token" --timeout "${A04_OFFLINE_MONITOR_TIMEOUT:-1800}" | tee "$out/monitor.log"; then
  echo MMA_MONITOR_INDETERMINATE_NO_RESET_NO_RESTORE >&2; exit 3
fi
# Success was established from two current-boot framebuffer snapshots.
UART="$uart" "$root/../../unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh"
ready=0
for ((i=0;i<45;i++)); do
  if fastboot devices | awk '{print $1}' | grep -Fxq "$serial"; then ready=1; break; fi
  sleep 2
done
[[ $ready == 1 ]] || { echo MMA_SAFE_RESET_NOT_CONFIRMED >&2; exit 4; }
fastboot -s "$serial" getvar product 2>&1 | grep -q 'product: A04BR3' || exit 4
fastboot -s "$serial" flash boot "$restore_boot" >> "$flash_log" 2>&1
A04_SERIAL="$serial" "$root/tools/recovery/flash-efi.sh" "$restore_esp" >> "$flash_log" 2>&1
echo MMA_RESTORE_COMMANDS_SUCCEEDED
