# 所有GPU資源の正常停止・解放契約

固定source: kernel-4.14 eb47f42e1f84fe5716ef239620725f82718a620a、
`drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693/`。
通常source調査であり、実機停止成功の記録ではない。

## upstreamが実際にfreeへ進む順序

1. `services/server/common/pvrsrv.c:PVRSRVDeviceDestroy` (2526以降) は
   PowerLockを取り、forced idle (2617前後) の後、
   `PVRSRVSetDevicePowerStateKM(OFF,TRUE)` (2635) を呼ぶ。
   正常services-stateでの失敗はreturnする。bad-state/force-unloadの
   失敗無視分岐は本契約に含めない。
2. `common/power.c` のPrePowerStateはdevice callback (402以降) を先に、
   platform callback (421以降) を後に実行する。
   `rgxinit.c` (1265前後) が登録するdevice callbackはRGXPrePowerState。
3. `rgxpower.c:RGXPrePowerState` (160以降) はFW off request/ACK、
   ePowState OFF、実ISRのhost sample countとFW countの一致を確認し、RGXStopする。
4. `rgxstartstop.c:RGXStop` (900〜1130) はSidekick/SLC idle、MTS association解除、
   BIF/BIFPM MMU requests、external reads、SLC_STATUS1、SLC/Garten idleを確認する。
5. `services/system/rgx_mtk/mt8167/mtk_mfgsys.c:MTKDevPrePowerState` (533以降)
   はAPM/clockを停止し、`mtk_mfg_disable_clock` (222以降) のruntime putsが
   MFG→MFG_2D→MFG_ASYNC (236〜239) のplatform-offへ進む。
6. この正常power transition成功後に `DevDeInitRGX` (pvrsrv.c:2665) を呼ぶ。
   `rgxinit.c:DevDeInitRGX` (3400以降) はLISR/MISR/process queues/APM ISRを
   uninstall (3500前後)、MMU unregister/register unmapの後、FW code (3594)、
   MIPS trampoline、FW data (3617)、FW kernel context (3646以降) を解放する。

## Windowsへ実装する同等の運用条件

対象は所有GPUとそのleaseであり、他SoC master全体の形式証明を追加条件にしない。
`native_owner_factory*.c` はactual Dxgk lockで新submitを閉じ、PASSIVE operationsを
rundownし、owner mutexでFW power/queue操作を直列化する。prepared/submitted tokenは
retireし、保持する実MDLへのCPU visibilityも確立する。

実IRQ counter drain→FW forced idle/OFF→実IRQ OFF stable count/empty queues/source close→
RGXStopを含むGPU0 scoped reset保持→MFG clock gate→parent1014 platform-off→fresh VerifyOffの
全成功後にのみhardware_owned/exposedを解除する。FW ACK、idle、IRQ mask、reset単独では
解放しない。未知・timeout・late count・失敗ではDMA/MDL/adapter/parent leaseを保持する。
passive内部graph再構築もmutex＋actual IRQ consumer gate closeで旧graphへの参照を止める。
物理IRQはDxgkが所有し続け、IoDisconnectInterruptを実施したとは扱わない。

masked未claim sourceが残る場合、off-drain開始はCLEARせずIRQ_ENABLEをreadback付きで
再開し、実登録ISRがsampleするまでBUSYを返す。手動ISR/偽DIRQL/偽通知はない。
初期root未publish・FW start未attemptのcold unwindは別の初期化失敗cleanup classであり、
FW ACK/platform-offを観測したと偽らない。D3はcoldでも実platform-offを必要とする。

## 必須のmatched platform構成

core/resetだけでfreeへ進む同版正常経路は採用していない。正常MTK platform-offを用いる。
parent branch `work/ge8300-platform-owner-1014` の2502364は、正規割当SPM第六resourceと
既存INFRAだけを単一ownerが操作するV2 interfaceを提供する。GPUCore/MFG/IRQは二重mapしない。
UEFI機種branch `work/windows-gpu-domain-owner` の2f6f73a3が既定OFFの
A04BR3_GPU_DOMAIN_OWNER=1でSPMを割当。共通EDK2不変、ACPI storage GUIDは既存値を保持。
旧ACPIでSPMをphysical fallback mapしない。PWRON_CONFIG_ENはread-onlyで既存unlockが
無ければ拒否し、CPU4/watchdog/PLL/PMICを変更しない。

host model testsはIRQ W1C、write failure、OFF readback、無関係bit保持を検査した。
parent2502364の実WDK build/linkは成功。factoryを含む実WDK、full machine FD、
最終KMD/UMD link、Windows実機の正常stop/reset/drawは別の検証段階であり未完。
