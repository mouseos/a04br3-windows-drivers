# 排他的MOU0006 WDDM variantのnative core

最終render候補は同じMOU0006を排他的に所有し、Dxgkだけが実GPU IRQを登録する。既存KMDF providerは復帰baselineとして保持し、同時にIRQを握らない。以前のNATIVE_OWNER_ABI_BINDING.mdの「IRQ構成の決定が必要」は、この方針で決定済み。実装/実OS検証は以下の段階で区別する。

run33で07285ac由来のprivate clear4framesがSTATUS0/PHASE4/MATCH75300/MISMATCH0/GUARD0/COMPLETED_FRAMES4を実測した（主担当報告、数値は16進）。fresh35実SYS hash照合はまだ待ち。この結果は新しいMDL/IRQ coreや一般D3D/triangle成功の証拠ではない。

## Herschelへ渡す共有headerと実module

- `native_owner_mdl.h/.c`: ntddkのみ、WDF/DDI/MIPS FWIF enumをheaderへ持ち込まない。`a04_native_mdl_bind`へ実adapter/PDO/VidMm locked MDL/全page extentとminiport-owned retain/drop leaseを渡す。lease acquireは実VidMm資源の参照とNormal RAM由来を確認し、cache flagだけで属性を捏造しない。成功すると同じMDLとNormal CPU mapping、real SGから得たDMA pagesを保持する。`a04_native_mdl_pages/cpu/original`で参照できる。
- `native_owner_irq.h/.c`: headerはruntime pointerをopaque forward declare。Dxgk adapterにresident `a04_native_irq_core`を埋め込み、実D0 MMIOと実m1.11 runtime、単調epochでbindする。MIPS registersの実mask/ackを使い、KMDF interrupt/workitemを作らない。`a04_native_irq_claim`は実Dxgk ISRだけから、本物の割込同期lock下で呼ぶ。初期enableもservice後のrearmを使う。
- 既存`windows_native_submit.h/.c`: prepare済みnative pairの非待機publish。native lifetime/command_busyと実割込同期をcallerが保持する。これはGEO ABI callback tableの代替ではない。

Herschelは上記headerをそのままincludeし、MDLとDxgk/adapterの参照を実lifetimeへ結びつける。m1.11実runtimeやnative resource初期化はprivate TU側に置く。modern FWIF型をこれらprivate Cファイルに混ぜない。coreの正本/変更担当はnative workerを維持する。headerだけを手作業で再定義しない。

## locked MDL bindingの実契約

全extentはpage-aligned開始、最大4MiB、page単位。部分MDLやbyte offsetはこの段階で扱わない。実OSのGetScatterGatherListを最大64KiBずつ呼び、各adapter logical pageを保持する。32bitで元MDL PFNと完全一致するmappingのみ受け付け、bounce/別PFN/IOMMU translationは未検証として拒否する。偽SG listやPA直送で代用しない。prepare失敗は取得済SGを全てPutしleaseをdropする。

CPU mappingは元のMemory Manager MDL mappingで、Device common-bufferをuserへ出さない。pre-device cache同期とGPU完了後のCPU cache同期は元MDLへKeFlushIoBuffersを行う。これはcache処理そのものの成功であり、GPU_DONE/CPU_VISIBLE/実IRQを捏造するreceiptではない。GPU完了・SLC/GP・その後のcache同期をjob stateで組み合わせる経路はまだ接続前。

GPU rootから見える前に`mark_exposed`を呼ぶ。一度公開したbindingはcold releaseできず、SG/MDL/pages/contextを保持する。現moduleに`exposed=false`を勝手に書き戻すAPI、任意boolean proofで解放するAPI、常時成功quiesceはない。実MMU root撤去、cache invalidation、DMA drain、IRQ callback rundown、epoch退役を実装した後にrelease経路を追加する。

## 実IRQ transportの意味

claimは実pending sourceを読み、source mask→readback→ack→status readbackの順で処理しepoch/serial receiptを返す。masked sourceをshared IRQ再入から再claimしない。fault!=0/serial0は障害であり完了ではない。receiptはIRQ発生だけの証拠で、DMA_COMPLETEDやCPU_VISIBLEではない。

rearmは同じepoch/serial、実FWCCB ring空、source deassertionを要求する。古いreceipt、未処理FWCCB、残存level source、readback異常ではenableしない。mask_closeは実source maskとreadbackまでを行い、それをDMA停止と呼ばない。unbind_disconnectedは実OS側のIRQ disconnect/rundownを済ませたcallerだけが使い、MMIO/DMAの所有はadapter lifecycleが維持する。

## 次に接続する義務

1. native clear targetを同じMDL pagesへmapするfactoryと、MDL CPU readback/cache path。現在のprivate output成功を外部bind成功に流用しない。現validated profileは800x600/fixed colourであり、DDI clientの256x256や任意ARGBを黙って変換しない。
2. PASSIVE prepare→非待機submit→TA/3D完了→SLC/GP→MDL cache処理→その後の実hardware IRQの状態遷移。Herschelのinterrupt_eventへは最後の実IRQからだけGPU_DONE|CPU_VISIBLEを返す。manual IRQLやpassive pollingからDxgk完了を呼ばない。
3. reset/quiesce/releaseの実drain+IRQ rundown+epoch retirement。GEO_PROOF_REQUIREDを返せるまでcontrol interfaceのfull capabilityを公開しない。

## 検証とbuild範囲

host OS doublesでreal API呼出し順、64KiB分割、非連続PFN、全map失敗のunwind、bounce拒否、exposed保持、IRQ pending/mask/ack/FWCCB/epoch/rearm/closeを検証した。OS callbackはdoubleであり実IRQ/ARM cache/WDKの証拠ではない。moduleは新variantへ組み込むためのsourceで、既存KMDF provider buildのsource listにはまだ追加していない。したがってこの追加でrun33の固定候補や復帰baselineを変えない。
