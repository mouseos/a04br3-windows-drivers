#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
cd "$(dirname "$0")/../.."
: "${TMPDIR:?set TMPDIR inside project root}"
: "${1:?explicit official source token artifact directory required}"
mkdir -p build
${CC:-cc} -O2 -std=c11 -Wall -Wextra -Werror tests/gpu_native_color_sm4_test.c -o build/gpu_native_color_sm4_test
./build/gpu_native_color_sm4_test "$1"
