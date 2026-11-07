# 内蔵panel固定WMI native client

GPU作業と分離したclient準備のみ。PS1、SSH、USB keyboardを実機上で必要とせず、
主担当の既存one-shot起動/回収経路から固定EXEを起動するためのソースとWDK recipe。
サービス登録、媒体更新、runner操作、実機投入は本変更に含めない。
既署名DOD/INF14/CAT、UEFIとGPU queue/DMA/parent所有権は変更しない。

## 固定操作と接続方法

`brightness_native.c`はWin32 C/COM、`brightness_sequence.c/.h`は固定手順のsource helper。
`brightness_native.h`の`A04RunBrightnessNativeProbe(void)`が固定引数なしの入口。
既定でGUI subsystemの`WinMainCRTStartup`から呼び、終了codeを返すのでconsole/入力は不要。
mainが既存の署名・one-shot起動経路で`mt8167-brightness-native.exe`を起動し、
固定`C:\MT8167EngineProbe\brightness-native.log`を既存回収処理で取得する。

logはCREATE_NEWで既存証拠を上書きしない。同じlogがあれば終了code16で無変更。
これは重複起動も防ぐ。再試験は主担当が旧logを回収・保存した後で明示的に退避する。
logを作れない/初期checkpointをflushできない場合も変更しない。
UART/MMIOアクセスは追加しない。任意path/query/command、remote WMI、設定file、
CLI引数を受け取るbrokerはない。ソースの固定2queryと固定WmiSetBrightnessだけを使う。

同一processへソース接続する場合は`A04_BRIGHTNESS_NATIVE_NO_ENTRY`でEXE入口を除外できる。
ただしhelperは自分でCOM securityを初期化する**fresh process用**で、既にCOM securityを
初期化したservice内へ直接呼ぶものではない。CoInitializeSecurity等の失敗も無変更で記録。
通常は独立EXEの固定起動とする。コードは起動serviceや任意command実行口を追加しない。

## 判定・復元

1. IWbemLocatorでlocal ROOT\WMIへ現在のsecurity contextで接続する。
2. ActiveなWmiMonitorBrightnessがちょうど1つであることを要求する。
   元CurrentBrightnessがuint8の1～100、Levelに元値/30/70が存在することを確認する。
   同じInstanceNameのActive methodsがちょうど1つあることと公式method入力signatureを取得。
   ここで失敗ならSetは一度も呼ばない。曖昧なpanelを勝手に選ばない。
3. instance名と元percentを固定fileへ書き、FlushFileBuffers後に30→70を実行する。
   各設定後は保存snapshotを使わず、元の__RELPATHをGetObjectしてActive/InstanceNameと
   CurrentBrightnessを検査する。各readbackは最大4回、間隔250ms。
4. 書込みを試みた後はerror/timeout/readback不一致/log失敗でも必ず元percentのSetへ進む。
   復元Setと復元readbackを別々に記録する。途中失敗を復元成功で上書きしない。

MOFのuint32はAutomationでは**VT_I4**、uint8はVT_UI1として渡す。
Timeout=0、Brightness=30/70/元値をofficial method入力instanceへPutし、ExecMethodする。
transport HRESULTとmethod ReturnValueを別々に記録し、両方0のみ成功と扱う。
正のWBEM_S_TIMEDOUTをSUCCEEDEDとして扱わない。readback対象の消失/型不一致も失敗。

ExecQuery/ExecMethod/GetObjectはsemisynchronous、Next/GetResultObjectの待ちは3秒。
methods列挙は16件で打切り、readbackも固定回数。ConnectServerは公式
WBEM_FLAG_CONNECT_USE_MAX_WAIT（最大2分）を指定する。
これは壊れたCOM/RPC/provider全体のhard deadlineを保証するものではない。
method timeout後の呼出しが後から実行される可能性やprocess強制終了時には、復元を
保証できない。既に失敗した試験を成功とは報告せず、復元もbest effortとして残す。
policy/ALS/serviceを停止・変更せず、他の輝度制御との競合もreadback失敗として扱う。

## logと終了code

ASCII固定行にtick、phase、requested_hex、observed_hex、status_hexを8桁hexで保存する。
`wmi-method-return`はrequested=percent、observed=生WMI ReturnValue、status=HRESULT。
`original-percent`は元値を示す。`set-percent`/`read-percent`、`restore-set`/`restore-read`で
各操作を区別する。観測値ffffffffは未取得。instance名は別UTF-8行へ記録する。
通常結果は`result-flags`のrequested_hexにも保存する。

終了codeはbitmask：0=全標準操作/readback/復元/log成功、1=30/70試験失敗、
2=復元失敗、4=log失敗、8=preflight失敗、16=log新規作成不可。
log自体が書けない場合は終了codeを主担当の起動側でも回収する。

**WMI readbackはraw PWMDUTY観測ではない。** headerにraw_pwm_observed=0を固定記録する。
復元対象も元の標準percentで、元raw PWM bitsの完全復元や光学輝度は証明しない。
DOD GetがCON1を読む事実だけから、Monitor.sys/WMIの毎照会でrawが更新されるとは断定しない。

## 固定WDK8.1 build recipe

```powershell
& .\tools\ci\build-mt8167-brightness-native.ps1 `
  -OutputDirectory "$PWD\build\brightness-native-unique" `
  -CertificateThumbprint <主担当既存のEXE署名証明書thumbprint>
```

thumbprint省略時はunsignedのcompile/link確認だけ。既存env
A04_TEST_CERTIFICATE_THUMBPRINTがあればそれを使用する。証明書の作成/登録/信頼変更はしない。
正式なWindows RT code integrity条件は主担当の既存方式で満たす。

clang thumbv7-pc-windows-msvc→ARMNT、Windows8.1 ARMのkernel32/ole32/oleaut32/wbemuuidを
/nodefaultlibでlinkする。引数とlibraryが固定されたrecipeで、任意code実行機能は追加しない。
TEMP/TMPは出力内。manifestにsource/header/library/EXE hashes、source commit/dirty、
署名実行の有無を記録する。WDDM SYSの再compile・再署名/CAT再作成は行わない。

## 検証の範囲

`make test-mt8167-brightness-native`はportable helperの10ケースを確認：
30→70→元値、snapshot失敗/範囲外元値/log checkpoint失敗で無変更、
Set30/70失敗後の復元、readback不一致の有界停止、read失敗、復元Set失敗、
書込み後log失敗時の復元。ASan/UBSanでも確認する。

ホストでMinGW-w64 11.0.1の実COM宣言を使いC sourceをx64/ARMNT COFFへcompileして
callback/vtable signatureと型を確認した。ARM確認には同headerのGCC向けvarargs分岐を
clangで選ぶcompatibility definesを使用した。これは**WDK8.1 build/linkではない**。
固定WDK compile/link、署名、実COM/WMI/ARM32通常OSでの起動は主担当で未実施。
runner/実機/REAは操作していない。source helperはraw MMIOやSYSを模擬する試験ではない。

## 一次資料

- [Monitor.sysと内蔵panelのWMI/KMD経路](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/supporting-brightness-controls-on-integrated-display-panels)
- [WmiMonitorBrightness](https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmimonitorbrightness)
- [WmiSetBrightness](https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmisetbrightness-method-in-class-wmimonitorbrightnessmethods)
- [MOFとVARIANT numeric型](https://learn.microsoft.com/en-us/windows/win32/wmisdk/numbers)
- [IWbemLocator::ConnectServer](https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemlocator-connectserver)
- [IWbemServices::ExecMethod](https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemservices-execmethod)
- [IWbemCallResult::GetResultObject](https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemcallresult-getresultobject)
