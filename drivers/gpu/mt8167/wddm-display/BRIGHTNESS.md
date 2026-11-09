# A04BR3: MediaTek MT8167 DISP_PWM0 brightness

## 実ハードと一次ソース

対象はMediaTek MT8167 **DISP_PWM0**（0x1100f000/0x1000）。外付けbacklight IC名は
一次ソースで確定していないため付けない。既存display PDO MOU0007が専有し、
GE8300 GPU providerとBEN0001共有parentの資源・API・leaseは変更しない。

Linux CT3 commit `7c4782127d4ea11965be00ce1343056be9bfb769`:

- [mt8167.dtsi](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/arch/arm64/boot/dts/mediatek/mt8167.dtsi):
  `disp_pwm0@1100f000`。`led6`は`mediatek,lcd-backlight`、led_mode=5。
  a04br3.dtsはled6のpwm_configを指定し、このmodeを上書きしない。
- `drivers/misc/mediatek/leds/mt8167/mtk_leds_sw.h`: mode5はCUST_BLS_PWM。
- [ddp_pwm.c](https://github.com/s1204IT/CT3_GPL_LGPL_Src/blob/7c4782127d4ea11965be00ce1343056be9bfb769/kernel-4.4/drivers/misc/mediatek/video/common/pwm10/ddp_pwm.c):
  CON1のperiod=1023、dutyはbits28:16の13bit、level remapはidentity。
  duty更新後COMMITへ1→0をCPUから書く。0/OFFは別途ENとGPIO enableを扱う。
- `drivers/misc/mediatek/video/mt8167/dispsys/ddp_reg.h`:
  EN+0、COMMIT+8、CON0+0x10、CON1+0x14。

XNU機種source [bsd/dev/a04br3_display.c](https://github.com/mouseos/xnu_kernel_benesse_a04br3/blob/f5d43bf1677955e7e367545b9bbbd6449776cdda/bsd/dev/a04br3_display.c)
のGPIO_LCD_BACKLIGHT=1、panel power-on/offはGPIO1を制御する。
この機種コードは連続brightness/PWM levelの実装根拠ではなく、別のGPIO enable ownerがある根拠。
Windows BEN0001がGPIO窓を所有するため、DODからGPIO1をmap・書換えしない。

## Windowsへの実接続

`KMDDOD_INITIALIZATION_DATA.DxgkDdiQueryInterface`を登録し、公開の
GUID_DEVINTERFACE_BRIGHTNESS / DXGK_BRIGHTNESS_INTERFACE version1を返す。
GetPossibleBrightness、SetBrightness、GetBrightnessを実MMIOへ接続し、
機種flag有効時はfixed built-in panelをINTERNALとして通知する。
追加service、WMI偽装、SYSTEM hive変更、registry brightness値の疑似保存は行わない。

公開契約:
[DXGK_BRIGHTNESS_INTERFACE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/ns-dispmprt-_dxgk_brightness_interface)、
[DxgkDdiQueryInterface](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkddi_query_interface)。
v1構造・callback形状とKMDDODのQueryInterface memberは公開WDK dispmprt.hでも照合した。
ホストの最小WDK型stubでの試験と本物のWDK SYS buildを区別する。

対応levelは**1～100%**。0/OFFをサポートしたと偽ってGPIO enable ownerを迂回しない。
PWM EN=1、period=1023、duty1～1023、COMMIT=0で引き継いだ場合だけinterfaceを提供する。
EN=0や未知設定ならbrightnessは非対応/エラーに留め、POST表示のStartDevice自体は継続する。
PLL、TOPCK mux、divider、MMSYS、PMIC、GPIO、DSI、GPU DMA/queueには書かない。
調光で書くのはCON1 duty fieldとCOMMITだけ。ENも変えない。

dutyはpercentageを1023段階へ丸めて変換し、他fieldを保持する。書込み後にCON1/CON0/EN/COMMITを確認し、
不一致はエラーとpoisonを返す。Getはcached successではなく現在の実registerを読む。
輝度の物理応答、panelへのPWM配線、Windows UIでの受理は実機未確認。

brightness操作とStop/Powerは独立mutexで直列化する。D3ではcallbackを閉じ、電源callback内の
MMIO書込みやGPIO操作は行わない。D0後も引継ぎ設定を再検査してから調光する。
interfaceは独立したnonpaged参照count付きcontextを保持する。
Stopは先にcontextを閉じてunmapし、古いinterfaceはDEVICE_NOT_READYを返す。
interfaceが残っていてもDOD objectやunmapped MMIOへアクセスしない。

## build recipe

通常buildは両方のflagが既定OFF。主担当の既存GPU/DOD設定に次だけ追加する。

```powershell
$env:TEMP=Join-Path (Get-Location) 'build\brightness-tmp'
$env:TMP=$env:TEMP
$env:RUNNER_TEMP=$env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null
$env:A04_MT8167_DISPLAY_BRIGHTNESS='1'
# 既存WDK8.1 ARM / MSVC ARM / GPU設定を維持する
& tools/ci/build-wddm-display-arm.ps1
```

scriptはcompile/link確認用で、`mt8167_brightness.cxx`を含めflagを全DOD TUへ渡す。
生成build-check SYSをそのまま配布せず、主担当の正式package buildへ同じTU/flagを反映する。
INF DriverVerは`11/06/2026,0.0.1.4`。既存0.0.1.2より新版で、環境時計の変更は不要。
SYS/CAT署名とPnP package配置は既存の主担当transaction方式を使用する。

UEFI正本の機種package用独立branch `work/mt8167-display-brightness` も必要。
既存MOU0007 `_CRS`へDISP_PWM0 pageだけを追加する条件付き変更である。
旧bootのままDODだけを有効にしても、未割当アドレスを勝手にmapしない。

```bash
export TMPDIR="$TASK_ROOT/port/gsi/logs/ios-feasibility/mt8167-brightness/tmp"
export A04_DIAGNOSTIC_DISPLAY_BRIGHTNESS_RESOURCES=1
# 他の既存baseline envと引数をそのまま維持して機種build-diagnostic.shを使用する
```

flagはdisplay provider有効時のみ受理し、firmware manifestの
`A04BR3_DISPLAY_BRIGHTNESS_RESOURCES`にも記録する。
Windows側とUEFI側の両commitを統合してから、主担当が新boot/WDK packageをbuildする。
既存bootは本作業で置換しない。

## 検証と未確認

`make test-mt8167-brightness`で実callback/ownerを模擬MMIO上で実行する。
資源なし/重複/非専有/無効PWMを拒否し、1～100全level、size query、未知version、
reserved field/他register保持、D3拒否、設定変化拒否、write失敗poison、Stop後interfaceの寿命を確認。
ASan/UBSanも確認する。UEFIはbrightness/GCEのOFF/ON全4組をiasl/checksum/resourceで検証する。

未実施: WDK8.1全DOD compile/link/CAT/署名、新FD/boot生成、Windows UI上のbrightness slider、
実PWM duty/画面輝度、長時間・panel power cycle/resume。実機・runner・REA・カメラは操作しない。
GPIO enableを伴う0/OFF、CLK未初期化状態からの起動、ALS/自動輝度、nits interfaceは未実装。

## 変更ファイル

- `drivers/gpu/mt8167/wddm-display/mt8167_brightness.cxx` / `.hxx`: DISP_PWM0 owner、標準brightness interface。
- 同directoryの`bdd.cxx` / `bdd.hxx` / `bdd_ddi.cxx`: 実DDI、Start/Stop/Power、internal panelの接続。
- 同directoryの`mt8167-display.inf`: DriverVer更新。本資料は`BRIGHTNESS.md`。
- `tools/ci/build-wddm-display-arm.ps1`: TU/flagとbuild-check manifest。
- `Makefile`、`tests/mt8167_brightness_test.cpp`、`tests/brightness_wdk_stubs/*.h`: ホスト検証入口と型stub。
- UEFI機種packageの`WindowsArm32/Acpi/DisplayProvider.asl`、`WindowsArm32/scripts/build-diagnostic.sh`、
  `Tests/check_display_brightness.py`、`WindowsArm32/DISPLAY-BRIGHTNESS.md`: 専有資源、flag/manifest、iasl検証。

## 既署名SYSを再利用する修正版package

run28では実SYS hashが候補と異なり、候補INFが0.0.1.2の同版だったとの主担当報告。
source 0c73f27のINFは0.0.1.3だったが、包装されたINFをsourceと同一とみなしてはならない。
現在のsource INFを0.0.1.4へ上げ、旧候補INFを入力にせずこのsource INFを必ず使う。

既署名brightness候補SYSの期待SHA256は
`71f4ddaec639045a8b7dc1aaa7bc4c341e3f5955e2bec3b00f7a1a91528c7dce`。
このSYSは再compile・再署名しない。新CATは新INFと既署名SYSからInf2Catで生成し、CATだけを既存証明書で署名する。
`sign-ge8300-display-test.ps1`はSYSも再署名するため、この再包装には使用しない。

```powershell
& .\tools\ci\repackage-signed-mt8167-display.ps1 `
  -SignedSys <既署名brightness候補のmt8167-display.sys> `
  -OutputDirectory (Join-Path (Get-Location) 'build\brightness-inf-1.4') `
  -CatalogCertificateThumbprint <既存の署名証明書thumbprint>
```

scriptは固定SYS hash・埋込署名の存在、source INF新版、空の新出力directoryを要求し、
CAT生成/署名後も入力/出力SYSが同一hashであることを確認する。環境時計・証明書storeは変更しない。
OS側の既存証明書信頼とPnP適用は主担当の既存transaction方式で別途扱う。
recipeは今回Windows/runnerで未実行。新INF/CAT作成と、適用後の実SYS hash確認を区別する。

## 標準Windows APIの単発30%/70% probe

`tools/display/test-mt8167-brightness.ps1`はWindows PowerShell 3以上とCimCmdletsを
使うローカル専用client。Windows 8.1/ARM32の通常OS向けで、追加native DLLや
Dxva2/DDC/CI、private IOCTL、MMIO mapping、署名済みSYS変更は不要。
Monitor.sysが内蔵panelへ公開する`root\wmi`の`WmiMonitorBrightness`と
`WmiMonitorBrightnessMethods.WmiSetBrightness`を使う。
対応WMI instanceが公開されていなければ、別経路へ迂回せず失敗を記録する。

公式根拠（確認2026-11-07）：

- [内蔵panelとMonitor.sys/WMI、KMD brightness interfaceの関係](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/supporting-brightness-controls-on-integrated-display-panels)
- [WmiMonitorBrightnessのCurrentBrightness/Level/InstanceName](https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmimonitorbrightness)
- [WmiSetBrightnessのuint32 Timeout/uint8 Brightness、戻り値0=成功](https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmisetbrightness-method-in-class-wmimonitorbrightnessmethods)
- [Invoke-CimMethodのnamed ArgumentsとOperationTimeoutSec](https://learn.microsoft.com/en-us/powershell/module/cimcmdlets/invoke-cimmethod?view=powershell-5.1)
- [Get-CimInstanceによるsnapshotの再取得](https://learn.microsoft.com/en-us/powershell/module/cimcmdlets/get-ciminstance?view=powershell-5.1)

```powershell
# ソースcheckout、またはtools/display以下の配置を保ったdeployment rootで実行。
# 出力は同root内の未作成directoryを指定する。
powershell.exe -NoProfile -NonInteractive -File .\tools\display\test-mt8167-brightness.ps1 `
  -OutputDirectory "$PWD\out\brightness-api-once"
# Active instanceが複数の場合だけ、WMIの実InstanceNameを完全一致で指定：
#   -InstanceName '<実際のWMI InstanceName>'
```

Activeなbrightness instanceを1つに限定し、同じInstanceNameのActive methodsを
1つ要求する。30、70、初期値がすべてLevel配列に存在することを確認し、近似levelへ
丸めない。元のpercentと復元情報をresult.jsonへ保存した後、Timeout=uint32 0 /
Brightness=uint8を名前付きで渡し、30→70→元percentを実行する。
各設定の戻り値0だけでなく、新しいGet-CimInstanceによるCurrentBrightness一致を
確認する。初期snapshotの再利用はしない。成功には30/70/復元の全readbackが必要。

各CIM操作はOperationTimeoutSec既定3秒、readback最大4回、間隔250ms。
通信・provider timeoutも失敗として記録し、最初のSetが失敗・timeoutを返しても
副作用の可能性があるためfinallyで元percentのSet/readbackを試みる。
復元失敗は独立restore_error/restore_verified=falseと非zero終了で残す。
OS停止・process強制終了・応答しないkernel/providerからの復元までは保証しない。
hard-kill watchdogでfinallyを潰す設計にはしない。電源policy/ALS/サービスは変更せず、
競合で値が戻ればreadback不一致として扱う。常駐・連続試験にはしない。

**このAPIは生PWMDUTY/CON1を公開しない。** result.jsonのCurrentBrightnessは
標準WMI結果であり、Monitor.sys/providerのcacheを排除したraw register観測とは
断定できない。既存DODのGetはCON1を読むが、WMIの毎照会がGetへ直結することは
今回未実測。`expected_duty_from_source`の30%=307、70%=716は0c73f27の式からの
予測値だけで、`raw_pwm_observed=false`を常に記録する。
復元も元の標準percentの復元であり、初期PWMがpercent境界でなければ元raw bitsと
完全一致する保証はない。raw PWMDUTYの証拠には別の正規kernel観測口が必要だが、
今回その追加やSYS再署名はしない。光学的な輝度の証明とも区別する。

result.jsonには対象、対応levels、初期percent、各設定status、各readback、時刻、
復元結果とclient SHA256を保存する。失敗でも保存しPowerShellの終了を失敗にする。
主担当は既存transactionの実採用SYS hash/INF14確認と結果を対で回収する。
署名済み71f4 SYS/新CATの作成成功だけをWMI成功へ読み替えない。

ホスト試験（実CIM/実機アクセスなし）：

```powershell
pwsh -NoProfile -NonInteractive -File .\tests\brightness_client_test.ps1 `
  -OutputDirectory "$PWD\build\brightness-client-tests-unique"
```

mockで成功30→70→50、設定error/timeout後の復元、readback不一致の有界停止、
非対応level/複数instance/method不在で書込みゼロ、復元失敗の独立判定を確認する。
PowerShell parserと実scriptをホストPowerShell 7.4.6で実行する試験であり、
Windows PowerShell 3/4の実CIM・ARM32通常OS・実WMI/実PWM成功は未検証。

PS1を起動できない通常Windows RT向けには、独立EXEの固定COM版を追加した。
source helper/固定log/one-shot接続/WDK recipeは`tools/display/BRIGHTNESS-NATIVE.md`を参照。
既存署名済みDODを変更せず、主担当の既存起動/回収経路へ接続する準備だけである。
