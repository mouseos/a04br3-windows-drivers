# eMMC read pathと性能実測の対応

確認基点は `d751c67`。これはソースの経路確認で、現在実配置SYSの出自確認や新しい性能測定ではない。
未反映の安全なPIO一括化は見つからず、今回runtime・build flags・安全ゲートは変更しない。

|経路|ソースと選択条件|一括化|導入状態|
|---|---|---|---|
|単一PIO|`pio.c` の `a04_msdc_read_sector`、初期GPT/EXT_CSD取得や非DMA読取り|CMD17、512 bytes。EXT_CSDはCMD8|起動driverの通常SCSI READすべてがこの経路という意味ではない|
|同期PIO試作|`sync_pio.c`、`A04_SYNC_PIO_MULTIREAD`|既に1..8 sectors。1はCMD17、2..8はCMD23+CMD18|read-only物理構成専用。`A04Initialize`で開始拒否|
|既存boot family|`miniport.c` の `A04RunIo`、`A04_DMA_PROBE`|通常は `A04DmaSector`、有効なmultiblock構成では最大8の `A04DmaTransfer`|既存の隔離passive/virtual-bounce構成。PIO試作とは別|
|既存CMD25候補|`6b5043e`、再現helper/verifier `d4d8d57`|最大8のCMD25書込み。PIO読取りの実装ではない|この調査では再実装も有効化もしない|

`build-wdk81-probe.ps1` は同期PIOとDMA/書込み/passive-worker/boot-multiblockの併用を拒否する。
`A04RunIo`もDMAを先に選択し、その `#elif` が同期PIO。PIOフラグを足して既存bootのreadだけ
差し替えられる構造ではない。`A04Initialize`はboot/scratch multiblockと同期PIOの開始を拒否する。
従って `d4d8d57` helperのpackage生成も、その設定での起動成功やゲート解除を保証しない。

## Linux元コードとの照合

Benesse公開 `CT3_GPL_LGPL_Src/kernel-4.4/drivers/mmc/host/mediatek/mt8167/sd.c` の
`msdc_pio_read` はFIFO閾値分を32bitでまとめて読み、短い末尾だけbyteで読む。
XFER_COMPLと全要求bytes消費の両方を待ち、DATA timeout/CRCは成功より先に処理する。
`msdc_do_request` はPIOでAutoCMD12/23を扱い直し、失敗時はstop経路へ進む。
このLinuxの割込み・clock・SG・停止所有はWindowsにそのまま移せない。

現 `sync_pio.c` にも、FIFO countを一度読み、そのcount内を32bit RXアクセスで消費するループがある。
全bytesをprivate stagingへ読み、DONEも確認してから要求bufferへコピーする。
CMD23によるblocks制限、PIO有効/DMA非稼働/イベントmask、CRC/timeout優先、
取消し確認、時間/poll上限、失敗時CMD12/R1b/DAT0（取消し後は追加MMIOを避ける）を保持する。
既に一括化されており、単純な追加CMD18実装やFIFO閾値レジスタ変更の根拠はない。
取消し確認や完了待ちを減らした速度向上も、今回の根拠からは認めない。

## 過去実測の範囲

保存記録 `port/gsi/logs/ios-feasibility/windows-rt-oobe-black-20261103.md` の
「eMMC第3回」「単一ブロック比較」「BQHR/残量/eMMC一括実機結果」に対応する。
生ログ・privateデータはこのソースrepoへ含めない。

- read-only DMA診断の4KiB×32768 read（16384 pairs、合計128MiB）で、単一29.687秒、
  最大8の複数13.485秒。約4.31→9.49MiB/s、約2.20倍。比較・API呼出しを含む。
- 当時同じ3CPU UEFI、同じ署名済み計時EXE、同じ範囲、実施順は複数→単一、各単回。
  これは測定条件の記録で、CPU数の再試験を提案するものではない。
- 後続の複数読取りは13.860秒でpairs一致し、追加16MiBのSHA256が別Recoveryでの参照と一致。
  その照合値や端末データは公開ソースへ転記しない。
- これらはCMD18 DMAの限定結果。同期PIO・通常Windows起動・CMD25書込みの速度や
  sleep/resume、現在構成の整合性を証明しない。PIOへの切替で同じ2.2倍になるとは言えない。

## 導入条件

今回の成果は経路と証拠の対応を固定したもので、新しい性能候補SYSはない。
GPU優先の現試験へ、同期PIOゲート解除・新DMA・Storport全面変更を混ぜない。
既存候補を主担当が後に採用する場合も、次を満たす別の固定packageとして扱う。

1. 使用source commit、実効flags、INF/SYS/CAT hashと実配置SYSを照合する。
   旧SYSでの結果を新候補の成否にしない。
2. 同期PIOをbootへ接続する前に、現在の開始拒否理由であるStorport要求/資源寿命、
   stop/reset/remove時の排出、power復帰を解決する。compile成功で解除しない。
3. 実投入可能な経路に限り既存のread-only計時・独立参照内容照合をまとめて使う。
   CMD25候補のwrite/read内容検証は既存 `msdc-file-verify` の専用新規file方式を使い、
   任意raw LBAや既存fileへ書かない。今回その実行や追加実機試験は行わない。

旧 [BOOT-MULTIBLOCK.md](BOOT-MULTIBLOCK.md) はDMA起動試作の記録、
[SYNC-PIO-DESIGN.md](SYNC-PIO-DESIGN.md) は独立同期PIOの設計と未達条件。
どちらも現状の実機起動driverへの安全な差替え許可として扱わない。
