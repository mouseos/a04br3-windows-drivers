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

- provider INF: `11/07/2026,0.0.1.3`
- DOD INF: `11/07/2026,0.0.1.1`

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
