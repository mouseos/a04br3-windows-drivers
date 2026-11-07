# Einstein中継用：GE8300 render owner最小公開ABI v2案

対象は`owner_control.h`＋既存`owner_abi.h`。kernel-only、QueryInterface GUID `36871ace-58aa-49b8-b6f3-2e51d5e7f893`、INTERFACE.Version=2/Size=sizeof(geo_owner_control_v2)。現providerが公開済み・合意済みとは扱いません。shader/triangle/runtime/provider sourceはこの担当から変更しません。

## 操作と寿命

| 操作 | IRQL | 成功の実条件 |
|---|---|---|
| QueryInterface | PASSIVE | 実provider PDO/stackへの問い合わせ。成功時1参照を返し、INTERFACE.Context/Reference/Dereference有効 |
| acquire | PASSIVE | D0とrender leaseを実取得。active DOD leaseを強制破壊しない。非zero cookie/単調64bit epoch、実ops、IRQ route/levelを返す |
| create/bind/unbind/destroy | PASSIVE | surface tokenはkernel専用。bindはVidMmの同じlocked MDL、MDL byte offset、extentに対応するPFNを実GPUへmap。Normal CPU mappingとcache処理を確認。別private outputへ描いて成功不可 |
| prepare/discard_prepared | PASSIVE | FW command/resources/cache準備を完了し、非zero job tokenを返す。まだpublishしない |
| submit(job,fence,generation) | 実IRQ同期下 | bounded publishのみ。alloc/wait/poll/mutexなし。成功は受付であり完了ではない。submit中のconsumer再入禁止 |
| interrupt_event | 実登録Dxgk ISR/IRQ lock下 | 実HWイベントからepoch/job/generation/status/visibilityを回収。tokenは未完了間で一意。GPU_DONE＋CPU_VISIBLE両方が実証できる場合のみ両bitを返す。無イベントはSTATUS_NOT_FOUND |
| quiesce(reason) | PASSIVE | 実DMA停止、IRQ/callback drain、全prepared/submitted token失効、同一surface/MDL binding保持、CPU可視性の5項目を全て証明。単なるmask/timeout/Sleepでは成功不可 |
| reset | PASSIVE | 実reset＋上記5項目、新epoch（旧値より大）。旧epoch完了は新jobを解放できない |
| restart/set_power | PASSIVE | 実D0/IRQ再開、または証明済みdrain後の実D3。metadata flagだけをpower成功にしない |
| release | PASSIVE | surface/binding/job/callbackが全てなく、leaseを実返却できたときのみ成功。失敗ならinterface/lease/MDLを保持 |

opsはv2 `geo_owner_ops`をleaseごとに返す。`acquire`が成功しても返却値不正ならcookieを失わずpinし、無根拠にreleaseしない。失敗したbind/submit/reset/powerはpoisonを維持する。quiesce/reset proofのepoch/flagsが不足したらsidecarは再開・解放を拒否する。

## IRQについて決定が必要な点

現在の一次source `vendor_m1_11/windows_irq.c`はWdfInterruptCreateで実IRQを所有し、ISRでmask/ack、DPC→workitemでFWCCBを処理する。既存の同期pollやPASSIVE work成功を、そのままDxgk ISR完了へ変換できない。

v2の`irq_route=GEO_IRQ_DXGK_REGISTERED`は「render miniportの本物のDxgkDdiInterruptRoutineへIRQがPnP上正しく割り当てられ、低水準event処理をownerへ委譲できる」ことを意味する。既存WDF ISRと別PDOで同じIRQを二重所有しない。物理FDOをrender miniportへ移す／ownerをkernel内部serviceへ分離する等の正式な構成決定が必要。現providerを保持する場合の別ISR→Dxgk callback転送を公式に支持できる根拠があれば、合意してABIを改訂する。IRQL一致だけを根拠にその経路を有効化しない。

CPU可視性の成立にPASSIVEのSLC/FW処理が必須なら、interrupt_eventだけでCPU_VISIBLEを返せるかも未確定。この場合は真の後続HW割込を伴う完了設計か、公式WDDMに沿う別経路を先に決める必要がある。偽interrupt/poll完了は使わない。

## owner側へ依頼する回答

同一MDL mappingとNormal CPU/cache契約の実装可否、IRQ topologyとCPU_VISIBLE成立IRQL、quiesce/resetの5proofを満たせるか、retained surface/bindingとepochの扱いを回答してほしい。満たせない操作の具体的欠落を返し、常時拒否stubだけの実装を成果としない。実GPU candidateの既知成功を標準D3D成功とは扱わない。

## 単一miniportへの方針更新

主担当/Einsteinの合意で、MOU0006に既存KMDF providerと排他的に適用するrender miniportへ変更。native coreを同じSYSへsource-linkし、DxgkだけがIRQを所有する。既存KMDF candidateは復帰baselineとして保存、MOU0007 DODは非変更。したがってQueryInterfaceは最終必須ではない。`native_binding.h`のGeoNativeOwnerCreate/Deleteでcontrol_v2 tableを内部取得し、GeoStartInProcessOwnerSessionがInterfaceReference/leaseを保持する。別provider PDO検索・別FDO・WdfInterruptCreateは使わない。

native factoryに渡すのは実PDO、StartDeviceのDXGKRNL_INTERFACE、DxgkCbGetDeviceInformationが返す実資源。factoryはIRQを接続せず、acquire後もsourceをmaskしたまま、sidecar通知初期化後のrestartで有効化する。削除は実lease解放後のみ、失敗ならbase owner contextを保持する。GeoNativeOwnerCreate/Deleteは現時点でnative担当が実装する未解決symbolで、fallback定義はない。

sidecar側はDriverEntry/Start/Stop/Remove/ISR/DPC/power/TDRと、実runtime contextを使うUMD初期化を追加した。残るgeo_ge8300_render_contractは**このWindows frontend担当が埋める**mandatory render DDI/caps/segment/preemption契約で、native担当へWindows caps偽装を依頼するものではない。geo_ge8300_frontendも必須shader/format/stateが実装できるまで定義しない。当初の未解決render contractは今回sidecarで定義した。実frontendは引き続き未解決で、空の表や成功stubで代替しない。

## f62bc96との照合後の接続差分

E固定source f62bc96525f4acbe9c4c8afa9d622fe1969ea7acのnative_owner_mdl/irqを読み取った。native_owner_mdl_bindはoffset0・全MDL・page-aligned extent・bytecount完全一致を要求するため、sidecar MapApertureもMdlOffset0/ByteOffset0/丸めextent完全一致へ制限した。ops v2 bind callbackのbytesは有効画像bytes、native glueはROUND_TO_PAGES(bytes)をE APIへ渡す。800×600ならactive 1,920,000byte、full mapping 1,921,024byte（469pages）。勝手に別PFN・別private outputへ置換しない。

IRQ receiptはcompletionではない。control_v2のinterrupt_eventにSTATUS_PENDING＝実sourceをclaim/mask済み・完了なしを追加し、service(cookie,epoch)を追加した。pendingではinitialized event/current epoch/job0/visibility0を返す。sidecarは実DPCからoperations rundown参照を取得してIoWorkItemのPASSIVE serviceへ渡す。queued workerも参照を保持し、stop/reset/powerは実rundown待ちする。service成功をGeoCompleteFromInterruptへ変換しない。後続の実HW IRQでだけGPU_DONE＋CPU_VISIBLEを配信する。v2はまだ実provider binaryへ公開しておらず、sizeofによる版固定を要求する。

E coreの共有header/APIは手作業で再定義せず、factory private TUでそのままincludeする。現E coreは低水準MDL/SGと実IRQ mask/ack/rearmであり、同じMDLへのClear factory、任意色/寸法、GP/SLC/cache後の実IRQ完了、quiesce/reset proofはまだ未接続。GEO_OWNER_REQUIRED/GEO_PROOF_REQUIREDを先に満たしたことにしない。現在はcombined module compileの段階で、標準D3D実機用combined SYSの最終link段階ではない。

## apertureとops版の更新

現factory出力は`geo_native_binding.version=2`。新`aperture.h`のGPU VA base/bytes/commit limit/highest DMA addressを実native ownerから返す。値を推測して埋めない。sidecarはPAGE_SIZE整列、ARM32 extent、範囲overflow、重複mapを検査し、VidMm指定のGPU aperture offsetをbindの末尾引数でnativeへ渡す。`geo_owner_ops.version=GEO_OWNER_OPS_VERSION(2)`を必須化した。旧v1 bind署名は拒否する。controlのInterface.Versionは2のまま、sizeとservice callbackも厳密照合する。

新bind署名: `bind(context,surface,locked_mdl,mdl_byte_offset,active_bytes,aperture_byte_offset)`。MDL offset0/full rounded extent、destinationは実GPU baseに相対。nativeはこの指定範囲へ同じPFNをredirectする。任意の別GPU VAにmapしてVidMm segment addressを無視しない。

`QueryAdapterInfo`は実apertureを元にQUERYSEGMENT/QUERYSEGMENT3と、公開adapter_info.h形式のmemory/protocol snapshotを返す。CpuVisible/CacheCoherentのsegment flagは立てず、CPUはVidMmのcached backing storeをLockで使う。allocation側Cached/CpuVisible/SynchronousPagingはMicrosoftのbacking-store/実fence待ち契約に基づく。SLC/GP完了と元MDL cache処理は引き続きnativeの実責務。GeoBuildRequiredDriverCapsと実PreemptCommandはWindows frontend側で実装済み。実frontend/shader/capsは未接続で、native factoryへWindows caps偽装を求めない。

## 結合時の固定確認

- `owner_abi.h`: `GEO_OWNER_OPS_VERSION=2`。bind末尾の64bit aperture offset追加を古い関数pointer castで回避しない。
- `native_binding.h`: factory出力version=2。`aperture.h`の実reserved GPU window/commit/highest DMA addressを返す。
- `owner_control.h`: Interface.Version=2、厳密Size、STATUS_PENDING receiptとPASSIVE serviceを含む。service成功はDMA完了ではない。
- 1 native DMA pairずつpublishする。未publish jobはowner tokenを保持し、preempt後の新nonpaging fenceで再submit、またはPASSIVE discardする。実completed/cancel/drain済みgenerationのみretire。quiesceは未publish tokenも失効させる。
- `umd/bootstrap.h`の`geo_ge8300_frontend`は別の実shader/state/format実装。factory完成やkernel link成功だけでFL9_1を広告しない。

## Hの実MDL pin/resolver入力

最新factory呼出しはGeoNativeOwnerCreateV3へ変更（旧4argとの誤linkを拒否）。frontend_memory.hのversion1入力を渡す。出力binding/control/opsはversion2のまま。具体的なresolve-owned-reference→native acquire→resolve releaseと、独立PFN lock/解放責務はFRONTEND-MDL.mdが正本。EはCreate/Delete/native allocation/control/stop-rundown、Hはactual VidMm resolver/独立物理lockを実装する。
