# ARM32 Native WLAN scan probe

通常Windows RT 8.1のWlanSvcとwlanapi.dllを使用する。WinPEでこれらが不在または
サービス停止中ならNORMAL_OS_REQUIRED=1と数値エラーを返す。サービスを起動せず、
通常OS向けであることを明示する。profile/接続/鍵入力APIは使わない。

WDK8.1 UM/shared headers、UM ARM kernel32.lib/advapi32.lib、clang/lld-linkが必要。
checkoutルートから以下を実行する（runtime一括buildからも呼ばれる）。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\build-mt6630-wlan-probe.ps1
```

出力はbuild/mt6630-runtime/wlan-probe/配下のEXE、mt6630-wlan-observe.cmd、本文、
probe-manifest.json。単一parent統合buildでは出力ルートのwifi/wlan-probe/へ同じ一式を生成する。
Wi-Fi runtime manifestはprobe manifestのSHA256も記録する。Ramanの最新統合build
`tools/ci/build-mt8167-peripherals.ps1` はこれらのファイルを全体manifestにも列挙する。
5d2cb2a（Audio v11/KS client/単一parent構成）を基点とし、そのABIとbuild方法は変更しない。
署名・配置・実行は主担当が行う。

通常OSで主担当が配置後、EXEと同じディレクトリのcmdを起動する。

```batch
call X:\a04br3\wifi\wlan-probe\mt6630-wlan-observe.cmd
```

cmdは自分のディレクトリを作業場所としてprobeを1回起動し、数値の起動結果/終了値を返す。
引数はprobeへ渡さない。サービス開始、profile生成、ネットワーク設定変更、driver導入は行わない。
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
| SCAN_EXIT | WLAN処理だけの終了値。0は完了後BSS API成功（0件も含む） |
| UART_OPEN_ERROR / UART_ERROR | 既存UART ownerのopen/送信エラー。0は到着証明ではない |
| BEGIN / END | 同じRUNの記録範囲。END値はSCAN_EXITと同じ |
| SAVE_ERROR | 結果ファイルの保存エラー |
| FINAL_SAVE_ERROR / FINAL_UART_ERROR / EXIT | close後stdoutのみの最終結果。EXIT=6は保存、7はUARTエラー（先行WLAN失敗は優先） |

BSS総件数にはキャッシュが含まれる。host timestampはdriver側でsystem timeから取得する。
新規/更新件数も電波で受信した全BSSの保証ではない。WLAN通知には要求IDが無いため、
同じinterfaceの外部scanと一意に対応付く保証はない。併走scanを止めた主担当の実測として扱う。
request受付、完了event、実BSS数、WPA2接続成功はそれぞれ別の観測である。

## 既存UART ownerとaudit-stream

probeは既存TOPRGUの `\\.\A04BR3ScreenUart` / A04_UART_SCREEN_SENDを使う。
UART MMIOやCOM portを開かず、driver/serviceを追加・置換しない。1レコードは既存96-byte上限内。
queue busyは最大2秒だけ待ち、他の送信者を停止しない。UART不在/共有競合/タイムアウトは
数値エラーとしてstdoutとファイルへ残し、WLAN処理自体は続ける。

各レコードは `A04WLAN RUN=<起動時tick> SEQ=<連番> IF=<番号> <固定ラベル>=<整数>`。
直接UARTでは同じRUNのBEGINからENDまで連続SEQをホストが受信したことを確認する。
GetTickCount由来のRUNは永続的な一意IDではなく、既存のUART回収offsetと組み合わせる。
IOCTL成功と最後の2秒保持はqueue受付のみ。END未受信なら転送未完了。
BEGIN/END受信と実scan成功も別であり、REQUEST_ACCEPTED、COMPLETE_EVENT、BSS_ERROR、
BSS_TOTAL/FRESH_KNOWN/新規更新件数をそれぞれ確認する。

主担当のCRC付きaudit-streamで収録する場合は、既存のRAM reportへstdoutを追加する。
既存senderが読む固定report pathはX:\a04br3\toprgu-audit-result.txt。

```batch
call X:\a04br3\wifi\wlan-probe\mt6630-wlan-observe.cmd >> X:\a04br3\toprgu-audit-result.txt
```

probe終了後に、主担当の既存uart-audit-stream.exeをその手順の16桁token付きで起動する。
転送元reportの生成・初期化とhost offset/token管理は既存audit担当が行い、probeは既存reportを消さない。
直接UARTのA04WLAN行とCRC付きA04AU2* framingは異なる。audit-stream decoderは従来どおり
frameを組立て、復元したstdoutからA04WLANのcount/statusを読む。転送完了はscan成功を意味しない。

ARM32 compile/link、数値レコード長、PowerShell構文は開発側で確認する。
通常OSでのcmd起動、WlanSvc/NDISによる実完了通知、UARTの実受信は主担当の確認が残る。

物理入力なしで通常OSから1回起動するサービスはREADME-mt8167-hardware-probe.mdを参照。
LocalSystemのMT8167HardwareProbeが、署名済みの固定WLAN/audio clientだけを起動する。
