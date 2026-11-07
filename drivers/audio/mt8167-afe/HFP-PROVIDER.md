# MT8167 Audio / MT6630 HFP provider v2 checkpoint

既存Engine/audio v11、touch/buttons、USBを保持する別BT専用profile。
Nash `63c32fdc829a8ba65152a49e30ffdc8a1131ae17` の
`drivers/bluetooth/mt6630/HFP-ENDPOINTS.md` とprovider v2 ABIを依存元とする。
通常WLAN profile、04ce/7df固定service、既存実機媒体は変更しない。
BT source/GPU source/EDK2はこの実装の編集対象外。

## 接続済みのソース

- `adapter.cpp`：既存analog 2＋HFP 4、計6 PortCls枠。PnP Start、D0 power管理、
  StopからNash PnP factoryを呼ぶ。HFP失敗を音声接続成功に変換しない。
- `hfp_provider.cpp`：provider v2の全callbackを実装。8kHz/mono/S16、
  DL1/I05 playbackとDAI/O11 capture、方向別common buffer MDL、実CUR position、
  IRQ1/IRQ2、inactive時の実silence/discard ring、世代とrundownを使用する。
  推定timer position、仮想tone、架空DMA完了は使用しない。
- `pnp_backend.cpp`：既存AFE mapping/DMA adapterを参照共有し、MMIOを重複mapしない。
  HFP profileではanalog IRQ leaseを最初のanalog buffer要求まで遅延する。
  analogのretained DMA/leaseをHFPが奪わない。双方同時利用成功は未確認。
- `hfp_power.cpp`：IAdapterPowerManagementでD0開始/終了とStart/Stopを直列化。
  不確実な停止はcontext/FDO/module/mappingを保持する。実surprise removal時に
  物理resourceの撤去を阻止できるという保証はしない。
- 別GUIDの`hfp_interface.h`：audio v11を変更/castしない。BT controllerの実参照、
  target ancestry照合、PCM整合照合と、AFE clock/GPIO leaseを接続する。
- 親HFP/BTフラグは既定OFF。BT profileのみMT6630BGF PDOを生成し、WLAN PDOと排他。
  新親1007、新audio1005の独立INF。既存EngineのINF/packageは変更しない。
- `build-hfp-portcls.ps1`：matched親manifestのflags全値を復元し、上記明示profileを
  選択。63固定のBT sourceをgit archiveで取り込み、portcls.libを一度linkする。
  ARM非VFP/非NEON flagsとlibcntprを使用。明示証明書によるSYS/CAT署名は任意。

## 一次ソースと限界

同型番Linux一次ソース：CT3 GPL kernel-4.4
`sound/soc/mediatek/mt8167/mt8167-afe-pcm.c` のmemif_data、dais_hw_params、
dais_trigger、enable/disable_irq、MRG DAPM route。
DL1 base/end/CUR=0x40/48/44、mono bit21、enable bit1、IRQ1。
DAI base/end/CUR=0x90/98/9c、rate bits25:24、enable bit4、IRQ2。
IRQ counters=0x3ac/3b0、IRQ enable/rate=0x3a0、mask=0x3b4、ack=0x3a8。
8k format/fsは0。MRG固定四registerはNash sideband coreが所有する。
GPIO36..39 function1、mux0x370 mask0x7ff8/value0x1248、pullen0x520 mask0xf0は
同kernelのpinctrl-mt8167/common記述に基づくmasked snapshot/restore。
既存main AFE clock手順はmt8167-afe-util.cに基づく。
Linux実装本文を複製せず、registerの事実から実装している。

**独立した方向別DMA bus-drainの一次根拠は未取得。** Linuxのmemif disableとIRQ停止、
CUR停止、readback、待ち時間を、旧MDLへの全bus access完了証明へ読み替えない。
AFE_MEMIF_MON*というregister名だけから未文書のbit意味を推測しない。
`VerifyDmaQuiescent`はNULLのままで、Mt8167HfpStartはfactory公開前に拒否する。
実provider本体はあるが、このcheckpointを使用可能HFP driverとは扱わない。
未取得のFIFO/codec latencyはSTATUS_NOT_SUPPORTED。HCIBypass能力は変更しない。

## 主担当からNashへ渡すABI要点

1. BT実ownerから新親のRegisterControllerへversion1/正確なsizeの
   A04_HFP_CONTROLLERを登録する必要がある。controller/context/codeを実referenceで固定。
   MatchHfpTargetは保持中controllerとの実PnP親子関係、ValidatePcmはGORM/FC72の
   実PCM master/format/rate整合を検証する。WLAN leaseへの名称だけの読み替えは禁止。
2. HFP factory開始より前のBT登録が必要。現checkpointでは遅延登録時の再通知経路は
   未接続であり、PnP起動順に依存する状態を完成扱いしない。
3. VerifyDmaQuiescentはdirection/MDL/generationごと、相手RUN継続中の独立drainと
   全停止の証明が必要。現時点では実装根拠待ち。成功stubを提供しない。
4. controller Liveは<=DISPATCH。Audio ISR/DIRQLからBT callbackを呼ばず、
   既に保持したlocal mapping/leaseの範囲だけを扱う。
5. fault workerもISRと同じKeSynchronizeExecutionを使用。IoDisconnectInterrupt前に
   GetPosition rundownを待つ。PAUSE後の保持MDLはdrain成功後だけrundownを再開。
   partial prepareで未exposedのbufferだけは通常解放し、成功drainを捏造しない。

## ビルドと検証の区別

```powershell
powershell -NoProfile -File drivers\audio\mt8167-afe\build-hfp-portcls.ps1 `
  -MatchedParentManifest <matched-parent-manifest.json> `
  -OutputDirectory <checkout>\build\hfp-provider-v2 -EnableHfpCandidate
```

これはsource composition/checkpoint用。標準profileや試験媒体へ自動採用しない。
独立drainとBT登録が揃うまでHFP endpointが実PnP公開されないことをmanifestへ記録する。

host確認：portable memifの単一fixtureをASan/UBSanで実行、provider/powerは公開mingw
DDK参照headersによるx64 syntax確認、PowerShell AST parse。
参考header不足によるPcGetPhysicalDeviceObject宣言欠落、ARM専用WaveRT cache検証の
host x64 #errorは迂回していない。親WDF compile、WDK8.1 ARM最終link、標準stack列挙、
SCO双方向音声、電源遷移、電気的連続性はいずれも未検証。


## 停止と最終解放の追加checkpoint

BT sessionのserial gateだけではAudio fault/power workを排他できないため、
sidebandのAcquire/Prepare/Start/Stop/ReleaseはAudio owner mutexで直列化する。
fault/closed状態で新しいPrepare/Startを受けず、Stop/Releaseは不確実状態の
cleanupを試みられるように分ける。DIRQLのposition/ISRは既存rundown/interrupt
synchronizationを使用し、このPASSIVE mutexを取得しない。

NashのPnpStop成功は通知/worker/endpoint登録の停止を表し、既存KS handleの
最終Releaseとは別である。証明済みDMA/IRQ停止後は、未exposed/drainedのclient
MDLがKS handleに残っていてもcreator参照を解放できる。実session/provider参照が
backend/FDO/mappingを保持し、free_drainedと最後のReleaseが完了した時だけ解放する。
exposed MDL、fallback MDL、未解放clock/combo/interruptがあればStopは成功にしない。

PortCls power objectも最終ReleaseがDISPATCHなら専用work itemでPASSIVE解放する。
生きたHFP ownerが残る異常終了はstorage/refを保持し、自動再試行しない。
factory公開前に親・controller・DMA adapterの全使用callback/ABIを検証する。

これらはowner/commonの寿命管理修正であり、run42の親1008/Audio1006、map、plan、
媒体には取り込まない。run42 NativeStartの結果待ち中に開始失敗の仮説を変更しない。
独立DMA drain、BT登録順序、実latencyは引き続き未完了で、成功stubは追加しない。

owner/commonだけを必要な外部moduleから分離して確認する場合：

```powershell
powershell -NoProfile -File drivers\audio\mt8167-afe\build-hfp-owner-modules.ps1 `
 -OutputDirectory <checkout>\build\hfp-owner-common
```

実WDK8.1 ARM headersでmemif C＋provider/power C++の3TUをcompile/archiveする。
BTの63固定sourceはABI headerとして参照し、Nashの実装を変更しない。
このlibraryだけで最終SYS linkやendpoint登録が完了したとは扱わない。
