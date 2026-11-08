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

## 別入口のSM4 SAMPLE / PCO producer

texture2d_sm4.cは、PS4.0・CB0からUV・t0 float2D・s0通常sampler・SAMPLE→o0→RETの
明示subsetだけをdecodeする。LOD/comparison/offset/array/MSAA/flow/relative index/入力varyingは拒否。
旧geo_sm4_decodeは一切変えず、324 laneも混ぜない。default UMDはこの入口を呼ばない。
pco_texture2d.cはその実tokenからNIRを作り、PCOのpco_emit_nir_smpへ渡す。
preloaded sampler sh0..3/image sh4..7、CB vectorはsh8以降。texture GPU VAをUSCへ埋め込まない。
実SMP結果をBGRA8へpackし、shaderを置換しない。旧compiler ABI構造体は変更しない。
このproducerはARM compile検証対象であり、PCO codegen実行/標準DDI/実GPU samplingは未検証。
全consumerを新owned texture jobへ結合するまではruntime/capsを開放しない。

## codegenとnative admission

既存Linux host compiler（Mesa0d94b3f3）を読み取り専用で再利用し、消費する主要PCO/NIR/headerが
Windows source snapshotと同byteであることを確認。library再build無し、新adapter6TUだけで
実codegenに成功（USC88byte、temps0、shareds12、CB0mask1）。ARM3TU compileと既存実PCO/NIRへの
linkcheckも成功。ただしARM上のcodegen実行/実GPU samplingは未検証。

native_texture2d_program.cは、その出力を**比較専用の検証witness**として保持する。
実tokenを再decodeし、初期範囲CB0.xy/t0/s0、metadataと実USC一致を確認する。
別shaderの命令をこのwitnessに差し替えて描画してはならない。将来のsubmitでは一致検査した
入力USC自身をcopyする。arbitrary USCや未実装memory opcodeをnativeへ通さないための有限admission。
現在のABI1/2 prepareには未接続であり、標準Texture対応を宣言するものではない。
