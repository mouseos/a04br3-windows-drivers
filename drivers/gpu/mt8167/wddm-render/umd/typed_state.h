/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_TYPED_STATE_H
#define GEO_TYPED_STATE_H
#include <windows.h>
#ifndef D3D_UMD_INTERFACE_VERSION
#define D3D_UMD_INTERFACE_VERSION 0x4002
#endif
#ifndef DXGKDDI_INTERFACE_VERSION
#define DXGKDDI_INTERFACE_VERSION 0x4002
#endif
#include <d3d10umddi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GEO_TYPED_STATE_ABI 1u
#define GEO_TYPED_STATE_STAGES 6u
#define GEO_TYPED_STATE_SAMPLERS 16u
/* Stage ordering is independent of tokenized shader enum values. */
enum geo_typed_state_stage { GEO_TYPED_PS, GEO_TYPED_VS, GEO_TYPED_GS,
 GEO_TYPED_HS, GEO_TYPED_DS, GEO_TYPED_CS };
struct geo_typed_state;
struct geo_typed_state_snapshot {
 UINT size, abi;
 D3D11_1_DDI_RASTERIZER_DESC raster;
 D3D10_DDI_DEPTH_STENCIL_DESC depth;
 D3D11_1_DDI_BLEND_DESC blend;
 FLOAT blend_factor[4];
 UINT sample_mask, stencil_ref;
 UINT sampler_bound_mask[GEO_TYPED_STATE_STAGES];
 D3D10_DDI_SAMPLER_DESC sampler[GEO_TYPED_STATE_STAGES][GEO_TYPED_STATE_SAMPLERS];
};
enum geo_typed_flat_rejection {
 GEO_TYPED_FLAT_RASTER = 1u, GEO_TYPED_FLAT_DEPTH = 2u,
 GEO_TYPED_FLAT_BLEND = 4u, GEO_TYPED_FLAT_TEXTURE = 8u,
 GEO_TYPED_FLAT_SAMPLES = 16u
};
/* H defines these two functions; no dependency on H's device layout. */
struct geo_typed_state *geo_typed_state_from_device(D3D10DDI_HDEVICE);
void geo_typed_error(D3D10DDI_HDEVICE, HRESULT);
HRESULT geo_typed_state_create(struct geo_typed_state **);
void geo_typed_state_delete(struct geo_typed_state *);
void geo_typed_state_install(D3D11_1DDI_DEVICEFUNCS *);
HRESULT geo_typed_state_snapshot(const struct geo_typed_state *, struct geo_typed_state_snapshot *);
/* Single-sample flat draw preflight only, NOT full FL9_1 qualification or a
 * substitute for resource/shader/viewport/native-owner validation. Used masks
 * must come from validated shader metadata, never merely bound resources.
 * has_depth_view concerns the actual bound DSV (not DepthEnable alone).
 * Caller must honor every rejection before publishing any draw command. */
UINT geo_typed_blend_mode(const struct geo_typed_state *);
BOOL geo_typed_scissor_enabled(const struct geo_typed_state *);
HRESULT geo_typed_state_validate_flat(const struct geo_typed_state_snapshot *,
 const UINT used_sampler_masks[GEO_TYPED_STATE_STAGES], BOOL has_depth_view, UINT *unsupported);
#ifdef __cplusplus
}
#endif
#endif
