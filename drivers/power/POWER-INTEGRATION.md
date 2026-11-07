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
