# 所有権、測定完了、D0外に関する確認

2026-11-04、CT3公開Linux 4.4の `drivers/misc/mediatek/thermal/mt8167/mtk_ts_cpu.c`、
同 `include/mt-plat/mt8167/include/mach/mtk_thermal.h`、
`arch/arm64/boot/dts/mediatek/mt8167.dtsi` と機種UEFIを静的に照合した。
実機のレジスタや充電状態は調べていない。

- device treeはthermalを0x1100d000、AUXADCを0x11003000として記述する。
  温度コントローラはADC channel 11のdata registerをvalidとvoltage双方の
  読取先に指定し、`TEMPADCVALIDMASK=0x2c` は12番bitがvalid、1で有効と
  コメントされる。ただしこれはコントローラ内部のADC受付条件であり、Windowsが
  各sensorの新しい変換完了を判別できる証拠ではない。TEMPMSRの値変化は新鮮さの
  十分条件ではなく、一定値が続いても正常な場合がある。
- Linuxの割込みハンドラは`TEMPMONINTSTS`を読み、温度割込みを診断する。
  同ヘッダには即時・filter状態bitがあるが、読出し副作用、クリア方法と各sensorの
  更新保証は判明していない。いずれも変換完了の証明として採用しない。
- Linuxのsuspend経路は読取を無効化し、AHB状態を待ち、周期測定をpause／無効化し、
  クロック等を停止する。resumeはreset、クロック、アナログ回路、fast init、通常
  初期化後に読取を再開する。deep idle経路にもpauseとAUXADC clock停止、再初期化が
  ある。Linuxの手順はWindows電源遷移で安全に再現できる保証ではない。
- 機種UEFIのThermalCalDxeはHOBを揮発性変数に写すだけで、温度/AUXADCを停止する
  契約を出さない。BootShimにあるSMCはCPU起動・停止に関わるものであり、EL3が
  thermalやAUXADCを解放した証拠ではない。UniXNU-a04br3-mainlineのdrivers内を
  thermal／auxadc／対象アドレスで検索した範囲にも所有解放の契約は見つからなかった。
  これはEL3や他のファームウェア全体に所有者が存在しない証明ではない。
- WindowsのリースはtouchのD0退出時に失効するが、照会後にD0退出する競合を排除
  しない。校正診断は取得・即時解放とし、監視試作の所有ゲートは閉じたままにする。
  将来の監視にはサプライヤの電源寿命参照に加え、システムスリープや強制削除時の
  停止同期が必要。電源参照だけをD0外維持の証明としない。
- BQのWindows試作とUEFIの `BqAdmissionVerified` は共に未証明時に開始を拒否する。
  先行所有者の最終kick期限と単一I2C2所有、D0外・スリープ・OS停止・バス故障時の
  継続責任は未確立。UEFI/Windowsのどちらもreadを含むBQ通信を開始してはならない。

安全な測温開始には、ファームウェアとEL3の明示的な停止・引継ぎ、全AUXADC利用者の
排他、変換と各センサーの鮮度の根拠、touch寿命の同期、OS停止中も有効な独立停止
経路をそれぞれ確認する。充電維持の証明は別件として必要である。
