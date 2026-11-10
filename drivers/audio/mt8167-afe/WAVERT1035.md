# MT8167 AFE / MT6392 WaveRT1035

通常AudioSrvのPCMページを実DMAへ公開する既存speaker1034を基点に、標準DMAイベント通知を追加した候補。可聴出力、Windowsでのlink、通常PnP／電源遷移を含むproduction完成は未確認である。pollingも正規のWaveRT経路であり、1034に通知interfaceがないことだけで、既存の再生失敗原因と断定しない。

## 出自と実ハード経路

fresh198の保存SYS SHA256は`b6d2592a618723b808a4fc55e002637c57ae91afab47e6a60343b0714d229ed7`、46016 bytes。native registry receiptでは`MT8167\AFE`／`Mt8167Audio`、選択INFは`mt8167-afe-speaker1034.inf`／2026.11.8.1034。署名1034のSYSとbyte一致する。署名manifest sourceは`29b4b106ae0c91caf55f73cbb75b4c1d92a438b3`、音声object元は`5418520874e079b826f6e9ced05fe91701556c33`で、19objectの再利用履歴と4TUの再生成を区別する。このbranchは前者を基点とする。

実経路はAudioEndpointBuilder→speaker topology→PortCls WaveRT→同じPortCls MDL→単一HAL SG mapping→AFE DL1→MT8167 INT ADDA／内蔵codecのVOICE_AMP→AU_LOL→MT6392 Int Spk Amp/Class-D。I2S外付けcodecや別SoCの音声回路を導入しない。PMIC/PWRAP、クロック、trim、analogのsingle ownerは既親のままで、子からPWRAPを直接操作しない。

根拠はCT3 GPL commit `7c4782127d4ea11965be00ce1343056be9bfb769`の`a04br3.dts:68`（MT8167 codec、speaker-mode0）、`mt8167_evb.c:334`（Int Spk Amp←AU_LOL）、`mt8167-afe-pcm.c:1971`（period_sizeをframesとしてIRQ_CNTへ設定）。保存Android mixer_paths.xmlのspeakerはLINEOUT Mux=VOICE_AMP。XNU正本では、このWindows音声経路の独立根拠になるMT8167音声driverは確認できていない。既存codecのcold-start DC profile／trimと実測校正の限界は変更せず、未知値を新しく仮定しない。

## 実装

`A04_AUDIO_DMA_NOTIFICATION=1`で、speaker専用StreamがWindows Vista以降の`IMiniportWaveRTStreamNotification`を公開する。HP/captureへspeakerの通知能力やnominal latencyを流用しない。

- `AllocateBufferWithNotification`は1周1回または2回の要求を受け、同じWC／stage1 Normal-NC検査済みPortClsページを使う。buffer copyや固定toneはない。
- hardware IRQ1 counterは`ringBytes / notificationCount / stereoS16FrameBytes`。rate48k、18-bit counter、既存のperiod上限と32-bit DMA範囲を検証する。1回通知でperiodが既存上限524288Bを超える場合などは失敗を返す。
- DL1 IRQ status/enableを判定・ackする既存ISRがDPCをqueueし、DPCが登録されたkernel eventをsignalする。software timer、CPUで生成するprogress、成功時刻の合成はない。coalesced IRQは一度wakeし、失われたhardware期間数を復元したとは主張しない。
- event listはnonpaged、DPCとunregisterをspinlockで排他する。unregister完了後にそのeventをsignalしない。allocation前のevent登録も受け付け、別portへの持越しを拒否する。
- STOP/Free/stream破棄/PnP revokeで通知を切離す。旧世代DPCは既存generationで拒否する。実IRQ faultはpoisonを維持し、一度wakeしてGetPositionの失敗を観測可能にする。faultを完了periodに数えない。
- polling streamは従来の半周IRQと実DL1_CURのGetPositionを維持する。STOP/RUNと同MDL再openも既存retained経路を使う。

`Mt8167StreamBackend`末尾に内部callbackを追加するため、全23TUを同じflagで再生成する。1034のobjectを新layoutへ再利用してはならない。外部audio QI v11、capture v1、HP v2、snapshot型は親1121固定source `6741f2a1cac2cb6622c3b1d49bd2ac118c76471f`とbyte一致し、変更しない。共有親・GPU・USB・EDK2をこのbranch差分で変更／再buildしない。

## 一括buildと署名の引渡し

既存Windows VM／既存SDK／既存clang・lldで次を一度実行する。出力はclean checkout内の新directoryを指定する。ParentTupleは`compile-plan.json`、`signed-tuple-manifest.json`、`package/parent`、`package/hcd`を持つ回収済み1121tupleのroot。

```powershell
powershell -NoProfile -File tools\ci\build-mt8167-mt6392-wavert1035.ps1 `
 -WdkInclude <existing-kit81-Include> -WdkArmLib <existing-kit81-arm-libs> `
 -PortClsImportLibrary <existing-actual-ARM-portcls.lib> `
 -ParentTuple <existing-signed1121-tuple-root> `
 -OutputDirectory <checkout>\build\mt8167-mt6392-wavert1035
```

recipeは既1121 SYS SHA `75419c5e76dc44444663577d402c20aa735839eeb1999f00c1da079c08b6e409`と全tuple memberを照合してから音声だけを生成する。新SDK取得、共有親・client・USB/GPUのbuild、配布先への自動配置はしない。各compile/linkのlog、source/tool/lib hash、ARMNT/import表示を保存し、unsigned SYSとINF1035を返す。PowerShell5用UTF8 BOM付き。新出力のため旧failed／署名済み出力を上書きしない。

mainの既SignerでSYS署名→Inf2Cat `/os:6_3_ARM /uselocaltime`→CAT署名→SYS/CAT署名検証とCAT member検証を行い、署名後SYS/INF/CAT hashを保存する。さらにactual RT8.1 kernel/PortCls exportsと照合してから既存batchへ渡す。recipeのbuild-manifestはunsignedを明記し、これを署名成功のmanifestとして使わない。既存のsource3d／GPU試験profileや媒体をこの手順で置換しない。

## 検証と未完了条件

`make test-mt8167-wavert-notification test-mt8167-dl1`は既存2fixtureとplannerをまとめて実行する。COM interface/invalid count、実sourceのISR→DPC→event、1周/半周counter、STOPの通知抑止、同MDL再openの旧event破棄、旧DPC拒否／IRQ fault poisonを確認する。mock MMIO/parent/HAL/PortClsのホスト検証であり、Windowsのbuild／音出しの証拠ではない。

未完了：kernelとuser aliasの最終実属性、実PCM更新とCUR/IRQ/イベントの対応（PAUSE再開後の周期・位相を含む）、実analog出力、sample latency精度、DMA fabric drain／安全な解放、suspend/resumeと強制remove。現親のVerifyDmaDrainedは失敗を返す。DL1 enable=0、CUR停止、IRQ/DPC回収をfabric drainと偽ってページ解放する変更は行わない。これが通常production lifecycleの残課題であり、retained D0/pages/SGを完成品の電源管理として扱わない。

mainの次の許可済みbatchでbuild/sign/import確認と通常WASAPI経路の観測をまとめる。小さい単独hardware trialは追加しない。fabric drainはMT8167固有の停止／reset／outstanding transactionの一次契約が必要で、別SoCや推定delayでは補わない。この変更のための追加REA依頼はない。

一次契約：
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nn-portcls-iminiportwavertstreamnotification
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstreamnotification-allocatebufferwithnotification
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstreamnotification-registernotificationevent
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstreamnotification-unregisternotificationevent
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstreamnotification-freebufferwithnotification
