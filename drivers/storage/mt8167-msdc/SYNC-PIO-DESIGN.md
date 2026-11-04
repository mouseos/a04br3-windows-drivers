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

## マッピングと再開照合部品

同期PIO構成では `StorPortGetDeviceBase` が失敗した場合、管理外の `MmMapIoSpace` にフォールバックせず開始を拒否する。取得後に設定を拒否する場合は `StorPortFreeDeviceBase` を呼ぶ。この構成ではUART直接マッピングとドライバ全体のバグチェックコールバック登録もしない。Storportのマッピングを使うこと自体は、アクセス中の物理消失からの復旧を保証しない。

`resume_check.c` は再初期化後の観測値を既知状態と照合する部品であり、現時点ではハードウェアに接続されていない。電源・クロックの所有、DMA停止、PIO・割込み状態、カードの転送状態、CID・容量・RCA、バス幅、ユーザー領域とキャッシュ無効を保守的に検査する。合格だけで復帰やリソース所有を証明しない。信頼できる同一電源サイクルの世代・CID等の取得先、電源とクロック・ピン・カードの初期化主体、排他・再開順序は未実装。値を仮定で供給せず、再開と起動の拒否を維持する。

## 再開後の状態取得試作

`resume_capture.c` は排他所有と電源・クロック・ピンの有効性を保証する外部主体から、世代付きの許可コールバックを受けて初めて実行する。PIO・安定クロック・DMA停止・割込みマスクを確認し、選択済みRCAに対するCMD13、CMD10（CID）、CMD8（EXT_CSD）、再度のCMD13から `resume_check.c` 用の観測値を作る。途中で許可を失えば追加コマンドを送らず、中途半端な観測値を公開しない。CIDはメモリ上でのみ扱い、ログや配布物へ埋め込まない。この部品は実際の電源所有者、WindowsのPnP、再開処理のいずれにも接続されていない。コールバックが真を返すこと自体は物理的な電源や資源寿命の証明ではなく、再開ゲートを開く根拠ではない。

機種DTSではMSDC0は8-bit eMMCで、VEMC3V3をVMMC、常時有効のVIO18をVQMMCに指定する。Linuxはclock ungate後に保存レジスタを復元するが、必要なレギュレータ・ピン・時計の所有者と排他も使う。機種UEFIのCMD0/CMD1/CMD2/CMD3/CMD7等は一連のコールド初期化で、Windows側から安全に実行できるという証拠ではない。実際の電源・ピン・クロック復帰、保存状態の真正性、カード再初期化、物理除去とMMIOの競合は未解決。安全ゲートは維持する。
