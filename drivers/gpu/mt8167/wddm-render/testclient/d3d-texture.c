/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One standard D3D11 sequence: prior clear/triangle/color -> real2D sampling.
 * Separate executable; legacy f3af client bytes are never replaced. */
#define GEO_TEXTURE_CLIENT 1
#define GEO_COLOR_PAIR_CLIENT 1
#define GEO_NATIVE_PIPELINE10_CLIENT 1
#include "d3d-clear.c"
