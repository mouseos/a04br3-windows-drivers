# TAB-A04-BR3 Windows ドライバ

TAB-A04-BR3のWindows RT 8.1 ARM32向けドライバをまとめるリポジトリです。ストレージ、入力、USBなどを機能別のディレクトリに置き、ドライバごとにリポジトリは分けません。UEFIの共通コードやWindowsのバイナリは含めません。

## 現在の開発ソース（2026-11-08）

開発中のソースとビルド手順は、下記の公開ブランチに保存しています。GPUはKMD・native owner・UMD・compilerを単一checkoutから構築する手順を用意しています。各候補の固定ソースとビルド済みパッケージを混同せず、対応する機種UEFI・署名・設定を揃えてください。

Windows RT本体の起動、タッチ、４CPU、電池表示・LC709203Fの電池温度取得（約27℃）、加速度センサーの回転検出を実機で確認しています。回転方向の補正と、各デバイスの電源遷移・安定性は継続作業です。 MT6392の標準UEFI GetTime接続後、Windowsの時計が従来の2014年から2026年へ変わり、同起動のRTCとWindowsローカル暦の差は約0.75秒でした。タイムゾーンと長期精度は未確認です。

PowerVR GE8300は、標準Direct3Dで異なる２色の描画・stagingへのコピー・Map読戻しが連続で成功し、各800×600の480,000画素一致・不一致0を確認しました。今回の処理ではブルースクリーンなし。3.23では三角形用VS/PS・入力レイアウト・頂点/定数バッファの作成まで実機で成功しましたが、描画結果は全面黒でした。GPUジョブ完了と同じ起動の画素読戻しを照合し、PPP viewportのoffset/scale順序逆転を一次ソースと生成処理で特定しました。3.25を実機適用し、２色readbackと三角形用資源の作成成功を再確認しましたが、その後に予定外再起動が起き、三角形の画素結果は未確認です。3.27は標準SRV/SAMPLEと実texture allocation・sampler・MDL寿命を接続し、非一様2×2画像でPOINT/LINEAR等を検査する新clientまで署名回収しました。実機samplingと再起動原因の解決は未確認です。一般のDirect3Dアプリ、全３D機能、Windows UI全体へのGPU適用が完成したという意味ではありません。

|対象|ソース・手順|状態|
|---|---|---|
|GE8300の単一checkoutビルド・texture候補|[work/ge8300-texture-filter327](https://github.com/mouseos/a04br3-windows-drivers/tree/work/ge8300-texture-filter327)|3.27 source recipeへoverlayを統合。通常shallow checkoutのsource gateを検査済み。署名候補は既存compilerを使う増分ビルドで作成し、全compilerの再ビルドとは区別。標準texture clientの実機結果は未確認|
|GE8300の一般ALU候補|[work/ge8300-native-shader329](https://github.com/mouseos/a04br3-windows-drivers/tree/work/ge8300-native-shader329)|実post-RA IRからUSC全byteを検証する3.29署名候補。memoryless ALU/VS UVSW限定、実機未適用。一般Direct3D全面対応ではない|
|GE8300のreset修正|[work/ge8300-implemented-reset322](https://github.com/mouseos/a04br3-windows-drivers/tree/work/ge8300-implemented-reset322)|固定BVNCの実装済みreset controlを検査する3.22。全idle/FW OFF/IRQ drain/platform OFF条件を保持し、標準Direct3Dの２色連続readbackを実機確認|
|GE8300のRT8.1シェーダー署名修正|[work/ge8300-rt-shader-signature323](https://github.com/mouseos/a04br3-windows-drivers/tree/work/ge8300-rt-shader-signature323)|実ランタイムとWDKの契約に基づく3.23署名候補。VS/PS作成・２色readbackを実機確認。三角形は全面黒で、後続viewport修正が必要|
|GE8300のviewport順序修正|[work/ge8300-blacktriangle119](https://github.com/mouseos/a04br3-windows-drivers/tree/work/ge8300-blacktriangle119)|offset/scaleをPPP契約へ合わせた3.25を実機適用。２色readback後に予定外再起動し、三角形の修正成功は未確認|
|GE8300のWDDM aperture所有権・GPUコピー統合|[integrate/copy334-wddm-aperture](https://github.com/mouseos/a04br3-windows-drivers/tree/integrate/copy334-wddm-aperture)|Windows供給のlocked MDL/PFNとresident leaseに所有権を接続。旧packet DMA方式は採用しない。GPU CopyResource入口・実IRQからschedulerへの完了通知・資源退役を結合し、予約後エラー時に通常sessionを閉じる3.38を署名回収。3.38の遅延回収ログから、標準Direct3D CopyResourceの実GPU転送で２色とも各480,000画素一致・不一致0を確認（RT CPU Lock/memcpy fallbackなし）。後続のCopy中に予定外再起動。3.40は初回CopyのPatch失敗で119/3となり、cold/live初回arm契約を修正した3.41を署名回収。3.41は初回GPU Copyの480,000画素一致をGPR2へ耐久保存でき、初回119/3を今回観測しなかった。遅延回収ログではUnmap成功・RemovedReason0・PASS1の後、2回目ClearのRenderCbが未帰還。後続再起動は残り、Unmap完了・２色目・三角形画素成功は未確認|
|GE8300の同一FW上のClear・Copy|[work/ge8300-tq-clear342](https://github.com/mouseos/a04br3-windows-drivers/tree/work/ge8300-tq-clear342)|3.42はCopyのcold配置時にRT向けEOTを確保し、後続Clearを同じTQ contextへ送る。毎ClearのFW stop/rebuildを避け、実UFO/cache/IRQ/scheduler経路を保持。native7 TUとidentity miniport1 TUを増分ビルド・署名し、原341から無変更の80-member archive構成、UMD/PCO/FW/clientを照合。実機未適用、三角形・通常desktop Present・reset解消は未確認|
|GE8300の初回Copyと継続Copy契約|[fix/gpu341-resident-copy](https://github.com/mouseos/a04br3-windows-drivers/tree/fix/gpu341-resident-copy)|3.41はcold資産準備→boot/first-armと同FW rootでの再armを接続。全ticket/residency事前照合・prepared再Patch保持。実機で初回Copyの480,000画素一致を確認、後続再起動は残る。一般の119解消や三角形描画成功とは主張しない|
|GE8300の全allocation Lock指定|[LockEntire修正source](https://github.com/mouseos/a04br3-windows-drivers/commit/26b7c8ef2034bb41265dbd4347d952b7a838d7a8)|3.35を実機適用。RT/staging Lock・CPUコピー・Map通過を記録したが、その後に予定外再起動。単独修正では未解決|
|MT8167共有親・USB host所有権・温度v3|[work/mt8167-parent1115-thermal-v3](https://github.com/mouseos/a04br3-windows-drivers/tree/work/mt8167-parent1115-thermal-v3)|単一親1115にUSB1014、HP/GPIO/RTCとthermal record v3を統合。親1115の実選択を確認。USB実給電・転送、温度取得は未確認|
|MT8167温度のruntime proof消費修正|[fix/mt8167-thermal-acpi-consume1117](https://github.com/mouseos/a04br3-windows-drivers/tree/fix/mt8167-thermal-acpi-consume1117) / [matched機種UEFI](https://github.com/mouseos/uefi-a04br3/tree/fix/mt8167-thermal-acpi-consume)|実親1116/thermal1.10のstatusは校正/親query成功、測定C00000A2・parent段階2。runtime volatile変数削除がUEFIで拒否されるため、共通EDKを変えずSHR0 serialized ACPI latchへ一回消費を移した親1117＋UEFI候補を生成。署名・実AMLを検証、実機測温は未成功。旧新混載は許可しない|
|MT6630 Wi-Fi接続・QoS統合|[work/mt6630-connect-qos1020-20261108](https://github.com/mouseos/a04br3-windows-drivers/tree/work/mt6630-connect-qos1020-20261108)|1012のscan成功を基点に接続処理とQoS対応を統合。1020を実機適用して開始を確認。接続・通信は未確認|
|Samsung AJTD4R eMMC / MT8167 MSDC0|[work/msdc-ajtd4r-request-mdl](https://github.com/mouseos/a04br3-windows-drivers/tree/work/msdc-ajtd4r-request-mdl)|要求単位のMDL再利用と実ハード名を反映した1.9を実機適用。実速度は未確認|
|MT8167 USB 2.0|[work/peripheral-next-usb1010-wifi1020-sd8](https://github.com/mouseos/a04br3-windows-drivers/tree/work/peripheral-next-usb1010-wifi1020-sd8)|boot HID controller 1010を実機適用し開始を確認。実host給電・列挙・入力と標準USB host全機能は未確認|
|MT6392 RTC / MT8167温度|[work/mt8167-thermal-rtc1101-20261108](https://github.com/mouseos/a04br3-windows-drivers/tree/work/mt8167-thermal-rtc1101-20261108)|単一親1101とRTC/thermal子を実機適用。MT6392 RTCの日時取得を確認。温度取得は未成功|
|RTC/thermalに対応する機種UEFI|[work/rt-thermal-rtc1101-post-20261108](https://github.com/mouseos/uefi-a04br3/tree/work/rt-thermal-rtc1101-post-20261108)|POST表示を保持した8range構成。共通EDKコードは変更しない|
|MT8167 keypad / MT6392 keys|[work/mt8167-keypad-windows-home](https://github.com/mouseos/a04br3-windows-drivers/tree/work/mt8167-keypad-windows-home)|Volume Up/Homeを実board入力へ接続する1.2署名候補。Homeは標準Keyboard Left GUI。実キー入力とStart動作は未確認|
|LC709203F 電池温度・通知|[work/lc709203f-status-notify-20261108](https://github.com/mouseos/a04br3-windows-drivers/tree/work/lc709203f-status-notify-20261108)|1.7を実機適用し、標準温度queryから300.4 K（約27.25℃）を取得。実thermistor mode時だけ標準温度queryへ返し、status通知criteriaを実装。温度取得は実機確認済み。非同期通知は未確認|
|MT8167 AFE / ACCDET|[work/mt8167-afe-jack-sense](https://github.com/mouseos/a04br3-windows-drivers/tree/work/mt8167-afe-jack-sense)|AFE1027は標準jack propertyへ資格付きsenseを接続。親1115とAFE1033を実機適用し、ACCDET readと標準KS jack property取得、固定filter openを確認。AFE1034では標準WASAPIのInitializeから48kHz・2ch・48,000frameの無音PCM再生、DMA/IRQ進行、Stopまで実機で成功。可聴音と物理挿抜は未確認|
|MT8167 thermal資源競合修正|[work/mt8167-thermal-parent1103-20261108](https://github.com/mouseos/a04br3-windows-drivers/tree/work/mt8167-thermal-parent1103-20261108)|実機Problem12の親子二重resource要求を除去した候補。ACCDETの別1103候補とは同版のまま混載せず、後段で親1104へ統合する。UEFI reset assert読戻し0も未解決|
|MT6630 Bluetooth側band|[work/mt6630-sideband-provider](https://github.com/mouseos/a04br3-windows-drivers/tree/work/mt6630-sideband-provider)|本機stock初期化を反映中。clock/drain/latencyが未確定で、Bluetooth動作成功とは扱わない|

以下の2026-11-04の説明は当時の記録です。現在のビルド対象は上記ブランチと、その中の手順を参照してください。

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
