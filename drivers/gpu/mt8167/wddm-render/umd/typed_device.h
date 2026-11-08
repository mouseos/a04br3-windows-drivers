/* SPDX-License-Identifier: MIT */
#ifndef GEO_TYPED_DEVICE_H
#define GEO_TYPED_DEVICE_H
#include "clear_ddi.h"
#include "tokenized_shader.h"
#include "../adapter_info.h"
#include "../frontend/draw_abi.h"
#define GEO_TYPED_DEVICE_MAGIC 0x32445447u
#define GEO_TYPED_RESOURCE_MAGIC 0x32525447u
#define GEO_TYPED_SHADER_MAGIC 0x32535447u
struct geo_typed_adapter;
struct geo_typed_resource;
struct geo_typed_shader;
struct geo_typed_rtv;
struct geo_typed_layout;
struct geo_typed_query;
struct geo_typed_srv;
struct geo_typed_state;
/* Runtime owns the handle storage; referenced implementation objects live on
 * the heap and may outlive Destroy* when still bound or ambiguously pinned. */
struct geo_typed_handle {void *object;};
struct geo_typed_resource {
 UINT magic,refs,deleted,usage,bind,map_flags,width,height,bytes,mapped;
 DXGI_FORMAT format;D3D10DDIRESOURCE_TYPE dimension;
 HANDLE backend;struct geo_typed_device *device;
 struct geo_typed_resource *next;
};
struct geo_typed_shader {
 UINT magic,refs,deleted,stage;struct geo_umd_tokenized_shader *code;
 struct geo_typed_device *device;struct geo_typed_shader *next;
};
struct geo_typed_rtv {
 UINT refs,deleted;struct geo_typed_resource *resource;
 struct geo_typed_device *device;struct geo_typed_rtv *next;
};
struct geo_typed_device {
 UINT magic,pending;struct geo_typed_adapter *adapter;
 D3D10DDI_HRTCORELAYER core;D3D11DDI_CORELAYER_DEVICECALLBACKS callbacks;
 D3D11_1DDI_DEVICEFUNCS functions;
 struct geo_umd_device backend;D3DDDI_DEVICEFUNCS memory;
 struct geo_shader_compiler compiler;void *compiler_device;
 struct geo_typed_state *state;
 struct geo_typed_resource *resources,*vertex,*index,*constant[2];
 struct geo_typed_shader *shaders,*bound_shader[2];
 struct geo_typed_rtv *views,*target;
 struct geo_typed_layout *layouts,*layout;
 struct geo_typed_query *queries;
 UINT vertex_stride,vertex_offset,index_offset,index_stride,first_constant[2],constant_count[2];
 D3D10_DDI_PRIMITIVE_TOPOLOGY topology;
 D3D10_DDI_VIEWPORT viewport;D3D10_DDI_RECT scissor;
 UINT viewport_set,scissor_set;HANDLE converted;
 HRESULT error;
 HANDLE converted_color;
 struct geo_typed_srv *texture_views,*texture;
};
struct geo_typed_adapter {
 D3D10DDI_HRTADAPTER runtime;struct geo_adapter_memory_info info;UINT devices;HANDLE trace;
};
struct geo_typed_device *geo_typed_device(D3D10DDI_HDEVICE);
struct geo_typed_state *geo_typed_state_from_device(D3D10DDI_HDEVICE);
void geo_typed_error(D3D10DDI_HDEVICE,HRESULT);
void geo_typed_runtime_install(D3D11_1DDI_DEVICEFUNCS *);
void geo_typed_dxgi_install(DXGI1_2_DDI_BASE_FUNCTIONS *);
HRESULT geo_typed_wait_resource(struct geo_typed_resource *);
HRESULT geo_typed_allocation(struct geo_typed_resource *,D3DKMT_HANDLE *);
void geo_typed_resources_install(D3D11_1DDI_DEVICEFUNCS *);
void geo_typed_views_install(D3D11_1DDI_DEVICEFUNCS *);
HRESULT geo_typed_views_unbind(D3D10DDI_HDEVICE);
void geo_typed_shaders_install(D3D11_1DDI_DEVICEFUNCS *);
HRESULT geo_typed_shader_constants(struct geo_typed_device *,UINT,UINT [128]);
HRESULT geo_typed_shaders_unbind(D3D10DDI_HDEVICE);
void geo_typed_draw_install(D3D11_1DDI_DEVICEFUNCS *);
void geo_typed_resource_ref(struct geo_typed_resource *);
HRESULT geo_typed_resource_unref(struct geo_typed_resource *);
struct geo_typed_resource *geo_typed_resource(D3D10DDI_HDEVICE,D3D10DDI_HRESOURCE);
HRESULT geo_typed_read_resource(struct geo_typed_resource *,UINT,UINT,void *);
HRESULT geo_typed_draw_unbind(D3D10DDI_HDEVICE);
HRESULT geo_typed_validate_native_state(D3D10DDI_HDEVICE);
HRESULT geo_typed_submit_draw(D3D10DDI_HDEVICE,const FLOAT [3][4]);
HRESULT geo_typed_submit_pair(D3D10DDI_HDEVICE,const FLOAT [3][8]);
void geo_typed_texture_install(D3D11_1DDI_DEVICEFUNCS *);
HRESULT geo_typed_texture_unbind(D3D10DDI_HDEVICE);
HRESULT geo_typed_texture_allocation(struct geo_typed_device *,struct geo_surface_desc *,D3DKMT_HANDLE *);
#endif
