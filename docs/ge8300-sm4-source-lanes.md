# SM4の実source成分検査（署名候補へ未結合）

3.23の実shader結果を待つ間の独立source改善。3.23 package・main媒体・driver版・capsは変更しない。117bはservice token拒否でshader未実行であり、この不具合を実機失敗原因とは扱わない。

`compiler/sm4.c`は現在、temporary sourceのswizzle4成分すべてをdefinedとして要求する。component-wise MOV/ADD/MUL/MADのdst.maskで書かない成分や、DP3が消費しないWまで検査するため、正当な部分成分shaderを拒否する。

WDK tokenized opcode/operandのwrite maskと既存`pco_sm2.c`のloweringを根拠に、component-wise float命令ではdst.maskに対応するswizzleだけを検査する。DP3はXYZ、DP4はXYZWを必ず読む。実際に使う未定義成分、register bounds、output全成分定義、opcode制限は緩めない。NIR lowererはpartial tempをnir_undefから作り、dst.maskのlaneだけをinsertする。未使用成分をCPU未初期化値へ読み替える修正ではない。

既存MOV/DP3だけで過剰拒否を再現し、修正後はmasked MOV/DP3を受理。DP4の未定義W、DP3の未定義Z、実live ZZZZ read、output未完、SAMPLEは拒否。scalar replication/full vectorは従来どおり受理。実decoderをASAN/UBSAN付きhost環境で実行した検証であり、NIR/PCO GPU code生成・実shader・描画は未検証。

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/ge8300_sm4_lane_definedness_test.c drivers/gpu/mt8167/wddm-render/compiler/sm4.c -o BUILD_IN_PROJECT/after
BUILD_IN_PROJECT/after
```

## 一般shader/textureの残る境界

|場所|現在の範囲・未実装|
|---|---|
|compiler/sm4.c|SM4.0の限定signatureと6種類のfloat arithmetic。flow/integer/resource/SAMPLEは未対応。この変更で命令を増やさない|
|compiler/pco_sm2.c|arithmetic NIR lowerのみ。texture命令・sampler/image bindingのloweringなし|
|umd/typed_resource.c|固定RT/STAGINGとbuffer。SHADER_RESOURCEを含むTexture2Dのcreate契約は未対応|
|umd/typed_state.c|sampler descriptorの検証・保持は存在。未使用sampler state保持とshader sampling実行は別|
|native draw ABI/owner|target/vertex/code/constantsの契約。一般SRVのGPU layout・MDL寿命・sampler descriptor・fence/cache・subresourceの結合は未実装|

固定native1x1 texture helperの存在を標準D3D Texture/SRV対応と混同しない。SAMPLEをparserだけで受理したり、既定成功・texture caps宣言で未実装経路を埋めない。次のGPU実修正はmainから返るshader/triangle実結果を優先する。
