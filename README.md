# TAB-A04-BR3 Windows ドライバ

TAB-A04-BR3のWindows RT 8.1 ARM32向けドライバをまとめるリポジトリです。ストレージ、入力、USBなどを機能別のディレクトリに置き、ドライバごとにリポジトリは分けません。UEFIの共通コードやWindowsのバイナリは含めません。

現在、WinREは表示できますが、本端末のeMMCとタッチはWindowsのデバイスとして利用できません。`drivers/storage/mt8167-msdc` の診断用StorPortドライバは、テスト署名したWinRE上で起動し、eMMCのGPT先頭をPIOで読み取るところまで実機で確認しました。実用ドライバとしてのディスク公開、書込み、電源管理、DMA、割込みや通常の署名は未完成です。

## ローカル検証

C11コンパイラとmakeで `make test` を実行します。Clangがある場合、`make arm-coff` はARM32 Windows用COFFオブジェクトの生成を確認します。これらはWindowsドライバのビルドやロード成功を意味しません。

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

`A04_DMA_DISK_PROBE=1` を加えると、DMA読取り・REPORT LUNS・INQUIRY・MODE SENSE・READ CAPACITY・READ(10/16)を備える実験的な読み取り専用ディスク列挙を試せます。`A04_ENABLE_WRITES=1` も指定した場合のみWRITE(10/16)の処理をコンパイルし、起動時のEXT_CSDでカードの書込みキャッシュが無効と確認できた場合に限り、DMA書込み後のDAT0とCMD13を確認します。書込みが失敗・タイムアウトした場合は既に媒体に反映された可能性があるため再試行せず、コントローラーを失敗状態にします。書込み処理は模擬試験とビルドのみで実機未検証です。いずれも電源喪失からの初期化、DMAの完全な整合性、エラー復旧、正式署名は未完成で、起動ディスクや重要データに使用できません。
