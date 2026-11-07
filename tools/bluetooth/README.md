# MT6630 通常OS read-only probe

`mt6630-bluetooth-probe.exe` は引数なしのARM32通常OS用client。
BT排他profileのkernel起動結果と、標準Bluetooth radio列挙の結果を別に取得する。
Wi-Fi同時使用、pairing、通信、SCOの成功判定には使用しない。
HardwareProbe本体、native installer、一括builderは変更／呼出ししない。

## 固定build手順

同一repoの、このprobeとkernel status ABIを含む固定commitをclean checkoutして、
主担当のWindows WDK8.1 ARM32環境で次を実行する。

```powershell
# kernelのみ。既存のmatching BT排他親を使用する場合。
.\tools\ci\build-mt6630-bluetooth.ps1
# probeだけ（driver/parent/HardwareProbeを再buildしない）
.\tools\bluetooth\build-mt6630-bluetooth-probe.ps1
```

必要物：WDK8.1 `Include/um`, `Include/shared`, ARM `kernel32.lib`、clang、
lld-link、PowerShell、Git。kernelには従来どおりKMDF1.13とARM kernel libsが必要。
全TUで既存ARM GS helper回避と同じ `-fno-stack-protector` を使用する。
probe既定出力はcheckout内 `build/mt6630-bluetooth-probe/`。署名は主担当。
CI helperはEXE、README、SHA-256/source commit入りmanifestだけを生成する。
自動CI／runner起動、service登録、再起動、配置は行わない。

c715ad2の旧SYSにはstatus診断口がないため、新probeだけを追加してもkernel結果を
取得できない。この変更のSYS／status ABI1と組み合わせる必要がある。
現在Ramanが作業する最新audio／Engine親を古いbranchの親で置き換えない。
親profileは従来どおりBT専用。今回の変更では親sourceを変更していない。

## 実行・回収

通常Windows RT 8.1で管理者またはLocalSystemとして、主担当のprivate audit作業directoryを
カレントにし、署名済みEXEを引数なしで起動する。固定名
`mt6630-bluetooth-probe.txt` をカレントに上書き作成し、同じ数値行をstdoutへ出す。
任意のHCI、firmware、device path、出力path引数を受け取らない。

既存 `\\.\A04BR3ScreenUart` が存在すれば、TOPRGUの既存queueへ同じ短い行を送る。
UARTは任意出力であり、その不在／送信失敗をBT controller失敗とは扱わない。
`UART_OPEN_ERROR`／`UART_SEND_ERROR` とdisk log／実process exitを回収する。
queue受付はhost到着の証拠ではなく、host側の対応RUN／SEQ／ENDを確認する。
新serviceやMMIO accessは追加しない。HardwareProbeへの組込みは今回行わない。

## kernel snapshot

`\\.\MT6630BluetoothStatus` へGENERIC_READで固定IOCTLを1回送る。
診断口はdriver-load時に作成し、FDO起動失敗時にも保存済みstatusを読める構成。
DACLはSYSTEM full／Administrators readのみ。入力は0byteで、返すのは80byteの
ABI1 structに定義した数値だけ。未知IOCTLと入力付き要求を拒否する。
FDO pointerやNVRAM、HCI packet、任意読出し機能は公開しない。
診断IOCTLはSDIO／HCI／firmware／電源遷移を実行せず、spinlock内のcopyのみ。

`attempts`, `stage`, `start_status` はkernel自身の起動試行結果。
`stage` は0=入口、1=固定WMT取得、2=動的NVRAM、3=親lease、4=host/IRQ、
5=SDIO/WMT identity、6=WMT patch起動、7=BT初期化、8=Prepare失敗。
`state` は0=driver loaded、1=開始中、2=起動処理終了、3=transport故障、
4=停止、5=surprise removal、6=Prepare失敗。終了stateだけで成功と扱わない。
`start_status` はULONG表現のNTSTATUS、`transport_status` は同様に32bit表現の内部int。

- `read_version_sent`：起動時0x1001のHIF送信完了。応答完了とは別。
- `read_version_complete`：同opcodeの実Command Completeを受信。
- `read_version_status`：そのcontroller status。応答未受信は4294967295。
- `read_version_length`：statusを含むreturn parametersの実長。
- `version_valid`：status0／長さ9を確認し、version数値をdecode済み。
- version数値はHCI/LMP version/revision、manufacturer、subversionのみ。
  BD_ADDRやfriendly name、校正、鍵は要求・出力しない。

`KERNEL_START_VERSION_OK=1` はsnapshot読取り成功、state2、起動NTSTATUS0、
現在running、実0x1001送信／Command Complete／status0／長さ9／decode、
BT排他profileを全て満たした場合だけ。停止・故障後も起動時のversion記録は残るが、
この現在状態の判定は0になる。probeは未完了のkernel処理を待機・再試行しない。

## 標準OS受理は独立判定

System32の固定 `bthprops.cpl` から `BluetoothFindFirstRadio`／`NextRadio`／
`RadioClose` を動的取得し、標準radio handle総数だけを記録する。
`BluetoothGetRadioInfo`、remote device列挙、name取得、discoverability設定は呼ばない。
`RADIO_ENUM_COMPLETE` と `RADIO_API_ERROR` をcountと別記する。
総数には別adapterが含まれうるため、`RADIO_COUNT>0`だけでMT6630成功と扱わない。

別にSetupAPIでGUID_BTHPORT_DEVICE_INTERFACEを列挙し、hardware IDが固定
`A04BR3\MT6630HCI`と一致するdevnodeだけを数える。ID文字列自体はログしない。
`TARGET_INTERFACE_ACCEPTED=1` は列挙正常終了、同IDのactive interface、
CM_Get_DevNode_Status成功／DN_STARTED／problemなしを全て満たす場合。
これは標準interfaceの列挙受理の証拠であり、HCI version応答・無線接続・音声成功とは別。
各列挙は64件で上限エラーにし、不完全なcountを完了扱いしない。
DLL／export不在は数値エラーとして記録する。WinPEでAPIが無い場合は通常OS向けであり、
無線不良やkernel HCI応答失敗と混同しない。DLLを同梱・コピーして補わない。

process exitは0=kernel現在起動/version条件成立、2=その条件未成立／snapshot取得失敗、
4=disk結果保存失敗。標準radio列挙とUARTは独立した数値結果であり、exit0はそれらの成功を
意味しない。最終flush失敗時は既出のEXIT行より実process exitを優先する。

## 検証範囲

公開WDK/KMDF参照headerでのARM32 compile、限定した既存BT protocol回帰、
PowerShell Parserを使用。WDK8.1最終署名SYS／実probe起動／RT8.1でのDLL export／
実PnP受理／実UART回収は主担当の未確認事項。実機／runner／REAは操作していない。
SCO capabilitiesは従来どおりNone/0で、公式のWindows 8/8.1 BTHX load契約に不適合。
実機受理未確認というだけでなく、公開契約上の標準stack対応候補ではない。

一次仕様：
[BluetoothFindFirstRadio](https://learn.microsoft.com/en-us/windows/win32/api/bluetoothapis/nf-bluetoothapis-bluetoothfindfirstradio)、
[SetupDiGetDeviceInterfaceDetailW](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceinterfacedetailw)、
[CM_Get_DevNode_Status](https://learn.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_get_devnode_status)。

## BT用の親buildを音声clientから切り離す

`build-mt6630-bluetooth.ps1 -BuildSharedParent` は内部でfirst-playbackを
`-IntegratedUsb -BluetoothProfile -ParentOnly` として呼び出す。
親SYSとINF、flags／hash／runtime blob一覧のmanifestだけを生成し、
audio SYS、旧Direct KS client、USB childは再buildしない。
親のAudio v11／USB／touch／battery／buttonsのruntime flagsはそのまま保持する。
旧Direct KS clientのGUID link失敗を修正する必要はBT buildにはない。

```powershell
# 新規に親だけをcompile/linkし、その後BT SYSへ進む
.\tools\ci\build-mt6630-bluetooth.ps1 -BuildSharedParent
# 親単独入口（BT SYSもbuildしない）
.\tools\ci\build-mt8167-first-playback.ps1 -IntegratedUsb -BluetoothProfile -ParentOnly
```

主担当の既存c715ad2 checkoutを新固定commitへ更新し、gitignoredな既存build directoryを
保持した場合は、次で親の再compileを省略できる。`-BuildSharedParent`とは排他。

```powershell
.\tools\ci\build-mt6630-bluetooth.ps1 -ReuseParentDirectory `
  'C:\mt6630-bluetooth-c715ad2\src\build\mt6630-bluetooth-7999b7b\audio-usb\a04-msdc-parent-probe'
```

再利用元は同一checkout内の既存directoryに限定し、固定名のSYS／INF／manifest.jsonを読む。
同じdirectoryを出力先として使う場合もSYS／INFを再生成せず、元manifestをhash名で保存する。
別の出力先へcopyする場合もcopy後のSYS hashを照合する。

再利用条件は、manifestのclean source申告、明示source commit、全親flags一致、
SYS SHA-256一致、INFの固定source一致、Git上の親runtime blob集合の完全一致。
`runtime_blobs`がmanifestにあれば、その記録も照合する。旧manifestにblob一覧がなくても、
記載source commitと現HEADの実blob IDを比較する。旧SYSのbuild source自体はmanifestの
申告に基づくものであり、SYS逆解析や実機確認による証明ではない。

manifestは `source_dirty=false`、`source_commit`（再利用連鎖では`parent_source_commit`）、
`parent_flags`、`sys_sha256` を必要とする。SYS hashは従来の `files[].path/sha256` の
一意なmt8167-shared.sys記録でも受理する。不足・不一致時は拒否し、自動再compileへ
fallbackしない。c715ad2の旧標準wrapperはKS終了後に全体manifestを書くため、今回
runnerにあると申告されたmanifestの形式・必要項目の有無は、非操作条件のため未確認。
証拠のないflags/hashを補って再利用成功にはしない。不足時は上記ParentOnlyで親だけを作る。

BT最終manifestはBTの`source_commit`と別に`parent_source_commit`、親SYS／manifest hash、
`shared_parent_built`／`shared_parent_reused`を記録する。c715ad2由来なら親sourceはc715ad2の
完全commit IDのままで、新BT commitから親も再buildしたかのようには記録しない。
親成果物をdirectory全体から一括回収せず、manifestで指した親SYS／INFだけを使用する。
古い音声clientやUSB成果物が既存directoryに残っても今回の成果物には含めない。

今回の変更はrecipeのみ。実WDK成功のユーザー申告は親／audio SYSまでであり、
BT本体compile／link成功ではない。こちらではPowerShellのParentOnly再利用分岐・
不一致拒否・構文をローカル確認し、runner／実機にはアクセスしていない。

## 正式BT親INFへの更新（SYS再compileなし）

BT ParentOnlyは固定source
`drivers/bus/mt8167-shared/mt8167-shared-bluetooth.inf`を出力名`shared-parent.inf`へ配置する。
hardware IDは既存`ACPI\BEN0001`、表示名は実ICの
`MediaTek MT8167 Shared Resource Controller`、既存サービス`Mt8167Shared`／
SYS名／KMDF1.13は維持する。DriverVerは固定
`11/06/2026,2026.11.7.2001`。同日付のEngine親 `2026.11.7.1003`（normal `2026.11.6.1001`も含む）より版を上げる。
Bluetooth用排他profileであり、WLANとの同時使用を意味しない。
EnginePcm／audio v11の新cache親との組合せはRamanの後続source統合対象。

7999b7bで成功した親を利用し、INFとmanifestだけを組み直すには、固定commitの
clean checkoutで次を実行する。親SYS、BT SYS、audio、KS client、USBのcompilerを呼ばない。

```powershell
.\tools\bluetooth\prepare-mt6630-parent-profile.ps1 -ReuseParentDirectory `
  'C:\mt6630-bluetooth-c715ad2\src\build\mt6630-bluetooth-7999b7b\audio-usb\a04-msdc-parent-probe'
```

既定出力はcheckout内`build/mt6630-parent-profile/a04-msdc-parent-probe/`。
新INF、元とSHA-256が一致する親SYS、manifestを署名候補にする。
元の7999b7b packageは既定では変更しない。新CATの生成／署名は主担当が行う。
INFが変わるため旧CATは流用しない。SYSの再compileは不要だが、新packageのCATには
既存SYSと新INFの両方を含める。コード署名／実機適用はこのscriptでは実行しない。

manifest format2ではkernelの`parent_source_commit`／`runtime_blobs`と、
`inf_source_commit`／`inf_source_path`／`inf_source_blob`／`inf_driver_ver`／`inf_sha256`を分離する。
SYS hash、全flags、kernel runtime blob一致は引き続き必須。
format1でruntime一覧に混在していたprototype INFのblobは、元commitと一致を確認してから
別扱いへ移す。入力INFも元の固定source／記録hashと照合し、任意のINF改変を受け入れない。
INF更新だけをkernel変更と誤認して再compileさせず、新kernelからbuildしたとも記録しない。
出力は`parent_profile=mt6630-bluetooth-exclusive`、
`wifi_bluetooth_simultaneous=false`、`catalog_requires_regeneration=true`を記録する。
元manifest／元INFのhash名archiveは出自確認用で、署名packageへ混ぜない。
元INF archiveは`.inf-source`拡張子とし、Inf2Catが別の旧INFとして拾うことを避ける。


## 実capabilities応答とSCO契約を分離して記録する

新SYSは同じ固定read-only deviceの別IOCTLから40byteのcapabilities観測snapshotを返す。
旧80byteの起動時HCI snapshot ABI1は維持する。双方とも入力0byte・固定構造体のみで、
新queryはSCO route設定やHCI送信、power-up、radio探索を実行しない。

`BTHX_CAPS_QUERIES`はkernelから受けた実QUERY_CAPABILITIES回数。
`BTHX_CAPS_RETURNED`は最後の問い合わせに構造体を正常に返したか、
`BTHX_CAPS_STATUS`はそのNTSTATUS（ULONG表現）。実advertised mode/channelと、
公式必須mode2（HCIBypass）/channel1を別の数値行で記録する。
metadata IOCTL自体がSTATUS_SUCCESSでも、この契約へ適合したことやstack loadの証拠にはしない。

`BTHX_CONTRACT_SATISFIED`は実応答を比較した0/1。
問い合わせ未受信または構造体を返していなければ4294967295（未観測）とする。
現在のNone/0を実際に返した場合は0で、`BTHX_CONTRACT_BLOCKED=1`となる。
BLOCKED=0だけでは成功を意味せず、問い合わせ前かもしれない。
現在のsource構成は公式契約に不適合と確定しているが、kernelに実問い合わせが来たかは
この観測で別に確認する。単なるenum定義をSCO無し許容の根拠として扱わない。

古い02e3915以前のSYSで新queryが未対応なら`BTHX_CONTRACT_QUERY_ERROR`を記録し、
起動時HCI snapshot／radio API判定と混同しない。観測できない値をNoneや成功に補完しない。
`KERNEL_START_VERSION_OK`とprocess exit0は引き続きraw HCIの条件だけであり、
BTHX契約適合、標準interface受理、pairing、無線／SCO動作の成功を意味しない。
必須SCO bypass経路が成立していない限り、このpackageを標準Bluetooth利用可能として配布しない。

build手順は前述のkernel helperと独立probe helperのまま。
新機能はSYSとEXEの両方をbuild/署名する。親SYS／親INF、HardwareProbe、native、
一括builderは変更していない。実機／runner／REAは主担当の専有を維持する。
