# D0遷移と保持したDMA上のCPU fallback

常用候補のD0Exit/re-D0Entryを実KMDF callbackへ接続し、DODの描画継続をCPUへ移す。GPUを再起動する実装ではない。主担当の `work/ge8300-wide-repeat` を変更せず、`016b633` 基点の `work/ge8300-d0-fallback` で実装した。`d321736` のホスト互換修正もcherry-pick済みで、同系統への統合時はこの前提コミットを重ねて取り込む必要はない。

## 実際の遷移

- `GpuD0Exit` はGPU受付を閉じて既存のlifetime lockを取り、進行中の同期転送を待つ。active leaseがあるだけでは失敗させない。lease・device reference・DMA adapter・firmware heap・context・scratch・POSTのDMA mappingは保持する。
- 正常な起動済みGPUはISR同期下でwrapper割込み源をmaskし、既存m1.11 context cleanupで全client CCBのdrainと4 contextのRUN/BUSY応答を確認し、既存bounded forced idle/offを実行する。cleanup mask=15、power_off_ack、online=falseのすべてを確認したときだけCPU retirement完了とする。FW応答はfabric quiescenceやDMA解放の証明には使わない。
- poison、command busy、IRQ mask失敗、cleanup失敗、idle/off失敗、不整合応答は失敗として返す。受付は閉じたままにし、CPU安全・suspend成功・DMA解放を捏造しない。再起動の試行で状態を上書きしない。
- re-D0Entryは既存attemptedガードで再stageをしない。閉じたserviceの`GpuPostInterrupts`も成功で終了し、起動callbackエラーによるPnP失敗を避ける。GPUの再開は行わず、CPU描画のみ継続する。
- `A04_GE8300_IOCTL_RETAINED_CPU`（0x00226050、kernel-only METHOD_BUFFERED、16 bytes）はversion=1/bytes=16/token/flags=0を受ける。正しいactive display-common tokenの保持者だけがretirementを要求・確認できる。成功時flags=3（CPU_RETIRED|DMA_RETAINED）。旧provider、応答サイズ不一致、token/ABI/flags不一致をCPU安全と解釈しない。
- DODの通常live present失敗時に同IOCTLでretirementを確認する。失敗したGPU full frameがPOSTを部分的に書いている可能性があるため、確認後はstagingした**全1,024,000画素**をCPUで復元する。dirty rectangleだけで済ませない。以後はGPUを再試行せず従来CPU blitへ移る。停止確認失敗時は描画エラーのままにし、GPUとCPUを競合させない。
- DOD power callbackはpresent mutexと電源状態・shadow無効化を同期する。D3/D0 callback中にproviderへのpower-managed IOCTLやPOST書込みをしない。providerがまだD0でない場合の相互待ちを避ける。adapterがD0以外のpresentは書込みを行わない。monitorの論理電源状態も保存する。blackoutも未知のGPU失敗を無視せず、呼出し元へエラーを返す。

## 成功traceとpackage版

providerのwide upload/timing、DODのB0/B1/B3および形状success traceは通常は初回3回に限定する。失敗報告は抑制しない。初回3画面の全画素検証と、`016b633` のscratch guard制限を保持する。

WinPEで全success traceが必要な場合は、provider/DOD双方のビルド環境へ `A04_GE8300_PRESENT_TRACE_ALL=1` を指定する。既定0。両WDKビルドスクリプトが同じflagをコンパイラへ渡す。クロック・PLL・PMIC・UEFI・表示エンジンの設定変更はない。

- provider INF: `11/06/2026,0.0.1.3`
- DOD INF: `11/06/2026,0.0.1.1`

Root追加・DISM transaction媒体の生成と実機適用は主担当の工程。新INFに対応するcatalogの再生成・署名が必要。

## 一括検証

```
make -j4 test-gpu-d0-fallback test-gpu-vendor-m1-11
```

新しいホスト試験は実際のprovider D0Exit/retirement、IRQ同期mask、DOD SubmitLiveFrame/RetireToCpu、SetPowerState、TryGpuLogicalPresentを抽出して実行する。正常転送、失敗・timeout・poison、重複D0Exit、保持したlease/DMA、未知・偽のretirement応答、部分GPU書込みからの全画面復元、停止証明なしのCPU書込み拒否、D3/D0・monitor状態・trace制限/全出力を検査する。ハードウェア・KMDFスケジューラの模擬成功ではない。

既存m1.11一括試験で、不正firmwareのstaging失敗を成功で返す不具合も検出した。optional clock readがstatusをSUCCESSへ変えた後、parse/layout失敗時にstatusが残っていたため、clock検証後にconfiguration-errorの既定値を再設定した。既存の不正image・alloc失敗・DMA quarantine試験を通す。

参照した同版Linux: [m1.11ED5425693 rgxpower.c](https://github.com/mouseos/android_kernel_benesse_TAB-A04-BR3/blob/eb47f42e1f84fe5716ef239620725f82718a620a/drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693/services/server/devices/rgx/rgxpower.c) のRGXForcedIdleRequest、RGXPrePowerState、RGXPostPowerState。LinuxではFW応答後にRGXDoStop、復帰でRGXDoStartとFW started待ちまで行う。今回それらを実装したと偽らず、既存Windows側bounded command/cleanupとDMA保持に限定してCPUへ移る。

## 未検証と次の試験

WDKによるprovider/DOD全体のlink・catalog・署名、新版DriverVerの搭載OSでの選択、実suspend/resumeとCPU描画継続は未検証。実機・SSH・REA操作は禁止条件に従い行っていない。物理remove時のmapping解放を提供しない。保持したDMAがあるReleaseHardwareは従来どおり拒否する。

主担当はこの差分をd321736系へ統合してprovider/DODを同じpackageとしてビルドし、搭載OSの初回GPU描画、D0Exit、再D0、CPU描画、GPU timeout時の描画拒否を分けて確認する。run18/run19のUEFI b737a0f1は変更しない。UEFI raw snapshot診断は別コミットd9101a8a（commit/push済み、追加boot未生成）。

## 一起動だけの実S0 idle診断

provider/DOD双方のビルド環境へ `A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC=1` を指定する。既定0で、通常buildはS0 idle policyを一切設定せず、復元後の全画素readbackや追加markも出さない。providerはnative copy service、DODはlive full presentを必要とし、不適合な組合せはビルドで拒否する。既存ビルドスクリプトがflagを双方のコンパイラへ渡す。

診断policyは実 `GpuAdd` のpower-managed queue作成後に `WdfDeviceAssignS0IdleSettings` で設定する。`IdleCannotWakeFromS0`、`PowerDeviceD3`、`IdleTimeout=30000` ms、`DriverManagedIdleTimeout`、`IdleDoNotAllowUserControl`、`Enabled=WdfTrue`、`PowerUpIdleDeviceOnSystemWake=WdfFalse`、`ExcludeD3Cold=WdfTrue`。外部wake能力やD3coldは要求しない。割込み・DMA・PLL・PMIC・bus power stateを手動操作してD0 callbackを再現しない。policyを拒否された場合はstatusを保存してUARTへ出し、通常起動をfailさせない。`STATUS_POWER_STATE_INVALID`を成功と読み替えない。

30秒無要求後の実KMDF `GpuD0Exit(target=D3)` と、そのretirement成功を記録する。その後のpower-managed I/O到着でframeworkが実 `GpuD0Entry(previous=D3)` を呼び、CPU-onlyのproviderを復帰させる。成功したこの経路でpolicyのEnabledだけをFalseへ戻し、1サイクル後の自動idleを止める。disarm失敗は独立した失敗markとし隠さない。これらは登録されたcallback経路であり、callbackを直接呼ぶ実装はない。

### WinPE workloadと成功の読み方

主担当の診断用GDI workloadを使用する。初回5秒待ち、consoleを非表示にしてfullscreen 1 frame、その後40秒間静止、次frameを描画する。DOD側にsleep・presentの合成・callbackの直接呼出しは追加しない。初期stage中にidleさせることを避けるため30秒にした。40秒静止中のcaret更新、provider status polling、別のGPU要求、残存power referenceはidle成立を妨げるため、この区間には入れない。30秒は「frameworkがidleと判定してから」の時間であり、policy設定成功だけで遷移成功と判断しない。実際のS0状態を保った同一bootのUART区間で、次の順序を確認する。

| UART mark | 意味 |
| --- | --- |
| `A04GPU IDLE POLICY=00000000 MS=00007530` | 30秒policyの設定成功 |
| `A04GPU IDLE EXIT D3` | 実D0Exit callbackでD3への退出を観測 |
| `A04GPU IDLE RETIRED=00000000` | D0Exit内の既存retirement成功 |
| `A04GPU IDLE REENTRY D3` | 同instanceで実D3退出成功後の再D0Entry |
| `A04GPU IDLE DISARM=00000000` | 一回限りpolicyの無効化成功 |
| `A04GPU IDLE PROOF=00000000` | 再入場後、次presentに伴うRetireToCpu再確認成功 |
| `A04DISP STEP=000000C4 STATUS=00000000` | DODもkernel-only retirement応答のABI/token/flagsを検証済み |
| `A04DISP STEP=000000C5 STATUS=000FA000` | 復元先POSTの全1,024,000画素が期待stagingと一致 |
| `A04DISP STEP=000000C6 STATUS=00000000` | 復元後readback検証の成功 |

C4/C5/C6は診断flag時に各一回だけ出す。C5は実際に復元したPOSTをreadbackして数えた一致数であり、CPU側staging同士の比較ではない。C6不一致時はSTATUS_DATA_ERRORを返してLiveFaultedを立てる。markerの一部欠落、別boot混在、非zero status、一致数不足、C5/C6だけの成功は「実D0遷移からfallbackまで成功」の証拠にしない。normal buildにはreadback loopを含めない。DMAの解放は禁止のまま。

### 公開契約の確認範囲

- [WdfDeviceAssignS0IdleSettings](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdeviceassigns0idlesettings): power-policy owner用。policy設定失敗の戻り値、設定更新時の制約を確認。
- [WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/ns-wdfdevice-_wdf_device_power_policy_idle_settings): IdleTimeoutはms。DriverManagedIdleTimeoutとExcludeD3ColdはKMDF 1.11から、PowerUpIdleDeviceOnSystemWakeは1.9から利用でき、今回の1.13対象範囲内。
- [Supporting Idle Power-Down](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/supporting-idle-power-down): power-managed queueの待機・配信済み要求がなく、StopIdleの未解消referenceがないときにidle計時する。次のI/O要求によるD0復帰はframeworkの経路。
- Microsoft公開Windows-Driver-Frameworksの `src/framework/shared/core/fxdeviceapi.cpp` と公開KMDF 1.15 `wdfdevice.h` のinitializer、field/enum、引数検証も照合した。公開repoに1.13ディレクトリがないため、**公開1.15ヘッダを「実WDK 1.13でcompile済み」とは扱わない**。参照ソースとhashは作業ルートの `port/gsi/logs/ios-feasibility/ge8300-idle-transition/wdk-contract-sources.json` に保存。

ホスト一括試験はflag=0/1の実関数をコンパイル・実行し、30秒policy、D3cold除外、正しいD3 enum、退出失敗時の再入場mark抑止、一回限りのdisarm、復元後readbackの通常build非実行、診断buildの一致・不一致・重複mark抑止を検査する。実KMDF schedulerの遷移をホスト試験で実証したとは扱わない。追加差分の実WDK 1.13ビルド・実機試験は主担当工程。

### INF build日付の修正

provider/DODのDriverVer日付を、主担当が指定したRunner UTC build日 `11/06/2026` へ揃えた。版 `0.0.1.3` / `0.0.1.1` は維持する。旧追記の11/07は以前の値。環境の時計・タイムゾーン変更でInf2Catの判定を回避しない。主担当報告では676fed5のprovider WDK compile/linkは成功し、Inf2Catで停止した。future dateが正確な停止原因かは当時エラー回収中であり、この修正だけでcatalog成功とは扱わない。
