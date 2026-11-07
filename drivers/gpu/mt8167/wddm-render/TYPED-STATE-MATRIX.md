# GE8300 WinBlue typed state ABI 1

担当境界はNashの`umd/typed_state.[ch]`と本文のみ。Hはtyped device/entry/
resource/shaderと最終結合、Eはnative owner/packet/IRQ/停止を所有する。
このmoduleはCPU descriptorの作成・検証・binding・snapshotを実装する。
PPP/registerやGPU commandを生成せず、FL9_1 capsも変更しない。

## 固定した一次資料

- 実runner42355353070 WDK8.1 `Include/um/d3d10umddi.h`。
  SHA256 `d8f7678940ac847c10c998417eca611a5ac30c72e080cae17e9f759561520783`。
  既定`D3D11DDI_MINOR_HEADER_VERSION=4`。D3D11_1 raster/blend descriptor、
  D3D10 depth/sampler descriptor、各PFNの型を直接使う。新SDK headerを代用しない。
- [CalcPrivateRasterizerStateSize](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11_1ddi_calcprivaterasterizerstatesize)、
  [CreateRasterizerState](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11_1ddi_createrasterizerstate)。
  Calcはruntime private領域の必要sizeを返す。Createはその`pDrvPrivate`へ構築し、
  Destroyはdriver内部登録を解除する。runtime領域をHeapFreeしない。
- [RSSetState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-rssetstate)、
  [OMSetDepthStencilState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetdepthstencilstate)、
  [OMSetBlendState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetblendstate)、
  [PSSetSamplers](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetsamplers)。
  NULL stateはD3D既定値。特に既定depthは有効、NULL samplerはlinear/clamp。
- Mesa base `0d94b3f397fb9cf8470bb5600ada8af79ff5361a`、
  `src/imagination/vulkan/pvr_arch_cmd_buffer.c`（cull、depth/stencil、blend constants）、
  `pvr_arch_tex_state.c`（sampler/texture state）。参照のみ、移植コピーなし。
  CPU state保存をnative state packetの実装済とみなさない。
- 既存Linux由来`render_stream.c`のfragment streamには`isp_zlsctl`、
  `isp_zload_store_base`、`isp_stencil_load_store_base`が独立して存在する。
  depth/stencilはBOOLの保存だけで動く契約ではなく、surface所有とload/storeが必要。
- base `ebd69a4806518ae5c756cc8e47650aa5c33c5849` の
  `RUNTIME-ENTRY.md`を不足matrix、`frontend/draw_protocol.c`を現在の有限draw契約とする。

## Hへの結合接口

```c
HRESULT geo_typed_state_create(struct geo_typed_state **out);
void geo_typed_state_delete(struct geo_typed_state *state);
void geo_typed_state_install(D3D11_1DDI_DEVICEFUNCS *table);
HRESULT geo_typed_state_snapshot(const struct geo_typed_state *state,
                                struct geo_typed_state_snapshot *out);
HRESULT geo_typed_state_validate_flat(const struct geo_typed_state_snapshot *snapshot,
                                    const UINT used_sampler_masks[6],
                                    BOOL has_depth_view, UINT *unsupported);
```

Hが`geo_typed_state_from_device(D3D10DDI_HDEVICE)`と
`geo_typed_error(D3D10DDI_HDEVICE,HRESULT)`を定義する。opaque stateはcreateで
HeapAllocし、H deviceがpointer保持する。失敗時にcreateはout=NULLを保証する。
CreateDevice失敗rollback/DestroyDeviceでdeleteする。getterは実deviceを検証し、
errorはruntime ErrorCbへ渡す。Hのdevice layoutを本moduleへincludeしない。

installerはCalc/Create/Destroy×4、SetRasterizer/DepthStencil/Blend、6 shader
stageのSetSamplersだけを代入する。他のtable entryは触らない。HがD3D11_1 tableから
WDDM1.3 tableへ各fieldを型付きでexplicit copyする。header layoutをcastしない。
viewport/scissor rectangle、DSV/RTV/SRV、shader解析、deferred contextはH側。

runtime直列化されたimmediate-contextから呼ぶ。deferred-context tableは提供しない。
handleはdevice内のlive registryをaddress照合後にだけ参照する。別device、kind違い、
destroy済handleを拒否し、再Create時のprivate address重複も拒否する。bound stateは
値のcopyでありDestroy後もdangling pointerを参照しない。各sampler updateは全handleを
検証した後に一括反映する。snapshotもpointerを含まない値copyであり、後続Set/Destroyと
分離する。ただしkernelへこのCPU structを直送せず、H/E既存semantic captureへ変換する。

## 不足matrix

| 項目 | FL9_1との関係 | CPU実装 | native残件 |
|---|---|---|---|
| raster/cull/winding/depth clip/scissor | 必須集合 | descriptor、既定値、bind、snapshot | E packetとH viewport/clip/cull結合 |
| depth compare/write、stencil両面 | 必須集合 | 有効field検証、無効field正規化、StencilRef 8bit | 実DSV format/storage、ISP/ZLS load-store、完了寿命 |
| blend factors/ops/write mask | 必須集合 | descriptor/bind、factor clamp、sample mask | PBE/USC blend出力、対象format検証 |
| point/linear、wrap/mirror/clamp、LOD | 必須集合 | sampler descriptor、6stage×16slot、NULL既定値 | shader TEXLD、SRV実surface、TPU state/residency |
| anisotropic/comparison sampler | FL/formatで要件が異なる | descriptor保存、範囲検証 | native未接続、caps未広告 |
| independent blend/logic op/forced samples | FL9_1最小範囲外 | valid descriptor保存 | native未接続、caps未広告 |
| reduction min/max、text1bit | 本profile範囲外 | filter拒否 | 勝手にlinear等へ置換しない |

Create成功は有効なCPU state objectが作れた意味であり、native対応の宣言ではない。
`validate_flat`は単一RT/単一sample/no-textureの既存有限経路に対するpreflightである。
solid/cull-none/CW/no-bias/depth-clip-on/scissor-off、no-MSAA、full sample mask、
full RGBA write、無blend（またはONE/ZERO ADD恒等blend）のみ通す。
有効DSVとdepth/stencil有効が同時なら拒否。DSVなしはdepth効果なしと判定する。
shaderの実used sampler maskが一つでも立てば、bound samplerがNULLでも拒否する。
未使用samplerのbindだけでtextureを使用したと誤判定しない。

unsupported理由はRASTER/DEPTH/BLEND/TEXTURE/SAMPLES bitの和と
`DXGI_ERROR_UNSUPPORTED`を返す。Hは結果を無視せずdrawを止める。既定cull-backは
native flatへ無条件には通らない。viewport clipping、shader、実resource format、
RT数、native実行可否は別検証が必要で、S_OKをGPU描画成功として扱わない。

## WDK8.1 ARM32結合手順

既存UMD recipeへ`umd/typed_state.c`の1TUを追加する。Hの2つの実adapter関数が
必須link依存であり、代替実装は提供しない。`windows.h/d3d10umddi.h`用にWDK8.1
`Include/shared`、`Include/um`、`Include/km`とCRT headerを使う。
既存UMDと同じ`thumbv7-pc-windows-msvc`、C11、WinBlue、DDI 0x4002でcompileする。
浮動小数演算を含むuser-mode moduleなので通常ARM VFPを許可し、kernel NoFP設定を
流用しない。stack protector/vectorize/slp-vectorizeは既存UMD recipe同様無効。
linkは実UMDのkernel32/CRT依存へ統合する。単独moduleはDLL entry/capsを持たず、
署名済DLL/SYSやWindows標準描画成功を意味しない。実機/REA操作なし。
