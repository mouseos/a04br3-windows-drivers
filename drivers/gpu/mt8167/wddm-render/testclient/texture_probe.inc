/* SPDX-License-Identifier: MIT. Independent standard API/readback oracle.
 * Texture data is a nonuniform2x2 BGRA image, not a substituted shader/color.
 * Each UV is loaded through a real immutable D3D constant buffer. */
static HRESULT texture_probe(ID3D11Device *device,ID3D11DeviceContext *ctx,ID3D11RenderTargetView *view,ID3D11Texture2D *target,ID3D11Texture2D *staging) {
 ID3D11VertexShader *vs=NULL;ID3D11PixelShader *ps=NULL;ID3D11InputLayout *layout=NULL;ID3D11Buffer *vb=NULL,*cb=NULL;ID3D11RasterizerState *rs=NULL;
 ID3D11Texture2D *texture=NULL;ID3D11ShaderResourceView *srv=NULL,*nil_srv=NULL;ID3D11SamplerState *point=NULL,*linear=NULL,*chosen=NULL,*nil_sampler=NULL;
 D3D11_TEXTURE2D_DESC td={0};D3D11_SUBRESOURCE_DATA init={0};D3D11_BUFFER_DESC bd={0};D3D11_RASTERIZER_DESC rd={0};D3D11_SAMPLER_DESC sd={0};D3D11_VIEWPORT vp={0};D3D11_MAPPED_SUBRESOURCE map;
 D3D11_INPUT_ELEMENT_DESC elem={"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};
 const FLOAT vertices[3][4]={{-0.8f,0.8f,0.5f,1},{0.8f,0.8f,0.5f,1},{0,-0.8f,0.5f,1}};const FLOAT black[4]={0,0,0,1};
 const UINT texels[4]={0xffff0000u,0xff00ff00u,0xff0000ffu,0xffffffffu};
 const FLOAT uv[8][4]={{.25f,.25f,0,0},{.75f,.25f,0,0},{.25f,.75f,0,0},{.75f,.75f,0,0},{.5f,.5f,0,0},{.5f,.25f,0,0},{-.25f,1.25f,0,0},{.5f,.5f,0,0}};
 const UINT expected[8]={0xffff0000u,0xff00ff00u,0xff0000ffu,0xffffffffu,0xff808080u,0xff808000u,0xff0000ffu,0xff808080u};
 UINT support=0,pass,x,y,stride=16,offset=0;HRESULT hr;
 hr=ID3D11Device_CheckFormatSupport(device,DXGI_FORMAT_B8G8R8A8_UNORM,&support);line("TEXTURE_FORMAT_HRESULT",hr);line("TEXTURE_FORMAT_SUPPORT",support);
 if(FAILED(hr)||(support&(D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))!=(D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)){hr=DXGI_ERROR_UNSUPPORTED;goto done;}
 td.Width=td.Height=2;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 init.pSysMem=texels;init.SysMemPitch=8;
 hr=ID3D11Device_CreateTexture2D(device,&td,&init,&texture);line("TEXTURE_CREATE_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource *)texture,NULL,&srv);line("TEXTURE_SRV_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateVertexShader(device,geo_position_vs,sizeof(geo_position_vs),NULL,&vs);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreatePixelShader(device,geo_texture_ps,sizeof(geo_texture_ps),NULL,&ps);line("TEXTURE_PS_HRESULT",hr);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateInputLayout(device,&elem,1,geo_position_vs,sizeof(geo_position_vs),&layout);if(FAILED(hr))goto done;
 bd.ByteWidth=sizeof(vertices);bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;init.pSysMem=vertices;
 hr=ID3D11Device_CreateBuffer(device,&bd,&init,&vb);if(FAILED(hr))goto done;
 rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;hr=ID3D11Device_CreateRasterizerState(device,&rd,&rs);if(FAILED(hr))goto done;
 sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxAnisotropy=1;sd.ComparisonFunc=D3D11_COMPARISON_NEVER;sd.MinLOD=-3.402823466e+38F;sd.MaxLOD=3.402823466e+38F;
 hr=ID3D11Device_CreateSamplerState(device,&sd,&point);line("TEXTURE_POINT_HRESULT",hr);if(FAILED(hr))goto done;sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
 hr=ID3D11Device_CreateSamplerState(device,&sd,&linear);line("TEXTURE_LINEAR_HRESULT",hr);if(FAILED(hr))goto done;
 vp.Width=800;vp.Height=600;vp.MaxDepth=1;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&view,NULL);ID3D11DeviceContext_RSSetState(ctx,rs);ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);
 ID3D11DeviceContext_IASetInputLayout(ctx,layout);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&vb,&stride,&offset);
 ID3D11DeviceContext_VSSetShader(ctx,vs,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,ps,NULL,0);ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&srv);
 for(pass=0;pass<8;pass++) {
  UINT interior=0,exterior=0,edges=0,mismatch=0,hash=2166136261u;line("TEXTURE_PASS",pass);if(!log_ok){hr=E_FAIL;goto done;}
  bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;init.pSysMem=uv[pass];hr=ID3D11Device_CreateBuffer(device,&bd,&init,&cb);line("TEXTURE_UV_BUFFER_HRESULT",hr);if(FAILED(hr))goto done;
  chosen=pass<4?point:pass==7?NULL:linear;ID3D11DeviceContext_PSSetSamplers(ctx,0,1,&chosen);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&cb);
  ID3D11DeviceContext_ClearRenderTargetView(ctx,view,black);ID3D11DeviceContext_Draw(ctx,3,0);
  hr=ID3D11Device_GetDeviceRemovedReason(device);line("TEXTURE_REMOVED_REASON",hr);if(FAILED(hr))goto done;
  ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource *)staging,(ID3D11Resource *)target);ID3D11DeviceContext_Flush(ctx);
  hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource *)staging,0,D3D11_MAP_READ,0,&map);line("TEXTURE_MAP_HRESULT",hr);if(FAILED(hr))goto done;
  if(!map.pData||map.RowPitch<3200){ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);hr=E_UNEXPECTED;goto done;}
  for(y=0;y<600;y++)for(x=0;x<800;x++) {
   /* Independent screen geometry oracle, pixel centers, two-pixel edge band.
    * Triangle: (80,60),(720,60),(400,540). No GPU input rewrite. */
   int px=(int)(2*x+1),py=(int)(2*y+1);int left=3*(px-160)-2*(py-120),right=3*(1440-px)-2*(py-120),top=py-120;UINT want,value;const BYTE *p=(const BYTE *)map.pData+y*map.RowPitch+x*4;
   value=p[0]|((UINT)p[1]<<8)|((UINT)p[2]<<16)|((UINT)p[3]<<24);hash=(hash^value)*16777619u;
   if(left>16&&right>16&&top>4){interior++;want=expected[pass];}
   else if(left < -16||right < -16||top < -4){exterior++;want=0xff000000u;}
   else{edges++;continue;}
   for(UINT c=0;c<4;c++){int delta=(int)((value>>(8*c))&255u)-(int)((want>>(8*c))&255u);if(delta < -2||delta>2){mismatch++;break;}}
  }
  ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)staging,0);
  line("TEXTURE_EXPECTED",expected[pass]);line("TEXTURE_INTERIOR",interior);line("TEXTURE_EXTERIOR",exterior);line("TEXTURE_EDGE_EXCLUDED",edges);line("TEXTURE_MISMATCH",mismatch);line("TEXTURE_HASH",hash);
  {ID3D11Buffer *nil=NULL;ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&nil);}ID3D11Buffer_Release(cb);cb=NULL;
  if(!interior||!exterior||mismatch){hr=E_FAIL;goto done;}
 }
 hr=S_OK;
 done:
 ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&nil_srv);ID3D11DeviceContext_PSSetSamplers(ctx,0,1,&nil_sampler);
 {ID3D11Buffer *nil=NULL;ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&nil);}
 if(cb)ID3D11Buffer_Release(cb);if(rs)ID3D11RasterizerState_Release(rs);if(vb)ID3D11Buffer_Release(vb);if(layout)ID3D11InputLayout_Release(layout);
 if(ps)ID3D11PixelShader_Release(ps);if(vs)ID3D11VertexShader_Release(vs);if(srv)ID3D11ShaderResourceView_Release(srv);if(texture)ID3D11Texture2D_Release(texture);
 if(point)ID3D11SamplerState_Release(point);if(linear)ID3D11SamplerState_Release(linear);line("TEXTURE_HRESULT",hr);return hr;
}
