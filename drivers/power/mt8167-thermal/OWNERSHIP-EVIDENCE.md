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


## Superseding：未資格native測定の配布停止

旧a7400f3/親1012 thermal0.0.1.2署名tupleを次の無人batchへ入れない。
署名/compile成功は停止資格ではない。旧bugcheckはreset・電源隔離を証明せず、
削除すると旧ReleaseHardwareの無条件unmap/guard解放へ進む危険がある。

| 必要条件 | 現状と判定 |
|---|---|
| 全ADC/firmware/EL3利用者の排他 | Linux正常世界のresetのみでは証明不能。未資格 |
| IRQ mask/status/clear/ISR-DPC drain | 子にはIRQ資源/同期契約がなく、pending状態のclear根拠も不足。未資格 |
| thermal AHB/ADC停止 | 有限pollで失敗し得る。成功時だけのquiesceは失敗後retentionの証明にならない |
| 親clock/PWRAP/lease | StopIdle/参照は通常D0に関するもの。sleep/surprise/removeでの保持を保証しない |
| quarantineの所有者 | WDF子contextやReleaseHardwareの失敗返却だけではunload/resource再割当てを防げない。親変更なしでは資格化不能 |

従ってハードウェアへ入る前に拒否する。native INFは0.0.1.3へ更新し、
実PnP PrepareHardwareで資源tuple検査後STATUS_NOT_SUPPORTEDを返す。
mapping/interface/lease/clock/ADC/DMA/読出しinterfaceの取得・公開は行わない。
D0Entryも防御的に拒否する。通常はCode10となるが実PnP表示は未確認。
これは測温実装の完成でも温度値0の成功stubでもない。

旧monitor/oneshot/lease/shared/codec ADCの全測定flagはsourceとbuilderで拒否。
NativeOneShot/SharedOneShot一括wrapperも出力作成・親buildより前に拒否する。
native単独builderは拒否専用TUのみcompileし、危険な旧hardware objectをlinkしない。
校正専用buildは従来どおり測定しない。親1012・BQ・SD・既署名成果物は変更しない。

ownership.hのquarantineはアクセスを失効させる一方、resources/lease/handoffの
会計を保持し、通常revokeで消さない。これは物理保持機構の代用品ではなく、
ハードウェアbackendを再開する許可にもならない。今の拒否専用経路は所有権を
取得しないためquarantineへ入る未知DMA状態も作らない。

将来再開する場合はmain/Ramanと親側のIRQ/rundown/remove・非PnP寿命の隔離所有を
先に設計する。世代を無効化して新規操作/温度公開を止め、inflightを同期し、
未停止hardwareのmapping/clock/DMA backingと所有者code寿命を保持し、親の別機能を
無条件poisonしないこと。通常測定失敗をOS停止・成功解放へ変換してはならない。
共有親変更には別版の予約が必要であり、本修正で1012を同名異SYS化しない。
