# 標準allocationのサイズ出力契約

`DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA`の
`AllocationPrivateDriverDataSize`と`ResourcePrivateDriverDataSize`は出力値である。
WDK 8.1の`Include/shared/d3dkmddi.h`は、いずれも
`out : Size of buffer required to hold the PrivateDriverData`と定義する。
対応する非NULLポインターには、OSが確保したバッファへprivate dataを書く。
サイズ欄の呼出し時の値を、バッファ容量として検査しない。

共有primaryのfill呼出しでサイズ欄が0の場合、従来実装は十分なバッファが
渡されていても`STATUS_BUFFER_TOO_SMALL`を返していた。必要サイズを返し、
非NULLポインターに既存のprimary descriptorを書き込むよう修正した。
任意の短いバッファを許容する契約ではない。OSはsize照会で得たサイズに
従ってfill用バッファを確保する。

同じcallbackのshadow/staging分岐はサイズ欄を出力として扱っていた。
`QueryAdapterInfo.OutputDataSize`と`CollectDbgInfo.BufferSize`は呼出し元から
渡されるバッファ長であり、これらの容量検査は維持する。

`tests/standard_allocation_outputs.py --output <作業root内の新規ディレクトリ>`は
実callbackを抽出してホスト上で実行する。共有primary・shadow・stagingの
size照会とfillについて、サイズ欄を0、1、UINT32_MAXで初期化し、必要サイズと
descriptorの返却、書込み範囲外のcanary、未使用resourceバッファを検証する。
POSTレイアウト取得はstubであり、Windows ARMのビルドや実機描画の検証ではない。
run195のPOST解放に至った元failureとの因果も、この修正だけでは証明されない。

一次資料:

- WDK 8.1 `Include/shared/d3dkmddi.h`、`DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA`
- [DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/ns-d3dkmddi-_dxgkarg_getstandardallocationdriverdata)
- [DxgkDdiGetStandardAllocationDriverData](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmddi/nc-d3dkmddi-dxgkddi_getstandardallocationdriverdata)
