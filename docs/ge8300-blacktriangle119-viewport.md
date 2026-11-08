# run119 black triangle: PPP viewport DWORD order

run119は標準2色Clear/readback成功、DrawRender/Copy/Map/RemovedReason0だが全480000pixel黒。
native R23 prepare0/GOD1、R39 jobCOMPLETE3/dispatchCOMPLETE5の主担当receiptを受領。
実送信無しやtimeoutとは扱わない。3.24 lane修正は採用しない。

一次sourceはImagination MIT Mesa `src/imagination/vulkan/pvr_arch_cmd_buffer.c`。
`pvr_setup_viewport` は a0=x+width/2, m0=width/2, a1=y+height/2, m1=height/2,
a2=minDepth, m2=maxDepth-minDepth とする。`pvr_emit_ppp_state` は
**a0,m0,a1,m1,a2,m2** の順にDWORDをemitする。csbgen XMLにはこの6wordの定義を追加しない旨も明記。
<https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/imagination/vulkan/pvr_arch_cmd_buffer.c>

現helperはこれと逆のscale,offsetを返してnative_draw_job.data+0x274へそのまま書く。
Xは400/400なので隠れる。D3DのY反転ではoffset=-300,scale=+300と誤解され、
client実頂点(-.8,.8,.5,1),(.8,.8,.5,1),(0,-.8,.5,1)はY=-60/-60/-540となる。
Zもoffset1/scale0となる。修正後はX80/720/400,Y60/60/540,Z.5。
これはsource/host上で画面外に送ることを再現できる欠陥であるが、実機修正成功は未検証。

native_d3d_viewport.cだけのnative1TU差分。公開helperコメントも実PPP orderへ訂正。
構造体・関数signature・wire ABI・caps・client・timeout・DMA/fence/IRQ・reset意味は変更しない。
native_draw_job/linked pathとも同じhelperを使うので同時に修正される。旧SM2 TL経路はhelper未使用。
固定native1x1/triangle suite自体は変更しない。

追加観測はUMD typed_draw.cの初回flat drawだけ。3頂点各4float、PS CB0の4word、viewport6word、
PS constant maskを実値のまま記録する。InterlockedCompareExchangeでmodule内1回に制限。
アドレス・個体情報・固定期待値の代入はない。追加GPU描画/試験を行わない。

`tests/ge8300_ppp_viewport_order_test.c` はproduction helperと実client頂点を使用。
修正前にY全負を再現、修正後に三角形が画面内になることと4096矩形/odd half、無変更拒否を
ASAN/UBSAN付きhostで確認する。GPU raster成功の代替ではない。

独立候補3.25は署名3.23のUMD archiveからtyped_draw1memberだけ更新、
実3.22 native archiveからnative_d3d_viewport1memberだけ更新する。
KMD320 archive・残るnative322 member・PCO/NIR/CRT/pair・FW/clientを保持。
`build-blacktriangle325-incremental.ps1`/`package-blacktriangle325.ps1` が固定hash/許可source差分を検査する。
fullsource323のorigin gateは原本を変更しないため本deltaを拒否する。公開fullbuildへの将来統合は
別途、deltaのsource出自を明示して行う。本incremental helperは原本gateの代替として一般許可せず、
固定10430631からの上記3runtime fileと専用test/doc/helperだけを許可する。
