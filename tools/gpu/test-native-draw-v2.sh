#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
cd "$(dirname "$0")/../.."
: "${TMPDIR:?set TMPDIR inside project root}"
: "${1:?explicit source pair artifact directory required}"
mkdir -p build
vendor=drivers/gpu/mt8167/vendor_m1_11
sources=""
for unit in native_clear_job native_clear_data native_terminate_stream native_clear_surface native_clear_codespace native_clear_pds native_eot_program native_record_fields native_pair native_ccb native_triangle_ppp native_triangle_pds native_triangle_suite native_triangle_vdm native_triangle_texture native_draw_job native_varying_pds; do
 sources="$sources $vendor/$unit.c"
done
${CC:-cc} -O2 -std=c11 -Wall -Wextra -Werror -I"$vendor" tests/gpu_native_draw_v2_test.c $sources \
 drivers/gpu/mt8167/wddm-render/clear_protocol.c drivers/gpu/mt8167/wddm-render/frontend/draw_protocol.c \
 -o build/gpu_native_draw_v2_test
./build/gpu_native_draw_v2_test "$1"

owner_sources=""
for unit in windows_native_job windows_native_storage windows_native_cold windows_native_code windows_native_backing windows_native_target windows_native_execute windows_native_submit native_owner_irq native_owner_mdl windows_uncached native_resources native_hwrt_profile render_resources; do
 owner_sources="$owner_sources $vendor/$unit.c"
done
${CC:-cc} -O2 -std=c11 -Wall -Wextra -Werror -DA04_TEST_DISPLAY_SCANOUT \
 -DA04_GE8300_NATIVE_OWNER_MDL_TARGET=1 -DA04_GE8300_NATIVE_OWNER_DRAW=1 -DA04_GE8300_VENDOR_NATIVE_CLEAR_JOB=1 \
 -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -I"$vendor" tests/gpu_native_owner_draw_v2_test.c $sources $owner_sources \
 drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/dma_pages.c \
 drivers/gpu/mt8167/wddm-render/clear_protocol.c drivers/gpu/mt8167/wddm-render/frontend/draw_protocol.c \
 -o build/gpu_native_owner_draw_v2_test
./build/gpu_native_owner_draw_v2_test "$1"
