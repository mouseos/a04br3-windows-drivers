> 以下は初期combo broker候補の履歴。現在のclient2018は実transport predicateに
> 基づくBTHX capabilityとnamespace/admission限定の有界待機を持つ。
> 後述の「None/0を返す」は当時の状態であり、現在の実装の説明ではない。
> 実HFP PCM/latency/drain資格の未確定と、標準radio列挙の成否は区別する。
> 公開Wi-Fi1033系の2018 READ完了修正の同期については
> [source同期の根拠](../../../../docs/mt6630-bthx2018-source-sync.md)を参照。

# MT6630 SDIO/STP共存broker候補

基点はWi-Fi1013 `8fd9b62d`。既存1012成功package/1013署名packageは変更しない。
A04_WIFI_BT_BROKERは既定0、専用builderだけ1。現GPU/codec/eMMC媒体へ追加しない。
実接続・通信・共存・電源遷移は未確認。ビルド成功をその代わりにしない。

## 実接続した経路

Linux a04br3-linux-drivers `3d2c74e252d6f34a5980deda96bdc04064108d74` の
`a04br3_mt6630_core.c` は単一host runtime lockでfn1 WLANとfn2 STPを扱い、
STP task0をBT、task4をWMTへ分配する。WindowsでもMSDC2/RTC/firmwareの実所有は
従来のNDIS worker/親leaseだけに残す。BT clientはMMIO/IRQ/GPIO/PMIC/clockを扱わない。
GPLの実装文は取り込まず、既存XNU由来BSD SDIO/STP codecを再利用する。

NDISがNdisRegisterDeviceExでkernel専用METHOD_BUFFERED brokerを公開する。
要求は単一slotで既存PASSIVE workerへ移し、FileObjectでBT所有者を限定。
UserModeは拒否し、生NVRAM/firmware/ポインタ引数は設けない。HCI keysやアドレスはログに出さない。
OPENは実WLAN ready/calibrationとfn2 bootstrap退役を要求し、実WMT ON→vendor個体設定→
Read Local Version→Read BD_ADDR照合後だけ受理。fn1は再初期化しない。
WRITE/READは既存H4分割再構成・実TX completion会計を使用。RXは32packet固定上限。

Wi-Fiの長いOID処理内もwmt_bridge_poll_wlan_statusのsame-worker hookでfn2を排出。
Wi-Fi firmware-own/NDIS Pause時もBTがopenならNDIS poll workerを起動する。
WLANとBTでCHISR読取りを競合させず、bootstrapのfn2消費権を一度だけ引き継ぐ。
BT CLOSEは実function-off/ACK/TX/RX/partial drainだけを行い、whole-combo power-offはしない。
同一hardware epochでclean close後は再open可能。旧排他的BT profileは使わない。

Wi-Fi radio-off/cold reset/HaltはBTをIO有効な間に停止してhardware epochを失効する。
BT側は旧handleで成功を継続しない。独立したradio power lifetimeは未完成であり、
Wi-Fi OFF中のBT継続はこの候補の保証に含まれない。
BT実I/O不明/排出失敗ではshared faultを保持し、正常なWi-Fi状態やlease返却を偽装しない。

別SYS `mt6630-combo-bluetooth.sys` はROOT\\MT6630COMBOのWDF bus client。
KernelMode BTHX IOCTLをbrokerへ接続し、MS_BTHX_BTHMINI子を持つ。親の物理MSDC2子は
Wi-Fiのままで、新しい物理資源や排他親profileを要求しない。
root devnodeの生成・実起動順序/依存関係・PnP rundownは実機未検証。
broker OPENはWi-Fi実ready前なら失敗する。再試験や自動cold retryは実装しない。

## SCO bypass資格を分ける

Windows8/8.1 QUERY_CAPABILITIESはMaxScoChannels=1/ScoSupportHCIBypassが必須。
これは単にHCI SCO packetを転送しないという意味ではなく、実I2S/PCM audio pathの契約。
今回のSDIO共存と、通常のMT6392 codec/AFE audio更新だけではその資格を満たさない。
MT6630 FC72の4bytesは個体値を送るところまで確定しているが、bit/rate/master-clockの
意味、HFP codecとの整合、実codec/FIFO latency、方向別AFE DMA drainの根拠が未成立。
そのため新clientもNone/0を返す。標準stackは公開契約上loadしない。
PCM値やlatencyを推測したcallback、常時成功、架空のdrainを追加しない。

参照:
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthxddi/ni-bthxddi-ioctl_bthx_query_capabilities
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nf-ndis-ndisregisterdeviceex
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfrequest/nf-wdfrequest-wdfrequestforwardtoparentdeviceioqueue

## build/検証の境界

`tools/ci/build-mt6630-combo.ps1` は専用出力先と既知hashのWDK8.1 overlayを要求。
nested wmt_bridge layout変更のため依存driver TUsを再compileする。旧1013 objectを
異なるlayoutへ無検証流用しない。親/音声/GPU/EDK/SDKはbuildしない。

host回帰は実transportを使うshared attach/reopen/個体readback、fn1 read-clearとfn2 serviceの
同一呼出し経路、既存WIFI NVRAM checksumとscan-confirm→connectを対象とする。
Windows queue/driver実動作はこの合成fixtureだけでは確認できない。
