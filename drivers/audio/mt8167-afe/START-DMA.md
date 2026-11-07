# MT8167 AFE StartDevice / DMA candidate

run33 F6F2の実配置Audio1004はCM Code10/ProblemStatus C000009A。
Audio clientはendpoint0、HRESULT 80070490でKS RUNより前に失敗している。
C000009Aだけからpool枯渇やIoGetDmaAdapter NULLを断定しない。
run38はEngine prepare前に旧service.logの出現を検出して停止し、これらのログは旧run33。

## 変更

AFEの合成PDOにGUID_BUS_INTERFACE_STANDARDの親stack転送を追加する。
WDF_QUERY_INTERFACE_CONFIGのSendQueryToParentStackを用い、親ACPI busが実際に返す
interface/context/referenceをそのまま転送する。DMA adapter、bus address translation、
成功値を自作しない。既存audio v11の構造とMMIO割当は変更しない。

IoGetDmaAdapterのDEVICE_DESCRIPTIONをVersion3/DmaAddressWidth32へ揃える。
AFEはbus-master/contiguous ringのためScatterGather=FALSEを維持する。
GPUのScatterGather=TRUEを複製しない。返却DMA_ADAPTER.VersionからDMA_OPERATIONS版を
推測しない。NULLの詳細理由をAPIは返さないので、C000009Aは引き続き失敗として返す。
Version1がARMで一律非対応であるとか、Version3だけで今回直るという主張はしない。

Engine Audio INF1006、Engine parent INF1008へ昇版する（date11/06/2026）。
HFP別branchのAudio1005/parent1007と同版異SYSにしない。既存tone INFは変更しない。
既存Engine STOP/RUN/再open、digital DMA、touch/buttons/USB/WiFi flagsを保持する。
build helperは非VFP/非NEON codegen、libcntprと実INF版のmanifest取得を使用する。

## 固定NativeStart

明示Engine buildにA04_AUDIO_START_PERSIST=1を追加する。StartDeviceから戻る直前、
PASSIVEのkernel contextでだけ、固定
`HKLM\SYSTEM\CurrentControlSet\Services\Mt8167Audio\Parameters`へ保存する。
任意registry path、IOCTL、register-write brokerはない。
`NativeStartComplete`を0→payload書込み→1にし、ZwFlushKeyを一度だけ呼ぶ。
実保存/flush失敗はkernel debugのsnapshot=NTSTATUSで返し、driver開始の本来の結果を
成功に書き換えない。Completeだけでは次Recoveryでの永続化を証明しない。

`NativeStart`はREG_BINARY、14 DWORD / 56 bytes、little-endian：
`schema=1, bytes=56, inf_revision=1006, stage, result(NTSTATUS bits),
system_time_low, system_time_high, dma_called, description_version,
scatter_gather, address_width, map_registers, adapter_present, operations_present`。
system_timeはStart進入時のKeQuerySystemTime（UTC 100ns）。読出しはComplete=1、
schema/bytes/1006を確認し、実試験boot時刻と照合する。古いrecordを今回の成功にしない。
dma_called=0なら未呼出しであり、0 map registersやadapter absentを照会成功と扱わない。

stage：1 resources、2 PDO、3 owner pool、4 DMA adapter、5 parent query、
6 parent bind、7 shared attach、8 AFE map、9 AFE bind、10 IRQ、
11 backend ready（予約）、12 miniport、13 PortCls registration、14 ready。
失敗stageをbefore-callで記録するため、owner poolとadapter NULL、MMIO mapping、
PortCls factory failureを区別できる。既存NTSTATUSをそのまま保持する。

## 根拠と確認

- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iogetdmaadapter
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_device_description
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_bus_interface_standard
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfqueryinterface/ns-wdfqueryinterface-_wdf_query_interface_config
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwflushkey

既存DMA adapter/PnP adapter確認とPowerShell ASTを変更範囲で確認する。
標準DDIに基づく修正候補であり、WDK最終linkと実機StartDevice成功を分けて記録する。
NTFSログのreplayやeMMC cache仮説をDMA開始失敗の原因と断定しない。

```powershell
powershell -NoProfile -File tools\ci\build-mt8167-first-playback.ps1 `
 -OutputDirectory <checkout>\build\audio-dma-start `
 -EnginePcm -IntegratedUsb -IntegratedWifi -SkipPeripheralBuild
```

親＋Audioだけを生成し、既存USB/WiFi/client SYS/EXEを再compileしない。
既存媒体・run40は変更しない。実機操作は主担当が行う。
