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

## cloneからのfullsource recipe

公開originは8f5d372dの実装11ファイルへ更新し、以前のorigin行全体をprevious_originsとして保持する。native_source_commit・native TU67本のrouting・legacy114ファイルは変更しない。shared_surface_integrationをoriginとnative mapの両方で照合し、以前のorigin行のdigestも検査する。verifierは古いGit履歴もdriver cacheも使わない。

Windows側では既存のfullsource入口を使う。新しい出力先を選び、既存objectのReuseDirectory/ReuseVerifiedDirectoryは渡さない。この入口はnative67TU、全KMD module/bridge/display TU、全UMD TUを同じclean HEADで再compileする。SDK・licensed toolchainの既存ファイルだけを利用し、インストールやSDK再構築は行わない。

```powershell
# このcheckoutのHEADを固定した後に実行。既存のMSVC ARM32/WDK8.1/ARM32 UCRTを使用。
python -B .\tools\ci\verify-ge8300-fullsource.py
if ($LASTEXITCODE) { throw 'fullsource verifier failed' }
.\tools\ci\build-ge8300-from-source.ps1 -EnableCandidate -UnifiedDisplay -OutputDirectory "$PWD\build\shared-surface-owner"
```

これは証拠付きcacheを指定しない通常経路であり、PCO/NIR/support/pairの375TUもsourceからcompileする。SDK再構築は0。Windows上のbuildはこのbranch作業では未実行。

### PCO375だけを再利用する場合

既存cacheに`cache-evidence.json`があり、そのdigestが`link-manifest.json`に記録されている場合だけ再利用できる。新しいsource compiler recipeは各実compileに`-MD`を付け、system includeを含む全dependencyのhashを記録する。source-plan/API/pair、375TUすべてのsourceとflags、objectとlibrary、Clang binary、現在のMicrosoft API libraryが一致する必要がある。さらに現在のsource/全includeの解決先を375回の`clang -M`で再確認する。object compileは行わない。追加されたshadow headerも検出する。

古いcacheにcompile時点の全include証拠がなければ、現在のheaderだけを調べて旧objectの出自を後付け認定しない。指定cacheの検査失敗時は停止し、自動でPCOやSDKの再構築へ切り替えない。

```powershell
# $Pco375にはmainが保有する証拠付きcacheの実在directoryを指定する。
$PcoManifestSha = (Get-FileHash "$Pco375\link-manifest.json" -Algorithm SHA256).Hash
.\tools\ci\build-ge8300-from-source.ps1 -EnableCandidate -UnifiedDisplay -OutputDirectory "$PWD\build\shared-surface-owner-cached-pco" -VerifiedCompilerDirectory $Pco375 -ExpectedCompilerCacheManifestSha256 $PcoManifestSha
```

cacheを別commitから利用しても、compiled_source_commitは旧commitのまま記録する。current_source_commitと同一扱いに改名しない。検査receiptはcurrent HEAD、link/pair manifest、source-plan、all-include evidenceを結び付け、UMD build時にも照合する。

### 一括manifestと未確認事項

`source-build-manifest.json`が同じHEADのnative owner library・KMD library/SYS・UMD DLLと各子manifestのhashをまとめる。PCOの元compile commit/cache receipt、native/KMD/UMD旧object再利用0、SDK再構築0も記録する。SHARED DEFAULT single mip/slice/sample、既存storage範囲、RT-onlyまたはSRV-onlyという制約を維持し、RT/SRV・blend・DWMはfalseのまま。

Windowsでのリンク/署名/配置・実機/DWM成功は未確認。manifest生成は実際のbuild成功後だけ行う。Linuxのhost/clone verifier通過をWindows build成功と扱わない。
