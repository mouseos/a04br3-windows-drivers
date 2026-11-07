# MT8167 / MT6630 SCO sidebandのsource確定範囲

02e3915の実WDK済みSYSはraw HCI候補として主担当が保持する。
今回のsourceはその既存artifactを変更せず、新診断だけのSYS/EXE再buildも行わない。
親、Audio/Herschel共有source、native、HardwareProbe、一括builderは非変更。
SCOの実電気動作、Windows HFP/audio endpoint、標準radio成功は未確認。

## 機種sourceから判定できる経路

正本は [CT3 GPL source 7c4782127d4ea11965be00ce1343056be9bfb769](https://github.com/s1204IT/CT3_GPL_LGPL_Src/tree/7c4782127d4ea11965be00ce1343056be9bfb769)。
以下はその `kernel-4.4/` 配下の事実であり、レジスタ実測ではない。

|対象|sourceと確定した内容|
|---|---|
|board端子|`arch/arm64/boot/dts/mediatek/a04br3.dts`のconnectivity-comboはGPIO36 CLK、37 SYNC、38 PCM入力、39 PCM出力。対応pinctrlはMRG_CLK/SYNC/DI/DO、bias-disable。|
|mux|同directoryの`mt8167-pinfunc.h`でMRGはfunction1。PCM0はfunction5であり、同じものとして扱わない。|
|merge選択|`arch/arm64/configs/a04br3_defconfig`のCONFIG_MTK_MERGE_INTERFACE_SUPPORT=y。connectivity/common/MakefileもMT6630・非MT2601でMERGE_INTERFACE_SUPPORTを定義。|
|controller pad|`drivers/misc/mediatek/connectivity/common/common_main/core/wmt_ic_6630.c`はcoex後/FM strap前にmerge_pcm_tableのDAI_PADを送る。3 register書込みと実ACKがある。|
|AFE backend|`sound/soc/mediatek/mt8167/mt8167-afe-pcm.c`のMRG BTは8/16kHz S16_LE、送受信、symmetric rate。DAIBT Source=MRG、Playback O02、Capture I02→O11→DAIが定義される。|
|clock|MRG startupは`mt8167_afe_enable_main_clk`。`mt8167-afe-util.c`ではTOP_PDN_AUDとAFE gateを所有・reference countする。MRG専用コードだけで全体clockを勝手に切らない。|

同機Linux正本 [3d2c74e252d6f34a5980deda96bdc04064108d74](https://github.com/mouseos/a04br3-linux-drivers/tree/3d2c74e252d6f34a5980deda96bdc04064108d74) の
`wifi/mt6630-gen3/os/linux/hif/sdio/a04br3_mt6630_core.c`もstockと同位置にDAI_PADを送る。
Windowsの既存BSD WMT codecは同じ44byte commandと8byte ACK照合を実装済みだったが、
BT startup optionsがmerge_interface=0で未実行だった。今回BT側だけを1にして接続した。
これはcontroller pad初期化の実コマンド経路であり、host側GPIO/AFEやSCO音声の成功ではない。
Wi-Fi用options／shared parentは変更しない。

## BTCVSD SRAM経路との区別

stock defconfigはCONFIG_MTK_BTCVSD_ALSA=yだが、KconfigはこれをBT firmwareとの
**encoded data**授受用software BTCVSDと説明する。共通driverは
`mediatek,audio_bt_cvsd`のAP INFRA、0x18000000 PKV、0x18080000 SRAM、SPI194と、
SRAM packet pointer/indicatorを使う。`mt8167.dtsi`にnodeがあるだけで、外付けMT6630の
fn2 SDIO/SCO sidebandとの対応を確定できない。これらのAP windowをMT6630用としてmapしない。
SDIO fn2/STP task0は引き続きHCI control/ACL経路であり、MRGのPCM dataとは別。
WMT `mt6630_aif_ctrl`には成功0を返すだけの分岐やskip設定もあるため、それも実audio設定の
証拠にしない。実書込みがあるDAI_PADとhost AFEのrouteを別々に扱う。

## BT固有scopeで実装したhost core

`sideband.[ch]`は8kHz mono S16_LEの狭帯域CVSD用host engine。
Linuxが許す16kHz設定をそのままmSBC対応とは扱わず、今回は8kHzだけを実装する。
固定値は上記Linuxの実register操作から導出し、GPLコード本文のコピーはしていない。

- ownerの実lease／generation／livenessを取得し、対象4 registerをsnapshot。
  他streamのMRG/DAIBTが有効なら横取りせず、契約違反としてleaseを保持する。
- ownerにGPIO36～39 function1/bias-disableと、本物のplayback I05/capture DAI DMAを準備させる。
- MRGIF 0x3cのmask0xf00000へ固定field9、DAIBT 0x1cのmask0x1208へ0x1008を設定する。
  field9を勝手なbit clock周波数へ読み替えない。8kHzはDAIBT bit9=0の選択。
- O02のmono I05 routeとLinux hw_paramsのR bits（CONN1 mask0x3060/value0x3020）、
  capture I02→O11（CONN3 bit6）を設定。他のregister bitsを維持してreadbackする。
- Linuxの順序でMRGIF bit16→bit0→100us待機→DAIBT bits1:0を有効化し、owner DMAを開始。
- closeはownerによるDMA停止／IRQ rundown／独立drain確認を先に行い、DAIBT停止→
  100us→MRGIF停止→変更bitsとpinsの復元→quiescent release。
  drain/liveness/PIO失敗ではclockを切って成功扱いにせず、FAULTでleaseを保持する。

これは実callbackを実行するhost register/所有権実装であり、常時拒否するprovider stubはない。
ただし**現在のAudio v11 providerへ接続済みではない**。BT helperはTUをcompileするが、
現Windows FDOはhost engineを自動起動せず、未参照関数はlink時opt:refの対象となる。
現FDOに実接続した変更はWMT merge初期化のみ。成功fieldやBTHX capabilityを増やしていない。

## Audio/Herschel側へ必要な依存契約

`sideband.h`はBT固有のprivate C契約案とその利用側実装であり、既存QUERY_INTERFACE ABIを
変更したものではない。Audio v11のAcquireMuted/PreparePlayback（48kHz INT ADDA）を
8kHz SCOの許可として流用しない。提供側に必要な作業は次の通り。

1. 親/Audio側でAFE clock、MRG/DAIBT、O02/I02/O11、GPIO36～39、playback memifと
   DAI capture/IRQ2を一体の実leaseで調停する。FM/他I2S/Engineと競合すれば取得前に拒否。
   combo leaseと別のgenerationを保持し、両方のlivenessを確認する。
2. 実DMA bufferのallocation/mapping/cache publication、mono playbackをI05へ接続する
   内部DL経路の選択、DAI BASE/END/CUR・rate/format設定、IRQ2共有、実clientとの
   双方向data transportを実装する。固定tone、ダミーbuffer、clockだけで代用しない。
3. Windows 8/8.1 `bthhfpddi.h`の
   GUID_DEVINTERFACE_BLUETOOTH_HFP_SCO_HCIBYPASSを通知購読し、実HFP deviceの
   descriptor/connection、STREAM_OPEN/CLOSE/STATUSを扱い、PortCls/KS speaker/mic endpointへ
   結び付ける。これらはBthMini compatible IDを追加するだけでは成立しない。
   WinBlueにはGET_CODEC_ID/DESCRIPTOR2があるが、
   INDICATE_AUDIO_DEVICE_CAPABILITIESはWin10条件なのでRT8.1の依存APIにしない。
4. 実WMT DAI_PAD ACK、動的NVRAMからのFC72 codec設定等の実Command Completeと、
   HFPで交渉したcodec/PCM formatの整合を確認する。現在FC72の4byteは個体recordから
   動的適用しているが、その全field意味／controller master-slave設定は未確定。
   sourceのpinmux宣言は物理導通やPCM word format一致の実測ではない。
5. 同じSCO linkのspeaker/mic複数stream lifetimeを調停し、DMA/IRQとPnP停止順を保証する。
   capabilityの成立条件は「実装済みsideband backendとendpoint管理が利用可能で、確定した
   route/formatを提供できること」。BthMiniのD0前capabilities queryで、まだ存在しない
   active HFP接続やSTREAM_OPEN完了を待ってはならない（stack loadと循環する）。
   HFP interface arrival/codec交渉後に個々のDMA/AFE streamを開始する。能力を提供する
   実backendが未実装の現状ではHCIBypass/1を広告しない。

したがって現capabilitiesはNone/0、公開BTHX load契約不適合のまま。
Windows標準stack／実SCO接続の成功とは扱わない。GPIO/clock/DMA/stream provider実装と
controller codec整合が、このBT固有commitの先に残る依存条件となる。

## 限定確認

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror drivers/bluetooth/mt6630/sideband.c \
  drivers/bluetooth/mt6630/tests/sideband_test.c -o build/mt6630-sideband-check
./build/mt6630-sideband-check
```

単一のportable owner fixtureで順序・field保持・busy拒否・drain/世代喪失時のlease保持を確認。
これは合成providerでの制御確認であり、実DMA・実pins・SCO音声の証拠ではない。
新coreと変更driverのARM32参照compile／helper構文を確認し、実SYS/診断EXEは生成しない。
