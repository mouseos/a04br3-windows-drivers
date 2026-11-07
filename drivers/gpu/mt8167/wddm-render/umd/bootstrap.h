/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_UMD_BOOTSTRAP_H
#define GEO_UMD_BOOTSTRAP_H
#include "clear_ddi.h"
/* Source-linked real frontend, supplied by the shader/state implementation.
 * No LoadLibrary, user pointer, registry-selected DLL or arbitrary broker.
 * get_caps must describe actual shader/format/state support. This glue does
 * not manufacture FL9_1 capabilities from the existence of Clear. */
struct geo_umd_frontend {
 UINT size,version;
 HRESULT (*open)(const D3DDDIARG_OPENADAPTER *,void **adapter_context);
 HRESULT (*get_caps)(void *adapter_context,const D3DDDIARG_GETCAPS *);
 HRESULT (*initialize_device)(void *adapter_context,struct geo_umd_device *,D3DDDI_DEVICEFUNCS *);
 HRESULT (*quiesce_device)(struct geo_umd_device *);
 void (*destroy_device)(struct geo_umd_device *);
 void (*close)(void *adapter_context);
};
#ifdef __cplusplus
extern "C" {
#endif
HRESULT GeoOpenAdapterWithFrontend(D3DDDIARG_OPENADAPTER *,const struct geo_umd_frontend *);
#ifdef __cplusplus
}
#endif
#endif
