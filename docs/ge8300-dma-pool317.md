# RT8.1 DMA pool契約修正（3.17）

run101の10E/16は、実RT VidMm AcquireBufferのpool要求容量と実buffer容量の不一致分岐で発生した。公開symbolとREAで対応を確認したAddBufferはallocation-list非ゼロ容量1/2を3へ補正する。ContextInfo.AllocationListSize=2はこの最小容量と一致しない。3を要求し、PatchLocationListSize=2とDmaBufferSize=GEO_DRAW_LINK_COMMAND_MAXは保持する。DMAページ整列による不一致という仮説は撤回済み。

DXGKARG_RENDERのAllocationListSizeは使用数でなく利用可能容量である。clearはentry0だけ、drawはentry0/1だけを解決し、余剰entryを走査しない。Patchもpatch数1/2に対する必要entryの範囲だけを確認する。必要数未満は従来どおり拒否する。allocation番号の再割当や予約entryの新設はしない。公式資料はNULL unbind時のentry0利用を述べるが、常にentry0を予約する契約とはしていない。

pDmaBufferPrivateDataは入力ポインタとして保持し、その先にticketを書く。DMAとpatchの出力ポインタだけを消費量だけ進める。private pointerの変更は今回のpool取得前停止の直接原因とは扱わない。

runtime差分はkmd_objects.c/kmd_clear.cのみ。native owner、MDL/PTE、FW、UMD、client、DMAサイズ、patch容量は変更しない。既存316 KMD archiveの2TUだけ再compileし、他memberはhashとmember数を照合、native libraryは全byte保持する。

根拠:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgk_contextinfo
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_render
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_render
- Microsoft graphics-driver-samples de4a2161991eda254013da6c18226f5ea06e4a9c RosKmdContext.cpp
- 実RT dxgmms1 AddBuffer RVA3e2a8、AcquireBuffer RVA3e7dc（main解析）。調査ログは指定ログ領域に保存しrepoへ含めない。

ホスト回帰は実CreateContext関数をコンパイルし、DMA/private/patch保持と実RT容量補正後の等値条件を検査する。署名ビルドはWDK8.1。実機での解消と標準D3D成功は別途mainの試験で確認する。

## 後続320による訂正（原記録保持）

上記private pointerを固定する判断は、実RT FUN5079cの返却cursor差分→submission private used経路により撤回した。
現在は両Render cursorを16bytes進める。容量3への修正と余剰allocation-list受入れは保持する。
詳細はge8300-private-cursor320.md。過去の署名317成果物を再生成した記録ではない。
