/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_UMD_CLEAR_DDI_H
#define GEO_UMD_CLEAR_DDI_H
#include <windows.h>
#include <d3d9types.h>
#ifndef D3D_UMD_INTERFACE_VERSION
#define D3D_UMD_INTERFACE_VERSION 0x4002
#endif
#ifndef DXGKDDI_INTERFACE_VERSION
#define DXGKDDI_INTERFACE_VERSION 0x4002
#endif
#include <d3dumddi.h>
#include "../clear_protocol.h"
struct geo_umd_resource;
struct geo_umd_device {
 HANDLE runtime,context;
 void *adapter,*frontend_private;
 struct geo_umd_device *next;
 UINT initialized;
 D3DDDI_DEVICECALLBACKS cb;
 void *commands;UINT command_bytes;
 D3DDDI_ALLOCATIONLIST *allocations;UINT allocation_count;
 D3DDDI_PATCHLOCATIONLIST *patches;UINT patch_count;
 struct geo_umd_resource *target,*resources;
};
/* The full OpenAdapter/CreateDevice implementation must supply these real
 * runtime handles and context buffers. No D3D feature level is advertised by
 * this module, and it does not fill unimplemented shader DDIs with success. */
#ifdef __cplusplus
extern "C" {
#endif
HRESULT geo_umd_clear_init(struct geo_umd_device *,HANDLE,const D3DDDI_DEVICECALLBACKS *,const D3DDDICB_CREATECONTEXT *);
void geo_umd_install_clear_ddis(D3DDDI_DEVICEFUNCS *);
#ifdef __cplusplus
}
#endif
#endif
