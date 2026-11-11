# GDIの割当・部分コピー契約

376のソースレビューで確認した契約上の不備をまとめて修正する。今回の実機client375のexit1との因果は未確認であり、通常Windows描画の成功を意味しない。

- GdiContextのAllocationListSizeはWDK8.1のDXGK_ALLOCATION_LIST_SIZE_GDICONTEXT（256）を使う。通常UMDコンテキストの既存容量3は維持する。PatchLocationListSizeやRenderKm対応は変更しない。
- 標準shadow/stagingのprivate-dataサイズ照会では入力descriptorのPitchを変更しない。必要サイズだけを返し、private-dataのfill時にPitchを返す。
- 平行移動の外側SrcRect/DstRectが画面端をまたいでも、OSが事前clipした各subrectと対応source範囲が有効ならコピーする。両実範囲と演算を64bitで検証し、未clip範囲・stretch・範囲外・overflowは拒否する。1Presentを1GPU jobとして追跡する既実装を維持する。

根拠：

- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgk_contextinfo
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_getstandardallocationdriverdata
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_present

ビルドには回収済み376のarchiveと既WDK/VC/LLVMを使用し、変更C translation unit 2本だけを置換する。native owner・UMD・表示transport・firmwareは保持する。署名済み候補の生成と実機確認は別の工程として記録する。
