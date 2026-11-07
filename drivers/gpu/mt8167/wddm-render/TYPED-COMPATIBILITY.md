# WinBlue typed DDI compatibility matrix

標準入口は実 `OpenAdapter10_2`。DDI ABIはWindows 8.1で使える
`D3D11_1_DDI_INTERFACE_VERSION`、最初の目標feature levelは9_1。
WDDM1.3 tableへlayout castせず、対応版を明示して交渉する。
SM4 token/signatureを実PCOへ渡すためD3D9 ordinalの列挙は前提としない。

| 契約 | 現実装 | 広告を妨げる残件 |
| --- | --- | --- |
| Adapter/typed device | 実QueryAdapterInfo、context、ErrorCb、rollback、heap lifetime | 全必須DDI matrix完了前はversion/FLを広告しない |
| BGRA target/readback | 同VidMm allocation、実Render Clear、実Lock fence、staging copy、RTV参照 | native backing800×600、2048 texture要件に不足 |
| Buffer/constant | 実Allocate/Lock、immutable upload、dynamic write、binding参照 | 64KiB/1slot/32constant限定、FL9_1全slot未対応 |
| Shader | typed WinBlue signature、実SM4 decode→NIR→PCO、shader/binding寿命 | straight-line bounded subset、varying/texture/flow未完 |
| IA/draw | 実VB/IB、float4 layout、list/strip、same-allocation private vertex、RenderCb | 全input format/instancing/viewport/clipping未完 |
| State | Nash typed descriptor registry/snapshot | native depth/blend/texture packet未接続、未対応を拒否 |
| DXGI | 出力なしadapterの標準列挙をclient実装 | DXGI base resource callbacksと必要機能のtable閉包未完 |
| KMD | 実MDL resolver、single Dxgk IRQ、native control-v2 | WDDM1.2必須engine TDR/GDI契約未qualified |

`geo_typed_profile_missing()`は未完了義務を列挙する。現状の
GetSupportedVersionsは0件、3DPIPELINESUPPORTは0。Clearが動くはずという理由で
不完全なFL9_1を広告するoverrideやregistry switchは置かない。これは標準runtime
完成を主張する入口ではない。実DDI機能を順次接続し、このmatrixの欠落を解消して
初めて公開する。単独component build/署名で作業を終えない。

一次資料は固定WDK8.1 d3d10umddi.h/dxgiddi.h、
[OpenAdapter10_2](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_openadapter)、
[Win8 D3D software requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/software-requirements)、
[FL9_1必須caps](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/required-direct3d-9-capabilities)、
[Flush](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11_1ddi_flush)。
