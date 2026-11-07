# run33標準brightness失敗の修正候補

実ログ：作業root内port/xnu/out/private-windows-rt-deferred-run33-diagnostics/brightness-native.log。
original50取得成功、Set30/restore50はobserved=ffffffff/status=8000ffff、最後のread50成功、
result flags3。これは光学輝度成功でもSet成功でもない。fresh36の新DOD1.4実配置hashは
主担当回収中で、まだ候補SYSが実採用されたと断定しない。

## native clientの結果処理

4fafe9aのExecMethodはppOutParams=NULLを渡し、S_OKでもppCallResultがなければ
client自身がE_UNEXPECTEDを合成していた。run33の記録にはdispatch段階とobject有無が
ないため、providerのE_UNEXPECTEDかこのguardかを判別できない。

[公式ExecMethod契約](https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemservices-execmethod)
に沿ってppOutParams/ppCallResultの両方を受け取り、直接outputがある場合はそれをdecodeし、
なければbounded semisynchronous resultを取得する。既に起きたかもしれないSetを
自動的に再dispatchしない。両方なし・timeout・HRESULT失敗は失敗のままにする。
method ReturnValueとtransport HRESULTの区別も維持する。

新wmi-dispatch行のobservedはbit0=直接output、bit1=pending call。
wmi-arguments/wmi-pending-result/wmi-return-decodeで失敗段階を区別する。
旧wmi-method-return/restore/read/最終flagsも保持する。固定30→70→元値以外の命令/query、
remote、DDC/private MMIOを追加しない。live PWM観測だとは扱わない。

portable brightness_wmi_result.c/.hを実native setへ接続し、direct/pending/both/neither/
provider E_UNEXPECTED/timeout/nonzero method ReturnValueとref解放をhost検証する。
これはCOM providerの実挙動を再現・観測した主張ではない。

## DOD callback契約

[DXGK_BRIGHTNESS_GET_POSSIBLE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgk_brightness_get_possible)
は先頭AC default、次DC default、残りhardware levelsを要求する。
旧版は1..100だけを返し、1/2が既定値、3..100がhardware levelsと解釈される構成だった。
現在は起動時の検証済みfirmware dutyから得たpercentをAC/DC両既定値に置き、
続けて1..100の100level、合計102byteを返す。機種firmwareに独立AC/DC policyはないため
同じhandoff値を使い、ユーザーSet後もこの初期値は変えない。

Set callbackのPVOID/UCHAR/NTSTATUSとPASSIVE_LEVELは
[公式契約](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgk_brightness_set)
と一致。Setのready/power/poison失敗をDbgPrintし、実SetのstatusとCON1 before/afterは
初回3回と失敗時に出す。新UART mappingは追加しない。DbgPrintがUARTへ出る保証は
既存収集経路次第。成功時の毎present出力は増やさない。

DriverVer=11/07/2026,0.0.1.5。UTC実日付11/07を確認し、既存INF14より日付も新しくする。callbackを変えたので**新SYSのbuild/署名/CATが必要**。
旧71f4署名SYSを再利用するINF14 recipeは今回の候補生成に使わない。
mainの最新GPU/DODへ差分を取り込み、既存liveFull/repeat/directPOST等とbrightness=1を
維持する。この古いbaseのGPU全体でmainを置き換えない。

native recipe tools/ci/build-mt8167-brightness-native.ps1は新helper objもlinkする。
main既存の署名identityを使用し、証明書store/時刻は変えない。
main runner/実機/REAは今回操作していない。

host DOD callback/owner試験、native固定手順、実結果helper、ASan/UBSan、互換COMヘッダで
ARMNT compileとrecipe parserを確認する。正式WDK build/署名/実WMI再試験、配置hash一致、
PWM変化と光学輝度は未確認。GetPossible契約違反の修正と、実機Set失敗原因の確定は別。
