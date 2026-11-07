# ABI2: POSITION4 + COLOR4 linked pair

この契約はABI1 finite flatを変更しない。`frontend/draw_link_abi.h` の別
`geo_draw_request_v2` を使用する。ABI1 callbackへversion2を渡しても受理されない。
係数builder単体はcapabilityを公開しない。後述の有限source pairについては
完全producerと実owner prepare-v2へ接続済み。実GPU・標準アプリの検証は未実施。

## HからEへ渡す固定snapshot

HはVS/FS NIRをpco_link_nir / pco_rev_link_nirで結合した後のpco_shader_dataを読む。
ポインタを含まない `geo_draw_pair_link`、上限1varying。全fieldはuint32_t。
コード・constants・tokensのbounded spanとtemps/shareds/vtxins/vtxoutsは既存shader記述。
GPUVA、PA、PPP wordは入力にしない。semanticはCOLOR=1/index0、bit_size32、components4。
rangeのstart/countはDWORD component単位、offsetは元PCO値を保持する。
sourceのiteratorはstart/countを使用し、offsetを加算しない。

最初のprofile:

|項目|条件|
|---|---|
|position|start0,count4|
|VS total outputs|8 DWORD|
|COLOR VS range|start4,count4|
|VS f32_smooth/flat/npc|smooth4/0/0 または NPC0/0/4|
|VS f16_*|すべて0|
|FS uses.z/w|実compilerのboolean。補間方式から推測しない|
|COLOR FS coeff range|start=4*(uses.z+uses.w),count16|
|FS common.coeffs|16+4*(uses.z+uses.w)|
|entry_offset|USC span内BYTE offset。完全producerがコード範囲と併せて検査|

FS coeffはA/B/C/PADの4 DWORDで1成分を構成する。COLOR4は16 DWORD。
上限はZ/Wを含む3 iterator、24 coeff DWORD、data8/code3 DWORD。
flatはprovoking vertexが別途必要なため今回builderでは非対応。

## 一次sourceとencoding

固定Mesa base `0d94b3f397fb9cf8470bb5600ada8af79ff5361a`。
対象sourceはROOT/unixnu/UniXNU-a04br3/.cache/mesa-pvr。

- pco/pco_data.h: pco_range と最終VS/FS metadata。
- include/hwdef/rogue_hw_defs.h: coefficient A/B/C/PAD、SET_SIZE=4。
- vulkan/pvr_arch_pipeline.c:1505-1635: Z→W→userの順序。
  user TSP index = vs.start-2-!uses.z-!uses.w、F32/F16 offsetはその2倍。
- vulkan/pds/pvr_pds.c:4136-4240: 各iterator data2DWORD/code1DWORD。
  dataは4DWORD境界、destination=coeff.start/4、最後だけISSUE bit63とEND bit26。
- vulkan/pds/pvr_rogue_pds_defs.h と encode.h: DOUT opcodeF、dst4、
  constant64 index bit8、destination bit54。
- csbgen/rogue/pds.xml: Gouraud3、4D size3、perspective bit16、offset bit8/0。
- vulkan/pvr_arch_cmd_buffer.c:6840-6905 と 7685-7765:
  OUTPUT_SEL vtxsize8、rhw_pres=uses.w、z_pres=uses.z、varying0はVS metadata。
  PDS varying size=dataDWORD/4、USC varying size=ceil(coeffDWORD/16)。

`a04_m111_build_varying_pds` はこのsourceから実DOUTIとPPP scalarを生成する。
失敗時は出力不変。heap addressや現在owner mappingは後段producerが合成する。
既存固定PPPのrhw_pres=1をABI2へ流用するとoffsetがずれるので禁止。

## 次の接続

H: 上記snapshotと実linked VS/FS USC outputを生成し、Eへ固定hash/metadataを渡す。
E: USC profile検査、VS8 input fetch、係数領域、PPP header/size/state、
owner prepare-v2を接続する。既存ABI1 ISA allowlistを任意ISA許可へ緩和しない。
新LOAD背景およびABI2は実GPU未検証。標準FL9_1 capsは別qualificationが必要。

実PPPへの接続は `a04_m111_build_linked_triangle_ppp`。
実uniform PDS data extent、fragment PDS temps、FS common.sharedsを別入力とし、
SIZEINFO1/2とOUTPUT_SEL/VARYINGをsource unitsで合成する。
係数PDSが1 temp使うためfragmentが0でもPDS temp割当は1単位。
未使用uniformはサイズ0で停止する。固定背景LOADはこれと独立した既存program。
VSの実fetchは `a04_m111_build_linked_vertex_pds` で入力8DWORD/stride32、
DOUTU tempを4DWORD単位で切上げる。既存2/3/4成分fixture builderの出力は維持。

再現検査: project root内TMPDIRを指定して
`sh tools/gpu/test-native-varying-pds.sh`。


## 実owner接続と有限source pair

`GeoNativeOwnerGetDrawOpsV2(binding, ops)` → `ops.prepare_draw` →
`a04_vendor_native_prepare_draw_v2_cold` → `a04_m111_build_draw_job_v2`。
公開control-v2のsubmit/actual IRQ/service/visibility/quiesceを共用し、
prepare自体はpublishしない。旧V1 callback/clear/DOD ABIは変更しない。

Hの最終compiler snapshot到着前にもproducerを検査できるよう、Eの
`tools/gpu/shaders/native-color-pair.c` で同固定PCOのpreprocess/link/reverse-link/
lower/postprocess/encodeを実行した。PCOに渡すNIRはsourceで構築した参照pairであり、
HのSM4 decoderが生成したNIRを実行したものとは区別する。
D3DCompile側の対応HLSL:

```
struct V { float4 p:SV_Position; float4 c:COLOR; };
V main(float4 p:POSITION, float4 c:COLOR) { V v; v.p=p; v.c=c; return v; }
float4 main(float4 c:COLOR):SV_Target { return c; }
```

Windows official d3dcompiler_47の実出力tokenと、上記NIRから生成したUSCの
双方を有限allowlistとして検査。PCO同2process出力一致。

|stage|USC bytes|temps/shareds|vtxins/out|coeffs|
|---|---|---|---|---|
|VS|88|0/0|8/8|0|
|FS|80|5/0|0/0|20|

FS uses.z=0/uses.w=1、COLOR係数start4/count16。実smooth translatorは
`pco_trans_nir.c` でuses.wを要求するので、producerはこの実値に限る。
低level係数encoderのZ/W全組を実shader対応の広告に使わない。

USC SHA256: VS c623f3047d0dec747d81ab3b298979fdde89576403de6af66f89eb8bf65d710f、
FS 2103c3048ed23a9dd336274c708a61b6fc21438e960e9766ee2e63d3de815796。

ownerの既存各4096-byte USC/PDS allocation内で再配置する。
USC: first0x40、VS0x80..0xd8、FS0x100..0x150、背景LOAD0x200..0x2d0。
PDS: VS data0x140/code0x160、FS0x180、背景0x300、係数data0x380/code0x390。
新たなGPUVA/MDL allocationは不要、PDS書込extentを0x400へ拡張。
頂点入力は実別MDLの96 bytes。main VDM_STATE4 outputs8、STATE5 input8/4=2。
CAM/max instancesはpvr_arch_calculate_vertex_cam_sizeの8XE条件で
(8+1+4)*4=52 <14*4なので既存31/16の範囲。
format2のviewportはD3D Y=-300/+300、Z=1/0。旧format1は変更しない。

Hは実compilerのpairが上記code/metadataへ一致することを確認し、
request_v2のspanとshader fields、link snapshotを渡す。
一致しない場合は実artifactをEへ渡して比較し、未検査USCを許可しない。
初回のtyped flat開発候補は引き続きV1 opsで進められる。
限定開発profileと全FL9_1適合は区別し、未対応operationはエラーで返す。

再現生成: `tools/gpu/build-native-color-pair.py --help`。
検査: `sh tools/gpu/test-native-draw-v2.sh <explicit-source-pair-artifact-dir>`。
実cold MDL/SG/PT/resource/ring write/readbackはhost doublesで検査済み。
実FW start・GPU draw・標準runtime成功をこの検査結果から主張しない。

## 実SM4入力からのsource pair（v3）

`tools/gpu/shaders/native-color-sm4.h` の有限decoderを追加。
実公式FL9_1 VS/FSの宣言とMOV/RET、入力出力register、PS LINEAR補間、
完全float4 mask/identity swizzle、長さとterminalを検査する。
resource/定数/制御flow/未対応opはエラー。Hの汎用decoderを置換するものではない。
実runtime signature/input-layoutの照合はHが別途行う。

`build-native-color-pair.py` は実DXBC SHDR token→上記decode→NIR構築→
preprocess/link/reverse-link/lower/postprocess/encodeを実行する。
VS位置/colorは実MOVのsource registerから生成し、tokenを検査せず固定NIRを返さない。
2process一致、全USC/token byteがe837 admission profileと一致した。
新artifactはROOT/port/xnu/out/windows-rt-native-color-pair-sm4-v3/。
実SM4入力はHのwindows-rt-wddm-fixed-shaders-69385809、4 DXBC hashとも独立生成と一致。

e837に含まれる生成headerは31905d08時点のsource hashを保持している。
v3生成headerはartifact内だけに保存し、USC/token配列が同一であることを比較した。
初回flat実機候補のSYS/署名をこの作業で変更・再buildしない。
Hはこの有限COLOR経路を再利用可能。Hの最終compilerに統合した後のmetadata snapshotと
V2 frontend transport接続が残る。標準COLOR drawや実GPU成功は未確認。
