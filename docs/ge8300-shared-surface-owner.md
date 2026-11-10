# 共有surfaceの既存owner取込み

23e44487を基点とする実装。実機、Windowsのプロセス間動作、DWM成功は未確認。

## 対応する契約

D3D10/11 typed CreateResourceでMiscFlagsがSHAREDのみの、DEFAULT・CPU accessなし・single mip/slice/sampleのTexture2Dを扱う。既存native ownerが扱うRT単独BGRAと、SRV単独BGRA/RGBAだけを許可する。共有RTは既存の800×native height/pitch3200 backingとlogical extentを維持し、SRVは既存texture layoutを使う。RT/SRV併用は追加していない。

UMDはポインタを含まないv3 descriptorをpfnAllocateCbのresource private dataとallocation private dataの両方へ渡す。hResourceにはruntimeから渡されたresource handleを指定する。runtimeがkernel resource/allocation handleを返す経路であり、ドライバ独自の共有handleを作らない。

KMDは両descriptorの一致、版、format/usage/bind、実storage形状を検証し、一度だけ既存geo_create→owner.createを実行する。resource handle用の小さな管理情報はallocationと別に持つ。allocation解放とDestroyResourceの同時呼出し、別呼出しに対応する。元のallocationが消えてもresource管理情報を先に失わない。

OpenResourceはOSが開いたallocation handleと両descriptorを検証し、そのhandleを既存backend resourceへ結び付ける。Allocate、native resource作成、画素コピー、擬似完了は行わない。primaryのv2取込みと共有surfaceのv3取込みは別経路とし、共有surfaceをprimaryへ誤分類しない。

## 参照と同期

既存KMD OpenAllocationはDxgkCbGetHandleDataで同じadapter内の元allocationを取得する。共有descriptorを元allocationの記録と照合し、各deviceに独立したopenを置く。ReadOnlyは既存write resolverで引き続き拒否する。exporterのclose後もimporterのopen_countが元owner resourceの破棄を防ぐ。

CloseAllocationは既存IRQ synchronizerの下で、そのopenを使う未完了job（target/vertex/texture）の存在を確認する。未完了ならBUSYを返し、参照もopenも残す。GPU completionは作らない。実際のRender/Submit/fence、allocationのresidency/MDL/PFN、GPU/CPU cache処理は既存経路を使う。共有API自体にproducer/consumerの完了を仮定せず、アプリケーションが必要な所有権・順序を確立する必要がある。

resource管理情報が残るStopは既存owner drain後のHALTED/cleanup可能状態を維持する。allocation・deviceと同様、管理情報の解放が済むまでownerの最終releaseを報告しない。Removeも管理情報が残る場合はBUSY。

## この変更に含めないもの

RT/SRV併用、render-to-textureのlayout/cache遷移、blend/scissor/可変viewport、部分コピー、GDI互換、keyed mutex/NT handle/cross-adapterの新しい契約・capsは含まない。受入flagsやfeature levelの拡張だけでDWM対応とはしない。Present経路と既存固定clientも変更しない。

## 検証とビルド時の注意

`python tests/ge8300_shared_surface_host_test.py --output <プロジェクトroot内の出力先>`で実production UMD helper、KMD create/open/close/destroy本体とclear_engineをhostで実行する。OS callback、pool、IRQ同期、native ownerはmock。ASan/UBSan付きでrender-only/unified両geometryを確認する。descriptor不一致、未対応bind/flags、確保失敗、再openの無再確保、exporter先close、未完了jobのclose拒否、allocation/resource別解放を検証する。これは実OSの並行性やプロセス間同期を実証しない。

既存copy residency、texture lifetime、render residency hostテストも対象。保存済みMicrosoftヘッダを使うClang ARM構文確認はリンク・署名・実機動作を保証しない。

geo_allocation、geo_engine、geo_kmd_adapterのlayoutが変わる。変更した.cだけを旧KMD/native libraryへ差し替えてはならない。これらのheaderを読む全translation unitを同じsource/設定から再コンパイルし、構造体offsetの混在を防ぐ。UMDも対応v3と一組で更新する。旧v1非共有/v2 primary descriptorは維持するが、旧KMDで新共有descriptorを利用することはできない。

## 一次資料

- Microsoft WDK8.1相当の保存済みd3d10umddi.h: RESOURCE_MISC_SHARED=2、D3D10DDIARG_OPENRESOURCEのper-resource private dataとallocation情報。
- 保存済みd3dumddi.h: D3DDDICB_ALLOCATEのhResource/hKMResource/per-resource private data、D3DDDICB_DEALLOCATE。
- 保存済みd3dkmddi.h: DXGKARG_CREATEALLOCATION/OpenAllocation/DestroyAllocation、resourceとallocationの独立した寿命。
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddicb_allocate
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_openresource
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_createallocation
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_openallocation
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/handling-resource-creation-and-destruction
