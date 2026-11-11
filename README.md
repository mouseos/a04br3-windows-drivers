# TAB-A04-BR3 Windows ドライバ

## 実runtime379の公開source build入口

`work/public-runtime379-source-build` は実runtime正本 `2f210c9f` のGDI376/377・wire378・trace379を
通常buildのsource closureへ結合したbranch。旧controller6687とはGPU sourceが異なる。
[手順・検証範囲・正式pairとの境界](tools/ci/README-ge8300-source-build.md)を参照。
通常構成には `-EnableCandidate -UnifiedDisplay` が必須。
source gate成功とWindowsでの全compile/link、署名package byte再現、実機成功は区別する。

以下の362説明は当時の履歴。現在のsource対象と検証範囲は上記を優先する。

## 統合描画候補362と現在の入口

現在の公開作業branchは`work/ge8300-unified362`。
[統合内容・検証範囲](docs/ge8300-unified362.md)と[単一checkoutのbuild手順](tools/ci/README-ge8300-source-build.md)を参照。

```powershell
git clone --depth 1 --single-branch --branch work/ge8300-unified362 https://github.com/mouseos/a04br3-windows-drivers.git
Set-Location a04br3-windows-drivers
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -UnifiedDisplay
```

この入口はnative owner/KMD/SEH/display bridge/BDD/MT8167 display transportを同じ統合modeで作り、DEFAULT texture更新と標準評価clientも同じsourceからbuildする。SDK/WDKは正規の既存環境を使用し、古いdriver cacheやprivate hiveを通常buildの入力にしない。生成物は未署名。署名・INF/CAT包装・実機更新は別工程となる。

過去の実機ではWindows RT起動・タッチ・4 CPU・電池/温度取得、標準D3D11の三角形と2×2 texture描画を確認している。現在の362候補は、統合描画のCode 43につながるSetPalette登録漏れを修正し、DEFAULT texture UpdateSubresourceUPを追加したもの。362の実機でのCode 43解消・通常DXGI Present/Windows画面描画と、上記cold-buildのWindowsでの完走は未検証。source出自照合とPowerShell構文確認の成功を実機成功とは扱わない。

以下は以前の世代の実装・検証履歴として保持する。


## 後続332：ABI5機能＋readback安全化（実GPU未検証）

[332の契約と限界](docs/ge8300-readback-safety332.md)。331機能へlegacy SG DMA Get/PutのDISPATCH契約修正9d43d5a0を統合し、実LockCbの非待機・期限管理・失敗保持と段階記録を追加する。旧署名clientは保持し、新clientは`ge8300-d3d-texture-boundary.exe`。

```powershell
git clone --depth 1 --single-branch --branch work/ge8300-readback-safety332 https://github.com/mouseos/a04br3-windows-drivers.git
Set-Location a04br3-windows-drivers
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests
```

実host回帰・必要TU増分compile/link/signは確認したが、131resetの因果/解消と実GPUは未検証。UMDのDeviceHung通知やTDR記録を、未停止DMAの成功停止・scheduler回復・OS再起動防止へ読み替えない。旧327/329/330/331および現媒体は凍結保持する。

## 行列VS＋SAMPLEの後続331（実GPU未検証）

[ABI5の実装範囲](docs/ge8300-matrix-texture331.md)。330の実生成行列VSと327の実生成SAMPLE PSを、実texture backing/lifetimeを保った同じnative jobへ結合する。CB由来UVのみであり、補間varying UV・Present・通常Windows全描画の完成ではない。327resetを修正した候補とも扱わない。

```powershell
git clone --depth 1 --single-branch --branch work/ge8300-matrix-texture331 https://github.com/mouseos/a04br3-windows-drivers.git
Set-Location a04br3-windows-drivers
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests
```

通常source closureはnative61/compiler375TUを保持。最新の実検証はhostの実PCO→ABI5→native/PDSと拒否境界、必要TUのWindows増分buildであり、375TU全build/CI成功や実GPU資格を意味しない。旧327/329/330署名候補・現媒体は凍結する。

## VS定数バッファ・行列shader後続330（実機未検証）

[330の実装と一次source](docs/ge8300-vs-uniform330.md)。VS/PS CB0先頭4vectorを実captureし、VSはPDS DOUTDでcommon storeへ転送する。実生成4行DP4 VS368byteを受理し、各stage512byteを既存owned USC page内で分離する。CPUによる頂点変換・固定USC置換は行わない。

```powershell
git clone --depth 1 --single-branch --branch work/ge8300-vs-uniform330 https://github.com/mouseos/a04br3-windows-drivers.git
Set-Location a04br3-windows-drivers
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests
```

[通常source build手順](tools/ci/README-ge8300-source-build.md)はnative61/compiler375TUの最新closureを検査し、過去driver cache/private hiveを入力にしない。署名候補では必要15TUだけを再compileし、既存SDK/PCO coreを保持した。通常375TU全build/CI実行済みとは扱わない。

adapter contract6/ISA0x33001は329と混載禁止。texture ABI3/profile4は保持するが、行列VSとSAMPLE PSの一般合成、一般varying、depth/blend/scissor/Present/通常Windows全描画は未対応。標準API資格・実GPU描画成功は未検証。327/329の署名候補・現媒体は凍結保持する。

以下は以前の独立候補の履歴。

## 実生成ALU shaderのnative後続329（実機未検証）

327/328は凍結し、このbranchで[ABI4 native shader契約](docs/ge8300-native-shader329.md)を実装する。固定token/USC列との一致に代え、実post-RA IRから一次source encoderでUSC全byte・physical register・終了・resource境界を検証する。固定shaderへの差替えや任意USCの無検証受理はしない。

```sh
git clone --depth 1 --single-branch --branch work/ge8300-native-shader329 https://github.com/mouseos/a04br3-windows-drivers.git
```

新規MUL VS/CB演算PSの実NIR/PCO→ABI4→native構築とmalformed入力をASAN/UBSANで検証済み。adapter contract5、texture ABI3/profile4保持。memoryless ALU、VS POSITION4（CB無し）、PS先頭4CB vector、USC各192byteの範囲であり、一般shader/常用描画の全機能や実GPU成功を宣言しない。

[通常source recipe](tools/ci/README-ge8300-source-build.md)はnative61TU/compiler375TUのsource closureを保持する。署名候補の増分buildと375TU全build/CI実行は区別する。

以下は過去の独立候補の履歴。


## 独立compiler後続328（実機未検証）

このbranchは327の通常source closureに[SM4 scalar/vector即値](docs/ge8300-sm4-literals328.md)を統合する。1TUの実ARM link/署名とhost NIR/PCO生成を確認済み。nativeの有限shader受理は残り、一般shader描画完成・媒体適用可能とは扱わない。327署名packageは変更しない。

```sh
git clone --depth 1 --single-branch --branch work/ge8300-shader-literals328 https://github.com/mouseos/a04br3-windows-drivers.git
```

[通常source build手順](tools/ci/README-ge8300-source-build.md)とsource-origin/compiler-planは328差分を含む。372TUの通常fullbuild実行は今回未実施。[327 suite時間予算](docs/ge8300-texture-suite-time-budget.md)はclient900秒/diagnostic phase1200秒の未実測保守値。

以下はこれまでの統合履歴と327以前の実証範囲。


## 現行独立source候補3.27（実機未検証）

326のTexture2D/SRV/SAMPLE/native lifetimeに、通常POINT/LINEAR組合せ・anisotropic descriptorと
独立標準D3D11 texture clientを追加。native IRQ/reset/CPU profileは変更していない。
325/326の署名packageは凍結し、この後続branchでsource/helper/CIを統合する。

```powershell
git clone --depth 1 --single-branch --branch work/ge8300-texture-filter327 https://github.com/mouseos/a04br3-windows-drivers.git
Set-Location a04br3-windows-drivers
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests
```

この通常fullsource経路はnative58TU・compiler372TUをchecked-in sourceから生成し、過去cache/private hive不要。
現hostでの検証は既存依存を保持した必要TUの増分buildとsource gateであり、PCO/SDK全再構築や新実機試験をしたとは扱わない。
新`ge8300-d3d-texture.exe`はclear→triangle→color→非一様2x2画像の複数UV readbackを1processで検査する。
旧f3af clientを置換せず、通常feature-level全資格や一般UI完成は未達。

[Texture契約と一次source](docs/ge8300-texture2d-contract.md)を参照。

## 以前のsource候補3.25（履歴）

native322/KMD320/UMDshader323の単一checkoutへ、PPP viewportのoffset/scale順序修正325を統合。
[通常fullbuild手順](tools/ci/README-ge8300-source-build.md)は過去driver cache/private hive不要。
[修正根拠と限定検証](docs/ge8300-blacktriangle119-viewport.md)を参照。324 laneは未採用。

既存の正規WDK8.1/MSVC ARM32/LLVM/Python環境で、通常fullbuildする。

```powershell
git clone --depth 1 --single-branch --branch work/ge8300-single-checkout325 https://github.com/mouseos/a04br3-windows-drivers.git
Set-Location a04br3-windows-drivers
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests
```


run119の実GPU3.23ではVS/PS作成とDrawRender/Copy/Map/native completionが成功したが、
三角形は全480000pixel黒。色補間へ未到達。source上のviewport誤順序を直した3.25は
主担当の次試験待ちで、**公開fullsource生成物も実機未検証**。一般Texture/UI全描画・
全ドライバ完成とは扱わない。以下は以前のsource候補と到達点の記録。

## 公開単一checkout 3.23 source候補（2026-11-08）

KMD320(f73da1e3)・native322(f9694853)・実UMD基点3f8291d0＋shader323(eb4dca0e)を
一つのtreeへ統合した。[公開source build手順](tools/ci/README-ge8300-source-build.md)はshallow cloneから通常fullbuildし、
旧driver cache/private hive/別checkout/過去Git履歴を要求しない。元DOD/legacyは保持する。

主担当の実3.22では標準D3D2色連続Clear/readbackが各480000画素一致・不一致0・bugcheck無し。
続くTRIANGLE_VS作成は失敗し、323はruntime未初期化signature型fieldの読取りを除く候補である。
**この公開fullsource treeの生成物は実機未検証**で、既存署名package323のbyte同一再包装ではない。
一般shader/三角形/UI全描画・全ドライバ完成とはしない。以下の初期説明は各時点の履歴として保持する。


TAB-A04-BR3のWindows RT 8.1 ARM32向けドライバをまとめるリポジトリです。ストレージ、入力、USBなどを機能別のディレクトリに置き、ドライバごとにリポジトリは分けません。UEFIの共通コードやWindowsのバイナリは含めません。

2026-11-04現在、Windows RTの初期設定は完了し、ROHM BU21025タッチは操作できることを確認しています。タッチの長期安定性、電源管理、その他の周辺機器は引き続き検証中です。ドライバの公開名はBU21025、MediaTek MT8167 MSDC、MT8167/MT6392ボタンなど実際のコントローラに合わせます。

MT8167 MSDCの `A04_EMMC_IDENTITY_HANDOFF=1` は対応UEFIが起動時にCIDから抽出したメーカーIDと製品名を揮発性変数から取得し、SCSI INQUIRYへ反映する既定無効の試験機能です。端末ごとのシリアル番号を使用せず、変数がない場合は従来の汎用名に戻ります。この機能のWindows実機検証は未完了です。

MT8167 TOPRGU watchdogドライバはCPU1の復旧役をWindowsへ引き継ぐための試作品です。３コアのWindows本体でCPU1側の給餌を一時停止し、31秒の期限を超える45秒間、Windows側の給餌だけで動作を維持したことを確認しました。４コア移行時の開始期限、UART復旧、スリープは未検証です。

`A04_TOPRGU_DEADLINE_SECONDS` に正の秒数を指定してビルドすると、その期限でWindows側のwatchdog給餌を停止します。４コア試験でUART復旧が機能しない場合にもwatchdogでfastbootへ戻るための診断用で、通常ビルドの既定値は0（期限なし）です。

４コア所有後のシステムスリープ・休止・シャットダウンではwatchdogを無効にし、復帰時に31秒設定で再武装する処理を追加しています。この間はwatchdogによる自動復帰を利用できず、実機での睡眠・復帰は未検証です。通常の再起動や予期しないデバイス停止ではwatchdogを無効にしません。

テスト署名したストレージドライバと機種固有UEFIを使用し、実機でWindows RT本体を起動しています。物理ボタンと電源管理は未完成です。ストレージは非サポートの仮想StorPort経路を使用しており、DMAの整合性、エラー復旧、電源管理、通常の署名などは未完成です。実用的なドライバとしては使用できません。

## ローカル検証

C11コンパイラとmakeで `make test` を実行します。Clangがある場合、`make arm-coff` はARM32 Windows用COFFオブジェクトの生成を確認します。これらはWindowsドライバのビルドやロード成功を意味しません。

`tools/winpe/build-windows-ntfs.py` は新規のNTFSイメージに限り、ルート属性と `Windows/WinSxS/Manifests` の索引ビットマップを正規化します。メタデータだけの使い捨て画像では、ビットマップの内容を変えずに初期化済み長を補うとWindows Server 2022の読み取り専用CHKDSKが正常終了しました。Windows RT実機での修復や、生成イメージ全体の実機検証は未実施です。この処理を実機ブロックデバイスや既存インストールには使用しません。

Microsoftは `SdPortInitialize` の対象をWindows 10以降と記載しています。そのAPIをWindows RT 8.1で利用できると仮定せず、Windows 8.1 WDKと実際のOSにあるインターフェースを調査しています。Windows Actions上のWDK 8.1とClang/LLDでARM32診断ドライバをビルドし、テスト署名とテストモードでのロードを確認しました。

- [Microsoft: SdPortInitialize](https://learn.microsoft.com/en-us/previous-versions/mt715818%28v%3Dvs.85%29)
- [Microsoft: Windows 8.1のテスト署名](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/test-signing)
- UEFI開発元: https://github.com/mouseos/edk2-mtk （公開ブランチ `windows-rt/arm32`）

## WDK 8.1でのインターフェース検証

`drivers/storage/mt8167-msdc/miniport.c` は未完成の診断コードです。Windows RTのテストモードでDriverEntry、FindAdapter、HwInitializeへの到達とACPI資源の取得を確認しました。診断用の直接MMIOマッピングでGPTのLBA 1を読み、`EFI PART`シグネチャに一致しました。診断後は初期化を意図的に失敗させ、カードには書き込みません。StorPortの通常の資源マッピングや配布用署名は未解決です。

PIO読取りの基礎処理 `pio.c` は、先行ファームウェアが初期化したMSDCから単一セクターを読む実験コードです。MMIO模擬テストとARM32オブジェクト生成はホストで実行できます。独立した初期化と同期、エラー復旧は未完成です。

## 実験用DMA診断

通常ビルドは診断後にアダプターを開始しません。WindowsのWDK 8.1 SSHランナーで `A04_READONLY_DISK_PROBE=1` と `A04_DMA_PROBE=1` を指定すると、StorPortが確保した非キャッシュの連続バッファでGPTの1セクターをbasic DMA読取りする実験版をビルドできます。検証後は意図的に初期化を失敗させます。`dma.c` のホスト模擬テストは `make test` に含まれますが、OSのDMAマッピング、キャッシュ整合性、実機動作を証明しません。

実験の結果、StorPort開始時にE6/1F/04のキャッシュ未整列DMAバッファでWindowsが停止しました。整列マスクを64バイトにしても再発しており、割当元と根本原因は調査中です。StorPortの正式要件はDMAであり、既存PIO試験は実用ドライバではありません。LinuxのMT8167 MSDCドライバはDMAのレジスタ動作を調べる参考にしています。

`A04_DMA_DISK_PROBE=1` を加えると、DMA読取り・REPORT LUNS・INQUIRY・MODE SENSE・READ CAPACITY・READ(10/16)を備える実験的な読み取り専用ディスク列挙を試せます。`A04_ENABLE_WRITES=1` も指定した場合のみWRITE(10/16)の処理をコンパイルし、起動時のEXT_CSDでカードの書込みキャッシュが無効と確認できた場合に限り、DMA書込み後のDAT0とCMD13を確認します。書込みが失敗・タイムアウトした場合は既に媒体に反映された可能性があるため再試行せず、コントローラーを失敗状態にします。診断用の方向別キャッシュ処理を入れた後、指定したWindows領域の先頭１セクターに限ってパターン書込み、読取り比較、元データ復元と比較を実機で確認しました。一般の書込み要求は未検証です。非キャッシュとされる共通バッファで明示的なキャッシュ処理が必要だった理由も未確定です。いずれも電源喪失からの初期化、DMAの完全な整合性、エラー復旧、正式署名は未完成で、起動ディスクや重要データに使用できません。

書込み有効の実験構成では、GPTヘッダーとエントリーのCRC、全エントリーの範囲と重複、Windows基本データ領域とEFIシステム領域の一意性を起動時に確認します。書込み要求はWindows領域またはEFI領域の内側に制限し、境界をまたぐ要求やその他の領域への要求は拒否します。WindowsセットアップがEFIへの書込みを要求した実測に基づく変更で、EFI領域への書込み許可はまだ実機検証されていません。WinPEのARM32診断プログラムからWindows領域のバックアップ済み１セクターに書込み、SCSI経由での読取り比較と復元に成功しました。範囲外拒否の実機試験や障害時の安全性は未検証です。明示フラグがない通常構成では書込みを有効にしません。

`A04_BOOT_EXPERIMENT=1` は、書込み制限と非サポートの仮想要求経路を明示したビルドに限り、boot-startの実験用INFを同じテストパッケージに使用します。このテスト署名構成でWindows RT本体の初期設定画面まで起動したことを実機で確認しました。正式な署名や物理StorPortドライバの代替にはならず、配布用ではありません。
