# BatteryClass status通知

基点427e8a66の温度Observe完成版は保持し、この候補だけINF0.0.1.7へ進める。
SetStatusNotifyとDisableStatusNotifyの両方を実装する。BatteryClassから受けたtagと許容PowerState/容量範囲を既存battery.lockの下で保存する。範囲は両端inclusive、範囲外またはPowerState不一致なら既存実pollでBatteryClassStatusNotifyへ接続する。Set callback内で同期通知すると既存class_lockへ再入するので呼ばない。登録直前のpublishとの競合は、次pollが同じsampleでも登録windowを評価することで閉じる。通知latencyは既存30秒poll（BQ ownerでは10秒）の範囲で、即時IRQ通知ではない。

既存sample変更/無効化/電源状態変更の無条件通知を保持する。解除は明示criteriaを取り消すだけでこれらの通知を止めない。D0失効時に登録を消し、新tagへ持ち越さない。不正tag/null/反転した範囲/未知PowerState bitは登録を変更せず拒否する。新hardware thresholdやLC/BQ register writeはない。

対象は実LC sampleと既存producerが公開したPowerStateの通知である。ObserveのPowerState0から未知の実AC/充放電を推測しない。BQ owner受理cacheのONLINE/CHARGING経路は既存実装で、今回は変更しない。独立AC adapter _PSR、discharge電流/方向、critical閾値の実ハード検出を新規実装したものではない。CT3 GPIO35/34は接続検出だが、LinuxはBQ INSTAT/USBSTATも見て無入力を補正する。GPIOだけでusable ACは合成しない。BQ readにはwatchdog開始とfault消費があるため、owner外の新samplerは足さない。

一次契約:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/battery/setting-and-canceling-battery-notification
- https://learn.microsoft.com/en-us/windows-hardware/drivers/battery/interaction-of-battery-status-and-notification-routines
- https://learn.microsoft.com/en-us/windows/win32/api/batclass/nf-batclass-batteryclassstatusnotify
- 実WDK8.1 shared/batclass.h のBATTERY_NOTIFYとcallback。

`tests/lc709203f_status_notify_test.c`はproduction callbacksの条件境界を実行する。lockだけhost stubで、Windows IRP/class実行や実機成功は示さない。

増分buildはtools/ci/build-lc709203f-status-notify.ps1。BaselineDirectoryは固定427e8a66の既存build出力。compiler/source/object hashを照合してgauge/mt8167_i2cの2objectを再利用し、struct headerに依存するbattery_class/wdf_gaugeの2 TUだけcompile/linkする。温度EXEはsource/hashを検査して署名済bytesを再利用する。既存BCBF鍵で新SYS/CATを署名しmembershipを検査する。親/UEFI/charger owner/他driverはbuildしない。
