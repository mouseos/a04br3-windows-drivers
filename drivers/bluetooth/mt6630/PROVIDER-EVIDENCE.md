# Raman向け: provider v2のLinux根拠と実装境界

正本: [CT3 GPL/LGPL source](https://github.com/s1204IT/CT3_GPL_LGPL_Src/tree/7c4782127d4ea11965be00ce1343056be9bfb769)、
commit `7c4782127d4ea11965be00ce1343056be9bfb769`。
作業ルート内の実体は `CT3_GPL_LGPL_Src/kernel-4.4/`。
以下のpathはこのkernel directoryからの相対path。sourceで確定したregister/routeの
事実であり、実DMA drain、導通、PCM word formatの実機証拠ではない。
GPLの関数本文をBSD driverへコピーせず、公開sourceの契約・register事実を参照する。

| 根拠path/開始行 | providerへ反映すべき事実 |
| --- | --- |
| `sound/soc/mediatek/mt8167/mt8167-afe-pcm.c:482` | set_mrg/enable_mrg/disable_mrg。MRGIF field9、DAIBT use-MRG/rateと100usを含む順序。BT sideband.cが既に所有fieldを保存・masked設定・復元する。providerのDMA準備で同じ4 registerを上書きしない |
| 同 `:1174` | MRG BT startup/hw_params/trigger、main clock、O02のR bits |
| 同 `:2288` | MRG BT backendは8/16k S16_LE duplex/symmetric rate。本BT実装はCVSD 8k mono S16_LEに限定 |
| 同 `:2610` | I05/I06はDL1。I05→O02→MRG BT Playback。根拠なしにDL2をI05 sourceとして選ばない |
| 同 `:2673` | MRG BT Capture→DAIBT Mux(MRG)→I02→O11→DAI capture |
| 同 `:2720` | DL1 memifはIRQ1、DL2 memifはIRQ7。playback方向を「IRQ2専用」と扱わない |
| 同 `:2784` | DAI memif BASE/END/CUR、DAC_CON0 rate bits24..25/enable bit4、PBUF format bits24..25、IRQ_CNT2/IRQ2/rate shift8/clear bit1 |
| 同 `:2760`/`:2806`/`:2827` | VUL/AWB/MOD_DAIもIRQ2を使用。DAI captureがIRQ2を専有できると仮定せず、既存ownerの全利用者を調停 |
| 同 `:598` | enable_irq/disable_irq。IRQ2はafe_ctrl_lock下のreference countで保護。単独driverがIRQ2全体を無条件mask/clearしない |
| 同 `:1810` | 実DMA buffer、EMI clock、BASEとEND=base+bytes-1、format/rate設定。WindowsではDMA adapter/MDL/非cached mappingとclient publicationが別途必要 |
| 同 `:1960` | FE triggerでmemif enable、IRQ counter/rate、IRQ admissionを処理。stop writeだけをbus drain証明にしない |
| 同 `:2899` | 実AFE IRQ handler、各memif statusとperiod notification。IRQ発生だけをclient ring advanceや実bus drainとみなさない |
| `sound/soc/mediatek/mt8167/mt8167-afe-regs.h:59` | AFE_DAI_BASE=0x90、END=0x98、CUR=0x9c。IRQ_CNT2は`:96`の0x3b0 |
| `sound/soc/mediatek/mt8167/mt8167-afe-util.c:92` | TOP_PDN_AUD/AFE main clockの取得と解放。analog/Engine利用者を壊さないreference管理 |
| `arch/arm64/boot/dts/mediatek/a04br3.dts:476` | GPIO36 CLK/37 SYNC/38 DI/39 DO、MRG function1/bias-disable。PCM0/function5とは別 |
| `drivers/misc/mediatek/connectivity/common/common_main/core/wmt_ic_6630.c:448`/`:848` | merge_pcm_tableの実DAI_PAD command/ACK。Windows BT startupは26dd22cで接続済み |

**「DMA2」は専用DMA channel 2の証明ではない。** この経路はDL1 playbackとDAI captureの
別memifで、DAI側が共有IRQ2を使用する。資料中のIRQ2と別のAP BTCVSD SRAM/SPI194
経路を混同しない。AP BTCVSDの0x18000000/0x18080000を外付けMT6630用にmapしない。

同機Linux正本 `port/edk2/a04br3-linux-drivers`、commit
`3d2c74e252d6f34a5980deda96bdc04064108d74` の
`wifi/mt6630-gen3/os/linux/hif/sdio/a04br3_mt6630_core.c` はDAI_PAD command送信の
照合先。`docs/wifi-bringup.md` は個体情報を含むため、成果物/共有資料へ転載しない。

## provider v2と通知変更のABI

`hfp_endpoint.h` の `a04_bt_hfp_provider` version **2の構造は今回変更しない**。
volume/jackはHFP標準IOCTLとPortCls IPortEventsで処理し、Audio/shared親へ新しい
volume callbackやraw brokerは要求しない。

- 主担当/Ramanは63c32fd以降のBT library/headerを同じ固定sourceからまとめて使用する。
  新しいBT-private metadata/observer APIを別版のobjectと混ぜない。
- `Mt6630HfpPnpStart/Stop` 呼出し契約は同じ。BT library内部で実通知開始、
  topology observer停止、IRP cancel/completion joinを行う。
- topology factoryを個別に使用する場合は、session events開始→factory/Init、
  `Mt6630StopHfpTopology`→session detach→PortCls unregister/Releaseの順を守る。
  通常は既存PnpStart/Stopに任せる。
- provider.faultはPASSIVEの実owner rundownを非同期queueし、通知callbackから
  inlineでStop/待機しない。context/module/mappingを全reference解放まで保持する。
- 全体の所有権、実FC72/PCM/master-clock整合、Windows DMAの二方向実装/IRQ drain、
  独立bus drain、電源遷移は依然provider側の実装条件。関数pointerが揃うことや
  WMT/version応答だけでHCIBypass能力を広告しない。


## bf989以降のRaman source境界

読み取り照合したAudio checkpointは`4b2a461245274728d01c86e2dad833b0fa398b5f`、
停止修正は`a94ee27`。Audio/shared親sourceはBT側で変更しない。
`a04_bt_hfp_provider` v2の構造は旧bf989と同一（field追加なし）。
`Mt6630HfpPnpStart/Stop`呼出しと4 subdevice枠も同一。

BT library内部で正常COM最終ReleaseのPASSIVE deferralを追加した。
これはprovider.retireを置換するABIではない。通常の方向別buffer解放は、相手RUN中でも
実free_drainedが成立すれば完了できる。DMA不明時のfault/retireは既存provider契約のまま。
次の統合はRaman builderの63c32固定BT依存を新固定commitへ変更し、全5 TUと
`hfp_volume.h`を同一sourceから取り込むこと。旧/新objectの混在・二重sideband link禁止。
このBT-only修正を新親/Audioの能力資格やSCO成功証明に読み替えない。

残る接続はBT FDOからの実RegisterController（親controller登録順も含む）、
FC72の実PCM/master-slave fieldとHFP codecの一致、実codec/FIFO latency、
独立方向別bus drainの一次根拠。これらは名称一致・disable write・固定0 delay・
常時成功callbackで補わない。PWRAP/SD/USB凍結tupleやeMMC/EDKへ変更を広げない。
