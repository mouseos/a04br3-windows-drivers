# 通常Windows用 UART_SCREEN 候補

`tools/ci/build-toprgu-normal-uart.ps1` は既存WDK 8.1 helperを呼び、
ARM32/KMDF 1.13のTOPRGU候補を新しい出力先へ生成する。
実機・runner操作、配備、hive編集、証明書生成は行わない。

## 固定設定と出自

|項目|値|
|---|---|
|A04_TOPRGU_CPU1_HANDOFF|1|
|A04_TOPRGU_UART_SCREEN|1|
|A04_TOPRGU_THERMAL_STOP|0|
|A04_TOPRGU_DEADLINE_SECONDS|0|
|CPU条件|正確に4 CPU、CPU1上のbackup DPCとMPIDR affinity=1を確認して所有取得|
|package DriverVer|11/06/2026,0.0.11.0（生成INFのみ更新）|

旧normal設定の比較対象は `d6451d58ca82a4cbc888feaf1b716ce8f3398bc9`。
当時の4CPU manifestはUART=0、handoff=1、thermal=0、deadline=0。
そのmanifestは未署名buildの記録であり、現在実機に入っているSYSの出自を
証明するものではない。実適用前のinstalled SYS hash照合は主担当で行う。
今回の既存機能の変更はUART=1だけ。INFのversionと署名も変わる。

WinPE UART実証の比較対象は `7109a9009b0a755532d2aa4058d8e3c2620cd61a`。
当時のmanifestも同じ4フラグで、署名済SYS SHA256は
`9ca96c1f54964a132578a00e2362795c789ac627d4e925df69aa8bc17b28e7fa`。
WinPEでの動作実証は通常WindowsでのPnP、署名受理、電源遷移を保証しない。

レシピはruntime 7ファイルのgit blobが旧normalと同じことを強制し、dirty checkoutを拒否する。
manifestにはソースcommit、blob、実際の入力bytesのSHA256、全4フラグ、
compile/link設定、tool version、生成SYS/INF/CATのhashと長さ、署名情報を保存する。
改行変換されたcheckoutのSHA256とcanonical git blobは別々に記録する。

## WDK 8.1 Windowsホストでの実行例

既存LLVMのclang/lld-link/llvm-readobjとWDK8.1 ARM/KMDF1.13を使用する。
固定commitのclean checkoutで実行し、OutputDirectoryには作業ルート内の未作成dirを指定する。
既存runnerの起動・終了はこのレシピの範囲外。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\build-toprgu-normal-uart.ps1 `
  -OutputDirectory C:\TAB-A04-BR3\out\toprgu-normal-uart-01 -Sign
```

`-Sign`なしでは未署名候補。既定はCurrentUser/Myの既存開発鍵
`2655FC89A7082D3BAB3F26EC556EE8A3C52F4358`を使用する。
必要なら `-CertificateStore LocalMachine`。秘密鍵は生成・exportしない。
RTC 2014と現在を包含する証明書を要求し、SYS署名→CAT生成→CAT署名の順で処理する。
公開CERはpackage外へ出力する。署名済でも実OSのtrust/BCDによる受理は別途確認が必要。
失敗した出力dirは候補として使わず、新しいdirで再実行する。

## reset・PM・sleep・watchdogの維持条件

- `A04BR3!RST` parserとreset経路は既存ソースを維持する。
- 正確に4 CPUかつCPU1所有確認後だけ給餌し、CPU数減少時は所有を失効する。
- deadline=0は給餌の実験的期限をなくす設定。ハードウェアwatchdogの無効化ではない。
- 既存D0Exitのsleep/hibernate/shutdown/shutdown-off時のwatchdog保存・無効化と、
  CPU1再所有時の再武装を維持する。UART screen timerもD0Exitで停止する既存featureを使用。
- ACPI MOU0005、Mt8167Toprgu、Start=0、System Bus Extender、KMDF1.13を変更しない。
- UART送信は既存busyロックでwatchdog/RX処理と直列化される。
  `\\.\A04BR3ScreenUart` は既存IOCTL `A04_UART_SCREEN_SEND`で扱う。
  enqueue成功は物理送信完了を意味しない。短いASCII通知向けで、bufferは96 bytes。
- THERMAL_STOP=0。thermal HALT IOCTLを要求する構成とは組み合わせない。
  UART_SCREENとTHERMAL_STOPの同時有効は既存compile guardも拒否する。

以上はソースと設定の維持確認であり、今回の候補でsleep/resumeを実測した結果ではない。
EnginePcmとの同時実行も未検証。次回bulkへの候補は `package/` と `manifest.json`。
自動適用はせず、主担当がEnginePcmと併せて署名・PnP起動・UART名・通知・電源遷移を確認する。
現在進行中のrun26r2 servicingには投入しない。
