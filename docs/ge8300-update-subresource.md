# DEFAULT Texture2Dの標準UpdateSubresourceUP

基点804d48c7 (Mencius UItexture)。既CreateResource initial upload/Mapは存在するが、通常pfnResourceUpdateSubresourceUPは未登録だった。DEFAULT・single mip/array/sample・BGRA/RGBA・exact SRV bindへの標準callbackを追加する。Create/SRV descriptor/layout/caps/native/KMD/FWは変更しない。

Box NULLは全面、非empty boxはboundsとfront0/back1を検証する。標準empty boxはメモリアクセスもLockも行わない。sourceはbox先頭texelから始まりdestination offsetを加算しない。source pitchはdestinationと独立、最終行の実byte数までchecked64bit計算しARM32のSIZE_MAXとpointerwrapを検証。1行では未使用pitch0を受理、2Dのdepthpitchは無視する。backend actual format/extent/pitch、Lock実RowPitch/DepthPitch、既4MiB容量を確認し、行paddingとbox外を保存する。mip/array/MSAA/RT+SRV/非DEFAULT/非zero CopyFlagsは未対応を正しく通知し、追加capを広告しない。

GPUとの同期は既frontend Flush→VidMm allocation Lock(ReadOnly0、LockEntire、IgnoreSync/Discard無し、既DonotWait bounded poll)→CPU upload→Unlock。Drawは毎回RenderCbへ即時提出しtextureをread allocationとして登録するためLockは実in-flight read完了まで書込pointerを返さない。既30秒timeout/デバイスerrorを維持し、偽fence/rename/無同期書込はない。Unlock失敗時はmapped状態と参照を保持してdevice errorとし、曖昧なallocationをdraw/destroyしない。backend Lock成功でpData NULLという異常応答でも実Unlockし、失敗ならlocked状態を保持する。

本backendに独立したuploaded/cache flagはない。textureはretained VidMm MDLの同じ物理pages。native windows_native_execute.cは実前frame/SLC/GP完了後mdl_cpu_complete、次jobのpublication前にmdl_gpu_prepare→全linear bindingのKeFlushIoBuffers(FALSE,TRUE)+barrierを行う。この既cacheownership経路を保持し、2回目更新を古いshadowへ置換しない。新PMIC/MMIO/physical owner/leaseを作らない。

一次資料: WDK8.1 d3d10umddi.h PFND3D11_1DDI_RESOURCEUPDATESUBRESOURCEUP (8args/CopyFlags)、d3dumddi.h LOCK callback。Microsoft APIはsourceをreturn前にcopy/snapshotすること、emptybox no-op、source/destination row pitchの独立性を規定する。DDIはruntime ErrorCb経由の実失敗を返す。

- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11_1ddi_resourceupdatesubresourceup
- https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-updatesubresource
- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3dukmdt/ns-d3dukmdt-_d3dddicb_lockflags

production callback/Map/Unmapの22caseを1batch ASan/UBSan実行。runtime/WDK型はhost seamで、GPU成功ではない。実WDK8.1 ARMではtyped_resource.c/clear_ddi.cの2TUと実includeclosureをcompileし、他20UMD objects/entry/PCOを既署名UItexture artifactからpin再利用、UMD link/BCBF署名を回収する。KMD/native/SDK/PCO再構築0。runtime-only patch/2objects/lib/署名DLLを後段forward branchへ結合し、最終SYS/UMD/INF/CAT tupleは統合担当が一度生成する。旧359/3.61 packageへの丸戻し・新DLLと旧CATの混載は禁止。

185固定物/clock/実機は変更0。通常DWM/任意shader/実GPU更新結果は未検証。次の通常一括clientはDEFAULT CreateResource→初回Update→SAMPLE→同resource再Update→SAMPLEの期待画素を同bootで照合する。Menciusのlayout9TU/forward/clientとは本callback追加2箇所だけを限定合流する。
