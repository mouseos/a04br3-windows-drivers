# MediaTek MT6392 RTC

MT6392内蔵RTCの日時を読むKMDF 1.13 / Windows RT 8.1 ARMドライバ。
実機試験前。OS時刻の自動補正、時計設定、アラーム、復帰、発振精度の認証は実装していない。

## 接続と一次ソース

TAB-A04-BR3のstock `a04br3.dts` は `mt6392.dtsi` をincludeする。
実機から既存取得済みのLinux DTでは `pwrap@1000f000` の下に
`mediatek,mt6392` と `mediatek,mt6392-rtc` がある。

一次根拠はBenesse配布GPLソース `CT3_GPL_LGPL_Src/kernel-4.4/`。
下記はそのソースツリー内のパス。

- `arch/arm64/boot/dts/mediatek/a04br3.dts`: MT6392機種設定。
- `arch/arm64/boot/dts/mediatek/mt8167.dtsi`: PWRAP `0x1000f000`。
- `drivers/mfd/mt6397-core.c`: MT6392 RTC base `0x8000` とRTC resource。
- `drivers/rtc/rtc-mt6397.c`: 秒 `+0x0a`、分・時・日・曜日・月・年の16bit stride、
  1968年基準の7bit年、月1始まり、秒再読による桁上がり検出。
- XNUの `package/A04BR3AutoReset.cpp` は同じPWRAP/RTCのPDN1を使う。
  本実装はreset/PDN1/CON/IRQ_STA/アラームへアクセスしない。

## 実装と利用条件

`MT6392_RTC_CALENDAR=1` を親の既存フラグに追加したときだけ、
`MT6392\RTC` の子と専用QUERY_INTERFACEが生える。既定OFF。
実搭載親1026のsource `2d6231727e8ed72b6d7dca4148debc101ac5aeef` を基点とし、
既存32flagsとcaptureを保持する。既存32kHz lease/borrow、GPU、USB、AFEの処理は変更しない。
`mt8167-shared-rtc.inf` は候補親 `2026.11.8.1100`。自動配置は行わない。

子はMMIO/IRQを要求せず、既存親のPWRAPロック・D0 admission・poison判定を使う。
読出し全体を一つのロックで保護し、0x800A〜0x8016を取得後に秒を再読する。
秒が異なる場合は再試行し、最大3回で `STATUS_RETRY`。transport失敗は
`STATUS_DEVICE_NOT_READY`、暦として不正な値は `STATUS_DATA_ERROR`。
失敗時はIOCTL出力長0で、部分的な結果やホスト現在日時を返さない。
年1968〜2095のGregorian日付を検証し、曜日は日付から算出する。
raw曜日はそのまま保存し、有効性判定には使わない（stockも曜日を再計算）。

`mt6392-rtc.inf` のドライバ名は **MediaTek MT6392 Real-Time Clock**。
管理者/SYSTEMのみ `\\.\MT6392Rtc` を開ける。
`IOCTL_MT6392_RTC_READ` は入力なし、固定長version 1のraw+暦構造体を返す。
SYSTEM/管理者で `mt6392-rtc-probe.exe` を1回実行すると結果を標準出力へ記録する。
これは将来mainが実機試験するときの手順であり、今回実行した事実ではない。
RTCレジスタを読めても日時が正しい、発振が正常、UTCであるとは断定できない。

## ビルド

既存WDK 8.1 ARM、clang/lld、既存署名鍵を使用する。
clean commitのcheckoutで、`tools/ci/build-mt6392-rtc.ps1` に
`-OutputDirectory <checkout内の新しい専用パス> -CertificateThumbprint <既存鍵>` を渡す。
親、RTC、読出しクライアントだけを生成し、Audio/USB/GPUのSYSを再生成・更新しない。
SDK、Mesa、native GPUライブラリは再構築しない。signed-manifestはsource、flags、
全入力/出力hash、コンパイラ、`hardware_verified=false` を記録する。

ホスト検証は `tests/mt6392_calendar_test.c`（実WACS/ownerコードへfake MMIOを接続）と
`tests/mt6392_calendar_exhaustive.py`（Python標準datetimeと1968〜2095全日付を照合）。
MMIO試験はコマンドのread bitと全アドレスも検証し、PMIC write要求を許さない。

## ライセンス

追加C/HはBSD-2-Clause-Patent。stock GPLソースからレジスタ・接続・暦仕様を確認し、
独立した固定長API、境界検証、bounded snapshotとして実装した。Linuxソースの転載はない。
