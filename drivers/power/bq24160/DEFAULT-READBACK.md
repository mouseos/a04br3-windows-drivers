# BQ15 masked DEFAULT readbackとchip自律D3契約

既定OFF A04_BQ_DEFAULT_READBACK_DIAGNOSTIC=1、ColdLinux100D0Readback専用。
BQ14固定source/署名物は保全し、INF11/07/2026,0.0.1.5へ昇版。
親1010/53d6の既存I2C2 MOU0008単一ownerを使い、ACPI/UEFI/commonEDK/固定bootは変更しない。

一次資料: TI SLUSAO0H Rev.H (July2022)、https://www.ti.com/lit/ds/symlink/bq24161.pdf
8.3.8 DEFAULT、8.3.9 watchdog/safety、8.3.10 D+/D-、8.6.1--8.6.8 registers。
Linux CT3 kernel4.4 bq24160.c:891の機種profile、charging_hw_bq24160.c:268/285のSTAT/pet。
Linuxshutdownはlogだけなので、独立software successorの根拠にはしない。

## RESET ACKとreadbackを分ける

実D0Exitのquiesce/timer同期停止/単一bus_lockの後、健全な期限内ownerだけ02h=80hを一回write。
その後00--07hを各一回、全体1秒未満の期限でread。ACK/valid/stage/reason/mismatch/rawを個別保存。
全必須設定のreadback一致だけをsessionのDEFAULT結果として受理し、ACK単独では再入資格を作らない。
読出しはwatchdogを再始動する。最後に未観測のRESETを重ねて「停止確認」とは呼ばない。

reg | mask | expected | 理由
--- | --- | --- | ---
00 | 08 | 00 | SUPPLY_SELのみ設定、TMR_RSTはauto-clear/always0、STAT/FAULTは別にraw保存
01 | 09 | 00 | OTG_LOCK/EN_NOBATOP、input/battery動的statusは別評価
02 | 7f | 0c | RESETはread-always1のため除外、IUSB=100mA/TE/CE/HZは厳格照合
03 | fe | 14 | 3.6V/IN1.5A既定。self-clear D+/D-_ENは除外しraw保存
04 | f8 | 40 | familyのみ。revisionを端末固定しない。BQ24160Aとの差はこのIDだけでは証明しない
05 | ff | 32 | 既定1000mA/150mA終端（実電流ではない）
06 | 3f | 00 | live DPM/MINSYS readonlyを除外、DPM設定だけ検査
07 | 69 | 08 | TMR00/TS_EN1/LOW_CHG0。readonly TS_FAULTとreservedbit4を除外

07hのreset表は10011xxxだが2XTMR_ENの文章はdefault0で相違する。bit7を推測で固定せずraw保存。
これは8bit全一致の主張ではなく、上表の既知設定部分の照合。2x状態により安全timerが延長されるため
「27分ちょうど」や無期限充電を主張しない。2xrawもmainへ渡す。

fault bits/BATOVP/battery absent/TS faultやreserved STAT6/7は設定一致と別に拒否。
安全timer満了のCE1も不一致で拒否し、自動再enable/resetによる保護解除はしない。
00h fault readは消費的なので最初のrawを保持し、二度読みで消さない。

## D3中の保守主体はchip自身の制御・保護回路

仕様が定める経路はRESET→DEFAULT、その後のreadでwatchdog再arm→（該当siliconなら）
watchdog満了で既定値復帰/安全timer再開。CPUのpetが無くてもchipが充電・power path・TS/thermal・
安全timerを制御する。外部software successorが無いこと自体を不可能条件にはしない。
本候補はD0Exit後に追加BQ transaction/workerを出さず、このchip契約へ渡す。
readback一致はread時の設定観測であって、D3中のWD満了時刻/実充電を観測した証拠ではない。
UART CHIP_AUTONOMY_PENDINGはその未観測を示す。WDの無いA variantをIDだけで推定しない。

安全timer満了後はchipが充電停止するため、無期限充電/6時間Linuxprofile維持の代替ではない。
DEFAULTでは再開条件がinput POR/電池着脱/CDであり、本候補はGPIO toggle等を勝手に追加しない。
短時間のPowerprofile試験でTS/実充電/長時間timer停止・入力切替はmainが確認する。

## USB budget

ソフトはUSB100mA以外へ増やさず、DEFAULT readbackでもIUSB code0だけ受理する。
chip自律DEFAULTのBC1.2は仕様上SDP=100mA、CDP/DCP検出なら1.5Aにし得る。
D+/D-実配線/検出結果/negotiated budgetは未観測であり、将来のchip検出結果まで100mAに固定したとは言わない。
100mAのread値を実BC1.2資格や1.5A許可に読み替えず、read時にcode>0ならBUDGET拒否する。
再検出時も絶対100mA以下が必須の給電条件には、chip自律DEFAULTを適用済みとしない。
この制約を無視して強制D+/D-、500mA等のwriteを追加しない。

## 実D0Entryの再採用

D0入口でPIO/idle/DMAoffと旧transport無faultを先に確認。再入はworkerをarmし、bus_lock内で
新しいDEFAULT実readを一回実施。一致後だけ旧sessionのexit証拠を一回消費し、新epochを発行する。
その後Linuxboard profile+USB100mAのreadback、BQ受理snapshot/pet、LCの動的PEC値を公開。
exit結果のcopyだけで再epochを作らず、bootBQHRを再採用しない。外部broker queueは登録しない。
読出し失敗/不一致/期限切れはfaultをラッチ、追加RESET/retry/profile writeを行わない。
同一device lifetime限定。reload/global successorやclock復旧を保証しない。

LC残量/電圧は実値で、校正/IDをimageへ固定しない。実充電電流はUNKNOWNのまま。
実KMDF D0Exit/sleep/resume/充電はmainのみ、署名候補のsource実装と区別する。
