/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "bootstrap.h"
#ifndef GEO_LINKED_REAL_FRONTEND
#error Link the reviewed shader/caps implementation before building a UMD DLL
#endif
/* Deliberately no fallback definition: absent real frontend is a link error,
 * not a permanently rejecting or success-stub OpenAdapter implementation. */
extern const struct geo_umd_frontend geo_ge8300_frontend;
__declspec(dllexport) HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER *p) {
 return GeoOpenAdapterWithFrontend(p,&geo_ge8300_frontend);
}
