#!/bin/sh
# SPDX-License-Identifier: MIT
# Read-only metadata checks. No mount, driver open, module load or property write.
set -eu
if [ "${1-}" != --inspect ]; then
    echo 'usage: recovery-native-preflight.sh --inspect' >&2
    exit 2
fi
uname -m
id
for n in /dev/dri/card0 /dev/dri/renderD128 /dev/pvrsrvkm /dev/pvr_sync /dev/ion; do
    if [ -c "$n" ]; then ls -l "$n"; else echo "MISSING_NODE $n"; fi
done
for n in /system/bin/linker64 /system/lib64/libEGL.so /system/lib64/libGLESv2.so  /vendor/lib64/egl/libEGL_mtk.so /vendor/lib64/egl/libGLESv2_mtk.so  /vendor/lib64/libsrv_um.so /vendor/lib64/libusc.so /vendor/lib64/libIMGegl.so  /vendor/lib64/hw/gralloc.mt8167.so /vendor/firmware/rgx.fw.22.40.54.30; do
    if [ -r "$n" ]; then ls -l "$n"; else echo "MISSING_FILE $n"; fi
done
if command -v getprop >/dev/null 2>&1; then
    echo "ro.hardware.egl=$(getprop ro.hardware.egl)"
    echo "ro.vndk.version=$(getprop ro.vndk.version)"
fi
