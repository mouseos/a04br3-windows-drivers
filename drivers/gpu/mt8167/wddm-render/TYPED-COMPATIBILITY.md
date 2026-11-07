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

`geo_typed_profile_missing()`は製品互換性の欠落を保持する。ユーザー承認の
`-EnableLimitedDevelopmentProfile`でのみ、既定OFFの開発版をbuildする。
そのDLLは標準runtime交渉にD3D11.1 DDI/9_1 pipeline識別子を使うが、FL9_1全機能の
適合宣言ではない。既知の署名clientが使うClear/triangle/readbackを実bring-up対象とし、
未対応shader/state/resource操作は実ErrorCb/unsupported結果を返す。未対応処理を
成功にせず、全互換matrix完成を最初の実GPU確認の一律門にしない。
通常buildではversions/pipelineを広告しない。registryで製品capsを上書きする経路は無い。

### RT8.1のnative DDI経路

搭載RT8.1のd3d11.dllをREAで確認したところ、明示adapterでのFL9_1要求は
`D3D10Level9.dll!D3D11CreateDeviceExternalImplementation`を経由する。
同DLLはlegacy `OpenAdapter`を呼ぶため、`OpenAdapter10_2`だけを実装しても
この要求ではtyped DDIへ到達するとは限らない。根拠はREAの
`FUN_10050528`と`D3D10Level9!FUN_10023f74`。

`build-typed-wdk81.ps1 -EnableNativeDevelopmentPipeline10`は、
`-EnableLimitedDevelopmentProfile`との組合せでのみnative FL10経路の
開発用交渉を有効にする。clientにも同名switchを指定し、明示adapterで
FL10_0を1回だけ要求する。手動DDI呼出し、WARP fallback、OS DLL改変はしない。
既定の交渉は変更しない。FL10全機能の互換性を示すものではなく、上表の
未実装は残り、未対応要求は実ErrorCbで失敗する。描画成功はclientの実画素と
配置DLLの照合で別途判定する。

一次資料は固定WDK8.1 d3d10umddi.h/dxgiddi.h、
[OpenAdapter10_2](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_openadapter)、
[Win8 D3D software requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/software-requirements)、
[FL9_1必須caps](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/required-direct3d-9-capabilities)、
[Flush](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11_1ddi_flush)。
