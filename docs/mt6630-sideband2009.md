# MT6630 共存transportと実AFE sidebandの結合候補

独立branch。実機・main GPU/source/媒体/REAは操作しない。既存scan1012とsigned1015/2008は凍結。
この候補の署名・合成回帰は標準radio/接続/通信/SCO成功の証明ではない。

## 再利用と実装

- Bluetooth HFP/KS/topology/PnP/sidebandは同repoのdd31bcc418a85e17017b4802d0e68227df473677から再利用。
- Audio hfp_provider/hfp_memifと親DDI headerは6b5decfe17635e701d00b8315f7bfca0a7df4bcaから再利用。
- HCI PDOにkernel-only PnP QUERY_INTERFACEを追加し、Audioの実provider v2をBind/Unbindする。
  provider参照と実radio照合、CVSD 8k/mono/S16検証、方向別latency応答を要求する。
  Audio側はBthHfEnum標準通知を先に購読し、続いてradioへBindする。load前のactive HFP peerは要求しない。
  停止はHFP endpoint/通知/実DMAの排出を先に完了し、その後Unbindする。途中失敗はcontextを保持。
- clientのcapabilitiesは同じFileObjectでNDIS workerへ実transport状態を照会し、FC72適用ACKと
  実Audio provider資格の両方を確認する。未結合/資格未成立ならNone/0。Bypass/1の固定広告なし。
- Bindの間は実KMDF D0 holdを保持し、D0Exitは未解放bindingを成功扱いしない。
- Audio方向切替は対象memif停止後、旧MDLへの独立VerifyDmaQuiescentが成功して初めて
  BASE/ENDとclient/fallbackを切替える。以前の「fallbackへ再開してから旧MDLを検証」を修正。
  peer memif/clockは停止しない。失敗時はBASE/ENDを変更せずbuffer/clockを保持しfault workへ渡す。

## Linux/XNU根拠と区別

CT3 GPL7c4782127d4ea11965be00ce1343056be9bfb769のkernel-4.4:

- mt8167-afe-pcm.c set_mrg/enable_mrg/disable_mrg: DAIBT 8/16k、MRGIF field9、100usの順序。
- 同MRG BT backendはS16_LE duplex/symmetric。CVSD候補は8k mono S16_LEだけに限定。
- I05/DL1→O02再生、I02→O11/DAI収録。再生IRQ1、収録は他memifとも共有されるIRQ2。
- a04br3.dts: GPIO36..39、MRG function1/bias-disable。PCM0/function5やAP BTCVSD SRAMと混同しない。
- VOICE_PCMのVoice Ampはアナログcodec出力であり、Bluetooth MRG経路の代わりに呼ばない。

既存BSD sideband.cは以上のregister事実に基づくmasked snapshot/restoreを実装している。
GPL本文の転載なし。新しいMMIO/PMIC値・推定clock周波数を追加しない。
XNUのA04BR3WMTDownload.hと同機Linux3d2c74e252d6f34a5980deda96bdc04064108d74の
DAI_PAD処理は既にprovider1015のcold共存profileへ接続済みで、今回変更しない。
MT6630 FC72は現transportが動的record8..11を送り実Command Complete後だけ適用済みにする。
個体値は保存・配布しない。ACKだけではmaster/slave・word formatを証明しない。

## 未成立を明示

実MT6630 FC72 bit/rate/clock-master定義、独立したAFE bus-drain保証、FIFO/codec latencyは
参照sourceから確定できていない。100us待ち、CUR停止、disable readback、IRQ flushを独立drainにしない。
Audio providerのLatencyは依然STATUS_NOT_SUPPORTED、親VerifyDmaQuiescentも実provider未接続。
そのため現候補のBindは成立せず、None/0が保持される。BTHXの能力検査を通ると主張しない。

実controller登録とAudio起動順の現共存親への結合も残る。今回の親hfp_interface.hは既存v1定義の
取り込みで、現親1026/実RTC profileへ実装済みという意味ではない。親SYSは生成しない。
shared callback取得や実latency/drainを成功stubで埋めない。

## ビルド境界

rebuild-mt6630-sideband2009.ps1は1015固定cacheをhash検証し、provider41 objを再利用して
checkpoint/combo_brokerの2TU、client/radio_bindingの2TUだけ再compileしてSYSをlinkする。
HFP core5TU＋combo binding1TU＋Audio provider/memif2TUを同一checkoutからcompileし、
単一paired libraryへ結合する。これは最終Audio SYS link/実endpoint登録とは別。
親・既存Audio/GPU/SDK再build・媒体生成は行わない。
