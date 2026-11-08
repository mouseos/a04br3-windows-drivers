# 実RTから実stagingへのTQ copy（作業中）

現332正本は不変更。335全allocation Lock契約修正と別branch。

DODの`vendor_m1_11/windows_render_clear.c`はDM3 TQ task208byte＋client UFO updateを実KCCBへpublishし、TQ read/write/UFO一致、SLC flush、GP fenceを検査する。`windows_copy_scratch.c`の固定scratch32x32およびwide-frameは実copy資産であるが、通常render_ownerのsource/destination VidMm allocationを自動で所有しない。render_owner側snapshot宣言だけを実transferと扱わない。

## 実装済み

`render_owner/native_copy_job.{h,c}`は実TQ direct-copy record52DWORDと実source texture descriptor、destination PBE/EOT PDS、secondary PDSを構築する。sourceとdestinationはcallerの実mapped allocation VA/extentを取り、4MiB aperture内・全sample/write範囲・非重複を検証する。800x600以下、BGRA8/native22、同pitch、single-sample/single-mipの整数origin copyに限定する。sourceをCPU Lockしてmemcpyする処理は無い。

register encodingは既存DODのnative_copy_state/clear_image/wide producerを再利用する。copy用USCは既存152B assetのidentity検証を必要とし、任意D3D shaderを固定USCへ置換する経路ではない。assetの新規生成や推測bitを加えていない。

host回帰は800x600の非正方形clip、実VA binding、allocation overlap/extent不足/overflow、PDS span衝突を検査し、32x32では旧DOD snapshotと全52DWORDを照合する。ASAN/UBSANで成功。これはtask構築の検証であり、GPU copy実行成功ではない。

## 未完・次の結合

- source/destination双方のscheduler allocation referencesとcopy packet/wire version。
- residency確立後の実MDL/SG所有期間、PT bind、immutable code/task staging。
- 既存単一render ownerのDM3 submit→実IRQ→PASSIVE SLC/GP→SG返却/CPU ownership→新IRQ fence通知。
- staging Mapのreal LockCb、およびDestroy/unmap/timeout時retain。

Menciusが`native_owner_mdl`のSG所有権/Ex同期/uniquecontext/PT再bindを実装中。旧`sync_for_cpu`のKeFlushIoBuffersだけをDMA所有権返却に見立てて、このcopyを接続してはならない。現branchでは該当fileを変更していない。相手側の確定APIへ実leaseを結合してからwire consumersを同時version検証し、署名候補にする。現時点ではruntime未結合・未署名・未実機、caps不変更。未完成をrootfixとして媒体へ混載しない。
