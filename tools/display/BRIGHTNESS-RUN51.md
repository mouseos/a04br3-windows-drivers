# run51 completion/戻り値契約の修正

source基点ccef15ae、実証拠は作業rootの
port/xnu/out/private-windows-rt-run51-registry-post/Diagnostic-Client2Stdout.txt。
original49成功、ExecMethod S_OK/direct0/call1、GetResultObject S_OK/object0。
その後の8004100Fは旧brightness_wmi_result.cが!resultで合成したNO_RESULTである。
この値をMonitor.sys/DODが返したmethod ReturnValueやHRESULTと扱えない。
30/restore49が実際に適用された証拠もない。最後のread49は維持、光学/PWM成功未確認。

## 一次契約

- https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemservices-execmethod
  RETURN_IMMEDIATELYのcall objectはGetCallStatusで完了を取得し、outputはGetResultObject。
- https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemcallresult-getcallstatus
  API自身のHRESULTとplStatusの元operation HRESULTは独立。timeoutならplStatusを使わない。
- https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmisetbrightness-method-in-class-wmimonitorbrightnessmethods
  Timeout uint32/Brightness uint8、ReturnValue uint32、0成功。
- https://learn.microsoft.com/en-us/windows/win32/wmisdk/numbers
  CIM_UINT32のVARIANT表現はVT_I4、UINT8はVT_UI1。符号付きslotをunsignedへ保存する。

## 修正

semisynchronous completionをGetCallStatus(3000ms)で取得し、transport HRESULTとoperation HRESULTを
別にstdout/rootlogへ記録。両者S_OKの後だけoutput object/typed ReturnValueをdecodeする。
直接outputとcall object両方がある場合もcompletion失敗を無視しない。再dispatchはしない。
S_OK/null outputを成功やReturnValue0へ変換せず、wmi-output-missingとしてclient側契約エラーを明示。
完了待ちの欠落は修正するが、実機の次結果が必ず成功するとは断定しない。

初回snapshotでGetMethodのinput/out signatureを実classから取得し、Timeout CIM_UINT32、
Brightness CIM_UINT8、ReturnValue CIM_UINT32を確認してから変更する。署名のVARIANT値が
未設定VT_NULLなのは正常なので、definitionの値ではなくCIMTYPEを検査する。
実戻り値は従来のCIM_UINT32/VT_I4で厳密decodeする。
wmi-method-signature / wmi-completion-status / wmi-output-object / wmi-return-decode /
wmi-output-missing / wmi-method-outputで、dispatch・実completion・出力・client失敗を区別。
固定30→70→元値、失敗時も元値restore試行、readback一致、CREATE_NEW/Engine logs固定path/
stdout tee16384bytes/旧root非削除を維持。任意command/query/MMIO/DDC/API fallbackなし。

## DODレビュー

8a79/ccefのGetPossibleはAC/DC既定2値＋supported1..100、Get/Setは公式v1 NTSTATUS/percent契約。
Getは実PWMDUTY、Setは同じcontext lock下でPIOwrite/commit/readback、power/stop時は失効する。
CurrentBrightness49の取得はgetter稼働を示すが、setter到達はrun51から証明されない。
現証拠からDOD不具合の根因と断定してcode/INFを変更しない。main予約DOD1.6は維持、
将来DOD修正が必要なら1.7以降。本commitのclientは既存brightness DOD1.5/CPU-present1.6と組合せ可能。
GPU presentflags・provider ownership・固定UEFIには変更なし。

## 検証と残件

mockのpending/direct/both、completion transport timeout、元operation失敗、null output、
型decode失敗、nonzero methodreturn、ref解放を検査。実30→70→restore state-machine試験を保持。
DOD実callback/MMIOモデル試験も実行。WDK8.1 ARMcompile/link/BCBF署名をsourcepinで回収する。
実COMproviderの挙動、DODsetter到達、WMI30/70/元値、物理PWM/輝度はmain実機確認が残る。
