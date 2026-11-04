# 同期PIO複数読取り試作

`A04_SYNC_PIO_MULTIREAD=1` は既定OFF。物理Storport、read-only、非ワーカー、DMA無効、非仮想の組合せだけを許可する。まだ `HwInitialize` は開始を拒否する。既存のboot/scratch/仮想診断経路とは併用できない。`tools/ci/build-msdc-multiread-suite.ps1` の `sync-pio-compile-only` はこの構成のビルドを準備する。

非仮想miniportの `HwStorStartIo` はStorportのStartIoロックにより逐次呼出しされ、同期処理はSRB完了後に後続ワーカーを残さない。Storportは `ScsiStopAdapter` の前に未完了要求を排出する。`IoReleaseRemoveLockAndWait` はPASSIVE_LEVELでしか呼べないため、DIRQLの停止コールバックの解決策にはしない。デバイスextensionを独自のカウンタだけで保持できるとも仮定しない。[HwStorStartIo](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/nc-storport-hw_startio)、[SCSI_ADAPTER_CONTROL_TYPE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/ne-storport-scsi_adapter_control_type)、[IoReleaseRemoveLockAndWait](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-ioreleaseremovelockandwait)。

PIO部品はDMA有効中、PIO無効、関連割込み有効中に開始しない。最大8セクタをCMD23で制限しCMD18で読取り、正常完了後だけ専用ステージ領域から要求バッファへコピーする。1セクタはCMD17。データ異常やタイムアウトでは部分データを公開せず、複数読取りならCMD12/R1bとDAT0を待ち、停止失敗は別エラーにする。書込みコマンドもDMAレジスタも発行しない。CMD23とPIOの参照元は機種XNU `port/xnu/kernel/bsd/dev/a04br3_msdc.c`、`unixnu/UniXNU/drivers/NativeMsdc/NativeMsdc.cpp`。Linux v6.6 `drivers/mmc/host/mtk-sd.c` は要求のDMA mappingと停止を管理し、CMD23が使えない複数要求には停止コマンドを使う。Windows用の同期やPnPをそれらが保証するわけではない。

ホスト模擬はコマンド順、境界、DMA活動中の拒否、タイムアウト、CRC、停止失敗と部分結果非公開を確認する。ARM Thumb COFFコンパイルは部品の構文・ABI確認に限る。

**解除条件未達:** 既存の物理Storport経路では過去にE6/1F/04が発生し、SRBマッピングとalignmentの原因が未確定。同期PIOでも物理除去に伴うMMIO abortは防げず、タイムアウトの最悪時間は高IRQL滞在として実測・削減が必要。Windows 8.1 WDKの新構成のリンク、PnP・power・復帰と実機読取り、独立した正解データ照合も未検証。非同期DMAの旧経路は残っており、書込みにこの設計を転用していない。これらを理由に開始拒否は解除しない。

## 停止と要求の同期

同期PIO構成はStartIo内で状態の受付権を取得し、その呼出し内で処理と完了を行う。StopAdapter、adapter対象の停止・除去PnP、バスリセットは受付を閉じる。ポーリング中は閉鎖を確認して中断し、観測後のMMIOを抑止する。既に転送中の命令と閉鎖通知を原子的にできるわけではない。中断時はCMD12を含む追加のMMIOを避け、部分データを返さない。read-only構成のFLUSH/SHUTDOWNは未書込みとして完了する。再開時のハードウェア初期化は未実装なのでRestartは拒否する。バスリセットの成功は報告しない。

これはStorport管理の寿命を延長する機構ではない。StopAdapterは正常時に未完了SRBがないというポート契約に依存する。突発除去と進行中の1回のMMIO、バス例外、デバイス再初期化を解決したものではなく、開始拒否を維持する。直前コミット 4bd4de0 の４構成はWDK 8.1 ARM32リンク・CAT生成に成功したが、この追加差分は別途ビルドが必要。

同期PIOの各MMIOは状態ワードの単一アクセス所有権を取得してから行い、完了後に返す。閉鎖と取得は原子的で、閉鎖後の新規アクセスは拒否する。既に取得済みのアクセスが残る間は状態上の排出完了を認めない。これは実ハードウェアがアクセス命令の前または最中に消えることを防げず、Storport所有資源の解放も止めない。例外でアクセスから復帰しない場合に所有権を解放したことにしない。通常停止はStorportのSRB排出契約、突発除去の実際のハードウェア保証は別途必要。電源復帰はクロック・電源・カード再初期化の実装がないため引き続き拒否する。
