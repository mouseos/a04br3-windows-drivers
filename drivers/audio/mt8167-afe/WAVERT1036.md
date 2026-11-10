# MT8167 AFE / MT6392 speaker1036・親1122

通常speakerのSTOP/free・D3/D0・PnP所有契約を実装した署名候補。可聴出力・実機電源遷移の検証は未実施。capture/HPは別契約のため、このprofileでは無効。1034の6 endpoint構成と同じ構成とは扱わない。

## 停止から解放までの根拠

一次実装：s1204IT/CT3_GPL_LGPL_Src、commit `7c4782127d4ea11965be00ce1343056be9bfb769`。以下は同commitのpath/line。

- `kernel-4.4/sound/core/pcm_native.c:2230` release_substream → snd_pcm_drop → hw_free → close。`:1765` dropはstream lock下でsnd_pcm_stopを呼ぶ。
- `kernel-4.4/sound/soc/soc-pcm.c:946` ASoC hw_freeからCPU DAIのhw_freeへ。`:647` closeからCPU/codec shutdownへ。
- `kernel-4.4/sound/soc/mediatek/mt8167/mt8167-afe-pcm.c:2005` STOP/SUSPENDはmemif enableをclearしIRQをdisable。`:653` IRQ1のMCU_CON bit0 mask → IRQ_CLR bit0。`:1928` hw_freeはSRAMならdetach、DRAMならsnd_pcm_lib_free_pages → EMI clock off。
- `kernel-4.4/sound/core/pcm_memory.c:384` free_pagesは動的allocationをsnd_dma_free_pagesで実解放。preallocated bufferの場合はdetachのみであり、両者を混同しない。`sound/core/memalloc.c:265` → DEV bufferは同ファイル`:107` dma_free_coherentまで到達する。
- `mt8167-afe-pcm.c:1767` shutdownはAFE_ON off → substream=NULL → main clock release。`:1087` INT ADDA shutdownはSRC/ADDA/I2S disable。`:2973` resumeではIRQ_MCU_EN low8をunmask。
- `mt8167-afe-pcm.c:2899` ISRはpendingとmemif enableを確認してsnd_pcm_period_elapsed。`pcm_lib.c:1870` period_elapsedもstream lockを取得。通常hw_freeごとにsynchronize_irqを呼ぶコードはこのvendor版にない。remove時はdevm_request_irq (`:3036`)のdevres (`kernel/irq/devres.c:14`) → free_irq → `kernel/irq/manage.c:1484` synchronize_irq。

したがってvendorの根拠は「STOPでfetch元を無効にしてIRQ源を退役し、ソフトウェアの利用者を終えてからmemoryを返す」という全体契約である。独立したfabric-idle register、固定待ち、AFE resetを実施しているとは読めない。Linuxに存在しない物理idle計測を追加の必須条件にして永久保持する以前の判断は、このhw_freeまでの追跡で更新した。disable/readbackは設定の反映を確認し、IRQ同期はhandlerの実行終了を確認する。いずれも単独で物理busのoutstanding数を測定するものではない。

XNU `iokit/Kernel/IODMACommand.cpp:451,1053` のclearMemoryDescriptor/completeはmappingの利用完了後の所有移譲の参考。MT8167 AFE hardware stopを実装した証拠にはしない。

## Windowsで実装した順序

`render_lifecycle_backend.inc` のOwnerCompleteStopLocked:

1. PASSIVE mutexで新状態変更を排他。live position rundownを閉じ、実Class-D mute。
2. KeSynchronizeExecution下でDL1 enable clear → IRQ1 mask/ACK → DAC/IRQ/status readback。IDLEの場合もmask/ACKを確認。
3. IoDisconnectInterrupt、queued DPC remove/flush、通知停止。ISR/DPC/positionにmappingを再利用させない。
4. 新親QI lifecycle v1へ世代付きsoftware receipt。親がpowered exclusive lease下で全memif/IRQ control/statusとmuteを再確認する。
5. 同じ世代の証明後にHAL PutScatterGatherList。これはDMA sourceを止めるAPIではなく、SG/map-register解放・flushを行う。
6. 親はvoice stop/depop → INT ADDA/SRC/I2S/AFE_ON off → 所有route/gate/IRQ_MCU_EN復元 → Class-D/codec/clock返却 → WdfDeviceResumeIdle。
7. FreeAudioBuffer/checkinはPortCls FreePagesFromMdl/UnmapAllocatedPagesへ。D3はSGと親leaseのみ返しclient MDLは保持。D0後RUN時に同MDLのcache再検査、新世代lease・HAL mapping・hardware設定を行う。電源断中にゼロ埋めしない。

VAUD22/28はvendor `arch/arm64/boot/dts/mediatek/mt6392.dtsi`でalways-on/boot-on。releaseでrailをoffしない。未知のregister・固定delayによる完了推測なし。map callback timeoutはfailureでMDL/adapterを保持し、遅延callbackを成功へ変換しない。

`VerifyDmaDrained`も新親profileでは常時stubではない。同世代のCertifyStopped完了とpowered hardware readbackを再確認する。ただし新子のfreeは明示的なCertifyStopped/ReleaseStoppedに従う。旧v11の型・GUID・旧one-shot profileを無言で変更しない。旧profileにはsoftware receiptがないため拒否を維持する。

PnP revokeは新入場を閉じ、hardware/SG/親を先に返し、PortCls client pagesは各Free/destructorまで保持。TopologyのIUnregisterPhysicalConnection/IUnregisterSubdeviceを取得し、connection → wave → topology → COM refsの順に返す。WaveRTが同interfaceを提供すると仮定せず、公開契約のUnknown引数で登録を指定する。部分失敗では登録refsを保持し再STARTを拒否する。存在しないPcUnregisterSubdeviceをimportしない。

SURPRISEまたは停止証明失敗は成功としない。ISR/DPCを切離すが、未停止DMAのSG/MDL/親参照をquarantineしてfreeしない。故障後の自動復旧や強制解放を成功stubで補わない。

WDK一次契約：

- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pput_scatter_gather_list
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iodisconnectinterrupt
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kesynchronizeexecution
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nn-portcls-iadapterpowermanagement
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nn-portcls-iunregistersubdevice
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iunregistersubdevice-unregistersubdevice
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iunregisterphysicalconnection-unregisterphysicalconnection

## 増分build（既VM、setup再構築なし）

このbranchの固定HEADを独立checkoutへ反映し、PS5で実行する。変数は既VMの実pathを使用し、新SDK/鍵を生成しない。出力をcheckoutのgitignore済みbuild配下に置く。別profileのsourceや旧OBJを混ぜない。

```powershell
# $src: 固定source checkout。$reuse: 既1121 recipeのparent/*.objとlib/wdf。
# $clock1121: parent-clock_snapshot.objの既directory。
# $kit: 既WDK8.1 Include、$um: 既互換UM headers、$wdf: KMDF1.13 Include。
# $klib: 既ARM kernel libraries、$portcls: 既RT8.1 ARM PortCls importlib。
& "$src\tools\ci\build-mt8167-audio-parent1122.ps1" `
 -ReuseDirectory $reuse -Parent1121Objects $clock1121 `
 -KitInclude $kit -UmInclude $um -WdfInclude $wdf `
 -KernelLibraryDirectory $klib -OutputDirectory "$src\build\audio1122"
& "$src\tools\ci\build-mt8167-mt6392-lifecycle1036.ps1" `
 -WdkInclude $kit -UmInclude $um -WdkArmLib $klib `
 -PortClsImportLibrary $portcls -LifecycleParent "$src\build\audio1122" `
 -OutputDirectory "$src\build\audio1036"
```

親1121正本source6741f2a1のclosureをbranchへ固定済み。親はwdf_parent_probe.cの1TUのみ再compile、helper13 OBJ＋clock_snapshot OBJをhash/dependency pin付き再利用。追加flagはA04_SHARED_AUDIO_LIFECYCLE=1のみ、USB owner ABI3維持。既GPIO22定義を含む1121 baseline flagsはcache lockのままで、新GPIO/host操作を加えない。

音声は全23TUを同じflagsで再生成。A04_AUDIO_LIFECYCLE/DMA_NOTIFICATIONを有効化、capture/HP flagsは無効。既clang20.1.8/lld-linkを利用。既PortCls importlibにpowerの2関数がなければllvm-dlltoolでimport thunkだけ生成する。保存actual RT8.1 PortCls (`f9d0f5ce9d6072785a9cf8d242c3f490e4b2af6455ff05fec6a21815bc3e9db6`)に両exportあり。SDK/USB/HCD/GPU/PCO/UMD/EDK2の再buildなし。

両unsigned build-manifestを保持してから既Inf2Cat/SignTool・既証明書で親1122と音声1036を署名。署名前後hashを区別し、既1121/HCD1019ABI3tuple・音声1034をrollback原本として保持する。runtimeで新QI必須のため音声だけ旧親1121へ投入しない。package成功、署名成功、ARMでの開始、可聴出力を分けて記録する。

## ローカル検証

`make test-mt8167-audio-lifecycle` は実frontend/backend/IRQ/digital/lifecycle sourceを通し、MMIO/PMIC/HALのみmock。正常RUN/PAUSE/D3/D0/RUN/STOP/free/reopen/remove、stale generation、certification失敗、surprise quarantineを検証する。

保存WDK8.1実headersに対するclang18のARMNT compileでは音声23TU＋親1TUが成功。Linuxのcase-insensitive VFS overlayでheader名の大小差だけを吸収し、SDK内容は編集していない。この確認はWindows側clang20.1.8のlink/署名や実機試験の代わりではない。PS5実行・正式link・署名・実機可聴出力はmainの既VM順番待ち。
