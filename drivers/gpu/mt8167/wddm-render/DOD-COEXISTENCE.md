# PowerVR GE8300 render候補とMOU0007表示維持

MOU0006は既存KMDF providerと排他的に新render miniportを適用し、DxgkだけがIRQを所有する。MOU0007 DODは残す。別KMDFを同時にIRQ接続しない。この文書はsource確認であり、新renderとの実機表示共存の成功報告ではない。DODファイルはCarver担当のため編集していない。

## 現DOD 1.4の固定出自

回収package `port/xnu/out/windows-rt-brightness-6c665d4` とrun33媒体 `port/xnu/out/private-windows-rt-deferred-media-run33/stage/shared-io/display` のSYS hashは同一:

`71f4ddaec639045a8b7dc1aaa7bc4c341e3f5955e2bec3b00f7a1a91528c7dce`

signed-manifestはbinary source `0c73f273dc470dd3cb07c254808d534709dd711f`、INF source `6c665d43bf4a123605f83f6436562dffce25722f`、DriverVer `11/06/2026,0.0.1.4`、binary_recompiled=false。保存build recipeは `port/gsi/logs/ios-feasibility/windows-rt-brightness-0c73f27-build.ps1` と同 `windows-rt-brightness-6c665d4-package.ps1`。LIVE_FULL_PRESENT/SHARED_AUTOSTART/GPU_DIRECT_POST/DIRECT_EXPERIMENT/SHARED_COMMON等を有効にしているため、通常presentが旧provider経路を試す。

## interface不在時の実source経路

- `../wddm-display/bdd.cxx`: constructorがm_DisplayResourcesをzero。StartDeviceはoptional GPU resource取得失敗だけでDriverStartedを失敗にしない。
- `../wddm-display/ge8300_display_resources.cxx`: PrepareViaGpuは固定 `\Device\A04GE8300` をopenし、失敗ならGpuHandle=NULLで戻る。新renderはこのKMDF名を作らない。冷起動ではLiveReady=FALSEのまま。
- `../wddm-display/bdd.cxx`: TryGpuLogicalPresentはLiveReady=FALSEならSTATUS_NOT_SUPPORTED。
- `../wddm-display/blthw.cxx`: STATUS_NOT_SUPPORTEDなら既存BltBitsへ進む。tile/copy helperの旧provider open失敗もCPU経路へ進む。

従ってinterfaceが消えるだけで必ずCode10/blackになるsourceではない。一方でGPUへPOST/live jobを既に公開した状態からの切替はRetireToCpuの実retirement proofが必要。未証明のlive CPU fallbackはしない。正式更新＋冷再起動で切り替える。

## 次render候補の明示CPU-present設定

主担当がCarverと所有調整し、DOD専用build子プロセスだけ次の環境設定をすべて0にする。既存brightness設定は維持する。render/ownerのbuild環境へ流用せず、既存DOD署名baselineも保持する。

```text
A04_GE8300_WDDM_COPY
A04_GE8300_WDDM_VENDOR_TILE
A04_GE8300_WDDM_FENCED_TILE
A04_GE8300_WDDM_LIVE_FULL_PRESENT
A04_GE8300_WDDM_WIDE_REPEAT
A04_GE8300_WDDM_WIDE_FRAME
A04_GE8300_WDDM_COMMON_FRAME_DIAGNOSTIC
A04_GE8300_WDDM_SHARED_FULL_COMMON
A04_GE8300_WDDM_SHARED_COMMON_PAGE
A04_GE8300_WDDM_SHARED_DIAGNOSTIC
A04_GE8300_WDDM_SHARED_LEASE
A04_GE8300_WDDM_DIRECT_EXPERIMENT
A04_GE8300_WDDM_SHARED_AUTOSTART
A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
A04_GE8300_WDDM_GPU_DIRECT_POST
A04_GE8300_WDDM_EXPLICIT_GPU_FLUSH
A04_GE8300_WDDM_SHARED_PRESENT
A04_GE8300_WDDM_PRESENT_CAPTURE
A04_GE8300_WDDM_OVL_OBSERVE
A04_GE8300_WDDM_GCE_LEASE
A04_GE8300_SHARED_DISPLAY_COMMON
A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC
```

最終DOD build manifestへ実効flagsとSYS hashを記録する。CPUのGDI presentと、標準D3DからのGPU3D/job/CPU-visible結果を分けて回収する。表示が出たことをrender完了と扱わない。

公式根拠: [WDDM driver and feature caps](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/wddm-driver-and-feature-caps) はrender-onlyのrender-specific DDI実装とdisplay-specific DDIをNULLとするモデルを示す。wrapperはsources/children=0、display DDIsを登録しない。Windows RT 8.1で両adapterが正常起動する実証は主担当の結合候補段階で必要。
