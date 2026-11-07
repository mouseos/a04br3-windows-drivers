# MediaTek MT8167 MSDC1 microSD — Windows RT 8.1

独立SD profileのsource候補。Windowsの列挙、実SD読書き、抜去、電源遷移は未確認。
UEFIの既存SD ready証拠はWindows成功とは区別する。「SD2」という表示をMSDC2と
読み替えない。実配線は **MSDC1 / Linux mmc1 / 0x11130000**。

SD実装はこのfolderと`tools/storage/build-mt8167-sd.ps1`へ分離する。
電源provider接続に限り、既存shared親とそのhelperへ既定OFFの最小hookを追加する。
eMMC driver、MSDC2/Wi-Fi/BT、Audio実装、canonical/EDK2共通sourceは変更しない。

## 一次sourceと確定資源

CT3 GPL/LGPL sourceの固定commit
[7c4782127d4ea11965be00ce1343056be9bfb769](https://github.com/s1204IT/CT3_GPL_LGPL_Src/tree/7c4782127d4ea11965be00ce1343056be9bfb769)。
作業ルート内`CT3_GPL_LGPL_Src/kernel-4.4/`以下。

| 対象 | path・確定内容 |
| --- | --- |
| host番号/資源 | `arch/arm64/boot/dts/mediatek/mt8167.dtsi:1058`: mmc1、0x11130000/0x1000、GIC SPI79、Level Low。ACPIのGIC INTIDは111 |
| board/power/CD | `arch/arm64/boot/dts/mediatek/a04br3.dts:325`: bus-width4、CD GPIO20/cd_level1、VMMC=MT6392 VMCH、VQMMC=VMC |
| CDの極性 | `drivers/mmc/host/mtk-sd.c:426,1768`のnative MMC binding→`mmc_of_parse`→`mmc_gpiod_request_cd`。DTS GPIO flags0、cd-invertedなしではcap CD_ACTIVE_HIGHが立ち、`mmc_gpio_get_cd`はraw GPIO値を返す。active-high。vendor SDIOのcd_level parserをMSDC1へ流用しない |
| pins | 同DTS`:600`: GPIO104 CMD、105 CLK、106..109 DAT0..3、function1。CMD/DAT pull-up、CLK pull-down、6mA、CD GPIO20 pull-up |
| GPIO入力 | `drivers/pinctrl/mediatek/pinctrl-mt8167.c:348` / `pinctrl-mtk-common.c:68,927`: DIN+((pin>>4)<<4)、bit(pin&15)。GPIO20は0x10005210 bit4 |
| clock | `drivers/clk/mediatek/clk-mt8167.c:166,556`: TOP mux0x000 bits20..22、selector2はmainpll_d8。共有clockの変更は行わない |
| FIFO/PIO | `drivers/mmc/host/mediatek/mt8167/mt_sd.h:102`のFIFO128byte、vendorの同IPのPIO/read/write/R1処理を補助参照。MSDC1のbinding自体は`mtk-sd.c`。GPL関数本文はコピーせずregister/protocol factsを参照 |
| SD仕様処理 | `drivers/mmc/core/sd.c`、`sd_ops.c`、`include/linux/mmc/sd.h`: CMD8、ACMD41、CSD、SDHC/byte address、書込み保護 |

CDの追跡箇所: `drivers/mmc/core/host.c:194..233`、`slot-gpio.c:76..87,232..263`。
GPIO flags0に対し`gpio_invert=!gpiod_is_active_low`がtrue、cd-inverted=falseとのXORで
CD_ACTIVE_HIGH=true。`!raw ^ true`によりpresent=rawとなる。
`a04br3_defconfig`はCONFIG_MMC_MTK=yとMMC_MTK_SDIO=yを両方有効にするが、
後者のvendor driverはmt2701/mt8167-sdio compatible用であり、mmc1のnative MMCとは別。

Windows UEFI参照checkout:
`port/edk2/edk2-mtk-a04br3-windows-rt`、参照HEAD
`e8af0fb0a2cdf2b592ad16d63b4ad947a993a94b`。
`Platform/Benesse/A04BR3Pkg/Drivers/SdCardBlockIoDxe/SdCardBlockIoDxe.c` の
SdPrepareHost、SdSendCommand、SdInitializeCard、SdDecodeCsdとSD read経路を照合。
MT8167.decとa04br3.dscのPcdMt8167SdCardBaseも0x11130000。
実機FDがこのHEADと同一であると主張するものではない。

UEFIはmux2 sourceを188MHzと名付けるが、Linuxのvendor rate tableとは記述が異なる。
本profileは **transfer divisor4（source/16）**、初期化divisor181で保守的に動かし、
23.5MHz等を実測値として広告しない。4-bit/HS/UHS/1.8V/tuningは未対応。

## 実装

- `sd.[ch]`: native SD CMD0/8/55/ACMD41、CMD2/3/9/7/16、実RCA/CID/CSD/OCR、
  SDSC/SDHC/SDXC容量・CSD保護を動的取得。SDUC/CSDv3、SDIO機能は拒否する。
  CMD8 timeout時だけSD1 optional-probe分岐を持つ。CMD1/eMMC初期化は使用しない。
- readはCMD17、writeはCMD24の単一512byte PIO。最大128 sector/64KiBの要求を
  逐次処理。DAT0 highに加え実CMD13 READY_FOR_DATA/TRANまでwrite完了にしない。
  CRC、R1、受付時からqueue待機を含むrequest全体deadline、CD/D0/取消し世代を各pollとFIFO accessで確認する。
- SDのoptional cache featureをenableしない。SYNCHRONIZE CACHE、SRB flush/shutdown、
  IRP flushは実CMD13 barrierで状態を確認し、受付だけでsuccessにしない。
- Microsoft [classpnp ClassClaimDevice sample](https://github.com/microsoft/Windows-driver-samples/blob/main/storage/class/classpnp/src/class.c)
  のsoftware claim/releaseを実装し、実PDOと単一claim状態を返す。PnP query capabilityは
  実socketのremovable/surprise属性を返し、child stop/removeは受付を閉じて世代を失効。
  SRB powerはD0/D3-hotのprivate host準備・barrier/quiesceに接続する。D1/D2、adapter
  power要求や物理rails切断を実装済みとは扱わない。
- `scsi.c`: removable INQUIRY、TUR/NO MEDIA、UNIT ATTENTION、READ CAPACITY10/16、
  MODE SENSE、READ/WRITE10/16、flushを処理。FUA、MODE SELECT、未知CDB、hardware
  media-lock、erase/format commandは未対応として返す。全SD LBAのRWは可能だが、
  eMMC等の他hostを操作する経路はない。logical eject後は物理再挿入を必要とする。
- `windows.c`: KMDFの物理host FDOとpersistent removable `GenDisk` PDO。
  通常OSのdisk.sys/classpnp/partmgr/volmgrを利用する。空socketはPDOを消さず
  NO MEDIAとし、100msのone-shot passive timerがACPI SDCDを読む。
  CDの実変化で世代を更新し、古い要求を次のcardへ転用しない。
- SRBは下位PDOのIRP preprocessで受け、accepted IRP/MDL/SRBをCPU処理終了まで
  自身で保持する。最大8 outstanding、host mutexで単一PIO owner。
  queue中のcancelはIRP.Cancelを確認して失敗完了し、active cancelはpollで停止。
  他callbackが先にSRBをdetach/完了してbufferを解放する構造を取らない。
- D0停止はadmission閉鎖→timer停止→rundown join→private host quiesce。
  MMIO/MDL access終了後にrundownを解放してから上位completionを呼ぶ。
  child/parent referenceとIoWorkItemがcompletion終了までcontext/moduleを保持する。
  surprise callbackではMMIO/reset/unmapを行わない。
- write/transferが不確定になったらpoisonし、自動retryしない。CD read失敗からの回復、
  普通のsuspend/resumeだけを再挿入証明にしない。実CDのabsent→presentを確認するまで
  blockを保持する。部分書込みを全成功として返さない。
- `control.c`: fixed storage property/capability/address queriesのみ。
  raw MMIO/CMD/path/firmware指定のbrokerなし。card product/MIDは実値を使用し、
  CID serialや個体dataをログ・manifestへ出さない。

### 既存eMMCから再利用する契約

既存`mt8167-msdc/cdb.c`と`scsi_capacity.c`を**非変更**でcompileして範囲・容量処理を
再利用する。PIOのCRC/transfer-complete、writeのDAT0/CMD13、単一ownerと失効時の
停止契約をSD用coreへ適用する。旧Storport virtual DMA workerの寿命問題は引き継がず、
DMA descriptor/common buffer自体を作らない。割当て資源のraw/translated MMIOが
両方0x11130000/0x1000でなければ開始しない。MSDC0/eMMC 0x11120000や
MSDC2 0x11170000は拒否。INFはDemand Start、paging/dump/hibernate用途を宣言しない。

## 必要な機種ACPI/電源統合

`acpi/MT8167-SD.asl`は **統合案**。canonical/EDK2を自動変更・ロードしない。

- host `_HID=MOU0013`（project用binding）、実表示名MediaTek MT8167 MSDC1 microSD Host。
- `_CRS`: Memory32Fixed0x11130000/0x1000、Level/ActiveLow/Exclusive INTID111。
  このPIO profileではINTENを0にし、data/command completionをpollする。IRQを
  使用したDMAやCD EINT処理が実装済みとは扱わない。
- `SDCD`: GPIO20の実入力。`SDCF`: 実GPIO20 mode/input、GPIO104..109 function1、
  mux2をread-onlyで確認する。常に1を返すstubではない。driverは共有GPIO/TOP/PWRAPを
  map/writeせず、fixed ACPI methodだけを評価する。
- **platform側の必要条件**: Linuxのpin bias/driveと、VMCH/VMC 3.3V供給、source clockを
  本deviceのD0期間保持する実owner契約。既存UEFI ready stateを使うhandoff profileであり、
  SD準備のためにeMMC/Audioの共有regulatorやclockを切り替えない。
  SDCFはpin/muxを確認するが、PMIC railの実電圧/所有権まで証明しない。
  `_PSx/_PRx`を架空の成功methodで埋めていない。main側が機種ownerへ統合する必要がある。
- 同じMSDC1を別SD driver/ACPI nodeへ重複割当てしない。共通GPIO controller `_CRS`を
  取り上げず、読取りだけのAML fieldを利用する。Windowsの機種ACPI配置と電源条件が
  揃うまで、本INFだけで実PnP成立と主張しない。

## WDK8.1 ARM32ビルド

依存: Windows SDK/WDK8.1 ARM kernel headers/libs、KMDF1.13、LLVM clang/lld-link、
PowerShell、git。通常OSの標準disk stackが必要。実機/runnerへ接続するhelperではない。

```powershell
# clean checkoutの固定commit。output/tmpはこのcheckout内。
powershell -NoProfile -File tools\storage\build-mt8167-sd.ps1
```

SD側全7 TU（SDの5 TUと非変更で再利用するeMMC共通2 TU）に
`-fno-vectorize -fno-slp-vectorize -mno-unaligned-access -mfpu=none`を共通適用する。
CRTは正規WDK8.1 ARMの`Lib/winv6.3/km/arm/libcntpr.lib`を必須としてlinkし、
flags・runtimeのkit相対pathとSHA256をmanifestへ記録する。
SDK8.1旧`__ldrexd`宣言はE/Hの実WDK成功recipeと同じcompile時aliasで回避する。
SDK headerの改変や代替intrinsic/stubの実装は行わない。

出力`build/mt8167-sd/mt8167-sd.sys`、実ハード名INF、source/hash manifest、ACPI source案。
署名/CAT生成と配布前の実WDK最終link確認は主担当側。
ACPI案のAML生成はiaslで別途行い、既存canonicalへ黙って挿入しない。

## 限定検証と未完了条件

source参照compile（ARM32、public WDK10/WinBlue guardsとKMDF参照header）、iasl、
単一のportable protocol fixtureを使用する。fixtureは動的CSD/PIO読書きの制御、
保護、timeout/抜去/poison、SDSC addressing、未知CSD拒否を確認するだけで、
実SDやWindowsの動作証拠ではない。大量の追加mock群は作らない。

実WDK8.1最終link、standard disk stackへのPDO列挙、標準OSでのpartition/volume認識、
RW内容、物理抜去/再挿入、電源遷移、ACPI CD極性の実機照合、実電源owner/rail保持、
source clock実周波数・sampling余裕は未確認。UEFIのready証拠をこれらへ流用しない。

## Windows単一ownerによるD0電源契約 v1

`f729a77`は実WDK8.1 compile/link・SYS/CAT署名/検証済みの旧handoff候補。
以下の新電源consumer/providerは、その署名済みSYSに含まれない。新candidateは
SDとmatched parentを同じ固定sourceからbuildし直す必要がある。
Windows列挙/RW、rail復元、system sleep/resumeは未確認である。

### 一次sourceから確定した専用範囲

Linux `mt6392-regulator.c`のLDOはenable bitとQI bit15を別に確認する。
VMCは`0504 bit12`、selectorは`052a bit4`（0=1.8V/1=3.3V）。
VMCHは`0506 bit14`、selectorは`052c bit7`（0=3.0V/1=3.3V）。
VEMC3V3 `0508/052e`、VIO18 `0556`はeMMC用であり触れない。
Linux `msdc_ops_set_ios`に合わせ、VMMC=VMCH、VQMMC=VMCの順に有効化する。

TOP+24 bit18は通常gate（1=off、54 SET/84 CLR）。
TOP+3c bit29のMSDC1_INFRAは**inverted**（1=on、6c SET/9c CLR）。
同じTOP+24上のthermal bits1/30を含む他consumerのbitへ書かず、RMWしない。
TOP mux0 bits20..22はselector2=`mainpll_d8`を要求し、live muxは変更しない。
MAINPLLはAPMIXED+120のEN bit0/RST_BAR bit27、+130のPWR_ON=1/ISO=0を観測。
+124のPCW/post-dividerとAHB_INFRA selectorも取得時に保存し、lease中の変更を検出する。
PLLの起動・retuneや実周波数の成功広告は行わない。
UEFIの`SdUngateClock`にも同じSD専用gate/selectorがあるが、UEFIの準備だけで
Windowsでの所有/保持を証明したとは扱わない。

### providerとconsumerの実接続

- `power_owner.[ch]`: 親の同じPWRAP bus object/poison、同じblocking lockを借用する。
  新たなPWRAP/TOP/APMIXED mappingや別driverを作らない。
  Acquireは無変更のclaimだけを作り、UEFI-readyで両clockが既にonの場合だけ受理する。
  gated cold handoffをungateして未知のfirmware DMAを再開することはしない。
  consumerがMSDC+09c bit0をread-onlyで検査してDMA idleを確認後、Enableを呼ぶ。
  active DMAならtokenを保持してunsupportedを返し、firmware descriptorをabortしない。
  Enableは無効なrailだけselectorを
  3.3Vに設定してenable/QIをbounded確認し、clock gateをenableする。
  既に有効なrailのlive voltage切替や、software-disabledなのにQI=onのforeign制御を拒否。
  元々有効なrailはreleaseでも有効のまま、元々無効ならVQMMC→VMMCの順で停止し
  QI offを確認して元selectorへ戻す。QI待機は各回のWACS2 bounded timeoutに加え、
  Enable/Release各100msの全体budgetを持つ。常時給電policyを追加しない。
- `power_parent.inc`: 既存`ParentLock`と同じ`pmic.bus`だけを使うfixed interface v1。
  `Acquire(token*)` / `Enable(token)` / `Health(token)` / `ReleaseQuiescent(token)`以外の操作は公開しない。
  任意register/voltage/path IOCTLはない。Acquireは親に`WdfDeviceStopIdle(TRUE)`を取り、
  releaseが実成功した場合だけ`WdfDeviceResumeIdle`を1回対応させる。
  Acquire/Enable失敗でもtoken非0ならclaim/部分変更が残るquarantineであり、失敗statusを返して
  親power referenceとmappingを保持する。共有bus timeoutは同じpoisonへ伝播する。
- `power_client.c`: 固定interface GUIDから唯一のproviderだけを開き、kernel queryを行う。
  raw pointer/任意path指定を利用者へ要求しない。D0でacquire、SD要求前とCD timerでhealth。
  supplier消失/終端faultは失敗伝播し、自動restartしない。
- SD D0Exitは新規要求を止め、IRP rundown/workerをjoinし、必要なDAT0/CMD13 barrierと
  private PIO reset完了後だけ`ReleaseQuiescent`を呼ぶ。カードが残りwrite状態不明、
  poison、timeout、host surprise removal、supplier喪失ならleaseを保持し失敗を返す。
  Unknownをquiescent成功に変えない。強制PnP removeが物理資源を撤去することまで
  object参照で防げるとは主張しない。

### power ordering

[Microsoft IRP_MN_QUERY_DEVICE_RELATIONS](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mn-query-device-relations)
の`PowerRelations`（Windows7以降）は、独立nodeのsupplierをsystem S1..S5で先ON/後OFFに
する契約。S0 runtime Dxの保持は保証しないため、上記StopIdle leaseを別に持つ。
SD FDOのPowerRelationsへsupplier PDOを参照付きで追加し、既存relationを保持する。
remote targetのremove通知を処理し、paging/hibernate/dump等のspecial file使用は拒否する。
関係PDO参照は専用mutexで保護する。driver file/Interface参照だけで電源保持としない。

ACPI案の`_DEP=\_SB.SHR0`は列挙依存のためであり、PowerRelations/leaseの代替ではない。
Carverが示した`WindowsArm32/Acpi/SharedParentIo.asl`のSHR0/BEN0001/UID0を根拠にする。
**実採用FDのnamespaceが同じことをmainが確認する必要がある。** このsource変更で
DSC/FDF/正本EDKへ接続しない。GPIO/pin bias/driveのhandoff条件は引き続きSDCFと
機種側で満たす必要があり、架空の`_PSx/_PRx`を作らない。

### buildと統合hook

親`wdf_parent_probe.c`への変更はSD専用context、include、publish、D0Exit/Unmap保持判定のみ。
`A04_SHARED_SD_POWER=1`を新たに指定し、既存1006のAudio/USB/touch/buttons等の
**既存全flagsをそのまま保持**して`tools/ci/build-msdc-parent-probe.ps1`を使う。
`A04_SHARED_BUTTON_D0_EXPERIMENT=1`と`A04_SHARED_APMIXED_RESOURCE=1`が必須。
新core TUもNoFP/NEON flagsでcompileし、正規WDK `libcntpr.lib` linkを維持する。
manifestは新flag、ABI version、SD provider source hashesを記録する。
古い親binaryの再利用や、旧manifestへ新flagを書き足すだけの統合は禁止。
このhelperは音声/KS clientやUSBを再buildしない。既定OFFなら新providerは公開しない。
SD helperは全7 TUをcompileし、manifestにABI v1/PowerRelations/sourceを記録する。
親INFのrank/version調整と署名はmain/Ramanの統合契約に従う。

Raman側の`wdf_parent_probe.c`/親helperと競合する可能性があるため、同等の単一owner
hookへsource統合すること。独立serviceを増やしてPWRAPへ二重アクセスしない。
BQ/thermalはSD上記専用rail bitsを変更せず、同じ親lockとbus poisonを共有する。

### 単一consumer後続候補

SD4 checkpointは凍結し、INF6は別source候補として単一system thread＋上限8の標準CSQへ変更。
共有work itemは上位完了通知だけに集約し、PIO/serial/hardware rundown待ちを行わない。
受付時deadline、cancel、rundown、power停止とtimeout時の資源保持は
[SINGLE-CONSUMER.md](SINGLE-CONSUMER.md) を参照。SD helperはconsumer/checkpointを含む
9TUを実WDK8.1でbuildする。この変更をtrial47 Code31の根因修正やSD実動作成功とは扱わない。
