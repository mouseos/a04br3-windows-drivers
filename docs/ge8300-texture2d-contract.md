# Texture2Dの独立実装（325基点、未公開機能）

基点ecea76ecはnative322+viewport325/KMD320/UMDshader323。324 laneは不採用。
既存325署名package、121媒体、main/Mencius treeを変更しない。

一次sourceはImagination Mesa251b89ed9f7d501cea150591237d47dcc5cec0d9の
rogue/texstate.xml、pvr_arch_tex_state.c、pvr_arch_sampler.c、pvr_arch_formats.c。
BGRA8はU8U8U8U8、swizzle(2,1,0,3)、STRIDE4、row strideはpixel数-1。
image VAは4byte単位、40bit範囲。BGRA8 alphaはMSB。1mip/1layer/1sampleだけ。
POINT+CLAMP samplerはnormalized coords、U/V/W CLAMP_TO_EDGE2、min/mag/mip POINT、
LOD biasのzero encodingは4095。1mip用LOD範囲は0。
同BVNCのge8300.hにはBRN51025が無く、別IPのLOD+0.5 workaroundを持ち込まない。

native_texture2d_state.cはこのdescriptorを実生成する。範囲と乗算overflow、extent、
padded pitch、GPU address alignment/boundsを検査し、失敗時outputを変更しない。
初期実装限度128x128/64KiBはcapture/native backingを有限にするための選択で、GPU能力宣言ではない。
ASAN/UBSAN hostで16384 extent・padding・BGRA swizzle・無変更拒否を検証。

このmodule単体は実backingをacquireせず、GPU submissionも行わない。
現在のdraw ABI1/2にはSRV payload/owner参照が無く、native_draw_jobは有限tokens/USCの一致を要求する。
既存default geo_sm4_decodeはSAMPLEを拒否し、UMDもSRVを受理しない。これをsilent successにしない。
SAMPLEのcompiler接続、typed SRV/resource/sampler保持、descriptor shared配置、新versionを全消費側へ
同時検証するowned job contractが未完。単体descriptor成功を標準Texture描画成功と扱わない。
