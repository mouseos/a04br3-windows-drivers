# PowerVR GE8300 WDDM render sidecar（既定無効）

Windows RT 8.1 / ARM32 / WDDM 1.3向けの新規DDI実装部品です。run31のprivate GPU Clear成功を標準D3D成功とは扱いません。現在の成果物はKMD/UMDのstatic libraryと標準D3D clientで、インストール可能なrender miniport SYS / UMD DLLではありません。既存DOD、GPU provider、vendor_m1_11、shader/triangle/runtime、媒体生成には変更を加えません。

## 実装した経路

- `clear_protocol.[ch]`: 単一linear A8R8G8B8、最大1024×1024/4MiB。user packetにはallocation indexとpacked colorだけを許可。VA/PA/GPU command/shader blobを受け付けません。
- `clear_engine.[ch]`: 実ownerのcreate/bind/prepare/submit/completionを使う状態管理。prepareをPASSIVEと割込同期下の予約/確定に分離。slot世代、単調fence、古い完了の拒否、参照中解放拒否、障害時poison/pin保持を実装。GPU_DONEとCPU_VISIBLE両方の証拠が揃うまで完了にしません。fence wrap/reset、順不同完了は未対応です。
- `kmd_clear.[ch]`: CreateAllocation/DestroyAllocation/BuildPagingBuffer/Render/Patch/SubmitCommand/QueryCurrentFenceのDDI部品。VidMm allocation listでhandleを解決。DMA/private dataには世代ticketを置き、実ownerが準備済みjobをpublishします。アドレスを含まないためPatchはticket一致を検査。MapApertureのMdlOffsetはページ単位、MDL byte extentを検査。全mapと同じaperture offset/ページ数の全unmapのみ受け付けます。fill/transfer/discardは未実装として拒否します。
- `umd/clear_ddi.[ch]`: D3D9 DDIのresource/target/full Clear/Lock/Unlock/全域Blt readback。Allocate/Render/Lockは実runtime callbackへ渡し、GPUが書いた対象をruntime lockしてstagingへcopyします。CPUでClear結果を捏造しません。OpenAdapter/CreateDevice成功stubや架空capsを置きません。
- `owner_abi.h` / `owner_interface.h`: kernel専用の**未合意draft v1**。GUIDは本sidecar提案の識別子で、現providerが公開するGUIDではありません。PnP query成功やD0取得はまだ実装していません。

## 接続前に必要な実装（現候補の欠落）

1. miniportのDriverEntry/StartDevice/StopDevice、device/context/allocation rundown、segment/query-adapter/caps、paging、preemption/TDR/resetとPnP/D0所有権。PASSIVE資源操作はadapter寿命と直列化し、ISRのjob参照との二段階状態遷移が必要です。現DDI部品単独でこれを提供しません。DestroyAllocationエラーだけを根拠にVidMmやPnP removal後のページ寿命が保証されるとは扱いません。
2. UMD OpenAdapter/GetCaps/CreateDevice/DestroyDevice/CloseAdapter、実context作成/破棄と必須DDI（CreateResource2等を含むruntime経路）、必須shader/caps。ClearだけではFL9_1を満たせません。shader担当と実能力を揃えるまでfeature levelを広告しません。Microsoft RenderOnlySampleが示すhardware FL9.xのD3D9 DDI経由を参考にしていますが、WDDM版番号だけでD3D11 DDI対応を意味しません。
3. GPU ownerによる同一VidMm locked MDL/PFN backingの実登録。別の固定private outputに書いて成功とはしません。CPU側はNormal RAM mapping、GPU側は同じページへの正式mappingと実cache処理が必要です。Cached flagは属性の実証ではありません。Device memory共通バッファを一般のuser PCM/SIMD/unalignedアクセスへ公開しません。非page-aligned MDLと部分mapは現候補では扱いません。
4. ownerのPASSIVE prepareと、実interrupt同期IRQLで待たないsubmit。既存private同期poll APIをそのまま呼べません。新しいGeoInitializeClearNotifications/GeoCompleteFromInterrupt/GeoClearDpcが、実GPU完了＋CPU可視性確認後のgeo_complete→DxgkCbNotifyInterrupt(DMA_COMPLETED)、DxgkCbQueueDpc→DxgkCbNotifyDpcを実装しています。StartDeviceで実DXGKRNL interfaceと実割込IRQLを一度だけ渡し、GeoCompleteFromInterruptは本物のDxgkDdiInterruptRoutineからowner割込lock下で呼び、登録DPC wrapperからGeoClearDpcを呼ぶ接続が必要です。通知初期化前のRender/Submit/fence queryは拒否します。古い世代・障害・CPU可視性不足はscheduler完了へ変換しません。IRQL一致検査だけでは実ISR由来の証明にならず、手動で上げるだけではISRの代替になりません。DPC pendingはInterlockedで管理し、QueueDpcが既存queueを理由にFALSEを返すMicrosoft契約に従います。fault時のscheduler fault/TDR通知とadapter stop/resetはまだ未実装です。owner submit中の再入完了は禁止、tokenは未完了job間で一意、世代を完了へそのまま返す契約です。
5. 取消/未submit prepared jobの破棄経路とowner rundown。`geo_cancel_prepared`は実ISR/callback rundown後のPASSIVE teardownへ接続する必要があります。割込を動かしたままPASSIVE関数とISRの参照更新を並行させてはいけません。未証明の停止・ドレインではreleaseせず保持します。現状でsleep/removal/recovery対応を広告できません。

このため現provider/DODへINFでbindしません。ABIはEinsteinへの調整依頼をRamanへ送付していますが、provider側実装や合意済みとの報告はまだありません。

## WDK 8.1 ARM32ビルド単位

主担当の既存runner用です。この作業ではrunnerを操作していません。checkout内の新しい出力先だけを使います。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File drivers\gpu\mt8167\wddm-render\build-wdk81.ps1 -EnableCandidate -OutputDirectory build\ge8300-wddm-render
```

Clang/llvm-lib/lld-link/llvm-readobjとWindows Kits 8.1が必要です。KMD/UMD archiveと`ge8300-d3d-clear.exe`のみ生成し、source commit/dirty/source hash/output hash/importsを記録。親/USB/Audio/GPU providerを再buildしません。SYS/CAT/署名/配置は生成しません。出力内にtmpを限定し既定ではbuildを拒否します。kernel用にも使うTUはscalar/no-FP codegenを指定。archive成功は最終miniport/UMD linkの証拠ではありません。

## 標準D3D client

引数なしはDXGI adapter一覧（実description/vendor/device/LUID）をdisklogへ保存して非成功終了します。主担当が実PnP/DXGI対応を照合したGE8300 render adapterのLUIDを明示指定します。最初のadapterや名前部分一致、推測したPCI IDでは選びません。

```text
ge8300-d3d-clear.exe --luid=hhhhhhhh:llllllll
```

8桁hex high:lowのみ。D3D11CreateDeviceへ選択adapterとD3D_DRIVER_TYPE_UNKNOWN、要求FL9_1を渡します。WARP fallbackなし。256×256 BGRA8 render targetにmagentaとgreenを順にClearし、CopyResource→staging Mapで各65,536画素を検査。第二色も同じresourceへ出し、固定出力の読み違いを検出します。APIのHRESULT、Map直前、match/mismatch、GetDeviceRemovedReasonを保存。失敗をCPU描画へ迂回しません。現DODや未完成adapterではCreateDevice失敗が正しい結果です。

`C:\MT8167D3DClear\clear-<tick8hex>-<pid8hex>.log`をCREATE_NEW/WRITE_THROUGHで確保し、各記録をFlush。既存logは上書きしません。UARTは不要。任意ファイル/command/payloadを実行しません。exit0は両色のAPI readback成功＋disklog成功、1はAPI/選択失敗、2は必須disklog確保失敗、3は書込み/Flush失敗です。Mapが停止すればMAP_BEGINが最後に残り、成功とは判定しません。client結果だけでGPU実行を立証できないため、実owner fence/job/CPU可視性traceとの照合も必要です。providerのhang recoveryはまだ未実装で、現状の実機実行対象にはしません。

## 確認の範囲と一次資料

hostの単一`tests/lifecycle.c`でpending解放拒否、古い世代拒否、実完了flags不足時のpoison/pinをまとめて確認。Win32/DDI実headerによるUMDとclientのARM32構文確認・ARMNT COFF生成は通過。clientの未解決参照は標準Win32/DXGI/D3D11 APIと付属memory.cのmemcpy/memsetのみ。参照headerはWindows 10初期SDK mirror＋WinBlue version guardで、実WDK8.1ビルドの代用ではありません。host KMDのARM構文確認はMinGW DDKのarmddk.h欠落で停止。追加でx64 targetの実DDI/DDK header構文確認は-Wall/-Wextra/-Werrorで通過しました（MinGWにないSAL注記_Pre_opt_bytecap_だけを無効化、DDI型/field/関数signatureは改変なし）。このx64確認をARM32/WDK8.1成功とは扱いません。WDK8.1 compile/link、ARM mapping、実IRQ、標準D3D実機成功はいずれも未確認です。

- Microsoft [graphics-driver-samples RenderOnlySample](https://github.com/microsoft/graphics-driver-samples/tree/de4a2161991eda254013da6c18226f5ea06e4a9c/render-only-sample): ros UMD resource/context/DDIとFL9.x経路。参考読み取りのみ、sampleコードは複写していません。
- Microsoft [DXGKARG_BUILDPAGINGBUFFER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_buildpagingbuffer): MdlOffsetはページ単位。
- Microsoft [DXGKCB_NOTIFY_INTERRUPT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkcb_notify_interrupt): 実DIRQL通知の契約。
- Microsoft [DXGKCB_QUEUE_DPC](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/nc-dispmprt-dxgkcb_queue_dpc): 同一deviceのDPCが既にqueue済みならFALSEを返し追加queueしない。
- Microsoft [D3D11CreateDevice](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice)、[Map](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)。

ソースはBSD-2-Clause-Patent。一次資料・生ログはソースへ混ぜず作業ルートのport/gsi/logs/ios-feasibility/windows-rt-wddm-render-source-20261107/へ保存しています。
