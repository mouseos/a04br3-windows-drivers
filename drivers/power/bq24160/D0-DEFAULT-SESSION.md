# BQ14 ColdLinux100D0：実D0 callbackの再入候補

BQ13 a5eaf0dを保全し、INFは11/07/2026,0.0.1.4。default-off
A04_BQ_D0_DEFAULT_DIAGNOSTIC=1をColdLinux100D0 recipeだけで有効にする。
親/thermal/UEFIは変更しない。53d6 SD-only bootと親1010の既存I2C2排他資源を使う。
MOU0008/BAT0を更新し、別Root/BQ/gauge driverを追加しない。

## 一次sourceと意味

CT3_GPL_LGPL_Src/kernel-4.4/drivers/misc/mediatek/power/bq24160.c:891の
bq24160_charge_initが機種の6h/1.3A/100mA終端/4.32Vを設定する。
charging_hw_bq24160.c:268はSTAT3/4だけ充電中、:285はwatchdog pet。
LinuxはUSB判定付き500mAだが、本候補は分類を偽装せず既存USB100mAだけ。
Linuxshutdownはログのみであり、sleep中継続workerの根拠とはしない。
TI BQ24160 SLUSAO0HのRESET/DEFAULT契約に従う既存02h=80hの一回writeを使う。
RESET ACKはbus完了であり、DEFAULT実状態/実充電/安全な電流を独立確認した値ではない。
RESET後のBQ readbackはwatchdogを再始動するので行わない。

## 実PnP lifecycle

初回D0はBQ13と同じ今回bootのBQHR STOPPED state2/reason6/seal/zero kicks/fault/validを厳格検査。
開始前のI2C2 setup成功とraw/translated排他割当、idle/PIO/DMAoffを要求。
10秒timerを先にarmし、同じbus_lockの下で4register profile/readback、BQ snapshot/pet、
LC709203Fのmode/voltage/indicatorをPEC検査。全成功のsnapshotだけBatteryClassへ公開。
LCのアドレス0x0b、BQの0x6bを同じ所有者が直列化する。per-device校正/残量/IDをimageへ埋め込まない。

実D0Exitではonline admissionを閉じ、同期timer cancel、bus_lock内でendpoint閉鎖→期限内の
健全ownerだけDEFAULT exact write。一回write完了をprivate sessionに記録する。
再D0Entryはその記録を一回消費し、旧endpoint closed/owner stopped/controller無faultを検査し、
private epochを更新、cold期限を新設してcontroller/profileを再検査する。boot BQHRは再使用しない。
このprofileでは外部broker queueを登録しない。old client pointer/epochを保持する相手が無いため、
同期timer停止後のprivate storageだけを再初期化する。実PnP callbacksから呼ばれ、callback直接実行はしない。

外部owner_platform_successorは未実装のまま。DEFAULTでWindowsの給餌を終了し、
再D0時の新Windows local ownerへ接続する。D3中にLinux設定や保守workerが継続するとは言わない。
電源/clock再設定やPWRAP共有親の権限は足さず、再D0時の既存controllerが不適合ならCode10/未公開。
DEFAULT失敗、期限切れ、I2C fault、profile失敗では再入資格を作らず追加read/resetを強行しない。
初回read失敗もwatchdogが開始した可能性を残す。I2CはPIOのみでDMA backingは作らない。

UART: D0 ENTRY / COLD OWNER、POWER epoch/stat/source/mv/pct gauge=PEC_VALID rate=UNKNOWN（各世代初回3件）、
DEFAULT WRITE OKまたはUNCONFIRMED、再入D0 NEW LOCAL EPOCH。
BatteryClass POWER_ON_LINE/CHARGINGは受理済みBQ STAT3/4/5だけから、LC残量と同一ロックで公開。
DEFAULT/失敗時に失効する。STAT5は完了状態だが100%へ書き換えない。実電流/独立AC adapter/_PSRは未実装。

## 旧BQ13の単独追加

BQ13署名物は変更しない。53d6/親1010と資源/ACPIは整合し、明示ColdLinux100診断でだけ単独追加可能。
今回boot I2C2 setup=SuccessとBQHR strict cold条件が前提で、新thermal/GPU親や新bootは不要。
D0再入不可、通常常用充電ではない。現trial51へ自動混入しない。
BQ14も実温度/充電電流/入力能力/standby/resumeはmain実機未確認。
reload/別device生成を跨ぐglobal successorや再認証を保証しない。診断候補はfresh bootからの同一device lifetime限定。

build: tools/ci/build-bq-only-candidate.ps1 -CertificateThumbprint <existing> -ChargerProfile ColdLinux100D0
host: make test-bq-d0（実coreを使いcold→pet+PEC→DEFAULT→再世代、reset失敗/late/重複/overflow/PEC不正を検査）。
