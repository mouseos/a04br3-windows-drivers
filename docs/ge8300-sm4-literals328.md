# 一般SM4の即値定数: compiler後続候補328

Texture327 (`41cc2a01`) から独立。327および325/326/現媒体は変更しない。

通常SM4 decoderはsource operandをregister/CBに限定しており、HLSLの一般的な数値literalを受け取れなかった。WDK8.1 `d3d10TokenizedProgramFormat.hpp` のInstruction Operand Format、`D3D10_SB_OPERAND_TYPE_IMMEDIATE32=4`、1/4-componentと0D indexの定義に沿い、scalar/vector32bit immediateを実装する。

各IEEE32bit patternは既存DEF opcode81→`pco_sm2.c`の`nir_imm_int`/`nir_vec4`経路へそのまま渡す。値の意味やshader全体を固定プログラムへ差し替えない。scalarは4laneにbroadcastし、同bitのvectorは再利用する。literalはruntime CB0領域より上の内部constant slotを使う。CB0に32slot全てが必要なshaderなど領域不足は拒否。literalにはconstant_mask/shared preambleを割り当てない。CB0の範囲外参照がliteral slotへaliasすることも拒否。

追加DEFを含む128命令、32constant slot、2048tokenの既存上限を保持する。64bit immediate、extended/indexed即値、integer opcode、制御フロー、任意resourceアクセスは未対応のまま。wire/adapter/MDL ABI、struct layout、caps、KMD/native/FW/clientは不変。324のlane差分を混入しない。

検証:

- 実decoder ASAN/UBSAN: scalar/vector/IEEE signed-zero・NaN bit保持、同値共有、CBとの非alias、全切詰め、indexed/extended/64bit拒否。
- 実source→NIR→PCO: 異なるliteral入力から異なるUSC、constant_mask0/shareds0を検証。既存Linux PCO/NIR librariesを再利用し再buildしない。
- Windows ARM: sm4_adapter.objの1TUだけを既存327 closureへ差替え、他archive memberを全hash比較してlink/signする手順。

**一般shader描画の実機完成ではない。** native_draw_job.cにはtoken/USC・register metadataの有限プロファイル検証が残る。これを外して任意USCを通す変更はしない。今回の即値shaderはcompilerで処理可能になるが、既存native有限プロファイルを越えるDrawは拒否される。独立署名DLLも媒体へ自動混載しない。次に必要なのは、source由来の一般ALU USC検証とPDS/register layoutを同時に拡張する実装であり、capsの宣伝や固定shaderの追加では代替しない。

一次source:

- Microsoft WDK8.1 `d3d10TokenizedProgramFormat.hpp` Instruction Operand Format / IMMEDIATE32。
- 本tree `compiler/sm2.c`: DEFの既存IEEE bit保持。
- 本tree `compiler/pco_sm2.c`: DEF→NIR→PCOの実変換。
- 公開source snapshot `third_party/mesa-pco/mesa/src/compiler/nir/nir_builder.h` および `src/imagination/pco/pco_trans_nir.c`。PCO/NIRそのものへの変更無し。
