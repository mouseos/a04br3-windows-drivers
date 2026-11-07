# MediaTek MT6630 Bluetooth (SDIO/STP)

専用branchの実装。Windowsでの動作成功やWDK8.1最終link成功を示すものではない。
実機・runner・REA・EDK2にはアクセス／変更していない。

## 確定した経路と出典

- [Linux正本](https://github.com/mouseos/a04br3-linux-drivers/tree/3d2c74e252d6f34a5980deda96bdc04064108d74)
  の `wifi/mt6630-gen3/os/linux/hif/sdio/a04br3_mt6630_core.c` と
  `a04br3_mt6630_bt.c`。MSDC2 SDIO function 1=WLAN、function 2=BGF/WMT、
  STP task 0=Bluetooth H4、task 4=WMT。UART transportではない。
- fn2 CHLPCR=0x04、CHISR=0x10、CHIER=0x14、CTDR=0x18、CRDR=0x1c。
  driver-own要求はCMD52 fn2 address 5 value 2。既存XNU由来BSD codecと
  同じレジスタ／SDIO framingを使用。LinuxのGPL実装をコピーしていない。
- [Microsoft BTHX DDI](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthxddi/)
  と [serialhcibus sample](https://github.com/microsoft/Windows-driver-samples/tree/main/bluetooth/serialhcibus)
  をDDI/PDO契約の参照に使用。UART部分は使用しない。
  子PDO compatible IDは `MS_BTHX_BTHMINI`。
  READのinputは `BTHX_HCI_PACKET_TYPE` (ULONG enum)、outputはpack1 context。
  kernel内部METHOD_NEITHER要求のみ受理する。

## 実装した起動経路

1. 固定WMT patchと動的NVRAMをread-onlyで取得してからv2親leaseを取得する。
2. 既存MSDC2／GPIO／RTC32K経路でcold enumeration、fn2 WMT identityを確認。
3. 固定WMT bundleを検証・downloadし、STP readyの実応答を待つ。
   既存WLAN用function1起動は行わない（startup optionsの最後は0）。
4. bootstrapからruntimeへread-clear CHISRの消費権を一度だけ引き渡す。
5. WMT `01 06 02 00 00 01`を送信し、`02 06 01 00 00`を照合する。
   opcode 0x16通知をFUNC_CTRL応答と混同しない。
6. Linuxで確定したNVRAMフィールドから、0xfc1a（逆順BD_ADDR）、0xfc72
   （offset8/4byte）、0xfc79（12/6）、0xfc93（27/3）、0xfc7a（18/7）、
   0xfc22（30/6）を送信。各Command Completeのopcode／statusを確認する。
7. HCI Read Local Version 0x1001を送信。実Command Completeの9byte return
   parametersのみをversion構造体へ格納する。Command Statusだけでは成功にしない。
8. BTHX version/capabilities/command/ACL write、event/ACL readへ接続する。
   5msのPASSIVE_LEVEL polling、STP aggregateとH4分割再構築、credit管理、
   キャンセル可能なread queue、bounded RX queue（超過は失敗）を使用する。

固定firmwareは既存 `tools/winpe/prepare-mt6630-firmware.py` が生成する
`\SystemRoot\System32\drivers\a04br3\mt6630-wmt.bin`。
SHA-256は `4af7996b89ef110d8f270877f61aaf0b90fde21b6c5b0ed23ccbd32c2989dcee`。
vendor revisionは `0c756f413e7011231c8fb793af0930311023f68d`。
Bluetooth用に新しい個体入りfirmwareを配布しない。

## 動的NVRAMと所有権

既存Windows eMMC disk serviceでsystem volumeの単一disk extentを特定し、
同じdiskのGPT `nvram`領域をread-onlyで読む。AllMap内のcanonical path
`/data/nvram/APCFG/APRDEB/BT_Addr`を一意に検索する。LIDやpartition offsetは固定しない。
AllFile全体と、AllMapが示す実record末尾2byteの既存libnvram checksumを検証し、
既知の36byte prefixのみを使用する。Linuxの66byte exportがpayloadのみかどうかを
仮定して68byte recordを要求しない。WIFIのLID7／512byte検査は維持する。

BT_Addrがこの個体のAllMapへ保存されているかは未確認。存在しない／checksum不整合なら
起動に失敗する。利用者の抽出、既定BD_ADDR、他個体の校正を代用する経路はない。
NVRAM、HCI key packet、BD_ADDRをログへ出さず、所有する一時bufferを消去する。
version結果はkernel DbgPrintと固定read-only snapshotで取得できる。通常OSの独立probe／任意UART出力は [tools/bluetooth](../../../tools/bluetooth/README.md) を参照。

`A04_SHARED_BT_PROFILE=1`は単一のMSDC2子PDOを `A04BR3\MT6630BGF`へ切り替える。
既定0では従来の `A04BR3\MT6630SDIO`を維持する。既存v2 whole-combo leaseのABIを
変えず、Wi-FiとBluetoothに同じMMIOを二重配布しない。Audio v11、USB、touch、
battery、buttonsの親機能は保持する。起動失敗／surprise removalではIRQをdrainし、
MMIOをunmapしてleaseをquarantineする。通常停止はBT OFFの実応答、既存board
reset/PMU信号の受理、IRQ rundownを経て親へ返却する（物理停止の実測とは別）。

## WDK8.1 ARM32 build

主担当のWindows build環境で実行する。こちらからrunnerは起動しない。

```powershell
# BT SYS/INFのみ。親は同一commitのBluetoothProfileで一度だけbuildすること。
.\tools\ci\build-mt6630-bluetooth.ps1
# Audio v11/USB/inputのflagsを保持した単一親だけを作る場合:
.\tools\ci\build-mt6630-bluetooth.ps1 -BuildSharedParent
```

必要物：Windows WDK8.1 ARM kernel headers/libs、KMDF1.13 headers/ARM libs、
LLVM clang/lld-link、PowerShell、Git。BTの親buildはParentOnlyで、clang++や音声clientを要求しない。
全TUは既存ARM GS helper回避と同じ `-fno-stack-protector`。
既定出力はcheckout内 `build/mt6630-bluetooth`、TEMP/TMPもその配下。
BT ParentOnlyの親INFは正式profile `mt8167-shared-bluetooth.inf`、固定DriverVer `11/06/2026,2026.11.7.2001`。
既存親SYSからINFだけを更新する `tools/bluetooth/prepare-mt6630-parent-profile.ps1` も使用できる。
署名/CAT作成・配置は主担当。leafが親をbuildしない場合は既存Wi-Fi親をそのまま
代用できない。USB-only親も使用しない。SYS/INFの表示IC名はMT6630。

## 未完了条件

- WDK8.1/KMDF1.13の最終SYS link、署名、実PnP、実Read Local Versionの観測。
  ローカル確認は公開WDK10/KMDF1.15参照headerをNTDDI_WINBLUEでARM32 compileしたもの。
- 実AllMapでのBT_Addr可用性、初期化後のOS HCI Reset／電源遷移時の校正保持。
- SCOの実配線・別audio経路が未確定。capabilitiesは `ScoSupportNone`。
  MicrosoftのBthMiniに記載されるHCIBypass要件を偽って広告しないため、
  標準Bluetooth stackがこのcapabilitiesを受け入れるかは未確認／阻害条件になりうる。
- Wi-Fi／Bluetooth同時動作。現状は排他的BT profileのみ。共存にはfn2を単一consumerに
  保つcombo brokerが必要。既存Wi-Fi leaseを横取りする実装はない。
- pairing、鍵保存、SCO、RF操作、実機のsleep/resumeは未検証。controller versionを
  取得してもこれらの成功や通常OSでのBluetooth利用可能を意味しない。

今回のsource確認では、変更したBT 2 TU・firmware/NVRAM reader/calibrationの
ARM32参照compile、親のBT profile 0/1両compile、変更PowerShell 3本のParserを確認。
既存NVRAM・immutable firmware・calibration bridgeの限定回帰と、BT初期化／
aggregate通知／H4分割／失敗伝播を扱う単一のportable regressionを実行した。
これらはsynthetic fixtureと参照headerによる確認であり、controllerとの通信実測ではない。

```sh
make build/mt6630_bt_transport_test build/mt6630_nvram_test
./build/mt6630_bt_transport_test
./build/mt6630_nvram_test
```

主担当が最新統合branchへ移す際、競合しうる共通pathsは
`drivers/bus/mt8167-shared/wdf_parent_probe.c`、`drivers/network/mt6630/nvram.h`、
`windows_nvram_reader.[ch]`、`windows_firmware.[ch]`、
`tools/ci/build-msdc-parent-probe.ps1`、`build-mt8167-first-playback.ps1`、`Makefile`。
変更はprofile選択／汎用record lookup／WMT単独snapshot／明示build入口に限定する。
最新audio実装をこのbranchの古いfile全体で上書きせず、対応する追加差分だけ統合する。
