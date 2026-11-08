# Windows RT 8.1 shader signature fields

標準D3D11の2回連続Clear/readbackが成功した0.0.3.22で、続くCreateVertexShaderがErrorCb E_INVALIDARGで失敗した。GPU描画・completion・resetの成功とshader作成失敗を区別する。

実機のd3d11.dll（SHA256 `90672c01bde6ec862756cd12708e6ac2995efbfe8f9ace687c8781b606f050f2`）と一致するMicrosoft公開PDBを使い、REAで`CDevice::BuildDriverSignatures`（0x1002dea8）を確認した。入力・出力とも20byteのD3D11_1DDIARG_SIGNATURE_ENTRYのoffset0 SystemValue、4 Register、8 Mask、16 MinPrecisionを書き、offset12 RegisterComponentTypeは書いていない。したがって、このフィールドをFLOAT32との一致検査へ使うことは、本ランタイムのshader作成を拒否する根拠にならない。REA evidence `ev_`の完全ID・出自・原結果は指定の非公開作業ログに保持する。

修正は`geo_umd_compile_shader11`からRegisterComponentTypeの読み取りと拒否を除く。MinPrecision、signature count/配列、レジスタ・mask・system valueの検証と、実token decoderの限定float opcode検査は保持する。整数shader・一般SM4対応を追加するものではない。型の根拠は命令列のdecoderであり、不明なフィールドをFLOAT32へ書き換えたり成功を合成しない。

公式定義:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d11_1ddiarg_signature_entry
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11_1ddi_createvertexshader

WDK8.1の実定義と実ランタイムを併用した判断。生成済みの固定clientのVS/PSを実SM4 decoderへ渡すhost試験は成功しているが、この修正版でのshader作成・三角形描画はまだ実機未検証。
