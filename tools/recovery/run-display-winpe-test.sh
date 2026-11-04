#!/usr/bin/env bash
# One combined WinPE display test. Always attempt to restore the supplied
# known-good boot and ESP; no GPT or Windows partition is written.
set -euo pipefail
if [[ $# != 6 ]]; then
  echo 'usage: run-display-winpe-test.sh SERIAL TEST_BOOT TEST_ESP RESTORE_BOOT RESTORE_ESP OUTPUT_DIR' >&2
  exit 2
fi
serial=$1; test_boot=$2; test_esp=$3; restore_boot=$4; restore_esp=$5; out=$6
[[ $serial =~ ^[A-Za-z0-9._-]+$ ]] || exit 2
for input in "$test_boot" "$test_esp" "$restore_boot" "$restore_esp"; do [[ -f $input ]] || exit 2; done
[[ $(stat -c %s "$test_esp") == 268435456 && $(stat -c %s "$restore_esp") == 268435456 ]] || exit 2
[[ ! -e $out ]] || { echo 'Output directory already exists' >&2; exit 2; }
mkdir -p "$out"
root=$(cd "$(dirname "$0")/../.." && pwd)
uart=/dev/ttyUSB0
uart_log=/media/yuma/Expansion/TAB-A04-BR3/uart-recovery/uart-ttyUSB0-921600-before-restart.log
flash_log="$root/../../port/gsi/logs/flash.log"
[[ -c $uart && -f $uart_log && -x "$root/../../unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh" ]] || exit 2
sha256sum "$test_boot" "$test_esp" "$restore_boot" "$restore_esp" > "$out/inputs.sha256"
for name in A04_TEST_BOOT_SHA256 A04_TEST_ESP_SHA256 A04_RESTORE_BOOT_SHA256 A04_RESTORE_ESP_SHA256; do
  [[ ${!name:-} =~ ^[0-9a-f]{64}$ ]] || { echo "Missing or invalid $name" >&2; exit 2; }
done
mapfile -t hashes < <(cut -d ' ' -f 1 "$out/inputs.sha256")
[[ ${hashes[0]} == "$A04_TEST_BOOT_SHA256" && ${hashes[1]} == "$A04_TEST_ESP_SHA256" &&
   ${hashes[2]} == "$A04_RESTORE_BOOT_SHA256" && ${hashes[3]} == "$A04_RESTORE_ESP_SHA256" ]] || {
  echo 'Input hash mismatch' >&2; exit 2;
}

is_fastboot() { fastboot devices | awk '{print $1}' | grep -Fxq "$serial"; }
wait_fastboot() {
  local n
  for ((n=0;n<45;n++)); do if is_fastboot; then return 0; fi; sleep 2; done
  return 1
}
reset_to_fastboot() {
  if is_fastboot; then return 0; fi
  if [[ $(timeout 3 adb -s "$serial" get-state 2>/dev/null || true) == recovery ]]; then
    adb -s "$serial" reboot bootloader || true
  else
    UART="$uart" "$root/../../unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh"
  fi
  wait_fastboot
}
restore_needed=0
restore() {
  local original_status=$?; trap - EXIT
  if [[ $restore_needed == 1 ]]; then
    echo 'Restoring known boot and ESP'
    if reset_to_fastboot && fastboot -s "$serial" getvar product 2>&1 | grep -q 'product: A04BR3' &&
       fastboot -s "$serial" flash boot "$restore_boot" >> "$flash_log" 2>&1 &&
       A04_SERIAL="$serial" "$root/tools/recovery/flash-efi.sh" "$restore_esp" >> "$flash_log" 2>&1; then
      echo RESTORE_COMMANDS_SUCCEEDED
    else
      echo RESTORE_FAILED >&2
      exit 1
    fi
  fi
  exit "$original_status"
}
trap restore EXIT
reset_to_fastboot
fastboot -s "$serial" getvar product 2>&1 | grep -q 'product: A04BR3'
restore_needed=1
fastboot -s "$serial" flash boot "$test_boot" >> "$flash_log" 2>&1
start=$(stat -c %s "$uart_log")
A04_SERIAL="$serial" "$root/tools/recovery/flash-efi.sh" "$test_esp" >> "$flash_log" 2>&1
echo TEST_BOOT_STARTED
found=0
for ((i=0;i<45;i++)); do
  tail -c +$((start+1)) "$uart_log" > "$out/current-uart.log"
  if grep -q 'A04GPU DISPLAY COPY COMPLETE' "$out/current-uart.log"; then found=1; break; fi
  # CAPTURE READY is registration, not a bugcheck. Match only the callback's
  # emitted bugcheck data and preserve the first failure boot in the log.
  if grep -qE 'A04GPU BUGCHECK (DATA_UNAVAILABLE|[0-9A-F]{8})|A04GPU DISPLAY COPY FAILED|A04GPU DISPLAY PNP FAILED|Kernel panic' "$out/current-uart.log"; then break; fi
  # The opt-in service cannot restart after D0 exit. Further waiting cannot
  # produce a GPU copy in this boot, so capture and restore promptly.
  if grep -q 'A04GPU D0EXIT TARGET=' "$out/current-uart.log"; then break; fi
  if grep -q 'A04GPU SERVICE STATUS=' "$out/current-uart.log" &&
     ! grep -q 'A04GPU SERVICE STATUS=00000000' "$out/current-uart.log"; then break; fi
  if (( i > 5 )) && is_fastboot; then break; fi
  sleep 4
done
if (( found )); then echo GPU_COPY_UART_OBSERVED; sleep 25; else echo GPU_COPY_UART_NOT_OBSERVED; fi
tail -c +$((start+1)) "$uart_log" > "$out/first-boot-uart.log"
if ! python3 "$root/tools/uart/capture-framebuffer.py" --log "$uart_log" --uart "$uart" --output "$out/framebuffer.pbm" --timeout 20; then
  ffmpeg -hide_banner -loglevel error -f v4l2 -input_format mjpeg -video_size 1920x1080 -i /dev/video0 -frames:v 1 "$out/camera.jpg" || true
fi
echo TEST_CAPTURE_FINISHED
