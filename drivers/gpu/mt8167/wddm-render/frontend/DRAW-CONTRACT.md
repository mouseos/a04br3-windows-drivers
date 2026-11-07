# GE8300 draw/stateとshader入力の固定境界

`draw_abi.h` version1はsource-linked kernel拡張。packed-clear owner ops v2、
control-v2、CreateV3および既存providerは変更しない。Eは
`GeoNativeOwnerGetDrawOpsV1(binding,out_ops)`と実`prepare_draw`を所有し、
Hはruntime DDI→capture/元allocation解決/元token decoder/PCO compilerを所有する。
Hが追加した`geo_draw_validate_capture`はpacket範囲と状態の構造検証であり、
USCの安全性検証・pipeline対応・実job生成の成功ではない。

## 必須token形式

| 実UMD入口 | 実際のshader入力 | 現Hとの整合 |
| --- | --- | --- |
| D3D9 `CreateVertexShaderFunc/CreatePixelShader` | version token＋SM2/3 instruction/operand列＋END。byte countはDDI引数 | 現decoderはSM2の限定arithmeticのみ。SM3/全SM2を広告しない |
| D3D10/11 `CreateVertexShader/CreatePixelShader` | `pShaderCode[0]` version、`[1]` total DWORD count、tokenized instruction/operand列。入出力signatureは別DDI引数 | SM2 decoderへ渡せない。新SM4/5 decoder→NIRとsignature取得が必要 |
| アプリ `ID3D11Device::Create*Shader` | コンパイラ出力blob。runtimeがDDI入力へ処理する | API blob/DXBC containerをDDI token streamと同一視しない |

WDK8.1 `d3d10umddi.h`のPFND3D11_1DDI_CREATEVERTEXSHADER、
PFND3D10DDI_CREATEVERTEXSHADERと`d3d10TokenizedProgramFormat.hpp`が根拠。
D3D10 token stageはVS=1/PS=0であり、H内stage VS=0/PS=1と逆。
`draw_protocol.c`はこの変換、長さ、major4.0/4.1/5.0と上限を検査する。
命令意味・署名の検証やSM4→PCO compileを実装済みと扱わない。

既存exportはD3D9 `OpenAdapter`であり、D3D10/11の
`OpenAdapter10_2`/version negotiation/typed device tableは未実装。
Windows8の公式software-requirementsはD3D9 hardwareにD3D9 DDIを要求するため、
D3D11 APIのFL9_1要求だけを見て必ずSM4 DDIが呼ばれるとは決めない。
D3D9の10Level9経路とnative D3D11.1 DDIを区別する。
どちらも必要capability matrixが揃うまで作成成功を広告しない。
D3D10/11経路で未対応shaderをvoid CreateShaderからS_OK相当にしない。
`pfnSetErrorCb`の公式契約では任意のunsupported HRESULTを返すことも正常経路ではなく、
unsupported shaderが通常入力となるpipeline-level capsの公開を先に禁止する。

## Eが実装する最初の動的profile

- requestはimmutable kernel capture。target/vertexはHが本物VidMm handleから
  解決した参照付きowner allocation ID。targetは同じShadow MDL/SG/aperture。
  頂点bufferの範囲はEが実保持allocation byte countと照合する。
- 非indexed TRIANGLELIST、POSITION0 float4、stride16。first/count/offsetを
  実PDS fetchへ反映する。固定2頂点成分や固定triangle座標へ置換しない。
- 各stageに元DDI token列、実PCO USC、temps/shareds/vtxins/vtxouts/mask、
  32×float4定数のbitwise snapshotを渡す。spanはblob内offsetでありCPU/GPU VAでない。
  Hが次Set/Delete後もE job内のbytes/metadataを変更できないようEが自分でcopyする。
- **EがUSC instruction/branch/memory操作とmetadataを実検証する責務を持つ。**
  user-mode compiler由来という理由で任意USCを信用しない。初profileは
  arithmetic/input/shared-load/position export/BGRA color storeだけ。
  実PCO生成命令の安全性検証が未実装ならprepare成功もcapも返さない。
  正規GPU VM isolationを持たない現native contextへ任意memory ISAを通さない。
- vertex input4/output4、shared scalar `4*cN..4*cN+3` を実PDS/PPPに設定。
  PCO metadataのsharedsが旧固定予約を超える場合は実拡張か拒否。
  VS→FS varying/textureはversion1 profile対象外。constant-color FSのみ。
- outputはD3D A8R8G8B8＝BGRA bytes。native fixtureのRGBA PBEと混同せず、
  shader pack/PBE/storeを一致させる。800×600実固定profileしか対応しなければ
  その他寸法（既存clientの256×256を含む）は正直に拒否する。
- viewportはIEEE binary32 bit列をそのまま渡す。scissor/cull/blend/depth/stencil
  は現profileでは無効、全RGBA書込み。未対応stateを無視して成功にしない。
  KMD framing検証は整数演算だけ。kernel内でFP演算してviewportを解釈しない。
- PASSIVE prepareはcommand/PDS/PPP/cacheを整えjobを所有するだけでpublishしない。
  submit→実ISR→PASSIVE cache/fence→後続実IRQは既存control-v2を使う。
  completionはGPU_DONEとCPU_VISIBLEを独立して満たす。再submit/duplicate generationを拒否。
- NoDMAは未qualified。完了だけでtarget/vertex/shader/FW/SGを解放しない。
  discard/unbind/Stop/Deleteの安全性を仮定せず、必要な参照はpoison時も保持する。

## H側の実装済みと残件

ARM32のPCO/NIR/compiler DLLは81c292bで実link成功、stock Microsoft CRTと
NEON/FPを保持し、Windows CRT DLLの別配布依存はない。
このDLLのcompile callbackは現SM2 subsetのみ。SM4 tokenを受理するよう偽装しない。
`geo_draw_validate_capture`はARM32 no-FPでcompile済み、module helperの
新TUとしてKMD/UMD archiveへ収容する。既存clear packetをdrawに解釈しない。

残件はHのSM4/5 decode/signature→NIR、DDI10/11初期化/caps/state/resourcesと
実allocationからのdraw capture、EのGetDrawOps/USC検証/実prepare/factory/control。
本契約を先に双方で固定し、相互の未実装を前提とする成功stubを追加しない。

一次資料:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11_1ddi_createvertexshader
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_createvertexshader
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/software-requirements
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/shader-code-format
