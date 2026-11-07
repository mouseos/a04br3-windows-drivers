# 親1018 / thermal0.0.1.5 一回測温候補

署名した実装sourceは7ac48ffba0e99afbf4d13edcfb5a56ecfedc8cfc。
機種UEFIは95216ec29c6286fa488eb7680f630673cc2fc9a0、53d6b645基点。
この文書追加はbinary変更ではなく、署名物のsource pinを置き換えない。

## 実producerから実PnPまで

- LKの動的FDT /chosen または/chosen@0のatag,devinfoから実word60/61/62を
  PrePiでbounds検証して16byte HOBへコピーする。固定値/hostデータを埋め込まない。
- DXEがv1校正をvolatile BS|RT attrs6、A04BR3ThermalCalibrationへ公開。
  未programmed/破損/公開失敗では旧producer停止を行わない。
- 既定OFF A04BR3_THERMAL_PIO_HANDOFF=1だけ、BeforeEBSで旧thermal sourceを
  mask、AHB idle、pause、periodic disable/readback、INFRA reset保持へ移行。
  gated/既reset状態ではLinuxと同じreset先行で旧AHB programを再開しない。
  ADC channel triggerをclearしてidle確認。PLL/PMICを変更しない。
- 停止試行結果をA04BR3ThermalPioHandoff v2（全18 UINT32/72bytes、attrs6）で発行。
  全停止条件の成立時だけready1。停止試行失敗はready0/stage/raw状態を保持し、
  校正未取得では停止試行を行わない。旧v1 ownership0観測recordは維持。
- 親1018はactual query interface v2のMeasureで受領recordとlive reset/clocks/ADCを検証。
  volatile recordを消費し、一回だけ使用する。runtime variable操作はPASSIVE_LEVELで
  parent mutexより前に行う。old record/boot、消費失敗では測定しない。
- 子の実PnP PrepareHardwareが排他的2 MMIOをmapし、親へのAdopted=1で所有を移管。
  子はthermal/ADCレジスタへアクセスしない。親がsingle mutex/rundown/power参照を持つ。
- 旧controllerはreset保持。IRQ/ISR/DPC/autonomous AHB/DMA backingを新設しない。
  TS_CON1のsensor0/1/2とbuffer、200us settling、channel11の旧ready0→25us→新ready1を
  通して実3 rawを採取し、子が実校正でmillidegreeへ変換する。全3点とcleanupが成功した時だけ公開。

## lifetimeと失敗

ADC timeoutは局所trigger clear/idleを確認し、analogをrestoreできた場合も測定失敗で返す。
停止/restore/reset-statusが不明なら親だけをpoisonし、mapping/親object/power leaseを保持。
MmUnmap/ResumeIdle/再試行/KeBugCheckExへ進まない。子へ成功を返さず温度を公開しない。
ParentStopはadmissionを閉じrundownを待ち、retained clock会計を消さない。
再作成parentも消費済みboot recordを使えない。削除時の実PnP動作は実機で未検証。
成功時は旧controller reset保持・ADC idle・analog restoreを確認した後だけprivate mappingを返す。
親の他USB/audio/touch/PMIC ownerをthermal失敗だけでpoisonしない。

UART: A04 THM PIO real LK calibration publish、ready/stage、A04THM PIO START、
FAILED status/stage/adopted またはmc=3値/IDLE restored。queue受理とUART送信は別。
一回診断でありcontinuous thermal protection/周波数制御/suspend resumeの完成品ではない。
実測温度・sensor settling・実PnP retain/removeはmain実機確認が残る。

## 固定tuple

親2026.11.7.1018 / thermal0.0.1.5、両INF日付11/07/2026。
親1016 preliminary署名物は最終reset-status検査修正前として撤回、同版異SYSを生成しない。
1010/SD4/fresh48へこの親SYSを混ぜない。Windows側SDpowerは0、UEFIのSD表は基点保持。
全10 ACPI表/BootShim/配置/ramdisk/DTB/既存definesは53d6と比較する。
新flag以外の既定値、GPU/4CPU/touch/battery/buttons/sharedIO、共通EDKは変更しない。
BQ13は独立ColdLinux100候補であり、このthermal tupleを必要としない。

実WDK8.1 ARM compile/link、既存BCBF identityのSYS/CAT署名、SYS/INF membership検証と
全回収hash照合済み。署名/buildをhardware測温成功とは扱わない。
