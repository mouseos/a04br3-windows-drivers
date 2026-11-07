/* SPDX-License-Identifier: MIT */
/* Emit the genuine Microsoft UCRT inline bodies as C++ COMDATs. GNU C
 * private compiler TUs use extern GNU inline; varargs/address-taken calls
 * resolve here. Strong copies would conflict with UCRT's own COMDATs.
 * These are addresses of stock header implementations, not replacement CRT.
 */
#include <stdio.h>
auto geo_crt_stdio_sprintf = &sprintf;
auto geo_crt_stdio_vsprintf = &vsprintf;
auto geo_crt_stdio__snprintf = &_snprintf;
auto geo_crt_stdio__vsnprintf = &_vsnprintf;
auto geo_crt_stdio_snprintf = &snprintf;
auto geo_crt_stdio_vsnprintf = &vsnprintf;
auto geo_crt_stdio_fprintf = &fprintf;
auto geo_crt_stdio_printf = &printf;
auto geo_crt_stdio_vfprintf = &vfprintf;
auto geo_crt_stdio_vprintf = &vprintf;
auto geo_crt_stdio_sscanf = &sscanf;
auto geo_crt_stdio_fscanf = &fscanf;
auto geo_crt_stdio_scanf = &scanf;
