# 通常OSの一回限りHardware Probeサービス

`MT8167HardwareProbe` は標準SCMのLocalSystemサービスとして動くARM32 EXE。
物理入力や対話desktopを必要とせず、署名済みの固定clientをCreateProcessWで各1回起動する。
`b508de8` のMT8167DeviceInstallのSCM登録・自サービス無効化・WDK署名build構造を利用する。
PnP installerではなく、driver導入、UEFI変更、サービス再起動、書込み、復旧を行わない。

## 固定配置と実引数

配置先はGetSystemDirectoryWが返す通常OSのSystem32配下だけ。

```text
System32\MT8167HardwareProbe\
  MT8167HardwareProbe.exe
  mt6630-wlan-probe.exe
  mt8167-audio-first-playback.exe
```

WLAN probeは本branchのtools/winpe/mt6630-wlan-probe.c、音声clientはHerschelの
`ae1c716` と統合済みv11のREADME/コードを確認した。**どちらも実引数なし**。
音声clientはGetModuleFileNameWから自分のディレクトリを求めてログを作る。
WLAN probeの作業ディレクトリも上の固定配置先に設定する。
サービス開始引数、registry値、環境変数からclientのパスや引数を受け取らず、
cmd.exe、PATH探索、汎用のコマンド実行、権限昇格brokerは提供しない。

CreateProcessWの直前に、固定ファイルを読取り権限・FILE_SHARE_READだけで開き、
reparse point/ディレクトリを拒否し、WinVerifyTrustで署名とローカル信頼chainを確認する。
署名なし・信頼不成立・検証API不在は起動しない。検証中からchild終了まで同じfile handleを保持し、
書換え/削除/名前の差替えを禁止する。親ディレクトリもreparseを拒否してrenameを禁止する。
署名検証はUIなし・cache-onlyで、WLAN未接続時のオンライン照会に依存しない。
署名のビルド時成功と、LocalSystemが対象OSで信頼できることは別である。
主担当は配置先をSYSTEM/Administratorsだけ書込み可能にし、開発用の公開証明書chainを
対象OSのmachine trustへ用意する。署名用private keyを対象へ配布する必要はない。

## 一回限りの動作

1. SCM serviceとして開始し、実行tokenがLocalSystemか検査。
2. 自サービスをSERVICE_DISABLEDへ変更。失敗したらclientを起動しない。
3. 固定配置先にattempted.binをCREATE_NEWで作り、書込みとFlushFileBuffersを確認。
   既存marker、作成失敗、保存失敗でもclientを起動しない。markerを削除しない。
4. WLAN、audioの順に署名を確認し、各clientを最大1回起動する。
   WLANは600秒、audioは120秒が上限。両者とも起動引数なし。
5. stdout/stderrを別の固定ファイルへ直接継承させる。
   STARTUPINFOEXのhandle allowlistでstdin=NULと当該2ファイルだけを渡す。
6. processはsuspendedで作成し、専用Jobへ割当て後にresumeする。
   timeout/SCM STOP時はそのchild Jobだけを終了し、5秒以内のprocess終了を確認する。
   終了確認できなければ次のclientも起動しない。再起動/再試行は行わない。
7. processの終了code、保存エラー、UART通知状態を記録し、SCMへSTOPPEDを返す。

登録はdelayed-auto-startで、SCM recovery actionは空。WlanSvc依存でSCM開始自体を
阻止しないため、WLANコンポーネント不在でもサービスが試行を消費して無効化できる。
PnPとTOPRGU、WlanSvc/Native WLAN、Audio v11/KS/ksuser.dllの準備は主担当が行う。
準備不足によるclient失敗も同じbootで再実行しない。--startも無効化を解除しない。

timeoutでprocessを終了したことは、WLAN scan完了やaudioのSTOP/mute/DMA drainの証拠ではない。
既存音声driverは資源を保持する設計なので、audio timeout後のhardware状態は未確認として扱う。
SCM STOPは進行中のprocessを終了するが、PnP resetやhardware復旧は行わない。

## 出力

同じ固定ディレクトリにservice.log、attempted.bin、wlan.stdout.txt、wlan.stderr.txt、
audio.stdout.txt、audio.stderr.txtを保存する。child出力はCREATE_NEWで、既存結果を上書きしない。
署名拒否などでprocessを作成しなかった場合、child出力が未作成の場合がある。
client自身のmt6630-wlan-probe.txt/audio-first-playback-*.logも同じ場所に残る。

service.logと既存TOPRGU UART queueへの短い通知は次の形式。全値は32-bit hexadecimal。

```text
A04HW RUN=12345678 ID=00000001 EVT=00000005 CODE=00000000
```

IDは0=サービス、1=WLAN、2=audio。serviceはclientのstdout/stderr本文をUARTへ中継しない。
WLAN client自身のUARTはcount/statusだけ。サービス通知にもSSID/BSSID/鍵/PSKを含めない。

| EVT | 意味 | CODE |
| --- | --- | --- |
| 1 | service処理開始 | 0 |
| 2 | 自サービス無効化 | Win32 error |
| 3 | attempted marker作成/flush | Win32 error |
| 4 | 固定EXE署名確認 | 0、Win32 errorまたはWinVerifyTrust status |
| 5 | process作成・resume | 0（probe動作成功とは別） |
| 6 | process終了 | 実process exit code |
| 7 | timeout | ERROR_TIMEOUT |
| 8 | SCM STOP等による中断 | ERROR_CANCELLED等 |
| 9 | 起動前失敗 | Win32 error等 |
| 10 | service終了 | 最初の非0結果、または保存/UARTエラー |
| 11 | 終了要求/確認 | API error、続いてprocess待機結果 |
| 12 | childログ保存エラー | Win32 error |
| 13 | service UART状態 | それまでの通知エラー（0も到着証明ではない） |
| 14 | 最終SCM結果（service.logのみ） | END自身のUART失敗も含む最終結果 |

UARTは既存96-byteキューだけを利用する。通常は各通知後にhandleを閉じ、clientと共有する。
pending IOCTLは最大1秒、cancel後250msで打切り、未完了storageをprocess終了まで保持して
後続UART送信を止める。UART MMIO、COM port、新driverを追加しない。
IOCTL受付とホスト受信を区別し、実UARTの開始/終了レコードは主担当が確認する。
service終了0だけで実scan、WPA2接続、可聴成功を宣言しない。

## 主担当のビルド・署名・配置

WDK8.1 UM/shared ARM headers、kernel32.lib/advapi32.lib、LLVM clang/lld-link/llvm-readobj、
Windows PowerShellが必要。署名時はWDK8.1 signtoolとb508de8と同じ証明書store構造を使う。
WinVerifyTrustはSystem32のwintrust.dllから動的ロードし、検証不能なら起動拒否する。
build scriptはSCM登録・サービス開始・実機/runner操作を行わない。

```powershell
.\tools\ci\build-mt8167-hardware-probe.ps1 -OutputDirectory .\build\hardware-probe -WlanProbePath .\build\peripherals-integrated\wifi\wlan-probe\mt6630-wlan-probe.exe -AudioClientPath .\build\peripherals-integrated\audio-usb\client\mt8167-audio-first-playback.exe -Sign
```

- 入力clientは両方指定するか、両方省略してserviceだけをcompileする。
- -Signはserviceとコピーした2つのclientを署名する。元clientを変更しない。
- 出力manifestは署名後のhashを記録するが、対象OSのLocalSystemでの署名信頼は未確認とする。
- 統合build-mt8167-peripherals.ps1も未署名serviceと2つのclientをhardware-probe/へまとめる。

主担当が署名済み3 EXEを固定ディレクトリへ配置した後、管理権限の既存実行経路で
以下を呼べば、物理入力なしでSCMによる次回起動が設定される。

```batch
"%SystemRoot%\System32\MT8167HardwareProbe\MT8167HardwareProbe.exe" --install
```

通常OSでPnP準備済みなら同じ既存実行経路から--startで直ちに開始できる。
登録後の既存サービス更新/再有効化/marker削除は提供しない。
追加の物理操作や対話loginを求める実装ではない。

## 確認範囲

公開WDK参照headersでのARM32 compileと実DLL importのlink、script構文、固定allowlistと
自サービス無効化→marker→CreateProcessの経路を開発側で確認する。
模擬SCM/偽署名verifierの成功を実起動の証拠にしない。
WDK8.1の署名build、対象OSの署名信頼、LocalSystem開始、実probeとUART回収は主担当が確認する。
