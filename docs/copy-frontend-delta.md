# Copy frontend限定delta（merge owner Mencius）

基点0bfc91cf9a87c98b0ca1d540d00726159614072d、実装出典1a23664590825a73301f323bba28d7c67ab55d2d。mainの最新分担に従いnative factory/IRQ/scheduler/StopTDRは一切変更しない。51cbe2d4 owner最終契約を読取り、既存署名候補のfrontendだけを分離した。

## 最小consumer契約

- clear_protocol.h: GOP1=0x31504f47、geo_copy_packetは32B/version1。destination index0/write、source index1/read、800x600 BGRA8全extent/single mip/sampleだけ。
- owner_abi.h: ops version3/size厳密、prepare_copy(context, source_surface, destination_surface, out_job)。呼出しはPASSIVE、両surfaceはbound。成功はprepared予約であり完了ではない。submit/discard/completionは既存owner経路を使用。
- query adapter version8、owner3、control2を両UMD entryで照合。Mencius factoryは同じsize/versionとprepare_copy実体を提供する必要がある。旧owner2へのfallbackは無し。
- UMD bltはRenderCbへ実2allocation参照を渡す。RT CPU Lock/memcpyを使用しない。返却command/list cursorはcallback成功時のみ更新。staging MapはLockEntire/DonotWait/実callbackを保持。
- clear_engineはsourceとdestinationをjobのvertex/allocationへ保持。Render-before-residencyを許容し、未完ref中はunbind/destroy不可。KMD Patchで実2handle/権限/residency/context/ticket/kindを照合してprepare_copyを呼ぶ。
- nativeはsource/destinationのWDDM resident lease、実GPU所有、実IRQとvisibility proofを満たす責任を持つ。phase4だけでscheduler完了しない。偽fence/偽NoDMAを返さない。これらnative実装は本deltaに含めずMenciusが統合する。

## 検証範囲

元337署名候補で同一sourceのARM compile/link、production UMD mock callback回帰を実施済み。本deltaでは2ref/Render-before-residency/未完unbind拒否/完了後解放と既存clear/texture回帰だけをまとめて実行する。factory未結合のこの限定branch単独を実行packageとは扱わない。新署名/rebuildはmerge ownerの最終統合後に一回行う。
