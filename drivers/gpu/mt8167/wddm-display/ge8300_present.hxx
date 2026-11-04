/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_PRESENT_HXX
#define A04_GE8300_PRESENT_HXX
#include "bdd.hxx"
/* Returns TRUE only after the whole rectangle has been copied. On FALSE the
 * caller must redraw the entire rectangle using its existing CPU path. */
BOOLEAN A04Ge8300Present(_In_ const BLT_INFO* dst,_In_ const BLT_INFO* src,
                         _In_ const RECT* rect);
#endif
