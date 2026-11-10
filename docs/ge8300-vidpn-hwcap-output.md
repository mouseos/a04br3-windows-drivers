# VidPn capability出力のReserved初期化

source基点は`3d4a3d1097b7f6a796972e0425e37d0263b2b070`。
`BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability`は6個のcapabilityだけを書き、
Reserved 26bitを呼出元bufferの値のまま返していた。

Microsoftの[D3DKMDT_VIDPN_HW_CAPABILITY契約](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmdt/ns-d3dkmdt-_d3dkmdt_vidpn_hw_capability)
はReservedを必ず0にするよう要求する。WDK8.1の`d3dkmdt.h`でも26bitが定義され、
`d3dkmddi.h`の`DXGKARG_QUERYVIDPNHWCAPABILITY::VidPnHWCaps`はOUTである。

出力capability構造だけを初期化してから既存の6bitを書き込む。
入力のVidPn handle、SourceId、TargetIdには書かない。
full unified表示と旧DODで既存のcapability値を維持する。

最小検証は実callback本体と指定WDKの型定義を抽出し、Linuxで実行する。

```
python3 tests/test-ge8300-vidpn-hwcap-output.py --wdk-shared <WDK8.1/Include/shared> --output <新規出力先>
```

ゼロ・全1・A5 sentinelの出力bufferについて、旧sourceのReserved残存を再現し、
修正版のReserved=0、既存6bit、入力handle/IDs、前後canaryを確認する。
両構成をASan/UBSan付きで実行する。Windows ARM driver build・実機検証の代用ではない。

run197でこのcallbackが呼ばれた証拠や、OS入力bufferの初期値、Reservedを理由にした
失敗statusは得られていない。これは具体的な出力契約修正であり、Code43解消の確定修正とは扱わない。
