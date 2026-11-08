# Texture2Dの限定実装候補（326、実機未検証）

実装基点はnative322＋viewport325、KMD320、UMD shader323。324 lane修正を混ぜていない。
325署名package・現在媒体・main/Mencius treeは不変。native IRQ/reset/CPU profile制御も変更していない。

## 現在結合した経路

- BGRA8 Texture2D、1mip/1layer/1sample、最大128x128・64KiB。
- D3D11 typed CreateResource初期データ／実Lock・Unlock、SRV Create/Set/Destroy。
- samplerは既存typed_stateの検査済みimmutable snapshotを再利用。PS t0/s0のみ使用。
  POINT＋U/V/W CLAMP、正規化UV、bias0、LOD範囲が0を含む設定のみ受理。
  未指定samplerはD3DのLINEAR既定値なのでPOINTへ置換せず拒否する。
- SM4 SAMPLE PSはCB0.xyをUVに使う実token→NIR→PCO出力。固定shader差替え無し。
  nativeはtoken/metadata/USCの有限admissionを再検査し、**入力USCそのもの**をcopyする。
- RenderはRT/VB/Textureの3個のVidMmハンドルを所有resourceへ解決し、参照を保持。
  未resident時はowned packetを保持し、MapAperture後のPatchで実native prepareする。
  再bind前は実prepared tokenをdiscard、submit中はunmapを拒否、実IRQ/fence後に参照をretireする。
- Textureは既存linear input bindingで独立pin済みMDL・SG・PTE・cache同期へ接続。
  staging/indexへの偽装、CPU私有texelのGPU backing差替え、第二IRQ/HW ownerは追加しない。
- nativeのshared0..3へsampler、4..7へimage、8..11へCB0を置き、DOUTDは12DWORD。
  既存16 shared allocation内に収まる。USC88byteは0xC0..0x117、背景USC0x200と重ならない。

## versionと拒否範囲

adapter private contract3、draw wire3、Texture storage2を両端で明示検査。
従来flat/link draw1/2とowner/control2、frontend resolve1は区別して維持。
古いadapter contractのUMD/KMDは新peerを無条件受理しない。

初期範囲外のmip/array/MSAA、別format、filter/address/comparison/LOD命令、任意USC、
varying UV、相対index、flow、RTとの別名を厳密拒否する。
通常FL10/FL9の全要件を満たしておらずlimited-development profileを維持する。
WDKのFORMAT_SUPPORT_SHADER_SAMPLEは「any filter」を意味するので、POINT限定対応だけで
そのbitを追加しない。標準profile資格や汎用アプリの描画成功を宣言しない。

## 検証・ビルドの区別

実source-compiled SAMPLE PS＋既存flat VS wire fixtureをABI3→production native builderへ通し、
提供USCがそのままjobへ入ること、sampler/version/VA別名の無変更拒否をhostで確認。
既存Linux PCO archiveはPS codegenのみに使い、VSの全metadata資格をこの試験から主張しない。
production clear_engineの3参照寿命試験はASAN/UBSANで検証。
production typed_texture＋typed_stateのWindows x64回帰はallocator/fence/lookupをmock化し、
SRVとsampler snapshotのDDI契約だけを検証する。GPU実行試験ではない。

増分build helperは325の全hashを固定し、native8TU、共有構造体の依存KMD12TU、UMD側23TU
（entryを含む）をcompile、残るnative50memberとKMDの実MSVC SEH mdl_pinを保持する。
既存PCO/NIR/CRT10lib・pair object・FW・clientを再利用し、SDK/PCO全再構築は行わない。
archiveの全leaf一意性・数・順序・保持member hashと、新compiled closureのsource hashを記録する。
SYS/DLL/CAT署名とCAT membership成功は実機Texture成功と区別する。

再現helper：`tools/ci/build-ge8300-texture326-incremental.ps1`、
`tools/ci/package-ge8300-texture326.ps1`。
通常cache無し325 fullsource手順は既存`work/ge8300-single-checkout325`に保持しており、
326の新overlay/recipeをその固定325 source gateが無条件受理するようには変更しない。

## producer段階の履歴（現在の結合以前）

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
