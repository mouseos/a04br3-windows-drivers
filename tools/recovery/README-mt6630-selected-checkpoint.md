# 保存SYSTEMからWi-Fi選択と1029接続stageを一度に読む

既205 readerを同じclone内の固定Wi-Fi Enum→選択Net class読取りだけ拡張した。新helper/service/profileはない。新CaptureのSYSTEM/LOG1/LOG2とmanifestを既解析環境へ回収後、一つの新規PowerShellプロセスで実行する。現在の端末/VMへ操作するための入口ではない。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$src\tools\recovery\read-mt6630-failure-checkpoint.ps1" `
 -WorkspaceRoot $work -Manifest "$capture\manifest.json" -ExpectedManifestSha256 $captureSha `
 -SourceDirectory "$capture\files\Windows\System32\config" -OutputDirectory $newOutput `
 -EvidenceRole post-update-session-snapshot
```

回収済みreceiptと4個の `.bin`だけに対して、既decoderで集約する。

```bash
python3 -B "$src/tools/winpe/decode-mt6630-checkpoint.py" --receipt-directory "$newOutput" > "$summary"
```

`selection.fields.InfPath/InfSection/DriverVersion/MatchingDeviceId`は実選択class情報。`records.*.driver_build/generation/parent_generation`は保存checkpoint。`initialize_parent_bound`はDriverEntry→Initializeのgeneration関係、`connection_parent_bound`は1029 RuntimeFailure→同Initializeの関係。`connect_request_context_observed`はcontext2を実際に保存した場合だけtrue。`last_observed_connection_event`は結合済みの最後のstage/raw/detailであり、原因やIP通信成功の判定ではない。

選択未取得は`selection_1029=null`。古いRuntimeFailure・build不一致・親generation不一致は現在connection stageとして採用しない。ring overflowは`history_truncated`を保持する。これは複数接続試行の個別同定やwallclock同bootの証明ではない。選択INFと保存記録はloaded SYS byteの証明とも区別する。

原本とmanifestのhash不変、clone unload/privilege restore成功、raw4値のhashを確認する。Select.Currentが1以外は拒否し、他ControlSetへ推測fallbackしない。WLAN profile/SSID/鍵/ユーザーcredentialsは読まない。

ホスト確認：`python3 -B tests/mt6630_saved_checkpoint_summary_test.py`。新PS読取り自体のnative実行は、後日の保存物解析時に既手順で行う。
