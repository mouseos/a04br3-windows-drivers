# D3D shaderと実PCO compilerの接続

5a26a0dの実WDK増分module/client再利用成功は主担当報告。0172810のnative triangle/textureはsource-defined NIRからPCOを使う固定800×600/private RGBA候補であり、Windows実機結果や一般D3D shader対応と扱わない。provider/vendor_m1_11は編集しない。

## 今回の実装

`compiler/sm2.c`がruntimeのD3D9 SM2 tokenをbounded decodeし、`compiler/pco_sm2.c`が実NIRを構築して同じpatched PCOのpreprocess/lower/postprocess/trans/process/encodeへ渡す。固定USC lookupやshader成功stubはない。PSはfloat4色をBGRA8へpack、VSはfloat4 inputからoPosへUVSW exportする。実PCOのcode/temps/shareds/vtxins/vtxoutsとinput semantic/constant maskを返す。

受け付けるのはversion VS2_0/PS2_0、straight-line MOV/ADD/MUL/MAD/DP3/DP4、vertex DCL、float DEF、swizzle/negate/saturate/writemask。temp32、float constants32、input16、最大128instruction/2048DWORD。未定義temp読出し、範囲外、relative addressing、predication/flow、int/bool、PS varying、TEXLD等は拒否。頂点宣言は全4成分、出力は全成分のoPosまたはoC0を必要とする。コメント長とENDも検査する。これはSM2完全対応ではなく、PS/VS version capsへ2_0を広告する根拠にはならない。

`umd/shader_state.c`は実compiler callbackを用いるCreate/Set/Delete VS/PS、float constant設定、現在shader/constant/元tokenのsnapshotを実装。bytecodeを自分のheapへcopyし、compilerが実生成したcode/storage/metadataを検査する。以前の状態を使う描画は実frontend Flushへ出してから変更し、失敗なら旧状態を保つ。削除済みでもbind中のshaderは置換まで保持。shutdownもFlush失敗なら保持する。初期float constantsは実zero backing。整数/BOOLや未実装draw/state DDIsは成功stubにしない。

既存`bootstrap.c`が要求する本物の`geo_ge8300_frontend`はまだ未定義であり、shader module単独ではOpenAdapterを成功させない。新stateは実frontendが `geo_umd_shader_init`/`geo_umd_install_shader_ddis` を呼んで使う。device初期化後のruntime操作ではbootstrapが実frontend Flushを設定している。teardownは実frontend quiesce内でshader_shutdownし、成功時だけdestroyへ進める。compiler contextは実deviceが所有し、compileを直列化する。

## Eへ必要な接続（現在のABIへ未追加）

| 項目 | 必要な実装・差分 |
| --- | --- |
| vertex input | 現EのRG32/input2固定から、宣言semantic/stream stride/offset/formatに基づくfloat4 input/PDS fetchへ。今回のidentity VSは実PCO input4/output4 |
| float constants | constant cNはshared scalar4N..4N+3。DEFはcodeへfold。metadata.sharedsとmaskに従い実PDS DMA/PPP shared予約を設定し、既存shared16を超えるなら拒否または実拡張 |
| export/PBE | E private fixtureのRGBAとUMD BGRA8(format21)を混同しない。PCO PSはBGRA順pack、PBE/storeのchannel契約も一致させる。pixel不一致時はpack前後と実PBE設定を切り分ける |
| VS→FS varying | 現Eのvarying0/固定(.5,.5)は一般TEXCOORDではない。link_nir、varying割当/PDS係数/interpolationを実装してから対応tokenを有効化 |
| texture | TEXLDのsource NIR＋実resource format/row pitch/sampler filter/wrap/LOD、同一VidMm MDL bindingとresidency。1×1固定texelを任意D3D resourceの代替にしない |
| command | 元SM2 token/constant/stream/resource/描画stateをbounded semantic requestとしてKMDで再検証。UMD snapshot pointerやunchecked USCをownerへ渡さない。現在のClear packetにshader payloadを混ぜない |
| lifetime | KMD Render成功前にownerが実prepared shader/constant/resourceを所有し、UMDの後続Set/Deleteと分離。実IRQ/CPU-visible/reset proofは既存control契約を維持 |

現Clear専用`owner_abi.h`の署名を今回変更していない。Eの次private candidateと主担当のWDK buildを阻害しない。dynamic drawの別versioned ABIと同一MDL/IRQ接続は実native実装が揃ってから追加する。

## capsを有効化する条件

SM2 arithmeticのcompile成功だけでFL9_1、ShaderVersion、texture/blend/depth/format capsは設定しない。標準D3D GetCapsは実shader＋state＋draw＋resource/formatの全matrixを満たす`geo_ge8300_frontend`からのみ返す。現bootstrapのmandatory callback検査も維持する。次の具体的残件はvertex declaration/stream fetch、varying/TEXLD、実render state（viewport/scissor/cull/depth/blend）、DrawPrimitive/Indexed/UPのsemantic KMD経路、PBE formatと各capsの照合である。

## buildと確認

主担当が成功済み5a26a0d module directoryを`build-wdk81.ps1 -ReuseDirectory <5a成果物>`へ指定する。既定ReuseSourceCommitは5a26a0dのfull hash。kernel root headers/sourceが同一なら全KMD objectを既存archiveから再利用し、opaque entry/clientも再利用する。UMD layout依存のclear_ddi/bootstrapと、新shader_state/sm2だけをcompileする。archive生成は最終frontend/PCO DLL link成功ではない。

PCO backendはWDK標準ライブラリにない。Windows ARM32へ同じpatched Mesa PCO/NIR/rallocをsource-buildする作業が残る。host用ELF archiveをARM UMDへlinkしない。`pco_sm2.c`はその実sourceに対するadapterであり、build-wdk81.ps1はbackendをdummy実装で置換しない。

開発hostではEの保存manifestの全link input/object hashを検証し、依存再buildなしで次を実行できる。

```text
python3 drivers/gpu/mt8167/wddm-render/compiler/build-host.py --reference-manifest <Eのusc-source-v3/manifest.json> --output <作業root内の新directory>
```

実確認はVS位置転送、PS定数色、PS MADのbytecode→NIR→実PCO USC生成を一括で実施。最初の固定入力ではVS24byte/input4/output4、PS定数64byte/temp5/shared4、PS MAD72byte/temp5/shared4を得た。これはhost実compilerの結果で、GPU pixel correctness、Windows ARM compiler DLL、FL9_1や標準D3D成功ではない。ログ/manifestはrootの`port/xnu/out/windows-rt-wddm-sm2-pco-source-v2`。UMD shader_state/既存layout依存TUとdecoderは実DDI headersでARM構文確認済み。実WDK追加TU/PCO ARM buildは未実施。

一次資料はWDKの`d3dumddi.h` CreateVertexShaderFunc.Size／CreatePixelShader.CodeSizeと、`d3d9types.h` shader token fields。Microsoft [Shader code format](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/shader-code-format)、[CreateVertexShaderFunc](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createvertexshaderfunc)、[CreatePixelShader](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createpixelshader) を参照。PCO sourceはMesa base `0d94b3f397fb9cf8470bb5600ada8af79ff5361a`＋Eのhash固定patched archiveを使い、MesaやE sourceは編集しない。
