# TAB-A04-BR3 Windows ドライバ

## 現行の実装・検証範囲（2026-11-08、run109開始時点）

本repoはTAB-A04-BR3のWindows RT 8.1 ARM32向けdriver sourceを管理する。
以下の初期記録にある「未接続」「初期化だけ」といった記述は各試験時点の履歴であり、現在の全実装状態を表すものではない。

GE8300は実native factory/control/MDL/apertureをKMDへ結合し、署名SYSの実機run107でRender予約、独立SpanPin、NativeBind、Mapの成功まで確認された。
その後Patchで119/3となった。private DMA cursor消費を訂正した320はmainがrun109実機試験中で、結果は未確定。
通常の標準D3D描画・画素一致/readbackの完遂はまだ成功していない。限定10_0開発profileの到達を一般D3D対応と扱わない。

現在のsource出自とGPUの到達点は[render README](drivers/gpu/mt8167/wddm-render/README.md)、公開単一checkoutの手順は[source build手順](tools/ci/README-ge8300-source-build.md)を参照。
署名packageのclient f4f1b4f9/UMD3f8291d0/native fb217359と、公開single-checkoutから再compileした未署名buildは別の成果物である。
全device完成・長期安定・一般配布可を宣言するものではない。

## 初期説明と経過記録（原文を保持）

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
