#!/usr/bin/env bash
# Build installed-Windows update media for BU21025, LC709203F and MMA8652 from
# the verified CPU/USB audit media and a signed package directory containing
# touch/, battery/ and accel/.
# usage: build-installed-drivers-update-media.sh <base-media-dir> <package-dir> <out-dir> <wimtools-dir> <python-fat-dir>
set -euo pipefail
base=$1 pkg=$2 out=$3 wt=$4 pyfat=$5
update_command=${6:-run-update-installed-drivers-once.cmd}
here=$(cd "$(dirname "$0")" && pwd)
wim() { LD_LIBRARY_PATH="$wt" "$wt/wimlib-imagex" "$@"; }
echo "0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4  $base/maintenance-staged.wim" | sha256sum -c -
echo "22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce  $base/maintenance-staged-esp.img" | sha256sum -c -
for f in touch/shared-touch.inf touch/bu21025.sys touch/bu21025.cat battery/lc709203f.inf battery/lc709203f.sys battery/lc709203f.cat accel/mma8652.inf accel/mma8652.sys accel/mma8652.cat; do test -s "$pkg/$f"; done
mkdir -p "$out/stage"
test ! -e "$out/update.wim"
cp "$base/maintenance-staged.wim" "$out/update.wim"
sed 's/$/\r/' "$here/$update_command" > "$out/stage/run-update-once.cmd"
wim extract "$out/update.wim" 1 /Windows/System32/startnet.cmd --dest-dir="$out/stage" --no-acls >/dev/null
tr -d '\r' < "$out/stage/startnet.cmd" \
  | sed 's#^call X:\\a04br3\\run-audit-once.cmd$#call X:\\a04br3\\run-update-once.cmd#' \
  | sed 's/$/\r/' > "$out/stage/startnet.new"
grep -q 'run-update-once.cmd' "$out/stage/startnet.new"
if [[ "$update_command" == run-update-mma8652-once.cmd ]]; then
  # A sensor package update does not require CPU load, USB observation or GPU
  # diagnostics. TOPRGU is already a boot driver in this pinned WinPE image.
  printf '@echo off\r\nmountvol /N\r\nwpeinit\r\nif errorlevel 1 exit /b 2\r\nmountvol /N\r\ncall X:\\a04br3\\run-update-once.cmd\r\n' > "$out/stage/startnet.new"
fi
printf 'add %s /a04br3/run-update-once.cmd\nadd %s /Windows/System32/startnet.cmd\nadd %s /a04br3/upd/touch\nadd %s /a04br3/upd/battery\nadd %s /a04br3/upd/accel\n' \
  "$out/stage/run-update-once.cmd" "$out/stage/startnet.new" "$pkg/touch" "$pkg/battery" "$pkg/accel" | wim update "$out/update.wim" 1
wim optimize "$out/update.wim"
wim verify "$out/update.wim"
wim info "$out/update.wim" | grep -q 'Boot Index:     1'
PYTHONPATH="$pyfat" python3 "$here/../../../a04br3-windows-ge8300-copy-safe-stop/tools/winpe/replace-esp-wim.py" \
  --template "$base/maintenance-staged-esp.img" --wim "$out/update.wim" --output "$out/update-esp.img" \
  --template-bcd-sha256 65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d \
  --template-wim-sha256 0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4
sha256sum "$out/update-esp.img" "$out/update.wim"
