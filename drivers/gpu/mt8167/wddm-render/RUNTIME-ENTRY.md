# WinBlue ARM32 runtime entry qualification

## 現結論（実機適用の前提にしない）

`miniport.c` は StartDevice で NumVidPnSources/children=0、display-specific
DDIをNULLとする独立render-only。MOU0007 DODと同一adapterでもhybrid pairでもない。
`query_adapter.c` の HybridDiscrete/CrossAdapterResource は0であり、DODを
GE8300の出力として広告していない。D3DKMT_ADAPTERTYPEはOSへ問い合わせる型で、
KMDに同名のflagsを成功設定すればD3D9 ordinalが生成されるという契約ではない。

現 `d3d9-flat.c` は `GetAdapterCount` → `GetAdapterLUID(ordinal)` の照合のみ。
LUIDから任意adapterをD3D9へ挿入するAPIではない。WinBlue ARM32がこの独立
render-onlyをD3D9 ordinalとして列挙することを保証する一次資料は未確認。
D3D9に常に存在する／絶対に存在しない、どちらも実測済とはしない。
このclientを標準描画到達の確定経路として実機flashする根拠は不足している。

一方、DXGI `EnumAdapters` は出力なしadapterも列挙すると明記する。今後の標準
entryはそのLUID一致hardware adapter＋D3D11CreateDevice(UNKNOWN)を基準とする。
既存 `d3d-clear.c` はこの選択を実装済みでWARP/REF fallbackを行わない。
ただしclientだけの切替で描画できるという意味ではない。

## 次の最小profileと実装差分

D3D11 API FL9_1→10Level9/D3D9 DDIを最初の候補とする。FL10/11/11.1全部の
機能を先に作るという要求ではない。ただしFL9_1を名乗るには公式の最小capsを
実装する必要があり、現finite flat profileはその要件を満たしていない。

c1e21ca capsはPixelShaderVersion=0、MaxTexture=800x600、MaxPrimitiveCount=1、
Gouraud/texture/filter/depth/stencil未広告。公式FL9_1はPS2.0、2048 texture、
65535 primitive、65534 index、8 texture stages、1 RT、Gouraud、mipmap/cubemap、
point/linear、wrap/mirror/clamp、複数blend、depth/stencil等を要求する。
88624ffは実逐次triangle組立でMaxPrimitiveCount=65535/MaxVertexIndex=65534/1RTを実装したが、残る要件を満たしていない。
一般SM2→PCO compilerの存在はnative draw/stateの対応を証明しない。
E d13ce50のdraw prepareは有限pass-through VS＋constant FS allowlistであり、
一般compiler出力、texture/depth、blend、複数primitiveに対応したとは扱わない。

* H: D3D9 DDI resource/stateを実implementationのまま拡張し、capsは実装と同時に変更。
* E: native drawのshader metadata/VDM/PPP/texture/depth/retained MDL契約を拡張。
* KMD: WDDM1.2 render-only必須のpreemption/engine TDR/GDI contractを完成。
  現Win7 packet-boundary policyをPreemptionAwareに置換するだけでは不可。
* 標準SM4を直接受けるentryを選ぶ場合はtyped token/signature compilerを再利用するが、
  `OpenAdapter10[_2]`、resource/state callbacks、feature-level契約が別途必要。
  現DLLのOpenAdapterはD3D9のみ。tokenized_shaderのcompile helperを入口完成と扱わない。

## 一次資料

* [Render-only DDI契約](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/wddm-driver-and-feature-caps)
* [DXGI EnumAdapters: 出力なしadapterも列挙](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-enumadapters)
* [GetAdapterLUID: ordinalのadapter識別](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3d9ex-getadapterluid)
* [D3DKMT_ADAPTERTYPE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ns-d3dkmthk-_d3dkmt_adaptertype)
* [FL9_1に必要なD3D9 caps](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/required-direct3d-9-capabilities)
* [Windows 8 D3D software requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/software-requirements)

署名済SYS/DLLはsource結合の成果物。runtime列挙、CreateDevice受理、描画、
NoDMA停止の成功を意味しない。既定オフとし、既存KMDF baseline/DODを保持する。
