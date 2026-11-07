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

## 最終PortCls SYSへの結合

既存6b5decfeのadapter/pnp_backend/power lifecycleとDMA adapter/登録名寿命修正を
独立worktreeへ再利用し、既存2 Engine slot＋4 HFP slotを持つDriverEntryの最終SYSをbuildする。
標準KS/PortCls/PnP経路が実entryから到達する構成であり、noentry library検査だけで終えない。
8 paired objectはe4b66080から固定再利用し、最終adapterに必要な19 TUだけ追加compileする。
private INF3001は実機/現媒体へ適用しない。mainの現Audio/親選択を置換したものではない。
HFP資格欠落時は従来どおりHFP endpointを公開せず、None/0の判定を維持する。

stock mt8167-afe-util.cのemi_clk_on/offはidle禁止vote/reference管理で、DMA bus-drain APIではない。
AFE_MEMIF_MON/IRQ_CNT_MONの定義に停止完了/latencyの単位保証はなく、名前だけから証明にしない。
同機LinuxのFC72は4byte転送まで。XNU WMT co_clockは共有発振器の設定であり、
SCO PCM clock-master/word-formatを確定するfieldではない。

## controller2010・最終Audio3002

root controller到着をbounded通知queue/PASSIVE workerで受信し、実PDOのqueryから
参照付きcontrollerを取得する。Audioは親v2へRegisterControllerし、実HFP停止後に解除する。
未知のValidatePcm/CodecLatencyはNULL、VerifyDmaQuiescent欠落時も公開前に拒否する。
参照不確定/解除失敗ではcontroller/Audio/D0/元物理epochを保持し、成功を返さない。

scalar SCO_HOLD/RELEASEは同じbroker FileObjectと実NDIS workerで管理する。hold中の
通常radio-off/reset/closeは拒否し、Halt/removalは従来のquarantineへ残す。
元の親WiFi v2 LiveはPASSIVE専用。DISPATCH利用へ転用しない。既定offの親HFP拡張で
元wifiActive/generation/admission/removedを非待機で確認するControllerEpochLiveを提供し、
実NDIS PDOのQUERY_INTERFACEからだけ取得する。元親profile/SYSを更新した意味ではない。
拡張未配置の親は実transportを共有できてもcontroller資格には進まない。

provider1017/client2010/Audio3002の最終SYSを増分生成する。親HFPはcompile-only、
RTC/WiFi/GPU/USBの既存profile/媒体は変更しない。None/0、実機未投入を維持する。

## controller2011・Audio3003：正常D0再結合と実latency転送

正常なUnbindでも永久retiredにしていたため、次のD0で新しいAudio ownerをBindできなかった。
provider v3のcan_unbindで実ownerのlease/IRQ/memif/露出ringが停止済みと確認した場合だけ
参照と公開を解除し、新世代での再Bindを許す。失敗は旧参照/世代/公開状態を保持する。
これは独立DMA drainの代替ではなく、その成功後のowner状態検査である。

未使用だったcontroller CodecLatencyと、optional親の方向別QueryDmaLatencyをAudioへ接続。
FIFO bytes/ChipsetDelay/CodecDelay(100ns)を変換せず転送し、各valid bitがない成功応答も拒否する。
未知値や固定値を実装していない。現controller/親のcallbackはNULLなのでNone/0のまま。
providerのPCM/方向別latency事前検査を、通知購読前と実Bind時に同じ共通実装で行う。
既存HFP到着が資格検査前にfactoryへ進む窓を閉じ、購読→Bindの順序は維持する。

private paired ABIはprovider3/親HFP3。旧ABIと混ぜない。provider1018/client2011/Audio3003のみを
独立増分署名する。親はcompile-only、RTC/現在親profile/媒体/実機は変更しない。
