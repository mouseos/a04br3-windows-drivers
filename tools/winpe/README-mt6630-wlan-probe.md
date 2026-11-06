# ARM32 Native WLAN scan probe

通常Windows RT 8.1のWlanSvcとwlanapi.dllを使用する。WinPEでこれらが不在または
サービス停止中ならNORMAL_OS_REQUIRED=1と数値エラーを返す。サービスを起動せず、
通常OS向けであることを明示する。profile/接続/鍵入力APIは使わない。

WDK8.1 UM/shared headers、UM ARM kernel32.lib/advapi32.lib、clang/lld-linkが必要。
checkoutルートから以下を実行する（runtime一括buildからも呼ばれる）。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\build-mt6630-wlan-probe.ps1
```

出力はbuild/mt6630-runtime/wlan-probe/mt6630-wlan-probe.exe。署名・実行は主担当が行う。
既存audit-streamの作業ディレクトリから実行しstdoutを回収する。
同じディレクトリへmt6630-wlan-probe.txtを上書き保存する。
数値と固定ラベルだけを出力し、SSID/BSSID/GUID/description/鍵/PSKは出力しない。
BSS比較に必要な識別情報はメモリ内だけに置き、API bufferを消去して返す。

処理はWlanEnumInterfaces → baseline BSS → 通知登録 → WlanScan → 最大30秒待機 →
完了通知後のWlanGetNetworkBssList。各interfaceは番号だけで区別する。

| フィールド | 意味 |
| --- | --- |
| REQUEST_ACCEPTED | WlanScanの受付。scan成功を意味しない |
| COMPLETE_EVENT / FAIL_EVENT / FAIL_REASON | ACMの実完了/失敗通知と理由 |
| WAIT_STATUS | 待機結果。タイムアウトは完了扱いしない |
| BSS_TOTAL | 完了後にAPIが返したBSS総件数。0件もそのまま記録 |
| FRESH_KNOWN | baseline比較が可能なら1。不可能なら新規件数を推測しない |
| BSS_NEW_IDENTITIES | baselineに無かったBSSID/PHY/frequencyの件数 |
| BSS_NEW_OR_UPDATED | 新規またはullHostTimestampが進んだ既存BSSの件数 |
| UNREGISTER_ERROR | 通知解除の結果 |
| SAVE_ERROR / EXIT | 保存エラーと終了値。最後の2行はclose後stdoutのみ |

BSS総件数にはキャッシュが含まれる。host timestampはdriver側でsystem timeから取得する。
新規/更新件数も電波で受信した全BSSの保証ではない。WLAN通知には要求IDが無いため、
同じinterfaceの外部scanと一意に対応付く保証はない。併走scanを止めた主担当の実測として扱う。
request受付、完了event、実BSS数、WPA2接続成功はそれぞれ別の観測である。
