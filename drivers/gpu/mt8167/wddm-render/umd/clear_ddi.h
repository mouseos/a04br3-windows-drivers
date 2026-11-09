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
struct geo_umd_shader_state;
struct geo_umd_device {
 HANDLE runtime,context;
 void *adapter,*frontend_private;
 struct geo_umd_shader_state *shaders;
 struct geo_umd_device *next;
 UINT initialized;
 D3DDDI_DEVICECALLBACKS cb;
 PFND3DDDI_FLUSH frontend_flush;
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
HRESULT geo_umd_read_buffer(struct geo_umd_device *,HANDLE,UINT offset,UINT bytes,void *);
HRESULT geo_umd_write_private_vertex(struct geo_umd_device *,HANDLE *,const void *,UINT bytes,D3DKMT_HANDLE *);
HRESULT geo_umd_destroy_private_vertex(struct geo_umd_device *,HANDLE *);
int geo_umd_has_external_resources(struct geo_umd_device *);
HRESULT geo_umd_resource_info(struct geo_umd_device *,HANDLE,UINT usage,struct geo_surface_desc *,D3DKMT_HANDLE *);
HRESULT geo_umd_target_extent(struct geo_umd_device *,UINT *,UINT *);
HRESULT geo_umd_target_info(struct geo_umd_device *,struct geo_surface_desc *,D3DKMT_HANDLE *);
HRESULT geo_umd_open_primary(struct geo_umd_device *,const D3DDDI_OPENALLOCATIONINFO *,HANDLE,HANDLE *,struct geo_surface_desc *);
void geo_umd_install_clear_ddis(D3DDDI_DEVICEFUNCS *);
#ifdef __cplusplus
}
#endif
#endif
