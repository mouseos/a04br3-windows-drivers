# MT6630 Native 802.11 cold-start runtime

この実装は既存の常時初期化失敗candidateとは別の、PnPから開始するminiport。
`ndis_runtime.c` がDriverEntryを持つ。Windowsによる受理、SYSリンク・署名、
実機のscan・OPEN/WPA2接続・IP通信はまだ確認していない。実機とrunnerの操作は主担当が行う。

## 接続した経路

1. 既存共有親の `ACPI\BEN0001` → 子PDO `A04BR3\MT6630SDIO` → 新NetクラスINF。
2. NdisMGetDevicePropertyのraw/translated資源とPDO QueryInterfaceから親を取得。
3. `A04_SHARED_WIFI_RUNTIME=1` の親は同一mutex内でRTC32Kを取得し、
   重複しない世代番号を発行する。二重取得・停止中・poison後は失敗させる。
   部分的なRTC変更は正常な拒否に偽装せず、無効なLiveを持つ保持中leaseとして返す。
4. MSDC2の専有MMIOとGSIV141を取得し、割込みsourceをmaskして接続。
   現段階は10ms周期のworkerによるpolled HIF。IRQの再armは行わない。
   MSDC2の割込みとGPIO5のcombo EINTは別物。IRQ極性・配送は実機確認が必要。
5. 250msのcombo電源断、3.3V/4-bit SDIO列挙、silicon revision確認、
   digest固定のWMT/WLAN snapshot転送。WMTのantenna mode1はCT3のWMT.cfgが選ぶ
   mt6630_ant_m1.cfg、FM mode2はstp_uart_launcherの既定値に基づく。
   shared26MHz・mergeは使用しない。これは実機での設定適合の証明ではない。
6. 既存のWindows eMMCサービスを通した読取り専用のNVRAM取得、製造データ設定、
   BSS/OwnMAC/STA/WTBL割当て、NDIS general/native attributes登録。
7. NDIS work item一つでOID・RX poll・TXを直列化する。scanは実SCAN_DONEから
   SCAN_CONFIRMを通知し、観測したBSSをENUM_BSS_LISTへ返す。
8. 接続は2.4GHz/ERP/20MHz、OPEN/NONEまたはRSNA-PSK/CCMP。
   実CH_PRIVILEGE grant→authentication response→association response→
   ACTIVATE_STA_RECを待つ。成功通知には実送受信したassociation frame bodyを使う。
   WPA2のOS key OID、CID7、controlled portは [WPA2.md](WPA2.md) を参照。
   WPA/TKIP、WPA3、必須PMF、5GHz、channel14、ローミング、省電力運用は対応に含めない。
9. NBL送信は既存HIF TXとTX_DONE照合へ、受信は実RX vectorからPHY/rate/RCPIを
   取り出してNDISへ同期RESOURCES indicationする。未観測のリンク速度は不明値のまま。
   disconnectとPHY+MACのRESET_REQUESTはordered stopと新しいcold-start世代へ進む。
   MAC単独／PHY単独resetと個体MACのoverrideは対応に含めない。

## 動的NVRAMとeMMC

既存 `drivers/storage/mt8167-msdc/mt8167-msdc.inf` のサービスは `a04msdcboot`。
miniport.c/A04RunIoとscsi.cは標準READ(10/16)を処理し、disk class経由で読める構造。
`windows_nvram_reader.c` はREAD権限だけでシステムvolumeを開き、単一disk extentと
一致するdisk interfaceを特定する。disk classが返すGPT layoutのnvram領域だけを
読み、bitmap、全データ、WIFIレコードのchecksumを検証する。新しいeMMC MMIO所有者、
バックアップ抽出、固定disk番号、利用者の校正ファイル指定は追加しない。

これはソース上の接続確認であり、搭載Windowsのdisk stackが当該IOCTLと領域読取りを
許可することは未観測。RAM diskのWinPEをsystem volumeとする構成は対象diskを特定
できず失敗する。手動抽出へフォールバックしない。runtime INFはa04msdcbootへ依存する。

## 国別設定

`regulatory.c` は元の512バイトからMAC、国コード、channel-map形式を読む。
Linux表番号とcustomized subbandを扱い、不明な国を世界共通設定へ置換しない。
最小ERP subsetではchannel14と5GHzを除外し、customのDFS指定はactive maskから除く。
scanと接続で同じmaskを強制する。

gen3の製造データ手順に従い、TX power(0x24)、mapped EFUSE channel offset(0x41)、
RSSI compensation(0x43)、domain(0x0f)、country channel power limit(0x49)、
band edge(0x36)、2G AC power(0x42)、元NVRAM(0x48)を順に送る。
国別電力上限はLinuxのdefault表と2G overrideを使用する。個体校正値は常に実行時読取り。
作業バッファとHIF payloadを消去する。コマンド送信成功はfirmwareの適用ACKではない。

## ビルド単位と依存関係

主担当のWindowsビルド環境で、checkoutのルートから実行する。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\build-mt6630-runtime.ps1
```

必要なもの:

- Windows WDK8.1のkm/shared/um headers、ARMのndis/ntoskrnl/hal/
  bufferoverflowfastfailk import libraries。
- KMDF1.13のheaders、ARM wdfdriverentry/wdfldr libraries（共有親用）。
- LLVM clang、lld-link、llvm-readobjとWindows PowerShell。
- 同一repoのnetwork core、shared parent、buttons PMIC、USB GPIO helper。
  SYSを別repoや外部の未記録patchに依存させない。

scriptはsession/IRQ/calibrationと親runtime/RTC/child IRQ/button D0/touch D0/
APMIXEDを明示的に有効化し、親とminiportをビルドする。通常のcandidateのDriverEntryは
リンクしない。TEMP/TMP/RUNNER_TEMPをcheckout内へ向け、終了時に環境を戻す。
出力は `build/mt6630-runtime/mt6630-runtime.sys` とそのINF/manifest、
`build/mt6630-runtime/a04-msdc-parent-probe/mt8167-shared.sys`。
同時に `build/mt6630-runtime/wlan-probe/mt6630-wlan-probe.exe` を生成する。
probeはWDK8.1 UM ARMのkernel32/advapi32.libとwlanapi.hも必要。
SYS/EXEは未署名。自動署名・実機配置・runner起動はしない。

firmwareは既存 `tools/winpe/prepare-mt6630-firmware.py` が固定元・digestから生成する
二つのsnapshotを主担当のmedia作成工程でWindows\System32\drivers\a04br3へ置く。
利用者に抽出や再ビルドを要求する方式ではない。firmwareの配布権は別途確認する。
実個体のMACや校正値をこのmediaへ組み込まない。

## 検証状態と主担当への引継ぎ

既存の変更関連テストをまとめて確認し、中核regulatory/station_runtime/wmt_bridgeの
ARM32 COFFコンパイルを実施した。新しい模擬テスト群は作成していない。
NDIS本体・Native control・NVRAM reader等は、公開WDK10の参照headersとNTDDI_WINBLUE、
既存と同じNDISマクロ展開overlayでARM32 COFFコンパイルを確認した。これは模擬型ではないが、
WDK8.1のheaders/librariesによる全体ビルドとは区別する。
WDF親の新フラグ付きコンパイル、WDK8.1 SYS link、CAT、署名は未確認。

主担当は上の一括ビルドを先に実行し、型・リンクエラーがあればそのログで修正する。
続いて既存共有親との統合とPnP資源、a04msdcboot経由のNVRAM読取り、RTC readback、
SDIO/CIS、WMT/WLAN起動、country設定、実SCAN_DONEとBSS列挙、OPEN APの認証・
関連付け・STA activationを順に実測する。観測のない段階を成功と扱わない。
通常WindowsのWLANサービスが必要とするOID集合、Native属性の受理、キャンセル・
PnP取り外し、省電力、DHCPを含む通常通信は実環境での確認が残る。

surprise removalでは新要求を閉じてworkerを排出し、MMIOなしでIRQを切り離す。
失敗途中のleaseと親参照は保持する。この保持はPnPの物理資源保持を保証しない。
この異常状態からの回復には主担当のreset/次試験が必要で、通常の再取得は許可しない。

## 一次ソース

- a04br3-linux-drivers `3d2c74e252d6f34a5980deda96bdc04064108d74`:
  pwrseq/a04br3-mt6630-pwrseq.c、wifi/mt6630-gen3のCFG_Wifi_File.h、
  common/wlan_lib.c、mgmt/rlm_domain.c、include/mgmt/rlm_txpwr_init.h、
  include/nic_cmd_event.h、include/nic/nic_rx.h。
- CT3 GPL kernel4.4: connectivity/commonのwmt_lib.c/wmt_ic_6630.c、
  combo_tool/src/stp_uart_launcher.cとcfg_folder/WMT.cfg/mt6630_ant_m1.cfg。
- Microsoft WDK: NdisMGetDeviceProperty、Native 802.11 attributes、
  DOT11_DATA_RATE_MAPPING_ENTRY（usDataRateValueは500Kbps単位）、各OID・status定義。

## 2026-11-07 WPA2・ARM link引継ぎ

主担当の報告ではc29fcbfはWDK8.1の全compileとshared parent linkを通過し、
Native最終linkだけがntoskrnl.lib(helpexcept.obj)のGS helperで失敗した。
全Wi-Fi/core/parent TUを `-O2 -fno-stack-protector` に統一した。
元からfno指定はあり、追加の原因はruntime clockの64bit除算による `__rt_udiv64` 参照。
`arm_clock.h` で同値の32bit分割計算へ置換し、GS helperの偽stubは追加しない。
最新変更のARM COFFと既存テストの確認は行ったが、最新WDK8.1 SYS link、
PnP、WPA2実接続、実scanは未確認で、主担当が検証する。
probeの使用法は `tools/winpe/README-mt6630-wlan-probe.md` を参照。

接続後の再scanでもBeacon/Probe Responseをdata受信成功扱いで消費せず、
station ownerのBSS観測処理へ渡す。peer deauth/disassocは引き続き受信入口で処理する。
この経路のソース修正とARM compileを確認したが、接続後再scanの実測は未確認。

## USB/audioとの単一parentビルド

既存touch/battery/buttons/audio/USBを維持する統合版は
`drivers/bus/mt8167-shared/WIFI_USB_AUDIO_INTEGRATION.md` を参照。
`build-mt8167-first-playback.ps1 -IntegratedUsb -IntegratedWifi` はparentを1回だけ生成し、
Wi-Fi runtimeはfeature/commit/hashを照合してそのSYSを再利用する。
個別runtime buildのparent SYSを統合版の後から上書きしない。

scan probe一式には起動用 `mt6630-wlan-observe.cmd` とprobe manifestを含む。
既存TOPRGU UART ownerへcount/statusレコードを送り、stdoutは主担当のCRC付きaudit-streamにも
収録できる。起動・レコード解釈・転送とscan成功の区別はprobe READMEを参照。
Ramanの `build-mt8167-peripherals.ps1` はprobe一式とmanifestを統合成果物へ含める。


## NDIS 6.30 初期登録のdirect OID契約

`ndis_runtime.c` はNDIS 6.30、driver characteristics Revision 2を登録する。
NDIS 6.20以降で必須の `DirectOidRequestHandler` と
`CancelDirectOidRequestHandler` を両方登録する。旧実装ではゼロ初期化のままだった。

このminiportはdirect OIDによるoffloadを実装・広告していないため、direct要求は
`NDIS_STATUS_NOT_SUPPORTED` で同期完了する。要求を保持せずpendingにもせず、
通常OIDのworkerへ流さず、`NdisMDirectOidRequestComplete` も呼ばない。
cancel handlerには保持中要求がなく、通常OIDやSDIOへ触れない。
直接要求は通常OIDと直列化されないので、この分離は必須。
NDIS申告versionを下げたり、未処理要求を成功扱いしたりしない。

根拠はMicrosoftのversion別契約とcallback仕様：

- [Implementing an NDIS 6.20 Driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/network/implementing-an-ndis-6-20-driver)
- [NDIS 6.20 Updates to NDIS 6.1 Features](https://learn.microsoft.com/en-us/windows-hardware/drivers/network/ndis-6-20-updates-to-ndis-6-1-features)
- [Implementing an NDIS 6.30 Driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/network/implementing-an-ndis-6-30-driver)
- [MiniportDirectOidRequest](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nc-ndis-miniport_direct_oid_request)
- [MiniportCancelDirectOidRequest](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nc-ndis-miniport_cancel_direct_oid_request)

汎用callback資料の「optional」はNDIS 6.1を含む説明。
6.20以降はversion別資料の必須要件も適用する。
この修正は登録契約の欠落を解消するが、実機で得られた `CM_PROB_FAILED_ADD (0x1f)`
の正確なNTSTATUSや、修正後のPnP受理・WLAN interface作成を実測したものではない。
SetupAPIのproblem値だけから `NDIS_STATUS_BAD_CHARACTERISTICS` 等を断定しない。

既存build入口は `tools/ci/build-mt6630-runtime.ps1`。
既存親packageを再利用する場合は `-SharedParentDirectory` のcapability/hash/commit確認を維持する。
署名・配備・実機確認は主担当で行う。INF・親driver・firmware・通常OID・MMIO・Bluetoothは
この登録修正では変更しない。


## 旧DriverStore再利用を避ける1002 package

run27後のrun28退避SYSは旧版 `d11e86bc728f0cd5d519becbd6bf7f328883a84672cc6c6678608a65742182af`。
新署名37a5bb1 SYSは `77b53ac0b902d5d6c864247707e9f319585ebb64d3a17b8a14696c8153e1a821`。
したがって旧SYSでのCode31/interfaces0は9d3a889修正の実行結果ではない。
同一1001 INFでの旧DriverStore再利用は原因候補で、返却されたproblemだけで断定しない。

source INFとbuild manifestのDriverVerを `11/06/2026,2026.11.6.1002` に更新。
同日付1001よりversionを上げ、Inf2Catの未来日問題を避ける。SYSの機能は変更しない。
既に署名済みの新SYSを再利用する最小手順（cleanな固定sourceで実行）：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\package-mt6630-runtime-1002.ps1 `
  -SignedSys .\build\wifi37a-signed\mt6630-runtime.sys `
  -OutputDirectory .\build\wifi1002-package-01
```

この専用レシピは確認済み新SYS hashだけを受理し、新規dirへSYSとsourceの1002 INFだけをコピー、
WDK8.1 Inf2Cat `/os:6_3_ARM /uselocaltime` でCATを新規生成し、既存開発鍵でCATを署名する。
SYSは再署名も再buildもしない。公開CERとmanifestはpackage外へ保存し、
manifestにpackage source commit、binary source37a5bb1、INF版、全package hash/lengthを記録する。
旧CATや1001 INFを上書き流用しない。秘密鍵生成・配備・runner操作は行わない。
別のSYSをbuildした場合はこの固定hashレシピの対象外で、出自確認後に別候補として扱う。

主担当は1002 packageをstage/bind後、実選択oem INFのDriverVer=1002と、
実配置SYS hash=`77b53ac0...`の両方を回収して確認する。
bind成功やDISM exit0だけで新SYS実適用としない。
新SYS実適用確認後に初めてCode/CM problem/interface数を比較する。


NDIS6.30初期化・標準WLAN列挙・scanの属性修正と、未実装必須OID/容量の一覧は
[INITIALIZATION-CONTRACT.md](INITIALIZATION-CONTRACT.md) を参照。INF1002候補とは別のsource差分で、
この修正だけで全面準拠や実WLAN列挙成功とは扱わない。
