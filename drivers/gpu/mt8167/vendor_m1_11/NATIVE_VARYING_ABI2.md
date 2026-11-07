# ABI2: POSITION4 + COLOR4 linked pair

この契約はABI1 finite flatを変更しない。`frontend/draw_link_abi.h` の別
`geo_draw_request_v2` を使用する。ABI1 callbackへversion2を渡しても受理されない。
GPUへの完全draw接続とISA検証は別工程で、係数builder単体はcapabilityを公開しない。

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
