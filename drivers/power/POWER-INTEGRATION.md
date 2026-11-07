# BQ24160 / MT8167 thermal 次候補

専用branch work/bq24160-thermal-integration-20261107。driver repo main=d6dbb37は
古いprototypeのため、統合04ce9b11851fd2950bd0ba1f4325d04fea9e4695をbaseにする。
BQ/LC独立moduleは既存担当23c9a4ba10003abc294d5280ac44ab05083bf0a5から移植した。
main/Ramanのwdf_parent_probeは変更していない。SD用VMC/VMCH bitsも操作しない。

## 新しい実接続

- 既存cold owner/BQHR停止record検査/10秒worker/単一I2C2 lock/失敗ラッチと
  BatteryClassの充電状態通知を含むmodule群を統合baseへ移植。
- 既定OFF A04_BQ_LINUX_PROFILE_BOOT=1はcold owner診断が必須。
  実GaugeD0Entryから、workerを開始し、同I2C2 lockを保持してcold_profileを呼ぶ。
  有効なcold token・まだ無給餌/無試行・10秒の当初期限内だけ入り、1秒以内で終了する。
- Linuxの6時間timer/1.3A charge/100mA termination/4.32V regulationを07/05/03/02へ
  設定してmask付きreadbackする。USB input limitは100mA固定。500mA/ホスト判定を
  推測しない。IN入力の供給能力、実電流、発熱を証明するものではない。
- FAULT/BATOVP/TS異常/無入力/HiZ/未知chip/DPM設定不一致は**書込み前に拒否**。
  HiZ/CDの切替・BATOVP復旧・バス再初期化は接続しない。途中失敗は部分状態を保持し、
  再試行・rollbackをしない。最初のBQ readでもwatchdogが開始し得る。
- 成功後のみ既存broker保守を開始、BatteryClassへ同じcacheの状態を返す。
  D0終了のDEFAULT resetは既存flag/期限付きのまま。DEFAULTは3.6V/27分等へ戻り、
  電源の安全性・充電継続や次D0再入を保証しない。常用resume完成品ではない。
- thermalのboot校正を独立boot_record moduleにし、実ThermalD0へ接続。
  attributesは厳密に6、版/bytes/実校正を検査し、不正時に前のcalを成功扱いしない。
  部分対応属性をmaskして通さない。校正はADC所有/実測温度の証明ではない。

根拠はCT3 GPL/LGPL固定7c4782127d4ea11965be00ce1343056be9bfb769の
`drivers/misc/mediatek/power/bq24160.c:bq24160_charge_init`と既存recovery moduleの
TI SLUSAO0H照合値。thermalの校正式は既存Linux由来moduleを維持する。
個体calibrationをSYSへ焼き込まない。

## main/Ramanへの必要接続（未完了）

親は現在thermal firmware_released=0/adc_exclusive=0でAcquireを拒否する。
これをbuild flagだけでtrueにしない。必要なのはthermal/AUXADCの排他取得・停止・
世代付き寿命と、使用後quiesce→releaseを、既存単一親lockへ接続する実ownerである。
現在のUEFI handoff record observation=1/ownership=0はその許可ではない。
親が未対応ならSharedOneShot buildも温度採取前に拒否する。thermal保護・周波数制御を
実装済みとは扱わない。SD担当にはTOP/PMIC専用bitの非変更と親lock共用を伝達済み。

## 一括build/署名recipe

SYSを変更したので旧signed SYS再包装は不可。両INFは11/07/2026,0.0.1.1。
既存署名identityを使用し、証明書生成・時刻変更・新たな実機操作をしない。

```powershell
powershell -NoProfile -File tools\ci\build-power-candidate.ps1 `
  -CertificateThumbprint <既存identityのthumbprint> `
  -ChargerProfile ColdLinux100 -ThermalProfile SharedOneShot
```

既定はObserve/Calibrationで充電設定を変更しない。profile引数を明示してcandidateを
buildする。builderの従来の単独出力先は維持し、このwrapperでは全出力/tmpを
checkout内build/power-candidateへ固定する。kernel全TUへno-vector/no-VFP/
no-unaligned-accessとWDK8.1 __ldrexd宣言alias、正規libcntprを使う。
SYS/補助EXE署名→CAT再生成→CAT署名→全署名と既存signer照合→最終hash manifestの順。
署名後hash、source/header/recipe hash、全profile flags、実UTC、CRT hash、実link object
hashを記録する。source dirtyなら拒否する。インストール・flashはしない。

現段階はsourceとrecipe。正式WDK compile/link/署名成果物はまだ無い。
mainの既存runnerで一括実行し、実機投入はmainが行う。署名だけでWindowsロード/充電/
温度測定/熱保護の成功とは扱わない。RTC d0aとこのdriver候補を混ぜて同時検証しない。

## 限定検証

`python3 tests/check_power_candidate.py`で新しいcold設定/拒否/部分失敗とstrict校正の
ASan/UBSanを一括実行し、既存thermal/charger/gauge検証を同じbatchで確認する。
小さい変更ごとに実機投入を繰り返すためのものではない。

## 親1012 native tuple（追加）

mainへ親2026.11.7.1012を予約通知、Nashが競合なしと確認。SD親は1010へ分離。
親1008のecaa7b7全flagsをparent1008-baseline-flags.jsonへ固定し、差はthermal診断1と
native実験1だけ、SDは明示0。既存engine親INF1008は保持し、1012専用
shared-parent-thermal-bq.infを選ぶ。親1008を別SYSとして再包装しない。

`-ThermalProfile NativeOneShot`を明示した場合、実thermal子PDOから専用ABI v1を
WdfFdoQueryForInterfaceで直接取得する。touch経由の旧診断ABIを流用しない。
親はStopIdle参照と世代tokenを保持し、同一parent lock/mappingでclock/reset/analogを
実施する。Windows PnPの実親子関係でsupplierは実shared parentであり、別ACPI siblingの
SDに必要なPowerRelationsを、この子へ偽装して作らない。

これはLinuxと同じ正常世界のMMIO/reset/initを使う**明示native OS実験**である。
旧firmware_released/adc_exclusive flagsは0のままで、旧VerifyOwnershipはnative claimを
認証済みfirmware解放と返さない。EL3無干渉・全電源条件の実証とは扱わない。
親はresetをassert/readbackしてからthermal/AUXADC用gate bit1/30だけを解除し、
未知の旧thermal AHB programをungateで再開しない。ADC/thermal私有pagesはPnPで
排他割当てされた子だけがmapし、shared clock/analog/PWRAPを重複mapしない。

有限one-shotは実校正→親1012照会→native claim→Linux fast init/通常init→実3点採取→
thermal AHB idle/periodic停止→ADC channel11 trigger clear/ADC idleを確認する。
この後だけ親へquiescent/releaseを報告し、元のanalog bit0x30、reset bit0、clock gated
差分を復元する。他bit・SD dedicated masks・PMIC電源設定には触れない。
途中失敗でshutdown不明ならclock/lease/mappingを返さず、既存TOPRGU復旧を前提とする
bugcheckを要求する。bugcheck自体を実reset完了の証拠としない。

UARTは既存TOPRGU queueへSTART/CLAIM/実millidegree3値/QUIESCENT等を短く送る。
queue受理とUART送信完了は別であり、校正・tuple不一致も拒否として記録する。
温度値は既存readout interfaceでもfresh期間だけ取得できる。連続監視、冷却、周波数制御、
独立hardware熱停止の完成品ではない。実機はmainがこの明示profileだけで検証する。

matched batch例は`-ChargerProfile ColdLinux100 -ThermalProfile NativeOneShot`。
BQのBATOVP/HiZ拒否は維持し、TS異常時に設定を強行しない。100mAという値はUSB入力制限で、
IN入力能力や実充電電流の検証ではない。thermal native INFは0.0.1.2、BQ INFも0.0.1.2。
署名tupleには親1012を含め、最終SYS/CAT対応とARMNTを検査してhashを固定する。

主担当の最新予約でthermal/BQ親は1012に変更。1009はsource作業中の旧予約だけでSYS未生成。SD専用1010とrun42親1008は保持する。


## 最新の配布制約：thermal hardware profile撤回

上のNativeOneShot/SharedOneShotの署名例は履歴であり、次batchへの適用許可ではない。
IRQ/drainとPnP removal quarantineの未資格を解消できていないため、両profileを
wrapperで拒否する。親1012 thermal0.0.1.2を次batchに投入しない。
詳細はmt8167-thermal/OWNERSHIP-EVIDENCE.mdのsuperseding節。
新native0.0.1.3はPnP startを拒否し、温度interfaceを公開しない安全な未対応binding。
BQの既存候補/署名SYS/CAT、SD1010 tupleは変更しない。この修正のために親を再buildしない。
