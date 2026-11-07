# SD5: 専用consumerと有界cancel-safe queue

SD4 source16ed4f8と署名tupleは凍結。SD5は別branchの後続候補であり、trial47 Code31や
初回DIFのE0000204の原因修正とは主張しない。BT/USB/親1010/ACPI/eMMC/Audioは非編集。

## 実処理と所有権

- 1台のhostにつき1本のPsCreateSystemThreadをD0Entryで起動する。
  このthreadだけがqueueから取り出した要求でPIO/カードinit/serial待ちを行う。
  既存8個の共有system work itemをhardware待ちに使う方式を廃止した。
- 標準IoCsqInitializeEx/IoCsqInsertIrpEx/IoCsqRemoveNextIrpを使い、待機要求のcancelと
  consumerへの引渡しをOSが排他化する。DriverContext[3]はCSQ用に空け、[0]は完了前に復元する。
- 受付数はactive/queued/completion待ちを含め最大8。queue fullは実STATUS_DEVICE_BUSYと
  SRB_STATUS_BUSY。closing/offlineは未対応成功に変えずNOT_READY。
- rundownと枠の予約はCSQ挿入前、queue spinlockでstopと排他化する。
  既cancel済みIRPはinsert callbackを通らず完了し得るため、callback内の予約に頼らない。
  挿入元の別参照と一度だけのfinish判定で、同期cancel/拒否/consumer完了の競合を扱う。
- CSQから取り出したIRPはconsumerだけがSRB/MDLへアクセスする。active cancelはlive判定で
  PIOを止め、CPU使用終了後にCANCELLED/ABORTED。曖昧なカードwriteのpoisonは解除しない。
  待機cancel/期限切れはカードに触らない。
- 要求受付時のKeQueryInterruptTimeを起点とする既存deadline（既定5秒、上限10秒）を保持し、
  queue滞留やserial取得で再設定しない。consumerのmutex待ちも残り時間に制限する。
  descriptor queryはserialを即時取得できなければBUSYを返し、共有threadを待たせない。

## 完了再入とpower/rundown

PIOは専用threadだが、IoCompleteRequestは1個の短いcompletion work itemへ集約する。
上位completionが同じthreadで同期power/removeへ再入し、D0Exitで自分自身のthread終了を
待つdeadlockを避けるため。このwork itemにはhardware/serial/rundownの待ちを置かない。
上位completion自体の実行時間までは保証しない。完了待ちも上限8に含めて保持する。

finishはSRB/IRP resultとCPU/MDL使用終了の後にhardware rundownを解放する。
上位completion前の解放順を維持し、実host/child参照とschedule参照は完了終了まで保持する。
専用threadにcompletion終了を待たせない。

D0Exit/supplier喪失/Surpriseはqueue gateで新規受付を停止しgeneration/liveを失効する。
専用threadは残queueをMMIO無しの失敗で処理し終えて終了する。
D0Exitはhardware usersのidleを最大12秒、thread joinを最大2秒待ち、最後のserial取得は
最大1秒。idleイベントの更新は新規予約と同一lockで行う。rundown待ちはidle確認後だけ。
timeout時は早期IRP完了/偽drain/unmap/lease解放を行わず、実失敗を返して参照を保持する。
ReleaseHardwareも未joinなら資源を解放しない。強制removeが物理資源を撤去することまで
object参照で阻止できるとは主張しない。

既存のsupplier/ACPI同期要求やWdfTimerStopの下位完了待ちは別契約であり、異常下位driverを
このthread化だけで中断できるとは扱わない。card CMD/PIOの実deadlineは変更しない。
D0Exit成功時だけ従来のカードbarrier/PIO reset/ReleaseQuiescentを行い、DMA機能を追加しない。

## 版とcheckpoint

INFは2026.11.7.5。NativeCheckpointはSD4と同じ40byte/schema1、driver_revision=5。
Add stage4=IoCsqInitializeExを追加。Device/TimerのPassive指定は許可済みのため維持する。
consumer thread生成はAddでなくD0Entryであり、SD4のfirstAdd実結果と混同しない。
新たなWDF work itemや明示WorkItem ExecutionLevelは追加しない。

## 一次資料と確認範囲

- https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/cancel-safe-irp-queues
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocsqinitializeex
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocsqinsertirpex
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-io_csq_complete_canceled_irp
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-pscreatesystemthread
- Microsoft Windows-driver-samples general/cancel/sys と general/cancel/startio を参照し、
  実装本文をコピーせずSD固有のbounded admission/rundownと結合した。

既存SD protocol fixtureをまとめてASan/UBSanで確認する。CSQ/電源競合の実Windows動作は
WDK compile/linkでは証明しない。新しい大量のmock/test fileや単独実機試験を追加しない。
実機はmainの一括tupleのみ。回収成功やfixture通過をSD列挙/RW成功と報告しない。
