# Herschelへのowner checkpoint連絡

主担当からHerschelへ中継するための共有事項。GPU runtime/producer担当は維持する。
H側sourceは5a26a0dのnative_binding.h/owner_control.hを確認した。

- checkpoint82d8ba1は同じVidMm locked MDL/PFNへcoldでPTを切替える実装。
  `a04_vendor_native_attach_mdl_target_cold`、
  `a04_vendor_native_target_before_fw_start`、
  `a04_native_mdl_hold_cold/drop_cold/extent/adapter`が既存の実coreへ追加済み。
- `A04_GE8300_NATIVE_OWNER_MDL_TARGET`既定0。実800x600 active1,920,000bytes、
  rounded469pages=1,921,024bytes、offset0のみ。source/host試験済みだが、
  flag1の実WDKは未実施。既存acf523eの5TU module+kernel API LinkCheckは成功済み。
- cold PT照合→元SG/PFNへの置換→Normal CPU sentinel/cache同期を行い、
  成功後private pixel common buffersを解放する。exposed後は保持し、
  poison/不明停止時に解放しない。別private outputへの描画をbind成功としない。
- Create/Delete/control-v2全tableは未接続。新Hのaperture configを含むfactory ABI、
  service callback、実DxgkだけがIRQ所有する構成を維持する。
- `GEO_OWNER_REQUIRED`、DMA_STOPPED/CPU_VISIBLE/IRQ_DRAINED等のproofは未広告。
  aperture reserve/map、任意surface/colour、実IRQ後visibility、quiesce/reset/rundown
  を満たすまでGeoNativeOwnerCreate成功やrender capsを捏造しない。
- triangle candidateは同じ3枚の4KiB owned code/data page内へ収まる。
  mapper追加・owner ABI変更は要求しない。独立したprivate2job実証候補であり、
  HのD3D UMD/standard DDI完成を意味しない。

現在はユーザー指示に従いtriangle/texture candidateを優先し、owner変更を保全している。
H側はnative factoryの未実装symbolを成功stubで埋めず、source/link依存として残すこと。

## H5a26a0d aperture bindへの実接続追加

provider側に以下を実装した（実機/runner未操作、GEO tableは未公開）。

- `a04_vendor_native_target_aperture(n,&window)`：実ownerの確保済み
  PD/PTから、GPU base0x8200000000、window4MiB、現在のone-surface commit limit
  469pages、最高DMA0xffffffffを返す。CPU BAR/identity mappingの推測ではない。
- `a04_vendor_native_attach_mdl_target_at_cold(n,mdl_binding,aperture_byte_offset,packed_argb)`：
  Hのbindが受け取るaperture_byte_offsetそのものへ469pageを配置する。
  offsetはpage aligned、末端は4MiB以内。他のPTEは全てinvalidにする。
  実OS SG/PFN一致、MDL extentとNormal lease契約は従来どおり必須。
- target変更後に元private outputを解放し、PBE/EOT/clear code/record/CCB shadowを
  実target VAとpackedDWORDにより再生成・Device readback確認する。
  A8R8G8B8のメモリはB,G,R,A bytesなのでpacked ARGBをそのままpixel DWORDとして扱う。
  alpha0を含めてCPU projectionを確認。GPUで任意色が成功したという主張ではない。
- 現profileは800x600/pitch3200/full-MDL/first-coldのみ。
  offset500pagesのPT leaf跨ぎ、offset555pagesのwindow末端、
  非整列/UINT64 overflow/範囲外で無変更、既存exposed保持/cold detachをhost確認。
- 実WDK helperは`-WithMdlTarget -LinkCheck`で新entryもexportしてlink対象へ残す。
  helperの実WDK実行は主担当管理、ここでは未実施。

H側で次に必要な接点（実装担当を重複させず主担当から中継）：

1. VidMm allocationの**実Normal-RAM originとMDL/pages参照**をretain/dropする
   frontend口をnative factoryへ渡す。現a04_native_mdl_leaseはこの実参照を必要とする。
   MDL_PAGES_LOCKEDだけやcache flagだけでoriginを捏造しない。
   推奨はgeo_native_bindingのsource-linked factory入力にfrontend contextと
   retain/dropを追加、または同じ意味のfrontend accessorを提供すること。
   public DOD/copy interface変更は不要。GEO bindのbytesはactive1920000、
   actual MDL countはrounded1921024であることを保持する。
2. `GeoNativeOwnerCreate`がfullcontrolを返す前に、actual Dxgk IRQだけを経由する
   completion/visibilityと、platform/fabric停止を結合する。
   `A04BR3RogueStop.h`自体がexternal fabric drainの証明ではないと明記している。
   power_off_ack/IRQ mask/Stop成功だけからGEO_PROOF_DMA_STOPPEDを返さない。
   その証拠がない間、exposed DMA/MDLは保持し、capsやquiesce成功を返さない。

このowner追加はGPU2.5最終候補860f697とは別のhost作業。
主担当の2.5 buildは860f697固定のまま。owner commitを無断で混ぜて再buildしない。

## nonwaiting submit / PASSIVE finishの実分離

`a04_vendor_native_owner_submit(n,start,expected_kccb_write)`を追加。
実Dxgk interrupt同期lock＋保持済D0/MDL leaseのcaller専用で、actual bound MDL、
元CCB/UFO/cleanup/KCCB空状態を固定量だけ照合し、既存two-kick publisherを呼ぶ。
allocation/wait/cache/callbackなし。accepted後はcommand_busyを保持し、
candidateのattempted/published/owner_pendingで二重投入を拒否する。

`a04_vendor_native_owner_finish_passive(n,start,&result)`は、既存validated
publisherから共通化したfinishを実行する。TA/3D/UFO/cleanup完了→SLC flush→
GP SYNC→元MDLのKeFlushIoBuffers→同じMDL上のpixel/guard確認を行う。
成功はCPU側completion/cache/readbackの観測であり、
GEO_COMPLETION_GPU_DONE|CPU_VISIBLEのDIRQL deliveryやquiescence proofではない。
実source claim/serialに基づく後続hardware IRQへの結合は引き続き必要。
同期private baselineも同じfinishを使い、host1/16frame・triangle2frameの回帰を確認。

host試験はreal coreにOS/SG/FW RAMの明示的doubleを組み合わせたもの。
aperture500pages・非連続実SGモデル・packedARGBの同MDLを使い、
IRQL5モデルでzero allocation/waitのaccept（実ISRの証拠ではない）、
127→0→1 wrap、accepted≠completed、PASSIVE fence/cache/readback、
timeout/pixel mismatchのpoison、SG/MDL保持を確認。IRQ通知は作っていない。
実WDK helper `-WithMdlTarget -LinkCheck`へ両entryをexport追加。

現APIはone cold-prepared surface/jobを動かす実coreである。
一般live prepare/discard、後続IRQ visibility、platform/fabric quiesce、
frontend実MDL lease口を結合するまでGEO table/63capsは公開しない。


## 後続実IRQのvisibility receipt実装

`a04_vendor_native_owner_arm_visibility(n,start,irq,epoch,job,generation)`を追加。
PASSIVE finish成功後、実Dxgk interrupt同期lock内からだけ呼ぶ。
実runtime/MMIO/epoch、同MDL、cache/readback済candidate、空KCCB/FWCCBを確認し、
masked sourceをrearm後、fresh GP SYNCを発行する。追加render jobではない。
epoch/job/generationをsource-linked owner側で保持し、publish失敗はpoison+保持。

実Dxgk ISRから`a04_vendor_native_owner_interrupt_event`を呼ぶ。
内部で実MIPS IRQ statusをclaim/mask/ackし、fresh GP fence更新・queue drain・
epoch/serial一致・先行MDL cache完了を確認した場合だけcompleted=TRUEを返す。
sourceなし、fence前、古いepoch、duplicateではcompletedを返さない。
pendingはjob/epoch/generation=0。関数内にKeRaiseIrqlやDxgk通知callbackはない。

H側control-v2への対応：
- `interrupt_event`：claimed=false→STATUS_NOT_FOUND、
  claimed=true/completed=false→STATUS_PENDING(job=visibility=0)として実DPC/serviceへ。
  completed=trueだけをGEOのGPU_DONE|CPU_VISIBLEへ変換して実ISRから通知する。
- `service`：submitted jobはfinish_passive→成功後に実Dxgk同期callbackで
  arm_visibility。startup/非job FW requestsは別stateで実handlerへ。
- IRQ rearm/FWCCB pendingやdevice faultを成功に丸めない。hardwareがfresh GPの
  通知sourceを出さなければpending/timeoutであり、passive pollから完了を捏造しない。
- event deliveryでもexposed MDL/SG/DMAは解放しない。quiescence proofとは独立。

host W1C MMIO/FW doubleで、sourceなしは未完了、先行IRQ/fence未更新はpending、
その後のfresh source+GP更新で正しいepoch/job/generationを一度だけ返すこと、
duplicate sourceでは再完了しないことを確認した。real IRQ/WDKの実測ではない。
helper-WithMdlTarget-LinkCheckに両entryをexportした。

run40のWindows private triangle/texture成功はproducerの実証。
現GEO owner_ops.prepareはpacked clearのみなので、triangle/textureをstandard DDIへ
運ぶにはH側の明示的なdraw/shader/resource semantic契約も必要。
clear packetをtriangleとして解釈したり、固定profileを任意D3D成功として返さない。
resource生成はnative_triangle_suiteの実VA入力と同じ4KiB heap3枚を再利用する。

run40 baseline-retireはCLEANUP0xF/FAILED4/DMA RETAINED後、正規BOOT_REASON9に到達した。
shutdown進行の実測であり、同一OS内のWDDM quiesce/fabric停止成功とは扱わない。


## run40以後のcontrol-v2 execution接続

H42c1834を読み、既存owner_ops/control-v2 ABIがclear中心のままであることを確認。
`native_owner_dispatch.[ch]`に実coreの呼出順を統合した。factory/63capsは未公開。
Hのprovider cookieへresidentな`a04_native_owner_dispatch`を埋め込み、
保持済のnative/start/irq/epochと実Dxgk同期関数をinitializeへ渡す。

- `ops.submit`は実同期lock内で`a04_native_dispatch_submit`。
  初期IRQはreal startup/restart側でrearm済みであることが必要。
  受理時にjob/generationを保持し、同じCCB/UFO/cleanupの実publisherへ送る。
- `control.interrupt_event`は実ISRから`a04_native_dispatch_interrupt`。
  FALSEならNOT_FOUND、claimedかつ非完了ならPENDING（GEO event.epochはlease epoch、
  job/generation/visibilityは0）、completedのときだけ保持済IDと一致する
  GPU_DONE|CPU_VISIBLEを返す。fault statusはそのまま伝搬する。
- `control.service`は単一PASSIVE workerから`a04_native_dispatch_service`。
  実source claimがないと拒否する。TA/3D→SLC/GP→同MDLcache/readbackを実行し、
  実同期callback内で後続GPをarmする。ここでDxgk通知はしない。
  fence前のIRQはserviceでsourceをrearmするだけでrender再送もcache再実行もしない。
  FWCCB非emptyでBUSYなら本当のFW request serviceが必要。捨てたり成功に丸めない。
- synchronizeは`BOOLEAN(void *context,BOOLEAN(*routine)(void *),void *arg)`。
  実DxgkCbSynchronizeExecutionへ委譲し、同期callback完了後にだけ返す。
  worker/lifecycleは外側で直列化し、停止中もpointed-to resourceを保持する。
  serviceは外側callback失敗/未実行を成功扱いしない。
- 現dispatchはcold-prepared one-jobの結合。live再prepare/discard/unbind、
  D0取得、real Normal origin/MDL保持口、DMA停止+rundown、Create/Delete全体は残る。
  この不足を成功stubで埋めない。H側draw/shader semanticとproducer結合も別途必要。

hostのreal-core+OS/FW/MMIO doubleでdispatch経由の正常・timeout・pixel不一致を追加確認。
受理≠完了、初期source必須、fence前IRQでrearmのみ、post-fence実sourceのID一致、
失敗時DMA/MDL保持を確認した。WDK/実IRQ/一般D3D成功の証拠ではない。
`build-ge8300-native-owner-core.ps1 -WithMdlTarget -LinkCheck`に新TU/4exportを追加。
run40の2.5成功source860f697とは独立し、INF/installed providerは変更していない。


## provider-owned allocation / frontend最小ABI

`native_owner_allocation.[ch]`はproviderが実PDO referenceとVersion3 DMA adapterを
取得し、既存uncached FW stage→render contexts→native HWRT/freelist/code/packetを
構築する。copy-provider、WDF DMA/IRQ、別PDOへの依存は無い。
失敗時はnative→同MDL binding→render→FW→adapter→PDOの順に解放。
下位解放失敗やhardware exposureではopaque ownerを残して保持する。
createの成功はcold resourcesの成功のみ。GEO factory成功/capsではない。
`a04_native_allocation_bind_cold`はfrontendの元MDLから実OS SGを取得し、
既存same-MDL targetへaperture offset/packedARGBを使って接続する。

H側に依頼する入力は`native_owner_frontend.h`のversion1:
`{size,version,normal_mdl:{context,acquire,release}}`。
acquire(context,original_mdl)はfrontendの実allocation registry内で元MDLをresolveし、
Normal RAM由来とpages lifetimeを同時に確認・保持する。releaseはその参照だけを落とす。
unknown MDLへ成功しない。provider側がSG/cache/VA/packetを構築するのでHが
GPU packetを扱う必要は無い。context/callbackはbindingのpoison保持期間もresident。
この入力を受け取る最終factory呼出しのABI更新はH、factory/owner実装は当方。

現在のGPU0 ACPI正本はGPST（snapshot）とcore/MFG/IRQ217だけを公開し、
SPM/INFRAの停止・保護ack操作を持たない。Linux mt8167のpower-offは
MFG_2Dのbus_prot_mask BIT2|BIT5のset+ack等を含む。
既存RGXStop/forced-power-off成功だけではこのfabric停止を代替できない。
providerが共有INFRA/SPMを無断mapしてDMA_STOPPEDを返すことはしない。
provider側で正規platform停止経路とIRQ rundown/epoch retirementを実装する作業は維持。

WDK helperの`-WithOwnerAllocation -WithMdlTarget -LinkCheck`は実allocation APIを
exportして下位FW/render実装ごとlinkする。host ownership testは明示的な下位doubleで
各取得失敗・target bind失敗・解放失敗・exposure時の保持を確認するもの。
成功profile/layout自体は既存real-core testsと区別する。


## H作業中CreateV3/resolverへの具体的な対応

Hの作業中`GeoNativeOwnerCreateV3(...,const geo_frontend_memory *,...)`と
`geo_frontend_mdl_lease`を読み、独立pinした同PFNのMDLを使うことを確認した。
factoryのallocation_createはfrontend=NULLで作成可能（cold資源のみ）。
GEO bind時にH入力memory.resolve(context,original_vidmm_mdl,1920000,&lease)を呼び、
成功時はsize/version1/originGEO_MDL_VIDMM_CACHED_SHADOW/active1920000/
page_bytes1921024/mdl/acquire/releaseを照合する。
`a04_native_allocation_bind_lease_cold(owner,lease.mdl,offset,argb,&normal)`へ、
normal={lease.context,lease.acquire,lease.release}を渡す。
その後lease.release(lease.context,lease.mdl)でresolve参照を成否とも必ず1回落とす。
成功bindingは別途acquireした参照を保持する。元VidMm metadataのMDLを
独立pinの代わりに渡さない。providerのSG/PFN一致/cache検査は従来どおり行う。
このbridgeを含むGEO factory本体とcontrol/停止は当方の担当を維持する。
Hはresolver実装・最終factory呼出し・UMD/PCO/draw state側。
この記録時点でHの新APIはworking-tree draft、固定commit待ちであり、
未記録sourceを最終結合再現条件にしない。


## H resolver f492756 固定版の実bind接続

f492756の`frontend_memory.h`を同じblobで取り込み、provider側
`native_owner_frontend_resolver.[ch]`に実transactionを実装した。
`a04_native_allocation_bind_frontend_cold(owner,memory,original_mdl,active,offset,argb)`
がH resolverを呼び、version/size/origin/active/page_bytes/独立MDLを照合し、
実same-MDL/OS SG bindへ渡し、resolve参照を成否とも1回releaseする。
正常bindはnative側の別acquireだけを保持する。
成功なのにrelease口が欠ける壊れたresolver結果はowner内に保持し、
そのownerのcold deleteを拒否する。所有したpages referenceを失わない。
host試験はsource-linked H resolver doubleで正常、origin不一致、unreleaseable
成功結果の保持を確認。HのSEH pin実行/GPU/WDKの成功とは区別する。
Hのfixed commitは受領済みであり、以後はdraft待ちを阻害条件にしない。
factory/controls/rundownの実装担当は引き続き当方。
