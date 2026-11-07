#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
cd "$(dirname "$0")/../.."
: "${TMPDIR:?set TMPDIR inside the project root}"
mkdir -p build
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror \
 tests/gpu_native_varying_pds_test.c \
 drivers/gpu/mt8167/vendor_m1_11/native_varying_pds.c \
 drivers/gpu/mt8167/vendor_m1_11/native_triangle_pds.c \
 -o build/gpu_native_varying_pds_test
./build/gpu_native_varying_pds_test
