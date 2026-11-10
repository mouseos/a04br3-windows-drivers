/* SPDX-License-Identifier: MIT. Standard D3D11, finite CB-coordinate SMP shader.
 * No private rendering IOCTL, WARP, CPU output synthesis or Present assertion. */
#include "ui_texture_oracle.h"
static UINT ui_detail=0xffffffffu;
static int ui_mark(const char *key,UINT stage,UINT status){line(key,status);return geo_progress_write(stage,status,ui_detail)&&log_ok;}
#undef GEO_BOUNDARY
#define GEO_BOUNDARY(key,stage,status) do{if(!ui_mark(key,stage,(UINT)(status))){hr=E_FAIL;goto done;}}while(0)
static BYTE ui_upload[16+(257*4+7)*129+16];
static HRESULT ui_texture_case(ID3D11Device *device,ID3D11DeviceContext *ctx,
 ID3D11RenderTargetView *view,ID3D11Texture2D *target,ID3D11Texture2D *staging,UINT case_id) {
 ID3D11VertexShader *vs=NULL;ID3D11PixelShader *ps=NULL;ID3D11InputLayout *layout=NULL;
 ID3D11Buffer *vb=NULL,*cb=NULL,*nil_cb=NULL;ID3D11RasterizerState *rs=NULL;
 ID3D11Texture2D *texture=NULL;ID3D11ShaderResourceView *srv=NULL,*nil_srv=NULL;
 ID3D11SamplerState *sampler=NULL,*nil_sampler=NULL;
 D3D11_TEXTURE2D_DESC td={0};D3D11_SUBRESOURCE_DATA init={0};D3D11_BUFFER_DESC bd={0};
 D3D11_RASTERIZER_DESC rd={0};D3D11_SAMPLER_DESC sd={0};D3D11_VIEWPORT vp={0};D3D11_MAPPED_SUBRESOURCE map;
 D3D11_INPUT_ELEMENT_DESC elem={"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};
 const FLOAT vertices[3][4]={{-.8f,.8f,.5f,1},{.8f,.8f,.5f,1},{0,-.8f,.5f,1}},black[4]={0,0,0,1};
 UINT w=case_id<2?3:257,h=case_id<2?5:129,rgba=case_id&1,pitch=w*4+7;
 UINT x,y,i,pass,stride=16,offset=0,support=0,n=pitch*h;HRESULT hr;
 ui_detail=case_id<<16;line("UI_CASE",case_id);line("UI_WIDTH",w);line("UI_HEIGHT",h);line("UI_UPLOAD_PITCH",pitch);
 line("UI_FORMAT",rgba?28:87);
 memset(ui_upload,0xa7,sizeof(ui_upload));
 for(y=0;y<h;y++)for(x=0;x<w;x++)ui_store(ui_upload+16+y*pitch+x*4,ui_color(x,y),rgba);
 td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
 td.Format=rgba?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
 td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 hr=ID3D11Device_CheckFormatSupport(device,td.Format,&support);line("UI_SUPPORT_HRESULT",hr);line("UI_SUPPORT",support);
 if(FAILED(hr))goto done;
 if((support&(D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))!=(D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)){hr=DXGI_ERROR_UNSUPPORTED;goto done;}
 init.pSysMem=ui_upload+16;init.SysMemPitch=pitch;
 GEO_BOUNDARY("UI_CREATE_BEFORE",40,0);
 hr=ID3D11Device_CreateTexture2D(device,&td,&init,&texture);GEO_BOUNDARY("UI_CREATE_AFTER",41,hr);if(FAILED(hr))goto done;
 /* Different live resources; never bind the sampled texture as RT or staging. */
 if(texture==target||texture==staging||target==staging){hr=E_UNEXPECTED;goto done;}
 line("UI_DISTINCT_RESOURCES",1);
 hr=ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource *)texture,NULL,&srv);line("UI_SRV_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateVertexShader(device,geo_position_vs,sizeof(geo_position_vs),NULL,&vs);line("UI_VS_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreatePixelShader(device,geo_texture_ps,sizeof(geo_texture_ps),NULL,&ps);line("UI_PS_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateInputLayout(device,&elem,1,geo_position_vs,sizeof(geo_position_vs),&layout);if(FAILED(hr))goto done;
 bd.ByteWidth=sizeof(vertices);bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;init.pSysMem=vertices;init.SysMemPitch=0;
 hr=ID3D11Device_CreateBuffer(device,&bd,&init,&vb);if(FAILED(hr))goto done;
 rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
 hr=ID3D11Device_CreateRasterizerState(device,&rd,&rs);if(FAILED(hr))goto done;
 sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
 sd.MaxAnisotropy=1;sd.ComparisonFunc=D3D11_COMPARISON_NEVER;sd.MinLOD=-3.402823466e+38F;sd.MaxLOD=3.402823466e+38F;
 hr=ID3D11Device_CreateSamplerState(device,&sd,&sampler);if(FAILED(hr))goto done;
 vp.Width=800;vp.Height=600;vp.MaxDepth=1;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&view,NULL);ID3D11DeviceContext_RSSetState(ctx,rs);ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);
 ID3D11DeviceContext_IASetInputLayout(ctx,layout);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&vb,&stride,&offset);ID3D11DeviceContext_VSSetShader(ctx,vs,NULL,0);
 ID3D11DeviceContext_PSSetShader(ctx,ps,NULL,0);ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&srv);ID3D11DeviceContext_PSSetSamplers(ctx,0,1,&sampler);
 for(pass=0;pass<2;pass++) {
  UINT sx=pass?w-1:0,sy=pass?h-1:0,want=ui_color(sx,sy),inside=0,outside=0,edge=0,bad=0,hash=2166136261u;
  FLOAT uv[4]={((FLOAT)sx+.5f)/(FLOAT)w,((FLOAT)sy+.5f)/(FLOAT)h,0,0};
  ui_detail=(case_id<<16)|pass;line("UI_PASS",pass);line("UI_SAMPLE_X",sx);line("UI_SAMPLE_Y",sy);line("UI_EXPECTED_BGRA",want);
  if(!log_ok){hr=E_FAIL;goto done;}
  bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;init.pSysMem=uv;
  hr=ID3D11Device_CreateBuffer(device,&bd,&init,&cb);line("UI_CB_HRESULT",hr);if(FAILED(hr))goto done;
  ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&cb);
  GEO_BOUNDARY("UI_CLEAR_BEFORE",42,0);ID3D11DeviceContext_ClearRenderTargetView(ctx,view,black);
  GEO_BOUNDARY("UI_DRAW_BEFORE",43,0);ID3D11DeviceContext_Draw(ctx,3,0);GEO_BOUNDARY("UI_DRAW_AFTER",44,0);
  GEO_BOUNDARY("UI_COPY_BEFORE",45,0);ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource *)staging,(ID3D11Resource *)target);GEO_BOUNDARY("UI_COPY_AFTER",46,0);
  GEO_BOUNDARY("UI_FLUSH_BEFORE",47,0);ID3D11DeviceContext_Flush(ctx);GEO_BOUNDARY("UI_FLUSH_AFTER",48,0);
  GEO_BOUNDARY("UI_MAP_BEFORE",49,0);hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource *)staging,0,D3D11_MAP_READ,0,&map);
  /* Record even a failed Map; unmap first if the durable writer fails. */
  if(!ui_mark("UI_MAP_AFTER",50,(UINT)hr)){if(SUCCEEDED(hr))ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=E_FAIL;goto done;}
  if(FAILED(hr))goto done;
  line("UI_READBACK_PITCH",map.RowPitch);
  if(!map.pData||map.RowPitch<3200){ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=E_UNEXPECTED;goto done;}
  for(y=0;y<600;y++)for(x=0;x<800;x++) {
   const BYTE *p=(const BYTE *)map.pData+y*map.RowPitch+x*4;
   UINT value=p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);int region=ui_region(x,y);
   hash=(hash^value)*16777619u;
   if(region>0){inside++;if(!ui_matches(value,want))bad++;}
   else if(region<0){outside++;if(!ui_matches(value,0xff000000u))bad++;}else edge++;
  }
  ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);
  line("UI_INTERIOR",inside);line("UI_EXTERIOR",outside);line("UI_EDGE_EXCLUDED",edge);line("UI_MISMATCH",bad);line("UI_READBACK_HASH",hash);
  ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&nil_cb);ID3D11Buffer_Release(cb);cb=NULL;
  if(!inside||!outside||bad){hr=E_FAIL;goto done;}
  hr=ID3D11Device_GetDeviceRemovedReason(device);line("UI_REMOVED_REASON",hr);if(FAILED(hr))goto done;
  GEO_BOUNDARY("UI_PASS_COMPLETE",51,pass);
 }
 /* Keep immutable source and all COM refs alive through both completed Maps.
  * Inspect source bytes and canaries: this is not a GPU output oracle. */
 for(i=0;i<16;i++)if(ui_upload[i]!=0xa7||ui_upload[16+n+i]!=0xa7){hr=E_FAIL;goto done;}
 for(y=0;y<h;y++){
  for(x=0;x<w;x++){BYTE pixel[4];ui_store(pixel,ui_color(x,y),rgba);for(i=0;i<4;i++)if(ui_upload[16+y*pitch+x*4+i]!=pixel[i]){hr=E_FAIL;goto done;}}
  for(x=w*4;x<pitch;x++)if(ui_upload[16+y*pitch+x]!=0xa7){hr=E_FAIL;goto done;}
 }
 line("UI_UPLOAD_GUARDS_UNCHANGED",1);hr=S_OK;
 done:
 ID3D11DeviceContext_ClearState(ctx); /* Removes all bindings before Release; runtime owns in-flight refs on failure. */
 if(cb)ID3D11Buffer_Release(cb);if(rs)ID3D11RasterizerState_Release(rs);if(vb)ID3D11Buffer_Release(vb);
 if(layout)ID3D11InputLayout_Release(layout);if(ps)ID3D11PixelShader_Release(ps);if(vs)ID3D11VertexShader_Release(vs);
 if(srv)ID3D11ShaderResourceView_Release(srv);if(texture)ID3D11Texture2D_Release(texture);if(sampler)ID3D11SamplerState_Release(sampler);
 line("UI_CASE_HRESULT",hr);return hr;
}
static HRESULT ui_texture_probe(IDXGIAdapter *adapter) {
 ID3D11Device *device=NULL;ID3D11DeviceContext *ctx=NULL;ID3D11Texture2D *target=NULL,*staging=NULL;ID3D11RenderTargetView *view=NULL;
 D3D_FEATURE_LEVEL level,wanted=D3D_FEATURE_LEVEL_10_0;D3D11_TEXTURE2D_DESC td={0};HRESULT hr;UINT i;
 GEO_BOUNDARY("CREATE_DEVICE_BEFORE",2,0);hr=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,NULL,0,&wanted,1,D3D11_SDK_VERSION,&device,&level,&ctx);
 GEO_BOUNDARY("CREATE_DEVICE_AFTER",3,hr);if(FAILED(hr))goto done;line("FEATURE_LEVEL",level);if(level!=wanted){hr=E_UNEXPECTED;goto done;}
 td.Width=800;td.Height=600;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
 td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET;
 hr=ID3D11Device_CreateTexture2D(device,&td,NULL,&target);line("UI_TARGET_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource *)target,NULL,&view);if(FAILED(hr))goto done;
 td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 hr=ID3D11Device_CreateTexture2D(device,&td,NULL,&staging);line("UI_STAGING_HRESULT",hr);if(FAILED(hr))goto done;
 for(i=0;i<4;i++){hr=ui_texture_case(device,ctx,view,target,staging,i);if(FAILED(hr)||!log_ok)goto done;}
 done:
 if(ctx)ID3D11DeviceContext_ClearState(ctx);
 if(view)ID3D11RenderTargetView_Release(view);if(staging)ID3D11Texture2D_Release(staging);if(target)ID3D11Texture2D_Release(target);
 if(ctx)ID3D11DeviceContext_Release(ctx);if(device)ID3D11Device_Release(device);return hr;
}
