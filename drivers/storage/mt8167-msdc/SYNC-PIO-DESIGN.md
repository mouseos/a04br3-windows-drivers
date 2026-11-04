# 同期PIO複数読取り試作

`A04_SYNC_PIO_MULTIREAD=1` は既定OFF。物理Storport、read-only、非ワーカー、DMA無効、非仮想の組合せだけを許可する。まだ `HwInitialize` は開始を拒否する。既存のboot/scratch/仮想診断経路とは併用できない。`tools/ci/build-msdc-multiread-suite.ps1` の `sync-pio-compile-only` はこの構成のビルドを準備する。

非仮想miniportの `HwStorStartIo` はStorportのStartIoロックにより逐次呼出しされ、同期処理はSRB完了後に後続ワーカーを残さない。Storportは `ScsiStopAdapter` の前に未完了要求を排出する。`IoReleaseRemoveLockAndWait` はPASSIVE_LEVELでしか呼べないため、DIRQLの停止コールバックの解決策にはしない。デバイスextensionを独自のカウンタだけで保持できるとも仮定しない。[HwStorStartIo](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/nc-storport-hw_startio)、[SCSI_ADAPTER_CONTROL_TYPE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/ne-storport-scsi_adapter_control_type)、[IoReleaseRemoveLockAndWait](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-ioreleaseremovelockandwait)。

PIO部品はDMA有効中、PIO無効、関連割込み有効中に開始しない。最大8セクタをCMD23で制限しCMD18で読取り、正常完了後だけ専用ステージ領域から要求バッファへコピーする。1セクタはCMD17。データ異常やタイムアウトでは部分データを公開せず、複数読取りならCMD12/R1bとDAT0を待ち、停止失敗は別エラーにする。書込みコマンドもDMAレジスタも発行しない。CMD23とPIOの参照元は機種XNU `port/xnu/kernel/bsd/dev/a04br3_msdc.c`、`unixnu/UniXNU/drivers/NativeMsdc/NativeMsdc.cpp`。Linux v6.6 `drivers/mmc/host/mtk-sd.c` は要求のDMA mappingと停止を管理し、CMD23が使えない複数要求には停止コマンドを使う。Windows用の同期やPnPをそれらが保証するわけではない。

ホスト模擬はコマンド順、境界、DMA活動中の拒否、タイムアウト、CRC、停止失敗と部分結果非公開を確認する。ARM Thumb COFFコンパイルは部品の構文・ABI確認に限る。

**解除条件未達:** 既存の物理Storport経路では過去にE6/1F/04が発生し、SRBマッピングとalignmentの原因が未確定。同期PIOでも物理除去に伴うMMIO abortは防げず、タイムアウトの最悪時間は高IRQL滞在として実測・削減が必要。Windows 8.1 WDKの新構成のリンク、PnP・power・復帰と実機読取り、独立した正解データ照合も未検証。非同期DMAの旧経路は残っており、書込みにこの設計を転用していない。これらを理由に開始拒否は解除しない。
