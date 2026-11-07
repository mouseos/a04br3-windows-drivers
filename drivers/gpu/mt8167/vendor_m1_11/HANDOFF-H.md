# H / E 最終native bindingの実装契約

H固定source: f492756aa51e6a19527b5d2ca65526a4d1c640dc。
Eのresolver実配線: 501083b、停止/IRQ候補を含む検証済みcheckpoint: 780b7c7。
共有repository: https://github.com/mouseos/a04br3-windows-drivers.git
E branch: work/ge8300-native-render-packet。
H branch: work/ge8300-wddm-render-sidecar-20261107。

## 分担（未実装をHへ委譲しない）

E: GeoNativeOwnerCreateV3/Delete本体、native allocation/queue/prepare/publish、
control-v2全table、実IRQ/visibility、停止/reset/epoch/rundownと失敗時資源保持。
H: 最終factory呼出し、実VidMm allocation/MDL resolver、frontendのadapter寿命、
UMD/PCO/NIR/draw stateおよびscheduler通知/miniportの実Dxgk IRQ登録。
GPU IRQはDxgkだけが所有。KMDF baseline providerを同時接続しない。

## 固定factory ABI（H native_binding.hが正本）

```c
NTSTATUS GeoNativeOwnerCreateV3(PDEVICE_OBJECT,
    const DXGKRNL_INTERFACE *, const DXGK_DEVICE_INFO *,
    const struct geo_frontend_memory *, struct geo_native_binding *);
NTSTATUS GeoNativeOwnerDelete(struct geo_native_binding *);
```

binding/control/owner_opsはversion2、frontend memoryはversion1。
Create成功後にcontrol.acquireでleaseを取得し、実IRQ通知初期化後restart。
partial Create失敗で保持が必要ならbinding.native_contextを残す。
Delete/release失敗ではそのcontext、callback、adapterを解放しない。
Eの現checkpointにはこのfactory symbol/table全体はまだ無い。
成功stub、unresolved無視、cold資源成功をGEO start成功へ置換してはならない。

## H resolverから実bindingまで（501083bに実装済み）

```c
NTSTATUS a04_native_allocation_bind_frontend_cold(
    struct a04_native_owner_allocation *owner,
    const struct geo_frontend_memory *memory,
    PMDL original_vidmm_mdl, ULONG active_bytes,
    uint64_t aperture_byte_offset, uint32_t packed_argb);
```

profileは800x600/pitch3200/active1920000/page_bytes1921024。
resolve成功のsize/version1/originGEO_MDL_VIDMM_CACHED_SHADOW/独立lease.mdlを検査。
lease.mdl（元VidMm MDLではない）を実OS SG/PFN/cache/target bindへ渡す。
native bindがlease.acquireで別参照を取得し、resolveの所有参照1個は成否ともrelease。
HのUnmapはmap参照だけをdrop。native参照が残れば独立pinを保持する。
元VidMm MDLのunlock/freeは禁止。unknown MDLやcache flagだけでは成功させない。
成功なのにrelease口が無いresolver結果はE ownerに保持してDeleteを拒否する。

Hに確認する寿命契約:
`memory_api.context`/callbackとadapter本体は、map参照が落ちてもnative参照・
failed ownerが存在する全期間、E Delete成功までresidentで保持すること。
確認/修正はこの固定ABIへ反映したcommitで返す。EはH待ちだけで作業を止めない。

## control-v2の必要な接続

- acquire: 排他的な実D0/IRQ resource所有、referenced lease/epoch。未資格capsを返さない。
- synchronize: 実DxgkCbSynchronizeExecutionに委譲。偽IRQL/別IRQ/常時TRUE不可。
- ops.create/bind/prepare/discard/submit: surface/job/tokenを元allocationへ結合。
  現実装のsubmitは実IRQ lock下のbounded publishのみ、acceptedとcompletedを区別。
- interrupt_event: 実ISRだけからnative dispatchを呼ぶ。未claimはNOT_FOUND。
  claimのみはPENDING（GEO event epochはlease epoch、job/generation/visibility=0）。
  実source＋後続GP fence＋cache proofの一致時だけGPU_DONE|CPU_VISIBLEを通知する。
- service: 単一PASSIVE workerでnative finish/cache→実IRQ同期内fresh GP arm。
  polling側からscheduler completionを通知しない。
- quiesce/reset/set_power/restart: FW off/IRQ counter drain/実HW停止、callback/work rundown、
  token retirement、epochを結合。mask/idle/FWackだけでGEO_PROOF_REQUIREDを返さない。
- release/Delete: live surface/job/binding/IRQ/work/interface参照が残れば保持してerror。
  exposed common-buffer/MDL/SGの解放は実停止とretirement後のみ。

Eの780b7c7は実ISR sampleのFW counter照合・OFF/queue空のsource close、
GPU0 scoped reset hold/readbackを追加したが、外部AXI write-response完了の契約が未確定。
NoDMA/全stop proofを広告しない。詳細はNATIVE_CORE_STOP_REVIEW.md。

## draw/shader semanticの現在範囲

owner公開操作の実装対象はpacked clear。固定triangle/texture producerはWindows実機で
solid→texture2job成功した独立経路で、動的VS/FS/vertex/constant入力を受けない。
PCOが生成した任意binaryを固定producerへ貼り付けるABIは存在しない。
Hの動的draw側は、そのsemanticと検証可能なresource/compiler出力契約を先に提示する。
Eがsource/同版一次根拠でPDS/USC/VDM/PPP配置・参照閉包・再配置を実装してから結合する。
固定capture VA再送、未取得zero補完、clearをdraw成功へ偽装することは禁止。

## build / 最終link

E helper: tools/ci/build-ge8300-native-owner-core.ps1
`-EnableCandidate -ExpectedSourceCommit <fixed> -WithMdlTarget -WithOwnerAllocation -LinkCheck`
780b7c7は実WDK ARM37TU（36TU検証再利用）＋kernel API LinkCheck成功。
H f492756はH報告で17 Clang TU＋Microsoft ARM SEH mdl_pin成功。
H SEH TUは正式Microsoft ARM compiler、他TUは既存LLVM/WDK契約。
最終同一SYSでのfactory/full-control linkは未実施。既存copy/DOD public ABIは変えない。
モジュールlink成功は実GPU IRQ/WDDM/D3D成功ではない。
