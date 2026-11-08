# VS constant buffer / PDS common store 後続330

329の署名packageと327媒体は凍結し、別branchで実装。

一般shaderの次の実blocking条件はVS constant buffer。world/view/projectionの典型的な4行DP4変換は、source compilerが実USCへ変換できても329ではnativeがVS shared領域を拒否していた。

330はVS/PSのCB0先頭4vectorを実captureし、VS CBをnative suffix0x540..0x57fへ独立copy。元1ページ4096byteのsuffix内で、PS CB0x4a0..0x4df、RT/EOT descriptor0x500..0x51fと非重複。draw job dataを0x580へ拡張。旧ABI1/2/3の値・copy箇所と所有権は変更しない。

新vertex PDSはDOUTDで共通storeへCBをDMAし、既存DDMADで実VBをunified storeへDMA、DOUTUで実USCを起動する。CB DMAはLASTなし、最後のVB DMAだけLASTあり。DMA sourceはnativeが構成する現在のowned suffix VAであり、shader/witnessからaddressを受けない。

一次source:

- Mesa `pvr_rogue_pds_defs.h`: Vertex Shader programs use DOUTD/DDMAD then DOUTU。DOUTD SRC1 DEST_COMMON_STORE bit28、AO13、burst DWORD0..11、LAST31、SRC0の40bit GPUVA、DOUT src1 shift16/src0 shift8。
- Mesa `pvr_rogue_pds_encode.h`: `pvr_pds_inst_encode_dout`。
- Mesa `pvr_pipeline_pds.c`: `pvr_encode_burst` はcommonstore DMAのsize/offset/lastとDOUT命令を構成。
- `rogue/vdm.xml`: VDM_STATE5 common store単位64byte、unified/PDS temporary/data単位16byte。VS CB16DWORDへcommon64byte、PDS data48byteを明示配分。

PDS constantsは0x280..0x2af、code0x2c0..0x2cfへ配置し、既存first-task(0x100/0x120)、fragment(0x180)、EOT(0x300以後)、coefficient(0x380以後)と重ならない。VDMの実vertex-task data/code pointerとdata/common sizeを同時に更新する。最終DMA終了/USC実行条件を省略しない。

4行DP4は実生成USC368byteとなり329の192byte域を越える。既存USC owned page1枚4096byte内に、VS0x400..0x5ff、PS0x800..0x9ffとして各512byteへ拡張。背景LOAD/EOT0x200以後の旧codeと非重複。最終instructionがpage末尾26byteへ入らないことも範囲上保証する。GPU VA/PTE/IRQの配置は不変。

semantic ISAを0x33001、adapter contract6へ進め、329peerはversion/ISA不一致で拒否。draw layout4、texture ABI3/profile4、owner/control2、MDL resolve1は保持。caps追加無し。

resource lifetimeは329と同じimmutable capture、real target/VB owned MDL、Patch prepare、Submit/IRQ/fence/retireを維持する。native stack/heap payloadサイズはsizeof依存closureの再compileで揃える。

検証は実4CB行DP4 VS368byte/PS104byte→ABI4→native copy/PDS構築、全USC改変・不足temps・禁止opcode/ISA/aliasの拒否、ASAN/UBSAN。CB値はGPU計算の代わりにCPUで頂点へ適用しない。元入力と実CB bytesがGPUへ渡る。GPU実行は未検証、327と329は混載しない。

残制約: CB0の4vector上限、有限float32 opcode、128命令/512byteUSC、POSITION4、800x600、一般varying/任意texture合成/depth/blend/scissor/Present等。Windows常用描画完成とは扱わない。
