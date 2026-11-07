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

opsは既存v1 `geo_owner_ops`をleaseごとに返す。`acquire`が成功しても返却値不正ならcookieを失わずpinし、無根拠にreleaseしない。失敗したbind/submit/reset/powerはpoisonを維持する。quiesce/reset proofのepoch/flagsが不足したらsidecarは再開・解放を拒否する。

## IRQについて決定が必要な点

現在の一次source `vendor_m1_11/windows_irq.c`はWdfInterruptCreateで実IRQを所有し、ISRでmask/ack、DPC→workitemでFWCCBを処理する。既存の同期pollやPASSIVE work成功を、そのままDxgk ISR完了へ変換できない。

v2の`irq_route=GEO_IRQ_DXGK_REGISTERED`は「render miniportの本物のDxgkDdiInterruptRoutineへIRQがPnP上正しく割り当てられ、低水準event処理をownerへ委譲できる」ことを意味する。既存WDF ISRと別PDOで同じIRQを二重所有しない。物理FDOをrender miniportへ移す／ownerをkernel内部serviceへ分離する等の正式な構成決定が必要。現providerを保持する場合の別ISR→Dxgk callback転送を公式に支持できる根拠があれば、合意してABIを改訂する。IRQL一致だけを根拠にその経路を有効化しない。

CPU可視性の成立にPASSIVEのSLC/FW処理が必須なら、interrupt_eventだけでCPU_VISIBLEを返せるかも未確定。この場合は真の後続HW割込を伴う完了設計か、公式WDDMに沿う別経路を先に決める必要がある。偽interrupt/poll完了は使わない。

## owner側へ依頼する回答

同一MDL mappingとNormal CPU/cache契約の実装可否、IRQ topologyとCPU_VISIBLE成立IRQL、quiesce/resetの5proofを満たせるか、retained surface/bindingとepochの扱いを回答してほしい。満たせない操作の具体的欠落を返し、常時拒否stubだけの実装を成果としない。実GPU candidateの既知成功を標準D3D成功とは扱わない。

## 単一miniportへの方針更新

主担当/Einsteinの合意で、MOU0006に既存KMDF providerと排他的に適用するrender miniportへ変更。native coreを同じSYSへsource-linkし、DxgkだけがIRQを所有する。既存KMDF candidateは復帰baselineとして保存、MOU0007 DODは非変更。したがってQueryInterfaceは最終必須ではない。`native_binding.h`のGeoNativeOwnerCreate/Deleteでcontrol_v2 tableを内部取得し、GeoStartInProcessOwnerSessionがInterfaceReference/leaseを保持する。別provider PDO検索・別FDO・WdfInterruptCreateは使わない。

native factoryに渡すのは実PDO、StartDeviceのDXGKRNL_INTERFACE、DxgkCbGetDeviceInformationが返す実資源。factoryはIRQを接続せず、acquire後もsourceをmaskしたまま、sidecar通知初期化後のrestartで有効化する。削除は実lease解放後のみ、失敗ならbase owner contextを保持する。GeoNativeOwnerCreate/Deleteは現時点でnative担当が実装する未解決symbolで、fallback定義はない。

sidecar側はDriverEntry/Start/Stop/Remove/ISR/DPC/power/TDRと、実runtime contextを使うUMD初期化を追加した。残るgeo_ge8300_render_contractは**このWindows frontend担当が埋める**mandatory render DDI/caps/segment/preemption契約で、native担当へWindows caps偽装を依頼するものではない。geo_ge8300_frontendも必須shader/format/stateが実装できるまで定義しない。いずれも未解決symbolを残して最終linkを拒否し、空の表や成功stubで代替しない。
