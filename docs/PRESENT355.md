# 355：標準DXGI Presentのruntime境界を実装

354/175の有限標準D3D texture読戻し成功を基点とする独立branch。実機未投入。通常Windows/DWM画面、物理表示、任意shader/texture、長期安定、電源9Fは未解決。BT2018/provider1027の標準radio成功を保持し、追加BT再試験を行わない。

## 実装と所有権

CreateDeviceで入力DXGI_DDI_BASE_ARGS.pDXGIBaseCallbacksのpfnPresentCbだけを保持する。出力function tableをcallbackと誤認せず、optional overlay fieldを読まない。

typed_dxgi.Presentは、既deviceに属する非mapped BGRA8 Texture2D render targetを実geo_typed_allocationでkernel allocation handleへ翻訳する。optional destinationも同device/format/寸法を照合する。既frontend_flushを先に呼び、既pfnRenderCb送信済みcommandのerrorを伝播した後、同runtime device/contextと未変更のopaque pDXGIContextをpfnPresentCbへ渡す。broadcastは0。callbackの失敗を成功へ変換しない。

callback再入中のC resource lifetimeをref/unrefで保持し、同deviceの再入PresentはWAS_STILL_DRAWING。非同期DMA lifetimeとfenceはOS/KMDのallocation/job refsが所有し、callbackから帰ったことをGPU/表示完了としない。CPU lock/readback/copy、idle待ち、scheduler completion合成は行わない。flip/stereo/rotation/subresourceは拒否する。

このUMD境界は表示機能の広告とは別であり、render-only/source0/child0/cross-adapter無効を保持する。primary/shared resource作成やSetDisplayModeを成功扱いにしない。現KMDのPresentはNOT_SUPPORTEDなので、runtimeが同経路へ進んでもそのエラーをそのまま返す。これは配備用display packageではない。

## 単一adapterを選ぶ根拠と残る実装契約

現在のDOD1.7(MOU0007)はPOSTをAcquirePostDisplayOwnershipで所有してCPU PresentDisplayOnlyする。render側GPU0は独立owner/IRQとapertureを持ち、共有primary/DDIを持たない。CrossAdapter capsだけの有効化や旧別CDO GPUcopyの復活では、正規なshared allocation/fence/scanout lifetimeは成立しない。

次の実装は既unified_display bridgeの同adapter化を利用する。同じGPU miniportでBDD由来のVidPn/POST/child ownershipを保持し、OSが選んだprimaryの実backingとGPU transfer destinationを同一にする。DODとの二重POST ownerは許可しない。表示完了はreal GPU transferとscanout所有権の返却後に通知し、独立DPCやCPU blit後付けfenceを作らない。

現時点の具体的な未接続点：

- KMD GetStandardAllocationDriverDataはSHADOW/STAGINGのみでSHAREDPRIMARYSURFACEを拒否。SetVidPnSourceAddressに結合する標準primary backingがない。
- 実POSTは800x1280/pitch3200/4096000B(1000pages)。現native target/copyは800x600/469pages、aperture4MiB/1024PTE。primaryを追加すると同時resident backingが収まらない。native graph/storage/PT/viewportとVidMm segment契約を一組で拡張する必要がある。
- POSTのphysical addressをDMA logical addressとみなさない。正規なbacking/mapping lifetimeを成立させ、Stop/Reset/POST releaseは未完DMAをdrainしscanout参照が退役してから行う。
- WDK8.1 DXGKARG_PRESENTはpAllocationList/pAllocationInfoのunionで、現行文書はpAllocationInfoとpatch fieldsを予約扱いとする。旧試作のsource1/destination2 list解釈をRT8.1 ABI確認なしにtableへ登録しない。
- Present bltだけではprimary作成/共有open/RotateResourceIdentities/SetDisplayMode/VidPnとmode agreementは完成しない。

## 検証と再現

`tools/ci/build-ge8300-present355.ps1` は保持済み337 UMD22member/独立entryとcompiler10libs/pco_pairをhash照合して使う。clang -Mの実include closureでtyped_device.h依存TUを選び、そのTUだけARM32 compileする。未影響archive member、PCO、SDK、KMD354、FW、client9934を再buildしない。DLLは独立署名するがINF/CAT/媒体は生成しない。

`tests/ge8300_dxgi_present_test.c` はproduction typed_dxgi.cを実行するWindows x64境界test。allocation/runtimeだけmockし、handle翻訳、flush順、callback status伝播、再入、mapped/deleted/foreign相当の拒否、ref balanceを検証する。mock S_OKは実GPU/画面成功の証拠ではない。

## 一次契約

- WDK8.1 dxgiddi.h: DXGI_DDI_BASE_ARGS、DXGI_DDI_ARG_PRESENT、DXGIDDICB_PRESENT、PFNDDXGIDDI_PRESENTCB。
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dxgiddi/ns-dxgiddi-dxgiddicb_present
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dxgiddi/nc-dxgiddi-pfnddxgiddi_presentcb
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_present
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_present
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkcb_acquirepostdisplayownership

実機はmain専有。固定354/175を変更せず、本branchの動作成功を未検証のまま広告しない。
