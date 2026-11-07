# run42の診断内失敗と1006候補

## 確定した範囲

回収されたrun42/core44のschema1はdriver_build1004、DriverEntry generation1、
Initialize generation2。selected INF1005は意図した別の包装版である。
DriverEntry登録、registration attributes、RX pool/64slots、configuration open、
親照会、resource選択、Acquire、MSDC map、IRQ connect、activateは成功。
SESSION_DIAGNOSTICはBACKEND_INT -1、Detail3 (A04_DIAG_FAILED)。
SESSION_STARTは-1/Detail772 (session quarantined4、diagnostic failed3)。
InitializeはNDIS_STATUS_ADAPTER_NOT_READY (0xc0010011)をreturn-preparedとして記録し、
CMはCode10/C0000001、WLAN Open0/Enum0/interfaces0/exit3。
NDISへのreturn受理・末尾flushの永続成功はcheckpointから証明しない。
general/Native attributes、TC1、station init、PS profileには未到達。

## 根本原因をまだ特定できない理由

既存windows_diagnostic_startは次の全失敗をstate3/戻り値-1へ集約していた。
firmware file read/hash、board init、control open、WMT/WLAN firmware起動、calibration。
この記録だけからSDIO、firmware不在、hash不一致、calibration失敗のいずれかを
選ぶことはできない。媒体上のWMT/WLAN bytesのhashはsource期待値と一致するが、
実Windows起動時の固定path読込み成功を証明しない。
旧Code31/C000014Dは今回の失敗原因に流用しない。

## 1006で修正した観測の欠落

起動/cleanup/隔離/返すstatus/firmware commandは変えず、既存PASSIVE traceへ
以下のRESULTのみを追加した。旧APIはNULL callback wrapperとして保持する。

|Stage|Kind/Raw|Detail|
|---|---|---|
|FW_READ_WMT/WLAN|read_snapshotの実NTSTATUS|0|
|FW_HASH_WMT/WLAN|OBSERVATION、1=一致/0=不成立|WMTは1ならhash関数失敗、0ならdigest比較|
|DIAGNOSTIC_BOARD_INIT|実backend戻り値|0|
|DIAGNOSTIC_CONTROL_OPEN|実backend戻り値|bits0..7 life state、bit8 board started、bit9 ready、bit10 poisoned|
|FW_SELECT_WMT/WLAN|実selector戻り値|0|
|FW_BOOT_WMT|実bridge戻り値|既存wmt_state.phase|
|FW_BOOT_WLAN|実bridge戻り値|既存wlan_state.section|
|DIAGNOSTIC_CALIBRATION|実backend戻り値|0|

hashやphaseだけでfirmware ACK/RF準備完了を主張しない。未到達stageは未観測。
selector前には従来のsnapshot再hashもあり、途中不一致やprecondition拒否なら
selectorは未観測のまま。これもFW boot失敗とは区別する。
callback内の記録失敗で起動結果を上書きしない。レジストリ操作は従来の
PASSIVE checkpoint経由だけ。追加flush無し、schema1/1624bytes/64events/10flush上限を維持。
追加は最大11eventsで、通常Initializeの既存経路と合わせ64未満。
個体値、path文字列、firmware bytes、鍵、SSIDは保存しない。

これは実機Code10の動作修正ではなく、観測された失敗区間の詳細消失を直す候補。
次の統合試験で上記の実失敗stage/statusを回収し、そこに沿った動作修正が必要。
同じ1005 SYSを根拠なく再試験する提案ではない。

## 出自とbuild recipe

固定1005 branch/署名済み3ca SYSは保持。本branchだけを新SYS/INF1006とし、
checkpoint driver_buildも1006へ変更する。1005のrepackage helperは使用できない。
新SYSが必要。既存親1006はhash/capability/interface blob照合を通す入力として再利用し、
親再build不要。旧sourceのobjを誤って再利用しない。

```powershell
& .\tools\ci\build-mt6630-runtime.ps1 `
 -OutputDirectory "$PWD\build\native1006" `
 -SharedParentDirectory "$PWD\build\existing-parent"
& .\tools\ci\package-mt6630-runtime-1006.ps1 `
 -BuildDirectory "$PWD\build\native1006" `
 -OutputDirectory "$PWD\build\native1006-signed" `
 -CertificateThumbprint BCBF6C456C6F0DD0818434763B189488FBF13570
```

今回実施したのは変更5 TUのARM32/scalar参照compile、VFP/NEON不在検査、
既存firmware/diagnostic/calibration/session fixtureとdecoder互換確認。
WDK8.1最終link/署名と実機検証は未実施。端末/runner/REAには触れていない。
